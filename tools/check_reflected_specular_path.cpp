// Independent binary64 FORWARD paths, not a CPU version of the inverse solve.
// The optical helper is exercised on the real GPU; this is not acceptance of
// native RT path witnesses, history visibility or a completed game owner.
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "reflected_specular_path_dxil.hpp"
#include "reflected_specular_path_spirv.hpp"
namespace {
void check(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
using V=std::array<double,3>;using F=std::array<float,4>;
V add(V a,V b) {return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
V sub(V a,V b) {return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V scale(V a,double n) {return {a[0]*n,a[1]*n,a[2]*n};}
double dot(V a,V b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V cross(V a,V b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
V unit(V a) {return scale(a,1/std::sqrt(dot(a,a)));}
V xyz(F a) {return {a[0],a[1],a[2]};}
F words(V a,float w) {return {float(a[0]),float(a[1]),float(a[2]),w};}
struct Plane {F a,b,c;};
struct Case {F a,b,c,projection,extent,settings;std::array<Plane,4> planes;F terminal,source;std::array<unsigned,4> control;};
struct Result {F motion,origin,outgoing;};
static_assert(sizeof(Case)==336 && sizeof(Result)==48);
struct Ray {V point,origin,outgoing;double depth{},bias{};bool valid{};};
bool analytic(Plane p) {return p.a[3]==2 && p.b[3]==2 && p.c[3]==2;}
V plane_normal(Plane p) {return unit(analytic(p)?xyz(p.b):cross(sub(xyz(p.b),xyz(p.a)),sub(xyz(p.c),xyz(p.a))));}
bool inside(Plane p,V point) {
    if(analytic(p)) return true;
    const V u=sub(xyz(p.b),xyz(p.a)),v=sub(xyz(p.c),xyz(p.a)),r=sub(point,xyz(p.a));
    const double aa=dot(u,u),ab=dot(u,v),bb=dot(v,v),ra=dot(r,u),rb=dot(r,v),det=aa*bb-ab*ab;
    const double x=(bb*ra-ab*rb)/det,y=(aa*rb-ab*ra)/det;
    return std::isfinite(x) && std::isfinite(y) && x>=-.00001 && y>=-.00001 && x+y<=1.00001;
}
Ray primary(const Case& c,double x,double y) {
    Ray ray{};
    if(x<0 || y<0 || x>=c.extent[0] || y>=c.extent[1]) return ray;
    const V direction=unit({(x-c.projection[2])/c.projection[0],(y-c.projection[3])/c.projection[1],1});
    V normal=plane_normal({c.a,c.b,c.c});
    if(dot(normal,direction)>0) normal=scale(normal,-1);
    const double denominator=dot(direction,normal);if(std::abs(denominator)<=1.e-12) return ray;
    const double distance=dot(xyz(c.a),normal)/denominator;
    ray.depth=distance*direction[2];
    if(distance<=0 || ray.depth<c.extent[2] || ray.depth>c.extent[3]) return ray;
    ray.point=scale(direction,distance);if(!inside({c.a,c.b,c.c},ray.point)) return ray;
    ray.bias=std::max(analytic({c.a,c.b,c.c})?.05:.01,distance*1.e-5);ray.origin=add(ray.point,scale(normal,ray.bias));
    const V reflected=sub(direction,scale(normal,2*dot(direction,normal)));
    const V tangent=unit(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0}));
    const V bitangent=cross(reflected,tangent);
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    const unsigned lobe=unsigned(c.settings[1]);
    ray.outgoing=unit(add(reflected,scale(add(scale(tangent,taps[lobe][0]),scale(bitangent,taps[lobe][1])),double(c.settings[0])*c.settings[0])));
    if(dot(ray.outgoing,normal)<=0) ray.outgoing=reflected;
    ray.valid=true;return ray;
}
bool advance(Ray& ray,Plane p,bool biased=true) {
    V normal=plane_normal(p);
    if(dot(normal,ray.outgoing)>0) normal=scale(normal,-1);
    const double denominator=dot(ray.outgoing,normal);if(std::abs(denominator)<=1.e-12) return false;
    const double distance=dot(sub(xyz(p.a),ray.origin),normal)/denominator;
    if(distance<=ray.bias || distance>=65536) return false;
    ray.point=add(ray.origin,scale(ray.outgoing,distance));if(!inside(p,ray.point)) return false;
    ray.bias=std::max(.05,distance*1.e-5);
    ray.origin=biased?add(ray.point,scale(normal,ray.bias)):ray.point;
    ray.outgoing=sub(ray.outgoing,scale(normal,2*dot(ray.outgoing,normal)));return true;
}
Ray forward(const Case& c,double x,double y,bool biased=true) {
    auto ray=primary(c,x,y);if(!ray.valid) return ray;
    for(unsigned hop=0;hop<c.control[0];++hop) if(!advance(ray,c.planes[hop],biased)) {ray.valid=false;break;}
    return ray;
}
Plane plane_at(V centre,V normal,double radius) {
    const V u=unit(cross(normal,std::abs(normal[1])<.95?V{0,1,0}:V{1,0,0})),v=cross(normal,u);
    return {words(add(centre,add(scale(u,-2*radius),scale(v,-2*radius))),1),
        words(add(centre,add(scale(u,2*radius),scale(v,-2*radius))),1),words(add(centre,scale(v,3*radius)),1)};
}
Plane analytic_at(V centre,V normal) {return {words(centre,2),words(normal,2),words({},2)};}
double error(const Case& c,const Ray& ray) {
    if(!ray.valid) return INFINITY;
    const V travel=c.terminal[3]==0?unit(xyz(c.terminal)):unit(sub(xyz(c.terminal),ray.origin));
    if(dot(travel,ray.outgoing)<=0) return INFINITY;
    const V difference=cross(travel,ray.outgoing);
    return std::sqrt(dot(difference,difference))*std::max(c.projection[0],c.projection[1]);
}
struct Gpu {
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};SDL_GPUBuffer *input{},*output{};
    SDL_GPUTransferBuffer *upload{},*download{};
    ~Gpu() {
        if(device) {
            SDL_WaitForGPUIdle(device);
            if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
            if(input) SDL_ReleaseGPUBuffer(device,input);
            if(output) SDL_ReleaseGPUBuffer(device,output);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
            SDL_DestroyGPUDevice(device);
        }
        SDL_Quit();
    }
    std::vector<Result> run(std::string_view backend,const std::vector<Case>& cases) {
        check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_SPIRV,true,std::string(backend).c_str());check(device,SDL_GetError());
        const bool vk=backend=="vulkan";SDL_GPUComputePipelineCreateInfo shader{};
        shader.code=vk?reflected_specular_path_spirv:reflected_specular_path_dxil;
        shader.code_size=vk?sizeof(reflected_specular_path_spirv):sizeof(reflected_specular_path_dxil);
        shader.entrypoint="main";shader.format=vk?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.num_readonly_storage_buffers=shader.num_readwrite_storage_buffers=shader.num_uniform_buffers=1;
        shader.threadcount_x=64;shader.threadcount_y=shader.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&shader);check(pipeline,SDL_GetError());
        const unsigned inBytes=unsigned(cases.size()*sizeof(Case)),outBytes=unsigned(cases.size()*sizeof(Result));
        const SDL_GPUBufferCreateInfo ii{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,inBytes,0},oi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,outBytes,0};
        input=SDL_CreateGPUBuffer(device,&ii);output=SDL_CreateGPUBuffer(device,&oi);check(input&&output,SDL_GetError());
        const SDL_GPUTransferBufferCreateInfo ui{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,inBytes,0},di{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,outBytes,0};
        upload=SDL_CreateGPUTransferBuffer(device,&ui);download=SDL_CreateGPUTransferBuffer(device,&di);check(upload&&download,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(device,upload,false);check(mapped,SDL_GetError());
        std::memcpy(mapped,cases.data(),inBytes);SDL_UnmapGPUTransferBuffer(device,upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(device);check(cmd,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(cmd);check(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{upload,0};const SDL_GPUBufferRegion to{input,0,inBytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=output;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);check(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,&input,1);
        const std::array<unsigned,4> constants{unsigned(cases.size()),0,0,0};
        SDL_PushGPUComputeUniformData(cmd,0,constants.data(),sizeof(constants));
        SDL_DispatchGPUCompute(pass,(constants[0]+63)/64,1,1);SDL_EndGPUComputePass(pass);
        copy=SDL_BeginGPUCopyPass(cmd);check(copy,SDL_GetError());
        const SDL_GPUBufferRegion source{output,0,outBytes};const SDL_GPUTransferBufferLocation dest{download,0};
        SDL_DownloadFromGPUBuffer(copy,&source,&dest);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);check(fence,SDL_GetError());
        check(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
        const auto* result=static_cast<const Result*>(SDL_MapGPUTransferBuffer(device,download,false));check(result,SDL_GetError());
        std::vector<Result> values(result,result+cases.size());SDL_UnmapGPUTransferBuffer(device,download);return values;
    }
};
}
int main(int argc,char** argv) try {
    const std::string_view backend=argc>1?argv[1]:"direct3d12";
    check(argc<=2 && (backend=="direct3d12" || backend=="vulkan"),"Expected direct3d12 or vulkan");
    std::vector<Case> cases;std::vector<bool> invalid;
    std::array<std::array<std::array<unsigned,5>,2>,3> samples{},accepted{},roundtrip{};
    // Domain 0 retains every original finite case. Domain 1 has a sharp
    // analytic primary floor and alternates finite plates with that SAME
    // floor. Domain 2 has the native model rough quadrature, a finite primary
    // and one actual analytic floor reused at later bounces.
    for(unsigned domain=0;domain<3;++domain)
    for(float rough:{0.F,.04F,.12F,.24F,.35F,.62F,1.F})
    for(unsigned pose=0;pose<16;++pose) for(unsigned eye=0;eye<2;++eye) for(unsigned lobe=0;lobe<8;++lobe)
    for(unsigned point=0;point<3;++point) for(unsigned hops=0;hops<=4;++hops) for(unsigned environment=0;environment<2;++environment) {
        if(domain==1 && (rough!=0 || lobe!=0)) continue;
        constexpr unsigned widths[]{96,256,640,1280,2048,4096,8192,16384};
        Case c{};const unsigned w=widths[pose/2],h=w*3/4;
        c.projection={float(w*.86),float(h*1.17),float(w*(eye?.529:.481)),float(h*.453)};
        c.extent={float(w),float(h),5,65536};c.settings={rough,float(lobe),0,0};c.control={hops,domain,0,0};
        const double yaw=(int(pose)%4-2)*.25,pitch=(int(pose)/4-1)*.36;
        const V normal=unit({std::sin(yaw),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)});
        const V centre{eye?17.:-13.,(int(pose)-3)*7.,230.+pose*39};
        const auto face=domain==1?analytic_at(centre,normal):plane_at(centre,normal,1400.+pose*80);
        c.a=face.a;c.b=face.b;c.c=face.c;
        c.source={float(w*(.16+point*.31)),float(h*(.22+point*.27)),0,0};
        auto ray=primary(c,c.source[0],c.source[1]);if(!ray.valid) continue;
        bool valid=true;Plane floor=domain==1?face:Plane{};
        for(unsigned hop=0;hop<hops;++hop) {
            const V tilt{.22*std::sin(double(pose+hop+1)),.25*std::cos(double(hop+lobe+1)),.2};
            const V n=unit(add(scale(ray.outgoing,-1),tilt));
            const V hit=add(ray.origin,scale(ray.outgoing,140.+hop*71+point*39));
            if(domain==2 && hop==0) floor=analytic_at(hit,n);
            c.planes[hop]=(domain==1 && hop%2==1) || (domain==2 && hop%2==0)
                ?floor:plane_at(hit,n,2000.+hop*300);
            if(!advance(ray,c.planes[hop])) {valid=false;break;}
        }
        if(!valid) continue;
        c.terminal=environment?words(ray.outgoing,0):words(add(ray.origin,scale(ray.outgoing,320.+point*117+pose*41)),1);
        cases.push_back(c);invalid.push_back(false);++samples[domain][environment][hops];
    }
    const unsigned valid_cases=unsigned(cases.size());check(valid_cases>50000,"Insufficient specular-path coverage");
    const auto found=std::find_if(cases.begin(),cases.end(),[](const Case& c){return c.control[0]==4 && c.terminal[3]==1;});
    check(found!=cases.end(),"Four-hop fixture absent");const Case base=*found;
    constexpr unsigned faults=42;
    for(unsigned fault=0;fault<30;++fault) {
        Case c=base;const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
        switch(fault) {
        case 0:c.control[0]=5;break;case 1:c.terminal[3]=2;break;case 2:c.terminal[0]=nan;break;
        case 3:c.terminal[2]=1.e15F;break;case 4:c.terminal={};break;case 5:c.a[3]=0;break;
        case 6:c.b[3]=.5F;break;case 7:c.c=c.b;break;case 8:c.a[0]=nan;break;
        case 9:c.projection[0]=0;break;case 10:c.projection[1]=-1;break;case 11:c.projection[2]=inf;break;
        case 12:c.extent[0]=0;break;case 13:c.extent[1]=16385;break;case 14:c.extent[2]=-1;break;
        case 15:c.extent[3]=c.extent[2];break;case 16:c.extent[3]=inf;break;case 17:c.settings[0]=nan;break;
        case 18:c.settings[0]=-1;break;case 19:c.settings[0]=1.01F;break;case 20:c.settings[1]=8;break;
        case 21:c.settings[2]=1;break;case 22:c.planes[0].a[3]=0;break;case 23:c.planes[1].b[3]=.5F;break;
        case 24:c.planes[2].c=c.planes[2].b;break;case 25:c.planes[3].a[0]=nan;break;
        case 26:c.planes[0].b[1]=inf;break;case 27:c.planes[1].c[2]=1.e15F;break;
        case 28:case 29:{
            auto& p=c.planes[fault==28?0:3];const V move=scale(unit(sub(xyz(p.b),xyz(p.a))),1.e6);
            p.a=words(add(xyz(p.a),move),1);p.b=words(add(xyz(p.b),move),1);p.c=words(add(xyz(p.c),move),1);break;
        }
        }
        cases.push_back(c);invalid.push_back(true);
    }
    const auto analytic_found=std::find_if(cases.begin(),cases.begin()+valid_cases,
        [](const Case& c){return c.control[0]==4 && c.control[1]==1 && c.terminal[3]==1;});
    const auto mixed_found=std::find_if(cases.begin(),cases.begin()+valid_cases,
        [](const Case& c){return c.control[0]==4 && c.control[1]==2 && c.terminal[3]==1;});
    check(analytic_found!=cases.begin()+valid_cases && mixed_found!=cases.begin()+valid_cases,"Actual analytic/mixed four-hop fixtures absent");
    const Case analytic_base=*analytic_found,mixed_base=*mixed_found;
    for(unsigned fault=0;fault<12;++fault) {
        Case c=fault<6?analytic_base:mixed_base;
        const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
        if(fault==0)c.a[3]=1;
        if(fault==1)c.b[3]=1;
        if(fault==2)c.c[0]=1;
        if(fault==3)c.b={0,0,0,2};
        if(fault==4)c.b[0]=nan;
        if(fault==5)c.a[0]=1.e15F;
        auto& p=c.planes[0];
        if(fault==6)p.a[3]=1;
        if(fault==7)p.b[3]=1;
        if(fault==8)p.c[0]=1;
        if(fault==9)p.b={0,0,0,2};
        if(fault==10)p.b[0]=inf;
        if(fault==11)p.a[0]=1.e15F;
        cases.push_back(c);invalid.push_back(true);
    }
    Gpu gpu;const auto results=gpu.run(backend,cases);
    double max_forward{},max_error{},max_known_pixel{};unsigned bias_sensitive{},chain_sensitive{},alternate{};
    for(unsigned i=0;i<cases.size();++i) {
        const auto& c=cases[i];const auto& r=results[i];
        for(float value:r.motion) check(std::isfinite(value),"Nonfinite specular guide");
        if(invalid[i]) {
            if(r.motion!=F{}) throw std::runtime_error("Invalid specular path accepted: fault="+std::to_string(i-valid_cases));
            continue;
        }
        const auto ray=forward(c,c.source[0],c.source[1]);check(ray.valid,"Independent forward path became invalid");
        for(unsigned k=0;k<3;++k) {
            const double miss=std::abs(r.outgoing[k]-ray.outgoing[k]);max_forward=std::max(max_forward,miss);
            if(miss>1.e-4 || std::abs(r.origin[k]-ray.origin[k])>std::max(.03,std::sqrt(dot(ray.origin,ray.origin))*3.e-5))
                throw std::runtime_error("Specular forward oracle mismatch: case="+std::to_string(i));
        }
        check(std::abs(r.origin[3]-ray.bias)<.0001 && std::abs(r.outgoing[3]-ray.depth)<std::max(.02,ray.depth*1.e-5),
            "Specular path lost primary depth or per-hop bias");
        const unsigned environment=c.terminal[3]==0,hops=c.control[0],domain=c.control[1];
        if(!environment && hops) bias_sensitive+=error(c,forward(c,c.source[0],c.source[1],false))>.015;
        if(hops) {auto only=c;only.control[0]=0;chain_sensitive+=error(only,forward(only,c.source[0],c.source[1]))>.1;}
        if(r.motion[3]==0) {check(r.motion==F{},"Rejected specular path has partial coordinates");continue;}
        check(r.motion[3]==1,"Noncanonical specular path validity");++accepted[domain][environment][hops];
        const auto reconstructed=forward(c,r.motion[0],r.motion[1]);
        const double optical_error=error(c,reconstructed);max_error=std::max(max_error,optical_error);
        const double pixel=std::max(std::abs(r.motion[0]-c.source[0]),std::abs(r.motion[1]-c.source[1]));
        if(!reconstructed.valid || optical_error>.015)
            throw std::runtime_error("Specular inverse forward/finite-path mismatch: case="+std::to_string(i)
                +" error="+std::to_string(optical_error)+" pixel="+std::to_string(pixel));
        // The native rough quadrature/reference/fallback map is not injective:
        // it can yield another finite, forward-valid path to the SAME endpoint.
        // Binary64 tracing above still checks every face and the full angular
        // error. Do not call that a unique source-pixel round trip, nor permit
        // colour reuse without the actual cached full-path visibility witness.
        if(pixel<=.1) {++roundtrip[domain][environment][hops];max_known_pixel=std::max(max_known_pixel,pixel);}
        else ++alternate;
        check(std::abs(r.motion[2]-reconstructed.depth)<std::max(.02,reconstructed.depth*1.e-5),"Specular guide replaced the primary receiver depth");
    }
    for(unsigned domain=0;domain<3;++domain)
    for(unsigned environment=0;environment<2;++environment) for(unsigned hops=0;hops<=4;++hops) {
        const unsigned minimum=domain==1?30:1000;
        check(samples[domain][environment][hops]>minimum && accepted[domain][environment][hops]>minimum && roundtrip[domain][environment][hops]>minimum,
            "Missing positive bounded path/terminal/known-source coverage");
        std::cout<<(domain==0?"finite":domain==1?"analytic primary / same floor":"finite primary / same floor")<<" / "
            <<(environment?"environment direction":"finite feature")<<" / "<<hops<<" secondary planes: "
            <<accepted[domain][environment][hops]<<" / "<<samples[domain][environment][hops]<<" accepted, "
            <<roundtrip[domain][environment][hops]<<" known-source round trips\n";
    }
    check(bias_sensitive>100 && chain_sensitive>1000,"Fixture did not distinguish full biased paths from final-hit/primary-only motion");
    std::cout<<valid_cases<<" independent specular forward paths; "<<faults<<" malformed/off-face exclusions; max forward/error/known-pixel "
        <<max_forward<<" / "<<max_error<<" / "<<max_known_pixel<<"; bias/chain sensitive "<<bias_sensitive<<" / "<<chain_sensitive
        <<"; other independently valid roots "<<alternate
        <<". Optical helper only: native witnesses, colour history and runtime owner integration remain required.\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
