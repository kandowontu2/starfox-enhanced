#pragma once
#include "starfox/render/calibrated_dlss_api.hpp"
#include "starfox/vr/eye_camera.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <optional>

namespace starfox::render {
// Viewport resources can retire independently of the SDK's frame-token cache.
// A reconnected owner must not reuse a token with different common constants.
// Shared across native owners/SDK restarts; exhaustion fails rather than wraps.
inline std::optional<std::uint32_t> next_calibrated_dlss_frame_index() noexcept {
    static std::atomic<std::uint64_t> next{1};
    const auto value=next.fetch_add(1,std::memory_order_relaxed);
    if(value>UINT32_MAX) return {};
    return static_cast<std::uint32_t>(value);
}
namespace calibrated_dlss_math {
inline std::optional<vr::Matrix4> inverse(const vr::Matrix4& matrix) noexcept {
    double rows[4][8]{};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) {
        if(!std::isfinite(matrix[c*4+r]) || std::abs(matrix[c*4+r])>1.e8F) return {};
        rows[r][c]=matrix[c*4+r];rows[r][c+4]=r==c;
    }
    for(unsigned c=0;c<4;++c) {
        unsigned pivot=c;
        for(unsigned r=c+1;r<4;++r) if(std::abs(rows[r][c])>std::abs(rows[pivot][c])) pivot=r;
        if(std::abs(rows[pivot][c])<1.e-12) return {};
        for(unsigned k=0;k<8;++k) std::swap(rows[c][k],rows[pivot][k]);
        const double divisor=rows[c][c];for(double& value:rows[c]) value/=divisor;
        for(unsigned r=0;r<4;++r) if(r!=c) {
            const double factor=rows[r][c];for(unsigned k=0;k<8;++k) rows[r][k]-=factor*rows[c][k];
        }
    }
    vr::Matrix4 result{};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) {
        const auto v=rows[r][c+4];if(!std::isfinite(v) || std::abs(v)>1.e8) return {};
        result[c*4+r]=float(v);
    }
    return result;
}
inline vr::Matrix4 multiply(const vr::Matrix4& a,const vr::Matrix4& b) noexcept {
    vr::Matrix4 result{};
    for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) {
        double value=0;for(unsigned k=0;k<4;++k) value+=double(a[k*4+r])*b[c*4+k];
        result[c*4+r]=float(value);
    }
    return result;
}
inline bool perspective(const vr::Matrix4& p) noexcept {
    for(float v:p) if(!std::isfinite(v)) return false;
    return p[0]>0 && p[5]!=0 && p[1]==0 && p[2]==0 && p[3]==0 && p[4]==0
        && p[6]==0 && p[7]==0 && p[11]==-1 && p[12]==0 && p[13]==0
        && p[15]==0 && p[10]<=-1 && p[14]<0;
}
}
// Calibrated cameras are unjittered, right-handed, in metres, with [0,1]
// clip depth. Native surface guides separately store positive cartridge depth
// (256 units/metre). SDK row-vector/row-major storage is the SAME flat array
// as our transposed column-vector/column-major transform, not a second transpose.
inline std::optional<StarfoxDlssFrameV1> calibrated_dlss_frame(
    const vr::EyeCamera& current,const vr::EyeCamera& previous,
    std::array<float,2> jitter,bool reset) noexcept {
    using namespace calibrated_dlss_math;
    if(!perspective(current.projection) || !perspective(previous.projection)) return {};
    for(const auto* v:{&current.view,&previous.view})
        if((*v)[3]!=0 || (*v)[7]!=0 || (*v)[11]!=0 || (*v)[15]!=1) return {};
    for(float v:jitter) if(!std::isfinite(v) || std::abs(v)>.5F) return {};
    const auto inv_projection=inverse(current.projection),world=inverse(current.view);
    if(!inv_projection || !world || !inverse(previous.view)) return {};
    const auto backwards=multiply(multiply(multiply(previous.projection,previous.view),*world),*inv_projection);
    const auto forwards=inverse(backwards);if(!forwards) return {};
    StarfoxDlssFrameV1 f{};f.size=sizeof(f);f.reset=reset;
    std::copy(current.projection.begin(),current.projection.end(),f.view_to_clip);
    std::copy(inv_projection->begin(),inv_projection->end(),f.clip_to_view);
    std::copy(backwards.begin(),backwards.end(),f.clip_to_previous);
    std::copy(forwards->begin(),forwards->end(),f.previous_to_clip);
    for(unsigned k=0;k<3;++k) {
        f.camera_position[k]=(*world)[12+k];f.camera_right[k]=(*world)[k];
        f.camera_up[k]=(*world)[4+k];f.camera_forward[k]=-(*world)[8+k];
    }
    // Reject scaled/sheared tracking views, not just singular matrices.
    for(const auto* axis:{f.camera_right,f.camera_up,f.camera_forward}) {
        double length=0;for(unsigned k=0;k<3;++k) length+=double(axis[k])*axis[k];
        if(std::abs(length-1)>.001) return {};
    }
    for(const auto axes:{std::pair{f.camera_right,f.camera_up},std::pair{f.camera_right,f.camera_forward},std::pair{f.camera_up,f.camera_forward}}) {
        double dot=0;for(unsigned k=0;k<3;++k) dot+=double(axes.first[k])*axes.second[k];
        if(std::abs(dot)>.001) return {};
    }
    const auto& p=current.projection;
    f.near_plane=p[14]/p[10];f.far_plane=p[10]==-1?1.e6F:p[14]/(p[10]+1);
    const double low=(p[9]-1)/p[5],high=(p[9]+1)/p[5];
    f.vertical_fov=float(std::abs(std::atan(high)-std::atan(low)));f.aspect=std::abs(p[5])/p[0];
    f.jitter[0]=jitter[0];f.jitter[1]=jitter[1];
    // Off-axis projection already encodes each calibrated frustum. Do not add
    // an optional pinhole correction on top of that physical projection.
    if(!(f.near_plane>0 && f.far_plane>f.near_plane && std::isfinite(f.far_plane)
        && f.vertical_fov>0 && f.vertical_fov<3.141593F && f.aspect>0 && std::isfinite(f.aspect))) return {};
    return f;
}
}
