#pragma once
#include "starfox/platform/nintendo_3ds/frontend.hpp"
#include "starfox/simulation/game_simulation.hpp"

namespace starfox::platform::nintendo_3ds {
struct GameRouting {ScreenUse screen;bool move_hud;};
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
