#pragma once
#include "starfox/vr/eye_camera.hpp"
#include <algorithm>
#include <cmath>
namespace starfox::vr {
// Presentation preferences never write cartridge camera/input registers.
struct PresentationPreferences {
    bool cockpit{};
    unsigned head_translation{2}; // 0, 0.5, default 1, 1.5, 2 times head-centre displacement.
    unsigned world_scale{}; // Default1:1; metres per source scene metre.
    int origin_x{},origin_y{},origin_z{}; // centimetres relative to authored player reference
    bool follow_ship_rotation{}; // Opt-in pilot camera; physical head tracking stays independent.
    static constexpr float scales[]{1.F,.5F,.75F,1.25F,1.5F,2.F};
    float scale() const noexcept {return scales[world_scale<6?world_scale:0];}
    float translation_scale() const noexcept {return float(head_translation<5?head_translation:2)*.5F;}
    bool valid() const noexcept {return head_translation<5 && world_scale<6 && origin_x>=-100 && origin_x<=100
        && origin_y>=-100 && origin_y<=100 && origin_z>=-100 && origin_z<=100;}
};
inline constexpr Matrix4 identity_matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
inline Matrix4 multiply_matrix(const Matrix4& a,const Matrix4& b) noexcept {
    Matrix4 out{};
    for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r)
        for(unsigned k=0;k<4;++k) out[c*4+r]+=a[k*4+r]*b[c*4+k];
    return out;
}
// Seat in the cutscene Arwing's canopy, ~0.3 m above its top, in the scaled
// ship reference; saved offsets remain additive.
inline constexpr std::array<float,3> cockpit_seat_m{0.F,.95F,.75F};
// The cabin encloses the live ship enlarged to pilot size. The world shares
// that enlargement so the native ship, its shots and the scenery match the
// cabin; world_scale stays a multiplier on top.
inline constexpr float cockpit_ship_scale=24.F; // ~9.4 m Arwing; the cabin fits inside the fuselage
inline float cockpit_world_scale(const PresentationPreferences& preferences) noexcept {
    return preferences.scale()*cockpit_ship_scale;
}
inline constexpr float interface_panel_distance=1.75F;
inline constexpr float interface_panel_width=1.15F;
// Initial comfort value, not tuned on hardware yet. Keep the original angular size.
inline constexpr float overlay_panel_distance=.75F;
inline constexpr float panel_width_at(float distance) noexcept {
    return interface_panel_width*distance/interface_panel_distance;
}
// Independent physical width/distance. Do not reuse source_layer_matrix:
// its perspective-sized plane is authoritative for source aiming.
inline Matrix4 panel_matrix(float width=interface_panel_width,float distance=interface_panel_distance) noexcept {
    const float s=width/256.F;
    return {s,0,0,0,0,-s,0,0,0,0,1,0,-128*s,112*s,-distance,1};
}
inline Matrix4 overlay_panel_matrix() noexcept {
    return panel_matrix(panel_width_at(overlay_panel_distance),overlay_panel_distance);
}
inline EyeCamera panel_raster_camera() noexcept {
    EyeCamera out;out.view=identity_matrix;
    out.projection={2.F/256,0,0,0,0,2.F/224,0,0,0,0,1,0,-1,-1,0,1};return out;
}
// Capture in the real runtime LOCAL space, with yaw only: level text, stable
// world position. Both eyes see one retained pose, not per-eye head locking.
class WorldPanelAnchor {
public:
    void reset() noexcept {pose_.reset();}
    XrPosef pose(const std::array<XrView,2>& views,float distance=interface_panel_distance) noexcept {
        // A family transition must not retain the previous far/near anchor.
        if(distance!=distance_) {pose_.reset();distance_=distance;}
        if(!pose_) {
            const auto& q=views[0].pose.orientation;
            const float yaw=std::atan2(2*(q.w*q.y+q.x*q.z),1-2*(q.y*q.y+q.x*q.x));
            XrPosef p{};p.orientation={0,std::sin(yaw*.5F),0,std::cos(yaw*.5F)};
            p.position={(views[0].pose.position.x+views[1].pose.position.x)*.5F-distance*std::sin(yaw),
                (views[0].pose.position.y+views[1].pose.position.y)*.5F,
                (views[0].pose.position.z+views[1].pose.position.z)*.5F-distance*std::cos(yaw)};
            pose_=p;
        }
        return *pose_;
    }
private:std::optional<XrPosef> pose_;float distance_{};
};
}
