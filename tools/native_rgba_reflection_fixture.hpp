#pragma once
#include "native_rgba_shadow_fixture.hpp"
#include "starfox/render/effects.hpp"

namespace native_reflection_fixture {
// Colour expectations use the ordinary CPU presentation equations, not the
// translated shader function or any GPU hit/UV/colour readback. Native solids
// arrive pre-styled; textures keep their styles for the committed sample.
struct NativeReflectedSource {
    NativeShadowSource source;
    std::array<std::array<unsigned,2>,4> solid;
    std::array<std::array<unsigned,4>,4> texel;
    bool empty_geometry{}; // Logical source topology, not degenerate fake casters.
};
inline unsigned cpu_native_style(unsigned word,starfox::render::Effect style,unsigned intensity) {
    starfox::render::Framebuffer pixel(1,1);pixel.enable_layer_tags(true);
    std::vector<unsigned char> rgba{static_cast<unsigned char>(word),static_cast<unsigned char>(word>>8),
        static_cast<unsigned char>(word>>16),static_cast<unsigned char>(word>>24)},scratch;
    starfox::render::apply_effect(style,pixel,rgba,scratch,static_cast<unsigned char>(intensity));
    unsigned result=0;for(unsigned c=0;c<4;++c)result|=unsigned(rgba[c])<<(8*c);return result;
}
inline NativeReflectedSource native_reflected_source(unsigned variant,starfox::render::Effect style,
    unsigned intensity,bool srgb) {
    NativeReflectedSource result;result.source=native_shadow_source(variant);
    constexpr std::array<unsigned,6> colours{0xff332211,0xff55bb99,0xff2266ee,0xffb32f7a,0xff000000,0xffecddb8};
    for(unsigned i=0;i<4;++i) {
        auto& input=result.source.opacity[i];auto& words=result.source.words;const unsigned at=i*16;
        if(!input.textured) {
            for(unsigned odd=0;odd<2;++odd) {
                const unsigned colour=(colours[(i+odd)%colours.size()]&0xffffff)|(unsigned(input.solid_alpha[odd])<<24);
                result.solid[i][odd]=cpu_native_style(colour,style,intensity);words[at+8+odd]=result.solid[i][odd];
            }
            continue;
        }
        words[at+8]=unsigned(style);words[at+9]=intensity;words[at+10]=unsigned(srgb);
        words[at+11]=0x08000000u; // Non-colour metadata must not tint the sample.
        words[at+7]=(words[at+7]&~2u)|(srgb?2u:0u);
        if(variant==4) {
            const unsigned base=64+i*257;
            const std::array<unsigned,4> native{0xff000000,colours[(i+2)%colours.size()],0x00443322,0xfeaabbcc};
            input.texel_alpha={255,255,0,254};
            words[base]=native[0];words[base+3]=native[1];words[base+2]=native[2];words[base+255]=native[3];
            words[base+256]=0xff020300; // Opaque index0, coloured3, hole2, fractional255.
            for(unsigned t=0;t<4;++t)result.texel[i][t]=cpu_native_style(native[t],style,intensity);
        } else for(unsigned t=0;t<4;++t) {
            const unsigned colour=(colours[(i+t)%colours.size()]&0xffffff)|(unsigned(input.texel_alpha[t])<<24);
            words[64+i*4+t]=colour;result.texel[i][t]=cpu_native_style(colour,style,intensity);
        }
    }
    return result;
}
inline IndexedHit trace_native_colour(const NativeReflectedSource& f,V origin,V direction,double minimum,
    unsigned x,unsigned y,double maximum=65536) {
    if(f.empty_geometry)return {};
    auto result=trace_native_shadow(f.source,origin,direction,minimum,x,y,maximum);
    if(!result.found)return result;
    const unsigned primitive=result.primitive;const auto& input=f.source.opacity[primitive];
    if(!input.textured) {
        const unsigned d=std::max(1U,input.scale);
        result.colour=f.solid[primitive][input.scale && (((x/d)^(y/d))&1)];return result;
    }
    const auto at=[&](unsigned corner){const auto& a=f.source.geometry.vertices[primitive*3+corner];return V{a[0],a[1],a[2]};};
    const V a=at(0),e1=sub(at(1),a),e2=sub(at(2),a),q=cross(direction,e2),s=sub(origin,a),r=cross(s,e1);
    const double det=dot(e1,q),u=dot(s,q)/det,v=dot(direction,r)/det;
    const double tx=input.uv[0]*(1-u-v)+input.uv[2]*u+input.uv[4]*v;
    const double ty=input.uv[1]*(1-u-v)+input.uv[3]*u+input.uv[5]*v;
    result.colour=f.texel[primitive][(unsigned(int(std::floor(ty)))&1)*2+(unsigned(int(std::floor(tx)))&1)];return result;
}
template<class Dispatch> void run_native_rgba_reflections(Dispatch dispatch) {
    using starfox::render::Effect;
    constexpr std::array styles{Effect::off,Effect::monochrome,Effect::sepia,Effect::thermal,Effect::pastel,
        Effect::posterized,Effect::ice,Effect::film,Effect::negative,Effect::solarized,Effect::amber,
        Effect::emerald,Effect::cyanotype,Effect::copper,Effect::lavender,Effect::cga,Effect::teal_orange,
        Effect::handheld,Effect::bleach_bypass,Effect::risograph,Effect::duotone,Effect::tritone,
        Effect::iridescent,Effect::noir,Effect::uv_glow,Effect::topographic};
    unsigned checked=0,clear=0,rejected=0,secondary=0,direct_model=0,recursive=0,edges=0,max_error=0,submissions=0;
    std::array<unsigned,styles.size()> styled{};
    const auto run=[&](unsigned variant,unsigned style_index,unsigned intensity,bool srgb,unsigned eye,bool ground,bool mirrored) {
        const auto source=native_reflected_source(variant,styles[style_index],intensity,srgb);
        Parameters p{};p.dimensions={width,height,1,0};p.camera={eye?40.f:96.f,eye?32.f:85.f,eye?33.4f:30.6f,22.7f};
        p.settings={0,float(ground),0,float(ground)};p.environment[0]=0xff29b50d;
        p.point={0,20,0,0};p.normal={0,-1,0,0};p.water={0,1,1,float(unsigned(ground)|(mirrored?16:0))};
        p.material_info={2,unsigned(source.source.words.size()*4),0,0};
        const auto actual=dispatch(p,source);require(actual.size()==width*height,"Native RGBA reflection extent");++submissions;
        const auto normal_at=[&](unsigned primitive,V ray) {
            const auto at=[&](unsigned corner){const auto& a=source.source.geometry.vertices[primitive*3+corner];return V{a[0],a[1],a[2]};};
            V n=unit(cross(sub(at(1),at(0)),sub(at(2),at(0))));return dot(n,ray)>0?scale(n,-1):n;
        };
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1};
            const auto primary=ground?IndexedHit{}:trace_native_colour(source,{},raw,1,x,y);
            if(primary.edge){++edges;continue;}rejected+=primary.rejected;
            const double floor_t=raw[1]>0?20/raw[1]:65536;
            if((!ground && !primary.found) || (ground && (floor_t<=1 || floor_t>=65536))) {
                require(actual[pixel]==0,"Rejected native primary still hides the scene");++clear;continue;
            }
            const double primary_t=ground?floor_t:primary.t;
            V origin=scale(raw,primary_t),n=ground?V{0,-1,0}:normal_at(primary.primitive,raw);
            V direction=reflected_liquid_oracle::reflect(unit(raw),n);
            double lower=ground?std::max(.05,primary_t*std::sqrt(dot(raw,raw))*1e-5):std::max(.1,primary_t*1e-5);
            if(ground)origin=add(origin,scale(n,lower));
            unsigned expected=p.environment[0],hops=0;bool boundary=false,terminal_model=false;
            for(unsigned bounce=0;bounce<(mirrored || ground?4u:1u);++bounce) {
                const double to_floor=ground && direction[1]>1e-6?(20-origin[1])/direction[1]:65536;
                const double next_floor=to_floor>lower && to_floor<65536?to_floor:65536;
                const auto hit=trace_native_colour(source,origin,direction,lower,x,y,next_floor);
                if(hit.edge){boundary=true;break;}rejected+=hit.rejected;
                if(!hit.found && next_floor==65536)break;
                const double distance=hit.found?hit.t:next_floor;++hops;
                if(hit.found && !mirrored){expected=hit.colour;terminal_model=true;break;}
                n=hit.found?normal_at(hit.primitive,direction):V{0,-1,0};
                if(dot(n,direction)>0)n=scale(n,-1);lower=std::max(.05,distance*1e-5);
                origin=add(add(origin,scale(direction,distance)),scale(n,lower));direction=reflected_liquid_oracle::reflect(direction,n);
            }
            if(boundary){++edges;continue;}
            require((actual[pixel]>>24)==(ground?254u:255u),"Native model/ground coverage marker changed");
            for(unsigned c=0;c<3;++c) {
                const int a=(actual[pixel]>>(8*c))&255,e=(expected>>(8*c))&255;
                max_error=std::max(max_error,unsigned(std::abs(a-e)));
                if(std::abs(a-e)>1)throw std::runtime_error("Native RGBA reflection mismatch variant="+std::to_string(variant)
                    +" style="+std::to_string(unsigned(styles[style_index]))+" intensity="+std::to_string(intensity)
                    +" srgb="+std::to_string(srgb)+" eye="+std::to_string(eye)+" ground="+std::to_string(ground)
                    +" mirrored="+std::to_string(mirrored)+" pixel="+std::to_string(pixel)+" actual/expected="
                    +std::to_string(a)+"/"+std::to_string(e));
            }
            ++checked;secondary+=hops;direct_model+=terminal_model && !ground;recursive+=hops>1;
            if(terminal_model)++styled[style_index];
        }
    };
    for(unsigned variant=0;variant<native_shadow_variant_count;++variant)for(bool srgb:{false,true})
        for(unsigned eye=0;eye<2;++eye)for(bool ground:{false,true})for(bool mirror:{false,true})
            run(variant,0,100,srgb,eye,ground,mirror);
    for(unsigned style=0;style<styles.size();++style)for(unsigned intensity:{0U,1U,37U,100U,255U})
        for(bool srgb:{false,true})for(unsigned variant:{0U,3U,4U})for(unsigned eye=0;eye<2;++eye)
        for(bool ground:{false,true})run(variant,style,intensity,srgb,eye,ground,false);
    require(checked>3000000 && clear>1000000 && rejected>1000000 && secondary>10000 && direct_model>100 && recursive>100,
        "Insufficient native reflection primary/secondary/recursive coverage");
    for(unsigned n:styled)require(n>1000,"Missing committed native colour style");
    std::cout<<"Native RGBA reflections: "<<submissions<<" submissions, opaque="<<checked<<" clear="<<clear
        <<" rejected="<<rejected<<" secondary="<<secondary<<" direct model="<<direct_model<<" recursive="<<recursive
        <<" boundary exclusions="<<edges<<" max RGB byte error="<<max_error<<"; "<<styles.size()
        <<" CPU palette styles at 0/1/37/100/255 intensity, UNORM/sRGB native solid/RGBA/palette, input-only binary64 rays/UVs\n";
}
} // namespace native_reflection_fixture
