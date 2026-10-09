#pragma once
#include "starfox/render/shadow_mask.hpp"
#include "starfox/render/ray_materials.hpp"
#include "starfox/render/native_water_layers.hpp"
#include "starfox/render/ray_reflection_history.hpp"
#include <memory>
#include <string>
#include <array>
#include <span>

namespace starfox::render { struct GpuBackgroundDraw; }
namespace starfox::render::shadows {
// World-anchored analytic water receiver. Palette classification is applied
// by the compositor, so UI and non-water ground cannot become reflective.
struct RayWater {
    float time{};
    float reflection_strength{};
    unsigned material{}; // 0 water, 1 mirror ground, 2 gold ground, 3 lava.
    bool mirror_models{}; // Secondary hits must follow the model's mirror, not its base colour.
    std::array<float,9> world_to_view{1,0,0,0,1,0,0,0,1};
    std::array<float,3> camera_position{};
    float brightness{1.f};
    unsigned caustics{}; // Off/low/medium/high, only transmitted water light.
    // Optional live authored ground colour for calibrated native water. This
    // is three palette values, never a CPU eye image or backdrop readback.
    std::optional<std::array<float,3>> source_colour;
    // Request a ray-finished world underlay and visible water normal/depth.
    // No allocation or extra liquid trace when no consumer needs these layers.
    bool auxiliary_layers{};
    // Depth/style consumers need only visible guides, not the hidden-liquid
    // ray workload. auxiliary_layers takes precedence when a shutter needs it.
    bool surface_layers{};
};
inline bool reflection_history_receiver_valid(const RayReflectionHistory& history,
    const RayWater* water,bool ground) noexcept {
    if(!history.valid()) return false;
    if(history.model_lobes) {
        if(history.curved_paths) return ground && water
            && ((history.curved_receivers && water->material==0)
                || (!water->auxiliary_layers && !water->surface_layers))
            && history.previous_liquid->material==water->material
            && (water->material==0?water->source_colour.has_value():water->material==3 && !water->source_colour);
        if(!history.scene_paths) return !water && !ground;
        return ground && water && (water->material==1 || water->material==2)
            && !water->source_colour && !water->auxiliary_layers && !water->surface_layers;
    }
    if(!history.separated) return !water && !ground;
    if(!water || !ground || water->mirror_models) return false;
    if(water->material==1 || water->material==2)
        return !history.previous_liquid && !water->source_colour
            && !water->auxiliary_layers && !water->surface_layers;
    if(water->material!=0 && water->material!=3) return false;
    if(history.previous_ground || (history.previous_liquid
        && history.previous_liquid->material!=water->material)) return false;
    if(water->material==0) return water->source_colour.has_value();
    return !water->source_colour && !water->auxiliary_layers && !water->surface_layers;
}
inline bool reflection_history_transport_valid(const RayReflectionHistory& history,
    float roughness,unsigned metallic,bool specular_models) noexcept {
    if(!history.valid() || !std::isfinite(roughness) || roughness<0 || roughness>1 || metallic>3
        || (specular_models && !history.model_paths)) return false;
    return history.model_lobes?history.model_lobes==(roughness>0?8U:1U):roughness==0 && metallic==0;
}
// Optional Windows DXR 1.1 backend. Unsupported devices/platforms return false;
// callers retain the CPU implementation. Unchanged geometry may reuse its BLAS.
class DxrShadows {
public:
    struct ResidentOutput {
        void* device{};void* resource{};
        std::uint32_t width{},height{},row_bytes{};
        std::array<std::uint8_t,8> adapter_luid{};
        std::uint64_t ready_value{};
        std::uint32_t bytes_per_pixel{1}; // Shadow=1, reflected RGBA=4.
        NativeWaterLayers water_layers{};
        NativeReflectionHistory reflection_history{};
    };
    struct ResidentGeometry {
        void* resource{}; // ID3D12Resource on this producer's device, COMMON state.
        std::uint32_t vertex_count{},stride{16}; // Triangle list, float3 positions.
        std::array<std::uint8_t,8> adapter_luid{};
        std::uint64_t ready_value{}; // Initial shared-fence value, before first trace.
    };
    struct TriangleCoverage {
        std::array<float,6> uv{};
        std::uint32_t offset{},u_mask{},v_mask{},flags{}; // flags: 0 opaque, 1 alpha texture
    };
    struct Coverage {
        std::span<const TriangleCoverage> triangles;
        std::span<const std::uint32_t> texels; // RGBA8, alpha in high byte; zero alpha is a hole.
    };
    struct ReflectionInput {
        const render::RayMaterials* materials{};
        std::span<const std::uint32_t> palette; // Exactly 256 RGBA entries.
        std::uint32_t environment{};
        float roughness{}; // 0: sharp mirror; bounded deterministic ray cone otherwise.
        std::uint32_t metallic{}; // 0 dielectric, 1 palette conductor, 2 gold, 3 copper.
        // Optional camera-space cube, faces +X,-X,+Y,-Y,+Z,-Z. Each face is
        // face_size squared packed RGBA pixels. Scene hits take precedence.
        std::span<const std::uint32_t> environment_cube;
        std::uint32_t face_size{};
        // Camera ray -> cube-space direction, row-major. Identity preserves
        // legacy camera-space cubes; world-aligned cubes use inverse view.
        std::array<float,9> environment_rotation{1,0,0,0,1,0,0,0,1};
        // Borrowed authored BG2 tile source; uploaded as tiles, never a HUD screenshot.
        const render::GpuBackgroundDraw* background{};
        // Camera-space physical receiver, only for scenes with real ground.
        std::optional<ReceiverPlane> ground;
        float background_eye_x{}; // Eye-space hit -> central authored camera.
        // Optional ID3D12Resource on this producer's device, COMMON state.
        // Exactly one 64-byte RayMaterial per triangle; native RGBA texels may
        // follow those records within resident_material_bytes. Caller
        // synchronizes GPU writes before dispatch. Indexed CPU materials
        // supply their legacy texels; native metadata contains no CPU pixels.
        void* resident_materials{};
        std::uint32_t resident_material_offset{}; // 16-byte aligned, may follow geometry.
        const RayWater* water{};
        // Trace the liquid/metal ground hidden by foreground models, retaining
        // those models in reflected, transmitted and shadow rays.
        bool ground_only{};
        std::uint32_t resident_material_bytes{}; // Native records + RGBA texels, relative to offset.
        bool coverage_only{}; // Use native alpha coverage for a byte shadow mask, not reflections.
        // Optional cube in the same resident allocation, relative to
        // resident_material_offset, AFTER resident_material_bytes. No CPU cube
        // pixels; face_size and environment_rotation still define its sampling.
        std::uint32_t resident_cube_offset{};
        unsigned cube_encoding{}; // 0 legacy gamma approximation, 1 linear RGBA, 2 sRGB RGBA.
        // Secondary model hits transport the selected mirror/conductor rather
        // than returning a pre-material source colour. Independent of water.
        bool specular_models{};
        // Optional sharp, single-bounce model/analytic/liquid correspondence.
        // Liquid reprojection retains the actual accepted wave; invalid on rough,
        // multibounce and environment samples. No CPU geometry readback.
        std::optional<RayReflectionHistory> history{};
    };
    // When supplied, only this Windows adapter may produce shared output.
    // Never falls back to a different GPU if the requested adapter lacks DXR.
    explicit DxrShadows(std::optional<std::array<std::uint8_t,8>> adapter_luid=std::nullopt);
    ~DxrShadows();
    DxrShadows(const DxrShadows&) = delete;
    DxrShadows& operator=(const DxrShadows&) = delete;
    // Capability-only menu probe: no queues, shader pipelines, acceleration
    // structures or rendering buffers. available() prepares the render path.
    [[nodiscard]] bool hardware_supported();
    [[nodiscard]] bool available();
    // Nonblocking native queue completion. A removed device cannot execute
    // further work. Owners still confirm any external producer/copy queue.
    [[nodiscard]] bool work_complete() const noexcept;
    bool render(const Scene&, Camera, Vec3 light, std::optional<ReceiverPlane>,
        std::vector<std::uint8_t>&);
    // Diagnostic first-bounce reflected RGBA. Materials follow Scene::triangles
    // order; do not build/reorder the Scene after preparing them. Transparent
    // output means no primary surface; misses reflect the supplied environment.
    bool render_reflections(const Scene&,Camera,const render::RayMaterials&,
        std::span<const std::uint32_t,256> palette,std::uint32_t environment,
        std::vector<std::uint8_t>& rgba);
    // Native D3D12 buffer, not an SDL buffer. Normally completed GPU work, UAV state
    // (COMMON when release_for_external is selected);
    // borrowed until the next render/destruction. No mask readback or copy.
    // Optional geometry replaces Scene input without a CPU vertex upload.
    // Caller finishes external writes/releases ownership first. Resource is
    // borrowed until completion and returned to COMMON by the trace commands.
    // release_for_external folds the UAV->COMMON release into the trace
    // submission; exporting its fence then requires no second GPU submission.
    // defer_completion requires release_for_external. The consumer must wait
    // on exported ready_value before reading; producer reuse waits internally.
    // ground_only selects the revealed plane as primary receiver while models
    // remain shadow casters. Requires a ground plane and no ReflectionInput.
    // An explicit primary range applies calibrated near/far depth planes to
    // model and ground receivers only; secondary/shadow casters are not clipped.
    bool render_resident(const Scene&,Camera,Vec3,std::optional<ReceiverPlane>,const ResidentGeometry* geometry=nullptr,const Coverage* coverage=nullptr,bool release_for_external=false,bool defer_completion=false,const ReflectionInput* reflection=nullptr,bool ground_only=false,std::optional<PrimaryRayRange> primary_range=std::nullopt);
    // Explicit diagnostic/fallback download; presentation need not call this.
    bool readback_resident(std::vector<std::uint8_t>&);
    // Diagnostic image transfer count; camera/palette metadata is separate.
    [[nodiscard]] std::size_t last_backdrop_upload_bytes() const noexcept;
    // Retained output/optional image readback allocations, including alignment
    // and a previous larger output. CPU metadata only: no initialize/wait/map.
    // Geometry, acceleration structures and driver storage are separate.
    [[nodiscard]] std::uint64_t working_image_bytes() const noexcept;
    [[nodiscard]] ResidentOutput resident_output() const noexcept {return resident_;}
    // Windows NT handle for current output. Caller must CloseHandle and, for
    // deferred completion, wait on the exported ready fence before consuming.
    // Consume before the next render; importing does not provide frame ownership.
    // Null on unsupported platforms or when no resident output is valid.
    [[nodiscard]] void* export_resident_handle();
    // Shared UAV triangle storage, fixed float4 stride. No vertex upload.
    // Complete all external use before resizing/reusing or destroying it.
    [[nodiscard]] ResidentGeometry prepare_shared_geometry(std::uint32_t vertex_count);
    // Independent incoming timeline: consumer submits its signal BEFORE asking
    // DXR to wait. This never advances/waits the outgoing producer serial.
    // Exported handle is owned by caller. No CPU geometry download or wait.
    void* export_geometry_completion_handle();
    bool wait_for_geometry(std::uint64_t value);
    [[nodiscard]] void* export_geometry_handle();
    // Releases valid output to COMMON and returns a caller-owned shared fence handle.
    // Also works after prepare_shared_geometry, before any output exists; use
    // the returned geometry's initial ready_value for that first submission.
    // Read ready_value AFTER this call. Consumer must finish and release external
    // ownership before any subsequent render/readback/destruction on this object.
    [[nodiscard]] void* export_ready_fence_handle();
    [[nodiscard]] const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ResidentOutput resident_{};
};
}
