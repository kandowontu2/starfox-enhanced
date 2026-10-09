#pragma once
#include "fortuna_route_inputs.hpp"
#include <ostream>
#include <cstdlib>

namespace starfox::platform::nintendo_3ds::corneria_route_diagnostic {
// Original course 3 Corneria's tank: GB3STRAT.ASM exposes three turrets
// (BOSS_A_1) and three cups (BOSS_A_6). Only aim at currently damageable
// children. This host driver reads immutable objects and emits ordinary
// Control A input; it never writes HP, positions, strategies or map exits.
struct Inputs {
    std::uint16_t turret,cup;
    explicit Inputs(const assets::SymbolMap& symbols)
        :turret(fortuna_route_diagnostic::shape(symbols,"BOSS_A_1")),
         cup(fortuna_route_diagnostic::shape(symbols,"BOSS_A_6")) {}
    void describe(const GameSession& session,std::ostream& out) const {
        const auto& game=session.game();const auto& pool=game.objects();
        for(auto handle=pool.first_active();handle;handle=pool.next_active(handle)) {
            const auto& object=pool.at(handle);
            if(handle!=game.player() && object.shape!=turret && object.shape!=cup) continue;
            out<<" ["<<handle<<" shape="<<object.shape<<" xyz="<<object.world_x<<','<<object.world_y<<','<<object.world_z
                <<" hp="<<unsigned(object.health)<<" blocked="<<bool(object.strategy_flags[1]&1)
                <<" immune="<<bool(object.strategy_flags[2]&0x20)<<" hidden="<<bool(object.strategy_flags[3]&8)<<']';
        }
    }
    input::ButtonMask held(const GameSession& session,unsigned phase,bool boss_seen,bool results_seen) const {
        if(phase<=24 || results_seen) return 0;
        input::ButtonMask buttons=input::y;
        const auto& game=session.game();const auto& pool=game.objects();
        if(boss_seen && pool.is_active(game.player())) {
            const auto& player=pool.at(game.player());
            const simulation::GameObject* target=nullptr;
            int rank=std::numeric_limits<int>::max();
            for(auto handle=pool.first_active();handle;handle=pool.next_active(handle)) {
                const auto& object=pool.at(handle);
                if((object.shape!=turret && object.shape!=cup) || !object.health
                    || (object.strategy_flags[1]&1)   // colldisable
                    || (object.strategy_flags[2]&0x20) // nohitaffect
                    || (object.strategy_flags[3]&8)) continue; // invisible
                const int depth=std::int16_t(std::uint16_t(object.world_z)-std::uint16_t(player.world_z));
                if(depth<=0) continue;
                // Do not lock onto the first child in list order. The side
                // turrets sit above the ship's flight ceiling; the exposed
                // middle turret is reachable before the native bomb attacks.
                const int dx=std::int16_t(std::uint16_t(object.world_x)-std::uint16_t(player.world_x));
                const int dy=std::int16_t(std::uint16_t(object.world_y)-std::uint16_t(player.world_y));
                const int candidate=(object.shape==turret?0:131072)+std::abs(dx)+std::abs(dy);
                if(candidate<rank) {rank=candidate;target=&object;}
            }
            if(target) {
                const int dx=std::int16_t(std::uint16_t(target->world_x)-std::uint16_t(player.world_x));
                const int dy=std::int16_t(std::uint16_t(target->world_y)-std::uint16_t(player.world_y));
                if(dx>8) buttons|=input::right;else if(dx< -8) buttons|=input::left;
                if(dy>8) buttons|=input::up;else if(dy< -8) buttons|=input::down;
            }
            // Ordinary mapped nova-bomb input: finite cartridge stock, no
            // infinite-bomb preference, HP writes or forced strategy exits.
            if(phase%600<6)buttons|=input::a;
        }
        // PSTRATS rearms held laser bursts on a release lasting an FX tick.
        if(phase%24>=18) buttons&=input::ButtonMask(~input::y);
        return buttons;
    }
};
} // namespace starfox::platform::nintendo_3ds::corneria_route_diagnostic
