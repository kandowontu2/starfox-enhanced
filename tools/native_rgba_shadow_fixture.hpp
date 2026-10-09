#pragma once
#include "native_shadow_cutout_fixture.hpp"
#include <bit>
#include <cstring>
#include <limits>

namespace native_reflection_fixture {
inline constexpr unsigned native_shadow_word_capacity=4*16+4*257;
inline constexpr unsigned native_shadow_variant_count=24;
struct NativeOpacity {
    bool valid{true},textured{},clamp{};
    unsigned scale{};
    std::array<unsigned char,2> solid_alpha{255,255};
    std::array<unsigned char,4> texel_alpha{};
    std::array<float,6> uv{};
};
struct NativeShadowSource {
    Fixture geometry;
    std::array<NativeOpacity,4> opacity;
    std::vector<unsigned> words;
};
inline NativeShadowSource native_shadow_source(unsigned variant) {
    NativeShadowSource result;result.geometry=make_fixture();result.words.resize(4*16);
    for(unsigned i=0;i<4;++i) {
        auto& input=result.opacity[i];Material m{};
        m.reserved=2;m.even=m.odd=0xff000000; // Opaque black is still a caster.
        input.scale=variant==2?3:0;m.dither=input.scale;
        if(variant==1)input.solid_alpha={0,0};
        if(variant==2)input.solid_alpha={254,255};
        m.even=unsigned(input.solid_alpha[0])<<24;m.odd=unsigned(input.solid_alpha[1])<<24;
        if(variant>=3 && variant!=15 && variant!=21) {
            input.textured=true;input.clamp=variant==7;
            input.uv={-3.37f,-2.19f,12.13f,-2.19f,12.13f,9.41f};
            if(i&1)input.uv={-3.37f,-2.19f,12.13f,9.41f,-3.37f,9.41f};
            for(unsigned texel=0;texel<4;++texel)
                input.texel_alpha[texel]=variant==5?0:((texel+i)&1)?255:variant==6?254:0;
            m.reserved=3;m.textured=1;m.dither=1|(variant==7?4U:0U)|(variant==14?2U:0U);
            m.uv=input.uv;m.umask=m.vmask=1;m.offset=unsigned(result.words.size()*4);
            // Style words deliberately look unlike colours; opacity must not
            // interpret their alpha or apply model colour/effect transforms.
            m.even=0x12345678;m.odd=0x89abcdef;m.base=0x20001;m.face=0x301;
            if(variant==4) {
                m.dither|=536870912U;
                const auto first=result.words.size();result.words.resize(first+257,0);
                // Native index zero is a palette lookup, not legacy ink zero.
                result.words[first]=0xff000000;result.words[first+2]=0x0000ff00;
                result.words[first+256]=0x02000200;
                input.texel_alpha={255,0,255,0};
            } else for(auto alpha:input.texel_alpha)result.words.push_back((unsigned(alpha)<<24)|0x00715329);
            if(variant==8){m.dither|=8;input.valid=false;}
            if(variant==9){m.offset+=2;input.valid=false;}
            if(variant==10){m.vmask=2;input.valid=false;}
            if(variant==11){m.uv[0]=std::numeric_limits<float>::quiet_NaN();input.valid=false;}
            if(variant==12){m.reserved=4;input.valid=false;}
            if(variant==13){m.uv[0]=-65537;input.valid=false;}
            if(variant==16){m.dither=0;input.valid=false;}
            if(variant==17){m.offset=252;input.valid=false;}
            if(variant==18){m.offset=0xfffffffcu;input.valid=false;}
            if(variant==19){m.umask=4095;input.valid=false;}
            if(variant==20){m.uv[5]=std::numeric_limits<float>::infinity();input.valid=false;}
            if(variant==22){m.offset=320;input.valid=false;}
            if(variant==23){m.textured=0;input.valid=false;}
        }
        if(variant==15){m.dither=4097;input.valid=false;}
        if(variant==21){m.textured=1;input.valid=false;}
        std::memcpy(result.words.data()+i*16,&m,sizeof(m));
    }
    require(result.words.size()<=native_shadow_word_capacity,"Native shadow fixture storage bound");
    return result;
}
inline IndexedHit trace_native_shadow(const NativeShadowSource& f,V origin,V direction,
    double minimum,unsigned x,unsigned y,double maximum=65536) {
    IndexedHit result;result.t=maximum;
    for(unsigned i=0;i<4;++i) {
        const auto at=[&](unsigned corner){const auto& p=f.geometry.vertices[i*3+corner];return V{p[0],p[1],p[2]};};
        const V a=at(0),e1=sub(at(1),a),e2=sub(at(2),a),q=cross(direction,e2);
        const double det=dot(e1,q);if(std::abs(det)<1e-10)continue;
        const V s=sub(origin,a),r=cross(s,e1);
        const double u=dot(s,q)/det,v=dot(direction,r)/det,t=dot(e2,r)/det;
        if(u<0 || v<0 || u+v>1 || t<=minimum || t>=maximum)continue;
        const auto& input=f.opacity[i];bool edge=std::min({u,v,1-u-v})<1e-4;
        unsigned alpha=0;
        if(input.valid) {
            if(!input.textured) {
                const unsigned divisor=std::max(1U,input.scale);
                alpha=input.solid_alpha[input.scale && (((x/divisor)^(y/divisor))&1)];
            } else {
                const double tx=input.uv[0]*(1-u-v)+input.uv[2]*u+input.uv[4]*v;
                const double ty=input.uv[1]*(1-u-v)+input.uv[3]*u+input.uv[5]*v;
                edge|=std::min({tx-std::floor(tx),std::ceil(tx)-tx,ty-std::floor(ty),std::ceil(ty)-ty})<1e-4;
                if(!input.clamp || (tx>=0 && tx<2 && ty>=0 && ty<2))
                    alpha=input.texel_alpha[(unsigned(int(std::floor(ty)))&1)*2+(unsigned(int(std::floor(tx)))&1)];
            }
        }
        if(alpha!=255){++result.rejected;result.edge|=edge;continue;}
        if(t<result.t){result.t=t;result.found=true;result.primitive=i;result.edge|=edge;}
    }
    return result;
}
template<class Dispatch> void run_native_rgba_shadows(Dispatch dispatch) {
    unsigned pixels=0,clear=0,blocked_pixels=0,rejections=0,edges=0,model=0,ground=0;
    std::array<unsigned,native_shadow_variant_count> variants{};
    for(unsigned variant=0;variant<native_shadow_variant_count;++variant)for(unsigned eye=0;eye<2;++eye)
        for(unsigned quality=0;quality<4;++quality)for(unsigned underlay=0;underlay<2;++underlay)
        for(float bank:{-.13f,.13f}) {
        const auto source=native_shadow_source(variant);ShadowParameters p{};
        p.extent={float(width),float(height),eye?40.f:96.f,eye?32.f:85.f};p.center={eye?33.4f:30.6f,22.7f,1,float(underlay)};
        p.point={0,0,700,0};p.normal={bank,.11f,1,0};p.coverage={4,0,2,unsigned(source.words.size()*4)};
        const unsigned samples=quality==0?1:quality==1?4:quality==2?8:16;
        const V light=unit({.34,-.25,-1}),tangent=unit(cross(light,{0,1,0})),bitangent=cross(light,tangent);
        for(unsigned i=0;i<samples;++i) {
            const double radius=(quality==0?0.:.015)*std::sqrt((i+.5)/samples),angle=i*2.399963229728653;
            const auto direction=unit(add(light,add(scale(tangent,radius*std::cos(angle)),scale(bitangent,radius*std::sin(angle)))));
            p.lights[i]={float(direction[0]),float(direction[1]),float(direction[2]),0};
        }
        p.lights[0][3]=float(samples);
        const auto actual=dispatch(p,source);require(actual.size()==width*height,"Native RGBA shadow extent");
        const V plane{0,0,700},normal{bank,.11f,1};
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const V raw{(x+.5-p.center[0])/p.extent[2],(y+.5-p.center[1])/p.extent[3],1};
            bool edge=false,is_ground=false;double receiver=65536;
            if(!underlay){const auto hit=trace_native_shadow(source,{},raw,1,x,y);receiver=hit.t;edge|=hit.edge;rejections+=hit.rejected;}
            const double den=dot(raw,normal),t=std::abs(den)>1e-10?dot(plane,normal)/den:65536;
            if(t>1 && t<receiver){receiver=t;is_ground=true;}
            unsigned blocked=0;
            if(receiver<65536)for(unsigned i=0;i<samples;++i) {
                const auto& l=p.lights[i];const auto hit=trace_native_shadow(source,scale(raw,receiver),{l[0],l[1],l[2]},
                    std::max(.1,receiver*1e-5),x,y);blocked+=hit.found;edge|=hit.edge;rejections+=hit.rejected;
            }
            if(edge){++edges;continue;}
            const unsigned expected=160*blocked/samples;
            if(actual[y*width+x]!=expected)throw std::runtime_error("Native RGBA shadow mismatch variant="+std::to_string(variant)
                +" eye="+std::to_string(eye)+" quality="+std::to_string(quality)+" underlay="+std::to_string(underlay)
                +" pixel="+std::to_string(y*width+x)+" actual/expected="+std::to_string(actual[y*width+x])+"/"+std::to_string(expected));
            ++pixels;++variants[variant];clear+=!expected;blocked_pixels+=expected!=0;ground+=is_ground;model+=receiver<65536 && !is_ground;
        }
    }
    require(pixels>2300000 && blocked_pixels>10000 && clear>10000 && rejections>10000 && model>1000 && ground>1000,
        "Insufficient native RGBA shadow coverage");
    for(auto count:variants)require(count>90000,"Missing native RGBA material case");
    std::cout<<"Native RGBA shadows: exact mask pixels="<<pixels<<" clear="<<clear<<" shadowed="<<blocked_pixels
        <<" rejected="<<rejections<<" model/ground="<<model<<'/'<<ground<<" boundary exclusions="<<edges
        <<"; "<<native_shadow_variant_count<<" solid/native-palette/RGBA/clamp/malformed cases, binary64 input-only ray/UV reference\n";
}
} // namespace native_reflection_fixture
