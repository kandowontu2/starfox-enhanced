#pragma once
#include "native_rgba_reflection_fixture.hpp"
#include "starfox/render/native_water_layers.hpp"
#include "starfox/render/water_caustics.hpp"
#include <functional>

namespace native_reflection_fixture {
inline V native_linear(unsigned packed,unsigned encoding) {
    V value{};for(unsigned c=0;c<3;++c) {
        double v=double((packed>>(c*8))&255)/255;
        value[c]=encoding==2?(v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4)):v;
    }return value;
}
inline unsigned native_pack(V value,unsigned encoding,unsigned marker) {
    unsigned result=marker<<24;for(unsigned c=0;c<3;++c) {
        double v=std::clamp(value[c],0.,1.);
        if(encoding==2)v=v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;
        result|=unsigned(v*255+.5)<<(c*8);
    }return result;
}
inline V native_forward(V value,const reflected_liquid_oracle::Frame& f) {
    const V a{f.rotation[0],f.rotation[1],f.rotation[2]},b{f.rotation[3],f.rotation[4],f.rotation[5]},
        c{f.rotation[6],f.rotation[7],f.rotation[8]};
    const auto x=cross(b,c),y=cross(c,a),z=cross(a,b);const double d=dot(a,x);
    return {dot(x,value)/d,dot(y,value)/d,dot(z,value)/d};
}
struct NativeWaterExpected {V normal{},light{};double fresnel{},bias{},depth{};bool edge{};};
inline NativeWaterExpected native_water_expected(const NativeReflectedSource& source,const Parameters& p,
    const reflected_liquid_oracle::Frame& f,V origin,V direction,double distance,unsigned x,unsigned y) {
    NativeWaterExpected result;const V hit=add(origin,scale(direction,distance)),position=add(reflected_liquid_oracle::row(hit,f),f.offset);
    const double footprint=std::sqrt(dot(hit,hit))/std::max(double(p.camera[0]),1.)/std::max(std::abs(dot(direction,f.normal)),.04),t=p.water[0];
    const double dx=.055*std::cos(position[0]*.018+position[2]*.011-t*.8)*reflected_liquid_oracle::band(footprint,.022)
        +.025*std::cos(position[0]*.047-position[2]*.025+t*1.2)*reflected_liquid_oracle::band(footprint,.054);
    const double dz=.045*std::cos(position[2]*.022-position[0]*.009-t*.65)*reflected_liquid_oracle::band(footprint,.024)
        -.020*std::cos(position[0]*.047-position[2]*.025+t*1.2)*reflected_liquid_oracle::band(footprint,.054);
    V n=unit(reflected_liquid_oracle::col({dx,-1,dz},f));const bool entering=dot(n,direction)<0;
    if(!entering)n=scale(n,-1);result.normal=n;result.depth=hit[2];result.bias=std::max(.05,distance*1e-5);
    const V sun=unit(native_forward({-1,-1,-1},f));
    const auto blocker=trace_native_colour(source,add(hit,scale(n,result.bias)),sun,result.bias,x,y);
    result.edge=blocker.edge;const double visible=blocker.found?0:1;
    V authored{p.source_colour[0],p.source_colour[1],p.source_colour[2]};
    if(p.material_info[2]==2)for(auto& a:authored)a=a<=.04045?a/12.92:std::pow((a+.055)/1.055,2.4);
    const double luminance=dot(authored,{.3,.59,.11});V radiance=scale({.20,.58,.85},luminance*(.65+.35*visible*std::max(0.,dot(n,sun))));
    const double eta=entering?.75:1/.75,dn=dot(n,direction),k=1-eta*eta*(1-dn*dn);
    if(k>=0) {
        const V through=sub(hit,scale(n,result.bias)),transmitted=sub(scale(direction,eta),scale(n,eta*dn+std::sqrt(k)));
        const V world_origin=add(reflected_liquid_oracle::row(through,f),f.offset),world_direction=reflected_liquid_oracle::row(transmitted,f);
        const double bottom=world_direction[1]>0?(position[1]+640-world_origin[1])/world_direction[1]:65536;
        const auto submerged=trace_native_colour(source,through,transmitted,result.bias,x,y,std::min(65536.,std::max(result.bias,bottom)));
        result.edge|=submerged.edge;const double travel=submerged.found?submerged.t:bottom;
        if(submerged.found || (bottom>result.bias && bottom<65536)) {
            V receiver=submerged.found?native_linear(submerged.colour,p.material_info[2]):scale({.28,.24,.16},p.water[2]);
            const V receiver_view=add(through,scale(transmitted,travel)),receiver_world=add(reflected_liquid_oracle::row(receiver_view,f),f.offset);
            const double depth=receiver_world[1]-position[1];double up=1;
            if(submerged.found) {
                const unsigned first=submerged.primitive*3;const auto at=[&](unsigned i){const auto& a=source.source.geometry.vertices[first+i];return V{a[0],a[1],a[2]};};
                V normal=unit(cross(sub(at(1),at(0)),sub(at(2),at(0))));if(dot(normal,transmitted)>0)normal=scale(normal,-1);
                // row normal * inverse transpose == inverse rotation * column normal.
                // Evaluate using basis vectors, not the shader's matrix operation.
                V world_normal{};for(unsigned j=0;j<3;++j) {V axis{};axis[j]=1;world_normal[j]=dot(normal,native_forward(axis,f));}
                up=std::clamp(-unit(world_normal)[1],0.,1.);
            }
            const unsigned quality=(unsigned(p.water[3])>>5)&3;
            if(quality && depth>0 && up>0) {
                // Irradiance uses the shared scalar recipe; visibility, optical
                // geometry and all colour/ownership expectations remain independent.
                const auto focus=starfox::render::water_caustic_sample(float(receiver_world[0]),float(receiver_world[2]),float(t),float(depth),float(footprint));
                const V entry=native_forward(sub({focus.entry_x,position[1],focus.entry_z},f.offset),f),segment=sub(entry,receiver_view);
                const double path=std::sqrt(dot(segment,segment));
                const auto b0=trace_native_colour(source,receiver_view,unit(segment),result.bias,x,y,path-result.bias);
                const auto b1=trace_native_colour(source,entry,unit(native_forward({0,-1,0},f)),result.bias,x,y);
                result.edge|=b0.edge || b1.edge;
                if(path>result.bias*2 && !b0.found && !b1.found)receiver=scale(receiver,std::clamp(1+(focus.irradiance-std::exp(-depth/1600))*up*quality/3,.25,3.));
            }
            constexpr V absorption{.0025,.0008,.00035};for(unsigned c=0;c<3;++c) {
                const double transmission=std::exp(-travel*absorption[c]);radiance[c]=receiver[c]*transmission+radiance[c]*(1-transmission);
            }
        }
    }
    result.fresnel=k<0?1:std::clamp((.02+.98*std::pow(1-std::clamp(dot(scale(direction,-1),n),0.,1.),5))*p.water[1],0.,1.);
    const double highlight=std::pow(std::max(0.,dot(n,unit(sub(sun,direction)))),96)*visible*std::max({authored[0],authored[1],authored[2]})*.55;
    result.light=add(scale(radiance,1-result.fresnel),scale({1,.95,.82},highlight));return result;
}
template<class Dispatch> void run_native_water(Dispatch dispatch) {
    unsigned submissions=0,wet=0,dry=0,hidden=0,submerged=0,excluded=0,max_error=0;
    double max_depth=0,max_normal=0;unsigned changed_formats=0;
    for(unsigned encoding:{1U,2U})for(unsigned transform=0;transform<3;++transform)
    for(unsigned variant:{0U,3U,4U})for(float strength:{0.f,.7f,1.f})for(float time:{1.75f,9.125f})for(unsigned caustics:{0U,3U}) {
        auto source=native_reflected_source(variant,starfox::render::Effect::off,100,encoding==2);
        Parameters p{};p.dimensions={width,height,1,0};p.camera={transform?46.f:96.f,transform?39.f:85.f,31.3f,22.7f};
        p.settings={0,1,0,0};p.environment[0]=0xffb07841;
        p.material_info={2,unsigned(source.source.words.size()*4),encoding,0};p.source_colour={.2f,.3f,.4f,1};p.water={time,strength,.85f,float(caustics<<5)};
        reflected_liquid_oracle::Frame f;f.rotation=transform==0?std::array<double,9>{1,0,0,0,1,0,0,0,1}
            :transform==1?std::array<double,9>{.984,.1,.02,-.12,.99,.07,.015,-.06,1.012}
            :std::array<double,9>{1.1,.13,-.03,-.04,.91,.11,.02,-.1,1.06};
        f.offset=transform?V{175,0,-280}:V{};
        p.row0={float(f.rotation[0]),float(f.rotation[1]),float(f.rotation[2]),float(f.offset[0])};
        p.row1={float(f.rotation[3]),float(f.rotation[4]),float(f.rotation[5]),float(f.offset[1])};
        p.row2={float(f.rotation[6]),float(f.rotation[7]),float(f.rotation[8]),float(f.offset[2])};
        for(unsigned i=0;i<9;++i)f.rotation[i]=i<3?p.row0[i]:i<6?p.row1[i-3]:p.row2[i-6];
        f.normal=reflected_liquid_oracle::col({0,-1,0},f);f.point=native_forward(sub({0,20,0},f.offset),f);
        p.point={float(f.point[0]),float(f.point[1]),float(f.point[2]),0};p.normal={float(f.normal[0]),float(f.normal[1]),float(f.normal[2]),0};
        f.point={p.point[0],p.point[1],p.point[2]};f.normal={p.normal[0],p.normal[1],p.normal[2]};
        const auto ordinary=dispatch(p,source);++submissions;require(ordinary.size()>=width*height,"Native water image extent");
        const auto full=*starfox::render::shadows::native_water_layers(width,height);
        p.liquid_layers={full.world_offset/4,full.surface_offset/4,3,full.storage_bytes/4};
        const auto layers=dispatch(p,source);++submissions;require(layers.size()==full.storage_bytes/4,"Native water layered extent");
        require(std::equal(ordinary.begin(),ordinary.begin()+width*height,layers.begin()),"Water auxiliary layers changed primary colour");
        const auto compact=*starfox::render::shadows::native_water_layers(width,height,false);
        p.liquid_layers={0,compact.surface_offset/4,2,compact.storage_bytes/4};const auto surface_only=dispatch(p,source);++submissions;
        require(surface_only.size()==compact.storage_bytes/4 && std::equal(ordinary.begin(),ordinary.begin()+width*height,surface_only.begin()),"Water surface-only changed primary colour");
        require(std::equal(surface_only.begin()+compact.surface_offset/4,surface_only.end(),layers.begin()+full.surface_offset/4),"Water compact/full guides differ");
        p.liquid_layers={};p.settings[3]=1;const auto world_only=dispatch(p,source);++submissions;
        require(std::equal(world_only.begin(),world_only.begin()+width*height,layers.begin()+full.world_offset/4),"Hidden water differs from actual world-only secondary transport");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1},direction=unit(raw);
            const double den=dot(raw,f.normal),depth=std::abs(den)>1e-10?dot(f.point,f.normal)/den:0;
            const bool floor=depth>1 && depth<65536;const auto model=trace_native_colour(source,{},raw,1,x,y);
            const bool liquid=floor && (!model.found || depth<model.t);
            const unsigned surface_at=full.surface_offset/4+pixel*4;V guide{};for(unsigned c=0;c<3;++c)guide[c]=std::bit_cast<float>(layers[surface_at+c]);
            const double guide_depth=std::bit_cast<float>(layers[surface_at+3]);
            if(!liquid) {require(guide==V{} && guide_depth==0,"Dry primary inherited water guides");++dry;}
            if(model.edge){++excluded;continue;}
            require((ordinary[pixel]>>24)==(liquid?253U:model.found?255U:0U),"Native water nearer ownership marker changed");
            require((world_only[pixel]>>24)==(floor?253U:0U),"Hidden water plane lost physical coverage");
            if(!floor)continue;
            const auto expected=native_water_expected(source,p,f,{},direction,depth*std::sqrt(dot(raw,raw)),x,y);
            if(expected.edge){++excluded;continue;}
            V radiance=expected.light;
            if(expected.fresnel>0) {
                const V origin=add(scale(raw,depth),scale(expected.normal,expected.bias)),reflected=reflected_liquid_oracle::reflect(direction,expected.normal);
                const auto reflection=trace_native_colour(source,origin,reflected,expected.bias,x,y);
                if(reflection.edge){++excluded;continue;}
                radiance=add(radiance,scale(native_linear(reflection.found?reflection.colour:p.environment[0],encoding),expected.fresnel));
            }
            const auto packed=native_pack(radiance,encoding,253);
            for(unsigned c=0;c<3;++c) {
                const int actual=(world_only[pixel]>>(c*8))&255,reference=(packed>>(c*8))&255;const unsigned error=std::abs(actual-reference);
                max_error=std::max(max_error,error);
                if(error>1)throw std::runtime_error("Native water RGB mismatch format="+std::to_string(encoding)+" transform="+std::to_string(transform)+" variant="+std::to_string(variant)+" strength="+std::to_string(strength)+" time="+std::to_string(time)+" caustics="+std::to_string(caustics)+" pixel="+std::to_string(pixel)+" actual/reference="+std::to_string(actual)+"/"+std::to_string(reference));
            }
            if(liquid) {
                ++wet;submerged+=model.found;
                const double depth_error=std::abs(guide_depth-depth),normal_error=std::sqrt(dot(sub(guide,expected.normal),sub(guide,expected.normal)));
                max_depth=std::max(max_depth,depth_error);max_normal=std::max(max_normal,normal_error);
                if(!std::isfinite(guide_depth) || depth_error>=std::max(.01,depth*2e-5) || normal_error>=2e-5)
                    throw std::runtime_error("Native water optical mismatch format="+std::to_string(encoding)+" transform="+std::to_string(transform)+" variant="+std::to_string(variant)
                        +" strength="+std::to_string(strength)+" time="+std::to_string(time)+" caustics="+std::to_string(caustics)+" pixel="+std::to_string(pixel)
                        +" depth="+std::to_string(guide_depth)+" expected="+std::to_string(depth)+" error="+std::to_string(depth_error)+" normal error="+std::to_string(normal_error));
            } else ++hidden;
            if(encoding==2 && packed!=native_pack(radiance,1,253))++changed_formats;
        }
    }
    require(wet>100000 && dry>100000 && hidden>1000 && submerged>1000 && changed_formats>1000,"Insufficient water ownership/format coverage");
    std::cout<<"Native water: "<<submissions<<" submissions, wet="<<wet<<" dry="<<dry<<" hidden="<<hidden<<" submerged="<<submerged<<" boundary exclusions="<<excluded<<" max RGB error="<<max_error<<" max depth error="<<max_depth<<" max normal error="<<max_normal<<"; independent binary64 rays/normal/linear transport, shared caustic irradiance recipe; primary/full/compact/underlay equivalence\n";
}
}
