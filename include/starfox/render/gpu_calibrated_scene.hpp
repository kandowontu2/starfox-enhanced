#pragma once
#include "starfox/vr/eye_camera.hpp"
#include "starfox/vr/scene_types.hpp"
#include "starfox/render/ray_materials.hpp"
#include <memory>
#include <span>
#include <string>
#include <optional>
#include <vector>
#include <limits>
namespace starfox::vr {struct DrawPacket;}
namespace starfox::render {
enum class CalibratedScenePhase {all,before_rays,after_rays,environment,world_only,upscale_scene,upscale_world};
// Real hardware sample coverage, not a spatial filter or an enlarged eye.
// The caller owns matching multisample colour/D32 targets. Optional resolve
// colour is single-sample, same format/extent, and must not alias either target.
// Retain samples when a later ordered pass will load this colour/depth pair.
struct CalibratedSceneMultisample {
    unsigned samples{1}; // Exactly 1, 2, 4 or 8; unsupported counts fail.
    void* resolve_color{};
    bool retain_samples{};
};
struct CalibratedSceneMotionDraw {
    vr::Matrix4 previous_model{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::optional<vr::EyeCamera> previous_camera_override;
    bool valid{}; // Explicit accepted primitive correspondence, never inferred.
    // Optional accepted primitive stream, in the SAME triangle or line order.
    // The native vertex shader evaluates its original destruction/billboard
    // payload with the previous model/eye; no CPU deformation/projection.
    // Empty keeps the existing explicit rigid-mapping path. Nonempty requires
    // valid correspondence and the exact current vertex count/topology.
    // Lines require this stream even when rigid: their interpolated endpoint
    // motion must not be inferred from a fictitious pixel-centre surface.
    std::span<const vr::SceneVertex> previous_vertices;
    // Explicit index in the accepted ray batch. Never infer from current draw
    // order: actors may be inserted, removed or reordered between frames.
    std::uint32_t previous_ray_first_triangle{std::numeric_limits<std::uint32_t>::max()};
};
struct CalibratedSceneMotion {
    vr::EyeCamera previous_camera;
    std::span<const CalibratedSceneMotionDraw> draws; // Exactly the uploaded draw count/order.
    unsigned previous_width{},previous_height{};
};
struct CalibratedSceneDraw {
    std::span<const vr::SceneVertex> vertices;
    std::span<const std::uint32_t> texels;
    vr::Matrix4 model{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    vr::SceneTopology topology{vr::SceneTopology::triangles};
    vr::SceneBlend blend{vr::SceneBlend::opaque};
    bool depth_test{true};
    std::optional<vr::EyeCamera> camera_override; // Explicit head/screen-locked layer.
    bool preserve_native_colour{};
    // Per-layer styling must not replace either eye's calibrated pose/FOV.
    std::optional<std::array<unsigned,4>> effects_override;
    bool ray_caster{}; // Explicit world-model selection, never inferred from UI/depth.
    bool after_rays{}; // Ordered compositor boundary, not inferred from layer/depth.
    bool reflection_environment{}; // Authored world scenery only, never HUD/models.
    bool reflective_material{}; // Explicit model receiver, not dielectric rock/UI.
    unsigned effect_layer{}; // 0 protected/native, 1 world, 2 model; exact raster ownership.
    bool ground_receiver{}; // Explicit native water/mirror/gold floor; never a model caster.
};
struct CalibratedScenePacket {
    const vr::DrawPacket* packet{};
    vr::SceneBlend blend{vr::SceneBlend::opaque};
    bool depth_test{true};
    std::optional<vr::EyeCamera> camera_override;
    std::optional<std::array<unsigned,4>> effects_override;
    bool ray_caster{};
    bool after_rays{};
    bool reflection_environment{};
    bool reflective_material{};
    unsigned effect_layer{};
    std::optional<vr::Matrix4> model_override; // Retained presentation-only world transform.
    bool ground_receiver{};
};
struct CalibratedRayGeometryOutput {
    void* device{};void* buffer{};
    std::uint32_t vertex_count{};
    bool complete{};
    // +Y down/+Z forward, in units_per_metre source units, matching the
    // existing ray backend. Full calibrated view/model transforms are on GPU.
    unsigned width{},height{};
    std::array<double,4> projection{}; // focal X/Y, centre X/Y in output pixels.
    double near_plane{};
    std::optional<double> far_plane;
    const RayMaterials* materials{}; // Encoding only; native records/texels are GPU resident.
    std::uint32_t material_offset{}; // One 64-byte native record per triangle.
    std::uint32_t material_bytes{}; // Records plus resident RGBA texels, relative to offset.
    std::uint32_t environment_offset{},environment_face_size{}; // Relative to material_offset, after its bytes.
    std::array<float,9> environment_rotation{1,0,0,0,1,0,0,0,1};
    // Optional accepted primitive correspondence, in exactly current ray order.
    // Float4 XYZ is the previous calibrated eye position in source units; W
    // is one only for a matched, previously visible primitive (zero otherwise).
    // This is not primary-receiver flow or history inferred from draw order.
    // Lives after materials/cube in the same borrowed per-eye allocation.
    std::uint32_t previous_vertex_offset{};
    std::uint32_t previous_index_offset{}; // One accepted primitive index per current triangle; UINT_MAX rejects.
    std::array<unsigned,2> previous_extent{};
    std::array<double,4> previous_projection{};
    double previous_near_plane{};
    std::optional<double> previous_far_plane;
};
// Full calibrated matrices, not the flat renderer's fixed focal-length/eye-X
// approximation. Uses the VR scene shader and unprojected SceneVertex ABI.
// Raw draws support solid/dithered geometry and ordinary textures/billboards.
// Native packets reuse VR validation, including tile backgrounds, sprites,
// fonts, photographs, source spans, dust/grid and shadow masks. Connected
// ground grids run the shared VR projection/binning passes on the GPU; only
// their fourteen source camera/matrix/endpoint words are uploaded.
class GpuCalibratedScene {
public:
    GpuCalibratedScene();~GpuCalibratedScene();
    GpuCalibratedScene(const GpuCalibratedScene&)=delete;
    GpuCalibratedScene& operator=(const GpuCalibratedScene&)=delete;
    bool initialize(void* sdl_device,int sdl_color_format);
    bool supports_multisample(unsigned samples,bool receiver=false,bool surfaces=false) const noexcept;
    // Copies source data during encoding. Caller owns the command, submits
    // once after both eyes, or cancels it on any failure. No submission/wait/
    // readback here. No simulation state advances between eyes.
    bool upload(void* command,std::span<const CalibratedSceneDraw>);
    bool upload_packets(void* command,std::span<const CalibratedScenePacket>);
    // world_only rasterizes the same uploaded world-labelled source packets
    // before the ordered overlay boundary, without any model/protected ink.
    // Its caller-owned colour/depth/optional guides form a same-frame blur
    // underlay. It is not the reflection-only environment selection and does
    // not reassemble, reproject on CPU, or upload a second background image.
    // After a SUCCESSFUL caller submission, confirm this upload's token. This
    // permits immutable artwork reuse on the same ordered GPU queue. Encoding
    // alone is not proof: cancelled/unsubmitted uploads are uploaded again.
    // An old/zero token cannot mark a newer upload ready.
    std::uint64_t upload_token() const noexcept;
    bool notify_submitted(std::uint64_t token) noexcept;
    // Vertex bytes uploaded, texture bytes uploaded, immutable bytes reused.
    std::array<std::uint64_t,3> upload_cost() const noexcept;
    // Target and D32 depth texture belong to the caller and must match the
    // initialized device/format and extent. Camera uses SDL's upward NDC Y
    // convention on BOTH backends (SDL flips Vulkan's viewport internally).
    // Presenter already adapts the shared VR downward projection once.
    // Full view, projection
    // and model transforms are applied in the GPU vertex shader.
    bool enqueue_eye(void* command,void* color,void* depth,unsigned width,
        unsigned height,const vr::EyeCamera&,std::array<float,4> clear={0,0,0,1},
        CalibratedScenePhase phase=CalibratedScenePhase::all,void* receiver=nullptr,
        std::uint64_t continuation_token=0,void* surfaces=nullptr,bool aa_ownership=false,
        const CalibratedSceneMultisample& multisample={},void* motion=nullptr,
        const CalibratedSceneMotion* previous=nullptr);
    // Optional fourth RGBA32_FLOAT MRT requires surface + receiver targets and
    // matching sample counts. XY is previous-minus-current pixel motion; Z is
    // previous forward depth in source units; W is validity. Optional matched
    // previous vertices include actual nonrigid/billboard correspondence.
    // Excluded/unknown
    // correspondence clears guides through the SAME depth/discard/alpha path.
    // No image transport, history acceptance, or pose-only motion substitute.
    // MSAA geometry/colour supports the same native draws and ordered phases.
    // MSAA receiver/surface MRTs retain exact per-sample labels/depth, never
    // averaged hardware resolves. They require the private desktop shader-read
    // capability and retained colour samples. Enqueue_sample extracts one
    // matching colour/receiver/surface/motion plane for native ray/effect composition.
    // Opt-in AA ownership requires the receiver. Its otherwise-unused alpha
    // channel becomes exact filterable flat-model coverage, clearing under
    // textures/UI/emissive ink. Disabled calls retain the old receiver bytes.
    // Optional RGBA32_FLOAT third target requires the receiver. It stores
    // eye-space normals (+Y down/+Z forward) and forward depth in source units,
    // with identical raster ownership. Sky/UI/emissive/blended ink writes zero.
    // Only requested depth effects allocate/use it; ordinary raster is unchanged.
    // before_rays clears colour/depth and optionally writes an RGBA8_UNORM
    // receiver target with the EXACT raster coverage/visibility. Excluded
    // foreground models clear it, so emissive geometry cannot inherit a
    // reflection from geometry behind it. after_rays loads colour/depth and
    // may also LOAD the receiver target, clearing history ownership only at
    // its visible protected UI/shutter fragments without erasing the world.
    // preserves source order (including early dust before backgrounds). A new
    // command may continue only the confirmed submitted upload's token (even
    // when SDL recycles a command-buffer address). after_rays always requires
    // this confirmed token; it is not allowed inside an unsubmitted upload.
    // Encodes complete selected opaque triangle/line casters from the SAME uploaded
    // vertices. No CPU transforms, readback, submission, or wait. The caller
    // must submit before a native ray consumer accesses the result. Independent
    // eye buffers survive encoding the other eye; borrowed until next upload,
    // release or encoding of this eye. False/empty on unsupported selection:
    // specialized/blended casters must not silently become opaque. Ordinary
    // textures support wrapping/clamping and binary alpha cutouts; fractional
    // alpha still rejects the entire selected batch.
    // Source lines become two finite, eye-facing triangles at one projected
    // raster pixel wide. GPU depth clipping prevents camera-crossing infinities;
    // full tracked view/FOV, destruction, visibility and native materials apply.
    // Solid/dithered material colours are evaluated from those same vertices
    // on GPU, using the raster colour-space and per-layer style overrides.
    // Ordinary RGBA and indexed source-billboard palette/texture words are
    // GPU-copied from the existing raster binding. Source sizing/capping is
    // shared with the headset shader, not approximated by a different quad.
    // Neither payload is repacked/read back on CPU; colours are styled at the hit.
    // Native RGBA records are currently consumed by DXR only; other backends
    // must decline this explicit encoding, not interpret it as palette indices.
    // Projection must be a canonical asymmetric perspective matrix using
    // SDL's upward NDC; unsupported projective terms are rejected, not dropped.
    CalibratedRayGeometryOutput enqueue_ray_geometry(void* command,unsigned eye,
        unsigned width,unsigned height,const vr::EyeCamera&,float units_per_metre=256,
        unsigned environment_face_size=0,std::array<float,4> environment_clear={0,0,0,1},
        const CalibratedSceneMotion* previous=nullptr);
    // Optional previous geometry uses the caller's accepted source matches,
    // evaluating destruction, billboard and one-pixel line ribbons on GPU with
    // the OLD eye/model/FOV. Invalid/new/recycled primitives have no witness.
    // An omitted history allocates/encodes no previous triangle stream. This
    // exports correspondence only; it does not yet transport reflected RGB.
    // Optional cube is rasterized from explicitly tagged native scenery into
    // the SAME resident allocation. All six faces use the original GPU source
    // bindings/styles and this eye's world position; no CPU image transfer.
    // Capture sizes are power-of-two 8..512. Zero keeps legacy no-cube behavior.
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    bool upload_impl(void* command,std::span<const CalibratedSceneDraw>,
        std::span<const std::shared_ptr<const std::vector<std::uint32_t>>> immutable_texels);
    struct State;std::unique_ptr<State> state_;
};
}
