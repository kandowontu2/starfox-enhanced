#include "starfox/render/face_material.hpp"
#include "starfox/render/software_renderer.hpp"
#include <algorithm>
namespace starfox::render {
const assets::TextureImage* texture_for_colour(const assets::Shape& shape,std::uint8_t colour,std::uint32_t frame) {
    if(colour>=shape.colour_words.size()) return nullptr;
    auto descriptor=shape.colour_words[colour];
    if(colour<shape.colour_materials.size() && !shape.colour_materials[colour].animation_frames.empty()) {
        const auto& frames=shape.colour_materials[colour].animation_frames;
        descriptor=frames[frame%frames.size()];
    }
    const auto found=std::find_if(shape.textures.begin(),shape.textures.end(),[&](const auto& value){return value.descriptor==descriptor;});
    return found==shape.textures.end()?nullptr:&*found;
}
FaceMaterial face_material(const assets::Shape& shape,const assets::Face& face,
    std::uint32_t colour_frame,std::size_t depth_band,const std::array<std::int8_t,3>& light,
    const RenderPose& pose,std::optional<std::uint16_t> descriptor_override,std::uint8_t colour_index_base) {
    if(pose.palette_override) {
        const auto relative=static_cast<std::uint8_t>(*pose.palette_override-colour_index_base);
        return {{relative,relative,false},nullptr};
    }
    if(pose.force_colour) {
        const auto even=static_cast<std::uint8_t>(pose.forced_colour&0x0fU);
        const auto odd=static_cast<std::uint8_t>(pose.forced_colour>>4U);
        return {{even,odd,even!=odd},nullptr};
    }
    if(!descriptor_override && face.colour_id>=shape.colour_words.size()) {
        const auto fallback=static_cast<std::uint8_t>(face.colour_id&0x0fU);
        return {{fallback,fallback,false},nullptr};
    }
    // value_or evaluates its fallback even when an override is present. The
    // warp path may override a face with no color-table entry, so branch here.
    auto word=descriptor_override?*descriptor_override:shape.colour_words[face.colour_id];
    if(!descriptor_override && face.colour_id<shape.colour_materials.size()) {
        const auto& material=shape.colour_materials[face.colour_id];
        if(!material.animation_frames.empty()) word=material.animation_frames[colour_frame%material.animation_frames.size()];
    }
    if((word&0xc000U)==0x4000U) {
        const auto texture=std::find_if(shape.textures.begin(),shape.textures.end(),
            [word](const auto& candidate){return candidate.descriptor==word;});
        return {{15,15,false},texture==shape.textures.end()?nullptr:&*texture};
    }
    // COLSMOOTH's low nibble is its solid material, not a black/green dither.
    if((word&0xc000U)==0xc000U) {
        const auto colour=static_cast<std::uint8_t>(word&0x0fU);
        return {{colour,colour,false},nullptr};
    }
    depth_band=std::min(depth_band,std::size_t(3));
    const auto material=static_cast<std::uint8_t>(word>>8U);
    auto byte=static_cast<std::uint8_t>(word);
    if(material<62U && shape.has_diffuse_shade_tables && material<shape.diffuse_shade_tables[depth_band].size()) {
        const auto dot=face.normal.x*light[0]+face.normal.y*light[1]+face.normal.z*light[2];
        const auto intensity=std::clamp<std::int32_t>(dot>>10,6,15);
        byte=shape.diffuse_shade_tables[depth_band][material][static_cast<std::size_t>(intensity-6)];
    } else if(material==62U && pose.has_depth_colour_tables) byte=pose.depth_colour_tables[depth_band][byte&0x1fU];
    const auto even=static_cast<std::uint8_t>(byte&0x0fU),odd=static_cast<std::uint8_t>(byte>>4U);
    return {{even,odd,even!=odd},nullptr};
}
}
