#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <string>
namespace starfox::vr {
// One owned depth image per eye. Serial eye submissions may reuse each image
// across swapchain indices. Destroy eye framebuffers first, after GPU idle.
class VulkanDepthTargets {
public:
    VulkanDepthTargets()=default;
    ~VulkanDepthTargets();
    VulkanDepthTargets(const VulkanDepthTargets&)=delete;
    VulkanDepthTargets& operator=(const VulkanDepthTargets&)=delete;
    bool initialize(VkInstance,VkPhysicalDevice,VkDevice,PFN_vkGetInstanceProcAddr,
        const std::array<VkExtent2D,2>&);
    void close() noexcept;
    VkFormat format() const noexcept {return format_;}
    const std::array<VkImageView,2>& views() const noexcept {return views_;}
    const std::string& status() const noexcept {return status_;}
private:
    VkDevice device_{};VkFormat format_{VK_FORMAT_UNDEFINED};
    std::array<VkImage,2> images_{};
    std::array<VkDeviceMemory,2> memory_{};
    std::array<VkImageView,2> views_{};
    PFN_vkDestroyImage destroy_image_{};PFN_vkFreeMemory free_{};PFN_vkDestroyImageView destroy_view_{};
    std::string status_{"Depth targets not initialized"};
};
}
