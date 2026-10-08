#include "starfox/render/gpu_temporal_aa.hpp"
#include "starfox/render/gpu_fsr1.hpp"
#include "starfox/render/temporal_aa.hpp"
#include "starfox/render/gpu_smaa.hpp"
#include "starfox/render/gpu_msaa.hpp"
#include "starfox/render/raster_commands.hpp"
#include "starfox/render/gpu_clip.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/gpu_composite.hpp"
#include "starfox/render/gpu_motion_blur.hpp"
#include "starfox/render/gpu_scene_shutter.hpp"
#include "starfox/render/scene_motion_blur.hpp"
#include "starfox/render/sdl_gpu_effects.hpp"
#include "starfox/render/gpu_temporal_inputs.hpp"
#include "starfox/render/camera_response.hpp"
#include <bit>
#include <chrono>
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace starfox::render;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(std::string(message)+": "+SDL_GetError());}
struct Fixture {
    SDL_GPUDevice* device;
    unsigned width,height,count;
    SDL_GPUTexture* color{};
    std::array<SDL_GPUBuffer*,3> buffers{};
    SDL_GPUTransferBuffer *upload{},*download{};
    Fixture(SDL_GPUDevice* d,unsigned w,unsigned h):device(d),width(w),height(h),count(w*h) {
        SDL_GPUTextureCreateInfo t{};t.type=SDL_GPU_TEXTURETYPE_2D;
        t.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;t.width=w;t.height=h;
        t.layer_count_or_depth=t.num_levels=1;t.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        color=SDL_CreateGPUTexture(d,&t);require(color,"input color");
        for(unsigned i=0;i<3;++i) {
            SDL_GPUBufferCreateInfo b{};b.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
            b.size=count*(i==1?16:4);buffers[i]=SDL_CreateGPUBuffer(d,&b);require(buffers[i],"guide buffer");
        }
        SDL_GPUTransferBufferCreateInfo u{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,count*28,0};
        upload=SDL_CreateGPUTransferBuffer(d,&u);u.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;u.size=count*4;
        download=SDL_CreateGPUTransferBuffer(d,&u);require(upload&&download,"transfer buffers");
    }
    ~Fixture() {
        SDL_ReleaseGPUTexture(device,color);for(auto* b:buffers) SDL_ReleaseGPUBuffer(device,b);
        SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);
    }
    void upload_buffer(SDL_GPUCommandBuffer* cb,SDL_GPUBuffer* target,std::span<const std::byte> data) {
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,Uint32(data.size()),0};
        auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);require(transfer,"test buffer upload");
        auto* bytes=SDL_MapGPUTransferBuffer(device,transfer,false);
        if(!bytes) {SDL_ReleaseGPUTransferBuffer(device,transfer);require(false,"map test buffer");}
        std::memcpy(bytes,data.data(),data.size());SDL_UnmapGPUTransferBuffer(device,transfer);
        auto* pass=SDL_BeginGPUCopyPass(cb);require(pass,"test buffer copy");
        SDL_GPUTransferBufferLocation from{transfer,0};SDL_GPUBufferRegion to{target,0,Uint32(data.size())};
        SDL_UploadToGPUBuffer(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);
        SDL_ReleaseGPUTransferBuffer(device,transfer);
    }
    void encode_input(SDL_GPUCommandBuffer* cb,const std::vector<uint8_t>& pixels,
        const std::vector<TemporalAaGuide>& guides) {
        auto* bytes=static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device,upload,true));require(bytes,"map upload");
        std::memcpy(bytes,pixels.data(),count*4);
        for(unsigned i=0;i<count;++i) {
            std::memcpy(bytes+count*4+i*4,&guides[i].depth,4);
            const std::array<float,4> motion{guides[i].motion_x,guides[i].motion_y,guides[i].depth,guides[i].valid?1.f:0.f};
            std::memcpy(bytes+count*8+i*16,motion.data(),16);
            const uint32_t tag=guides[i].eligible?0:256;
            std::memcpy(bytes+count*24+i*4,&tag,4);
        }
        SDL_UnmapGPUTransferBuffer(device,upload);
        auto* pass=SDL_BeginGPUCopyPass(cb);require(pass,"upload copy pass");
        SDL_GPUTextureTransferInfo source{upload,0,0,0};
        SDL_GPUTextureRegion region{color,0,0,0,0,0,width,height,1};
        SDL_UploadToGPUTexture(pass,&source,&region,false);
        const unsigned offsets[]{count*4,count*8,count*24};
        for(unsigned i=0;i<3;++i) {
            SDL_GPUTransferBufferLocation from{upload,offsets[i]};
            SDL_GPUBufferRegion to{buffers[i],0,count*(i==1?16U:4U)};
            SDL_UploadToGPUBuffer(pass,&from,&to,false);
        }
        SDL_EndGPUCopyPass(pass);
    }
    std::vector<uint8_t> finish(SDL_GPUCommandBuffer* cb,SDL_GPUTexture* output,GpuTemporalAa& taa) {
        auto* pass=SDL_BeginGPUCopyPass(cb);require(pass,"download copy pass");
        SDL_GPUTextureRegion from{output,0,0,0,0,0,width,height,1};
        SDL_GPUTextureTransferInfo to{download,0,0,0};SDL_DownloadFromGPUTexture(pass,&from,&to);SDL_EndGPUCopyPass(pass);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cb);require(fence,"submit TAA");taa.commit();
        require(SDL_WaitForGPUFences(device,true,&fence,1),"wait TAA");SDL_ReleaseGPUFence(device,fence);
        const auto* bytes=static_cast<const uint8_t*>(SDL_MapGPUTransferBuffer(device,download,false));require(bytes,"map result");
        std::vector<uint8_t> result(bytes,bytes+count*4);SDL_UnmapGPUTransferBuffer(device,download);return result;
    }
};
void check_neural_artwork(SDL_GPUDevice* device) {
    Fixture native(device,128,64),reconstructed(device,128,64);
    GpuTemporalInputs protection;GpuTemporalAa unused;
    std::vector<uint8_t> sharp(native.count*4),smooth(native.count*4);
    std::vector<TemporalAaGuide> guides(native.count);
    std::vector<uint32_t> tags(native.count);
    // Pure HUD, world OAM sprite, tile artwork, generated terrain, model.
    constexpr std::array<uint32_t,5> kinds{256,256|0x10000000U,512,512|0x08000000U,0x01000000U};
    for(unsigned y=0;y<native.height;++y) for(unsigned x=0;x<native.width;++x) {
        const auto i=y*native.width+x;tags[i]=kinds[x%kinds.size()];
        const bool lit=(x+y)%2;sharp[i*4]=lit?255:0;sharp[i*4+1]=lit?0:255;
        sharp[i*4+2]=37;sharp[i*4+3]=255;
        smooth[i*4]=63;smooth[i*4+1]=79;smooth[i*4+2]=91;smooth[i*4+3]=255;
    }
    for(bool artwork:{false,true}) for(bool hud:{false,true}) {
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"neural artwork command");
        native.encode_input(cb,sharp,guides);reconstructed.encode_input(cb,smooth,guides);
        native.upload_buffer(cb,native.buffers[2],std::as_bytes(std::span{tags}));
        auto* output=static_cast<SDL_GPUTexture*>(protection.restore_hud(device,cb,native.color,
            reconstructed.color,native.buffers[2],native.width,native.height,artwork,hud));
        require(output,protection.status().c_str());const auto actual=native.finish(cb,output,unused);
        for(unsigned i=0;i<native.count;++i) {
            const auto kind=(i%native.width)%kinds.size();
            const bool preserve=(hud && kind==0) || (artwork && (kind==1 || kind==2));
            const auto& expected=preserve?sharp:smooth;
            for(unsigned c=0;c<4;++c) require(actual[i*4+c]==expected[i*4+c],
                "neural reconstruction blurred native artwork or restored HUD into world underlay");
        }
    }
    // DLSS artwork restoration must not expose the jittered model silhouette
    // as a hard native-background mask. Its narrow edge guard must neither
    // soften distant artwork nor leak into HUD/world-sprite ownership.
    for(bool edge_guard:{false,true}) for(bool hud:{false,true}) {
        std::fill(tags.begin(),tags.end(),512U);
        for(unsigned y=0;y<native.height;++y) {
            tags[y*native.width+64]=0x02000000U; // current jittered model owner
            tags[y*native.width+16]=512U|0x08000000U; // terrain is not a model
            tags[y*native.width+63]=256U; // HUD beside model stays native
            tags[y*native.width+65]=256U|0x10000000U; // native world sprite
            tags[y*native.width+30]=512U|0x02000000U; // tile/text covering an older model
        }
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"neural edge guard command");
        native.encode_input(cb,sharp,guides);reconstructed.encode_input(cb,smooth,guides);
        native.upload_buffer(cb,native.buffers[2],std::as_bytes(std::span{tags}));
        auto* output=static_cast<SDL_GPUTexture*>(protection.restore_hud(device,cb,native.color,
            reconstructed.color,native.buffers[2],native.width,native.height,true,hud,edge_guard));
        require(output,protection.status().c_str());const auto actual=native.finish(cb,output,unused);
        for(unsigned i=0;i<native.count;++i) {
            const unsigned x=i%native.width;
            const bool preserve=(x==63?hud:x==65?true:x!=16 && x!=64 && !(edge_guard && (x==62 || x==66)));
            const auto& expected=preserve?sharp:smooth;
            for(unsigned c=0;c<4;++c) require(actual[i*4+c]==expected[i*4+c],
                "neural model edge guard clipped reconstruction or softened protected artwork");
        }
    }
    std::cout<<"DLSS artwork protection: native HUD/tilemaps/world sprites exact; models/terrain remain reconstructed; camera underlay excludes HUD passed\n";
}
void benchmark_motion(SDL_GPUDevice* device,bool particles=false,bool joint=false) {
    // Submit-to-completion wall time, not a GPU timestamp or game FPS claim.
    // Uploads and shader compilation are outside the measured warm iterations.
    for(const auto size:{std::array<unsigned,2>{400,224},{800,448},{1280,720}}) {
        Fixture source(device,size[0],size[1]),background(device,size[0],size[1]);
        std::vector<std::uint8_t> pixels(source.count*4,255),underlay;
        std::vector<TemporalAaGuide> guides(source.count,{0,0,100,100,true,true});
        for(unsigned i=0;i<source.count;++i) {
            pixels[i*4]=32;pixels[i*4+1]=64;pixels[i*4+2]=96;
        }
        underlay=pixels;
        for(unsigned y=size[1]/4;y<3*size[1]/4;++y) for(unsigned x=size[0]/3;x<2*size[0]/3;++x) {
            const auto i=y*size[0]+x;guides[i]={12,3,10,10,true,true};
            pixels[i*4]=220;pixels[i*4+1]=100;pixels[i*4+2]=30;
        }
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"benchmark upload command");
        source.encode_input(cb,pixels,guides);background.encode_input(cb,underlay,guides);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cb);require(fence,"benchmark upload submit");
        require(SDL_WaitForGPUFences(device,true,&fence,1),"benchmark upload wait");SDL_ReleaseGPUFence(device,fence);
        GpuCompositeOutput input{device,source.color,source.buffers[2],nullptr,size[0],size[1],source.buffers[0],source.buffers[1]};
        GpuCompositeOutput back{device,background.color,nullptr,nullptr,size[0],size[1]};
        GpuMotionBlur blur;MotionBlurSettings settings;settings.interval_seconds=1./60;
        GpuSceneShutter particle_blur;SceneFxFrame fx;
        for(unsigned n=0;n<scene_fx_capacity;++n) {
            const float x=float((n%8+1)*size[0]/9),y=float((n/8+1)*size[1]/7);
            fx.add({x,y,12,.8f},{n%2?6.f:7.f,50,.2f,0},{1,.4f,.1f,0});
            fx.motion_previous[n]={{x+20,y+4,55},true};
        }
        for(unsigned samples:{3U,9U,17U}) {
            settings.samples=samples;std::array<double,8> timings{};
            for(unsigned frame=0;frame<10;++frame) {
                const auto start=std::chrono::steady_clock::now();
                cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"benchmark blur command");void* output{};
                if(particles) {
                    auto particle_input=input;particle_input.surfaces=source.buffers[1];
                    require(particle_blur.enqueue(cb,particle_input,fx,settings,1,output),particle_blur.status().c_str());
                } else require(blur.enqueue(cb,input,back,settings,true,output,{},joint?&fx:nullptr,1),blur.status().c_str());
                fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cb);require(fence,"benchmark blur submit");
                require(SDL_WaitForGPUFences(device,true,&fence,1),"benchmark blur wait");SDL_ReleaseGPUFence(device,fence);
                if(frame>=2) timings[frame-2]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            }
            std::sort(timings.begin(),timings.end());
            std::cout<<(joint?"joint-benchmark: driver=":particles?"particle-benchmark: driver=":"motion-benchmark: driver=")<<SDL_GetGPUDeviceDriver(device)<<" extent="<<size[0]<<'x'<<size[1]
                <<" samples="<<samples<<" dispatches="<<(particles?1:blur.dispatch_count())<<" median-submit-ms="<<(timings[3]+timings[4])*.5
                <<" max-submit-ms="<<timings.back()<<'\n';
        }
    }
}
void check_particle_shutter(SDL_GPUDevice* device) {
    Fixture f(device,32,16);GpuSceneShutter gpu;GpuTemporalAa unused;
    Framebuffer frame(32,16);frame.enable_layer_tags(true);
    for(unsigned x=0;x<32;++x) frame.set_stored(x,8,0,PixelLayer::two_d);
    std::vector<uint8_t> source(f.count*4,24),expected;
    for(unsigned i=0;i<f.count;++i) source[i*4+3]=uint8_t(i%256);
    std::vector<TemporalAaGuide> guides(f.count);
    std::vector<uint32_t> packed(f.count);
    std::vector<std::array<float,4>> surfaces(f.count);
    MotionBlurSettings settings;settings.interval_seconds=1./60;settings.exposure_seconds=1./60;
    for(unsigned type=2;type<=7;++type) for(unsigned mode=0;mode<9;++mode) {
        SceneFxFrame fx;fx.add({16,8,3,.8f},{float(type),100,.15f,0},{1,.4f,.2f,0});
        fx.motion_previous[0]={{24,10,200},true};settings.paused=mode==3;
        // Exercise both words of the shader's candidate mask, including its
        // highest live bit, with intervening particles outside the viewport.
        for(unsigned n=1;n<scene_fx_capacity;++n) {
            fx.add({n==47?25.f:-1000.f,12,2,.7f},{float(type),100,.3f,0},{.1f,.3f,.8f,0});
            fx.motion_previous[n]={{n==47?20.f:-998.f,10,150},true};
        }
        settings.samples=mode==8?65:9;
        settings.exposure_seconds=mode==8?1./30:1./60;
        settings.maximum_radius=mode==7?4:32;
        if(mode==4) fx.motion_previous[0].previous={16,8,240}; // Near-plane crossing and expansion.
        if(mode==5) {fx.data[1][1]=9900;fx.motion_previous[0].previous={24,10,10500};}
        if(mode==6) {fx.data[0][0]=-4;fx.motion_previous[0].previous={24,8,100};}
        if(mode==7) fx.motion_previous[0].previous={100000,8,100};
        SurfaceBuffer depth(32,16);
        for(unsigned i=0;i<f.count;++i) {
            const float z=mode==1?20.f:mode==2?75.f:0.f;
            packed[i]=(i/32==8?256U:0U)|(z>0?0x01000000U:0U);
            surfaces[i]={0,0,-1,z};depth.set(i%32,i/32,{0,0,-1,z,0,z>0},0);
        }
        require(render_scene_shutter(fx,settings,frame,source,expected,&depth),"particle CPU reference");
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"particle command");
        f.encode_input(cb,source,guides);
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span(packed)));
        f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span(surfaces)));
        GpuCompositeOutput input{device,f.color,f.buffers[2],f.buffers[1],32,16};void* result{};
        require(gpu.enqueue(cb,input,fx,settings,1,result),gpu.status().c_str());
        const auto actual=f.finish(cb,static_cast<SDL_GPUTexture*>(result),unused);
        for(unsigned i=0;i<actual.size();++i) {
            require(std::abs(int(actual[i])-expected[i])<=1,"particle GPU/reference mismatch");
            if(i%4==3||i/(32*4)==8) require(actual[i]==source[i],"particle altered HUD/alpha");
        }
    }
    // A rejected pass must never hand the caller a stale texture or record a
    // partial exposure. Cancel each command just as the production caller will.
    for(unsigned invalid=0;invalid<9;++invalid) {
        SceneFxFrame fx;fx.add({16,8,3,.8f},{2,100,0,0},{1,.4f,.2f,0});
        auto shutter=settings;shutter.paused=false;
        GpuCompositeOutput input{device,f.color,f.buffers[2],f.buffers[1],32,16};
        switch(invalid) {
        case 0: fx.data[1][0]=1;break;
        case 1: fx.data[1][0]=8;break;
        case 2: fx.camera[3]=49;break;
        case 3: fx.camera[3]=.5f;break;
        case 4: fx.data[0][0]=std::numeric_limits<float>::quiet_NaN();break;
        case 5: shutter.samples=66;break;
        case 6: input.surfaces=nullptr;break;
        case 7: input.width=16385;break;
        case 8: fx.data[0][3]=2;break;
        }
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"invalid particle command");
        void* result=f.color;
        require(!gpu.enqueue(cb,input,fx,shutter,1,result)&&!result,"invalid particle pass retained output");
        require(SDL_CancelGPUCommandBuffer(cb),"cancel invalid particle pass");
    }
    {
        SceneFxFrame empty;GpuCompositeOutput input{device,f.color,nullptr,nullptr,32,16};
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"empty particle command");void* result{};
        require(gpu.enqueue(cb,input,empty,settings,1,result)&&result==f.color,"empty particle pass needs guides or changes texture");
        require(SDL_CancelGPUCommandBuffer(cb),"cancel empty particle pass");
    }
    std::cout<<"Particle shutter: six particle types, foreground and partial occlusion, pause, HUD, alpha, invalid inputs and empty bypass passed\n";
}
void check(SDL_GPUDevice* device) {
    GpuTemporalAa gpu;
    {
        SceneFxFrame scene;require(scene.surface_lighting_only(),"empty scene is not surface-only");
        for(unsigned type=0;type<=8;++type) {
            scene={};scene.add({0,0,10,1},{float(type),100,0,0},{});
            require(scene.surface_lighting_only()==(type==1),"spatial scene effect accepted as surface lighting");
            scene.add({0,0,10,1},{1,100,0,0},{});
            require(scene.surface_lighting_only()==(type==1),"mixed scene effects accepted as surface lighting");
        }
        for(float count:{-.5f,.5f,float(scene_fx_capacity+1),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            scene.camera[3]=count;require(!scene.surface_lighting_only(),"invalid scene count accepted");
        }
    }
    {
        constexpr unsigned w=32,h=16;
        Fixture source(device,w,h),back(device,w,h),normals(device,w,h);
        std::vector<uint8_t> rgba(w*h*4),underlay(w*h*4),styled,actual,expected;
        std::vector<TemporalAaGuide> input(w*h);
        std::vector<MotionBlurGuide> guides(w*h);
        std::vector<std::array<float,4>> surfaces(w*h);
        std::vector<uint32_t> ownership(w*h);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const auto i=y*w+x;const bool hud=y==0;
            const bool moving=x>=8 && x<13 && y>=4 && y<12;
            const float z=moving?300.f:x<w/2?400.f:500.f;
            const uint8_t c=((x/2+y/2)&1)?200:40;
            for(unsigned ch=0;ch<3;++ch) {rgba[i*4+ch]=hud?255:c;underlay[i*4+ch]=80;}
            rgba[i*4+3]=underlay[i*4+3]=255;
            input[i]={moving?4.f:0,0,z,z,true,!hud};
            guides[i]={moving?4.f:0,0,z,true,!hud};
            surfaces[i]={0,0,-1,z};
            ownership[i]=hud?256:0x01000000;
        }
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"depth blur upload");
        source.encode_input(cb,rgba,input);back.encode_input(cb,underlay,input);
        source.upload_buffer(cb,source.buffers[2],std::as_bytes(std::span(ownership)));
        normals.upload_buffer(cb,normals.buffers[1],std::as_bytes(std::span(surfaces)));
        require(SDL_SubmitGPUCommandBuffer(cb),"depth blur upload submit");
        GpuCompositeOutput scene{device,source.color,source.buffers[2],normals.buffers[1],w,h,source.buffers[0],source.buffers[1]};
        GpuCompositeOutput background{device,back.color,back.buffers[2],back.buffers[1],w,h};
        Framebuffer frame(w,h);SdlGpuEffects style,combined;
        MotionBlurSettings shutter;shutter.interval_seconds=shutter.exposure_seconds=1./60;
        for(unsigned light=0;light<=2;++light) for(unsigned modes=0;modes<=15;++modes) {
            if(!light && !modes) continue;
            GpuEffectSettings settings;settings.depth_fx.modes=modes;settings.depth_fx.camera={16,8,256,40};
            if(light) {
                settings.scene_fx.camera={16,8,256,0};
                settings.scene_fx.add({16,8,100,1},{1,100,light==2?1.f:0.f,0},{0,0,0,1000});
            }
            require(style.apply_resident(scene,frame,styled,settings),style.status().c_str());
            require(styled!=rgba,"depth blur fixture did not exercise depth appearance");
            std::vector<uint8_t> styled_background;
            require(style.apply_resident(background,frame,styled_background,settings),style.status().c_str());
            require(styled_background==underlay,"depth effects invented surfaces in the revealed underlay");
            require(reconstruct_motion_blur(w,h,styled,underlay,guides,shutter,expected),"depth blur CPU reconstruction");
            settings.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,true};
            require(combined.apply_resident(scene,frame,actual,settings),combined.status().c_str());
            unsigned error=0;for(unsigned i=0;i<actual.size();++i)
                error=std::max(error,unsigned(std::abs(int(actual[i])-int(expected[i]))));
            require(error<=2,"AO/DOF plus motion blur differs from styled CPU reconstruction");
            require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"depth motion blur changed HUD");
        }
        std::cout<<"Depth/light/motion blur: 47 AO/DOF and weapon-light combinations, moving foreground and protected HUD passed\n";
        // Resolved colour is shaded with aligned normals/ownership, while the
        // exposure pass still samples the original jittered velocity/depth.
        // Exercise all AO/DOF levels, not just the all-high gameplay preset.
        GpuTemporalSurfaces surface_resolve;
        for(const auto jitter:{std::array<float,2>{.375f,-.25f},{-.375f,.25f}}) {
            GpuCompositeOutput aligned;
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"depth aligned command");
            require(surface_resolve.enqueue(cb,scene,scene.packed,jitter,aligned),surface_resolve.status().c_str());
            require(SDL_SubmitGPUCommandBuffer(cb),"depth aligned submit");
            std::vector<MotionBlurGuide> aligned_guides;
            require(resolve_motion_blur_guides(w,h,guides,jitter,aligned_guides),"aligned depth reference");
            for(unsigned light=0;light<=2;++light) for(unsigned modes=1;modes<=15;++modes) {
                GpuEffectSettings settings;settings.depth_fx.modes=modes;settings.depth_fx.camera={16,8,256,40};
                if(light) {
                    settings.scene_fx.camera={16,8,256,0};
                    settings.scene_fx.add({16,8,100,1},{1,100,light==2?1.f:0.f,0},{0,0,0,1000});
                }
                require(style.apply_resident(aligned,frame,styled,settings),style.status().c_str());
                require(styled!=rgba,"aligned depth fixture had no visible effect");
                require(reconstruct_motion_blur(w,h,styled,underlay,aligned_guides,shutter,expected),"aligned depth CPU reconstruction");
                settings.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,true,jitter,scene};
                require(combined.apply_resident(aligned,frame,actual,settings),combined.status().c_str());
                for(unsigned i=0;i<actual.size();++i)
                    require(std::abs(int(actual[i])-int(expected[i]))<=2,"aligned AO/DOF exposure differs from reference");
                require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"aligned depth exposure changed HUD");
            }
        }
        std::cout<<"Aligned TAA depth/light exposure: 90 signed-jitter combinations and protected HUD passed\n";
        {
            // FSR replaces colour only. Surface, ownership and velocity must
            // remain the full-resolution native inputs, not reduced guides.
            Fixture reduced(device,w/2,h/2);GpuFsr1 fsr;
            std::vector<uint8_t> low(w*h);
            std::vector<TemporalAaGuide> low_guides(w*h/4);
            for(unsigned y=0;y<h/2;++y) for(unsigned x=0;x<w/2;++x)
                for(unsigned c=0;c<4;++c) low[(y*(w/2)+x)*4+c]=rgba[(y*2*w+x*2)*4+c];
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"FSR exposure upload");
            reduced.encode_input(cb,low,low_guides);
            GpuCompositeOutput low_scene{device,reduced.color,reduced.buffers[2],nullptr,w/2,h/2};
            auto resolved=fsr.enqueue_composite(cb,low_scene,scene,.2f);
            require(resolved.rgba && resolved.rgba!=scene.rgba,"FSR exposure resolve");
            require(resolved.packed==scene.packed && resolved.surfaces==scene.surfaces
                && resolved.geometry_depth==scene.geometry_depth && resolved.motion==scene.motion
                && resolved.width==w && resolved.height==h,"FSR replaced native exposure guides");
            require(SDL_SubmitGPUCommandBuffer(cb),"FSR exposure submit");
            std::vector<uint8_t> resolved_colour;
            require(style.apply_resident(resolved,frame,resolved_colour,{}),style.status().c_str());
            require(resolved_colour!=rgba,"FSR exposure colour fixture was a no-op");
            for(unsigned light=0;light<=2;++light) for(unsigned modes=0;modes<=15;++modes) {
                if(!light && !modes) continue;
                GpuEffectSettings settings;settings.depth_fx.modes=modes;settings.depth_fx.camera={16,8,256,40};
                if(light) {
                    settings.scene_fx.camera={16,8,256,0};
                    settings.scene_fx.add({16,8,100,1},{1,100,light==2?1.f:0.f,0},{0,0,0,1000});
                }
                require(style.apply_resident(resolved,frame,styled,settings),style.status().c_str());
                require(reconstruct_motion_blur(w,h,styled,underlay,guides,shutter,expected),"FSR styled exposure reference");
                settings.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,true};
                require(combined.apply_resident(resolved,frame,actual,settings),combined.status().c_str());
                for(unsigned i=0;i<actual.size();++i)
                    require(std::abs(int(actual[i])-int(expected[i]))<=2,"FSR depth/light exposure mismatch");
                require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"FSR exposure changed native HUD");
            }
            std::cout<<"FSR exposure: native guides/HUD retained, 47 depth/light combinations passed\n";
            frame.enable_layer_tags(true);
            for(unsigned x=0;x<w;++x) frame.set_stored(x,0,0,PixelLayer::two_d);
            for(unsigned type=2;type<=7;++type) for(float z:{100.f,450.f,600.f}) {
                SceneFxFrame particles;particles.camera={16,8,256,0};
                particles.add({11,7,4,1},{float(type),z,.2f,0},{1,.4f,.2f,0});
                particles.motion_previous[0]={{23,9,z},true};
                require(render_scene_shutter(particles,shutter,frame,resolved_colour,expected,nullptr,0,0,guides,underlay),
                    "FSR joint particle reference");
                GpuEffectSettings settings;
                settings.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,true};
                settings.particle_shutter=particles;
                require(combined.apply_resident(resolved,frame,actual,settings),combined.status().c_str());
                for(unsigned i=0;i<actual.size();++i)
                    require(std::abs(int(actual[i])-int(expected[i]))<=2,"FSR particle exposure/occlusion mismatch");
                require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"FSR particles changed native HUD");
                if(z==100) {
                    std::vector<uint8_t> model_only;
                    require(reconstruct_motion_blur(w,h,resolved_colour,underlay,guides,shutter,model_only),"FSR model-only reference");
                    require(actual!=model_only,"FSR particle fixture was invisible");
                }
            }
            std::cout<<"FSR particles: six types at front/partial/hidden depths match joint exposure\n";
            Fixture rays(device,w,h);
            std::vector<uint32_t> reflection(w*h),shadow(w*h);
            for(unsigned i=0;i<w*h;++i) {
                reflection[i]=((i%3==0?254U:255U)<<24)|0x003366ccU;
                shadow[i]=(i*17)%180;
            }
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"FSR ray upload");
            rays.upload_buffer(cb,rays.buffers[0],std::as_bytes(std::span(reflection)));
            rays.upload_buffer(cb,rays.buffers[1],std::as_bytes(std::span(shadow)));
            require(SDL_SubmitGPUCommandBuffer(cb),"FSR ray submit");
            for(bool conductor:{false,true}) for(bool with_particles:{false,true}) {
                GpuEffectSettings settings;settings.shadow_before_style=true;
                settings.shadow_width=w;settings.shadow_height=h;
                settings.resident_shadow={device,rays.buffers[1],w,h,0};
                settings.resident_reflection={device,rays.buffers[0],w,h,w*4};
                settings.reflection_material=conductor;settings.reflection_intensity=65;
                // Get the styled colour separately; the reference shutter is
                // independent of the GPU combined pass. Ray coordinates must
                // stay full-resolution even though FSR's input is half-size.
                require(style.apply_resident(resolved,frame,styled,settings),style.status().c_str());
                require(styled!=resolved_colour,"FSR ray fixture was a no-op");
                SceneFxFrame particles;particles.camera={16,8,256,0};
                particles.add({11,7,4,1},{6,450,.2f,0},{1,.4f,.2f,0});
                particles.motion_previous[0]={{23,9,450},true};
                if(with_particles)
                    require(render_scene_shutter(particles,shutter,frame,styled,expected,nullptr,0,0,guides,underlay),"FSR ray particle reference");
                else require(reconstruct_motion_blur(w,h,styled,underlay,guides,shutter,expected),"FSR ray reference");
                settings.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,true};
                if(with_particles) settings.particle_shutter=particles;
                require(combined.apply_resident(resolved,frame,actual,settings),combined.status().c_str());
                for(unsigned i=0;i<actual.size();++i)
                    require(std::abs(int(actual[i])-int(expected[i]))<=2,"FSR ray exposure ordering mismatch");
                require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"FSR ray exposure changed HUD");
            }
            std::cout<<"FSR rays: combined shadows/reflections, dielectric/conductor and particle exposure passed\n";
        }
        {
            // Fixed-time shadow composition prerequisite. This does not model
            // an animated caster's shadow across the shutter interval.
            constexpr unsigned sw=w-3,sh=h-2,stride=(sw+3)&~3U;
            Fixture mask(device,w,h),shadow_background(device,w,h);
            std::vector<uint8_t> shades(sw*sh),packed_shades(stride*sh),shadowed=rgba,shadowed_underlay=underlay;
            std::vector<uint32_t> wide_shades(sw*sh);
            for(unsigned y=0;y<sh;++y) for(unsigned x=0;x<sw;++x) {
                const auto shade=uint8_t((x*17+y*29)%240);const auto i=y*sw+x;
                shades[i]=shade;wide_shades[i]=shade;packed_shades[y*stride+x]=shade;
                const auto dst=((y+2)*w+x)*4;
                for(unsigned c=0;c<3;++c) {
                    shadowed[dst+c]=uint8_t(unsigned(rgba[dst+c])*(255-shade)/255);
                    shadowed_underlay[dst+c]=uint8_t(unsigned(underlay[dst+c])*(255-shade)/255);
                }
            }
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"shadow exposure upload");
            mask.upload_buffer(cb,mask.buffers[0],std::as_bytes(std::span(wide_shades)));
            mask.upload_buffer(cb,mask.buffers[1],std::as_bytes(std::span(packed_shades)));
            shadow_background.encode_input(cb,shadowed_underlay,input);
            require(SDL_SubmitGPUCommandBuffer(cb),"shadow exposure upload submit");
            auto shadow_back=background;shadow_back.rgba=shadow_background.color;
            frame.enable_layer_tags(true);
            for(unsigned x=0;x<w;++x) frame.set_stored(x,0,0,PixelLayer::two_d);
            SceneFxFrame particles;particles.camera={16,8,256,0};
            particles.add({10,8,4,1},{6,200,0,0},{1,.4f,.2f,0});
            particles.motion_previous[0]={{20,9,220},true};
            for(unsigned format=0;format<3;++format) for(bool with_particles:{false,true}) {
                GpuEffectSettings settings;settings.shadow_width=sw;settings.shadow_height=sh;
                settings.shadow_offset_y=2;settings.shadow_before_style=true;
                if(format==0) settings.shadow_mask=shades;
                else settings.resident_shadow={device,format==1?mask.buffers[0]:mask.buffers[1],sw,sh,format==2?stride:0};
                require(style.apply_resident(scene,frame,styled,settings),style.status().c_str());
                require(styled==shadowed,"shadow format/offset differs from CPU shade reference");
                if(with_particles)
                    require(render_scene_shutter(particles,shutter,frame,shadowed,expected,nullptr,0,0,guides,shadowed_underlay),"shadow particle CPU exposure");
                else require(reconstruct_motion_blur(w,h,shadowed,shadowed_underlay,guides,shutter,expected),"shadow CPU exposure");
                settings.motion_blur=GpuEffectSettings::MotionBlurPass{shadow_back,shutter,true};
                if(with_particles) settings.particle_shutter=particles;
                require(combined.apply_resident(scene,frame,actual,settings),combined.status().c_str());
                for(unsigned i=0;i<actual.size();++i)
                    require(std::abs(int(actual[i])-int(expected[i]))<=2,"shadow/model/particle ordering differs from CPU exposure");
                require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"shadow exposure changed HUD");
                for(const auto jitter:{std::array<float,2>{.375f,-.25f},{-.375f,.25f}}) {
                    GpuCompositeOutput aligned;
                    cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"shadow aligned command");
                    require(surface_resolve.enqueue(cb,scene,scene.packed,jitter,aligned),surface_resolve.status().c_str());
                    require(SDL_SubmitGPUCommandBuffer(cb),"shadow aligned submit");
                    std::vector<MotionBlurGuide> aligned_guides;
                    require(resolve_motion_blur_guides(w,h,guides,jitter,aligned_guides),"shadow aligned guides");
                    // Ray masks already use the unjittered presentation camera.
                    // Only geometry guides move; shifting the mask again would
                    // visibly move this nonuniform shadow across the surface.
                    if(with_particles)
                        require(render_scene_shutter(particles,shutter,frame,shadowed,expected,nullptr,0,0,aligned_guides,shadowed_underlay),"aligned shadow particle reference");
                    else require(reconstruct_motion_blur(w,h,shadowed,shadowed_underlay,aligned_guides,shutter,expected),"aligned shadow reference");
                    auto temporal=settings;
                    temporal.motion_blur=GpuEffectSettings::MotionBlurPass{shadow_back,shutter,true,jitter,scene};
                    require(combined.apply_resident(aligned,frame,actual,temporal),combined.status().c_str());
                    for(unsigned i=0;i<actual.size();++i)
                        require(std::abs(int(actual[i])-int(expected[i]))<=2,"TAA shadow mask or exposure alignment mismatch");
                    require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"TAA shadow exposure changed HUD");
                }
                if(with_particles) {
                    settings.shadow_before_style=false;
                    SdlGpuEffects rejected;
                    require(!rejected.apply_resident(scene,frame,actual,settings),"late shadows accepted after particle exposure");
                }
            }
            std::cout<<"Shadow exposure: CPU/u32/packed masks, offset/stride, signed TAA jitter, particles, HUD and late-order rejection passed\n";
        }
        {
            constexpr unsigned rw=w-3,rh=h-2;
            Fixture reflections(device,w,h);
            std::vector<uint32_t> rays(rw*rh);
            for(unsigned y=0;y<rh;++y) for(unsigned x=0;x<rw;++x) {
                const unsigned marker=x%3==0?0:x%3==1?254:255;
                rays[y*rw+x]=(marker<<24)|(10U<<16)|(50U<<8)|220U;
            }
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"reflection exposure upload");
            reflections.upload_buffer(cb,reflections.buffers[0],std::as_bytes(std::span(rays)));
            require(SDL_SubmitGPUCommandBuffer(cb),"reflection exposure upload submit");
            SceneFxFrame particles;particles.camera={16,8,256,0};
            particles.add({10,8,4,1},{6,200,0,0},{1,.4f,.2f,0});
            particles.motion_previous[0]={{20,9,220},true};
            for(bool conductor:{false,true}) for(unsigned intensity:{0U,35U,100U}) for(bool with_particles:{false,true}) {
                GpuEffectSettings settings;
                settings.resident_reflection={device,reflections.buffers[0],rw,rh,rw*4};
                settings.reflection_offset_y=2;settings.reflection_material=conductor;settings.reflection_intensity=intensity;
                auto reflected=rgba;
                for(unsigned y=0;y<rh;++y) for(unsigned x=0;x<rw;++x) {
                    if((rays[y*rw+x]>>24)!=255) continue; // ground/miss markers are not model reflections
                    unsigned alpha=intensity*255/100;
                    if(!conductor) alpha=std::min(unsigned(std::lround(float(alpha)*.08f)),77U);
                    const unsigned dst=((y+2)*w+x)*4;
                    for(unsigned c=0;c<3;++c) {
                        const unsigned reflected_channel=(rays[y*rw+x]>>(c*8))&255;
                        reflected[dst+c]=uint8_t((reflected_channel*alpha+rgba[dst+c]*(255-alpha)+127)/255);
                    }
                }
                require(style.apply_resident(scene,frame,styled,settings),style.status().c_str());
                require(styled==reflected,"model reflection markers/material/intensity mismatch");
                std::vector<uint8_t> exposed_background;
                require(style.apply_resident(background,frame,exposed_background,settings),style.status().c_str());
                require(exposed_background==underlay,"model reflections leaked into surface-free underlay");
                if(with_particles)
                    require(render_scene_shutter(particles,shutter,frame,reflected,expected,nullptr,0,0,guides,underlay),"reflection particle CPU exposure");
                else require(reconstruct_motion_blur(w,h,reflected,underlay,guides,shutter,expected),"reflection CPU exposure");
                settings.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,true};
                if(with_particles) settings.particle_shutter=particles;
                require(combined.apply_resident(scene,frame,actual,settings),combined.status().c_str());
                for(unsigned i=0;i<actual.size();++i)
                    require(std::abs(int(actual[i])-int(expected[i]))<=2,"model reflection exposure ordering differs from reference");
                require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"model reflection exposure changed HUD");
                for(const auto jitter:{std::array<float,2>{.375f,-.25f},{-.375f,.25f}}) {
                    GpuCompositeOutput aligned;
                    cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"reflection aligned command");
                    require(surface_resolve.enqueue(cb,scene,scene.packed,jitter,aligned),surface_resolve.status().c_str());
                    require(SDL_SubmitGPUCommandBuffer(cb),"reflection aligned submit");
                    std::vector<MotionBlurGuide> aligned_guides;
                    require(resolve_motion_blur_guides(w,h,guides,jitter,aligned_guides),"reflection aligned guides");
                    // Preserve the ray buffer's native coordinates and markers.
                    // The shutter must use resolved geometry, not shift rays.
                    if(with_particles)
                        require(render_scene_shutter(particles,shutter,frame,reflected,expected,nullptr,0,0,aligned_guides,underlay),"aligned reflection particle reference");
                    else require(reconstruct_motion_blur(w,h,reflected,underlay,aligned_guides,shutter,expected),"aligned reflection reference");
                    auto temporal=settings;
                    temporal.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,true,jitter,scene};
                    require(combined.apply_resident(aligned,frame,actual,temporal),combined.status().c_str());
                    for(unsigned i=0;i<actual.size();++i)
                        require(std::abs(int(actual[i])-int(expected[i]))<=2,"TAA reflection markers or exposure alignment mismatch");
                    require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"TAA reflection exposure changed HUD");
                }
            }
            std::cout<<"Reflection exposure: dielectric/conductor, intensity, markers, signed TAA jitter, particles, clean underlay and HUD passed\n";
        }
        // Independent joint CPU model/particle exposure. Run bloom only after
        // temporal integration, matching the production composition order.
        frame.enable_layer_tags(true);
        for(unsigned x=0;x<w;++x) frame.set_stored(x,0,0,PixelLayer::two_d);
        SurfaceBuffer depth(w,h);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
            depth.set(x,y,{0,0,-1,surfaces[y*w+x][3],0,true},0);
        Fixture reference(device,w,h);
        for(unsigned bloom=0;bloom<=3;++bloom) for(bool history:{false,true}) {
            SceneFxFrame particles;particles.camera={16,8,256,0};
            particles.add({11,7,4,1},{6,200,0,0},{1,.3f,.1f,0});
            particles.motion_previous[0]={{23,9,220},true};
            for(unsigned n=1;n<47;++n)
                particles.add({-1000,9,1,1},{6,200,0,0},{1,.2f,.1f,0});
            particles.add({21,9,3,.9f},{7,350,.2f,0},{.2f,.8f,1,0});
            particles.motion_previous[47]={{14,8,300},true};
            auto cpu_particles=particles;
            if(!history) for(auto& previous:cpu_particles.motion_previous) previous={};
            std::vector<uint8_t> model_exposed=rgba,particle_exposed;
            if(history) require(reconstruct_motion_blur(w,h,rgba,underlay,guides,shutter,model_exposed),"combined model reference");
            auto combined_shutter=shutter;if(!history) combined_shutter.paused=true;
            require(render_scene_shutter(cpu_particles,combined_shutter,frame,rgba,particle_exposed,nullptr,0,0,guides,underlay),"combined particle reference");
            require(particle_exposed!=model_exposed,"combined fixture has no visible particles");
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"combined reference upload");
            reference.encode_input(cb,particle_exposed,input);
            reference.upload_buffer(cb,reference.buffers[2],std::as_bytes(std::span(ownership)));
            require(SDL_SubmitGPUCommandBuffer(cb),"combined reference submit");
            GpuCompositeOutput ref{device,reference.color,reference.buffers[2],normals.buffers[1],w,h};
            GpuEffectSettings post;post.bloom_model=post.bloom_world=bloom;
            require(style.apply_resident(ref,frame,expected,post),style.status().c_str());
            auto full=post;full.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,history};
            full.particle_shutter=particles;
            require(combined.apply_resident(scene,frame,actual,full),combined.status().c_str());
            unsigned error=0;for(unsigned i=0;i<actual.size();++i)
                error=std::max(error,unsigned(std::abs(int(actual[i])-int(expected[i]))));
            require(error<=2,"combined particle/model exposure or bloom ordering differs");
            require(std::equal(actual.begin(),actual.begin()+w*4,rgba.begin()),"combined particle exposure changed HUD");
            if(bloom==0) {
                auto joint_settings=shutter;if(!history) joint_settings.paused=true;
                require(render_scene_shutter(cpu_particles,joint_settings,frame,rgba,expected,nullptr,0,0,guides,underlay),
                    "joint CPU exposure reference");
                GpuMotionBlur joint;GpuTemporalAa unused;
                cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"joint GPU command");void* joint_output{};
                require(joint.enqueue(cb,scene,background,joint_settings,history,joint_output,{},&particles,1),joint.status().c_str());
                actual=source.finish(cb,static_cast<SDL_GPUTexture*>(joint_output),unused);
                unsigned joint_error=0;for(unsigned i=0;i<actual.size();++i)
                    joint_error=std::max(joint_error,unsigned(std::abs(int(actual[i])-int(expected[i]))));
                require(joint_error<=2,"joint GPU moving-occluder exposure differs from CPU");
                if(!history) for(unsigned depth_mode=0;depth_mode<3;++depth_mode) {
                    auto reset_input=scene;reset_input.motion=nullptr;
                    if(depth_mode!=1) reset_input.geometry_depth=nullptr;
                    if(depth_mode==0) reset_input.surfaces=nullptr;
                    require(render_scene_shutter(cpu_particles,joint_settings,frame,rgba,expected,depth_mode?&depth:nullptr),
                        "missing-guide reset CPU reference");
                    cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"missing-guide reset command");
                    require(joint.enqueue(cb,reset_input,background,joint_settings,false,joint_output,{},&particles,1),joint.status().c_str());
                    actual=source.finish(cb,static_cast<SDL_GPUTexture*>(joint_output),unused);
                    for(unsigned i=0;i<actual.size();++i)
                        require(std::abs(int(actual[i])-int(expected[i]))<=2,"reset without motion/depth dropped particles or changed scene");
                    cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"missing-guide history command");
                    joint_output=scene.rgba;
                    require(!joint.enqueue(cb,reset_input,background,shutter,true,joint_output,{},&particles,1)&&!joint_output,
                        "valid-history exposure accepted missing velocity guides");
                    require(SDL_CancelGPUCommandBuffer(cb),"cancel missing-guide history command");
                }
                if(history) {
                    for(const auto jitter:{std::array<float,2>{.25f,-.375f},std::array<float,2>{-.25f,.375f}}) {
                        std::vector<MotionBlurGuide> aligned;
                        require(resolve_motion_blur_guides(w,h,guides,jitter,aligned),"joint jitter reference alignment");
                        require(render_scene_shutter(cpu_particles,shutter,frame,rgba,expected,nullptr,0,0,aligned,underlay),"joint jitter CPU reference");
                        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"joint jitter command");
                        require(joint.enqueue(cb,scene,background,shutter,true,joint_output,jitter,&particles,1),joint.status().c_str());
                        actual=source.finish(cb,static_cast<SDL_GPUTexture*>(joint_output),unused);
                        for(unsigned i=0;i<actual.size();++i)
                            require(std::abs(int(actual[i])-int(expected[i]))<=2,"joint jittered depth/velocity differs from reference");
                        GpuEffectSettings resolved;
                        resolved.motion_blur=GpuEffectSettings::MotionBlurPass{background,shutter,true,jitter,scene};
                        resolved.particle_shutter=particles;
                        require(combined.apply_resident(scene,frame,actual,resolved),combined.status().c_str());
                        for(unsigned i=0;i<actual.size();++i)
                            require(std::abs(int(actual[i])-int(expected[i]))<=2,"joint compositor lost jittered guide source");
                    }
                    // Same instance, same extent: shrink to model-only storage,
                    // then grow again. Neither transition may retain mask state.
                    void* stable_output=joint_output;
                    for(bool with_particles:{false,true}) {
                        if(with_particles)
                            require(render_scene_shutter(cpu_particles,joint_settings,frame,rgba,expected,nullptr,0,0,guides,underlay),"joint resize reference");
                        else require(reconstruct_motion_blur(w,h,rgba,underlay,guides,shutter,expected),"model resize reference");
                        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"joint resize command");
                        require(joint.enqueue(cb,scene,background,shutter,true,joint_output,{},with_particles?&particles:nullptr,1),joint.status().c_str());
                        require(joint_output==stable_output,"particle toggle unnecessarily recreated output texture");
                        actual=source.finish(cb,static_cast<SDL_GPUTexture*>(joint_output),unused);
                        for(unsigned i=0;i<actual.size();++i)
                            require(std::abs(int(actual[i])-int(expected[i]))<=2,"particle storage transition changed exposure");
                    }
                }
            }
        }
        std::cout<<"Combined model/particle exposure: reset, four bloom levels, signed jitter and compositor guide forwarding match joint reference\n";
    }
    // Exercise the diagnostic motion-blur bridge through real resident
    // composition, not just an independently populated CPU guide array.
    {
        Fixture native(device,9,3);
        std::vector<std::uint8_t> unused(native.count*4,255);
        std::vector<TemporalAaGuide> input(native.count,{0,0,100,100,true,true});
        std::vector<std::uint32_t> packed(native.count,1);
        for(unsigned y=0;y<3;++y) {
            const auto i=y*9+4;input[i]={4,0,10,10,true,true};packed[i]=2;
        }
        packed[0]=3|(1U<<8);input[0].eligible=false;
        // This stationary green surface differs from the blue underlay and
        // is only reached by the moving red stripe late in the shutter.
        packed[15]=4;
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"motion bridge command");
        native.encode_input(cb,unused,input);
        native.upload_buffer(cb,native.buffers[2],std::as_bytes(std::span(packed)));
        require(SDL_SubmitGPUCommandBuffer(cb),"motion bridge upload");
        GpuRasterOutput raster{device,native.buffers[2],nullptr,9,3,1,native.buffers[0],native.buffers[1]};
        Framebuffer backing(9,3),read(9,3);backing.enable_layer_tags(true);read.enable_layer_tags(true);
        Palette256 palette{};palette[1]={0,0,255,255};palette[2]={255,0,0,255};palette[3]={255,255,255,255};
        palette[4]={0,255,0,255};
        GpuComposite composite;
        require(composite.compose(raster,1,backing,{}, {},palette),composite.status().c_str());
        std::vector<MotionBlurGuide> guides;
        require(composite.readback_motion_guides(true,guides),composite.status().c_str());
        require(guides.size()==native.count&&!guides[0].eligible&&guides[13].valid
            &&guides[13].motion_x==4&&guides[13].depth==10,"resident motion guide decode");
        std::vector<std::uint8_t> rgba,blurred,underlay(native.count*4,0);
        require(composite.readback(read,rgba),composite.status().c_str());
        for(unsigned i=0;i<native.count;++i) {underlay[i*4+2]=255;underlay[i*4+3]=255;}
        MotionBlurSettings shutter;shutter.interval_seconds=shutter.exposure_seconds=1./60;
        require(reconstruct_motion_blur(9,3,rgba,underlay,guides,shutter,blurred),"resident-guide reconstruction");
        require(blurred[12*4]>0&&blurred[13*4]<255&&blurred[13*4+2]>0,"resident silhouette not reconstructed");
        require(std::equal(blurred.begin(),blurred.begin()+4,rgba.begin()),"resident blur changed HUD");
        Fixture background(device,9,3);
        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"blur underlay command");
        background.encode_input(cb,underlay,input);
        require(SDL_SubmitGPUCommandBuffer(cb),"blur underlay upload");
        const GpuCompositeOutput underlay_input{device,background.color,nullptr,nullptr,9,3};
        GpuMotionBlur motion_blur;
        for(unsigned samples:{3U,9U,17U,65U}) {
            shutter.samples=samples;
            require(reconstruct_motion_blur(9,3,rgba,underlay,guides,shutter,blurred),"GPU blur reference");
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"blur command");void* texture{};
            require(motion_blur.enqueue(cb,composite.output(),underlay_input,shutter,true,texture),motion_blur.status().c_str());
            require(motion_blur.dispatch_count()==3*samples+1,"motion blur redundant clear passes");
            const auto actual=native.finish(cb,static_cast<SDL_GPUTexture*>(texture),gpu);
            unsigned error=0;for(unsigned i=0;i<actual.size();++i)
                error=std::max(error,unsigned(std::abs(int(actual[i])-int(blurred[i]))));
            require(error<=2,"GPU motion blur differs from reference");
            require(std::equal(actual.begin(),actual.begin()+4,rgba.begin()),"GPU motion blur changed HUD");
        }
        // Fractional/vertical motion, even shutter taps (vacated centre),
        // bounded extreme vectors, stationary input and a near occluder.
        for(unsigned scenario=0;scenario<6;++scenario) {
            for(unsigned y=0;y<3;++y) {
                auto& g=input[y*9+4];g.motion_x=scenario==0?-3.375f:scenario==2?1e38f:scenario==3?0:4;
                g.motion_y=scenario==1?2.25f:0;
            }
            if(scenario==4) input[12].depth=1;
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"blur scenario upload");
            native.encode_input(cb,unused,input);native.upload_buffer(cb,native.buffers[2],std::as_bytes(std::span(packed)));
            require(SDL_SubmitGPUCommandBuffer(cb),"blur scenario submit");
            require(composite.compose(raster,1,backing,{}, {},palette)&&composite.readback(read,rgba)
                &&composite.readback_motion_guides(true,guides),"blur scenario composition");
            shutter.samples=scenario==5?2:9;
            require(reconstruct_motion_blur(9,3,rgba,underlay,guides,shutter,blurred),"blur scenario reference");
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"blur scenario command");void* texture{};
            require(motion_blur.enqueue(cb,composite.output(),underlay_input,shutter,true,texture),motion_blur.status().c_str());
            const auto actual=native.finish(cb,static_cast<SDL_GPUTexture*>(texture),gpu);
            unsigned error=0;for(unsigned i=0;i<actual.size();++i)
                error=std::max(error,unsigned(std::abs(int(actual[i])-int(blurred[i]))));
            if(error>2) std::cerr<<"motion scenario="<<scenario<<" error="<<error<<'\n';
            require(error<=2,"GPU motion blur scenario differs from reference");
            for(const std::array<float,2> jitter:{std::array<float,2>{.375f,-.25f},{-.375f,.25f},{1.f,1.f},{0.f,0.f}}) {
                std::vector<MotionBlurGuide> aligned;
                require(resolve_motion_blur_guides(9,3,guides,jitter,aligned),"jittered guide reference failed");
                require(reconstruct_motion_blur(9,3,rgba,underlay,aligned,shutter,blurred),"aligned blur reference failed");
                cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"aligned blur command");texture=nullptr;
                require(motion_blur.enqueue(cb,composite.output(),underlay_input,shutter,true,texture,jitter),motion_blur.status().c_str());
                const auto aligned_actual=native.finish(cb,static_cast<SDL_GPUTexture*>(texture),gpu);
                unsigned aligned_error=0;
                for(unsigned i=0;i<aligned_actual.size();++i)
                    aligned_error=std::max(aligned_error,unsigned(std::abs(int(aligned_actual[i])-int(blurred[i]))));
                if(aligned_error>2) std::cerr<<"aligned motion scenario="<<scenario<<" jitter="<<jitter[0]<<','<<jitter[1]<<" error="<<aligned_error<<'\n';
                require(aligned_error<=2,"GPU aligned blur differs from reference");
                require(std::equal(aligned_actual.begin(),aligned_actual.begin()+4,rgba.begin()),"aligned blur changed HUD");
            }
        }
        for(unsigned reason=0;reason<3;++reason) {
            auto identity=shutter;identity.paused=reason==0;if(reason==2) identity.interval_seconds=1;
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"identity blur command");void* texture{};
            require(motion_blur.enqueue(cb,composite.output(),underlay_input,identity,reason!=1,texture),motion_blur.status().c_str());
            require(motion_blur.dispatch_count()==0&&texture==composite.output().rgba,"identity blur dispatched or copied");
            require(native.finish(cb,static_cast<SDL_GPUTexture*>(texture),gpu)==rgba,"GPU blur pause/cut/gap not identity");
        }
        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"invalid blur command");void* rejected=background.color;
        auto first_frame=composite.output();first_frame.packed=nullptr;first_frame.geometry_depth=nullptr;first_frame.motion=nullptr;
        require(motion_blur.enqueue(cb,first_frame,{},shutter,false,rejected)
            &&rejected==first_frame.rgba&&motion_blur.dispatch_count()==0,"first frame without motion must bypass blur");
        auto invalid=shutter;invalid.maximum_radius=129;
        require(!motion_blur.enqueue(cb,composite.output(),underlay_input,invalid,true,rejected)&&!rejected,"invalid blur output retained");
        SDL_CancelGPUCommandBuffer(cb);
        motion_blur.release_device();
        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"reacquire blur command");
        require(motion_blur.enqueue(cb,composite.output(),underlay_input,shutter,true,rejected),motion_blur.status().c_str());
        require(!native.finish(cb,static_cast<SDL_GPUTexture*>(rejected),gpu).empty(),"reacquired blur empty");
        {
            Fixture styled(device,9,3),styled_back(device,9,3);
            SdlGpuEffects scene_effects,background_effects,expected_effects,combined_effects;
            GpuEffectSettings appearance;appearance.hdr=1;appearance.presentation_texture=styled.color;
            std::vector<std::uint8_t> scratch,expected,actual;
            if(!scene_effects.apply_resident(composite.output(),read,scratch,appearance)) throw std::runtime_error("scene appearance: "+scene_effects.status());
            auto background_input=underlay_input;background_input.packed=background.buffers[2];
            // Contrast reads no normals, but the resident compositor contract
            // still requires a valid surface buffer binding.
            background_input.surfaces=composite.output().surfaces;
            appearance.presentation_texture=styled_back.color;
            if(!background_effects.apply_resident(background_input,read,scratch,appearance)) throw std::runtime_error("background appearance: "+background_effects.status());
            auto styled_input=composite.output();styled_input.rgba=styled.color;
            auto styled_underlay=underlay_input;styled_underlay.rgba=styled_back.color;
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"styled motion command");void* texture{};
            require(motion_blur.enqueue(cb,styled_input,styled_underlay,shutter,true,texture),motion_blur.status().c_str());
            require(SDL_SubmitGPUCommandBuffer(cb),"styled motion submit");
            auto expected_input=styled_input;expected_input.rgba=texture;
            GpuEffectSettings presentation;
            presentation.circle=GpuEffectSettings::Circle{4,1,1,0,0,8,2,3,5,7,false,false,true};
            presentation.colour_math=GpuEffectSettings::ColourMath{7,11,13,false,false,true};
            presentation.host_overlay=GpuEffectSettings::HostOverlay{};
            presentation.host_overlay->x=2;presentation.host_overlay->width=1;
            presentation.host_overlay->height=1;presentation.host_overlay->bits[0]=1;
            require(expected_effects.apply_resident(expected_input,read,expected,presentation),expected_effects.status().c_str());
            auto combined=presentation;combined.hdr=1;
            combined.motion_blur=GpuEffectSettings::MotionBlurPass{styled_underlay,shutter,true};
            if(!combined_effects.apply_resident(composite.output(),read,actual,combined)) throw std::runtime_error("combined appearance: "+combined_effects.status());
            require(actual==expected,"motion pipeline must style before blur and composite flashes/HUD afterward");
            presentation.camera_response=CameraResponsePose{.01,-.009,.08};
            presentation.camera_response_focal={8,8};
            combined.camera_response=presentation.camera_response;combined.camera_response_focal={8,8};
            require(expected_effects.apply_resident(expected_input,read,expected,presentation),"reference blur/camera composition");
            require(combined_effects.apply_resident(composite.output(),read,actual,combined),"combined blur/camera composition");
            require(actual==expected,"camera reprojection must follow motion exposure");
            presentation.camera_response.reset();
            combined.camera_response.reset();
            // Resolved colour with separately supplied jittered guides must
            // reach the same blur as a direct invocation, retaining the final
            // scene's HUD ownership rather than borrowing the world's tags.
            for(const std::array<float,2> jitter:{std::array<float,2>{.375f,-.25f},{-.375f,.25f}}) {
                cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"aligned styled motion command");
                require(motion_blur.enqueue(cb,styled_input,styled_underlay,shutter,true,texture,jitter),motion_blur.status().c_str());
                require(SDL_SubmitGPUCommandBuffer(cb),"aligned styled motion submit");
                expected_input.rgba=texture;
                require(expected_effects.apply_resident(expected_input,read,expected,presentation),"aligned reference presentation");
                combined.motion_blur->guide_jitter=jitter;
                combined.motion_blur->guide_source=composite.output();
                auto without_guides=composite.output();without_guides.geometry_depth=nullptr;without_guides.motion=nullptr;
                require(combined_effects.apply_resident(without_guides,read,actual,combined),"aligned combined presentation");
                require(actual==expected,"compositor dropped jitter or separate motion guides");
            }
            // Disabling the pass must not reuse its old output or change legacy ordering.
            require(expected_effects.apply_resident(composite.output(),read,expected,presentation),expected_effects.status().c_str());
            require(combined_effects.apply_resident(composite.output(),read,actual,presentation),combined_effects.status().c_str());
            require(actual==expected,"disabled motion pipeline retains stale blur");
        }
        {
            GpuTemporalAa resolve;
            for(const std::array<float,2> jitter:{std::array<float,2>{.375f,-.25f},{-.375f,.25f},{0.f,0.f}}) {
                GpuTemporalAaSettings aa;aa.width=9;aa.height=3;aa.weight=0;
                aa.projection={8,8,4,1};aa.jitter={jitter[0],jitter[1],0,0};
                cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"TAA blur resolve command");
                const auto scene=composite.output();
                auto* stable=resolve.enqueue(device,cb,scene.rgba,scene.geometry_depth,scene.motion,scene.packed,aa);
                require(stable,resolve.status().c_str());
                const auto resolved=native.finish(cb,static_cast<SDL_GPUTexture*>(stable),resolve);
                std::vector<MotionBlurGuide> aligned;
                require(resolve_motion_blur_guides(9,3,guides,jitter,aligned),"TAA blur aligned reference guides");
                require(reconstruct_motion_blur(9,3,resolved,underlay,aligned,shutter,blurred),"TAA blur reference");
                auto stable_input=scene;stable_input.rgba=stable;
                cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"TAA blur exposure command");void* result{};
                require(motion_blur.enqueue(cb,stable_input,underlay_input,shutter,true,result,jitter),motion_blur.status().c_str());
                const auto actual=native.finish(cb,static_cast<SDL_GPUTexture*>(result),gpu);
                unsigned error=0;for(unsigned i=0;i<actual.size();++i)
                    error=std::max(error,unsigned(std::abs(int(actual[i])-int(blurred[i]))));
                require(error<=2,"actual TAA resolve followed by blur differs from CPU reconstruction");
                require(std::equal(actual.begin(),actual.begin()+4,rgba.begin()),"TAA blur changed protected HUD");
            }
            std::cout<<"TAA/motion blur: signed jitter, aligned guides, stable colour and protected HUD passed\n";
        }
        require(composite.readback_motion_guides(false,guides)
            &&std::none_of(guides.begin(),guides.end(),[](auto g){return g.valid;}),"resident cut kept velocity");
    }
    // Exercise the actual resident HUD restoration used by the runtime with
    // a reprojected world. Sky remains part of the moved world, not artwork
    // that should be copied back at its original screen coordinates.
    {
        Fixture original(device,128,64),warped(device,128,64);
        std::vector<std::uint8_t> world(original.count*4,255),final,coverage(original.count),response,expected;
        std::vector<TemporalAaGuide> guides(original.count);
        for(unsigned i=0;i<original.count;++i) {
            world[i*4]=std::uint8_t(32+(i%128)/2);world[i*4+1]=std::uint8_t(32+(i/128)*2);world[i*4+2]=64;
            guides[i].eligible=true;
        }
        final=world;
        for(unsigned y=20;y<30;++y) for(unsigned x=12;x<40;++x) {
            const auto i=y*128+x;coverage[i]=1;guides[i].eligible=false;
            if(x%2) final[i*4]=final[i*4+1]=final[i*4+2]=0;
        }
        const CameraResponsePose pose{.009,-.007,.024};
        require(apply_camera_response(world,response,128,64,80,80,pose),"camera reference warp");
        require(composite_camera_response(world,final,coverage,expected,128,64,80,80,pose),"camera reference HUD");
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"camera HUD command");
        original.encode_input(cb,final,guides);warped.encode_input(cb,response,guides);
        GpuTemporalInputs restore;
        auto* result=static_cast<SDL_GPUTexture*>(restore.restore_hud(device,cb,original.color,warped.color,original.buffers[2],128,64,false));
        require(result,restore.status().c_str());
        require(original.finish(cb,result,gpu)==expected,"camera response moved HUD or restored unrotated world");
        std::cout<<"Camera response resident HUD composition passed\n";
    }
    for(unsigned width:{16U,19U}) {
        Fixture f(device,width,13);TemporalAa cpu;
        for(unsigned frame=0;frame<12;++frame) {
            std::vector<uint8_t> pixels(f.count*4);
            std::vector<TemporalAaGuide> guides(f.count);
            for(unsigned i=0;i<f.count;++i) {
                const unsigned x=i%width,y=i/width;
                for(unsigned c=0;c<3;++c) pixels[i*4+c]=uint8_t((x*31+y*17+c*53+frame*7)%256);
                pixels[i*4+3]=uint8_t(i%256);
                guides[i]={0,0,100,100,true,y>1};
                if(frame==2) guides[i].motion_x=.25f;
                if(frame==3) guides[i].motion_x=-1.f;
                if(frame==4) guides[i].depth=guides[i].previous_depth=50;
                if(frame==5) guides[i].valid=false;
                if(frame==6) guides[i].motion_x=std::numeric_limits<float>::quiet_NaN();
                if(frame==7) guides[i].motion_y=1000;
                // Camera moves toward the surface: current Z changes but its
                // expected Z in the old camera still matches history.
                if(frame==9) {guides[i].depth=90;guides[i].previous_depth=100;}
            }
            if(frame==11) {gpu.discard();cpu.reset();}
            GpuTemporalAaSettings s;s.width=width;s.height=f.height;s.epoch=frame>=8?2:1;s.weight=.8f;
            if(frame==9) s.previous_z={0,0,1,10};
            if(frame==2 || frame==3) s.jitter={.125f,-.125f,-.125f,.125f};
            auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"acquire TAA command");
            f.encode_input(cb,pixels,guides);
            auto* result=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,f.buffers[0],f.buffers[1],f.buffers[2],s));
            require(result,gpu.status().c_str());
            if(frame==10) {
                require(SDL_CancelGPUCommandBuffer(cb),"cancel TAA");gpu.discard();cpu.reset();continue;
            }
            const auto actual=f.finish(cb,result,gpu);
            // Reference takes motion in sampled-image coordinates; native
            // vectors deliberately omit the jitter delta supplied to the GPU.
            for(auto& guide:guides) {guide.motion_x+=s.jitter[2]-s.jitter[0];guide.motion_y+=s.jitter[3]-s.jitter[1];}
            auto expected=cpu.resolve(width,f.height,pixels,guides,s.epoch,s.weight);
            if(s.jitter[0]!=0 || s.jitter[1]!=0) {
                const auto raster=expected;
                for(unsigned y=0;y<f.height;++y) for(unsigned x=0;x<width;++x) {
                    const auto i=y*width+x;if(!guides[i].eligible) continue;
                    const float px=std::clamp(float(x)+s.jitter[0],0.f,float(width-1));
                    const float py=std::clamp(float(y)+s.jitter[1],0.f,float(f.height-1));
                    const unsigned ax=unsigned(px),ay=unsigned(py);const float fx=px-ax,fy=py-ay;
                    std::array<float,4> sum{};bool geometry=false;
                    for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                        auto n=std::min(ay+dy,f.height-1)*width+std::min(ax+dx,width-1);
                        const float w=(dx?fx:1-fx)*(dy?fy:1-fy);
                        geometry=geometry || (w>0 && guides[n].eligible && guides[n].depth>0);
                        if(!guides[n].eligible) n=i;
                        for(unsigned c=0;c<4;++c) sum[c]+=raster[n*4+c]*w;
                    }
                    if(geometry) for(unsigned c=0;c<4;++c) expected[i*4+c]=uint8_t(std::lround(sum[c]));
                }
            }
            for(unsigned i=0;i<actual.size();++i) {
                const int tolerance=(!guides[i/4].eligible || (i%4==3 && s.jitter[0]==0 && s.jitter[1]==0))?0:1;
                if(std::abs(int(actual[i])-int(expected[i]))>tolerance)
                    throw std::runtime_error("TAA GPU/reference mismatch: frame="+std::to_string(frame)+" byte="+std::to_string(i)
                        +" expected="+std::to_string(expected[i])+" actual="+std::to_string(actual[i]));
            }
        }
    }
}
void check_temporal_surfaces(SDL_GPUDevice* device) {
    GpuTemporalSurfaces resolve;
    for(const auto size:{std::array<unsigned,4>{5,3,5,3},{5,3,11,7},{9,5,17,8},{9,5,9,5},{5,3,5,3}}) {
        Fixture f(device,size[0],size[1]),hud(device,size[2],size[3]);
        const bool resized=f.width!=hud.width || f.height!=hud.height;
        std::vector<uint32_t> packed(f.count,0x01010001u),protected_pixels(hud.count,0);
        std::vector<float> depths(f.count);
        std::vector<std::array<float,4>> surfaces(f.count);
        for(unsigned i=0;i<f.count;++i) {
            depths[i]=i%f.width==2?10.f:100.f+float(i/f.width);
            // Coplanar neighbours deliberately have different normals: equal
            // depth must select the strongest donor, not the first tap.
            surfaces[i]={float(i%f.width==2),float(i%f.width!=2),float(i%2),depths[i]};
        }
        protected_pixels[hud.width+2]=0x01010101u; // same palette as model, but HUD wins
        if(resized) protected_pixels[hud.width-2]=0x02000201u; // tile over stale model coverage
        packed.back()=0x01020001u; // stale surface owner must never produce lighting
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"surface upload command");
        f.upload_buffer(cb,f.buffers[0],std::as_bytes(std::span(depths)));
        f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span(surfaces)));
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span(packed)));
        hud.upload_buffer(cb,hud.buffers[2],std::as_bytes(std::span(protected_pixels)));
        require(SDL_SubmitGPUCommandBuffer(cb),"surface upload submit");
        GpuCompositeOutput source{device,f.color,f.buffers[2],f.buffers[1],f.width,f.height,f.buffers[0]};
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,hud.count*20,0};
        auto* download=SDL_CreateGPUTransferBuffer(device,&info);require(download,"surface download");
        for(const auto jitter:{std::array<float,2>{0,0},{.375f,-.25f},{-.375f,.25f},{.5f,.5f}}) {
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"surface resolve command");
            GpuCompositeOutput actual;
            require(resolve.enqueue(cb,source,hud.buffers[2],jitter,actual,{hud.width,hud.height}),resolve.status().c_str());
            require(actual.width==hud.width && actual.height==hud.height,"resolved metadata extent differs");
            if(resized) require(!actual.rgba && !actual.geometry_depth && !actual.motion,"resized metadata advertised mismatched image/depth/motion extents");
            auto* pass=SDL_BeginGPUCopyPass(cb);require(pass,"surface download pass");
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(actual.packed),0,hud.count*4};
            SDL_GPUTransferBufferLocation to{download,0};SDL_DownloadFromGPUBuffer(pass,&from,&to);
            from={static_cast<SDL_GPUBuffer*>(actual.surfaces),0,hud.count*16};to.offset=hud.count*4;
            SDL_DownloadFromGPUBuffer(pass,&from,&to);SDL_EndGPUCopyPass(pass);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cb);require(fence,"surface submit");
            require(SDL_WaitForGPUFences(device,true,&fence,1),"surface wait");SDL_ReleaseGPUFence(device,fence);
            auto* bytes=static_cast<const uint8_t*>(SDL_MapGPUTransferBuffer(device,download,false));require(bytes,"surface map");
            for(unsigned i=0;i<hud.count;++i) {
                const float ax=std::clamp((float(i%hud.width)+.5f)*float(f.width)/hud.width-.5f+jitter[0],0.f,float(f.width-1));
                const float ay=std::clamp((float(i/hud.width)+.5f)*float(f.height)/hud.height-.5f+jitter[1],0.f,float(f.height-1));
                const unsigned x0=unsigned(ax),y0=unsigned(ay);const float fx=ax-x0,fy=ay-y0;
                const unsigned base_x=std::min(unsigned((float(i%hud.width)+.5f)*float(f.width)/hud.width),f.width-1);
                const unsigned base_y=std::min(unsigned((float(i/hud.width)+.5f)*float(f.height)/hud.height),f.height-1);
                unsigned donor=base_y*f.width+base_x;float z=std::numeric_limits<float>::max(),best_weight=-1;
                if(((packed[donor]>>8)&255)==0) for(unsigned y=0;y<2;++y) for(unsigned x=0;x<2;++x) {
                    const unsigned j=std::min(y0+y,f.height-1)*f.width+std::min(x0+x,f.width-1);
                    const float weight=(x?fx:1-fx)*(y?fy:1-fy);
                    if(((packed[j]>>8)&255)==0 && weight>0 && (depths[j]<z || (depths[j]==z && weight>best_weight))) {
                        z=depths[j];donor=j;best_weight=weight;
                    }
                }
                uint32_t expected=packed[donor];auto normal=surfaces[donor];
                if(((expected>>16)&255)!=(expected&255)) {expected&=~0x01ff0000u;normal={};}
                const auto protected_tag=(protected_pixels[i]>>8)&255;
                if(protected_tag==1 || (resized && protected_tag==2)) {expected=protected_pixels[i]&~0x01ff0000u;normal={};}
                uint32_t value;std::array<float,4> actual_normal;
                std::memcpy(&value,bytes+i*4,4);std::memcpy(actual_normal.data(),bytes+hud.count*4+i*16,16);
                // GPU reciprocal/FMA rounding can choose either of two truly
                // coplanar, equally weighted donors. Accept only an intact
                // tied donor, never an average of unrelated normals.
                bool tied_normal=false;
                if(protected_tag!=1 && !(resized && protected_tag==2))
                    for(unsigned y=0;y<2;++y) for(unsigned x=0;x<2;++x) {
                        const unsigned j=std::min(y0+y,f.height-1)*f.width+std::min(x0+x,f.width-1);
                        const float weight=(x?fx:1-fx)*(y?fy:1-fy);
                        tied_normal|=depths[j]==z && weight>0 && std::abs(weight-best_weight)<.000002f
                            && packed[j]==value && actual_normal==surfaces[j];
                    }
                if(value!=expected || (actual_normal!=normal && !tied_normal))
                    throw std::runtime_error("temporal surface donor/ownership mismatch: source="+std::to_string(f.width)+"x"+std::to_string(f.height)
                        +" target="+std::to_string(hud.width)+"x"+std::to_string(hud.height)+" pixel="+std::to_string(i)
                        +" jitter="+std::to_string(jitter[0])+","+std::to_string(jitter[1])+" donor="+std::to_string(donor)
                        +" packed="+std::to_string(value)+" expected="+std::to_string(expected));
            }
            SDL_UnmapGPUTransferBuffer(device,download);
            cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"surface invalid command");
            GpuCompositeOutput rejected;
            require(!resolve.enqueue(cb,actual,hud.buffers[2],jitter,rejected) && !rejected.packed,"surface alias accepted");
            require(!resolve.enqueue(cb,source,hud.buffers[2],{std::numeric_limits<float>::quiet_NaN(),0},rejected),"surface NaN accepted");
            SDL_CancelGPUCommandBuffer(cb);
        }
        SDL_ReleaseGPUTransferBuffer(device,download);
    }
    std::cout<<"Temporal surfaces: signed jitter, nearest foreground normals, stale ownership, HUD, resize and invalid inputs passed\n";
}
void check_taa_stationary(SDL_GPUDevice* device) {
    GpuTemporalAa gpu;Fixture f(device,24,16);
    for(float jitter:{-.5f,-.25f,.25f,.5f}) {
        std::vector<uint8_t> pixels(f.count*4);
        std::vector<TemporalAaGuide> guides(f.count,{0,0,100,100,true,true});
        for(unsigned i=0;i<f.count;++i) {
            for(unsigned c=0;c<3;++c) pixels[i*4+c]=uint8_t(32+8*(float(i%24)-jitter));
            pixels[i*4+3]=255;
        }
        GpuTemporalAaSettings s;s.width=24;s.height=16;s.weight=0;s.jitter={jitter,0,0,0};
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"stationary TAA command");
        f.encode_input(cb,pixels,guides);
        auto* result=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,f.buffers[0],f.buffers[1],f.buffers[2],s));
        require(result,gpu.status().c_str());const auto actual=f.finish(cb,result,gpu);
        for(unsigned y=0;y<16;++y) for(unsigned x=1;x<23;++x)
            require(std::abs(int(actual[(y*24+x)*4])-int(32+8*x))<=1,"TAA exposed projection jitter in stationary output");
    }
    std::cout<<"TAA stationary presentation: alternating projection offsets cancel without disabling jitter\n";
}
void check_smaa(SDL_GPUDevice* device) {
    GpuSmaa gpu;GpuTemporalAa unused;
    for(unsigned width:{96U,99U}) {
        Fixture f(device,width,64);
        for(unsigned quality=1;quality<=3;++quality) for(unsigned pattern=0;pattern<3;++pattern) {
            std::vector<uint8_t> pixels(f.count*4);
            std::vector<TemporalAaGuide> guides(f.count);
            for(unsigned i=0;i<f.count;++i) {
                const unsigned x=i%width,y=i/width;
                const unsigned ink=pattern==0?80:((x>y/2+20)?230:10);
                for(unsigned c=0;c<3;++c) pixels[i*4+c]=ink;
                pixels[i*4+3]=uint8_t(i%256);
                guides[i]={0,0,100,100,true,pattern!=2 && y>=4};
            }
            auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"SMAA command");f.encode_input(cb,pixels,guides);
            auto* result=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,f.buffers[2],f.width,f.height,quality));
            require(result,gpu.status().c_str());const auto actual=f.finish(cb,result,unused);
            unsigned changed=0;
            for(unsigned i=0;i<actual.size();++i) {
                if(i%4==3 || !guides[i/4].eligible || pattern==0)
                    require(actual[i]==pixels[i],"SMAA altered HUD, alpha or constant color");
                else if(actual[i]!=pixels[i]) ++changed;
            }
            if(pattern==1) require(changed>30 && changed<f.count,"SMAA failed to resolve localized diagonal edges");
        }
    }
    std::cout<<"SMAA: low/medium/high, diagonal edges, constant colors, protected HUD/alpha, cleared intermediates and resize passed\n";
}
void check_msaa(SDL_GPUDevice* device) {
    GpuMsaa gpu,continuation;GpuTemporalAa unused;
    const std::array<GpuMsaaTriangle,4> triangles{{
        {{0,0},{32,0},{0,24},0,1},{{32,0},{32,24},{0,24},0,1},
        {{3.125f,2.25f},{25.875f,3.0625f},{10.5f,20.5f},2,0xffffffffU},
        {{1,1},{1,1},{1,1},1,0xffffffffU}}};
    std::array<std::uint32_t,256> palette{};
    palette[0]=0xff0000ff;palette[1]=0xffff0000;palette[2]=0xff00ff00;palette[3]=0xff634221;
    const auto unpack=[](std::uint32_t word) {
        return std::array<float,4>{float(word&255)/255,float((word>>8)&255)/255,float((word>>16)&255)/255,float(word>>24)/255};
    };
    for(unsigned width:{32U,37U}) for(unsigned samples:{2U,4U,8U}) for(unsigned layered:{0U,1U,2U}) {
        Fixture f(device,width,24);std::vector<uint8_t> pixels(f.count*4);
        std::vector<TemporalAaGuide> guides(f.count);
        for(unsigned i=0;i<f.count;++i) {pixels[i*4]=33;pixels[i*4+1]=66;pixels[i*4+2]=99;pixels[i*4+3]=255;}
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA command");f.encode_input(cb,pixels,guides);
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span{palette}));
        SDL_GPUTexture* output{};
        if(layered) {
            GpuMsaaSamples previous{};
            GpuMsaaLayer layer{f.buffers[0]};
            if(layered==2) {
                std::vector<uint32_t> background(f.count,0x04000003);
                f.upload_buffer(cb,f.buffers[0],std::as_bytes(std::span{background}));
                output=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,nullptr,nullptr,f.buffers[2],width,24,0,samples,nullptr,true,nullptr,0,false,&layer));
                require(output,gpu.status().c_str());previous=gpu.sample_output();
            }
            for(unsigned i=0;i<triangles.size();++i) {
                auto& current=layered==1 && i%2?continuation:gpu;
                f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{triangles}.subspan(i,1)));
                output=static_cast<SDL_GPUTexture*>(current.enqueue(device,cb,layered==2?nullptr:f.color,f.buffers[1],f.buffers[2],width,24,1,samples,
                    previous.buffer?&previous:nullptr,true,nullptr,0,false,layered==2?&layer:nullptr));
                require(output,current.status().c_str());previous=current.sample_output();
                require(previous.buffer && previous.count==samples,"Missing continuation samples");
            }
        } else {
            f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{triangles}));
            output=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,f.buffers[1],f.buffers[2],width,24,unsigned(triangles.size()),samples));
            require(!gpu.sample_output().buffer,"Stale MSAA sample output");
        }
        require(output,gpu.status().c_str());const auto actual=f.finish(cb,output,unused);
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<width;++x) {
            std::array<std::array<float,4>,8> storage;
            std::fill(storage.begin(),storage.end(),std::array<float,4>{33.f/255,66.f/255,99.f/255,1});
            std::span<std::array<float,4>> colors(storage.data(),samples);
            for(const auto& t:triangles) msaa_paint(colors,msaa_coverage({t.a,t.b,t.c},x,y,samples),[&]{
                auto color=unpack(palette[t.color0]);
                if(t.color1!=0xffffffffU) {const auto other=unpack(palette[t.color1]);for(unsigned c=0;c<4;++c) color[c]=(color[c]+other[c])*.5f;}
                return color;
            });
            const auto expected=msaa_resolve(colors);
            for(unsigned c=0;c<4;++c) require(std::abs(int(actual[(y*width+x)*4+c])-int(std::lround(expected[c]*255)))<=1,"MSAA GPU coverage/color mismatch");
        }
    }
    std::cout<<"MSAA: GPU/CPU 2/4/8 samples, shared-edge ownership, material dither resolve, painter order, split model submissions and resize passed\n";
}
void check_msaa_scene(SDL_GPUDevice* device) {
    GpuScene scene;GpuTemporalAa unused;
    starfox::assets::Shape shape;shape.name="MSAA scene line";
    shape.vertices={{-20,-12,0},{20,12,0}};shape.word_coordinates.resize(2,true);shape.colour_words={0x11};
    starfox::assets::Face face;face.vertex_indices={0,1};shape.faces={face};
    GpuModelDraw model;model.shape=&shape;model.pose.z=128;model.pose.vanish_x=16.125;model.pose.vanish_y=12.25;
    model.pose.continuous_geometry=model.pose.subpixel_projection=true;
    model.pose.palette_override=1;model.settings.focal_length=64;
    std::array<uint32_t,256> palette{};palette[0]=0xff000000;palette[1]=0xff0000ff;palette[2]=0xff00ff00;
    const auto encode=[&](SDL_GPUCommandBuffer* cb,std::span<const GpuSceneDraw> draws,const GpuScene::MsaaSettings* settings) {
        const auto output=scene.enqueue_batch(device,cb,32,24,draws,{},settings);
        require(output.pixels,scene.status().c_str());return output;
    };
    for(unsigned samples:{2U,4U,8U}) {
        std::array<Rgba8,256> colors{};for(unsigned i=0;i<colors.size();++i)
            colors[i]={uint8_t(palette[i]),uint8_t(palette[i]>>8),uint8_t(palette[i]>>16),uint8_t(palette[i]>>24)};
        colors[1].r=uint8_t(256/samples);
        Fixture f(device,32,24);GpuScene::MsaaSettings settings{nullptr,samples,colors};
        std::vector<GpuSceneDraw> draws{model};
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA scene command");
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span{palette}));
        const auto modelOutput=encode(cb,draws,&settings);
        require(scene.msaa_output(),"MSAA scene missing color stream");
        const auto line=f.finish(cb,static_cast<SDL_GPUTexture*>(scene.msaa_output()),unused);
        unsigned partial=0;for(unsigned i=0;i<f.count;++i) if(line[i*4+3]>0 && line[i*4+3]<255) ++partial;
        require(partial>5,"Actual model did not produce multisample edges");
        for(unsigned i=0;i<f.count;++i)
            require(std::abs(int(line[i*4])*255-int(line[i*4+3])*int(colors[1].r))<=255,"MSAA used a stale presentation palette");
        // Exercise the final compositor, not just the isolated AA texture.
        GpuScene blank;require(blank.render_resident(device,32,24,{}),"MSAA compositor empty source");
        const auto blankOutput=blank.resident_output();
        GpuComposite compositor;
        for(unsigned placement=0;placement<3;++placement) for(bool overlay:{false,true}) {
            Framebuffer cpu(32,24),actualFrame(32,24);cpu.clear(2);
            std::vector<uint8_t> coverage(f.count);
            if(overlay) for(unsigned y=0;y<24;++y) for(unsigned x=10;x<20;++x) {cpu.set_stored(x,y,0,PixelLayer::two_d);coverage[y*32+x]=1;}
            GpuCompositeBackground bg;bg.raster=modelOutput;
            require(compositor.compose(placement==0?modelOutput:blankOutput,1,cpu,placement==0?std::span<const uint8_t>(coverage):std::span<const uint8_t>{},
                {},colors,placement==1?&modelOutput:nullptr,placement==2?&bg:nullptr,placement?std::span<const uint8_t>(coverage):std::span<const uint8_t>{}),"MSAA final composition");
            std::vector<uint8_t> composed;require(compositor.readback(actualFrame,composed),"MSAA final readback");
            for(unsigned y=0;y<24;++y) for(unsigned x=0;x<32;++x) {
                const auto i=(y*32+x)*4;const bool covered=overlay && x>=10 && x<20;
                require(std::abs(int(composed[i])-(covered?0:int(line[i])))<=1
                    && std::abs(int(composed[i+1])-(covered?0:255-int(line[i+3])))<=1
                    && composed[i+2]==0 && composed[i+3]==255,"MSAA final edge/background/HUD composition mismatch");
            }
        }
        RasterCommands hud;hud.reset(32,24);RasterCommand ink;ink.left=10;ink.right=20;ink.top=0;ink.bottom=24;ink.tag=1;hud.add(ink);
        RasterCommands layer;layer.reset(8,8);ink={};ink.left=1;ink.right=7;ink.top=1;ink.bottom=7;ink.even=2;layer.add(ink);
        GpuIndexedLayerDraw mapped; mapped.commands=&layer;mapped.reference_size={32,24};mapped.settings.offset_x=23;mapped.settings.offset_y=3;
        mapped.settings.mosaic=0x11;mapped.settings.mosaic_layer_mask=1;
        draws.emplace_back(GpuRasterDraw{&hud});draws.emplace_back(mapped);
        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA layered scene command");
        encode(cb,draws,&settings);
        const auto mixed=f.finish(cb,static_cast<SDL_GPUTexture*>(scene.msaa_output()),unused);
        Framebuffer layerSource(8,8),reference(32,24);replay_raster_commands(layer,layerSource,nullptr);
        composite_transparent_layer(layerSource,reference,mapped.settings);
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<32;++x) {
            const auto i=(y*32+x)*4;
            if(reference.get_stored(x,y)) require(mixed[i]==0 && mixed[i+1]==255 && mixed[i+3]==255,"MSAA indexed overlay mapping mismatch");
            else if(x>=10 && x<20) require(mixed[i]==0 && mixed[i+1]==0 && mixed[i+2]==0 && mixed[i+3]==255,"MSAA lost opaque black HUD");
            else for(unsigned c=0;c<4;++c) require(mixed[i+c]==line[i+c],"MSAA lost retained model samples through overlays");
        }
        GpuScene owned;
        auto pending=settings;pending.defer_palette=true;
        require(owned.render_resident(device,32,24,draws,{},&pending),"MSAA owned recolor scene");
        const auto beforeRecolor=owned.resident_output();
        require(!beforeRecolor.msaa_color,"Provisional MSAA palette exposed to composition");
        colors[1].r/=2;
        require(owned.resolve_msaa_palette(colors),"MSAA late palette resolve");
        require(owned.resident_output().pixels==beforeRecolor.pixels,"MSAA recolor replaced original geometry output");
        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA recolor download command");
        const auto recolored=f.finish(cb,static_cast<SDL_GPUTexture*>(owned.msaa_output()),unused);
        for(unsigned i=0;i<f.count;++i) {
            require(std::abs(int(recolored[i*4])*2-int(mixed[i*4]))<=2,"Late palette did not recolor covered samples");
            for(unsigned c=1;c<4;++c) require(recolored[i*4+c]==mixed[i*4+c],"Late palette resolve changed unrelated color/coverage");
        }
        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA empty scene command");
        encode(cb,{},&settings);
        const auto empty=f.finish(cb,static_cast<SDL_GPUTexture*>(scene.msaa_output()),unused);
        require(std::all_of(empty.begin(),empty.end(),[](auto b){return b==0;}),"MSAA empty scene retained stale samples");
        cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA disabled scene command");
        encode(cb,draws,nullptr);
        require(!scene.msaa_output(),"Disabled MSAA exposed stale scene output");SDL_CancelGPUCommandBuffer(cb);
    }
    std::cout<<"MSAA scene: actual model edges, retained samples, black HUD, indexed mosaic layer, native/late/background final composition, empty/disabled reset passed\n";
}
void check_msaa_lines(SDL_GPUDevice* device) {
    GpuMsaa gpu;GpuTemporalAa unused;
    const std::array<std::array<MsaaPoint,2>,3> lines{{{{{2.25f,5.25f},{25.5f,5.25f}}},{{{2,2},{20,16}}},{{{12.25f,9.25f},{12.25f,9.25f}}}}};
    for(const auto& line:lines) for(bool reverse:{false,true}) for(unsigned thickness:{1U,3U}) for(unsigned samples:{2U,4U,8U}) {
        Fixture f(device,32,24);std::array<std::array<int32_t,4>,129> clipped{};clipped[0]={2,0,1,0};
        for(unsigned v=0;v<2;++v) {auto p=line[reverse?1-v:v];clipped[v+1]={std::bit_cast<int32_t>(p.x),std::bit_cast<int32_t>(p.y),0,0};}
        const std::array<RasterCommand,1> materials{};std::array<uint32_t,256> palette{};palette[0]=0xff0000ff;
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA line command");
        std::vector<uint8_t> pixels(f.count*4);for(unsigned i=0;i<f.count;++i) pixels[i*4+3]=255;
        std::vector<TemporalAaGuide> guides(f.count);f.encode_input(cb,pixels,guides);
        f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{clipped}));f.upload_buffer(cb,f.buffers[0],std::as_bytes(std::span{materials}));f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span{palette}));
        const auto faces=gpu.pack_faces(device,cb,f.buffers[1],f.buffers[0],nullptr,1,1,true,{1,1},nullptr,thickness);
        require(faces.triangles,gpu.status().c_str());
        auto* output=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,faces.triangles,f.buffers[2],32,24,faces.triangle_count,samples,nullptr,false,nullptr,0,true));
        require(output,gpu.status().c_str());const auto actual=f.finish(cb,output,unused);
        const auto a=line[0],b=line[1];const float dx=b.x-a.x,dy=b.y-a.y,length=std::sqrt(dx*dx+dy*dy),half=float(thickness)*.5f;
        const float ux=length?dx/length:1,uy=length?dy/length:0;
        const std::array<MsaaPoint,4> quad{{{a.x-ux*half+uy*half,a.y-uy*half-ux*half},{b.x+ux*half+uy*half,b.y+uy*half-ux*half},{b.x+ux*half-uy*half,b.y+uy*half+ux*half},{a.x-ux*half-uy*half,a.y-uy*half+ux*half}}};
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<32;++x) {
            const auto mask=msaa_coverage({quad[0],quad[1],quad[2]},x,y,samples)|msaa_coverage({quad[0],quad[2],quad[3]},x,y,samples);
            const auto expected=int(std::lround(float(std::popcount(mask))*255/samples));
            require(std::abs(int(actual[(y*32+x)*4])-expected)<=1 && actual[(y*32+x)*4+1]==0 && actual[(y*32+x)*4+3]==255,"MSAA line coverage mismatch");
        }
    }
    std::cout<<"MSAA lines: horizontal/diagonal/zero-length, reversal, thickness and 2/4/8 coverage passed\n";
}
void check_msaa_textures(SDL_GPUDevice* device) {
    GpuMsaa gpu;GpuTemporalAa unused;
    for(unsigned samples:{2U,4U,8U}) for(unsigned scroll:{0U,1U}) for(bool reverse:{false,true}) for(unsigned bytes:{3U,4U}) for(bool pack:{false,true}) {
        Fixture f(device,32,24);
        std::array<GpuMsaaTriangle,2> triangles{};
        triangles[0].a={0,0};triangles[0].b={64,0};triangles[0].c={0,48};triangles[0].color0=4;
        triangles[1]=triangles[0];auto& texture=triangles[1];texture.textured=1;
        texture.uv_a={0,0};texture.uv_b={4,0};texture.uv_c={0,4};
        texture.u_mask=texture.v_mask=1;texture.color_base=32;texture.scroll_x=texture.scroll_y=scroll;
        if(reverse) {std::swap(texture.b,texture.c);std::swap(texture.uv_b,texture.uv_c);}
        std::array<uint32_t,256> palette{};palette[4]=0xff884422;palette[33]=0xff0000ff;palette[34]=0xff00ff00;palette[35]=0xffff0000;
        const std::array<uint8_t,4> texels{0,1,2,3};
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA texture command");
        std::vector<uint8_t> pixels(f.count*4,255);std::vector<TemporalAaGuide> guides(f.count);f.encode_input(cb,pixels,guides);
        f.upload_buffer(cb,f.buffers[0],std::as_bytes(std::span{texels}));
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span{palette}));
        void* triangle_buffer=f.buffers[1];unsigned triangle_count=2;SDL_GPUBuffer* material_buffer{};
        if(pack) {
            std::array<std::array<int32_t,4>,258> clipped{};std::array<RasterCommand,2> materials{};
            for(unsigned i=0;i<2;++i) {
                clipped[i*129][0]=3;
                const std::array<MsaaPoint,3> positions{triangles[i].a,triangles[i].b,triangles[i].c};
                const std::array<MsaaPoint,3> uvs{triangles[i].uv_a,triangles[i].uv_b,triangles[i].uv_c};
                for(unsigned v=0;v<3;++v) clipped[i*129+1+v]={std::bit_cast<int32_t>(positions[v].x),std::bit_cast<int32_t>(positions[v].y),std::bit_cast<int32_t>(uvs[v].x),std::bit_cast<int32_t>(uvs[v].y)};
                materials[i].even=triangles[i].color0;materials[i].textured=triangles[i].textured;
                materials[i].u_mask=materials[i].v_mask=1;materials[i].colour_base=32;
                materials[i].reserved0=scroll;materials[i].reserved1=i?scroll:0;
            }
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizeof(materials),0};
            material_buffer=SDL_CreateGPUBuffer(device,&info);require(material_buffer,"MSAA texture materials");
            f.upload_buffer(cb,material_buffer,std::as_bytes(std::span{materials}));
            f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{clipped}));
            const auto faces=gpu.pack_faces(device,cb,f.buffers[1],material_buffer,nullptr,2,2,true);
            require(faces.triangles,gpu.status().c_str());triangle_buffer=faces.triangles;triangle_count=faces.triangle_count;
        } else f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{triangles}));
        auto* output=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,triangle_buffer,f.buffers[2],32,24,triangle_count,samples,nullptr,false,f.buffers[0],bytes,pack));
        require(output,gpu.status().c_str());const auto actual=f.finish(cb,output,unused);
        if(material_buffer) SDL_ReleaseGPUBuffer(device,material_buffer);
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<32;++x) {
            const auto address=(((y/12+scroll)&1)*2)+((x/16+scroll)&1);
            const auto ink=address<bytes?texels[address]:0;
            const auto expected=palette[ink?32+ink:4];
            for(unsigned c=0;c<4;++c) require(actual[(y*32+x)*4+c]==((expected>>(8*c))&255),"MSAA affine texture/transparent coverage mismatch");
        }
    }
    std::cout<<"MSAA textures: affine UVs, palette base, scroll/wrap, both windings, transparency and bounds passed\n";
}
void check_msaa_repeated_rows(SDL_GPUDevice* device) {
    GpuMsaa gpu;GpuTemporalAa unused;
    const std::array<MsaaRowSpan,4> spans{{{.25f,1.25f},{4.25f,5.25f},{.75f,1.5f},{4.25f,5.25f}}};
    for(unsigned samples:{2U,4U,8U}) {
        Fixture f(device,32,24);std::array<GpuMsaaTriangle,8> triangles{};
        for(unsigned n=0;n<spans.size();++n) {
            auto& a=triangles[n*2];a.a={spans[n].left,1};a.b={spans[n].right,1};a.c={spans[n].right,2};
            auto& b=triangles[n*2+1];b=a;b.b=a.c;b.c={spans[n].left,2};
        }
        std::array<uint32_t,256> palette{};palette[0]=0xff0000ff;
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA repeated-row command");
        std::vector<uint8_t> pixels(f.count*4);for(unsigned p=0;p<f.count;++p) pixels[p*4+3]=255;
        std::vector<TemporalAaGuide> guides(f.count);f.encode_input(cb,pixels,guides);
        f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{triangles}));
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span{palette}));
        auto* output=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,f.buffers[1],f.buffers[2],32,24,8,samples));
        require(output,gpu.status().c_str());const auto actual=f.finish(cb,output,unused);
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<32;++x) {
            const auto coverage=y==1?msaa_row_coverage(spans,x,samples):0;
            const auto expected=int(std::lround(float(std::popcount(coverage))*255/samples));
            require(std::abs(int(actual[(y*32+x)*4])-expected)<=1,"MSAA repeated-row union/holes mismatch");
        }
    }
    std::cout<<"MSAA repeated-row resolve: disjoint gaps, overlapping spans and duplicate coverage passed\n";
}
void check_msaa_sprites(SDL_GPUDevice* device) {
    GpuMsaa gpu;GpuTemporalAa unused;
    for(unsigned samples:{2U,4U,8U}) for(bool fractional:{false,true}) {
        Fixture f(device,32,24);
        std::array<std::array<int32_t,4>,129> clipped{};clipped[0]={1,0,2,0};
        clipped[1]=fractional?std::array<int32_t,4>{std::bit_cast<int32_t>(16.f),std::bit_cast<int32_t>(12.f),std::bit_cast<int32_t>(256.f),0}:std::array<int32_t,4>{16,12,256,0};
        std::array<RasterCommand,1> materials{};materials[0].textured=1;materials[0].u_mask=materials[0].v_mask=3;
        std::array<uint32_t,256> palette{};palette[1]=0xff0000ff;
        std::array<uint8_t,16> texels;texels.fill(1);texels[0]=0;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,16,0};
        auto* texture=SDL_CreateGPUBuffer(device,&info);require(texture,"MSAA sprite texture");
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA sprite command");
        std::vector<uint8_t> pixels(f.count*4);for(unsigned i=0;i<f.count;++i) pixels[i*4+3]=255;
        std::vector<TemporalAaGuide> guides(f.count);f.encode_input(cb,pixels,guides);
        f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{clipped}));
        f.upload_buffer(cb,f.buffers[0],std::as_bytes(std::span{materials}));
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span{palette}));
        f.upload_buffer(cb,texture,std::as_bytes(std::span{texels}));
        const auto faces=gpu.pack_faces(device,cb,f.buffers[1],f.buffers[0],nullptr,1,1,fractional);
        require(faces.triangles,gpu.status().c_str());
        auto* output=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,faces.triangles,f.buffers[2],32,24,faces.triangle_count,samples,nullptr,false,texture,16,true));
        require(output,gpu.status().c_str());const auto actual=f.finish(cb,output,unused);SDL_ReleaseGPUBuffer(device,texture);
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<32;++x) {
            const unsigned expected=x>=14 && x<18 && y>=10 && y<14 && !(x==14 && y==10)?255:0;
            require(actual[(y*32+x)*4]==expected,"MSAA sprite depth/UV/transparency mismatch");
        }
    }
    std::cout<<"MSAA model sprites: depth sizing, bounded UVs and transparency at 2/4/8 samples passed\n";
}
void check_msaa_wireframe(SDL_GPUDevice* device) {
    GpuMsaa gpu;GpuTemporalAa unused;
    for(unsigned samples:{2U,4U,8U}) for(bool reverse:{false,true}) for(unsigned vertices:{4U,64U,128U}) for(unsigned mode:{0U,1U,2U,3U,4U}) {
        if(mode==3 && vertices!=4) continue;
        if(mode==4 && vertices!=4) continue;
        const bool cel=mode==1,wave=mode==2;
        Fixture f(device,32,24);
        std::array<std::array<int32_t,4>,129> clipped{};clipped[0][0]=vertices;
        const std::array<MsaaPoint,4> corners{{{3.25f,2.25f},{27.75f,2.25f},{27.75f,20.75f},{3.25f,20.75f}}};
        std::vector<MsaaPoint> points(vertices);
        for(unsigned v=0;v<vertices;++v) {
            const unsigned edge=v/(vertices/4);const float t=float(v%(vertices/4))/(vertices/4);
            const auto a=corners[edge],b=corners[(edge+1)%4];points[v]={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
        }
        if(mode==3) points={{3.25f,2.25f},{3.25f,10.25f},{3.25f,20.75f},{27.75f,20.75f},{27.75f,2.25f}};
        clipped[0][0]=int(points.size());
        if(reverse) std::reverse(points.begin(),points.end());
        for(unsigned v=0;v<points.size();++v) clipped[v+1]={std::bit_cast<int32_t>(points[v].x),std::bit_cast<int32_t>(points[v].y),0,0};
        std::array<RasterCommand,1> materials{};materials[0].reserved1=mode==4?65536:mode==3?4:wave?131072:cel?1:2;
        materials[0].du=65530;materials[0].dv=7;
        std::array<uint32_t,256> palette{};palette[0]=0xff0000ff;
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA wireframe command");
        std::vector<uint8_t> pixels(f.count*4);for(unsigned i=0;i<f.count;++i) pixels[i*4+3]=255;
        std::vector<TemporalAaGuide> guides(f.count);f.encode_input(cb,pixels,guides);
        f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{clipped}));
        f.upload_buffer(cb,f.buffers[0],std::as_bytes(std::span{materials}));
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span{palette}));
        const auto faces=gpu.pack_faces(device,cb,f.buffers[1],f.buffers[0],nullptr,1,1,true);
        require(faces.triangles,gpu.status().c_str());
        auto* output=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,faces.triangles,f.buffers[2],32,24,faces.triangle_count,samples,nullptr,false,nullptr,0,true));
        require(output,gpu.status().c_str());const auto actual=f.finish(cb,output,unused);
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<32;++x) {
            unsigned covered=0;
            for(unsigned s=0;s<samples;++s) {
                const auto offset=msaa_offsets[samples-2+s];const float px=x+.5f+offset.x;
                const std::array<int,16> sine{0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
                const int phase=int(std::int16_t(std::uint16_t(65530+x)));
                const int shift=wave?sine[unsigned((phase>>1)+7-1)&15]:0;
                const float py=y+.5f+offset.y-shift;
                const bool continuationEdge=mode==3 && !reverse && y>10;
                if(mode==4 ? (y>2 && y<21 && px>=3.25f && px<4.25f && py-1>=2.25f && py-1<20.75f) : mode==3 ? (px>=3.25f && px<27.75f && py>=2.25f && py<20.75f && (!continuationEdge || px<4.25f || px>=26.75f)) : wave ? (px>=3.25f && px<27.75f && py>=2.25f && py<20.75f) : cel ? (px>=4.25f && px<26.75f && py>=2.25f && py<20.75f) :
                    (px>=2.75f && px<28.25f && py>=1.75f && py<21.25f &&
                    !(px>=3.75f && px<27.25f && py>=2.75f && py<20.25f))) ++covered;
            }
            const auto expected=int(std::lround(float(covered)*255/samples));
            require(std::abs(int(actual[(y*32+x)*4])-expected)<=1,"MSAA wireframe/cel/wave boundary coverage mismatch");
        }
    }
    std::cout<<"MSAA wireframe/cel: 2/4/8 boundary coverage, both windings, 4/64/128 vertices, no fan seams passed\n";
}
void check_msaa_pack(SDL_GPUDevice* device) {
    GpuMsaa gpu;GpuTemporalAa unused;
    for(bool fractional:{false,true}) for(unsigned mode=0;mode<7;++mode) for(bool skip_empty:{false,true}) {
        const bool ordered=mode!=0,bsp_mode=mode>=2;
        const unsigned drawn=mode==3 || mode==4 || mode==6?0:mode==5?1:2;
        Fixture f(device,32,24);
        std::array<std::array<std::int32_t,4>,129*4> clipped{};
        std::array<RasterCommand,4> materials{};
        const std::array<std::array<MsaaPoint,4>,2> points{{
            {{{0,0},{16,0},{16,12},{0,12}}},
            {{{2.125f,1.25f},{13.875f,2.0625f},{5.5f,10.5f},{0,0}}}}};
        for(unsigned face=0;face<2;++face) {
            clipped[face*129][0]=face==0?4:3;
            for(unsigned v=0;v<unsigned(clipped[face*129][0]);++v) {
                const auto p=points[face][v];
                clipped[face*129+v+1]={fractional?std::bit_cast<std::int32_t>(p.x):int(p.x),fractional?std::bit_cast<std::int32_t>(p.y):int(p.y),0,0};
            }
        }
        materials[0].even=0;materials[0].odd=1;materials[0].dither=1;
        materials[1].even=2;
        // Unsupported texture must remain explicitly distinguishable from a
        // clipped face. Neither may leave previous invocation's triangles.
        clipped[258][0]=3;materials[2].textured=2;
        clipped[387]={3,1,0,0};
        std::array<uint32_t,256> palette{};palette[0]=0xff0000ff;palette[1]=0xffff0000;palette[2]=0xff00ff00;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,32,0};
        auto* order=SDL_CreateGPUBuffer(device,&info);require(order,"MSAA order allocation");
        auto* results=SDL_CreateGPUBuffer(device,&info);require(results,"MSAA order results allocation");
        const std::array<uint32_t,5> indices=bsp_mode?std::array<uint32_t,5>{99,1,0,2,3}:std::array<uint32_t,5>{1,0,2,3,99};
        const std::array<uint32_t,4> counts{99,99,mode==4?5U:mode==5?1U:mode==6?0U:4U,mode==3?1U:0U};
        auto* cb=SDL_AcquireGPUCommandBuffer(device);require(cb,"MSAA pack command");
        std::vector<uint8_t> pixels(f.count*4,255);std::vector<TemporalAaGuide> guides(f.count);
        f.encode_input(cb,pixels,guides);
        f.upload_buffer(cb,f.buffers[1],std::as_bytes(std::span{clipped}));
        f.upload_buffer(cb,f.buffers[0],std::as_bytes(std::span{materials}));
        f.upload_buffer(cb,f.buffers[2],std::as_bytes(std::span{palette}));
        f.upload_buffer(cb,order,std::as_bytes(std::span{indices}));
        f.upload_buffer(cb,results,std::as_bytes(std::span{counts}));
        const GpuSpanOrder bsp{order,results,1,4,1};
        const auto packed=gpu.pack_faces(device,cb,f.buffers[1],f.buffers[0],mode==1?order:nullptr,4,4,fractional,{2,2},bsp_mode?&bsp:nullptr);
        require(packed.triangles,gpu.status().c_str());
        auto* output=static_cast<SDL_GPUTexture*>(gpu.enqueue(device,cb,f.color,packed.triangles,f.buffers[2],32,24,packed.triangle_count,4,nullptr,false,nullptr,0,skip_empty));
        require(output,gpu.status().c_str());
        SDL_GPUTransferBufferCreateInfo read{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,16,0};
        auto* kinds=SDL_CreateGPUTransferBuffer(device,&read);require(kinds,"MSAA kind readback");
        auto* copy=SDL_BeginGPUCopyPass(cb);require(copy,"MSAA kind copy");
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(packed.kinds),0,16};SDL_GPUTransferBufferLocation to{kinds,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        const auto actual=f.finish(cb,output,unused);
        const auto* values=static_cast<const uint32_t*>(SDL_MapGPUTransferBuffer(device,kinds,false));require(values,"MSAA kinds map");
        const std::array<uint32_t,4> expectedKinds=mode==3 || mode==4?std::array<uint32_t,4>{2,2,2,2}:
            mode==5?std::array<uint32_t,4>{1,0,0,0}:mode==6?std::array<uint32_t,4>{0,0,0,0}:std::array<uint32_t,4>{1,1,2,0};
        require(std::equal(expectedKinds.begin(),expectedKinds.end(),values),"MSAA primitive dispatch classification");
        SDL_UnmapGPUTransferBuffer(device,kinds);SDL_ReleaseGPUTransferBuffer(device,kinds);SDL_ReleaseGPUBuffer(device,order);SDL_ReleaseGPUBuffer(device,results);
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<32;++x) {
            std::array<std::array<float,4>,4> colors;std::fill(colors.begin(),colors.end(),std::array<float,4>{1,1,1,1});
            for(unsigned n=0;n<drawn;++n) {
                const unsigned face=ordered?1-n:n;
                const auto point=[&](unsigned v) {auto p=points[face][v];return MsaaPoint{2*(fractional?p.x:float(int(p.x))),2*(fractional?p.y:float(int(p.y)))};};
                for(unsigned v=1;v<(face==0?3U:2U);++v)
                    msaa_paint(std::span{colors},msaa_coverage({point(0),point(v),point(v+1)},x,y,4),[&]{
                        return face==0?std::array<float,4>{.5f,0,.5f,1}:std::array<float,4>{0,1,0,1};
                    });
            }
            const auto expected=msaa_resolve(std::span{colors});
            for(unsigned c=0;c<4;++c) require(std::abs(int(actual[(y*32+x)*4+c])-int(std::lround(expected[c]*255)))<=1,"MSAA clipped-face coverage mismatch");
        }
    }
    std::cout<<"MSAA clipped packing: fractional/integer vertices, scale, quad fan, painter order, GPU BSP counts/offsets/failures and special dispatch passed\n";
}
}
int main(int argc,char** argv) {
    SDL_GPUDevice* device{};
    try {
        require(SDL_Init(SDL_INIT_VIDEO),"SDL init");
        const bool particle_benchmark=argc>2 && std::string_view(argv[2])=="--benchmark-particles";
        const bool joint_benchmark=argc>2 && std::string_view(argv[2])=="--benchmark-joint";
        const bool benchmark=joint_benchmark || particle_benchmark || (argc>2 && std::string_view(argv[2])=="--benchmark-motion");
        device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_MSL,!benchmark,argc>1?argv[1]:nullptr);
        require(device,"GPU device");
        if(benchmark) {
            benchmark_motion(device,particle_benchmark,joint_benchmark);SDL_DestroyGPUDevice(device);SDL_Quit();return 0;
        }
        check_neural_artwork(device);
        if(argc>2 && std::string_view(argv[2])=="--neural-artwork") {
            SDL_DestroyGPUDevice(device);SDL_Quit();return 0;
        }
        check_temporal_surfaces(device);check_particle_shutter(device);
        check(device);check_taa_stationary(device);check_smaa(device);check_msaa(device);check_msaa_pack(device);check_msaa_textures(device);check_msaa_lines(device);check_msaa_repeated_rows(device);check_msaa_sprites(device);check_msaa_wireframe(device);check_msaa_scene(device);
        std::cout<<SDL_GetGPUDeviceDriver(device)<<": TAA GPU/reference comparison passed: fractional motion, disocclusion, camera Z, unknown motion, HUD, alpha, resize, epoch and cancelled frames\n";
        SDL_DestroyGPUDevice(device);SDL_Quit();return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';if(device) SDL_DestroyGPUDevice(device);SDL_Quit();return 1;}
}
