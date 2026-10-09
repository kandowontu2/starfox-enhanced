#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace starfox::render {
// Closed form of MSHOWGRID2's leftward PLOT loop. Negative DX emits only
// its first pixel. A steep segment advances Y at most once per X step.
// The strict-negative error test makes an exact boundary stay on the old row.
[[nodiscard]] inline std::optional<std::array<std::int32_t,2>> grid_line_sample(
    std::array<std::int16_t,2> current,std::array<std::int16_t,2> previous,
    std::uint32_t step) noexcept {
    const auto dx=std::int32_t(current[0])-previous[0];
    if(step>std::uint32_t(std::max(dx,0))) return std::nullopt;
    const auto dy=std::int32_t(current[1])-previous[1];
    const auto absolute_dy=dy<0?-dy:dy;
    std::int64_t y_steps=0;
    if(step && dx>0)
        y_steps=std::min<std::int64_t>(step,std::max<std::int64_t>(0,
            (std::int64_t(step)*absolute_dy-1)/dx));
    const auto y_step=current[1]<previous[1]?1:-1;
    return std::array<std::int32_t,2>{std::int32_t(current[0])-2-std::int32_t(step),
        std::int32_t(current[1])+std::int32_t(y_steps)*y_step};
}
}
