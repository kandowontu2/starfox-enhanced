#include "starfox/render/gpu_smaa.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/smaa_portable.hpp"
#include "../../third_party/smaa/AreaTex.h"
#include "../../third_party/smaa/SearchTex.h"
#endif
namespace starfox::render {
struct GpuSmaa::Impl {
    std::string status{"SMAA not initialized"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};SDL_GPUSampler* sampler{};
    SDL_GPUTexture *area{},*search{};std::array<SDL_GPUTexture*,3> outputs{};
    unsigned width{},height{};
    bool ready{};
    ~Impl() {
        if(!device) return;
        for(auto* t:outputs) if(t) SDL_ReleaseGPUTexture(device,t);
        if(area) SDL_ReleaseGPUTexture(device,area);
        if(search) SDL_ReleaseGPUTexture(device,search);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
    SDL_GPUTexture* texture(unsigned w,unsigned h,SDL_GPUTextureFormat format,bool target) {
        SDL_GPUTextureCreateInfo t{};t.type=SDL_GPU_TEXTURETYPE_2D;t.width=w;t.height=h;
        t.layer_count_or_depth=t.num_levels=1;t.format=format;t.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if(target) t.usage|=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
        auto* result=SDL_CreateGPUTexture(device,&t);if(!result) throw std::runtime_error(SDL_GetError());return result;
    }
    void initialize(SDL_GPUDevice* d) {
        device=d;
        SDL_GPUComputePipelineCreateInfo p{};const auto formats=SDL_GetGPUShaderFormats(d);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {p.format=SDL_GPU_SHADERFORMAT_SPIRV;p.code=smaa_shader::spirv;p.code_size=sizeof(smaa_shader::spirv);p.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {p.format=SDL_GPU_SHADERFORMAT_DXIL;p.code=smaa_shader::dxil;p.code_size=sizeof(smaa_shader::dxil);p.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {p.format=SDL_GPU_SHADERFORMAT_MSL;p.code=reinterpret_cast<const Uint8*>(smaa_shader::metal);p.code_size=sizeof(smaa_shader::metal)-1;p.entrypoint="main0";}
        else throw std::runtime_error("SMAA requires compute shaders");
        p.num_samplers=5;p.num_readonly_storage_buffers=1;p.num_readwrite_storage_textures=1;p.num_uniform_buffers=1;
        p.threadcount_x=p.threadcount_y=8;p.threadcount_z=1;
        pipeline=create_gpu_compute_pipeline(d,&p);if(!pipeline) throw std::runtime_error(SDL_GetError());
        SDL_GPUSamplerCreateInfo s{};s.min_filter=s.mag_filter=SDL_GPU_FILTER_LINEAR;s.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        s.address_mode_u=s.address_mode_v=s.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler=SDL_CreateGPUSampler(d,&s);if(!sampler) throw std::runtime_error(SDL_GetError());
        area=texture(AREATEX_WIDTH,AREATEX_HEIGHT,SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,false);
        search=texture(SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,SDL_GPU_TEXTUREFORMAT_R8_UNORM,false);
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,AREATEX_SIZE+SEARCHTEX_SIZE,0};
        auto* upload=SDL_CreateGPUTransferBuffer(d,&info);if(!upload) throw std::runtime_error(SDL_GetError());
        auto* bytes=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(d,upload,false));
        if(!bytes) {SDL_ReleaseGPUTransferBuffer(d,upload);throw std::runtime_error(SDL_GetError());}
        std::memcpy(bytes,areaTexBytes,AREATEX_SIZE);std::memcpy(bytes+AREATEX_SIZE,searchTexBytes,SEARCHTEX_SIZE);
        SDL_UnmapGPUTransferBuffer(d,upload);
        auto* command=SDL_AcquireGPUCommandBuffer(d);
        if(!command) {SDL_ReleaseGPUTransferBuffer(d,upload);throw std::runtime_error(SDL_GetError());}
        auto* pass=SDL_BeginGPUCopyPass(command);
        if(!pass) {SDL_CancelGPUCommandBuffer(command);SDL_ReleaseGPUTransferBuffer(d,upload);throw std::runtime_error(SDL_GetError());}
        SDL_GPUTextureTransferInfo from{upload,0,0,0};SDL_GPUTextureRegion to{area,0,0,0,0,0,AREATEX_WIDTH,AREATEX_HEIGHT,1};
        SDL_UploadToGPUTexture(pass,&from,&to,false);
        from.offset=AREATEX_SIZE;to.texture=search;to.w=SEARCHTEX_WIDTH;to.h=SEARCHTEX_HEIGHT;
        SDL_UploadToGPUTexture(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);
        const bool submitted=SDL_SubmitGPUCommandBuffer(command);SDL_ReleaseGPUTransferBuffer(d,upload);
        if(!submitted) throw std::runtime_error(SDL_GetError());
        ready=true;
    }
    void resize(unsigned w,unsigned h) {
        if(width==w && height==h) return;
        width=height=0;
        for(auto*& t:outputs) {if(t) SDL_ReleaseGPUTexture(device,t);t=nullptr;}
        for(auto*& t:outputs) t=texture(w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,true);
        width=w;height=h;
    }
#endif
};
GpuSmaa::GpuSmaa():impl_(std::make_unique<Impl>()) {}
GpuSmaa::~GpuSmaa()=default;
void* GpuSmaa::enqueue(void* device,void* command,void* color,void* packed,unsigned w,unsigned h,unsigned quality,bool packed_tags) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!device || !command || !color || !packed || !w || !h || w>8192 || h>8192 || quality<1 || quality>3)
            throw std::runtime_error("Invalid SMAA inputs");
        if(impl_->device!=device || !impl_->ready) {
            impl_=std::make_unique<Impl>();impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        }
        for(auto* t:impl_->outputs) if(t==color) throw std::runtime_error("Aliased SMAA input");
        impl_->resize(w,h);
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        for(unsigned stage=0;stage<3;++stage) {
            const std::array<Uint32,8> constants{w,h,stage,quality,packed_tags?1U:0U,0,0,0};SDL_PushGPUComputeUniformData(cb,0,constants.data(),sizeof(constants));
            SDL_GPUStorageTextureReadWriteBinding output{};output.texture=impl_->outputs[stage];
            auto* pass=SDL_BeginGPUComputePass(cb,&output,1,nullptr,0);if(!pass) throw std::runtime_error(SDL_GetError());
            SDL_BindGPUComputePipeline(pass,impl_->pipeline);
            SDL_GPUTexture* textures[]{static_cast<SDL_GPUTexture*>(color),stage==0?static_cast<SDL_GPUTexture*>(color):impl_->outputs[0],
                stage==2?impl_->outputs[1]:static_cast<SDL_GPUTexture*>(color),impl_->area,impl_->search};
            SDL_GPUTextureSamplerBinding inputs[5]{};
            for(unsigned i=0;i<5;++i) inputs[i]={textures[i],impl_->sampler};
            SDL_BindGPUComputeSamplers(pass,0,inputs,5);
            auto* buffer=static_cast<SDL_GPUBuffer*>(packed);SDL_BindGPUComputeStorageBuffers(pass,0,&buffer,1);
            SDL_DispatchGPUCompute(pass,(w+7)/8,(h+7)/8,1);SDL_EndGPUComputePass(pass);
        }
        impl_->status="SMAA three passes encoded";return impl_->outputs[2];
    } catch(const std::exception& e) {impl_->status=e.what();}
#else
    (void)device;(void)command;(void)color;(void)packed;(void)w;(void)h;(void)quality;(void)packed_tags;
#endif
    return nullptr;
}
void GpuSmaa::release_device() noexcept {impl_=std::make_unique<Impl>();}
const std::string& GpuSmaa::status() const {return impl_->status;}
}
