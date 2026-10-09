#include <SDL3/SDL.h>
#include "../src/render/shaders/generated/colour_warp_portable.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
using U=Uint32;
void check(bool value){if(!value)throw std::runtime_error(SDL_GetError());}
U next_word(U& state,bool& carry){
    U word=0;
    for(unsigned hop=0;hop<32;++hop){
        const U swapped=((state&255)*256)+(state/256);
        const U rotated=swapped/2+(carry?32768:0);
        const U first=rotated+state;
        const U second=(first%65536)+state+(first>=65536);
        carry=second>=65536;state=(second+1)%65536;word=state;
        if((word&0xc000)!=0x8000)return word;
    }
    return word&~0x8000U;
}
int main()try{
    check(SDL_Init(SDL_INIT_VIDEO));
    auto* device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,true,nullptr);check(device);
    SDL_GPUComputePipelineCreateInfo pi{};
    pi.code=starfox::render::colour_warp_shader::spirv;pi.code_size=sizeof(starfox::render::colour_warp_shader::spirv);
    pi.format=SDL_GPU_SHADERFORMAT_SPIRV;pi.entrypoint="main";
    pi.num_readonly_storage_buffers=4;pi.num_readwrite_storage_buffers=2;pi.num_uniform_buffers=1;
    pi.threadcount_x=pi.threadcount_y=pi.threadcount_z=1;
    auto* pipeline=SDL_CreateGPUComputePipeline(device,&pi);check(pipeline);
    constexpr std::array<U,6> sizes{1024,8,256,64,1024,8};
    std::array<SDL_GPUBuffer*,6> buffers{};
    for(unsigned i=0;i<6;++i){SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,sizes[i],0};buffers[i]=SDL_CreateGPUBuffer(device,&bi);check(buffers[i]);}
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,1352,0};
    auto* upload=SDL_CreateGPUTransferBuffer(device,&ti);check(upload);
    ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=1032;
    auto* download=SDL_CreateGPUTransferBuffer(device,&ti);check(download);
    U compared=0;
    for(U fixture=0;fixture<135;++fixture){
        std::array<U,256> order{};std::array<std::array<U,4>,16> polygons{};std::array<U,16> visible{};
        for(U i=0;i<16;++i){polygons[i][2]=i;visible[i]=(i+fixture)%3!=0;}
        for(U i=0;i<256;++i)order[i]=(i*7+fixture)%16;
        std::array<U,2> traversal{256,0};
        const bool invalid=fixture>=128 && fixture<131;
        if(fixture==128)traversal[1]=1;
        if(fixture==129)traversal[0]=257;
        if(fixture==130)order[17]=16;
        if(fixture==131)traversal={0,0};
        if(fixture==132)traversal={7,0};
        const bool destruction=fixture>=133;
        if(fixture==134)visible.fill(0);
        const std::array<U,4> settings{256,16,16,fixture*521U|(destruction?0x80000000U:0U)};
        auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));check(mapped);
        std::memcpy(mapped,order.data(),1024);std::memcpy(mapped+1024,traversal.data(),8);
        std::memcpy(mapped+1032,polygons.data(),256);std::memcpy(mapped+1288,visible.data(),64);
        SDL_UnmapGPUTransferBuffer(device,upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);check(command);
        auto* copy=SDL_BeginGPUCopyPass(command);check(copy);U offset=0;
        for(unsigned i=0;i<4;++i){SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{buffers[i],0,sizes[i]};SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=sizes[i];}
        SDL_EndGPUCopyPass(copy);SDL_PushGPUComputeUniformData(command,0,settings.data(),16);
        SDL_GPUStorageBufferReadWriteBinding outputs[2]{};outputs[0].buffer=buffers[4];outputs[1].buffer=buffers[5];
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,outputs,2);check(pass);
        SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,buffers.data(),4);
        SDL_DispatchGPUCompute(pass,1,1,1);SDL_EndGPUComputePass(pass);
        copy=SDL_BeginGPUCopyPass(command);check(copy);
        for(unsigned i=4;i<6;++i){SDL_GPUBufferRegion from{buffers[i],0,sizes[i]};SDL_GPUTransferBufferLocation to{download,i==4?0U:1024U};SDL_DownloadFromGPUBuffer(copy,&from,&to);}
        SDL_EndGPUCopyPass(copy);auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence);
        check(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        auto* actual=static_cast<const U*>(SDL_MapGPUTransferBuffer(device,download,false));check(actual);
        if(invalid){if(actual[256]!=0 || actual[257]!=1)throw std::runtime_error("invalid list accepted");}
        else{
            if(actual[256]!=traversal[0] || actual[257]!=0)throw std::runtime_error("valid list rejected");
            U state=settings[3]&65535U;bool carry=false;
            for(U i=0;i<traversal[0];++i){const U expected=(destruction || visible[order[i]])?next_word(state,carry):0xffffffffU;if(actual[i]!=expected)throw std::runtime_error("descriptor sequence mismatch");++compared;}
        }
        for(U i=invalid?0U:traversal[0];i<256;++i)
            if(actual[i]!=0xffffffffU)throw std::runtime_error("unused descriptor retained stale material");
        SDL_UnmapGPUTransferBuffer(device,download);
    }
    SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);
    for(auto* b:buffers)SDL_ReleaseGPUBuffer(device,b);SDL_ReleaseGPUComputePipeline(device,pipeline);
    SDL_DestroyGPUDevice(device);SDL_Quit();std::cout<<compared<<" ordered descriptors match; hidden/repeated faces, invalid lists and empty reuse pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
