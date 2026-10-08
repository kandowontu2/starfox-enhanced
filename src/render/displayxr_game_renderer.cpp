#include "starfox/render/displayxr_game_renderer.hpp"
#include "starfox/render/gpu_calibrated_ray_composite.hpp"
#include "starfox/render/gpu_calibrated_persistence.hpp"
#include "starfox/render/gpu_calibrated_global.hpp"
#include "starfox/render/gpu_calibrated_bloom.hpp"
#include "starfox/render/gpu_calibrated_exposure.hpp"
#include "starfox/render/gpu_calibrated_depth.hpp"
#include "starfox/render/gpu_calibrated_scene_fx.hpp"
#include "starfox/render/gpu_calibrated_volumetric.hpp"
#include "starfox/render/calibrated_game_scene_fx.hpp"
#include "starfox/render/gpu_calibrated_aa.hpp"
#include "starfox/render/gpu_calibrated_fsr1.hpp"
#include "starfox/render/gpu_calibrated_dlss.hpp"
#include "starfox/render/gpu_calibrated_msaa.hpp"
#include "starfox/render/calibrated_reconstructed_post_memory.hpp"
#include "starfox/render/gpu_calibrated_temporal_aa.hpp"
#include "starfox/render/gpu_calibrated_reflection_history.hpp"
#include "starfox/render/calibrated_reflection_timeline.hpp"
#include "starfox/render/calibrated_game_motion.hpp"
#include "starfox/render/calibrated_motion_history.hpp"
#include "starfox/render/calibrated_ground_receiver.hpp"
#include "starfox/render/calibrated_environment.hpp"
#include "starfox/render/gpu_calibrated_motion_blur.hpp"
#include "starfox/render/temporal_jitter.hpp"
#include "starfox/render/sdl_multisample.h"
#include "starfox/render/global_enhancements.hpp"
#include "starfox/render/camera_response.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace starfox::render {
struct DisplayXrGameRenderer::State {
    SDL_GPUDevice* device{};DisplayXrD3D12Presenter presenter;GpuCalibratedScene scene;
    std::array<SDL_GPUTexture*,2> color{},depth{};
    std::array<SDL_GPUTexture*,2> supersampled{},native_ink{},native_receiver{},native_depth{};
    std::array<SDL_GPUTexture*,2> fsr_color{},fsr_ink{},fsr_receiver{},fsr_depth{};
    std::array<GpuCalibratedFsr1,2> fsr;
    bool fsr_ready{};
    std::array<SDL_GPUTexture*,2> base{},receiver{};
    std::array<SDL_GPUTexture*,2> surfaces{};
    std::array<SDL_GPUTexture*,2> water_receiver{},water_surface{};
    std::array<SDL_GPUTexture*,2> temporal_input{},temporal_middle{};
    // Independent world trails, model trails and global CRT afterglow per eye.
    std::array<GpuCalibratedPersistence,6> persistence;
    GpuCalibratedGlobal global;
    GpuCalibratedBloom bloom;
    std::array<GpuCalibratedExposure,2> exposure;
    GpuCalibratedDepth depth_effect;
    GpuCalibratedSceneFx scene_effects;
    bool scene_effects_ready{};
    GpuCalibratedVolumetric volumetric;
    bool volumetric_ready{};
    std::array<CalibratedRayGeometryOutput,2> fog_geometry;
    std::array<GpuCalibratedAa,2> aa;
    std::optional<CalibratedGameSettings> history_settings;
    std::uint64_t history_revision{},history_epoch{};unsigned history_flow{};
    timing::TransformSnapshot history_camera{};
    bool images_ready{},history_ready{},globals_ready{},bloom_ready{},exposure_ready{},depth_ready{},aa_ready{},history_accepted{},presentation_failed{};
    GpuCalibratedRayComposite composite;
    std::array<shadows::SdlDxrShadows,2> shadow{shadows::SdlDxrShadows{true},shadows::SdlDxrShadows{true}},
        reflection{shadows::SdlDxrShadows{true},shadows::SdlDxrShadows{true}};
    struct LiquidDiagnostics : std::array<std::array<ReflectionLiquidDiagnosticInputs,8>,2> {
        std::array<std::optional<vr::EyeCamera>,2> raster_cameras;
    };
    std::unique_ptr<LiquidDiagnostics> liquid_diagnostics;
    bool curved_validation{}; // Selection survives per-frame diagnostic reset.
    bool source_validation{};
    ReflectionSourceStageObserver source_stage_observer{};void* source_stage_context{};
    std::array<float,2> diagnostic_sample_offset{};
    vr::EyeCamera diagnostic_source_camera(vr::EyeCamera camera,unsigned eye) const noexcept {
        if(liquid_diagnostics) {
            const auto size=scene_extent(eye);
            camera.projection[8]+=2*diagnostic_sample_offset[0]/size[0];
            camera.projection[9]-=2*diagnostic_sample_offset[1]/size[1];
        }
        return camera;
    }
    // Output is fixed by XR. The scene/AA/ray grids really use the FSR input
    // extent, not a downscaled copy of an already full-resolution scene.
    std::array<std::array<std::uint32_t,2>,2> extent{},output_extent{};
    unsigned sample_factor{1},fsr_mode{},dlss_mode{},dlss_model{};
    std::array<std::uint32_t,2> scene_extent(unsigned eye) const noexcept {
        return {extent[eye][0]*sample_factor,extent[eye][1]*sample_factor};
    }
    SDL_GPUTexture* scene_color(unsigned eye) const noexcept {
        return sample_factor==1?native_color(eye):supersampled[eye];
    }
    SDL_GPUTexture* native_color(unsigned eye) const noexcept {return fsr_mode || dlss_mode?fsr_color[eye]:color[eye];}
    bool prepare_fsr() {
        if(fsr_ready) return true;
        for(auto& eye:fsr) if(!eye.initialize(device,format)) return false;
        fsr_ready=true;return true;
    }
#include "displayxr_motion_blur.inc"
#include "displayxr_dlss.inc"
    bool prepare_samples(unsigned factor,unsigned mode,unsigned sdk_mode,unsigned sdk_model) {
        if(factor==sample_factor && mode==fsr_mode && sdk_mode==dlss_mode && sdk_model==dlss_model
            && (!sdk_mode || (dlss && dlss->mode==sdk_mode && dlss->model==sdk_model))) return true;
        configure_dlss(sdk_mode,sdk_model);
        auto next_extent=output_extent;
        for(unsigned e=0;e<2;++e) {
            const auto input=sdk_mode?dlss->resolve[e].input_extent():fsr1_input_extent({output_extent[e][0],output_extent[e][1]},static_cast<Fsr1Mode>(mode));
            next_extent[e]={input.width,input.height};
        }
        std::uint64_t scene_pixels=0,output_pixels=0;
        for(unsigned e=0;e<2;++e) {
            scene_pixels+=std::uint64_t(next_extent[e][0])*next_extent[e][1]*factor*factor;
            output_pixels+=std::uint64_t(output_extent[e][0])*output_extent[e][1];
        }
        // Combined conservative bound, including both eyes' largest sample,
        // shutter/history/ray layout and the extra full-panel ink/FSR images.
        // SDK uses one centre guide set, not eight retained sample histories.
        // Actual MSAA/fog/shutter owners apply their additional selected bounds
        // below. Do not charge an ordinary DLSS eye for unselected 8x MSAA.
        const auto bytes_per_scene=sdk_mode?576ULL:1504ULL;
        if((mode || sdk_mode) && (scene_pixels*bytes_per_scene+output_pixels*48+64ULL*1024*1024>4ULL*1024*1024*1024))
            throw std::runtime_error("Native upscaler exceeds the resident eye/history working bound");
        // Allocate transactionally before changing retained resources. A bad
        // extent/allocation must not partially replace either eye's targets.
        std::array<SDL_GPUTexture*,2> next_depth{},next_color{},next_ink{},next_receiver{},next_ink_depth{};
        std::array<SDL_GPUTexture*,2> next_fsr{},next_fsr_ink{},next_fsr_receiver{},next_fsr_depth{};
        const auto release=[&](auto& images) {
            for(auto*& image:images) if(image) {SDL_ReleaseGPUTexture(device,image);image=nullptr;}
        };
        try {
            for(unsigned e=0;e<2;++e) {
                if(!calibrated_sample_extent_supported(next_extent[e][0],next_extent[e][1],factor))
                    throw std::runtime_error("Native SSAA exceeds the 64-megapixel per-eye working-image limit");
                SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
                info.width=next_extent[e][0]*factor;info.height=next_extent[e][1]*factor;
                info.layer_count_or_depth=info.num_levels=1;info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
                info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
                next_depth[e]=SDL_CreateGPUTexture(device,&info);if(!next_depth[e]) throw std::runtime_error(SDL_GetError());
                info.format=format;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
                if(factor>1) {
                next_color[e]=SDL_CreateGPUTexture(device,&info);if(!next_color[e]) throw std::runtime_error(SDL_GetError());
                info.width=next_extent[e][0];info.height=next_extent[e][1];
                next_ink[e]=SDL_CreateGPUTexture(device,&info);if(!next_ink[e]) throw std::runtime_error(SDL_GetError());
                info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                next_receiver[e]=SDL_CreateGPUTexture(device,&info);if(!next_receiver[e]) throw std::runtime_error(SDL_GetError());
                info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
                next_ink_depth[e]=SDL_CreateGPUTexture(device,&info);if(!next_ink_depth[e]) throw std::runtime_error(SDL_GetError());
                }
                if(mode || sdk_mode) {
                    info.width=next_extent[e][0];info.height=next_extent[e][1];info.format=format;
                    info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
                    next_fsr[e]=SDL_CreateGPUTexture(device,&info);if(!next_fsr[e]) throw std::runtime_error(SDL_GetError());
                    info.width=output_extent[e][0];info.height=output_extent[e][1];
                    next_fsr_ink[e]=SDL_CreateGPUTexture(device,&info);if(!next_fsr_ink[e]) throw std::runtime_error(SDL_GetError());
                    info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                    next_fsr_receiver[e]=SDL_CreateGPUTexture(device,&info);if(!next_fsr_receiver[e]) throw std::runtime_error(SDL_GetError());
                    info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
                    next_fsr_depth[e]=SDL_CreateGPUTexture(device,&info);if(!next_fsr_depth[e]) throw std::runtime_error(SDL_GetError());
                }
            }
        } catch(...) {
            release(next_depth);release(next_color);release(next_ink);release(next_receiver);release(next_ink_depth);
            release(next_fsr);release(next_fsr_ink);release(next_fsr_receiver);release(next_fsr_depth);throw;
        }
        // All allocations succeeded; no live command may reference this layout.
        msaa.reset();taa.reset();reflected.reset();reconstructed_post.reset();
        release(native_ink);release(native_receiver);release(native_depth);
        release(fsr_color);release(fsr_ink);release(fsr_receiver);release(fsr_depth);
        for(auto& eye:fsr) eye.release_device();fsr_ready=false;
        fsr_color=next_fsr;fsr_ink=next_fsr_ink;fsr_receiver=next_fsr_receiver;fsr_depth=next_fsr_depth;
        release(depth);release(supersampled);depth=next_depth;supersampled=next_color;
        for(unsigned e=0;e<2;++e) if(next_ink[e]) {
            native_ink[e]=next_ink[e];native_receiver[e]=next_receiver[e];native_depth[e]=next_ink_depth[e];
        }
        release(base);release(receiver);release(surfaces);release(temporal_input);release(temporal_middle);
        release(water_receiver);release(water_surface);
        blur.reset();
        // This source-grid change has drained native/presenter work. Release
        // old eye-sized caches too: merely clearing the ready flags retains
        // larger ray, trail, meter and bloom images behind the new grid's bound.
        for(unsigned e=0;e<2;++e) {shadow[e].release_device();reflection[e].release_device();}
        for(auto& h:persistence) h.release_device();
        for(auto& meter:exposure) meter.release_device();
        for(auto& eye:aa) eye.release_device();
        global.release_device();bloom.release_device();depth_effect.release_device();
        scene_effects.release_device();scene_effects_ready=false;
        volumetric.release_device();volumetric_ready=false;
        images_ready=rays_ready=history_ready=globals_ready=bloom_ready=exposure_ready=depth_ready=aa_ready=false;
        extent=next_extent;sample_factor=factor;fsr_mode=mode;dlss_mode=sdk_mode;dlss_model=sdk_model;return true;
    }
    std::shared_ptr<const CalibratedGameFrame> frame;
    bool srgb{},retiring_native{},rays_ready{};
    SDL_GPUTextureFormat format{};
    bool native_complete() const noexcept {
        for(unsigned e=0;e<2;++e) if(!shadow[e].native_work_complete() || !reflection[e].native_work_complete()) return false;
        return !msaa || msaa->native_complete();
    }
    bool prepare_rays() {
        if(rays_ready) return true;
        if(!composite.initialize(device,format)) return false;
        for(unsigned e=0;e<2;++e) {
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=format;
            info.width=scene_extent(e)[0];info.height=scene_extent(e)[1];info.layer_count_or_depth=info.num_levels=1;
            info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            if(!base[e]) base[e]=SDL_CreateGPUTexture(device,&info);
            if(!base[e]) return false;
            info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            if(!receiver[e]) receiver[e]=SDL_CreateGPUTexture(device,&info);
            if(!receiver[e]) return false;
        }
        rays_ready=true;return true;
    }
    bool prepare_images() {
        if(images_ready) return true;
        for(unsigned eye=0;eye<2;++eye) {
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=format;
            info.width=scene_extent(eye)[0];info.height=scene_extent(eye)[1];info.layer_count_or_depth=info.num_levels=1;
            info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            if(!temporal_input[eye]) temporal_input[eye]=SDL_CreateGPUTexture(device,&info);
            if(!temporal_middle[eye]) temporal_middle[eye]=SDL_CreateGPUTexture(device,&info);
            if(!temporal_input[eye] || !temporal_middle[eye]) return false;
        }
        images_ready=true;return true;
    }
    bool prepare_history() {
        if(history_ready) return true;
        if(!prepare_images()) return false;
        for(auto& history:persistence) if(!history.initialize(device,format)) return false;
        history_ready=true;return true;
    }
    bool prepare_global() {
        if(globals_ready) return true;
        if(!prepare_images() || !global.initialize(device,format)) return false;
        globals_ready=true;return true;
    }
    bool prepare_bloom() {
        if(bloom_ready) return true;
        if(!prepare_images() || !bloom.initialize(device,format)) return false;
        bloom_ready=true;return true;
    }
    bool prepare_exposure() {
        if(exposure_ready) return true;
        if(!prepare_images()) return false;
        for(auto& meter:exposure) if(!meter.initialize(device,format)) return false;
        exposure_ready=true;return true;
    }
    bool prepare_depth() {
        if(depth_ready) return true;
        if(!prepare_images() || !depth_effect.initialize(device,format)) return false;
        if(!SDL_GPUTextureSupportsFormat(device,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER)) return false;
        for(unsigned e=0;e<2;++e) {
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
            info.width=scene_extent(e)[0];info.height=scene_extent(e)[1];info.layer_count_or_depth=info.num_levels=1;
            info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            if(!surfaces[e]) surfaces[e]=SDL_CreateGPUTexture(device,&info);
            if(!surfaces[e]) return false;
        }
        depth_ready=true;return true;
    }
    bool prepare_scene_effects() {
        if(scene_effects_ready) return true;
        if(!prepare_images() || !scene_effects.initialize(device,format)) return false;
        scene_effects_ready=true;return true;
    }
    bool prepare_water_guides(unsigned samples,bool fog) {
        const auto a=scene_extent(0),b=scene_extent(1);
        const auto pixels=std::uint64_t(a[0])*a[1]+std::uint64_t(b[0])*b[1];
        // Conservative combined eye/history/shutter/ray bound, not available
        // VRAM. Native+SDL layers are retained together; merged MRTs add 20 B.
        if(pixels*(512+40*samples+(fog?32:0))+64ULL*1024*1024>4ULL*1024*1024*1024) return false;
        if(!SDL_GPUTextureSupportsFormat(device,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER)) return false;
        for(unsigned eye=0;eye<2;++eye) {
            const auto size=scene_extent(eye);
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
            info.width=size[0];info.height=size[1];info.layer_count_or_depth=info.num_levels=1;
            info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            if(!water_receiver[eye]) water_receiver[eye]=SDL_CreateGPUTexture(device,&info);
            info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
            if(!water_surface[eye]) water_surface[eye]=SDL_CreateGPUTexture(device,&info);
            if(!water_receiver[eye] || !water_surface[eye]) return false;
        }
        return true;
    }
    bool prepare_volumetric() {
        if(volumetric_ready) return true;
        if(!prepare_images() || !volumetric.initialize(device,format)) return false;
        volumetric_ready=true;return true;
    }
    void settle_history(bool accepted) {
        for(auto& h:persistence) if(accepted) h.commit();else h.cancel();
        for(auto& meter:exposure) if(accepted) meter.commit();else meter.cancel();
        if(msaa) msaa->settle_history(accepted);
        if(taa) taa->settle(accepted);
        if(reflected) reflected->settle(accepted);
        if(dlss) dlss->settle(accepted);
        if(blur) blur->history.settle(accepted);
    }
    bool prepare_aa(bool scratch=true) {
        if(scratch && !prepare_images()) return false;
        if(aa_ready) return true;
        for(auto& eye:aa) if(!eye.initialize(device,format)) return false;
        aa_ready=true;return true;
    }
#include "displayxr_temporal_aa.inc"
#include "displayxr_reflection_history.inc"
#include "displayxr_msaa.inc"
#include "displayxr_reconstructed_post.inc"
    ~State() {
        scene.release_device();
        composite.release_device();
        global.release_device();
        bloom.release_device();
        depth_effect.release_device();
        scene_effects.release_device();
        volumetric.release_device();
        for(auto& eye:aa) eye.release_device();
        for(auto& eye:fsr) eye.release_device();
        for(auto& meter:exposure) meter.release_device();
        for(auto& h:persistence) h.release_device();
        for(unsigned e=0;e<2;++e) {shadow[e].release_device();reflection[e].release_device();}
        if(device) for(unsigned eye=0;eye<2;++eye) {
            if(color[eye]) SDL_ReleaseGPUTexture(device,color[eye]);
            if(depth[eye]) SDL_ReleaseGPUTexture(device,depth[eye]);
            if(base[eye]) SDL_ReleaseGPUTexture(device,base[eye]);
            if(receiver[eye]) SDL_ReleaseGPUTexture(device,receiver[eye]);
            if(surfaces[eye]) SDL_ReleaseGPUTexture(device,surfaces[eye]);
            if(water_receiver[eye]) SDL_ReleaseGPUTexture(device,water_receiver[eye]);
            if(water_surface[eye]) SDL_ReleaseGPUTexture(device,water_surface[eye]);
            if(temporal_input[eye]) SDL_ReleaseGPUTexture(device,temporal_input[eye]);
            if(temporal_middle[eye]) SDL_ReleaseGPUTexture(device,temporal_middle[eye]);
            if(supersampled[eye]) SDL_ReleaseGPUTexture(device,supersampled[eye]);
            if(native_ink[eye]) SDL_ReleaseGPUTexture(device,native_ink[eye]);
            if(native_receiver[eye]) SDL_ReleaseGPUTexture(device,native_receiver[eye]);
            if(native_depth[eye]) SDL_ReleaseGPUTexture(device,native_depth[eye]);
            if(fsr_color[eye]) SDL_ReleaseGPUTexture(device,fsr_color[eye]);
            if(fsr_ink[eye]) SDL_ReleaseGPUTexture(device,fsr_ink[eye]);
            if(fsr_receiver[eye]) SDL_ReleaseGPUTexture(device,fsr_receiver[eye]);
            if(fsr_depth[eye]) SDL_ReleaseGPUTexture(device,fsr_depth[eye]);
        }
    }
};
DisplayXrGameRenderer::DisplayXrGameRenderer()=default;
DisplayXrGameRenderer::~DisplayXrGameRenderer() {
    // As with the presenter, failed completion is not permission to free live
    // images. The owner must retain the device/runtime and retry close().
    if(!close()) (void)state_.release();
}
bool DisplayXrGameRenderer::close() noexcept {
    if(state_ && !state_->presenter.close()) {status_=state_->presenter.status();return false;}
    if(state_ && !state_->native_complete()) {state_->retiring_native=true;status_="Leia cleanup pending: native ray queue";return false;}
    if(state_ && !state_->finish_dlss_frame()) {status_=state_->dlss_frame_end_error;return false;}
    if(state_ && !state_->retire_dlss()) {status_="Leia cleanup pending: native DLSS viewport";return false;}
    state_.reset();return true;
}
bool DisplayXrGameRenderer::try_close() noexcept {
    if(state_ && !state_->presenter.try_close()) {status_=state_->presenter.status();return false;}
    if(state_ && !state_->native_complete()) {state_->retiring_native=true;status_="Leia cleanup pending: native ray queue";return false;}
    if(state_ && !state_->finish_dlss_frame()) {status_=state_->dlss_frame_end_error;return false;}
    if(state_ && !state_->retire_dlss()) {status_="Leia cleanup pending: native DLSS viewport";return false;}
    state_.reset();return true;
}
bool DisplayXrGameRenderer::initialize(const DisplayXrRuntime& runtime,SDL_GPUDevice* device,void* window,
    DisplayXrVulkanBinding* vulkan_creation,CalibratedDlssApi dlss) {
    if(!close()) return false;
    auto next=std::make_unique<State>();next->device=device;next->dlss_api=dlss;
    try {
        const auto check=[](bool ok,const char* error) {if(!ok) throw std::runtime_error(error);};
        check(device && window,"Leia game renderer requires the actual panel window and GPU");
        check(next->presenter.initialize(runtime,device,window,vulkan_creation),next->presenter.status().c_str());
        const auto format=static_cast<SDL_GPUTextureFormat>(next->presenter.color_format());
        next->format=format;
        next->srgb=format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        check(next->scene.initialize(device,format),next->scene.status().c_str());
        for(unsigned eye=0;eye<2;++eye) {
            next->output_extent[eye]=next->extent[eye]=next->presenter.eye_extent(eye);
            const auto size=next->extent[eye];check(size[0] && size[1],"Leia eye extent is empty");
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=format;
            info.width=size[0];info.height=size[1];info.layer_count_or_depth=info.num_levels=1;
            info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            next->color[eye]=SDL_CreateGPUTexture(device,&info);check(next->color[eye],SDL_GetError());
            info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            next->depth[eye]=SDL_CreateGPUTexture(device,&info);check(next->depth[eye],SDL_GetError());
        }
        state_=std::move(next);status_="Leia native game renderer ready";return true;
    } catch(const std::exception& error) {
        status_=error.what();
        if(!next->presenter.close()) state_=std::move(next);
        return false;
    }
}
bool DisplayXrGameRenderer::running() const noexcept {return state_ && state_->presenter.running();}
bool DisplayXrGameRenderer::exit_requested() const noexcept {return state_ && state_->presenter.exit_requested();}
bool DisplayXrGameRenderer::frame_pending() const noexcept {return state_ && (state_->presenter.frame_pending() || state_->retiring_native);}
std::uint64_t DisplayXrGameRenderer::presented_frames() const noexcept {return state_?state_->presenter.presented_frames():0;}
bool DisplayXrGameRenderer::srgb_target() const noexcept {return state_ && state_->srgb;}
std::array<std::uint32_t,2> DisplayXrGameRenderer::scene_extent(unsigned eye) const noexcept {
    return state_ && eye<2?state_->scene_extent(eye):std::array<std::uint32_t,2>{};
}
shadows::GpuReflectionOutput DisplayXrGameRenderer::accepted_reflection_sample(unsigned eye,unsigned sample) const noexcept {
    if(!state_ || frame_pending() || eye>=2 || !state_->reflected
        || sample>=state_->reflected->timeline.samples()) return {};
    const auto& bank=state_->reflected->resolve[eye][sample];
    return bank?bank->accepted_output():shadows::GpuReflectionOutput{};
}
GpuReflectionSourceAdmission DisplayXrGameRenderer::accepted_reflection_source_admission(unsigned eye,unsigned sample) const noexcept {
    if(!state_ || frame_pending() || !state_->source_validation || !state_->liquid_diagnostics
        || eye>=2 || !state_->reflected || sample>=state_->reflected->timeline.samples())return {};
    const auto& bank=state_->reflected->resolve[eye][sample];
    return bank?bank->accepted_source_admission():GpuReflectionSourceAdmission{};
}
std::uint64_t DisplayXrGameRenderer::reflection_history_resident_bytes() const noexcept {
    if(!state_ || !state_->liquid_diagnostics || !state_->reflected) return 0;
    std::uint64_t bytes=0;
    for(const auto& eye:state_->reflected->resolve) for(const auto& bank:eye)
        if(bank) bytes+=bank->working_image_bytes();
    return bytes;
}
bool DisplayXrGameRenderer::enable_reflection_diagnostics(bool enabled) {
    if(!state_ || frame_pending()) return false;
    if(enabled && !state_->liquid_diagnostics) state_->liquid_diagnostics=std::make_unique<State::LiquidDiagnostics>();
    if(!enabled) {
        if(state_->diagnostic_sample_offset!=std::array<float,2>{} && state_->dlss) state_->dlss->accepted.reset();
        if(state_->curved_validation || state_->source_validation) state_->reflected.reset();
        state_->curved_validation=state_->source_validation=false;
        state_->source_stage_observer=nullptr;state_->source_stage_context=nullptr;
        state_->diagnostic_sample_offset={};state_->liquid_diagnostics.reset();
    }
    return true;
}
bool DisplayXrGameRenderer::enable_curved_reflection_validation(bool enabled) noexcept {
    if(!state_ || frame_pending()) return false;
    if(!state_->liquid_diagnostics) return !enabled;
    const auto* driver=SDL_GetGPUDeviceDriver(state_->device);
    if(enabled && (!driver || std::string_view(driver)!="direct3d12")) return false;
    if(state_->curved_validation!=enabled) {
        state_->reflected.reset();
        state_->curved_validation=enabled;
    }
    return true;
}
bool DisplayXrGameRenderer::enable_source_reflection_validation(bool enabled) noexcept {
    if(!state_ || frame_pending())return false;
    if(!state_->liquid_diagnostics)return !enabled;
#if !defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
    if(enabled)return false;
#endif
    const auto* driver=SDL_GetGPUDeviceDriver(state_->device);
    if(enabled && (!driver || (std::string_view(driver)!="direct3d12" && std::string_view(driver)!="vulkan")))return false;
    if(state_->source_validation!=enabled) {state_->reflected.reset();state_->source_validation=enabled;}
    return true;
}
bool DisplayXrGameRenderer::set_source_sample_diagnostic_offset(std::array<float,2> pixels) noexcept {
    if(!state_ || frame_pending() || !state_->liquid_diagnostics
        || !std::all_of(pixels.begin(),pixels.end(),[](float value){return std::isfinite(value) && std::abs(value)<=1;})) return false;
    if(pixels!=state_->diagnostic_sample_offset) {
        if(state_->dlss) state_->dlss->accepted.reset();
        state_->diagnostic_sample_offset=pixels;
    }
    return true;
}
bool DisplayXrGameRenderer::set_source_stage_observer(ReflectionSourceStageObserver observer,void* context) noexcept {
    if(!state_ || frame_pending() || (observer && !state_->liquid_diagnostics) || (!observer && context))return false;
    state_->source_stage_observer=observer;state_->source_stage_context=context;return true;
}
void* DisplayXrGameRenderer::source_sample_diagnostic_image(unsigned eye,unsigned sample) const noexcept {
    if(!state_ || frame_pending() || !state_->liquid_diagnostics || eye>=2
        || !state_->liquid_diagnostics->raster_cameras[eye]) return nullptr;
    if(state_->msaa) {
        if(sample>=state_->msaa->count || !state_->msaa->samples[eye][sample]) return nullptr;
        return state_->msaa->samples[eye][sample]->finished;
    }
    return sample==0?state_->scene_color(eye):nullptr;
}
std::optional<vr::EyeCamera> DisplayXrGameRenderer::source_raster_diagnostic_camera(unsigned eye) const noexcept {
    if(!state_ || frame_pending() || !state_->liquid_diagnostics || eye>=2) return {};
    return state_->liquid_diagnostics->raster_cameras[eye];
}
shadows::GpuReflectionOutput DisplayXrGameRenderer::current_reflection_sample(unsigned eye,unsigned sample) const noexcept {
    if(!state_ || frame_pending() || !state_->liquid_diagnostics || eye>=2) return {};
    if(state_->msaa) {
        if(sample>=state_->msaa->count || !state_->msaa->samples[eye][sample]) return {};
        return state_->msaa->samples[eye][sample]->reflection.reflection_output();
    }
    return sample==0?state_->reflection[eye].reflection_output():shadows::GpuReflectionOutput{};
}
ReflectionLiquidDiagnosticInputs DisplayXrGameRenderer::reflection_liquid_inputs(unsigned eye,unsigned sample) const noexcept {
    if(!state_ || frame_pending() || !state_->liquid_diagnostics || eye>=2 || sample>=8) return {};
    return (*state_->liquid_diagnostics)[eye][sample];
}
std::shared_ptr<const CalibratedGameFrame> DisplayXrGameRenderer::retained_frame() const noexcept {
    return state_?state_->frame:nullptr;
}
bool DisplayXrGameRenderer::poll_events() {
    if(!state_ || state_->retiring_native) return false;
    const bool ok=state_->presenter.poll_events();status_=state_->presenter.status();return ok;
}
DisplayXrSubmission DisplayXrGameRenderer::poll() {
    if(!state_ || !frame_pending()) {status_="No pending Leia game frame";return DisplayXrSubmission::failed;}
    const bool was_pending=state_->presenter.frame_pending();
    // The compositor may already have accepted the image while a native ray
    // producer is still retiring. Preserve that result on later polls rather
    // than turning a successfully presented frame into a spurious failure.
    auto outcome=was_pending?state_->presenter.continue_frame(state_->color[0],state_->color[1])
        :state_->history_accepted?DisplayXrSubmission::submitted:DisplayXrSubmission::failed;
    // Completing a cancelled producer fence is not a successful presentation.
    if(state_->presentation_failed && outcome!=DisplayXrSubmission::waiting) outcome=DisplayXrSubmission::failed;
    if(was_pending && outcome!=DisplayXrSubmission::waiting) state_->history_accepted=outcome==DisplayXrSubmission::submitted;
    status_=state_->presenter.status();
    if(!state_->presenter.frame_pending()) {
        if(!state_->native_complete()) {state_->retiring_native=true;status_="Leia native ray completion pending";return DisplayXrSubmission::waiting;}
        if(!state_->finish_dlss_frame()) {state_->retiring_native=true;status_=state_->dlss_frame_end_error;return DisplayXrSubmission::waiting;}
        state_->settle_history(state_->history_accepted);state_->retiring_native=false;state_->frame.reset();
    }
    return outcome;
}
DisplayXrSubmission DisplayXrGameRenderer::submit(std::shared_ptr<const CalibratedGameFrame> frame) {
    if(!state_ || !frame || !frame->current || !frame->previous || frame->settings.srgb!=state_->srgb || !state_->presenter.running()
        || frame_pending() || frame->settings.ray_tracing>3 || frame->settings.reflections>3 || frame->settings.shadow_softness>3
        || frame->settings.water_caustics>3
        || !valid_material(frame->settings.material) || frame->settings.model_effects[0]>=effect_count || frame->settings.world_effects[0]>=effect_count
        || frame->settings.model_effects[1]>100 || frame->settings.world_effects[1]>100
        || frame->settings.manipulation_intensity>100 || !valid_manipulation(frame->settings.manipulation)
        || !valid_manipulation(frame->settings.extra_effects[0]) || !valid_special_fx(frame->settings.extra_effects[1])
        || !valid_special_fx(frame->settings.extra_effects[2])
        || !std::isfinite(frame->settings.effect_seconds) || frame->settings.effect_seconds<0 || frame->settings.phosphor>3
        || frame->settings.bloom_model>3 || frame->settings.bloom_world>3 || frame->settings.contrast>3 || frame->settings.chromatic>3 || frame->settings.exposure>3
        || frame->settings.depth_enhancements>15 || frame->settings.volumetric_fog>3 || frame->settings.motion_blur>3
        || !std::isfinite(frame->presentation_seconds) || frame->presentation_seconds<0
        || frame->settings.scene_enhancements>255 || frame->settings.particle_enhancements>15
        || !valid_calibrated_scene_fx(*frame)
        || frame->settings.aa_type>6 || frame->settings.aa_quality>3 || frame->settings.fsr1_mode>4
        || frame->settings.dlss_mode>4 || frame->settings.dlss_model>1
        || (frame->settings.dlss_mode && (frame->settings.fsr1_mode || !state_->dlss_api.complete()))
        || (frame->settings.aa_quality && !calibrated_aa_supported(frame->settings.aa_type))
        || (frame->settings.global_enhancements&~global_enhancement_mask)!=0
        || (frame->settings.camera_response_modes&~63U)
        || !camera_response_world_transform({frame->settings.camera_response_pose[0],frame->settings.camera_response_pose[1],frame->settings.camera_response_pose[2]}))
        {status_="Leia game renderer is not ready for a new source frame";return DisplayXrSubmission::failed;}
    SDL_GPUCommandBuffer* command{};
    bool native_encoded=false;
    if(state_->liquid_diagnostics) *state_->liquid_diagnostics={};
    try {
        const auto check=[](bool ok,const char* error) {if(!ok) throw std::runtime_error(error);};
        const auto calibrated=state_->presenter.begin_frame(calibrated_game_rig(frame->settings.convergence_source_units));
        check(calibrated.has_value(),state_->presenter.status().c_str());
        if(!calibrated->calibrated.xr.should_render) {
            status_=state_->presenter.status();return DisplayXrSubmission::submitted;
        }
        const bool supersample=frame->settings.aa_type==3 && frame->settings.aa_quality;
        const bool multisample=frame->settings.aa_type==6 && frame->settings.aa_quality;
        const bool sdk_aa=frame->settings.dlss_mode!=0;
        // DLSS owns temporal reconstruction; never run a second TAA history on
        // top. Spatial AA and genuine SSAA/MSAA remain separately honoured.
        const bool temporal_aa=!sdk_aa && frame->settings.aa_type==4 && frame->settings.aa_quality;
        // No pending presenter/native producer remains at this boundary.
        check(state_->prepare_samples(supersample?frame->settings.aa_quality+1:1,frame->settings.fsr1_mode,
            frame->settings.dlss_mode,frame->settings.dlss_model),"Native scene targets could not initialize");
        const bool upscaling=state_->fsr_mode!=0 || sdk_aa;
        if(state_->fsr_mode) check(state_->prepare_fsr(),"Native FSR pipelines could not initialize");
        if(frame->settings.volumetric_fog) {
            const unsigned factor=supersample?frame->settings.aa_quality+1:1;
            std::uint64_t pixels=0;
            for(const auto size:state_->extent) {
                const auto count=std::uint64_t(size[0])*size[1]*factor*factor;
                check(std::uint64_t(size[0])*factor<=16384 && std::uint64_t(size[1])*factor<=16384
                    && count<=512ULL*1024*1024/16,"Native fog exceeds its per-eye integral working limit");
                pixels+=count;
            }
            if(!multisample && !temporal_aa) check(pixels*336+32ULL*1024*1024<=4ULL*1024*1024*1024,
                "Native fog exceeds the four-GiB resident eye/history working bound");
        } else if(state_->volumetric_ready) {
            state_->volumetric.release_device();state_->volumetric_ready=false;
        }
        // No pending presenter/native producer remains at this boundary.
        if(!multisample) state_->msaa.reset();
        if(!temporal_aa) state_->taa.reset();
        state_->frame=std::move(frame);
        state_->history_accepted=false;
        state_->presentation_failed=false;
        command=SDL_AcquireGPUCommandBuffer(state_->device);check(command,SDL_GetError());
        auto packets=state_->frame->packets();
        const auto& settings=state_->frame->settings;
        if(temporal_aa) state_->prepare_taa(calibrated->cameras);
        if(sdk_aa) state_->prepare_dlss(calibrated->cameras);
        const auto jitter=temporal_aa?state_->taa->pending_jitter:sdk_aa?state_->dlss->pending_jitter:std::array<float,2>{};
        const unsigned raster_samples=multisample?1U<<settings.aa_quality:1;
        if(!settings.motion_blur) state_->blur.reset();
        else {
            if(state_->blur && state_->blur->ready && (state_->blur->count!=raster_samples || state_->blur->factor!=state_->sample_factor)) state_->blur.reset();
            if(!state_->blur) state_->blur=std::make_unique<State::BlurImages>(state_->device);
            check(state_->blur->history.prepare(state_->frame,calibrated->cameras,jitter),
                "Invalid native motion-blur presentation time");
        }
        const bool physical_blur=state_->blur && state_->blur->history.interval()>0;
        const bool shadows_enabled=state_->frame->current->dots_mode>=0;
        const bool water_enabled=std::any_of(packets.begin(),packets.end(),[](const auto& p){
            return p.ground_receiver && calibrated_ground_material(*p.packet)==5;
        });
        const bool lava_enabled=std::any_of(packets.begin(),packets.end(),[](const auto& p){
            return p.ground_receiver && calibrated_ground_material(*p.packet)==9;
        });
        const bool rays=state_->frame->settings.ray_tracing && (shadows_enabled || state_->frame->settings.reflections || water_enabled || lava_enabled)
            && std::any_of(packets.begin(),packets.end(),
            [](const auto& p){return p.ground_receiver || (p.ray_caster
                && (!p.packet->geometry.vertex_view().empty() || !p.packet->geometry.line_view().empty()));});
        const auto ray_material=static_cast<Effect>(settings.material);
        const auto ground_count=std::count_if(packets.begin(),packets.end(),[](const auto& p){return p.ground_receiver;});
        const bool ground_history=ground_count==1 && std::all_of(packets.begin(),packets.end(),[](const auto& p){
            const auto material=p.ground_receiver?calibrated_ground_material(*p.packet):0;
            return !p.ground_receiver || material==6 || material==7;
        }) && std::any_of(packets.begin(),packets.end(),[](const auto& p){return p.ray_caster
            && (!p.packet->geometry.vertex_view().empty() || !p.packet->geometry.line_view().empty());});
        const bool liquid_history=ground_count==1 && (water_enabled || lava_enabled)
            && std::any_of(packets.begin(),packets.end(),[](const auto& p){return p.ray_caster
                && (!p.packet->geometry.vertex_view().empty() || !p.packet->geometry.line_view().empty());});
        const bool curved_history=liquid_history && state_->liquid_diagnostics
            && (state_->curved_validation || state_->source_validation);
        const bool path_history=!ground_count || ground_history || curved_history;
        const float path_roughness=ray_material==Effect::off?0:material_roughness(ray_material);
        // Each native sample retains its own optical receiver seed, accepted
        // projection and RGB bank. Curved water/lava solve the real old waves;
        // CURRENT material, emission and primary coverage remain unfiltered.
        bool secondary_history=rays && settings.reflections && !physical_blur
            && (path_history || ((ray_material==Effect::off || material_roughness(ray_material)==0)
                && !conductor(ray_material) && !reflective_material(ray_material)))
            && (!ground_count || ground_history || liquid_history);
        if(secondary_history) {
            auto optical_cameras=calibrated->cameras;
            for(unsigned eye=0;eye<2;++eye) optical_cameras[eye]=sdk_aa
                ?state_->dlss_camera(optical_cameras[eye],eye):state_->diagnostic_source_camera(optical_cameras[eye],eye);
            // Retain the EXACT SDK source projection, not jitter reconstructed
            // in a different-sized SSAA raster. SDK primary correspondence is
            // still separate; no primary flow certifies secondary radiance.
            secondary_history=state_->prepare_reflection_history(optical_cameras,
                sdk_aa?std::array<float,2>{}:jitter,
                curved_history?(water_enabled?(path_roughness>0?576:128):(path_roughness>0?556:108))
                :path_history?(ground_history?(path_roughness>0?460:96):(path_roughness>0?444:80)):liquid_history?108:52,raster_samples);
        }
        if(state_->source_validation)check(secondary_history && path_history,
            "Complete source-reflection validation requires every native owner and an ordered path layout; no silent partial bypass");
        if(!secondary_history) state_->reflected.reset();
        const bool reflected_history=secondary_history && !state_->reflected->timeline.held();
        const bool trace_reflections=rays && (settings.reflections || water_enabled || lava_enabled) && (!secondary_history || reflected_history);
        // Identical accepted secondary RGB needs neither another trace nor six
        // environment captures. Shadows/fog still retain their live geometry;
        // never freeze them merely because the reflection image is held.
        const bool trace_models=rays && (shadows_enabled || trace_reflections);
        const auto post=calibrated_game_post_effects(settings);
        const State::PassPlan plan(settings,settings.aa_quality && !supersample && !multisample
            && !(sdk_aa && settings.aa_type==4),state_->frame->scene_fx.active(),physical_blur,
            (temporal_aa || sdk_aa) && rays && water_enabled);
        const bool sdk_reconstructed_post=sdk_aa && (plan.reconstruct_first(post)
            || calibrated_sdk_post_after_reconstruction(post) || plan.temporal() || plan.exposure);
        const bool reconstructed_post=(temporal_aa && plan.reconstruct_first(post)) || sdk_reconstructed_post;
        if(reconstructed_post) for(auto& packet:packets) {
            // Reconstruct an unstyled radiance timeline, including colours
            // seen THROUGH or reflected BY the liquid. Raster receivers defer
            // these styles already, but ray materials and environment faces
            // also use the packet bindings. Leaving their palette style here
            // bakes it into optical radiance before the centre-grid post pass,
            // applying the same selected style twice to submerged receivers.
            if(packet.effects_override && calibrated_composite_effect((*packet.effects_override)[0]))
                (*packet.effects_override)[0]=0;
        }
        auto input_plan=plan;
        if(sdk_reconstructed_post) {
            // Spatial AA remains on the genuine source raster. All authored
            // styles/finishing/history run once AFTER opaque reconstruction.
            input_plan.model_history=input_plan.world_history=input_plan.phosphor=input_plan.exposure=false;
            input_plan.globals=input_plan.appearance=input_plan.bloom=input_plan.depth=input_plan.fx=input_plan.fog=false;
        }
        const bool final_passes=input_plan.finishing();
        auto signature=settings;signature.effect_seconds=0;signature.exposure_paused=false;signature.camera_response_pose={};
        const auto& tick=*state_->frame->current;
        if(!state_->history_settings || *state_->history_settings!=signature || state_->history_revision!=tick.scene_epoch
            || state_->history_flow!=unsigned(tick.flow)
            || timing::camera_transform_is_discontinuous(state_->history_camera,tick.camera)) ++state_->history_epoch;
        state_->history_settings=signature;state_->history_revision=tick.scene_epoch;
        state_->history_flow=unsigned(tick.flow);state_->history_camera=tick.camera;
        if(!plan.world_history) for(unsigned i=0;i<2;++i) state_->persistence[i].discard();
        if(!plan.model_history) for(unsigned i=2;i<4;++i) state_->persistence[i].discard();
        if(!plan.phosphor) for(unsigned i=4;i<6;++i) state_->persistence[i].discard();
        if(!plan.exposure) for(auto& meter:state_->exposure) meter.discard();
        const bool compose=upscaling || rays || plan.active() || supersample || multisample || std::any_of(post.begin(),post.end(),[](const auto& effect){return effect.active();});
        const bool liquid_guides=water_enabled && rays && (sdk_aa || plan.active() || supersample || multisample
            || std::any_of(post.begin(),post.end(),[](const auto& effect){return effect.active();}));
        state_->prepare_reconstructed_post(sdk_reconstructed_post,plan,raster_samples,rays,liquid_guides,rays && water_enabled);
        if(compose) check(state_->prepare_rays(),state_->composite.status().c_str());
        if(liquid_guides) check(state_->prepare_water_guides(raster_samples,plan.fog),"Native liquid guides exceed the resident working bound or could not initialize");
        if(physical_blur) state_->prepare_blur(raster_samples,water_enabled && rays);
        if(multisample) {
            auto allocation_plan=plan;
            if(sdk_reconstructed_post) allocation_plan.model_history=allocation_plan.world_history=allocation_plan.phosphor=allocation_plan.exposure=false;
            state_->prepare_msaa(1U<<settings.aa_quality,allocation_plan,rays,liquid_guides,sdk_reconstructed_post);
            if(final_passes) check(state_->prepare_images(),"Native MSAA scratch resources could not initialize");
        }
        if(!multisample || sdk_reconstructed_post) {
            if(plan.temporal()) check(state_->prepare_history(),"Native persistence resources could not initialize");
            if(plan.exposure) check(state_->prepare_exposure(),"Native adaptive exposure resources could not initialize");
        }
        if(plan.globals || plan.appearance) check(state_->prepare_global(),"Native global enhancement resources could not initialize");
        if(plan.bloom) check(state_->prepare_bloom(),"Native bloom resources could not initialize");
        // Both float targets must exist even for ownership-only MSAA styles:
        // each sample swaps the merged MRTs and reuses the former targets.
        if(plan.needs_surfaces() || temporal_aa || liquid_guides) check(state_->prepare_depth(),"Native effect depth guides could not initialize");
        if(plan.fx) check(state_->prepare_scene_effects(),state_->scene_effects.status().c_str());
        if(plan.fog) check(state_->prepare_volumetric(),state_->volumetric.status().c_str());
        if((plan.aa && !temporal_aa) || supersample) check(state_->prepare_aa(!supersample),"Native AA resources could not initialize");
        check(state_->scene.upload_packets(command,packets),state_->scene.status().c_str());
        const auto token=state_->scene.upload_token();
        std::array<CalibratedRayGeometryOutput,2> geometry;
        state_->fog_geometry={};
        // Entity/primitive correspondence does not depend on the eye. Keep
        // each accepted timeline distinct (SDK versus physical blur/TAA), but
        // prepare its immutable source matches once for the whole eye pair.
        // No preparation is retained across image waits, cancellation or a
        // different frame; only resident per-eye guides become accepted later.
        std::optional<CalibratedGameMotionPreparation> sdk_motion,temporal_motion,reflected_motion;
        if(reflected_history) reflected_motion.emplace(*state_->frame,state_->reflected->timeline.previous(),packets);
        if(sdk_aa && !state_->dlss->held)
            sdk_motion.emplace(*state_->frame,state_->dlss->reuse?state_->dlss->accepted.get():nullptr,packets);
        if(temporal_aa || physical_blur) {
            // Actual secondary RGB is excluded locally by its ray/receiver
            // witness. Keep dry geometry's real motion, even with reflections.
            // Deferred finishing cannot contaminate raw geometric history.
            const bool reactive=!reconstructed_post && (plan.fx || plan.fog);
            temporal_motion.emplace(*state_->frame,physical_blur?state_->blur->history.accepted().get():
                !reactive && state_->taa->pending_epoch==state_->taa->accepted_epoch?state_->taa->accepted.get():nullptr,packets);
        }
        for(unsigned eye=0;eye<2;++eye) {
            const auto size=state_->scene_extent(eye);
            const auto before=upscaling?CalibratedScenePhase::upscale_scene:CalibratedScenePhase::before_rays;
            if(upscaling) {
                const auto full=state_->output_extent[eye];
                check(state_->scene.enqueue_eye(command,state_->fsr_ink[eye],state_->fsr_depth[eye],full[0],full[1],
                    calibrated->cameras[eye],state_->frame->clear,CalibratedScenePhase::before_rays,state_->fsr_receiver[eye],0,
                    sdk_reconstructed_post?state_->reconstructed_post->surface[eye]:nullptr,true),
                    state_->scene.status().c_str());
            }
            const auto camera=temporal_aa?state_->taa_camera(calibrated->cameras[eye],eye)
                :sdk_aa?state_->dlss_camera(calibrated->cameras[eye],eye):state_->diagnostic_source_camera(calibrated->cameras[eye],eye);
            if(state_->liquid_diagnostics) state_->liquid_diagnostics->raster_cameras[eye]=camera;
            if(sdk_aa && !state_->dlss->held) {
                // Centre input-grid correspondence is independent of SSAA/MSAA
                // sample positions and of later colour-only post distortions.
                const auto input=state_->extent[eye];
                const auto old=state_->dlss->reuse?state_->dlss_camera(state_->dlss->accepted_cameras[eye],eye,true):camera;
                auto draws=sdk_motion->draws(camera,old);
                const CalibratedSceneMotion guides{old,draws,input[0],input[1]};
                check(state_->scene.enqueue_eye(command,state_->dlss->guide_color[eye],state_->dlss->depth[eye],input[0],input[1],
                    camera,state_->frame->clear,before,state_->dlss->ownership[eye],0,state_->dlss->surface[eye],false,{},
                    state_->dlss->motion[eye],&guides),state_->scene.status().c_str());
            }
            std::vector<CalibratedSceneMotionDraw> motion_draws;CalibratedSceneMotion motion{};
            if(temporal_aa || physical_blur) {
                auto old=physical_blur?state_->blur->history.cameras()[eye]
                    :state_->taa->accepted?state_->taa_camera(state_->taa->accepted_cameras[eye],eye,true):camera;
                if(physical_blur) {
                    old.projection[8]-=2*state_->blur->history.jitter()[0]/size[0];
                    old.projection[9]+=2*state_->blur->history.jitter()[1]/size[1];
                }
                // Liquid floors already have invalid local correspondence;
                // actual traced water and untracked post layers also exclude
                // their samples from history in the resolve. Keep real dry
                // model/floor guides instead of discarding the whole eye.
                // Untracked scene FX/volume retain their conservative guard;
                // reflected RGB is excluded by actual local ray consumers.
                motion_draws=temporal_motion->draws(camera,old);
                motion={old,motion_draws,size[0],size[1]};
            }
            auto* motion_target=temporal_aa?state_->taa->motion[eye]:physical_blur?state_->blur->motion[eye]:nullptr;
            if(multisample) {
                const bool ok=state_->scene.enqueue_eye(command,state_->msaa->color[eye],state_->msaa->depth[eye],size[0],size[1],
                    camera,state_->frame->clear,before,state_->msaa->receiver[eye],0,
                    plan.needs_surfaces()?state_->msaa->surface[eye]:nullptr,false,{state_->msaa->count,nullptr,true},
                    physical_blur?state_->msaa->motion[eye]:nullptr,physical_blur?&motion:nullptr);
                check(ok,state_->scene.status().c_str());
            } else check(state_->scene.enqueue_eye(command,compose?state_->base[eye]:state_->scene_color(eye),state_->depth[eye],size[0],size[1],
                camera,state_->frame->clear,compose?before:CalibratedScenePhase::all,
                compose?state_->receiver[eye]:nullptr,0,(plan.needs_surfaces() || temporal_aa)?state_->surfaces[eye]:nullptr,plan.aa,{},
                motion_target,(temporal_aa || physical_blur)?&motion:nullptr),state_->scene.status().c_str());
            if(physical_blur) check(state_->scene.enqueue_eye(command,state_->blur->world[eye],state_->blur->world_depth[eye],size[0],size[1],
                camera,state_->frame->clear,upscaling?CalibratedScenePhase::upscale_world:CalibratedScenePhase::world_only,state_->blur->world_receiver[eye],0,nullptr,false,
                {raster_samples,nullptr,multisample}),state_->scene.status().c_str());
            if(trace_models || plan.fog) {
                const auto cube_size=trace_reflections && settings.reflections?32U<<settings.reflections:0U;
                std::vector<CalibratedSceneMotionDraw> reflected_draws;CalibratedSceneMotion reflected_guides{};
                if(reflected_history) {
                    const auto old=state_->reflected->timeline.previous()?state_->reflection_previous_camera(eye):camera;
                    const auto old_size=state_->reflected->timeline.previous()?state_->reflected->timeline.extents()[eye]:size;
                    reflected_draws=reflected_motion->draws(camera,old);
                    reflected_guides={old,reflected_draws,old_size[0],old_size[1]};
                }
                geometry[eye]=state_->scene.enqueue_ray_geometry(command,eye,size[0],size[1],camera,256,
                    cube_size,state_->frame->clear,reflected_history?&reflected_guides:nullptr);
                check(geometry[eye].complete,state_->scene.status().c_str());
                if(plan.fog) state_->fog_geometry[eye]=geometry[eye];
            }
            if(supersample || multisample || temporal_aa) {
                const auto native=state_->extent[eye];
                check(state_->scene.enqueue_eye(command,state_->native_ink[eye],state_->native_depth[eye],native[0],native[1],
                    calibrated->cameras[eye],state_->frame->clear,before,state_->native_receiver[eye],0,
                    temporal_aa?state_->taa->centre_surface[eye]:nullptr,temporal_aa),
                    state_->scene.status().c_str());
            }
        }
        auto* submitted=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;
        state_->presenter.retain_scene_submission(submitted);check(submitted,SDL_GetError());
        check(state_->scene.notify_submitted(token),"Leia source upload lost its submitted ownership");
        std::array<shadows::GpuShadowOutput,2> masks;
        std::array<shadows::GpuReflectionOutput,2> reflections;
        std::array<std::array<std::optional<CalibratedReflectionLobeGeometry>,8>,2> lobe_inputs;
        std::array<bool,2> curved_old_liquid{};
        std::array<std::optional<CalibratedGroundRayReceiver>,2> ground_receivers;
        std::array<std::array<shadows::GpuShadowOutput,8>,2> sample_masks;
        std::array<std::array<shadows::GpuReflectionOutput,8>,2> sample_reflections;
        if(trace_models) {
            const auto material=static_cast<Effect>(settings.material);
            const std::array<std::uint32_t,256> unused_palette{};
            // Misses sample this eye's GPU-captured native world scenery, never
            // a flat image containing HUD, portraits or foreground models.
            // Clear remains the fallback where source scenery has no coverage.
            std::uint32_t environment=0xff000000U;
            for(unsigned c=0;c<3;++c) {
                double value=state_->frame->clear[c];
                if(state_->srgb) value=value<=.0031308?value*12.92:1.055*std::pow(value,1/2.4)-.055;
                environment|=std::uint32_t(std::lround(std::clamp(value,0.,1.)*255))<<(c*8);
            }
            native_encoded=true; // includes failed native encodes and their SDL copies
            for(unsigned eye=0;eye<2;++eye) {
                const auto& g=geometry[eye];
                auto& ground=ground_receivers[eye];
                for(const auto& p:packets) if(p.ground_receiver) {
                    check(!ground,"Multiple native reflective ground receivers");
                    ground=calibrated_ground_ray_receiver(*p.packet,calibrated->cameras[eye],float(settings.reflections)/3,
                        reflective_material(material) || conductor(material)!=0,p.model_override?&*p.model_override:nullptr,settings.water_caustics);
                    check(bool(ground),"Native reflective ground lost its retained plane");
                    ground->transport.auxiliary_layers=physical_blur && water_enabled;
                    ground->transport.surface_layers=liquid_guides;
                }
                const GpuScene::RayGeometryOutput resident{g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
                const shadows::PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
                const auto light=shadows_enabled?calibrated_game_light(temporal_aa?state_->taa_camera(calibrated->cameras[eye],eye):calibrated->cameras[eye],state_->frame->source_light,settings.camera_response_pose)
                    :std::optional<std::array<double,3>>{};
                check(!shadows_enabled || bool(light),"Invalid calibrated source light");
                const unsigned count=multisample?state_->msaa->count:1;
                for(unsigned sample=0;sample<count;++sample) {
                    shadows::Camera camera{g.width,g.height,g.projection[0],g.projection[2],g.projection[3],g.projection[1],
                        settings.ray_tracing,settings.shadow_softness};
                    if(multisample) {
                        // Geometry and hardware samples were rasterized with
                        // the SDK source jitter. Add the sample offset to THAT
                        // camera, not the unjittered physical panel camera.
                        const auto source_camera=sdk_aa?state_->dlss_camera(calibrated->cameras[eye],eye):state_->diagnostic_source_camera(calibrated->cameras[eye],eye);
                        const auto shifted=calibrated_msaa_sample_camera(source_camera,g.width,g.height,count,sample);
                        check(bool(shifted),"Invalid native MSAA ray sample");
                        camera.center_x=g.width*(1.0-shifted->projection[8])/2;
                        camera.center_y=g.height*(1.0+shifted->projection[9])/2;
                    }
                    auto& shadow=multisample?state_->msaa->samples[eye][sample]->shadow:state_->shadow[eye];
                    auto& reflection=multisample?state_->msaa->samples[eye][sample]->reflection:state_->reflection[eye];
                    // An empty analytic world has no model shadow casters. Its
                    // liquid/metal pass can run without a redundant shadow trace.
                    if(shadows_enabled && g.vertex_count) {
                        const bool ok=shadow.render_resident(state_->device,{},camera,{(*light)[0],(*light)[1],(*light)[2]},{},&resident,false,range);
                        check(ok,shadow.status().c_str());
                        if(multisample) sample_masks[eye][sample]=shadow.output();else masks[eye]=shadow.output();
                    }
                    if(trace_reflections) {
                        shadows::RayReflectionHistory history{g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                            g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset};
                        if(reflected_history && multisample) {
                            // Vertices/primitive IDs share the centre view
                            // transform, but each accepted native sample owns
                            // its EXACT old projection. A centre guide would
                            // read another sample's footprint at subpixel edges.
                            const auto source_camera=sdk_aa?state_->dlss_camera(calibrated->cameras[eye],eye):state_->diagnostic_source_camera(calibrated->cameras[eye],eye);
                            const auto shifted=calibrated_msaa_sample_camera(source_camera,g.width,g.height,count,sample);
                            check(bool(shifted),"Invalid first native reflection sample camera");
                            const auto old=state_->reflected->timeline.previous()
                                ?state_->reflection_previous_camera(eye,sample):*shifted;
                            history.projection={double(history.extent[0])*old.projection[0]/2.,double(history.extent[1])*old.projection[5]/2.,
                                history.extent[0]*(1.-old.projection[8])/2.,history.extent[1]*(1.+old.projection[9])/2.};
                        }
                        if(reflected_history && liquid_history && !curved_history) {
                            history.separated=true;history.previous_liquid=state_->reflection_previous_liquid(eye);
                        } else if(reflected_history && path_history) {
                            history.separated=true;history.model_lobes=path_roughness>0?8:1;history.model_paths=true;
                            history.scene_paths=ground_history;
                            if(ground_history) history.previous_ground=state_->reflection_previous_ground(eye);
                            const auto bytes=std::uint64_t(g.previous_index_offset)+std::uint64_t(g.vertex_count/3)*4;
                            check(bytes<=UINT32_MAX,"Ordered reflection correspondence exceeds its address space");
                            const auto current_cube=calibrated_environment_cameras(calibrated->cameras[eye]);
                            const auto previous_cube=calibrated_environment_cameras(state_->reflected->timeline.previous()
                                ?state_->reflection_previous_camera(eye):calibrated->cameras[eye]);
                            check(current_cube && previous_cube,"Ordered reflection lost a rigid environment camera");
                            lobe_inputs[eye][sample]=CalibratedReflectionLobeGeometry{g.buffer,std::uint32_t(bytes),g.vertex_count,history,
                                path_roughness,current_cube->ray_to_cube,previous_cube->ray_to_cube};
                            lobe_inputs[eye][sample]->current_projection={camera.focal_length,camera.vertical_focal_length(),camera.center_x,camera.center_y};
                            lobe_inputs[eye][sample]->current_clip={range.near_depth,range.far_depth};
                            if(ground_history) lobe_inputs[eye][sample]->current_ground=shadows::RayReflectionGround{
                                {ground->plane.point.x,ground->plane.point.y,ground->plane.point.z},
                                {ground->plane.normal.x,ground->plane.normal.y,ground->plane.normal.z}};
                            if(curved_history) {
                                check(bool(ground),"Ordered liquid history lost its actual receiver");
                                shadows::RayReflectionLiquid liquid;
                                liquid.ground={{ground->plane.point.x,ground->plane.point.y,ground->plane.point.z},
                                    {ground->plane.normal.x,ground->plane.normal.y,ground->plane.normal.z}};
                                std::copy(ground->transport.world_to_view.begin(),ground->transport.world_to_view.end(),liquid.world_to_view.begin());
                                std::copy(ground->transport.camera_position.begin(),ground->transport.camera_position.end(),liquid.offset.begin());
                                liquid.time=ground->transport.time;liquid.material=ground->transport.material;
                                const auto previous=state_->reflection_previous_liquid(eye);
                                curved_old_liquid[eye]=bool(previous);
                                // Cold/incompatible old receivers still produce complete CURRENT
                                // records. Weight zero below prevents invented old calibration
                                // from certifying reuse, without mutating accepted banks before
                                // the whole presentation succeeds.
                                history.curved_paths=history.curved_receivers=true;
                                history.previous_liquid=previous.value_or(liquid);
                                auto& input=*lobe_inputs[eye][sample];input.history=history;
                                input.current_liquid=liquid;
                                input.current_projection={camera.focal_length,camera.vertical_focal_length(),camera.center_x,camera.center_y};
                                input.current_clip={range.near_depth,range.far_depth};
                                check(input.valid(),"Ordered liquid owner lost its current/accepted optical calibration");
                            }
                        }
                        const bool ok=reflection.render_reflections(state_->device,camera,resident,unused_palette,environment,
                            material==Effect::off?0:material_roughness(material),conductor(material),{},g.environment_face_size,g.environment_rotation,nullptr,
                            ground?std::optional(ground->plane):std::nullopt,0,ground?&ground->transport:nullptr,false,range,
                            g.environment_offset,state_->srgb?2U:1U,reflective_material(material) || conductor(material)!=0,
                            reflected_history?&history:nullptr);
                        check(ok,reflection.status().c_str());
                        if(state_->liquid_diagnostics && reflected_history && history.previous_liquid) {
                            auto& diagnostic=(*state_->liquid_diagnostics)[eye][sample];
                            diagnostic={g.device,g.buffer,g.previous_vertex_offset,g.vertex_count,
                                shadows::reflection_liquid_frame_words(history)};
                            if(lobe_inputs[eye][sample] && lobe_inputs[eye][sample]->current_liquid) {
                                const auto& input=*lobe_inputs[eye][sample];
                                diagnostic.current_cube=input.current_cube;diagnostic.previous_cube=input.previous_cube;
                                for(unsigned r=0;r<3;++r) {
                                    for(unsigned c=0;c<3;++c)
                                        diagnostic.current_liquid_rotation[r*4+c]=float(input.current_liquid->world_to_view[r*3+c]);
                                    diagnostic.current_liquid_rotation[r*4+3]=float(input.current_liquid->offset[r]);
                                }
                                for(unsigned c=0;c<4;++c)diagnostic.current_projection[c]=float(input.current_projection[c]);
                            }
                        }
                        if(multisample) sample_reflections[eye][sample]=reflection.reflection_output();else reflections[eye]=reflection.reflection_output();
                    }
                }
            }
        }
        if(compose) {
            command=SDL_AcquireGPUCommandBuffer(state_->device);check(command,SDL_GetError());
            for(unsigned eye=0;eye<2;++eye) {
                const auto size=state_->scene_extent(eye);
                if(temporal_aa && liquid_guides) {
                    // Reuse the existing spare liquid MRTs. After this centre
                    // encode, their former centre targets become the source
                    // merge's spare pair below; all three same-sized owned
                    // pairs remain distinct even on cancellation or retry.
                    check(size==state_->extent[eye] && ground_receivers[eye]
                        && ground_receivers[eye]->transport.material==0,"Invalid native TAA liquid centre layout/plane");
                    const bool ok=state_->composite.enqueue_water_panel_guides(command,state_->native_receiver[eye],
                        state_->taa->centre_surface[eye],state_->water_receiver[eye],state_->water_surface[eye],
                        size[0],size[1],calibrated->cameras[eye],*ground_receivers[eye]);
                    check(ok,state_->composite.status().c_str());
                    std::swap(state_->native_receiver[eye],state_->water_receiver[eye]);
                    std::swap(state_->taa->centre_surface[eye],state_->water_surface[eye]);
                }
                if(temporal_aa && !reconstructed_post) {
                    const bool ok=state_->composite.enqueue_sequence(command,state_->native_ink[eye],state_->native_receiver[eye],
                        state_->taa->centre_styled[eye],size[0],size[1],nullptr,nullptr,1,0,0,
                        std::max(1U,unsigned(std::lround(double(size[1])/224))),post,&state_->taa->centre_edge_witness[eye],2+eye);
                    check(ok,state_->composite.status().c_str());
                    auto centre=plan;centre.aa=centre.world_history=centre.model_history=centre.phosphor=centre.exposure=false;
                    centre.blur=false; // Centre artwork has no joint shutter pass.
                    if(centre.active()) state_->finish_plane(command,eye,calibrated->cameras[eye],packets,centre,{},
                        state_->taa->centre_styled[eye],state_->native_ink[eye],state_->native_receiver[eye],state_->taa->centre_surface[eye]);
                }
                const unsigned count=multisample?state_->msaa->count:1;
                std::array<void*,8> finished{};
                for(unsigned sample=0;sample<count;++sample) {
                    auto* final_color=multisample?state_->msaa->samples[eye][sample]->finished:state_->scene_color(eye);
                    auto* styled=final_passes?state_->temporal_input[eye]:final_color;
                    auto camera=temporal_aa?state_->taa_camera(calibrated->cameras[eye],eye)
                        :sdk_aa?state_->dlss_camera(calibrated->cameras[eye],eye):state_->diagnostic_source_camera(calibrated->cameras[eye],eye);
                    if(multisample) {
                        const auto shifted=calibrated_msaa_sample_camera(camera,size[0],size[1],count,sample);
                        check(bool(shifted),"Invalid native MSAA effect sample");camera=*shifted;
                        const bool ok=state_->msaa->transport.enqueue_sample(command,state_->msaa->color[eye],state_->msaa->receiver[eye],
                            plan.needs_surfaces()?state_->msaa->surface[eye]:nullptr,state_->base[eye],state_->receiver[eye],
                            plan.needs_surfaces()?state_->surfaces[eye]:nullptr,size[0],size[1],count,sample,
                            physical_blur?state_->msaa->motion[eye]:nullptr,physical_blur?state_->blur->motion[eye]:nullptr);
                        check(ok,state_->msaa->transport.status().c_str());
                        if(physical_blur) check(state_->msaa->transport.enqueue_sample(command,state_->blur->world[eye],state_->blur->world_receiver[eye],
                            nullptr,state_->blur->sample_world[eye],state_->blur->sample_receiver[eye],nullptr,size[0],size[1],count,sample),
                            state_->msaa->transport.status().c_str());
                    }
                    const auto& mask=multisample?sample_masks[eye][sample]:masks[eye];
                    auto& reflection=multisample?sample_reflections[eye][sample]:reflections[eye];
                    if(reflected_history) {
                        // Resolve AFTER extracting this hardware sample's
                        // ownership, BEFORE any nonlinear material/style or
                        // multisample reconstruction. No centre coverage proxy.
                        auto& owner=*state_->reflected->resolve[eye][sample];
                        const auto& lobes=lobe_inputs[eye][sample];
                        if(curved_history && !multisample)
                            (*state_->liquid_diagnostics)[eye][sample].ownership=state_->receiver[eye];
                        check(owner.enqueue(command,reflection,state_->receiver[eye],
                            {state_->reflected->timeline.epoch(),curved_history && !curved_old_liquid[eye]?0.F:.85F,
                                state_->srgb,curved_history,state_->source_validation,state_->source_validation},lobes?&*lobes:nullptr),owner.status().c_str());
                        if(state_->source_validation) {
                            struct Observe {ReflectionSourceStageObserver callback;void* context;unsigned eye,sample;};
                            Observe observing{state_->source_stage_observer,state_->source_stage_context,eye,sample};
                            const auto forward=[](void* context,const ReflectionSourceStageEvent& input) {
                                const auto& observing=*static_cast<Observe*>(context);
                                auto event=input;event.eye=observing.eye;event.sample=observing.sample;
                                observing.callback(observing.context,event);
                            };
                            check(owner.enqueue_source_frame(command,nullptr,nullptr,
                                observing.callback?+forward:nullptr,observing.callback?&observing:nullptr),owner.status().c_str());
                        }
                        reflection=owner.output();
                    } else if(secondary_history) {
                        // Each held sample reads only its fully accepted RGB;
                        // do not retrace, reblend or advance any history bank.
                        reflection=state_->reflected->resolve[eye][sample]->accepted_output();
                        check(reflection.buffer,"Held native reflection has no accepted resident RGB");
                    }
                    if(liquid_guides) {
                        check(state_->composite.enqueue_water_guides(command,state_->receiver[eye],
                            (plan.needs_surfaces() || temporal_aa)?state_->surfaces[eye]:nullptr,
                            state_->water_receiver[eye],state_->water_surface[eye],size[0],size[1],reflection),
                            state_->composite.status().c_str());
                        // The next raster/sample overwrites these same-sized
                        // retained targets. All downstream styles/depth/AA use
                        // the liquid receiver now; no CPU guide copy or fence.
                        std::swap(state_->receiver[eye],state_->water_receiver[eye]);
                        std::swap(state_->surfaces[eye],state_->water_surface[eye]);
                    }
                    const bool composed=state_->composite.enqueue_sequence(command,state_->base[eye],state_->receiver[eye],
                        physical_blur?state_->blur->raw[eye]:styled,size[0],size[1],
                        mask.buffer?&mask:nullptr,reflection.buffer?&reflection:nullptr,1,float(settings.reflections)/3,
                        rays && settings.reflections && decorative_material(static_cast<Effect>(settings.material))?settings.material:0,
                        std::max(1U,unsigned(std::lround(double(size[1])/224))),physical_blur || reconstructed_post?std::array<CalibratedPostEffects,3>{}:post,
                        temporal_aa && !reconstructed_post?&state_->taa->edge_witness[eye]:nullptr,eye);
                    check(composed,state_->composite.status().c_str());
                    if(physical_blur) {
                        void* world=multisample?state_->blur->sample_world[eye]:state_->blur->world[eye];
                        if(reflection.water_layers.world_offset) {
                            const bool ok=state_->composite.enqueue(command,world,
                                multisample?state_->blur->sample_receiver[eye]:state_->blur->world_receiver[eye],
                                state_->blur->ray_world[eye],size[0],size[1],nullptr,&reflection,1,0,0,1,{},true);
                            check(ok,state_->composite.status().c_str());world=state_->blur->ray_world[eye];
                        }
                        auto shutter=motion_blur_preset(settings.motion_blur,std::max(1U,unsigned(std::lround(double(size[1])/224))));
                        shutter.interval_seconds=state_->blur->history.interval();
                        const auto prior=state_->blur->history.jitter();void* blurred{};
                        void* joint_particles{};
                        const auto& fx=state_->frame->scene_fx;
                        const bool particles=std::any_of(fx.points.begin(),fx.points.begin()+fx.count,
                            [](const auto& p){return p.type>=2 && p.type<=7;});
                        if(particles) {
                            const auto& accepted=*state_->blur->history.accepted();
                            auto previous_camera=state_->blur->history.cameras()[eye];
                            // Use the SAME current raster offset on both poses:
                            // physical particle velocity must not contain TAA
                            // phase changes or an MSAA sample-grid displacement.
                            if(multisample) {
                                const auto shifted=calibrated_msaa_sample_camera(previous_camera,size[0],size[1],count,sample);
                                check(bool(shifted),"Invalid prior native particle sample");previous_camera=*shifted;
                            } else {
                                previous_camera.projection[8]-=2*jitter[0]/size[0];
                                previous_camera.projection[9]+=2*jitter[1]/size[1];
                            }
                            const CalibratedSceneFxPrevious before{&accepted.scene_fx,previous_camera,accepted.scene_fx_rig(),accepted.scene_fx_origin};
                            const bool projected=state_->scene_effects.project_particles(command,size[0],size[1],
                                std::max(1U,unsigned(std::lround(double(size[1])/224))),fx,camera,
                                state_->frame->scene_fx_rig(),state_->frame->scene_fx_origin,&before,joint_particles);
                            check(projected,state_->scene_effects.status().c_str());
                        }
                        const bool ok=state_->blur->reconstruct.enqueue(command,state_->blur->raw[eye],world,
                            state_->receiver[eye],state_->surfaces[eye],temporal_aa?state_->taa->motion[eye]:state_->blur->motion[eye],
                            state_->blur->finished[eye],size[0],size[1],shutter,true,blurred,{jitter[0]-prior[0],jitter[1]-prior[1]},
                            {},joint_particles,float(std::max(1U,unsigned(std::lround(double(size[1])/224)))),
                            reflection.water_layers.world_offset?&reflection:nullptr);
                        check(ok,state_->blur->reconstruct.status().c_str());
                        check(state_->composite.enqueue_sequence(command,blurred,state_->receiver[eye],styled,size[0],size[1],nullptr,nullptr,1,0,0,
                            std::max(1U,unsigned(std::lround(double(size[1])/224))),reconstructed_post?std::array<CalibratedPostEffects,3>{}:post,
                            temporal_aa && !reconstructed_post?&state_->taa->edge_witness[eye]:nullptr,eye),state_->composite.status().c_str());
                    }
                    if(!upscaling && !plan.fog && !physical_blur && !supersample && !multisample && !temporal_aa) {
                        const bool ok=state_->scene.enqueue_eye(command,styled,state_->depth[eye],size[0],size[1],camera,{},
                            CalibratedScenePhase::after_rays,final_passes?state_->receiver[eye]:nullptr,token,
                            plan.needs_surfaces()?state_->surfaces[eye]:nullptr,plan.aa);
                        check(ok,state_->scene.status().c_str());
                    }
                    const State::PlaneOwners owners=multisample?state_->msaa->samples[eye][sample]->owners()
                        :State::PlaneOwners{&state_->persistence[eye],&state_->persistence[2+eye],&state_->persistence[4+eye],&state_->exposure[eye]};
                    state_->finish_plane(command,eye,camera,packets,input_plan,owners,styled,final_color,nullptr,nullptr,
                        temporal_aa && reflection.buffer?&reflection:nullptr);
                    if(!upscaling && (plan.fog || physical_blur) && !supersample && !multisample && !temporal_aa) {
                        // Translucent menu dimmers must blend the finished
                        // volume, not protect the raw unenhanced scene below.
                        const bool ok=state_->scene.enqueue_eye(command,final_color,state_->depth[eye],size[0],size[1],camera,{},
                            CalibratedScenePhase::after_rays,state_->receiver[eye],token);
                        check(ok,state_->scene.status().c_str());
                    }
                    finished[sample]=final_color;
                }
                if(supersample || multisample || temporal_aa) {
                    const auto native=state_->extent[eye];
                    const bool resolved=temporal_aa?true:multisample
                        ?state_->msaa->transport.enqueue_resolve(command,std::span<void* const>(finished.data(),count),state_->native_color(eye),
                            native[0],native[1],state_->native_ink[eye],state_->native_receiver[eye])
                        :state_->aa[eye].enqueue_supersample(command,state_->scene_color(eye),state_->receiver[eye],state_->native_ink[eye],
                            state_->native_receiver[eye],state_->native_color(eye),native[0],native[1],state_->sample_factor);
                    check(resolved,multisample?state_->msaa->transport.status().c_str():state_->aa[eye].status().c_str());
                    // Native overlays are ordered AFTER all sample effects and
                    // the colour resolve. A menu dims the enhanced scene.
                    const bool ok=upscaling || state_->scene.enqueue_eye(command,state_->color[eye],state_->native_depth[eye],native[0],native[1],
                        calibrated->cameras[eye],{},CalibratedScenePhase::after_rays,state_->native_receiver[eye],token);
                    check(ok,state_->scene.status().c_str());
                }
                if(upscaling) {
                    const auto full=state_->output_extent[eye],input=state_->extent[eye];
                    if(sdk_aa) {
                        auto& sdk=*state_->dlss;
                        auto* sdk_output=sdk_reconstructed_post?state_->reconstructed_post->raw[eye]:state_->color[eye];
                        if(sdk.held) check(sdk.resolve[eye].enqueue_accepted(command,state_->fsr_ink[eye],state_->fsr_receiver[eye],
                            sdk_output),sdk.resolve[eye].status().c_str());
                        else {
                            // Liquid/untracked geometry reject history locally
                            // through the SDK's current-colour bias guide. Keep
                            // actual secondary RGB and its accepted footprint
                            // locally reactive. Keep whole-image resets only
                            // for untracked finishing/shutter inputs. Setting
                            // changes already reset through the owner signature.
                            const bool reactive=(!sdk_reconstructed_post && (plan.fx || plan.fog
                                || plan.temporal() || plan.globals || settings.chromatic)) || physical_blur;
                            const auto constants=calibrated_dlss_frame(calibrated->cameras[eye],
                                sdk.reuse?sdk.accepted_cameras[eye]:calibrated->cameras[eye],sdk.pending_jitter,!sdk.reuse || reactive);
                            check(bool(constants),"Invalid native DLSS calibrated camera constants");auto f=*constants;
                            f.frame_index=state_->dlss_frame_index;f.width=input[0];f.height=input[1];
                            f.output_width=full[0];f.output_height=full[1];
                            const auto water=reflections[eye].buffer || (multisample && sample_reflections[eye][0].buffer)?multisample
                                ?std::span<const shadows::GpuReflectionOutput>{sample_reflections[eye].data(),raster_samples}
                                :std::span<const shadows::GpuReflectionOutput>{&reflections[eye],1}
                                :std::span<const shadows::GpuReflectionOutput>{};
                            const CalibratedDlssCoverage coverage=multisample?CalibratedDlssCoverage{state_->msaa->receiver[eye],1,raster_samples}
                                :supersample?CalibratedDlssCoverage{state_->receiver[eye],state_->sample_factor,1}:CalibratedDlssCoverage{};
                            unsigned guided_rejected_layers=0;
                            if(!sdk_reconstructed_post) for(const auto& pass:post) guided_rejected_layers|=calibrated_post_motion_rejection_layers(pass,true);
                            const auto patterns=calibrated_pattern_guide(sdk_reconstructed_post?std::array<CalibratedPostEffects,3>{}:post,
                                std::max(1U,unsigned(std::lround(double(size[1])/224))));
                            check(sdk.resolve[eye].enqueue(command,state_->native_color(eye),sdk.ownership[eye],sdk.surface[eye],
                                sdk.motion[eye],state_->fsr_ink[eye],state_->fsr_receiver[eye],sdk_output,f,
                                sdk.reuse?sdk.accepted_jitter:sdk.pending_jitter,water,guided_rejected_layers,coverage,patterns,settings.reflections!=0),sdk.resolve[eye].status().c_str());
                        }
                        if(sdk_reconstructed_post) state_->finish_reconstructed_post(command,eye,calibrated->cameras[eye],packets,plan,post);
                    } else check(state_->fsr[eye].enqueue(command,state_->native_color(eye),{input[0],input[1]},state_->fsr_ink[eye],
                        state_->fsr_receiver[eye],state_->color[eye],{full[0],full[1]}),state_->fsr[eye].status().c_str());
                    // Text, portraits, original artwork and translucent menus
                    // are panel-sized and never enter EASU/RCAS or history.
                    check(state_->scene.enqueue_eye(command,state_->color[eye],state_->fsr_depth[eye],full[0],full[1],
                        calibrated->cameras[eye],{},CalibratedScenePhase::after_rays,nullptr,token),state_->scene.status().c_str());
                }
            }
            auto* completed=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;
            state_->presenter.retain_scene_submission(completed);check(completed,SDL_GetError());
        }
        const auto outcome=state_->presenter.submit_frame(state_->color[0],state_->color[1]);
        status_=state_->presenter.status();
        if(outcome!=DisplayXrSubmission::waiting) state_->history_accepted=outcome==DisplayXrSubmission::submitted;
        if(!state_->presenter.frame_pending()) {
            if(!state_->native_complete()) {state_->retiring_native=true;return DisplayXrSubmission::waiting;}
            if(!state_->finish_dlss_frame()) {state_->retiring_native=true;status_=state_->dlss_frame_end_error;return DisplayXrSubmission::waiting;}
            state_->settle_history(state_->history_accepted);state_->frame.reset();
        }
        return outcome;
    } catch(const std::exception& error) {
        const bool sdk_recorded=state_->dlss && std::any_of(state_->dlss->resolve.begin(),state_->dlss->resolve.end(),
            [](const auto& eye){return eye.has_recorded_sdk_work();});
        if(command && sdk_recorded) {
            // SDK evaluation changes PRIVATE native resource-state tracking as
            // it records commands. Cancelling a later-eye failure leaves that
            // tracker ahead of the real GPU; even reset/slFreeResources does
            // not repair it. Drain the valid recorded work, without presenting
            // or committing the failed pair. Keep all sources until its fence.
            auto* rejected=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;
            state_->presenter.retain_scene_submission(rejected);
        } else if(command) SDL_CancelGPUCommandBuffer(command);
        state_->presentation_failed=true;state_->history_accepted=false;
        state_->settle_history(false);
        if(native_encoded) {
            // Native imports/copies may have submitted even when a later trace
            // or composition failed. Fence the ordered queue BEFORE cancellation
            // so cleanup retains both source images and all native producers.
            auto* retirement=SDL_AcquireGPUCommandBuffer(state_->device);
            state_->presenter.retain_scene_submission(retirement?SDL_SubmitGPUCommandBufferAndAcquireFence(retirement):nullptr);
        }
        const auto failure=std::string(error.what());state_->presenter.cancel_frame();
        if(!state_->presenter.frame_pending()) {
            state_->retiring_native=!state_->native_complete();
            if(!state_->retiring_native && !state_->finish_dlss_frame()) state_->retiring_native=true;
            if(!state_->retiring_native) state_->frame.reset();
        }
        status_=failure;return DisplayXrSubmission::failed;
    }
}
}
