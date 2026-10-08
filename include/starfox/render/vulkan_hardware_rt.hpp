#pragma once
#include "starfox/render/portable_shadows.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include <array>
#include <memory>
#include <span>
#include <string>

namespace starfox::render::shadows {
// Native calibrated environment in the SAME submitted allocation as geometry.
// Offset is byte-relative to material_offset and must equal material_bytes.
// Six square +X,-X,+Y,-Y,+Z,-Z faces, 8..512 power-of-two; colour_encoding
// supplies its linear/sRGB format. Rotation rows map eye rays into cube space.
struct ResidentEnvironmentCube {
    std::uint32_t relative_offset{},face_size{};
    std::array<float,9> rotation{1,0,0,0,1,0,0,0,1};
};
// Vulkan acceleration structures + ray-query compute. The SDL GPU buffer
// stays resident for the existing shadow compositor; unsupported devices
// simply leave the portable traversal path available.
class VulkanHardwareRt {
public:
    VulkanHardwareRt();
    ~VulkanHardwareRt();
    VulkanHardwareRt(const VulkanHardwareRt&)=delete;
    VulkanHardwareRt& operator=(const VulkanHardwareRt&)=delete;
    bool available(void* sdl_device) const noexcept;
    // Underlay mode ignores model primary hits, not secondary shadow casters.
    // Requires a ground receiver; invalid requests clear the borrowed output.
    // Resident indexed texture coverage keeps source texel-zero cutouts; CPU
    // geometry-only scenes and indexed solid inks use opaque traversal. Native
    // resident kind2/kind3 records use their actual binary RGBA coverage.
    // Explicit range clips primary +Z depth only (including water layers).
    // Omitting it retains legacy limits; secondary transport is never clipped.
    // Empty CPU caster scenes produce clear masks. Resident empty native scenes
    // require material_offset >=16 aligned to 16, material_bytes >=16 aligned
    // to 4, and complete resident native metadata without CPU records/texels.
    bool render_shadows(void* sdl_device,const Scene&,Camera,Vec3,
        std::optional<ReceiverPlane>,const GpuScene::RayGeometryOutput* geometry=nullptr,bool ground_only=false,
        std::optional<PrimaryRayRange> primary_range=std::nullopt);
    // colour_encoding: 0 legacy indexed gamma approximation, 1 native linear,
    // 2 native sRGB. Calibrated water source colour requires explicit 1/2.
    // Optional water layers share the primary buffer/fence and canonical ABI.
    // Native 1/2 uses calibrated sharp/eight-lobe transport and conductor Fresnel.
    // specular_models enables the same material response at secondary model hits;
    // it requires native materials and explicit 1/2, not a legacy palette route.
    // Header-only native geometry supports analytic/cube receivers without
    // inventing a caster; incomplete/malformed batches are still rejected.
    // Optional sharp MODEL-only history carries accepted secondary feature
    // motion/explicit identities in the canonical 52-byte layout. Compact
    // MODEL lobes and ordered MODEL or MODEL/planar paths have distinct
    // canonical records. Separated mirror/gold ground history has the 88-byte
    // layout with current base/response. Curved water/lava history uses its
    // canonical liquid prefix and the actual previous optical frame.
    bool render_reflections(void* sdl_device,
        const GpuScene::RayGeometryOutput& geometry,Camera camera,
        std::span<const std::uint32_t,256> palette,std::uint32_t environment,
        std::uint8_t quality,float roughness,std::uint32_t metallic,
        std::optional<ReceiverPlane> ground,const GpuBackgroundDraw* background=nullptr,
        const RayWater* water=nullptr,bool ground_only=false,unsigned colour_encoding=0,
        std::optional<PrimaryRayRange> primary_range=std::nullopt,
        const ResidentEnvironmentCube* resident_environment=nullptr,bool specular_models=false,
        const RayReflectionHistory* history=nullptr);
    GpuShadowOutput shadow_output() const noexcept;
    GpuReflectionOutput reflection_output() const noexcept;
    const std::string& status() const noexcept;
    // No waits/readbacks. Unfenced failures acquire an ordered recovery marker,
    // never replay the failed native work. External SDL consumers are caller-owned.
    bool native_work_complete() const noexcept;
    // SDL output image capacities, including retained larger water layouts;
    // excludes source/AS/scratch/driver allocations, not available VRAM.
    std::uint64_t working_image_bytes() const noexcept;
    // Nonblocking cleanup; pending/error completion retains resources. Both
    // cleanup calls invalidate borrowed outputs. Device must outlive retries.
    bool try_release_device() noexcept;
    // Explicit blocking cleanup. A failed wait retains the native resources,
    // rather than treating timeout/device error as permission to destroy them.
    void release_device() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
