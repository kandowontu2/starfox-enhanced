#include "starfox/vr/scene_material.hpp"
#include <cmath>
namespace starfox::vr {
bool apply_scene_material(SceneVertex& vertex,const render::FaceMaterial& material,
    std::span<const render::Rgba8> palette,uint8_t base,uint32_t scale,bool srgb) {
    if(material.texture || (material.colour.dither && !scale)) return false;
    const auto even=static_cast<uint8_t>(base+material.colour.even);
    const auto odd=static_cast<uint8_t>(base+material.colour.odd);
    if(even>=palette.size() || odd>=palette.size()) return false;
    const auto channel=[srgb](uint8_t value) {
        const float x=value/255.0F;
        return !srgb?x:x<=.04045F?x/12.92F:std::pow((x+.055F)/1.055F,2.4F);
    };
    const auto put=[&](float* destination,const render::Rgba8& color) {
        destination[0]=channel(color.r);destination[1]=channel(color.g);destination[2]=channel(color.b);
        destination[3]=color.a/255.0F;
    };
    put(vertex.color,palette[even]);put(vertex.odd_color,palette[odd]);
    vertex.dither_scale=material.colour.dither?scale:0;return true;
}
}
