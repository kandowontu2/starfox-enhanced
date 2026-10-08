#include "starfox/render/sdl_gpu_effects.hpp"
#include "starfox/render/gpu_scalefx.hpp"
#include "starfox/render/gpu_smaa.hpp"
#include "starfox/render/submission_retirement.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "starfox/render/gpu_retirement.hpp"
#include <SDL3/SDL_gpu.h>
#include "starfox/render/sdl_d3d12_bridge.h"
#if __has_include(<vulkan/vulkan.h>)
#define STARFOX_SCENE_VULKAN_TIMESTAMPS 1
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#endif
#include <cstring>
#include <bit>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "shaders/generated/dedither_portable.hpp"
#if defined(STARFOX_ENABLE_XBRZ)
#include "shaders/generated/effects_portable_1.hpp"
#else
#include "shaders/generated/effects_portable_0.hpp"
#endif
#endif
namespace starfox::render {
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {
#include "gpu_scene_timestamps.inc"
void require(bool ok) { if(!ok) throw std::runtime_error(SDL_GetError()); }
struct Parameters {
    Uint32 width,height,scale,stage,hdr,chromatic,smoothing,model,world,model_intensity,world_intensity,aa;
    Uint32 lighting,surface_width,surface_height,reserved;
    Sint32 surface_x,surface_y,min_x,min_y,max_x,max_y,pad0,pad1;
    Uint32 bloom_model,bloom_world,bloom_width,bloom_height,filter,highlight_filter,overlay_filter,pad3;
    Uint32 shadow_width,shadow_height;Sint32 shadow_y;Uint32 shadow_enabled;
    std::array<Uint32,192> window_rows{};
    std::array<Uint32,256> environment_classes{};
    std::array<Uint32,4> environment_modes{};
    std::array<float,4> environment_motion{};
    std::array<float,4> environment_plane{};
    std::array<float,4> backdrop_projection{};
    std::array<std::array<float,4>,2> backdrop_keep{};
    std::array<std::array<float,4>,2> backdrop_palette{};
    std::array<Uint32,16> backdrop_ramp{};
    std::array<float,4> scroll_fraction{};
    std::array<std::array<float,4>,2> ground_gradient{};
};
struct SceneParameters {
    std::array<float,4> scene_camera{};
    std::array<std::array<float,4>,scene_fx_capacity*3> scene_data{};
    std::array<Uint32,4> depth_modes{};
    std::array<float,4> depth_camera{};
};
static_assert(sizeof(Parameters)==2176);
static_assert(sizeof(SceneParameters)==2352);
static_assert(sizeof(Parameters)<=4096 && sizeof(SceneParameters)<=4096);
static_assert(sizeof(SurfaceSample)==20 && offsetof(SurfaceSample,valid)==17);
}
struct SdlGpuEffects::Impl {
    SceneParameters scene_parameters{};
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUComputePipeline* dedither_pipeline{};
    SDL_GPUCommandBuffer* command{};SDL_GPUFence* fence{};
    SceneGpuTimestamps gpu_timestamps{"STARFOX_TRACE_EFFECTS_GPU_TIMESTAMPS","effects"};
    int timestamp_ticket{-1};
    std::vector<SDL_GPUFence*> retired_fences;
    bool ordered_queue_reuse{},reported_ordered_reuse{};
    bool environment_reflection_default{};
    bool environment_reflection_reported{};
    bool borrow_resident_default{};
    int reported_copy_route{-1};
    SDL_GPUTransferBuffer *upload{},*download{};Uint32 upload_size{},download_size{};
    Uint32 last_staging_upload{};
    SDL_GPUBuffer* buffers[5]{};Uint32 buffer_sizes[5]{};
    SDL_GPUBuffer* input_buffers[6]{};
    // Each filtered layer is consumed before the next ordered filter enqueue.
    GpuScaleFx scalefx;
    GpuSmaa smaa;
    GpuMotionBlur motion_blur;
    std::vector<Uint8> overlay_payload;
    SDL_GPUBuffer* backdrop_buffer{};
    Uint32 backdrop_capacity{};
    BackdropUploadCache backdrop_pixels;
    SDL_GPUTexture *images[2]{},*snapshots[2]{},*bloom[4]{},*native{},*side{},*dummy_read{},*dummy_write[4]{};
    SDL_GPUTexture* capture{};
    // Borrow an owned ping-pong image until the next apply. Only split-layer
    // extraction needs a separate snapshot, because it can overwrite both.
    SDL_GPUTexture* capture_source{};
    struct History {
        SDL_GPUTexture* textures[2]{};
        unsigned index{},key{};
        Uint32 width{},height{},scale{};
        std::uint64_t epoch{};
        double time{};
        bool valid{};
    };
    std::array<History,6> histories{}; // Trails then independent phosphor, mono/L/R each.
    std::array<History,3> exposures{};
    SDL_GPUTexture* exposure_histogram{};
    bool capture_pending{};
    bool overlay_debug_captured{};
    Uint32 width{},height{},scale{};std::string status{"SDL GPU effects not initialized"};
    bool failed{};
    ~Impl() {
        if(!device) return;
        if(command) {SDL_CancelGPUCommandBuffer(command);gpu_timestamps.cancel(timestamp_ticket);}
        if(fence) {wait_gpu_retirement(device,true,&fence,1);gpu_timestamps.retired(fence);SDL_ReleaseGPUFence(device,fence);}
        for(auto* pending:retired_fences) {wait_gpu_retirement(device,true,&pending,1);gpu_timestamps.retired(pending);SDL_ReleaseGPUFence(device,pending);}
        gpu_timestamps.release();
        release_textures();
        for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(backdrop_buffer) SDL_ReleaseGPUBuffer(device,backdrop_buffer);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(dedither_pipeline) SDL_ReleaseGPUComputePipeline(device,dedither_pipeline);
    }
    void retire(std::size_t limit) {
        require(retire_submission_queue(retired_fences,limit,
            [&](auto* previous){return SDL_QueryGPUFence(device,previous);},
            [&](auto* previous){return SDL_WaitForGPUFences(device,true,&previous,1);},
            [&](auto* previous){gpu_timestamps.retired(previous);SDL_ReleaseGPUFence(device,previous);}));
    }
    void finish() {
        if(fence) {require(SDL_WaitForGPUFences(device,true,&fence,1));gpu_timestamps.retired(fence);SDL_ReleaseGPUFence(device,fence);fence=nullptr;}
        retire(0);
    }
    void release_textures(bool release_history=true) {
        const auto release=[&](SDL_GPUTexture*& t){if(t) SDL_ReleaseGPUTexture(device,t);t=nullptr;};
        for(auto& t:images) release(t);
        for(auto& t:snapshots) release(t);
        for(auto& t:bloom) release(t);
        if(release_history) for(auto& history:histories) {for(auto& t:history.textures)release(t);history.valid=false;}
        if(release_history) for(auto& state:exposures) {for(auto& t:state.textures)release(t);state.valid=false;}
        release(exposure_histogram);
        for(auto& t:dummy_write) release(t);
        release(native);release(side);release(dummy_read);release(capture);
        capture_pending=false;capture_source=nullptr;
    }
    SDL_GPUTexture* texture(Uint32 w,Uint32 h,bool floating=false) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
        info.format=floating?SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width=w;info.height=h;info.layer_count_or_depth=1;info.num_levels=1;
        auto* t=SDL_CreateGPUTexture(device,&info);require(t);return t;
    }
    void initialize(SDL_GPUDevice* source) {
        device=source;SDL_GPUComputePipelineCreateInfo info{};
        // Clean paired D3D12 bloom measurements were consistently slower when
        // sampling the compositor directly. Keep the resident GPU copy there;
        // this never downloads the scene or switches to a CPU effects path.
        borrow_resident_default=std::string_view(SDL_GetGPUDeviceDriver(device))!="direct3d12";
        const auto formats=SDL_GetGPUShaderFormats(device);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=portable_shader::spirv;
            info.code_size=sizeof(portable_shader::spirv);info.entrypoint="main";
        }
#if defined(__APPLE__)
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(portable_shader::metal);
            info.code_size=sizeof(portable_shader::metal)-1;info.entrypoint="main0";
        }
#endif
#if defined(_WIN32)
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=portable_shader::dxil;
            info.code_size=sizeof(portable_shader::dxil);info.entrypoint="main";
        }
#endif
        else throw std::runtime_error("SDL GPU effects require a supported native shader format");
        info.num_readonly_storage_textures=6;info.num_readonly_storage_buffers=6;
        info.num_readwrite_storage_textures=4;info.num_uniform_buffers=2;
        info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        pipeline=create_gpu_compute_pipeline(device,&info);require(pipeline);
        const char* driver=SDL_GetGPUDeviceDriver(device);
        environment_reflection_default=driver && std::strcmp(driver,"direct3d12")==0;
        status=std::string("SDL GPU compute: ")+driver;
    }
    void resize(const Framebuffer& frame) {
        if(width==frame.stored_width() && height==frame.stored_height() && scale==frame.draw_scale()) return;
        // Overlay/filter work can use a different extent between temporal
        // frames. History owns its own dimensions and is checked by apply().
        release_textures(false);width=height=scale=0;
        const auto w=frame.stored_width(),h=frame.stored_height(),s=frame.draw_scale();
        for(auto& t:images) t=texture(w,h);
        dummy_read=texture(1,1);
        for(unsigned i=0;i<4;++i) dummy_write[i]=texture(1,1,i==1);
        width=w;height=h;scale=s;
    }
    void ensure_optional_textures(const GpuEffectSettings& settings,bool overlays) {
        if((settings.filter || overlays) && !native) native=texture(width/scale,height/scale);
        if((settings.presentation_model_texture || settings.presentation_glow_texture
            || (overlays && settings.filter)) && !side) side=texture(width,height);
        if(settings.bloom_model || settings.bloom_world) {
            for(auto& t:snapshots) if(!t) t=texture(width,height);
            for(auto& t:bloom) if(!t) t=texture((width+2*scale-1)/(2*scale),(height+2*scale-1)/(2*scale),true);
        }
    }
    std::uint64_t texture_payload_bytes() const noexcept {
        std::uint64_t total=0;
        const auto full=std::uint64_t(width)*height*4;
        for(auto* t:images) if(t) total+=full;
        for(auto* t:snapshots) if(t) total+=full;
        for(const auto& history:histories) for(auto* t:history.textures) if(t)
            total+=std::uint64_t(history.width)*history.height*16;
        for(const auto& state:exposures) for(auto* t:state.textures) if(t) total+=16;
        if(exposure_histogram) total+=std::uint64_t(64)*((width+31)/32)*((height+31)/32)*16;
        if(side) total+=full;
        if(capture) total+=full;
        if(native && scale) total+=std::uint64_t(width/scale)*(height/scale)*4;
        if(scale) for(auto* t:bloom) if(t)
            total+=std::uint64_t((width+2*scale-1)/(2*scale))*((height+2*scale-1)/(2*scale))*16;
        if(dummy_read) total+=4;
        for(unsigned i=0;i<4;++i) if(dummy_write[i]) total+=i==1?16:4;
        return total;
    }
    void transfer(SDL_GPUTransferBuffer*& b,Uint32& capacity,Uint32 size,SDL_GPUTransferBufferUsage usage) {
        if(b && capacity>=size) return;
        if(b) SDL_ReleaseGPUTransferBuffer(device,b);
        b=nullptr;SDL_GPUTransferBufferCreateInfo info{usage,size,0};
        b=SDL_CreateGPUTransferBuffer(device,&info);require(b);capacity=size;
    }
    void copy(SDL_GPUTexture* src,SDL_GPUTexture* dst) {
        auto* pass=SDL_BeginGPUCopyPass(command);require(pass);
        SDL_GPUTextureLocation a{src,0,0,0,0,0},b{dst,0,0,0,0,0};
        SDL_CopyGPUTextureToTexture(pass,&a,&b,width,height,1,false);SDL_EndGPUCopyPass(pass);
    }
    void dispatch(Parameters& p,Uint32 stage,SDL_GPUTexture* input,SDL_GPUTexture* output,
        SDL_GPUTexture* bright=nullptr,SDL_GPUTexture* core=nullptr,SDL_GPUTexture* bright_output=nullptr,
        SDL_GPUTexture* native_output=nullptr,SDL_GPUTexture* side_output=nullptr) {
        p.stage=stage;SDL_PushGPUComputeUniformData(command,0,&p,sizeof(p));
        SDL_PushGPUComputeUniformData(command,1,&scene_parameters,sizeof(scene_parameters));
        SDL_GPUStorageTextureReadWriteBinding targets[4]{};
        SDL_GPUTexture* out[]{output,bright_output,native_output,side_output};
        for(unsigned i=0;i<4;++i) targets[i].texture=out[i]?out[i]:dummy_write[i];
        auto* pass=SDL_BeginGPUComputePass(command,targets,4,nullptr,0);require(pass);
        SDL_BindGPUComputePipeline(pass,pipeline);
        SDL_GPUTexture* inputs[]{input,bright?bright:dummy_read,core?core:dummy_read,
            native?native:dummy_read,snapshots[0]?snapshots[0]:dummy_read,snapshots[1]?snapshots[1]:dummy_read};
        for(auto& t:inputs) for(auto* written:out) if(written && t==written) t=dummy_read;
        SDL_BindGPUComputeStorageTextures(pass,0,inputs,6);SDL_BindGPUComputeStorageBuffers(pass,0,input_buffers,6);
        const auto w=stage>=7 && stage<=11?p.bloom_width:(stage==13 || stage==28)?width/scale:width;
        const auto h=stage>=7 && stage<=11?p.bloom_height:(stage==13 || stage==28)?height/scale:height;
        SDL_DispatchGPUCompute(pass,stage==36?(width+31)/32:stage==37?1:(w+7)/8,
            stage==36?(height+31)/32:stage==37?1:(h+7)/8,1);SDL_EndGPUComputePass(pass);
    }
    void dedither(SDL_GPUTexture* input,SDL_GPUTexture* output,bool packed_tags) {
        if(!dedither_pipeline) {
            SDL_GPUComputePipelineCreateInfo p{};
            const auto formats=SDL_GetGPUShaderFormats(device);
            if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
                p.format=SDL_GPU_SHADERFORMAT_SPIRV;p.code=dedither_shader::spirv;
                p.code_size=sizeof(dedither_shader::spirv);p.entrypoint="main";
            }
#if defined(__APPLE__)
            else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
                p.format=SDL_GPU_SHADERFORMAT_MSL;p.code=reinterpret_cast<const Uint8*>(dedither_shader::metal);
                p.code_size=sizeof(dedither_shader::metal)-1;p.entrypoint="main0";
            }
#endif
#if defined(_WIN32)
            else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
                p.format=SDL_GPU_SHADERFORMAT_DXIL;p.code=dedither_shader::dxil;
                p.code_size=sizeof(dedither_shader::dxil);p.entrypoint="main";
            }
#endif
            else throw std::runtime_error("Model dedither requires native compute shaders");
            p.num_readonly_storage_textures=1;p.num_readonly_storage_buffers=1;
            p.num_readwrite_storage_textures=1;p.num_uniform_buffers=1;
            p.threadcount_x=p.threadcount_y=8;p.threadcount_z=1;
            dedither_pipeline=create_gpu_compute_pipeline(device,&p);require(dedither_pipeline);
            if(std::getenv("STARFOX_TRACE_GPU")) SDL_Log("effects-dedither: specialized native pass");
        }
        const std::array<Uint32,4> constants{width,height,scale,packed_tags?1U:0U};
        SDL_PushGPUComputeUniformData(command,0,constants.data(),sizeof(constants));
        SDL_GPUStorageTextureReadWriteBinding target{};target.texture=output;
        auto* pass=SDL_BeginGPUComputePass(command,&target,1,nullptr,0);require(pass);
        SDL_BindGPUComputePipeline(pass,dedither_pipeline);
        SDL_BindGPUComputeStorageTextures(pass,0,&input,1);
        SDL_BindGPUComputeStorageBuffers(pass,0,input_buffers,1);
        SDL_DispatchGPUCompute(pass,(width+7)/8,(height+7)/8,1);SDL_EndGPUComputePass(pass);
    }
    void download_texture(SDL_GPUTexture* t,Uint32 offset) {
        auto* pass=SDL_BeginGPUCopyPass(command);require(pass);
        SDL_GPUTextureRegion region{t,0,0,0,0,0,width,height,1};
        SDL_GPUTextureTransferInfo target{download,offset,0,0};
        SDL_DownloadFromGPUTexture(pass,&region,&target);SDL_EndGPUCopyPass(pass);
    }
    bool read(std::vector<std::uint8_t>& rgba,Uint32 offset=0) {
        finish();
        if(capture_pending) {
            // Presentation normally never needs CPU pixels. Keep the composed
            // frame on-device until a screenshot/history consumer asks for it.
            transfer(download,download_size,width*height*4,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
            command=SDL_AcquireGPUCommandBuffer(device);require(command);
            download_texture(capture_source,0);
            fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence);
            finish();capture_pending=false;
        }
        auto* bytes=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,download,false));
        require(bytes);rgba.resize(std::size_t(width)*height*4);
        std::memcpy(rgba.data(),bytes+offset,rgba.size());SDL_UnmapGPUTransferBuffer(device,download);return true;
    }
    void apply(const Framebuffer& frame,std::vector<std::uint8_t>& rgba,const GpuEffectSettings& s,
        const GpuCompositeOutput* resident=nullptr) {
        const bool profile=std::getenv("STARFOX_TRACE_GPU_PASS_COST_ALL")!=nullptr;
        const auto begin=profile?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        if(s.motion_blur && !resident) throw std::runtime_error("Motion blur requires resident depth and velocity");
        if(!s.motion_blur) motion_blur.release_device();
        if(s.particle_shutter && (!s.motion_blur || !s.scene_fx.surface_lighting_only()
            || (!s.shadow_before_style && (!s.shadow_mask.empty() || s.resident_shadow.buffer))))
            throw std::runtime_error("Particle exposure requires surface-only lighting and pre-style shadows");
        if(s.background_subtract && s.background_subtract_protect_models && !resident)
            throw std::runtime_error("Background fade requires resident foreground coverage");
        if(s.setup_overlay && (!s.setup_overlay->frame || s.setup_overlay->brightness>15
            || s.setup_overlay->frame->draw_scale()!=1 || s.setup_overlay->frame->width()>8192
            || s.setup_overlay->frame->height()>8192))
            throw std::runtime_error("Invalid setup overlay");
        bool has_overlays=false;
        for(const auto& overlay:s.subtractive_overlays) if(overlay) {
            has_overlays=true;
            if(!overlay->frame || overlay->frame->draw_scale()!=1
                || overlay->frame->width()!=frame.width() || overlay->frame->height()!=frame.height()
                || s.overlay_palette.empty() || s.overlay_palette.size()>256)
                throw std::runtime_error("Invalid subtractive overlay");
            if(overlay->resident.pixels && (overlay->resident.device!=device
                || overlay->resident.width!=frame.width() || overlay->resident.height!=frame.height()))
                throw std::runtime_error("Invalid resident subtractive overlay");
        }
        for(const auto* overlay:{&s.host_overlay,&s.confirmation_overlay})
            if(*overlay && ((*overlay)->width>6144 || (*overlay)->height>6144
                || std::uint64_t((*overlay)->width)*(*overlay)->height>6144))
                throw std::runtime_error("Host overlay exceeds packed glyph capacity");
        if(s.circle) {
            const auto& c=*s.circle;
            const auto absolute=[](std::int32_t v) {return v<0?-std::int64_t(v):std::int64_t(v);};
            if(c.radius<0 || c.radius>GpuEffectSettings::Circle::exact_limit
                || absolute(c.x)+frame.stored_width()>GpuEffectSettings::Circle::exact_limit
                || absolute(c.y)+frame.stored_height()>GpuEffectSettings::Circle::exact_limit
                || c.red>31 || c.green>31 || c.blue>31)
                throw std::runtime_error("GPU circle outside exact integer range");
        }
        if(ordered_queue_reuse) {
            if(fence) {retired_fences.push_back(fence);fence=nullptr;}
            retire(2);
        } else finish();
        const auto waited=profile?std::chrono::steady_clock::now():begin;
        resize(frame);capture_pending=false;capture_source=nullptr;
        auto& history_state=histories[std::min(s.persistence_slot,2U)];
        auto& history=history_state.textures;
        auto& history_valid=history_state.valid;
        auto& history_index=history_state.index;
        auto& history_key=history_state.key;
        auto& history_epoch=history_state.epoch;
        auto& history_time=history_state.time;
        const bool persistence=s.persistence_mode>0 && s.persistence_mode<=2
            && (s.persistence_models || s.persistence_world) && s.persistence_intensity
            && std::isfinite(s.presentation_seconds);
        if(!persistence && !s.preserve_persistence) {
            for(unsigned slot=0;slot<3;++slot) {
                auto& state=histories[slot];
                for(auto& t:state.textures) {if(t)SDL_ReleaseGPUTexture(device,t);t=nullptr;}
                state.valid=false;
            }
        } else if(persistence) {
            if(history_state.width!=width || history_state.height!=height || history_state.scale!=scale) {
                for(auto& t:history) {if(t)SDL_ReleaseGPUTexture(device,t);t=nullptr;}
                history_valid=false;
            }
            history_state.width=width;history_state.height=height;history_state.scale=scale;
            for(auto& t:history) if(!t)t=texture(width,height,true);
        }
        ensure_optional_textures(s,has_overlays);
        auto& phosphor=histories[3+std::min(s.persistence_slot,2U)];
        const bool use_phosphor=s.phosphor>0 && s.phosphor<=3 && std::isfinite(s.presentation_seconds);
        if(!use_phosphor && !s.preserve_phosphor) {
            for(unsigned slot=3;slot<6;++slot) {
                auto& state=histories[slot];
                for(auto& t:state.textures) {if(t)SDL_ReleaseGPUTexture(device,t);t=nullptr;}
                state.valid=false;
            }
        } else if(use_phosphor) {
            if(phosphor.width!=width || phosphor.height!=height || phosphor.scale!=scale) {
                for(auto& t:phosphor.textures) {if(t)SDL_ReleaseGPUTexture(device,t);t=nullptr;}
                phosphor.valid=false;
            }
            phosphor.width=width;phosphor.height=height;phosphor.scale=scale;
            for(auto& t:phosphor.textures) if(!t)t=texture(width,height,true);
        }
        auto& exposure=exposures[std::min(s.persistence_slot,2U)];
        const bool use_exposure=s.exposure>0 && s.exposure<=3 && std::isfinite(s.presentation_seconds);
        if(!use_exposure && !s.preserve_exposure) {
            for(auto& state:exposures) {
                for(auto& t:state.textures) {if(t)SDL_ReleaseGPUTexture(device,t);t=nullptr;}
                state.valid=false;
            }
            if(exposure_histogram)SDL_ReleaseGPUTexture(device,exposure_histogram);
            exposure_histogram=nullptr;
        } else if(use_exposure) {
            if(exposure.width!=width || exposure.height!=height || exposure.scale!=scale) exposure.valid=false;
            exposure.width=width;exposure.height=height;exposure.scale=scale;
            for(auto& t:exposure.textures) if(!t)t=texture(1,1,true);
            if(!exposure_histogram)exposure_histogram=texture(64,((width+31)/32)*((height+31)/32),true);
        }
        const Uint32 bytes=width*height*4;
        struct OverlayDebug {
            SDL_GPUDevice* device{};
            std::array<SDL_GPUTexture*,4> images{};
            SDL_GPUTransferBuffer* download{};
            ~OverlayDebug() {
                for(auto* t:images) if(t) SDL_ReleaseGPUTexture(device,t);
                if(download) SDL_ReleaseGPUTransferBuffer(device,download);
            }
        };
        std::unique_ptr<OverlayDebug> debug;
        const char* debug_path=SDL_getenv("STARFOX_TEST_OVERLAY_DEBUG");
        if(debug_path && !overlay_debug_captured && s.filter==5 && s.subtractive_overlays[0]
            && s.subtractive_overlays[1] && s.subtractive_overlays[0]->brightness==30) {
            debug=std::make_unique<OverlayDebug>();debug->device=device;
            for(auto& t:debug->images) t=texture(width,height);
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes*4,0};
            debug->download=SDL_CreateGPUTransferBuffer(device,&info);require(debug->download);
        }
        Parameters p{width,height,scale,0,s.hdr,s.chromatic,s.smoothing,s.model_effect,s.world_effect,
            s.model_intensity,s.world_intensity,s.anti_aliasing,0,0,0,0,0,0,0,0,0,0,0,0,
            s.bloom_model,s.bloom_world,(width+2*scale-1)/(2*scale),(height+2*scale-1)/(2*scale),
            s.filter,s.highlight_filter,s.overlay_filter?1U:0U,0,s.shadow_width,s.shadow_height,s.shadow_offset_y,0};
        scene_parameters.scene_camera=s.scene_fx.camera;scene_parameters.scene_data=s.scene_fx.data;
        scene_parameters.depth_modes[0]=s.depth_fx.modes;scene_parameters.depth_camera=s.depth_fx.camera;
        auto* present=static_cast<SDL_GPUTexture*>(s.presentation_texture);
        auto* glow=static_cast<SDL_GPUTexture*>(s.presentation_glow_texture);
        auto* model=static_cast<SDL_GPUTexture*>(s.presentation_model_texture);
        if((glow || model) && !present) throw std::runtime_error("GPU layer output requires base output");
        if(model && !resident && (!s.surfaces || s.surfaces->empty())) throw std::runtime_error("Missing model surfaces");
        if(glow && !(s.bloom_model || s.bloom_world)) throw std::runtime_error("Missing bloom settings");
        std::span<const Uint8> data[5]{frame.layer_tags(),frame.pixels(),{},s.shadow_mask,
            s.setup_overlay?s.setup_overlay->frame->pixels():std::span<const Uint8>{}};
        Uint32 palette_offset{},overlay_offsets[2]{},backdrop_upload_offset{},backdrop_upload_size{};
        const BackdropImage* backdrop=s.environment.modes[2]?s.environment.backdrop:nullptr;
        if(has_overlays) {
            // Share the auxiliary upload with setup ink and the source palette.
            // Resident overlays bind packed GPU indices directly; only legacy
            // CPU overlays append index bytes. Neither needs CPU RGBA expansion.
            overlay_payload.assign(data[4].begin(),data[4].end());
            overlay_payload.resize((overlay_payload.size()+3)&~std::size_t(3),0);
            palette_offset=Uint32(overlay_payload.size());
            for(const auto& c:s.overlay_palette)
                overlay_payload.insert(overlay_payload.end(),{c.r,c.g,c.b,c.a});
            for(unsigned i=0;i<2;++i) if(s.subtractive_overlays[i] && !s.subtractive_overlays[i]->resident.pixels) {
                overlay_payload.resize((overlay_payload.size()+3)&~std::size_t(3),0);
                overlay_offsets[i]=Uint32(overlay_payload.size());
                const auto& pixels=s.subtractive_overlays[i]->frame->pixels();
                overlay_payload.insert(overlay_payload.end(),pixels.begin(),pixels.end());
            }
            data[4]=overlay_payload;
        }
        if(backdrop) {
            const auto& sky=*backdrop;
            if(!sky.width || !sky.height || sky.width>8192 || sky.height>8192 || sky.pixels.size()!=std::size_t(sky.width)*sky.height)
                throw std::runtime_error("Invalid enhanced backdrop");
            // Sealed assets use stable identities; mutable callers compare
            // contents, never addresses. Scroll/style never require reupload.
            if(!backdrop_buffer || !backdrop_pixels.matches(sky))
                backdrop_upload_size=Uint32(sky.pixels.size()*4);
            if(backdrop_upload_size>backdrop_capacity) {
                if(backdrop_buffer) SDL_ReleaseGPUBuffer(device,backdrop_buffer);
                backdrop_buffer=nullptr;
                SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,backdrop_upload_size,0};
                backdrop_buffer=SDL_CreateGPUBuffer(device,&info);require(backdrop_buffer);
                backdrop_capacity=backdrop_upload_size;
            }
        }
        if((s.lighting || model || s.resident_reflection.buffer || s.scene_fx.active() || s.depth_fx.active()) && s.surfaces && !s.surfaces->empty()) {
            const auto& surface=*s.surfaces;p.lighting=s.lighting;p.surface_width=surface.width();p.surface_height=surface.height();
            p.surface_x=s.surface_x;p.surface_y=s.surface_y;
            p.min_x=std::max(1,p.surface_x+int(surface.minimum_x()));p.min_y=std::max(1,p.surface_y+int(surface.minimum_y()));
            p.max_x=std::min(int(width)-1,p.surface_x+int(surface.maximum_x()));p.max_y=std::min(int(height)-1,p.surface_y+int(surface.maximum_y()));
            data[2]={reinterpret_cast<const Uint8*>(surface.samples().data()),surface.samples().size_bytes()};
        }
        if(resident) {
            if(resident->device!=device || resident->width!=width || resident->height!=height
                || !resident->rgba || !resident->packed || !resident->surfaces)
                throw std::runtime_error("Incompatible GPU composition input");
            data[0]={};data[1]={};data[2]={};p.reserved=1;
            p.lighting=s.lighting;p.surface_width=width;p.surface_height=height;p.surface_x=p.surface_y=0;
            p.min_x=p.min_y=1;p.max_x=int(width)-1;p.max_y=int(height)-1;
        }
        if(s.volumetric.buffer && (s.volumetric.device!=device || s.volumetric.width!=width || s.volumetric.height!=height))
            throw std::runtime_error("Incompatible resident volumetric input");
        if(!s.shadow_mask.empty()) {
            if(!s.shadow_width || !s.shadow_height || s.shadow_mask.size()!=std::size_t(s.shadow_width)*s.shadow_height)
                throw std::runtime_error("Invalid shadow dimensions");
            p.shadow_enabled=1;
        }
        if(s.resident_shadow.buffer) {
            if(s.resident_shadow.device!=device || s.resident_shadow.width!=s.shadow_width
                || s.resident_shadow.height!=s.shadow_height || !s.shadow_width || !s.shadow_height)
                throw std::runtime_error("Incompatible resident shadow input");
            if(s.resident_shadow.packed_row_bytes
                && s.resident_shadow.packed_row_bytes!=((s.shadow_width+3U)&~3U))
                throw std::runtime_error("Invalid packed resident shadow stride");
            p.shadow_enabled=s.resident_shadow.packed_row_bytes?3:2;data[3]={};
        }
        if(s.resident_reflection.buffer) {
            const auto& r=s.resident_reflection;
            if(r.device!=device || !r.width || !r.height || r.width>width
                || r.row_bytes!=std::uint64_t(r.width)*4
                || (!resident && !s.environment.ray_water && (!s.surfaces || s.surfaces->empty())))
                throw std::runtime_error("Incompatible resident reflection input");
        }
        Uint32 offsets[5]{},sizes[5]{},total=resident?0U:(bytes+255)&~255U;
        for(unsigned i=0;i<5;++i) {
            // Resident composition already supplies these bindings. Do not
            // allocate, map or upload placeholder buffers that are never read.
            if((resident && i<3) || (i==3 && s.resident_shadow.buffer)) continue;
            if(data[i].empty()) continue;
            sizes[i]=std::max(4U,(Uint32(data[i].size())+3)&~3U);offsets[i]=total;total+=(sizes[i]+255)&~255U;
            if(!buffers[i] || buffer_sizes[i]<sizes[i]) {
                if(buffers[i]) SDL_ReleaseGPUBuffer(device,buffers[i]);
                buffers[i]=nullptr;
                SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
                buffers[i]=SDL_CreateGPUBuffer(device,&info);require(buffers[i]);buffer_sizes[i]=sizes[i];
            }
        }
        if(backdrop_upload_size) {
            backdrop_upload_offset=total;
            total+=(backdrop_upload_size+255)&~255U;
        }
        last_staging_upload=total;
        if(total) transfer(upload,upload_size,total,SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
        if(!present) transfer(download,download_size,bytes*3,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        auto* mapped=total?static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true)):nullptr;
        if(total) require(mapped);
        if(!resident) std::memcpy(mapped,rgba.data(),bytes);
        for(unsigned i=0;i<5;++i) {
            if(!sizes[i]) continue;
            if(!data[i].empty()) std::memcpy(mapped+offsets[i],data[i].data(),data[i].size());
            // Only DWORD padding is uploaded; alignment gaps are never read.
            std::memset(mapped+offsets[i]+data[i].size(),0,sizes[i]-data[i].size());
        }
        if(backdrop_upload_size)
            std::memcpy(mapped+backdrop_upload_offset,backdrop->pixels.data(),backdrop_upload_size);
        if(total) SDL_UnmapGPUTransferBuffer(device,upload);
        const auto prepared=profile?std::chrono::steady_clock::now():begin;
        command=SDL_AcquireGPUCommandBuffer(device);require(command);
        timestamp_ticket=gpu_timestamps.begin(device,command,0);
        const bool borrow_resident=resident && !SDL_getenv("STARFOX_TEST_COPY_EFFECTS_INPUT")
            && (borrow_resident_default || SDL_getenv("STARFOX_TEST_BORROW_EFFECTS_INPUT"));
        const bool separate_capture=glow || model || SDL_getenv("STARFOX_TEST_SEPARATE_EFFECTS_CAPTURE");
        if(SDL_getenv("STARFOX_TEST_EFFECTS_COPY_RESULT")) {
            const int route=(resident?1:0)|(borrow_resident?2:0)|(present?4:0)
                |(separate_capture?8:0)|((glow || model)?16:0);
            if(route!=reported_copy_route) {
                std::cerr<<"effects-copy-route: resident="<<(resident!=nullptr)
                    <<" input="<<(borrow_resident?"borrowed":"copied")
                    <<" capture="<<(!present?"none":separate_capture?"separate":"owned")
                    <<" split="<<(glow || model)<<'\n';
                reported_copy_route=route;
            }
        }
        auto* pass=SDL_BeginGPUCopyPass(command);require(pass);
        SDL_GPUTextureTransferInfo source{upload,0,0,0};SDL_GPUTextureRegion target{images[0],0,0,0,0,0,width,height,1};
        if(!resident) SDL_UploadToGPUTexture(pass,&source,&target,false);
        else if(!borrow_resident) {
            SDL_GPUTextureLocation a{static_cast<SDL_GPUTexture*>(resident->rgba),0,0,0,0,0},b{images[0],0,0,0,0,0};
            SDL_CopyGPUTextureToTexture(pass,&a,&b,width,height,1,false);
        }
        std::fill(std::begin(input_buffers),std::end(input_buffers),nullptr);
        for(unsigned i=0;i<5;++i) {
            if(!sizes[i]) continue;
            SDL_GPUTransferBufferLocation a{upload,offsets[i]};SDL_GPUBufferRegion b{buffers[i],0,sizes[i]};
            SDL_UploadToGPUBuffer(pass,&a,&b,false);
            input_buffers[i==4?5:i]=buffers[i];
        }
        if(backdrop_upload_size) {
            SDL_GPUTransferBufferLocation a{upload,backdrop_upload_offset};
            SDL_GPUBufferRegion b{backdrop_buffer,0,backdrop_upload_size};
            SDL_UploadToGPUBuffer(pass,&a,&b,false);
        }
        if(resident) {
            input_buffers[0]=input_buffers[1]=static_cast<SDL_GPUBuffer*>(resident->packed);
            input_buffers[2]=static_cast<SDL_GPUBuffer*>(resident->surfaces);
        }
        if(s.resident_shadow.buffer) input_buffers[3]=static_cast<SDL_GPUBuffer*>(s.resident_shadow.buffer);
        input_buffers[4]=input_buffers[0]; // valid unused binding when ScaleFX is off
        // Disabled stages never read their slots; keep valid bindings without
        // allocating or uploading empty placeholder buffers. Reset every call
        // so toggles cannot leave a stale pointer from an earlier frame.
        for(auto& buffer:input_buffers) if(!buffer) buffer=input_buffers[0];
        SDL_EndGPUCopyPass(pass);
        gpu_timestamps.mark(timestamp_ticket,command);
        std::array<SDL_GPUTexture*,2> work{images[0],images[1]};
        if(borrow_resident) work[0]=static_cast<SDL_GPUTexture*>(resident->rgba);
        unsigned current=0;
        // The first effect reads the immutable compositor image directly.
        // Its destination is owned scratch; subsequent passes ping-pong only
        // between our own textures. Submission and retirement stay unchanged.
        const auto advance=[&] {
            current=1-current;work[1-current]=images[1-current];
        };
        const auto run=[&](unsigned stage){dispatch(p,stage,work[current],work[1-current]);advance();};
        if(p.filter) {
            dispatch(p,13,work[current],nullptr,nullptr,nullptr,nullptr,native);
            if(p.filter==5) {
                const auto result=scalefx.enqueue_texture(device,command,native,width/scale,height/scale);
                if(!result.buffer) throw std::runtime_error(scalefx.status());
                input_buffers[4]=static_cast<SDL_GPUBuffer*>(result.buffer);
            }
            run(14);
        }
        if(s.resident_reflection.buffer && s.reflection_intensity) {
            const auto& r=s.resident_reflection;auto reflection=p;
            reflection.shadow_width=r.width;reflection.shadow_height=r.height;
            reflection.shadow_y=s.reflection_offset_y;
            reflection.pad0=int(std::min(s.reflection_intensity,100U));
            reflection.pad1=0;
            reflection.overlay_filter=s.reflection_material?1:0;
            auto* saved=input_buffers[3];input_buffers[3]=static_cast<SDL_GPUBuffer*>(r.buffer);
            dispatch(reflection,30,work[current],work[1-current]);advance();
            input_buffers[3]=saved;
        }
        // Isolated artwork owns its final pixels, not the model metadata below
        // it. Illuminate the scene before placing portraits/briefing text.
        if(has_overlays && p.lighting) run(6);
        const auto apply_isolated_overlays=[&] {
        for(unsigned i=0;i<2;++i) if(s.subtractive_overlays[i]) {
            auto overlay=p;
            auto* saved_shadow=input_buffers[3];
            if(s.subtractive_overlays[i]->resident.pixels)
                input_buffers[3]=static_cast<SDL_GPUBuffer*>(s.subtractive_overlays[i]->resident.pixels);
            overlay.pad0=int(overlay_offsets[i]);overlay.pad1=int(palette_offset);
            overlay.pad3=s.subtractive_overlays[i]->resident.pixels?1:0;
            overlay.lighting=Uint32(s.overlay_palette.size());
            overlay.hdr=30-std::min(s.subtractive_overlays[i]->brightness,30U);
            overlay.overlay_filter=1;
            dispatch(overlay,28,work[current],nullptr,nullptr,nullptr,nullptr,native);
            if(p.filter) {
                if(p.filter==5) {
                    const auto result=scalefx.enqueue_texture(device,command,native,width/scale,height/scale);
                    if(!result.buffer) throw std::runtime_error(scalefx.status());
                    input_buffers[4]=static_cast<SDL_GPUBuffer*>(result.buffer);
                }
                dispatch(overlay,14,work[current],side);
                if(debug) copy(side,debug->images[i+2]);
            }
            dispatch(overlay,29,work[current],work[1-current],p.filter?side:nullptr);advance();
            if(debug) copy(work[current],debug->images[i]);
            input_buffers[3]=saved_shadow;
        }
        };
        if(!s.motion_blur) apply_isolated_overlays();
        // Replace scenery before cartridge flashes/wipes, never over them.
        if(s.environment.active()) {
            auto env=p;env.environment_classes=s.environment.classes;
            env.environment_modes=s.environment.modes;env.environment_motion=s.environment.motion;env.environment_plane=s.environment.plane;env.scroll_fraction=s.environment.scroll_fraction;
            env.ground_gradient=s.environment.ground_gradient;
            env.backdrop_projection=s.environment.backdrop_projection;env.backdrop_keep=s.environment.backdrop_keep;
            env.backdrop_palette=s.environment.backdrop_palette;
            env.backdrop_ramp=s.environment.backdrop_ramp;
            env.overlay_filter=0;
            env.surface_width=backdrop?backdrop->width:0;
            env.surface_height=backdrop?backdrop->height:0;
            env.surface_x=0;
            const bool ray_ground=s.environment.ray_water && s.resident_reflection.buffer;
            // Paired real-eye timings support this on D3D12, not Vulkan. Keep
            // Vulkan's previous resolve until its submission/material stalls
            // are understood; the same exact fusion remains testable there.
            const bool fusion_policy=environment_reflection_default
                || SDL_getenv("STARFOX_TEST_FUSE_ENVIRONMENT_REFLECTION");
            const bool fuse_ray_ground=ray_ground && fusion_policy
                && !SDL_getenv("STARFOX_TEST_SEPARATE_ENVIRONMENT_REFLECTION");
            env.pad3=fuse_ray_ground?1U:0U;
            auto* saved_reflection=input_buffers[3];
            if(fuse_ray_ground) {
                const auto& r=s.resident_reflection;
                env.shadow_width=r.width;env.shadow_height=r.height;env.shadow_y=s.reflection_offset_y;
                input_buffers[3]=static_cast<SDL_GPUBuffer*>(r.buffer);
            }
            auto* saved_setup=input_buffers[5];
            if(backdrop) input_buffers[5]=backdrop_buffer;
            dispatch(env,31,work[current],work[1-current]);advance();
            input_buffers[5]=saved_setup;
            input_buffers[3]=saved_reflection;
            // Reflect the finished environment, not the old cartridge sky.
            // Both passes stay in the same resident ping-pong/command owner;
            // no readback, extra texture or physical-ray double resolve.
            if(environment_screen_reflections(s.environment)) {
                dispatch(env,41,work[current],work[1-current]);advance();
            }
            if(ray_ground && !fuse_ray_ground) {
                const auto& r=s.resident_reflection;
                env.shadow_width=r.width;env.shadow_height=r.height;env.shadow_y=s.reflection_offset_y;
                env.pad0=100;env.pad1=1;
                auto* saved=input_buffers[3];input_buffers[3]=static_cast<SDL_GPUBuffer*>(r.buffer);
                dispatch(env,30,work[current],work[1-current]);advance();
                input_buffers[3]=saved;
            }
            if(ray_ground && !environment_reflection_reported && SDL_getenv("STARFOX_TEST_ENVIRONMENT_REFLECTION_RESULT")) {
                SDL_Log("environment-reflection: %s",fuse_ray_ground?"fused":"separate");
                environment_reflection_reported=true;
            }
        }
        const auto apply_background_fade=[&] {if(s.background_subtract) {
            auto fade=p;
            fade.pad0=int(std::min(s.background_subtract,31U));
            fade.pad1=s.background_subtract_protect_models?1:0;
            dispatch(fade,20,work[current],work[1-current]);advance();
        }};
        if(!s.motion_blur) apply_background_fade();
        if(s.volumetric.buffer) {
            auto* previous=input_buffers[3];
            input_buffers[3]=static_cast<SDL_GPUBuffer*>(s.volumetric.buffer);
            run(40);
            input_buffers[3]=previous;
        }
        const auto apply_cartridge_overlays=[&] {
        if(s.circle) {
            const auto& c=*s.circle;
            auto disk=p;
            disk.surface_x=c.x;disk.surface_y=c.y;disk.pad0=c.radius;
            disk.min_x=c.left;disk.min_y=c.top;disk.max_x=c.right;disk.max_y=c.bottom;
            disk.hdr=c.red;disk.chromatic=c.green;disk.smoothing=c.blue;
            disk.pad1=(c.subtract?1:0)|(c.half?2:0)|(c.affect_sprites?4:0);
            dispatch(disk,19,work[current],work[1-current]);advance();
        }
        if(s.colour_math) {
            const auto& c=*s.colour_math;
            auto tint=p;
            tint.hdr=c.red;tint.chromatic=c.green;tint.smoothing=c.blue;
            tint.pad1=(c.subtract?1:0)|(c.half?2:0)|(c.affect_sprites?4:0);
            dispatch(tint,21,work[current],work[1-current]);advance();
        }
        if(s.planet_fade) {
            const auto& f=*s.planet_fade;auto fade=p;
            fade.min_x=f.left;fade.min_y=f.top;fade.max_x=f.right;fade.max_y=f.bottom;
            fade.hdr=std::min(f.isolate_amount,31U);fade.chromatic=std::min(f.level_amount,31U);
            fade.pad0=(f.isolate?1:0)|(f.level?2:0);
            if(f.coverage) {fade.pad0|=4;std::copy(f.rows.begin(),f.rows.end(),fade.window_rows.begin());}
            dispatch(fade,27,work[current],work[1-current]);advance();
        }
        if(s.window_mask) {
            const auto& w=*s.window_mask;
            auto mask=p;
            mask.window_rows=w.rows;mask.surface_x=w.origin_x;mask.surface_y=w.origin_y;
            mask.pad0=int(w.logic&3U);mask.pad1=(w.expand_x?1:0)|(w.expand_y?2:0);
            dispatch(mask,22,work[current],work[1-current]);advance();
        }
        };
        if(!s.motion_blur) apply_cartridge_overlays();
        const auto apply_horizontal_wipe=[&] {
            if(!s.horizontal_wipe) return;
            const auto& w=*s.horizontal_wipe;
            auto shutter=p;
            shutter.surface_x=w.band_top;shutter.surface_y=w.band_bottom;
            shutter.min_x=w.open_top;shutter.min_y=w.open_bottom;
            shutter.max_x=w.guard_width;shutter.max_y=w.origin_x;
            shutter.pad0=w.expanded?1:0;
            dispatch(shutter,18,work[current],work[1-current]);advance();
        };
        if(!s.motion_blur) apply_horizontal_wipe();
        if(p.lighting && !has_overlays) run(6);
        if(p.hdr) run(1);
        if(p.chromatic) run(2);
        if(p.shadow_enabled && s.shadow_before_style) run(15);
        const auto apply_host_overlays=[&] {
        if(s.host_overlay) {
            const auto& h=*s.host_overlay;
            auto overlay=p;
            overlay.window_rows=h.bits;overlay.surface_x=h.x;overlay.surface_y=h.y;
            overlay.surface_width=h.width;overlay.surface_height=h.height;
            dispatch(overlay,23,work[current],work[1-current]);advance();
        }
        if(s.confirmation_overlay) {
            const auto& h=*s.confirmation_overlay;
            auto overlay=p;
            overlay.window_rows=h.bits;overlay.surface_x=h.x;overlay.surface_y=h.y;
            overlay.surface_width=h.width;overlay.surface_height=h.height;
            dispatch(overlay,24,work[current],work[1-current]);advance();
        }
        };
        if(!s.motion_blur) apply_host_overlays();
        // Diagnostic for mobile Vulkan drivers which appear to reuse the
        // environment output before the following bloom extraction sees it.
        if(const auto* barrier=std::getenv("STARFOX_GPU_STAGE_BARRIER"); barrier && s.environment.active()
            && (p.bloom_model || p.bloom_world)) {
            gpu_timestamps.end(timestamp_ticket,command);
            if(std::string_view(barrier)=="2" && !gpu_timestamps.active()) {
                require(SDL_SubmitGPUCommandBuffer(command));command=nullptr;
            } else {
                auto* completed=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
                command=nullptr;require(completed);
                gpu_timestamps.submitted(timestamp_ticket,completed);
                require(SDL_WaitForGPUFences(device,true,&completed,1));
                gpu_timestamps.retired(completed);
                SDL_ReleaseGPUFence(device,completed);
            }
            command=SDL_AcquireGPUCommandBuffer(device);require(command);
            timestamp_ticket=gpu_timestamps.begin(device,command,0);
            gpu_timestamps.mark(timestamp_ticket,command);
        }
        if(p.smoothing==4 && scale>1 && !std::getenv("STARFOX_TEST_GENERIC_DEDITHER")) {
            dedither(work[current],work[1-current],resident!=nullptr);advance();
        } else if(p.smoothing) run(3);
        if(decorative_material(static_cast<Effect>(s.material))) {
            auto material=p;material.model=s.material;material.world=0;material.model_intensity=100;
            dispatch(material,4,work[current],work[1-current]);advance();
        }
        if(p.model || p.world) run(4);
        if((spatial_manipulation(static_cast<Effect>(s.manipulation)) && s.manipulation_intensity) || s.extra_effects[0]) {
            auto manipulation=p;
            manipulation.model=spatial_manipulation(static_cast<Effect>(s.manipulation))?s.manipulation:0;manipulation.world=s.extra_effects[0];
            manipulation.model_intensity=s.manipulation_intensity;
            manipulation.world_intensity=100;manipulation.pad1=std::bit_cast<std::int32_t>(float(s.presentation_seconds));
            dispatch(manipulation,4,work[current],work[1-current]);advance();
        }
        if(s.extra_effects[1] || s.extra_effects[2]) {
            auto fx=p;fx.model=s.extra_effects[1];fx.world=s.extra_effects[2];fx.model_intensity=fx.world_intensity=100;
            fx.pad1=std::bit_cast<std::int32_t>(float(s.presentation_seconds));
            dispatch(fx,4,work[current],work[1-current]);advance();
        }
        if(s.depth_fx.active()) run(35);
        if(s.scene_fx.active()) run(34);
        if(s.global_enhancements) {
            auto global=p;global.pad0=int(s.global_enhancements&0x03ffffffU);
            global.pad1=std::bit_cast<std::int32_t>(float(s.presentation_seconds));
            dispatch(global,33,work[current],work[1-current]);advance();
        }
        const auto apply_model_exposure=[&]() {
            auto input=*resident;input.rgba=work[current];void* blurred{};
            const auto& blur=*s.motion_blur;
            if(blur.guide_source) {
                const auto& guides=*blur.guide_source;
                if(guides.device!=input.device || guides.width!=input.width || guides.height!=input.height)
                    throw std::runtime_error("Motion blur guide extent/device mismatch");
                input.geometry_depth=guides.geometry_depth;input.motion=guides.motion;
            }
            if(!motion_blur.enqueue(command,input,blur.underlay,blur.settings,blur.history_valid,blurred,blur.guide_jitter,
                s.particle_shutter?&*s.particle_shutter:nullptr,float(scale)))
                throw std::runtime_error(motion_blur.status());
            if(blurred!=input.rgba) {copy(static_cast<SDL_GPUTexture*>(blurred),work[1-current]);advance();}
        };
        if(s.particle_shutter) {
            apply_model_exposure();
        }
        if(p.bloom_model || p.bloom_world) {
            copy(work[current],snapshots[0]);
            dispatch(p,7,work[current],nullptr,nullptr,nullptr,bloom[0]);
            dispatch(p,8,work[current],nullptr,bloom[0],nullptr,bloom[1]);
            dispatch(p,9,work[current],nullptr,bloom[1],nullptr,bloom[2]);
            dispatch(p,10,work[current],nullptr,bloom[2],nullptr,bloom[1]);
            dispatch(p,11,work[current],nullptr,bloom[1],nullptr,bloom[3]);
            dispatch(p,12,work[current],work[1-current],bloom[3],bloom[2]);advance();
            copy(work[current],snapshots[1]);
        }
        if(p.aa && p.aa/4==5) {
            auto* resolved=static_cast<SDL_GPUTexture*>(smaa.enqueue(device,command,work[current],input_buffers[0],
                width,height,p.aa%4,resident!=nullptr));
            if(!resolved) throw std::runtime_error(smaa.status());
            copy(resolved,work[1-current]);advance();
            if(std::getenv("STARFOX_TRACE_GPU")) SDL_Log("smaa: resolved %ux%u quality=%u",width,height,p.aa%4);
        } else if(p.aa && p.aa/4<4) run(5);
        if(p.shadow_enabled && !s.shadow_before_style) run(15);
        if(s.motion_blur) {
            if(!s.particle_shutter) apply_model_exposure();
            // These consume the reconstructed scene; none enters the exposure.
            apply_isolated_overlays();apply_background_fade();apply_cartridge_overlays();apply_host_overlays();
        }
        // Spatial effects may pull coloured neighbours across the shutter or
        // erode its first visible row. Keep the authored opening a straight,
        // monotonic clip after styling too, before host/menu overlays.
        apply_horizontal_wipe();
        if(persistence) {
            const unsigned key=s.persistence_mode|(s.persistence_models?4U:0U)|(s.persistence_world?8U:0U);
            const bool reset=!history_valid || key!=history_key || s.scene_epoch!=history_epoch
                || s.presentation_seconds<history_time || s.presentation_seconds-history_time>1.;
            const float decay=reset?0.f:s.persistence_mode==2?1.f:
                float(std::exp2(-(s.presentation_seconds-history_time)/.35));
            auto temporal=p;
            temporal.pad0=std::bit_cast<Sint32>(decay);
            temporal.pad1=int(std::min(s.persistence_intensity,100U));
            temporal.chromatic=(s.persistence_models?1U:0U)|(s.persistence_world?2U:0U)|(reset?4U:0U);
            dispatch(temporal,32,work[current],work[1-current],history[history_index],nullptr,history[1-history_index]);
            advance();history_index=1-history_index;
            history_valid=true;history_key=key;history_epoch=s.scene_epoch;history_time=s.presentation_seconds;
        }
        if(use_exposure) {
            const bool reset=!exposure.valid || exposure.key!=s.exposure || exposure.epoch!=s.scene_epoch
                || s.presentation_seconds<exposure.time || s.presentation_seconds-exposure.time>1.;
            auto meter=p;
            meter.pad0=std::bit_cast<Sint32>(reset || s.exposure_paused?0.f:float(s.presentation_seconds-exposure.time));
            meter.pad1=int(s.exposure);meter.chromatic=reset?1:0;
            dispatch(meter,36,work[current],nullptr,nullptr,nullptr,exposure_histogram);
            dispatch(meter,37,work[current],nullptr,exposure_histogram,exposure.textures[exposure.index],exposure.textures[1-exposure.index]);
            dispatch(meter,38,work[current],work[1-current],exposure.textures[1-exposure.index]);
            advance();exposure.index=1-exposure.index;
            exposure.valid=true;exposure.key=s.exposure;exposure.epoch=s.scene_epoch;exposure.time=s.presentation_seconds;
        }
        if(use_phosphor) {
            const bool reset=!phosphor.valid || phosphor.key!=s.phosphor || phosphor.epoch!=s.scene_epoch
                || s.presentation_seconds<phosphor.time || s.presentation_seconds-phosphor.time>1.;
            auto temporal=p;
            temporal.pad0=std::bit_cast<Sint32>(reset?0.f:float(std::exp2(-(s.presentation_seconds-phosphor.time)/(.03*s.phosphor))));
            temporal.pad1=100;temporal.chromatic=1U|2U|8U|(reset?4U:0U);
            dispatch(temporal,32,work[current],work[1-current],phosphor.textures[phosphor.index],nullptr,phosphor.textures[1-phosphor.index]);
            advance();phosphor.index=1-phosphor.index;
            phosphor.valid=true;phosphor.key=s.phosphor;phosphor.epoch=s.scene_epoch;phosphor.time=s.presentation_seconds;
        }
        if(s.camera_response) {
            const auto matrix=camera_response_matrix(*s.camera_response);
            const auto crop=camera_response_crop(width,height,s.camera_response_focal[0],s.camera_response_focal[1],matrix);
            if(crop) {
                auto response=p;
                for(unsigned i=0;i<3;++i) {
                    response.backdrop_projection[i]=float(matrix[i]);
                    response.backdrop_keep[0][i]=float(matrix[3+i]);
                    response.backdrop_keep[1][i]=float(matrix[6+i]);
                }
                response.environment_motion={float(width-1)*.5f,float(height-1)*.5f,float(s.camera_response_focal[0]),float(s.camera_response_focal[1])};
                response.environment_plane[0]=float(*crop);
                dispatch(response,39,work[current],work[1-current]);advance();
            }
        }
        // Host UI has no world-space ownership in the source's packed tags.
        // Drawing it earlier lets exposure/afterglow/camera motion process the
        // model or sky *under* its glyphs, causing text colour flicker and smear.
        // Match the software path: overlay the native UI after world history.
        if(s.setup_overlay) {
            const auto& setup=*s.setup_overlay;auto overlay=p;
            overlay.surface_width=setup.frame->width();overlay.surface_height=setup.frame->height();
            overlay.min_x=setup.left;overlay.max_x=setup.right;overlay.pad0=setup.brightness;
            dispatch(overlay,25,work[current],work[1-current]);advance();
        }
        if(s.touch_controls) run(26);
        gpu_timestamps.mark(timestamp_ticket,command);
        if(present) {
            if(work[current]!=images[current]) {
                // OFF must retain an owned image for deferred/repeated reads,
                // even when the compositor is reused before a screenshot.
                copy(work[current],images[current]);work[current]=images[current];
            }
            if(separate_capture) {
                if(!capture) capture=texture(width,height);
                copy(work[current],capture);capture_source=capture;
            } else {
                // Presentation copies from this image but does not modify it.
                // Retain the existing submission fence for readback/reuse; no
                // new queue-order or lifetime assumption is needed.
                capture_source=work[current];
            }
            capture_pending=true;
        } else download_texture(work[current],0);
        if(glow) {
            dispatch(p,16,work[current],work[1-current],nullptr,nullptr,nullptr,nullptr,side);advance();copy(side,glow);
        }
        if(model) {
            dispatch(p,17,work[current],work[1-current],nullptr,nullptr,nullptr,nullptr,side);advance();copy(side,model);
        }
        if(present) copy(work[current],present);
        else if(p.bloom_model || p.bloom_world) {
            if(s.bloom_base) download_texture(snapshots[0],bytes);
            if(s.bloom_glow) download_texture(snapshots[1],bytes*2);
        }
        if(debug) {
            auto* pass=SDL_BeginGPUCopyPass(command);require(pass);
            for(unsigned i=0;i<4;++i) {
                SDL_GPUTextureRegion region{debug->images[i],0,0,0,0,0,width,height,1};
                SDL_GPUTextureTransferInfo destination{debug->download,bytes*i,0,0};
                SDL_DownloadFromGPUTexture(pass,&region,&destination);
            }
            SDL_EndGPUCopyPass(pass);
        }
        const auto encoded=profile?std::chrono::steady_clock::now():begin;
        gpu_timestamps.end(timestamp_ticket,command);
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence);
        gpu_timestamps.submitted(timestamp_ticket,fence);timestamp_ticket=-1;
        if(ordered_queue_reuse && !reported_ordered_reuse
            && (std::getenv("STARFOX_TRACE_GPU") || std::getenv("STARFOX_TEST_STEREO_RESULT"))) {
            std::cerr<<"effects-ordered-queue: submitted\n";reported_ordered_reuse=true;
        }
        if(profile) {
            const auto done=std::chrono::steady_clock::now();
            const auto us=[](auto a,auto b){return std::chrono::duration_cast<std::chrono::microseconds>(b-a).count();};
            std::cerr<<"gpu-effects-cost-us wait="<<us(begin,waited)<<" prepare="<<us(waited,prepared)
                <<" encode="<<us(prepared,encoded)<<" submit="<<us(encoded,done)<<" upload="<<total<<'\n';
        }
        if(backdrop_upload_size) backdrop_pixels.remember(*backdrop);
        if(debug) {
            finish();auto* data=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,debug->download,false));require(data);
            for(unsigned i=0;i<4;++i) {
                auto* surface=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,data+bytes*i,width*4);require(surface);
                const auto path=std::string(debug_path)+"-"+std::to_string(i)+".bmp";
                SDL_SaveBMP(surface,path.c_str());SDL_DestroySurface(surface);
            }
            SDL_UnmapGPUTransferBuffer(device,debug->download);overlay_debug_captured=true;
        }
        if(!present) {
            // Keep input untouched until all requested snapshots succeed.
            if(p.bloom_model || p.bloom_world) {if(s.bloom_base) read(*s.bloom_base,bytes);if(s.bloom_glow) read(*s.bloom_glow,bytes*2);}
            read(rgba);
        }
    }
};
#else
struct SdlGpuEffects::Impl { std::string status{"SDL GPU effects unavailable on this build"}; };
#endif
SdlGpuEffects::SdlGpuEffects():impl_(std::make_unique<Impl>()) {}
SdlGpuEffects::~SdlGpuEffects()=default;
void SdlGpuEffects::release_device() noexcept {impl_.reset();}
const std::string& SdlGpuEffects::status() const {static const std::string empty{"SDL GPU device released"};return impl_?impl_->status:empty;}
std::uint64_t SdlGpuEffects::texture_payload_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_?impl_->texture_payload_bytes():0;
#else
    return 0;
#endif
}
std::uint32_t SdlGpuEffects::last_staging_upload_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_?impl_->last_staging_upload:0;
#else
    return 0;
#endif
}
bool SdlGpuEffects::readback(std::vector<std::uint8_t>& rgba) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_ || (!impl_->download && !impl_->capture_pending)) return false;
    try {return impl_->read(rgba);} catch(const std::exception& e){impl_->status=e.what();return false;}
#else
    (void)rgba;return false;
#endif
}
bool SdlGpuEffects::apply(void* source,const Framebuffer& frame,std::vector<std::uint8_t>& rgba,const GpuEffectSettings& settings) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!source || !frame.layer_tags_enabled() || rgba.empty() || rgba.size()!=frame.pixels().size()*4) return false;
#if !defined(STARFOX_ENABLE_XBRZ)
    if(settings.filter==2) return false;
#endif
    if(!impl_ || impl_->device!=source) impl_=std::make_unique<Impl>();
    if(impl_->failed) return false;
    try {if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(source));
        impl_->ordered_queue_reuse=ordered_queue_reuse_;impl_->apply(frame,rgba,settings);return true;}
    catch(const std::exception& e) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;impl_->gpu_timestamps.cancel(impl_->timestamp_ticket);impl_->timestamp_ticket=-1;}
        impl_->status=e.what();impl_->failed=true;return false;
    }
#else
    (void)source;(void)frame;(void)rgba;(void)settings;return false;
#endif
}
bool SdlGpuEffects::apply_resident(const GpuCompositeOutput& source,const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba,const GpuEffectSettings& settings) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!source.device) return false;
#if !defined(STARFOX_ENABLE_XBRZ)
    if(settings.filter==2) return false;
#endif
    if(!impl_ || impl_->device!=source.device) impl_=std::make_unique<Impl>();
    if(impl_->failed) return false;
    try {if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(source.device));
        impl_->ordered_queue_reuse=ordered_queue_reuse_;
        impl_->apply(frame,rgba,settings,&source);return true;}
    catch(const std::exception& e) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;impl_->gpu_timestamps.cancel(impl_->timestamp_ticket);impl_->timestamp_ticket=-1;}
        impl_->status=e.what();impl_->failed=true;return false;
    }
#else
    (void)source;(void)frame;(void)rgba;(void)settings;return false;
#endif
}
}
