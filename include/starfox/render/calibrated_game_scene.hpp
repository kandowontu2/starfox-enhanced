#pragma once
#include "starfox/render/gpu_calibrated_scene.hpp"
#include "starfox/render/displayxr_session.hpp"
#include "starfox/vr/game_scene.hpp"
#include "starfox/render/effect_types.hpp"
#include "starfox/render/calibrated_effects.hpp"
#include "starfox/render/scene_enhancements.hpp"
#include "starfox/vr/draw_packet.hpp"
#include <functional>

namespace starfox::render {
// Source models/backgrounds use metres (256 cartridge units per metre).
// The runtime must return that same world space; do not scale XR poses again.
DisplayXrRigSettings calibrated_game_rig(float convergence_source_units=1024);

enum class CalibratedGameLayer {world,model,native_ui,screen};
struct CalibratedGameDraw {
    vr::DrawPacket packet;
    CalibratedGameLayer layer{};
    vr::SceneBlend blend{vr::SceneBlend::opaque};
    bool depth_test{};
    std::uint32_t source_key{};
    bool ray_caster{},after_rays{};
    bool reflection_environment{};
};
struct CalibratedGameSettings {
    bool enhanced_sky{},srgb{};
    bool enhanced_ground{};
    unsigned ground_material{}; // Same Auto/Grass/.../Lava indices as the menu.
    unsigned ground_motion{}; // OFF/WIND/RIPPLES/PULSE, captured once for both eyes.
    std::uint8_t language{};
    float convergence_source_units{1024};
    std::array<unsigned,4> world_effects{},model_effects{};
    unsigned manipulation{},manipulation_intensity{100};
    std::array<unsigned,3> extra_effects{};
    float effect_seconds{};
    // Host resets (including loading the same scene's save state) are separate
    // from immutable source-tick revision and cartridge scene identity.
    std::uint64_t history_epoch{};
    unsigned phosphor{};
    std::uint32_t global_enhancements{};
    unsigned bloom_model{},bloom_world{};
    unsigned contrast{},chromatic{}; // OFF/LOW/MED/HIGH, authored SDR colour.
    unsigned exposure{};bool exposure_paused{};
    unsigned depth_enhancements{}; // AO / depth of field, two bits each.
    unsigned volumetric_fog{}; // OFF/LOW/MED/HIGH; resident geometry-occluded scattering.
    unsigned motion_blur{}; // OFF/LOW/MED/HIGH; physical centred shutter, not persistence.
    unsigned scene_enhancements{},particle_enhancements{}; // Four / two 2-bit options, matching the menu.
    unsigned aa_type{},aa_quality{}; // Menu indices; spatial AA, SSAA, resident TAA and 2/4/8 MSAA.
    unsigned fsr1_mode{}; // OFF / Ultra Quality / Quality / Balanced / Performance.
    unsigned dlss_mode{},dlss_model{}; // OFF/Quality/Balanced/Performance/DLAA; K/M, exclusive with FSR.
    unsigned camera_response_modes{};
    std::array<double,3> camera_response_pose{}; // pitch/yaw/roll, presentation only.
    unsigned ray_tracing{},reflections{},shadow_softness{2},water_caustics{};
    // Requested model material. Native reflective materials are gated by both
    // RT and reflections, exactly like the desktop option; never style UI.
    unsigned material{};
    bool operator==(const CalibratedGameSettings&) const = default;
};
inline std::array<CalibratedPostEffects,3> calibrated_game_post_effects(const CalibratedGameSettings& settings) {
    const auto supported=[](unsigned value) {return calibrated_composite_effect(value)?value:0;};
    return {{{supported(settings.world_effects[0]),supported(settings.model_effects[0]),
        settings.world_effects[1],settings.model_effects[1],settings.effect_seconds},
        {supported(settings.extra_effects[0]),supported(settings.manipulation),100,settings.manipulation_intensity,settings.effect_seconds},
        {supported(settings.extra_effects[2]),supported(settings.extra_effects[1]),100,100,settings.effect_seconds}}};
}
struct CalibratedGameFrame {
    // Retain completed-tick state across both eyes and compositor wait retries.
    std::shared_ptr<const vr::GameSceneSnapshot> previous,current;
    std::vector<CalibratedGameDraw> draws;
    std::array<float,4> clear{};
    CalibratedGameSettings settings;
    double alpha{};
    // Monotonic host presentation time. Frozen source/FX clocks are NOT the
    // displacement interval, and image-wait retries must retain this value.
    double presentation_seconds{};
    std::array<double,3> source_light{-.4,-.7,-1}; // Game-rig +Y down/+Z forward, not a tracked-eye vector.
    SceneFxWorldFrame scene_fx;
    std::array<double,3> scene_fx_origin{};
    vr::Matrix4 scene_fx_source_to_rig{1.F/256,0,0,0,0,-1.F/256,0,0,0,0,-1.F/256,0,0,0,0,1};
    // Compose the same presentation-only world rotation used by packets().
    // Neither events nor source camera are re-sampled per eye or wait retry.
    vr::Matrix4 scene_fx_rig() const;
    // Borrowed descriptors refer to this frame. Styling preserves the supplied
    // per-eye camera; only the final normalized-device shutter is screen locked.
    std::vector<CalibratedScenePacket> packets() const;
};
std::optional<std::array<double,3>> calibrated_game_light(const vr::EyeCamera&,
    std::array<double,3> source, std::array<double,3> response_pose);
// Only host UI artwork is converted here; game models/backdrops remain native
// geometry. Indexed zero is transparent. Output sits at the calibrated rig's
// convergence plane and owns its geometry across native compositor retries.
void append_calibrated_host_ui(CalibratedGameFrame&,const Framebuffer&,
    std::span<const Rgba8> palette,unsigned brightness=15,
    std::optional<std::array<int,4>> dim_rect=std::nullopt);
using CalibratedBackdropLoader=std::function<std::span<const std::uint8_t>(unsigned,std::string_view)>;

// One owner per game/ROM, not per eye. Builds native packets, not a flat RGBA
// screen reprojection. Unsupported/pending source passes fail transactionally.
class CalibratedGameScene {
public:
    CalibratedGameScene(const assets::RomImage&,const assets::SymbolMap&);
    ~CalibratedGameScene();
    CalibratedGameScene(const CalibratedGameScene&)=delete;
    CalibratedGameScene& operator=(const CalibratedGameScene&)=delete;
    CalibratedGameFrame assemble(std::shared_ptr<const vr::GameSceneSnapshot> previous,
        std::shared_ptr<const vr::GameSceneSnapshot> current,
        const simulation::GameSimulation&,double alpha,const CalibratedGameSettings& = {},
        const CalibratedBackdropLoader& = {});
private:
    struct State;std::unique_ptr<State> state_;
};
}
