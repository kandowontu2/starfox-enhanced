#pragma once

#include "starfox/platform/nintendo_3ds/ppu_tile_dependencies.hpp"

namespace starfox::platform::nintendo_3ds {

// Exact indexed-pixel dependencies for draw_bg1(all, origin=0, inset=16,
// transparent_cgram_black=true), without text outlines. RGB changes that keep
// a CGRAM entry nonblack do not change the indexed raster; presentation still
// uses the current palette. No VM revision, hash or scene heuristic is used.
[[nodiscard]] inline bool same_bg1_overlay_source(
    const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b) noexcept {
    const auto enabled = [](const auto& ppu) {
        return (ppu.main_screen & 1U) != 0U
            && ppu.background_mode >= 1U && ppu.background_mode <= 3U;
    };
    if (!enabled(a) || !enabled(b)) return enabled(a) == enabled(b);
    if (a.background_mode != b.background_mode
        || a.bg1_tile_size_16 != b.bg1_tile_size_16
        || a.bg1_character_base != b.bg1_character_base
        || a.bg1_screen_base != b.bg1_screen_base
        || a.bg1_screen_size != b.bg1_screen_size
        || a.bg1_scroll_x != b.bg1_scroll_x
        || a.bg1_scroll_y != b.bg1_scroll_y
        || (a.mosaic & 1U) != (b.mosaic & 1U)
        || ((a.mosaic & 1U) && (a.mosaic >> 4U) != (b.mosaic >> 4U)))
        return false;
    for (unsigned ink = 1U; ink < 256U; ++ink)
        if (((a.cgram[ink] & 32767U) == 0U)
            != ((b.cgram[ink] & 32767U) == 0U)) return false;
    return same_background_tiles(a, b, a.bg1_character_base,
        a.bg1_screen_base, a.bg1_screen_size,
        a.background_mode == 3U ? 8U : 4U, a.bg1_tile_size_16);
}

} // namespace starfox::platform::nintendo_3ds
