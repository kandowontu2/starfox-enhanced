#pragma once
#include "starfox/vr/eye_camera.hpp"
namespace starfox::render {struct RenderPose;}
namespace starfox::vr {
// Converts an already camera-relative game pose (+Y down, +Z forward) to
// native world coordinates (+Y up, -Z forward). Mesh decoding must apply
// pose.scale/header shifts beforehand, preserving word-coordinate bypass.
// This is the continuous geometry path, not Super FX per-product rounding.
std::optional<Matrix4> game_model_matrix(const render::RenderPose&,float scene_units_per_metre=256.F) noexcept;
// Place logical source pixels on the same viewing rays as native models.
// Vanishing point includes the host's bitmap origin; focal length is 256 pixels.
std::optional<Matrix4> source_layer_matrix(float vanish_x,float vanish_y,
    float distance_metres=2.F) noexcept;
// Native EX menu pages move the model preview's vanishing point. Their
// screen-space text must remain on one centered plane, independent of it.
std::optional<Matrix4> source_ui_layer_matrix(float vanish_x,float vanish_y,
    bool fixed_menu,bool inner_coordinates=false) noexcept;
}
