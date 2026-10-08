#pragma once
#include "starfox/vr/scene_types.hpp"
#include "starfox/render/face_material.hpp"
#include "starfox/render/palette.hpp"
namespace starfox::vr {
// Both dither colors remain distinct. Returns false for textures until the
// texture pipeline handles them; never silently replaces them with flat ink.
// Set srgb_target for an sRGB color attachment, whose stores encode linear RGB.
bool apply_scene_material(SceneVertex&,const render::FaceMaterial&,
    std::span<const render::Rgba8> palette,uint8_t palette_base,uint32_t dither_scale,
    bool srgb_target);
}
