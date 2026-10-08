// Real GPU compute, independent double-precision forward optics. Only this
// diagnostic owns a transfer/readback/fence; the runtime helper contains none.
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include "reflected_liquid_motion_dxil.hpp"
#include "reflected_liquid_motion_spirv.hpp"
namespace {
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
using V=std::array<double,3>;
using F=std::array<float,4>;
V add(V a,V b) {return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
V sub(V a,V b) {return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V scale(V a,double f) {return {a[0]*f,a[1]*f,a[2]*f};}
double dot(V a,V b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V cross(V a,V b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
V normal(V a) {return scale(a,1/std::sqrt(dot(a,a)));}
V xyz(F a) {return {a[0],a[1],a[2]};}
F words(V a,float w=0) {return {float(a[0]),float(a[1]),float(a[2]),w};}
double band(double f,double frequency) {const auto x=f*frequency;return 1/(1+x*x*x*x);}
double hash(int x,int z) {
    std::uint32_t n=std::uint32_t(x)*1597334677u ^ std::uint32_t(z)*3812015801u;
    n^=n>>16;n*=2246822519u;n^=n>>13;return double(n&65535)/65535;
}
// Geometry only: no colour/heat/noise code from the shared implementation.
std::array<double,4> lava(double x,double z,double t,double footprint) {
    const double bend=x*.006-z*.008+t*.11,filter=band(footprint,.010);
    const double a=x*.018+z*.011-t*.45+.65*std::sin(bend)*filter;
    const double b=x*.047-z*.025+t*.60,c=z*.022-x*.009-t*.32;
    const double fa=band(footprint,.022),fb=band(footprint,.054),fc=band(footprint,.024);
    double h=9*std::sin(a)*fa+2.5*std::sin(b)*fb+5*std::sin(c)*fc;
    double dx=9*(.018+.0039*std::cos(bend)*filter)*std::cos(a)*fa+.1175*std::cos(b)*fb-.045*std::cos(c)*fc;
    double dz=9*(.011-.0052*std::cos(bend)*filter)*std::cos(a)*fa-.0625*std::cos(b)*fb+.110*std::cos(c)*fc;
    const double ripple=x*.173+z*.129-t*.73,fr=band(footprint,.216);
    h+=.38*std::sin(ripple)*fr;dx+=.06574*std::cos(ripple)*fr;dz+=.04902*std::cos(ripple)*fr;
    double bubbleHeight{};
    const int cx=int(std::floor(x/128)),cz=int(std::floor(z/128));const double seed=hash(cx,cz);
    if(seed>.64 && footprint<24) {
        const double bx=x/128-cx-(.28+.44*hash(cx+19,cz)),bz=z/128-cz-(.28+.44*hash(cx,cz+29));
        const double phase=t*.14+seed*7-std::floor(t*.14+seed*7),life=std::pow(std::sin(phase*3.14159265),2);
        const double radius=.055+.14*phase,dome=std::clamp(1-(bx*bx+bz*bz)/(radius*radius),0.,1.);
        const double amplitude=10*life*band(footprint,.18),derivative=-6*amplitude*dome*dome/(128*radius*radius);
        bubbleHeight=amplitude*dome*dome*dome;h+=bubbleHeight;dx+=derivative*bx;dz+=derivative*bz;
    }
    return {h,dx,dz,bubbleHeight};
}
struct Case {std::array<F,8> frame;F a,b,c,bary,source;};
static_assert(sizeof(Case)==208);
struct Result {F motion,hit,normal,seed;std::array<F,48> trace;};
static_assert(sizeof(Result)==832);
// Independent pivoted solve of R^T * previous = world-offset. This is not
// the shader's cofactor inverse and detects row/column/scale mistakes.
F transported_seed(const Case& c,unsigned index) {
    const V now{31.25+.125*(index%7),94.75,320.5+(index%5)*7.25};
    const V world{now[0]*double(.9998F)-now[1]*double(.019998F)+7.25,
        now[0]*double(.019998F)+now[1]*double(.9998F)-11.5,now[2]*double(1.0004F)+19.75};
    double augmented[3][4]{};
    for(unsigned r=0;r<3;++r) {
        for(unsigned column=0;column<3;++column) augmented[r][column]=c.frame[2+column][r];
        augmented[r][3]=world[r]-c.frame[2+r][3];
    }
    for(unsigned column=0;column<3;++column) {
        unsigned pivot=column;
        for(unsigned r=column+1;r<3;++r) if(std::abs(augmented[r][column])>std::abs(augmented[pivot][column])) pivot=r;
        for(unsigned j=0;j<4;++j) std::swap(augmented[pivot][j],augmented[column][j]);
        const double divisor=augmented[column][column];check(std::abs(divisor)>1.e-12,"Independent seed matrix singular");
        for(unsigned j=0;j<4;++j) augmented[column][j]/=divisor;
        for(unsigned r=0;r<3;++r) if(r!=column) {
            const double scale=augmented[r][column];
            for(unsigned j=0;j<4;++j) augmented[r][j]-=scale*augmented[column][j];
        }
    }
    if(augmented[2][3]<=0) return {-1,-1,0,0};
    return {float(c.frame[5][0]*augmented[0][3]/augmented[2][3]+c.frame[5][2]),
        float(c.frame[5][1]*augmented[1][3]/augmented[2][3]+c.frame[5][3]),0,0};
}
V row_mul(V v,const Case& c) {
    V r{};for(unsigned j=0;j<3;++j) for(unsigned i=0;i<3;++i) r[j]+=v[i]*c.frame[2+i][j];return r;
}
V col_mul(V v,const Case& c) {
    V r{};for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j) r[i]+=c.frame[2+i][j]*v[j];return r;
}
struct Ray {V direction,hit,normal;double bias,depth,bubbleHeight;bool valid{};};
Ray optical(const Case& c,double px,double py) {
    const auto& p=c.frame[5];const auto& e=c.frame[6];
    Ray ray{};
    if(px<0 || py<0 || px>=e[0] || py>=e[1]) return ray;
    ray.direction=normal({(px-p[2])/p[0],(py-p[3])/p[1],1});
    const V n=xyz(c.frame[1]);const double den=dot(ray.direction,n);
    if(std::abs(den)<1.e-12) return ray;
    const double distance=dot(xyz(c.frame[0]),n)/den;ray.depth=distance*ray.direction[2];
    if(distance<=0 || ray.depth<e[2] || ray.depth>e[3]) return ray;
    ray.hit=scale(ray.direction,distance);
    V pos=add(row_mul(ray.hit,c),{c.frame[2][3],c.frame[3][3],c.frame[4][3]});
    const double f=distance/std::max(double(p[0]),1.)/std::max(std::abs(den),.04),t=c.frame[7][0];
    double dx=.055*std::cos(pos[0]*.018+pos[2]*.011-t*.8)*band(f,.022)
        +.025*std::cos(pos[0]*.047-pos[2]*.025+t*1.2)*band(f,.054);
    double dz=.045*std::cos(pos[2]*.022-pos[0]*.009-t*.65)*band(f,.024)
        -.020*std::cos(pos[0]*.047-pos[2]*.025+t*1.2)*band(f,.054);
    if(c.frame[7][1]==3) {
        auto l=lava(pos[0],pos[2],t,f);const V travel=row_mul(ray.direction,c);
        const double shift=std::clamp(-l[0]/std::max(travel[1],.12),-distance*.2,distance*.2);
        pos=add(pos,scale(travel,shift));ray.hit=add(ray.hit,scale(ray.direction,shift));
        l=lava(pos[0],pos[2],t,f);dx=l[1];dz=l[2];ray.bubbleHeight=l[3];
    }
    ray.normal=normal(col_mul({dx,-1,dz},c));if(dot(ray.normal,ray.direction)>0) ray.normal=scale(ray.normal,-1);
    ray.bias=std::max(.05,distance*1.e-5);ray.valid=true;return ray;
}
V feature(const Case& c) {return add(add(scale(xyz(c.a),1-c.bary[0]-c.bary[1]),scale(xyz(c.b),c.bary[0])),scale(xyz(c.c),c.bary[1]));}
double error(const Case& c,V target,double px,double py) {
    const auto ray=optical(c,px,py);if(!ray.valid) return INFINITY;
    V to=sub(target,add(ray.hit,scale(ray.normal,ray.bias)));
    if(dot(to,to)<=ray.bias*ray.bias) return INFINITY;
    const auto outgoing=normal(to),incoming=sub(outgoing,scale(ray.normal,2*dot(outgoing,ray.normal)));
    if(incoming[2]<=0) return INFINITY;
    const auto& p=c.frame[5];
    return std::max(std::abs(p[0]*incoming[0]/incoming[2]+p[2]-px),std::abs(p[1]*incoming[1]/incoming[2]+p[3]-py));
}
std::array<double,2> flat(const Case& c,V target) {
    const V n=normal(xyz(c.frame[1])),v=sub(target,scale(n,2*dot(sub(target,xyz(c.frame[0])),n)));
    return {c.frame[5][0]*v[0]/v[2]+c.frame[5][2],c.frame[5][1]*v[1]/v[2]+c.frame[5][3]};
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
    std::vector<Result> run(std::string_view backend,const std::vector<Case>& cases,unsigned receiverSeed=0) {
        check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_SPIRV,true,std::string(backend).c_str());check(device,SDL_GetError());
        const bool vk=backend=="vulkan";
        SDL_GPUComputePipelineCreateInfo shader{};
        shader.code=vk?reflected_liquid_motion_spirv:reflected_liquid_motion_dxil;
        shader.code_size=vk?sizeof(reflected_liquid_motion_spirv):sizeof(reflected_liquid_motion_dxil);
        shader.entrypoint="main";shader.format=vk?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.num_readonly_storage_buffers=1;shader.num_readwrite_storage_buffers=1;shader.num_uniform_buffers=1;
        shader.threadcount_x=64;shader.threadcount_y=shader.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&shader);check(pipeline,SDL_GetError());
        const unsigned inBytes=unsigned(cases.size()*sizeof(Case)),outBytes=unsigned(cases.size()*sizeof(Result));
        const SDL_GPUBufferCreateInfo ii{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,inBytes,0};
        const SDL_GPUBufferCreateInfo oi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,outBytes,0};
        input=SDL_CreateGPUBuffer(device,&ii);output=SDL_CreateGPUBuffer(device,&oi);check(input&&output,SDL_GetError());
        const SDL_GPUTransferBufferCreateInfo ui{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,inBytes,0},di{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,outBytes,0};
        upload=SDL_CreateGPUTransferBuffer(device,&ui);download=SDL_CreateGPUTransferBuffer(device,&di);check(upload&&download,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(device,upload,false);check(mapped,SDL_GetError());
        std::memcpy(mapped,cases.data(),inBytes);SDL_UnmapGPUTransferBuffer(device,upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(device);check(cmd,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(cmd);check(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{upload,0};const SDL_GPUBufferRegion to{input,0,inBytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        SDL_GPUStorageBufferReadWriteBinding target{};target.buffer=output;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&target,1);check(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,&input,1);
        const std::array<unsigned,4> constants{unsigned(cases.size()),receiverSeed,0,0};
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
    check(backend=="direct3d12" || backend=="vulkan","Expected direct3d12 or vulkan");
    const bool localSeed=argc==3 && std::string_view(argv[2])=="--local-seed";
    if(argc==3 && !localSeed) {
        // Replay the exact old-frame/triangle/barycentric uint words printed
        // by the explicit runtime lava diagnostic; no decimal round-tripping.
        std::ifstream input(argv[2]);check(bool(input),"Cannot open liquid case-bits log");
        std::vector<Case> replay;std::string line;
        while(std::getline(input,line)) if(line.starts_with("case-bits ")) {
            std::istringstream words(line.substr(10));std::array<std::uint32_t,52> bits{};
            for(auto& value:bits) {words>>std::hex>>value;check(bool(words),"Truncated liquid case-bits record");}
            replay.push_back(std::bit_cast<Case>(bits));
        }
        check(!replay.empty(),"No exact liquid case-bits records found");
        Gpu gpu;const auto results=gpu.run(backend,replay,1);std::cout<<std::setprecision(10);
        for(unsigned i=0;i<replay.size();++i) {
            const auto& r=results[i];std::cout<<"Replay "<<i<<" motion";
            for(float value:r.motion) std::cout<<' '<<value;std::cout<<'\n';
            for(unsigned n=0;n<12;++n) {
                bool empty=true;for(unsigned s=0;s<4;++s) empty&=r.trace[n*4+s]==F{};
                if(empty) continue;
                std::cout<<"  iteration "<<n;
                for(unsigned s=0;s<4;++s) {std::cout<<" |";for(float v:r.trace[n*4+s]) std::cout<<' '<<v;}
                std::cout<<'\n';
            }
        }
        return 0;
    }
    std::vector<Case> cases;std::vector<bool> invalid;
    std::array<unsigned,2> samples{};
    for(unsigned material: {0u,3u}) for(unsigned variant=0;variant<8;++variant) for(unsigned eye=0;eye<2;++eye)
        for(unsigned y=0;y<9;++y) for(unsigned x=0;x<11;++x) {
            Case c{};const double roll=(int(variant)%3-1)*.075,pitch=(int(variant)%4-2)*.045;
            const double cr=std::cos(roll),sr=std::sin(roll),cp=std::cos(pitch),sp=std::sin(pitch),s=variant==7?1.0017:1;
            c.frame[2]={float(cr*s),float(-sr*cp*s),float(sr*sp*s),float((int(variant)-3)*73+(eye?6:-6))};
            c.frame[3]={float(sr),float(cr*cp),float(-cr*sp),float(variant*23)};
            c.frame[4]={0,float(sp),float(cp),float((int(variant)-4)*127)};
            const V n=normal(col_mul({0,-1,0},c));c.frame[1]=words(n);c.frame[0]=words(scale(n,-(90.+variant*27)));
            const unsigned w=variant<2?96:variant<4?256:variant<6?512:1280,h=w*3/4;
            c.frame[5]={float(w*.85),float(h*1.11),float(w*(eye?.517:.489)),float(h*.47)};
            c.frame[6]={float(w),float(h),5,65536};c.frame[7]={float(variant*2.91+eye*.13),float(material),0,0};
            c.source={float(w*(.13+x*.071)),float(h*(.57+y*.037)),0,0};
            const auto ray=optical(c,c.source[0],c.source[1]);if(!ray.valid) continue;
            const V reflected=sub(ray.direction,scale(ray.normal,2*dot(ray.direction,ray.normal)));
            const V target=add(add(ray.hit,scale(ray.normal,ray.bias)),scale(reflected,80+((x*31+y*23+variant*11)%211)*9));
            // A non-axis-aligned old feature triangle, with asymmetric barycentrics.
            c.bary={.25F,.35F,0,0};const V u{19,3,7},v{-4,15,5};
            c.a=words(sub(target,add(scale(u,c.bary[0]),scale(v,c.bary[1]))),1);
            c.b=words(add(xyz(c.a),u),1);c.c=words(add(xyz(c.a),v),1);
            cases.push_back(c);invalid.push_back(false);++samples[material==3];
        }
    check(cases.size()>100 && samples[0]>100 && samples[1]>100,"No independent physical liquid fixtures generated");
    // Deliberate bubble centres/lifetime boundaries, not an uncounted chance
    // that one broad grid ray happened to land on an active dome.
    for(int cell=-7;cell<8;++cell) if(hash(cell,2)>.64) for(unsigned phase=0;phase<7;++phase) {
        Case c=cases[100];c.frame[0]={0,120,0,0};c.frame[1]={0,-1,0,0};
        c.frame[2]={1,0,0,0};c.frame[3]={0,1,0,0};c.frame[4]={0,0,1,0};
        c.frame[5]={400,360,256,192};c.frame[6]={512,384,5,65536};c.source={241.25F,322.5F,0,0};
        const double seed=hash(cell,2),age=phase==0?1.e-5:phase==1?.08:phase==2?.3:phase==3?.5:phase==4?.8:phase==5?.99999:1.00001;
        c.frame[7]={float((8+age-seed*7)/.14),3,0,0};
        const V incident=normal({(c.source[0]-256)/400,(c.source[1]-192)/360,1});
        const V planeHit=scale(incident,120/incident[1]);
        c.frame[2][3]=float(128*(cell+.28+.44*hash(cell+19,2))-planeHit[0]);
        c.frame[4][3]=float(128*(2+.28+.44*hash(cell,31))-planeHit[2]);
        const auto ray=optical(c,c.source[0],c.source[1]);check(ray.valid,"Bubble fixture missed surface");
        const V reflected=sub(ray.direction,scale(ray.normal,2*dot(ray.direction,ray.normal)));
        const V target=add(add(ray.hit,scale(ray.normal,ray.bias)),scale(reflected,400));
        c.bary={.25F,.35F,0,0};const V u{19,3,7},v{-4,15,5};
        c.a=words(sub(target,add(scale(u,c.bary[0]),scale(v,c.bary[1]))),1);
        c.b=words(add(xyz(c.a),u),1);c.c=words(add(xyz(c.a),v),1);
        cases.push_back(c);invalid.push_back(false);++samples[1];
    }
    // All malformed inputs must produce exactly zero, not a flat-plane fallback.
    constexpr unsigned faults=34;
    for(unsigned fault=0;fault<faults;++fault) {
        Case c=cases[100];const float nan=std::numeric_limits<float>::quiet_NaN();
        switch(fault) {
        case 0:c.frame[7][0]=nan;break;case 1:c.frame[7][1]=1;break;case 2:c.frame[7][1]=4;break;
        case 3:c.frame[1]={};break;case 4:c.frame[2][1]=nan;break;case 5:c.frame[3]=c.frame[2];break;
        case 6:c.frame[5][0]=0;break;case 7:c.frame[5][1]=-1;break;case 8:c.frame[6][0]=0;break;
        case 9:c.frame[6][2]=-1;break;case 10:c.frame[6][3]=c.frame[6][2];break;
        case 11:c.frame[0][2]=nan;break;case 12:c.a[3]=0;break;case 13:c.c=c.b;break;
        case 14:c.bary[0]=-.1F;break;case 15:c.bary[1]=1;break;case 16:c.a[0]=nan;break;
        case 17:c.frame[6][0]=16385;break;case 18:c.frame[5][2]=nan;break;case 19:c.bary[0]=nan;break;
        case 20:c.frame[6][2]=.5F;c.frame[6][3]=1;break; // real plane beyond old far clip
        case 21:c.a[2]=c.b[2]=c.c[2]=-1.e6F;break; // behind the previous eye
        case 22:c.a[0]+=1.e7F;c.b[0]+=1.e7F;c.c[0]+=1.e7F;break; // outside old coverage
        case 23:c.frame[2][0]=std::numeric_limits<float>::infinity();break;
        case 24:c.frame[7][1]=.5F;break;
        case 25:c.frame[6][2]=nan;break;
        case 26:c.c[3]=.5F;break;
        case 27:c.frame[6][3]=std::numeric_limits<float>::infinity();break;
        case 28:c.frame[1][0]=1.e30F;break; // finite components but overflowing norm
        case 29:c.frame[2][3]=1.e15F;break; // unbounded world offset
        case 30:c.b[0]=1.e30F;c.c[1]=1.e30F;break; // overflowing feature-plane area
        case 31:c.frame[6][3]=1.e15F;break;
        case 32:c.frame[7][0]=-1;break;
        case 33:c.frame[7][0]=1.e15F;break;
        }
        cases.push_back(c);invalid.push_back(true);
    }
    Gpu gpu;const auto results=gpu.run(backend,cases,localSeed?2:0);
    std::cout<<(localSeed?"Displaced local receiver seed":"Flat-mirror seed")<<": ";
    std::array<unsigned,2> accepted{},sensitive{},same_root{},other_clock{};unsigned bubbles{};double max_error{},max_forward{};
    for(unsigned i=0;i<cases.size();++i) {
        const auto& c=cases[i];const auto& r=results[i];
        for(float f:r.motion) check(std::isfinite(f),"Nonfinite liquid motion");
        if(invalid[i]) {check(r.motion==F{},"Malformed liquid frame accepted or flat motion substituted");continue;}
        const unsigned type=c.frame[7][1]==3;const auto ray=optical(c,c.source[0],c.source[1]);
        const auto seed=transported_seed(c,i);
        for(unsigned component=0;component<4;++component)
            check(std::isfinite(r.seed[component]) && std::abs(r.seed[component]-seed[component])<.002,
                "Current receiver seed lost the accepted eye pose or quantized scale");
        check(ray.valid,"Independent source ray unexpectedly invalid");
        bubbles+=ray.bubbleHeight>.05;
        for(unsigned a=0;a<3;++a) {
            const double hitError=std::abs(r.hit[a]-ray.hit[a]),normalError=std::abs(r.normal[a]-ray.normal[a]);
            max_forward=std::max(max_forward,normalError);
            if(hitError>std::max(.025,std::sqrt(dot(ray.hit,ray.hit))*2.e-5) || normalError>1.e-4)
                throw std::runtime_error("Forward wave oracle mismatch case="+std::to_string(i)+" axis="+std::to_string(a)
                    +" hit="+std::to_string(hitError)+" normal="+std::to_string(normalError));
        }
        check(std::abs(r.hit[3]-ray.bias)<.0001 && std::abs(r.normal[3]-ray.depth)<std::max(.02,ray.depth*1.e-5),"Analytic liquid depth/bias mismatch");
        const V target=feature(c);const auto flatPixel=flat(c,target);
        sensitive[type]+=error(c,target,flatPixel[0],flatPixel[1])>.1;
        if(r.motion[3]==0) {check(r.motion==F{},"Rejected liquid history has partial coordinates");continue;}
        check(r.motion[3]==1,"Liquid validity is not canonical");++accepted[type];
        const double residual=error(c,target,r.motion[0],r.motion[1]);max_error=std::max(max_error,residual);
        if(residual>.015) throw std::runtime_error("Accepted liquid optical ray missed old feature case="+std::to_string(i)+" residual="+std::to_string(residual));
        const auto receiver=optical(c,r.motion[0],r.motion[1]);
        check(receiver.valid && std::abs(r.motion[2]-receiver.depth)<std::max(.02,receiver.depth*1.e-5),"Liquid guide used displaced/current/invalid primary depth");
        same_root[type]+=std::hypot(r.motion[0]-c.source[0],r.motion[1]-c.source[1])<.05;
        auto wrong=c;wrong.frame[7][0]+=.71F;other_clock[type]+=error(wrong,target,r.motion[0],r.motion[1])>.1;
    }
    for(unsigned type=0;type<2;++type) {
        check(accepted[type]>samples[type]/3 && same_root[type]>samples[type]/5,"Liquid solver/oracle coverage insufficient");
        check(sensitive[type]>samples[type]/2 && other_clock[type]>accepted[type]/3,"Liquid fixture cannot detect flat/current-time motion regression");
        std::cout<<(type?"Lava":"Water")<<": "<<samples[type]<<" double forward rays, "<<accepted[type]<<" accepted old feature solves, "
            <<same_root[type]<<" source root roundtrips, "<<sensitive[type]<<" flat-plane-sensitive and "<<other_clock[type]<<" wrong-clock-sensitive checks\n";
    }
    check(bubbles>15,"Lava proof has insufficient active bubble coverage");
    std::cout<<backend<<": "<<faults<<" malformed/behind/clipped/out-of-coverage rejections, "<<bubbles<<" explicit active bubble rays, independent Gaussian pose/scale seed transport, scaled/asymmetric projection, both eyes, translated/tilted/Q15-like old surface, old clock, lava displacement/bubbles, bounded conservative rejections; max accepted pixel error="
        <<max_error<<", max normal error="<<max_forward<<". Kernel-only, no runtime liquid history or FPS acceptance.\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
