#include "starfox/render/volumetric_fog.hpp"
#include <iostream>
#include <stdexcept>
using namespace starfox::render;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    require(volumetric_fog_medium(0).extinction==0,"Off fog has nonzero density");
    for(unsigned quality=1;quality<=3;++quality) {
        require(volumetric_fog_medium(quality).extinction>volumetric_fog_medium(quality-1).extinction,
            "fog strength is not monotonic");
        require(volumetric_fog_medium(quality).samples>volumetric_fog_medium(quality-1).samples,
            "fog quality does not increase samples");
    }
    shadows::Scene geometry;
    geometry.add({{-2,-2,5},{2,-2,5},{0,2,5}});geometry.build();
    const std::array<std::uint8_t,3> coverage{1,1,0};
    std::vector<VolumetricPixel> geometry_guides;
    const VolumetricGround plane{{0,0,10},{0,0,-1}};
    require(build_volumetric_guides(geometry,{1,1,.5,.5},3,1,coverage,100,plane,geometry_guides),"geometry depth rejected");
    require(std::abs(geometry_guides[0].view_depth-5)<1e-12,"nearest model did not stop fog");
    require(std::abs(geometry_guides[1].view_depth-10)<1e-12,"ground depth not axial Z");
    require(!geometry_guides[2].eligible,"HUD became fog eligible");
    require(build_volumetric_guides(geometry,{1,1,.5,.5},3,1,coverage,100,std::nullopt,geometry_guides)
        &&geometry_guides[1].view_depth==0,"sky incorrectly acquired ground");
    require(!build_volumetric_guides(geometry,{1,1,.5,.5},3,1,coverage,100,VolumetricGround{},geometry_guides)
        &&geometry_guides[1].view_depth==0,"invalid plane modified guides");
    shadows::Scene empty;empty.build();
    const std::array<std::uint8_t,3> all_world{1,1,1};
    require(build_volumetric_guides(empty,{1,1,.5,1.5},1,3,all_world,100,
        VolumetricGround{{0,0,10},{0,1,-1}},geometry_guides)
        &&geometry_guides[0].view_depth==5&&geometry_guides[1].view_depth==10
        &&geometry_guides[2].view_depth==0,"tilted ground has wrong horizon/depth");
    require(build_volumetric_guides(empty,{1,1,.5,.5},3,1,all_world,100,
        VolumetricGround{{0,0,-10},{0,0,-1}},geometry_guides)
        &&std::all_of(geometry_guides.begin(),geometry_guides.end(),[](auto p){return p.view_depth==0;}),
        "ground behind camera terminated forward fog");
    const VolumetricMedium daylight;
    for(double cosine:{-1.,0.,1.}) {
        const auto haze=integrate_volumetric_fog(daylight,{},{0,0,1},daylight.maximum_distance,
            {std::sqrt(1-cosine*cosine),0,cosine},empty);
        require(haze&&haze->transmittance>.6,"default haze erases distant scenery");
        require(haze->transmittance+haze->scattering.x<=1
            &&haze->transmittance+haze->scattering.y<=1
            &&haze->transmittance+haze->scattering.z<=1,"default forward phase clips white surfaces");
    }
    VolumetricMedium medium;medium.extinction=.01;medium.anisotropy=0;
    medium.albedo={1,1,1};medium.ambient={.25,.5,.75};medium.sunlight={0,0,0};
    for(unsigned samples:{1U,16U,32U,64U,128U}) {
        medium.samples=samples;
        const auto fog=integrate_volumetric_fog(medium,{},{0,0,3},100,{0,-2,0},empty);
        require(bool(fog),"valid fog rejected");
        require(std::abs(fog->transmittance-std::exp(-1.))<1e-12,"Beer attenuation depends on steps");
        require(std::abs(fog->scattering.y-.5*(1-std::exp(-1.)))<1e-12,"analytic homogeneous scattering mismatch");
    }
    shadows::Scene wall;
    wall.add({{-100,-5,-100},{100,-5,-100},{0,-5,300}});wall.build();
    medium.samples=32;medium.ambient={};medium.sunlight={1,1,1};
    const auto lit=integrate_volumetric_fog(medium,{},{0,0,1},100,{0,-1,0},empty);
    const auto blocked=integrate_volumetric_fog(medium,{},{0,0,1},100,{0,-1,0},wall);
    require(lit&&blocked&&lit->scattering.x>.6&&blocked->scattering.x==0,"geometry does not occlude fog light");
    require(lit->transmittance==blocked->transmittance,"occluder changed medium extinction");
    shadows::Scene partial;
    partial.add({{-100,-5,-100},{100,-5,-100},{100,-5,50}});
    partial.add({{-100,-5,-100},{100,-5,50},{-100,-5,50}});partial.build();
    const auto half=integrate_volumetric_fog(medium,{},{0,0,1},100,{0,-1,0},partial);
    require(half&&std::abs(half->scattering.x-(std::exp(-.5)-std::exp(-1.)))<1e-12,
        "partial shadow not integrated through view volume");
    const auto prepared=partial.prepare_direction({0,-1,0});
    const auto accelerated=integrate_volumetric_fog(medium,{},{0,0,1},100,{0,-1,0},partial,&prepared);
    require(accelerated&&accelerated->scattering.x==half->scattering.x,"prepared light query changed fog");
    partial.clear();
    partial.add({{1000,-5,-100},{1100,-5,-100},{1000,-5,300}});
    partial.build();
    const auto rebuilt=integrate_volumetric_fog(medium,{},{0,0,1},100,{0,-1,0},partial,&prepared);
    require(rebuilt&&std::abs(rebuilt->scattering.x-lit->scattering.x)<1e-12,"stale geometry query retained volumetric shadow");
    medium.ambient={.1,.1,.1};
    const auto ambient=integrate_volumetric_fog(medium,{},{0,0,1},100,{0,-1,0},wall);
    require(ambient&&ambient->scattering.x>0,"shadow incorrectly removed ambient fog");
    medium.maximum_distance=10;
    const auto capped=integrate_volumetric_fog(medium,{},{0,0,1},1000,{0,-1,0},empty);
    require(capped&&std::abs(capped->transmittance-std::exp(-.1))<1e-12,"sky distance cap ignored");
    const auto near=integrate_volumetric_fog(medium,{},{0,0,1},0,{0,-1,0},empty);
    require(near&&near->transmittance==1&&near->scattering.x==0,"fog extends before surface");
    medium.extinction=0;
    const auto clear=integrate_volumetric_fog(medium,{},{0,0,1},100,{0,-1,0},empty);
    require(clear&&clear->transmittance==1&&clear->scattering.x==0,"zero density changes scene");
    medium.extinction=-1;
    require(!integrate_volumetric_fog(medium,{},{0,0,1},100,{0,-1,0},empty),"invalid density accepted");
    medium.extinction=.01;medium.maximum_distance=100;medium.ambient={};medium.sunlight={};
    std::vector<std::array<float,4>> pixels(3,{1,.5f,.25f,.3f}),out;
    std::vector<VolumetricPixel> guides{{25,true},{25,true},{100,false}};
    require(render_volumetric_fog(medium,{1,1,.5,.5},3,1,pixels,guides,empty,{0,-1,0},out),"valid frame fog rejected");
    require(std::abs(out[0][0]-std::exp(-.25))<1e-6,"surface depth ignored");
    require(std::abs(out[1][0]-std::exp(-.25*std::sqrt(2.)))<1e-6,"off-axis depth not converted to ray distance");
    require(out[2]==pixels[2]&&out[0][3]==pixels[0][3],"fog changed HUD or alpha");
    auto inplace=pixels;
    require(render_volumetric_fog(medium,{1,1,.5,.5},3,1,inplace,guides,empty,{0,-1,0},inplace)&&inplace==out,"in-place fog differs");
    const auto preserved=out;guides[0].view_depth=-1;
    require(!render_volumetric_fog(medium,{1,1,.5,.5},3,1,pixels,guides,empty,{0,-1,0},out)&&out==preserved,"invalid frame partially changed output");
    guides[0].view_depth=0;
    require(render_volumetric_fog(medium,{1,1,.5,.5},3,1,pixels,guides,empty,{0,-1,0},out)
        &&std::abs(out[0][0]-std::exp(-1.))<1e-6,"sky guide did not use bounded distance");
    Framebuffer frame(3,1);frame.enable_layer_tags(true);
    frame.layer_tags()[1]=std::uint8_t(PixelLayer::two_d);
    const std::vector<std::uint8_t> bytes{123,45,67,200, 240,80,30,255, 95,75,55,0};
    auto adapted=bytes;medium.extinction=0;
    require(apply_volumetric_fog(medium,{1,1,.5,.5},frame,empty,{0,-1,0},plane,adapted)
        &&adapted==bytes,"zero-density byte adapter changed color");
    medium.extinction=.1;
    require(apply_volumetric_fog(medium,{1,1,.5,.5},frame,empty,{0,-1,0},plane,adapted)
        &&adapted[0]<bytes[0]&&adapted[3]==bytes[3]
        &&std::equal(adapted.begin()+4,adapted.end(),bytes.begin()+4),"adapter changed HUD, alpha, or transparent pixels");
    const auto before_invalid=adapted;
    const std::array<std::uint8_t,3> host_mask{1,0,0};
    require(apply_volumetric_fog(medium,{1,1,.5,.5},frame,empty,{0,-1,0},plane,adapted,host_mask)
        &&adapted==before_invalid,"host overlay mask changed protected pixels");
    require(!apply_volumetric_fog(medium,{1,1,.5,.5},frame,empty,{0,-1,0},plane,adapted,
        std::span<const std::uint8_t>(host_mask).first(1))&&adapted==before_invalid,"invalid host mask mutated pixels");
    require(!apply_volumetric_fog(medium,{0,1,.5,.5},frame,empty,{0,-1,0},plane,adapted)
        &&adapted==before_invalid,"invalid adapter projection mutated pixels");
    frame.enable_layer_tags(false);
    require(!apply_volumetric_fog(medium,{1,1,.5,.5},frame,empty,{0,-1,0},plane,adapted)
        &&adapted==before_invalid,"untagged adapter modified scene");
    std::cout<<"Volumetric integration tests passed\n";
}
