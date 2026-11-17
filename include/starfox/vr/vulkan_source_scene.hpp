#pragma once
#include "starfox/vr/source_models.hpp"
#include "starfox/vr/vulkan_draw_packets.hpp"
#include <memory>
#include <span>
namespace starfox::vr {
class VulkanPipelineCache;
// Combined ordered legacy/compute scene. Caller completes both eyes before
// initialize/close; record_compute is outside a render pass and precedes draws.
class VulkanSourceScene {
public:
    struct RaySource {
        std::array<VkDescriptorBufferInfo,3> ranges{};
        Matrix4 placement{};
        float units{};
        uint64_t generation{};
    };
    struct LegacyRaySource {VulkanDrawPackets::RaySource geometry;uint64_t generation{};};
    VulkanSourceScene();
    ~VulkanSourceScene();
    // MODERN (set before the first initialize): scenery the cartridge paints
    // over regardless of depth (vr::painter_scenery) draws first in native
    // order with the depth test always passing, then everything else
    // depth-tested. `moving` (source handles that moved since the previous
    // tick) never paint.
    void set_modern(bool enabled) noexcept {modern_=enabled;}
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
        const VkPhysicalDeviceLimits&,VkRenderPass,const SourceModelPackets&,VulkanPipelineCache* cache=nullptr,
        std::span<const uint16_t> moving={});
    bool record_compute(VkCommandBuffer) const;
    // Set only after a complete replacement shadow pass is ready for this eye.
    // Failure/fallback paths leave this false so native shadows remain visible.
    bool record(VkCommandBuffer,VkExtent2D,const EyeCamera&,bool replacement_shadows_ready=false) const;
    // Borrow by full pass key, not packet position. Valid only until the next
    // initialize attempt or close; record_compute + a compute read barrier
    // must precede expansion. No ownership or completed-GPU-work is implied.
    bool ray_source(uint32_t key,RaySource&,bool include_warp_candidates=false) const;
    bool legacy_ray_source(uint32_t key,LegacyRaySource&) const;
    // MODERN: whether a source packet is painter scenery (vr::painter_scenery).
    bool painter(std::size_t packet) const noexcept;
    void close() noexcept;
    bool reused_resources() const noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    struct State;
    std::unique_ptr<State> state_;
    std::string status_;
    uint64_t generation_{};
    bool modern_{};
};
}
