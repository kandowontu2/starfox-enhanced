// Real DXIL/SPIR-V execution against independent binary64 FORWARD optics.
// This does not prove native path recording, old visibility or cached radiance.
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "reflected_liquid_oracle.hpp"
#include "reflected_curved_path_guide_dxil.hpp"
#include "reflected_curved_path_guide_spirv.hpp"
#include "reflected_curved_path_witness_dxil.hpp"
#include "reflected_curved_path_witness_spirv.hpp"
#include "reflected_curved_device.hpp"
namespace {
void check(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
std::vector<std::uint32_t> binary_words(const std::filesystem::path& path) {
    const auto bytes=std::filesystem::file_size(path);
    check(bytes!=0 && bytes%4==0 && bytes<=64*1024*1024,"Invalid bounded comparison binary size");
    std::vector<std::uint32_t> words(bytes/4);
    std::ifstream stream(path,std::ios::binary);
    stream.read(reinterpret_cast<char*>(words.data()),std::streamsize(bytes));
    check(bool(stream),"Unable to read complete comparison binary");return words;
}
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
struct Case {
    F a,b,c,projection,extent,settings;
    std::array<Plane,4> planes;
    std::array<F,8> liquid;
    F terminal,source,seed;
    std::array<unsigned,4> control; // hops, actual liquid-hop mask, liquid primary, domain
};
struct Result {F motion,origin,outgoing,residual;std::array<F,10> trace;};
static_assert(sizeof(Case)==480 && sizeof(Result)==224);
struct Ray {V point,origin,outgoing;double depth{},bias{};bool valid{};};
bool analytic(Plane p) {return p.a[3]==2 && p.b[3]==2 && p.c[3]==2;}
V plane_normal(Plane p) {return unit(analytic(p)?xyz(p.b):cross(sub(xyz(p.b),xyz(p.a)),sub(xyz(p.c),xyz(p.a))));}
bool inside(Plane p,V position) {
    if(analytic(p)) return true;
    const V u=sub(xyz(p.b),xyz(p.a)),v=sub(xyz(p.c),xyz(p.a)),r=sub(position,xyz(p.a));
    const double aa=dot(u,u),ab=dot(u,v),bb=dot(v,v),ra=dot(r,u),rb=dot(r,v),det=aa*bb-ab*ab;
    const double x=(bb*ra-ab*rb)/det,y=(aa*rb-ab*ra)/det;
    return std::isfinite(x) && std::isfinite(y) && x>=-.00001 && y>=-.00001 && x+y<=1.00001;
}
Plane plane_at(V centre,V normal,double radius) {
    const V u=unit(cross(normal,std::abs(normal[1])<.95?V{0,1,0}:V{1,0,0})),v=cross(normal,u);
    return {words(add(centre,add(scale(u,-2*radius),scale(v,-2*radius))),1),
        words(add(centre,add(scale(u,2*radius),scale(v,-2*radius))),1),words(add(centre,scale(v,3*radius)),1)};
}
reflected_liquid_oracle::Frame liquid_frame(const Case& c) {
    reflected_liquid_oracle::Frame f;
    f.point=xyz(c.liquid[0]);f.normal=xyz(c.liquid[1]);
    for(unsigned r=0;r<3;++r) {
        for(unsigned column=0;column<3;++column) f.rotation[r*3+column]=c.liquid[2+r][column];
        f.offset[r]=c.liquid[2+r][3];
    }
    for(unsigned k=0;k<4;++k) f.projection[k]=c.liquid[5][k];
    f.width=unsigned(c.liquid[6][0]);f.height=unsigned(c.liquid[6][1]);
    f.near=c.liquid[6][2];f.far=c.liquid[6][3];f.time=c.liquid[7][0];f.material=unsigned(c.liquid[7][1]);return f;
}
Ray primary(const Case& c,double x,double y,bool flat=false) {
    Ray ray{};
    if(c.control[2]) {
        const auto f=liquid_frame(c);const auto optical=reflected_liquid_oracle::optical(f,x,y);
        if(!optical.valid) return ray;
        ray.point=optical.hit;V normal=optical.normal;ray.bias=optical.bias;ray.depth=optical.depth;
        if(flat) {ray.point=scale(optical.direction,optical.distance);normal=unit(f.normal);
            if(dot(normal,optical.direction)>0) normal=scale(normal,-1);}
        ray.origin=add(ray.point,scale(normal,ray.bias));
        ray.outgoing=reflected_liquid_oracle::reflect(optical.direction,normal);ray.valid=true;return ray;
    }
    if(x<0 || y<0 || x>=c.extent[0] || y>=c.extent[1]) return ray;
    const V direction=unit({(x-c.projection[2])/c.projection[0],(y-c.projection[3])/c.projection[1],1});
    V normal=plane_normal({c.a,c.b,c.c});if(dot(normal,direction)>0) normal=scale(normal,-1);
    const double den=dot(direction,normal);if(std::abs(den)<=1.e-12) return ray;
    const double distance=dot(xyz(c.a),normal)/den;ray.depth=distance*direction[2];
    if(distance<=0 || ray.depth<c.extent[2] || ray.depth>c.extent[3]) return ray;
    ray.point=scale(direction,distance);if(!inside({c.a,c.b,c.c},ray.point)) return ray;
    ray.bias=std::max(.01,distance*1.e-5);ray.origin=add(ray.point,scale(normal,ray.bias));
    const V reflected=reflected_liquid_oracle::reflect(direction,normal);
    const V tangent=unit(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    const unsigned lobe=unsigned(c.settings[1]);
    ray.outgoing=unit(add(reflected,scale(add(scale(tangent,taps[lobe][0]),scale(bitangent,taps[lobe][1])),double(c.settings[0])*c.settings[0])));
    if(dot(ray.outgoing,normal)<=0) ray.outgoing=reflected;
    ray.valid=true;return ray;
}
// Independent arbitrary-origin old-liquid sample. It deliberately does NOT
// call the production shared wave, forward-path or inverse implementations.
bool advance_liquid(Ray& ray,const Case& c,bool flat=false,bool localFootprint=false,bool noBias=false) {
    const auto f=liquid_frame(c);const double den=dot(ray.outgoing,f.normal);
    if(std::abs(den)<=1.e-8) return false;
    const double distance=dot(sub(f.point,ray.origin),f.normal)/den;
    if(distance<=ray.bias || distance>=65536) return false;
    ray.point=add(ray.origin,scale(ray.outgoing,distance));
    V p=add(reflected_liquid_oracle::row(ray.point,f),f.offset);
    const double footprint=(localFootprint?distance:std::sqrt(dot(ray.point,ray.point)))
        /std::max(f.projection[0],1.)/std::max(std::abs(den),.04);
    double dx=.055*std::cos(p[0]*.018+p[2]*.011-f.time*.8)*reflected_liquid_oracle::band(footprint,.022)
        +.025*std::cos(p[0]*.047-p[2]*.025+f.time*1.2)*reflected_liquid_oracle::band(footprint,.054);
    double dz=.045*std::cos(p[2]*.022-p[0]*.009-f.time*.65)*reflected_liquid_oracle::band(footprint,.024)
        -.020*std::cos(p[0]*.047-p[2]*.025+f.time*1.2)*reflected_liquid_oracle::band(footprint,.054);
    if(f.material==3 && !flat) {
        auto l=reflected_liquid_oracle::lava(p[0],p[2],f.time,footprint);
        const V travel=reflected_liquid_oracle::row(ray.outgoing,f);
        const double shift=std::clamp(-l[0]/std::max(travel[1],.12),-distance*.2,distance*.2);
        p=add(p,scale(travel,shift));ray.point=add(ray.point,scale(ray.outgoing,shift));
        l=reflected_liquid_oracle::lava(p[0],p[2],f.time,footprint);dx=l[1];dz=l[2];
    }
    V normal=flat?unit(f.normal):unit(reflected_liquid_oracle::col({dx,-1,dz},f));
    if(dot(normal,ray.outgoing)>0) normal=scale(normal,-1);
    ray.bias=std::max(.05,distance*1.e-5);ray.origin=noBias?ray.point:add(ray.point,scale(normal,ray.bias));
    ray.outgoing=reflected_liquid_oracle::reflect(ray.outgoing,normal);return true;
}
bool advance_plane(Ray& ray,Plane p,bool noBias=false) {
    V normal=plane_normal(p);if(dot(normal,ray.outgoing)>0) normal=scale(normal,-1);
    const double den=dot(ray.outgoing,normal);if(std::abs(den)<=1.e-12) return false;
    const double distance=dot(sub(xyz(p.a),ray.origin),normal)/den;
    if(distance<=ray.bias || distance>=65536) return false;
    ray.point=add(ray.origin,scale(ray.outgoing,distance));if(!inside(p,ray.point)) return false;
    ray.bias=std::max(.05,distance*1.e-5);ray.origin=noBias?ray.point:add(ray.point,scale(normal,ray.bias));
    ray.outgoing=reflected_liquid_oracle::reflect(ray.outgoing,normal);return true;
}
Ray forward(const Case& c,double x,double y,bool flat=false,bool localFootprint=false,bool noBias=false) {
    auto ray=primary(c,x,y,flat);if(!ray.valid) return ray;
    for(unsigned hop=0;hop<c.control[0];++hop) {
        const bool ok=(c.control[1]&(1U<<hop))?advance_liquid(ray,c,flat,localFootprint,noBias):advance_plane(ray,c.planes[hop],noBias);
        if(!ok) {ray.valid=false;break;}
    }
    return ray;
}
double error(const Case& c,const Ray& ray) {
    if(!ray.valid) return INFINITY;
    V travel=c.terminal[3]==0?xyz(c.terminal):sub(xyz(c.terminal),ray.origin);
    if(dot(travel,travel)<=ray.bias*ray.bias) return INFINITY;
    travel=unit(travel);if(dot(travel,ray.outgoing)<=0) return INFINITY;
    const V difference=cross(travel,ray.outgoing);
    return std::sqrt(dot(difference,difference))*std::max(c.projection[0],c.projection[1]);
}
struct Gpu {
    SDL_GPUDevice* device{};std::array<SDL_GPUComputePipeline*,2> pipelines{};SDL_GPUBuffer *input{},*output{},*trig{};
    SDL_GPUTransferBuffer *upload{},*download{},*trigUpload{};
    ~Gpu() {
        if(device) {
            SDL_WaitForGPUIdle(device);
            for(auto* pipeline:pipelines) if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
            if(input) SDL_ReleaseGPUBuffer(device,input);
            if(output) SDL_ReleaseGPUBuffer(device,output);
            if(trig) SDL_ReleaseGPUBuffer(device,trig);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
            if(trigUpload) SDL_ReleaseGPUTransferBuffer(device,trigUpload);
            SDL_DestroyGPUDevice(device);
        }
        SDL_Quit();
    }
    std::vector<Result> run(std::string_view backend,const std::vector<Case>& cases,
        const std::vector<std::uint32_t>& referenceGuide) {
        check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        device=create_reflected_curved_device(std::string(backend).c_str());check(device,SDL_GetError());
        require_reflected_curved_precision(device,std::string(backend).c_str());
#if defined(SDL_PROP_GPU_DEVICE_NAME_STRING)
        const auto properties=SDL_GetGPUDeviceProperties(device);
        std::cerr<<"curved checker device="<<SDL_GetStringProperty(properties,SDL_PROP_GPU_DEVICE_NAME_STRING,"unreported")
            <<" driver="<<SDL_GetStringProperty(properties,SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING,"unreported")<<std::endl;
#endif
        const bool vk=backend=="vulkan";SDL_GPUComputePipelineCreateInfo shader{};
        const std::array<const unsigned char*,2> code=vk
            ?std::array{reflected_curved_path_guide_spirv,reflected_curved_path_witness_spirv}
            :std::array{reflected_curved_path_guide_dxil,reflected_curved_path_witness_dxil};
        const std::array<std::size_t,2> sizes=vk
            ?std::array{sizeof(reflected_curved_path_guide_spirv),sizeof(reflected_curved_path_witness_spirv)}
            :std::array{sizeof(reflected_curved_path_guide_dxil),sizeof(reflected_curved_path_witness_dxil)};
        shader.entrypoint="main";shader.format=vk?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.num_readonly_storage_buffers=2;shader.num_readwrite_storage_buffers=shader.num_uniform_buffers=1;
        shader.threadcount_x=64;shader.threadcount_y=shader.threadcount_z=1;
        const auto started=std::chrono::steady_clock::now();
        const auto phase=[&](const char* label,unsigned completed) {
            std::cerr<<"curved checker "<<backend<<" "<<label<<" "<<completed<<"/"<<cases.size()
                <<" elapsed="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"s"<<std::endl;
        };
        for(unsigned stage=0;stage<pipelines.size();++stage) {
            shader.code=code[stage];shader.code_size=sizes[stage];
            if(stage==0 && !referenceGuide.empty()) {
                check(referenceGuide.front()==(vk?0x07230203U:0x43425844U),"Comparison guide format does not match backend");
                shader.code=reinterpret_cast<const Uint8*>(referenceGuide.data());
                shader.code_size=referenceGuide.size()*sizeof(std::uint32_t);
            }
            phase(stage==0?"guide-pipeline-create-begin":"witness-pipeline-create-begin",0);
            pipelines[stage]=SDL_CreateGPUComputePipeline(device,&shader);check(pipelines[stage],SDL_GetError());
            phase(stage==0?"guide-pipeline-create-complete":"witness-pipeline-create-complete",0);
        }
        const unsigned inBytes=unsigned(cases.size()*sizeof(Case)),outBytes=unsigned(cases.size()*sizeof(Result));
        const SDL_GPUBufferCreateInfo ii{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,inBytes,0},oi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,outBytes,0};
        input=SDL_CreateGPUBuffer(device,&ii);output=SDL_CreateGPUBuffer(device,&oi);check(input&&output,SDL_GetError());
        constexpr unsigned trigBytes=1024*4*sizeof(float);
        const SDL_GPUBufferCreateInfo ti{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,trigBytes,0};
        const SDL_GPUTransferBufferCreateInfo tui{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,trigBytes,0};
        trig=SDL_CreateGPUBuffer(device,&ti);trigUpload=SDL_CreateGPUTransferBuffer(device,&tui);check(trig&&trigUpload,SDL_GetError());
        auto* trigWords=static_cast<float*>(SDL_MapGPUTransferBuffer(device,trigUpload,false));check(trigWords,SDL_GetError());
        for(int i=0;i<1024;++i) {
            const double phase=(i<=512?i:i-1024)*(2*std::numbers::pi/1024);
            const double sine=std::sin(phase),cosine=std::cos(phase);
            const float sh=float(sine),ch=float(cosine);
            trigWords[i*4]=sh;trigWords[i*4+1]=float(sine-sh);
            trigWords[i*4+2]=ch;trigWords[i*4+3]=float(cosine-ch);
        }
        SDL_UnmapGPUTransferBuffer(device,trigUpload);
        const SDL_GPUTransferBufferCreateInfo ui{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,inBytes,0},di{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,outBytes,0};
        upload=SDL_CreateGPUTransferBuffer(device,&ui);download=SDL_CreateGPUTransferBuffer(device,&di);check(upload&&download,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(device,upload,false);check(mapped,SDL_GetError());
        std::memcpy(mapped,cases.data(),inBytes);SDL_UnmapGPUTransferBuffer(device,upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(device);check(cmd,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(cmd);check(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{upload,0};const SDL_GPUBufferRegion to{input,0,inBytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);
        const SDL_GPUTransferBufferLocation trigFrom{trigUpload,0};const SDL_GPUBufferRegion trigTo{trig,0,trigBytes};
        SDL_UploadToGPUBuffer(copy,&trigFrom,&trigTo,false);SDL_EndGPUCopyPass(copy);
        // Bounded diagnostic submissions avoid turning the expanded optical
        // sweep into one long non-preemptible desktop dispatch. This waiting
        // policy belongs to the checker, never the normal rendering pipeline.
        for(unsigned first=0;first<cases.size();first+=512) {
            const unsigned end=std::min(first+512,unsigned(cases.size()));
            SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=output;
            const std::array<unsigned,4> constants{unsigned(cases.size()),first,end,0};
            for(auto* pipeline:pipelines) {
                // Separate passes retain SDL's storage write/read barriers.
                // Witnesses read this batch's exact newly produced guide.
                auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);check(pass,SDL_GetError());
                const std::array<SDL_GPUBuffer*,2> reads{input,trig};
                SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,reads.data(),unsigned(reads.size()));
                SDL_PushGPUComputeUniformData(cmd,0,constants.data(),sizeof(constants));
                SDL_DispatchGPUCompute(pass,(end-first+63)/64,1,1);SDL_EndGPUComputePass(pass);
            }
            if(end==cases.size()) {
                copy=SDL_BeginGPUCopyPass(cmd);check(copy,SDL_GetError());
                const SDL_GPUBufferRegion source{output,0,outBytes};const SDL_GPUTransferBufferLocation dest{download,0};
                SDL_DownloadFromGPUBuffer(copy,&source,&dest);SDL_EndGPUCopyPass(copy);
            }
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);check(fence,SDL_GetError());
            const bool complete=SDL_WaitForGPUFences(device,true,&fence,1);
            SDL_ReleaseGPUFence(device,fence);check(complete,SDL_GetError());
            if(end==cases.size() || end%8192==0) phase("dispatch-complete",end);
            if(end<cases.size()) {cmd=SDL_AcquireGPUCommandBuffer(device);check(cmd,SDL_GetError());}
        }
        const auto* result=static_cast<const Result*>(SDL_MapGPUTransferBuffer(device,download,false));check(result,SDL_GetError());
        std::vector<Result> values(result,result+cases.size());SDL_UnmapGPUTransferBuffer(device,download);return values;
    }
};
}
int main(int argc,char** argv) try {
    const std::string_view backend=argc>1?argv[1]:"direct3d12";
    check(backend=="direct3d12" || backend=="vulkan","Expected direct3d12 or vulkan");
    // Opt-in diagnostic comparison only. Both versions must independently pass
    // the entire unchanged binary64 oracle before any dump/comparison is made.
    std::filesystem::path dumpPath,comparePath,guidePath;
    for(int arg=2;arg<argc;++arg) {
        const std::string_view option=argv[arg];check(arg+1<argc,"Missing diagnostic option path");
        if(option=="--dump") {check(dumpPath.empty(),"Duplicate dump");dumpPath=argv[++arg];}
        else if(option=="--compare") {check(comparePath.empty(),"Duplicate comparison");comparePath=argv[++arg];}
        else if(option=="--guide") {check(guidePath.empty(),"Duplicate guide");guidePath=argv[++arg];}
        else check(false,"Expected --dump, --compare or --guide followed by a path");
    }
    const auto referenceGuide=guidePath.empty()?std::vector<std::uint32_t>{}:binary_words(guidePath);
    std::vector<Case> cases;std::vector<bool> invalid;
    std::array<std::array<std::array<std::array<unsigned,2>,5>,2>,2> samples{},accepted{},roundtrip{};
    for(unsigned material:{0U,3U}) for(unsigned primaryLiquid=0;primaryLiquid<2;++primaryLiquid)
    for(unsigned pose=0;pose<10;++pose) for(unsigned eye=0;eye<2;++eye)
    for(float rough:{0.F,.15F,.4F,.75F}) for(unsigned lobe=0;lobe<8;++lobe)
    for(unsigned point=0;point<15;++point) for(unsigned hops=1;hops<=4;++hops)
    for(unsigned environment=0;environment<2;++environment) {
        if(primaryLiquid && (rough!=0 || lobe!=0)) continue;
        constexpr unsigned widths[]{96,320,640,1280,4096};const unsigned w=widths[pose/2],h=w*3/4;
        Case c{};c.projection={float(w*.89),float(h*1.13),float(w*(eye?.527:.479)),float(h*.46)};
        c.extent={float(w),float(h),1,65536};c.settings=primaryLiquid?F{}:F{rough,float(lobe),0,0};
        c.control={hops,0,primaryLiquid,material==3?1U:0U};
        const double angle=(int(pose)%5-2)*.04,s=std::sin(angle),co=std::cos(angle);
        c.liquid[0]={float((int(pose)-4)*3),96,30,0};
        // Quantized affine scale/shear, not an assumed orthonormal transform.
        c.liquid[2]={float(co*1.0004),float(-s),.001F,float(eye?7.25:-5.5)};
        c.liquid[3]={float(s),float(co*.9997),.002F,float(int(pose)*4-17)};
        c.liquid[4]={-.001F,0,1.0002F,float(int(pose)*11-31)};
        c.liquid[1]=words(unit(reflected_liquid_oracle::col({0,-1,0},liquid_frame(c))),0);
        c.liquid[5]=c.projection;c.liquid[6]=c.extent;c.liquid[7]={float(.75+pose*.31),float(material),0,0};
        if(!primaryLiquid) {
            const auto face=plane_at({eye?9.:-7.,-45.,160.+pose*7},unit({.09,.46,-1}),1800.);
            c.a=face.a;c.b=face.b;c.c=face.c;
        }
        // Retain the original three witnesses, then add DISTINCT receiver
        // locations. Repeated lava/primary-liquid four-hop paths had only ten
        // constructed samples: a >15 positive gate was mathematically
        // impossible, not a reason to lower its acceptance threshold.
        const double px=point<3?.25+point*.23:.285+((point-3)%4)*.137;
        const double py=point<3?.63+point*.10:.647+((point-3)/4)*.073;
        c.source={float(w*px),float(h*py),0,0};
        // Same bounded, non-exact transport perturbation for the new grid;
        // do not provide the expected source pixel to the inverse solver.
        const unsigned perturbation=point<3?point+1:1+(point-3)%3;
        c.seed={c.source[0]+float(perturbation*1.75),c.source[1]-float(perturbation*1.125),0,0};
        auto ray=primary(c,c.source[0],c.source[1]);if(!ray.valid) continue;
        bool valid=true;
        for(unsigned hop=0;hop<hops;++hop) {
            const bool liquid=primaryLiquid?(hop%2==1):(hop%2==0);
            if(liquid) {
                c.control[1]|=1U<<hop;
                if(!advance_liquid(ray,c)) {valid=false;break;}
            } else {
                double distance=180.+point*23+hop*31;
                const auto f=liquid_frame(c);const double den=dot(ray.outgoing,f.normal);
                if(std::abs(den)>1.e-8) {
                    const double toFloor=dot(sub(f.point,ray.origin),f.normal)/den;
                    if(toFloor>ray.bias) distance=std::min(distance,toFloor*.45);
                }
                const V centre=add(ray.origin,scale(ray.outgoing,distance));
                const V desired=unit({.25*std::sin(double(pose+hop+point+1)),.8,.35*std::cos(double(lobe+1))});
                const V n=unit(sub(ray.outgoing,desired));
                c.planes[hop]=plane_at(centre,n,2400.);
                if(!advance_plane(ray,c.planes[hop])) {valid=false;break;}
            }
        }
        if(!valid) continue;
        c.terminal=environment?words(ray.outgoing,0):words(add(ray.origin,scale(ray.outgoing,320.+point*91)),1);
        cases.push_back(c);invalid.push_back(false);++samples[material==3][primaryLiquid][hops][environment];
    }
    const unsigned validCases=unsigned(cases.size());check(validCases>20000,"Insufficient actual curved mixed-path coverage");
    const auto findBase=[&](bool primaryLiquid) {
        const auto found=std::find_if(cases.begin(),cases.end(),[&](const Case& c){return c.control[0]==4 && c.control[2]==unsigned(primaryLiquid) && c.terminal[3]==1;});
        check(found!=cases.end(),"Actual repeated-liquid four-hop fixture missing");return *found;
    };
    const auto modelBase=findBase(false),liquidBase=findBase(true);
    constexpr unsigned faults=41;
    for(unsigned fault=0;fault<faults;++fault) {
        Case c=fault>=26?liquidBase:modelBase;
        const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
        switch(fault) {
        case 0:c.control[0]=5;break;case 1:c.control[1]|=16;break;case 2:c.control[2]=2;break;
        case 3:c.terminal[3]=2;break;case 4:c.terminal[0]=nan;break;case 5:c.terminal[2]=1.e15F;break;
        case 6:c.terminal={};break;case 7:c.a[3]=0;break;case 8:c.b=c.c;break;
        case 9:c.settings[0]=-1;break;case 10:c.settings[1]=8;break;case 11:c.settings[2]=1;break;
        case 12:c.liquid[1]={};break;case 13:c.liquid[0][0]=inf;break;
        case 14:c.liquid[2]={};break;case 15:c.liquid[3][1]=nan;break;
        case 16:c.liquid[5][0]+=1;break;case 17:c.liquid[6][2]+=1;break;
        case 18:c.liquid[7][0]=-1;break;case 19:c.liquid[7][1]=1;break;case 20:c.liquid[7][2]=1;break;
        case 21:c.planes[1].a[3]=0;break;case 22:c.planes[1].c=c.planes[1].b;break;
        case 23:c.planes[3].b[1]=inf;break;
        case 24:case 25:{
            auto& p=c.planes[fault==24?1:3];const V move=scale(unit(sub(xyz(p.b),xyz(p.a))),1.e6);
            p.a=words(add(xyz(p.a),move),1);p.b=words(add(xyz(p.b),move),1);p.c=words(add(xyz(p.c),move),1);break;
        }
        case 26:c.settings[0]=.1F;break;case 27:c.settings[1]=1;break;
        case 28:c.liquid[5][0]=0;break;case 29:c.liquid[6][0]=16385;break;
        case 30:c.liquid[6][3]=c.liquid[6][2];break;case 31:c.liquid[7][0]=nan;break;
        case 32:c.liquid[7][3]=1;break;case 33:c.liquid[7][1]=2;break;
        case 34:c.planes[0].a[3]=0;break;case 35:c.planes[2].c=c.planes[2].b;break;
        case 36:{auto& p=c.planes[2];p.a[0]+=1.e6F;p.b[0]+=1.e6F;p.c[0]+=1.e6F;break;}
        case 37:c.liquid[2][3]=2097152;break;
        case 38:c.liquid[4][3]=-2097152;break;
        case 39:c.liquid[7][0]=2097152;break;
        case 40:c.liquid[2][0]=1.e10F;break;
        }
        cases.push_back(c);invalid.push_back(true);
    }
    std::cout<<validCases<<" independently constructed water/lava mixed paths; running "<<backend<<"\n";
    Gpu gpu;const auto results=gpu.run(backend,cases,referenceGuide);
    unsigned flatSensitive{},footprintSensitive{},biasSensitive{},otherRoots{},acceptedTotal{};
    unsigned forwardFailures{},inverseFailures{};
    double maximumError{},maximumForward{},maximumKnown{};
    for(unsigned i=0;i<cases.size();++i) {
        const auto& c=cases[i];const auto& r=results[i];
        for(float v:r.motion) check(std::isfinite(v),"Nonfinite curved guide");
        if(invalid[i]) {
            if(r.motion!=F{}) throw std::runtime_error("Malformed/off-face curved path accepted: fault="+std::to_string(i-validCases));
            continue;
        }
        const auto ray=forward(c,c.source[0],c.source[1]);check(ray.valid,"Independent actual path became invalid");
        bool forwardFailed=false;
        for(unsigned k=0;k<3;++k) {
            const double miss=std::abs(r.outgoing[k]-ray.outgoing[k]);maximumForward=std::max(maximumForward,miss);
            if(miss>1.e-4 || std::abs(r.origin[k]-ray.origin[k])>std::max(.03,std::sqrt(dot(ray.origin,ray.origin))*3.e-5)) {
                forwardFailed=true;
                if(forwardFailures>=2) continue;
                std::cerr<<std::setprecision(12);
                std::cerr<<"curved witness material/primary/hops/mask="<<c.liquid[7][1]<<'/'<<c.control[2]<<'/'<<c.control[0]<<'/'<<c.control[1]
                    <<" source="<<c.source[0]<<','<<c.source[1]<<" rough/lobe="<<c.settings[0]<<'/'<<c.settings[1]
                    <<" time="<<c.liquid[7][0]<<" axis="<<k<<" origin actual/expected="<<r.origin[k]<<'/'<<ray.origin[k]
                    <<" depth="<<ray.depth<<" direction actual/expected="<<r.outgoing[k]<<'/'<<ray.outgoing[k]<<'\n';
                auto path=primary(c,c.source[0],c.source[1]);
                for(unsigned hop=0;hop<=c.control[0];++hop) {
                    if(hop) {if(c.control[1]&(1U<<(hop-1))) advance_liquid(path,c);else advance_plane(path,c.planes[hop-1]);}
                    std::cerr<<"hop "<<hop<<" origin ";
                    for(unsigned axis=0;axis<3;++axis) std::cerr<<r.trace[hop*2][axis]<<'/'<<path.origin[axis]<<' ';
                    std::cerr<<"direction ";
                    for(unsigned axis=0;axis<3;++axis) std::cerr<<r.trace[hop*2+1][axis]<<'/'<<path.outgoing[axis]<<' ';
                    std::cerr<<'\n';
                }
                std::cerr<<"Curved forward path mismatch: case="<<i<<" miss="<<miss<<'\n';
            }
        }
        forwardFailures+=forwardFailed;
        check(std::abs(r.origin[3]-ray.bias)<.0001 && std::abs(r.outgoing[3]-ray.depth)<std::max(.02,ray.depth*1.e-5),
            "Curved path lost primary depth/per-hop bias");
        flatSensitive+=error(c,forward(c,c.source[0],c.source[1],true))>.1;
        footprintSensitive+=error(c,forward(c,c.source[0],c.source[1],false,true))>.015;
        biasSensitive+=error(c,forward(c,c.source[0],c.source[1],false,false,true))>.015;
        if(r.motion[3]==0) {check(r.motion==F{},"Rejected curved guide has partial values");continue;}
        check(r.motion[3]==1 && r.residual[3]==1,"Noncanonical curved guide validity");
        const auto reconstructed=forward(c,r.motion[0],r.motion[1]);const double miss=error(c,reconstructed);
        maximumError=std::max(maximumError,miss);
        if(!reconstructed.valid || miss>.015) {
            if(inverseFailures<4) std::cerr<<"Curved inverse actual-path mismatch: case="<<i<<" error="<<miss
                <<" material/primary/hops/mask="<<c.liquid[7][1]<<'/'<<c.control[2]<<'/'<<c.control[0]<<'/'<<c.control[1]
                <<" seed="<<c.seed[0]<<','<<c.seed[1]<<" source="<<c.source[0]<<','<<c.source[1]
                <<" guide="<<r.motion[0]<<','<<r.motion[1]<<'\n';
            ++inverseFailures;
            continue; // Not a positive optical path/round trip or a valid alternate root.
        }
        const unsigned material=c.liquid[7][1]==3,primaryLiquid=c.control[2],hops=c.control[0],environment=c.terminal[3]==0;
        ++accepted[material][primaryLiquid][hops][environment];++acceptedTotal;
        const double pixel=std::max(std::abs(r.motion[0]-c.source[0]),std::abs(r.motion[1]-c.source[1]));
        if(pixel<=.1) {++roundtrip[material][primaryLiquid][hops][environment];maximumKnown=std::max(maximumKnown,pixel);}else ++otherRoots;
        check(std::abs(r.motion[2]-reconstructed.depth)<std::max(.02,reconstructed.depth*1.e-5),"Curved guide replaced primary analytic depth");
    }
    unsigned coverageFailures{};
    for(unsigned material=0;material<2;++material) for(unsigned primaryLiquid=0;primaryLiquid<2;++primaryLiquid)
    for(unsigned hops=1;hops<=4;++hops) for(unsigned environment=0;environment<2;++environment) {
        const unsigned minimum=primaryLiquid?15:150;
        std::cout<<(material?"lava":"water")<<" / "<<(primaryLiquid?"liquid primary":"model primary")<<" / "<<hops
            <<" hops / "<<(environment?"environment":"finite")<<": "<<accepted[material][primaryLiquid][hops][environment]
            <<" / "<<samples[material][primaryLiquid][hops][environment]<<" accepted; "
            <<roundtrip[material][primaryLiquid][hops][environment]<<" known-source round trips\n";
        if(samples[material][primaryLiquid][hops][environment]<=minimum || accepted[material][primaryLiquid][hops][environment]<=minimum
            || roundtrip[material][primaryLiquid][hops][environment]<=minimum) ++coverageFailures;
    }
    check(flatSensitive>1000 && footprintSensitive>100 && biasSensitive>100,"Fixtures did not distinguish flat/local-footprint/unbiased approximations");
    std::cout<<acceptedTotal<<" accepted complete optical paths; "<<faults<<" malformed/off-face exclusions; max forward/error/known pixel "
        <<maximumForward<<" / "<<maximumError<<" / "<<maximumKnown<<"; flat/footprint/bias sensitive "
        <<flatSensitive<<" / "<<footprintSensitive<<" / "<<biasSensitive<<"; other independently valid roots "<<otherRoots
        <<"; strict forward/inverse/positive-coverage failures "<<forwardFailures<<'/'<<inverseFailures<<'/'<<coverageFailures
        <<". Optical guide only: native ordered witnesses, visibility, additive light and owner integration remain required.\n";
    check(forwardFailures==0 && inverseFailures==0 && coverageFailures==0,
        "Curved optical precision/coverage acceptance FAILED; no tolerance or failed-case exception");
    if(!comparePath.empty()) {
        const auto reference=binary_words(comparePath);
        check(reference.size()*sizeof(std::uint32_t)==results.size()*sizeof(Result),"Comparison result ABI/count mismatch");
        unsigned different{};
        for(std::size_t i=0;i<results.size();++i)
            if(std::memcmp(&results[i],reference.data()+i*sizeof(Result)/4,sizeof(Result))!=0) ++different;
        std::cout<<"Exact 224-byte reference comparison: "<<different<<" different of "<<results.size()<<" cases.\n";
        check(different==0,"Opt-in exact reference comparison failed");
    }
    if(!dumpPath.empty()) {
        check(!std::filesystem::exists(dumpPath),"Refusing to overwrite a previous diagnostic dump");
        std::ofstream stream(dumpPath,std::ios::binary|std::ios::out);
        stream.write(reinterpret_cast<const char*>(results.data()),std::streamsize(results.size()*sizeof(Result)));
        stream.close();check(bool(stream),"Unable to save complete checked diagnostic results");
        std::cout<<"Saved complete checked diagnostic results.\n";
    }
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
