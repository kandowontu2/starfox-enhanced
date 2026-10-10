#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace starfox::render {
// Candidate enclosure only. A matching tile is NOT an optical root, uniqueness
// certificate or permission to read old colour. Each eye/sample/lobe is separate.
struct ReflectionSourceIndexLayout {
    std::uint32_t width{},height{},lobes{},primary_prefix{},record_prefix{},path_stride{};
    std::uint32_t total_nodes{},level_count{},storage_bytes{};
    std::array<std::array<std::uint32_t,4>,12> levels{};
    bool operator==(const ReflectionSourceIndexLayout&) const=default;
};
inline std::optional<ReflectionSourceIndexLayout> reflection_source_index_layout(
    std::uint32_t width,std::uint32_t height,std::uint32_t stride) noexcept {
    if(!width || !height || width>16384 || height>16384) return {};
    ReflectionSourceIndexLayout r;r.width=width;r.height=height;
    switch(stride) {
    case 80:case 96:case 444:case 460:r.path_stride=52;break;
    case 92:case 108:case 124:case 128:case 540:case 556:case 572:case 576:r.path_stride=64;break;
    default:return {};
    }
    r.lobes=stride>=444?8:1;r.record_prefix=stride-r.path_stride*r.lobes;
    r.primary_prefix=r.path_stride==64 && r.record_prefix!=28?r.record_prefix-40:4;
    auto w=(width+7)/8,h=(height+7)/8;
    for(;;) {
        if(r.level_count==r.levels.size()) return {};
        r.levels[r.level_count++]={r.total_nodes,w,h,0};r.total_nodes+=w*h;
        if(w==1 && h==1) break;
        w=(w+1)/2;h=(h+1)/2;
    }
    const auto bytes=std::uint64_t(r.total_nodes)*r.lobes*32;
    if(bytes>UINT32_MAX) return {};
    r.storage_bytes=std::uint32_t(bytes);return r;
}
struct GpuReflectionSourceIndex {
    void* device{};
    void* buffer{};
    void* source_buffer{}; // Exact native bank indexed, never a CPU copy.
    ReflectionSourceIndexLayout layout{};
};
struct ReflectionSourceQueryLimits {
    std::uint32_t leaves{64},nodes{4096},regions{128},supports{16384};
    bool valid() const noexcept {return leaves && leaves<=64 && nodes && nodes<=4096
        && regions && regions<=128 && supports && supports<=16384;}
};
struct GpuReflectionSourceQueries {
    // Streaming scratch, borrowed only before the next query encoding or
    // commit/discard/reset. MUST be consumed in the same unsubmitted command.
    // No region, empty result or nonzero status is a colour/root certificate.
    // The 64 leaf-ID slots after each 16-byte header are fully initialized:
    // active IDs retain traversal order; every unused slot is zero, including
    // empty/refused queries and reusable scratch across streamed batches.
    // The 128 support records follow the same rule: emitted records keep
    // their existing order/bytes; all records beyond the count are zero.
    static constexpr std::uint32_t capacity=64,query_stride=48,leaf_stride=272,region_stride=6160;
    static constexpr std::uint64_t working_bytes=std::uint64_t(capacity)*(query_stride+leaf_stride+region_stride);
    void* device{};
    void* queries{};
    void* leaves{};
    void* regions{};
    void* current_buffer{};
    GpuReflectionSourceIndex source{};
    std::uint32_t first{},count{};
    ReflectionSourceQueryLimits limits{};
};
struct ReflectionSourceOpticalLimits {
    std::uint32_t boxes{8192},depth{12};
    bool valid() const noexcept {return boxes && boxes<=8192 && depth<=12;}
};
struct GpuReflectionSourceOptical {
    // Borrowed until the next query/optical encoding (including refusal), or
    // commit/discard/reset/release. Consume in the SAME unsubmitted command.
    // Kept boxes are unresolved;
    // even complete exclusion is NOT an existence/uniqueness/RGB certificate.
    static constexpr std::uint32_t frame_stride=512,result_stride=128*32;
    static constexpr std::uint64_t working_bytes=std::uint64_t(GpuReflectionSourceQueries::capacity)*(frame_stride+result_stride);
    // Extra scratch only for complete-frame GPU indirect scheduling; it is not
    // part of the generic optical packet ABI or its ordinary working forecast.
    static constexpr std::uint32_t stream_task_capacity=GpuReflectionSourceQueries::capacity*128;
    // Stream groups cover the same cells in parallel, without changing the
    // region/cell quotas, scratch ABI or shared optical/interval writer.
    static constexpr std::uint32_t stream_lanes=256;
    static constexpr bool stream_lanes_supported(std::uint32_t invocations,std::uint32_t size_x) noexcept {
        return invocations>=stream_lanes && size_x>=stream_lanes;
    }
    static constexpr std::uint64_t stream_working_bytes=std::uint64_t(stream_task_capacity)*8+12;
    GpuReflectionSourceQueries queries{};
    void* frames{};
    void* results{};
    ReflectionSourceOpticalLimits limits{};
};
struct GpuReflectionSourceLocalRoots {
    // LOCAL fixed-point classifications over retained native optical hulls.
    // No global source/branch uniqueness or old RGB permission. Borrowed only
    // until the next query/optical/root encoding (including refusal), or
    // commit/discard/reset/release; consume in the SAME unsubmitted command.
    static constexpr std::uint32_t region_capacity=128,record_stride=192;
    static constexpr std::uint32_t result_stride=region_capacity*record_stride;
    static constexpr std::uint64_t working_bytes=std::uint64_t(GpuReflectionSourceQueries::capacity)*result_stride;
    // Header: valid, local classification, projection axis, refusal/reason.
    // Reason 4096 is complete UPSTREAM optical exclusion, not local uniqueness.
    GpuReflectionSourceOptical optical{};
    void* results{};
};
struct GpuReflectionSourceRootHull {
    // One closed bounding hull of EVERY retained necessary-source region per
    // native query, including shared boundaries and zero-area supports. A
    // strict unique root on this superset rules out distinct roots in covered
    // regions, but may lie in a hole: actual source membership, visibility,
    // angular and colour guards are still required. No old RGB permission.
    // Shares local-root scratch/lifetime; only the first 192-byte record of
    // each query stride is used. Other records have no usable certificate;
    // complete batches zero them, refusals may retain only diagnostic reasons.
    static constexpr std::uint32_t record_stride=GpuReflectionSourceLocalRoots::record_stride;
    static constexpr std::uint32_t result_stride=GpuReflectionSourceLocalRoots::result_stride;
    GpuReflectionSourceOptical optical{};
    void* results{};
};
struct GpuReflectionSourceWitness {
    // Native source-footprint witness over the WHOLE certified root enclosure,
    // not its midpoint. Reads actual accepted path/visibility/depth/features.
    // Still NOT RGB permission: angular/subcell-fold/current colour guards and
    // producer/consumer qualification remain required. No incident RGB output.
    // Reuses an otherwise unused record in root scratch; encoding invalidates
    // the older local/hull descriptor. SAME unsubmitted command/lifetime only.
    static constexpr std::uint32_t record_offset=192,record_bytes=64;
    static constexpr std::uint32_t result_stride=GpuReflectionSourceRootHull::result_stride;
    // Header: proved footprint (0/1), stage bits (root/bounds/path/depth/feature),
    // Stage bits are proofs ONLY with header==1; refused records show progress,
    // not partial eligibility. No bit in this record permits old incident RGB.
    // refusal reason, evaluated source cells. Reasons: 1 root, 2 bounds, 3 path,
    // 4 depth, 5 feature, 6 arithmetic, 7 bounded cell quota, 8 frame/ABI.
    GpuReflectionSourceRootHull hull{};
    void* results{};
};
struct GpuReflectionSourceFolds {
    // Existing cell/ring/five-half-point guards for every possible fractional
    // source cell in the certified enclosure. Actual accepted geometry/path
    // and wave clock only. No midpoint, CPU guide or old incident RGB output.
    // Shares root scratch and invalidates the preceding witness descriptor.
    // SAME pending command/presentation lifetime. NOT colour permission.
    static constexpr std::uint32_t record_offset=256,record_bytes=64;
    static constexpr std::uint32_t result_stride=GpuReflectionSourceRootHull::result_stride;
    // Header: fold guard proved (0/1), refusal reason, cells, forward traces.
    // Reasons: 1 upstream proof, 2 path, 3 cell fold, 4 ring fold, 5 forward,
    // 6 subcell fold, 7 bounded cells, 8 arithmetic/ABI. A refusal proves none.
    GpuReflectionSourceWitness witness{};
    void* results{};
};
struct GpuReflectionSourceGuide {
    // Full unchanged colour-root solver + finite-face/angular retrace, seeded
    // ONLY from a preceding native certified enclosure. The solved point must
    // remain inside that enclosure; root/footprint/fold proofs stay immutable.
    // This does NOT sample or authorize old RGB. CURRENT clamp/atomic complete
    // lobe recomposition and producer/cost qualification remain separate.
    static constexpr std::uint32_t record_offset=320,record_bytes=64;
    static constexpr std::uint32_t result_stride=GpuReflectionSourceRootHull::result_stride;
    // Header: guide valid, refusal reason, reserved zero, reserved zero.
    // Reasons: 1 upstream, 2 ABI/path, 3 target, 4 solver/finite face,
    // 5 outside certified enclosure, 6 angular retrace. Followed by exact
    // certified enclosure, solved pixel/depth/valid, residual/depth/reserved.
    GpuReflectionSourceFolds folds{};
    void* results{};
};
struct GpuReflectionSourceColour {
    // Certified per-lobe linear incident RGB, bounded by freshly traced CURRENT
    // matching neighbours. Never a partially updated displayed/native bank.
    // CURRENT base/throughput and whole-pixel/all-lobe commit remain separate.
    // Reuses root scratch; SAME pending command and exact old/current bank.
    static constexpr std::uint32_t record_offset=384,record_bytes=96;
    static constexpr std::uint32_t result_stride=GpuReflectionSourceRootHull::result_stride;
    // Header: valid, refusal (1 upstream,2 ABI,3 CURRENT,4 source,5 sample,
    // 6 neighbourhood), matching CURRENT neighbours, reserved zero. Followed
    // by CURRENT incident/weight, low, high, filtered old, admitted linear RGB.
    GpuReflectionSourceGuide guide{};
    void* results{};
};
struct GpuReflectionSourceComposition {
    // Complete per-pixel material composition, staged in existing scratch.
    // Every lobe contributes CURRENT response/base. Only independently proven
    // old incident RGB replaces that lobe's freshly traced CURRENT; refused
    // lobes stay CURRENT. Transported lobes are quantized BEFORE quadrature.
    // No CURRENT/source/displayed bank mutation or partial publication.
    static constexpr std::uint32_t record_offset=512,record_bytes=64;
    static constexpr std::uint32_t result_stride=GpuReflectionSourceRootHull::result_stride;
    // Per-lobe packet: complete-pixel valid, refusal (1 no admitted lobe,
    // 2 ABI,3 CURRENT material,4 arithmetic/proof), pixel, admitted-lobe mask;
    // canonical packed pixel, packed incident, packed transported lobe, zero;
    // composed linear pixel RGB/zero, then four reserved zero words.
    // All lobes carry the same pixel/mask/canonical/linear result. A refused
    // complete packet carries exact CURRENT canonical/incident words only.
    GpuReflectionSourceColour colour{};
    void* results{};
};
}
