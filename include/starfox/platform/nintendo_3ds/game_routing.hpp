#pragma once
#include "starfox/platform/nintendo_3ds/frontend.hpp"
#include "starfox/simulation/game_simulation.hpp"

namespace starfox::platform::nintendo_3ds {
struct GameRouting {ScreenUse screen;bool move_hud;};
inline bool reference_mono_eligible(simulation::GameFlowState flow,bool stereo,
    bool source_effect,bool menu_visible,bool runtime_options) noexcept {
    // Intro's reference CPU background composition is substantially slower
    // than the ordered PICA layers already used by its wipe/colour frames.
    // Keep that whole sequence on PICA, without changing its eye plan, source
    // cadence or HUD placement. Other mono flows retain their existing path.
    return flow!=simulation::GameFlowState::intro && !stereo && !source_effect
        && !menu_visible && !runtime_options;
}
inline GameRouting game_routing(simulation::GameFlowState flow,bool preview=false) noexcept {
    using enum simulation::GameFlowState;
    if(preview && flow!=pregame_menu)
        return {ScreenUse::menu_preview,flow==gameplay || flow==training};
    switch(flow) {
    case pregame_menu: return {preview?ScreenUse::menu_preview:ScreenUse::setup,false};
    case ex_pregame_menu: return {ScreenUse::front_end,false}; // Cartridge menu is retained in full.
    case training: case gameplay: return {ScreenUse::world,true};
    case intro: return {ScreenUse::world,false};
    default: return {ScreenUse::front_end,false}; // Never move menu/map/credits artwork by HUD heuristics.
    }
}
} // namespace starfox::platform::nintendo_3ds
