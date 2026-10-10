#pragma once
// Independent binary64 geometry/optics, shared across actual Vulkan/Metal
// producer tests. RGB deliberately shares the scalar lava recipe; this is NOT
// an independent artistic-quality or owner/lifetime/presentation oracle.
#include "starfox/render/lava_surface.hpp"
#include "reflected_liquid_oracle.hpp"
#include <algorithm>
#include <array>
#include <span>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace native_reflection_fixture {
inline void require(bool ok,const char* text) {if(!ok) throw std::runtime_error(text);}
using V=reflected_liquid_oracle::V;
using reflected_liquid_oracle::add;using reflected_liquid_oracle::scale;using reflected_liquid_oracle::dot;using reflected_liquid_oracle::unit;
inline V sub(V a,V b){return add(a,scale(b,-1));}
inline V cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
struct Hit {double t{65536};unsigned primitive{};bool found{},edge{};};
inline Hit trace(std::span<const std::array<float,4>> vertices,V origin,V direction,double minimum,double maximum) {
    Hit hit;hit.t=maximum;
    for(unsigned i=0;i<vertices.size();i+=3) {
        const auto position=[&](unsigned j){const auto& p=vertices[j];return V{p[0],p[1],p[2]};};
        const V a=position(i),b=position(i+1),c=position(i+2),e1=sub(b,a),e2=sub(c,a),q=cross(direction,e2);
        const double det=dot(e1,q);if(std::abs(det)<1e-10) continue;
        const V s=sub(origin,a),r=cross(s,e1);const double u=dot(s,q)/det,v=dot(direction,r)/det,t=dot(e2,r)/det;
        if(u<0 || v<0 || u+v>1 || t<=minimum || t>=hit.t) continue;
        hit={t,i/3,true,std::min({u,v,1-u-v})<1e-4};
    }
    return hit;
}
struct alignas(16) Parameters {
    std::array<unsigned,4> dimensions{};std::array<float,4> camera{},settings{},point{},normal{};
    std::array<unsigned,4> environment{};std::array<float,4> water{},row0{},row1{},row2{};
    std::array<unsigned,4> enhanced_size{},enhanced_modes{};
    std::array<float,4> enhanced_motion{},enhanced_plane{},enhanced_projection{},enhanced_palette{},keep0{},keep1{};
    std::array<unsigned,4> material_info{};
    std::array<float,4> source_colour{};
    std::array<unsigned,4> liquid_layers{};
    std::array<float,4> primary_range{};
    std::array<unsigned,4> cube_info{};
    std::array<float,4> cube_row0{},cube_row1{},cube_row2{};
};
static_assert(sizeof(Parameters)==416); // Native Vulkan ABI; Metal uses explicit conversion.
struct Material {std::array<float,6> uv{};unsigned textured{},dither{},even{},odd{},base{},face{},offset{},umask{},vmask{},reserved{};};
static_assert(sizeof(Material)==64);
inline V linear(unsigned word){V v{};for(unsigned c=0;c<3;++c){v[c]=double((word>>(8*c))&255)/255;v[c]*=v[c];}return v;}
inline unsigned pack(V v){unsigned word=0xff000000;for(unsigned c=0;c<3;++c) word|=unsigned(std::sqrt(std::clamp(v[c],0.,1.))*255+.5)<<(8*c);return word;}
inline constexpr unsigned width=64,height=48;
struct Fixture {
    std::array<std::array<float,4>,12> vertices{};
    std::array<Material,4> materials{};
    std::array<unsigned,256> palette{};
};
inline Fixture make_fixture() {
    Fixture f;
    f.vertices={{{-300,-220,0,0},{300,-220,0,0},{300,220,0,0},{-300,-220,0,0},{300,220,0,0},{-300,220,0,0},
        {115,-130,230,0},{180,-130,230,0},{180,-70,230,0},{115,-130,230,0},{180,-70,230,0},{115,-70,230,0}}};
    for(unsigned i=0;i<6;++i) f.vertices[i][2]=400+.7f*f.vertices[i][1]+.08f*f.vertices[i][0];
    for(unsigned i=0;i<4;++i) f.materials[i].even=f.materials[i].odd=i<2?1:2;
    f.palette[1]=0xff4d4d4d;f.palette[2]=0xff1a1acc;
    return f;
}
struct PathResult {
    V radiance{};
    unsigned ground_hops{},model_hops{};
    bool edge{};
};
inline PathResult reflection_path(const Fixture& fixture,const Parameters& p,
    const reflected_liquid_oracle::Frame& frame,V origin,V direction,double lower,bool escaping_source_ground) {
    const auto& vertices=fixture.vertices;const auto& palette=fixture.palette;
    const unsigned material=unsigned(p.water[3])&15;
    const V floor_point{p.point[0],p.point[1],p.point[2]},floor_normal{p.normal[0],p.normal[1],p.normal[2]};
    const auto floor_distance=[&](V o,V d,double near,double far) {const double den=dot(d,floor_normal);if(std::abs(den)<1e-6)return far;
        const double t=dot(sub(floor_point,o),floor_normal)/den;return t>near && t<far?t:far;};
    const auto normal_at=[&](unsigned primitive,V ray) {const unsigned i=primitive*3;
        const auto at=[&](unsigned j){return V{vertices[j][0],vertices[j][1],vertices[j][2]};};
        V n=unit(cross(sub(at(i+1),at(i)),sub(at(i+2),at(i))));return dot(n,ray)>0?scale(n,-1):n;};
    V throughput{1,1,1},sum{};bool boundary=false;unsigned lava_count=0,models=0;
    for(unsigned bounce=0;bounce<4;++bounce) {
        const double floor_t=escaping_source_ground && bounce==0 && dot(direction,floor_normal)>0?65536:floor_distance(origin,direction,lower,65536);const auto hit=trace(vertices,origin,direction,lower,floor_t);
        if(hit.edge){boundary=true;break;}
        if(hit.found) {
            ++models;
            if((unsigned(p.water[3])&16)==0) {const V colour=linear(palette[hit.primitive<2?1:2]);for(unsigned c=0;c<3;++c) sum[c]+=throughput[c]*colour[c];throughput={};break;}
            const V n=normal_at(hit.primitive,direction);lower=std::max(.05,hit.t*1e-5);origin=add(add(origin,scale(direction,hit.t)),scale(n,lower));direction=reflected_liquid_oracle::reflect(direction,n);continue;
        }
        if(floor_t==65536) break;
        ++lava_count;
        if(material!=3) {
            V n=unit(floor_normal);if(dot(n,direction)>0)n=scale(n,-1);
            if(material==2) {
                const V f0{1,.766,.336};const double grazing=std::pow(1-std::clamp(dot(scale(direction,-1),n),0.,1.),5);
                for(unsigned c=0;c<3;++c)throughput[c]*=f0[c]+(1-f0[c])*grazing;
            }
            lower=std::max(.05,floor_t*1e-5);origin=add(add(origin,scale(direction,floor_t)),scale(n,lower));direction=reflected_liquid_oracle::reflect(direction,n);continue;
        }
        V hp=add(origin,scale(direction,floor_t)),world=add(reflected_liquid_oracle::row(hp,frame),frame.offset);
        const double footprint=std::sqrt(dot(hp,hp))/std::max(double(p.camera[0]),1.)/std::max(std::abs(dot(direction,floor_normal)),.04);
        const auto geometry=reflected_liquid_oracle::lava(world[0],world[2],p.water[0],footprint);const V travel=reflected_liquid_oracle::row(direction,frame);
        const double shift=std::clamp(-geometry[0]/std::max(travel[1],.12),-floor_t*.2,floor_t*.2);hp=add(hp,scale(direction,shift));world=add(world,scale(travel,shift));
        const auto waved=reflected_liquid_oracle::lava(world[0],world[2],p.water[0],footprint);V n=unit(reflected_liquid_oracle::col({waved[1],-1,waved[2]},frame));if(dot(n,direction)>0)n=scale(n,-1);
        const auto surface=starfox::render::lava_surface(float(world[0]),float(world[2]),p.water[0],float(footprint));const V viewer=reflected_liquid_oracle::row(scale(direction,-1),frame);
        const auto colour=starfox::render::lava_shade(surface,float(viewer[0]),float(viewer[1]),float(viewer[2]));const V molten{double(colour.r*p.water[2]),double(colour.g*p.water[2]),double(colour.b*p.water[2])};
        for(unsigned c=0;c<3;++c) sum[c]+=throughput[c]*molten[c]*molten[c];
        const double share=(.035+.40*std::pow(1-std::clamp(dot(scale(direction,-1),n),0.,1.),5))*p.water[1]*(.3+.7*surface.crust);throughput=scale(throughput,share);
        if(*std::max_element(throughput.begin(),throughput.end())<1e-5){throughput={};break;}
        lower=std::max(.05,floor_t*1e-5);origin=add(hp,scale(n,lower));direction=reflected_liquid_oracle::reflect(direction,n);
    }
    const V environment=linear(p.environment[0]);for(unsigned c=0;c<3;++c)sum[c]+=throughput[c]*environment[c];
    return {sum,lava_count,models,boundary};
}
template<class Dispatch>
void run(const Fixture& fixture,Dispatch dispatch) {
    const auto& vertices=fixture.vertices;
    const auto& palette=fixture.palette;
    unsigned samples=0,edges=0,ground_hops=0,model_hops=0,repeated=0,changes=0,sentinels=0,max_error=0;
    std::array<unsigned,5> selected{};
    for(unsigned variant=0;variant<5;++variant) for(unsigned eye=0;eye<2;++eye) {
        Parameters p{};p.dimensions={width,height,1,0};p.camera={96,85,eye?33.4f:30.6f,22.7f};p.settings={0,1,0,0};
        const float angle=variant==2?.13f:0;p.point={0,180,0,0};p.normal={std::sin(angle),-std::cos(angle),0,0};
        p.row0={std::cos(angle),-std::sin(angle),0,29};p.row1={std::sin(angle),std::cos(angle),0,-3};p.row2={0,0,1,17};
        std::array<std::vector<unsigned>,2> first;
        for(unsigned phase=0;phase<2;++phase) for(unsigned sentinel=0;sentinel<2;++sentinel) {
            const unsigned material=variant==3?1:variant==4?2:3;
            p.environment[0]=sentinel?0xff29b50d:0xffa035e0;p.water={phase?4.7f:1.3f,variant? .8f:0,.72f,float(material|(variant==1?0:16))};
            const auto actual=dispatch(p);require(actual.size()==width*height,"Native reflection output extent");
            reflected_liquid_oracle::Frame frame;frame.rotation={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};frame.offset={29,-3,17};
            const V floor_point{0,180,0},floor_normal{p.normal[0],p.normal[1],0};
            const auto floor_distance=[&](V o,V d,double lower,double upper) {const double den=dot(d,floor_normal);if(std::abs(den)<1e-6) return upper;
                const double t=dot(sub(floor_point,o),floor_normal)/den;return t>lower && t<upper?t:upper;};
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const unsigned index=y*width+x;const V primary_direction{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1};
                const auto primary=trace(vertices,{},primary_direction,1,floor_distance({},primary_direction,1,65536));
                if(!primary.found) continue;
                if(primary.edge){++edges;continue;}
                const auto normal_at=[&](unsigned primitive,V direction) {const unsigned i=primitive*3;
                    const auto at=[&](unsigned j){return V{vertices[j][0],vertices[j][1],vertices[j][2]};};
                    V n=unit(cross(sub(at(i+1),at(i)),sub(at(i+2),at(i))));return dot(n,direction)>0?scale(n,-1):n;};
                V origin=scale(primary_direction,primary.t),direction=reflected_liquid_oracle::reflect(unit(primary_direction),normal_at(primary.primitive,primary_direction));
                const auto path=reflection_path(fixture,p,frame,origin,direction,std::max(.1,primary.t*1e-5),false);
                if(path.edge){++edges;continue;}if(!path.ground_hops)continue;
                const auto lava_count=path.ground_hops,models=path.model_hops;const unsigned expected=pack(path.radiance);

                require((actual[index]>>24)==255,"Model -> lava alpha is not opaque");
                for(unsigned c=0;c<3;++c){const auto a=int((actual[index]>>(8*c))&255),e=int((expected>>(8*c))&255);max_error=std::max(max_error,unsigned(std::abs(a-e)));
                    if(std::abs(a-e)>2) throw std::runtime_error("Native secondary lava mismatch variant="+std::to_string(variant)+" eye="+std::to_string(eye)+" phase="+std::to_string(phase)+" pixel="+std::to_string(index)+" channel="+std::to_string(c)+" actual/expected="+std::to_string(a)+"/"+std::to_string(e));}
                ++samples;++selected[variant];ground_hops+=lava_count;model_hops+=models;repeated+=lava_count>1;
                if(variant==0 && sentinel){require(actual[index]==first[phase][index],"Emission retained original floor/environment");++sentinels;}
                if(!sentinel && phase) for(unsigned c=0;c<3;++c) changes+=std::abs(int((actual[index]>>(8*c))&255)-int((first[0][index]>>(8*c))&255))>2;
            }
            if(!sentinel)first[phase]=actual;
        }
    }
    require(samples>10000 && ground_hops>samples && model_hops>0 && repeated>0 && changes>100 && sentinels>1000,"Insufficient positive native GLSL lava coverage");
    for(auto n:selected)require(n>1000,"Missing positive lava/mirror/gold variant");
    std::cout<<"Native secondary lava/mirror/gold: samples="<<samples<<" selected variants=";
    for(auto n:selected)std::cout<<n<<',';
    std::cout<<" ground/model hops="<<ground_hops<<'/'<<model_hops<<" repeated="<<repeated<<" time changes="<<changes<<" old-floor-independent pairs="<<sentinels<<" source edge exclusions="<<edges<<" max RGB byte error="<<max_error<<" alpha exact255\n";
}
inline unsigned pack_gamma(V colour,unsigned marker) {
    unsigned word=marker<<24;for(unsigned c=0;c<3;++c)word|=unsigned(std::clamp(colour[c],0.,1.)*255+.5)<<(8*c);return word;
}
template<class Dispatch>
void run_primary(const Fixture& fixture,Dispatch dispatch) {
    unsigned samples=0,edges=0,clear_pixels=0,escapes=0,max_error=0;
    std::array<unsigned,3> selected{};
    // Real ground-only exposure keeps all model casters in secondary rays.
    // Both normal and mirrored models must coexist with the completed surface.
    for(unsigned material:{1u,2u,3u}) for(float angle:{-.13f,0.f,.13f})
        for(unsigned eye=0;eye<2;++eye) for(float time:{1.3f,4.7f})
        for(float strength:{0.f,.8f}) for(unsigned model_mirror=0;model_mirror<2;++model_mirror) {
        Parameters p{};p.dimensions={width,height,1,0};p.camera={96,85,eye?33.4f:30.6f,22.7f};p.settings={0,1,0,1};
        p.point={0,180,0,0};p.normal={std::sin(angle),-std::cos(angle),0,0};
        p.environment[0]=0xffa035e0;p.water={time,strength,.72f,float(material|(model_mirror?16:0))};
        p.row0={std::cos(angle),-std::sin(angle),0,29};p.row1={std::sin(angle),std::cos(angle),0,-3};p.row2={0,0,1,17};
        const auto actual=dispatch(p);require(actual.size()==width*height,"Native primary surface output extent");
        reflected_liquid_oracle::Frame frame;frame.rotation={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};frame.offset={29,-3,17};
        const V floor_point{0,180,0},floor_normal{p.normal[0],p.normal[1],0};
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const unsigned index=y*width+x;const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1};
            const double den=dot(raw,floor_normal),parameter=std::abs(den)>1e-10?dot(floor_point,floor_normal)/den:65536;
            if(parameter<=1 || parameter>=65536) {require(actual[index]==0,"Ground-only surface painted the background");++clear_pixels;continue;}
            const double distance=parameter*std::sqrt(dot(raw,raw));const V incoming=unit(raw);
            V hit=scale(raw,parameter),normal=unit(floor_normal),colour{};
            if(dot(normal,incoming)>0)normal=scale(normal,-1);
            double share=strength;const double bias=std::max(.05,distance*1e-5);
            if(material==3) {
                V world=add(reflected_liquid_oracle::row(hit,frame),frame.offset);
                const double footprint=distance/std::max(double(p.camera[0]),1.)/std::max(std::abs(dot(incoming,floor_normal)),.04);
                const auto first=reflected_liquid_oracle::lava(world[0],world[2],time,footprint);const V travel=reflected_liquid_oracle::row(incoming,frame);
                const double shift=std::clamp(-first[0]/std::max(travel[1],.12),-distance*.2,distance*.2);hit=add(hit,scale(incoming,shift));world=add(world,scale(travel,shift));
                const auto geometry=reflected_liquid_oracle::lava(world[0],world[2],time,footprint);
                normal=unit(reflected_liquid_oracle::col({geometry[1],-1,geometry[2]},frame));if(dot(normal,incoming)>0)normal=scale(normal,-1);
                const auto surface=starfox::render::lava_surface(float(world[0]),float(world[2]),time,float(footprint));const V viewer=reflected_liquid_oracle::row(scale(incoming,-1),frame);
                const auto shaded=starfox::render::lava_shade(surface,float(viewer[0]),float(viewer[1]),float(viewer[2]));
                colour={double(shaded.r*p.water[2]),double(shaded.g*p.water[2]),double(shaded.b*p.water[2])};
                share=(.035+.40*std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5))*strength*(.3+.7*surface.crust);
            }
            const V origin=add(hit,scale(normal,bias)),outgoing=reflected_liquid_oracle::reflect(incoming,normal);
            if(material==3 && share>0) {const double outgoing_den=dot(outgoing,floor_normal);
                if(outgoing_den>1e-6 && dot(sub(floor_point,origin),floor_normal)/outgoing_den>bias)++escapes;}
            if(share>0) {
                const auto path=reflection_path(fixture,p,frame,origin,outgoing,bias,true);
                if(path.edge){++edges;continue;}
                const auto terminal=pack(path.radiance);
                if(material==3) for(unsigned c=0;c<3;++c)colour[c]+=double((terminal>>(8*c))&255)/255*share;
                else {
                    const V f0=material==2?V{1,.766,.336}:V{1,1,1};const double grazing=std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5);
                    for(unsigned c=0;c<3;++c)colour[c]=double((terminal>>(8*c))&255)/255*std::sqrt(strength*(f0[c]+(1-f0[c])*grazing))*p.water[2];
                }
            }
            const auto expected=pack_gamma(colour,254);require((actual[index]>>24)==254,"Completed ground surface lost its coverage marker");
            for(unsigned c=0;c<3;++c) {const auto a=int((actual[index]>>(8*c))&255),e=int((expected>>(8*c))&255);max_error=std::max(max_error,unsigned(std::abs(a-e)));
                if(std::abs(a-e)>2)throw std::runtime_error("Native primary ground mismatch material="+std::to_string(material)+" angle="+std::to_string(angle)+" strength="+std::to_string(strength)+" eye="+std::to_string(eye)+" time="+std::to_string(time)+" pixel="+std::to_string(index)+" channel="+std::to_string(c)+" actual/expected="+std::to_string(a)+"/"+std::to_string(e));}
            ++samples;++selected[material-1];
        }
    }
    require(samples>100000 && clear_pixels>100000 && escapes>1000,"Insufficient native primary liquid/mirror/gold coverage");
    for(auto n:selected)require(n>30000,"Missing native primary surface variant");
    std::cout<<"Native primary lava/mirror/gold: samples="<<samples<<" selected="<<selected[0]<<','<<selected[1]<<','<<selected[2]
        <<" outside-ground exact0="<<clear_pixels<<" displaced outward plane escapes="<<escapes<<" source edge exclusions="<<edges<<" max RGB byte error="<<max_error<<" ground marker exact254\n";
}
} // namespace native_reflection_fixture
