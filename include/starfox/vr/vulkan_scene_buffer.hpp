#pragma once
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include <span>
namespace starfox::vr {
// Immutable uploaded vertex batch. Topology/count validation belongs to the
// drawing pipeline. GPU use must finish before close or
// reinitialization. Both eyes may reference the same batch.
class VulkanSceneBuffer {
public:
    VulkanSceneBuffer()=default;
    ~VulkanSceneBuffer();
    VulkanSceneBuffer(const VulkanSceneBuffer&)=delete;
    VulkanSceneBuffer& operator=(const VulkanSceneBuffer&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
        std::span<const SceneVertex>);
    void close() noexcept;
    VkBuffer buffer() const noexcept {return buffer_;}
    uint32_t count() const noexcept {return count_;}
    const std::string& status() const noexcept {return status_;}
private:
    VkDevice device_{};VkBuffer buffer_{};VkDeviceMemory memory_{};
    uint32_t count_{};bool mapped_{};
    PFN_vkDestroyBuffer destroy_{};PFN_vkFreeMemory free_{};PFN_vkUnmapMemory unmap_{};
    std::string status_{"Scene buffer not initialized"};
};
}
