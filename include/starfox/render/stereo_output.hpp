#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include "starfox/render/raster_commands.hpp"

namespace starfox::render {
enum class StereoOutput : std::uint8_t { off, half_sbs, full_sbs, half_top_bottom, full_top_bottom,
    interlaced, interlaced_reversed, anaglyph_red_cyan, crossview, sr_platform };
// Append formats: existing persisted IDs must not change.
inline constexpr unsigned stereo_output_count=10;
// Independent eye owners submit every consumer on the same SDL queue before
// the next frame can overwrite their backing. They can retain bounded pending
// fences instead of synchronously joining every previous eye. A shared owner
// (mono, separated-model fallback) or explicit diagnostic wait cannot do so.
inline constexpr bool stereo_ordered_layer_reuse(bool independent_eye,bool forced_wait) noexcept {
    return independent_eye && !forced_wait;
}
inline bool stereo_interlaced(StereoOutput mode) noexcept {
    return mode==StereoOutput::interlaced || mode==StereoOutput::interlaced_reversed;
}
inline bool stereo_overlay(StereoOutput mode) noexcept {
    return stereo_interlaced(mode) || mode==StereoOutput::anaglyph_red_cyan;
}
inline bool stereo_double_width(StereoOutput mode) noexcept {
    return mode==StereoOutput::full_sbs || mode==StereoOutput::crossview;
}
// Translate only verified reticle OBJ commands, retaining their exact position
// in the ordered batch. The caller owns this eye's copy; mono data stays intact.
// Commands use stored pixels, while displacement is in logical game pixels.
inline bool stereo_translate_crosshair(RasterCommands& batch,double displacement) noexcept {
    if(!std::isfinite(displacement) || std::abs(displacement)>65536) return false;
    const auto eligible=[](const RasterCommand& c) {return c.textured==4 && (c.reserved1&4U)!=0;};
    for(const auto& c:batch.commands) if(eligible(c)) {
        if(c.du<1 || c.du>10) return false;
        const auto offset=std::llround(displacement*c.du);
        for(const auto value:{c.left,c.right,c.u}) {
            const auto moved=std::int64_t(value)+offset;
            if(moved<std::numeric_limits<std::int32_t>::min() || moved>std::numeric_limits<std::int32_t>::max()) return false;
        }
    }
    for(auto& c:batch.commands) if(eligible(c)) {
        const auto offset=std::llround(displacement*c.du);
        c.left=std::int32_t(c.left+offset);c.right=std::int32_t(c.right+offset);c.u=std::int32_t(c.u+offset);
    }
    return true;
}
// SDL renderer packing of already-composited eyes. Target must already be set
// and cleared. Interlaced width/height and viewport are FINAL physical pixels;
// never resize its result. Optional viewport is {x,y,width,height} for bars.
bool render_stereo_overlay(void* renderer,void* left,void* right,StereoOutput mode,
    std::uint32_t width,std::uint32_t height,
    std::optional<std::array<float,4>> viewport=std::nullopt);
// TV stereo uses game-world units, not centimetres. Keep the convergence
// beyond the player's ship so nearby geometry has visible crossed disparity.
// At focal length 256 the far disparity is four logical pixels. All geometry,
// particles, shadows and reflections must share this rig (VR has its own IPD).
inline constexpr double kSbsEyeSeparation = 16.0;
inline constexpr double kSbsConvergence = 1024.0;
inline constexpr float sbs_eye_x(unsigned eye) noexcept {
    return float((eye ? 1.0 : -1.0) * kSbsEyeSeparation * .5);
}
// GPU-only final packing, after independent per-eye effects. Source textures
// are equally sized sampler-capable 2D textures; destination is color-target
// capable, sized by stereo_output_layout(mode,width,height). All handles share
// the caller's SDL GPU device. No active render/copy pass may be open.
// Caller owns command submission and synchronization. No CPU pixel copy.
bool enqueue_stereo_texture_pack(void* command,void* left,void* right,
    void* destination,StereoOutput mode,std::uint32_t width,std::uint32_t height);
struct StereoOutputLayout {
    std::uint32_t width{}, height{}, eye_count{};
    struct Viewport { std::uint32_t x{}, width{}, y{}, height{}; };
    std::array<Viewport, 2> eyes{};
    // The scene aspect is independent of the squeezed Half-SBS viewport.
    double scene_aspect{};
};
inline std::optional<StereoOutputLayout> stereo_output_layout(
    StereoOutput mode, std::uint32_t width, std::uint32_t height) noexcept {
    if (!width || !height) return {};
    StereoOutputLayout result{width, height, 1, {{{0, width}, {0, 0}}},
        double(width) / height};
    result.eyes[0].height=result.eyes[1].height=height;
    switch (mode) {
    case StereoOutput::off: return result;
    case StereoOutput::half_sbs:
        // Equal-sized eyes are essential to matching disparity. The caller
        // must select an even target size rather than stretch one eye.
        if (width % 2 || width < 2) return {};
        result.eyes = {{{0, width / 2}, {width / 2, width / 2}}};
        result.eyes[0].height=result.eyes[1].height=height;
        break;
    case StereoOutput::full_sbs:
    case StereoOutput::crossview:
    case StereoOutput::sr_platform:
        if (width > std::numeric_limits<std::uint32_t>::max() / 2) return {};
        result.width = width * 2;
        result.eyes = {{{0, width}, {width, width}}};
        result.eyes[0].height=result.eyes[1].height=height;
        // Cross-eyed viewing exchanges only the final viewports, never the
        // physical eye cameras or the eye-specific effects/history.
        if(mode==StereoOutput::crossview) std::swap(result.eyes[0],result.eyes[1]);
        break;
    case StereoOutput::half_top_bottom:
        if(height%2 || height<2) return {};
        result.eyes={{{0,width,0,height/2},{0,width,height/2,height/2}}};
        break;
    case StereoOutput::full_top_bottom:
        if(height>std::numeric_limits<std::uint32_t>::max()/2) return {};
        result.height=height*2;
        result.eyes={{{0,width,0,height},{0,width,height,height}}};
        break;
    case StereoOutput::interlaced:
    case StereoOutput::interlaced_reversed:
    case StereoOutput::anaglyph_red_cyan:
        result.eyes={{{0,width,0,height},{0,width,0,height}}};
        break;
    default: return {};
    }
    result.eye_count = 2;
    return result;
}

// Parallel (not toe-in) cameras: translate world X by -eye_x before
// projection, then add projection_offset_x in normalized device space.
// This keeps vertical disparity zero and the convergence plane stationary.
struct StereoEyeProjection { double eye_x{}, projection_offset_x{}; };
// Horizontal displacement of a point originally projected by the mono camera.
// depth=nullopt is an infinite-distance backdrop; a finite depth can place
// reticles and other world-attached artwork without shifting screen-space HUD.
// Return logical pixels when focal_length is expressed in logical pixels.
inline std::optional<double> stereo_layer_displacement(unsigned eye,
    double separation,double convergence,double focal_length,
    std::optional<double> depth=std::nullopt) noexcept {
    if(eye>1 || !std::isfinite(separation) || separation<=0
        || !std::isfinite(convergence) || convergence<=0
        || !std::isfinite(focal_length) || focal_length<=0
        || (depth && (!std::isfinite(*depth) || *depth<=0))) return {};
    const double eye_x=(eye?1.:-1.)*separation*.5;
    const double displacement=focal_length*eye_x*(1./convergence-(depth?1. / *depth:0.));
    if(!std::isfinite(displacement)) return {};
    return displacement;
}
inline std::optional<std::array<StereoEyeProjection, 2>> stereo_eye_projections(
    double separation, double convergence, double horizontal_focal_scale) noexcept {
    if (!std::isfinite(separation) || separation <= 0
        || !std::isfinite(convergence) || convergence <= 0
        || !std::isfinite(horizontal_focal_scale) || horizontal_focal_scale <= 0)
        return {};
    const double eye = separation / 2;
    const double offset = horizontal_focal_scale * (eye / convergence);
    if (!std::isfinite(offset) || offset == 0) return {};
    return std::array<StereoEyeProjection, 2>{{{-eye, -offset}, {eye, offset}}};
}
}
