#pragma once
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/gpu_projection.hpp"
#include "starfox/render/gpu_background.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/ray_materials.hpp"
#include "starfox/render/grid_projection.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/particle_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/packed_bsp.hpp"
#include "starfox/render/packed_projection.hpp"
#include "starfox/render/packed_faces.hpp"
#include <span>
#include <variant>
#include <deque>
namespace starfox::render {
class GpuModelSourcePool;
class GpuPreparedModelSource;
struct GpuModelUploadInfo;
// Source slot alone is not an entity identity: the native pool recycles it.
// Shape/strategy/type changes also break temporal correspondence. Renderers
// must additionally reset history on scene/load-state changes and missed frames.
struct GpuModelIdentity {
    std::uint16_t slot{};
    std::uint64_t generation{};
    std::uint16_t shape{};
    std::uint32_t strategy{};
    std::uint8_t type{};
    bool operator==(const GpuModelIdentity&) const = default;
};
struct GpuModelDraw {
    const assets::Shape* shape{};
    RenderPose pose{};
    RenderSettings settings{};
    bool surface_metadata{};
    // Unidentified draws (shadows, title helpers, etc.) must not silently share
    // temporal history by shape pointer or draw order. The caller validates
    // identity/history before supplying previous_pose for motion generation.
    std::optional<GpuModelIdentity> identity;
    bool geometry_depth{};
    bool ray_geometry{}; // Opt-in caster, never inferred from HUD/native shadows.
    std::optional<RenderPose> previous_pose; // Validated presentation history only.
    // Current raster-pixel displacement. History poses remain unjittered.
    std::array<float,2> raster_jitter{};
    // Optional projection viewport in logical pixels. Scene dimensions then
    // specify an independent raster size (temporal upscaling).
    std::array<std::uint32_t,2> logical_viewport{};
    bool emissive{}; // Clear hidden receiver metadata; overrides ray_geometry for light beams.
    bool ray_materials{}; // Optional reflection data; shadows do not pay its packing cost.
    bool ray_material_reference{}; // Explicit diagnostic only.
    // Immutable CPU-only source preparation, borrowed for this encoding call.
    // Stereo constructs fresh packets per pair; no cross-frame pointer cache.
    const PreparedBspSource* prepared_topology{};
    const PreparedProjectionSource* prepared_projection{};
    const GpuPreparedModelSource* prepared_gpu{}; // Borrowed for this encoding only.
    const PreparedFacesSource* prepared_faces{}; // Exact immutable material dependencies, per pair.
    const PreparedRayTopology* prepared_rays{}; // Requested immutable connectivity only; results remain eye-owned.
    // GPU FAST: when fused onto the running scene, raster only the model's
    // screen box, in place. Set by the app; never inferred.
    bool bounded_raster{};
};
struct GpuRasterDraw {
    RasterCommands* commands{};
    bool surface_metadata{};
    bool gpu_binning{};
    bool independent_raster_size{}; // Commands retain their original canvas.
    // Caller prepared CPU row bins before parallel readers; keep the source
    // commands, texels, rows and indices immutable until encoding completes.
    bool prepared_row_bins{};
};
struct GpuGridDraw {
    timing::RenderTransform camera{};
    simulation::MatrixQ15 matrix{};
    std::uint32_t scale{1};
    std::uint8_t colour{126};
    float eye_x{},convergence{512};
    bool lines{};
    std::array<std::int16_t,2> line_start{};
    std::array<std::uint32_t,2> logical_viewport{};
};
struct GpuDustDraw {
    DustRenderer::DustFrame frame;
    std::uint32_t scale{1};
    float eye_x{},convergence{512};
    std::array<std::uint32_t,2> logical_viewport{};
};
struct GpuParticleDraw {
    ParticleRenderer::OwnerFrame frame;
    std::uint32_t scale{1};
    float eye_x{},convergence{512};
    std::array<std::uint32_t,2> logical_viewport{};
};
struct GpuTextDraw {
    ScaledTextRenderer::ProjectedFrame frame;
    std::uint32_t scale{1};
    float eye_x{},convergence{512};
    std::array<std::uint32_t,2> logical_viewport{};
};
struct GpuBackgroundDraw {
    std::shared_ptr<const simulation::SnesPpuState> ppu;
    GpuBackgroundSettings settings;
    std::uint32_t scale{1};
};
struct GpuIndexedLayerDraw {
    // A separate palette-zero-transparent source. Replay/render its commands
    // before remapping, so a zero write erases earlier source ink without
    // painting black into the destination. All pointers outlive submission.
    RasterCommands* commands{};
    LayerCompositeSettings settings;
    std::uint32_t source_scale{1},scale{1};
    std::array<std::uint32_t,2> reference_size{};
};
using GpuSceneDraw = std::variant<GpuModelDraw, GpuRasterDraw, GpuGridDraw, GpuDustDraw, GpuParticleDraw, GpuTextDraw, GpuBackgroundDraw, GpuIndexedLayerDraw>;
// Only immutable screen-space producers may be shared within one stereo pair.
// Projected geometry/particles always remain eye-owned. Sky and marked reticle
// draws are rejected when the caller applies their per-eye displacement. This
// classifies source recordings, not composed/effected eye results or history.
[[nodiscard]] bool stereo_screen_fixed_layer(std::span<const GpuSceneDraw>,
    bool sky_displacement=false,bool crosshair_displacement=false) noexcept;
// Copy an original-resolution recording for independent rasterization. Keeps
// original projection coordinates and source references; never mutates the
// fallback recording. Unsupported geometry rejects the entire conversion.
std::optional<std::vector<GpuSceneDraw>> resize_scene_raster(
    std::span<const GpuSceneDraw>,std::uint32_t source_width,std::uint32_t source_height);
// Copies one ordered frame into an eye view. Raster chunks remain shared at
// the convergence plane; model transforms are eye-specific. Source references
// must outlive submission. Source lighting/animation state is never advanced.
std::optional<std::vector<GpuSceneDraw>> stereo_scene_eye(
    std::span<const GpuSceneDraw>,unsigned eye,double separation,double convergence);
// Retains unique topology, animation-frame projection and material sources for one pair.
// Reads neither transformed vertices nor eye results. Bind only copied eye
// recordings, keep source shapes immutable, and join encoders before destruction.
// The common 8 MiB budget bounds retained source payload; overflow keeps ordinary packing,
// not reduced geometry/quality. Special axes/billboards retain their own paths.
class StereoSourceTopologies {
public:
    StereoSourceTopologies(std::span<GpuSceneDraw> left,std::span<GpuSceneDraw> right,
        std::uint64_t budget=8U*1024*1024,bool prepare_projection=true,bool prepare_faces=true,bool prepare_rays=true);
    StereoSourceTopologies(const StereoSourceTopologies&)=delete;
    StereoSourceTopologies& operator=(const StereoSourceTopologies&)=delete;
    [[nodiscard]] std::size_t source_count() const noexcept {return sources_.size();}
    [[nodiscard]] std::size_t model_count() const noexcept {return models_;}
    [[nodiscard]] std::uint64_t storage_bytes() const noexcept {return bytes_;}
    [[nodiscard]] std::size_t projection_source_count() const noexcept {return projections_.size();}
    [[nodiscard]] std::size_t projection_model_count() const noexcept {return projection_models_;}
    [[nodiscard]] std::size_t face_source_count() const noexcept {return faces_.size();}
    [[nodiscard]] std::size_t face_model_count() const noexcept {return face_models_;}
    [[nodiscard]] std::size_t ray_source_count() const noexcept {return rays_.size();}
    [[nodiscard]] std::size_t ray_model_count() const noexcept {return ray_models_;}
private:
    std::deque<PreparedBspSource> sources_;
    std::deque<PreparedProjectionSource> projections_;
    std::deque<PreparedFacesSource> faces_;
    std::deque<PreparedRayTopology> rays_;
    std::size_t models_{},projection_models_{};
    std::size_t face_models_{};
    std::size_t ray_models_{};
    std::uint64_t bytes_{};
};
// Records the boundaries between native raster work and deferred GPU models.
// Shape references must survive submission or replay. Raster chunks own their
// texels; moving the pending chunk keeps the framebuffer's recorder address
// stable. Finish recording before consuming draws().
class GpuSceneRecording {
public:
    GpuSceneRecording()=default;
    GpuSceneRecording(const GpuSceneRecording&)=delete;
    GpuSceneRecording& operator=(const GpuSceneRecording&)=delete;
    void reset(std::uint32_t width,std::uint32_t height);
    void append_model(RasterCommands& pending,GpuModelDraw draw);
    void append_grid(RasterCommands& pending,GpuGridDraw draw);
    void append_dust(RasterCommands& pending,GpuDustDraw draw);
    void append_particles(RasterCommands& pending,GpuParticleDraw draw);
    void append_text(RasterCommands& pending,GpuTextDraw draw);
    void append_background(RasterCommands& pending,GpuBackgroundDraw draw);
    void append_indexed_layer(RasterCommands& pending,RasterCommands source,
        const LayerCompositeSettings&,std::uint32_t source_scale,std::uint32_t destination_scale);
    void finish(RasterCommands& pending);
    [[nodiscard]] std::span<const GpuSceneDraw> draws() const noexcept {return draws_;}
    // Full ordered fallback, including models not supported by GPU packing.
    void replay(Framebuffer&,SurfaceBuffer*) const;
private:
    void flush(RasterCommands& pending);
    std::uint32_t width_{},height_{};
    std::deque<RasterCommands> raster_;
    std::vector<GpuSceneDraw> draws_;
};
// Resident painter merge. Inputs must carry explicit pixel coverage in bit 26,
// as produced by GpuModel or enqueue_row_spans(pixel_coverage=true).
class GpuScene {
public:
    struct MsaaSettings {
        void* palette{}; // Resident 256-entry RGBA8 palette on the scene device.
        unsigned samples{}; // 2, 4 or 8; omitted settings allocate no MSAA resources.
        std::span<const Rgba8> cpu_palette; // Used when palette is null; copied during encoding.
        bool defer_palette{}; // Do not publish provisional color through resident_output.
    };
    struct RayGeometryOutput {
        void* device{};void* buffer{};
        std::uint32_t vertex_count{};
        bool complete{}; // False means use the full fallback, not partial casters.
        const RayMaterials* materials{}; // Borrowed until next enqueue/release; null if incomplete.
        std::uint32_t material_offset{}; // Optional 64-byte records in buffer after geometry.
        std::uint32_t material_bytes{}; // Native records + texels; zero means legacy records only.
    };
    GpuScene();~GpuScene();
    // Same-size/device inputs. Null background starts a cleared scene. Ping-pong
    // storage allows output from the preceding call as the next background.
    // No upload/submit/readback; caller owns command, cancels it on failure.
    // Consume borrowed outputs before their ping-pong slot is reused.
    // front_world preserves sprite styling while excluding world objects from
    // temporal HUD protection (packed bit 28 follows colour ownership).
    // Optional layer maps an independently rasterized source into output_width/
    // output_height with source-style clipping/mosaic and palette transparency.
    // Optional rigid_motion computes foreground motion during this merge from
    // its OWN depth/transform. Requires a same-sized unfused foreground without
    // an existing motion buffer or indexed-layer mapping; background motion
    // still follows painter coverage exactly.
    GpuRasterOutput enqueue(void* command,const GpuRasterOutput& front,const GpuRasterOutput* back=nullptr,bool front_world=false,bool emissive=false,
        const GpuIndexedLayerDraw* layer=nullptr,std::uint32_t output_width=0,std::uint32_t output_height=0,
        const GpuProjection::MotionSurfaceSettings* rigid_motion=nullptr);
    // Ordered mixed model/legacy batch. Width/height are final pixel dimensions;
    // model logical dimensions are divided by its render_scale. Referenced
    // shapes/commands need only survive this call. No CPU geometry conversion,
    // submit or readback. Empty input produces a cleared scene. Cancel the
    // caller-owned command on failure (including partially encoded batches).
    // Compatible fused models consume canonical working storage in painter
    // order, skipping untouched tiles/texture holes rather than copying the
    // entire frame for each model. Intermediate painter results are NOT
    // immutable snapshots. Final output lifetime and submission fences stay
    // unchanged; motion/MSAA and new metadata storage keep their copy paths.
    // Consume the result before another operation on this scene instance.
    GpuRasterOutput enqueue_batch(void* device,void* command,std::uint32_t width,
        std::uint32_t height,std::span<const GpuSceneDraw> draws,std::array<float,2> raster_jitter={},const MsaaSettings* msaa=nullptr);
    // Owned submission for presentation. No readback; retains at most two older
    // submissions, cycling buffer storage on reuse. Borrowed enqueue calls reject
    // pending owned work until wait_for_completion succeeds.
    bool render_resident(void* device,std::uint32_t width,std::uint32_t height,
        std::span<const GpuSceneDraw> draws,std::array<float,2> raster_jitter={},const MsaaSettings* msaa=nullptr,
        GpuModelSourcePool* source_upload=nullptr);
    // Optional premultiplied RGBA scene coverage, accumulated in painter order.
    // Empty MSAA scenes clear it to transparent; failed/non-MSAA submissions
    // invalidate it. Borrowed like resident_output.
    [[nodiscard]] void* msaa_output() const noexcept;
    // Resolve retained indexed coverage with the final presentation palette.
    // Requires an owned resident submission; leaves geometry/ray outputs intact.
    bool resolve_msaa_palette(std::span<const Rgba8> palette);
    [[nodiscard]] GpuRasterOutput resident_output() const noexcept;
    // Triangle float4s, concatenated in draw order, borrowed through the next
    // batch/release. Submission ownership/fence rules match the colour output.
    // Validated model-free batches publish complete zero-caster metadata with
    // no geometry allocation. Unsupported/unrequested opaque casters remain
    // incomplete; a previous model/material buffer is never an empty scene.
    [[nodiscard]] RayGeometryOutput ray_geometry_output() const noexcept;
    bool wait_for_completion();
    // Optional diagnostic timing: retire, encode, submit microseconds, draws.
    // Populated only with scene/slow-frame tracing; never logs on the hot path
    // for slow-frame-only tracing.
    [[nodiscard]] std::array<std::uint64_t,4> submission_cost() const noexcept;
    [[nodiscard]] GpuModelUploadInfo model_upload_info() const noexcept;
    // Compatibility composition only. Downloads the completed scene without
    // running model transforms/rasterization again on the CPU.
    bool readback(Framebuffer&,SurfaceBuffer*);
    void release_device()noexcept;
    const std::string& status()const noexcept;
    // GPU FAST scene options, set by the app; never inferred. Grid, dust and
    // particle row spans then bin into compact tile lists when dense lists
    // would not fit. Models carry their own GpuModelDraw::bounded_raster.
    void set_gpu_fast(bool enabled)noexcept;
private:
    friend class GpuStereoScene;
    struct Impl;std::unique_ptr<Impl> impl_;
};
// Separate scene instances retain both borrowed outputs simultaneously.
// Both eyes enqueue into one caller-owned command: submit once, or cancel the
// entire command if either eye fails. No simulation tick or readback here.
class GpuStereoScene {
public:
    GpuStereoScene();
    ~GpuStereoScene();
    GpuStereoScene(const GpuStereoScene&)=delete;
    GpuStereoScene& operator=(const GpuStereoScene&)=delete;
    // Owned stereo submissions keep both eye outputs independent. Serial split
    // recording is the reference/default. Joined/parallel experiments retain
    // fences and invalidate the entire pair on either encoding failure; the
    // parallel worker owns its right command through submission/cancellation;
    // the caller retires the retained fences after joining the worker.
    bool render_resident(void* device,std::uint32_t eye_width,std::uint32_t eye_height,
        std::span<const GpuSceneDraw> frame,double separation,double convergence,
        const GpuScene::MsaaSettings* msaa=nullptr);
    // Independent retained sample identities per eye, including deferred
    // presentation palettes. Resolving either eye unsuccessfully invalidates
    // publication of the whole pair; geometry is never replayed to recolor it.
    [[nodiscard]] unsigned msaa_samples() const noexcept;
    [[nodiscard]] GpuModelUploadInfo model_upload_info() const noexcept;
    bool resolve_msaa_palette(std::span<const Rgba8> palette);
    [[nodiscard]] GpuRasterOutput resident_output(unsigned eye) const noexcept {
        return resident_ready_ && eye<2 ? eyes_[eye].resident_output() : GpuRasterOutput{};
    }
    [[nodiscard]] GpuScene::RayGeometryOutput ray_geometry_output(unsigned eye) const noexcept {
        return resident_ready_ && eye<2 ? eyes_[eye].ray_geometry_output() : GpuScene::RayGeometryOutput{};
    }
    [[nodiscard]] std::array<std::uint64_t,4> submission_cost() const noexcept;
    bool wait_for_completion();
    std::optional<std::array<GpuRasterOutput,2>> enqueue(void* device,void* command,
        std::uint32_t eye_width,std::uint32_t eye_height,
        std::span<const GpuSceneDraw> frame,double separation,double convergence,
        const GpuScene::MsaaSettings* msaa=nullptr);
    void release_device() noexcept;
    void set_gpu_fast(bool enabled) noexcept {for(auto& eye:eyes_) eye.set_gpu_fast(enabled);}
private:
    struct Parallel;
    std::array<GpuScene,2> eyes_;
    bool resident_ready_{};
    std::unique_ptr<Parallel> parallel_;
    std::unique_ptr<GpuModelSourcePool> source_uploads_;
};
}
