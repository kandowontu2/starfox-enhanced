#include <SDL3/SDL.h>
#include "starfox/render/sdl_d3d12_bridge.h"
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

static void require(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);}
struct Device {
    SDL_GPUDevice* value{};
    ~Device(){if(value){SDL_WaitForGPUIdle(value);SDL_DestroyGPUDevice(value);}SDL_Quit();}
};
static void wait(SDL_GPUDevice* device,SDL_GPUCommandBuffer* command) {
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,SDL_GetError());
    unsigned polls=0;
    while(!SDL_QueryGPUFence(device,fence) && polls<10000){SDL_Delay(1);++polls;}
    const bool finished=SDL_QueryGPUFence(device,fence);
    if(!finished)SDL_WaitForGPUIdle(device);
    SDL_ReleaseGPUFence(device,fence);require(finished,"Descriptor regression exceeded completion deadline");
}
static void validate(ID3D12InfoQueue* queue) {
    require(queue->GetNumMessagesDiscardedByMessageCountLimit()==0,"D3D12 validation discarded messages");
    const auto count=queue->GetNumStoredMessages();
    for(UINT64 i=0;i<count;++i){
        SIZE_T size{};require(SUCCEEDED(queue->GetMessage(i,nullptr,&size)),"Message size");
        std::vector<unsigned char> bytes(size);auto* msg=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
        require(SUCCEEDED(queue->GetMessage(i,msg,&size)),"Message read");
        if(msg->Severity==D3D12_MESSAGE_SEVERITY_ERROR || msg->Severity==D3D12_MESSAGE_SEVERITY_CORRUPTION)
            throw std::runtime_error(msg->pDescription);
    }
}
int main(int argc,char** argv) try {
    require(argc==3,"Usage: check_sdl_d3d12_descriptors plain.dxil samplers.dxil");
    require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());Device owner;
    owner.value=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,true,"direct3d12");
    auto* device=owner.value;require(device,SDL_GetError());
    auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device),STARFOX_SDL_D3D12_DEVICE,nullptr));
    require(native,"Native D3D12 bridge unavailable");
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> queue;
    require(SUCCEEDED(native->QueryInterface(IID_ID3D12InfoQueue,reinterpret_cast<void**>(queue.GetAddressOf()))),"Debug queue unavailable");
    validate(queue.Get());
    D3D12_MESSAGE_SEVERITY critical[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
    D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=critical;
    require(SUCCEEDED(queue->AddStorageFilterEntries(&filter)),"Debug filter failed");
    std::cout<<"Descriptor regression adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<std::endl;
    constexpr unsigned maximum=36000;
    const std::array<Uint32,4> values{0x10203040,0x50607080,0x12345678,0x87654321};
    std::array<SDL_GPUBuffer*,4> inputs{};
    for(auto& input:inputs){SDL_GPUBufferCreateInfo ci{};ci.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;ci.size=4;input=SDL_CreateGPUBuffer(device,&ci);require(input,SDL_GetError());}
    SDL_GPUBufferCreateInfo out_info{};out_info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;out_info.size=maximum*4;
    auto* output=SDL_CreateGPUBuffer(device,&out_info);require(output,SDL_GetError());
    SDL_GPUTransferBufferCreateInfo upload_info{};upload_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;upload_info.size=1024;
    auto* upload=SDL_CreateGPUTransferBuffer(device,&upload_info);require(upload,SDL_GetError());
    auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));require(mapped,SDL_GetError());
    std::memset(mapped,0,1024);std::memcpy(mapped,values.data(),16);
    mapped[512]=17;mapped[515]=255;mapped[768]=197;mapped[771]=255;SDL_UnmapGPUTransferBuffer(device,upload);
    std::array<SDL_GPUTexture*,2> textures{};
    for(auto& tex:textures){SDL_GPUTextureCreateInfo ci{};ci.type=SDL_GPU_TEXTURETYPE_2D;ci.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;ci.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;ci.width=ci.height=ci.layer_count_or_depth=ci.num_levels=1;tex=SDL_CreateGPUTexture(device,&ci);require(tex,SDL_GetError());}
    SDL_GPUSamplerCreateInfo sampler_info{};auto* sampler=SDL_CreateGPUSampler(device,&sampler_info);require(sampler,SDL_GetError());
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());auto* copy=SDL_BeginGPUCopyPass(command);
    for(unsigned i=0;i<4;++i){SDL_GPUTransferBufferLocation from{upload,i*4};SDL_GPUBufferRegion to{inputs[i],0,4};SDL_UploadToGPUBuffer(copy,&from,&to,false);}
    for(unsigned i=0;i<2;++i){SDL_GPUTextureTransferInfo from{};from.transfer_buffer=upload;from.offset=512+i*256;from.pixels_per_row=from.rows_per_layer=1;SDL_GPUTextureRegion to{};to.texture=textures[i];to.w=to.h=to.d=1;SDL_UploadToGPUTexture(copy,&from,&to,false);}
    SDL_EndGPUCopyPass(copy);wait(device,command);
    SDL_GPUTransferBufferCreateInfo download_info{};download_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;download_info.size=maximum*4;
    auto* download=SDL_CreateGPUTransferBuffer(device,&download_info);require(download,SDL_GetError());
    std::array<SDL_GPUComputePipeline*,2> pipelines{};
    for(unsigned mode=0;mode<2;++mode){
        std::ifstream file(argv[mode+1],std::ios::binary);require(bool(file),"Shader open failed");
        const std::vector<unsigned char> code{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        SDL_GPUComputePipelineCreateInfo ci{};ci.code=code.data();ci.code_size=code.size();ci.entrypoint="main";ci.format=SDL_GPU_SHADERFORMAT_DXIL;
        ci.num_samplers=mode?2:0;ci.num_readonly_storage_buffers=2;ci.num_readwrite_storage_buffers=1;ci.num_uniform_buffers=1;
        ci.threadcount_x=ci.threadcount_y=ci.threadcount_z=1;pipelines[mode]=SDL_CreateGPUComputePipeline(device,&ci);require(pipelines[mode],SDL_GetError());
    }
    // Original capacities stay 65536 views / 2048 samplers. Exercise tables
    // straddling the end, both with pipeline rebinds and cached UAV tables.
    for(unsigned mode=0;mode<3;++mode){
        const unsigned count=mode==0?24000:mode==1?maximum:3000;
        command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=output;
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&binding,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipelines[mode==2]);
        for(unsigned i=0;i<count;++i){
            if(mode==0)SDL_BindGPUComputePipeline(pass,pipelines[0]);
            SDL_GPUBuffer* reads[]{inputs[(i&1)*2],inputs[(i&1)*2+1]};
            SDL_BindGPUComputeStorageBuffers(pass,0,reads,2);
            if(mode==2){SDL_GPUTextureSamplerBinding texs[]{{textures[i&1],sampler},{textures[(i&1)^1],sampler}};SDL_BindGPUComputeSamplers(pass,0,texs,2);}
            const std::array<Uint32,4> constants{i,0,0,0};SDL_PushGPUComputeUniformData(command,0,constants.data(),16);
            SDL_DispatchGPUCompute(pass,1,1,1);
        }
        SDL_EndGPUComputePass(pass);copy=SDL_BeginGPUCopyPass(command);
        SDL_GPUBufferRegion from{output,0,count*4};SDL_GPUTransferBufferLocation to{download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);
        SDL_EndGPUCopyPass(copy);wait(device,command);
        const auto* actual=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,download,false));require(actual,SDL_GetError());
        for(unsigned i=0;i<count;++i){
            Uint32 expected=values[(i&1)*2]^values[(i&1)*2+1]^i;
            if(mode==2)expected^=(i&1?197U:17U)^((i&1?17U:197U)<<8);
            if(actual[i]!=expected)throw std::runtime_error("Descriptor result mismatch mode="+std::to_string(mode)+" index="+std::to_string(i));
        }
        SDL_UnmapGPUTransferBuffer(device,download);validate(queue.Get());
        std::cout<<"Descriptor mode "<<mode<<": "<<count<<" exact GPU results passed"<<std::endl;
    }
    for(auto* pipeline:pipelines)SDL_ReleaseGPUComputePipeline(device,pipeline);
    for(auto* input:inputs)SDL_ReleaseGPUBuffer(device,input);
    for(auto* tex:textures)SDL_ReleaseGPUTexture(device,tex);
    SDL_ReleaseGPUSampler(device,sampler);SDL_ReleaseGPUBuffer(device,output);
    SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);
    SDL_WaitForGPUIdle(device);SDL_DestroyGPUDevice(device);owner.value=nullptr;SDL_Quit();validate(queue.Get());
    std::cout<<"All heap rollover results exact; zero critical/discarded D3D12 messages through teardown"<<std::endl;
    return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
