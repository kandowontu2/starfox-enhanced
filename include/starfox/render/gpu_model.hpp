#pragma once
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/gpu_msaa.hpp"
#include "starfox/render/gpu_projection.hpp"
#include "starfox/render/ray_materials.hpp"
#include "starfox/render/packed_bsp.hpp"
#include <array>
#include <vector>
#include <optional>
namespace starfox::render {
class PreparedProjectionSource;
class PreparedFacesSource;
class PreparedRayTopology;
// Optional resident camera geometry for a ray-tracing consumer. The only CPU
// data is immutable topology (point indices + face ID), never transformed XYZ.
// Empty sources mean the model needs its existing specialized fallback.
// GPU buffers are borrowed until the next enqueue/release on their GpuModel;
// consume them on the owning command before reusing that model object.
struct GpuModelRaySource {
    void* points{};void* residuals{};
    std::uint32_t point_count{},mode{};
    std::vector<std::array<std::uint32_t,4>> triangles;
    bool request_materials{};
    bool reference_materials{}; // Diagnostic CPU packing; never needed for GPU transport.
    bool materials_complete{};
    bool reflection_excluded{}; // Axis-line raster has no polygon reflection surface; retains shadow casters.
    RayMaterials materials;
    void *material_corners{},*material_polygons{},*material_commands{};
    std::uint32_t material_corner_count{},material_count{};
    void* material_lookup{};
    std::uint32_t material_lookup_count{};
    std::vector<std::array<std::uint32_t,4>> material_topology;
    // Optional immutable pair-owned inputs. Consume before the owning pair
    // ends; transformed points/material buffers keep their per-eye lifetime.
    std::span<const std::array<std::uint32_t,4>> borrowed_triangles,borrowed_material_topology;
    [[nodiscard]] std::span<const std::array<std::uint32_t,4>> triangle_indices() const noexcept {
        return borrowed_triangles.empty()?std::span<const std::array<std::uint32_t,4>>(triangles):borrowed_triangles;
    }
    [[nodiscard]] std::span<const std::array<std::uint32_t,4>> material_indices() const noexcept {
        return borrowed_material_topology.empty()?std::span<const std::array<std::uint32_t,4>>(material_topology):borrowed_material_topology;
    }
    // Validated immutable buffers from the current stereo source recording.
    // Already encoded on its first command; consume before that pair ends.
    // These are connectivity only, never eye-transformed geometry/materials.
    void *triangle_topology{},*material_connectivity{};
};
// Optional diagnostic handles only; enqueue never submits or reads them back.
// Borrowed through the next enqueue/release, with the same command lifetime.
struct GpuModelDiagnostics {
    void* projected_points{};
    void* clipped_polygons{};
    std::uint32_t point_count{},polygon_count{};
    bool continuous{};
};
// Host input accounting only; no GPU readback or timing query.
struct GpuModelUploadInfo {
    std::uint64_t input_bytes{},uploaded_bytes{};
    std::uint32_t uploaded_buffers{},reused_buffers{};
    std::uint64_t snapshot_bytes{}; // Retained CPU capacity, bounded to 1 MiB.
    std::uint64_t shared_bytes{}; // Inputs borrowed from this immutable stereo recording.
    std::uint32_t shared_buffers{};
    // Logical pool GPU + transfer capacity, at most 16 MiB. SDL may retain
    // backing versions until the existing in-flight eye fences retire.
    std::uint64_t source_storage_bytes{};
    std::uint64_t ray_shared_bytes{};
    std::uint32_t ray_shared_buffers{};
    // Separate from explicit storage-buffer copies above. Constants include
    // shader padding/unused slots, not just logical pose bytes; do not claim
    // they are zero-upload data or a reduction in total GPU transport.
    std::uint64_t pose_uniform_bytes{};
    std::uint32_t pose_uniform_pushes{};
    std::uint64_t pose_uniform_input_bytes{}; // Logical pose bytes within input_bytes.
};
struct GpuModelSourceRequest {
    const PreparedProjectionSource* projection{};
    const PreparedBspSource* topology{};
    const assets::TextureImage* billboard_texture{};
    const PreparedFacesSource* faces{};
    const PreparedRayTopology* rays{};
};
// An immutable source packet for ONE stereo encoding. Not a cross-frame shape
// cache. The source pool owns these buffers; camera transforms, visibility
// results, depth, motion and painter targets always remain per eye. Ordinary
// face inputs may also be shared when their complete material dependencies match.
class GpuPreparedModelSource {
    friend class GpuModelSourcePool;
    friend class GpuModel;
    void* device_{};
    GpuModelSourceRequest source_{};
    // Vertices, visibility source, BSP nodes, face IDs, billboard texels,
    // ordinary corners, polygons, source materials, model texels, ray triangles
    // and ray material connectivity. Optional ray inputs allocate nothing OFF.
    std::array<void*,11> buffers_{};
    std::array<std::uint32_t,11> sizes_{};
    const bool* encoded_{};
};
class GpuModelSourcePool {
public:
    GpuModelSourcePool();~GpuModelSourcePool();
    // Prepare immutable source bytes while both recordings are alive. Rebuild
    // on EVERY pair, including retries/cancellation/address reuse. Budget
    // exhaustion/allocation failure means the ordinary full-quality path.
    bool prepare(void* device,std::span<const GpuModelSourceRequest>,
        std::uint64_t budget=8U*1024*1024);
    // Encode once BEFORE either eye consumes packets, on the left/sole command.
    // Split/parallel commands must submit left before right. No extra submit,
    // wait, readback or CPU transform. Never move an SDL command across threads.
    bool enqueue(void* command);
    [[nodiscard]] std::span<const GpuPreparedModelSource> packets() const noexcept;
    [[nodiscard]] GpuModelUploadInfo upload_info() const noexcept;
    // Call only after both encoders join. Clears borrowed source identities;
    // retained SDL buffers cycle on each new recording, preserving queued work.
    void end_recording() noexcept;
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
// Model geometry pipeline. Supports ordinary polygons, lines and sprite faces;
// unsupported primitives/effects explicitly fail instead of dropping faces.
    // Indexed output starts cleared unless a background is supplied.
    // Packed bit 26 marks coverage, including black writes, for GpuScene merging.
class GpuModel {
public:
    GpuModel();~GpuModel();
    // Caller-owned SDL device and command. Packs/uploads model data, then chains
    // transform, visibility, BSP, clipping, spans and raster without readback.
    // Output borrowed until next call. Cancel command on failure; release before
    // device destruction. Optional normals/depth are generated on-device.
    // GpuScene::enqueue_batch composes multiple ordered model/legacy draws.
    // This primitive still encodes one model's geometry at a time.
    // An optional non-aliased background is composited in the raster dispatch.
    // geometry_depth requests separate per-pixel camera Z for planar polygons.
    // Folded faces and screen-space/sprite effects remain unknown (zero or a
    // null depth buffer); effects' historical mean depth is never substituted.
    // previous_pose opts into rigid per-pixel motion and depth. It must belong
    // to the same entity/topology and preceding submitted frame. Unsupported
    // correspondence remains null (never valid zero motion). Motion requires
    // an unfused draw: merge its output through GpuScene afterwards.
    // raster_jitter is a current-frame displacement in output raster pixels;
    // both supplied poses remain unjittered. Requires continuous subpixel
    // projection. Motion removes this displacement, depth follows the raster.
    // Optional raster_size decouples logical viewport/projection from output
    // dimensions. Continuous subpixel geometry is required; whole-object
    // billboards and wave distortion currently require the normal path.
    // Optional MSAA face packets are borrowed through the next enqueue. Each
    // kind must be dispatched explicitly; empty packets mean a specialized
    // model path. No MSAA allocation/packing is performed when not requested.
    // Optional deferred_motion receives the exact per-object correspondence
    // instead of allocating/dispatching a motion buffer. The caller MUST merge
    // this unfused output through GpuScene with the returned settings; motion
    // remains absent for unsupported correspondence. Reset on every call.
    // painter_flags is the GpuRaster foreground policy. GpuScene uses it to
    // fuse world-sprite/emissive ownership without a second painter dispatch.
    // in_place_background explicitly consumes the canonical background's
    // writable storage for sparse painter updates (see GpuRaster). Temporal,
    // MSAA and incompatible metadata retain their existing separate paths.
    // Optional source_topology is a validated immutable source packet borrowed
    // through this call only. Camera and visibility work stays per draw.
    // Mismatched source/explosion policy rejects; axes/billboards remain special.
    // Whole-object sprites may borrow the selected immutable texture from
    // gpu_source. Position, palette and any temporal depth remain draw-owned.
    // Unready/mismatched packets retain the ordinary full-quality upload path.
    // source_projection borrows frame coordinates/descriptors only. Transform
    // constants stay eye-owned; mutable fragment expansion materializes a copy.
    // source_faces borrows ordinary immutable topology/material/texel inputs
    // only after validating every face-packing dependency against this draw.
    // Explosions, active colour warp and axes retain their mutable paths.
    GpuRasterOutput enqueue(void* device,void* command,const assets::Shape&,const RenderPose&,
        const RenderSettings&,std::uint32_t width,std::uint32_t height,bool surface_metadata=false,
        const GpuRasterOutput* background=nullptr,GpuModelDiagnostics* diagnostics=nullptr,
        bool geometry_depth=false,GpuModelRaySource* ray_source=nullptr,
        const RenderPose* previous_pose=nullptr,std::array<float,2> raster_jitter={},
        std::array<std::uint32_t,2> raster_size={},GpuMsaaFaces* msaa_faces=nullptr,unsigned msaa_samples=8,
        std::optional<GpuProjection::MotionSurfaceSettings>* deferred_motion=nullptr,
        std::uint32_t painter_flags=0,bool in_place_background=false,
        const PreparedBspSource* source_topology=nullptr,
        const PreparedProjectionSource* source_projection=nullptr,
        const GpuPreparedModelSource* gpu_source=nullptr,
        const PreparedFacesSource* source_faces=nullptr,
        const PreparedRayTopology* source_rays=nullptr,
        bool bounded_raster=false,bool compact_tiles=false);
    void release_device() noexcept;
    // Explicit single-recording scope for byte-exact read-only input reuse.
    // End on every success/error path BEFORE submitting/canceling this command.
    // Never carry a scope across command buffers or begin a nested scope.
    // GpuScene supplies this lifetime; standalone enqueue remains uncached.
    // Borrowed stereo slots never seed owned-buffer reuse. Other owned inputs
    // remain reusable; returning a borrowed slot to owned storage uploads it.
    void begin_upload_batch() noexcept;
    void end_upload_batch() noexcept;
    [[nodiscard]] GpuModelUploadInfo upload_info() const noexcept;
    [[nodiscard]] bool output_aliases(const GpuRasterOutput&) const noexcept;
    const std::string& status()const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
