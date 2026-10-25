#pragma once
#include "starfox/vr/source_models.hpp"
#include <algorithm>
#include <cstdint>
#include <span>

namespace starfox::modern {
inline bool source_models_supported(std::span<const vr::SourceComputeModel> models) noexcept {
    return std::none_of(models.begin(),models.end(),[](const auto& model) {return model.warp || model.axis;});
}
// render::StereoOutput::sr_platform: the Leia SR runtime weaves its own
// eyes, and only the existing presenter drives it.
inline constexpr std::uint8_t stereo_sr_platform=9;
constexpr bool flat_frame_supported(bool setup,bool options,std::uint8_t stereo_output) noexcept {
    // Every TV stereo format (SBS, over-under, interlaced, anaglyph,
    // cross-view) draws both eyes in MODERN; Leia SR keeps GPU FAST.
    return !setup && !options && stereo_output!=stereo_sr_platform;
}
// TV stereo cameras for one eye (0 left, 1 right), as GPU FAST's rig places
// them: parallel eyes `separation` source units apart, converged at
// `convergence` source units, so that distance has no disparity.
// - world: the eye's own camera for 3D geometry. The eye offset is folded
//   into the projection; the view (and so every world position the
//   ray-traced shadows and lights use) stays the mono camera's.
// - backdrop: flat background art and sky, at infinite distance.
// - reticle: the reticle at `crosshair_depth` source units (0 = the screen
//   plane, no disparity, like the HUD).
// The HUD itself keeps the mono camera.
struct StereoEyeCameras {vr::EyeCamera world,backdrop,reticle;};
inline StereoEyeCameras stereo_eye_cameras(const vr::EyeCamera& mono,unsigned eye,
    double separation,double convergence,double crosshair_depth) noexcept {
    StereoEyeCameras result{mono,mono,mono};
    if(!(separation>0) || !(convergence>0)) return result;
    // Scene metres are 256 source units. Clip x = m0 x + m8 z + m12 with
    // w = -z: moving the eye by e (translating x by -e) adds -e m0 to m12,
    // and m8 -= m0 e / C brings depth C back to the mono position.
    const double e=(eye?1.:-1.)*separation*.5/256.,c=convergence/256.;
    const float m0=mono.projection[0];
    result.world.projection[12]-=float(e)*m0;
    result.world.projection[8]-=float(e/c)*m0;
    result.backdrop.projection[8]-=float(e/c)*m0;
    if(crosshair_depth>0) result.reticle.projection[8]-=float(e*(1./c-256./crosshair_depth))*m0;
    return result;
}
}
