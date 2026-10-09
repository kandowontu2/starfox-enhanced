#include "starfox/platform/nintendo_3ds/game_hud.hpp"
#include "starfox/render/palette.hpp"
#include <algorithm>

namespace starfox::platform::nintendo_3ds {
namespace {
std::uint32_t ram(const assets::SymbolMap& symbols,const char* name) {
    for(auto address:symbols.find(name)) {
        const auto bank=address>>16;
        if(bank==0 || bank==0x70 || bank==0x7e || bank==0x7f) return address;
    }
    throw std::runtime_error(std::string("Missing 3DS HUD RAM symbol: ")+name);
}
unsigned percent(unsigned value,unsigned maximum) noexcept {
    return maximum?std::min(value,maximum)*100/maximum:0;
}
void rgb(const render::Framebuffer& source,std::span<const render::Rgba8,256> palette,
         std::vector<std::uint8_t>& destination,bool transparent_zero) {
    destination.resize(std::size_t(source.width())*source.height()*3);
    for(std::size_t i=0;i<source.pixels().size();++i) {
        const auto index=source.pixels()[i];
        const auto colour=transparent_zero && !index?render::Rgba8{6,18,28,255}:palette[index];
        destination[i*3]=colour.r;destination[i*3+1]=colour.g;destination[i*3+2]=colour.b;
    }
}
bool same_radio(const GameHudFrame& a,const GameHudFrame& b) noexcept {
    return a.routing.move_hud==b.routing.move_hud && a.paused==b.paused
        && a.dialogue.active==b.dialogue.active && a.dialogue.text_visible==b.dialogue.text_visible
        && a.dialogue.portrait_frame==b.dialogue.portrait_frame
        && a.dialogue.alternate_portraits==b.dialogue.alternate_portraits
        && a.dialogue.text_address==b.dialogue.text_address
        && a.palette==b.palette && a.brightness==b.brightness && a.language==b.language;
}
} // namespace
GameHud::GameHud(const assets::RomImage& rom,const assets::SymbolMap& symbols):text_(rom,symbols) {
    addresses_[0]=ram(symbols,"LIVES");
    addresses_[1]=ram(symbols,!symbols.find("SPECWEPCNTONE").empty()?"SPECWEPCNTONE":"SPECWEPCNT");
    addresses_[2]=ram(symbols,"FRIENDS_HP");
    if(!symbols.find("SPECWEPCNTONE").empty()) {
        addresses_[3]=ram(symbols,"LIVESTWO");
        addresses_[4]=ram(symbols,"SPECCNTTWO");
    }
}
GameHudFrame GameHud::capture(const simulation::GameSimulation& game) const {
    const auto read=[&](std::uint32_t address) {return game.map().peek_ram_byte(address).value_or(0);};
    GameHudFrame result;
    result.routing=game_routing(game.flow_state(),game.menu_preview());
    result.meters=game.peek_meter_state();result.dialogue=game.dialogue_state();
    result.lives=read(addresses_[0]);result.bombs=read(addresses_[1]);
    if(addresses_[3]) result.player_two=GameHudFrame::PlayerTwo{read(addresses_[3]),read(addresses_[4])};
    for(unsigned i=0;i<3;++i) result.teammate_health[i]=read(addresses_[2]+i);
    result.palette=game.map().ppu_state().cgram;
    result.brightness=game.map().display_brightness();result.language=game.language();
    result.paused=game.paused();return result;
}
HudState GameHud::status(const GameHudFrame& frame) noexcept {
    HudState result;
    const auto& m=frame.meters;
    result.meters_enabled=frame.routing.move_hud && m.enabled;
    result.boost_enabled=!m.extended || m.boost_enabled;
    const bool player_two=m.extended && m.player_two_activated && frame.player_two.has_value();
    result.second_player_view=m.extended && m.second_player_view;
    result.counters_enabled=frame.routing.move_hud && (!result.second_player_view || player_two);
    result.lives=frame.lives?unsigned(frame.lives)-1:0;result.bombs=frame.bombs;
    if(player_two && frame.routing.move_hud) {
        const auto& counts=*frame.player_two;
        const HudCounters secondary{counts.lives?unsigned(counts.lives)-1:0,counts.bombs};
        if(result.second_player_view) {result.lives=secondary.lives;result.bombs=secondary.bombs;}
        else result.second_counters=secondary;
    }
    const auto maximum=m.extended?m.player_health_max:36U;
    result.shield_percent=percent(m.extended && m.second_player_view?m.damage_two:m.damage,maximum);
    if(m.extended && m.player_one_dead && !m.second_player_view) result.shield_percent=0;
    result.boost_percent=percent(m.boost,36);
    if(m.extended && m.player_two_activated && !m.second_player_view)
        result.second_shield_percent=percent(m.damage_two,maximum);
    if(result.meters_enabled && m.boss_max_health) {
        unsigned current=m.boss_health,maximum_health=m.boss_max_health;
        if(current>=maximum_health+10) current=0; // Source destruction sentinel.
        if(maximum_health&128) {current>>=1;maximum_health>>=1;}
        result.boss_percent=percent(current,maximum_health);
    }
    if(frame.routing.move_hud) for(unsigned i=0;i<3;++i)
        result.ally_percent[i]=percent(frame.teammate_health[i],40);
    return result;
}
bool GameHud::update(const GameHudFrame& frame) {
    const bool radio_changed=!previous_ || !same_radio(*previous_,frame);
    const bool active=frame.routing.move_hud && frame.dialogue.active && !frame.paused;
    if(radio_changed) {
        text_.set_language(frame.language);
        portrait_.clear(0);radio_.clear(0);
        if(active) {
            text_.draw_face(frame.dialogue.portrait_frame,0,0,portrait_,112,frame.dialogue.alternate_portraits);
            if(frame.dialogue.text_visible)
                text_.draw_game_text(frame.dialogue.text_address,0,0,radio_,112,std::nullopt,284);
        }
        const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(frame.palette),frame.brightness);
        rgb(portrait_,palette,portrait_rgb_,false);rgb(radio_,palette,radio_rgb_,true);
    }
    auto hud=status(frame);
    if(frame.routing.move_hud) hud.layout=layout_;
    if(active) {
        hud.portrait={portrait_rgb_,32,40,32*3};
        if(frame.dialogue.text_visible) hud.radio_artwork={radio_rgb_,284,56,284*3};
    }
    if(!frame.routing.move_hud) hud.radio_message="STAR FOX ENHANCED\nMENU ON UPPER SCREEN";
    else if(frame.paused) hud.radio_message="PAUSED";
    const bool redrawn=dashboard_.update(hud);
    previous_=frame;return redrawn;
}
} // namespace starfox::platform::nintendo_3ds
