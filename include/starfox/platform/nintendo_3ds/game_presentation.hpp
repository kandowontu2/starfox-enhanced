#pragma once
#include "starfox/platform/nintendo_3ds/frontend.hpp"
#include "starfox/render/sprite_selection.hpp"
#include "starfox/vr/game_scene.hpp"

namespace starfox::platform::nintendo_3ds {
struct GameRasterSnapshot {
    // Native 60 Hz display state is independent of slower FX model updates.
    // Fades, OAM, HDMA, flashes and wipes must not inherit a 20 Hz lock.
    std::shared_ptr<const simulation::SnesPpuState> ppu;
    simulation::CircleEffectState circle;
    simulation::WindowWipeState wipe;
    simulation::ColourMathEffectState colour_math;
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
