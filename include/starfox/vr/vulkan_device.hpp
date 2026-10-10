#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#include <openxr/openxr_platform.h>
#include <string>
#include <array>
#include <optional>

namespace starfox::vr {
// Owns the graphics objects created through XR_KHR_vulkan_enable2. The XR
// instance and Vulkan loader must outlive this object; destroy sessions first.
class VulkanDevice {
public:
    ~VulkanDevice();
    VulkanDevice()=default;
    VulkanDevice(const VulkanDevice&)=delete;
    VulkanDevice& operator=(const VulkanDevice&)=delete;
    bool initialize(XrInstance, XrSystemId, PFN_vkGetInstanceProcAddr,
        PFN_xrGetInstanceProcAddr=xrGetInstanceProcAddr);
    void close() noexcept;
    const XrGraphicsBindingVulkan2KHR& binding() const noexcept {return binding_;}
    VkQueue queue() const noexcept {return queue_;}
    std::uint32_t api_version() const noexcept {return version_;}
    std::uint32_t timestamp_valid_bits() const noexcept {return timestamp_valid_bits_;}
    double timestamp_period_ns() const noexcept {return timestamp_period_ns_;}
    bool external_shadows_enabled() const noexcept {return external_shadows_;}
    const std::optional<std::array<uint8_t,8>>& adapter_luid() const noexcept {return luid_;}
    const std::string& status() const noexcept {return status_;}
private:
    XrGraphicsBindingVulkan2KHR binding_{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    VkQueue queue_{};
    std::uint32_t version_{};
    std::uint32_t timestamp_valid_bits_{};
    double timestamp_period_ns_{};
    bool external_shadows_{};
    std::optional<std::array<uint8_t,8>> luid_;
    PFN_vkDestroyInstance destroy_instance_{};
    PFN_vkDestroyDevice destroy_device_{};
    PFN_vkDeviceWaitIdle wait_idle_{};
    std::string status_{"VR Vulkan device not initialized"};
};
}
