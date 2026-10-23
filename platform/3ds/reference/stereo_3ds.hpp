// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#pragma once

#include "starfox/render/software_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace starfox::platform_3ds {

// Keep the source word raster stable when the analogue slider jitters. The
// two parallel eye cameras converge at 256 source units, without toe-in.
inline std::uint8_t stereo_slider_strength(float slider) noexcept {
    if (!std::isfinite(slider) || slider <= 0.0F) return 0U;
    return static_cast<std::uint8_t>(std::clamp(
        std::lround(std::min(slider, 1.0F) * 8.0F), 1L, 8L));
}

inline void apply_stereo_eye(render::RenderPose& pose, int eye) {
    pose.x -= eye;
    // This imported renderer is mono-only. Slider stereo uses NativeGpu.
    if (eye != 0) throw std::logic_error("Reference stereo renderer is not enabled");
}

// Citro2D has 16-bit vertex indices and four vertices per quad. Reserve one
// foreground image per eye and reject a BG2 plan that cannot fit both views.
inline bool stereo_bg2_quads_fit(std::size_t quads) noexcept {
    return quads <= 8191U;
}

} // namespace starfox::platform_3ds
