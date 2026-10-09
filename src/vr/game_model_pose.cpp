#include "starfox/vr/game_model_pose.hpp"
#include "starfox/render/software_renderer.hpp"
#include <cmath>
#include <limits>
#include <numbers>
namespace starfox::vr {
std::optional<Matrix4> source_ui_layer_matrix(float vanish_x,float vanish_y,
    bool fixed_menu,bool inner_coordinates) noexcept {
    if(fixed_menu) {vanish_x=112;vanish_y=96;}
    const float guard=inner_coordinates?0.F:16.F;
    return source_layer_matrix(vanish_x+guard,vanish_y+guard);
}
std::optional<Matrix4> source_layer_matrix(float vanish_x,float vanish_y,float distance) noexcept {
    if(!std::isfinite(vanish_x) || !std::isfinite(vanish_y)
        || !std::isfinite(distance) || distance<=0) return std::nullopt;
    const float scale=distance/256.F;
    Matrix4 result{scale,0,0,0,0,-scale,0,0,0,0,1,0,
        -vanish_x*scale,vanish_y*scale,-distance,1};
    for(const auto value:result) if(!std::isfinite(value)) return std::nullopt;
    return result;
}
std::optional<Matrix4> game_model_matrix(const render::RenderPose& pose,float units) noexcept {
    if(!std::isfinite(units) || units<=0 || !std::isfinite(pose.x) || !std::isfinite(pose.y) || !std::isfinite(pose.z)) return std::nullopt;
    Matrix4 result{};result[15]=1;
    const auto put=[&](unsigned column,double x,double y,double z) {
        const double values[]{x/units,-y/units,-z/units};
        for(unsigned row=0;row<3;++row) {
            if(!std::isfinite(values[row]) || std::abs(values[row])>std::numeric_limits<float>::max()) return false;
            result[column*4+row]=static_cast<float>(values[row]);
        }
        return true;
    };
    if(pose.use_rotation_matrix) {
        for(unsigned column=0;column<3;++column)
            if(!put(column,pose.rotation_matrix[column*3]/32768.,pose.rotation_matrix[column*3+1]/32768.,pose.rotation_matrix[column*3+2]/32768.)) return std::nullopt;
    } else {
        if(!std::isfinite(pose.pitch) || !std::isfinite(pose.yaw) || !std::isfinite(pose.roll)) return std::nullopt;
        constexpr double radians=2*std::numbers::pi/65536.;
        const auto cx=std::cos(pose.pitch*radians),sx=std::sin(pose.pitch*radians);
        const auto cy=std::cos(pose.yaw*radians),sy=std::sin(pose.yaw*radians);
        const auto cz=std::cos(pose.roll*radians),sz=std::sin(pose.roll*radians);
        // Source order is pitch, then yaw, then roll.
        for(unsigned column=0;column<3;++column) {
            double x=column==0,y=column==1,z=column==2;
            const double py=y*cx-z*sx,pz=y*sx+z*cx;
            const double yx=x*cy+pz*sy,yz=-x*sy+pz*cy;
            if(!put(column,yx*cz-py*sz,yx*sz+py*cz,yz)) return std::nullopt;
        }
    }
    if(!put(3,pose.x,pose.y,pose.z)) return std::nullopt;
    return result;
}
}
