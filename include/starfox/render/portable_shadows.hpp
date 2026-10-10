#pragma once
#include "starfox/render/dxr_shadows.hpp"
namespace starfox::render::shadows {
struct GpuShadowOutput {
    void* device{};
    void* buffer{}; // One uint32 shade per pixel, borrowed until next render/release.
    std::uint32_t width{}, height{};
    // Zero: uint32 shades. Otherwise packed bytes with four-byte row alignment.
    std::uint32_t packed_row_bytes{};
};
// Compute BVH traversal for Vulkan/Metal devices without DXR. This is GPU
// ray tracing, not hardware ray-tracing-unit acceleration.
class PortableShadows {
public:
    PortableShadows();
    ~PortableShadows();
    bool render(const Scene&, Camera, Vec3, std::optional<ReceiverPlane>, std::vector<std::uint8_t>&);
    // Underlay mode ignores models as primary receivers, but retains every
    // model as a shadow caster onto the plane. A missing plane produces no mask.
    bool render_resident(void* device, const Scene&, Camera, Vec3, std::optional<ReceiverPlane>, bool ground_only = false);
    GpuShadowOutput output() const;
    bool readback(std::vector<std::uint8_t>&);
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
