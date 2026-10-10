#pragma once

#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/portable_shadows.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include <memory>
#include <span>
#include <string>

namespace starfox::render::shadows {

struct ResidentEnvironmentCube;

// Native Metal acceleration-structure traversal. Unlike PortableShadows this
// class uses Metal's ray intersector and keeps its result in an SDL GPU buffer.
// The SDL Metal interop hook is pinned to our SDL version by CMake.
class MetalHardwareRt {
public:
    MetalHardwareRt();
    ~MetalHardwareRt();
    MetalHardwareRt(const MetalHardwareRt&) = delete;
    MetalHardwareRt& operator=(const MetalHardwareRt&) = delete;

    [[nodiscard]] bool available(void* sdl_device) const noexcept;
    bool render_shadows(void* sdl_device, const Scene& scene,
        const render::GpuScene::RayGeometryOutput* resident_geometry,
        Camera camera, Vec3 light, std::optional<ReceiverPlane> ground,bool ground_only=false,
        std::optional<PrimaryRayRange> primary_range=std::nullopt);
    bool render_reflections(void* sdl_device,
        const render::GpuScene::RayGeometryOutput& resident_geometry,
        Camera camera, std::span<const std::uint32_t,256> palette,
        std::uint32_t environment, std::uint8_t quality, float roughness,
        std::uint32_t metallic, std::optional<ReceiverPlane> ground,
        const RayWater* water=nullptr,const GpuBackgroundDraw* background=nullptr,bool ground_only=false,
        std::optional<PrimaryRayRange> primary_range=std::nullopt,
        unsigned colour_encoding=0,const ResidentEnvironmentCube* resident_environment=nullptr,
        bool specular_models=false);
    [[nodiscard]] GpuShadowOutput shadow_output() const noexcept;
    [[nodiscard]] GpuReflectionOutput reflection_output() const noexcept;
    [[nodiscard]] const std::string& status() const noexcept;
    void release_device() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace starfox::render::shadows
