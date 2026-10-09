#pragma once
#include "native_ground_fixture.hpp"
namespace native_reflection_fixture {
template<class Dispatch>void run_native_models(Dispatch dispatch) {
    unsigned submissions=0,checked=0,clear=0,edges=0,max_error=0,model_hops=0,recursive=0,budget=0,seams=0,clips=0,rough_pixels=0,format_witnesses=0;
    std::array<unsigned,4> conductors{};
    unsigned ranged_primary=0,depth_not_distance=0,secondary_outside=0;
    for(unsigned metallic:{1U,2U,3U,0U})for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})
    for(float roughness:{0.f,.35f,.85f})for(unsigned specular=0;specular<2;++specular)for(unsigned context=0;context<6;++context)for(unsigned view=0;view<2;++view) {
        auto input=cube_input(16,variant,encoding);auto p=cube_parameters(input,encoding,view,6);
        p.dimensions={width,height,view?3U:1U,metallic};p.settings={roughness,0,0,0};p.material_info[3]=specular;
        p.environment[0]=0xffb07841;
        if(context==3)p.primary_range={250,500,1,0};
        if(context==4)p.primary_range={450,800,1,0};
        if(context==0){p.cube_info={};input.cube.clear();input.size=0;}
        if(context==2) {
            // Camera is inside two broad source planes. Actual four-bounce
            // exits must resolve the current sky, never the base model image.
            for(unsigned i=0;i<12;++i)input.source.source.geometry.vertices[i][2]=i<6?400.f:-200.f;
            constexpr std::array<std::array<float,2>,6> rectangle{{{-300,-220},{300,-220},{300,220},{-300,-220},{300,220},{-300,220}}};
            for(unsigned i=0;i<6;++i){auto& vertex=input.source.source.geometry.vertices[i+6];vertex[0]=rectangle[i][0];vertex[1]=rectangle[i][1];}
        }
        if(context==5) {
            // Broad, grazing source faces exercise lobe hemisphere rejection.
            for(unsigned i=0;i<6;++i){auto& vertex=input.source.source.geometry.vertices[i];vertex[0]*=6;vertex[1]*=6;vertex[2]=400+5*vertex[0];}
        }
        const auto actual=dispatch(p,input);++submissions;require(actual.size()==width*height,"Native model image extent");
        reflected_liquid_oracle::Frame frame;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1},incoming=unit(raw);
            const double near=p.primary_range[2]?p.primary_range[0]:1,far=p.primary_range[2]?p.primary_range[1]:65536;
            const auto primary=trace_native_colour(input.source,{},raw,near,x,y,far);
            if(primary.edge){++edges;continue;}
            if(!primary.found){require(actual[pixel]==0,"Native model painted clear primary space");++clear;continue;}
            const V normal=cube_model_normal(input,primary.primitive,raw);const double distance=primary.t*std::sqrt(dot(raw,raw)),bias=std::max(.01,distance*1e-5);
            const V origin=add(scale(raw,primary.t),scale(normal,bias));const auto lobes=native_model_lobes(incoming,normal,roughness);
            V sum{};bool boundary=false;clips+=lobes.hemisphere_clips;
            for(unsigned sample=0;sample<lobes.count;++sample) {
                const auto path=native_ground_path(input,p,frame,origin,lobes.direction[sample],bias,x,y,false);
                if(path.edge){boundary=true;break;}model_hops+=path.models;recursive+=path.models>1;budget+=path.models==4;seams+=path.seams;secondary_outside+=path.models_outside_primary;
                sum=add(sum,scale(native_linear(path.packed,encoding),1./lobes.count));
            }
            if(boundary){++edges;continue;}
            if(metallic) {const V f0=native_model_f0(primary.colour,metallic,encoding);const double grazing=std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5);
                sum=product(sum,add(f0,scale(sub(V{1,1,1},f0),grazing)));}
            const auto expected=native_pack(sum,encoding,255);require(actual[pixel]>>24==255,"Native conductor changed source ownership");
            for(unsigned c=0;c<3;++c){const unsigned error=std::abs(int((actual[pixel]>>(8*c))&255)-int((expected>>(8*c))&255));max_error=std::max(max_error,error);
                if(error>1)throw std::runtime_error("Native model RGB mismatch metallic="+std::to_string(metallic)+" encoding="+std::to_string(encoding)+" variant="+std::to_string(variant)+" roughness="+std::to_string(roughness)+" specular="+std::to_string(specular)+" context="+std::to_string(context)+" view="+std::to_string(view)+" pixel="+std::to_string(pixel)+" actual/reference="+std::to_string(actual[pixel])+"/"+std::to_string(expected));}
            ++checked;++conductors[metallic];rough_pixels+=roughness>0;format_witnesses+=encoding==2 && expected!=native_pack(sum,1,255);
            ranged_primary+=p.primary_range[2]!=0;depth_not_distance+=p.primary_range[2]!=0 && distance>far;
        }
    }
    std::cout<<"Native calibrated models: "<<submissions<<" submissions, checked="<<checked<<" clear="<<clear<<" secondary model hops="<<model_hops<<" recursive="<<recursive<<" four-hop exits="<<budget<<" cube seams="<<seams<<" hemisphere clips="<<clips<<" rough pixels="<<rough_pixels<<" sRGB witnesses="<<format_witnesses<<" ranged primary="<<ranged_primary<<" radial distance past far="<<depth_not_distance<<" secondary models outside primary="<<secondary_outside<<" boundary exclusions="<<edges<<" max RGB error="<<max_error<<"; binary64 input geometry/UV/Fresnel, stable eight-lobe linear-light quadrature, exact opaque255/no-source0\n"<<std::flush;
    require(checked>1000000 && clear>100000 && model_hops>10000 && recursive>1000 && budget>1000 && seams>100 && clips>100 && rough_pixels>100000 && format_witnesses>10000,"Insufficient native model transport witnesses");
    for(auto count:conductors)require(count>100000,"Missing native model conductor");
    require(ranged_primary>10000 && depth_not_distance>100 && secondary_outside>100,"Insufficient native model primary-range witnesses");
}
}
