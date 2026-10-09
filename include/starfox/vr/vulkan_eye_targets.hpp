#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <span>
#include <string>
#include <vector>
namespace starfox::vr {
// Owns views/framebuffers, never the runtime-owned swapchain images. Destroy
// after GPU work completes, before the swapchains and Vulkan device.
class VulkanEyeTargets {
public:
    ~VulkanEyeTargets();
    VulkanEyeTargets()=default;
    VulkanEyeTargets(const VulkanEyeTargets&)=delete;
    VulkanEyeTargets& operator=(const VulkanEyeTargets&)=delete;
    bool initialize(VkDevice, PFN_vkGetDeviceProcAddr, VkFormat,
        const std::array<std::span<const VkImage>,2>&,
        const std::array<VkExtent2D,2>&,
        VkFormat depth_format=VK_FORMAT_UNDEFINED,
        const std::array<VkImageView,2>& depth_views={});
    // Optional caller-owned depth views outlive these framebuffers. A depth
    // view may be reused within an eye only when submissions are serialized.
    bool has_depth() const noexcept {return depth_;}
    void close() noexcept;
    VkRenderPass render_pass() const noexcept {return pass_;}
    VkFramebuffer framebuffer(unsigned eye,unsigned image) const noexcept;
    VkExtent2D extent(unsigned eye) const noexcept {return eye<2?extents_[eye]:VkExtent2D{};}
    const std::string& status() const noexcept {return status_;}
private:
    VkDevice device_{};
    VkRenderPass pass_{};
    bool depth_{};
    std::array<std::vector<VkImageView>,2> views_;
    std::array<std::vector<VkFramebuffer>,2> frames_;
    std::array<VkExtent2D,2> extents_{};
    PFN_vkDestroyImageView destroy_view_{};
    PFN_vkDestroyFramebuffer destroy_frame_{};
    PFN_vkDestroyRenderPass destroy_pass_{};
    std::string status_{"Eye targets not initialized"};
};
}
