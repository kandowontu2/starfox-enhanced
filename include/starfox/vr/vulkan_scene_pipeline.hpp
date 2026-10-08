#pragma once
#include "starfox/vr/eye_camera.hpp"
#include "starfox/vr/scene_types.hpp"
#include <vulkan/vulkan.h>
#include <string>
#include <cstdint>
namespace starfox::vr {
class VulkanPipelineCache;
// Flat-color triangle/line pass. Input positions are right-handed world coordinates
// in the same units as EyeCamera. Caller owns vertex buffers and GPU lifetime.
class VulkanScenePipeline {
public:
    ~VulkanScenePipeline();
    VulkanScenePipeline()=default;
    VulkanScenePipeline(const VulkanScenePipeline&)=delete;
    VulkanScenePipeline& operator=(const VulkanScenePipeline&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,VkRenderPass,bool depth_test=false,
        SceneTopology topology=SceneTopology::triangles,VkDescriptorSetLayout textures=VK_NULL_HANDLE,
        SceneBlend blend=SceneBlend::opaque,VulkanPipelineCache* cache=nullptr);
    void close() noexcept;
    bool record(VkCommandBuffer,VkExtent2D,VkBuffer,uint32_t vertices,const EyeCamera&,VkDescriptorSet textures=VK_NULL_HANDLE) const;
    // Draw a primitive-aligned subrange without uploading another buffer.
    // total_vertices is the allocation's vertex count, not the requested count.
    bool record_range(VkCommandBuffer,VkExtent2D,VkBuffer,uint32_t total_vertices,
        uint32_t first,uint32_t count,const EyeCamera&,VkDescriptorSet textures=VK_NULL_HANDLE) const;
    bool record_model(VkCommandBuffer,VkExtent2D,VkBuffer,uint32_t vertices,
        const EyeCamera&,const Matrix4& model,VkDescriptorSet textures=VK_NULL_HANDLE) const;
    const std::string& status() const noexcept {return status_;}
private:
    SceneTopology topology_{SceneTopology::triangles};
    bool textured_{};
    PFN_vkCmdBindDescriptorSets bind_descriptors_{};
    VkDevice device_{};VkPipeline pipeline_{};VkPipelineLayout layout_{};
    PFN_vkDestroyPipeline destroy_pipeline_{};
    PFN_vkDestroyPipelineLayout destroy_layout_{};
    PFN_vkCmdBindPipeline bind_pipeline_{};
    PFN_vkCmdBindVertexBuffers bind_vertices_{};
    PFN_vkCmdPushConstants push_{};
    PFN_vkCmdSetViewport viewport_{};
    PFN_vkCmdSetScissor scissor_{};
    PFN_vkCmdDraw draw_{};
    std::string status_{"Scene pipeline not initialized"};
};
}
