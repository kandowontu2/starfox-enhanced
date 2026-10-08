#pragma once
#include "starfox/vr/game_scene.hpp"
namespace starfox::vr {
// Source interpolation has no OpenXR/graphics dependency. Console renderers
// and tracked-headset renderers share the same attachment/birth/FX semantics.
struct SceneInterpolationRules {
    uint32_t trail{},flash_player{},crosshair{};
    uint16_t discrete_rotation_shape{};
    bool fixed_landscape_height{};
};
struct SceneObjectInterpolation {
    render::ObjectPresentationSnapshot previous,current;
    timing::RenderTransform transform;
    double alpha{};
};
SceneObjectInterpolation interpolate_scene_object(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,const GameSceneObject&,double alpha,const SceneInterpolationRules&);
// Poses align with current.objects. Only geometry changes: source lighting,
// LOD depth, material state and source animation frames remain current.
std::vector<render::RenderPose> interpolate_scene_poses(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,double alpha,const SceneInterpolationRules&,bool shadows=false);
}
