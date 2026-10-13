// Esteban PDN / starwing-3ds, pinned 01bf4925; author permission in UPSTREAM-STARWING.md.
#pragma once

#include <cstdint>

namespace starfox::platform_3ds {

// The dump 022 cartridge sky has about one lit pixel per 420 black pixels.
// Keep the added native columns and lower screen at that same density.
constexpr std::uint32_t game_over_star_hash(unsigned x, unsigned y,
    std::uint32_t seed = 0U) noexcept {
    auto value = x * 0x9e3779b1U + y * 0x85ebca77U + seed;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

constexpr bool game_over_star(unsigned x, unsigned y,
    std::uint32_t seed = 0U) noexcept {
    return (game_over_star_hash(x,y,seed) & 4095U) < 10U;
}

} // namespace starfox::platform_3ds
