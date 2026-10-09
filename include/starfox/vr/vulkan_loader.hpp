#pragma once
#include <vulkan/vulkan.h>
#include <string>
namespace starfox::vr {
// Keep alive until all Vulkan devices and instances have been destroyed.
class VulkanLoader {
public:
    ~VulkanLoader();
    VulkanLoader()=default;
    VulkanLoader(const VulkanLoader&)=delete;
    VulkanLoader& operator=(const VulkanLoader&)=delete;
    bool initialize();
    void close() noexcept;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr() const noexcept {return get_;}
    const std::string& status() const noexcept {return status_;}
private:
    void* library_{};
    PFN_vkGetInstanceProcAddr get_{};
    std::string status_{"Vulkan loader not initialized"};
};
}
