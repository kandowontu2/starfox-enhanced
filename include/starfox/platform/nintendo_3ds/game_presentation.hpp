#pragma once
#include "starfox/platform/nintendo_3ds/frontend.hpp"
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/render/sprite_selection.hpp"
#include "starfox/vr/game_scene.hpp"

namespace starfox::platform::nintendo_3ds {
inline std::optional<PicaClip> game_controls_clip(simulation::GameFlowState flow) noexcept {
    // CONT.SCR's [24,136)x[24,112) flight panel in the canonical 256x224
    // raster, centered at LCD (72,8). Both eyes use this same LCD window;
    // keep world vertices/depth intact and let PICA clip intersecting ink.
    using enum simulation::GameFlowState;
    if(flow==controls_type || flow==controls_choice) return PicaClip{96,32,208,120};
    return std::nullopt;
}
struct GameNativeModelSnapshot {
    std::uint16_t shape{},colour_table{};
    render::RenderPose pose;
};
struct GameRasterSnapshot {
    // Native 60 Hz display state is independent of slower FX model updates.
    // Fades, OAM, HDMA, flashes and wipes must not inherit a 20 Hz lock.
    std::shared_ptr<const simulation::SnesPpuState> ppu;
    simulation::CircleEffectState circle;
    simulation::WindowWipeState wipe;
    simulation::ColourMathEffectState colour_math;
    // MSHOWOBJ3 (Continue and EX model viewer) has no ObjectPool entry.
    // Capture its complete source pose with the raster, including live zoom;
    // model preparation and either eye must never read mutable VM registers.
    std::optional<GameNativeModelSnapshot> native_model;
    std::uint8_t brightness{};
    bool boss_roll{},stage_hud{},final_score{}; // Source flow policy, captured with the 60 Hz raster.
};
struct GamePresentation {
    FramePlan plan;
    std::shared_ptr<const vr::GameSceneSnapshot> previous,current;
    std::shared_ptr<const GameRasterSnapshot> raster;
    double interpolation_alpha{};
    render::SpriteSelection sprites{render::SpriteSelection::all};
    ImageView dashboard; // Borrowed until the next advance; upload/copy now.
};
} // namespace starfox::platform::nintendo_3ds
