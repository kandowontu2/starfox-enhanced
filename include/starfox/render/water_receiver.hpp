#pragma once
#include "starfox/render/water_caustics.hpp"
#include "starfox/render/water_transmission.hpp"
#include "starfox/render/palette.hpp"

namespace starfox::render {
// Linear-light receiver transport for the CPU presentation path. The caller
// supplies the refracted camera-space ray; no screen-colour sampling is used.
inline std::array<float,3> water_receiver(const shadows::Scene& scene,
    const WaterCaustics& settings,shadows::Vec3 origin,shadows::Vec3 direction,
    std::span<const Rgba8> palette,std::array<float,3> body,float footprint,
    const WaterCausticVisibility* visibility=nullptr) {
    using namespace shadows;
    const auto& m=settings.view_to_world;
    const auto world=[&](Vec3 v,bool point) {
        std::array<float,3> result{};
        for(unsigned c=0;c<3;++c) result[c]=float(m[c][0]*v.x+m[c][1]*v.y+m[c][2]*v.z)+(point?m[c][3]:0.f);
        return result;
    };
    const auto start=world(origin,true),travel_direction=world(direction,false);
    if(palette.size()<256 || !std::isfinite(travel_direction[1]) || travel_direction[1]<=0) return body;
    double distance=(settings.water_y+640-start[1])/travel_direction[1];
    if(!std::isfinite(distance) || distance<=.05 || distance>=65536) return body;
    std::array<float,3> receiver{.28f,.24f,.16f};
    float up=1;
    if(const auto hit=scene.nearest_hit(origin,direction,.05,distance)) {
        distance=hit->distance;
        const auto& triangle=scene.triangles()[hit->triangle];
        if(triangle.reflection_valid) {
            const auto a=palette[triangle.reflection_even],b=palette[triangle.reflection_odd];
            receiver={float(unsigned(a.r)+b.r)/510,float(unsigned(a.g)+b.g)/510,float(unsigned(a.b)+b.b)/510};
            for(auto& channel:receiver) channel*=channel;
        }
        auto normal=cross(triangle.b-triangle.a,triangle.c-triangle.a);
        const double length=std::sqrt(dot(normal,normal));
        if(length<=1e-12) return body;
        normal=normal*(1/length);
        if(dot(normal,direction)>0) normal=normal*(-1);
        up=std::clamp(-world(normal,false)[1],0.f,1.f);
    }
    const auto point=world(origin+direction*distance,true);
    const float depth=point[1]-settings.water_y;
    if(settings.quality && settings.quality<=3 && depth>0 && up>0) {
        const auto focus=water_caustic_sample(point[0],point[2],settings.seconds,depth,footprint);
        const std::array<float,3> entry{focus.entry_x,settings.water_y,focus.entry_z};
        if((visibility?(*visibility)(point,entry):WaterCausticVisibility(scene,settings)(point,entry))>0) {
            const float gain=std::clamp(1+(focus.irradiance-std::exp(-depth/1600))*up*float(settings.quality)/3,.25f,3.f);
            for(auto& channel:receiver) channel*=gain;
        }
    }
    constexpr std::array<float,3> absorption{.0025f,.0008f,.00035f};
    for(unsigned c=0;c<3;++c) receiver[c]=water_optics::water_transmitted_channel(receiver[c],body[c],float(distance),absorption[c]);
    return receiver;
}

inline std::optional<std::array<float,3>> water_surface_sample(const shadows::Scene& scene,
    const WaterCaustics& settings,std::span<const Rgba8> palette,float px,float py,
    float sample_width,const WaterCausticVisibility& visibility) {
    using caustics_detail::water_light_band;
    const auto& m=settings.view_to_world;const float focal=settings.projection[2];
    if(!std::isfinite(focal) || focal<=0) return {};
    shadows::Vec3 direction{(px+.5f-settings.projection[0])/focal,(py+.5f-settings.projection[1])/focal,1};
    direction=direction*(1/std::sqrt(shadows::dot(direction,direction)));
    const double down=m[1][0]*direction.x+m[1][1]*direction.y+m[1][2]*direction.z;
    const double distance=down>1e-6?(settings.water_y-m[1][3])/down:-1;
    if(distance<=0 || distance>=65536) return {};
    const auto hit=direction*distance;
    const float wx=float(m[0][0]*hit.x+m[0][1]*hit.y+m[0][2]*hit.z+m[0][3]);
    const float wz=float(m[2][0]*hit.x+m[2][1]*hit.y+m[2][2]*hit.z+m[2][3]);
    const float footprint=sample_width*float(distance)/focal/std::max(float(down),.04f),t=settings.seconds;
    const float nx=.055f*std::cos(wx*.018f+wz*.011f-t*.8f)*water_light_band(footprint,.022f)
        +.025f*std::cos(wx*.047f-wz*.025f+t*1.2f)*water_light_band(footprint,.054f);
    const float nz=.045f*std::cos(wz*.022f-wx*.009f-t*.65f)*water_light_band(footprint,.024f)
        -.020f*std::cos(wx*.047f-wz*.025f+t*1.2f)*water_light_band(footprint,.054f);
    shadows::Vec3 normal{m[0][0]*nx-m[1][0]+m[2][0]*nz,
        m[0][1]*nx-m[1][1]+m[2][1]*nz,m[0][2]*nx-m[1][2]+m[2][2]*nz};
    normal=normal*(1/std::sqrt(shadows::dot(normal,normal)));
    double cosine=shadows::dot(normal,direction);
    if(cosine>0) {normal=normal*(-1);cosine=-cosine;}
    const auto refracted=direction*.75-normal*(.75*cosine+std::sqrt(1-.75*.75*(1-cosine*cosine)));
    return water_receiver(scene,settings,hit-normal*.05,refracted,palette,{.02f,.08f,.14f},footprint,&visibility);
}
}
