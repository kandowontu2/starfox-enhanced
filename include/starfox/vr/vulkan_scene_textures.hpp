#pragma once
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include <span>
namespace starfox::vr {
// Immutable packed RGBA texels. Finish all GPU use before replacing/closing.
class VulkanSceneTextures {
public:
    ~VulkanSceneTextures();
    VulkanSceneTextures()=default;
    VulkanSceneTextures(const VulkanSceneTextures&)=delete;
    VulkanSceneTextures& operator=(const VulkanSceneTextures&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,std::span<const uint32_t>);
    // Bind a caller-owned storage buffer without uploading or taking ownership.
    // Caller supplies the actual byte size, synchronizes producer writes, and
    // keeps the buffer alive until all draws and this descriptor are finished.
    bool initialize_external(VkDevice,PFN_vkGetDeviceProcAddr,VkBuffer,VkDeviceSize bytes);
    void close() noexcept;
    VkDescriptorSetLayout layout() const {return layout_;}
    VkDescriptorSet descriptor() const {return set_;}
    const std::string& status() const {return status_;}
private:
    void bind_storage(const VkDescriptorBufferInfo&);
    VkDevice device_{};VkBuffer buffer_{};VkDeviceMemory memory_{};
    VkDescriptorPool pool_{};VkDescriptorSetLayout layout_{};VkDescriptorSet set_{};
    PFN_vkGetDeviceProcAddr get_{};
    std::string status_;
};
}
