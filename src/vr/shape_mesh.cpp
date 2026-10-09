#include "starfox/vr/shape_mesh.hpp"
#include <cmath>
#include <limits>
#include <utility>
namespace starfox::vr {
bool decode_shape_mesh(const assets::Shape& shape,uint32_t animation,float scale,
    ShapeMesh& output,std::string& error) {
    const auto fail=[&](const char* reason) {error=reason;return false;};
    if(!std::isfinite(scale) || scale<=0 || shape.header.shift>15) return fail("Invalid shape scale/shift");
    const auto& points=shape.frames.empty()?shape.vertices:shape.frames[animation%shape.frames.size()].vertices;
    const auto& words=shape.frames.empty()?shape.word_coordinates:shape.frames[animation%shape.frames.size()].word_coordinates;
    if(points.size()>65536 || shape.faces.size()>65536) return fail("Shape geometry exceeds batch limits");
    ShapeMesh pending;
    pending.vertices.reserve(points.size());pending.faces.reserve(shape.faces.size());
    for(std::size_t i=0;i<points.size();++i) {
        // Source word-coordinate vertices bypass both header shift and object
        // scale; applying either recreates the EX oversized-model regression.
        const double factor=i<words.size() && words[i]?1.0:std::ldexp(double(scale),shape.header.shift);
        const auto& p=points[i];
        const std::array<double,3> coordinates{p.x*factor,p.y*factor,p.z*factor};
        ShapeMeshVertex vertex{};
        for(unsigned axis=0;axis<3;++axis) {
            if(!std::isfinite(coordinates[axis]) || std::abs(coordinates[axis])>std::numeric_limits<float>::max())
                return fail("Shape vertex overflow");
            vertex.position[axis]=static_cast<float>(coordinates[axis]);
        }
        pending.vertices.push_back(vertex);
    }
    for(std::size_t i=0;i<shape.faces.size();++i) {
        const auto& source=shape.faces[i];
        ShapeMeshFace face{i,{},source.normal,source.colour_id,source.visibility_index,source.sprite,
            source.sprite_visibility_parameter,source.sprite_size};
        for(auto index:source.vertex_indices) {
            if(index>=points.size()) return fail("Shape face references a missing vertex");
            face.indices.push_back(index);
        }
        pending.faces.push_back(std::move(face));
    }
    output=std::move(pending);error.clear();return true;
}
}
