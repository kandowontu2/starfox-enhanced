#pragma once
#include <cstdint>

namespace starfox::render {
enum class SpanClearMode : unsigned { full, bounds, parallel };

// Small draws keep clearing in their existing tracer dispatch. Larger draws
// avoid serial 96-byte stores per row; sample-mask draws already need a clear
// pass, so fuse their validity bounds into that pass regardless of row count.
// This is a dispatch-overhead guard, not a device-specific performance claim.
constexpr SpanClearMode span_clear_mode(std::uint32_t rows,std::uint32_t mask_words,
    bool force_full=false,bool force_parallel=false,bool force_bounds=false) noexcept {
    if(force_full) return SpanClearMode::full;
    if(force_parallel) return SpanClearMode::parallel;
    if(force_bounds) return SpanClearMode::bounds;
    return rows>=4096 || mask_words?SpanClearMode::parallel:SpanClearMode::full;
}
}
