#pragma once
#include "starfox/modern/sdl_layer_filter.hpp"
#include "starfox/modern/sdl_source_scene.hpp"
#include "starfox/modern/texture_dump.hpp"
#include "starfox/vr/frame_layers.hpp"
#include "starfox/vr/modern_frame.hpp"
#include <optional>
namespace starfox::modern {
// The frame planning moved to starfox_scene (vr::ModernFrame), shared with
// the PCVR application.
using vr::BackgroundPresentation;
using vr::interpolated_background;
using vr::mode2_horizon_fit;
using vr::reversed_depth_camera;
// MODERN's whole frame, drawn from the shared VR scene code in VR's layer
// order: surrounding stars, backgrounds, tunnel surround, world (compute
// route), first-person MHUD lines, circle wipe, sprites/HUD/dialogue, shutter. Only the camera and
// the target differ from the headset path.
class SdlFrameRenderer {
public:
    bool initialize(SDL_GPUDevice*,SDL_GPUTextureFormat colour,SDL_GPUTextureFormat depth);
    // Once per presented frame, before the render pass. `world` is
    // SourceModels::assemble_world_interpolated(previous,current,alpha,srgb,true).
    // Sprites/HUD rebuild per source tick; backgrounds also at display rate
    // while their scroll registers or scanline tables move.
    bool prepare(SDL_GPUCommandBuffer*,const vr::GameSceneSnapshot& previous,const vr::GameSceneSnapshot& current,
        double alpha,vr::SourceModelPackets world,const vr::FrameLayerInputs&);
    // Inside a render pass on targets of the initialized formats, with the
    // depth target cleared to clear_depth(). The frame draws with reversed
    // depth (reversed_depth_camera) for uniform precision at every distance:
    // coplanar source surfaces resolve by draw order, not depth rounding.
    bool record(SDL_GPUCommandBuffer*,SDL_GPURenderPass*,const vr::EyeCamera&);
    // After prepare(), before the frame's pass: draws the layers the 2D
    // filter applies to at source resolution and filters them; record()
    // then composites them in their place.
    bool record_filtered_layers(SDL_GPUCommandBuffer*,const vr::EyeCamera&);
    // The desktop's 2D FILTER (render::TwoDFilter value; 0 = off) for
    // sprites, HUD and text, and for the backgrounds of 2D screens (title,
    // map, briefing...). Replacement artwork is drawn unfiltered above them.
    void set_two_d_filter(unsigned filter) noexcept {two_d_filter_=filter;}
    static constexpr float clear_depth() noexcept {return 0.F;}
    // Smooth (bilinear) filtering of sprites, HUD, flat backgrounds and model
    // textures; on by default (STARFOX_MODERN_FILTER=0 turns it off).
    void set_filter_2d(bool enabled) noexcept {filter_2d_=enabled;}
    // Replacement landscape artwork: <directory>/bg_XXXX_sky.png and
    // bg_XXXX_ground.png (XXXX = background id, hex), loaded once per level.
    void set_texture_directory(std::string directory);
    // Writes each planet background's art once (owned by the caller; null = off).
    void set_texture_dump(TextureDump* dump);
    // Headset preview (harness): plan the frame as the PCVR app does.
    void set_frame_settings(vr::ModernFrame::Settings settings) noexcept {frame_.set_settings(settings);}
    // Target size and pixels per source pixel for source line thickness.
    void set_target(unsigned width,unsigned height,unsigned source_pixel) noexcept {
        pipelines_.set_line_brush(source_pixel,width,height);
        source_pixel_=float(height)/224.F;target_height_=float(height);target_width_=width;
    }
    // Samples of the colour and depth targets record() draws into (MSAA).
    void set_sample_count(SDL_GPUSampleCount samples) noexcept {pipelines_.set_sample_count(samples);samples_=samples;}
    const SdlSourceScene& world() const noexcept {return world_;}
    void release() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    // Fragment uniform slot 1: the frame's fog for the next draws (mode 0
    // off, 1 everything, 2 the landscape ground only).
    void push_fog(SDL_GPUCommandBuffer*,const vr::EyeCamera&,unsigned mode) const noexcept;
    SDL_GPUDevice* device_{};
    SdlScenePipelines pipelines_;
    SdlUploadBatch uploads_;
    SdlDrawPackets stars_,backgrounds_,tunnel_,cockpit_,sprites_,reticle_,circle_,shutter_,textures_,colour_math_,replaced_sprites_;
    SdlLayerFilter layer_filter_;
    unsigned two_d_filter_{},target_width_{};
    bool filter_backgrounds_{},filter_sprites_{},sprites_split_{};
    SDL_GPUSampleCount samples_{SDL_GPU_SAMPLECOUNT_1};
    vr::ModernFrame frame_;
    std::string texture_directory_;
    TextureDump* texture_dump_{};
    float source_pixel_{1},target_height_{224};
    SdlSourceScene world_;
    bool filter_2d_{true};
    std::string status_{"MODERN frame not initialized"};
};
}
