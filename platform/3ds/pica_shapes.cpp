#include "starfox/platform/nintendo_3ds/pica_shapes.hpp"
#include "starfox/platform/nintendo_3ds/pica_source_spans.hpp"

namespace starfox::platform::nintendo_3ds {
namespace {
using Camera=std::array<double,3>;
Point3 camera_point(Camera source) {
    Point3 result{float(source[0]),float(-source[1]),float(source[2])};
    for(unsigned i=0;i<3;++i) if(!std::isfinite(source[i]) || !std::isfinite(result[i]))
        throw std::invalid_argument("Invalid 3DS source camera vertex");
    return result;
}
std::array<float,4> rgba(render::Rgba8 colour) {
    return {colour.r/255.F,colour.g/255.F,colour.b/255.F,colour.a/255.F};
}
// Source two-point ink is a one-pixel-wide camera-facing ribbon, not a missing
// triangle. Width remains one source pixel as depth varies along the line.
std::vector<Camera> line_quad(Camera a,Camera b,double focal) {
    if(a[2]<1 && b[2]<1) return {};
    if(a[2]<1 || b[2]<1) {
        auto& outside=a[2]<1?a:b;const auto inside=a[2]<1?b:a;
        const auto t=(1-outside[2])/(inside[2]-outside[2]);
        for(unsigned axis=0;axis<3;++axis) outside[axis]+=t*(inside[axis]-outside[axis]);
    }
    const auto dx=focal*(b[0]/b[2]-a[0]/a[2]),dy=focal*(b[1]/b[2]-a[1]/a[2]);
    const auto length=std::hypot(dx,dy);
    if(length<=1.e-12) {
        const auto half=a[2]/focal*.5;
        return {{a[0]-half,a[1]-half,a[2]},{a[0]+half,a[1]-half,a[2]},
            {a[0]+half,a[1]+half,a[2]},{a[0]-half,a[1]+half,a[2]}};
    }
    const auto nx=length>1.e-12?-dy/length:.0,ny=length>1.e-12?dx/length:1.;
    const auto corner=[&](Camera p,double sign) {
        const auto half=std::max(1.,p[2])/focal*.5;
        p[0]+=sign*nx*half;p[1]+=sign*ny*half;return p;
    };
    return {corner(a,-1),corner(b,-1),corner(b,1),corner(a,1)};
}
} // namespace
void PicaShapes::clear() {vertices_.clear();draws_.clear();textures_.clear();views_.clear();}
unsigned PicaShapes::texture(Texture image) {
    for(unsigned i=0;i<textures_.size();++i) {
        const auto& old=textures_[i];
        if(old.width==image.width && old.height==image.height && old.repeat==image.repeat
            && old.rgba==image.rgba) return i;
    }
    if(textures_.size()>=pica_texture_limit) throw std::length_error("3DS source texture limit exceeded");
    auto bytes=pica_texture_layout({image.rgba,image.width,image.height,image.width*4,4,image.repeat}).bytes;
    // Reserve the 512x256 padded lower LCD texture too.
    unsigned total=512*256*4;
    for(const auto& old:textures_) total+=pica_texture_layout({old.rgba,old.width,old.height,old.width*4,4,old.repeat}).bytes;
    if(bytes>pica_texture_budget-total) throw std::length_error("3DS source texture memory exceeded");
    textures_.push_back(std::move(image));return unsigned(textures_.size()-1);
}
void PicaShapes::submit(std::span<const PicaVertex> vertices,unsigned texture_index,
    const PicaMatrix& model,bool dither,std::array<std::uint8_t,4> odd,std::optional<PicaClip> clip,PicaShapeOrder order) {
    if(vertices.empty()) return;
    if(vertices.size()%3 || vertices.size()>pica_vertex_limit-vertices_.size())
        throw std::length_error("3DS source geometry limit exceeded");
    const bool depth=order==PicaShapeOrder::depth;
    const bool merge=!draws_.empty() && draws_.back().texture==texture_index
        && draws_.back().model==model && draws_.back().screen_dither==dither && draws_.back().clip==clip
        && draws_.back().depth_test==depth && draws_.back().depth_write==depth
        && (!dither || draws_.back().dither_odd==odd);
    if(!merge && draws_.size()>=pica_draw_limit) throw std::length_error("3DS source draw limit exceeded");
    const auto first=unsigned(vertices_.size()),count=unsigned(vertices.size());
    vertices_.insert(vertices_.end(),vertices.begin(),vertices.end());
    if(merge) draws_.back().count+=count;
    else {
        PicaDraw draw;draw.first=first;draw.count=count;draw.texture=texture_index;
        draw.model=model;draw.screen_dither=dither;draw.dither_odd=odd;draw.clip=clip;
        draw.depth_test=draw.depth_write=depth;draws_.push_back(draw);
    }
}
void PicaShapes::append(const render::PreparedShapePrimitives& source,
    std::span<const render::Rgba8> palette,std::array<double,2> origin,const FramePlan* span_plan,PicaShapeOrder order) {
    if(palette.empty() || palette.size()>256 || !std::isfinite(source.focal_length)
        || source.focal_length<=0 || !std::isfinite(origin[0]) || !std::isfinite(origin[1])
        || !std::isfinite(source.pose.vanish_x) || !std::isfinite(source.pose.vanish_y)
        || (order!=PicaShapeOrder::depth && order!=PicaShapeOrder::painter))
        throw std::invalid_argument("Invalid 3DS source projection/palette");
    std::optional<PicaClip> clip;
    if(source.pose.effect_clip_right>source.pose.effect_clip_left) {
        // Source clips use the same canonical bitmap coordinates as vanish_x.
        // Shift to the native LCD centre, not to either projected eye position.
        const double offset=double(top_width)*.5-origin[0];
        const auto left=std::clamp(double(source.pose.effect_clip_left)+offset,0.,double(top_width));
        const auto right=std::clamp(double(source.pose.effect_clip_right)+offset,0.,double(top_width));
        if(left>=right) return; // A completely clipped primitive is legitimately invisible.
        clip=PicaClip{int(std::ceil(left)),0,int(std::ceil(right)),int(screen_height)};
        if(clip->left>=clip->right) return;
    }
    const auto old_vertices=vertices_.size(),old_draws=draws_.size(),old_textures=textures_.size();
    const unsigned old_count=draws_.empty()?0:draws_.back().count;
    try {
        PicaMatrix model=pica_identity;
        model[0][2]=float((source.pose.vanish_x-origin[0])/source.focal_length);
        model[1][2]=float(-(source.pose.vanish_y-origin[1])/source.focal_length);
        if(!std::isfinite(model[0][2]) || !std::isfinite(model[1][2]))
            throw std::invalid_argument("Unrepresentable 3DS source vanishing point");
        const auto ink=[&](unsigned relative) {
            const auto index=std::uint8_t(source.colour_index_base+relative);
            if(index>=palette.size()) throw std::invalid_argument("Missing 3DS source palette ink");
            return palette[index];
        };
        for(const auto& primitive:source.primitives) {
            const auto kind=primitive.kind;
            if((kind==render::ShapePrimitiveKind::polygon && primitive.vertices.size()<3)
                || (kind==render::ShapePrimitiveKind::line && primitive.vertices.size()!=2)
                || (kind==render::ShapePrimitiveKind::sprite && primitive.vertices.size()!=1)
                || (kind!=render::ShapePrimitiveKind::polygon && kind!=render::ShapePrimitiveKind::line
                    && kind!=render::ShapePrimitiveKind::sprite))
                throw std::invalid_argument("Incomplete 3DS source primitive");
            const auto* art=primitive.material.texture;
            const bool sparse=kind==render::ShapePrimitiveKind::polygon && !art && (source.pose.wireframe_mode
                || source.pose.wobble_mode || source.pose.wave_mode || source.pose.cel_mode);
            if(sparse && !span_plan) throw std::invalid_argument("3DS EX spans require the immutable active eye plan");
            if(kind==render::ShapePrimitiveKind::sprite && !art)
                throw std::invalid_argument("Missing 3DS source sprite texture");
            unsigned texture_index=pica_no_texture;bool dither=false;std::array<std::uint8_t,4> odd_colour{};
            auto colour=rgba(ink(primitive.material.colour.even));
            if(art) {
                const unsigned width=unsigned(art->u_mask)+1,height=unsigned(art->v_mask)+1;
                if(art->texels.size()!=std::size_t(width)*height)
                    throw std::invalid_argument("Incomplete 3DS source texture");
                Texture image;image.width=width;image.height=height;
                image.repeat=kind==render::ShapePrimitiveKind::polygon;
                image.rgba.resize(std::size_t(width)*height*4);
                std::optional<render::Rgba8> forced;
                if(primitive.simple_sprite && source.pose.palette_override) {
                    if(*source.pose.palette_override>=palette.size())
                        throw std::invalid_argument("Missing 3DS forced sprite ink");
                    forced=palette[*source.pose.palette_override];
                }
                for(std::size_t i=0;i<art->texels.size();++i) {
                    const auto index=art->texels[i];
                    const auto c=index?(forced?*forced:ink(index)):render::Rgba8{0,0,0,0};
                    image.rgba[i*4]=c.r;image.rgba[i*4+1]=c.g;image.rgba[i*4+2]=c.b;image.rgba[i*4+3]=index?c.a:0;
                }
                texture_index=texture(std::move(image));colour={1,1,1,1};
            } else if(primitive.material.colour.dither
                && primitive.material.colour.even!=primitive.material.colour.odd) {
                Texture image;image.width=image.height=8;image.repeat=true;image.rgba.resize(8*8*4);
                const auto odd=ink(primitive.material.colour.odd);odd_colour={odd.r,odd.g,odd.b,odd.a};
                for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
                    const std::uint8_t mask=((x^y)&1)?255:0;const unsigned i=(y*8+x)*4;
                    std::fill_n(image.rgba.begin()+i,4,mask);
                }
                // One parity mask handles every source ink pair through TEV.
                // COLOR WARP must not allocate hundreds of tiny textures.
                texture_index=texture(std::move(image));dither=true;
            }
            if(sparse) {
                const auto triangles=pica_source_span_geometry(primitive.vertices,source.pose,
                    source.focal_length,origin,*span_plan,colour,unsigned(pica_vertex_limit-vertices_.size()));
                submit(triangles,texture_index,model,dither,odd_colour,clip,order);
                continue;
            }
            std::vector<PicaVertex> boundary;
            if(kind==render::ShapePrimitiveKind::polygon) {
                for(const auto& vertex:primitive.vertices) {
                    std::array<float,2> uv{};
                    if(art) uv={float(vertex.uv[0]/(unsigned(art->u_mask)+1)),float(vertex.uv[1]/(unsigned(art->v_mask)+1))};
                    for(float value:uv) if(!std::isfinite(value) || std::abs(value)>65536)
                        throw std::invalid_argument("Invalid 3DS source texture coordinate");
                    boundary.push_back({camera_point(vertex.camera),colour,uv});
                }
            } else if(kind==render::ShapePrimitiveKind::line) {
                for(const auto& vertex:primitive.vertices) static_cast<void>(camera_point(vertex.camera));
                for(auto point:line_quad(primitive.vertices[0].camera,primitive.vertices[1].camera,source.focal_length))
                    boundary.push_back({camera_point(point),colour,{}});
            } else {
                const auto centre=primitive.vertices[0].camera;
                static_cast<void>(camera_point(centre));
                const auto half=primitive.sprite_half_extent;
                if(!std::isfinite(half) || half<0) throw std::invalid_argument("Invalid 3DS source sprite extent");
                if(!half) continue; // Source zero-dimensional sprite is a documented no-op.
                double left=centre[0]-half,right=centre[0]+half,top=centre[1]-half,bottom=centre[1]+half;
                double u0=0,u1=1,v1=1;
                if(!primitive.simple_sprite) {
                    const double width=unsigned(art->u_mask)+1,height=unsigned(art->v_mask)+1;
                    if(height<width) bottom=top+2*half*height/width;
                    else v1=width/height;
                }
                const std::array<Camera,4> points{{{left,top,centre[2]},{right,top,centre[2]},
                    {right,bottom,centre[2]},{left,bottom,centre[2]}}};
                const std::array<std::array<float,2>,4> uv{{{float(u0),0},{float(u1),0},{float(u1),float(v1)},{float(u0),float(v1)}}};
                for(unsigned corner=0;corner<4;++corner) boundary.push_back({camera_point(points[corner]),colour,uv[corner]});
            }
            std::vector<PicaVertex> triangles;
            if(boundary.size()>=3) {
                triangles.reserve((boundary.size()-2)*3);
                for(unsigned corner=1;corner+1<boundary.size();++corner)
                    for(unsigned index:{0U,corner,corner+1}) triangles.push_back(boundary[index]);
            }
            submit(triangles,texture_index,model,dither,odd_colour,clip,order);
        }
        views_.clear();
    } catch(...) {
        vertices_.resize(old_vertices);draws_.resize(old_draws);textures_.resize(old_textures);
        if(!draws_.empty()) draws_.back().count=old_count;
        views_.clear();throw;
    }
}
PicaFrame PicaShapes::frame(const FramePlan& plan,Rgb clear_colour) {
    views_.clear();views_.reserve(textures_.size());
    for(const auto& image:textures_) views_.push_back({image.rgba,image.width,image.height,image.width*4,4,image.repeat});
    return {plan,vertices_,draws_,views_,clear_colour};
}
} // namespace starfox::platform::nintendo_3ds
