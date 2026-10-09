// Genuine resident GPU index/query staging, not cached-colour acceptance.
// CPU binary64 necessary domains independently check that no old source region
// is removed. Readbacks belong ONLY to this diagnostic executable.
#include <SDL3/SDL.h>
#include "full_curved_owner_replay.hpp"
#include "reflected_curved_device.hpp"
#include "reflected_visibility_tiles_dxil.hpp"
#include "reflected_visibility_tiles_spirv.hpp"
#include "reflected_visibility_reduce_dxil.hpp"
#include "reflected_visibility_reduce_spirv.hpp"
#include "reflected_visibility_query_dxil.hpp"
#include "reflected_visibility_query_spirv.hpp"
#include "reflected_visibility_domains_dxil.hpp"
#include "reflected_visibility_domains_spirv.hpp"
#include "reflected_visibility_optical_dxil.hpp"
#include "reflected_visibility_optical_spirv.hpp"
#if defined(STARFOX_OPTICAL_ARITHMETIC_TRACE_ONLY)
#include "reflected_visibility_arithmetic_dxil.hpp"
#include "reflected_visibility_arithmetic_spirv.hpp"
#else
#include "reflected_visibility_jets_dxil.hpp"
#include "reflected_visibility_jets_spirv.hpp"
#endif
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string_view>
#if defined(_WIN32)
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include "../include/starfox/render/sdl_d3d12_bridge.h"
#undef near
#undef far
#endif
namespace o=reflected_curved_owner_oracle;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Query {
    unsigned primary{};std::array<unsigned,4> mirrors{};unsigned control{},terminal{};
    std::array<float,3> feature{};unsigned unused{},lobe{};
};
struct Uniform {
    unsigned width{},height{},lobes{},primary_prefix{24},record_prefix{64},path_stride{64},total_nodes{},level_count{};
    unsigned work_level{},query_count{},leaf_budget{64},node_budget{4096};
    std::array<std::array<unsigned,4>,12> levels{};
};
struct Result {unsigned count{},status{},visited{},unused{};std::array<unsigned,64> tiles{};};
constexpr unsigned region_capacity=128;
struct DomainSettings {unsigned region_budget{region_capacity},support_budget{16384},capacity{region_capacity},unused{};};
struct Region {
    std::array<unsigned,4> support{};std::array<float,4> bounds{};std::array<float,2> initializer{};std::array<unsigned,2> padding{};
    bool operator==(const Region&)const=default;
};
struct DomainResult {unsigned count{},status{},visited{},unused{};std::array<Region,region_capacity> regions{};};
using F4=std::array<float,4>;
struct OpticalFrame {
    std::array<F4,6> receiver{};std::array<std::array<F4,3>,4> planes{};
    std::array<F4,8> liquid{};F4 terminal{};std::array<unsigned,4> control{};
};
struct OpticalResult {unsigned kept{},status{},visited{},excluded{};F4 hull{};bool operator==(const OpticalResult&)const=default;};
struct OpticalSettings {unsigned budget{8192},depth{12},capacity{region_capacity},unused{};};
struct JetInput {OpticalFrame frame;F4 box;};
struct JetResult {
    std::array<unsigned,4> status{};F4 box{};
    std::array<std::array<float,2>,8> ray{};
    std::array<std::array<float,2>,2> residual{};
    std::array<std::array<float,2>,4> jacobian{};
    F4 enclosure{},settings{};
    std::array<std::array<float,2>,2> centre_residual{};
};
static_assert(sizeof(JetInput)==464 && sizeof(JetResult)==192);
static_assert(sizeof(Query)==48 && sizeof(Uniform)==240 && sizeof(Result)==272 && sizeof(DomainSettings)==16
    && sizeof(Region)==48 && sizeof(DomainResult)==6160 && sizeof(OpticalFrame)==448 && sizeof(OpticalResult)==32 && sizeof(OpticalSettings)==16);
struct Dataset {unsigned width{},height{},lobes{1};std::vector<o::SourceTap> taps;};
Dataset selected_lobe_bank(Dataset mono,unsigned lobe) {
    require(mono.lobes==1 && lobe<8,"Invalid selected-lobe input capture");
    Dataset bank{mono.width,mono.height,8,std::vector<o::SourceTap>(mono.taps.size()*8)};
    for(std::size_t p=0;p<mono.taps.size();++p)for(unsigned n=0;n<8;++n) {
        auto& tap=bank.taps[p*8+n];tap.identity=mono.taps[p].identity;tap.depth=mono.taps[p].depth;
        if(n==lobe)tap.path=mono.taps[p].path;
        // This input capture authors only its selected physical quadrature.
        // Uncaptured lobes remain invalid, not synthetic colour/path evidence.
    }
    return bank;
}
struct Readback {std::vector<Result> results;std::vector<DomainResult> domains;std::vector<unsigned> atlas;Uniform layout;DomainSettings domain_settings;};
Uniform layout(const Dataset& d,unsigned queries,unsigned leaves,unsigned nodes) {
    require(d.width && d.height && d.width<=16384 && d.height<=16384 && std::uint64_t(d.width)*d.height<=65536
        && (d.lobes==1 || d.lobes==8)
        && std::uint64_t(d.width)*d.height*d.lobes==d.taps.size(),"Invalid resident-index dataset");
    Uniform u;u.width=d.width;u.height=d.height;u.lobes=d.lobes;u.query_count=queries;u.leaf_budget=leaves;u.node_budget=nodes;
    unsigned w=(d.width+7)/8,h=(d.height+7)/8;
    while(true) {require(u.level_count<u.levels.size(),"Unbounded feature hierarchy");
        u.levels[u.level_count++]={u.total_nodes,w,h,0};u.total_nodes+=w*h;if(w==1 && h==1)break;w=(w+1)/2;h=(h+1)/2;}
    require(std::uint64_t(u.total_nodes)*32*d.lobes<=UINT32_MAX,"Unbounded resident index storage");return u;
}
std::vector<unsigned> pack(const Dataset& d,unsigned salt) {
    const auto pixels=std::size_t(d.width)*d.height;
    std::vector<unsigned> words(pixels*(64+64*d.lobes)/4,salt);
    for(std::size_t p=0;p<pixels;++p) {
        words[pixels*24/4+p]=d.taps[p*d.lobes].identity;
        words[pixels*28/4+p]=std::bit_cast<unsigned>(float(d.taps[p*d.lobes].depth));
        for(unsigned l=0;l<d.lobes;++l) {
            const auto& tap=d.taps[p*d.lobes+l];
            require(tap.identity==d.taps[p*d.lobes].identity
                && std::bit_cast<unsigned>(float(tap.depth))==words[pixels*28/4+p],"Lobe dataset changed shared primary ownership/depth");
            std::copy(tap.path.begin(),tap.path.end(),words.begin()+std::ptrdiff_t(pixels*64/4+(p*d.lobes+l)*16));
        }
    }
    return words;
}
struct Gpu {
    SDL_GPUDevice* device{};std::array<SDL_GPUComputePipeline*,5> pipelines{};
    SDL_GPUComputePipeline* jet_pipeline{};bool spirv{};
    // Diagnostic-only A/B shader selection. No player/history/source/root input
    // changes, and the ordinary embedded pipeline remains the default.
    std::vector<unsigned char> external_jet_spirv;
    std::array<SDL_GPUBuffer*,7> buffers{};std::array<SDL_GPUTransferBuffer*,7> transfers{};
    std::optional<Uniform> resident;
    unsigned domain_queries{};
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> validation;
#endif
    ~Gpu(){destroy();SDL_Quit();}
    void destroy() noexcept {clear();for(auto*& p:pipelines){if(p)SDL_ReleaseGPUComputePipeline(device,p);p=nullptr;}
        if(jet_pipeline)SDL_ReleaseGPUComputePipeline(device,jet_pipeline);
        jet_pipeline=nullptr;
        if(device)SDL_DestroyGPUDevice(device);
        device=nullptr;}
    void clear(){resident.reset();domain_queries=0;if(!device)return;SDL_WaitForGPUIdle(device);for(auto*& b:buffers){if(b)SDL_ReleaseGPUBuffer(device,b);b=nullptr;}
        for(auto*& t:transfers){if(t)SDL_ReleaseGPUTransferBuffer(device,t);t=nullptr;}}
    void load_jet_spirv(const char* path) {
        require(spirv && !jet_pipeline,"External differential SPIR-V requires an unused Vulkan pipeline");
        std::ifstream stream(path,std::ios::binary|std::ios::ate);
        require(bool(stream),"Cannot open external differential SPIR-V");
        const std::streamoff bytes=stream.tellg();
        require(bytes>=20 && bytes<=16*1024*1024 && bytes%4==0,"Truncated/oversize external differential SPIR-V");
        std::vector<unsigned char> shader(static_cast<std::size_t>(bytes));
        stream.seekg(0);stream.read(reinterpret_cast<char*>(shader.data()),static_cast<std::streamsize>(bytes));
        require(bool(stream),"Incomplete external differential SPIR-V read");
        std::array<std::uint32_t,5> header{};std::memcpy(header.data(),shader.data(),20);
        require(header[0]==0x07230203U && header[1]>=0x00010000U && header[1]<=0x00010500U
            && header[3]>0 && header[3]<=1048576 && !header[4],"Invalid/incompatible external differential SPIR-V header");
        external_jet_spirv=std::move(shader);
        std::cout<<"Diagnostic external differential SPIR-V bytes="<<external_jet_spirv.size()<<std::endl;
    }
    explicit Gpu(const char* backend,bool low_power,bool jets_only=false) {
        try {
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());device=create_reflected_curved_device(backend,low_power);require(device,SDL_GetError());
        require_reflected_curved_precision(device,backend);const bool sp=std::string_view(backend)=="vulkan";spirv=sp;
#if defined(_WIN32)
        if(!sp) {
            auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device),STARFOX_SDL_D3D12_DEVICE,nullptr));
            require(native,"Resident index missing native D3D12 device");
            require(SUCCEEDED(native->QueryInterface(IID_ID3D12InfoQueue,reinterpret_cast<void**>(validation.GetAddressOf()))),"Resident index D3D12 validation unavailable");
            require(validation->GetNumMessagesDiscardedByMessageCountLimit()==0,"D3D12 validation overflow before index staging");
            D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
            D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
            require(SUCCEEDED(validation->PushStorageFilter(&filter)),"Resident index storage filter unavailable");
        }
#endif
        std::cout<<"Resident feature index GPU="<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")
            <<" backend="<<backend<<std::endl;
        const unsigned char* code[5]{sp?reflected_visibility_tiles_spirv:reflected_visibility_tiles_dxil,
            sp?reflected_visibility_reduce_spirv:reflected_visibility_reduce_dxil,sp?reflected_visibility_query_spirv:reflected_visibility_query_dxil,
            sp?reflected_visibility_domains_spirv:reflected_visibility_domains_dxil,
            sp?reflected_visibility_optical_spirv:reflected_visibility_optical_dxil};
        const std::size_t sizes[5]{sp?sizeof(reflected_visibility_tiles_spirv):sizeof(reflected_visibility_tiles_dxil),
            sp?sizeof(reflected_visibility_reduce_spirv):sizeof(reflected_visibility_reduce_dxil),sp?sizeof(reflected_visibility_query_spirv):sizeof(reflected_visibility_query_dxil),
            sp?sizeof(reflected_visibility_domains_spirv):sizeof(reflected_visibility_domains_dxil),
            sp?sizeof(reflected_visibility_optical_spirv):sizeof(reflected_visibility_optical_dxil)};
        const char* names[5]{"feature_tiles_main","feature_reduce_main","feature_query_main","feature_domains_main","feature_optical_main"};
        for(unsigned n=0;n<5;++n) {
            if(jets_only)break;
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint=names[n];info.code=code[n];info.code_size=sizes[n];
            info.format=sp?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;info.num_readonly_storage_buffers=n==1?0:n==0?1:n==2?2:3;
            info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=n>=3?2:1;
            info.threadcount_x=n==2 || n==4?64:8;info.threadcount_y=n==2 || n==4?1:8;info.threadcount_z=1;
            std::cout<<"Creating resident diagnostic pipeline "<<names[n]<<std::endl;
            pipelines[n]=SDL_CreateGPUComputePipeline(device,&info);require(pipelines[n],SDL_GetError());
            std::cout<<"Created resident diagnostic pipeline "<<names[n]<<std::endl;
        }
        }catch(...){destroy();SDL_Quit();throw;}
    }
    void finish() {
        destroy();
#if defined(_WIN32)
        if(validation) {
            require(validation->GetNumMessagesDiscardedByMessageCountLimit()==0,"Resident index discarded D3D12 validation messages");
            D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
            D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
            require(SUCCEEDED(validation->PushRetrievalFilter(&filter)),"Resident index retrieval filter unavailable");
            struct Pop {ID3D12InfoQueue* queue;~Pop(){queue->PopRetrievalFilter();}} pop{validation.Get()};
            const auto count=validation->GetNumStoredMessagesAllowedByRetrievalFilter();
            for(UINT64 n=0;n<std::min<UINT64>(count,8);++n) {
                SIZE_T bytes{};require(SUCCEEDED(validation->GetMessage(n,nullptr,&bytes)),"D3D12 validation message size unavailable");
                std::vector<unsigned char> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                require(SUCCEEDED(validation->GetMessage(n,message,&bytes)),"D3D12 validation message unavailable");std::cerr<<message->pDescription<<'\n';
            }
            require(count==0,"Resident index emitted D3D12 error/corruption messages");
            std::cout<<"Resident index: zero stored critical/discarded D3D12 messages through owned resources and SDL-device teardown.\n";
        }
#endif
    }
    Readback run(const Dataset& d,const std::vector<Query>& queries,unsigned salt=0x2a2a2a2aU,unsigned leaves=64,unsigned nodes=4096,
        unsigned region_budget=region_capacity,unsigned support_budget=16384) {
        clear();require(!queries.empty() && queries.size()<=1024,"Unbounded resident query batch");
        auto u=layout(d,unsigned(queries.size()),leaves,nodes);const auto data=pack(d,salt);
        const std::uint64_t source_bytes=data.size()*4,atlas_bytes=std::uint64_t(u.total_nodes)*d.lobes*32,
            query_bytes=queries.size()*sizeof(Query),output_bytes=queries.size()*sizeof(Result),domain_bytes=queries.size()*sizeof(DomainResult);
        require(source_bytes<=UINT32_MAX && query_bytes<=UINT32_MAX && output_bytes<=UINT32_MAX && domain_bytes<=UINT32_MAX,"Unbounded native buffer ABI");
        const unsigned sizes[5]{unsigned(source_bytes),unsigned(atlas_bytes),unsigned(query_bytes),unsigned(output_bytes),unsigned(domain_bytes)};
        for(unsigned n=0;n<5;++n) {
            SDL_GPUBufferCreateInfo b{};b.size=sizes[n];b.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
            buffers[n]=SDL_CreateGPUBuffer(device,&b);require(buffers[n],SDL_GetError());
            SDL_GPUTransferBufferCreateInfo t{};t.size=sizes[n];t.usage=n==0 || n==2?SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD:SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            transfers[n]=SDL_CreateGPUTransferBuffer(device,&t);require(transfers[n],SDL_GetError());
        }
        for(unsigned n:{0U,2U}) {auto* p=SDL_MapGPUTransferBuffer(device,transfers[n],false);require(p,SDL_GetError());
            std::memcpy(p,n==0?static_cast<const void*>(data.data()):static_cast<const void*>(queries.data()),sizes[n]);SDL_UnmapGPUTransferBuffer(device,transfers[n]);}
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        struct Cancel {SDL_GPUCommandBuffer* command;~Cancel(){if(command)SDL_CancelGPUCommandBuffer(command);}} cancel{command};
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        for(unsigned n:{0U,2U}) {const SDL_GPUTransferBufferLocation from{transfers[n],0};const SDL_GPUBufferRegion to{buffers[n],0,sizes[n]};
            SDL_UploadToGPUBuffer(copy,&from,&to,false);}SDL_EndGPUCopyPass(copy);
        for(unsigned level=0;level<u.level_count;++level) {
            u.work_level=level;SDL_GPUStorageBufferReadWriteBinding rw{};rw.buffer=buffers[1];
            auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&rw,1);require(pass,SDL_GetError());
            SDL_BindGPUComputePipeline(pass,pipelines[level?1:0]);if(!level)SDL_BindGPUComputeStorageBuffers(pass,0,&buffers[0],1);
            SDL_PushGPUComputeUniformData(command,0,&u,sizeof(u));
            SDL_DispatchGPUCompute(pass,level?(u.levels[level][1]+7)/8:u.levels[0][1],level?(u.levels[level][2]+7)/8:u.levels[0][2],d.lobes);
            SDL_EndGPUComputePass(pass);
        }
        SDL_GPUStorageBufferReadWriteBinding rw{};rw.buffer=buffers[3];auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&rw,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipelines[2]);SDL_GPUBuffer* read[]{buffers[1],buffers[2]};SDL_BindGPUComputeStorageBuffers(pass,0,read,2);
        SDL_PushGPUComputeUniformData(command,0,&u,sizeof(u));SDL_DispatchGPUCompute(pass,(u.query_count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        const DomainSettings settings{region_budget,support_budget,region_capacity,0};
        refine_domains(command,u,settings);auto result=download(cancel.command,u,settings);resident=u;return result;
    }
    void refine_domains(SDL_GPUCommandBuffer* command,const Uniform& u,const DomainSettings& settings) {
        domain_queries=u.query_count;
        SDL_GPUStorageBufferReadWriteBinding rw{};rw.buffer=buffers[4];
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&rw,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipelines[3]);SDL_GPUBuffer* read[]{buffers[0],buffers[2],buffers[3]};SDL_BindGPUComputeStorageBuffers(pass,0,read,3);
        SDL_PushGPUComputeUniformData(command,0,&u,sizeof(u));SDL_PushGPUComputeUniformData(command,1,&settings,sizeof(settings));
        SDL_DispatchGPUCompute(pass,u.query_count,1,1);SDL_EndGPUComputePass(pass);
    }
    Readback download(SDL_GPUCommandBuffer*& command,const Uniform& u,const DomainSettings& settings) {
        const unsigned sizes[5]{0,u.total_nodes*u.lobes*32,0,u.query_count*unsigned(sizeof(Result)),u.query_count*unsigned(sizeof(DomainResult))};
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        for(unsigned n:{1U,3U,4U}) {const SDL_GPUBufferRegion from{buffers[n],0,sizes[n]};const SDL_GPUTransferBufferLocation to{transfers[n],0};
            SDL_DownloadFromGPUBuffer(copy,&from,&to);}SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence,SDL_GetError());
        struct Fence {SDL_GPUDevice* device;SDL_GPUFence* fence;~Fence(){SDL_ReleaseGPUFence(device,fence);}} release{device,fence};
        require(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());
        Readback r;r.layout=u;r.domain_settings=settings;r.results.resize(u.query_count);r.domains.resize(u.query_count);r.atlas.resize(sizes[1]/4);
        for(unsigned n:{1U,3U,4U}) {const auto* p=SDL_MapGPUTransferBuffer(device,transfers[n],false);require(p,SDL_GetError());
            std::memcpy(n==1?static_cast<void*>(r.atlas.data()):n==3?static_cast<void*>(r.results.data()):static_cast<void*>(r.domains.data()),p,sizes[n]);SDL_UnmapGPUTransferBuffer(device,transfers[n]);}
        return r;
    }
    Readback query_held(const std::vector<Query>& queries,unsigned region_budget=region_capacity,unsigned support_budget=16384) {
        require(device && resident && !queries.empty() && queries.size()<=resident->query_count,
            "No live resident index or query capacity");
        auto u=*resident;u.query_count=unsigned(queries.size());const unsigned bytes=u.query_count*unsigned(sizeof(Query));
        auto* mapped=SDL_MapGPUTransferBuffer(device,transfers[2],false);require(mapped,SDL_GetError());
        std::memcpy(mapped,queries.data(),bytes);SDL_UnmapGPUTransferBuffer(device,transfers[2]);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        struct Cancel {SDL_GPUCommandBuffer* command;~Cancel(){if(command)SDL_CancelGPUCommandBuffer(command);}} cancel{command};
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{transfers[2],0};const SDL_GPUBufferRegion to{buffers[2],0,bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        // ONLY new query parameters are uploaded. The old bank and resident
        // index remain untouched; no tile/reduction dispatch or CPU old read.
        SDL_GPUStorageBufferReadWriteBinding rw{};rw.buffer=buffers[3];auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&rw,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipelines[2]);SDL_GPUBuffer* read[]{buffers[1],buffers[2]};SDL_BindGPUComputeStorageBuffers(pass,0,read,2);
        SDL_PushGPUComputeUniformData(command,0,&u,sizeof(u));SDL_DispatchGPUCompute(pass,(u.query_count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        const DomainSettings settings{region_budget,support_budget,region_capacity,0};
        refine_domains(command,u,settings);return download(cancel.command,u,settings);
    }
    std::vector<JetResult> jet_boxes(const std::vector<JetInput>& input,unsigned refinements=0,bool intersection_contract=false) {
        require(device && !input.empty() && input.size()<=1024 && refinements<=2,"Unbounded differential box batch/refinement count");
        if(!jet_pipeline) {
#if defined(STARFOX_OPTICAL_ARITHMETIC_TRACE_ONLY)
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_arithmetic_main";
            info.code=spirv?reflected_visibility_arithmetic_spirv:reflected_visibility_arithmetic_dxil;
            info.code_size=spirv?sizeof(reflected_visibility_arithmetic_spirv):sizeof(reflected_visibility_arithmetic_dxil);
#else
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_jets_main";
            info.code=spirv?reflected_visibility_jets_spirv:reflected_visibility_jets_dxil;
            info.code_size=spirv?sizeof(reflected_visibility_jets_spirv):sizeof(reflected_visibility_jets_dxil);
            if(!external_jet_spirv.empty()) {
                info.code=external_jet_spirv.data();info.code_size=external_jet_spirv.size();
            }
#endif
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.num_readonly_storage_buffers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            std::cout<<"Creating complete optical differential pipeline"<<std::endl;
            jet_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(jet_pipeline,SDL_GetError());
            std::cout<<"Created complete optical differential pipeline"<<std::endl;
        }
        const unsigned sizes[]{unsigned(input.size()*sizeof(JetInput)),unsigned(input.size()*sizeof(JetResult))};
        struct Scratch {
            SDL_GPUDevice* device;std::array<SDL_GPUBuffer*,2> buffers{};std::array<SDL_GPUTransferBuffer*,2> transfers{};
            ~Scratch(){for(auto* b:buffers)if(b)SDL_ReleaseGPUBuffer(device,b);for(auto* t:transfers)if(t)SDL_ReleaseGPUTransferBuffer(device,t);}
        } scratch{device};
        for(unsigned n=0;n<2;++n) {
            SDL_GPUBufferCreateInfo b{};b.size=sizes[n];b.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
            scratch.buffers[n]=SDL_CreateGPUBuffer(device,&b);require(scratch.buffers[n],SDL_GetError());
            SDL_GPUTransferBufferCreateInfo t{};t.size=sizes[n];t.usage=n?SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD:SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            scratch.transfers[n]=SDL_CreateGPUTransferBuffer(device,&t);require(scratch.transfers[n],SDL_GetError());
        }
        auto* mapped=SDL_MapGPUTransferBuffer(device,scratch.transfers[0],false);require(mapped,SDL_GetError());
        std::memcpy(mapped,input.data(),sizes[0]);SDL_UnmapGPUTransferBuffer(device,scratch.transfers[0]);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        struct Cancel {SDL_GPUCommandBuffer* command;~Cancel(){if(command)SDL_CancelGPUCommandBuffer(command);}} cancel{command};
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{scratch.transfers[0],0};const SDL_GPUBufferRegion to{scratch.buffers[0],0,sizes[0]};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        SDL_GPUStorageBufferReadWriteBinding output{scratch.buffers[1],false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,jet_pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,&scratch.buffers[0],1);
        const std::array<unsigned,4> settings{unsigned(input.size()),refinements,unsigned(intersection_contract),0};SDL_PushGPUComputeUniformData(command,0,settings.data(),sizeof(settings));
        SDL_DispatchGPUCompute(pass,(settings[0]+63)/64,1,1);SDL_EndGPUComputePass(pass);
        copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        const SDL_GPUBufferRegion result{scratch.buffers[1],0,sizes[1]};const SDL_GPUTransferBufferLocation destination{scratch.transfers[1],0};
        SDL_DownloadFromGPUBuffer(copy,&result,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);cancel.command=nullptr;require(fence,SDL_GetError());
        struct Fence {SDL_GPUDevice* device;SDL_GPUFence* fence;~Fence(){SDL_ReleaseGPUFence(device,fence);}} release{device,fence};
        require(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());
        std::vector<JetResult> result_words(input.size());mapped=SDL_MapGPUTransferBuffer(device,scratch.transfers[1],false);require(mapped,SDL_GetError());
        std::memcpy(result_words.data(),mapped,sizes[1]);SDL_UnmapGPUTransferBuffer(device,scratch.transfers[1]);return result_words;
    }
    std::vector<OpticalResult> optical_held(const std::vector<OpticalFrame>& frames,OpticalSettings settings={}) {
        require(device && resident && frames.size()==domain_queries && !frames.empty(),"No current GPU support batch for optical refinement");
        SDL_WaitForGPUIdle(device);
        const unsigned sizes[2]{unsigned(frames.size()*sizeof(OpticalFrame)),unsigned(frames.size()*region_capacity*sizeof(OpticalResult))};
        try {
        for(unsigned n=5;n<7;++n) {
            if(buffers[n])SDL_ReleaseGPUBuffer(device,buffers[n]);
            buffers[n]=nullptr;
            if(transfers[n])SDL_ReleaseGPUTransferBuffer(device,transfers[n]);
            transfers[n]=nullptr;
            SDL_GPUBufferCreateInfo b{};b.size=sizes[n-5];b.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
            buffers[n]=SDL_CreateGPUBuffer(device,&b);require(buffers[n],SDL_GetError());
            SDL_GPUTransferBufferCreateInfo t{};t.size=b.size;t.usage=n==5?SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD:SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            transfers[n]=SDL_CreateGPUTransferBuffer(device,&t);require(transfers[n],SDL_GetError());
        }
        auto* mapped=SDL_MapGPUTransferBuffer(device,transfers[5],false);require(mapped,SDL_GetError());
        std::memcpy(mapped,frames.data(),sizes[0]);SDL_UnmapGPUTransferBuffer(device,transfers[5]);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        struct Cancel {SDL_GPUCommandBuffer* command;~Cancel(){if(command)SDL_CancelGPUCommandBuffer(command);}} cancel{command};
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{transfers[5],0};const SDL_GPUBufferRegion to{buffers[5],0,sizes[0]};SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        SDL_GPUStorageBufferReadWriteBinding rw{};rw.buffer=buffers[6];auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&rw,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipelines[4]);SDL_GPUBuffer* read[]{buffers[4],buffers[5],buffers[2]};SDL_BindGPUComputeStorageBuffers(pass,0,read,3);
        auto u=*resident;u.query_count=domain_queries;SDL_PushGPUComputeUniformData(command,0,&u,sizeof(u));SDL_PushGPUComputeUniformData(command,1,&settings,sizeof(settings));
        SDL_DispatchGPUCompute(pass,region_capacity,domain_queries,1);SDL_EndGPUComputePass(pass);
        copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());const SDL_GPUBufferRegion output{buffers[6],0,sizes[1]};const SDL_GPUTransferBufferLocation destination{transfers[6],0};
        SDL_DownloadFromGPUBuffer(copy,&output,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);cancel.command=nullptr;
        if(!fence)throw std::runtime_error(std::string("Optical GPU submission failed: ")+SDL_GetError());
        struct Fence {SDL_GPUDevice* device;SDL_GPUFence* fence;~Fence(){SDL_ReleaseGPUFence(device,fence);}} release{device,fence};
        if(!SDL_WaitForGPUFences(device,true,&fence,1))throw std::runtime_error(std::string("Optical GPU fence failed: ")+SDL_GetError());
        std::vector<OpticalResult> results(frames.size()*region_capacity);
        const auto* p=SDL_MapGPUTransferBuffer(device,transfers[6],false);require(p,SDL_GetError());std::memcpy(results.data(),p,sizes[1]);SDL_UnmapGPUTransferBuffer(device,transfers[6]);return results;
        }catch(...){clear();throw;}
    }
};
Query old_query(unsigned primary,std::span<const unsigned,16> record,const o::Inputs& inputs,unsigned lobe) {
    Query q;q.primary=primary;q.lobe=lobe;std::copy_n(record.begin(),4,q.mirrors.begin());q.control=record[4];q.terminal=record[5];
    if(primary!=0xfffffffdU)q.primary=inputs.mapping.at(primary);
    const unsigned hops=q.control&7U,mask=(q.control>>4)&15U,kind=q.control>>8;
    require(hops<=4 && (mask>>hops)==0 && kind>=1 && kind<=3,"Unsupported query path control");
    for(unsigned h=0;h<hops;++h)if(!(mask&(1U<<h)))q.mirrors[h]=inputs.mapping.at(q.mirrors[h]);
    o::V target;for(unsigned c=0;c<3;++c)target[c]=std::bit_cast<float>(record[6+c]);
    require(o::valid_feature(target,kind),"Malformed current query feature");
    if(kind==1)q.terminal=inputs.mapping.at(q.terminal);else target=o::old_environment_direction(inputs,target);
    for(unsigned c=0;c<3;++c)q.feature[c]=float(target[c]);
    return q;
}
void contains_domains(const Result& result,const Uniform& u,std::span<const o::VisibilityRegion> domains) {
    require(result.status==0 && result.count<=u.leaf_budget && result.visited<=u.node_budget,"Incomplete resident query was treated as a usable result");
    for(unsigned n=0;n<result.count;++n) {
        require(result.tiles[n]<u.levels[0][1]*u.levels[0][2],"Resident query returned an out-of-bounds tile");
        require(std::find(result.tiles.begin(),result.tiles.begin()+n,result.tiles[n])==result.tiles.begin()+n,"Resident traversal repeated a leaf");
    }
    for(const auto& region:domains) {
        const unsigned tile=region.y/8*u.levels[0][1]+region.x/8;
        require(std::find(result.tiles.begin(),result.tiles.begin()+result.count,tile)!=result.tiles.begin()+result.count,
            "Resident index pruned an independently necessary visible source region");
    }
}
std::vector<Region> ordered_regions(const DomainResult& result) {
    require(result.status==0 && result.count<=region_capacity,"Incomplete GPU refinement was treated as usable source regions");
    std::vector<Region> values(result.regions.begin(),result.regions.begin()+result.count);
    std::sort(values.begin(),values.end(),[](const auto& a,const auto& b){return a.support<b.support;});return values;
}
void contains_domains(const Readback& readback,unsigned query,std::span<const o::VisibilityRegion> expected) {
    contains_domains(readback.results.at(query),readback.layout,expected);
    const auto& result=readback.domains.at(query);
    if(result.status)std::cerr<<"GPU refinement refusal: query="<<query<<" status="<<result.status<<" regions="<<result.count
        <<" work="<<result.visited<<" arithmetic-bits="<<result.unused<<" expected-regions="<<expected.size()<<'\n';
    const auto regions=ordered_regions(result);
    require(result.count<=readback.domain_settings.region_budget && result.visited<=readback.domain_settings.support_budget,
        "GPU refinement ignored its output/work budget");
    for(std::size_t n=0;n<regions.size();++n) {
        const auto& r=regions[n];const auto& s=r.support;
        require(s[2]<=1 && s[3]<=1 && s[0]+s[2]<readback.layout.width && s[1]+s[3]<readback.layout.height,
            "GPU refinement returned an out-of-bounds source support");
        require(!n || s!=regions[n-1].support,"GPU refinement repeated a source support");
        for(unsigned c=0;c<2;++c) {
            require(std::isfinite(r.bounds[c]) && std::isfinite(r.bounds[c+2]) && std::isfinite(r.initializer[c])
                && double(r.bounds[c])>=double(s[c]) && double(r.bounds[c+2])<=double(s[c]+s[c+2]) && r.bounds[c]<=r.bounds[c+2],
                "GPU refinement returned malformed source bounds");
            const double storage_error=2*std::numeric_limits<float>::epsilon()*std::max(1.F,r.initializer[c]);
            require(r.initializer[c]>=double(r.bounds[c])+.5-storage_error && r.initializer[c]<=double(r.bounds[c+2])+.5+storage_error,
                "GPU source initializer lies outside its emitted enclosure");
        }
    }
    for(const auto& domain:expected) {
        const auto found=std::find_if(regions.begin(),regions.end(),[&](const auto& r){return r.support==std::array<unsigned,4>{domain.x,domain.y,domain.span_x,domain.span_y};});
        require(found!=regions.end(),"GPU refinement omitted an independently necessary source support");
        for(unsigned c=0;c<2;++c)require(found->bounds[c]<=domain.minimum[c]+1.e-10 && found->bounds[c+2]>=domain.maximum[c]-1.e-10,
            "GPU source bounds cut away an independent necessary half-plane region");
    }
}
void synthetic_checks(Gpu& gpu) {
    Dataset d;d.width=17;d.height=9;d.lobes=8;d.taps.resize(d.width*d.height*d.lobes);
    o::Inputs input;input.liquid.width=d.width;input.liquid.height=d.height;input.previous.resize(101);input.mapping.resize(101);
    std::iota(input.mapping.begin(),input.mapping.end(),0);
    for(unsigned y=0;y<d.height;++y)for(unsigned x=0;x<d.width;++x)for(unsigned l=0;l<d.lobes;++l) {
        auto& t=d.taps[(y*d.width+x)*d.lobes+l];t.identity=100;t.depth=20;std::fill_n(t.path.begin(),4,UINT32_MAX);
        t.path[4]=256;t.path[5]=l;t.path[6]=std::bit_cast<unsigned>(float((x+.5)/d.width*.35));
        t.path[7]=std::bit_cast<unsigned>(float((y+.5)/d.height*.35));
    }
    std::vector<Query> queries;std::vector<std::vector<o::VisibilityRegion>> expected;
    for(unsigned l:{0U,7U})for(const auto xy:{std::array<unsigned,2>{0,0},{16,8},{8,4},{5,7}}) {
        const auto& tap=d.taps[(xy[1]*d.width+xy[0])*d.lobes+l];std::array<unsigned,16> record{};std::copy(tap.path.begin(),tap.path.end(),record.begin());
        queries.push_back(old_query(100,record,input,l));
        expected.push_back(o::old_visibility_domains(input,100,record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[(y*d.width+x)*d.lobes+l];}));
        require(!expected.back().empty(),"Synthetic resident test lost its necessary source regions");
    }
    const auto r=gpu.run(d,queries);
    for(unsigned n=0;n<queries.size();++n)contains_domains(r,n,expected[n]);
    auto held_queries=queries;std::reverse(held_queries.begin(),held_queries.end());const auto held=gpu.query_held(held_queries);
    require(held.atlas==r.atlas,"Held queries rewrote the resident accepted-old index");
    for(unsigned n=0;n<queries.size();++n) {
        contains_domains(held,n,expected[queries.size()-1-n]);
        require(ordered_regions(held.domains[n])==ordered_regions(r.domains[queries.size()-1-n]),"Held GPU query changed necessary source enclosures");
    }
    const auto changed_rgb=gpu.run(d,queries,0xfacade01U);require(changed_rgb.atlas==r.atlas,"Resident index incorporated RGB/response/base or padding words");
    for(unsigned n=0;n<queries.size();++n) {
        contains_domains(changed_rgb,n,expected[n]);
        require(ordered_regions(changed_rgb.domains[n])==ordered_regions(r.domains[n]),"GPU refinement incorporated RGB/response/base/padding words");
    }
    auto invalid=queries;invalid[0].primary=UINT32_MAX;invalid[1].lobe=8;invalid[2].feature[0]=NAN;
    invalid[3].control=256+5;invalid[4].control=256+16;invalid[5].control=0;
    const auto bad=gpu.run(d,invalid);for(unsigned n=0;n<6;++n)require(bad.results[n].status==4 && !bad.results[n].count,"Malformed query acquired usable leaves");
    for(unsigned n=0;n<6;++n)require(bad.domains[n].status==4 && !bad.domains[n].count,"Malformed leaf query acquired usable source regions");
    // Dense equal-feature case must reject partial results when either budget
    // expires. All lobes remain independent, including empty/invalid old data.
    for(auto& t:d.taps) {t.path[6]=std::bit_cast<unsigned>(.2F);t.path[7]=std::bit_cast<unsigned>(.3F);}
    Query dense=queries[0];dense.feature={.2F,.3F,0};
    const auto leaf_stop=gpu.run(d,{dense},0,1);require(leaf_stop.results[0].status==1 && leaf_stop.results[0].count==1,"Leaf overflow falsely certified a partial index traversal");
    require(leaf_stop.domains[0].status==1 && !leaf_stop.domains[0].count,"Incomplete leaf traversal acquired source regions");
    const auto node_stop=gpu.run(d,{dense},0,64,1);require(node_stop.results[0].status==2 && node_stop.results[0].visited==1,"Node overflow falsely certified a partial index traversal");
    require(node_stop.domains[0].status==2 && !node_stop.domains[0].count,"Incomplete node traversal acquired source regions");
    const auto region_stop=gpu.run(d,{dense},0,64,4096,1);
    require(!region_stop.results[0].status && region_stop.domains[0].status==16 && region_stop.domains[0].count==1,"Region overflow did not invalidate partial source enclosures");
    const auto work_stop=gpu.run(d,{dense},0,64,4096,region_capacity,1);
    require(!work_stop.results[0].status && work_stop.domains[0].status==32 && work_stop.domains[0].visited==1,"Source-support work overflow was accepted");
    const auto no_regions=gpu.query_held({dense},0),too_many_regions=gpu.query_held({dense},region_capacity+1),
        no_work=gpu.query_held({dense},region_capacity,0),too_much_work=gpu.query_held({dense},region_capacity,16385);
    for(const auto* refused:{&region_stop.domains[0],&work_stop.domains[0],&no_regions.domains[0],&too_many_regions.domains[0],&no_work.domains[0],&too_much_work.domains[0]}) {
        bool rejected=false;try{(void)ordered_regions(*refused);}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Incomplete/invalid source refinement was decoded as usable source regions");
    }
    for(const auto* invalid_budget:{&no_regions.domains[0],&too_many_regions.domains[0],&no_work.domains[0],&too_much_work.domains[0]})
        require(invalid_budget->status==4 && !invalid_budget->count && !invalid_budget->visited,"Malformed refinement budget was not refused before source work");
    for(auto& t:d.taps)t.identity=UINT32_MAX;
    const auto empty=gpu.run(d,{dense});require(empty.results[0].status==0 && empty.results[0].count==0,"Empty bank retained stale visible feature tiles");
    require(!empty.domains[0].status && !empty.domains[0].count,"Empty replacement retained stale source regions");
    std::cout<<"GPU resident index: odd extents, dyadic edge children, eight separate lobes, necessary source regions, held query-only reuse, RGB/base independence, malformed query, leaf/node budget refusal and empty replacement passed; "
        <<r.atlas.size()*4<<" index bytes for "<<d.width*d.height*d.lobes<<" old lobe records. NOT colour-history/optical-uniqueness acceptance.\n";
    std::cout<<"GPU source refinement: independent support/bounds enclosure, bit-exact held/RGB-independent regions, upstream failure propagation and region/work-budget refusal passed.\n";
}
void refinement_checks(Gpu& gpu) {
    Dataset d;d.width=29;d.height=21;d.taps.resize(d.width*d.height);
    o::Inputs input;input.liquid.width=d.width;input.liquid.height=d.height;input.previous.resize(4);input.mapping={2,0,3,1};
    std::array<unsigned,16> record{};std::fill_n(record.begin(),4,UINT32_MAX);
    record[0]=3;record[4]=256+1;record[5]=1;record[6]=std::bit_cast<unsigned>(.2F);record[7]=std::bit_cast<unsigned>(.3F);
    auto query=old_query(0,record,input,0);
    require(query.primary==2 && query.mirrors[0]==1 && query.terminal==0,"Refinement fixture did not remap finite path IDs");
    for(unsigned y=0;y<d.height;++y)for(unsigned x=0;x<d.width;++x) {
        auto& tap=d.taps[y*d.width+x];tap.identity=query.primary;tap.depth=(x==7 || x==8) && (y==7 || y==8)?20:NAN;
        std::copy(query.mirrors.begin(),query.mirrors.end(),tap.path.begin());tap.path[4]=query.control;tap.path[5]=query.terminal;
        tap.path[6]=std::bit_cast<unsigned>(.19F+.02F*float(int(x)-7));tap.path[7]=std::bit_cast<unsigned>(.29F+.02F*float(int(y)-7));
    }
    const auto expected=o::old_visibility_domains(input,0,record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[y*d.width+x];});
    require(expected.size()==1 && expected[0].x==7 && expected[0].y==7 && expected[0].span_x && expected[0].span_y
        && expected[0].minimum[0]>7.3 && expected[0].maximum[0]<7.7 && expected[0].minimum[1]>7.3 && expected[0].maximum[1]<7.7,
        "Remapped tile-corner fixture lost its two-axis half-plane intersection");
    const auto result=gpu.run(d,{query});contains_domains(result,0,expected);
    require(result.domains[0].count==1,"GPU refinement failed to narrow the remapped tile-corner support");
    // Return to an exact authored feature: point/edge/cell supports all matter
    // when the remaining nonzero-weight interpolation support changes.
    record[6]=d.taps[7*d.width+7].path[6];record[7]=d.taps[7*d.width+7].path[7];query=old_query(0,record,input,0);
    const auto boundaries=o::old_visibility_domains(input,0,record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[y*d.width+x];});
    std::array<bool,4> kinds{};for(const auto& region:boundaries)kinds[region.span_x+2*region.span_y]=true;
    require(std::all_of(kinds.begin(),kinds.end(),[](bool value){return value;}),"Refinement fixture did not exercise all four support dimensions");
    const auto held=gpu.query_held({query});contains_domains(held,0,boundaries);
    std::cout<<"GPU source refinement: remapped receiver/hop/terminal, two-axis clipping across a four-tile corner, invalid-depth gradient halo and held point/horizontal/vertical/cell supports passed.\n";
}
void curved_feature_checks(Gpu& gpu) {
    Dataset d;d.width=35;d.height=27;d.lobes=8;d.taps.resize(std::size_t(d.width)*d.height*d.lobes);
    o::Inputs input;input.liquid.width=d.width;input.liquid.height=d.height;input.previous.resize(101);input.mapping.resize(101);
    std::iota(input.mapping.begin(),input.mapping.end(),0);
    std::array<std::array<unsigned,16>,8> records{};
    for(unsigned l=0;l<8;++l) {
        auto& record=records[l];std::fill_n(record.begin(),4,UINT32_MAX);const unsigned hops=l%5,kind=l<4?1:l<6?2:3;
        const unsigned mask=kind!=1 && hops>1?2:0;record[4]=kind*256+mask*16+hops;record[5]=kind==1?l:UINT32_MAX;
        for(unsigned h=0;h<hops;++h)record[h]=(mask&(1U<<h))?0xfffffffdU:l+h+1;
        for(unsigned y=0;y<d.height;++y)for(unsigned x=0;x<d.width;++x) {
            auto& tap=d.taps[(std::size_t(y)*d.width+x)*d.lobes+l];tap.identity=100;tap.depth=20;
            std::copy_n(record.begin(),6,tap.path.begin());
            o::V feature{.05+.006*x+.002*std::sin(.27*x+.19*l),.08+.005*y+.002*std::cos(.23*y+.17*l),0};
            if(kind!=1)feature=o::unit({.001*(double(x)-17)+.0002*std::sin(.27*x+l),.001*(double(y)-13)+.0002*std::cos(.23*y+l),1});
            for(unsigned c=0;c<3;++c)tap.path[c+6]=std::bit_cast<unsigned>(float(feature[c]));
        }
    }
    std::vector<Query> queries;std::vector<std::vector<o::VisibilityRegion>> expected;
    for(unsigned l=0;l<8;++l)for(const auto xy:{std::array<unsigned,2>{7,7},{15,15},{33,25}}) {
        auto record=records[l];o::V feature{};
        constexpr double fx=.313,fy=.719;
        for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
            const auto& tap=d.taps[((std::size_t(xy[1])+dy)*d.width+xy[0]+dx)*d.lobes+l];
            for(unsigned c=0;c<3;++c)feature[c]+=double(std::bit_cast<float>(tap.path[c+6]))*(dx?fx:1-fx)*(dy?fy:1-fy);
        }
        for(unsigned c=0;c<3;++c)record[c+6]=std::bit_cast<unsigned>(float(feature[c]));
        queries.push_back(old_query(100,record,input,l));
        expected.push_back(o::old_visibility_domains(input,100,record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[(std::size_t(y)*d.width+x)*d.lobes+l];}));
        require(!expected.back().empty(),"Nonlinear feature fixture lost all necessary domains");
    }
    const auto result=gpu.run(d,queries);
    for(unsigned n=0;n<queries.size();++n)contains_domains(result,n,expected[n]);
    std::reverse(queries.begin(),queries.end());const auto held=gpu.query_held(queries);
    require(held.atlas==result.atlas,"Nonlinear held queries changed resident old features");
    for(unsigned n=0;n<queries.size();++n) {
        contains_domains(held,n,expected[queries.size()-1-n]);
        require(ordered_regions(held.domains[n])==ordered_regions(result.domains[queries.size()-1-n]),"Nonlinear held query changed GPU source bounds");
    }
    std::cout<<"GPU source refinement: 24 nonlinear-feature queries across all eight lobes, finite/sky/sun features, zero-to-four ordered mixed hops, tile boundaries and partial final leaves passed independent enclosure and held-query checks.\n";
}
void boundary_checks(Gpu& gpu) {
    Dataset d;d.width=33;d.height=17;d.taps.resize(d.width*d.height);
    for(auto& tap:d.taps)tap.identity=UINT32_MAX;
    o::Inputs input;input.liquid.width=d.width;input.liquid.height=d.height;
    auto now=input.current_eye_view,old=now;
    now[12]=500;now[13]=-100;old={0,0,-1,0,0,1,0,0,1,0,0,0,-90,30,1000,1};
    o::eye_views(input,now,old);
    std::array<unsigned,16> record{};std::fill_n(record.begin(),4,UINT32_MAX);
    record[0]=0xfffffffdU;record[4]=512+16+1;record[5]=UINT32_MAX;record[6]=std::bit_cast<unsigned>(1.F);
    const auto query=old_query(0xfffffffdU,record,input,0);
    require(query.feature==std::array<float,3>{0,0,1},"Environment index query used translation or the wrong old eye basis");
    for(const auto xy:{std::array<unsigned,2>{0,0},{7,8},{31,15},{32,16}}) {
        auto& tap=d.taps[xy[1]*d.width+xy[0]];tap.identity=0xfffffffdU;tap.depth=20;
        std::copy_n(record.begin(),6,tap.path.begin());tap.path[8]=std::bit_cast<unsigned>(1.F);
    }
    const auto domains=o::old_visibility_domains(input,0xfffffffdU,record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[y*d.width+x];});
    require(domains.size()==4 && std::all_of(domains.begin(),domains.end(),[](const auto& region){return !region.span_x && !region.span_y;}),
        "Disconnected environment fixture lost its integer-only islands");
    const auto indexed=gpu.run(d,{query});contains_domains(indexed,0,domains);
    require(indexed.results[0].count==4,"Resident environment index lost a disconnected old island");
    const auto held=gpu.query_held({query});require(held.atlas==indexed.atlas,"Environment held query changed index words");
    contains_domains(held,0,domains);
    require(ordered_regions(held.domains[0])==ordered_regions(indexed.domains[0]),"Held environment query changed its source enclosures");
    // A zero-weight horizontal edge straddles two 8x8 leaves. A same-path
    // neighbour with invalid depth still contributes a feature gradient.
    d.width=17;d.height=1;d.taps.assign(d.width,o::SourceTap{});
    for(auto& tap:d.taps)tap.identity=UINT32_MAX;
    input=o::Inputs{};input.liquid.width=d.width;input.liquid.height=d.height;input.previous.resize(2);input.mapping={0,1};
    record={};std::fill_n(record.begin(),4,UINT32_MAX);record[4]=256;record[5]=1;
    record[6]=std::bit_cast<unsigned>(.21F);record[7]=std::bit_cast<unsigned>(.3F);
    for(unsigned x:{6U,7U,8U,9U}) {
        auto& tap=d.taps[x];tap.identity=0;tap.depth=x==6 || x==9?NAN:20;
        std::copy_n(record.begin(),6,tap.path.begin());tap.path[6]=std::bit_cast<unsigned>(x==6 || x==9?.22F:.2F);tap.path[7]=record[7];
    }
    const auto edges=o::old_visibility_domains(input,0,record,[&](unsigned x,unsigned)->std::optional<o::SourceTap>{return d.taps[x];});
    require(edges.size()==1 && edges[0].x==7 && edges[0].span_x==1 && !edges[0].span_y
        && edges[0].minimum[0]>7.3 && edges[0].maximum[0]<7.7,
        "Tile-boundary fixture lost its zero-weight edge");
    const auto edge_result=gpu.run(d,{old_query(0,record,input,0)});contains_domains(edge_result,0,edges);
    // Exercise the complete bounded raw bank, all eight lobes and the last
    // legal texel without making the independent reference read RGB.
    d.width=d.height=256;d.lobes=8;d.taps.assign(std::size_t(d.width)*d.height*d.lobes,o::SourceTap{});
    for(auto& tap:d.taps)tap.identity=UINT32_MAX;
    input.liquid.width=d.width;input.liquid.height=d.height;record[6]=std::bit_cast<unsigned>(.2F);
    for(unsigned pixel:{0U,d.width*d.height-1})for(unsigned l=0;l<d.lobes;++l) {
        auto& tap=d.taps[std::size_t(pixel)*d.lobes+l];tap.identity=0;tap.depth=20;
        if(l==7)std::copy_n(record.begin(),9,tap.path.begin());
    }
    const auto ends=o::old_visibility_domains(input,0,record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[(std::size_t(y)*d.width+x)*d.lobes+7];});
    require(ends.size()==2,"Maximum-bank fixture lost either endpoint");
    const auto maximum=gpu.run(d,{old_query(0,record,input,7)});contains_domains(maximum,0,ends);
    require(maximum.results[0].count==2,"Maximum bank index lost an endpoint or mixed lobes");
    const auto empty_lobe=gpu.query_held({old_query(0,record,input,0)});
    require(!empty_lobe.results[0].status && !empty_lobe.results[0].count && empty_lobe.atlas==maximum.atlas,"Held query leaked another lobe or changed the maximum bank index");
    require(!empty_lobe.domains[0].status && !empty_lobe.domains[0].count,"Empty held lobe acquired another lobe's source regions");
    bool over_capacity=false;try{(void)gpu.query_held({query,query});}catch(const std::runtime_error&){over_capacity=true;}
    require(over_capacity,"Held query exceeded allocated batch capacity");
    ++d.height;bool oversized=false;try{(void)gpu.run(d,{query});}catch(const std::runtime_error&){oversized=true;}
    require(oversized,"Resident diagnostic accepted an unbounded raw bank");
    bool invalidated=false;try{(void)gpu.query_held({query});}catch(const std::runtime_error&){invalidated=true;}
    require(invalidated,"Rejected bank replacement left an old resident index usable");
    --d.height;const auto recovered=gpu.run(d,{old_query(0,record,input,7)});
    contains_domains(recovered,0,ends);
    require(recovered.atlas==maximum.atlas,"Index recovery changed the accepted-old bank");
    std::cout<<"GPU resident index: rotated old-eye environment, disconnected integer islands, tile-crossing zero-weight edge, invalid neighbour depth, 65536-pixel/eight-lobe endpoints, empty held lobe and capacity refusals passed; "
        <<maximum.atlas.size()*4<<" index bytes at the bounded maximum.\n";
}
o::Path old_optical_path(const FullCurvedOwnerReplay& replay) {
    return full_replay_optical_path(replay);
}
OpticalFrame optical_frame(const o::Inputs& input,const o::Path& path) {
    const auto xyz=[](o::V v,float w){return F4{float(v[0]),float(v[1]),float(v[2]),w};};
    OpticalFrame f;f.receiver[0]=xyz(path.receiver.a,1);f.receiver[1]=xyz(path.receiver.b,1);f.receiver[2]=xyz(path.receiver.c,1);
    const auto& l=input.liquid;
    for(unsigned c=0;c<4;++c)f.receiver[3][c]=float(l.projection[c]);
    f.receiver[4]={float(l.width),float(l.height),float(l.near),float(l.far)};
    f.receiver[5]={float(path.roughness),float(path.lobe),0,0};
    for(unsigned h=0;h<4;++h) {f.planes[h][0]=xyz(path.mirrors[h].a,1);f.planes[h][1]=xyz(path.mirrors[h].b,1);f.planes[h][2]=xyz(path.mirrors[h].c,1);}
    f.liquid[0]=xyz(l.point,0);f.liquid[1]=xyz(l.normal,0);
    for(unsigned r=0;r<3;++r)f.liquid[2+r]={float(l.rotation[r*3]),float(l.rotation[r*3+1]),float(l.rotation[r*3+2]),float(l.offset[r])};
    f.liquid[5]=f.receiver[3];f.liquid[6]=f.receiver[4];f.liquid[7]={float(l.time),float(l.material),0,0};
    f.terminal=xyz(path.terminal,path.finite_terminal?1:0);f.control={path.hops,path.liquid_mask,unsigned(path.liquid_primary),path.finite_terminal?1U:2U};
    return f;
}
std::vector<o::Solution> audit_gpu_initializers(const FullCurvedOwnerReplay& replay,const Dataset& data,const Readback& gpu,
    std::span<const o::VisibilityRegion> reference) {
    const auto& input=replay.inputs;const auto& record=replay.record;const auto path=old_optical_path(replay);
    const auto regions=ordered_regions(gpu.domains[0]);
    require(replay.lobe<data.lobes,"Physical quadrature is missing from the input bank");
    const auto read=[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return data.taps[(y*data.width+x)*data.lobes+replay.lobe];};
    const auto inside_gpu=[&](const o::Solution& root){return std::any_of(regions.begin(),regions.end(),[&](const auto& region){
        return root.x>=region.bounds[0]-1.e-10 && root.x<=region.bounds[2]+1.e-10
            && root.y>=region.bounds[1]-1.e-10 && root.y<=region.bounds[3]+1.e-10;});};
    const auto same_root=[](const o::Solution& a,const o::Solution& b){return std::abs(a.x-b.x)<=1./4096 && std::abs(a.y-b.y)<=1./4096;};
    unsigned gpu_converged=0,gpu_visible=0;
    const auto collect=[&](const auto& initializers,bool actual_gpu) {
        std::vector<o::Solution> roots;
        for(const auto& start:initializers) {
            const auto solved=o::solve_forward(input,path,start[0],start[1]);if(!solved.valid)continue;
            if(actual_gpu)++gpu_converged;
            o::Solution root{double(float(solved.x))-.5,double(float(solved.y))-.5,double(float(solved.depth)),solved.error,true,false};
            if(o::source_guard(input,replay.primary,record,replay.roughness,replay.lobe,root,read)!=o::SourceGuard::accepted)continue;
            if(actual_gpu)++gpu_visible;
            require(inside_gpu(root),"GPU necessary source enclosures cut away an independently qualified physical branch");
            require(std::any_of(reference.begin(),reference.end(),[&](const auto& region){return region.contains(root.x,root.y);}),
                "GPU-started physical branch escaped the independent necessary domains");
            if(std::none_of(roots.begin(),roots.end(),[&](const auto& other){return same_root(root,other);}))roots.push_back(root);
        }
        return roots;
    };
    std::vector<std::array<double,2>> cpu_starts,gpu_starts;
    for(const auto& region:reference)cpu_starts.push_back(region.initializer);
    for(const auto& region:regions)gpu_starts.push_back({region.initializer[0],region.initializer[1]});
    const auto cpu_roots=collect(cpu_starts,false),gpu_roots=collect(gpu_starts,true);
    for(const auto& root:cpu_roots)require(std::any_of(gpu_roots.begin(),gpu_roots.end(),[&](const auto& other){return same_root(root,other);}),
        "GPU source initializers lost a branch recovered from independent domain initializers");
    std::cout<<"Independent binary64 forward audit of GPU regions: starts="<<gpu_starts.size()<<" converged="<<gpu_converged
        <<" visible="<<gpu_visible<<" distinct="<<gpu_roots.size()<<" reference-distinct="<<cpu_roots.size()
        <<". Centre-start coverage is NOT a uniqueness certificate; "<<(gpu_roots.size()>1?"ambiguous branch, reuse forbidden":"GPU optical certification still required")<<".\n";
    return gpu_roots;
}
void audit_optical_exclusions(const FullCurvedOwnerReplay& replay,const Readback& gpu,
    const std::vector<OpticalResult>& optics,const std::vector<o::Solution>& roots) {
    require(optics.size()==region_capacity,"Wrong optical output size");const auto& regions=gpu.domains[0];
    unsigned kept=0,excluded=0,visited=0,removed=0,samples=0;const auto path=old_optical_path(replay);
    for(unsigned n=0;n<regions.count;++n) {
        const auto& region=regions.regions[n];const auto& result=optics[n];
        if(result.status)std::cerr<<"Optical region "<<n<<" status="<<result.status<<" visited="<<result.visited<<'\n';
        require(!result.status && result.visited && result.visited<=8192,"Incomplete optical cover treated as a valid exclusion");
        require(result.kept+result.excluded==result.visited,"Invalid/incomplete optical cover accounting");
        if(result.kept)require(std::all_of(result.hull.begin(),result.hull.end(),[](float v){return std::isfinite(v);})
            && result.hull[0]<=result.hull[2] && result.hull[1]<=result.hull[3],"Nonfinite/empty unresolved optical hull");
        const auto retained=[&](double x,double y){return result.kept && x>=result.hull[0] && x<=result.hull[2] && y>=result.hull[1] && y<=result.hull[3];};
        for(const auto& root:roots) {
            if(root.x>=region.bounds[0] && root.x<=region.bounds[2] && root.y>=region.bounds[1] && root.y<=region.bounds[3])
                require(retained(root.x+.5,root.y+.5),"Continuum optical cover removed an independently qualified visible branch");
        }
        // Independent finite samples verify actual accepted angular rays too,
        // not only the centre-start root. This checks the implementation; finite
        // sampling is NOT the basis of the shader's continuum exclusion.
        for(unsigned y=0;y<=32;++y)for(unsigned x=0;x<=32;++x) {
            const double px=double(region.bounds[0])+.5+(double(region.bounds[2])-region.bounds[0])*x/32,
                py=double(region.bounds[1])+.5+(double(region.bounds[3])-region.bounds[1])*y/32;
            const auto ray=o::forward(replay.inputs,path,px,py);const auto error=o::residual(replay.inputs,path,px,py);
            if(ray.valid && error && std::max(std::abs((*error)[0]),std::abs((*error)[1]))<=.002) {
                ++samples;require(retained(px,py),"Optical cover removed an independently angular-qualified point");
            }
        }
        kept+=result.kept;excluded+=result.excluded;visited+=result.visited;removed+=unsigned(!result.kept);
    }
    std::cout<<"GPU optical continuum cover: "<<removed<<'/'<<regions.count<<" entire supports excluded, "<<visited<<" boxes, "<<excluded
        <<" excluded boxes, "<<kept<<" unresolved boxes; "<<roots.size()<<" independent physical root candidates and "<<samples<<" angular-qualified samples retained. No uniqueness/colour reuse accepted.\n";
}
bool optical_complete(const Readback& supports,const std::vector<OpticalResult>& optics) {
    if(optics.size()!=supports.domains.size()*region_capacity)return false;
    for(unsigned q=0;q<supports.domains.size();++q) {
        const auto& domain=supports.domains[q];if(domain.status || domain.count>region_capacity)return false;
        for(unsigned n=0;n<domain.count;++n)if(optics[q*region_capacity+n].status)return false;
    }
    return true;
}
void optical_checks(Gpu& gpu) {
    const std::array<unsigned,3> reciprocals[]{
#define REFLECTED_RECIPROCAL(D,L,H) {D,L,H},
#include "shaders/reflected_optical_reciprocals.inc"
#undef REFLECTED_RECIPROCAL
    };
    static_assert(std::numeric_limits<double>::radix==2 && std::numeric_limits<double>::digits>=53);
    for(const auto& words:reciprocals) {
        const auto lo=std::bit_cast<float>(words[1]),hi=std::bit_cast<float>(words[2]);
        // A 24-bit binary32 endpoint times these <=17-bit integer constants is
        // exact in binary64. No rounded CPU division supplies the certificate.
        require(lo>0 && lo<=hi && double(lo)*words[0]<=1 && double(hi)*words[0]>=1,
            "Fixed optical reciprocal does not enclose the exact rational constant");
    }
    FullCurvedOwnerReplay r;r.width=r.height=9;r.primary=0;
    auto& i=r.inputs;i.current=i.previous={{{-100,-100,10},{100,-100,10},{0,100,10}}};i.mapping={0};
    auto& l=i.liquid;l.width=l.height=9;l.near=1;l.far=100;l.point={0,10,0};l.normal={0,-1,0};l.projection={64,64,4,4};i.current_projection=l.projection;
    r.record[4]=2U<<8;r.record[8]=std::bit_cast<unsigned>(-1.F);
    Dataset d{9,9,1,std::vector<o::SourceTap>(81)};
    const auto path=old_optical_path(r);
    for(unsigned y=0;y<9;++y)for(unsigned x=0;x<9;++x) {
        auto& tap=d.taps[y*9+x];tap.identity=0;tap.depth=(x==3 || x==4) && (y==3 || y==4)?10:0;std::copy_n(r.record.begin(),9,tap.path.begin());
        const auto ray=o::forward(i,path,x+.5,y+.5);require(ray.valid,"Independent flat input-bank ray invalid");
        for(unsigned c=0;c<3;++c)tap.path[6+c]=std::bit_cast<unsigned>(float(ray.outgoing[c]));
    }
    const auto domains=o::old_visibility_domains(i,0,r.record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[y*9+x];});
    const auto q=old_query(0,r.record,i,0);const auto support=gpu.run(d,{q});contains_domains(support,0,domains);
    auto frame=optical_frame(i,old_optical_path(r));const auto good=gpu.optical_held({frame});
    require(optical_complete(support,good),"Flat optical cover failed");
    const auto roots=audit_gpu_initializers(r,d,support,domains);require(roots.size()==1,"Independent flat fixture lost its physical root");
    audit_optical_exclusions(r,support,good,roots);
    unsigned kept=0,excluded=0;for(unsigned n=0;n<support.domains[0].count;++n) {kept+=good[n].kept;excluded+=good[n].excluded;}
    require(kept && excluded,"Optical continuum pass did not separate empty boxes from the root boxes");
    require(gpu.optical_held({frame})==good,"Held optical cover changed without rebuilding source/index/supports");
    for(const auto settings:{OpticalSettings{0,12,128,0},OpticalSettings{8193,12,128,0},OpticalSettings{4096,21,128,0},OpticalSettings{4096,12,127,0},OpticalSettings{1,12,128,0}}) {
        const auto refused=gpu.optical_held({frame},settings);require(!optical_complete(support,refused),"Partial/invalid optical cover accepted by host");
    }
    auto bad=frame;bad.control[0]=1;require(!optical_complete(support,gpu.optical_held({bad})),"Optical frame/control mismatch accepted");
    bad=frame;bad.control[2]=1;require(!optical_complete(support,gpu.optical_held({bad})),"Optical finite/liquid primary ownership mismatch accepted");
    bad=frame;bad.receiver[5][1]=1;require(!optical_complete(support,gpu.optical_held({bad})),"Optical rough-lobe/query mismatch accepted");
    bad=frame;bad.receiver[3][0]+=1;require(!optical_complete(support,gpu.optical_held({bad})),"Optical accepted-eye projection mismatch accepted");
    bad=frame;bad.receiver[4][2]+=.5F;require(!optical_complete(support,gpu.optical_held({bad})),"Optical accepted-eye clip mismatch accepted");
    bad=frame;bad.terminal[3]=1;require(!optical_complete(support,gpu.optical_held({bad})),"Wrong finite/environment terminal accepted");
    bad=frame;bad.liquid[7][0]=std::numeric_limits<float>::quiet_NaN();require(!optical_complete(support,gpu.optical_held({bad})),"Nonfinite old optical frame accepted");
    r.record[8]=std::bit_cast<unsigned>(1.F);for(auto& tap:d.taps)std::copy_n(r.record.begin(),9,tap.path.begin());
    const auto opposite=gpu.run(d,{old_query(0,r.record,i,0)});frame=optical_frame(i,old_optical_path(r));
    const auto away=gpu.optical_held({frame});require(optical_complete(opposite,away),"Opposite optical cover failed");
    for(unsigned n=0;n<opposite.domains[0].count;++n)require(!away[n].kept,"Ray facing away retained an impossible terminal");
    std::cout<<"GPU optical cover: flat finite receiver/environment, continuum root retention, empty supports, opposite terminal, held-bank stability and malformed/budget refusals passed.\n";
}
o::Plane authored_plane(o::V centre,o::V normal,double radius) {
    const auto u=o::unit(o::cross(normal,std::abs(normal[1])<.95?o::V{0,1,0}:o::V{1,0,0})),v=o::cross(normal,u);
    o::Plane p{o::add(centre,o::add(o::scale(u,-2*radius),o::scale(v,-2*radius))),
        o::add(centre,o::add(o::scale(u,2*radius),o::scale(v,-2*radius))),o::add(centre,o::scale(v,3*radius))};
    // Geometry inputs use the actual native binary32 vertex encoding.
    for(auto* vertex:{&p.a,&p.b,&p.c})for(auto& value:*vertex)value=float(value);
    return p;
}
void curved_optical_checks(Gpu& gpu) {
    unsigned cases=0;
    for(unsigned material:{0U,3U})for(unsigned variant=0;variant<10;++variant) {
        FullCurvedOwnerReplay r;r.width=17;r.height=13;r.primary=variant<2?0xfffffffdU:0;r.lobe=variant<2?0:variant-2;r.roughness=variant<2?0:double(float(.15));
        auto& i=r.inputs;auto& l=i.liquid;l.width=r.width;l.height=r.height;l.point={0,8,0};l.normal={0,-1,0};l.near=1;l.far=8000;l.projection={48,48,8.5,6.5};l.time=float(.9);l.material=material;i.current_projection=l.projection;
        o::Path p;p.liquid_primary=variant<2;p.lobe=r.lobe;p.roughness=r.roughness;
        constexpr double sx=7.75,sy=10.75;
        if(material==3) {
            auto flat=l;flat.material=0;const auto base=reflected_liquid_oracle::optical(flat,sx,sy);require(base.valid,"Invalid authored lava base");
            bool placed=false;
            for(int z=-3;z<=3 && !placed;++z)for(int x=-3;x<=3 && !placed;++x) {
                const double seed=reflected_liquid_oracle::hash(x,z),phase=l.time*.14+seed*7-std::floor(l.time*.14+seed*7);
                if(seed>.7 && phase>.2 && phase<.8) {l.offset[0]=float(128*(x+.28+.44*reflected_liquid_oracle::hash(x+19,z))-base.hit[0]);
                    l.offset[2]=float(128*(z+.28+.44*reflected_liquid_oracle::hash(x,z+29))-base.hit[2]);placed=true;}
            }
            require(placed,"Could not author active lava dome input");
        }
        if(!p.liquid_primary) {p.receiver=authored_plane({0,0,40},{0,0,1},300);i.previous.push_back(p.receiver);p.hops=1;p.liquid_mask=1;r.record[0]=0xfffffffcU;}
        if(variant==1) {
            const auto primary=o::forward(i,p,sx,sy);require(primary.valid,"Authored primary liquid ray failed");
            p.mirrors[0]=authored_plane(o::add(primary.origin,o::scale(primary.outgoing,64)),primary.outgoing,300);i.previous.push_back(p.mirrors[0]);
            p.hops=2;p.liquid_mask=2;r.record[0]=0;r.record[1]=0xfffffffcU;
        }
        const auto ray=o::forward(i,p,sx,sy);require(ray.valid,"Authored mixed liquid/finite path failed");
        const auto terminal=authored_plane(o::add(ray.origin,o::scale(ray.outgoing,128)),ray.outgoing,300);const unsigned terminal_id=unsigned(i.previous.size());i.previous.push_back(terminal);
        i.current=i.previous;i.mapping.resize(i.previous.size());std::iota(i.mapping.begin(),i.mapping.end(),0U);
        r.record[4]=256U+p.hops+(p.liquid_mask<<4);r.record[5]=terminal_id;
        const auto normal=o::unit(o::cross(o::sub(terminal.b,terminal.a),o::sub(terminal.c,terminal.a)));
        const double distance=o::dot(o::sub(terminal.a,ray.origin),normal)/o::dot(ray.outgoing,normal);
        const auto bary=o::bary(terminal,o::add(ray.origin,o::scale(ray.outgoing,distance)));
        r.record[6]=std::bit_cast<unsigned>(float(bary[0]));r.record[7]=std::bit_cast<unsigned>(float(bary[1]));
        Dataset d{r.width,r.height,1,std::vector<o::SourceTap>(r.width*r.height)};
        for(unsigned y=0;y<r.height;++y)for(unsigned x=0;x<r.width;++x) {
            auto& tap=d.taps[y*r.width+x];tap.identity=UINT32_MAX;
            const auto actual=o::forward(i,p,x+.5,y+.5);if(!actual.valid)continue;
            const double den=o::dot(actual.outgoing,normal),travel=o::dot(o::sub(terminal.a,actual.origin),normal)/den;
            const auto hit=o::add(actual.origin,o::scale(actual.outgoing,travel));if(std::abs(den)<=1.e-12 || travel<=actual.bias || travel>=65536 || !o::inside(terminal,hit))continue;
            tap.identity=r.primary;std::copy_n(r.record.begin(),9,tap.path.begin());const auto b=o::bary(terminal,hit);
            tap.path[6]=std::bit_cast<unsigned>(float(b[0]));tap.path[7]=std::bit_cast<unsigned>(float(b[1]));tap.depth=(x==7 || x==8) && (y==10 || y==11)?float(actual.depth):0;
        }
        if(!p.liquid_primary)d=selected_lobe_bank(std::move(d),r.lobe);
        const auto domains=o::old_visibility_domains(i,r.primary,r.record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[(y*r.width+x)*d.lobes+r.lobe];});
        require(!domains.empty(),"Authored physical path lost every necessary source support");
        const auto supports=gpu.run(d,{old_query(r.primary,r.record,i,r.lobe)});contains_domains(supports,0,domains);
        auto frame=optical_frame(i,old_optical_path(r));const auto optics=gpu.optical_held({frame});require(optical_complete(supports,optics),"Authored curved optical cover incomplete");
        const auto roots=audit_gpu_initializers(r,d,supports,domains);
        // A direct independent solve starts from the authored ray ONLY on the
        // reference side. Neither this initializer nor its root reaches GPU.
        const auto truth=o::solve_forward(i,old_optical_path(r),sx,sy);require(truth.valid,"Authored optical terminal lost its independent root");
        require(std::any_of(domains.begin(),domains.end(),[&](const auto& domain){return domain.contains(truth.x-.5,truth.y-.5);}),
            "Authored independent physical root escaped every necessary source domain");
        auto all_roots=roots;all_roots.push_back({truth.x-.5,truth.y-.5,truth.depth,truth.error,true,false});
        audit_optical_exclusions(r,supports,optics,all_roots);++cases;
    }
    std::cout<<"GPU optical continuum cover: "<<cases<<" independent water/lava dome cases, liquid primaries, finite/liquid ordered hops and all eight rough lobes retained physical roots and angular-qualified samples.\n";
}
#include "check_reflected_optical_jets.inc"
}
int main(int argc,char** argv)try {
    require(argc>=2 && argc<=7 && (std::string_view(argv[1])=="direct3d12" || std::string_view(argv[1])=="vulkan"),
        "Use direct3d12|vulkan [--low-power] [--jets] [--jet-spirv file] [complete-owner-input-replay.log]");
    bool low_power=false,jets_only=false;const char* replay=nullptr;const char* external_jet=nullptr;
    for(int n=2;n<argc;++n) {
        if(std::string_view(argv[n])=="--low-power" && !low_power)low_power=true;
        else if(std::string_view(argv[n])=="--jets" && !jets_only)jets_only=true;
        else if(std::string_view(argv[n])=="--jet-spirv") {
            require(!external_jet && n+1<argc,"Missing or duplicate external differential SPIR-V");external_jet=argv[++n];
        }
        else {require(!replay && std::string_view(argv[n]).substr(0,2)!="--","Unknown or duplicate resident index option");replay=argv[n];}
    }
#if defined(STARFOX_OPTICAL_ARITHMETIC_TRACE_ONLY)
    require(jets_only && !replay && !external_jet,"Primary arithmetic trace requires --jets, no replay or shader override; it is not the complete verifier");
#endif
    require(!external_jet || (jets_only && std::string_view(argv[1])=="vulkan"),
        "External differential SPIR-V is diagnostic-only: use Vulkan --jets");
    Gpu gpu(argv[1],low_power,jets_only);
    if(external_jet)gpu.load_jet_spirv(external_jet);
    if(jets_only){
#if defined(STARFOX_OPTICAL_ARITHMETIC_TRACE_ONLY)
        const auto failures=jet_arithmetic_checks(gpu);gpu.finish();return failures?1:0;
#else
        jet_checks(gpu);
        if(replay)jet_replay_checks(gpu,load_full_curved_owner_replay(replay));
        gpu.finish();return 0;
#endif
    }
    synthetic_checks(gpu);refinement_checks(gpu);curved_feature_checks(gpu);boundary_checks(gpu);optical_checks(gpu);curved_optical_checks(gpu);
    if(replay) {
        auto r=load_full_curved_owner_replay(replay);Dataset d{r.width,r.height,1,std::move(r.taps)};
        if(r.roughness>0 || r.lobe>0)d=selected_lobe_bank(std::move(d),r.lobe);
        const auto query=old_query(r.primary,r.record,r.inputs,r.lobe);
        const auto domains=o::old_visibility_domains(r.inputs,r.primary,r.record,[&](unsigned x,unsigned y)->std::optional<o::SourceTap>{return d.taps[(y*d.width+x)*d.lobes+r.lobe];});
        require(!domains.empty(),"Actual replay lost its independently necessary source domains");
        const auto result=gpu.run(d,{query});contains_domains(result,0,domains);
        const auto held=gpu.query_held({query});contains_domains(held,0,domains);
        require(ordered_regions(held.domains[0])==ordered_regions(result.domains[0]),"Actual held query changed source enclosures");
        require(held.atlas==result.atlas && held.results[0].count==result.results[0].count
            && held.results[0].visited==result.results[0].visited
            && std::equal(held.results[0].tiles.begin(),held.results[0].tiles.begin()+held.results[0].count,result.results[0].tiles.begin()),
            "Actual held query changed the accepted-old index or candidate traversal");
        std::cout<<"Actual old bank: "<<r.width*r.height<<" pixels, "<<domains.size()<<" necessary regions all retained in "
            <<result.results[0].count<<'/'<<result.layout.levels[0][1]*result.layout.levels[0][2]<<" leaf tiles; "
            <<result.results[0].visited<<'/'<<result.layout.total_nodes<<" nodes visited, "<<result.atlas.size()*4<<" resident index bytes; "
            <<result.domains[0].count<<" GPU source enclosures from "<<result.domains[0].visited<<" bounded support attempts.\n";
        const auto roots=audit_gpu_initializers(r,d,result,domains);auto frame=optical_frame(r.inputs,old_optical_path(r));frame.control[3]=r.record[4]>>8;
        const auto optics=gpu.optical_held({frame});audit_optical_exclusions(r,held,optics,roots);
    }
    gpu.finish();bool released_rejected=false;try{(void)gpu.query_held({Query{}});}catch(const std::runtime_error&){released_rejected=true;}
    require(released_rejected,"Released resident index accepted a new query");
    std::cout<<"Conservative GPU candidate-index/source-region/optical-exclusion staging PASS. No root-uniqueness certificate, RGB reuse or production enablement.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
