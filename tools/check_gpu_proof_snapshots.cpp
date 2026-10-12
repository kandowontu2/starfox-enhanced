// Isolate native read/write-buffer/copy ordering from optical computation.
// All test patterns originate on the GPU; no certificate is ever uploaded.
#include "reflected_curved_device.hpp"
#include "gpu_proof_snapshots_seed_dxil.hpp"
#include "gpu_proof_snapshots_seed_spirv.hpp"
#include "gpu_proof_snapshots_marker_dxil.hpp"
#include "gpu_proof_snapshots_marker_spirv.hpp"
#include <array>
#include <cstring>
#include <vector>
#if defined(_WIN32)
#include "starfox/render/sdl_d3d12_bridge.h"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#endif
namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
constexpr unsigned words_per_query=6144,capacity=64,words=capacity*words_per_query,bytes=words*4;
using Words=std::vector<Uint32>;
struct Owner {
    SDL_GPUDevice* device{};
    SDL_GPUBuffer* scratch{};
    std::array<SDL_GPUComputePipeline*,2> pipelines{};
    std::array<SDL_GPUTransferBuffer*,5> captures{};
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> validation;
#endif
    void release() {
        if(!device)return;
        check(SDL_WaitForGPUIdle(device),SDL_GetError());
        for(auto*& transfer:captures)if(transfer){SDL_ReleaseGPUTransferBuffer(device,transfer);transfer=nullptr;}
        if(scratch){SDL_ReleaseGPUBuffer(device,scratch);scratch=nullptr;}
        for(auto*& pipeline:pipelines)if(pipeline){SDL_ReleaseGPUComputePipeline(device,pipeline);pipeline=nullptr;}
        SDL_DestroyGPUDevice(device);device=nullptr;
    }
    ~Owner() {
        const bool failed=device && std::uncaught_exceptions()>0;
        try{release();if(failed)validation_report(false);}catch(...){}
        SDL_Quit();
    }
    void validation_report(bool assert_clean) {
#if defined(_WIN32)
        if(validation) {
            const auto count=validation->GetNumStoredMessagesAllowedByRetrievalFilter();
            for(UINT64 n=0;n<count;++n) {
                SIZE_T size{};check(SUCCEEDED(validation->GetMessage(n,nullptr,&size)),"Missing D3D12 message size");
                std::vector<unsigned char> storage(size);
                auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                check(SUCCEEDED(validation->GetMessage(n,message,&size)),"Missing D3D12 message");
                std::cerr<<message->pDescription<<'\n';
            }
            const auto discarded=validation->GetNumMessagesDiscardedByMessageCountLimit();
            std::cout<<"D3D12 through device teardown: critical="<<count<<" discarded="<<discarded<<'\n';
            if(assert_clean)check(!count && !discarded,
                "Critical/discarded D3D12 messages through snapshot-device teardown");
        }
#else
        static_cast<void>(assert_clean);
#endif
    }
    void finish() {release();validation_report(true);}
    void submit(SDL_GPUCommandBuffer* command) {
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
        const bool waited=SDL_WaitForGPUFences(device,true,&fence,1);
        SDL_ReleaseGPUFence(device,fence);check(waited,SDL_GetError());
    }
    void snapshot(SDL_GPUCommandBuffer* command,unsigned slot) {
        auto* pass=SDL_BeginGPUCopyPass(command);check(pass,SDL_GetError());
        const SDL_GPUBufferRegion source{scratch,0,bytes};
        const SDL_GPUTransferBufferLocation destination{captures.at(slot),0};
        SDL_DownloadFromGPUBuffer(pass,&source,&destination);SDL_EndGPUCopyPass(pass);
    }
    Words read(unsigned slot) {
        const auto* mapped=SDL_MapGPUTransferBuffer(device,captures.at(slot),false);check(mapped,SDL_GetError());
        Words result(words);std::memcpy(result.data(),mapped,bytes);
        SDL_UnmapGPUTransferBuffer(device,captures.at(slot));return result;
    }
    void dispatch(SDL_GPUCommandBuffer* command,unsigned pipeline,unsigned count,unsigned phase) {
        const SDL_GPUStorageBufferReadWriteBinding output{scratch,false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&output,1);check(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipelines.at(pipeline));
        const std::array<Uint32,4> parameters{count,phase,0,0};
        SDL_PushGPUComputeUniformData(command,0,parameters.data(),unsigned(sizeof(parameters)));
        SDL_DispatchGPUCompute(pass,pipeline?1:words/64,1,1);SDL_EndGPUComputePass(pass);
    }
};
Uint32 expected(unsigned at,unsigned active,unsigned phase) {
    const unsigned query=at/words_per_query,word=at%words_per_query;
    if(phase && query<active && word>=48 && word<64)
        return 0x5afe0000U^(query*0x01010101U)^((word-48)*0x9e3779b9U)^phase;
    return 0xa5c30001U^(query*0x01010101U)^(word*0x9e3779b9U);
}
}
int main(int argc,char** argv)try {
    check((argc==2 || (argc==3 && std::string_view(argv[2])=="--write-only-diagnostic"))
        && (std::string_view(argv[1])=="direct3d12" || std::string_view(argv[1])=="vulkan"),
        "Use direct3d12|vulkan [--write-only-diagnostic]");
    const bool write_only=argc==3;
    check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());Owner owner;
    owner.device=create_reflected_curved_device(argv[1]);check(owner.device,SDL_GetError());
    std::cout<<"Snapshot protocol GPU="<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(owner.device),
        SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<" backend="<<argv[1]
        <<" buffer-usage="<<(write_only?"WRITE-only diagnostic, not production READ|WRITE":"READ|WRITE")<<'\n';
#if defined(_WIN32)
    if(std::string_view(argv[1])=="direct3d12") {
        auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(owner.device),
            STARFOX_SDL_D3D12_DEVICE,nullptr));
        check(native && SUCCEEDED(native->QueryInterface(IID_ID3D12InfoQueue,
            reinterpret_cast<void**>(owner.validation.GetAddressOf()))),"Missing D3D12 validation queue");
        D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
        D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
        check(SUCCEEDED(owner.validation->PushStorageFilter(&filter))
            && SUCCEEDED(owner.validation->PushRetrievalFilter(&filter)),"Missing D3D12 validation filters");
    }
#endif
    const bool vulkan=std::string_view(argv[1])=="vulkan";
    for(unsigned n=0;n<2;++n) {
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint=n?"marker_main":"seed_main";
        if(n) {
            info.code=vulkan?gpu_proof_snapshots_marker_spirv:gpu_proof_snapshots_marker_dxil;
            info.code_size=vulkan?sizeof(gpu_proof_snapshots_marker_spirv):sizeof(gpu_proof_snapshots_marker_dxil);
        }else {
            info.code=vulkan?gpu_proof_snapshots_seed_spirv:gpu_proof_snapshots_seed_dxil;
            info.code_size=vulkan?sizeof(gpu_proof_snapshots_seed_spirv):sizeof(gpu_proof_snapshots_seed_dxil);
        }
        info.format=vulkan?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=n?1:0;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        owner.pipelines[n]=SDL_CreateGPUComputePipeline(owner.device,&info);check(owner.pipelines[n],SDL_GetError());
    }
    const SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
        |(write_only?0U:SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ),bytes,0};
    owner.scratch=SDL_CreateGPUBuffer(owner.device,&buffer);check(owner.scratch,SDL_GetError());
    for(auto*& capture:owner.captures) {
        const SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes,0};
        capture=SDL_CreateGPUTransferBuffer(owner.device,&transfer);check(capture,SDL_GetError());
    }
    unsigned comparisons=0;
    for(unsigned active:std::array{1U,8U,64U}) {
        std::array<Words,5> previous;
        for(unsigned round=0;round<2;++round) {
            auto* command=SDL_AcquireGPUCommandBuffer(owner.device);check(command,SDL_GetError());
            owner.dispatch(command,0,active,0);owner.snapshot(command,0);
            for(unsigned phase=1;phase<=3;++phase) {owner.dispatch(command,1,active,phase);owner.snapshot(command,phase);}
            owner.submit(command);
            // Independent post-submit download: never replaces an earlier
            // snapshot or feeds its data back into the GPU owner.
            command=SDL_AcquireGPUCommandBuffer(owner.device);check(command,SDL_GetError());
            owner.snapshot(command,4);owner.submit(command);
            for(unsigned slot=0;slot<5;++slot) {
                const auto result=owner.read(slot);
                for(unsigned at=0;at<words;++at) {
                    const auto want=expected(at,active,slot<4?slot:3);
                    if(result[at]!=want) {
                        std::cerr<<"Snapshot mismatch active="<<active<<" round="<<round<<" slot="<<slot
                            <<" query="<<at/words_per_query<<" word="<<at%words_per_query
                            <<" expected=0x"<<std::hex<<want<<" actual=0x"<<result[at]<<std::dec<<'\n';
                        throw std::runtime_error("GPU scratch/prefix/copy ordering lost a required word");
                    }
                    ++comparisons;
                }
                if(round)check(result==previous[slot],"Held GPU scratch snapshots changed");else previous[slot]=result;
            }
            std::cout<<"active="<<active<<" held-round="<<round<<" complete64-slot scratch/snapshots match.\n";
        }
    }
    owner.finish();
    std::cout<<"GPU scratch/copy protocol PASS: "<<comparisons
        <<" exact words, independent before/after/fresh downloads. Not an optical/source/RGB or performance pass.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
