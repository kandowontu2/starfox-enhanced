#pragma once
#include "starfox/vr/vulkan_source_bindings.hpp"
#include "starfox/vr/vulkan_source_storage.hpp"
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include "starfox/vr/vulkan_scene_textures.hpp"
#include <memory>
namespace starfox::vr {
struct SourceGraphicsPipelines {
    VulkanPipelineCache* cache{};
    VkDevice device{};VkRenderPass pass{};
    VulkanScenePipeline triangles,lines;
    bool triangles_ready{},lines_ready{};
};
// Per-model compute resources. Pipelines are shared/borrowed and must remain
// initialized while recording compute work. Caller owns submission/fences:
// finish GPU use before closing, reinitializing or diagnostic readback.
// Serialize uses of this single output arena; do not record a new generation
// while a previous generation is still being consumed by either eye.
class VulkanSourceModel {
public:
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
        const VkPhysicalDeviceLimits&,const SourceSpanModel&,
        const std::array<const VulkanSpanPipeline*,5>&,
        const SourceWarpInputs* warp=nullptr,
        const std::array<const VulkanSpanPipeline*,3>* warp_pipelines=nullptr,
        const SourceAxisInputs* axis=nullptr,const VulkanSpanPipeline* axis_pipeline=nullptr);
    bool record(VkCommandBuffer) const;
    // Same-layout update reuses allocation and descriptor sets. Validation
    // failures preserve current contents; wait for both eyes before calling.
    bool update(const SourceSpanModel&,const SourceWarpInputs* warp=nullptr,const SourceAxisInputs* axis=nullptr);
    // Bytes submitted by the most recent successful full input update.
    size_t updated_input_bytes() const noexcept {return updated_input_bytes_;}
    // Call only after the recorded command completes on the GPU, not merely
    // after recording: an unsubmitted command produces no reusable output.
    void compute_completed() noexcept {if(compute_recorded_) compute_dirty_=false;compute_recorded_=false;}
    bool compute_needed() const noexcept {return compute_dirty_;}
    // Palette-only frame changes (fades/flashes) preserve GPU geometry and
    // span outputs. Like update(), call only after both eyes finish reading.
    // Flags must match the existing mode; changing mode needs update().
    bool update_palette(std::span<const uint32_t,256>);
    // Prepare once per compatible render pass, outside active GPU use.
    // Only immutable corner templates are uploaded; geometry stays resident.
    bool prepare_graphics(VkRenderPass,float units,const SceneVertex& material,
        std::shared_ptr<SourceGraphicsPipelines> shared={});
    // Call inside the eye render pass, after record() and its barriers.
    bool record_graphics(VkCommandBuffer,VkExtent2D,const EyeCamera&) const;
    void close() noexcept;
    VkBuffer buffer() const noexcept {return storage_.buffer();}
    // Borrow resident projection/corner inputs for downstream ray expansion.
    // No readback. Caller inserts compute-write -> compute-read synchronization
    // and retains this model until consumption completes. Axis/warp outputs
    // need their own topology path and explicitly reject, never silently omit.
    // Explicit candidate mode exposes warp's original (not view-culled)
    // topology. Consumers must still resolve material/transparent coverage;
    // these ranges alone are not a finished opaque shadow-caster list.
    bool ray_source_ranges(std::array<VkDescriptorBufferInfo,3>& output,
        bool include_warp_candidates=false) const noexcept;
    const SourceSpanArenaLayout& layout() const noexcept {return layout_;}
    const SourceWarpArenaLayout& warp_layout() const noexcept {return warp_layout_;}
    const SourceAxisArenaLayout& axis_layout() const noexcept {return axis_layout_;}
    bool readback_axis(SourceAxisRegion,std::span<std::byte>);
    bool readback_warp(SourceWarpRegion,std::span<std::byte>);
    bool readback(SourceSpanRegion,std::span<std::byte>);
    // Immutable slot/corner templates; shader fetches positions from the arena.
    // Material is uniform across slots here; source palette binding is separate.
    bool cover_geometry(float units,const SceneVertex& material,std::vector<SceneVertex>&);
    const std::string& status() const noexcept {return status_;}
private:
    VulkanSourceStorage storage_;
    VulkanSourceBindings bindings_; // destroyed before borrowed storage
    VulkanWarpBindings warp_bindings_;
    VulkanAxisBindings axis_bindings_;
    SourceAxisArenaLayout axis_layout_;
    bool axis_{};
    SourceWarpArenaLayout warp_layout_;
    bool warp_{};
    VulkanSceneTextures graphics_textures_;
    VulkanSceneBuffer graphics_vertices_;
    std::shared_ptr<SourceGraphicsPipelines> graphics_pipelines_;
    VulkanSceneBuffer line_vertices_;
    VkDevice device_{};
    PFN_vkGetDeviceProcAddr get_{};
    VkPhysicalDeviceMemoryProperties memory_{};
    SourceSpanArenaLayout layout_;
    std::array<const VulkanSpanPipeline*,5> pipelines_{};
    std::array<uint32_t,5> counts_{};
    bool unclipped_{};
    bool has_lines_{};
    bool mixed_{};
    uint32_t corner_capacity_{3};
    size_t updated_input_bytes_{};
    bool compute_dirty_{true};
    mutable bool compute_recorded_{};
    std::vector<SourceSpanInputWrite> inputs_,input_scratch_;
    std::vector<VulkanSourceStorage::Write> upload_views_;
    PFN_vkCmdPipelineBarrier barrier_{};
    std::string status_{"Source model not initialized"};
};
}
