// Upload/readback only in this independent diagnostic; never in production.
#include "starfox/render/gpu_calibrated_motion_blur.hpp"
#include "starfox/render/gpu_calibrated_scene.hpp"
#include "starfox/render/gpu_calibrated_msaa.hpp"
#include "starfox/render/gpu_calibrated_scene_fx.hpp"
#include "starfox/render/scene_motion_blur.hpp"
#include "starfox/render/sdl_multisample.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace starfox;
using Bytes=std::vector<unsigned char>;
std::uint64_t checks{},changed{},swept{};
void check(bool ok,const char* error) {++checks;if(!ok) throw std::runtime_error(error);}
bool bgra(SDL_GPUTextureFormat f) {return f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;}
bool srgb(SDL_GPUTextureFormat f) {return f==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;}
struct Gpu {
    SDL_GPUDevice* device{};SDL_GPUCommandBuffer* command{};
    ~Gpu() {if(device) {if(command) SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);SDL_DestroyGPUDevice(device);}SDL_Quit();}
    void begin() {check(!command,"Fixture command already open");command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());}
    void submit() {const bool ok=SDL_SubmitGPUCommandBuffer(command);command=nullptr;check(ok,SDL_GetError());check(SDL_WaitForGPUIdle(device),SDL_GetError());}
    void cancel() {check(SDL_CancelGPUCommandBuffer(command),SDL_GetError());command=nullptr;}
};
struct Texture {
    Gpu& gpu;SDL_GPUTexture* texture{};unsigned w,h,stride;SDL_GPUTextureFormat format;
    Texture(Gpu& g,unsigned width,unsigned height,SDL_GPUTextureFormat f,unsigned samples=1):gpu(g),w(width),h(height),stride(f==SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT?16:4),format(f) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=f;info.width=w;info.height=h;
        info.layer_count_or_depth=info.num_levels=1;
        info.usage=f==SDL_GPU_TEXTUREFORMAT_D32_FLOAT?SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
            :SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.sample_count=samples==8?SDL_GPU_SAMPLECOUNT_8:samples==4?SDL_GPU_SAMPLECOUNT_4:samples==2?SDL_GPU_SAMPLECOUNT_2:SDL_GPU_SAMPLECOUNT_1;
        if(samples>1 && f!=SDL_GPU_TEXTUREFORMAT_D32_FLOAT) {
            info.props=SDL_CreateProperties();check(info.props,SDL_GetError());
            check(SDL_SetBooleanProperty(info.props,STARFOX_SDL_MULTISAMPLE_READ,true),SDL_GetError());
        }
        texture=SDL_CreateGPUTexture(g.device,&info);if(info.props) SDL_DestroyProperties(info.props);check(texture,SDL_GetError());
    }
    ~Texture() {SDL_ReleaseGPUTexture(gpu.device,texture);}
    Texture(const Texture&)=delete;
    void upload(Bytes bytes) {
        check(gpu.command && bytes.size()==w*h*stride,"Bad fixture upload");
        if(bgra(format)) for(unsigned i=0;i<bytes.size();i+=4) std::swap(bytes[i],bytes[i+2]);
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,unsigned(bytes.size()),0};
        auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());
        std::memcpy(mapped,bytes.data(),bytes.size());SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
        auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());
        const SDL_GPUTextureTransferInfo from{transfer,0,w,h};const SDL_GPUTextureRegion to{texture,0,0,0,0,0,w,h,1};
        SDL_UploadToGPUTexture(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
    }
    Bytes read() {
        gpu.begin();const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,w*h*stride,0};
        auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
        auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());
        const SDL_GPUTextureRegion from{texture,0,0,0,0,0,w,h,1};const SDL_GPUTextureTransferInfo to{transfer,0,w,h};
        SDL_DownloadFromGPUTexture(pass,&from,&to);SDL_EndGPUCopyPass(pass);gpu.submit();
        auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(mapped,SDL_GetError());
        Bytes bytes(mapped,mapped+w*h*stride);SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
        if(bgra(format)) for(unsigned i=0;i<bytes.size();i+=4) std::swap(bytes[i],bytes[i+2]);return bytes;
    }
};
Bytes words(const std::vector<std::array<float,4>>& data) {
    Bytes bytes(data.size()*16);std::memcpy(bytes.data(),data.data(),bytes.size());return bytes;
}
void compare(const Bytes& actual,const Bytes& expected,const Bytes& original,const Bytes& tags,unsigned tolerance,const char* label) {
    check(actual.size()==expected.size(),"Wrong native motion blur output size");
    for(unsigned i=0;i<actual.size();++i) {
        const unsigned pixel=i/4,layer=tags[pixel*4+2];
        const bool exact=i%4==3 || (layer!=1 && layer!=2) || original[pixel*4+3]==0;
        if(unsigned(std::abs(int(actual[i])-int(expected[i])))>(exact?0:tolerance)) {
            std::cerr<<label<<" byte="<<i<<" got="<<unsigned(actual[i])<<" expected="<<unsigned(expected[i])<<" original="<<unsigned(original[i])<<" layer="<<layer<<'\n';
            check(false,"Native centred-shutter reconstruction differs from independent CPU reference");
        }
        ++checks;if(actual[i]!=original[i]) ++changed;
        if(expected[i]!=original[i] && layer==1) ++swept;
    }
}
void fixtures(Gpu& gpu,SDL_GPUTextureFormat format,unsigned w,unsigned h,render::GpuCalibratedMotionBlur& blur) {
    Texture source(gpu,w,h,format),underlay(gpu,w,h,format),owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
        surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
        left(gpu,w,h,format),right(gpu,w,h,format);
    const unsigned count=w*h;Bytes original(count*4),background(count*4),tags(count*4);
    std::vector<std::array<float,4>> depth(count),native(count);std::vector<render::MotionBlurGuide> guides(count);
    for(unsigned i=0;i<count;++i) {
        const unsigned x=i%w,y=i/w;
        for(unsigned c=0;c<3;++c) original[i*4+c]=background[i*4+c]=30+(x*(7+c*3)+y*5+c*20)%150;
        original[i*4+3]=background[i*4+3]=255;tags[i*4+2]=1;
        const bool moving=x>w/4 && x<w/2 && y>h/4 && y<h*3/4;
        const bool still=x>w*3/5 && x<w*4/5 && y>h/3 && y<h*2/3;
        if(moving || still) {
            tags[i*4+2]=2;depth[i]={0,0,-1,moving?512.F:256.F};
            original[i*4]=moving?210:20;original[i*4+1]=moving?70:230;original[i*4+2]=moving?30:110;
        }
        // Protected foreground and alpha holes may never inherit trails.
        if(x==w-3 || y==2) tags[i*4+2]=0;
        if(x==1 && y%3==0) {tags[i*4+2]=255;original[i*4+3]=0;}
    }
    gpu.begin();source.upload(original);underlay.upload(background);owner.upload(tags);surface.upload(words(depth));gpu.submit();
    const auto encode=[&](const render::MotionBlurSettings& settings,bool history,Texture& destination,
            std::array<float,2> delta={},std::array<float,2> jitter={}) {
        void* output=reinterpret_cast<void*>(1);
        check(blur.enqueue(gpu.command,source.texture,underlay.texture,owner.texture,surface.texture,motion.texture,
            destination.texture,w,h,settings,history,output,delta,jitter),blur.status().c_str());
        return output;
    };
    std::array<std::array<Bytes,5>,3> frame_rate_reference;
    for(unsigned quality=1;quality<=3;++quality) for(unsigned fps:{60U,120U,240U}) for(unsigned variant=0;variant<5;++variant) {
        auto settings=render::motion_blur_preset(quality);settings.interval_seconds=1./fps;
        const std::array<float,2> delta=variant==1?std::array<float,2>{.47F,-.36F}:std::array<float,2>{};
        const std::array<float,2> jitter=variant==4?std::array<float,2>{-.31F,.27F}:std::array<float,2>{};
        for(unsigned i=0;i<count;++i) {
            const bool eligible=tags[i*4+2]==1 || tags[i*4+2]==2;
            const bool moving=eligible && depth[i][3]==512;
            const float dx=moving && variant!=1 ?-480.F/fps:0;
            const float dy=moving && variant!=1 ?120.F/fps:0;
            // Native previous depth intentionally differs from current. The
            // adapter must NOT reject camera-Z motion as stale correspondence.
            native[i]={dx-delta[0],dy-delta[1],moving?640.F:depth[i][3],moving?1.F:0.F};
            if(variant==2 && moving) native[i][3]=0;
            if(variant==3 && moving) native[i][2]=std::numeric_limits<float>::quiet_NaN();
            const bool valid=moving && variant!=2 && variant!=3;
            guides[i]={valid?dx:0,valid?dy:0,eligible?depth[i][3]:0,valid,eligible};
        }
        gpu.begin();motion.upload(words(native));check(encode(settings,true,left,delta,jitter)==left.texture,"Active native blur did not return destination");
        check(blur.dispatch_count()==2+3*settings.samples,"Native bridge dispatch count unexpected");gpu.submit();
        std::vector<render::MotionBlurGuide> aligned;
        check(render::resolve_motion_blur_guides(w,h,guides,jitter,aligned),"CPU guide alignment failed");
        Bytes expected;check(render::reconstruct_motion_blur(w,h,original,background,aligned,settings,expected),"CPU shutter reference failed");
        const auto actual=left.read();compare(actual,expected,original,tags,srgb(format)?2:1,"uploaded fixture");
        if(fps==60) frame_rate_reference[quality-1][variant]=actual;
        else check(actual==frame_rate_reference[quality-1][variant],"Fixed physical motion changed exposure at a different presentation FPS");
        if(variant==1) check(actual==original,"Jitter-only native guides manufactured physical blur");
    }
    auto settings=render::motion_blur_preset(2);settings.interval_seconds=1./60;
    for(unsigned i=0;i<count;++i) native[i]={-8,0,640,depth[i][3]==512?1.F:0.F};
    gpu.begin();motion.upload(words(native));gpu.submit();
    // Ordered scratch reuse: both independently retained destinations in ONE
    // unsubmitted command, with no image download/submission in the encoder.
    gpu.begin();encode(settings,true,left);auto reverse=settings;reverse.exposure_seconds*=2;encode(reverse,true,right);gpu.submit();
    const auto before=left.read(),after=right.read();check(before!=after,"Different shutters lost independently retained eye outputs");
    gpu.begin();encode(reverse,true,left);gpu.cancel();gpu.begin();encode(settings,true,left);gpu.submit();
    check(left.read()==before,"Cancelled native blur contaminated retry");
    for(unsigned kind=0;kind<6;++kind) {
        auto identity=settings;bool history=true;
        switch(kind) {case 0:identity.exposure_seconds=0;break;case 1:identity.paused=true;break;case 2:identity.interval_seconds=0;break;
            case 3:identity.interval_seconds=.3;break;case 4:history=false;break;case 5:identity.samples=1;break;}
        gpu.begin();void* output{};
        check(blur.enqueue(gpu.command,source.texture,nullptr,nullptr,nullptr,nullptr,nullptr,w,h,identity,history,output),blur.status().c_str());
        check(output==source.texture && blur.dispatch_count()==0,"OFF/pause/cut prepared shaders or dispatched work");gpu.cancel();
    }
    for(unsigned fault=0;fault<9;++fault) {
        gpu.begin();void* output=reinterpret_cast<void*>(1);auto bad=settings;unsigned width=w,height=h;
        void* destination=right.texture;std::array<float,2> delta{};
        switch(fault) {case 0:bad.maximum_radius=129;break;case 1:bad.interval_seconds=-1;break;case 2:bad.samples=66;break;
            case 3:bad.exposure_seconds=std::numeric_limits<double>::infinity();break;case 4:destination=source.texture;break;
            case 5:width=16385;break;case 6:width=height=16384;break;case 7:delta[0]=3;break;
            case 8:bad.interval_seconds=1.e-300;break;}
        check(!blur.enqueue(gpu.command,source.texture,underlay.texture,owner.texture,surface.texture,motion.texture,destination,
            width,height,bad,true,output,delta) && !output && blur.dispatch_count()==0,"Bad native blur request retained output or encoded work");gpu.cancel();
    }
    std::cout<<"Native motion bridge format="<<unsigned(format)<<" "<<w<<'x'<<h
        <<": 60/120/240 FPS, all shutters, current/previous Z, jitter-only/stale/nonfinite guides, protected ink, silhouette underlay, eye retention/cancel/resize/OFF passed.\n";
}
void native_raster(Gpu& gpu,SDL_GPUTextureFormat format,unsigned variant,unsigned eye) {
    constexpr unsigned w=80,h=48;constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    vr::EyeCamera camera;camera.view=identity;camera.projection={1,0,0,0,0,1,0,0,0,0,-1.005F,-1,0,0,-.10025F,0};
    camera.view[12]=eye?.04F:-.04F;
    auto old_camera=camera;
    if(variant) {
        camera.view[12]-=.06F;camera.view[13]=.03F;camera.view[14]=-.11F;
        camera.projection[0]=.83F;camera.projection[5]=1.19F;camera.projection[8]=-.09F;camera.projection[9]=.04F;
        old_camera.view[12]+=.035F;old_camera.view[13]=-.08F;old_camera.view[14]=-.2F;
        old_camera.projection[0]=.94F;old_camera.projection[5]=1.06F;old_camera.projection[8]=.03F;old_camera.projection[9]=-.02F;
    }
    const auto quad=[](float x,float y,float z,float rx,float ry,std::array<float,4> color) {
        std::vector<vr::SceneVertex> vertices(6);constexpr int corners[][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(unsigned i=0;i<6;++i) {auto& v=vertices[i];v.position[0]=x+corners[i][0]*rx;v.position[1]=y+corners[i][1]*ry;v.position[2]=z;
            std::copy(color.begin(),color.end(),v.color);std::copy(color.begin(),color.end(),v.odd_color);}
        return vertices;
    };
    auto model=quad(-.1F,.05F,-2,.52F,.4F,{.83F,.19F,.13F,1}),ink=quad(.5F,.15F,-1.2F,.12F,.1F,{.09F,.79F,.36F,1});
    auto world=quad(-.1F,.05F,-6,3.F,2.F,{.15F,.32F,.46F,1});
    auto reflection=quad(-1.F,-1.1F,-5,.15F,.15F,{1,0,0,1});
    std::array<render::CalibratedSceneDraw,4> draws{};draws[0].vertices=model;draws[0].effect_layer=2;draws[1].vertices=ink;
    draws[2].vertices=world;draws[2].effect_layer=1;
    // An explicitly reflection-tagged protected packet must NOT leak into
    // the blur's world underlay. Conversely world[2] has no reflection flag.
    draws[3].vertices=reflection;draws[3].reflection_environment=true;
    if(variant) {draws[0].model[12]=.12F;draws[0].model[13]=-.02F;draws[0].model[14]=.06F;}
    draws[1].preserve_native_colour=true;
    std::array<render::CalibratedSceneMotionDraw,4> prior{};prior[0].valid=true;
    prior[0].previous_model=identity;prior[0].previous_model[12]=-.3F;prior[0].previous_model[14]=.4F;
    const render::CalibratedSceneMotion previous{old_camera,prior,w,h};
    Texture source(gpu,w,h,format),underlay(gpu,w,h,format),owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
        surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
        result(gpu,w,h,format),depth(gpu,w,h,SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
    render::GpuCalibratedScene scene;check(scene.initialize(gpu.device,format),scene.status().c_str());
    const std::array<float,4> clear{.07F,.19F,.31F,1};
    gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());
    const bool rasterized=scene.enqueue_eye(gpu.command,source.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::before_rays,
        owner.texture,0,surface.texture,false,{},motion.texture,&previous);
    check(rasterized,scene.status().c_str());
    const auto uploaded_cost=scene.upload_cost();
    const bool underlaid=scene.enqueue_eye(gpu.command,underlay.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::world_only);
    check(underlaid,scene.status().c_str());check(scene.upload_cost()==uploaded_cost,"Native underlay re-uploaded geometry/artwork");gpu.submit();
    const auto original=source.read(),background=underlay.read(),tags=owner.read();
    // Analytic sky coverage (no depth receiver) and explicit absence of model,
    // protected UI and unrelated reflection capture packets in the underlay.
    unsigned world_pixels{},excluded_pixels{};
    for(unsigned i=0;i<w*h;++i) {
        if(tags[i*4+2]==1) {check(std::equal(background.begin()+i*4,background.begin()+i*4+4,original.begin()+i*4),"Native underlay changed visible source-world pixels");++world_pixels;}
        if(tags[i*4+2]!=1 && !std::equal(background.begin()+i*4,background.begin()+i*4+3,original.begin()+i*4)) ++excluded_pixels;
    }
    check(world_pixels>100 && excluded_pixels>100,"Native world-only selection lacked real world/model coverage");
    std::vector<render::MotionBlurGuide> guides(w*h);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const unsigned i=y*w+x;const bool eligible=tags[i*4+2]==1 || tags[i*4+2]==2;
        if(tags[i*4+2]!=2) {guides[i]={0,0,0,false,eligible};continue;}
        // Scalar current ray/plane, object translation, tracked-eye translation
        // and asymmetric perspective. No downloaded GPU motion/depth values.
        const double z=2-draws[0].model[14]-camera.view[14];
        const double px=(2*(x+.5)/w-1+camera.projection[8])*z/camera.projection[0];
        const double py=(1-2*(y+.5)/h+camera.projection[9])*z/camera.projection[5];
        const double oldX=px-camera.view[12]-draws[0].model[12]+prior[0].previous_model[12]+old_camera.view[12];
        const double oldY=py-camera.view[13]-draws[0].model[13]+prior[0].previous_model[13]+old_camera.view[13];
        const double oldZ=2-prior[0].previous_model[14]-old_camera.view[14];
        const double priorX=(oldX*old_camera.projection[0]/oldZ-old_camera.projection[8]+1)*w/2;
        const double priorY=(1-oldY*old_camera.projection[5]/oldZ+old_camera.projection[9])*h/2;
        guides[i]={float(priorX-(x+.5)),float(priorY-(y+.5)),float(z*256),true,true};
    }
    render::GpuCalibratedMotionBlur blur;check(blur.initialize(gpu.device,format),blur.status().c_str());
    auto settings=render::motion_blur_preset(2);settings.interval_seconds=1./60;Bytes expected;
    check(render::reconstruct_motion_blur(w,h,original,background,guides,settings,expected),"Native raster scalar reference failed");
    gpu.begin();void* output{};check(blur.enqueue(gpu.command,source.texture,underlay.texture,owner.texture,surface.texture,motion.texture,
        result.texture,w,h,settings,true,output),blur.status().c_str());gpu.submit();
    compare(result.read(),expected,original,tags,srgb(format)?2:1,"actual native raster");
    std::cout<<"Actual native raster/motion MRT format="<<unsigned(format)<<" variant="<<variant<<" eye="<<eye
        <<": perspective-Z, asymmetric tracked camera and object motion against scalar ray/plane reference passed.\n";
}
#include "check_calibrated_nonrigid_motion.inc"
#include "check_calibrated_line_motion.inc"
#include "check_calibrated_joint_particles.inc"
#include "check_calibrated_water_motion.inc"
}
int main(int argc,char** argv) try {
    Gpu gpu;check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());const char* backend=argc>1?argv[1]:"direct3d12";
    gpu.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,true,backend);check(gpu.device,SDL_GetError());
    for(auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
        if(argc>2 && std::string_view(argv[2])=="--line-only") {
            for(unsigned variant=0;variant<4;++variant) native_line_motion(gpu,format,variant);continue;
        }
        native_water_motion(gpu,format);
        if(argc>2 && std::string_view(argv[2])=="--water-only") continue;
        render::GpuCalibratedMotionBlur blur;check(blur.initialize(gpu.device,format),blur.status().c_str());
        fixtures(gpu,format,33,17,blur);fixtures(gpu,format,65,37,blur);
        for(unsigned variant=0;variant<2;++variant) for(unsigned eye=0;eye<2;++eye) native_raster(gpu,format,variant,eye);
        for(unsigned variant=0;variant<7;++variant) native_nonrigid(gpu,format,variant);
        for(unsigned samples:{2U,4U,8U}) for(unsigned variant:{1U,2U,3U,4U,6U}) native_nonrigid(gpu,format,variant,samples);
        native_joint_particles(gpu,format);
        for(unsigned variant=0;variant<4;++variant) native_line_motion(gpu,format,variant);
    }
    if(argc>2 && std::string_view(argv[2])=="--line-only") {
        std::cout<<"Native line component "<<backend<<": "<<checks<<" checks passed. No physical Leia or timing acceptance claimed.\n";return 0;
    }
    check(changed>100 && swept>100,"No visible moving silhouette response");
    if(argc>2 && std::string_view(argv[2])=="--water-only") {
        std::cout<<"Native liquid shutter component "<<backend<<": "<<checks<<" checks, "<<changed
            <<" changed bytes, "<<swept<<" swept-world bytes. No joint-particle or physical-panel acceptance claimed.\n";return 0;
    }
    std::cout<<"Native motion blur component "<<backend<<": "<<checks<<" checks, "<<changed<<" changed bytes, "<<swept
        <<" swept-world bytes. Joint particle component verified; owner/frontend and physical-panel validation are separate.\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
