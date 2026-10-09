#pragma once
#include "starfox/platform/nintendo_3ds/pica_projection.hpp"
#include <bit>

namespace starfox::platform::nintendo_3ds {
inline constexpr unsigned pica_vertex_limit=32'766,pica_draw_limit=256;
// There can be one distinct source texture per submitted draw (EX's texture
// test models exceed 32). This is a metadata bound, not a PICA sampler limit:
// only the current draw's texture is bound. The 4 MiB resident-byte limit still
// includes all padded textures and the lower LCD, independently of this count.
inline constexpr unsigned pica_texture_limit=pica_draw_limit;
inline constexpr unsigned pica_texture_budget=4*1024*1024;
inline constexpr unsigned pica_no_texture=~0U;
using PicaMatrix=PicaProjection::Rows;
inline constexpr PicaMatrix pica_identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
struct PicaVertex {
    Point3 position; // Model-local; GPU matrix produces X-right/Y-up/Z-forward.
    std::array<float,4> colour{1,1,1,1};
    std::array<float,2> uv{}; // Logical texture, top-left origin, before padding.
    bool operator==(const PicaVertex&) const=default;
};
static_assert(sizeof(PicaVertex)==9*sizeof(float));
enum class PicaSpace {world,screen,scenery}; // Scenery is at infinity, not HUD depth.
struct PicaClip {
    int left{},top{},right{int(top_width)},bottom{int(screen_height)}; // LCD pixels; right/bottom exclusive.
    bool operator==(const PicaClip&) const=default;
};
inline std::array<unsigned,4> pica_screen_scissor(const PicaClip& clip) {
    if(clip.left<0 || clip.top<0 || clip.right>int(top_width) || clip.bottom>int(screen_height)
        || clip.left>=clip.right || clip.top>=clip.bottom)
        throw std::invalid_argument("Invalid 3DS LCD effect clip");
    // The upper render target is 240x400, clockwise-rotated by our projection.
    // Citro3D takes exclusive bounds and encodes right-1/bottom-1 itself.
    return {screen_height-unsigned(clip.bottom),top_width-unsigned(clip.right),
        screen_height-unsigned(clip.top),top_width-unsigned(clip.left)};
}
// SNES CGADSUB layer bits: BG1..4, OBJ, backdrop. Zero protects host UI
// and black window masks. A native Super FX model belongs to BG1, not OBJ.
inline constexpr bool pica_source_layer(unsigned layer) noexcept {
    return layer==0 || (layer<=32 && std::has_single_bit(layer));
}
struct PicaColourOp {
    bool subtract{},half{};
    std::uint8_t layers{};
    bool operator==(const PicaColourOp&) const=default;
};
struct PicaDraw {
    unsigned first{},count{},texture{pica_no_texture};
    PicaMatrix model{pica_identity};
    PicaSpace space{PicaSpace::world};
    bool depth_test{true},depth_write{true},alpha_blend{};
    // Two source inks remain distinct at fixed LCD pixel parity. The shader
    // emits homogeneous screen UV/Q, sampled as a projection texture; ordinary
    // model UVs must not stretch the checkerboard along perspective geometry.
    bool screen_dither{};
    std::array<std::uint8_t,4> dither_odd{}; // TEV constant; one shared parity mask.
    std::optional<PicaClip> clip{}; // Source effect window, identical for both eye submissions.
    std::uint8_t source_layer{1};
    std::optional<PicaColourOp> colour_op{}; // Screen fixed-colour operation; preserves layer/depth ownership.
    // Project cartridge-authored BG artwork onto finite terrain. Homogeneous
    // UV/Q retains its source pixel registration instead of perspective-
    // stretching the native colour bands across a large ground triangle.
    bool projected_uv{};
};
inline std::array<float,4> pica_uv_mode(bool screen_dither,bool projected_uv) {
    if(screen_dither && projected_uv) throw std::invalid_argument("Conflicting 3DS texture projectors");
    return {screen_dither || projected_uv?0.F:1.F,screen_dither?1.F:0.F,projected_uv?1.F:0.F,0};
}
struct PicaImage {
    std::span<const std::uint8_t> pixels;
    unsigned width{},height{},pitch{},channels{4}; // RGB24 or RGBA8, row-major.
    bool repeat{}; // Source wrapping artwork must have power-of-two dimensions.
    // Optional one-byte provenance for an opaque/transparent PPU painter
    // group. Resident GPU_A8 storage, NOT six duplicated RGBA layer textures.
    std::span<const std::uint8_t> source_layers{};
    unsigned layer_pitch{};
};
struct PicaTextureLayout {
    unsigned width{},height{},bytes{};
    std::array<float,2> uv_scale{};
};
inline PicaTextureLayout pica_texture_layout(PicaImage image) {
    if(!image.width || image.width>1024 || !image.height || image.height>1024
        || (image.channels!=3 && image.channels!=4) || image.pitch<image.width*image.channels
        || image.pitch>16384 || !image.pixels.data()
        || image.pixels.size()<std::size_t(image.pitch)*(image.height-1)+image.width*image.channels
        || (!image.source_layers.empty() && (image.channels!=4 || image.layer_pitch<image.width
            || image.layer_pitch>16384 || !image.source_layers.data()
            || image.source_layers.size()<std::size_t(image.layer_pitch)*(image.height-1)+image.width))
        || (image.source_layers.empty() && image.layer_pitch!=0)
        || (image.repeat && (!std::has_single_bit(image.width) || !std::has_single_bit(image.height))))
        throw std::invalid_argument("Invalid 3DS GPU texture");
    const auto w=std::max(8U,std::bit_ceil(image.width)),h=std::max(8U,std::bit_ceil(image.height));
    return {w,h,w*h*4,{float(image.width)/w,float(image.height)/h}};
}
inline unsigned pica_resident_texture_bytes(PicaImage image) {
    const auto layout=pica_texture_layout(image);
    return layout.bytes+(image.source_layers.empty()?0:layout.width*layout.height);
}
inline unsigned pica_texel_offset(unsigned x,unsigned y,unsigned width) noexcept {
    // PICA RGBA8 uses 8x8 Morton tiles, not a linear RGBA framebuffer.
    unsigned morton{};
    for(unsigned bit=0;bit<3;++bit) {
        morton|=((x>>bit)&1U)<<(bit*2);
        morton|=((y>>bit)&1U)<<(bit*2+1);
    }
    return (((y/8)*(width/8)+x/8)*64+morton)*4;
}
inline void pack_pica_texture(PicaImage source,std::span<std::uint8_t> destination) {
    const auto layout=pica_texture_layout(source);
    if(destination.size()!=layout.bytes || !destination.data())
        throw std::invalid_argument("Incomplete 3DS GPU texture allocation");
    const auto src=reinterpret_cast<std::uintptr_t>(source.pixels.data()),dst=reinterpret_cast<std::uintptr_t>(destination.data());
    if((dst>=src && dst-src<source.pixels.size()) || (src>=dst && src-dst<destination.size()))
        throw std::invalid_argument("3DS texture upload requires independent storage");
    for(unsigned y=0;y<layout.height;++y) for(unsigned x=0;x<layout.width;++x) {
        const auto sx=source.repeat?x%source.width:std::min(x,source.width-1);
        const auto sy=source.repeat?y%source.height:std::min(y,source.height-1);
        const auto from=std::size_t(sy)*source.pitch+sx*source.channels;
        // The shader converts top-left V to 1-V, and PICA's texture sampler
        // then addresses rows from bottom to top. Store source rows directly;
        // reversing them here as well flips uploaded artwork on both LCDs.
        const auto to=pica_texel_offset(x,y,layout.width);
        destination[to]=source.channels==4?source.pixels[from+3]:255; // ABGR bytes.
        destination[to+1]=source.pixels[from+2];destination[to+2]=source.pixels[from+1];destination[to+3]=source.pixels[from];
    }
}
// Validate provenance on changed uploads only. Frame/eye validation is O(draws),
// not a second full image walk on every presentation. Bitset returns exactly
// the populated one-hot classes, so mixed groups need no empty layer draws.
inline unsigned validate_pica_layers(PicaImage source) {
    static_cast<void>(pica_texture_layout(source));
    if(source.source_layers.empty()) throw std::invalid_argument("Missing 3DS source layer bytes");
    unsigned classes=0;
    for(unsigned y=0;y<source.height;++y) for(unsigned x=0;x<source.width;++x) {
        const unsigned layer=source.source_layers[std::size_t(y)*source.layer_pitch+x];
        const unsigned alpha=source.pixels[std::size_t(y)*source.pitch+x*4+3];
        if(!pica_source_layer(layer) || (layer==0?alpha!=0:alpha!=255))
            throw std::invalid_argument("Invalid/ambiguous 3DS PPU source ownership");
        classes|=layer;
    }
    return classes;
}
inline unsigned pack_pica_layers(PicaImage source,std::span<std::uint8_t> destination) {
    const auto layout=pica_texture_layout(source);
    if(source.source_layers.empty() || destination.size()!=layout.width*layout.height || !destination.data())
        throw std::invalid_argument("Incomplete 3DS source layer allocation");
    const auto independent=[&](std::span<const std::uint8_t> from) {
        const auto src=reinterpret_cast<std::uintptr_t>(from.data()),dst=reinterpret_cast<std::uintptr_t>(destination.data());
        return !((dst>=src && dst-src<from.size()) || (src>=dst && src-dst<destination.size()));
    };
    if(!independent(source.source_layers) || !independent(source.pixels))
        throw std::invalid_argument("3DS source layer upload requires independent storage");
    const auto classes=validate_pica_layers(source);
    for(unsigned y=0;y<layout.height;++y) for(unsigned x=0;x<layout.width;++x) {
        const unsigned sx=source.repeat?x%source.width:std::min(x,source.width-1);
        const unsigned sy=source.repeat?y%source.height:std::min(y,source.height-1);
        destination[pica_texel_offset(x,y,layout.width)/4]=source.source_layers[std::size_t(sy)*source.layer_pitch+sx];
    }
    return classes;
}
inline PicaMatrix pica_screen_matrix(unsigned width,unsigned height=screen_height) {
    if((width!=top_width && width!=bottom_width) || height!=screen_height)
        throw std::invalid_argument("Invalid 3DS screen projection");
    // Top-left pixel coordinates; clockwise LCD rotation and [-w,0] depth.
    return {{{0,-2.F/height,0,1},{-2.F/width,0,0,1},{0,0,0,-.5F},{0,0,0,1}}};
}
inline PicaMatrix pica_multiply(const PicaMatrix& a,const PicaMatrix& b) {
    PicaMatrix result{};
    for(unsigned row=0;row<4;++row) for(unsigned col=0;col<4;++col) {
        double value=0;
        for(unsigned k=0;k<4;++k) value+=double(a[row][k])*b[k][col];
        if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max())
            throw std::invalid_argument("Unrepresentable 3DS GPU matrix");
        result[row][col]=float(value);
    }
    return result;
}
inline PicaMatrix pica_draw_matrix(const FramePlan& plan,unsigned eye,const PicaDraw& draw) {
    if(eye>=plan.eye_count) throw std::invalid_argument("Inactive 3DS GPU draw eye");
    if(draw.space==PicaSpace::world) return pica_multiply(PicaProjection(plan,eye).rows(),draw.model);
    auto model=draw.model;
    if(draw.space==PicaSpace::scenery) model[0][3]+=background_offset(plan,eye);
    else if(draw.space!=PicaSpace::screen) throw std::invalid_argument("Unknown 3DS GPU coordinate space");
    return pica_multiply(pica_screen_matrix(top_width),model);
}
struct PicaFrame {
    FramePlan plan;
    std::span<const PicaVertex> vertices; // One immutable geometry source for both eyes.
    std::span<const PicaDraw> draws; // Authored painter/pass order, never sorted by texture.
    std::span<const PicaImage> textures;
    Rgb clear{8,15,28};
};
// Complete validation precedes any frame recording/texture replacement. This
// is not a primitive converter: unresolved source faces must not be omitted.
inline void validate_pica_group(const PicaFrame& frame,unsigned reserved_texture_bytes) {
    if(reserved_texture_bytes>pica_texture_budget
        || frame.vertices.size()>pica_vertex_limit || frame.draws.size()>pica_draw_limit
        || frame.textures.size()>pica_texture_limit)
        throw std::invalid_argument("Invalid 3DS GPU frame dimensions/budget");
    std::array<PicaProjection::Rows,2> projections{};
    for(unsigned eye=0;eye<frame.plan.eye_count;++eye) {
        if(eye>=2) throw std::invalid_argument("Invalid 3DS GPU eye count");
        projections[eye]=PicaProjection(frame.plan,eye).rows();
    }
    if(frame.plan.eye_count!=(frame.plan.stereo?2U:1U))
        throw std::invalid_argument("Incomplete 3DS eye plan");
    unsigned bytes=reserved_texture_bytes;
    for(auto texture:frame.textures) {
        const auto size=pica_resident_texture_bytes(texture);
        if(size>pica_texture_budget-bytes) throw std::invalid_argument("3DS GPU texture budget exceeded: "
            +std::to_string(bytes)+" + "+std::to_string(size)+" > "+std::to_string(pica_texture_budget));
        bytes+=size;
    }
    unsigned cursor=0;
    for(const auto& draw:frame.draws) {
        if(draw.first!=cursor || !draw.count || draw.count%3
            || draw.count>frame.vertices.size()-cursor || (draw.texture!=pica_no_texture && draw.texture>=frame.textures.size())
            || (draw.space!=PicaSpace::world && draw.space!=PicaSpace::screen && draw.space!=PicaSpace::scenery)
            || draw.model[3]!=std::array<float,4>{0,0,0,1}
            || (draw.space!=PicaSpace::world && (draw.depth_test || draw.depth_write))
            || (draw.depth_write && !draw.depth_test))
            throw std::invalid_argument("Invalid/omitted 3DS GPU draw range");
        if(!pica_source_layer(draw.source_layer)) throw std::invalid_argument("Invalid 3DS source draw layer");
        if(draw.texture!=pica_no_texture && !frame.textures[draw.texture].source_layers.empty()
            && (draw.space==PicaSpace::world || draw.screen_dither || draw.colour_op || draw.projected_uv))
            throw std::invalid_argument("3DS per-pixel source layers require opaque PPU artwork");
        if(draw.colour_op && (draw.space!=PicaSpace::screen || draw.texture!=pica_no_texture
            || draw.source_layer!=0 || draw.screen_dither || draw.alpha_blend || draw.model!=pica_identity
            || !draw.colour_op->layers || draw.colour_op->layers>63))
            throw std::invalid_argument("Invalid 3DS screen colour operation");
        if(draw.screen_dither && (draw.texture==pica_no_texture
            || frame.textures[draw.texture].width!=8 || frame.textures[draw.texture].height!=8
            || !frame.textures[draw.texture].repeat))
            throw std::invalid_argument("Invalid 3DS source dither texture");
        if(draw.projected_uv && (draw.space!=PicaSpace::world || draw.texture==pica_no_texture
            || draw.screen_dither || draw.colour_op || frame.textures[draw.texture].repeat))
            throw std::invalid_argument("Invalid 3DS source terrain projection");
        if(draw.clip) static_cast<void>(pica_screen_scissor(*draw.clip));
        for(const auto& row:draw.model) for(float value:row) if(!std::isfinite(value))
            throw std::invalid_argument("Non-finite 3DS model matrix");
        for(unsigned eye=0;eye<frame.plan.eye_count;++eye)
            static_cast<void>(pica_draw_matrix(frame.plan,eye,draw));
        for(unsigned i=cursor;i<cursor+draw.count;++i) {
            const auto& vertex=frame.vertices[i];
            for(float value:vertex.position) if(!std::isfinite(value)) throw std::invalid_argument("Non-finite 3DS vertex");
            for(float value:vertex.colour) if(!std::isfinite(value) || value<0 || value>1) throw std::invalid_argument("Invalid 3DS vertex colour");
            for(float value:vertex.uv) if(!std::isfinite(value) || std::abs(value)>65536
                || (draw.texture!=pica_no_texture && !frame.textures[draw.texture].repeat && (value<0 || value>1)))
                throw std::invalid_argument("Invalid 3DS texture coordinate");
            if(draw.colour_op && (vertex.colour!=frame.vertices[cursor].colour || vertex.colour[3]!=1))
                throw std::invalid_argument("3DS fixed-colour operation cannot interpolate different colours");
            if(draw.texture!=pica_no_texture && !frame.textures[draw.texture].source_layers.empty() && vertex.colour[3]!=1)
                throw std::invalid_argument("3DS source layer artwork cannot interpolate opacity");
        }
        cursor+=draw.count;
    }
    if(cursor!=frame.vertices.size()) throw std::invalid_argument("3DS GPU frame has unsubmitted vertices");
}
inline void validate_pica_frame(const PicaFrame& frame,ImageView dashboard) {
    if(!valid_image(dashboard,bottom_width,screen_height) || !dashboard.pixels.data())
        throw std::invalid_argument("Invalid 3DS lower LCD image");
    validate_pica_group(frame,pica_texture_layout(
        {dashboard.pixels,dashboard.width,dashboard.height,dashboard.pitch,3}).bytes);
}
} // namespace starfox::platform::nintendo_3ds
