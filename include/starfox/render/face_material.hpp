#pragma once
#include "starfox/assets/shape.hpp"
#include <array>
#include <cstddef>
#include <optional>
namespace starfox::render {
struct RenderPose;
struct FaceColour {std::uint8_t even{},odd{};bool dither{};};
// Texture references remain owned by Shape; neither raster backend may
// replace a texture or a dithered material with an averaged flat color.
struct FaceMaterial {FaceColour colour{};const assets::TextureImage* texture{};};
const assets::TextureImage* texture_for_colour(const assets::Shape&,std::uint8_t,std::uint32_t);
FaceMaterial face_material(const assets::Shape&,const assets::Face&,std::uint32_t colour_frame,
    std::size_t depth_band,const std::array<std::int8_t,3>& light,const RenderPose&,
    std::optional<std::uint16_t> descriptor_override=std::nullopt,std::uint8_t colour_index_base=0);
}
