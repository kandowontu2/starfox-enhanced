#pragma once
#include "starfox/render/reflection_source_diagnostics.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include "starfox/render/reflection_source_index.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
namespace starfox::render {
struct CalibratedReflectionHistorySettings {
    std::uint64_t epoch{};
    float weight{.85F};
    bool srgb{};
    // Diagnostic-only staging until the complete curved producer/consumer,
    // accepted owner and practical frame-cost gates pass. Normal callers keep
    // tracing CURRENT full-quality radiance; never silently reuse a subset.
    bool curved_validation{};
    // Explicit staging: pair the resident candidate index with this exact
    // accepted native bank. Pending indexed data stays freshly traced CURRENT
    // even at nonzero weight with compatible history; it does NOT enable
    // curved colour/history reuse or contaminate CURRENT-neighbour evidence.
    bool source_index_validation{};
    // Explicit test-only sidecar for final unrounded rough-colour auditing.
    // No colour stage reads this buffer; normal callers allocate none.
    bool source_admission_diagnostics{};
};
struct GpuReflectionSourceAdmission {
    void* device{};void* buffer{};
    std::uint32_t width{},height{},bytes{};
};
struct CalibratedReflectionLobeGeometry {
    void* buffer{}; // SAME submitted SDL geometry/accepted vertex allocation as the ray producer.
    std::uint32_t bytes{},vertex_count{};
    shadows::RayReflectionHistory history{};
    float roughness{};
    std::array<float,9> current_cube{1,0,0,0,1,0,0,0,1},previous_cube{1,0,0,0,1,0,0,0,1};
    std::optional<shadows::RayReflectionGround> current_ground{};
    // Exact CURRENT native liquid calibration, not the old plane or a host
    // effect clock. Required by the explicit 64-byte ordered path consumer.
    std::optional<shadows::RayReflectionLiquid> current_liquid{};
    // Also supply these for planar resident-optical validation. Missing optional
    // planar calibration declines that stage; it never guesses an accepted eye.
    std::array<double,4> current_projection{};
    std::array<double,2> current_clip{};
    static bool cube_rotation_valid(const std::array<float,9>& m) noexcept {
        for(float v:m) if(!std::isfinite(v)) return false;
        for(unsigned r=0;r<3;++r) for(unsigned s=0;s<3;++s) {
            float product=0;for(unsigned c=0;c<3;++c) product+=m[r*3+c]*m[s*3+c];
            if(std::abs(product-float(r==s))>.0005F) return false;
        }
        return true;
    }
    bool valid() const noexcept {
        if(history.curved_paths) {
            if(!current_liquid || !current_liquid->valid() || !history.previous_liquid
                || current_liquid->material!=history.previous_liquid->material) return false;
            for(double v:current_projection) if(!std::isfinite(v) || std::abs(v)>1.e12) return false;
            if(current_projection[0]<=0 || current_projection[1]<=0) return false;
            if(!std::isfinite(current_clip[0]) || !std::isfinite(current_clip[1])
                || current_clip[0]<=0 || current_clip[1]<=current_clip[0] || current_clip[1]>1.e12) return false;
        } else if(current_liquid) return false;
        return buffer && vertex_count && vertex_count%3==0 && history.valid() && history.model_lobes
            && (history.scene_paths?current_ground && current_ground->valid():!current_ground)
            && (!history.model_paths || (cube_rotation_valid(current_cube) && cube_rotation_valid(previous_cube)))
            && std::isfinite(roughness) && roughness>=0 && roughness<=1
            && history.model_lobes==(roughness>0?8U:1U)
            && std::uint64_t(history.previous_vertex_offset)+std::uint64_t(vertex_count)*16<=bytes
            && history.previous_index_offset>=std::uint64_t(history.previous_vertex_offset)+std::uint64_t(vertex_count)*16
            && std::uint64_t(history.previous_index_offset)+std::uint64_t(vertex_count/3)*4<=bytes;
    }
};
// Separate secondary-radiance history. Never transport the whole composed
// model using reflected motion, or reflections using the primary surface flow.
// One owner per eye AND native raster sample; commands and all
// colour/identity/depth data stay resident. Never merge sample identities.
class GpuCalibratedReflectionHistory {
public:
    GpuCalibratedReflectionHistory();~GpuCalibratedReflectionHistory();
    GpuCalibratedReflectionHistory(const GpuCalibratedReflectionHistory&)=delete;
    GpuCalibratedReflectionHistory& operator=(const GpuCalibratedReflectionHistory&)=delete;
    bool initialize(void* device);
    // Preflight only; no allocation, GPU wait or change to accepted history.
    // Failure means trace CURRENT full-quality radiance without colour reuse.
    bool can_allocate(std::uint32_t width,std::uint32_t height,std::uint32_t bytes_per_pixel,bool source_index=false,bool admission_diagnostics=false) const noexcept;
    std::optional<std::uint64_t> allocation_bytes(std::uint32_t width,std::uint32_t height,std::uint32_t bytes_per_pixel,bool source_index=false,bool admission_diagnostics=false) const noexcept;
    std::uint64_t working_image_bytes() const noexcept;
    static constexpr bool working_bound_fits(std::uint64_t next,std::uint64_t accepted,bool same) noexcept {
        constexpr auto bound=1024ULL*1024*1024;
        return next && next<=bound && accepted<=bound && (same || next<=bound-accepted);
    }
    bool enqueue(void* command,const shadows::GpuReflectionOutput&,void* ownership,
        const CalibratedReflectionHistorySettings&,const CalibratedReflectionLobeGeometry* lobes=nullptr);
    // Borrowed candidate for composition in the SAME caller-owned command.
    // It is not accepted history until the complete presentation succeeds.
    shadows::GpuReflectionOutput output() const noexcept;
    // Immutable freshly traced candidate, even after a complete separately
    // composed frame is available. SAME pending-command lifetime only.
    shadows::GpuReflectionOutput fresh_current_output() const noexcept;
    // Read-only accepted RGB for an identical held presentation. No new
    // resolve, sample or history write; remains alive until replacement/reset.
    shadows::GpuReflectionOutput accepted_output() const noexcept;
    // Borrowed discrete lobe mask paired with the exact accepted image. Empty
    // without explicit staging, while pending, or after reset/device release.
    // Diagnostic branch evidence only; never source coordinates or RGB input.
    GpuReflectionSourceAdmission accepted_source_admission() const noexcept;
    // Read-only borrowed index/source pair; follows the SAME pending/accepted
    // presentation lifetime as RGB. Held frames never rebuild the hierarchy.
    GpuReflectionSourceIndex source_index() const noexcept;
    GpuReflectionSourceIndex accepted_source_index() const noexcept;
    // Map native current pixel/lobe records through the SAME GPU primitive
    // correspondence and query the exact accepted bank. No CPU query/feature
    // upload. Stream bounded contiguous batches; never publish a partial colour
    // history because a batch or one of its queries refuses.
    bool enqueue_source_queries(void* command,std::uint64_t first,std::uint32_t count,
        const ReflectionSourceQueryLimits& limits={});
    GpuReflectionSourceQueries source_queries() const noexcept;
    // Build accepted optical frames from submitted GPU old vertices and raw
    // current features, then conservatively exclude impossible whole boxes.
    // Requires exact accepted/current eye calibration; no rounded host target.
    // Never authorizes old colour. Call after the batch and consume in SAME CB.
    bool enqueue_source_optical(void* command,const ReflectionSourceOpticalLimits& limits={});
    GpuReflectionSourceOptical source_optical() const noexcept;
    std::optional<std::uint64_t> source_optical_allocation_bytes() const noexcept;
    // Differentiate the complete native map and classify retained optical hulls
    // locally, using GPU raw targets/accepted vertices. No host box/root input,
    // global uniqueness or colour permission. Requires preceding SAME-CB optics.
    bool enqueue_source_local_roots(void* command);
    GpuReflectionSourceLocalRoots source_local_roots() const noexcept;
    std::optional<std::uint64_t> source_local_root_allocation_bytes() const noexcept;
    // Classify one bounding hull of the complete necessary-source cover. Never
    // merge roots by proximity or omit shared/zero-area supports. Uses the same
    // bounded scratch, invalidates local descriptors, and does not permit RGB.
    bool enqueue_source_root_hull(void* command);
    GpuReflectionSourceRootHull source_root_hull() const noexcept;
    // Verify the complete certified root enclosure against actual accepted
    // source taps. No midpoint substitution, CPU witness/seed or RGB output.
    // Requires preceding whole-hull encoding in the SAME pending command;
    // source-footprint evidence alone does not qualify colour history.
    bool enqueue_source_witness(void* command);
    GpuReflectionSourceWitness source_witness() const noexcept;
    // Apply actual accepted-source fold guards after the native footprint
    // witness in the SAME pending command; angular/colour gates still required.
    bool enqueue_source_folds(void* command);
    GpuReflectionSourceFolds source_folds() const noexcept;
    // Opt-in SAME-command proof stage only; no old-colour reuse permission.
    bool enqueue_source_guide(void* command);
    GpuReflectionSourceGuide source_guide() const noexcept;
    // Stage certified per-lobe RGB against fresh CURRENT neighbours; no bank
    // mutation or partial pixel composition. Requires SAME-command guide.
    bool enqueue_source_colour(void* command);
    GpuReflectionSourceColour source_colour() const noexcept;
    // Stage whole CURRENT-material pixel packets; refused lobes retain CURRENT
    // incident light. No displayed/native bank write. SAME-CB complete-pixel
    // batches only; preserves fresh CURRENT for every following batch.
    bool enqueue_source_composition(void* command);
    GpuReflectionSourceComposition source_composition() const noexcept;
    // Explicit staged publication, NOT normal player enablement. Stream all
    // aligned complete-pixel batches from record zero, in order, in SAME CB.
    // Lazily copies fresh CURRENT to a separate bounded image and applies only
    // complete packets. output() changes ONLY after full-frame coverage; commit
    // then accepts the matching image/index. No partial image is exposed.
    bool enqueue_source_publication(void* command);
    bool source_publication_complete() const noexcept;
    std::optional<std::uint64_t> source_publication_allocation_bytes() const noexcept;
    // Full opt-in resident source/proof/colour/composition/publication stream.
    // Cold/cut/zero-weight frames keep the complete fresh candidate. Hot frames
    // process EVERY native record, not a selected pixel/eye/rough lobe subset.
    // No CPU query upload, readback or wait; practical frame cost must still be
    // qualified before a normal player uses this explicit validation path.
    // Optional diagnostic observer runs after each complete composition and
    // before publication seals its descriptors. It may only encode read-only
    // captures into this SAME unsubmitted command: no upload, submit, wait or
    // proof mutation. Normal/actual producer callers pass no observer.
    using SourceBatchObserver=void(*)(void* context,void* command,const GpuReflectionSourceComposition&);
    // Stage observer has the same read-only restrictions; null is the normal
    // path, with no query allocation, marker dispatch, readback or extra wait.
    bool enqueue_source_frame(void* command,SourceBatchObserver observer=nullptr,void* context=nullptr,
        ReflectionSourceStageObserver stages=nullptr,void* stage_context=nullptr);
    // Complete-frame forecast also reserves the lazy GPU-only optical work map
    // and indirect arguments. Generic manual publication needs neither.
    std::optional<std::uint64_t> source_frame_allocation_bytes() const noexcept;
    // Combined logical image/index/scratch bound; no allocation or GPU wait.
    std::optional<std::uint64_t> source_query_allocation_bytes() const noexcept;
    void commit() noexcept;
    void discard() noexcept;
    void reset() noexcept;
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    bool enqueue_source_root_stage(void* command,bool whole_cover);
    bool enqueue_source_guard_stage(void* command,unsigned stage);
    struct State;std::unique_ptr<State> state_;
};
}
