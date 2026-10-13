// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#pragma once

#include "starfox/simulation/snes_ppu.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace starfox::platform_3ds {

// One unscaled source-raster fragment of an 8x8 4bpp BG2 character. The
// source coordinates already include SNES tile flips. A consumer walks them
// forward or backward according to reverse_x/reverse_y.
struct Bg2Rect {
    std::uint16_t x{}, y{}, width{}, height{};
    std::uint16_t character{};
    std::uint8_t palette_bank{};
    std::uint8_t source_x{}, source_y{};
    bool reverse_x{}, reverse_y{};
    // A solid palette index used to continue rolled ground after its source
    // tilemap wraps or a ground character has a transparent pixel.
    bool solid{};
    std::uint8_t solid_colour{};
    friend constexpr bool operator==(const Bg2Rect&, const Bg2Rect&) = default;
};

// Plans a native Mode 2 BG2 raster. The SNES 256x224 viewport begins at
// (origin_x, origin_y); pixels outside it continue the BG2 tile map while
// line-specific HDMA uses the nearest source scanline. Returns false, with no
// partial output, for unsupported mosaic/mode or excessive rectangle count.
// Colour-zero transparency is applied by the consumer's tile atlas.
[[nodiscard]] bool plan_bg2_rects(
    const simulation::SnesPpuState& ppu,
    std::int32_t scroll_x, std::int32_t scroll_y,
    std::vector<Bg2Rect>& output,
    std::size_t capacity = 14560U,
    std::uint32_t target_width = 256U,
    std::uint32_t target_height = 224U,
    std::int32_t origin_x = 0,
    std::int32_t origin_y = 0,
    bool cache_rolled_ground_samples = true);

} // namespace starfox::platform_3ds
