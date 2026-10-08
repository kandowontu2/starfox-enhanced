#pragma once
#include "native_water_fixture.hpp"

namespace native_reflection_fixture {
struct NativeCubeInput {NativeReflectedSource source;std::vector<unsigned> cube;unsigned size{};};
struct CubeWitness {unsigned face{};bool seam{},corner{};};
// Binary64 geometric face projection uses face normal/U/V bases. It does not
// call or translate the shader's largest-component coordinate implementation.
inline constexpr std::array<V,6> cube_normals{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
inline constexpr std::array<V,6> cube_u{{{0,0,-1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{-1,0,0}}};
inline constexpr std::array<V,6> cube_v{{{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}}};
inline std::pair<unsigned,std::array<double,2>> cube_project(V ray) {
    unsigned face=0;double largest=-1;
    for(unsigned f=0;f<6;++f) {const double d=dot(ray,cube_normals[f]);if(d>largest){largest=d;face=f;}}
    const V plane=scale(ray,1/largest);return {face,{dot(plane,cube_u[face]),dot(plane,cube_v[face])}};
}
inline V cube_linear_sample(const NativeCubeInput& input,const Parameters& p,V ray,CubeWitness* witness=nullptr) {
    const V turned{dot(ray,{p.cube_row0[0],p.cube_row0[1],p.cube_row0[2]}),
        dot(ray,{p.cube_row1[0],p.cube_row1[1],p.cube_row1[2]}),dot(ray,{p.cube_row2[0],p.cube_row2[1],p.cube_row2[2]})};
    const auto [face,uv]=cube_project(turned);const double xx=(uv[0]+1)*input.size*.5-.5,yy=(uv[1]+1)*input.size*.5-.5;
    const int lx=int(std::floor(xx)),ly=int(std::floor(yy));const double fx=xx-lx,fy=yy-ly;
    if(witness){witness->face=face;witness->seam=lx<0 || ly<0 || lx+1>=int(input.size) || ly+1>=int(input.size);
        witness->corner=(lx<0 || lx+1>=int(input.size)) && (ly<0 || ly+1>=int(input.size));}
    V result{};
    for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
        int tx=lx+x,ty=ly+y;unsigned neighbour=face;
        if(tx<0 || ty<0 || tx>=int(input.size) || ty>=int(input.size)) {
            const double u=(tx+.5)*2/input.size-1,v=(ty+.5)*2/input.size-1;
            const auto mapped=cube_project(add(cube_normals[face],add(scale(cube_u[face],u),scale(cube_v[face],v))));
            neighbour=mapped.first;tx=int(std::floor((mapped.second[0]+1)*input.size*.5));ty=int(std::floor((mapped.second[1]+1)*input.size*.5));
        }
        tx=std::clamp(tx,0,int(input.size)-1);ty=std::clamp(ty,0,int(input.size)-1);
        const unsigned packed=input.cube[neighbour*input.size*input.size+unsigned(ty)*input.size+unsigned(tx)];
        const V colour=native_linear(packed,p.material_info[2]);
        result=add(result,scale(colour,(x?fx:1-fx)*(y?fy:1-fy)));
    }
    return result;
}
inline unsigned cube_packed_sample(const NativeCubeInput& input,const Parameters& p,V ray,CubeWitness* witness=nullptr) {
    return native_pack(cube_linear_sample(input,p,ray,witness),p.material_info[2],255);
}
inline NativeCubeInput cube_input(unsigned size,unsigned variant,unsigned encoding) {
    NativeCubeInput input{native_reflected_source(variant,starfox::render::Effect::off,100,encoding==2),{},size};
    input.cube.resize(size*size*6);
    for(unsigned face=0;face<6;++face)for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x) {
        const unsigned r=24+(face*29+x*71+y*17)%208,g=19+(face*53+x*13+y*61)%213,b=21+(face*37+x*43+y*23)%211;
        const unsigned alpha=(x+y+face)%3==0?0:(x+y+face)%3==1?254:255;
        input.cube[face*size*size+y*size+x]=r|(g<<8)|(b<<16)|(alpha<<24);
    }
    return input;
}
inline Parameters cube_parameters(const NativeCubeInput& input,unsigned encoding,unsigned view,unsigned rotation) {
    constexpr float c=.7071067811865475f;
    constexpr std::array<std::array<float,9>,7> rotations{{{1,0,0,0,1,0,0,0,1},{0,0,1,0,1,0,-1,0,0},
        {0,0,-1,0,1,0,1,0,0},{-1,0,0,0,1,0,0,0,-1},{1,0,0,0,0,1,0,-1,0},{1,0,0,0,0,-1,0,1,0},{c,0,c,.5f,c,-.5f,-.5f,c,.5f}}};
    Parameters p{};p.dimensions={width,height,1,0};p.camera={view?13.f:96.f,view?11.f:85.f,31.3f,22.7f};
    p.environment[0]=0xffff00ff;p.material_info={2,unsigned(input.source.source.words.size()*4),encoding,0};
    p.cube_info={p.material_info[1]/4,input.size,encoding,1};const auto& r=rotations[rotation];
    p.cube_row0={r[0],r[1],r[2],0};p.cube_row1={r[3],r[4],r[5],0};p.cube_row2={r[6],r[7],r[8],0};return p;
}
inline float cube_hash(unsigned n) {n=(n^61U)^(n>>16U);n*=9U;n^=n>>4U;n*=0x27d4eb2dU;n^=n>>15U;return float(n&65535U)/65535.f;}
inline V cube_model_normal(const NativeCubeInput& input,unsigned primitive,V ray) {
    const auto at=[&](unsigned i){const auto& v=input.source.source.geometry.vertices[primitive*3+i];return V{v[0],v[1],v[2]};};
    V normal=unit(cross(sub(at(1),at(0)),sub(at(2),at(0))));return dot(normal,ray)>0?scale(normal,-1):normal;
}
struct NativeModelLobes {unsigned count{};std::array<V,8> direction{};unsigned hemisphere_clips{};};
inline NativeModelLobes native_model_lobes(V incoming,V normal,double roughness) {
    const V central=reflected_liquid_oracle::reflect(unit(incoming),normal);
    NativeModelLobes result;result.count=roughness>0?8:1;result.direction[0]=central;
    if(!roughness)return result;
    const V tangent=unit(cross(central,std::abs(central[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(central,tangent);
    constexpr std::array<std::array<double,2>,8> taps{{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}}};
    for(unsigned sample=0;sample<8;++sample) {
        V direction=unit(add(central,scale(add(scale(tangent,taps[sample][0]),scale(bitangent,taps[sample][1])),roughness*roughness)));
        if(dot(direction,normal)<=0){direction=central;++result.hemisphere_clips;}result.direction[sample]=direction;
    }return result;
}
inline V native_model_f0(unsigned source,unsigned metallic,unsigned encoding) {
    return metallic==2?V{1,.766,.336}:metallic==3?V{.955,.638,.538}:native_linear(source,encoding);
}
template<class Dispatch>void run_native_environment(Dispatch dispatch) {
    unsigned submissions=0,checked=0,clear=0,sky=0,seams=0,corners=0,edges=0,max_error=0,rough=0;
    std::array<unsigned,6> faces{};
    const auto check_case=[&](unsigned size,unsigned variant,unsigned encoding,unsigned view,unsigned rotation,unsigned quality) {
        const auto input=cube_input(size,variant,encoding);auto p=cube_parameters(input,encoding,view,rotation);
        p.dimensions[2]=quality;p.settings[0]=quality==3?.6f:0;
        const auto actual=dispatch(p,input);++submissions;require(actual.size()==width*height,"Resident cube output extent");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;const V ray{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1};
            const auto primary=trace_native_colour(input.source,{},ray,1,x,y);bool edge=primary.edge;unsigned expected=0;
            if(primary.found) {
                const V normal=cube_model_normal(input,primary.primitive,ray);
                const double bias=std::max(.01,primary.t*std::sqrt(dot(ray,ray))*1e-5);
                const V origin=add(scale(ray,primary.t),scale(normal,bias));
                const auto lobes=native_model_lobes(ray,normal,p.settings[0]);V sum{};
                for(unsigned sample=0;sample<lobes.count;++sample) {
                    const V direction=lobes.direction[sample];
                    const auto secondary=trace_native_colour(input.source,origin,direction,bias,x,y);edge|=secondary.edge;
                    unsigned colour=secondary.colour;
                    if(!secondary.found) {
                        CubeWitness witness;colour=cube_packed_sample(input,p,direction,&witness);++sky;++faces[witness.face];seams+=witness.seam;corners+=witness.corner;
                    }
                    sum=add(sum,scale(native_linear(colour,encoding),1./lobes.count));
                }
                expected=native_pack(sum,encoding,255);rough+=quality==3;
            }
            if(edge){++edges;continue;}
            require((actual[pixel]>>24)==(expected>>24),"Resident environment changed native coverage");
            for(unsigned c=0;c<3;++c) {
                const unsigned error=std::abs(int((actual[pixel]>>(8*c))&255)-int((expected>>(8*c))&255));max_error=std::max(max_error,error);
                if(error>1)throw std::runtime_error("Resident cube RGB mismatch size="+std::to_string(size)+" format="+std::to_string(encoding)+" rotation="+std::to_string(rotation)+" quality="+std::to_string(quality)+" pixel="+std::to_string(pixel)+" actual/reference="+std::to_string(actual[pixel])+"/"+std::to_string(expected));
            }
            ++checked;clear+=!primary.found;
        }
    };
    for(unsigned size:{8U,32U,128U})for(unsigned variant:{0U,3U,4U})for(unsigned encoding:{1U,2U})
        for(unsigned view=0;view<2;++view)for(unsigned rotation=0;rotation<7;++rotation)for(unsigned quality:{1U,3U})
        check_case(size,variant,encoding,view,rotation,quality);
    for(unsigned encoding:{1U,2U})check_case(512,3,encoding,1,6,3);
    std::cout<<"Native resident environment: "<<submissions<<" submissions, checked="<<checked<<" clear="<<clear<<" sky rays="<<sky
        <<" seams/corners="<<seams<<'/'<<corners<<" rough model pixels="<<rough<<" boundary exclusions="<<edges<<" max RGB error="<<max_error<<" faces=";
    for(auto n:faces)std::cout<<n<<',';std::cout<<"; input-only binary64 face bases/rays/UV, linear-light edge-reprojected taps, 8/32/128/512 faces\n"<<std::flush;
    require(checked>1500000 && clear>10000 && sky>100000 && seams>1000 && corners>20 && rough>10000,"Insufficient native cube witnesses");
    for(auto n:faces)require(n>100,"Missing native environment cube face");
}
template<class Dispatch>void run_native_water_environment(Dispatch dispatch) {
    unsigned submissions=0,checked=0,clear=0,seams=0,edges=0,max_error=0;
    for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})for(unsigned rotation:{0U,4U,6U})for(float time:{1.75f,9.125f}) {
        const auto input=cube_input(16,variant,encoding);auto p=cube_parameters(input,encoding,1,rotation);p.camera={46,39,31.3f,22.7f};
        p.settings={0,1,0,1};p.point={0,200,0,0};p.normal={0,-1,0,0};p.source_colour={.2f,.3f,.4f,1};p.water={time,.7f,.85f,96};
        p.row0={1,0,0,0};p.row1={0,1,0,0};p.row2={0,0,1,0};
        const auto full=*starfox::render::shadows::native_water_layers(width,height);p.liquid_layers={full.world_offset/4,full.surface_offset/4,3,full.storage_bytes/4};
        reflected_liquid_oracle::Frame frame;frame.rotation={1,0,0,0,1,0,0,0,1};frame.point={0,200,0};frame.normal={0,-1,0};
        const auto actual=dispatch(p,input);++submissions;require(actual.size()==full.storage_bytes/4,"Native cube water layer extent");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x,surface=full.surface_offset/4+pixel*4;const V ray{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1},direction=unit(ray);
            const double depth=ray[1]>0?200/ray[1]:0;const bool floor=depth>1 && depth<65536;
            if(!floor) {require(actual[pixel]==0 && actual[full.world_offset/4+pixel]==0,"Cube painted water into excluded sky");for(unsigned c=0;c<4;++c)require(actual[surface+c]==0,"Dry cube water retained guides");++clear;continue;}
            const auto water=native_water_expected(input.source,p,frame,{},direction,depth*std::sqrt(dot(ray,ray)),x,y);
            const auto reflected=reflected_liquid_oracle::reflect(direction,water.normal);
            const auto model=trace_native_colour(input.source,add(scale(ray,depth),scale(water.normal,water.bias)),reflected,water.bias,x,y);
            if(water.edge || model.edge){++edges;continue;}
            CubeWitness witness;const unsigned reflected_colour=model.found?model.colour:cube_packed_sample(input,p,reflected,&witness);seams+=!model.found && witness.seam;
            const auto expected=native_pack(add(water.light,scale(native_linear(reflected_colour,encoding),water.fresnel)),encoding,253);
            require(actual[pixel]==actual[full.world_offset/4+pixel] && actual[pixel]>>24==253U,"Native cube water world/primary ownership mismatch");
            for(unsigned c=0;c<3;++c) {const unsigned error=std::abs(int((actual[pixel]>>(8*c))&255)-int((expected>>(8*c))&255));max_error=std::max(max_error,error);require(error<=1,"Native cube water RGB mismatch");}
            V normal{};for(unsigned c=0;c<3;++c)normal[c]=std::bit_cast<float>(actual[surface+c]);
            require(std::sqrt(dot(sub(normal,water.normal),sub(normal,water.normal)))<2e-5,"Native cube changed water normal");
            require(std::abs(std::bit_cast<float>(actual[surface+3])-depth)<std::max(.01,depth*2e-5),"Native cube changed water forward depth");++checked;
        }
    }
    std::cout<<"Native resident water environment: "<<submissions<<" submissions, checked="<<checked<<" dry="<<clear<<" seam witnesses="<<seams<<" boundary exclusions="<<edges<<" max RGB error="<<max_error<<"; independent native optical/coverage/face-basis reference, exact primary/hidden-world equivalence\n"<<std::flush;
    require(checked>40000 && clear>40000 && seams>100,"Insufficient native cube water witnesses");
}
}
