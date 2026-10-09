#pragma once
#include "starfox/render/shadow_mask.hpp"
#include <array>
#include <cstdint>
namespace starfox::vr {
struct SourceShadowEnvironment {
    std::optional<render::shadows::ReceiverPlane> ground;
    render::shadows::Vec3 light;
};
inline std::optional<SourceShadowEnvironment> source_shadow_environment(
    const std::array<int16_t,9>& view,double camera_y,double ground_y,bool enabled,double depth_offset=0) {
    using namespace render::shadows;
    if(!std::isfinite(camera_y) || !std::isfinite(ground_y) || !std::isfinite(depth_offset)) return {};
    const Vec3 x{view[0]/32768.,-view[1]/32768.,-view[2]/32768.};
    const Vec3 y{view[3]/32768.,-view[4]/32768.,-view[5]/32768.};
    const Vec3 z{view[6]/32768.,-view[7]/32768.,-view[8]/32768.};
    // Q15 rotations aren't perfectly orthogonal. Construct the plane normal
    // from its two tangent axes, rather than assuming Y is perpendicular.
    auto normal=cross(z,x);const double length=std::sqrt(dot(normal,normal));
    if(!std::isfinite(length) || length<1e-10) return {};
    normal=normal*(1/length);
    double height=std::fmod(ground_y-camera_y,65536.);
    if(height>32767.) height-=65536.;else if(height< -32768.) height+=65536.;
    SourceShadowEnvironment result;result.light=(x+y+z)*-1;
    if(enabled) result.ground=ReceiverPlane{y*(height/256.)+Vec3{0,0,depth_offset},normal};
    return result;
}
}
