#pragma once
#include "starfox/vr/source_pose_interpolation.hpp"
#include "starfox/vr/background_tiles.hpp"
namespace starfox::vr {
simulation::MatrixQ15 landscape_scene_view(const GameSceneSnapshot&,const GameSceneSnapshot&,double alpha);
Matrix4 landscape_camera_motion(const GameSceneSnapshot&,const GameSceneSnapshot&,double alpha);
}
