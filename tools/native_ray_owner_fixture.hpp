#pragma once
#include "starfox/render/vulkan_hardware_rt.hpp"
#include "starfox/render/vulkan_ray_support.hpp"

namespace starfox::render::shadows {
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
// Diagnostic API adapter only: every render/output/lifetime operation below
// goes through the same SdlDxrShadows owner used by the calibrated renderer.
// The existing input-only oracles and fault gates are not changed.
class NativeRayOwner {
    SdlDxrShadows owner_{true};
public:
    SdlDxrShadows& adapter() noexcept {return owner_;}
    bool available(void* device) const noexcept {return query_vulkan_ray_query(device).available;}
    bool render_shadows(void* device,const Scene& scene,Camera camera,Vec3 light,
        std::optional<ReceiverPlane> plane,const GpuScene::RayGeometryOutput* geometry=nullptr,
        bool ground_only=false,std::optional<PrimaryRayRange> range=std::nullopt) {
        return owner_.render_resident(device,scene,camera,light,plane,geometry,ground_only,range);
    }
    bool render_reflections(void* device,const GpuScene::RayGeometryOutput& geometry,Camera camera,
        std::span<const std::uint32_t,256> palette,std::uint32_t environment,
        std::uint8_t quality,float roughness,std::uint32_t metallic,std::optional<ReceiverPlane> ground,
        const GpuBackgroundDraw* background=nullptr,const RayWater* water=nullptr,bool ground_only=false,
        unsigned encoding=0,std::optional<PrimaryRayRange> range=std::nullopt,
        const ResidentEnvironmentCube* cube=nullptr,bool specular_models=false,const RayReflectionHistory* history=nullptr) {
        camera.quality=quality;
        return owner_.render_reflections(device,camera,geometry,palette,environment,roughness,metallic,{},
            cube?cube->face_size:0,cube?cube->rotation:std::array<float,9>{1,0,0,0,1,0,0,0,1},
            background,ground,0,water,ground_only,range,cube?cube->relative_offset:0,encoding,specular_models,history);
    }
    GpuShadowOutput shadow_output() const noexcept {return owner_.output();}
    GpuReflectionOutput reflection_output() const noexcept {return owner_.reflection_output();}
    const std::string& status() const noexcept {return owner_.status();}
    bool native_work_complete() const noexcept {return owner_.native_work_complete();}
    std::uint64_t working_image_bytes() const noexcept {return owner_.working_image_bytes();}
    bool try_release_device() noexcept {return owner_.try_release_device();}
    void release_device() noexcept {owner_.release_device();}
};
#else
using NativeRayOwner=VulkanHardwareRt;
#endif
}
