#pragma once
#include "starfox/modern/sdl_frame.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/vr/frame_layers.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include <SDL3/SDL_render.h>
#include <memory>
#include <string>
namespace starfox::modern {
// A host text layer (the ON-SCREEN FPS counter) drawn over MODERN's frame like
// the existing presentation draws it: non-zero ink is white with a black
// shadow one source pixel down-right. x, y are in the canvas (canvas_width x
// canvas_height source pixels) that the frame fills.
// With `panel`, the rest of the overlay's rectangle is a dark backing.
struct HostOverlay {
    const render::Framebuffer* ink{};
    int x{},y{};
    int canvas_width{},canvas_height{};
    bool panel{};
};
// MODERN in the desktop runtime: keeps the shared VR scene history, assembles
// and draws a whole frame through SDL_GPU into an SDL renderer target, and
// presents it. The desktop loop owns simulation, pacing and fallback.
class ModernRuntime {
public:
    ModernRuntime(const simulation::GameSimulation&,const assets::RomImage&,const assets::SymbolMap&);
    ~ModernRuntime();
    ModernRuntime(const ModernRuntime&)=delete;
    ModernRuntime& operator=(const ModernRuntime&)=delete;
    // After every completed logic tick (catch-up ticks included).
    void capture_tick();
    // Pause, stage replacement or camera cut: do not interpolate across it.
    void reset_interpolation() noexcept;
    // The host must call this before destroying/replacing SDL's renderer.
    // Keep scene history, but retire every resource using the live old device.
    void release_renderer() noexcept;
    // Frames MODERN draws; others (host menus) fall back to the existing
    // renderer.
    static bool draws(const simulation::GameSimulation&) noexcept;
    // Draws and presents one frame on `renderer` (SDL's GPU render driver
    // only). `alpha` is the desktop's display interpolation fraction. The
    // frame is drawn at width x height (the desktop's internal raster: canvas
    // x render scale; zero means the window's output size) and scaled to the
    // window like the existing renderer's frame. With `capture_path`, the
    // drawn frame is also written as BMP (without `overlay` or `dialog`).
    // `dialog` is the host's EXIT GAME? / save-slot box, drawn over overlay.
    // `tally` is the stage-clear tally, drawn with the frame's 2D layers.
    bool present(SDL_Renderer*,double alpha,int width=0,int height=0,const char* capture_path=nullptr,
        const HostOverlay& overlay={},const HostOverlay& dialog={},const vr::StageTallyLayer* tally=nullptr);
    const std::string& status() const noexcept {return status_;}
    // Compute-route models skipped this frame (warp/axis not yet ported).
    std::size_t unsupported_models() const noexcept;
private:
    struct State;
    std::unique_ptr<State> state_;
    std::string status_;
};
// The SNES camera (vr::source_panel_camera: focal length 256, live vanishing
// point) widened to `aspect`, with square source pixels: 224 rows fill the
// height and the 256-pixel canvas stays centred.
vr::EyeCamera flat_scene_camera(const vr::GameSceneSnapshot&,float aspect) noexcept;
// Widescreen HUD: the top/bottom HUD groups move out to the edges of the
// view, but never wider than max_hud_aspect (16:9), so on 21:9 and 32:9
// they stay within comfortable reach of the centre; 4:3 keeps the native
// layout. Returns vr::FrameLayerInputs::hud_edge_shift in source pixels.
inline constexpr float max_hud_aspect=16.F/9.F;
int hud_edge_shift(float aspect) noexcept;
}
