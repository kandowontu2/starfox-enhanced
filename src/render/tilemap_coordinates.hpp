#pragma once

#include <cstdint>

namespace starfox::render::detail {

// SNES BG maps are 32/64 tiles wide/high with 8/16-pixel tiles: every
// dimension is 256, 512 or 1024. Unsigned masking is floor-modulo even for
// negative scroll coordinates, without a signed division on the ARM11.
// This helper is only for those power-of-two tilemap dimensions, not mosaic
// blocks (whose sizes may be any integer from 1 through 16).
[[nodiscard]] constexpr std::int32_t wrap_tilemap_coordinate(
    std::int32_t coordinate, std::uint32_t dimension) noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(coordinate) & (dimension - 1U));
}

} // namespace starfox::render::detail
