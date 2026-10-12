// Native exact-identity checks. No source/root/colour inputs or permission.
#include "reflected_curved_device.hpp"
#include "reflected_interval_identities_dxil.hpp"
#include "reflected_interval_identities_spirv.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <vector>
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Packet {std::array<unsigned,4> flags;std::array<std::array<float,2>,16> bounds;};
static_assert(sizeof(Packet)==144);
struct Owner {
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};SDL_GPUBuffer* buffer{};SDL_GPUTransferBuffer* transfer{};
    ~Owner(){if(!device)return;SDL_WaitForGPUIdle(device);
        if(transfer)SDL_ReleaseGPUTransferBuffer(device,transfer);
        if(buffer)SDL_ReleaseGPUBuffer(device,buffer);
        if(pipeline)SDL_ReleaseGPUComputePipeline(device,pipeline);
        SDL_DestroyGPUDevice(device);SDL_Quit();}
};
int main(int argc,char** argv)try {
    require(argc==2 && (std::string_view(argv[1])=="direct3d12" || std::string_view(argv[1])=="vulkan"),
        "Use direct3d12|vulkan");
    require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());Owner owner;
    owner.device=create_reflected_curved_device(argv[1]);require(owner.device,SDL_GetError());
    require_reflected_curved_precision(owner.device,argv[1]);
    const bool spirv=std::string_view(argv[1])=="vulkan";
    SDL_GPUComputePipelineCreateInfo info{};
    info.entrypoint="interval_identity_main";
    info.code=spirv?reflected_interval_identities_spirv:reflected_interval_identities_dxil;
    info.code_size=spirv?sizeof(reflected_interval_identities_spirv):sizeof(reflected_interval_identities_dxil);
    info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
    info.num_readwrite_storage_buffers=1;info.threadcount_x=16;info.threadcount_y=info.threadcount_z=1;
    owner.pipeline=SDL_CreateGPUComputePipeline(owner.device,&info);require(owner.pipeline,SDL_GetError());
    constexpr unsigned size=16*sizeof(Packet);
    const SDL_GPUBufferCreateInfo b{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,size,0};
    owner.buffer=SDL_CreateGPUBuffer(owner.device,&b);require(owner.buffer,SDL_GetError());
    const SDL_GPUTransferBufferCreateInfo t{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,size,0};
    owner.transfer=SDL_CreateGPUTransferBuffer(owner.device,&t);require(owner.transfer,SDL_GetError());
    constexpr std::array<unsigned,16> bits{0,0x80000000U,0x00800000U,0x80800000U,1,0x80000001U,
        0x3f800000U,0xbf800000U,0x3dcccccdU,0xbdcccccdU,0x2f800000U,0xaf800000U,
        0x501502f9U,0xd01502f9U,0x5d800000U,0xdd800000U};
    std::array<Packet,16> previous{};
    for(unsigned round=0;round<2;++round) {
        auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command,SDL_GetError());
        const SDL_GPUStorageBufferReadWriteBinding output{owner.buffer,false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,owner.pipeline);SDL_DispatchGPUCompute(pass,1,1,1);SDL_EndGPUComputePass(pass);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        const SDL_GPUBufferRegion from{owner.buffer,0,size};const SDL_GPUTransferBufferLocation to{owner.transfer,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,SDL_GetError());
        require(SDL_WaitForGPUFences(owner.device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(owner.device,fence);
        const auto* mapped=SDL_MapGPUTransferBuffer(owner.device,owner.transfer,false);require(mapped,SDL_GetError());
        std::array<Packet,16> results;std::memcpy(results.data(),mapped,size);SDL_UnmapGPUTransferBuffer(owner.device,owner.transfer);
        if(round)require(std::memcmp(results.data(),previous.data(),size)==0,"Held identity inputs changed output");
        for(unsigned n=0;n<results.size();++n) {
            const auto& r=results[n];
            require(r.flags==std::array<unsigned,4>{n,0,15,1},"Identity path hid nonfinite/zero-denominator failure or rejected valid input");
            for(unsigned k=0;k<16;++k) {
                const unsigned expected=k==2 || k==3 || k==10 || k==11 || k==14?0:
                    k==6 || k==7 || k==9?bits[n]^0x80000000U:bits[n];
                if(k==12 || k==13)require(r.bounds[k]==std::array<float,2>{-2,3},"Zero/unit widened a finite interval");
                else if(k==15)require(r.bounds[k]==std::array<float,2>{-3,2},"Negative-unit division did not reverse endpoints exactly");
                else for(float endpoint:r.bounds[k]) {
                    const auto actual=std::bit_cast<unsigned>(endpoint);
                    const bool same=(expected&0x7fffffffU)?actual==expected:(actual&0x7fffffffU)==0;
                    if(!same) {
                        std::cout<<"Identity diagnostic input="<<n<<" operation="<<k
                            <<" expected-bits="<<std::hex<<expected<<" actual-bits="<<actual<<std::dec<<std::endl;
                        throw std::runtime_error("Exact identity widened or flushed a finite/subnormal endpoint");
                    }
                }
            }
        }
        previous=results;std::cout<<"round="<<round<<" 16 native inputs x16 exact interval identities; nonfinite and zero-denominator refusals passed.\n";
    }
    std::cout<<"Native interval identity unit PASS, including signed zero/subnormal/min-normal/large inputs and held equality. Not physical optical/source/RGB/FPS acceptance.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
