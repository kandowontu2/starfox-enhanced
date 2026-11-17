#pragma once
#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/vulkan_draw_packets.hpp"
#include <vulkan/vulkan.h>
#include <functional>
#include <span>
#include <string>
#include <vector>
namespace starfox::vr {
class VulkanPipelineCache;
// PCVR MODERN's xBRZ fallback for native 2D art (the flat renderer's
// modern::SdlLayerFilter, for a headset): the sprite/HUD layer drawn at one
// texel per SNES pixel, upscaled with the desktop's xBRZ (the same
// layer_filter.hlsl compute shader), and drawn as one quad exactly where its
// sprites would be, so it keeps the HUD plane's depth in both eyes.
// Replacement (HD) art does not go through it. Keep alive until every eye
// that recorded it has completed.
class VulkanLayerFilter {
public:
    static constexpr unsigned canvas_width=256,canvas_height=224;
    VulkanLayerFilter()=default;
    ~VulkanLayerFilter();
    VulkanLayerFilter(const VulkanLayerFilter&)=delete;
    VulkanLayerFilter& operator=(const VulkanLayerFilter&)=delete;
    // eye_pass: the render pass record() draws in. srgb: the eye target is
    // sRGB (the layer then stores encoded colours, as the desktop filters them).
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
        VkRenderPass eye_pass,bool srgb,VulkanPipelineCache* cache=nullptr);
    // The layer's sprite packets and their canvas placement (the sprite
    // packets' source_ui_layer_matrix, canvas pixels to world). Only between
    // frames, like VulkanDrawPackets::initialize.
    bool upload(std::span<const DrawPacket> sprites,const Matrix4& layer);
    // Outside any render pass, before the eyes that draw it this frame: draw
    // the layer and filter it at `factor` output pixels per source pixel
    // (2-6, xBRZ's range). Recreates the output when the factor changes.
    bool filter(VkCommandBuffer,unsigned factor);
    // Inside the eye pass: the filtered layer on the HUD plane.
    bool record(VkCommandBuffer,VkExtent2D,const EyeCamera&) const;
    bool ready() const noexcept {return filtered_;}
    void close() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    void close_output() noexcept;
    bool create_output(unsigned factor);
    VkDevice device_{};PFN_vkGetDeviceProcAddr get_{};
    VkPhysicalDeviceMemoryProperties memory_{};
    bool srgb_{};
    // Device objects released by close(), newest first.
    std::vector<std::function<void()>> release_,release_output_;
    VkRenderPass layer_pass_{};VkFramebuffer framebuffer_{};
    VkImage source_{};VkImageView source_target_{},source_read_{};
    VkImage output_{};VkImageView output_view_{};
    unsigned factor_{};
    VkDescriptorSetLayout compute_layouts_[3]{},quad_layout_{};
    VkDescriptorPool pool_{};VkDescriptorSet compute_sets_[3]{},quad_set_{};
    VkPipelineLayout compute_layout_{},quad_pipeline_layout_{};
    VkPipeline compute_{},quad_{};
    VkSampler sampler_{};
    VkBuffer settings_{},empty_{};uint32_t* settings_words_{};
    VulkanDrawPackets sprites_;
    Matrix4 layer_{};EyeCamera layer_camera_{};
    bool has_sprites_{},filtered_{};
    PFN_vkCmdBeginRenderPass begin_pass_{};PFN_vkCmdEndRenderPass end_pass_{};
    PFN_vkCmdPipelineBarrier barrier_{};PFN_vkCmdBindPipeline bind_pipeline_{};
    PFN_vkCmdBindDescriptorSets bind_sets_{};PFN_vkCmdDispatch dispatch_{};
    PFN_vkCmdPushConstants push_{};PFN_vkCmdSetViewport viewport_{};PFN_vkCmdSetScissor scissor_{};
    PFN_vkCmdDraw draw_{};PFN_vkUpdateDescriptorSets update_sets_{};
    std::string status_{"Layer filter not initialized"};
};
}
