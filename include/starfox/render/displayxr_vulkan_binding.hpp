#pragma once
#include "starfox/render/displayxr_runtime.hpp"
#include <SDL3/SDL_properties.h>
struct SDL_GPUDevice;

namespace starfox::render {
// OpenXR Vulkan2 creation owner. Configure before SDL device creation, then
// attach only that exact runtime-created device. SDL owns all Vulkan handles.
// No second graphics device/queue or global active-runtime override is used.
// close() revokes callbacks BEFORE the borrowed XR dispatch can be unloaded;
// property/device leases remain safe until SDL destroys their native handles.
// A session using binding() must be retired before close/device destruction.
class DisplayXrVulkanBinding {
public:
    DisplayXrVulkanBinding()=default;
    ~DisplayXrVulkanBinding();
    DisplayXrVulkanBinding(const DisplayXrVulkanBinding&)=delete;
    DisplayXrVulkanBinding& operator=(const DisplayXrVulkanBinding&)=delete;
    bool initialize(const DisplayXrRuntime&);
    // Graphics-only fixture/embedding API, not physical panel discovery.
    bool initialize_with_api(XrInstance,XrSystemId,PFN_xrGetInstanceProcAddr);
    bool configure_gpu_properties(SDL_PropertiesID) const;
    bool attach(SDL_GPUDevice*);
    const void* binding() const noexcept;
    void close() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    struct State;State* state_{};
    std::string status_{"Leia Vulkan graphics requirements not checked"};
};
}
