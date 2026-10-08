// Real GPU kernel and independent double-precision FORWARD optics. No CPU
// inverse solve is used as the expected result. Readback/fence are fixture-only.
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
#include "reflected_rough_motion_dxil.hpp"
#include "reflected_rough_motion_spirv.hpp"
namespace {
void check(bool ok,const char* text) {if(!ok) throw std::runtime_error(text);}
using V=std::array<double,3>;using F=std::array<float,4>;
V add(V a,V b) {return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
V sub(V a,V b) {return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V scale(V a,double x) {return {a[0]*x,a[1]*x,a[2]*x};}
double dot(V a,V b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V cross(V a,V b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
V normal(V a) {return scale(a,1/std::sqrt(dot(a,a)));}
V xyz(F a) {return {a[0],a[1],a[2]};}
F words(V a,float w=0) {return {float(a[0]),float(a[1]),float(a[2]),w};}
struct Case {F a,b,c,qa,qb,qc,bary,projection,extent,settings,source;};
struct Result {F motion,hit,outgoing;};
static_assert(sizeof(Case)==176 && sizeof(Result)==48);
struct Ray {V point,normal,origin,outgoing;double depth,bias;bool valid{},fallback{};};
Ray optical(const Case& c,double px,double py) {
    Ray ray{};if(px<0 || py<0 || px>=c.extent[0] || py>=c.extent[1]) return ray;
    const auto& p=c.projection;
    const V direction=normal({(px-p[2])/p[0],(py-p[3])/p[1],1});
    ray.normal=normal(cross(sub(xyz(c.b),xyz(c.a)),sub(xyz(c.c),xyz(c.a))));
    if(dot(ray.normal,direction)>0) ray.normal=scale(ray.normal,-1);
    const double denominator=dot(direction,ray.normal);if(std::abs(denominator)<=1.e-12) return ray;
    const double distance=dot(xyz(c.a),ray.normal)/denominator;ray.depth=distance*direction[2];
    if(distance<=0 || ray.depth<c.extent[2] || ray.depth>c.extent[3]) return ray;
    ray.point=scale(direction,distance);ray.bias=std::max(.01,distance*1.e-5);
    ray.origin=add(ray.point,scale(ray.normal,ray.bias));
    const V reflected=sub(direction,scale(ray.normal,2*dot(direction,ray.normal)));
    const V tangent=normal(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0}));
    const V bitangent=cross(reflected,tangent);
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    const auto index=unsigned(c.settings[1]);
    ray.outgoing=normal(add(reflected,scale(add(scale(tangent,taps[index][0]),scale(bitangent,taps[index][1])),double(c.settings[0])*c.settings[0])));
    ray.fallback=dot(ray.outgoing,ray.normal)<=0;if(ray.fallback) ray.outgoing=reflected;
    ray.valid=true;return ray;
}
V feature(const Case& c) {return add(add(scale(xyz(c.qa),1-c.bary[0]-c.bary[1]),scale(xyz(c.qb),c.bary[0])),scale(xyz(c.qc),c.bary[1]));}
bool finite_face(const Case& c,V point) {
    const V u=sub(xyz(c.b),xyz(c.a)),v=sub(xyz(c.c),xyz(c.a)),r=sub(point,xyz(c.a));
    const double aa=dot(u,u),ab=dot(u,v),bb=dot(v,v),ra=dot(r,u),rb=dot(r,v),d=aa*bb-ab*ab;
    const double x=(bb*ra-ab*rb)/d,y=(aa*rb-ab*ra)/d;
    return x>=-.00001 && y>=-.00001 && x+y<=1.00001;
}
double residual(const Case& c,V target,double px,double py,bool biased=true) {
    const auto ray=optical(c,px,py);if(!ray.valid) return INFINITY;
    const V travel=sub(target,biased?ray.origin:ray.point);
    if(dot(travel,travel)<=ray.bias*ray.bias) return INFINITY;
    const auto unit=normal(travel);if(dot(unit,ray.outgoing)<=0) return INFINITY;
    // Full three-component angular miss, not the implementation's chosen
    // two-component residual frame. This detects reverse or wrong-lobe solves.
    const double distance=std::sqrt(dot(cross(unit,ray.outgoing),cross(unit,ray.outgoing)));
    return distance*std::max(c.projection[0],c.projection[1]);
}
std::array<double,2> sharp(const Case& c,V target) {
    const V n=normal(cross(sub(xyz(c.b),xyz(c.a)),sub(xyz(c.c),xyz(c.a))));
    const V q=sub(target,scale(n,2*dot(sub(target,xyz(c.a)),n)));
    return {c.projection[0]*q[0]/q[2]+c.projection[2],c.projection[1]*q[1]/q[2]+c.projection[3]};
}
void target(Case& c,V value) {
    c.bary={.23F,.31F,0,0};const V u{17,3,6},v{-2,13,9};
    c.qa=words(sub(value,add(scale(u,c.bary[0]),scale(v,c.bary[1]))),1);
    c.qb=words(add(xyz(c.qa),u),1);c.qc=words(add(xyz(c.qa),v),1);
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
        shader.code=vk?reflected_rough_motion_spirv:reflected_rough_motion_dxil;
        shader.code_size=vk?sizeof(reflected_rough_motion_spirv):sizeof(reflected_rough_motion_dxil);
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
    std::array<unsigned,8> samples{},accepted{},roundtrip{},sharp_sensitive{},lobe_sensitive{};
    unsigned fallback{},branchY{},branchX{};
    // Every material roughness plus the full [0,1] endpoints; not just a sharp
    // substitute for the actual metal presets. Source data is quantized to the
    // same float input before the independent forward oracle runs.
    for(float rough:{0.F,.04F,.08F,.12F,.20F,.24F,.27F,.28F,.35F,.43F,.55F,.62F,1.F})
    for(unsigned pose=0;pose<12;++pose) for(unsigned eye=0;eye<2;++eye) for(unsigned lobe=0;lobe<8;++lobe)
    for(unsigned y=0;y<3;++y) for(unsigned x=0;x<4;++x) {
        Case c{};const unsigned w=pose<3?96:pose<6?256:pose<9?640:1280,h=w*3/4;
        c.projection={float(w*.86),float(h*1.17),float(w*(eye?.529:.481)),float(h*.453)};
        c.extent={float(w),float(h),5,65536};c.settings={rough,float(lobe),0,0};
        const double yaw=(int(pose)%4-2)*.29,pitch=(int(pose)/4-1)*.52;
        const V n=normal({std::sin(yaw),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)});
        const V u=normal(cross(n,{0,1,0})),v=cross(n,u),centre{eye?17.:-13.,(int(pose)-5)*7.,180.+pose*39};
        const double radius=1200.+pose*80;
        c.a=words(add(centre,add(scale(u,-2*radius),scale(v,-2*radius))),1);
        c.b=words(add(centre,add(scale(u,2*radius),scale(v,-2*radius))),1);
        c.c=words(add(centre,scale(v,3*radius)),1);
        c.source={float(w*(.071+x*.282)),float(h*(.113+y*.376)),0,0};
        const auto ray=optical(c,c.source[0],c.source[1]);
        if(!ray.valid || !finite_face(c,ray.point)) continue;
        const V incident=normal(ray.point),reflected=sub(incident,scale(ray.normal,2*dot(incident,ray.normal)));
        branchY+=std::abs(reflected[1])<.95;branchX+=std::abs(reflected[1])>=.95;fallback+=ray.fallback;
        target(c,add(ray.origin,scale(ray.outgoing,25.+((x*29+y*41+pose*17)%29)*31)));
        cases.push_back(c);invalid.push_back(false);++samples[lobe];
    }
    check(cases.size()>10000 && branchY && branchX && fallback,"Rough fixture missed quadrature/reference/fallback coverage");
    const unsigned valid_cases=unsigned(cases.size());
    constexpr unsigned faults=40;
    for(unsigned fault=0;fault<faults;++fault) {
        Case c=cases[1200];const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
        switch(fault) {
        case 0:c.settings[0]=nan;break;case 1:c.settings[0]=-.01F;break;case 2:c.settings[0]=1.01F;break;
        case 3:c.settings[1]=8;break;case 4:c.settings[1]=-.1F;break;case 5:c.settings[1]=.5F;break;
        case 6:c.settings[2]=1;break;case 7:c.settings[3]=nan;break;case 8:c.a[3]=0;break;
        case 9:c.b[3]=.5F;break;case 10:c.c=c.b;break;case 11:c.a[0]=nan;break;
        case 12:c.b[1]=inf;break;case 13:c.c[2]=1.e15F;break;case 14:c.qa[3]=0;break;
        case 15:c.qb[3]=.5F;break;case 16:c.qc=c.qb;break;case 17:c.qa[0]=nan;break;
        case 18:c.qb[0]=1.e30F;c.qc[1]=1.e30F;break;case 19:c.bary[0]=-.1F;break;
        case 20:c.bary[0]=.8F;c.bary[1]=.7F;break;case 21:c.bary[1]=nan;break;
        case 22:c.projection[0]=0;break;case 23:c.projection[1]=-1;break;case 24:c.projection[2]=nan;break;
        case 25:c.projection[3]=1.e15F;break;case 26:c.extent[0]=0;break;case 27:c.extent[1]=16385;break;
        case 28:c.extent[2]=-1;break;case 29:c.extent[3]=c.extent[2];break;case 30:c.extent[3]=inf;break;
        case 31:c.extent[3]=1.e15F;break;case 32:c.settings[0]=inf;break;case 33:c.settings[1]=inf;break;
        case 34:c.extent[2]=100000;c.extent[3]=200000;break;
        case 35:c.extent[2]=.1F;c.extent[3]=.2F;break;
        case 36:for(auto* vertex:{&c.a,&c.b,&c.c}) (*vertex)[2]=-100000;break;
        case 37:for(auto* vertex:{&c.a,&c.b,&c.c}) (*vertex)[0]+=1.e6F;break;
        case 38:{const auto ray=optical(c,c.source[0],c.source[1]);target(c,sub(ray.point,scale(ray.normal,1000)));break;}
        case 39:c.a={0,0,0,1};c.b={100,0,0,1};c.c={0,100,0,1};break;
        }
        cases.push_back(c);invalid.push_back(true);
    }
    Gpu gpu;const auto results=gpu.run(backend,cases);
    double max_miss{},max_forward{};unsigned biased_sensitive{},outside_sharp{};
    for(unsigned i=0;i<cases.size();++i) {
        const auto& c=cases[i];const auto& r=results[i];
        for(float f:r.motion) check(std::isfinite(f),"Nonfinite rough guide");
        if(invalid[i]) {
            if(r.motion!=F{}) throw std::runtime_error("Invalid rough optics accepted: fault="+std::to_string(i-valid_cases));
            continue;
        }
        const auto ray=optical(c,c.source[0],c.source[1]);check(ray.valid,"Invalid independent source ray");
        for(unsigned k=0;k<3;++k) {
            const double e=std::abs(r.outgoing[k]-ray.outgoing[k]);max_forward=std::max(max_forward,e);
            if(e>1.e-4 || std::abs(r.hit[k]-ray.origin[k])>std::max(.02,std::sqrt(dot(ray.origin,ray.origin))*2.e-5))
                throw std::runtime_error("Native rough forward oracle mismatch case="+std::to_string(i));
        }
        check(std::abs(r.hit[3]-ray.bias)<.0001 && std::abs(r.outgoing[3]-ray.depth)<std::max(.02,ray.depth*1.e-5),"Rough receiver depth/origin bias differs");
        const V q=feature(c);const auto mirror=sharp(c,q);const unsigned lobe=unsigned(c.settings[1]);
        sharp_sensitive[lobe]+=residual(c,q,mirror[0],mirror[1])>.1;
        if(r.motion[3]==0) {check(r.motion==F{},"Rejected rough guide has partial coordinates");continue;}
        check(r.motion[3]==1,"Noncanonical rough validity");++accepted[lobe];
        const double error=residual(c,q,r.motion[0],r.motion[1]);max_miss=std::max(max_miss,error);
        const auto at=optical(c,r.motion[0],r.motion[1]);
        if(error>.015 || !at.valid || !finite_face(c,at.point)) {
            std::cerr<<"rough="<<c.settings[0]<<" lobe="<<lobe<<" source="<<c.source[0]<<','<<c.source[1]
                <<" guide="<<r.motion[0]<<','<<r.motion[1]<<" depth="<<at.depth
                <<" targetTravel="<<std::sqrt(dot(sub(q,at.origin),sub(q,at.origin)))
                <<" target="<<q[0]<<','<<q[1]<<','<<q[2]<<" normal="<<at.normal[0]<<','<<at.normal[1]<<','<<at.normal[2]
                <<" forwardAngular="<<residual(c,q,c.source[0],c.source[1])<<" originError="
                <<std::sqrt(dot(sub(xyz(r.hit),ray.origin),sub(xyz(r.hit),ray.origin)))<<'\n';
            throw std::runtime_error("Rough guide misses old finite path case="+std::to_string(i)
                +" angular_pixel_error="+std::to_string(error));
        }
        check(std::abs(r.motion[2]-at.depth)<std::max(.02,at.depth*1.e-5),"Rough guide transports biased rather than primary depth");
        roundtrip[lobe]+=std::hypot(r.motion[0]-c.source[0],r.motion[1]-c.source[1])<.08;
        auto wrong=c;wrong.settings[1]=float((lobe+1)%8);
        lobe_sensitive[lobe]+=residual(wrong,q,r.motion[0],r.motion[1])>.1;
        biased_sensitive+=residual(c,q,r.motion[0],r.motion[1],false)>.02;
        outside_sharp+=mirror[0]<0 || mirror[1]<0 || mirror[0]>=c.extent[0] || mirror[1]>=c.extent[1];
    }
    for(unsigned lobe=0;lobe<8;++lobe) {
        check(accepted[lobe]>samples[lobe]*3/4 && roundtrip[lobe]>samples[lobe]/2,"Rough inverse lobe coverage insufficient");
        check(sharp_sensitive[lobe]>samples[lobe]/2 && lobe_sensitive[lobe]>accepted[lobe]/2,"Rough oracle cannot detect sharp/shared-lobe guide substitution");
        std::cout<<"Lobe "<<lobe<<": "<<samples[lobe]<<" physical forward rays, "<<accepted[lobe]<<" accepted solves, "
            <<roundtrip[lobe]<<" source roundtrips, "<<sharp_sensitive[lobe]<<" sharp-sensitive, "<<lobe_sensitive[lobe]<<" wrong-lobe-sensitive\n";
    }
    check(biased_sensitive>50 && outside_sharp>20,"Rough proof missed real origin bias or outside-central-ray recovery");
    std::cout<<backend<<": "<<valid_cases<<" valid rough rays; "<<faults<<" malformed/behind/clip/finite-face/reverse exclusions; "
        <<branchY<<" Y-reference, "<<branchX<<" X-reference, "<<fallback<<" below-surface native fallbacks, "
        <<biased_sensitive<<" bias-sensitive and "<<outside_sharp<<" offscreen central mirror recoveries; max accepted angular-pixel miss="
        <<max_miss<<", max outgoing error="<<max_forward<<". Kernel-only; per-lobe radiance/identity/visibility/multibounce history not integrated, no FPS or physical XR acceptance.\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
