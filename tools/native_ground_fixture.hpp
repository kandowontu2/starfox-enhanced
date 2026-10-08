#pragma once
#include "native_environment_cube_fixture.hpp"

namespace native_reflection_fixture {
// Geometry, source coverage, optical waves and colour transport are evaluated
// from inputs in binary64, not GPU hits. Lava's artistic scalar colour recipe
// remains shared; this checks transport, not subjective lava realism.
inline V native_authored_linear(V value,unsigned encoding) {
    if(encoding==2)for(auto& v:value)v=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);
    return value;
}
inline V native_bytes(unsigned packed) {
    return {double(packed&255)/255,double((packed>>8)&255)/255,double((packed>>16)&255)/255};
}
inline V product(V a,V b) {return {a[0]*b[0],a[1]*b[1],a[2]*b[2]};}
struct NativeGroundOptical {V hit{},normal{},emission{};double share{},bias{};};
inline NativeGroundOptical native_lava_optical(const Parameters& p,const reflected_liquid_oracle::Frame& frame,
    V origin,V direction,double distance) {
    NativeGroundOptical result;result.hit=add(origin,scale(direction,distance));
    V position=add(reflected_liquid_oracle::row(result.hit,frame),frame.offset);
    const double footprint=std::sqrt(dot(result.hit,result.hit))/std::max(double(p.camera[0]),1.)
        /std::max(std::abs(dot(direction,frame.normal)),.04);
    const auto wave=reflected_liquid_oracle::lava(position[0],position[2],p.water[0],footprint);
    const V travel=reflected_liquid_oracle::row(direction,frame);
    const double shift=std::clamp(-wave[0]/std::max(travel[1],.12),-distance*.2,distance*.2);
    position=add(position,scale(travel,shift));result.hit=add(result.hit,scale(direction,shift));
    const auto displaced=reflected_liquid_oracle::lava(position[0],position[2],p.water[0],footprint);
    result.normal=unit(reflected_liquid_oracle::col({displaced[1],-1,displaced[2]},frame));
    if(dot(result.normal,direction)>0)result.normal=scale(result.normal,-1);
    const auto surface=starfox::render::lava_surface(float(position[0]),float(position[2]),p.water[0],float(footprint));
    const V viewer=reflected_liquid_oracle::row(scale(direction,-1),frame);
    const auto colour=starfox::render::lava_shade(surface,float(viewer[0]),float(viewer[1]),float(viewer[2]));
    result.emission=native_authored_linear({double(colour.r*p.water[2]),double(colour.g*p.water[2]),double(colour.b*p.water[2])},p.material_info[2]);
    result.share=(.035+.40*std::pow(1-std::clamp(dot(scale(direction,-1),result.normal),0.,1.),5))
        *p.water[1]*(.3+.7*surface.crust);result.bias=std::max(.05,distance*1e-5);return result;
}
struct NativeGroundPath {
    unsigned packed{};unsigned ground{},models{},seams{},models_outside_primary{};bool edge{};
    // Diagnostic-only finite one-bounce witness, derived solely from inputs.
    unsigned secondary{UINT32_MAX};std::array<double,2> bary{};
};
inline NativeGroundPath native_ground_path(const NativeCubeInput& input,const Parameters& p,
    const reflected_liquid_oracle::Frame& frame,V origin,V direction,double minimum,
    unsigned x,unsigned y,bool escaping) {
    NativeGroundPath result;V sum{},throughput{1,1,1};const unsigned material=unsigned(p.water[3])&15;
    const auto terminal=[&](unsigned word){return native_pack(add(sum,product(throughput,native_linear(word,p.material_info[2]))),p.material_info[2],255);};
    for(unsigned bounce=0;bounce<4;++bounce) {
        const auto model=trace_native_colour(input.source,origin,direction,minimum,x,y);
        if(model.edge){result.edge=true;return result;}
        const double den=dot(direction,frame.normal),floor=std::abs(den)>1e-6?dot(sub(frame.point,origin),frame.normal)/den:-1;
        const bool floor_hit=p.settings[1]!=0 && floor>minimum && floor<model.t && !(escaping && bounce==0 && den>0);
        if(floor_hit) {
            ++result.ground;
            if(material==0 && p.source_colour[3]) {
                const auto water=native_water_expected(input.source,p,frame,origin,direction,floor,x,y);
                if(water.edge){result.edge=true;return result;}
                sum=add(sum,product(throughput,water.light));throughput=scale(throughput,water.fresnel);
                if(*std::max_element(throughput.begin(),throughput.end())<1e-5){result.packed=native_pack(sum,p.material_info[2],255);return result;}
                origin=add(add(origin,scale(direction,floor)),scale(water.normal,water.bias));
                direction=reflected_liquid_oracle::reflect(direction,water.normal);minimum=water.bias;continue;
            }
            if(material==3) {
                const auto lava=native_lava_optical(p,frame,origin,direction,floor);
                sum=add(sum,product(throughput,lava.emission));throughput=scale(throughput,lava.share);
                if(*std::max_element(throughput.begin(),throughput.end())<1e-5){result.packed=native_pack(sum,p.material_info[2],255);return result;}
                origin=add(lava.hit,scale(lava.normal,lava.bias));direction=reflected_liquid_oracle::reflect(direction,lava.normal);minimum=lava.bias;continue;
            }
            V normal=unit(frame.normal);if(dot(normal,direction)>0)normal=scale(normal,-1);
            if(material==2) {const double grazing=std::pow(1-std::clamp(dot(scale(direction,-1),normal),0.,1.),5);
                throughput=product(throughput,add(V{1,.766,.336},scale(V{0,.234,.664},grazing)));}
            minimum=std::max(.05,floor*1e-5);origin=add(add(origin,scale(direction,floor)),scale(normal,minimum));
            direction=reflected_liquid_oracle::reflect(direction,normal);continue;
        }
        if(model.found) {
            ++result.models;
            if(p.primary_range[2]) {
                const double depth=add(origin,scale(direction,model.t))[2];
                result.models_outside_primary+=depth<p.primary_range[0] || depth>p.primary_range[1];
            }
            if((unsigned(p.water[3])&16)==0 && !p.material_info[3]) {
                if(bounce==0) {
                    result.secondary=model.primitive;
                    const auto& vertices=input.source.source.geometry.vertices;const unsigned first=model.primitive*3;
                    const auto at=[&](unsigned c){const auto& v=vertices[first+c];return V{v[0],v[1],v[2]};};
                    const V a=at(0),e1=sub(at(1),a),e2=sub(at(2),a),q=cross(direction,e2),s=sub(origin,a),r=cross(s,e1);
                    const double d=dot(e1,q);result.bary={dot(s,q)/d,dot(direction,r)/d};
                }
                result.packed=terminal(model.colour);return result;
            }
            const V normal=cube_model_normal(input,model.primitive,direction);minimum=std::max(.05,model.t*1e-5);
            if(p.material_info[3] && p.dimensions[3]) {
                const V f0=native_model_f0(model.colour,p.dimensions[3],p.material_info[2]);
                const double grazing=std::pow(1-std::clamp(dot(scale(direction,-1),normal),0.,1.),5);
                throughput=product(throughput,add(f0,scale(sub(V{1,1,1},f0),grazing)));
            }
            origin=add(add(origin,scale(direction,model.t)),scale(normal,minimum));direction=reflected_liquid_oracle::reflect(direction,normal);continue;
        }
        break;
    }
    CubeWitness witness;const unsigned sky=p.cube_info[3]?cube_packed_sample(input,p,direction,&witness):p.environment[0];
    result.seams+=witness.seam;result.packed=terminal(sky);return result;
}
template<class Dispatch>void run_native_ground(Dispatch dispatch) {
    unsigned submissions=0,checked=0,clear=0,edges=0,max_error=0,ground_hops=0,model_hops=0,seams=0,
        primary_ground=0,primary_models=0,shadowed=0,unshadowed=0,zero_strength=0,format_witnesses=0;
    std::array<unsigned,3> materials{};
    for(unsigned material:{1U,2U,3U})for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})
    for(unsigned transform=0;transform<6;++transform)for(float time:{1.3f,4.7f})for(float strength:{0.f,.7f,1.f})for(unsigned mode=0;mode<2;++mode) {
        auto input=cube_input(16,variant,encoding);auto p=cube_parameters(input,encoding,0,transform?6:0);
        p.camera={46,39,mode?33.4f:30.6f,22.7f};p.settings={0,1,0,float(mode==0)};
        p.environment[0]=time<2?0xffa035e0:0xff29b50d;
        if(transform==0){p.cube_info={};input.cube.clear();input.size=0;}
        const float angle=transform==1?.13f:transform==2?-.13f:0;
        p.point={0,180,0,0};p.normal={std::sin(angle),-std::cos(angle),0,0};
        p.row0={std::cos(angle),-std::sin(angle),0,29};p.row1={std::sin(angle),std::cos(angle),0,-3};p.row2={0,0,1,17};
        p.water={time,strength,time<2?.72f:0.f,float(material|((mode || transform==2)?16:0))};
        reflected_liquid_oracle::Frame frame;frame.point={0,180,0};frame.normal={p.normal[0],p.normal[1],0};
        frame.rotation={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};frame.offset={29,-3,17};
        if(transform==3 || transform==4) {
            // Native cartridge/model views need not be exactly orthonormal.
            // Derive the eye plane from the same affine source-world floor.
            constexpr std::array<std::array<float,9>,2> affine{{{.984f,.1f,.02f,-.12f,.99f,.07f,.015f,-.06f,1.012f},
                {1.1f,.13f,-.03f,-.04f,.91f,.11f,.02f,-.1f,1.06f}}};const auto& r=affine[transform-3];
            p.row0={r[0],r[1],r[2],29};p.row1={r[3],r[4],r[5],-3};p.row2={r[6],r[7],r[8],17};
            std::copy(r.begin(),r.end(),frame.rotation.begin());
            const V point=native_forward(sub(V{0,180,0},frame.offset),frame),normal=unit(reflected_liquid_oracle::col({0,-1,0},frame));
            p.point={float(point[0]),float(point[1]),float(point[2]),0};p.normal={float(normal[0]),float(normal[1]),float(normal[2]),0};
            frame.point={p.point[0],p.point[1],p.point[2]};frame.normal={p.normal[0],p.normal[1],p.normal[2]};
        }
        if(transform==5) {
            // An independent .001 basis is valid despite a small determinant.
            // Keep the floor visible while changing source-world units.
            p.row0={.001f,0,0,0};p.row1={0,.001f,0,0};p.row2={0,0,.001f,0};
            p.point={0,20,0,0};p.normal={0,-1,0,0};frame.rotation={.001f,0,0,0,.001f,0,0,0,.001f};
            frame.point={0,20,0};frame.normal={0,-1,0};frame.offset={};
        }
        const auto actual=dispatch(p,input);++submissions;require(actual.size()==width*height,"Native ground image extent");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1},incoming=unit(raw);
            const double den=dot(raw,frame.normal),depth=std::abs(den)>1e-10?dot(frame.point,frame.normal)/den:65536;
            const bool floor=depth>1 && depth<65536;const auto primary=mode?trace_native_colour(input.source,{},raw,1,x,y):IndexedHit{};
            if(primary.edge){++edges;continue;}
            const bool ground=floor && (!primary.found || depth<primary.t);
            if(!ground && !primary.found){require(actual[pixel]==0,"Native ground painted outside its physical plane");++clear;continue;}
            V radiance{};NativeGroundPath path;bool boundary=false;unsigned marker=ground?254:255;
            if(ground) {
                ++primary_ground;const double distance=depth*std::sqrt(dot(raw,raw));V hit=scale(raw,depth),normal=unit(frame.normal);
                if(dot(normal,incoming)>0)normal=scale(normal,-1);const double bias=std::max(.05,distance*1e-5);
                if(material==3) {
                    const auto lava=native_lava_optical(p,frame,{},incoming,distance);radiance=lava.emission;
                    if(lava.share>0){path=native_ground_path(input,p,frame,add(lava.hit,scale(lava.normal,bias)),reflected_liquid_oracle::reflect(incoming,lava.normal),bias,x,y,true);
                        radiance=add(radiance,scale(native_linear(path.packed,encoding),lava.share));}
                } else {
                    // Canonical native planar transport: authored fallback
                    // colour, shadowed diffuse base, linear Fresnel and highlight.
                    const V authored=native_bytes(p.environment[0]),tint=material==2?V{1,.875,.58}:V{.8,.82,.85};
                    const V light=unit(reflected_liquid_oracle::col({-1,-1,-1},frame));
                    const auto occluder=trace_native_colour(input.source,add(hit,scale(normal,bias)),light,bias,x,y);
                    boundary=occluder.edge;const double visible=occluder.found?0:1;shadowed+=occluder.found;unshadowed+=!occluder.found;
                    radiance=scale(native_authored_linear(scale(tint,dot(authored,{.3,.59,.11})),encoding),.65+.35*visible*std::max(0.,dot(normal,light)));
                    if(strength>0){path=native_ground_path(input,p,frame,add(hit,scale(normal,bias)),reflected_liquid_oracle::reflect(incoming,normal),bias,x,y,true);
                        const double grazing=std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5);
                        const V f0=material==2?V{1,.766,.336}:V{1,1,1};
                        radiance=add(scale(radiance,1-strength),scale(product(native_linear(path.packed,encoding),add(f0,scale(sub(V{1,1,1},f0),grazing))),strength));}
                    const double specular=std::pow(std::max(0.,dot(normal,unit(sub(light,incoming)))),96)*visible;
                    radiance=add(radiance,scale(V{1,.95,.82},specular*std::max({authored[0],authored[1],authored[2]})*.55));
                }
            } else {
                ++primary_models;const V hit=scale(raw,primary.t),normal=cube_model_normal(input,primary.primitive,raw);
                const double bias=std::max(.01,primary.t*std::sqrt(dot(raw,raw))*1e-5);
                path=native_ground_path(input,p,frame,add(hit,scale(normal,bias)),reflected_liquid_oracle::reflect(incoming,normal),bias,x,y,false);
                // The sharp non-conductor fast path preserves packed transport.
                radiance=native_linear(path.packed,encoding);
            }
            if(boundary || path.edge){++edges;continue;}
            ground_hops+=path.ground;model_hops+=path.models;seams+=path.seams;
            const unsigned expected=native_pack(radiance,encoding,marker);require(actual[pixel]>>24==marker,"Native ground/model ownership marker mismatch");
            for(unsigned c=0;c<3;++c){const unsigned error=std::abs(int((actual[pixel]>>(8*c))&255)-int((expected>>(8*c))&255));max_error=std::max(max_error,error);
                if(error>1)throw std::runtime_error("Native ground RGB mismatch material="+std::to_string(material)+" encoding="+std::to_string(encoding)+" transform="+std::to_string(transform)+" variant="+std::to_string(variant)+" strength="+std::to_string(strength)+" time="+std::to_string(time)+" mode="+std::to_string(mode)+" pixel="+std::to_string(pixel)+" actual/reference="+std::to_string(actual[pixel])+"/"+std::to_string(expected));}
            ++checked;++materials[material-1];zero_strength+=ground && strength==0;format_witnesses+=encoding==2 && expected!=native_pack(radiance,1,marker);
        }
    }
    std::cout<<"Native calibrated mirror/gold/lava: "<<submissions<<" submissions, checked="<<checked<<" clear="<<clear<<" primary ground/models="<<primary_ground<<'/'<<primary_models
        <<" secondary ground/model hops="<<ground_hops<<'/'<<model_hops<<" seam witnesses="<<seams<<" shadowed/unshadowed="<<shadowed<<'/'<<unshadowed
        <<" zero-reflection floor="<<zero_strength<<" sRGB witnesses="<<format_witnesses<<" boundary exclusions="<<edges<<" max RGB error="<<max_error
        <<"; binary64 input geometry/coverage/waves/linear transport, shared artistic lava colour recipe, exact ownership254/255 and no-ground0\n"<<std::flush;
    require(checked>1000000 && clear>100000 && primary_models>10000 && ground_hops>10000 && model_hops>1000 && seams>100 && shadowed>100 && unshadowed>10000 && zero_strength>10000 && format_witnesses>10000,"Insufficient native ground transport witnesses");
    for(auto count:materials)require(count>100000,"Missing native ground material");
}
}
