// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/snes_ppu.hpp"

#include <span>

namespace starfox::platform_3ds {

enum class NativeCanvasStyle {
    gameplay,
    intro,
    controls,
    star_map,
    space,
    game_over,
    plain,
};

// Compose a native top-screen image without scaling the cartridge's 256x224
// artwork. Only world geometry outside that artwork is copied into the added
// gameplay columns; the source frame keeps its original layer priority.
void compose_native_canvas(const render::Framebuffer& source,
    const render::Framebuffer& world, render::Framebuffer& target,
    const simulation::SnesPpuState& ppu, NativeCanvasStyle style,
    std::span<const std::uint8_t> background_left = {},
    std::span<const std::uint8_t> background_right = {},
    int intro_vertical_shift = 0) noexcept;

// Place only source pixels actually written by foreground passes over the
// native world. Palette index zero and same-colour writes remain opaque when
// their coverage bit is set; unwritten centre pixels retain the 3D world.
void overlay_source_coverage_on_native(const render::Framebuffer& source,
    render::Framebuffer& target) noexcept;

// The shared tile renderer addresses 224 logical rows. Place those rows at
// native y=8, holding the first/last source row in the LCD's extra margins.
// No resampling, source-palette change or synthetic ending-star extension.
void compose_mode2_background(const render::Framebuffer& source,
    render::Framebuffer& target) noexcept;

} // namespace starfox::platform_3ds
