#pragma once

#include "starfox/render/framebuffer.hpp"
#include <span>

namespace starfox::platform::nintendo_3ds {

// Native mono's two existing indexed rasters, before the temporary 400x240
// composition. Source coverage owns even index zero; the world retains its
// existing zero-ink transparency. No allocation, cross-frame cache or GPU work.
inline bool valid_mode2_foreground_sources(const render::Framebuffer& world,
    const render::Framebuffer& source) noexcept {
    return world.width() == 400U && world.height() == 240U
        && world.draw_scale() == 1U && source.width() == 256U
        && source.height() == 224U && source.draw_scale() == 1U
        && source.write_coverage().size() == source.pixels().size();
}

// Palette entries are opaque RGB5A1. Writes only the 400 active columns of
// each row; the caller keeps the existing texture padding. Output must not
// alias either source. Validate all extents before touching borrowed storage.
inline bool pack_mode2_foreground(const render::Framebuffer& world,
    const render::Framebuffer& source, std::span<const std::uint16_t, 256> colours,
    std::span<std::uint16_t> output, std::size_t stride) noexcept {
    if (!valid_mode2_foreground_sources(world, source) || stride < 400U
        || stride > output.size() / 240U) return false;
    const auto* world_pixels = world.pixels().data();
    const auto* source_pixels = source.pixels().data();
    const auto* coverage = source.write_coverage().data();
    for (unsigned y = 0; y < 240U; ++y) {
        const auto* world_row = world_pixels + y * 400U;
        auto* row = output.data() + y * stride;
        const auto world_span = [&](unsigned first, unsigned last) {
            for (auto x = first; x < last; ++x)
                row[x] = world_row[x] ? colours[world_row[x]] : 0U;
        };
        if (y < 8U || y >= 232U) {
            world_span(0U, 400U);
            continue;
        }
        world_span(0U, 72U);
        const auto source_row = (y - 8U) * 256U;
        for (unsigned x = 0; x < 256U; ++x) {
            const auto i = source_row + x;
            const auto pixel = world_row[x + 72U];
            row[x + 72U] = coverage[i] ? colours[source_pixels[i]]
                : pixel ? colours[pixel] : 0U;
        }
        world_span(328U, 400U);
    }
    return true;
}

} // namespace starfox::platform::nintendo_3ds
