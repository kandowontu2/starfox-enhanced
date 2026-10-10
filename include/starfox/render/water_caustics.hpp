#pragma once
#include <algorithm>
#include <cmath>
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/shadow_scene.hpp"
namespace starfox::render {
namespace caustics_detail {
using std::min;using std::max;using std::cos;using std::sin;using std::sqrt;using std::abs;using std::exp;
#define WC_FN inline
#include "water_caustics.inc"
#undef WC_FN
}
using caustics_detail::water_caustic_irradiance;
using caustics_detail::water_caustic_sample;

struct WaterCaustics {
    unsigned quality{};
    float seconds{},water_y{}; // World Y increases downward, as in the game.
    std::array<float,3> projection{0,0,256}; // Stored-pixel cx,cy,focal.
    // Affine camera-to-world transform. Translation is the camera position.
    std::array<std::array<float,4>,3> view_to_world{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
};

// Matches the camera-space BVH used by the software ray tracer. Test both
// portions of the light path: receiver -> refracted entry, then entry -> sun.
// A submerged blocker and an above-water overhang must both stop caustics.
class WaterCausticVisibility {
public:
    WaterCausticVisibility(const shadows::Scene& scene,const WaterCaustics& settings)
        :scene_(scene),matrix_(settings.view_to_world),
         sunlight_{-matrix_[1][0],-matrix_[1][1],-matrix_[1][2]},
         sun_query_(scene.prepare_direction(sunlight_)) {}
    float operator()(const std::array<float,3>& receiver,const std::array<float,3>& entry) const {
        const auto from=to_view(receiver),water=to_view(entry);
        const auto segment=water-from;
        const double distance=std::sqrt(shadows::dot(segment,segment));
        if(!std::isfinite(distance) || distance<=.002) return 0;
        const auto direction=segment*(1./distance);
        const double bias=std::min(.1,distance*.001);
        if(scene_.occluded(from,direction,bias,distance-bias)) return 0;
        return scene_.occluded(water,sunlight_,bias,65536.,nullptr,&sun_query_)?0.f:1.f;
    }
private:
    shadows::Vec3 to_view(const std::array<float,3>& world) const {
        const double x=double(world[0])-matrix_[0][3],y=double(world[1])-matrix_[1][3],z=double(world[2])-matrix_[2][3];
        // Camera rotation is orthonormal: inverse equals transpose.
        return {matrix_[0][0]*x+matrix_[1][0]*y+matrix_[2][0]*z,
            matrix_[0][1]*x+matrix_[1][1]*y+matrix_[2][1]*z,
            matrix_[0][2]*x+matrix_[1][2]*y+matrix_[2][2]*z};
    }
    const shadows::Scene& scene_;
    std::array<std::array<float,4>,3> matrix_;
    shadows::Vec3 sunlight_;
    shadows::Scene::DirectionQuery sun_query_;
};

// Host supplies the actual water footprint and light-path visibility. These
// are deliberately required: a plane crossing alone cannot establish that a
// surface is under water, nor that an opaque object does not block its light.
template<class WaterCoverage,class Visibility>
void apply_water_caustics(const WaterCaustics& settings,const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba,const SurfaceBuffer& surfaces,int ox,int oy,
    WaterCoverage water_covers,Visibility visible) {
    if(settings.quality==0 || settings.quality>3 || !std::isfinite(settings.seconds)
        || !std::isfinite(settings.water_y) || !std::isfinite(settings.projection[2]) || settings.projection[2]<=0
        || !frame.layer_tags_enabled() || rgba.size()!=frame.pixels().size()*4
        || surfaces.empty()) return;
    const auto& m=settings.view_to_world;
    for(unsigned y=0;y<frame.stored_height();++y) for(unsigned x=0;x<frame.stored_width();++x) {
        const auto i=std::size_t(y)*frame.stored_width()+x;
        if(frame.layer_tags()[i]==unsigned(PixelLayer::two_d) || !rgba[i*4+3]
            || int(x)<ox || int(y)<oy || int(x)-ox>=int(surfaces.width()) || int(y)-oy>=int(surfaces.height())) continue;
        const auto& s=surfaces.get(int(x)-ox,int(y)-oy);
        if(!s.valid || s.palette_index!=frame.pixels()[i] || s.depth<=0 || !std::isfinite(s.depth)) continue;
        const float focal=settings.projection[2];
        const std::array<float,3> p{(float(x)-settings.projection[0])*s.depth/focal,
            (float(y)-settings.projection[1])*s.depth/focal,s.depth};
        std::array<float,3> world{};
        for(unsigned c=0;c<3;++c) world[c]=m[c][0]*p[0]+m[c][1]*p[1]+m[c][2]*p[2]+m[c][3];
        if(!std::isfinite(world[0]) || !std::isfinite(world[1]) || !std::isfinite(world[2])) continue;
        const float depth=world[1]-settings.water_y;
        if(depth<=0) continue;
        const float up=std::clamp(-(m[1][0]*s.normal_x+m[1][1]*s.normal_y+m[1][2]*s.normal_z),0.f,1.f);
        if(!std::isfinite(up) || up<=0) continue;
        const auto light=water_caustic_sample(world[0],world[2],settings.seconds,depth,s.depth/focal);
        const std::array<float,3> entry{light.entry_x,settings.water_y,light.entry_z};
        if(!water_covers(entry)) continue;
        const float visibility=std::clamp(float(visible(world,entry)),0.f,1.f);
        if(!std::isfinite(visibility) || visibility<=0) continue;
        const float mean=std::exp(-depth/1600.f);
        const float gain=std::clamp(1.f+(light.irradiance-mean)*up*visibility*(float(settings.quality)/3.f),.25f,3.f);
        // Match the ray-water shaders: modulate linear radiance, not encoded RGB.
        for(unsigned c=0;c<3;++c)
            rgba[i*4+c]=std::uint8_t(std::clamp(std::lround(float(rgba[i*4+c])*std::sqrt(gain)),0L,255L));
    }
}
}
