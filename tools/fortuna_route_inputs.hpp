#pragma once
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/input/buttons.hpp"
#include <limits>

namespace starfox::platform::nintendo_3ds::fortuna_route_diagnostic {
// Host diagnostics only: observe immutable source records and submit ordinary
// mapped joypad bits. Never write health, position, strategy, timing or exits.
inline std::uint16_t shape(const assets::SymbolMap& symbols,const char* name) {
    for(const auto value:symbols.find(name))
        if(value && value<=std::numeric_limits<std::uint16_t>::max()) return std::uint16_t(value);
    throw std::runtime_error(std::string("Missing original shape symbol: ")+name);
}
inline bool front_facing(const simulation::GameObject& object) {
    // Original DSTRATS.ASM .chickenheadcol accepts only front-facing hits.
    const auto pitch=std::uint8_t(object.rotation_x-64);
    const auto yaw=std::uint8_t(object.rotation_y-(pitch<128?128:0));
    return std::uint8_t(yaw+128+45)<90;
}
inline input::ButtonMask aimed_input(const GameSession& session,std::uint16_t head,
    std::uint16_t tail,std::uint16_t body) {
    const auto& game=session.game();const auto& pool=game.objects();
    if(!pool.is_active(game.player())) return input::y;
    const auto& player=pool.at(game.player());
    const simulation::GameObject* target=nullptr;
    int rank=std::numeric_limits<int>::max();
    for(auto handle=pool.first_active();handle;handle=pool.next_active(handle)) {
        const auto& object=pool.at(handle);int priority=4;
        if(object.shape==body && !(object.strategy_flags[2]&0x20)) priority=0;
        else if(object.shape==tail && front_facing(object)) priority=1;
        else if(object.shape==head && front_facing(object)) priority=2;
        else continue;
        const int depth=std::int16_t(std::uint16_t(object.world_z)-std::uint16_t(player.world_z));
        if(depth<=0) continue;
        const int candidate=priority*65536+depth;
        if(candidate<rank) {rank=candidate;target=&object;}
    }
    if(!target) return input::y;
    input::ButtonMask held=input::y;
    const int dx=std::int16_t(std::uint16_t(target->world_x)-std::uint16_t(player.world_x));
    const int dy=std::int16_t(std::uint16_t(target->world_y)-std::uint16_t(player.world_y));
    if(dx>24) held|=input::right;else if(dx< -24) held|=input::left;
    // Original Control A is a flight stick: Down climbs (negative world Y).
    if(dy>24) held|=input::up;else if(dy< -24) held|=input::down;
    return held;
}
struct Inputs {
    std::uint16_t head,tail,body;
    explicit Inputs(const assets::SymbolMap& symbols):head(shape(symbols,"BOSS_D_0")),
        tail(shape(symbols,"BOSS_D_2")),body(shape(symbols,"BOSS_D_1")) {}
    input::ButtonMask held(const GameSession& session,unsigned phase,bool boss_seen,bool results_seen) const {
        if(phase<=24 || results_seen) return 0;
        auto buttons=boss_seen?aimed_input(session,head,tail,body):input::ButtonMask(input::y);
        // PSTRATS firecnt rearms only on release. Release spans an FX tick.
        if(phase%24>=18) buttons&=input::ButtonMask(~input::y);
        return buttons;
    }
};
} // namespace starfox::platform::nintendo_3ds::fortuna_route_diagnostic
