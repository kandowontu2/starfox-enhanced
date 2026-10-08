#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/background_tiles.hpp"
#include <cmath>
#include <stdexcept>
namespace starfox::vr {
simulation::MatrixQ15 landscape_scene_view(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha) {
    return simulation::interpolate_rotation_matrix_q15(previous.view_matrix,current.view_matrix,alpha);
}
Matrix4 landscape_camera_motion(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid landscape camera fraction");
    alpha=std::clamp(alpha,0.,1.);
    if(previous.flow!=current.flow || !same_landscape_mapping(previous,current)
        || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) alpha=1.;
    const auto view=landscape_scene_view(previous,current,alpha);
    Matrix4 motion{};motion[15]=1;
    constexpr int sign[]{1,-1,-1};
    for(unsigned column=0;column<3;++column) for(unsigned row=0;row<3;++row)
        motion[column*4+row]=float(view[column*3+row])*sign[column]*sign[row]/32768.F;
    const auto camera=timing::interpolate(previous.camera,current.camera,alpha);
    const float delta=float(camera.y-current.camera.y)/256.F;
    for(unsigned row=0;row<3;++row) motion[12+row]=motion[4+row]*delta;
    return motion;
}
}
