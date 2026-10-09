#pragma once
#include "starfox/render/portable_shadows.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/dxr_shadows.hpp"
namespace starfox::render::shadows {
struct GpuReflectionOutput {
    void* device{};void* buffer{};
    std::uint32_t width{},height{},row_bytes{}; // RGBA8, not a shadow mask.
    NativeWaterLayers water_layers{};
    NativeReflectionHistory reflection_history{};
};
// Publishes native ray images for SDL consumers: the copied DXR image on
// Windows, or the resident Vulkan image on Linux. The historical class name
// is retained for callers. The borrowed device AND consumer submissions must
// outlive cleanup; unsupported backends decline instead of changing quality.
class SdlDxrShadows {
public:
    // Live XR owners retain failed submissions until their ordered SDL fence
    // AND native_work_complete() confirm cleanup. Legacy callers retain their
    // existing immediate cleanup policy.
    explicit SdlDxrShadows(bool retain_failed_submissions=false);
    ~SdlDxrShadows();
    // Optional complete scene geometry must have been submitted on this SDL
    // device, and remain borrowed through this call. No vertex readback.
    bool render_resident(void*,const Scene&,Camera,Vec3,std::optional<ReceiverPlane>,
        const GpuScene::RayGeometryOutput* geometry=nullptr,bool ground_only=false,
        std::optional<PrimaryRayRange> primary_range=std::nullopt);
    GpuShadowOutput output() const;
    bool render_reflections(void*,Camera,const GpuScene::RayGeometryOutput&,
        std::span<const std::uint32_t,256> palette,std::uint32_t environment,
        float roughness=0,std::uint32_t metallic=0,
        std::span<const std::uint32_t> environment_cube={},std::uint32_t face_size=0,
        std::array<float,9> environment_rotation={1,0,0,0,1,0,0,0,1},
        const GpuBackgroundDraw* background=nullptr,std::optional<ReceiverPlane> ground={},float background_eye_x=0,
        const RayWater* water=nullptr,bool ground_only=false,
        std::optional<PrimaryRayRange> primary_range=std::nullopt,
        std::uint32_t resident_cube_offset=0,unsigned cube_encoding=0,bool specular_models=false,
        const RayReflectionHistory* history=nullptr);
    GpuReflectionOutput reflection_output() const;
    bool readback(std::vector<std::uint8_t>&); // Explicit fallback/diagnostic only.
    // No wait. Pending native/copy work retains the owner for a later retry.
    // External SDL consumers must already be drained by the caller.
    bool try_release_device() noexcept;
    void release_device() noexcept;
    const std::string& status() const;
    bool native_work_complete() const noexcept; // no wait; external SDL work is caller-owned
    // Native output + any SDL copy/diagnostic image downloads. Includes
    // retained larger liquid layouts; imported aliases are not extra copies.
    // CPU metadata only, not a free-VRAM/geometry/driver-storage query.
    std::uint64_t working_image_bytes() const noexcept;
    // Set optional native Vulkan creation requirements on SDL properties.
    // Caller retries without these options if the adapter cannot provide them.
    static bool request_vulkan_interop(std::uint32_t properties);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool retain_failed_submissions_{};
};
}
