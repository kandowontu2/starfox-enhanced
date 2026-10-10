#pragma once
#include "starfox/assets/shape.hpp"
#include <array>
#include <span>
#include <string>
#include <vector>
namespace starfox::vr {
struct ShapeMeshVertex {std::array<float,3> position;};
struct ShapeMeshFace {
    std::size_t source_face{};
    std::vector<uint32_t> indices;
    assets::Vec3i normal;
    uint8_t colour_id{};
    int16_t visibility_index{};
    bool sprite{};
    uint8_t sprite_visibility_parameter{},sprite_size{};
};
// Object-local geometry only: no CPU projection, visibility rejection or
// camera-dependent caching. Face metadata remains available to material,
// sprite and visibility stages instead of baking an approximate replacement.
struct ShapeMesh {
    std::vector<ShapeMeshVertex> vertices;
    std::vector<ShapeMeshFace> faces;
};
bool decode_shape_mesh(const assets::Shape&,uint32_t animation_frame,float object_scale,
    ShapeMesh& output,std::string& error);
struct ShapeFaceInstance {std::size_t face;int group_visibility{-1};};
// Builds camera-independent membership, not painter ordering. Every node's
// children are visited; its own batch is gated by that node's visibility.
bool shape_face_instances(const assets::Shape&,std::vector<ShapeFaceInstance>&,std::string& error);
}
