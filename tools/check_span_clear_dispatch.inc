#include "starfox/render/gpu_dispatch.hpp"
#include "../src/render/shaders/generated/span_clear_portable.hpp"

// Independent byte oracle for the actual minimal shader. Large masks cross
// SDL's 65535-group X limit without needing a huge polygon/height fixture.
void check_span_clear_dispatch(SDL_GPUDevice* device) {
    using namespace starfox::render;
    constexpr unsigned command_bytes=20*96,prefix=64,tail=16;
    constexpr unsigned maximum_words=2*65535U*128+7;
    constexpr unsigned maximum_bytes=command_bytes+prefix+maximum_words*4+tail;
    struct Owner {
        SDL_GPUDevice* device;
        SDL_GPUComputePipeline* pipeline{};
        SDL_GPUBuffer *commands{},*masks{};
        SDL_GPUTransferBuffer *upload{},*download{};
        ~Owner() {
            SDL_WaitForGPUIdle(device);
            if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
            if(commands) SDL_ReleaseGPUBuffer(device,commands);
            if(masks) SDL_ReleaseGPUBuffer(device,masks);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        }
    } owner{device};
    SDL_GPUComputePipelineCreateInfo info{};
    const auto formats=SDL_GetGPUShaderFormats(device);
    if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format=SDL_GPU_SHADERFORMAT_SPIRV;
        info.code=span_clear_shader::spirv;info.code_size=sizeof(span_clear_shader::spirv);
    }
#if defined(_WIN32)
    else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
        info.format=SDL_GPU_SHADERFORMAT_DXIL;
        info.code=span_clear_shader::dxil;info.code_size=sizeof(span_clear_shader::dxil);
    }
#endif
#if defined(__APPLE__)
    else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
        info.format=SDL_GPU_SHADERFORMAT_MSL;
        info.code=reinterpret_cast<const Uint8*>(span_clear_shader::metal);
        info.code_size=std::strlen(span_clear_shader::metal);
    }
#endif
    else throw std::runtime_error("Span clear shader unavailable");
    info.entrypoint=info.format==SDL_GPU_SHADERFORMAT_MSL?"main0":"main";
    info.num_readwrite_storage_buffers=2;info.num_uniform_buffers=1;
    info.threadcount_x=128;info.threadcount_y=info.threadcount_z=1;
    owner.pipeline=SDL_CreateGPUComputePipeline(device,&info);require(owner.pipeline);
    SDL_GPUBufferCreateInfo buffer{};
    buffer.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    buffer.size=command_bytes;owner.commands=SDL_CreateGPUBuffer(device,&buffer);require(owner.commands);
    buffer.size=maximum_bytes-command_bytes;owner.masks=SDL_CreateGPUBuffer(device,&buffer);require(owner.masks);
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,maximum_bytes,0};
    owner.upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(owner.upload);
    transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    owner.download=SDL_CreateGPUTransferBuffer(device,&transfer);require(owner.download);
    unsigned cases=0,wide=0;std::uint64_t checked=0;
    for(unsigned words:{0U,1U,127U,128U,129U,65535U*128-1,65535U*128,65535U*128+1,maximum_words}) {
        const unsigned rows=1+(cases%3)*5,mask_bytes=prefix+words*4+tail,bytes=command_bytes+mask_bytes;
        const auto dispatch=linear_gpu_dispatch128(std::max(rows,words));
        if(!dispatch.x || dispatch.x>65535 || !dispatch.y || dispatch.y>65535
            || std::uint64_t(dispatch.row_stride)*dispatch.y<std::max(rows,words))
            throw std::runtime_error("Invalid span-clear dispatch extent");
        wide+=dispatch.y>1;
        auto* uploaded=static_cast<Uint32*>(SDL_MapGPUTransferBuffer(device,owner.upload,true));require(uploaded);
        for(unsigned i=0;i<bytes/4;++i) uploaded[i]=0xd00dbeefU^(i*1664525U);
        SDL_UnmapGPUTransferBuffer(device,owner.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(auto [target,offset,size]:{std::tuple{owner.commands,0U,command_bytes},std::tuple{owner.masks,command_bytes,mask_bytes}}) {
            SDL_GPUTransferBufferLocation source{owner.upload,offset};SDL_GPUBufferRegion destination{target,0,size};
            SDL_UploadToGPUBuffer(copy,&source,&destination,false);
        }
        SDL_EndGPUCopyPass(copy);
        const Uint32 settings[]{rows,words,prefix,dispatch.row_stride};
        SDL_PushGPUComputeUniformData(command,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding targets[]{{owner.commands,false},{owner.masks,false}};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,targets,2);require(pass);
        SDL_BindGPUComputePipeline(pass,owner.pipeline);
        SDL_DispatchGPUCompute(pass,dispatch.x,dispatch.y,1);SDL_EndGPUComputePass(pass);
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(auto [source,offset,size]:{std::tuple{owner.commands,0U,command_bytes},std::tuple{owner.masks,command_bytes,mask_bytes}}) {
            SDL_GPUBufferRegion region{source,0,size};SDL_GPUTransferBufferLocation destination{owner.download,offset};
            SDL_DownloadFromGPUBuffer(copy,&region,&destination);
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* output=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,owner.download,false));require(output);
        bool exact=true;
        for(unsigned i=0;i<bytes/4;++i) {
            const bool cleared=i<command_bytes/4?(i/24<rows && i%24<4)
                :i>=(command_bytes+prefix)/4 && i<(command_bytes+prefix)/4+words;
            const Uint32 expected=cleared?0:0xd00dbeefU^(i*1664525U);
            exact &= output[i]==expected;
        }
        SDL_UnmapGPUTransferBuffer(device,owner.download);
        if(!exact) throw std::runtime_error("Minimal span clear changed payload/prefix/guard or missed a row/mask word");
        checked+=bytes;++cases;
    }
    if(wide!=2) throw std::runtime_error("Span clear oracle missed two-dimensional dispatch");
    std::cout<<"Minimal span clear: "<<cases<<" poisoned/recycled cases, "<<wide<<" wide dispatches; "
        <<checked<<" independently checked bytes, including command payload and mask prefix/tail guards\n";
}
