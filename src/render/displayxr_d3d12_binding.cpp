#include "starfox/render/displayxr_d3d12_binding.hpp"
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>
#include <wrl/client.h>
#include <array>
#include <stdexcept>

namespace starfox::render {
struct DisplayXrD3D12Binding::State {
    XrGraphicsBindingD3D12KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
};
DisplayXrD3D12Binding::DisplayXrD3D12Binding()=default;
DisplayXrD3D12Binding::~DisplayXrD3D12Binding()=default;
void DisplayXrD3D12Binding::close() noexcept {state_.reset();}
const void* DisplayXrD3D12Binding::binding() const noexcept {return state_?&state_->binding:nullptr;}
bool DisplayXrD3D12Binding::initialize(const DisplayXrRuntime& runtime,void* device,void* queue) {
    if(!runtime.detected() || runtime.backend()!=DisplayXrBackend::direct3d12) {
        close();status_="Confirmed D3D12 Leia runtime required";return false;
    }
    return initialize_with_api(runtime.instance(),runtime.system(),runtime.get_instance_proc(),device,queue);
}
bool DisplayXrD3D12Binding::initialize_with_api(XrInstance instance,XrSystemId system,
    PFN_xrGetInstanceProcAddr get,void* device_pointer,void* queue_pointer) {
    close();
    try {
        if(instance==XR_NULL_HANDLE || system==XR_NULL_SYSTEM_ID || !get || !device_pointer || !queue_pointer)
            throw std::runtime_error("Missing D3D12 runtime/device/queue");
        PFN_xrVoidFunction address{};
        auto result=get(instance,"xrGetD3D12GraphicsRequirementsKHR",&address);
        if(XR_FAILED(result) || !address) throw std::runtime_error("Runtime D3D12 requirements interface unavailable");
        XrGraphicsRequirementsD3D12KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
        result=reinterpret_cast<PFN_xrGetD3D12GraphicsRequirementsKHR>(address)(instance,system,&requirements);
        if(XR_FAILED(result)) throw std::runtime_error("D3D12 graphics requirements: OpenXR result "+std::to_string(result));
        auto* device=static_cast<ID3D12Device*>(device_pointer);
        auto* queue=static_cast<ID3D12CommandQueue*>(queue_pointer);
        LUID adapter{};
        D3D12_COMMAND_QUEUE_DESC description{};
#if defined(__MINGW32__)
        device->GetAdapterLuid(&adapter);queue->GetDesc(&description);
#else
        adapter=device->GetAdapterLuid();description=queue->GetDesc();
#endif
        if(adapter.LowPart!=requirements.adapterLuid.LowPart || adapter.HighPart!=requirements.adapterLuid.HighPart)
            throw std::runtime_error("Leia panel is connected to a different graphics adapter");
        Microsoft::WRL::ComPtr<ID3D12Device> queue_device;
        if(FAILED(queue->GetDevice(IID_ID3D12Device,reinterpret_cast<void**>(queue_device.GetAddressOf())))
            || queue_device.Get()!=device || description.Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)
            throw std::runtime_error("Leia must use the SDL device's direct graphics queue");
        // Microsoft's stable 12_2 enum value; older MinGW d3dcommon.h does
        // not name it, but the driver feature query still validates support.
        constexpr auto level_12_2=static_cast<D3D_FEATURE_LEVEL>(0xc200);
        constexpr std::array levels{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_12_0,D3D_FEATURE_LEVEL_12_1,level_12_2};
        bool valid_level=false;
        for(auto level:levels) valid_level|=requirements.minFeatureLevel==level;
        if(!valid_level) throw std::runtime_error("Runtime returned an invalid D3D12 feature requirement");
        D3D12_FEATURE_DATA_FEATURE_LEVELS support{};
        support.NumFeatureLevels=1;support.pFeatureLevelsRequested=&requirements.minFeatureLevel;
        if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,&support,sizeof(support)))
            || support.MaxSupportedFeatureLevel<requirements.minFeatureLevel)
            throw std::runtime_error("SDL device does not meet Leia's D3D12 feature requirement");
        state_=std::make_unique<State>();state_->binding.device=device;state_->binding.queue=queue;
        status_="Leia D3D12 requirements matched to the existing SDL device and queue";return true;
    } catch(const std::exception& error) {close();status_=error.what();return false;}
}
}
