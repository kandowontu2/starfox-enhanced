#pragma once
#include "starfox/vr/eye_camera.hpp"
#include "starfox/vr/scene_types.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <cstdint>
namespace starfox::vr {
class VulkanPipelineCache;
// MODERN on a headset: reversed infinite depth (depth cleared to 0, tests
// compare GREATER, cameras from reversed_depth_camera) for uniform precision
// at every distance, so the draw-order bias resolves coplanar source faces.
// Process-wide: set once before creating pipelines or recording eyes.
void set_scene_reversed_depth(bool) noexcept;
[[nodiscard]] bool scene_reversed_depth() noexcept;
// MODERN's distance haze on a headset (vr::ModernFrame::fog): one small
// uniform buffer, bound at descriptor set 1 by every scene pipeline created
// while it is registered (set_scene_fog), whose fragments then fade toward its
// colour by distance. Each draw picks its mode in camera.effects.w bits 2-3
// (0 off, 1 everything, 2 the 3D landscape's ground and sky dome).
class VulkanSceneFog {
public:
    ~VulkanSceneFog();
    VulkanSceneFog()=default;
    VulkanSceneFog(const VulkanSceneFog&)=delete;
    VulkanSceneFog& operator=(const VulkanSceneFog&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&);
    // Colour in the target encoding (alpha = strength, 0 = off), start/end in
    // source units, and the eye cameras' near plane in metres. Only between
    // frames: every eye that recorded the previous values must have completed.
    void set(const std::array<float,4>& colour,float start,float end,float near_metres) noexcept;
    void clear() noexcept {set({},0,0,0);}
    void close() noexcept;
    VkDescriptorSetLayout layout() const noexcept {return layout_;}
    // Set 0 of untextured pipelines, which use set 1 only.
    VkDescriptorSetLayout empty_layout() const noexcept {return empty_;}
    VkDescriptorSet set() const noexcept {return set_;}
    const std::string& status() const noexcept {return status_;}
private:
    VkDevice device_{};
    VkDescriptorSetLayout layout_{},empty_{};VkDescriptorPool pool_{};VkDescriptorSet set_{};
    VkBuffer buffer_{};VkDeviceMemory memory_{};float* mapped_{};
    PFN_vkDestroyDescriptorSetLayout destroy_layout_{};PFN_vkDestroyDescriptorPool destroy_pool_{};
    PFN_vkDestroyBuffer destroy_buffer_{};PFN_vkFreeMemory free_{};
    std::string status_{"Scene fog not initialized"};
};
// Process-wide, like set_scene_reversed_depth: register (or clear with null)
// before creating the pipelines that should use it; the fog must outlive them.
void set_scene_fog(const VulkanSceneFog*) noexcept;
[[nodiscard]] const VulkanSceneFog* scene_fog() noexcept;
// MODERN's ray-traced shadows and lights on a headset (vr::VulkanRayShadows):
// its descriptor set, bound at set 2 by every textured scene pipeline created
// while it and the fog are registered (scene.hlsl's STARFOX_SCENE_RT variant).
// Process-wide like the fog; the binding must outlive those pipelines.
struct SceneRayBinding {VkDescriptorSetLayout layout{};VkDescriptorSet set{};};
void set_scene_rays(const SceneRayBinding*) noexcept;
[[nodiscard]] const SceneRayBinding* scene_rays() noexcept;
// Instances per line draw for MODERN's one-source-pixel brush (scene.hlsl
// headset_line_brush): 2N-1, N the target pixels per source pixel in
// camera.effects.w bits 8-19 (4.4 fixed point) rounded; 1 where unset.
[[nodiscard]] inline uint32_t scene_line_instances(const EyeCamera& camera) noexcept {
    const float pixel=float((camera.effects[3]>>8)&4095U)/16.F;
    return 2*uint32_t(std::clamp(std::floor(pixel+.5F),1.F,64.F))-1;
}
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
        SceneBlend blend=SceneBlend::opaque,VulkanPipelineCache* cache=nullptr,bool depth_write=true,
        // Depth-tested pipelines only: always pass the test (still writing
        // depth), for scenery the cartridge draws as a painter.
        bool painter=false);
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
    VkDescriptorSet fog_{},rays_{};
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
