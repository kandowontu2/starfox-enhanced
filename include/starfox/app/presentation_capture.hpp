#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace starfox::app {
// Diagnostic-only frame selection. A bounded late window permits every jitter
// phase to be captured without retaining gigabytes of warm-up screenshots.
constexpr bool presentation_capture_frame(std::uint64_t frame,
    std::uint64_t first=1,
    std::uint64_t last=std::numeric_limits<std::uint64_t>::max(),
    std::uint64_t interval=1) noexcept {
    return first!=0 && frame>=first && frame<=last
        && (frame-first)%std::max(std::uint64_t{1},interval)==0;
}
}
