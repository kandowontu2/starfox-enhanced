#include "starfox/vr/source_pose_interpolation.hpp"
#include <cmath>
#include <stdexcept>
namespace starfox::vr {
SceneObjectInterpolation interpolate_scene_object(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,const GameSceneObject& item,double alpha,const SceneInterpolationRules& rules) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid scene interpolation fraction");
    alpha=std::clamp(alpha,0.,1.);
    if(previous.flow!=current.flow || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) alpha=1;
    auto before=item.presentation,now=item.presentation;
    const auto old=previous.transforms.find(item.handle);
    if(old!=previous.transforms.end() && old->second.generation==now.generation
        && old->second.shape==now.shape && old->second.type==now.type && old->second.strategy_address==now.strategy_address) {
        // A recycled object (Space Armada's pipe sections jump from behind
        // the camera to the far end) is repositioned, not moving: blending
        // would draw it for a frame across the tunnel. Snap a jump of more
        // than 1500 source units relative to the camera within one tick.
        const auto wrap=[](double d) {d=std::fmod(d,65536.);if(d>32767.) d-=65536.;else if(d< -32768.) d+=65536.;return d;};
        const double dx=wrap(double(now.transform.x)-old->second.transform.x)-wrap(double(current.camera.x)-previous.camera.x);
        const double dy=wrap(double(now.transform.y)-old->second.transform.y)-wrap(double(current.camera.y)-previous.camera.y);
        const double dz=wrap(double(now.transform.z)-old->second.transform.z)-wrap(double(current.camera.z)-previous.camera.z);
        if(dx*dx+dy*dy+dz*dz<=1500.*1500.) before=old->second;
    } else if(item.object.flags&1U) {
        // A newly born explosion piece (makeMEDexpobj and friends, spawned on
        // a ship at its newest position) has no previous pose, so it would
        // sit up to a tick ahead of the ship it was born on, which is drawn
        // between ticks. Give it that ship's motion: the nearest object that
        // existed last tick (its attached parent when it has one).
        const GameSceneObject* owner{};double best=1500.*1500.;
        for(const auto& other:current.objects) {
            if(other.handle==item.handle || (other.object.flags&1U)) continue;
            const auto was=previous.transforms.find(other.handle);
            if(was==previous.transforms.end() || was->second.generation!=other.presentation.generation) continue;
            if(item.object.attached && other.handle==item.object.attached) {owner=&other;break;}
            const double dx=double(int16_t(uint16_t(other.presentation.transform.x-now.transform.x)));
            const double dy=double(int16_t(uint16_t(other.presentation.transform.y-now.transform.y)));
            const double dz=double(int16_t(uint16_t(other.presentation.transform.z-now.transform.z)));
            const double d=dx*dx+dy*dy+dz*dz;
            if(d<best) {best=d;owner=&other;}
        }
        if(owner) {
            const auto birth=timing::relative_birth_snapshot(now.transform,
                previous.transforms.at(owner->handle).transform,owner->presentation.transform);
            before.transform.x=birth.x;before.transform.y=birth.y;before.transform.z=birth.z;
        }
    }
    if(rules.crosshair && item.object.strategy_address==rules.crosshair) {
        const auto* station=render::reticle_previous_snapshot(now,current.transforms,previous.transforms,current.player);
        if(station) before=*station;
        else {
            before=now;
            const auto owner=current.transforms.find(current.player),old_owner=previous.transforms.find(current.player);
            if(owner!=current.transforms.end() && old_owner!=previous.transforms.end()) {
                before.transform=timing::relative_birth_snapshot(now.transform,old_owner->second.transform,owner->second.transform);
                before.rotation_matrix=old_owner->second.rotation_matrix;
            }
        }
    }
    if(rules.flash_player && item.object.strategy_address==rules.flash_player)
        render::anchor_player_overlay(before,now,previous.transforms,current.transforms,current.player);
    const auto object_alpha=rules.trail && item.object.strategy_address==rules.trail?1.:alpha;
    auto transform=timing::interpolate(before.transform,now.transform,object_alpha);
    if(rules.crosshair && item.object.strategy_address==rules.crosshair)
        transform.y+=std::lerp(double(previous.view_float_y),double(current.view_float_y),alpha);
    return {before,now,transform,object_alpha};
}
std::vector<render::RenderPose> interpolate_scene_poses(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,double alpha,const SceneInterpolationRules& rules,bool shadows) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid scene interpolation fraction");
    alpha=std::clamp(alpha,0.,1.);
    if(previous.flow!=current.flow || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) alpha=1;
    auto camera=timing::interpolate(previous.camera,current.camera,alpha);
    auto view=simulation::interpolate_rotation_matrix_q15(previous.view_matrix,current.view_matrix,alpha);
    const auto delta=[](double value,double origin) {
        auto d=std::fmod(value-origin,65536.);if(d>32767.) d-=65536.;else if(d< -32768.) d+=65536.;return d;
    };
    std::vector<render::RenderPose> poses;poses.reserve(current.objects.size());
    for(const auto& item:current.objects) {
        const auto sample=interpolate_scene_object(previous,current,item,alpha,rules);
        const auto& before=sample.previous;const auto& now=sample.current;const auto object_alpha=sample.alpha;
        auto transform=sample.transform;const auto old=previous.transforms.find(item.handle);
        auto rotation=render::interpolate_object_rotation(before,now,object_alpha,rules.discrete_rotation_shape);
        auto pose=item.source_pose;
        pose.explosion_phase=render::interpolate_explosion_progress(
            old==previous.transforms.end()?nullptr:&old->second,now,object_alpha);
        if(shadows) {
            rotation[1]=rotation[4]=rotation[7]=0;
            auto source_rotation=item.presentation.rotation_matrix;
            source_rotation[1]=source_rotation[4]=source_rotation[7]=0;
            pose.source_lighting_matrix=simulation::multiply_matrix_q15(source_rotation,current.view_matrix);
            pose.simple_scaled_sprite=false;
            if(!(item.object.strategy_flags[0]&4U)) {
                transform.y=current.shadow_height;
                pose.force_colour=true;pose.forced_colour=9;
            }
        }
        const auto x=delta(transform.x,camera.x),y=delta(transform.y,camera.y),z=delta(transform.z,camera.z);
        pose.x=(x*view[0]+y*view[3]+z*view[6])/32768.;
        pose.y=(x*view[1]+y*view[4]+z*view[7])/32768.;
        pose.z=(x*view[2]+y*view[5]+z*view[8])/32768.;
        pose.rotation_matrix=alpha>0 && alpha<1?simulation::multiply_presentation_matrix_q15(rotation,view)
            :simulation::multiply_matrix_q15(rotation,view);
        pose.use_rotation_matrix=true;
        pose.continuous_geometry=object_alpha!=1 || !(rules.trail && item.object.strategy_address==rules.trail);
        pose.subpixel_projection=object_alpha>0 && object_alpha<1;
        poses.push_back(pose);
    }
    return poses;
}
}
