// Native GPU diagnosis of the exact scalar shader header. Full optical/path
// gates stay separate and unchanged; this must never claim radiance acceptance.
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <vector>
#include "reflected_curved_scalar_dxil.hpp"
#include "reflected_curved_scalar_spirv.hpp"
#include "reflected_curved_device.hpp"
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
using Pair=std::array<float,2>;
Pair split(double x){const float h=float(x);return {h,float(x-double(h))};}
long double join(Pair p){return static_cast<long double>(p[0])+p[1];}
struct Case{Pair a,b;};struct Result{std::array<Pair,6> value;};
static_assert(sizeof(Case)==16 && sizeof(Result)==48);
struct Gpu {
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};SDL_GPUBuffer *input{},*output{},*trig{};
    SDL_GPUTransferBuffer *upload{},*download{},*trig_upload{};
    ~Gpu(){if(device){SDL_WaitForGPUIdle(device);if(pipeline)SDL_ReleaseGPUComputePipeline(device,pipeline);
        for(auto* b:{input,output,trig})if(b)SDL_ReleaseGPUBuffer(device,b);
        for(auto* t:{upload,download,trig_upload})if(t)SDL_ReleaseGPUTransferBuffer(device,t);SDL_DestroyGPUDevice(device);}SDL_Quit();}
    std::vector<Result> run(const char* backend,const std::vector<Case>& cases) {
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        device=create_reflected_curved_device(backend);require(device,SDL_GetError());
        require_reflected_curved_precision(device,backend);
        const bool spirv=std::string_view(backend)=="vulkan";
        std::cout<<"Scalar precision device="<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")
            <<" backend="<<backend<<" cases="<<cases.size()<<std::endl;
        SDL_GPUComputePipelineCreateInfo pi{};pi.entrypoint="main";pi.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        pi.code=spirv?reflected_curved_scalar_spirv:reflected_curved_scalar_dxil;
        pi.code_size=spirv?sizeof(reflected_curved_scalar_spirv):sizeof(reflected_curved_scalar_dxil);
        pi.num_readonly_storage_buffers=2;pi.num_readwrite_storage_buffers=pi.num_uniform_buffers=1;pi.threadcount_x=64;pi.threadcount_y=pi.threadcount_z=1;
        const auto start=std::chrono::steady_clock::now();pipeline=SDL_CreateGPUComputePipeline(device,&pi);require(pipeline,SDL_GetError());
        std::cout<<"Scalar pipeline seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;
        const unsigned input_bytes=unsigned(cases.size()*sizeof(Case)),output_bytes=unsigned(cases.size()*sizeof(Result));
        const SDL_GPUBufferCreateInfo in{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,input_bytes,0},out{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,output_bytes,0},table{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,16384,0};
        input=SDL_CreateGPUBuffer(device,&in);output=SDL_CreateGPUBuffer(device,&out);trig=SDL_CreateGPUBuffer(device,&table);
        require(input && output && trig,SDL_GetError());
        const SDL_GPUTransferBufferCreateInfo up{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,input_bytes,0},down{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,output_bytes,0},tu{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,16384,0};
        upload=SDL_CreateGPUTransferBuffer(device,&up);download=SDL_CreateGPUTransferBuffer(device,&down);trig_upload=SDL_CreateGPUTransferBuffer(device,&tu);
        require(upload && download && trig_upload,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(device,upload,false);require(mapped,SDL_GetError());std::memcpy(mapped,cases.data(),input_bytes);SDL_UnmapGPUTransferBuffer(device,upload);
        auto* t=static_cast<float*>(SDL_MapGPUTransferBuffer(device,trig_upload,false));require(t,SDL_GetError());
        for(unsigned c=0;c<1024;++c){const double p=double(c)*std::numbers::pi/512.;const auto s=split(std::sin(p)),co=split(std::cos(p));
            t[c*4]=s[0];t[c*4+1]=s[1];t[c*4+2]=co[0];t[c*4+3]=co[1];}SDL_UnmapGPUTransferBuffer(device,trig_upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{upload,0},tf{trig_upload,0};const SDL_GPUBufferRegion to{input,0,input_bytes},tt{trig,0,16384};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_UploadToGPUBuffer(copy,&tf,&tt,false);SDL_EndGPUCopyPass(copy);
        const SDL_GPUStorageBufferReadWriteBinding rw{output,false};auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&rw,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipeline);SDL_GPUBuffer* inputs[]{input,trig};SDL_BindGPUComputeStorageBuffers(pass,0,inputs,2);
        const std::array<unsigned,4> settings{unsigned(cases.size()),0,0,0};SDL_PushGPUComputeUniformData(command,0,settings.data(),sizeof(settings));
        SDL_DispatchGPUCompute(pass,unsigned((cases.size()+63)/64),1,1);SDL_EndGPUComputePass(pass);
        copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());const SDL_GPUBufferRegion src{output,0,output_bytes};const SDL_GPUTransferBufferLocation dst{download,0};
        SDL_DownloadFromGPUBuffer(copy,&src,&dst);SDL_EndGPUCopyPass(copy);auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,SDL_GetError());
        require(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
        const auto* result=SDL_MapGPUTransferBuffer(device,download,false);require(result,SDL_GetError());std::vector<Result> values(cases.size());
        std::memcpy(values.data(),result,output_bytes);SDL_UnmapGPUTransferBuffer(device,download);return values;
    }
};
}
int main(int argc,char** argv)try {
    const bool trace=argc==3 && std::string_view(argv[2])=="--division-trace";
    require((argc==2 || trace) && (std::string_view(argv[1])=="direct3d12" || std::string_view(argv[1])=="vulkan"),"Use direct3d12|vulkan [--division-trace]");
    std::vector<Case> cases;
    for(unsigned i=0;i<=32768;++i) {
        const double phase=-1048576.+double(i)*64.+std::sin(double(i)*.731)*.037;
        const double denominator=(i&1?-1:1)*(.007+double(i%127)*.013);
        cases.push_back({split(phase),split(denominator)});
    }
    for(double p:{-1000.,-3.141592653589793,-.01,0.,.01,3.141592653589793,1000.})for(double tail:{-1.e-7,0.,1.e-7})
        cases.push_back({split(p+tail),split(1.113)});
    // Cancellation and reciprocal residual shapes, including the driver's
    // originally failing quotient. Keep all original broad-domain cases.
    for(float h:{-1048500.F,-1000.F,-1.F,1.F,1000.F,1048500.F})
        for(float tail:{-.03125F,-.0001F,0.F,.0001F,.03125F})
            for(float delta:{-.0625F,-.00001F,.00001F,.0625F})
                cases.push_back({split(double(h)+tail),split(-double(h)+delta)});
    Gpu gpu;const auto output=gpu.run(argv[1],cases);std::array<double,6> maximum{};std::array<unsigned,6> failures{};
    if(trace) {
        std::cout<<std::setprecision(17);
        for(unsigned i=0;i<12;++i) {std::cout<<"Division TRACE case="<<i<<" input="<<cases[i].a[0]<<','<<cases[i].a[1]<<','<<cases[i].b[0]<<','<<cases[i].b[1];
            for(unsigned k=0;k<6;++k)std::cout<<" stage"<<k<<'='<<output[i].value[k][0]<<','<<output[i].value[k][1];std::cout<<'\n';}
        std::cout<<"Intermediate inspection only; NO scalar/optical/radiance acceptance."<<std::endl;return 0;
    }
    unsigned printed=0;
    for(unsigned i=0;i<cases.size();++i) {
        const long double a=join(cases[i].a),b=join(cases[i].b);const std::array<long double,6> expected{a+b,a*b,a/b,std::sqrt(std::abs(a)+1),std::sin(a),std::cos(a)};
        for(unsigned op=0;op<6;++op) {
            const auto pair=output[i].value[op];const long double actual=join(pair),error=std::abs(actual-expected[op]);
            const double scaled=double(op<4?error/std::max(1.L,std::abs(expected[op])):error);maximum[op]=std::max(maximum[op],scaled);
            if(!std::isfinite(actual) || scaled>(op<4?1.e-12:1.e-9)) {
                ++failures[op];if(printed++<12)std::cerr<<"Scalar GPU failure case="<<i<<" op="<<op<<" a="<<a<<" pair="<<pair[0]<<','<<pair[1]<<" expected="<<expected[op]<<" error="<<scaled<<'\n';
            }
        }
    }
    std::cout<<"Scalar GPU max add/multiply/divide/sqrt/sine/cosine:";for(auto e:maximum)std::cout<<' '<<e;
    std::cout<<"; failures:";for(auto n:failures)std::cout<<' '<<n;std::cout<<". Exact scalar header, independent long-double arithmetic, no optical/radiance acceptance."<<std::endl;
    require(std::all_of(failures.begin(),failures.end(),[](unsigned n){return n==0;}),"Native scalar precision gate failed");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
