#pragma once
#include "native_indexed_cutout_fixture.hpp"
namespace native_reflection_fixture {
struct alignas(16) ShadowParameters {
    std::array<float,4> extent{},center{},point{},normal{};
    std::array<std::array<float,4>,16> lights{};
    std::array<unsigned,4> coverage{}; // Triangle count, byte texel count, indexed coverage enabled, reserved.
    std::array<float,4> primary_range{};
};
static_assert(sizeof(ShadowParameters)==352);
template<class Dispatch>
void run_shadow_cutouts(Fixture fixture,Dispatch dispatch) {
    unsigned pixels=0,clear=0,blocked_pixels=0,edges=0,rejections=0,model_receivers=0,ground_receivers=0;
    std::array<unsigned,5> selected{};std::array<unsigned,4> qualities{};
    for(unsigned variant=0;variant<5;++variant)for(unsigned eye=0;eye<2;++eye)
        for(unsigned quality=0;quality<4;++quality)for(unsigned underlay=0;underlay<2;++underlay)
        for(unsigned indexed=0;indexed<2;++indexed)for(float bank:{-.13f,.13f}) {
        std::array<unsigned char,16> texels{};
        for(unsigned i=0;i<texels.size();++i)texels[i]=variant==0?0:variant==3?2:((i+variant)&1)?2:0;
        for(unsigned i=0;i<4;++i) {
            auto& m=fixture.materials[i];m={};m.textured=variant!=4;m.dither=variant==4;
            m.even=0;m.odd=2;m.base=i&1?254:0;m.offset=i*4;m.umask=m.vmask=1;
            m.uv={-3.37f,-2.19f,12.13f,-2.19f,12.13f,9.41f};
            if(i&1)m.uv={-3.37f,-2.19f,12.13f,9.41f,-3.37f,9.41f};
        }
        ShadowParameters p{};p.extent={float(width),float(height),eye?40.f:96.f,eye?32.f:85.f};
        p.center={eye?33.4f:30.6f,22.7f,1,float(underlay)};
        p.point={0,0,700,0};p.normal={bank,.11f,1,0};p.coverage={4,unsigned(texels.size()),indexed,0};
        const unsigned samples=quality==0?1:quality==1?4:quality==2?8:16;
        const V light=unit({.34,-.25,-1}),tangent=unit(cross(light,{0,1,0})),bitangent=cross(light,tangent);
        for(unsigned i=0;i<samples;++i) {
            // Source input disk, including HARD plus LOW/MEDIUM/HIGH. Geometry
            // and UV expectations below remain separate binary64 ray tests.
            const double radius=(quality==0?0.:.015)*std::sqrt((i+.5)/samples),angle=i*2.399963229728653;
            const V direction=unit(add(light,add(scale(tangent,radius*std::cos(angle)),scale(bitangent,radius*std::sin(angle)))));
            p.lights[i]={float(direction[0]),float(direction[1]),float(direction[2]),0};
        }
        p.lights[0][3]=float(samples);
        const auto actual=dispatch(p,fixture,texels);require(actual.size()==width*height,"Shadow cutout output extent");
        const V plane{p.point[0],p.point[1],p.point[2]},normal{p.normal[0],p.normal[1],p.normal[2]};
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;bool edge=false;double receiver=65536;bool ground=false;
            const V raw{(x+.5-p.center[0])/p.extent[2],(y+.5-p.center[1])/p.extent[3],1};
            if(!underlay) {
                if(indexed) {const auto hit=trace_indexed(fixture,texels,{},raw,1,x,y);receiver=hit.t;edge|=hit.edge;rejections+=hit.rejected;}
                else {const auto hit=trace(fixture.vertices,{},raw,1,65536);receiver=hit.t;edge|=hit.edge;}
            }
            const double denominator=dot(raw,normal);
            const double t=std::abs(denominator)>1e-10?dot(plane,normal)/denominator:65536;
            if(t>1 && t<receiver){receiver=t;ground=true;}
            unsigned blocked=0;
            if(receiver<65536) {
                const V origin=scale(raw,receiver);const double bias=std::max(.1,receiver*1e-5);
                for(unsigned i=0;i<samples;++i) {
                    const auto& light=p.lights[i];const V direction{light[0],light[1],light[2]};
                    if(indexed) {const auto hit=trace_indexed(fixture,texels,origin,direction,bias,x,y);blocked+=hit.found;edge|=hit.edge;rejections+=hit.rejected;}
                    else {const auto hit=trace(fixture.vertices,origin,direction,bias,65536);blocked+=hit.found;edge|=hit.edge;}
                }
            }
            if(edge){++edges;continue;}
            const unsigned expected=160*blocked/samples;
            if(actual[pixel]!=expected)throw std::runtime_error("Native shadow cutout mismatch variant="+std::to_string(variant)
                +" indexed="+std::to_string(indexed)+" underlay="+std::to_string(underlay)+" eye="+std::to_string(eye)
                +" quality="+std::to_string(quality)+" pixel="+std::to_string(pixel)+" actual/expected="+std::to_string(actual[pixel])+"/"+std::to_string(expected));
            ++pixels;++selected[variant];++qualities[quality];clear+=expected==0;blocked_pixels+=expected!=0;
            ground_receivers+=ground;model_receivers+=receiver<65536 && !ground;
        }
    }
    require(pixels>950000 && blocked_pixels>10000 && clear>10000 && rejections>10000 && model_receivers>1000 && ground_receivers>1000,
        "Insufficient positive native shadow cutout coverage");
    for(auto n:selected)require(n>100000,"Missing shadow source texture variant");
    for(auto n:qualities)require(n>200000,"Missing shadow ray sample count");
    std::cout<<"Native indexed shadow cutouts: exact mask pixels="<<pixels<<" clear="<<clear<<" shadowed="<<blocked_pixels
        <<" source rejected texel intersections="<<rejections<<" model/ground receivers="<<model_receivers<<'/'<<ground_receivers
        <<" source boundary exclusions="<<edges<<"; all HARD/LOW/MEDIUM/HIGH 1/4/8/16-ray masks exact, binary64 input-only ray/UV reference\n";
}
} // namespace native_reflection_fixture
