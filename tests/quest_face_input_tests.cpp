#include "starfox/vr/game_frame_driver.hpp"
#include "starfox/vr/startup_menu.hpp"
#include "starfox/assets/embedded.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void check_cartridge(const std::vector<uint8_t>& bytes,const std::string& symbol_text) {
    using namespace starfox;
    const assets::RomImage rom(bytes);
    const auto symbols=assets::SymbolMap::parse(symbol_text);
    for(bool fixed:{false,true}) for(uint8_t type=0;type<4;++type) {
        simulation::GameSimulation game(rom,symbols,"LEVEL1_1",{},true);
        game.set_timing_mode(simulation::TimingMode::unlocked_20_fps);
        game.map().write_native_byte(symbols.find("C_TYPE").at(0),type);
        vr::GameSceneHistory history(game,rom,symbols);
        vr::GameFrameDriver driver(game,[](auto,auto) {return std::array<uint8_t,4>{};},&history);
        const auto held=[&] {
            return input::ButtonMask((game.map().read_native_byte(symbols.find("CONT0").at(0))<<8)
                |game.map().read_native_byte(symbols.find("CONTL0").at(0)));
        };
        (void)driver.advance(0,{},true,{},fixed);
        vr::VrControls controls;controls.fire=true;controls.steer.y=1;
        (void)driver.advance(50'000'000,controls,true,{},fixed);
        const auto fire=(fixed || !(type&1))?input::y:input::b;
        require((held()&(input::b|input::y))==fire,"Quest A does not reach the native fire bit");
        require((held()&(input::up|input::down))==((type&2)?input::down:input::up),"Vertical inversion changed");
        (void)driver.advance(100'000'000,{},true,{},fixed);
        require(!(held()&(input::b|input::y)),"Released fire remains held");
        controls={};controls.brake=true;
        (void)driver.advance(150'000'000,controls,true,{},fixed);
        const auto brake=(fixed || !(type&1))?input::b:input::y;
        require((held()&(input::b|input::y))==brake,"Quest Y does not reach the native brake bit");
        controls.fire=true;
        (void)driver.advance(200'000'000,controls,true,{},fixed);
        require((held()&(input::b|input::y))==(input::b|input::y),"Simultaneous actions changed");
        (void)driver.advance(250'000'000,{},true,{},fixed);
        controls={};controls.fire=true;
        (void)driver.advance(275'000'000,controls,true,{},fixed);
        (void)driver.advance(300'000'000,{},true,{},fixed);
        require(!(held()&(input::b|input::y)),"Queued tap incorrectly remained held");
        require((game.map().read_native_word(symbols.find("TRIG0").at(0))&(input::b|input::y))==fire,
            "Queued press/release did not reach the native fire trigger");
        require(game.map().read_native_byte(symbols.find("C_TYPE").at(0))==type,"Fixed actions overwrote control type");
    }
    // Host Controls handlers read raw TickInput, not the native remapped pad.
    // Test the real driver boundary: Touch A edits and Touch Y confirms for
    // every selected type, with both Quest policy and unchanged PCVR policy.
    for(bool fixed:{false,true}) for(uint8_t type=0;type<4;++type) for(bool brake:{false,true}) {
        simulation::GameSimulation game(rom,symbols,"CONTMAP");
        game.set_timing_mode(simulation::TimingMode::unlocked_20_fps);
        game.map().write_native_byte(symbols.find("C_TYPE").at(0),type);
        for(unsigned i=0;i<90;++i) (void)game.tick({});
        (void)game.tick({input::start,input::start,0});(void)game.tick({});
        require(game.flow_state()==simulation::GameFlowState::controls_choice,"Controls-menu fixture did not enter choice");
        vr::GameSceneHistory history(game,rom,symbols);
        vr::GameFrameDriver driver(game,[](auto,auto) {return std::array<uint8_t,4>{};},&history);
        (void)driver.advance(0,{},true,{},fixed);
        vr::VrControls controls;controls.fire=!brake;controls.brake=brake;
        (void)driver.advance(50'000'000,controls,true,{},fixed);
        if(brake) require(game.flow_state()==simulation::GameFlowState::controls_choice
            && game.map().fade_direction()<0,"Touch Y no longer confirms the native menu");
        else require(game.flow_state()==simulation::GameFlowState::controls_type,"Touch A no longer edits native controls");
    }
}
}
int main(int argc,char** argv) try {
    using namespace starfox;
    vr::StartupMenu menu;menu.fixed_face_buttons=true;menu.swap_face_buttons=true;
    vr::VrControls controls;controls.fire=true;
    require(menu.gameplay_controls(controls).fire && !menu.gameplay_controls(controls).boost,"Saved swap changed Quest A");
    controls={};controls.brake=true;
    require(menu.gameplay_controls(controls).brake && !menu.gameplay_controls(controls).bomb,"Saved swap changed Quest Y");
    menu.page=vr::StartupMenu::Page::options;menu.selection=2;menu.sample({},true);
    controls={};controls.fire=true;menu.sample(controls,true);
    require(menu.swap_face_buttons && menu.labels()[2]=="A: FIRE / Y: BRAKE","Fixed Quest menu row is not stable");
    require(argc==2,"usage: starfox_vr_face_input_check Starfox-Assets.BIN");
    std::ifstream input(argv[1],std::ios::binary);
    require(bool(input),"Cannot open test bundle");
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    const auto bundle=assets::decode_runtime_bundle(bytes,assets::runtime_companion_manifest(assets::embedded_asset));
    check_cartridge(bundle.original_rom,bundle.original_symbols);
    check_cartridge(bundle.starfox_ex_rom,bundle.starfox_ex_symbols);
    std::cout<<"Quest face buttons passed: Original + EX, all four control types, A fire/Y brake, queued press/release, saved swap, vertical inversion and raw Controls-menu actions\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
