#pragma once
#include "fortuna_route_inputs.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <ostream>

namespace starfox::platform::nintendo_3ds::attack_carrier_route_diagnostic {
// Original MAP1_1B/GB3STRAT: damage the exposed hatch/launchers, then the
// carrier body. The shield is deliberately not a target. Observe immutable
// cartridge objects and emit normal Control A input; never change source state.
struct Inputs {
    std::array<std::uint16_t,5> parts;
    explicit Inputs(const assets::SymbolMap& symbols):parts{
        fortuna_route_diagnostic::shape(symbols,"BOSS_7_0"),
        fortuna_route_diagnostic::shape(symbols,"BOSS_7_3"),
        fortuna_route_diagnostic::shape(symbols,"BOSS_7_4"),
        fortuna_route_diagnostic::shape(symbols,"BOSS_7_1"),
        fortuna_route_diagnostic::shape(symbols,"BOSS_7_1O")} {}
    bool relevant(const simulation::GameObject& object) const {
        return std::find(parts.begin(),parts.end(),object.shape)!=parts.end();
    }
    void describe(const GameSession& session,std::ostream& out) const {
        const auto& game=session.game();const auto& pool=game.objects();
        for(auto handle=pool.first_active();handle;handle=pool.next_active(handle)) {
            const auto& object=pool.at(handle);
            if(handle!=game.player() && !relevant(object))continue;
            out<<" ["<<handle<<" shape="<<object.shape<<" xyz="<<object.world_x<<','<<object.world_y<<','<<object.world_z
                <<" hp="<<unsigned(object.health)<<" blocked="<<bool(object.strategy_flags[1]&1)
                <<" immune="<<bool(object.strategy_flags[2]&0x20)<<" hidden="<<bool(object.strategy_flags[3]&8)<<']';
        }
    }
    input::ButtonMask held(const GameSession& session,unsigned phase,bool boss_seen,bool results_seen) const {
        if(phase<=24 || results_seen)return 0;
        auto buttons=input::ButtonMask(input::y);
        const auto& game=session.game();const auto& pool=game.objects();
        if(boss_seen && pool.is_active(game.player())) {
            const auto& player=pool.at(game.player());const simulation::GameObject* target=nullptr;
            int rank=std::numeric_limits<int>::max();
            for(auto handle=pool.first_active();handle;handle=pool.next_active(handle)) {
                const auto& object=pool.at(handle);
                if(!relevant(object) || !object.health || (object.strategy_flags[1]&1)
                    || (object.strategy_flags[2]&0x20) || (object.strategy_flags[3]&8))continue;
                const int depth=std::int16_t(std::uint16_t(object.world_z)-std::uint16_t(player.world_z));
                if(depth<=0)continue;
                const int dx=std::int16_t(std::uint16_t(object.world_x)-std::uint16_t(player.world_x));
                const int dy=std::int16_t(std::uint16_t(object.world_y)-std::uint16_t(player.world_y));
                const int candidate=std::abs(dx)+std::abs(dy);
                if(candidate<rank){rank=candidate;target=&object;}
            }
            if(target) {
                const int dx=std::int16_t(std::uint16_t(target->world_x)-std::uint16_t(player.world_x));
                const int dy=std::int16_t(std::uint16_t(target->world_y)-std::uint16_t(player.world_y));
                if(dx>8)buttons|=input::right;else if(dx< -8)buttons|=input::left;
                if(dy>8)buttons|=input::up;else if(dy< -8)buttons|=input::down;
            }
            // Finite native bomb stock, not an infinite-bomb preference.
            if(phase%600<6)buttons|=input::a;
        }
        if(phase%24>=18)buttons&=input::ButtonMask(~input::y);
        return buttons;
    }
};
}
