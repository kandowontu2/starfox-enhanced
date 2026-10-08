#include "starfox/render/dxr_shadows.hpp"
#include "starfox/render/environment_effects.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/responsive_preparation.hpp"
#include <bit>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#if defined(STARFOX_DXR)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "shadow_dxr_shader.hpp"
#include "shadow_dxr_shadow_shader.hpp"
#include "shadow_dxr_native_shader.hpp"
#include "shadow_dxr_native_water_shader.hpp"
#include "shadow_dxr_models_shader.hpp"
#include "shadow_dxr_native_models_shader.hpp"
#include "shadow_dxr_native_history_shader.hpp"
#include "shadow_dxr_model_lobes_shader.hpp"
#include "shadow_dxr_model_paths_shader.hpp"
#include "shadow_dxr_scene_paths_shader.hpp"
#include "shadow_dxr_curved_water_paths_shader.hpp"
#include "shadow_dxr_curved_lava_paths_shader.hpp"
#include "shadow_dxr_curved_water_receivers_shader.hpp"
#include "shadow_dxr_curved_lava_receivers_shader.hpp"
#include "shadow_dxr_water_history_shader.hpp"
#include "shadow_dxr_lava_history_shader.hpp"
#include "reflection_liquid_history_shader.hpp"
#include "shadow_dxr_stable_reflection_shader.hpp"
#include "shadow_dxr_stable_native_shader.hpp"
#include "shadow_dxr_stable_native_water_shader.hpp"
#include "shadow_dxr_stable_models_shader.hpp"
#include "shadow_dxr_stable_native_models_shader.hpp"
#include <cstring>
#include <atomic>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <mutex>
#endif

namespace starfox::render::shadows {
#if defined(STARFOX_DXR)
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << operation << " failed (0x" << std::hex << static_cast<unsigned long>(result) << ')';
        throw std::runtime_error(message.str());
    }
}
struct Buffer { ComPtr<ID3D12Resource> resource; UINT64 capacity{}; };
struct Float4 { float x{}, y{}, z{}, w{}; };
Float4 floats(Vec3 v) { return {float(v.x),float(v.y),float(v.z),0}; }
struct Constants { Float4 camera,options,point,normal; std::array<Float4,16> lights; Float4 primary_range; };
static_assert(sizeof(Constants)==336);
// Opt-in diagnostics fingerprint only bytes supplied to this ray dispatch.
// Never read a GPU buffer back or confuse a resource address with its contents.
std::uint64_t input_fingerprint(const void* data,std::size_t size) {
    auto value=std::uint64_t{14695981039346656037ULL};
    const auto* bytes=static_cast<const std::uint8_t*>(data);
    for(std::size_t i=0;i<size;++i) value=(value^bytes[i])*1099511628211ULL;
    return value;
}
// Only immutable root signatures and PSOs are shared. The registry owns weak
// references: destroying the last producer must release its actual device,
// shaders and root before unloading the producer's system DLL references.
struct DevicePipelines {
    ComPtr<IUnknown> identity;
    ComPtr<ID3D12RootSignature> root;
    std::vector<std::uint8_t> root_bytes;
    struct Entry {
        const void* shader{};
        std::size_t bytes{};
        UINT node_mask{};
        D3D12_PIPELINE_STATE_FLAGS flags{};
        ComPtr<ID3D12PipelineState> pipeline;
    };
    std::mutex mutex;
    std::vector<Entry> entries;
};
// Prebuild sizes are immutable driver answers, not acceleration structures.
// This cache is independent of the experimental PSO sharing switch. Only the
// fixed opaque float3/one-geometry/array/FAST_TRACE layout below uses it; the
// exact triangle count and vertex stride remain part of its key. Never infer a
// smaller layout's allocation from a larger one, or share mutable AS buffers.
struct DevicePrebuildSizes {
    ComPtr<IUnknown> identity;
    struct Entry {
        std::size_t triangles{};
        UINT stride{};
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
    };
    std::mutex mutex;
    std::vector<Entry> entries;
};
std::shared_ptr<DevicePrebuildSizes> device_prebuild_sizes(ID3D12Device5* device) {
    static std::mutex mutex;
    static std::vector<std::weak_ptr<DevicePrebuildSizes>> registry;
    ComPtr<IUnknown> identity;
    check(device->QueryInterface(IID_IUnknown,
        reinterpret_cast<void**>(identity.GetAddressOf())),"DXR prebuild device identity");
    std::lock_guard lock(mutex);
    for(auto it=registry.begin();it!=registry.end();) {
        if(auto cached=it->lock()) {
            if(cached->identity.Get()==identity.Get()) return cached;
            ++it;
        } else it=registry.erase(it);
    }
    auto cached=std::make_shared<DevicePrebuildSizes>();
    cached->identity=std::move(identity);
    cached->entries.reserve(64);
    registry.push_back(cached);
    return cached;
}
std::shared_ptr<DevicePipelines> device_pipelines(ID3D12Device5* device,ID3DBlob* blob,bool create=true) {
    static std::mutex mutex;
    static std::vector<std::weak_ptr<DevicePipelines>> registry;
    ComPtr<IUnknown> identity;
    check(device->QueryInterface(IID_IUnknown,
        reinterpret_cast<void**>(identity.GetAddressOf())),"DXR cache device identity");
    const auto* bytes=static_cast<const std::uint8_t*>(blob->GetBufferPointer());
    const auto size=blob->GetBufferSize();
    std::unique_lock lock(mutex,std::defer_lock);
    if(create) lock.lock();
    else if(!lock.try_lock()) return {};
    for(auto it=registry.begin();it!=registry.end();) {
        if(auto cached=it->lock()) {
            if(cached->identity.Get()==identity.Get() && cached->root_bytes.size()==size
                && std::memcmp(cached->root_bytes.data(),bytes,size)==0) return cached;
            ++it;
        } else it=registry.erase(it);
    }
    if(!create) return {};
    auto cached=std::make_shared<DevicePipelines>();
    cached->identity=std::move(identity);
    cached->root_bytes.assign(bytes,bytes+size);
    check(device->CreateRootSignature(0,bytes,size,IID_ID3D12RootSignature,
        reinterpret_cast<void**>(cached->root.GetAddressOf())),"DXR cached root signature");
    registry.push_back(cached);
    return cached;
}
}
struct DxrShadows::Impl {
    bool attempted{}, capability_attempted{}, failed{};
    std::optional<std::array<std::uint8_t,8>> requested_adapter;
    std::string status{"DXR not initialized"};
    HMODULE d3d{}, dxgi{};
    ComPtr<ID3D12Device5> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList4> list;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12Fence> geometry_fence;
    ComPtr<ID3D12RootSignature> root;
    std::shared_ptr<DevicePipelines> shared_pipelines;
    std::shared_ptr<DevicePrebuildSizes> prebuild_sizes;
    ComPtr<ID3D12PipelineState> pipeline,reflection_pipeline,native_pipeline,native_water_pipeline,models_pipeline,native_models_pipeline;
    ComPtr<ID3D12PipelineState> native_history_pipeline,model_lobe_pipeline,model_path_pipeline,scene_path_pipeline;
    std::array<ComPtr<ID3D12PipelineState>,2> curved_path_pipelines;
    std::array<ComPtr<ID3D12PipelineState>,2> curved_receiver_pipelines;
    ComPtr<ID3D12PipelineState> liquid_motion_pipeline;
    std::array<ComPtr<ID3D12PipelineState>,2> liquid_history_pipelines;
    std::array<ComPtr<ID3D12PipelineState>,5> stable_pipelines;
    HANDLE event{};
    UINT64 serial{};
    Buffer vertices,instances,constants,blas,tlas,scratch,output,readback,shared_geometry,coverage_buffer,backdrop_buffer,input_readback;
    // Explicit diagnostic only. Normal rendering allocates no query/readback
    // resources and records no timestamp commands. These tiny queries share
    // the existing submission/fence; timing must never introduce a GPU wait.
    bool profile_requested{std::getenv("STARFOX_PROFILE_DXR_TIMINGS")!=nullptr};
    ComPtr<ID3D12QueryHeap> profile_queries;
    Buffer profile_readback;
    UINT64 profile_frequency{};
    UINT64 profile_producer{},profile_calls{};
    struct ProfileFrame {
        UINT64 serial{},call{};unsigned width{},height{},material{UINT_MAX};
        std::size_t triangles{};bool rebuild{},history{},liquid_motion{};
    } profile_frame;
    BackdropUploadCache uploaded_backdrop;
    std::size_t backdrop_upload_bytes{};
    std::vector<float> built_positions;
    std::vector<float> position_scratch;
    std::vector<std::uint8_t> uploaded_coverage;
    std::vector<std::uint8_t> coverage_scratch;
    std::uint64_t input_trace_call{};
    bool output_common{};
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO cached_bottom_info{},cached_top_info{};
    std::size_t cached_triangle_count{};
    UINT cached_vertex_stride{};
    ~Impl() {
        // Submitted work is waited before resources are recycled or destroyed.
        if (queue && fence && event) {
            const auto value=++serial;
            if (SUCCEEDED(queue->Signal(fence.Get(),value))
                && SUCCEEDED(fence->SetEventOnCompletion(value,event)))
                WaitForSingleObject(event,5000);
        }
        // Retire the final diagnostic after the existing destructor wait only;
        // never let profiling throw during teardown or read an unfinished query.
        if(profile_requested && device && fence && SUCCEEDED(device->GetDeviceRemovedReason())) {
            try {report_profile();} catch(const std::exception& error) {
                std::clog<<"dxr-gpu-timing-error: "<<error.what()<<'\n';
            }
        }
        // Retain explicit PSO/root/resource references while the recorded
        // command objects are destroyed, including their recorded references.
        list.Reset(); allocator.Reset();
        profile_queries.Reset();profile_readback.resource.Reset();
        input_readback.resource.Reset();backdrop_buffer.resource.Reset();coverage_buffer.resource.Reset();shared_geometry.resource.Reset();readback.resource.Reset(); output.resource.Reset(); scratch.resource.Reset();
        tlas.resource.Reset(); blas.resource.Reset(); constants.resource.Reset();
        instances.resource.Reset(); vertices.resource.Reset(); native_water_pipeline.Reset();native_models_pipeline.Reset();models_pipeline.Reset();native_history_pipeline.Reset();
        for(auto& p:liquid_history_pipelines) p.Reset();
        model_lobe_pipeline.Reset();
        model_path_pipeline.Reset();
        scene_path_pipeline.Reset();
        for(auto& pipeline:curved_path_pipelines) pipeline.Reset();
        for(auto& pipeline:curved_receiver_pipelines) pipeline.Reset();
        liquid_motion_pipeline.Reset();
        for(auto& stable:stable_pipelines) stable.Reset();
        native_pipeline.Reset();reflection_pipeline.Reset();pipeline.Reset(); root.Reset();
        shared_pipelines.reset();
        prebuild_sizes.reset();
        queue.Reset(); geometry_fence.Reset(); fence.Reset(); device.Reset();
        if (event) CloseHandle(event);
        if (dxgi) FreeLibrary(dxgi);
        if (d3d) FreeLibrary(d3d);
    }
    void query_capability() {
        capability_attempted=true;
        d3d=LoadLibraryExW(L"d3d12.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        dxgi=LoadLibraryExW(L"dxgi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!d3d || !dxgi) throw std::runtime_error("Direct3D 12 unavailable");
        const auto createDevice=reinterpret_cast<decltype(&D3D12CreateDevice)>(GetProcAddress(d3d,"D3D12CreateDevice"));
        const auto createFactory=reinterpret_cast<decltype(&CreateDXGIFactory2)>(GetProcAddress(dxgi,"CreateDXGIFactory2"));
        if (!createDevice || !createFactory) throw std::runtime_error("Direct3D 12 entry points unavailable");
        ComPtr<IDXGIFactory6> factory;
        check(createFactory(0,IID_PPV_ARGS(factory.GetAddressOf())),"DXGI factory");
        for (UINT i=0;;++i) {
            ComPtr<IDXGIAdapter1> adapter;
            if (factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(adapter.GetAddressOf()))==DXGI_ERROR_NOT_FOUND) break;
            if (!adapter) continue;
            DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (requested_adapter && std::memcmp(requested_adapter->data(),&desc.AdapterLuid,8)!=0) continue;
            ComPtr<ID3D12Device5> candidate;
            if (FAILED(createDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,
                IID_ID3D12Device5, reinterpret_cast<void**>(candidate.GetAddressOf())))) continue;
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 caps{};
            D3D12_FEATURE_DATA_SHADER_MODEL model{D3D_SHADER_MODEL_6_5};
            if (FAILED(candidate->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&caps,sizeof(caps)))
                || caps.RaytracingTier<D3D12_RAYTRACING_TIER_1_1
                || FAILED(candidate->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&model,sizeof(model)))
                || model.HighestShaderModel<D3D_SHADER_MODEL_6_5) continue;
            device=std::move(candidate);
            char name[256]{};
            WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,name,sizeof(name),nullptr,nullptr);
            status=std::string("Hardware DXR 1.1: ")+name;
            break;
        }
        if (!device) throw std::runtime_error(requested_adapter
            ? "Requested adapter has no hardware DXR 1.1 device" : "No hardware DXR 1.1 device");
    }
    void initialize() {
        attempted=true;
        if(!capability_attempted) responsive_prepare([&] {query_capability();});
        const auto serialize=reinterpret_cast<decltype(&D3D12SerializeRootSignature)>(GetProcAddress(d3d,"D3D12SerializeRootSignature"));
        if(!serialize) throw std::runtime_error("Direct3D 12 root serialization unavailable");
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        check(device->CreateCommandQueue(&queueDesc,IID_ID3D12CommandQueue, reinterpret_cast<void**>(queue.GetAddressOf())),"DXR queue");
        check(device->CreateCommandAllocator(queueDesc.Type,IID_ID3D12CommandAllocator, reinterpret_cast<void**>(allocator.GetAddressOf())),"DXR allocator");
        check(device->CreateCommandList(0,queueDesc.Type,allocator.Get(),nullptr,
            IID_ID3D12GraphicsCommandList4, reinterpret_cast<void**>(list.GetAddressOf())),"DXR command list");
        check(list->Close(),"DXR initial close");
        check(device->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_ID3D12Fence, reinterpret_cast<void**>(fence.GetAddressOf())),"DXR fence");
        event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (!event) throw std::runtime_error("DXR fence event unavailable");
        D3D12_ROOT_PARAMETER parameters[7]{};
        parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[3].Descriptor.ShaderRegister=1;
        parameters[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[4].Descriptor.ShaderRegister=2;
        parameters[5].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[5].Descriptor.ShaderRegister=3;
        parameters[6].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[6].Descriptor.ShaderRegister=4;
        for (auto& p:parameters) p.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters=7; rootDesc.pParameters=parameters;
        ComPtr<ID3DBlob> blob,error;
        check(serialize(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,blob.GetAddressOf(),error.GetAddressOf()),"DXR root serialization");
        if(std::getenv("STARFOX_TEST_SHARE_DXR_PIPELINES")) {
            shared_pipelines=device_pipelines(device.Get(),blob.Get(),false);
            if(!shared_pipelines)
                shared_pipelines=responsive_prepare([&] {return device_pipelines(device.Get(),blob.Get());});
            root=shared_pipelines->root;
        } else check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),
            IID_ID3D12RootSignature, reinterpret_cast<void**>(root.GetAddressOf())),"DXR root signature");
        if(std::getenv("STARFOX_TRACE_DXR_PIPELINE_KEYS")) {
            // Diagnose immutable-pipeline duplication, not geometry identity.
            // Canonical COM identity distinguishes actual devices; matching
            // serialized root bytes are required before considering sharing.
            ComPtr<IUnknown> identity;
            check(device->QueryInterface(IID_IUnknown,
                reinterpret_cast<void**>(identity.GetAddressOf())),"DXR pipeline device identity");
            std::clog<<"dxr-pipeline-key: device="<<static_cast<const void*>(identity.Get())
                <<" root="<<static_cast<const void*>(root.Get())
                <<" root_hash="<<input_fingerprint(blob->GetBufferPointer(),blob->GetBufferSize())
                <<" root_bytes="<<blob->GetBufferSize()<<'\n';
        }
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature=root.Get();
        pipelineDesc.CS={shadow_dxr_shadow_shader,sizeof(shadow_dxr_shadow_shader)};
        create_pipeline(pipelineDesc,pipeline,"shadow");
        if(std::getenv("STARFOX_TRACE_GPU_RAYS")) std::clog<<"DXR rendering pipeline initialized\n";
    }
    void create_pipeline(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
        ComPtr<ID3D12PipelineState>& target,const char* kind) {
        const bool trace=std::getenv("STARFOX_TRACE_DXR_PIPELINE_KEYS")!=nullptr;
        const auto start=std::chrono::steady_clock::now();
        bool reused=false;
        const bool share=shared_pipelines && desc.pRootSignature==shared_pipelines->root.Get()
            && !desc.CachedPSO.pCachedBlob && !desc.CachedPSO.CachedBlobSizeInBytes;
        const auto reuse=[&] {
            for(const auto& entry:shared_pipelines->entries) {
                if(entry.shader==desc.CS.pShaderBytecode && entry.bytes==desc.CS.BytecodeLength
                    && entry.node_mask==desc.NodeMask && entry.flags==desc.Flags) {
                    target=entry.pipeline;reused=true;return true;
                }
            }
            return false;
        };
        // Ready immutable hits need no worker or UI sleep. A busy/missing cache
        // takes the joined preparation path instead of blocking the UI mutex.
        if(share) {
            std::unique_lock lock(shared_pipelines->mutex,std::try_to_lock);
            if(lock.owns_lock()) reuse();
        }
        const auto result=reused?S_OK:responsive_prepare([&] {
            const auto create=[&] {return device->CreateComputePipelineState(&desc,
                IID_ID3D12PipelineState,reinterpret_cast<void**>(target.GetAddressOf()));};
            if(!share) return create();
            std::lock_guard lock(shared_pipelines->mutex);
            // Every caller supplies a process-lifetime embedded shader array.
            // Exact array identity/size distinguishes ordinary and experimental
            // kernels; root bytes and canonical device were checked separately.
            if(reuse()) return S_OK;
            const auto status=create();
            if(SUCCEEDED(status)) shared_pipelines->entries.push_back({desc.CS.pShaderBytecode,
                desc.CS.BytecodeLength,desc.NodeMask,desc.Flags,target});
            return status;
        });
        if(trace) {
            const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-start).count();
            std::clog<<"dxr-pipeline-prepare: kind="<<kind<<" shared="<<bool(shared_pipelines)
                <<" reused="<<reused<<" us="<<elapsed<<" shader_bytes="<<desc.CS.BytecodeLength
                <<" result="<<result<<'\n';
        }
        check(result,"DXR shader pipeline");
    }
    void ensure(Buffer& buffer, UINT64 bytes, D3D12_HEAP_TYPE heap,
        D3D12_RESOURCE_STATES state, bool uav=false,bool shared=false) {
        if (buffer.resource && buffer.capacity>=bytes) return;
        buffer.resource.Reset(); buffer.capacity=0;
        D3D12_HEAP_PROPERTIES properties{}; properties.Type=heap;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width=(std::max<UINT64>(bytes,256)+255)&~UINT64(255);
        desc.Height=1; desc.DepthOrArraySize=1; desc.MipLevels=1;
        desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;
        check(device->CreateCommittedResource(&properties,shared?D3D12_HEAP_FLAG_SHARED:D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,
            IID_ID3D12Resource, reinterpret_cast<void**>(buffer.resource.GetAddressOf())),"DXR buffer allocation");
        buffer.capacity=desc.Width;
    }
    void upload(Buffer& buffer,const void* data,std::size_t size) {
        ensure(buffer,size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* target{}; const D3D12_RANGE empty{0,0};
        check(buffer.resource->Map(0,&empty,&target),"DXR upload map");
        std::memcpy(target,data,size);
        const D3D12_RANGE written{0,size}; buffer.resource->Unmap(0,&written);
    }
    void barrier(ID3D12Resource* resource) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV; barrier.UAV.pResource=resource;
        list->ResourceBarrier(1,&barrier);
    }
    void prepare_profile() {
        if(!profile_requested || profile_queries) return;
        check(queue->GetTimestampFrequency(&profile_frequency),"DXR diagnostic timestamp frequency");
        if(!profile_frequency) throw std::runtime_error("DXR diagnostic timestamp frequency is zero");
        const D3D12_QUERY_HEAP_DESC desc{D3D12_QUERY_HEAP_TYPE_TIMESTAMP,5,0};
        check(device->CreateQueryHeap(&desc,IID_ID3D12QueryHeap,
            reinterpret_cast<void**>(profile_queries.GetAddressOf())),"DXR diagnostic timestamp heap");
        ensure(profile_readback,5*sizeof(UINT64),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        static std::atomic_uint64_t producers{};profile_producer=++producers;
    }
    void profile_mark(unsigned index) {
        if(profile_queries) list->EndQuery(profile_queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,index);
    }
    void report_profile() {
        if(!profile_frame.serial || fence->GetCompletedValue()<profile_frame.serial) return;
        std::array<UINT64,5> ticks{};void* mapped{};
        const D3D12_RANGE range{0,sizeof(ticks)},empty{0,0};
        check(profile_readback.resource->Map(0,&range,&mapped),"DXR completed diagnostic timing map");
        std::memcpy(ticks.data(),mapped,sizeof(ticks));profile_readback.resource->Unmap(0,&empty);
        if(!std::is_sorted(ticks.begin(),ticks.end())) throw std::runtime_error("DXR diagnostic timestamps are not ordered");
        const auto ms=[&](unsigned begin,unsigned end){return double(ticks[end]-ticks[begin])*1000/double(profile_frequency);};
        std::clog<<"dxr-gpu-timing: producer="<<profile_producer<<" call="<<profile_frame.call
            <<" serial="<<profile_frame.serial<<" width="<<profile_frame.width
            <<" height="<<profile_frame.height<<" material="<<profile_frame.material
            <<" triangles="<<profile_frame.triangles<<" rebuild="<<profile_frame.rebuild
            <<" history="<<profile_frame.history<<" liquid_motion="<<profile_frame.liquid_motion
            <<" as_ms="<<ms(0,1)<<" trace_ms="<<ms(1,2)<<" motion_ms="<<ms(2,3)
            <<" finish_ms="<<ms(3,4)<<" total_ms="<<ms(0,4)<<" diagnostic_readback_bytes="<<sizeof(ticks)<<'\n';
        profile_frame={};
    }
    void await_producer() {
        if(!serial) return;
        check(device->GetDeviceRemovedReason(),"DXR producer device");
        if(fence->GetCompletedValue()>=serial) {report_profile();return;}
        const auto wait_start=std::chrono::steady_clock::now();
        check(fence->SetEventOnCompletion(serial,event),"DXR producer completion");
        if(WaitForSingleObject(event,5000)!=WAIT_OBJECT_0)
            throw std::runtime_error("DXR producer reuse timeout");
        check(device->GetDeviceRemovedReason(),"DXR completed device");
        const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now()-wait_start).count();
        if(std::getenv("STARFOX_TRACE_DXR_PIPELINE_KEYS"))
            std::clog<<"dxr-producer-wait: serial="<<serial<<" us="<<elapsed<<'\n';
        report_profile();
    }
    void render(const Scene& scene,Camera camera,Vec3 light,std::optional<ReceiverPlane> ground,
        std::vector<std::uint8_t>& mask,bool download=true,ID3D12Resource* external=nullptr,UINT external_count=0,UINT stride=16,const Coverage* coverage=nullptr,bool release_for_external=false,bool defer_completion=false,
        const render::RayMaterials* reflection=nullptr,const std::uint32_t* palette=nullptr,std::uint32_t environment=0,float roughness=0,std::uint32_t metallic=0,
        std::span<const std::uint32_t> environment_cube={},std::uint32_t face_size=0,
        const std::array<float,9>& environment_rotation={1,0,0,0,1,0,0,0,1},
        const render::GpuBackgroundDraw* background=nullptr,float background_eye_x=0,
        ID3D12Resource* resident_materials=nullptr,std::uint32_t material_offset=0,const RayWater* water=nullptr,bool ground_only=false,
        std::optional<PrimaryRayRange> primary_range=std::nullopt,std::uint32_t material_bytes=0,bool coverage_only=false,
        std::uint32_t resident_cube_offset=0,unsigned cube_encoding=0,bool specular_models=false,
        const RayReflectionHistory* history=nullptr) {
        // Upload buffers and the command allocator belong to the previous
        // producer submission until its fence completes.
        await_producer();
        backdrop_upload_bytes=0;
        const auto count=external?external_count/3:scene.triangle_count();
        const auto pixels=std::size_t(camera.width)*camera.height;
        const bool colour_output=reflection && !coverage_only;
        const auto row_bytes=colour_output?std::size_t(camera.width)*4:(std::size_t(camera.width)+3U)&~std::size_t(3U);
        const bool water_layers=colour_output && reflection->encoding==render::RayMaterialEncoding::native_rgba
            && water && water->source_colour && (water->auxiliary_layers || water->surface_layers);
        const auto layers=water_layers?native_water_layers(camera.width,camera.height,water->auxiliary_layers):std::optional<NativeWaterLayers>{};
        if(water_layers && !layers) throw std::runtime_error("Native water layers exceed buffer address space");
        const auto history_layers=history?native_reflection_history(camera.width,camera.height,history->extent,
            history->separated,layers.value_or(NativeWaterLayers{}),history->model_lobes,history->model_paths,history->scene_paths,history->curved_paths,history->curved_receivers):std::optional<NativeReflectionHistory>{};
        if(history && !history_layers) throw std::runtime_error("Native reflection history exceeds buffer address space");
        const auto transfer_bytes=history_layers?std::size_t(history_layers->storage_bytes):
            layers?std::size_t(layers->storage_bytes):row_bytes*camera.height;
        const bool trace_gpu_inputs=std::getenv("STARFOX_TRACE_DXR_GPU_INPUTS")!=nullptr;
        const auto trace_position_bytes=trace_gpu_inputs && external?std::uint64_t(external_count)*stride:0;
        const auto trace_material_bytes=trace_gpu_inputs && resident_materials
            ?(material_bytes?std::uint64_t(material_bytes):std::uint64_t(count)*64):0;
        const auto trace_cube_bytes=trace_gpu_inputs && resident_materials && resident_cube_offset
            ?std::uint64_t(face_size)*face_size*6*4:0;
        const auto trace_input_bytes=trace_position_bytes+trace_material_bytes+trace_cube_bytes;
        if (!pixels || (!count && !external)) { mask.assign(pixels,0); return; }
        if (count>UINT_MAX/3 || camera.width>16384 || camera.height>16384)
            throw std::runtime_error("DXR scene exceeds supported dimensions");
        static_assert(sizeof(TriangleCoverage)==40);
        const std::uint32_t header[4]={coverage?std::uint32_t(count):0,
            coverage?std::uint32_t(coverage->texels.size()):0, std::uint32_t(16+count*40),0};
        const BackdropImage* reflected_backdrop=nullptr;
        auto& coverage_bytes=coverage_scratch;
        coverage_bytes.resize(coverage?16+count*40+coverage->texels.size()*4:16);
        std::memcpy(coverage_bytes.data(),header,16);
        if(coverage) {
            std::memcpy(coverage_bytes.data()+16,coverage->triangles.data(),count*40);
            if(!coverage->texels.empty()) std::memcpy(coverage_bytes.data()+16+count*40,coverage->texels.data(),coverage->texels.size()*4);
        }
        if(reflection) {
            const std::uint32_t texels_at=16+(resident_materials?0:std::uint32_t(count)*64);
            const std::uint32_t palette_at=texels_at+std::uint32_t(reflection->texels.size())*4;
            const bool liquid_history=history && history->separated && water && (water->material==0 || water->material==3);
            const unsigned settings_bytes=history?(liquid_history?1344:history->separated?1248:1216):1168;
            coverage_bytes.resize(palette_at+settings_bytes+environment_cube.size_bytes());
            const std::uint32_t reflection_header[4]={std::uint32_t(count),palette_at,texels_at,
                (coverage_only?0U:1U)|(resident_materials?2U:0U)|(reflection->encoding==render::RayMaterialEncoding::native_rgba?4U:0U)
                    |(resident_cube_offset?8U:0U)|(cube_encoding?16U:0U)|(cube_encoding==2?32U:0U)|(specular_models?64U:0U)
                    |(water_layers?(water->auxiliary_layers?256U:512U):0U)
                    |(water && water->source_colour?128U:0U)|(history?1024U:0U)
                    |(history && history->separated?2048U:0U)|(history && history->previous_ground?4096U:0U)
                    |(history && history->previous_liquid?8192U:0U)|(history && history->model_lobes?16384U:0U)};
            std::memcpy(coverage_bytes.data(),reflection_header,16);
            if(!resident_materials) std::memcpy(coverage_bytes.data()+16,reflection->triangles.data(),count*64);
            for(std::size_t i=0;i<reflection->texels.size();++i) {
                const std::uint32_t index=reflection->texels[i];
                std::memcpy(coverage_bytes.data()+texels_at+i*4,&index,4);
            }
            std::memcpy(coverage_bytes.data()+palette_at,palette,1024);
            const std::uint32_t material_settings[4]={std::bit_cast<std::uint32_t>(roughness),metallic,face_size,
                resident_cube_offset?resident_cube_offset:palette_at+settings_bytes};
            std::memcpy(coverage_bytes.data()+palette_at+1024,material_settings,16);
            for(unsigned row=0;row<3;++row) {
                const float values[4]={environment_rotation[row*3],environment_rotation[row*3+1],environment_rotation[row*3+2],0};
                std::memcpy(coverage_bytes.data()+palette_at+1040+row*16,values,16);
            }
            const auto texel_count=std::uint32_t(reflection->texels.size());
            std::memcpy(coverage_bytes.data()+palette_at+1068,&texel_count,4);
            std::memset(coverage_bytes.data()+palette_at+1088,0,64);
            std::memset(coverage_bytes.data()+palette_at+1152,0,16);
            std::memcpy(coverage_bytes.data()+palette_at+1152,&material_bytes,4);
            if(water) {
                const float values[16]={water->time,water->reflection_strength,water->brightness,float(water->material+(water->mirror_models?16:0)+((water->caustics&3)<<5)),
                    water->world_to_view[0],water->world_to_view[1],water->world_to_view[2],water->camera_position[0],
                    water->world_to_view[3],water->world_to_view[4],water->world_to_view[5],water->camera_position[1],
                    water->world_to_view[6],water->world_to_view[7],water->world_to_view[8],water->camera_position[2]};
                std::memcpy(coverage_bytes.data()+palette_at+1088,values,64);
                if(water->source_colour) std::memcpy(coverage_bytes.data()+palette_at+1156,water->source_colour->data(),12);
            }
            if(history) {
                const std::uint32_t layout[4]{history->previous_vertex_offset,history_layers->motion_offset,history->extent[0],history->extent[1]};
                std::memcpy(coverage_bytes.data()+palette_at+1168,layout,16);
                const float projection[4]{float(history->projection[0]),float(history->projection[1]),float(history->projection[2]),float(history->projection[3])};
                std::memcpy(coverage_bytes.data()+palette_at+1184,projection,16);
                const float clip[4]{float(history->near_plane),float(history->far_plane),0,0};
                std::memcpy(coverage_bytes.data()+palette_at+1200,clip,16);
                std::memcpy(coverage_bytes.data()+palette_at+1208,&history->previous_index_offset,4);
                std::memcpy(coverage_bytes.data()+palette_at+1212,&history->model_lobes,4);
                if(liquid_history) {
                    const auto frame=reflection_liquid_frame_words(*history);
                    std::memcpy(coverage_bytes.data()+palette_at+1216,frame.data(),sizeof(frame));
                } else if(history->separated) {
                    float point[4]{},normal[4]{};
                    if(history->previous_ground) for(unsigned i=0;i<3;++i) {
                        point[i]=float(history->previous_ground->point[i]);normal[i]=float(history->previous_ground->normal[i]);
                    }
                    std::memcpy(coverage_bytes.data()+palette_at+1216,point,16);
                    std::memcpy(coverage_bytes.data()+palette_at+1232,normal,16);
                }
            }
            if(!environment_cube.empty()) std::memcpy(coverage_bytes.data()+palette_at+settings_bytes,
                environment_cube.data(),environment_cube.size_bytes());
            if(background) {
                const auto& p=*background->ppu;const auto& s=background->settings;
                const auto at=std::uint32_t(coverage_bytes.size());
                const auto cgram_at=at+67392+std::uint32_t(s.unique_regions.size())*32;
                coverage_bytes.resize(cgram_at+1024);
                std::memcpy(coverage_bytes.data()+palette_at+1052,&at,4);
                std::memcpy(coverage_bytes.data()+palette_at+1084,&background_eye_x,4);
                unsigned black=0,darkest=~0U;
                for(unsigned i=0;i<256;++i) {
                    const auto c=p.cgram[i];const unsigned l=77*(c&31)+150*((c>>5)&31)+29*((c>>10)&31);
                    if(l<darkest){darkest=l;black=i;}
                }
                // Fit the cartridge's quantised roll table once per upload.
                // Reflection rays extend beyond its 32 columns, just as the
                // widescreen background does; clamping would flatten the sky.
                double sx=0,sy=0,sxx=0,sxy=0;int samples=0,previous=0,unwrapped=0;
                if(p.background_mode==2 && p.bg2_vertical_offsets_enabled && s.extend_horizontal && !p.tunnel_scene)
                    for(unsigned col=0;col<32;++col) {
                        const unsigned address=(0x2fa0+col)*2;
                        const unsigned word=p.vram[address]|(unsigned(p.vram[address+1])<<8);
                        if(!(word&0x4000)) continue;
                        const int raw=word&8191;int delta=(raw-previous)&8191;if(delta>4095) delta-=8192;
                        unwrapped=samples?unwrapped+delta:raw;previous=raw;
                        const double x=col+1;sx+=x;sy+=unwrapped;sxx+=x*x;sxy+=x*unwrapped;++samples;
                    }
                const double denominator=samples*sxx-sx*sx;
                const float slope=samples>1 && denominator!=0?float((samples*sxy-sx*sy)/denominator):0;
                const float intercept=samples?float((sy-double(slope)*sx)/samples):0;
                const unsigned flags=(s.wrap_horizontal?1U:0U)|(p.bg2_horizontal_offsets_enabled?2U:0U)
                    |(p.bg2_scanline_scroll_enabled?4U:0U)
                    |(p.background_mode==2 && p.bg2_vertical_offsets_enabled?8U:0U)
                    |(p.tunnel_scene?16U:0U)|(s.transparent_cgram_black?32U:0U)|(samples?64U:0U);
                const std::int32_t header[16]={p.bg2_screen_base,p.bg2_character_base,
                    (p.bg2_screen_size&1)?64:32,(p.bg2_screen_size&2)?64:32,p.bg2_tile_size_16?16:8,
                    s.scroll_x,s.scroll_y,int(flags),int(s.unique_regions.size()),int(s.single_occurrence_top_rows),
                    int(black),int(s.sky_source_min),std::bit_cast<std::int32_t>(intercept),std::bit_cast<std::int32_t>(slope),int(cgram_at),0};
                std::memcpy(coverage_bytes.data()+at,header,64);
                std::memcpy(coverage_bytes.data()+at+64,p.vram.data(),65536);
                for(unsigned row=0;row<224;++row) {
                    const std::int32_t x=p.bg2_horizontal_offsets[row],y=p.bg2_scanline_scroll_y[row];
                    std::memcpy(coverage_bytes.data()+at+65600+row*4,&x,4);
                    std::memcpy(coverage_bytes.data()+at+66496+row*4,&y,4);
                }
                for(unsigned i=0;i<s.unique_regions.size();++i) {
                    const auto& r=s.unique_regions[i];const std::int32_t region[8]={r.left,r.top,r.right,r.bottom,
                        r.first_colour,r.last_colour,r.replacement_colour,r.replacement_x_offset};
                    std::memcpy(coverage_bytes.data()+at+67392+i*32,region,32);
                }
                for(unsigned i=0;i<256;++i) {
                    const std::uint32_t colour=p.cgram[i];
                    std::memcpy(coverage_bytes.data()+cgram_at+i*4,&colour,4);
                }
                if(s.reflection_environment && s.reflection_environment->active()) {
                    const auto& e=*s.reflection_environment;
                    const auto address=std::uint32_t(coverage_bytes.size());
                    coverage_bytes.resize(address+1232);
                    std::memcpy(coverage_bytes.data()+at+60,&address,4);
                    std::memcpy(coverage_bytes.data()+address,e.classes.data(),1024);
                    std::memcpy(coverage_bytes.data()+address+1024,e.modes.data(),16);
                    std::memcpy(coverage_bytes.data()+address+1040,e.motion.data(),16);
                    std::memcpy(coverage_bytes.data()+address+1056,e.plane.data(),16);
                    std::memset(coverage_bytes.data()+address+1072,0,16);
                    std::memcpy(coverage_bytes.data()+address+1088,e.backdrop_projection.data(),16);
                    std::memcpy(coverage_bytes.data()+address+1104,e.backdrop_keep.data(),32);
                    std::memcpy(coverage_bytes.data()+address+1136,e.backdrop_palette.data(),32);
                    std::memcpy(coverage_bytes.data()+address+1168,e.backdrop_ramp.data(),64);
                    if(e.backdrop && e.modes[2]) {
                        const auto& sky=*e.backdrop;
                        reflected_backdrop=&sky;
                        if(!sky.width || !sky.height || sky.width>8192 || sky.height>8192 || sky.pixels.size()!=std::size_t(sky.width)*sky.height)
                            throw std::runtime_error("Invalid reflected backdrop");
                        // Keep the immutable image separate from moving camera,
                        // palette and material metadata. Sealed asset keys
                        // avoid full scans; mutable images still compare pixels.
                        if(!uploaded_backdrop.matches(sky)) {
                            upload(backdrop_buffer,sky.pixels.data(),sky.pixels.size()*4);
                            uploaded_backdrop.remember(sky);
                            backdrop_upload_bytes=sky.pixels.size()*4;
                        }
                        const std::uint32_t image[4]{sky.width,sky.height,0,0};
                        std::memcpy(coverage_bytes.data()+address+1072,image,16);
                    }
                }
            }
        }
        // Compare complete bytes, not source identities: UV scroll, palettes,
        // transparency and recycled models must all invalidate retained data.
        if(coverage_bytes!=uploaded_coverage) {
            upload(coverage_buffer,coverage_bytes.data(),coverage_bytes.size());
            uploaded_coverage.swap(coverage_bytes);
        }
        auto& positions=position_scratch;
        positions.clear();
        if(!external) positions.reserve(count*9);
        if(!external) for (const auto& triangle:scene.triangles()) for (auto v:{triangle.a,triangle.b,triangle.c}) {
            positions.push_back(float(v.x)); positions.push_back(float(v.y)); positions.push_back(float(v.z));
        }
        // Compare actual GPU input bytes, not object identities or hashes:
        // animation, exploding faces and recycled objects must invalidate this.
        const bool rebuild=count && (external || positions.size()!=built_positions.size()
            || std::memcmp(positions.data(),built_positions.data(),positions.size()*sizeof(float))!=0);
        if(rebuild && !external) upload(vertices,positions.data(),positions.size()*sizeof(float));
        D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottom{},top{};
        auto bottomInfo=cached_bottom_info,topInfo=cached_top_info;
        const UINT vertex_stride=external?stride:12;
        if(count) {
        geometry.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry.Flags=D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometry.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;
        geometry.Triangles.VertexCount=UINT(count*3);
        geometry.Triangles.VertexBuffer={external?external->GetGPUVirtualAddress():vertices.resource->GetGPUVirtualAddress(),external?stride:12};
        bottom.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottom.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        bottom.NumDescs=1; bottom.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY; bottom.pGeometryDescs=&geometry;
        // Allocation requirements depend on layout/count, not animated positions.
        // External GPU geometry still rebuilds AS contents every frame below.
        if(!bottomInfo.ResultDataMaxSizeInBytes || cached_triangle_count!=count || cached_vertex_stride!=vertex_stride) {
            const auto prepare_start=std::chrono::steady_clock::now();
            const bool first=!cached_bottom_info.ResultDataMaxSizeInBytes;
            // Startup reuse is correct, but the current quiet two-scene
            // matrix is not a consistent frame-time win. Keep this opt-in
            // until wider performance acceptance; retain the old default.
            const bool cache_sizes=std::getenv("STARFOX_TEST_DXR_PREBUILD_CACHE")
                && !std::getenv("STARFOX_TEST_DISABLE_DXR_PREBUILD_CACHE");
            bool reused=false;
            if(cache_sizes) {
                if(!prebuild_sizes) prebuild_sizes=device_prebuild_sizes(device.Get());
                // A ready peer answer needs neither a driver call nor a joined
                // worker/UI sleep. If another producer is preparing a miss,
                // use the normal responsive first-query path below instead.
                std::unique_lock lock(prebuild_sizes->mutex,std::try_to_lock);
                if(lock.owns_lock()) for(const auto& entry:prebuild_sizes->entries) {
                    if(entry.triangles==count && entry.stride==vertex_stride) {
                        bottomInfo=entry.info;
                        reused=true;
                        break;
                    }
                }
            }
            const auto query_size=[&] {
                if(!cache_sizes) {
                    device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom,&bottomInfo);
                    return;
                }
                if(!prebuild_sizes) prebuild_sizes=device_prebuild_sizes(device.Get());
                std::lock_guard lock(prebuild_sizes->mutex);
                for(const auto& entry:prebuild_sizes->entries) {
                    if(entry.triangles==count && entry.stride==vertex_stride) {
                        bottomInfo=entry.info;
                        reused=true;
                        return;
                    }
                }
                device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom,&bottomInfo);
                // A failed/empty driver answer must take the existing error
                // path, not poison another producer's later query.
                if(bottomInfo.ResultDataMaxSizeInBytes && bottomInfo.ScratchDataSizeInBytes) {
                    if(prebuild_sizes->entries.size()==64) prebuild_sizes->entries.erase(prebuild_sizes->entries.begin());
                    prebuild_sizes->entries.push_back({count,vertex_stride,bottomInfo});
                }
            };
            if(!reused && first) {
                // Some drivers compile their AS-build kernels in this nominal
                // size query. The first call can block for seconds; subsequent
                // count/layout queries are warmed and must keep the old fast
                // path rather than spawning a compiler worker every frame.
                responsive_prepare(query_size);
            } else if(!reused) query_size();
            const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-prepare_start).count();
            if(std::getenv("STARFOX_TRACE_DXR_PIPELINE_KEYS")) {
                std::clog<<"dxr-as-prepare: level=bottom triangles="<<count<<" stride="<<vertex_stride
                    <<" first="<<first<<" cached="<<reused<<" us="<<elapsed<<'\n';
            }
        }
        ensure(blas,bottomInfo.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,true);
        D3D12_RAYTRACING_INSTANCE_DESC instance{};
        instance.Transform[0][0]=instance.Transform[1][1]=instance.Transform[2][2]=1;
        instance.InstanceMask=255;
        instance.Flags=D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
        instance.AccelerationStructure=blas.resource->GetGPUVirtualAddress();
        if(rebuild) upload(instances,&instance,sizeof(instance));
        top.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        top.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        top.NumDescs=1; top.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;
        top.InstanceDescs=instances.resource->GetGPUVirtualAddress();
        if(!topInfo.ResultDataMaxSizeInBytes) {
            const auto prepare_start=std::chrono::steady_clock::now();
            responsive_prepare([&] {device->GetRaytracingAccelerationStructurePrebuildInfo(&top,&topInfo);});
            const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-prepare_start).count();
            if(std::getenv("STARFOX_TRACE_DXR_PIPELINE_KEYS"))
                std::clog<<"dxr-as-prepare: level=top us="<<elapsed<<'\n';
        }
        ensure(tlas,topInfo.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,true);
        ensure(scratch,std::max(bottomInfo.ScratchDataSizeInBytes,topInfo.ScratchDataSizeInBytes),
            D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true);
        }
        if(!output.resource || output.capacity<transfer_bytes) output_common=false;
        ensure(output,transfer_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true,true);
        if(download) ensure(readback,transfer_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        if(trace_input_bytes) ensure(input_readback,trace_input_bytes+transfer_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        Constants settings{};
        settings.camera={float(camera.width),float(camera.height),float(camera.focal_length),float(camera.center_x)};
        settings.options={float(camera.center_y),ground?1.f:0.f,float(camera.vertical_focal_length()),float(((coverage || coverage_only)?1U:0U)|(ground_only?2U:0U))};
        const auto depth_range=primary_range.value_or(PrimaryRayRange{});
        settings.primary_range={float(depth_range.near_depth),float(depth_range.far_depth),primary_range?1.f:0.f,0};
        if(const auto* mode=std::getenv("STARFOX_TEST_DXR_HIT_DIAGNOSTIC"))
            settings.primary_range.w=float(std::clamp(std::atoi(mode),0,5));
        if(std::getenv("STARFOX_TEST_DXR_CANONICAL_NORMALS")) settings.primary_range.w+=16;
        if(std::getenv("STARFOX_TEST_DXR_STABLE_HITS")) settings.primary_range.w+=32;
        if (ground) { settings.point=floats(ground->point); settings.normal=floats(ground->normal); }
        if(reflection) {settings.point.w=std::bit_cast<float>(environment);settings.normal.w=float(external?stride:12);}
        light=light*(1.0/std::sqrt(dot(light,light)));
        const auto reference=std::abs(light.y)<.9?Vec3{0,1,0}:Vec3{1,0,0};
        auto tangent=cross(light,reference); tangent=tangent*(1.0/std::sqrt(dot(tangent,tangent)));
        const auto bitangent=cross(light,tangent);
        const auto samples=camera.shadow_samples();
        for (unsigned i=0;i<samples;++i) {
            const auto radius=camera.shadow_angular_radius()*std::sqrt((i+.5)/samples);
            const auto angle=i*2.399963229728653;
            auto direction=light+tangent*(radius*std::cos(angle))+bitangent*(radius*std::sin(angle));
            settings.lights[i]=floats(direction*(1.0/std::sqrt(dot(direction,direction))));
        }
        settings.lights[0].w=float(samples);
        upload(constants,&settings,sizeof(settings));
        if(std::getenv("STARFOX_TRACE_DXR_INPUTS")) {
            // uploaded_coverage owns the actual current bytes after the swap;
            // coverage_scratch can contain the previous dispatch's payload.
            // Resident positions/materials/cubes are deliberately reported as
            // incomplete, not falsely certified by hashing their CPU metadata.
            std::clog<<"dxr-inputs: call="<<++input_trace_call
                <<" triangles="<<count<<" width="<<camera.width<<" height="<<camera.height
                <<" reflection="<<bool(reflection)<<" water="<<bool(water)
                <<" native="<<(reflection && reflection->encoding==render::RayMaterialEncoding::native_rgba)
                <<" coverage_only="<<coverage_only<<" ground_only="<<ground_only
                <<" specular_models="<<specular_models
                <<" resident_positions="<<bool(external)<<" resident_materials="<<bool(resident_materials)
                <<" resident_cube="<<bool(resident_cube_offset)
                <<" cpu_inputs_complete="<<(!external && !resident_materials && !resident_cube_offset)
                <<" constants_hash="<<input_fingerprint(&settings,sizeof(settings))
                <<" positions_bytes="<<positions.size()*sizeof(float)
                <<" positions_hash="<<input_fingerprint(positions.data(),positions.size()*sizeof(float))
                <<" coverage_bytes="<<uploaded_coverage.size()
                <<" coverage_hash="<<input_fingerprint(uploaded_coverage.data(),uploaded_coverage.size())
                <<" backdrop_bytes="<<(reflected_backdrop?reflected_backdrop->pixels.size()*4:0)
                <<" backdrop_hash="<<input_fingerprint(reflected_backdrop?reflected_backdrop->pixels.data():nullptr,
                    reflected_backdrop?reflected_backdrop->pixels.size()*4:0)<<'\n';
        }
        check(allocator->Reset(),"DXR allocator reset");
        auto* trace_pipeline=pipeline.Get();
        if(reflection && !coverage_only) {
            // Keep native colour equations out of the flat shader's register
            // pressure and startup compilation. Compile its PSO only when a
            // native calibrated material batch is actually consumed.
            const bool native=reflection->encoding==render::RayMaterialEncoding::native_rgba;
            const bool models_only=water==nullptr;
            const bool native_water=native && water && water->source_colour.has_value();
            const bool stable_hits=(unsigned(settings.primary_range.w)&32U)!=0;
            const unsigned stable_index=native_water?2:models_only?(native?4:3):native?1:0;
            const bool model_lobes=history && history->model_lobes!=0;
            const bool model_paths=history && history->model_paths;
            const bool scene_paths=history && history->scene_paths;
            const bool curved_paths=history && history->curved_paths;
            const bool curved_receivers=history && history->curved_receivers;
            const bool liquid_history=history && water && (water->material==0 || water->material==3);
            const unsigned liquid_index=water && water->material==3?1:0;
            auto& selected=curved_receivers?curved_receiver_pipelines[liquid_index]:curved_paths?curved_path_pipelines[liquid_index]:scene_paths?scene_path_pipeline:model_paths?model_path_pipeline:model_lobes?model_lobe_pipeline:liquid_history?liquid_history_pipelines[liquid_index]:history?native_history_pipeline:stable_hits?stable_pipelines[stable_index]
                :native_water?native_water_pipeline:models_only?(native?native_models_pipeline:models_pipeline)
                    :(native?native_pipeline:reflection_pipeline);
            if(!selected) {
                D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};desc.pRootSignature=root.Get();
                if(curved_receivers) desc.CS=liquid_index
                    ?D3D12_SHADER_BYTECODE{shadow_dxr_curved_lava_receivers_shader,sizeof(shadow_dxr_curved_lava_receivers_shader)}
                    :D3D12_SHADER_BYTECODE{shadow_dxr_curved_water_receivers_shader,sizeof(shadow_dxr_curved_water_receivers_shader)};
                else if(curved_paths) desc.CS=liquid_index
                    ?D3D12_SHADER_BYTECODE{shadow_dxr_curved_lava_paths_shader,sizeof(shadow_dxr_curved_lava_paths_shader)}
                    :D3D12_SHADER_BYTECODE{shadow_dxr_curved_water_paths_shader,sizeof(shadow_dxr_curved_water_paths_shader)};
                else if(scene_paths) desc.CS={shadow_dxr_scene_paths_shader,sizeof(shadow_dxr_scene_paths_shader)};
                else if(model_paths) desc.CS={shadow_dxr_model_paths_shader,sizeof(shadow_dxr_model_paths_shader)};
                else if(model_lobes) desc.CS={shadow_dxr_model_lobes_shader,sizeof(shadow_dxr_model_lobes_shader)};
                else if(liquid_history) desc.CS=liquid_index
                    ?D3D12_SHADER_BYTECODE{shadow_dxr_lava_history_shader,sizeof(shadow_dxr_lava_history_shader)}
                    :D3D12_SHADER_BYTECODE{shadow_dxr_water_history_shader,sizeof(shadow_dxr_water_history_shader)};
                else if(history) desc.CS={shadow_dxr_native_history_shader,sizeof(shadow_dxr_native_history_shader)};
                else if(stable_hits) {
                    const std::array<D3D12_SHADER_BYTECODE,5> shaders{{
                        {shadow_dxr_stable_reflection_shader,sizeof(shadow_dxr_stable_reflection_shader)},
                        {shadow_dxr_stable_native_shader,sizeof(shadow_dxr_stable_native_shader)},
                        {shadow_dxr_stable_native_water_shader,sizeof(shadow_dxr_stable_native_water_shader)},
                        {shadow_dxr_stable_models_shader,sizeof(shadow_dxr_stable_models_shader)},
                        {shadow_dxr_stable_native_models_shader,sizeof(shadow_dxr_stable_native_models_shader)}}};
                    desc.CS=shaders[stable_index];
                }
                else if(native_water) desc.CS={shadow_dxr_native_water_shader,sizeof(shadow_dxr_native_water_shader)};
                else if(models_only) desc.CS=native
                    ?D3D12_SHADER_BYTECODE{shadow_dxr_native_models_shader,sizeof(shadow_dxr_native_models_shader)}
                    :D3D12_SHADER_BYTECODE{shadow_dxr_models_shader,sizeof(shadow_dxr_models_shader)};
                else desc.CS=native?D3D12_SHADER_BYTECODE{shadow_dxr_native_shader,sizeof(shadow_dxr_native_shader)}
                    :D3D12_SHADER_BYTECODE{shadow_dxr_shader,sizeof(shadow_dxr_shader)};
                if(std::getenv("STARFOX_TRACE_GPU_RAYS")) std::clog<<"DXR reflection pipeline creating: "
                    <<(native?"native":"indexed")<<' '<<(native_water?"water":models_only?"models":"fluid")
                    <<(stable_hits?" stable-depth":"")<<'\n';
                create_pipeline(desc,selected,curved_receivers?(liquid_index?"curved-lava-receivers":"curved-water-receivers"):curved_paths?(liquid_index?"curved-lava-paths":"curved-water-paths"):scene_paths?"scene-path-history":model_paths?"model-path-history":model_lobes?"model-lobe-history":liquid_history?(liquid_index?"lava-history":"water-history"):history?"native-history":native_water?"native-water":models_only
                    ?(native?"native-models":"indexed-models"):(native?"native-fluid":"indexed-fluid"));
                if(std::getenv("STARFOX_TRACE_DXR_PIPELINE_KEYS")) {
                    ComPtr<IUnknown> identity;
                    check(device->QueryInterface(IID_IUnknown,
                        reinterpret_cast<void**>(identity.GetAddressOf())),"DXR material pipeline device identity");
                    std::clog<<"dxr-pipeline-object: device="<<static_cast<const void*>(identity.Get())
                        <<" root="<<static_cast<const void*>(root.Get())
                        <<" pso="<<static_cast<const void*>(selected.Get())
                        <<" shader_hash="<<input_fingerprint(desc.CS.pShaderBytecode,desc.CS.BytecodeLength)
                        <<" shader_bytes="<<desc.CS.BytecodeLength<<'\n';
                }
                if(std::getenv("STARFOX_TRACE_GPU_RAYS")) std::clog<<"DXR reflection pipeline ready\n";
            }
            trace_pipeline=selected.Get();
        }
        const bool liquid_motion=history && history->previous_liquid.has_value() && !history->curved_paths;
        if(liquid_motion && !liquid_motion_pipeline) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};desc.pRootSignature=root.Get();
            desc.CS={reflection_liquid_history_shader,sizeof(reflection_liquid_history_shader)};
            create_pipeline(desc,liquid_motion_pipeline,"liquid-motion");
        }
        prepare_profile();
        check(list->Reset(allocator.Get(),trace_pipeline),"DXR command reset");
        profile_mark(0);
        D3D12_RESOURCE_BARRIER vertex_access{};
        D3D12_RESOURCE_BARRIER material_access{};
        if(resident_materials && resident_materials!=external) {
            material_access.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            material_access.Transition={resident_materials,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
            list->ResourceBarrier(1,&material_access);
        }
        if(external) {
            vertex_access.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            vertex_access.Transition={external,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
            list->ResourceBarrier(1,&vertex_access);
        }
        if(trace_input_bytes) {
            // Diagnostic only: queue-ordered copies of the actual resident
            // inputs, after the incoming geometry timeline dependency. Copy
            // Restore the read state even when vertices/materials share storage.
            auto copy_input=[&](ID3D12Resource* resource,std::uint64_t source,std::uint64_t target,std::uint64_t bytes) {
                if(!bytes) return;
                D3D12_RESOURCE_DESC description{};
#if defined(_MSC_VER)
                description=resource->GetDesc();
#else
                resource->GetDesc(&description);
#endif
                const auto capacity=description.Width;
                if(source>capacity || bytes>capacity-source) throw std::runtime_error("DXR input diagnostic exceeds resource");
                D3D12_RESOURCE_BARRIER access{};access.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                access.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE};
                list->ResourceBarrier(1,&access);
                list->CopyBufferRegion(input_readback.resource.Get(),target,resource,source,bytes);
                std::swap(access.Transition.StateBefore,access.Transition.StateAfter);
                list->ResourceBarrier(1,&access);
            };
            copy_input(external,0,0,trace_position_bytes);
            copy_input(resident_materials,material_offset,trace_position_bytes,trace_material_bytes);
            copy_input(resident_materials,std::uint64_t(material_offset)+resident_cube_offset,
                trace_position_bytes+trace_material_bytes,trace_cube_bytes);
        }
        if(output_common) {
            D3D12_RESOURCE_BARRIER acquire{};acquire.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            acquire.Transition={output.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
            list->ResourceBarrier(1,&acquire);output_common=false;
        }
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
        if(rebuild) {
        build.Inputs=bottom; build.DestAccelerationStructureData=blas.resource->GetGPUVirtualAddress();
        build.ScratchAccelerationStructureData=scratch.resource->GetGPUVirtualAddress();
        list->BuildRaytracingAccelerationStructure(&build,0,nullptr);
        barrier(blas.resource.Get()); barrier(scratch.resource.Get());
        build.Inputs=top; build.DestAccelerationStructureData=tlas.resource->GetGPUVirtualAddress();
        list->BuildRaytracingAccelerationStructure(&build,0,nullptr);
        barrier(tlas.resource.Get());
        }
        profile_mark(1);
        list->SetComputeRootSignature(root.Get());
        // DXR defines GPUVA 0 for an acceleration-structure SRV as a forced
        // miss, including inline RayQuery. Explicitly bind it for an empty
        // native world; never traverse a prior frame's TLAS or fake geometry.
        // https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#additional-srv-type
        list->SetComputeRootShaderResourceView(0,count?tlas.resource->GetGPUVirtualAddress():0);
        list->SetComputeRootUnorderedAccessView(1,output.resource->GetGPUVirtualAddress());
        list->SetComputeRootConstantBufferView(2,constants.resource->GetGPUVirtualAddress());
        list->SetComputeRootShaderResourceView(3,coverage_buffer.resource->GetGPUVirtualAddress());
        list->SetComputeRootShaderResourceView(4,(external?external:vertices.resource.Get())->GetGPUVirtualAddress());
        list->SetComputeRootShaderResourceView(5,(resident_materials?resident_materials:coverage_buffer.resource.Get())->GetGPUVirtualAddress()
            +(resident_materials?material_offset:0));
        list->SetComputeRootShaderResourceView(6,(backdrop_buffer.resource?backdrop_buffer.resource.Get():coverage_buffer.resource.Get())->GetGPUVirtualAddress());
        list->Dispatch((camera.width+7)/8,(camera.height+7)/8,1);
        profile_mark(2);
        if(liquid_motion) {
            // Both passes stay in this command list/submission and allocation.
            // The second pass consumes native identities/barycentrics, never
            // retraces the scene or rewrites current radiance/liquid layers.
            barrier(output.resource.Get());
            list->SetPipelineState(liquid_motion_pipeline.Get());
            list->Dispatch((camera.width+7)/8,(camera.height+7)/8,1);
        }
        profile_mark(3);
        if(download || trace_input_bytes) {
        D3D12_RESOURCE_BARRIER transition{};
        transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Transition={output.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};
        list->ResourceBarrier(1,&transition);
        if(download) list->CopyBufferRegion(readback.resource.Get(),0,output.resource.Get(),0,transfer_bytes);
        if(trace_input_bytes) list->CopyBufferRegion(input_readback.resource.Get(),trace_input_bytes,
            output.resource.Get(),0,transfer_bytes);
        std::swap(transition.Transition.StateBefore,transition.Transition.StateAfter);
        list->ResourceBarrier(1,&transition);
        }
        if(external) {
            std::swap(vertex_access.Transition.StateBefore,vertex_access.Transition.StateAfter);
            list->ResourceBarrier(1,&vertex_access);
        }
        if(resident_materials && resident_materials!=external) {
            std::swap(material_access.Transition.StateBefore,material_access.Transition.StateAfter);
            list->ResourceBarrier(1,&material_access);
        }
        if(release_for_external && !download) {
            D3D12_RESOURCE_BARRIER release{};
            release.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            release.Transition={output.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON};
            list->ResourceBarrier(1,&release);
            output_common=true;
        }
        profile_mark(4);
        if(profile_queries) list->ResolveQueryData(profile_queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,5,
            profile_readback.resource.Get(),0);
        check(list->Close(),"DXR command close");
        ID3D12CommandList* lists[]{list.Get()}; queue->ExecuteCommandLists(1,lists);
        const auto value=++serial;
        check(queue->Signal(fence.Get(),value),"DXR signal");
        if(profile_queries) profile_frame={value,++profile_calls,camera.width,camera.height,water?water->material:UINT_MAX,
            count,rebuild,history!=nullptr,liquid_motion};
        if(!defer_completion || trace_input_bytes) await_producer();
        if(trace_input_bytes) {
            void* diagnostic{};const D3D12_RANGE range{0,SIZE_T(trace_input_bytes+transfer_bytes)};
            check(input_readback.resource->Map(0,&range,&diagnostic),"DXR input diagnostic map");
            const auto* bytes=static_cast<const std::uint8_t*>(diagnostic);
            std::clog<<"dxr-resident-inputs: call="<<input_trace_call<<" diagnostic_readback=1"
                <<" positions_bytes="<<trace_position_bytes<<" positions_hash="<<input_fingerprint(bytes,trace_position_bytes)
                <<" materials_bytes="<<trace_material_bytes
                <<" materials_hash="<<input_fingerprint(bytes+trace_position_bytes,trace_material_bytes)
                <<" cube_bytes="<<trace_cube_bytes
                <<" cube_hash="<<input_fingerprint(bytes+trace_position_bytes+trace_material_bytes,trace_cube_bytes)
                <<" output_bytes="<<transfer_bytes
                <<" output_hash="<<input_fingerprint(bytes+trace_input_bytes,transfer_bytes)<<'\n';
            if(const auto* prefix=std::getenv("STARFOX_TRACE_DXR_OUTPUT_PREFIX")) {
                static std::atomic_uint64_t dump_sequence{};
                const auto index=++dump_sequence;
                if(index<=64 && (index<=2 || index%16==15 || index%16==0)) {
                    const auto stem=std::string(prefix)+std::to_string(index);
                    auto dump_bytes=[&](const char* suffix,const std::uint8_t* source,std::uint64_t size) {
                        std::ofstream dump(stem+suffix,std::ios::binary);
                        dump.write(reinterpret_cast<const char*>(source),std::streamsize(size));
                        if(!dump) throw std::runtime_error("Unable to write DXR byte diagnostic");
                    };
                    dump_bytes(".bin",bytes+trace_input_bytes,transfer_bytes);
                    dump_bytes(".positions.bin",bytes,trace_position_bytes);
                    dump_bytes(".materials.bin",bytes+trace_position_bytes,trace_material_bytes);
                    std::clog<<"dxr-output-dump: index="<<index<<" bytes="<<transfer_bytes
                        <<" mode="<<settings.primary_range.w<<'\n';
                }
            }
            const D3D12_RANGE empty{0,0};input_readback.resource->Unmap(0,&empty);
        }
        if(rebuild) {
            built_positions.swap(positions);
            cached_bottom_info=bottomInfo;cached_top_info=topInfo;
            cached_triangle_count=count;cached_vertex_stride=vertex_stride;
        }
        if(!download) return;
        void* data{}; const D3D12_RANGE range{0,transfer_bytes};
        check(readback.resource->Map(0,&range,&data),"DXR readback map");
        mask.resize(colour_output?pixels*4:pixels);
        const auto* values=static_cast<const std::uint8_t*>(data);
        if(colour_output || row_bytes==camera.width) std::memcpy(mask.data(),values,mask.size());
        else for(std::size_t y=0;y<camera.height;++y)
            std::memcpy(mask.data()+y*camera.width,values+y*row_bytes,camera.width);
        const D3D12_RANGE empty{0,0}; readback.resource->Unmap(0,&empty);
    }
};
#else
struct DxrShadows::Impl { std::string status{"Hardware DXR unavailable on this build"}; };
#endif
DxrShadows::DxrShadows(std::optional<std::array<std::uint8_t,8>> adapter_luid):impl_(std::make_unique<Impl>()) {
#if defined(STARFOX_DXR)
    impl_->requested_adapter=adapter_luid;
#else
    (void)adapter_luid;
#endif
}
DxrShadows::~DxrShadows()=default;
bool DxrShadows::render_reflections(const Scene& scene,Camera camera,
    const render::RayMaterials& materials,std::span<const std::uint32_t,256> palette,
    std::uint32_t environment,std::vector<std::uint8_t>& rgba) {
    resident_={};rgba.clear();
#if defined(STARFOX_DXR)
    if(materials.encoding!=render::RayMaterialEncoding::indexed
        || !scene.triangle_count() || materials.triangles.size()!=scene.triangle_count()
        || materials.triangles.size()>4'000'000 || materials.texels.size()>16'000'000
        || !camera.width || !camera.height || camera.width>16384 || camera.height>16384
        || !(camera.focal_length>0) || !(camera.vertical_focal_length()>0)) return false;
    for(const auto& m:materials.triangles) {
        if(m.textured>1 || m.even>255 || m.odd>255 || m.colour_base>255) return false;
        for(auto uv:m.uv) if(!std::isfinite(uv) || std::abs(uv)>1e8) return false;
        if(m.textured && (m.u_mask>4095 || m.v_mask>4095 || (m.u_mask&(m.u_mask+1))
            || (m.v_mask&(m.v_mask+1)) || std::uint64_t(m.offset)+std::uint64_t(m.u_mask+1)*(m.v_mask+1)>materials.texels.size())) return false;
    }
    if(!available()) return false;
    try {
        impl_->render(scene,camera,{0,1,0},{},rgba,true,nullptr,0,16,nullptr,false,false,&materials,palette.data(),environment);
        return true;
    }catch(const std::exception& error){impl_->status=std::string("DXR reflection failure: ")+error.what();rgba.clear();}
#else
    (void)scene;(void)camera;(void)materials;(void)palette;(void)environment;
#endif
    return false;
}
void* DxrShadows::export_geometry_completion_handle() {
#if defined(STARFOX_DXR)
    if(!available()) return nullptr;
    if(!impl_->geometry_fence && FAILED(impl_->device->CreateFence(0,D3D12_FENCE_FLAG_SHARED,
        IID_ID3D12Fence,reinterpret_cast<void**>(impl_->geometry_fence.GetAddressOf())))) return nullptr;
    HANDLE handle{};
    if(SUCCEEDED(impl_->device->CreateSharedHandle(impl_->geometry_fence.Get(),nullptr,GENERIC_ALL,nullptr,&handle))) return handle;
#endif
    return nullptr;
}
bool DxrShadows::wait_for_geometry(std::uint64_t value) {
#if defined(STARFOX_DXR)
    if(!value || !available() || !impl_->geometry_fence) return false;
    return SUCCEEDED(impl_->queue->Wait(impl_->geometry_fence.Get(),value));
#else
    (void)value;return false;
#endif
}
DxrShadows::ResidentGeometry DxrShadows::prepare_shared_geometry(std::uint32_t count) {
#if defined(STARFOX_DXR)
    if(!count || count%3 || count>12'000'000 || !available()) return {};
    try {
        impl_->await_producer();
        impl_->ensure(impl_->shared_geometry,uint64_t(count)*16,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON,true,true);
        ResidentGeometry result{impl_->shared_geometry.resource.Get(),count,16};
        LUID luid{};
#if defined(_MSC_VER)
        luid=impl_->device->GetAdapterLuid();
#else
        impl_->device->GetAdapterLuid(&luid);
#endif
        std::memcpy(result.adapter_luid.data(),&luid,sizeof(luid));
        const auto value=++impl_->serial;
        check(impl_->queue->Signal(impl_->fence.Get(),value),"DXR geometry ready signal");
        result.ready_value=value;
        return result;
    } catch(const std::exception& error) {impl_->status=error.what();}
#else
    (void)count;
#endif
    return {};
}
void* DxrShadows::export_geometry_handle() {
#if defined(STARFOX_DXR)
    if(impl_->shared_geometry.resource && !impl_->failed) {
        HANDLE handle{};
        if(SUCCEEDED(impl_->device->CreateSharedHandle(impl_->shared_geometry.resource.Get(),nullptr,GENERIC_ALL,nullptr,&handle))) return handle;
    }
#endif
    return nullptr;
}
void* DxrShadows::export_ready_fence_handle() {
#if defined(STARFOX_DXR)
    if((!resident_.resource && !impl_->shared_geometry.resource) || impl_->failed) return nullptr;
    HANDLE handle{};
    try {
        auto& p=*impl_;
        if(resident_.resource && !p.output_common) {
            p.await_producer();
            check(p.allocator->Reset(),"DXR export allocator");
            check(p.list->Reset(p.allocator.Get(),nullptr),"DXR export command");
            D3D12_RESOURCE_BARRIER release{};release.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            release.Transition={p.output.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON};
            p.list->ResourceBarrier(1,&release);
            check(p.list->Close(),"DXR export close");
            ID3D12CommandList* lists[]{p.list.Get()};p.queue->ExecuteCommandLists(1,lists);
            const auto value=++p.serial;
            check(p.queue->Signal(p.fence.Get(),value),"DXR export signal");
            check(p.fence->SetEventOnCompletion(value,p.event),"DXR export completion");
            if(WaitForSingleObject(p.event,5000)!=WAIT_OBJECT_0) throw std::runtime_error("DXR export timeout");
            check(p.device->GetDeviceRemovedReason(),"DXR export device");
            p.output_common=true;resident_.ready_value=value;
        }
    } catch(const std::exception& error) {
        impl_->status=error.what();impl_->failed=true;resident_={};return nullptr;
    }
    if(FAILED(impl_->device->CreateSharedHandle(impl_->fence.Get(),nullptr,GENERIC_ALL,nullptr,&handle))) return nullptr;
    return handle;
#else
    return nullptr;
#endif
}
void* DxrShadows::export_resident_handle() {
#if defined(STARFOX_DXR)
    if(!resident_.resource || impl_->failed) return nullptr;
    HANDLE handle{};
    const auto result=impl_->device->CreateSharedHandle(impl_->output.resource.Get(),nullptr,GENERIC_ALL,nullptr,&handle);
    if(FAILED(result)) {impl_->status="DXR shared-output handle export failed";return nullptr;}
    return handle;
#else
    return nullptr;
#endif
}
const std::string& DxrShadows::status() const { return impl_->status; }
std::size_t DxrShadows::last_backdrop_upload_bytes() const noexcept {
#if defined(STARFOX_DXR)
    return impl_->backdrop_upload_bytes;
#else
    return 0;
#endif
}
std::uint64_t DxrShadows::working_image_bytes() const noexcept {
#if defined(STARFOX_DXR)
    return (impl_->output.resource?impl_->output.capacity:0)
        +(impl_->readback.resource?impl_->readback.capacity:0);
#else
    return 0;
#endif
}
bool DxrShadows::hardware_supported() {
#if defined(STARFOX_DXR)
    if(impl_->failed) return false;
    try {if(!impl_->capability_attempted) responsive_prepare([&] {impl_->query_capability();});return true;}
    catch(const std::exception& error) {impl_->status=std::string("Hardware ray tracing unavailable: ")+error.what();impl_->failed=true;return false;}
#else
    return false;
#endif
}
bool DxrShadows::available() {
#if defined(STARFOX_DXR)
    if(impl_->failed) return false;
    try {if(!impl_->attempted) impl_->initialize();return true;}
    catch(const std::exception& error) {impl_->status=std::string("Hardware ray tracing unavailable: ")+error.what();impl_->failed=true;return false;}
#else
    return false;
#endif
}
bool DxrShadows::render(const Scene& scene,Camera camera,Vec3 light,
    std::optional<ReceiverPlane> ground,std::vector<std::uint8_t>& mask) {
    resident_={};
#if defined(STARFOX_DXR)
    if (impl_->failed) return false;
    if (!std::isfinite(dot(light,light)) || dot(light,light)<1e-20 || camera.focal_length<=0
        || camera.vertical_focal_length()<=0 || !std::isfinite(camera.vertical_focal_length())) return false;
    try {
        if (!impl_->attempted) impl_->initialize();
        impl_->render(scene,camera,light,ground,mask);
        return true;
    } catch (const std::exception& error) {
        impl_->status=std::string("CPU shadow fallback: ")+error.what();
        impl_->failed=true;
        return false;
    }
#else
    (void)scene; (void)camera; (void)light; (void)ground; (void)mask;
    return false;
#endif
}
bool DxrShadows::readback_resident(std::vector<std::uint8_t>& mask) {
    mask.clear();
#if defined(STARFOX_DXR)
    if(!resident_.resource || impl_->failed) return false;
    try {
        auto& p=*impl_;
        const auto bytes=std::size_t(resident_.row_bytes)*resident_.height;
        p.await_producer();
        p.ensure(p.readback,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        check(p.allocator->Reset(),"DXR download allocator");
        check(p.list->Reset(p.allocator.Get(),nullptr),"DXR download command");
        D3D12_RESOURCE_BARRIER transition{};
        transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Transition={p.output.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            p.output_common?D3D12_RESOURCE_STATE_COMMON:D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};
        p.list->ResourceBarrier(1,&transition);
        p.list->CopyBufferRegion(p.readback.resource.Get(),0,p.output.resource.Get(),0,bytes);
        std::swap(transition.Transition.StateBefore,transition.Transition.StateAfter);
        p.list->ResourceBarrier(1,&transition);
        check(p.list->Close(),"DXR download close");
        ID3D12CommandList* lists[]{p.list.Get()};p.queue->ExecuteCommandLists(1,lists);
        const auto value=++p.serial;
        check(p.queue->Signal(p.fence.Get(),value),"DXR download signal");
        check(p.fence->SetEventOnCompletion(value,p.event),"DXR download completion");
        if(WaitForSingleObject(p.event,5000)!=WAIT_OBJECT_0) throw std::runtime_error("DXR download timeout");
        check(p.device->GetDeviceRemovedReason(),"DXR download device");
        const auto pixel_row=std::size_t(resident_.width)*resident_.bytes_per_pixel;
        mask.resize(pixel_row*resident_.height);
        void* data{};const D3D12_RANGE range{0,bytes};
        check(p.readback.resource->Map(0,&range,&data),"DXR resident map");
        if(resident_.row_bytes==pixel_row)
            std::memcpy(mask.data(),data,mask.size());
        else for(std::size_t y=0;y<resident_.height;++y)
            std::memcpy(mask.data()+y*pixel_row,static_cast<const std::uint8_t*>(data)+y*resident_.row_bytes,pixel_row);
        const D3D12_RANGE empty{0,0};p.readback.resource->Unmap(0,&empty);
        return true;
    } catch(const std::exception& error) {
        impl_->status=std::string("DXR download failure: ")+error.what();impl_->failed=true;resident_={};mask.clear();
    }
#endif
    return false;
}
bool DxrShadows::work_complete() const noexcept {
#if defined(STARFOX_DXR)
    return !impl_->device || FAILED(impl_->device->GetDeviceRemovedReason())
        || !impl_->fence || impl_->fence->GetCompletedValue()>=impl_->serial;
#else
    return true;
#endif
}
bool DxrShadows::render_resident(const Scene& scene,Camera camera,Vec3 light,
    std::optional<ReceiverPlane> ground,const ResidentGeometry* geometry,const Coverage* coverage,bool release_for_external,bool defer_completion,const ReflectionInput* reflection,bool ground_only,std::optional<PrimaryRayRange> primary_range) {
    resident_={};
    if(primary_range && !primary_range->valid()) return false;
    if(ground_only && (!(reflection && reflection->coverage_only?reflection->ground:ground)
        || (reflection && !reflection->coverage_only))) return false;
    if(reflection && reflection->ground_only && (!reflection->ground || !reflection->water)) return false;
#if defined(STARFOX_DXR)
    if(impl_->failed || (defer_completion && !release_for_external) || (!geometry && !scene.triangle_count()) || !camera.width || !camera.height
        || !std::isfinite(dot(light,light)) || dot(light,light)<1e-20 || camera.focal_length<=0
        || camera.vertical_focal_length()<=0 || !std::isfinite(camera.vertical_focal_length())) return false;
    try {
        if(reflection) {
            if(reflection->history && (!reflection->history->valid() || !geometry || !geometry->vertex_count || geometry->stride!=16
                || !reflection->resident_materials || reflection->resident_materials!=geometry->resource
                || reflection->coverage_only || !reflection_history_receiver_valid(*reflection->history,
                    reflection->water,reflection->ground.has_value())
                || reflection->ground_only
                || !reflection_history_transport_valid(*reflection->history,reflection->roughness,reflection->metallic,reflection->specular_models)
                || !reflection->materials || reflection->materials->encoding!=render::RayMaterialEncoding::native_rgba
                || std::getenv("STARFOX_TEST_DXR_STABLE_HITS"))) return false;
            // Native material/output colour encoding is needed for transmitted
            // rays even with reflection OFF and no environment-cube allocation.
            if(reflection->cube_encoding>2 || (reflection->cube_encoding && !reflection->face_size
                && (!reflection->materials || reflection->materials->encoding!=render::RayMaterialEncoding::native_rgba))) return false;
            if(reflection->coverage_only && (reflection->water || reflection->background || reflection->ground_only || reflection->specular_models
                || !reflection->materials || reflection->materials->encoding!=render::RayMaterialEncoding::native_rgba)) return false;
            if(!std::isfinite(reflection->background_eye_x)) return false;
            if(reflection->water) {
                const auto& water=*reflection->water;
                if(!reflection->ground || !std::isfinite(water.time) || !std::isfinite(water.reflection_strength)
                    || !std::isfinite(water.brightness) || water.brightness<0 || water.brightness>1
                    || water.reflection_strength<0 || water.reflection_strength>1 || water.material>3 || water.caustics>3) return false;
                for(auto value:water.world_to_view) if(!std::isfinite(value)) return false;
                for(auto value:water.camera_position) if(!std::isfinite(value)) return false;
                if(water.source_colour) {
                    if(!reflection->materials || reflection->materials->encoding!=render::RayMaterialEncoding::native_rgba || water.material!=0) return false;
                    for(float value:*water.source_colour) if(!std::isfinite(value) || value<0 || value>1) return false;
                }
            }
            if(reflection->ground && (!std::isfinite(dot(reflection->ground->point,reflection->ground->point))
                || !std::isfinite(dot(reflection->ground->normal,reflection->ground->normal))
                || dot(reflection->ground->normal,reflection->ground->normal)<1e-20)) return false;
            if(reflection->background && (!reflection->background->ppu || reflection->background->settings.layer!=2
                || reflection->background->settings.unique_regions.size()>1024 || reflection->face_size)) return false;
            if(coverage || ground || !reflection->materials || reflection->palette.size()!=256
                || !std::isfinite(reflection->roughness) || reflection->roughness<0 || reflection->roughness>1
                || reflection->metallic>3) return false;
            if(reflection->resident_cube_offset) {
                if(!reflection->resident_materials || !reflection->materials
                    || reflection->materials->encoding!=render::RayMaterialEncoding::native_rgba
                    || !reflection->cube_encoding || reflection->coverage_only || reflection->background || !reflection->environment_cube.empty()
                    || reflection->resident_cube_offset<reflection->resident_material_bytes || reflection->resident_cube_offset%4
                    || reflection->face_size<8 || reflection->face_size>512 || (reflection->face_size&(reflection->face_size-1))) return false;
            } else if(reflection->face_size>1024 || reflection->environment_cube.size()!=
                std::size_t(reflection->face_size)*reflection->face_size*6) return false;
            for(unsigned row=0;row<3;++row) for(unsigned other=0;other<3;++other) {
                float product=0;
                for(unsigned axis=0;axis<3;++axis) product+=reflection->environment_rotation[row*3+axis]
                    *reflection->environment_rotation[other*3+axis];
                if(!std::isfinite(product) || std::abs(product-(row==other?1.f:0.f))>.01f) return false;
            }
            const auto& m=*reflection->materials;
            if(m.encoding!=render::RayMaterialEncoding::indexed && m.encoding!=render::RayMaterialEncoding::native_rgba) return false;
            if(m.encoding==render::RayMaterialEncoding::native_rgba
                && (!reflection->resident_materials || !m.triangles.empty() || !m.texels.empty())) return false;
            const auto count=geometry?geometry->vertex_count/3:scene.triangle_count();
            if(m.encoding==render::RayMaterialEncoding::native_rgba && (reflection->resident_material_bytes<std::uint64_t(count)*64
                || reflection->resident_material_bytes>std::uint64_t(count)*64+16'000'000
                || reflection->resident_material_bytes%4)) return false;
            if((!reflection->resident_materials && m.triangles.size()!=count) || count>4'000'000 || m.texels.size()>16'000'000) return false;
            for(const auto& t:m.triangles) {
                if(t.textured>1 || t.even>255 || t.odd>255 || t.colour_base>255) return false;
                for(auto uv:t.uv) if(!std::isfinite(uv) || std::abs(uv)>1e8f) return false;
                if(t.textured && (t.u_mask>4095 || t.v_mask>4095 || (t.u_mask&(t.u_mask+1))
                    || (t.v_mask&(t.v_mask+1)) || std::uint64_t(t.offset)+std::uint64_t(t.u_mask+1)*(t.v_mask+1)>m.texels.size())) return false;
            }
        }
        if(coverage) {
            const auto count=geometry?geometry->vertex_count/3:scene.triangle_count();
            if(coverage->triangles.size()!=count || count>4000000 || coverage->texels.size()>16000000) return false;
            for(const auto& c:coverage->triangles) {
                if(c.flags>1) return false;
                for(auto uv:c.uv) if(!std::isfinite(uv) || std::abs(uv)>1e8f) return false;
                if(c.flags && (c.u_mask>4095 || c.v_mask>4095 || (c.u_mask&(c.u_mask+1)) || (c.v_mask&(c.v_mask+1))
                    || std::uint64_t(c.offset)+std::uint64_t(c.u_mask+1)*(c.v_mask+1)>coverage->texels.size())) return false;
            }
        }
        if(!impl_->attempted) impl_->initialize();
        auto* external=geometry?static_cast<ID3D12Resource*>(geometry->resource):nullptr;
        auto* external_materials=reflection?static_cast<ID3D12Resource*>(reflection->resident_materials):nullptr;
        if(external_materials) {
            const auto offset=reflection->resident_material_offset;
            if(offset%16 || (external_materials==external && std::uint64_t(offset)<std::uint64_t(geometry->vertex_count)*geometry->stride)) return false;
            D3D12_RESOURCE_DESC desc{};
#if defined(_MSC_VER)
            desc=external_materials->GetDesc();
#else
            external_materials->GetDesc(&desc);
#endif
            ComPtr<ID3D12Device5> owner;
            const auto count=geometry?geometry->vertex_count/3:scene.triangle_count();
            const auto bytes=reflection->resident_material_bytes?reflection->resident_material_bytes:std::uint64_t(count)*64;
            const auto extent=reflection->resident_cube_offset?std::uint64_t(reflection->resident_cube_offset)
                +std::uint64_t(reflection->face_size)*reflection->face_size*6*4:bytes;
            if(bytes<std::uint64_t(count)*64 || desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER || desc.Width<std::uint64_t(offset)+bytes
                || extent<bytes || desc.Width<std::uint64_t(offset)+extent
                || FAILED(external_materials->GetDevice(IID_ID3D12Device5,reinterpret_cast<void**>(owner.GetAddressOf())))
                || owner.Get()!=impl_->device.Get()) return false;
            if(reflection->history && (reflection->history->previous_vertex_offset<std::uint64_t(offset)+extent
                || std::uint64_t(reflection->history->previous_vertex_offset)+std::uint64_t(geometry->vertex_count)*16>desc.Width)) return false;
            if(reflection->history && reflection->history->previous_index_offset
                && (reflection->history->previous_index_offset<std::uint64_t(reflection->history->previous_vertex_offset)
                    +std::uint64_t(geometry->vertex_count)*16
                    || std::uint64_t(reflection->history->previous_index_offset)+std::uint64_t(geometry->vertex_count/3)*4>desc.Width)) return false;
        }
        if(geometry) {
            const bool empty_native=geometry->vertex_count==0 && reflection && reflection->materials
                && reflection->materials->encoding==render::RayMaterialEncoding::native_rgba
                && external_materials && reflection->resident_material_bytes;
            if(!external || (!geometry->vertex_count && !empty_native) || geometry->vertex_count%3
                || geometry->stride<12 || geometry->stride>256 || geometry->stride%4) return false;
            D3D12_RESOURCE_DESC desc{};
#if defined(_MSC_VER)
            desc=external->GetDesc();
#else
            external->GetDesc(&desc);
#endif
            ComPtr<ID3D12Device5> owner;
            if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER
                || (geometry->vertex_count && desc.Width<uint64_t(geometry->vertex_count-1)*geometry->stride+12)
                || FAILED(external->GetDevice(IID_ID3D12Device5,reinterpret_cast<void**>(owner.GetAddressOf()))) || owner.Get()!=impl_->device.Get()) return false;
        }
        std::vector<std::uint8_t> unused;
        impl_->render(scene,camera,light,reflection?reflection->ground:ground,unused,false,external,geometry?geometry->vertex_count:0,geometry?geometry->stride:16,coverage,release_for_external,defer_completion,
            reflection?reflection->materials:nullptr,reflection?reflection->palette.data():nullptr,reflection?reflection->environment:0,
            reflection?reflection->roughness:0,reflection?reflection->metallic:0,
            reflection?reflection->environment_cube:std::span<const std::uint32_t>{},reflection?reflection->face_size:0,
            reflection?reflection->environment_rotation:std::array<float,9>{1,0,0,0,1,0,0,0,1},
            reflection?reflection->background:nullptr,reflection?reflection->background_eye_x:0,external_materials,
            reflection?reflection->resident_material_offset:0,reflection?reflection->water:nullptr,
            ground_only || (reflection && reflection->ground_only),primary_range,
            reflection?reflection->resident_material_bytes:0,reflection && reflection->coverage_only,
            reflection?reflection->resident_cube_offset:0,reflection?reflection->cube_encoding:0,reflection && reflection->specular_models,
            reflection && reflection->history?&*reflection->history:nullptr);
        const bool colour_output=reflection && !reflection->coverage_only;
        resident_={impl_->device.Get(),impl_->output.resource.Get(),camera.width,camera.height,colour_output?camera.width*4:(camera.width+3U)&~3U};
        resident_.bytes_per_pixel=colour_output?4:1;
        if(colour_output && reflection->materials->encoding==render::RayMaterialEncoding::native_rgba
            && reflection->water && reflection->water->source_colour
            && (reflection->water->auxiliary_layers || reflection->water->surface_layers))
            resident_.water_layers=*native_water_layers(camera.width,camera.height,reflection->water->auxiliary_layers);
        if(colour_output && reflection->history)
            resident_.reflection_history=*native_reflection_history(camera.width,camera.height,reflection->history->extent,
                reflection->history->separated,resident_.water_layers,reflection->history->model_lobes,reflection->history->model_paths,reflection->history->scene_paths,reflection->history->curved_paths,reflection->history->curved_receivers);
        LUID luid{};
#if defined(_MSC_VER)
        luid=impl_->device->GetAdapterLuid();
#else
        impl_->device->GetAdapterLuid(&luid);
#endif
        static_assert(sizeof(luid)==8);
        std::memcpy(resident_.adapter_luid.data(),&luid,sizeof(luid));
        resident_.ready_value=impl_->serial;
        return true;
    } catch(const std::exception& error) {
        impl_->status=std::string("DXR resident failure: ")+error.what();impl_->failed=true;
    }
#else
    (void)scene;(void)camera;(void)light;(void)ground;
    (void)geometry;(void)coverage;(void)release_for_external;(void)defer_completion;(void)reflection;
#endif
    return false;
}
}
