// Real official SDK evaluation. Upload/readback belong ONLY to this diagnostic.
#include "starfox/render/gpu_calibrated_dlss.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include "starfox/render/sdl_multisample.h"
#include "starfox/render/sdl_d3d12_bridge.h"
#include "../tests/calibrated_pattern_oracle.hpp"
#include <SDL3/SDL.h>
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace starfox;using Bytes=std::vector<unsigned char>;
std::uint64_t checks{},evaluations{},held{},protected_bytes{},guide_pixels{},world_changes{},release_count{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void validate_d3d12_errors(ID3D12InfoQueue* queue) {
    check(queue,"Native DLSS validation queue unavailable");
    check(queue->GetNumMessagesDiscardedByMessageCountLimit()==0,"Native DLSS debug messages overflowed; validation is incomplete");
    // Retain every ERROR/CORRUPTION message through SDK teardown. The private
    // diagnostic device filters only non-critical chatter at storage time, so
    // warnings cannot fill the bounded queue and discard a later real error.
    // The overflow gate above still rejects any loss of critical messages.
    D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
    D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
    check(SUCCEEDED(queue->PushRetrievalFilter(&filter)),"Native DLSS validation filter failed");
    struct Pop {ID3D12InfoQueue* queue;~Pop(){queue->PopRetrievalFilter();}} pop{queue};
    const auto count=queue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for(UINT64 i=0;i<std::min<UINT64>(count,16);++i) {
        SIZE_T bytes=0;check(SUCCEEDED(queue->GetMessage(i,nullptr,&bytes)),"Native DLSS validation message size unavailable");
        Bytes storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        check(SUCCEEDED(queue->GetMessage(i,message,&bytes)),"Native DLSS validation message unavailable");
        std::cerr<<"Native DLSS validation ID "<<message->ID<<": "<<message->pDescription<<'\n';
    }
    check(count==0,"Native DLSS component emitted D3D12 errors");
    std::cout<<"Native DLSS component: zero D3D12 error/corruption messages through SDK teardown"<<std::endl;
}
decltype(&starfox_dlss_evaluate_v1) real_evaluate{};
decltype(&starfox_dlss_evaluate_v2) real_evaluate_rejection{};
decltype(&starfox_dlss_release_viewport_v1) real_release{};
std::vector<StarfoxDlssFrameV1> recorded;
int evaluate(void* module,const StarfoxDlssFrameV1* frame,char* error,unsigned capacity) {
    recorded.push_back(*frame);++evaluations;return real_evaluate(module,frame,error,capacity);
}
int evaluate_rejection(void* module,const StarfoxDlssFrameV2* frame,char* error,unsigned capacity) {
    check(frame && frame->size==sizeof(*frame) && frame->current_color_bias && frame->bias_state==0x40,
        "Actual SDK current-colour rejection resource missing");
    recorded.push_back(frame->frame);++evaluations;
    return real_evaluate_rejection(module,frame,error,capacity);
}
int release(void* module,unsigned viewport,char* error,unsigned capacity) {
    ++release_count;return real_release(module,viewport,error,capacity);
}
struct Sdk {
    HMODULE adapter{};void* module{};render::CalibratedDlssApi api;
    decltype(&starfox_dlss_close_v1) close{};
    explicit Sdk(const std::filesystem::path& directory) {
        const auto path=std::filesystem::canonical(directory);
        adapter=LoadLibraryExW((path/L"starfox_dlss_native.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        check(adapter,"Cannot load explicit test adapter");
        const auto symbol=[&](const char* name){auto* p=GetProcAddress(adapter,name);check(p,name);return p;};
        auto open=reinterpret_cast<decltype(&starfox_dlss_open_v1)>(symbol("starfox_dlss_open_v1"));
        close=reinterpret_cast<decltype(close)>(symbol("starfox_dlss_close_v1"));
        api.bind=reinterpret_cast<decltype(api.bind)>(symbol("starfox_dlss_bind_device_v1"));
        api.configure=reinterpret_cast<decltype(api.configure)>(symbol("starfox_dlss_configure_v1"));
        api.configure_model=reinterpret_cast<decltype(api.configure_model)>(symbol("starfox_dlss_configure_v2"));
        real_evaluate=reinterpret_cast<decltype(real_evaluate)>(symbol("starfox_dlss_evaluate_v1"));
        real_evaluate_rejection=reinterpret_cast<decltype(real_evaluate_rejection)>(symbol("starfox_dlss_evaluate_v2"));
        real_release=reinterpret_cast<decltype(real_release)>(symbol("starfox_dlss_release_viewport_v1"));
        api.finish_frame=reinterpret_cast<decltype(api.finish_frame)>(symbol("starfox_dlss_finish_frame_v1"));
        api.evaluate=evaluate;api.release=release;
        api.evaluate_rejection=evaluate_rejection;
        char error[512]{};check(open(path.c_str(),&module,error,sizeof(error))==0,error);api.module=module;
    }
    void finish(){char error[512]{};check(close(module,error,sizeof(error))==0,error);module=nullptr;FreeLibrary(adapter);adapter=nullptr;}
    void finish_frame(){char error[512]{};check(api.finish_frame(module,error,sizeof(error))==0,error);}
    ~Sdk(){if(module){char error[512]{};close(module,error,sizeof(error));}if(adapter)FreeLibrary(adapter);}
};
bool bgra(SDL_GPUTextureFormat f){return f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;}
struct Gpu {
    SDL_GPUDevice* device{};SDL_GPUCommandBuffer* command{};
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> validation;
    Gpu(){check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,true,"direct3d12");check(device,SDL_GetError());
        auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device),STARFOX_SDL_D3D12_DEVICE,nullptr));
        check(native,"Native DLSS validation device unavailable");
        check(SUCCEEDED(native->QueryInterface(IID_ID3D12InfoQueue,reinterpret_cast<void**>(validation.GetAddressOf()))),
            "Native DLSS debug messages unavailable");
        check(validation->GetNumMessagesDiscardedByMessageCountLimit()==0,"Native DLSS validation overflowed before capture began");
        D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
        D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
        check(SUCCEEDED(validation->AddStorageFilterEntries(&filter)),"Native DLSS critical-message capture failed");
        // No clearing, error-ID deny list or larger unbounded message budget.
        // This device belongs only to the diagnostic, not to a running game.
        std::cout<<"Native DLSS adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<std::endl;}
    ~Gpu(){if(device){if(command)SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);SDL_DestroyGPUDevice(device);}SDL_Quit();}
    void begin(){check(!command,"Diagnostic command already open");command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());}
    void submit(){const bool ok=SDL_SubmitGPUCommandBuffer(command);command=nullptr;check(ok,SDL_GetError());check(SDL_WaitForGPUIdle(device),SDL_GetError());}
    void cancel(){check(SDL_CancelGPUCommandBuffer(command),SDL_GetError());command=nullptr;}
    Bytes read(SDL_GPUTexture* texture,unsigned w,unsigned h,unsigned stride){
        begin();SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,w*h*stride,0};
        auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);check(transfer,SDL_GetError());
        auto* pass=SDL_BeginGPUCopyPass(command);check(pass,SDL_GetError());
        const SDL_GPUTextureRegion from{texture,0,0,0,0,0,w,h,1};const SDL_GPUTextureTransferInfo to{transfer,0,w,h};
        SDL_DownloadFromGPUTexture(pass,&from,&to);SDL_EndGPUCopyPass(pass);submit();
        auto* data=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,transfer,false));check(data,SDL_GetError());
        Bytes result(data,data+w*h*stride);SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);return result;
    }
};
struct Texture {
    Gpu& gpu;SDL_GPUTexture* value{};unsigned w,h,stride;SDL_GPUTextureFormat format;
    Texture(Gpu& g,render::Fsr1Extent extent,SDL_GPUTextureFormat f,unsigned samples=1):gpu(g),w(extent.width),h(extent.height),
        stride(f==SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT?16:4),format(f){
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.width=w;info.height=h;info.format=f;
        info.layer_count_or_depth=info.num_levels=1;info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        // Use the same explicit readable-MSAA extension as the native owner.
        // Without it SDL forbids SAMPLER, or creates no multisample SRV.
        if(samples>1) {
            info.props=SDL_CreateProperties();check(info.props,SDL_GetError());
            check(SDL_SetBooleanProperty(info.props,STARFOX_SDL_MULTISAMPLE_READ,true),SDL_GetError());
        }
        info.sample_count=samples==8?SDL_GPU_SAMPLECOUNT_8:samples==4?SDL_GPU_SAMPLECOUNT_4:samples==2?SDL_GPU_SAMPLECOUNT_2:SDL_GPU_SAMPLECOUNT_1;
        value=SDL_CreateGPUTexture(gpu.device,&info);if(info.props)SDL_DestroyProperties(info.props);check(value,SDL_GetError());}
    ~Texture(){SDL_ReleaseGPUTexture(gpu.device,value);}
    void upload(Bytes data){check(gpu.command && data.size()==w*h*stride,"Malformed diagnostic upload");
        if(bgra(format))for(unsigned i=0;i<data.size();i+=4)std::swap(data[i],data[i+2]);
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,unsigned(data.size()),0};
        auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
        auto* p=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(p,SDL_GetError());std::memcpy(p,data.data(),data.size());SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
        auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());
        const SDL_GPUTextureTransferInfo from{transfer,0,w,h};const SDL_GPUTextureRegion to{value,0,0,0,0,0,w,h,1};
        SDL_UploadToGPUTexture(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);}
    Bytes read(){auto result=gpu.read(value,w,h,stride);if(bgra(format))for(unsigned i=0;i<result.size();i+=4)std::swap(result[i],result[i+2]);return result;}
    void clear_ownership(unsigned layer,unsigned receiver=0,unsigned mix=0) {
        SDL_GPUColorTargetInfo target{};target.texture=value;target.load_op=SDL_GPU_LOADOP_CLEAR;
        target.store_op=SDL_GPU_STOREOP_STORE;target.clear_color={float(receiver)/255,float(mix)/255,float(layer)/255,1};
        auto* pass=SDL_BeginGPURenderPass(gpu.command,&target,1,nullptr);check(pass,SDL_GetError());SDL_EndGPURenderPass(pass);
    }
};
struct Buffer {
    Gpu& gpu;SDL_GPUBuffer* value{};
    Buffer(Gpu& g,const Bytes& data):gpu(g) {
        SDL_GPUBufferCreateInfo info{};info.usage=SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;info.size=unsigned(data.size());
        value=SDL_CreateGPUBuffer(gpu.device,&info);check(value,SDL_GetError());
        SDL_GPUTransferBufferCreateInfo upload{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,unsigned(data.size()),0};
        auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&upload);check(transfer,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());
        std::memcpy(mapped,data.data(),data.size());SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
        auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{transfer,0};const SDL_GPUBufferRegion to{value,0,unsigned(data.size())};
        SDL_UploadToGPUBuffer(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
    }
    ~Buffer(){SDL_ReleaseGPUBuffer(gpu.device,value);}
};
Bytes floats(const std::vector<std::array<float,4>>& values){Bytes result(values.size()*16);std::memcpy(result.data(),values.data(),result.size());return result;}
float scalar(const Bytes& bytes,unsigned at){float result;std::memcpy(&result,bytes.data()+at*4,4);return result;}
void math_check(){
    XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;view.pose.position={.031F,.07F,.11F};
    view.fov={-.72F,.86F,.61F,-.68F};const auto current=vr::eye_camera(view,1,.05F,80);check(bool(current),"Camera fixture invalid");
    view.pose.position.x-=.04F;view.pose.position.z+=.03F;
    const auto prior=vr::eye_camera(view,1,.05F,80);check(bool(prior),"Prior fixture invalid");
    for(int y_sign:{1,-1}){
        auto now=*current,old=*prior;now.projection[5]*=y_sign;now.projection[9]*=y_sign;old.projection[5]*=y_sign;old.projection[9]*=y_sign;
        const auto f=render::calibrated_dlss_frame(now,old,{.23F,-.31F},false);check(bool(f),"Valid asymmetric camera rejected");
        // Independent row-vector scalar projection of a world point. No shared
        // inverse/matrix helper is used by this reference.
        const auto project=[](const vr::EyeCamera& camera,std::array<double,4> world){
            std::array<double,4> eye{},clip{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)eye[r]+=double(camera.view[c*4+r])*world[c];
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)clip[r]+=double(camera.projection[c*4+r])*eye[c];return clip;};
        for(const auto point:{std::array<double,4>{.2,-.1,-1.3,1},std::array<double,4>{-.7,.9,-7,1}}){
            auto clip=project(now,point),expected=project(old,point);std::array<double,4> actual{};
            for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)actual[c]+=clip[r]*f->clip_to_previous[r*4+c];
            for(unsigned c=0;c<4;++c)check(std::abs(actual[c]/actual[3]-expected[c]/expected[3])<2.e-6,"SDK row-major previous-clip mapping is wrong");
        }
        check(std::abs(f->camera_position[0]-.031F)<1.e-6F && f->camera_forward[2]==-1,"SDK camera position/axes are wrong");
        auto bad=now;bad.view[0]=2;check(!render::calibrated_dlss_frame(bad,old,{},false),"Scaled tracking view accepted");
        bad=now;bad.projection[11]=1;check(!render::calibrated_dlss_frame(bad,old,{},false),"Noncanonical depth accepted");
        check(!render::calibrated_dlss_frame(now,old,{1,0},false),"Out-of-range jitter accepted");
    }
}
void fixture(Gpu& gpu,Sdk& sdk,SDL_GPUTextureFormat format,unsigned variant,unsigned& serial,unsigned liquid_case=0,unsigned rejected_layers=0,
    bool reflections_only=false,bool models=true){
    using namespace render;const unsigned mode=variant%4+1,model=(variant/4)%3==1;
    const Fsr1Extent output=variant<8?Fsr1Extent{320,192}:Fsr1Extent{352,208};
    std::array<GpuCalibratedDlss,2> eyes;
    for(unsigned e=0;e<2;++e)check(eyes[e].initialize(gpu.device,format,sdk.api,100+e,mode,model,output),eyes[e].status().c_str());
    const auto input=eyes[0].input_extent();check(input.width==eyes[1].input_extent().width && input.height==eyes[1].input_extent().height,"Different SDK eye extents");
    if(mode==4)check(input.width==output.width && input.height==output.height,"DLAA changed resolution");
    else check(input.width<output.width && input.height<output.height,"SDK upscaling did not reduce its actual grid");
    auto cameras=vr::sbs_eye_cameras(1.1F,float(output.width)/output.height,.064F,4,.05F,80);check(bool(cameras),"Stereo fixture invalid");
    std::array<std::unique_ptr<Texture>,2> scene,owner,surface,motion,ink,panel_owner,dest;
    std::array<std::unique_ptr<Texture>,2> coverage_images;
    std::array<CalibratedDlssCoverage,2> coverage;
    std::array<Bytes,2> coverage_tags;
    std::array<std::vector<bool>,2> extra_rejection;
    std::array<Bytes,2> colors,tags,art,masks,accepted;
    std::array<std::vector<std::array<float,4>>,2> depths,vectors;
    std::array<std::vector<std::unique_ptr<Buffer>>,2> water_storage;
    std::array<std::vector<shadows::GpuReflectionOutput>,2> water;
    std::array<std::vector<float>,2> wet_depth;
    std::array<std::vector<bool>,2> secondary;
    std::array<std::vector<float>,2> cpu_phase;
    const unsigned factor=liquid_case>=5?liquid_case-3:1;
    const unsigned samples=liquid_case && liquid_case<5?1U<<(liquid_case-1):1;
    std::array<std::array<float,2>,2> jitter{{{.23F,-.31F},{-.17F,.29F}}},old_jitter{{{-.11F,.07F},{.19F,-.13F}}};
    for(unsigned e=0;e<2;++e){
        scene[e]=std::make_unique<Texture>(gpu,input,format);owner[e]=std::make_unique<Texture>(gpu,input,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        surface[e]=std::make_unique<Texture>(gpu,input,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);motion[e]=std::make_unique<Texture>(gpu,input,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
        ink[e]=std::make_unique<Texture>(gpu,output,format);panel_owner[e]=std::make_unique<Texture>(gpu,output,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);dest[e]=std::make_unique<Texture>(gpu,output,format);
        colors[e].resize(input.width*input.height*4);tags[e].resize(colors[e].size());depths[e].resize(colors[e].size()/4);vectors[e].resize(depths[e].size());
        art[e].resize(output.width*output.height*4);masks[e].resize(art[e].size());
        for(unsigned i=0;i<colors[e].size()/4;++i){unsigned at=i*4,x=i%input.width,y=i/input.width;
            colors[e][at]=20+(x*3+y+e*71)%190;colors[e][at+1]=24+(x+y*2+e*31)%190;colors[e][at+2]=30+(y*4+x+e*17)%170;colors[e][at+3]=255;
            tags[e][at]=255;tags[e][at+2]=i%9==0?0:i%9==1?255:i%2+1;tags[e][at+3]=255;
            if(reflections_only) {tags[e][at]=i%3==0?1:255;tags[e][at+1]=i%4==0?0:127;}
            // Include a genuine coherent region as well as alternating mixed
            // centre/sample labels. Fractional four-tap reprojection must be
            // able to recover SOME dry history, without weakening its oracle.
            if(samples>1 && x<input.width/2 && y<input.height/2) {
                tags[e][at]=e?255:1;tags[e][at+1]=reflections_only?127:0;tags[e][at+2]=e+1;
            }
            if(liquid_case && i%17==8)tags[e][at]=0;
            const float z=i%31==0?-.2F:i%31==1?8.F:i%31==2?std::numeric_limits<float>::quiet_NaN():256.F*(1+i%7*.3F);
            depths[e][i]={0,0,-1,z};vectors[e][i]={-.65F-float(jitter[e][0]-old_jitter[e][0]),.25F-float(jitter[e][1]-old_jitter[e][1]),z*1.2F,i%19==0?0.F:1.F};
        }
        for(unsigned i=0;i<art[e].size()/4;++i){unsigned at=i*4;
            art[e][at]=i*17+e*21;art[e][at+1]=i*31+e*53;art[e][at+2]=i*7+33;art[e][at+3]=i%256;
            masks[e][at+2]=i%5;masks[e][at+3]=255;}
        wet_depth[e].resize(depths[e].size(),std::numeric_limits<float>::infinity());
        extra_rejection[e].resize(depths[e].size(),false);
        secondary[e].resize(depths[e].size(),false);
        cpu_phase[e].resize(depths[e].size(),-1);
        if(rejected_layers || liquid_case) {
            const unsigned grid=liquid_case?factor:(variant==1?2U:variant==5?4U:1U),
                sample_count=liquid_case?samples:variant==3?4U:variant==7?8U:1U;
            coverage_images[e]=std::make_unique<Texture>(gpu,Fsr1Extent{input.width*grid,input.height*grid},SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,sample_count);
            coverage[e]={coverage_images[e]->value,grid,sample_count};
            coverage_tags[e].resize(input.width*input.height*grid*grid*4);
            for(unsigned p=0;p<depths[e].size();++p)for(unsigned y=0;y<grid;++y)for(unsigned x=0;x<grid;++x) {
                // The final SSAA tap can differ from the clean centre class.
                // MSAA clear independently authors a uniform class per eye;
                // the owner matrix additionally exercises real mixed geometry.
                const unsigned tag=sample_count>1?e+1:rejected_layers && x==grid-1 && y==grid-1 && p%5==0?(p%2?1U:2U):tags[e][p*4+2];
                const unsigned at=((p/input.width*grid+y)*input.width*grid+p%input.width*grid+x)*4;
                coverage_tags[e][at]=sample_count>1?(e?255:1):tags[e][p*4];
                coverage_tags[e][at+1]=sample_count>1?(reflections_only?127:0):tags[e][p*4+1];
                coverage_tags[e][at+2]=static_cast<unsigned char>(tag);coverage_tags[e][at+3]=255;
                extra_rejection[e][p]=extra_rejection[e][p] || (tag==1 && (rejected_layers&1U)) || (tag==2 && (rejected_layers&2U));
            }
        }
        if(liquid_case) {
            const unsigned w=input.width*factor,h=input.height*factor;
            const auto layout=reflections_only?shadows::NativeWaterLayers{}:*shadows::native_water_layers(w,h,liquid_case%2==0);
            gpu.begin();
            for(unsigned sample=0;sample<samples;++sample) {
                Bytes bytes(reflections_only?w*h*4:layout.storage_bytes,0);
                for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
                    const unsigned pixel=(y/factor)*input.width+x/factor,index=y*w+x,kind=pixel%17;
                    // Only the LAST selected sample/footprint corner has wet
                    // coverage: a centre-only shortcut must fail this oracle.
                    const bool covered=sample==samples-1 && x%factor==factor-1 && y%factor==factor-1;
                    const unsigned marker=reflections_only?(covered?(kind%4==0?255:kind%4==1?254:kind%4==2?253:0):0):
                        covered && kind<=10 && kind!=1?253:254;
                    bytes[index*4+3]=static_cast<unsigned char>(marker);
                    std::array<float,4> hit{0,-.8F,.6F,300.F+pixel%5*19.F};
                    if(kind==2)hit[0]=std::numeric_limits<float>::quiet_NaN();
                    if(kind==3)hit[3]=-1;
                    if(kind==4)hit={0,0,0,300};
                    if(kind==5)hit[3]=8;
                    if(kind==7)hit[1]=-2;
                    if(!reflections_only) std::memcpy(bytes.data()+layout.surface_offset+index*16,hit.data(),16);
                    const auto& native=coverage_tags[e];const unsigned at=index*4;
                    const bool eligible=native[at] && (native[at+2]==1 || native[at+2]==2)
                        && (tags[e][pixel*4+2]==1 || tags[e][pixel*4+2]==2);
                    secondary[e][pixel]=secondary[e][pixel] || (eligible && (marker==253 ||
                        (native[at]==1?marker==254:marker==255 && native[at+1] && models)));
                    if(!reflections_only && marker==253 && eligible && kind!=2 && kind!=3 && kind!=4 && kind!=7)
                        wet_depth[e][pixel]=std::min(wet_depth[e][pixel],hit[3]);
                }
                water_storage[e].push_back(std::make_unique<Buffer>(gpu,bytes));
                water[e].push_back({gpu.device,water_storage[e].back()->value,w,h,w*4,layout});
            }
            gpu.submit();
        }
    }
    const auto encode=[&](bool reset,bool upload){
        gpu.begin();++serial;
        for(unsigned e=0;e<2;++e){
            if(upload){scene[e]->upload(colors[e]);owner[e]->upload(tags[e]);surface[e]->upload(floats(depths[e]));motion[e]->upload(floats(vectors[e]));ink[e]->upload(art[e]);panel_owner[e]->upload(masks[e]);}
            if(upload && coverage_images[e]) {
                if(coverage[e].samples>1)coverage_images[e]->clear_ownership(e+1,liquid_case?(e?255:1):0,reflections_only?127:0);
                else coverage_images[e]->upload(coverage_tags[e]);
            }
            auto f=calibrated_dlss_frame((*cameras)[e],(*cameras)[e],jitter[e],reset);check(bool(f),"Native DLSS constants invalid");
            f->frame_index=serial;f->width=input.width;f->height=input.height;f->output_width=output.width;f->output_height=output.height;
            // Deliberate invalid submissions conservatively force SDK reset.
            // Exercise them on the cold frame, not immediately before the
            // valid warm sequence that proves LOCAL history preservation.
            if(reset && upload)check(!eyes[e].enqueue(gpu.command,scene[e]->value,owner[e]->value,surface[e]->value,motion[e]->value,ink[e]->value,panel_owner[e]->value,scene[e]->value,*f,old_jitter[e]),"Aliased native DLSS target accepted");
            if(reset && upload && !water[e].empty()) {
                auto malformed=water[e];malformed[0].water_layers.surface_offset+=4;
                check(!eyes[e].enqueue(gpu.command,scene[e]->value,owner[e]->value,surface[e]->value,motion[e]->value,
                    ink[e]->value,panel_owner[e]->value,dest[e]->value,*f,old_jitter[e],malformed),"Malformed liquid guide layout accepted");
            }
            if(reset && upload) check(!eyes[e].enqueue(gpu.command,scene[e]->value,owner[e]->value,surface[e]->value,motion[e]->value,
                ink[e]->value,panel_owner[e]->value,dest[e]->value,*f,old_jitter[e],water[e],4),"SDK accepted unknown rejected ownership class");
            if(reset && upload) for(unsigned fault=0;fault<6;++fault) {
                auto bad=coverage[e];if(fault==0)bad.factor=0;if(fault==1)bad.factor=5;if(fault==2)bad.samples=3;
                if(fault==3){bad.ownership=nullptr;bad.samples=4;}if(fault==4){bad.factor=2;bad.samples=4;}if(fault==5)bad.ownership=dest[e]->value;
                check(!eyes[e].enqueue(gpu.command,scene[e]->value,owner[e]->value,surface[e]->value,motion[e]->value,
                    ink[e]->value,panel_owner[e]->value,dest[e]->value,*f,old_jitter[e],water[e],rejected_layers,bad),"SDK accepted malformed raster coverage");
            }
            check(eyes[e].enqueue(gpu.command,scene[e]->value,owner[e]->value,surface[e]->value,motion[e]->value,ink[e]->value,panel_owner[e]->value,dest[e]->value,*f,old_jitter[e],water[e],rejected_layers,coverage[e],{},models),eyes[e].status().c_str());
            check(recorded.back().viewport==100+e && recorded.back().frame_index==serial,"Wrong eye viewport/shared SDK frame token");
        }
    };
    encode(true,true);gpu.submit();sdk.finish_frame();for(auto& eye:eyes)eye.commit();
    for(unsigned e=0;e<2;++e){
        accepted[e]=dest[e]->read();auto guides=eyes[e].resident_inputs();
        const auto packed_color=gpu.read(static_cast<SDL_GPUTexture*>(guides.color),input.width,input.height,4);
        const auto packed_depth=gpu.read(static_cast<SDL_GPUTexture*>(guides.depth),input.width,input.height,4);
        const auto packed_motion=gpu.read(static_cast<SDL_GPUTexture*>(guides.motion),input.width,input.height,8);
        const auto bias=gpu.read(static_cast<SDL_GPUTexture*>(guides.current_color_bias),input.width,input.height,1);
        const auto exposure=gpu.read(static_cast<SDL_GPUTexture*>(guides.exposure),1,1,4);check(scalar(exposure,0)==1,"Exposure is not unity");
        const auto phases=water[e].empty()?Bytes{}:gpu.read(static_cast<SDL_GPUTexture*>(guides.accepted_pattern_phase),input.width,input.height,4);
        for(unsigned i=0;i<colors[e].size()/4;++i){
            for(unsigned k=0;k<4;++k)check(packed_color[i*4+k]==colors[e][i*4+k],"SDK authored colour/alpha conversion changed bytes");
            const bool world=tags[e][i*4+2]==1 || tags[e][i*4+2]==2;
            double z=depths[e][i][3];const bool wet=std::isfinite(wet_depth[e][i]);
            if(wet)z=std::isfinite(z) && z>0?std::min(z,double(wet_depth[e][i])):wet_depth[e][i];
            double d=world && std::isfinite(z) && z>0?-double((*cameras)[e].projection[10])+double((*cameras)[e].projection[14])*256/z:1;
            bool visible=world && std::isfinite(z) && z>0 && std::isfinite(d) && d>=0 && d<1;if(!visible)d=1;
            check(std::abs(scalar(packed_depth,i)-d)<2.e-6,"SDK depth differs from independent normalized-depth oracle");
            const bool untracked=(tags[e][i*4+2]==1 && (rejected_layers&1U)) || (tags[e][i*4+2]==2 && (rejected_layers&2U))
                || (world && extra_rejection[e][i]);
            const bool valid=!secondary[e][i] && !untracked && visible && vectors[e][i][3]==1 && std::isfinite(vectors[e][i][2]) && vectors[e][i][2]>0;
            const bool foreground=world && std::isfinite(z) && z>0;
            if(!water[e].empty()) {
                bool coherent=world;
                for(unsigned dy=0;dy<factor;++dy)for(unsigned dx=0;dx<factor;++dx) {
                    const unsigned tap=((i/input.width*factor+dy)*input.width*factor+i%input.width*factor+dx)*4;
                    coherent=coherent && coverage_tags[e][tap+2]==tags[e][i*4+2];
                }
                cpu_phase[e][i]=!secondary[e][i] && !untracked && visible && coherent?0.F:-1.F;
                check(scalar(phases,i)==cpu_phase[e][i],"SDK secondary RGB seeded an accepted dry-history witness");
            }
            const bool reject=secondary[e][i] || untracked || (!valid && (foreground || tags[e][i*4+2]==2));
            if(bias[i]!=(reject?255:0)) throw std::runtime_error("SDK bias mismatch pixel="+std::to_string(i)
                +" eye="+std::to_string(e)+" layer="+std::to_string(tags[e][i*4+2])+" mask="+std::to_string(rejected_layers)
                +" samples="+std::to_string(coverage[e].samples)+" factor="+std::to_string(coverage[e].factor)
                +" expected="+std::to_string(reject?255:0)+" actual="+std::to_string(bias[i]));
            ++checks;
            for(unsigned k=0;k<2;++k){const auto actual=scalar(packed_motion,i*2+k);
                if(valid)check(std::abs(actual-(k?.25F:-.65F))<2.e-6,"SDK guide retained jitter or erased physical motion");
                else check(actual==-std::numeric_limits<float>::max(),"Missing correspondence became fake zero motion");}
            ++guide_pixels;
        }
        for(unsigned i=0;i<art[e].size()/4;++i){bool world=masks[e][i*4+2]==1 || masks[e][i*4+2]==2;
            for(unsigned k=0;k<4;++k)if(!world || k==3 || art[e][i*4+3]==0){check(accepted[e][i*4+k]==art[e][i*4+k],"SDK softened full-panel protected ink/opacity");++protected_bytes;}
                else world_changes+=accepted[e][i*4+k]!=art[e][i*4+k];}
    }
    check(accepted[0]!=accepted[1],"Distinct stereo eyes collapsed");
    const auto before=evaluations;
    for(unsigned repeat=0;repeat<3;++repeat){gpu.begin();for(unsigned e=0;e<2;++e)check(eyes[e].enqueue_accepted(gpu.command,ink[e]->value,panel_owner[e]->value,dest[e]->value),eyes[e].status().c_str());gpu.submit();sdk.finish_frame();
        for(unsigned e=0;e<2;++e){check(dest[e]->read()==accepted[e],"Held SDK eye accumulated blur/drift");++held;}}
    check(evaluations==before,"Held SDK eye was reevaluated");
    if(reflections_only) {
        // Remove all secondary RGB without changing primary depth, ownership or
        // motion. Independently inspect the complete positive bilinear previous
        // footprint, including fractional motion and rejected-to-dry recovery.
        gpu.begin();
        for(unsigned e=0;e<2;++e) {
            water_storage[e].clear();water[e].clear();
            const unsigned w=input.width*factor,h=input.height*factor;
            for(unsigned sample=0;sample<samples;++sample) {
                water_storage[e].push_back(std::make_unique<Buffer>(gpu,Bytes(w*h*4,0)));
                water[e].push_back({gpu.device,water_storage[e].back()->value,w,h,w*4,{}});
            }
        }
        gpu.submit();std::uint64_t dry_recovered=0,prior_rejected=0;
        for(unsigned frame=0;frame<2;++frame) {
            encode(false,false);
            check(!recorded[recorded.size()-2].reset && !recorded.back().reset,"Local secondary guide reset an entire SDK eye");
            gpu.submit();sdk.finish_frame();for(auto& eye:eyes)eye.commit();
            for(unsigned e=0;e<2;++e) {
                const auto guides=eyes[e].resident_inputs();
                const auto bias=gpu.read(static_cast<SDL_GPUTexture*>(guides.current_color_bias),input.width,input.height,1);
                const auto mv=gpu.read(static_cast<SDL_GPUTexture*>(guides.motion),input.width,input.height,8);
                auto next=cpu_phase[e];
                for(unsigned i=0;i<depths[e].size();++i) {
                    const bool world=tags[e][i*4+2]==1 || tags[e][i*4+2]==2;
                    const double z=depths[e][i][3],d=world && z>0 && std::isfinite(z)?
                        -double((*cameras)[e].projection[10])+double((*cameras)[e].projection[14])*256/z:1;
                    const bool visible=world && z>0 && std::isfinite(z) && std::isfinite(d) && d>=0 && d<1;
                    bool coherent=world;
                    for(unsigned dy=0;dy<factor;++dy)for(unsigned dx=0;dx<factor;++dx) {
                        const unsigned tap=((i/input.width*factor+dy)*input.width*factor+i%input.width*factor+dx)*4;
                        coherent=coherent && coverage_tags[e][tap+2]==tags[e][i*4+2];
                    }
                    next[i]=visible && coherent?0.F:-1.F;
                    bool valid=visible && vectors[e][i][3]==1 && std::isfinite(vectors[e][i][2]) && vectors[e][i][2]>0 && next[i]>=0;
                    const double px=i%input.width+double(vectors[e][i][0]),py=i/input.width+double(vectors[e][i][1]);
                    valid=valid && px>=0 && py>=0 && px<=input.width-1 && py<=input.height-1;
                    if(valid) {
                        const unsigned bx=unsigned(px),by=unsigned(py);const double fx=px-bx,fy=py-by;
                        for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx)
                            if((dx?fx:1-fx)*(dy?fy:1-fy)>0 && cpu_phase[e][std::min(by+dy,input.height-1)*input.width+std::min(bx+dx,input.width-1)]!=0)valid=false;
                    }
                    const bool foreground=world && z>0 && std::isfinite(z);
                    const bool reject=(!valid && (foreground || tags[e][i*4+2]==2)) || (visible && !valid);
                    check(bias[i]==(reject?255:0),"SDK dry pixel reused a previous reflected/liquid footprint");
                    if(valid) {check(std::abs(scalar(mv,i*2)+.65F)<2.e-6 && std::abs(scalar(mv,i*2+1)-.25F)<2.e-6,
                        "SDK dry geometry lost its real local motion");++dry_recovered;}
                    else {check(scalar(mv,i*2)==-std::numeric_limits<float>::max(),"SDK invalid secondary history became fake motion");prior_rejected+=visible;}
                }
                const auto phase=gpu.read(static_cast<SDL_GPUTexture*>(guides.accepted_pattern_phase),input.width,input.height,4);
                for(unsigned i=0;i<next.size();++i)check(scalar(phase,i)==next[i],"SDK dry eligibility failed to recover after secondary radiance");
                cpu_phase[e]=std::move(next);
            }
        }
        check(dry_recovered>100 && prior_rejected>100,"SDK ray fixture missed dry/secondary previous-footprint recovery");
        for(unsigned e=0;e<2;++e)accepted[e]=dest[e]->read();
    }
    if(variant==3 || variant==7){
        const auto before_recovery=release_count;
        encode(false,false);
        for(auto& eye:eyes)check(eye.has_recorded_sdk_work(),"Rejected SDK command omitted its native work witness");
        gpu.submit();sdk.finish_frame();for(auto& eye:eyes)eye.discard();
        gpu.begin();for(unsigned e=0;e<2;++e)check(eyes[e].enqueue_accepted(gpu.command,ink[e]->value,panel_owner[e]->value,dest[e]->value),eyes[e].status().c_str());gpu.submit();sdk.finish_frame();
        for(unsigned e=0;e<2;++e)check(dest[e]->read()==accepted[e],"Cancelled SDK work replaced an accepted eye");
        check(release_count==before_recovery,"Held SDK images recreated private resources after cancellation");
        encode(false,false);check(recorded[recorded.size()-2].reset && recorded.back().reset,"Rejected SDK work did not force both eyes to reset");gpu.submit();sdk.finish_frame();for(auto& eye:eyes)eye.commit();
        check(release_count==before_recovery,"SDK rejection unnecessarily recreated private histories");
    }
    for(auto& eye:eyes)check(eye.retire(),eye.status().c_str());
    std::cout<<"format="<<unsigned(format)<<" mode="<<mode<<" model="<<model<<" input="<<input.width<<'x'<<input.height<<" output="<<output.width<<'x'<<output.height
        <<" liquid="<<liquid_case<<" samples="<<samples<<" footprint="<<factor<<" rejected_layers="<<rejected_layers
        <<" rgba_rays="<<reflections_only<<" model_reflections="<<models<<" two SDK eyes/guides/full-panel ink/held history passed"<<std::endl;
}
#include "check_calibrated_dlss_patterns.inc"
}
int main(int argc,char** argv)try{
    if(argc<2 || argc>3 || (argc==3 && std::string_view(argv[2])!="--coverage-only" && std::string_view(argv[2])!="--patterns-only" && std::string_view(argv[2])!="--rays-only"))
        throw std::runtime_error("Usage: starfox_calibrated_dlss_check <absolute trusted SDK directory> [--coverage-only|--patterns-only|--rays-only]");
    const bool coverage_only=argc==3 && std::string_view(argv[2])=="--coverage-only";
    const bool patterns_only=argc==3 && std::string_view(argv[2])=="--patterns-only";
    const bool rays_only=argc==3 && std::string_view(argv[2])=="--rays-only";
    math_check();Sdk sdk(argv[1]);Microsoft::WRL::ComPtr<ID3D12InfoQueue> validation;
    {Gpu gpu;validation=gpu.validation;unsigned serial=0;
        if(patterns_only) {
            for(auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB})
                for(unsigned model:{0U,1U})for(unsigned effect:{5U,10U,25U,70U})
                    for(auto grid:{std::array{1U,1U},std::array{2U,1U},std::array{1U,4U}})
                        pattern_fixture(gpu,sdk,format,model,effect,grid[0],grid[1],serial);
            for(unsigned model:{0U,1U})for(unsigned effect:{5U,10U,25U,70U})
                for(auto grid:{std::array{3U,1U},std::array{4U,1U},std::array{1U,2U},std::array{1U,8U}})
                    pattern_fixture(gpu,sdk,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,model,effect,grid[0],grid[1],serial);
        }
        if(rays_only)for(auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB})
            for(unsigned grid=1;grid<=7;++grid)for(unsigned variant:{3U,7U})for(bool models:{false,true})
                fixture(gpu,sdk,format,variant,serial,grid,0,true,models);
        if(!rays_only && !coverage_only && !patterns_only)for(auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB})
            for(unsigned variant=0;variant<12;++variant)fixture(gpu,sdk,format,variant,serial);
        if(!rays_only && !patterns_only)for(auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB})
            for(unsigned mask:{1U,2U,3U})for(unsigned variant:{1U,3U,5U,7U})fixture(gpu,sdk,format,variant,serial,0,mask);
        if(!rays_only && !coverage_only && !patterns_only)for(unsigned liquid=1;liquid<=7;++liquid)for(unsigned variant=0;variant<8;++variant)
            fixture(gpu,sdk,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,variant,serial,liquid);
    }
    sdk.finish();validate_d3d12_errors(validation.Get());
    check(world_changes>1000 && release_count==(rays_only?224U:patterns_only?256U:coverage_only?96U:304U),"SDK world reconstruction/viewport retirement was not exercised");
    std::cout<<"PASS native DLSS component: "<<evaluations<<" real SDK evaluations, "<<held<<" exact held-eye reuses, "<<guide_pixels
        <<" scalar guide pixels, "<<protected_bytes<<" exact protected/opacity bytes; "<<checks<<" checks"<<std::endl;
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL native DLSS: "<<e.what()<<std::endl;return 1;}
