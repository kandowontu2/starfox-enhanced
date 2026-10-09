#pragma once
#include "starfox/render/global_enhancements.hpp"
#include "starfox/render/software_renderer.hpp"

namespace starfox::render {
struct DepthEnhancements {
    std::uint32_t modes{}; // AO and depth of field, two bits each.
    std::array<float,4> camera{0,0,256,500}; // cx,cy,focal,focus distance.
    float focal_y{}; // Zero preserves the desktop's square-pixel focal length.
    bool active() const {return (modes&15)!=0;}
};
template<class Surface> inline float depth_ambient(Surface surface,
    float x,float y,float scale,const DepthEnhancements& settings) {
    using std::sqrt;
    const unsigned modes=settings.modes;
#define D_MIN std::min
#define D_MAX std::max
#define D_SURFACE(a,b,c) surface(a,b,c)
#define D_CAMERA(i) settings.camera[i]
#define D_FOCAL_Y (settings.focal_y>0?settings.focal_y:settings.camera[2])
#define D_LOOP
#include "ambient_occlusion.inc"
#undef D_LOOP
#undef D_FOCAL_Y
#undef D_CAMERA
#undef D_SURFACE
#undef D_MIN
#undef D_MAX
}
template<class Colour,class Surface> inline float depth_channel(Colour colour,Surface surface,
    float x,float y,float scale,const DepthEnhancements& settings,float ambient=-1.f) {
    using std::sqrt;using std::abs;
    const unsigned modes=settings.modes;
    if(ambient<0) ambient=depth_ambient(surface,x,y,scale,settings);
#define D_MIN std::min
#define D_MAX std::max
#define D_COLOUR(a,b) colour(a,b)
#define D_SURFACE(a,b,c) surface(a,b,c)
#define D_CAMERA(i) settings.camera[i]
#include "depth_enhancements.inc"
#undef D_CAMERA
#undef D_SURFACE
#undef D_COLOUR
#undef D_MIN
#undef D_MAX
}
inline void apply_depth_enhancements(const DepthEnhancements& settings,const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba,std::vector<std::uint8_t>& scratch,
    const SurfaceBuffer* surfaces,int ox,int oy) {
    if(!settings.active() || !surfaces || surfaces->empty() || !frame.layer_tags_enabled()
        || rgba.size()!=frame.pixels().size()*4) return;
    scratch=rgba;const auto w=frame.stored_width(),h=frame.stored_height();
    const auto surface=[&](float xx,float yy,unsigned field) {
        const int x=int(xx),y=int(yy);
        if(x<0 || y<0 || x>=int(w) || y>=int(h) || x<ox || y<oy
            || x-ox>=int(surfaces->width()) || y-oy>=int(surfaces->height())) return 0.f;
        const auto i=std::size_t(y)*w+x;const auto& s=surfaces->get(x-ox,y-oy);
        if(frame.layer_tags()[i]==1 || !s.valid || s.palette_index!=frame.pixels()[i]) return 0.f;
        return field==3?s.depth:field==0?s.normal_x:field==1?s.normal_y:s.normal_z;
    };
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const auto i=std::size_t(y)*w+x;if(frame.layer_tags()[i]==1 || surface(float(x),float(y),3)<=0) continue;
        const float ambient=depth_ambient(surface,float(x),float(y),float(frame.draw_scale()),settings);
        for(unsigned c=0;c<3;++c) {
            const auto colour=[&](float sx,float sy) {
                sx=std::clamp(sx,0.f,float(w-1));sy=std::clamp(sy,0.f,float(h-1));
                const unsigned ax=unsigned(sx),ay=unsigned(sy);const float tx=sx-ax,ty=sy-ay;float value=0;
                for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                    auto n=std::size_t(std::min(ay+dy,h-1))*w+std::min(ax+dx,w-1);
                    if(frame.layer_tags()[n]==1) n=i;
                    value+=scratch[n*4+c]*(dx?tx:1-tx)*(dy?ty:1-ty);
                }
                return value/255.f;
            };
            rgba[i*4+c]=std::uint8_t(depth_channel(colour,surface,float(x),float(y),float(frame.draw_scale()),settings,ambient)*255.f+.5f);
        }
    }
}
}
