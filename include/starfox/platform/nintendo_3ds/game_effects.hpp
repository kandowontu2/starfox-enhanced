#pragma once
#include "starfox/platform/nintendo_3ds/game_presentation.hpp"
#include "starfox/platform/nintendo_3ds/pica_colour.hpp"
#include "starfox/platform/nintendo_3ds/pica_window.hpp"

namespace starfox::platform::nintendo_3ds {
struct GameEffectPlan {
    std::optional<PicaClip> circle_clip;
    WindowCoverage window{WindowCoverage::authored};
};
inline GameEffectPlan game_effect_plan(const GamePresentation& source) {
    if(!source.current || !source.raster) throw std::invalid_argument("Missing 3DS effect snapshot");
    using enum simulation::GameFlowState;
    const auto flow=source.current->flow;
    GameEffectPlan result;
    // The canonical 256x224 source raster is centered at LCD (72,8).
    // Controls' demonstration is only [24,136)x[24,112) in that raster,
    // not a world-wide bomb/death disk over the surrounding instructions.
    if(flow==controls_type || flow==controls_choice) result.circle_clip=PicaClip{96,32,208,120};
    // Follow the source scene-extension contract, rather than treating only
    // gameplay as an expanded scene. The 400px upper LCD is always wider than
    // the cartridge canvas. Boss dossiers keep authored X but expose the full
    // vertical raster; closed wipes must also hide its top/bottom guard rows.
    const bool expanded=flow==intro || flow==ex_pregame_menu || flow==gameplay
        || flow==training || flow==stage_results || (flow==credits && !source.raster->boss_roll)
        || source.raster->final_score;
    result.window=expanded?WindowCoverage::full_scene:source.raster->boss_roll
        ?WindowCoverage::vertical_scene:WindowCoverage::authored;
    return result;
}
struct GameEffectFrames {PicaFrame colour,window;};
// One raster/effect owner shared by both eye submissions. Returned spans stay
// valid until the next prepare; the lower LCD and host menu are not affected.
class GameEffects {
public:
    GameEffectFrames prepare(const GamePresentation& source) {
        const auto policy=game_effect_plan(source);
        return {colour_.prepare(source.raster->circle,source.raster->colour_math,
                    source.raster->brightness,source.plan,policy.circle_clip),
                window_.prepare(source.raster->wipe,source.plan,policy.window)};
    }
private:
    PicaColourEffects colour_;
    PicaWindow window_;
};
} // namespace starfox::platform::nintendo_3ds
