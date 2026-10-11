#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include <memory>

namespace starfox::platform::nintendo_3ds {
namespace detail {
struct PicaCompositionLifetime {
    std::uint64_t generation{};
    bool alive{true};
};
}
class PicaComposite;
// This handle can only be minted after the composer's full validation and
// owned geometry/metadata copy. Texture pixels remain borrowed, as before;
// their changed-upload ownership/opacity checks remain the presenter's job.
// A later successful prepare or owner destruction invalidates this handle
// before any borrowed geometry is read. Use only on the owner/render thread.
class PicaValidatedFrame {
public:
    [[nodiscard]] const PicaFrame& checked_frame(ImageView dashboard) const {
        if(!lifetime_ || !lifetime_->alive || lifetime_->generation!=generation_)
            throw std::invalid_argument("Expired 3DS validated composition");
        if(!valid_image(dashboard,bottom_width,screen_height) || !dashboard.pixels.data())
            throw std::invalid_argument("Invalid 3DS validated lower LCD image");
        if(pica_texture_layout({dashboard.pixels,dashboard.width,dashboard.height,dashboard.pitch,3}).bytes
            !=reserved_texture_bytes_)
            throw std::invalid_argument("3DS validated dashboard allocation changed");
        return frame_;
    }
private:
    friend class PicaComposite;
    PicaValidatedFrame(std::shared_ptr<const detail::PicaCompositionLifetime> lifetime,
        std::uint64_t generation,PicaFrame frame,unsigned reserved_texture_bytes)
        :lifetime_(std::move(lifetime)),generation_(generation),frame_(frame),
         reserved_texture_bytes_(reserved_texture_bytes) {}
    std::shared_ptr<const detail::PicaCompositionLifetime> lifetime_;
    std::uint64_t generation_{};
    PicaFrame frame_;
    unsigned reserved_texture_bytes_{};
};
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
    PicaComposite()=default;
    ~PicaComposite() {lifetime_->alive=false;}
    // Queued spans and their generation belong to this one stable owner.
    PicaComposite(const PicaComposite&)=delete;
    PicaComposite& operator=(const PicaComposite&)=delete;
    PicaComposite(PicaComposite&&)=delete;
    PicaComposite& operator=(PicaComposite&&)=delete;
    PicaFrame prepare(const FramePlan& plan,std::span<const PicaFrame> groups,
        ImageView dashboard,Rgb clear={8,15,28}) {
        if(!valid_image(dashboard,bottom_width,screen_height) || !dashboard.pixels.data())
            throw std::invalid_argument("Invalid 3DS composition dashboard");
        return prepare_groups(plan,groups,pica_texture_layout(
            {dashboard.pixels,dashboard.width,dashboard.height,dashboard.pitch,3}).bytes,clear);
    }
    PicaValidatedFrame prepare_validated(const FramePlan& plan,std::span<const PicaFrame> groups,
        ImageView dashboard,Rgb clear={8,15,28}) {
        if(!valid_image(dashboard,bottom_width,screen_height) || !dashboard.pixels.data())
            throw std::invalid_argument("Invalid 3DS composition dashboard");
        const auto reserved=pica_texture_layout(
            {dashboard.pixels,dashboard.width,dashboard.height,dashboard.pitch,3}).bytes;
        const auto frame=prepare_groups(plan,groups,reserved,clear);
        return {lifetime_,lifetime_->generation,frame,reserved};
    }
    // Artwork adapters run independently of the dashboard owner. Reserve its
    // actual fixed padded LCD allocation without manufacturing a fake image.
    PicaFrame prepare_layers(const FramePlan& plan,std::span<const PicaFrame> groups,Rgb clear={8,15,28}) {
        return prepare_groups(plan,groups,512U*256U*4U,clear);
    }
private:
    PicaFrame prepare_groups(const FramePlan& plan,std::span<const PicaFrame> groups,
        unsigned reserved_texture_bytes,Rgb clear) {
        if(lifetime_->generation==std::numeric_limits<std::uint64_t>::max())
            throw std::length_error("3DS composition generation exhausted");
        // Validate the plan even when there are no painter groups. Each input
        // is then checked exactly once, including all vertices, against the
        // cumulative resident budget. Concatenation only rebases already
        // checked ranges/texture indices; a second whole-vertex walk of that
        // identical output added substantial CPU work on the handheld.
        validate_pica_group({plan,{},{},{}},reserved_texture_bytes);
        unsigned resident_bytes=reserved_texture_bytes;
        std::size_t vertex_count=0,draw_count=0,texture_count=0;
        for(const auto& group:groups) {
            if(!same_pica_plan(plan,group.plan)) throw std::invalid_argument("3DS painter groups belong to different eye plans");
            validate_pica_group(group,resident_bytes);
            for(const auto& image:group.textures) resident_bytes+=pica_resident_texture_bytes(image);
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
        vertices_.swap(vertices);draws_.swap(draws);textures_.swap(textures);
        ++lifetime_->generation;
        return {plan,vertices_,draws_,textures_,clear};
    }
    std::shared_ptr<detail::PicaCompositionLifetime> lifetime_=
        std::make_shared<detail::PicaCompositionLifetime>();
    std::vector<PicaVertex> vertices_;
    std::vector<PicaDraw> draws_;
    std::vector<PicaImage> textures_;
    std::vector<PicaVertex> next_vertices_;
    std::vector<PicaDraw> next_draws_;
    std::vector<PicaImage> next_textures_;
};
} // namespace starfox::platform::nintendo_3ds
