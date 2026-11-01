#pragma once
#include "starfox/assets/rom.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace starfox::render {
// The final Armada pipe is not an authored rectangular corridor. Its exterior
// cruisers can cross the camera while the player is inside. Filter by source
// strategies, not model size, camera angle, stereo mode or screen coordinates.
// This is presentation-only: enemies, collisions and source timing stay live.
class ExteriorShipVisibility {
public:
    explicit ExteriorShipVisibility(const assets::SymbolMap& symbols) {
        load(symbols,inside_,std::array{"PLAYERINTUNNEL_STRAT","PLAYERINTUNNEL_ISTRAT",
            "PLAYERTOCSLOW_ISTRAT","PLAYERWASHENT_STRAT","PLAYERWASHENT_ISTRAT",
            "PLAYERINNUCLEUS_STRAT","PLAYERINNUCLEUS_ISTRAT"});
        load(symbols,cruisers_,std::array{"CRUISER1_STRAT","CRUISER1_ISTRAT",
            "CRUISER1F_ISTRAT","CRUISER1FALL_STRAT","CRUISER1FALL_ISTRAT"});
    }
    bool hide(std::uint32_t player_strategy,std::uint32_t object_strategy) const noexcept {
        return contains(inside_,player_strategy) && contains(cruisers_,object_strategy);
    }
private:
    template<std::size_t N> static void load(const assets::SymbolMap& symbols,
        std::array<std::uint32_t,N>& addresses,const std::array<const char*,N>& names) {
        for(std::size_t i=0;i<N;++i) {
            const auto& values=symbols.find(names[i]);
            if(!values.empty()) addresses[i]=values.front();
        }
    }
    template<std::size_t N> static bool contains(const std::array<std::uint32_t,N>& list,
        std::uint32_t strategy) noexcept {
        return strategy && std::find(list.begin(),list.end(),strategy)!=list.end();
    }
    std::array<std::uint32_t,7> inside_{};
    std::array<std::uint32_t,5> cruisers_{};
};
}
