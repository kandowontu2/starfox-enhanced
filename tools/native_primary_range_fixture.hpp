#pragma once
#include "native_shadow_cutout_fixture.hpp"
#include "native_water_fixture.hpp"

namespace native_reflection_fixture {
// Camera limits are +Z depths. Secondary rays deliberately retain their own
// 65536-unit transport range. Expectations use only source triangles/UVs.
template<bool Shadow,class Dispatch>
void run_primary_ranges(Dispatch dispatch) {
    unsigned submissions=0,checked=0,clear=0,model_pixels=0,ground_pixels=0;
    unsigned secondary_outside=0,depth_not_distance=0,changed=0,edges=0,max_error=0;
    constexpr std::array<std::array<float,2>,5> ranges{{{0,210},{300,510},{450,900},{550,650},{0,1000}}};
    for(float scale_factor:{.001f,1.f,200.f})for(const auto& range:ranges)
        for(unsigned variant=0;variant<2;++variant)for(unsigned eye=0;eye<2;++eye)
        for(unsigned underlay=0;underlay<(Shadow?2U:1U);++underlay) {
        Fixture f=make_fixture();for(auto& vertex:f.vertices)for(unsigned c=0;c<3;++c)vertex[c]*=scale_factor;
        std::array<unsigned char,16> ink{};ink[0]=ink[3]=2;
        for(unsigned i=0;i<4;++i) {
            auto& m=f.materials[i];m.textured=variant;m.offset=0;m.umask=m.vmask=1;
            m.uv=i&1?std::array<float,6>{-.37f,-.29f,8.13f,7.41f,-.37f,7.41f}
                :std::array<float,6>{-.37f,-.29f,8.13f,-.29f,8.13f,7.41f};
        }
        Parameters p{};p.dimensions={width,height,1,0};p.camera={eye?40.f:96.f,eye?32.f:85.f,eye?33.4f:30.6f,22.7f};
        p.settings={0,1,float(ink.size()),float(underlay)};p.point={0,0,700*scale_factor,0};p.normal={.13f,.11f,1,0};
        p.environment[0]=0xff29b50d;p.primary_range={range[0]*scale_factor,range[1]*scale_factor,1,0};
        const V light=unit({.34,-.25,-1});
        ShadowParameters s{};s.extent={float(width),float(height),p.camera[0],p.camera[1]};
        s.center={p.camera[2],p.camera[3],1,float(underlay)};s.point=p.point;s.normal=p.normal;s.primary_range=p.primary_range;
        s.coverage={4,unsigned(ink.size()),1,0};s.lights[0]={float(light[0]),float(light[1]),float(light[2]),1};
        const auto actual=[&]{if constexpr(Shadow)return dispatch(s,f,ink);else return dispatch(p,f,ink);}();
        ++submissions;require(actual.size()==width*height,"Primary range output extent");
        const double near=p.primary_range[0],far=p.primary_range[1];const V plane{p.point[0],p.point[1],p.point[2]},normal{p.normal[0],p.normal[1],p.normal[2]};
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;const V ray{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1};
            const auto primary=trace_indexed(f,ink,{},ray,near,x,y,far);
            const double denominator=dot(ray,normal),floor=std::abs(denominator)>1e-10?dot(plane,normal)/denominator:far;
            const bool ground=floor>near && floor<far && (underlay || !primary.found || floor<primary.t);
            const bool visible=ground || (!underlay && primary.found);const double depth=ground?floor:primary.t;
            bool boundary=!underlay && primary.edge;unsigned expected=0;
            const auto legacy=trace_indexed(f,ink,{},ray,1,x,y);
            if(visible) {
                const V hit=scale(ray,depth);const double bias=std::max(.1,depth*1e-5);
                V direction;
                if constexpr(Shadow)direction={s.lights[0][0],s.lights[0][1],s.lights[0][2]};
                else {
                    V n=normal;
                    if(!ground) {
                        const unsigned at=primary.primitive*3;const auto position=[&](unsigned i){const auto& v=f.vertices[i];return V{v[0],v[1],v[2]};};
                        n=cross(sub(position(at+1),position(at)),sub(position(at+2),position(at)));
                    }
                    n=unit(n);if(dot(n,ray)>0)n=scale(n,-1);
                    direction=reflected_liquid_oracle::reflect(unit(ray),n);
                }
                const auto secondary=trace_indexed(f,ink,hit,direction,bias,x,y);
                boundary|=secondary.edge;bool secondary_model=secondary.found;
                if constexpr(!Shadow) {
                    const double den=dot(direction,normal),t=std::abs(den)>1e-10?dot(sub(plane,hit),normal)/den:65536;
                    secondary_model&=!(t>bias && t<secondary.t);
                    expected=((secondary_model?secondary.colour:p.environment[0])&0xffffff)|((ground?254U:255U)<<24);
                } else expected=secondary_model?160:0;
                if(secondary_model) {
                    const double z=add(hit,scale(direction,secondary.t))[2];secondary_outside+=z<near || z>far;
                }
                depth_not_distance+=depth*std::sqrt(dot(ray,ray))>=far;
            }
            if(boundary){++edges;continue;}
            if constexpr(Shadow) {
                if(actual[pixel]!=expected)throw std::runtime_error("Primary-range shadow mismatch scale="+std::to_string(scale_factor)+" near/far="+std::to_string(near)+"/"+std::to_string(far)+" pixel="+std::to_string(pixel));
            } else {
                require((actual[pixel]>>24)==(expected>>24),"Primary-range reflection ownership mismatch");
                for(unsigned c=0;c<3;++c) {
                    const unsigned error=std::abs(int((actual[pixel]>>(8*c))&255)-int((expected>>(8*c))&255));max_error=std::max(max_error,error);
                    require(error<=1,"Primary-range reflection RGB mismatch");
                }
            }
            ++checked;clear+=!visible;ground_pixels+=ground;model_pixels+=visible && !ground;
            changed+=visible!=legacy.found || (visible && !ground && primary.primitive!=legacy.primitive);
        }
    }
    require(checked>150000 && clear>10000 && model_pixels>10000 && ground_pixels>10000 && secondary_outside>100
        && depth_not_distance>100 && changed>1000,"Insufficient independent primary-range witnesses");
    std::cout<<(Shadow?"Shadow":"Reflection")<<" primary ranges: "<<submissions<<" submissions, checked="<<checked<<" clear="<<clear
        <<" model/ground="<<model_pixels<<'/'<<ground_pixels<<" secondary outside camera range="<<secondary_outside
        <<" depth-not-distance="<<depth_not_distance<<" changed legacy visibility="<<changed<<" boundary exclusions="<<edges<<" max RGB error="<<max_error
        <<"; sub-unit and >65536 depths, binary64 input-only primary/secondary reference\n"<<std::flush;
}

template<class Dispatch>
void run_native_water_ranges(Dispatch dispatch) {
    unsigned submissions=0,checked=0,wet=0,dry=0,clipped=0,revealed=0,edges=0;
    constexpr std::array<std::array<float,2>,5> ranges{{{0,250},{300,600},{450,950},{1000,2000},{0,70000}}};
    for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})for(unsigned underlay=0;underlay<2;++underlay) {
        const auto source=native_reflected_source(variant,starfox::render::Effect::off,100,encoding==2);
        const auto full=*starfox::render::shadows::native_water_layers(width,height);
        Parameters p{};p.dimensions={width,height,1,0};p.camera={46,39,31.3f,22.7f};p.settings={0,1,0,float(underlay)};
        p.environment[0]=0xffb07841;p.material_info={2,unsigned(source.source.words.size()*4),encoding,0};p.source_colour={.2f,.3f,.4f,1};p.water={1.75f,.7f,.85f,96};
        p.row0={1,0,0,0};p.row1={0,1,0,0};p.row2={0,0,1,0};p.point={0,200,0,0};p.normal={0,-1,0,0};
        reflected_liquid_oracle::Frame frame;frame.rotation={1,0,0,0,1,0,0,0,1};frame.point={0,200,0};frame.normal={0,-1,0};
        p.liquid_layers={full.world_offset/4,full.surface_offset/4,3,full.storage_bytes/4};
        const auto baseline=dispatch(p,source);++submissions;
        for(const auto& range:ranges) {
            p.primary_range={range[0],range[1],1,0};const auto actual=dispatch(p,source);++submissions;
            require(actual.size()==baseline.size(),"Clipped water layer extent changed");
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                const unsigned pixel=y*width+x;const V ray{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1};
                const double depth=ray[1]>0?200/ray[1]:0;const bool floor=depth>range[0] && depth<range[1];
                const auto hit=trace_native_colour(source,{},ray,range[0],x,y,range[1]);if(hit.edge){++edges;continue;}
                const bool liquid=floor && (underlay || !hit.found || depth<hit.t);
                const unsigned marker=liquid?253U:!underlay && hit.found?255U:0U;
                require(actual[pixel]>>24==marker,"Clipped native water/model ownership mismatch");
                const unsigned world=full.world_offset/4+pixel,surface=full.surface_offset/4+pixel*4;
                require(actual[world]==(floor?baseline[world]:0U),"Camera clipping changed secondary water transport or painted excluded water");
                if(liquid) {require(actual[pixel]==actual[world],"Clipped water primary/underlay colour differs");++wet;revealed+=baseline[pixel]>>24!=253U;}
                else {++dry;for(unsigned c=0;c<4;++c)require(actual[surface+c]==0,"Excluded/dry water retained surface guides");}
                clipped+=!floor && baseline[world]!=0;
                if(liquid) {
                    const double actual_depth=std::bit_cast<float>(actual[surface+3]);
                    require(std::abs(actual_depth-depth)<std::max(.01,depth*2e-5),"Clipped water guide is radial instead of forward depth");
                    const auto expected=native_water_expected(source,p,frame,{},unit(ray),depth*std::sqrt(dot(ray,ray)),x,y);
                    V normal{};for(unsigned c=0;c<3;++c)normal[c]=std::bit_cast<float>(actual[surface+c]);
                    require(std::sqrt(dot(sub(normal,expected.normal),sub(normal,expected.normal)))<2e-5,"Newly revealed water guide normal changed");
                    for(unsigned c=0;c<4;++c)if(baseline[pixel]>>24==253U)require(actual[surface+c]==baseline[surface+c],"Clipping changed an unchanged water guide");
                }
                ++checked;
            }
        }
    }
    std::cout<<"Native water primary ranges: "<<submissions<<" submissions, checked="<<checked<<" wet/dry="<<wet<<'/'<<dry
        <<" clipped world="<<clipped<<" newly revealed water="<<revealed<<" boundary exclusions="<<edges
        <<"; independent binary64 coverage/depth, exact unclipped secondary transport/layer equivalence\n"<<std::flush;
    require(checked>180000 && wet>10000 && dry>10000 && clipped>10000 && revealed>100,"Insufficient clipped native water witnesses");
}
}
