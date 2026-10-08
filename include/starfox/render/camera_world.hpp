#pragma once
#include "starfox/render/framebuffer.hpp"

namespace starfox::render {
// Build the pre-HUD world with the same clipping, scaling and mosaic rules as
// normal cartridge composition. Never infer HUD ownership from its colour.
// Recorded GPU sources must first be replayed or use the resident compositor.
inline bool composite_camera_world(const Framebuffer& source, Framebuffer& world,
    const LayerCompositeSettings& settings) {
    if(source.command_buffer() || world.command_buffer()
        || !source.layer_tags_enabled() || !world.layer_tags_enabled()) return false;
    auto geometry=source;
    geometry.end_write_coverage();
    for(std::size_t i=0;i<geometry.pixels().size();++i) {
        if(geometry.layer_tags()[i]!=static_cast<std::uint8_t>(PixelLayer::two_d)) continue;
        geometry.pixels()[i]=0;
        // A dither pair can make colour zero opaque; remove its provenance too.
        geometry.mark_written(i);
    }
    composite_transparent_layer(geometry,world,settings);
    return true;
}
}
