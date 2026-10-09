#pragma once
#include "starfox/vr/vulkan_span_pipeline.hpp"
#include "starfox/vr/source_span_model.hpp"
namespace starfox::vr {
// Borrows the combined arena and axis pipeline. Caller supplies a projection
// write-to-read barrier before record(), and completes GPU use before close().
class VulkanAxisBindings {
public:
    VulkanAxisBindings()=default;
    ~VulkanAxisBindings();
    VulkanAxisBindings(const VulkanAxisBindings&)=delete;
    VulkanAxisBindings& operator=(const VulkanAxisBindings&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,VkBuffer,uint64_t,
        const SourceSpanArenaLayout&,const SourceAxisArenaLayout&,
        const SourceAxisInputs&,const VulkanSpanPipeline&);
    bool record(VkCommandBuffer) const;
    void close() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    VkDevice device_{};VkDescriptorPool pool_{};
    PFN_vkDestroyDescriptorPool destroy_{};
    PFN_vkCmdPipelineBarrier barrier_{};
    const VulkanSpanPipeline* pipeline_{};
    std::array<VkDescriptorSet,3> sets_{};
    std::string status_{"Axis bindings not initialized"};
};
// Borrows a combined source+warp arena and pipelines in PRNG/decode/expand
// order. Complete GPU work before destruction. Outputs stay in that arena.
class VulkanWarpBindings {
public:
    ~VulkanWarpBindings();
    VulkanWarpBindings()=default;
    VulkanWarpBindings(const VulkanWarpBindings&)=delete;
    VulkanWarpBindings& operator=(const VulkanWarpBindings&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,VkBuffer,uint64_t,
        const SourceSpanArenaLayout&,const SourceWarpArenaLayout&,
        const SourceWarpInputs&,const std::array<const VulkanSpanPipeline*,3>&);
    bool record(VkCommandBuffer) const;
    void close() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    VkDevice device_{};VkDescriptorPool pool_{};uint32_t capacity_{};
    PFN_vkDestroyDescriptorPool destroy_{};
    PFN_vkCmdPipelineBarrier barrier_{};
    std::array<const VulkanSpanPipeline*,3> pipelines_{};
    std::array<std::array<VkDescriptorSet,3>,3> sets_{};
    std::string status_{"Warp bindings not initialized"};
};
// Descriptor lifetime is independent of the borrowed buffer and pipelines.
// Complete GPU use before close/reinitialization. Pipeline array uses the
// SourceComputeStage enum order: spans, clip, projection, visibility, BSP.
// Supply a layout checked against this physical device's alignment/range
// limits by layout_source_span_model; initialize checks allocation bounds.
class VulkanSourceBindings {
public:
    ~VulkanSourceBindings();
    VulkanSourceBindings()=default;
    VulkanSourceBindings(const VulkanSourceBindings&)=delete;
    VulkanSourceBindings& operator=(const VulkanSourceBindings&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,VkBuffer,uint64_t,
        const SourceSpanArenaLayout&,const std::array<const VulkanSpanPipeline*,5>&,bool unclipped=false,
        const SourceWarpArenaLayout* warp=nullptr);
    void close() noexcept;
    std::array<VkDescriptorSet,3> sets(SourceComputeStage stage) const noexcept {
        const auto index=static_cast<size_t>(stage);
        return index<sets_.size()?sets_[index]:std::array<VkDescriptorSet,3>{};
    }
    const std::string& status() const noexcept {return status_;}
private:
    VkDevice device_{};VkDescriptorPool pool_{};
    PFN_vkDestroyDescriptorPool destroy_{};
    std::array<std::array<VkDescriptorSet,3>,5> sets_{};
    std::string status_{"Source bindings not initialized"};
};
}
