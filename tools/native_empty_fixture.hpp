#pragma once
#include "native_ground_fixture.hpp"

namespace native_reflection_fixture {
inline NativeCubeInput native_empty_input(unsigned encoding,bool cube) {
    auto input=cube_input(16,0,encoding);input.source.empty_geometry=true;
    // Match the calibrated producer's padded header-only material allocation.
    input.source.source.words={0xdeadbeefU,0xa5a5a5a5U,0x7f800000U,0xffffffffU};
    if(!cube){input.cube.clear();input.size=0;}return input;
}
inline V native_empty_planar(const NativeCubeInput& input,const Parameters& p,
    const reflected_liquid_oracle::Frame& frame,V incoming,double distance,unsigned x,unsigned y) {
    const unsigned material=unsigned(p.water[3])&15,encoding=p.material_info[2];
    const V hit=scale(incoming,distance);V normal=unit(frame.normal);
    if(dot(normal,incoming)>0)normal=scale(normal,-1);
    const double bias=std::max(.05,distance*1e-5);
    if(material==3) {
        const auto lava=native_lava_optical(p,frame,{},incoming,distance);V radiance=lava.emission;
        if(lava.share>0) {
            const auto path=native_ground_path(input,p,frame,add(lava.hit,scale(lava.normal,bias)),
                reflected_liquid_oracle::reflect(incoming,lava.normal),bias,x,y,true);
            require(!path.edge,"Empty lava invented a source boundary");radiance=add(radiance,scale(native_linear(path.packed,encoding),lava.share));
        }return radiance;
    }
    const V authored=native_bytes(p.environment[0]),tint=material==2?V{1,.875,.58}:V{.8,.82,.85};
    const V light=unit(reflected_liquid_oracle::col({-1,-1,-1},frame));
    V radiance=scale(native_authored_linear(scale(tint,dot(authored,{.3,.59,.11})),encoding),.65+.35*std::max(0.,dot(normal,light)));
    const double strength=p.water[1];
    if(strength>0) {
        const auto path=native_ground_path(input,p,frame,add(hit,scale(normal,bias)),reflected_liquid_oracle::reflect(incoming,normal),bias,x,y,true);
        require(!path.edge,"Empty planar ground invented a source boundary");
        const double grazing=std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5);
        const V f0=material==2?V{1,.766,.336}:V{1,1,1};
        radiance=add(scale(radiance,1-strength),scale(product(native_linear(path.packed,encoding),add(f0,scale(sub(V{1,1,1},f0),grazing))),strength));
    }
    const double highlight=std::pow(std::max(0.,dot(normal,unit(sub(light,incoming)))),96);
    return add(radiance,scale(V{1,.95,.82},highlight*std::max({authored[0],authored[1],authored[2]})*.55));
}
template<class Dispatch>void run_native_empty(Dispatch dispatch) {
    unsigned submissions=0,checked=0,clear=0,water_pixels=0,planar_pixels=0,lava_pixels=0,
        hidden_pixels=0,compact_pixels=0,clipped_pixels=0,max_error=0,zero_strength=0;
    double max_depth=0,max_normal=0;
    for(unsigned encoding:{1U,2U})for(unsigned cube=0;cube<2;++cube)for(unsigned kind=0;kind<5;++kind)
    for(unsigned transform=0;transform<3;++transform)for(float time:{1.3f,4.7f})for(float strength:{0.f,1.f})
    for(unsigned range=0;range<2;++range)for(unsigned layer=0;layer<(kind==0?3U:1U);++layer) {
        const auto input=native_empty_input(encoding,cube);auto p=cube_parameters(input,encoding,1,6);
        p.camera={46,39,31.3f,22.7f};p.dimensions={width,height,3,2};p.material_info[3]=1;
        if(!cube)p.cube_info={};
        p.environment[0]=time<2?0xffb07841:0xff29b50d;
        p.settings={.85f,float(kind!=4),0,0};p.water={time,strength,time<2?.85f:0.f,float(kind==4?0:kind)};
        if(kind==0)p.source_colour={.2f,.3f,.4f,1};
        reflected_liquid_oracle::Frame frame;frame.rotation={1,0,0,0,1,0,0,0,1};frame.offset={29,-3,17};
        if(transform==1)frame.rotation={.984f,.1f,.02f,-.12f,.99f,.07f,.015f,-.06f,1.012f};
        if(transform==2)frame.rotation={std::cos(.13f),-std::sin(.13f),0,std::sin(.13f),std::cos(.13f),0,0,0,1};
        const V plane=native_forward(sub(V{0,200,0},frame.offset),frame),normal=unit(reflected_liquid_oracle::col({0,-1,0},frame));
        p.point={float(plane[0]),float(plane[1]),float(plane[2]),0};p.normal={float(normal[0]),float(normal[1]),float(normal[2]),0};
        frame.point={p.point[0],p.point[1],p.point[2]};frame.normal={p.normal[0],p.normal[1],p.normal[2]};
        p.row0={float(frame.rotation[0]),float(frame.rotation[1]),float(frame.rotation[2]),29};
        p.row1={float(frame.rotation[3]),float(frame.rotation[4]),float(frame.rotation[5]),-3};
        p.row2={float(frame.rotation[6]),float(frame.rotation[7]),float(frame.rotation[8]),17};
        if(range)p.primary_range={350,2000,1,0};
        if(layer){const auto layout=*starfox::render::shadows::native_water_layers(width,height,layer==2);
            p.liquid_layers={layout.world_offset/4,layout.surface_offset/4,layer==2?3U:2U,layout.storage_bytes/4};}
        const auto actual=dispatch(p,input);++submissions;
        require(actual.size()==(layer?p.liquid_layers[3]:width*height),"Empty native image/layer extent mismatch");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1},incoming=unit(raw);
            const double den=dot(raw,frame.normal),depth=std::abs(den)>1e-10?dot(frame.point,frame.normal)/den:65536;
            const bool legacy_floor=kind!=4 && depth>1 && depth<65536;
            const double near=range?350:1,far=range?2000:65536;
            const bool floor=kind!=4 && depth>near && depth<far;clipped_pixels+=legacy_floor && !floor;
            if(!floor) {
                require(actual[pixel]==0,"Empty native scene retained a model/ground pixel");++clear;
                if(layer){if(layer==2)require(actual[p.liquid_layers[0]+pixel]==0,"Empty dry world retained water");
                    for(unsigned c=0;c<4;++c)require(actual[p.liquid_layers[1]+pixel*4+c]==0,"Empty dry surface retained guides");}
                continue;
            }
            V radiance{};unsigned marker=254;
            if(kind==0) {
                const auto water=native_water_expected(input.source,p,frame,{},incoming,depth*std::sqrt(dot(raw,raw)),x,y);
                require(!water.edge,"Empty water invented a source boundary");
                const auto path=native_ground_path(input,p,frame,add(scale(raw,depth),scale(water.normal,water.bias)),
                    reflected_liquid_oracle::reflect(incoming,water.normal),water.bias,x,y,true);
                require(!path.edge,"Empty reflected water invented a source boundary");
                radiance=add(water.light,scale(native_linear(path.packed,encoding),water.fresnel));marker=253;++water_pixels;
                if(layer) {
                    if(layer==2){require(actual[pixel]==actual[p.liquid_layers[0]+pixel],"Empty primary and hidden water differ");++hidden_pixels;}else ++compact_pixels;
                    const unsigned at=p.liquid_layers[1]+pixel*4;V actual_normal{};
                    for(unsigned c=0;c<3;++c)actual_normal[c]=std::bit_cast<float>(actual[at+c]);
                    const double normal_error=std::sqrt(dot(sub(actual_normal,water.normal),sub(actual_normal,water.normal))),depth_error=std::abs(std::bit_cast<float>(actual[at+3])-depth);
                    max_normal=std::max(max_normal,normal_error);max_depth=std::max(max_depth,depth_error);
                    require(normal_error<2e-5 && depth_error<std::max(.01,depth*2e-5),"Empty water normal/forward-depth mismatch");
                }
            } else {radiance=native_empty_planar(input,p,frame,incoming,depth*std::sqrt(dot(raw,raw)),x,y);planar_pixels+=kind!=3;lava_pixels+=kind==3;}
            const unsigned expected=native_pack(radiance,encoding,marker);require(actual[pixel]>>24==marker,"Empty surface ownership changed");
            for(unsigned c=0;c<3;++c){const unsigned error=std::abs(int((actual[pixel]>>(8*c))&255)-int((expected>>(8*c))&255));max_error=std::max(max_error,error);
                if(error>1)throw std::runtime_error("Empty native RGB mismatch encoding="+std::to_string(encoding)+" cube="+std::to_string(cube)+" kind="+std::to_string(kind)+" transform="+std::to_string(transform)+" time="+std::to_string(time)+" strength="+std::to_string(strength)+" range="+std::to_string(range)+" layer="+std::to_string(layer)+" pixel="+std::to_string(pixel)+" actual/reference="+std::to_string(actual[pixel])+"/"+std::to_string(expected));}
            ++checked;zero_strength+=strength==0;
        }
    }
    std::cout<<"Native empty scenes: "<<submissions<<" submissions, checked="<<checked<<" exact clear="<<clear<<" water/planar/lava="<<water_pixels<<'/'<<planar_pixels<<'/'<<lava_pixels<<" hidden/compact water="<<hidden_pixels<<'/'<<compact_pixels<<" clipped="<<clipped_pixels<<" zero strength="<<zero_strength<<" max RGB error="<<max_error<<" max normal/depth error="<<max_normal<<'/'<<max_depth<<"; zero source triangles/zero TLAS instances, binary64 input plane/optics, no fake casters, exact ownership253/254/0\n"<<std::flush;
    require(checked>300000 && clear>300000 && water_pixels>100000 && planar_pixels>100000 && lava_pixels>50000 && hidden_pixels>10000 && compact_pixels>10000 && clipped_pixels>100000 && zero_strength>100000,"Insufficient empty native witnesses");
}
template<class Dispatch>void run_native_empty_shadows(Dispatch dispatch) {
    unsigned submissions=0,checked=0,opaque=0,native=0,ground=0,sky=0;
    const auto input=native_empty_input(1,false);
    constexpr std::array<std::array<float,4>,4> cameras{{
        {96,85,31.3f,22.7f},{46,39,31.3f,22.7f},{63,57,34.1f,21.9f},{137,111,28.2f,25.1f}}};
    for(unsigned encoding:{0U,2U})for(unsigned samples:{1U,4U,8U,16U})for(const auto& camera:cameras)
    for(unsigned range=0;range<3;++range)for(unsigned receiver=0;receiver<2;++receiver)for(unsigned underlay=0;underlay<(receiver?2U:1U);++underlay) {
        ShadowParameters p{};p.extent={float(width),float(height),camera[0],camera[1]};
        p.center={camera[2],camera[3],float(receiver),float(underlay)};p.point={0,200,0,0};p.normal={0,-1,0,0};
        p.coverage={0,0,encoding,encoding?16U:0U};
        if(range==1)p.primary_range={350,2000,1,0};if(range==2)p.primary_range={0,210,1,0};
        const V direction=unit({.34,-.25,-1});for(unsigned i=0;i<samples;++i)p.lights[i]={float(direction[0]),float(direction[1]),float(direction[2]),0};
        p.lights[0][3]=float(samples);
        const auto actual=dispatch(p,input);++submissions;require(actual.size()==width*height,"Empty shadow extent mismatch");
        for(auto value:actual){require(value==0,"Empty caster scene retained a shadow");++checked;}
        opaque+=encoding==0;native+=encoding==2;ground+=receiver!=0;sky+=receiver==0;
    }
    std::cout<<"Native empty shadows: "<<submissions<<" submissions, exact clear="<<checked<<" opaque/native="<<opaque<<'/'<<native<<" ground/sky="<<ground<<'/'<<sky<<"; real zero-instance TLAS, no-source CPU/native casters, 1/4/8/16 samples, primary/underlay and asymmetric clipped cameras\n"<<std::flush;
    require(checked>500000 && opaque>50 && native>50 && ground>50 && sky>50,"Insufficient empty shadow witnesses");
}
template<class Dispatch>void run_native_empty_transitions(Dispatch dispatch) {
    const auto populated=cube_input(16,3,2),empty=native_empty_input(2,true);
    auto p=cube_parameters(populated,2,0,6),blank=cube_parameters(empty,2,0,6);
    const auto reference=dispatch(p,populated);unsigned painted=0,submissions=1;
    for(auto pixel:reference)painted+=pixel>>24==255;require(painted>100,"Populated transition baseline is empty");
    // More than three requests force every producer slot through both shapes.
    for(unsigned cycle=0;cycle<12;++cycle) {
        const auto erased=dispatch(blank,empty);++submissions;
        for(auto pixel:erased)require(pixel==0,"Empty transition retained a model");
        const auto restored=dispatch(p,populated);++submissions;
        require(restored==reference,"Empty-to-populated transition changed the source reflection");
    }
    std::cout<<"Native empty/populated transitions: "<<submissions<<" submissions, exact baseline="<<painted<<" model pixels, 12 erase/restore cycles across three slots\n"<<std::flush;
}
}
