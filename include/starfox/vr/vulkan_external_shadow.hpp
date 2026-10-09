#pragma once
#include "starfox/vr/vulkan_scene_textures.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include <array>
#include <cstdint>

namespace starfox::vr {
// Borrowed device must outlive this import. Caller must finish all GPU use and
// release EXTERNAL ownership before close, destruction, or producer reuse.
// NT handles are borrowed during initialize; the caller always closes them.
class VulkanExternalShadow {
public:
    ~VulkanExternalShadow() {close();}
    VulkanExternalShadow()=default;
    VulkanExternalShadow(const VulkanExternalShadow&)=delete;
    VulkanExternalShadow& operator=(const VulkanExternalShadow&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,VkPhysicalDevice,
        PFN_vkGetPhysicalDeviceProperties2,const std::array<std::uint8_t,8>& producer_luid,
        void* resource_handle,void* fence_handle,VkDeviceSize bytes);
    void close() noexcept;
    VkBuffer buffer() const noexcept {return buffer_;}
    VkSemaphore ready() const noexcept {return ready_;}
    // Compatible with VulkanScenePipeline's packed-R8 shadow fragment mode.
    // Descriptor and buffer share this import's lifetime; no mask upload.
    VkDescriptorSetLayout layout() const noexcept {return textures_.layout();}
    VkDescriptorSet descriptor() const noexcept {return textures_.descriptor();}
    // Prepare once after import. Repeating the identical configuration reuses
    // resources. No allocation/upload in record_draw; caller supplies GPU sync.
    bool prepare_draw(const VkPhysicalDeviceMemoryProperties&,VkRenderPass,uint32_t width,uint32_t height,
        std::array<float,4> color,SceneBlend blend=SceneBlend::shadow);
    bool record_draw(VkCommandBuffer,VkExtent2D) const;
private:
    VulkanSceneBuffer vertices_;
    VulkanScenePipeline pipeline_;
    VkRenderPass pass_{};
    uint32_t width_{},height_{};
    std::array<float,4> color_{};
    SceneBlend blend_{};
    VkDeviceSize bytes_{};
    VulkanSceneTextures textures_;
    VkDevice device_{};
    PFN_vkGetDeviceProcAddr get_{};
    VkBuffer buffer_{};
    VkDeviceMemory memory_{};
    VkSemaphore ready_{};
};
}
