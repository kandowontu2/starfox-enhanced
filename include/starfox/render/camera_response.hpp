#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <optional>
#include <span>
#include <vector>
#include <unordered_set>

namespace starfox::render {
struct CameraResponsePose {
    double pitch{},yaw{},roll{}; // Radians, presentation only.
};
inline bool camera_response_moves_world(CameraResponsePose pose) noexcept {
    return std::isfinite(pose.pitch) && std::isfinite(pose.yaw) && std::isfinite(pose.roll)
        && (pose.pitch!=0 || pose.yaw!=0 || pose.roll!=0);
}

// Caller supplies only positively identified player projectiles. Identities
// include object generation so a reused pool slot is a new shot. A volley
// observed in one simulation update produces one recoil impulse.
class CameraShotTracker {
    std::unordered_set<std::uint64_t> previous_;
    std::uint64_t epoch_{},serial_{};bool initialized_{};
public:
    std::uint64_t observe(std::uint64_t epoch,std::span<const std::uint64_t> projectiles) {
        std::unordered_set<std::uint64_t> now(projectiles.begin(),projectiles.end());
        if(!initialized_ || epoch!=epoch_) {epoch_=epoch;serial_=0;initialized_=true;}
        else if(std::any_of(now.begin(),now.end(),[&](auto id){return !previous_.contains(id);})) ++serial_;
        previous_=std::move(now);return serial_;
    }
};

// A pure camera rotation has one depth-independent perspective reprojection.
// Applying this to the composed world keeps sky, terrain, objects and their
// reflections together; HUD must be composited afterward, not reprojected.
// Row-major destination-ray -> source-ray matrix (inverse camera rotation).
inline std::array<double,9> camera_response_matrix(CameraResponsePose p) {
    const double x=std::cos(p.pitch),a=std::sin(p.pitch);
    const double y=std::cos(p.yaw),b=std::sin(p.yaw);
    const double z=std::cos(p.roll),c=std::sin(p.roll);
    return {y*z,y*c,-b, a*b*z-x*c,a*b*c+x*z,a*y, x*b*z+a*c,x*b*c-a*z,x*y};
}
// Equivalent native world rotation, about the game rig rather than either
// tracked eye. Flat destination rays sample R * destination; native source
// geometry therefore uses R-transpose. Conjugate +Y down/+Z forward into the
// OpenXR +Y up/-Z forward basis. Runtime eye poses and HUD never change.
inline std::optional<std::array<float,16>> camera_response_world_transform(CameraResponsePose pose) {
    for(double angle:{pose.pitch,pose.yaw,pose.roll})
        if(!std::isfinite(angle) || std::abs(angle)>.05) return {};
    std::array<float,16> result{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    if(!camera_response_moves_world(pose)) return result;
    const auto inverse=camera_response_matrix(pose);
    constexpr double basis[]{1,-1,-1};
    for(unsigned row=0;row<3;++row) for(unsigned col=0;col<3;++col)
        result[col*4+row]=float(basis[row]*inverse[col*3+row]*basis[col]);
    return result;
}
inline std::optional<std::array<double,2>> camera_response_source(
    const std::array<double,9>& matrix,double px,double py,
    double cx,double cy,double focal_x,double focal_y) {
    if(!(focal_x>0 && focal_y>0) || !std::isfinite(focal_x) || !std::isfinite(focal_y)) return {};
    const double x=(px-cx)/focal_x,y=(py-cy)/focal_y;
    const double rx=matrix[0]*x+matrix[1]*y+matrix[2];
    const double ry=matrix[3]*x+matrix[4]*y+matrix[5];
    const double rz=matrix[6]*x+matrix[7]*y+matrix[8];
    if(!std::isfinite(rx) || !std::isfinite(ry) || !std::isfinite(rz) || rz<=1e-6) return {};
    return std::array<double,2>{cx+focal_x*rx/rz,cy+focal_y*ry/rz};
}

// Reference pass over a WORLD-ONLY image. UI/portraits/crosshairs are composited
// by the caller afterward. Do not pass a frame with HUD already baked into it.
// A small perspective crop covers the rotated frame without stretching its
// borders. Corners bound the homography while all denominators stay positive.
inline std::optional<double> camera_response_crop(unsigned width,unsigned height,
    double focal_x,double focal_y,const std::array<double,9>& matrix) {
    if(width<2 || height<2) return {};
    const double cx=(width-1)*.5,cy=(height-1)*.5;
    const auto covered=[&](double zoom) {
        for(double y:{0.,double(height-1)}) for(double x:{0.,double(width-1)}) {
            const auto p=camera_response_source(matrix,cx+(x-cx)/zoom,cy+(y-cy)/zoom,cx,cy,focal_x,focal_y);
            if(!p || (*p)[0]<0 || (*p)[0]>width-1 || (*p)[1]<0 || (*p)[1]>height-1) return false;
        }
        return true;
    };
    double lower=1,upper=1.25;
    if(!covered(upper)) return {};
    if(covered(lower)) return lower;
    for(unsigned i=0;i<24;++i) {const double middle=(lower+upper)*.5;if(covered(middle)) upper=middle;else lower=middle;}
    return upper;
}
inline bool apply_camera_response(std::span<const std::uint8_t> world,
    std::vector<std::uint8_t>& output,unsigned width,unsigned height,
    double focal_x,double focal_y,CameraResponsePose pose) {
    if(width<2 || height<2 || world.size()!=std::size_t(width)*height*4
        || !std::isfinite(pose.pitch) || !std::isfinite(pose.yaw) || !std::isfinite(pose.roll)
        || !(focal_x>0 && focal_y>0) || !std::isfinite(focal_x) || !std::isfinite(focal_y)) return false;
    if(pose.pitch==0 && pose.yaw==0 && pose.roll==0) {
        if(world.data()!=output.data()) output.assign(world.begin(),world.end());
        return true;
    }
    const auto matrix=camera_response_matrix(pose);
    const double cx=(width-1)*.5,cy=(height-1)*.5;
    const auto source=[&](double x,double y,double zoom) {
        return camera_response_source(matrix,cx+(x-cx)/zoom,cy+(y-cy)/zoom,cx,cy,focal_x,focal_y);
    };
    const auto crop=camera_response_crop(width,height,focal_x,focal_y,matrix);
    if(!crop) return false;
    std::vector<std::uint8_t> result(world.size());
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        const auto p=source(x,y,*crop);if(!p) return false;
        const double sx=std::clamp((*p)[0],0.,double(width-1)),sy=std::clamp((*p)[1],0.,double(height-1));
        const auto ix=unsigned(sx),iy=unsigned(sy),jx=std::min(ix+1,width-1),jy=std::min(iy+1,height-1);
        const double tx=sx-ix,ty=sy-iy;
        for(unsigned c=0;c<4;++c) {
            const auto at=[&](unsigned a,unsigned b){return world[(std::size_t(b)*width+a)*4+c];};
            result[(std::size_t(y)*width+x)*4+c]=std::uint8_t(std::lround(
                (at(ix,iy)*(1-tx)+at(jx,iy)*tx)*(1-ty)+(at(ix,jy)*(1-tx)+at(jx,jy)*tx)*ty));
        }
    }
    output=std::move(result);return true;
}

// Explicit coverage, not a colour comparison: black HUD pixels and HUD ink
// matching the underlying world must still be restored at their original
// screen positions. `world` must retain what was behind those pixels.
inline bool composite_camera_response(std::span<const std::uint8_t> world,
    std::span<const std::uint8_t> unwarped_final,std::span<const std::uint8_t> hud_coverage,
    std::vector<std::uint8_t>& output,unsigned width,unsigned height,
    double focal_x,double focal_y,CameraResponsePose pose) {
    const auto pixels=std::size_t(width)*height;
    if(unwarped_final.size()!=pixels*4 || hud_coverage.size()!=pixels) return false;
    std::vector<std::uint8_t> result;
    if(!apply_camera_response(world,result,width,height,focal_x,focal_y,pose)) return false;
    for(std::size_t i=0;i<pixels;++i) if(hud_coverage[i])
        std::copy_n(unwarped_final.begin()+i*4,4,result.begin()+i*4);
    output=std::move(result);return true;
}

// Independent two-bit strengths for impact, recoil and banking. Events come
// from simulation observations, never controller polling or presentation FPS.
// No simulation state or random generator is modified.
class CameraResponse {
    double time_{-1},impact_time_{-1},recoil_time_{-1};
    double impact_{},recoil_{},bank_{},bank_velocity_{};
    std::uint64_t epoch_{},shot_{};
    unsigned modes_{},health_{};
    CameraResponsePose pose_{};
public:
    CameraResponsePose update(double time,std::uint64_t epoch,unsigned modes,
        unsigned health,std::uint64_t shot,double bank_input,bool paused=false) {
        modes&=63;
        if(!std::isfinite(time) || !std::isfinite(bank_input)) { *this={};return {}; }
        if(!modes || time_<0 || epoch!=epoch_ || modes!=modes_ || time<time_ || (!paused && time-time_>1)) {
            *this={};time_=time;epoch_=epoch;modes_=modes;health_=health;shot_=shot;
            return {};
        }
        if(paused) {
            // Freeze both pose and response ages even if a host clock advances.
            const double elapsed=time-time_;
            if(impact_time_>=0) impact_time_+=elapsed;
            if(recoil_time_>=0) recoil_time_+=elapsed;
            time_=time;health_=health;shot_=shot;return pose_;
        }
        const double dt=time-time_;
        if((modes&3) && health<health_) {
            impact_=std::min(1.,double(health_-health)/32.);
            impact_time_=time;
        }
        if(((modes>>2)&3) && shot!=shot_) {
            recoil_=1;recoil_time_=time;
        }
        health_=health;shot_=shot;time_=time;
        const double target=std::clamp(bank_input,-1.,1.)*((modes>>4)&3)*.008;
        // Exact critically damped spring for a constant target over dt.
        const double omega=9,offset=bank_-target,c=bank_velocity_+omega*offset;
        const double decay=std::exp(-omega*dt);
        bank_=target+(offset+c*dt)*decay;
        bank_velocity_=(bank_velocity_-omega*c*dt)*decay;
        pose_={0,0,bank_};
        if(impact_time_>=0) {
            const double age=time-impact_time_,a=impact_*(modes&3)*.003*std::exp(-age*9);
            pose_.pitch+=a*std::sin(age*57);
            pose_.yaw+=a*std::sin(age*43);
        }
        if(recoil_time_>=0) {
            const double age=time-recoil_time_;
            pose_.pitch-=recoil_*((modes>>2)&3)*.003*std::exp(-age*18);
        }
        return pose_;
    }
};
}
