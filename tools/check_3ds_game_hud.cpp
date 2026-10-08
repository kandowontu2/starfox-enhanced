#include "starfox/platform/nintendo_3ds/game_hud.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
void require(bool ok,const char* message) {++checks;if(!ok) throw std::runtime_error(message);}
std::uint32_t symbol(const assets::SymbolMap& symbols,const char* name) {
    const auto& found=symbols.find(name);
    if(found.empty()) throw std::runtime_error(std::string("Missing fixture symbol: ")+name);
    return found.front();
}
Rgb pixel(ImageView image,unsigned x,unsigned y) {
    const auto at=std::size_t(y)*image.pitch+x*3;
    return {image.pixels[at],image.pixels[at+1],image.pixels[at+2]};
}
Rgb colour(std::uint16_t word,unsigned brightness) {
    const auto channel=[&](unsigned shift) {
        const auto value=(word>>shift)&31;
        return std::uint8_t(((value<<3)|(value>>2))*std::min(brightness,15U)/15);
    };
    return {channel(0),channel(5),channel(10)};
}
void status_tests() {
    GameHudFrame frame;frame.routing={ScreenUse::world,true};
    frame.meters={18,27,false,true,150,200};frame.lives=4;frame.bombs=5;
    frame.teammate_health={0,20,255};
    auto state=GameHud::status(frame);
    require(state.shield_percent==50 && state.boost_percent==75,"Source health/boost direction changed");
    require(state.lives==3 && state.bombs==5,"Reserve lives or bombs changed");
    require(state.boss_percent==75 && state.ally_percent==std::array<std::optional<unsigned>,3>{0,50,100},"Boss/teammate source bounds changed");
    frame.meters.boss_health=210;
    require(GameHud::status(frame).boss_percent==0,"Boss destruction sentinel not handled");
    frame.lives=0;frame.meters.extended=true;frame.meters.player_health_max=72;
    frame.meters.damage=54;frame.meters.damage_two=36;frame.meters.player_two_activated=true;
    state=GameHud::status(frame);
    require(state.lives==0 && state.shield_percent==75 && state.second_shield_percent==50,"EX extended health changed");
    frame.meters.second_player_view=true;frame.meters.boost_enabled=false;
    state=GameHud::status(frame);
    require(state.shield_percent==50 && !state.counters_enabled && !state.boost_enabled,"EX second view claimed P1 counters/boost");
    frame.player_two=GameHudFrame::PlayerTwo{7,2};
    state=GameHud::status(frame);
    require(state.counters_enabled && state.second_player_view && state.lives==6 && state.bombs==2
        && !state.second_counters,"EX P2 view did not select its own cartridge counters");
    frame.player_two->lives=0;
    require(GameHud::status(frame).lives==0,"P2 zero lives underflowed reserve count");
    frame.player_two->lives=1;
    require(GameHud::status(frame).lives==0,"P2 active ship counted as reserve");
    frame.meters.second_player_view=false;
    state=GameHud::status(frame);
    require(state.counters_enabled && !state.second_player_view && state.lives==0 && state.bombs==5
        && state.second_counters==HudCounters{0,2},"P1 view lost P1 counters or omitted active P2 reserve/bombs");
    frame.meters.player_two_activated=false;
    require(!GameHud::status(frame).second_counters,"Inactive P2 showed stale counters");
    frame.meters.second_player_view=true;
    require(!GameHud::status(frame).counters_enabled,"Inactive P2 view showed counters");
    frame.meters.player_two_activated=true;
    frame.meters.player_health_max=0;
    require(GameHud::status(frame).shield_percent==0,"Zero EX health denominator");
    frame.routing=game_routing(simulation::GameFlowState::pregame_menu);
    state=GameHud::status(frame);
    require(!state.meters_enabled && !state.counters_enabled && !state.second_counters
        && GameHud::top_selection(frame)==render::SpriteSelection::all,"Pre-game artwork treated as gameplay HUD");
    for(auto flow:{simulation::GameFlowState::title,simulation::GameFlowState::ex_pregame_menu,
                  simulation::GameFlowState::planet_select,simulation::GameFlowState::stage_results,
                  simulation::GameFlowState::game_over,simulation::GameFlowState::credits}) {
        frame.routing=game_routing(flow);
        require(GameHud::top_selection(frame)==render::SpriteSelection::all,"Non-game screen lost cartridge objects");
    }
    frame.routing=game_routing(simulation::GameFlowState::gameplay);
    require(GameHud::top_selection(frame)==render::SpriteSelection::world_only,"Gameplay did not route source HUD before composition");
    frame.meters.extended=false;
    state=GameHud::status(frame);
    require(!state.second_player_view && !state.second_counters && state.bombs==5,"Retail adopted synthetic EX P2 state");
}
void portrait_oracle(const assets::RomImage& rom,const assets::SymbolMap& symbols,
                     const GameHudFrame& frame,ImageView actual) {
    const auto base=symbol(symbols,frame.dialogue.alternate_portraits?"FACEDATA2":"FACEDATA")
        +frame.dialogue.portrait_frame*640;
    // Independently decode the cartridge's tile-column-major, 4-plane data.
    for(unsigned y=0;y<40;++y) for(unsigned x=0;x<32;++x) {
        const auto tile=base+((x/8)*5+y/8)*32,row=(y%8)*2,mask=128U>>(x%8);
        unsigned index=112;
        for(unsigned plane=0;plane<4;++plane) {
            const auto offset=(plane/2)*16+row+plane%2;
            if(rom.read8(tile+offset)&mask) index|=1U<<plane;
        }
        require(pixel(actual,128+x,104+y)==colour(frame.palette[index],frame.brightness),"Cartridge portrait/palette/brightness changed");
    }
}
void text_oracle(const assets::RomImage& rom,const assets::SymbolMap& symbols,
                 const GameHudFrame& frame,ImageView actual) {
    std::array<std::uint8_t,284*56> expected{};
    const auto glyphs=symbol(symbols,"FONT0FON"),widths=symbol(symbols,"FONT0WID"),translation=symbol(symbols,"FONT0TRN");
    const auto ink=112+(rom.read8(frame.dialogue.text_address)&15);
    unsigned x=0,count=0;
    for(auto address=frame.dialogue.text_address+1;count++<256;++address) {
        const auto code=rom.read8(address);if(!code) break;
        require(code>=32 && code<127,"Text fixture requires one printable source line");
        const auto glyph=rom.read8(translation+code-32);
        const auto width=code==32?5U:rom.read8(widths+glyph);
        require(x+width<=284,"Text fixture must not wrap");
        if(code!=32) for(unsigned row=0;row<12;++row) {
            const auto bits=rom.read16(glyphs+glyph*24+row*2);
            for(unsigned column=0;column<std::min(width,16U);++column)
                // Cartridge rows are little-endian words with MSB-first pixels.
                if(bits&(0x8000U>>column)) expected[row*284+x+column]=std::uint8_t(ink);
        }
        x+=width;
    }
    for(unsigned y=0;y<56;++y) for(unsigned column=0;column<284;++column) {
        const auto index=expected[y*284+column];
        require(pixel(actual,18+column,16+y)==(index?colour(frame.palette[index],frame.brightness):Rgb{6,18,28}),"Radio did not preserve source glyphs/ink");
    }
}
void capture_bmp(ImageView image,const std::filesystem::path& path) {
    Canvas canvas;canvas.image(0,0,image);canvas.write_bmp(path.string());
}
void cartridge_tests(const char* rom_path,const char* symbol_path,const std::filesystem::path& output) {
    const auto rom=assets::RomImage::load(rom_path);const auto symbols=assets::SymbolMap::load(symbol_path);
    GameHud hud(rom,symbols);simulation::GameSimulation game(rom,symbols);
    // The game observer must leave RAM, PPU, open-bus state and timing untouched.
    const auto before=game.save_state();
    auto frame=hud.capture(game);hud.update(frame);
    require(game.save_state()==before,"3DS HUD observation changed simulation state");
    const bool extended=!symbols.find("SPECWEPCNTONE").empty();
    game.map().write_native_byte(symbol(symbols,"LIVES"),4);
    game.map().write_native_byte(symbol(symbols,extended?"SPECWEPCNTONE":"SPECWEPCNT"),5);
    if(extended) {
        game.map().write_native_byte(symbol(symbols,"LIVESTWO"),7);
        game.map().write_native_byte(symbol(symbols,"SPECCNTTWO"),2);
        game.map().write_native_byte(symbol(symbols,"M_PLAYERTWOACTIVATED"),1);
        game.map().write_native_byte(symbol(symbols,"M_PLAYERTWO"),1);
    }
    const auto teammate=symbol(symbols,"FRIENDS_HP");
    game.map().write_native_byte(teammate,0);game.map().write_native_byte(teammate+1,20);game.map().write_native_byte(teammate+2,40);
    const auto state=game.save_state();
    frame=hud.capture(game);
    require(frame.lives==4 && frame.bombs==5 && frame.teammate_health==std::array<std::uint8_t,3>{0,20,40},"3DS HUD read wrong cartridge counters");
    require(extended?frame.player_two==GameHudFrame::PlayerTwo{7,2}:!frame.player_two,
        "P2 capture did not use EX LIVESTWO/SPECCNTTWO exclusively");
    if(extended) {
        auto active=frame;active.routing={ScreenUse::world,true};
        const auto p2=GameHud::status(active);
        require(active.meters.second_player_view && active.meters.player_two_activated
            && p2.counters_enabled && p2.lives==6 && p2.bombs==2,"Actual EX second-view RAM did not select P2 counters");
    }
    require(game.save_state()==state,"Semantic status capture mutated VM");
    // Use the newly linked game-state owner, not an old standalone executable.
    const auto restored=game.restored_state(state);
    require(restored->save_state()==state,"HUD/source archive did not round-trip");
    const auto restored_frame=hud.capture(*restored);
    require(restored_frame.lives==frame.lives && restored_frame.bombs==frame.bombs && restored_frame.player_two==frame.player_two
        && restored_frame.teammate_health==frame.teammate_health
        && restored_frame.palette==frame.palette && restored_frame.brightness==frame.brightness,
        "Restored cartridge HUD observation differs");
    frame.routing={ScreenUse::world,true};frame.paused=false;frame.language=0;
    frame.meters={18,27,false,true,75,100};frame.dialogue.active=true;frame.dialogue.text_visible=true;
    frame.dialogue.text_address=symbol(symbols,"SCORETXT");
    for(unsigned i=0;i<256;++i) frame.palette[i]=std::uint16_t((i%32)|(((i*3)%32)<<5)|(((i*7)%32)<<10));
    for(bool alternate:{false,true}) {
        if(alternate && !extended) continue;
        for(unsigned portrait:{0U,7U,17U}) for(unsigned brightness:{0U,7U,15U}) {
            frame.dialogue.portrait_frame=std::uint8_t(portrait);frame.dialogue.alternate_portraits=alternate;
            frame.brightness=std::uint8_t(brightness);
            hud.update(frame);portrait_oracle(rom,symbols,frame,hud.view());text_oracle(rom,symbols,frame,hud.view());
            require(!hud.update(frame),"Unchanged source artwork redrew dashboard");
        }
    }
    if(!output.empty()) {
        std::filesystem::create_directories(output);
        capture_bmp(hud.view(),output/"cartridge-hud.bmp");
        if(extended) {
            frame.meters.extended=true;frame.meters.player_two_activated=true;
            frame.meters.damage_two=27;hud.update(frame);
            capture_bmp(hud.view(),output/"two-player-lower.bmp");
            frame.meters.second_player_view=true;hud.update(frame);
            capture_bmp(hud.view(),output/"p2-view-lower.bmp");
        }
    }
    frame.paused=true;require(hud.update(frame),"Pause did not retire radio artwork");
    require(pixel(hud.view(),128,104)==Rgb{6,18,28},"Pause left stale portrait");
    frame.paused=false;frame.dialogue.active=false;
    hud.update(frame);require(pixel(hud.view(),128,104)==Rgb{6,18,28},"Inactive/death communication left stale portrait");
    frame.routing=game_routing(simulation::GameFlowState::pregame_menu);
    hud.update(frame);require(GameHud::top_selection(frame)==render::SpriteSelection::all,"Setup screen lost native HUD-shaped menu sprites");
    if(!output.empty()) capture_bmp(hud.view(),output/"setup-lower.bmp");
}
} // namespace
int main(int argc,char** argv) {
    try {
        status_tests();
        if(argc==3 || argc==5) {
            if(argc==5 && std::string_view(argv[3])!="--capture") throw std::invalid_argument("Expected --capture directory");
            cartridge_tests(argv[1],argv[2],argc==5?std::filesystem::path(argv[4]):std::filesystem::path{});
        } else if(argc!=1) throw std::invalid_argument("Usage: game_hud_check [ROM SYMBOLS [--capture DIRECTORY]]");
        std::cout<<"3DS source HUD: "<<checks<<" checks passed; "<<(argc>1?"real cartridge assets":"status fixtures only")<<"\n";
    } catch(const std::exception& error) {std::cerr<<"3DS source HUD: "<<error.what()<<'\n';return 1;}
}
