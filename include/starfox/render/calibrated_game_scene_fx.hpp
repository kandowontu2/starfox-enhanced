#pragma once
#include "starfox/render/calibrated_game_scene.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include <unordered_map>
namespace starfox::render {
inline bool calibrated_scene_fx_flow(simulation::GameFlowState flow) noexcept {
    return flow==simulation::GameFlowState::gameplay || flow==simulation::GameFlowState::training
        || flow==simulation::GameFlowState::intro;
}
inline bool valid_calibrated_scene_fx(const CalibratedGameFrame& frame) noexcept {
    if(!frame.current || !valid_scene_fx_world_frame(frame.scene_fx)
        || frame.settings.scene_enhancements>255 || frame.settings.particle_enhancements>15) return false;
    if(!frame.scene_fx.active()) return true;
    if(!calibrated_scene_fx_flow(frame.current->flow)) return false;
    for(double value:frame.scene_fx_origin) if(!std::isfinite(value) || std::abs(value)>1.e9) return false;
    for(float value:frame.scene_fx_source_to_rig) if(!std::isfinite(value) || std::abs(value)>1.e4F) return false;
    const auto& rig=frame.scene_fx_source_to_rig;
    if(rig[3]!=0 || rig[7]!=0 || rig[11]!=0 || rig[15]!=1) return false;
    for(unsigned n=0;n<frame.scene_fx.count;++n) {
        const auto type=unsigned(frame.scene_fx.points[n].type);
        const unsigned mode=type==0?frame.settings.scene_enhancements&3U
            :type==1?(frame.settings.scene_enhancements>>2)&3U
            :type==2?(frame.settings.scene_enhancements>>4)&3U
            :type<=5?(frame.settings.scene_enhancements>>6)&3U
            :type<=7?frame.settings.particle_enhancements&3U
            :(frame.settings.particle_enhancements>>2)&3U;
        if(!mode) return false;
    }
    return true;
}
struct CalibratedSceneFxInputs {
    std::array<double,3> origin{};
    vr::Matrix4 source_to_rig{};
    std::vector<SceneFxEmitter> emitters;
};
// Capture source-world centres, not screen pixels or centre-eye positions.
// Current draw-list ownership excludes source shadow passes, portraits/HUD,
// protected reticles and objects after the ordered source overlay boundary.
// Generation/attachment/trail interpolation is shared with native model poses.
template<class Emissive,class CentreScale>
CalibratedSceneFxInputs calibrated_scene_fx_inputs(const CalibratedGameFrame& frame,
    const vr::SceneInterpolationRules& rules,Emissive emissive,CentreScale centre_scale) {
    if(!frame.current || !frame.previous || !std::isfinite(frame.alpha) || frame.alpha<0 || frame.alpha>1)
        throw std::invalid_argument("Invalid native scene-effect capture");
    const auto& now=*frame.current;const auto& old=*frame.previous;
    const double alpha=old.flow!=now.flow || timing::camera_transform_is_discontinuous(old.camera,now.camera)?1:frame.alpha;
    const auto camera=timing::interpolate(old.camera,now.camera,alpha);
    const auto view=simulation::interpolate_rotation_matrix_q15(old.view_matrix,now.view_matrix,alpha);
    CalibratedSceneFxInputs result;result.origin={camera.x,camera.y,camera.z};result.source_to_rig[15]=1;
    constexpr float sign[]{1,-1,-1};
    for(unsigned column=0;column<3;++column) for(unsigned row=0;row<3;++row)
        result.source_to_rig[column*4+row]=float(view[column*3+row])*sign[row]/(32768.F*256);
    if(!calibrated_scene_fx_flow(now.flow) || !(frame.settings.scene_enhancements|frame.settings.particle_enhancements)) return result;
    std::unordered_map<std::uint32_t,bool> visible;
    for(const auto& draw:frame.draws) if(draw.source_key && draw.source_key<=0xffff && !draw.after_rays
        && draw.layer==CalibratedGameLayer::model)
        visible[draw.source_key]=visible[draw.source_key] || !draw.packet.preserve_native_colour;
    for(const auto& item:now.objects) {
        const auto drawn=visible.find(item.handle);if(drawn==visible.end()) continue;
        const bool weapon=emissive(item.object.shape) && item.handle!=now.player;
        if(!drawn->second && !weapon) continue;
        const bool explosion=(item.object.flags&1U)!=0 && item.object.count>0;
        if(!explosion && !weapon && item.handle!=now.player && !(frame.settings.particle_enhancements&3U)) continue;
        const auto source=vr::interpolate_scene_object(old,now,item,alpha,rules);
        std::array<double,3> position{source.transform.x,source.transform.y,source.transform.z};
        const double scale=centre_scale(item.object.strategy_address);
        if(!std::isfinite(scale) || scale<=0 || scale>4) throw std::invalid_argument("Invalid native source-centre scale");
        if(scale!=1) for(unsigned axis=0;axis<3;++axis) {
            double delta=std::fmod(position[axis]-result.origin[axis],65536.);
            if(delta>32767.) delta-=65536.;else if(delta< -32768.) delta+=65536.;
            position[axis]=result.origin[axis]+delta*scale;
        }
        result.emitters.push_back({(item.presentation.generation<<16)|item.handle,position,
            explosion,weapon,item.handle==now.player,item.object.health});
    }
    return result;
}
}
