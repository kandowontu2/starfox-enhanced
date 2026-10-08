#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"

namespace starfox::platform::nintendo_3ds {
inline bool same_pica_plan(const FramePlan& a,const FramePlan& b) {
    if(a.stereo!=b.stereo || a.slider!=b.slider || a.separation!=b.separation || a.convergence!=b.convergence
        || a.focal_x!=b.focal_x || a.focal_y!=b.focal_y || a.near_plane!=b.near_plane || a.far_plane!=b.far_plane
        || a.eye_count!=b.eye_count) return false;
    for(unsigned eye=0;eye<2;++eye)
        if(a.eyes[eye].x!=b.eyes[eye].x || a.eyes[eye].projection_offset!=b.eyes[eye].projection_offset) return false;
    return true;
}
// Assemble contiguous painter groups in caller-supplied source order. Model
// depth and UI/scenery coordinate spaces are retained, never flattened/sorted.
// Texture bytes are borrowed from their original owners through GPU submission;
// this avoids copying all source artwork every presentation/eye.
class PicaComposite {
public:
    PicaFrame prepare(const FramePlan& plan,std::span<const PicaFrame> groups,
        ImageView dashboard,Rgb clear={8,15,28}) {
        if(!valid_image(dashboard,bottom_width,screen_height) || !dashboard.pixels.data())
            throw std::invalid_argument("Invalid 3DS composition dashboard");
        return prepare_groups(plan,groups,pica_texture_layout(
            {dashboard.pixels,dashboard.width,dashboard.height,dashboard.pitch,3}).bytes,clear);
    }
    // Artwork adapters run independently of the dashboard owner. Reserve its
    // actual fixed padded LCD allocation without manufacturing a fake image.
    PicaFrame prepare_layers(const FramePlan& plan,std::span<const PicaFrame> groups,Rgb clear={8,15,28}) {
        return prepare_groups(plan,groups,512U*256U*4U,clear);
    }
private:
    PicaFrame prepare_groups(const FramePlan& plan,std::span<const PicaFrame> groups,
        unsigned reserved_texture_bytes,Rgb clear) {
        std::size_t vertex_count=0,draw_count=0,texture_count=0;
        for(const auto& group:groups) {
            if(!same_pica_plan(plan,group.plan)) throw std::invalid_argument("3DS painter groups belong to different eye plans");
            validate_pica_group(group,reserved_texture_bytes);
            vertex_count+=group.vertices.size();draw_count+=group.draws.size();texture_count+=group.textures.size();
            if(vertex_count>pica_vertex_limit || draw_count>pica_draw_limit || texture_count>pica_texture_limit)
                throw std::length_error("3DS composed geometry/draw budget exceeded");
        }
        auto& vertices=next_vertices_;auto& draws=next_draws_;auto& textures=next_textures_;
        vertices.clear();draws.clear();textures.clear();
        vertices.reserve(vertex_count);draws.reserve(draw_count);textures.reserve(texture_count);
        for(const auto& group:groups) {
            const unsigned vertex_offset=vertices.size(),texture_offset=textures.size();
            vertices.insert(vertices.end(),group.vertices.begin(),group.vertices.end());
            for(auto draw:group.draws) {
                draw.first+=vertex_offset;if(draw.texture!=pica_no_texture) draw.texture+=texture_offset;
                draws.push_back(draw);
            }
            textures.insert(textures.end(),group.textures.begin(),group.textures.end());
        }
        validate_pica_group({plan,vertices,draws,textures,clear},reserved_texture_bytes);
        vertices_.swap(vertices);draws_.swap(draws);textures_.swap(textures);
        return {plan,vertices_,draws_,textures_,clear};
    }
    std::vector<PicaVertex> vertices_;
    std::vector<PicaDraw> draws_;
    std::vector<PicaImage> textures_;
    std::vector<PicaVertex> next_vertices_;
    std::vector<PicaDraw> next_draws_;
    std::vector<PicaImage> next_textures_;
};
} // namespace starfox::platform::nintendo_3ds
