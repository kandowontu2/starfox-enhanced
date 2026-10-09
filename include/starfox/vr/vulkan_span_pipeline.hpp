#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <string>
namespace starfox::vr {
enum class SourceComputeStage {spans,continuous_clip,continuous_projection,continuous_visibility,bsp,colour_warp,warp_material,warp_expand,axis,ray_expand};
// ray_expand: four inputs (points/residuals/corners/triangle indices), one
// float4 vertex output, 64-byte settings, 64 lanes (one triangle per lane).
// Native Vulkan binding of the shared source span compute shader. Caller owns
// descriptor pools/buffers, synchronization and in-flight resource lifetimes.
// Sets: 0 = four readonly storage buffers; 1 = commands/masks storage buffers;
// 2 = the shader's 64-byte Settings uniform buffer.
// continuous_clip instead uses six readonly buffers in set 0, one output
// buffer in set 1, and its 32-byte Settings in set 2. Its output is the shared
// 129-int4 clipped polygon format consumed by the spans stage.
// continuous_projection uses two inputs (vertices/poses), two outputs
// (points/residuals), and a 16-byte Settings block; it dispatches 64 lanes.
// continuous_visibility uses points/triples, one visibility output, and a
// 16-byte Settings block, also with 64 lanes.
// bsp uses nodes/visibility/faces/trees, ordered faces/results outputs, and
// a 32-byte Settings block with 32 lanes. count is the tree count.
// colour_warp uses four inputs, two outputs and 16-byte Settings. It is a
// serial ordered PRNG stage: record with count=1, capacity lives in Settings.
// warp_material uses six inputs, one output and 64-byte Settings; 64 lanes
// decode per-occurrence descriptors without collapsing repeated face IDs.
// warp_expand uses eight inputs, three outputs, 32-byte Settings and 32 lanes.
// axis uses points/indices/residuals, two endpoint outputs, 48-byte Settings
// and two lanes. Record count=2; reduction uses authored extrema membership.
class VulkanSpanPipeline {
public:
    VulkanSpanPipeline()=default;
    ~VulkanSpanPipeline();
    VulkanSpanPipeline(const VulkanSpanPipeline&)=delete;
    VulkanSpanPipeline& operator=(const VulkanSpanPipeline&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,SourceComputeStage=SourceComputeStage::spans,
        bool fast_compile=false,VkPipelineCache cache=VK_NULL_HANDLE);
    void close() noexcept;
    VkDescriptorSetLayout descriptor_layout(unsigned set) const noexcept {
        return set<sets_.size()?sets_[set]:VK_NULL_HANDLE;
    }
    // count must match Settings.count; settings dimensions and buffer bounds
    // are validated by the producer. Call outside a render pass.
    bool record(VkCommandBuffer,const std::array<VkDescriptorSet,3>&,uint32_t count) const;
    const std::string& status() const noexcept {return status_;}
    SourceComputeStage stage() const noexcept {return stage_;}
private:
    SourceComputeStage stage_{SourceComputeStage::spans};
    VkDevice device_{};VkPipeline pipeline_{};VkPipelineLayout layout_{};
    uint32_t group_width_{32};
    std::array<VkDescriptorSetLayout,3> sets_{};
    PFN_vkDestroyPipeline destroy_pipeline_{};
    PFN_vkDestroyPipelineLayout destroy_layout_{};
    PFN_vkDestroyDescriptorSetLayout destroy_set_{};
    PFN_vkCmdBindPipeline bind_{};
    PFN_vkCmdBindDescriptorSets bind_sets_{};
    PFN_vkCmdDispatch dispatch_{};
    std::string status_{"Span pipeline not initialized"};
};
}
