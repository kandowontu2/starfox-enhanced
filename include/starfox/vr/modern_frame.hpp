#pragma once
#include "starfox/vr/frame_layers.hpp"
#include "starfox/vr/scene_camera.hpp"
#include "starfox/vr/scene_types.hpp"
#include "starfox/vr/source_models.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace starfox::vr {
// Background state between two ticks for display-rate rebuilds: the PPU
// with integer scroll registers/scanline tables (null when static or
// discontinuous), the fractional scroll remainders, and the Mode 2 horizon
// line fitted to each tick's VRAM offset table and interpolated.
struct BackgroundPresentation {
    std::shared_ptr<const simulation::SnesPpuState> ppu;
    std::optional<std::array<float,2>> bg2_fraction,bg3_fraction,mode2_horizon;
};
BackgroundPresentation interpolated_background(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,double alpha);
// Least-squares line through the valid entries of the BG2 Mode 2 vertical
// offset table (VRAM 0x2fa0): {intercept, slope per column}, column 1 = x 0..7.
std::optional<std::array<double,2>> mode2_horizon_fit(const simulation::SnesPpuState&);
// A standard infinite perspective (VR's: clip z = -z - near) rewritten for
// reversed depth (clip z = near); other projections are returned unchanged.
// The scene shader detects it (projection[2][2] == 0) and switches its
// line/decal biases to relative ones.
EyeCamera reversed_depth_camera(EyeCamera) noexcept;
// Per world packet: scenery the cartridge paints over regardless of depth
// (large static objects over 2 m, i.e. 512 source units, or enclosing the
// camera: tunnel sections, hangars, buildings). Renderers draw these first,
// in native list order, with the depth test always passing (still writing
// depth), then everything else depth-tested against them. Moving objects
// (`moving` handles), line-only objects and non-object passes never qualify.
std::vector<bool> painter_scenery(const SourceModelPackets&,std::span<const uint16_t> moving);
// Replacement landscape artwork (RGBA words, red in the low byte).
struct ModernImage {unsigned width{},height{};std::vector<uint32_t> pixels;};
// MODERN's per-frame scene, free of any graphics API: which packets to draw,
// in VR's layer order, and where. The SDL_GPU flat renderer
// (modern::SdlFrameRenderer) and the PCVR application both draw from it; only
// the camera, the target and the GPU resources differ.
class ModernFrame {
public:
    struct Settings {
        // Headset eye cameras instead of the flat SNES camera: the sky dome
        // maps the art by direction (cylindrical, seamless all round) instead
        // of the flat screen's planar projection, VR's sky spheres keep their
        // own scale, and gameplay objects sit `seat_setback` metres further
        // away (the PCVR seat), with the ground and corridor moved to match.
        bool headset{};
        float seat_setback{};
    };
    // A full-screen or layer effect drawn with a blend, below or above OBJ.
    struct Overlay {DrawPacket packet;SceneBlend blend{SceneBlend::opaque};bool over_hud{};};
    void set_settings(Settings settings) noexcept {settings_=settings;}
    // The player's engine glow (optional, off unless set each frame): a soft
    // pool of the engine's red/yellow flicker on the landscape ground at its
    // shadow. `toward_light` is the direction to the sun in scene axes
    // (+Y up, -Z forward, before the eye's view); unset = straight above the ground.
    struct EngineGlowSettings {bool enabled{};std::optional<std::array<float,3>> toward_light;};
    void set_engine_glow(EngineGlowSettings glow) noexcept {engine_glow_settings_=glow;}
    const Settings& settings() const noexcept {return settings_;}
    // Replacement landscape art by file name (bg_XXXX_sky.png, ...); empty
    // when absent. Unset = none.
    void set_image_loader(std::function<std::optional<ModernImage>(const std::string&)> loader) {load_image_=std::move(loader);}
    // Called with each planet background's art and haze once per frame.
    void set_landscape_observer(std::function<void(uint16_t,const simulation::SnesPpuState&,float,std::span<const uint32_t>)> observer) {
        observe_landscape_=std::move(observer);
    }
    // Drop every retained layer (new device, new session).
    void reset() noexcept;
    // Once per presented frame. `world` is
    // SourceModels::assemble_world_interpolated(previous,current,alpha,srgb,true).
    void prepare(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha,
        SourceModelPackets world,const FrameLayerInputs&);
    // The surrounding starfield (distant environment, drawn first).
    std::vector<DrawPacket> stars;
    std::vector<uint32_t> star_keys;
    // The ordered world with the draw-order bias (and seat setback) applied,
    // and the source objects that moved since the previous tick (they always
    // draw depth-tested, never as painter scenery).
    SourceModelPackets world;
    std::vector<uint16_t> moving;
    // Sprites, HUD and the tunnel surround: rebuilt once per source tick.
    bool layers_rebuilt{};
    std::vector<DrawPacket> tunnel_surround,sprites;
    // Backgrounds: rebuilt per tick or at display rate while they scroll;
    // their models are updated every frame.
    bool backgrounds_rebuilt{};
    std::vector<DrawPacket> backgrounds;
    std::vector<Matrix4> background_models;
    // Replacement landscape art (rebuilt once per background) and models.
    bool textures_rebuilt{};
    std::vector<DrawPacket> textures;
    std::vector<Matrix4> texture_models;
    // camera.effects.z for the textures draw: sky horizon row and centre
    // column, 32x fixed point.
    uint32_t texture_mapping{};
    // camera.effects.z for the backgrounds draw on a headset: the source
    // camera heading in the landscape ground's frame (16-bit turn | 1<<16).
    uint32_t landscape_heading{};
    // camera.effects.z for the backgrounds draw on a flat view: the planet
    // art's column offset for the camera's world yaw, so the mountains turn
    // with the ground (16ths of a source column, mod 4096 | 1<<17); zero
    // with STARFOX_MODERN_FIXED_SKY=0 (the cartridge's own screen art).
    uint32_t sky_turn{};
    // Venom's surfaces: a haze in the colour of their cloud band that is
    // full where the cartridge's objects appear (about 3,000 source units
    // ahead), hiding the pop-in. Colour in the target encoding, faded with
    // the display; distances in source units. Unset elsewhere.
    struct Fog {std::array<float,4> colour{};float start{},end{};};
    std::optional<Fog> fog;
    // Flat first person: MHUD wing lines. Reticle as geometry, blended as
    // the cartridge's OBJ colour math blends it.
    std::optional<DrawPacket> cockpit;
    std::optional<Overlay> reticle;
    std::optional<Overlay> circle,colour_math;
    // The engine glow: drawn after the world, depth-tested without writing
    // depth, added (Overlay::blend), so objects in front cover it.
    std::optional<Overlay> engine_glow;
    // Head-locked normalized-device shutter (identity camera).
    std::optional<DrawPacket> shutter;
private:
    Settings settings_;
    EngineGlowSettings engine_glow_settings_;
    std::function<std::optional<ModernImage>(const std::string&)> load_image_;
    std::function<void(uint16_t,const simulation::SnesPpuState&,float,std::span<const uint32_t>)> observe_landscape_;
    std::optional<uint64_t> layer_revision_;
    std::optional<uint16_t> texture_background_;
    // Unwrapped camera yaw behind sky_turn: last heading and accumulated
    // art columns (continuous across the +-pi wrap).
    std::optional<double> sky_yaw_;
    double sky_columns_{};
    std::optional<float> level_horizon_;
    std::map<int,unsigned> horizon_votes_;
    uint16_t level_background_{};
    bool background_interpolated_{};
    std::optional<uint16_t> fog_background_;
    std::optional<std::array<float,3>> fog_colour_;
};
}
