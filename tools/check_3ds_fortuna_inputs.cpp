#include "starfox/platform/nintendo_3ds/game_assets.hpp"
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/input/buttons.hpp"
#include "companion_manifest.hpp"
#include "fortuna_route_inputs.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
// This diagnostic drives only ordinary joypad input through GameSession. It
// does not have a mutable VM, does not write health/position/flow, and does not
// claim PICA, ARM, native timing or complete compositor acceptance.
constexpr std::string_view usage=
    "Usage: check_3ds_fortuna_inputs --bundle-original BIN MAX_PHASES POLICY\n"
    "MAX_PHASES: 1..36000; POLICY: center, sweep, target\n"
    "Original LEVEL3_3, original FX pacing, existing god-mode preference; "
    "ordinary joypad inputs only. Exit 0 requires live boss, damage, visible "
    "completed results and return to a fully visible map. NOT hardware acceptance.\n";

using fortuna_route_diagnostic::shape;
using fortuna_route_diagnostic::front_facing;
using fortuna_route_diagnostic::aimed_input;
void describe(const GameSession& session,unsigned phase,std::uint16_t head,
    std::uint16_t tail,std::uint16_t body,input::ButtonMask held) {
    const auto& game=session.game();const auto meter=game.peek_meter_state();
    std::cout<<"Fortuna input phase="<<phase<<" flow="<<unsigned(game.flow_state())
        <<" boss="<<unsigned(meter.boss_health)<<'/'<<unsigned(meter.boss_max_health)
        <<" held="<<held;
    for(auto handle=game.objects().first_active();handle;handle=game.objects().next_active(handle)) {
        const auto& object=game.objects().at(handle);
        if(handle!=game.player() && object.shape!=head && object.shape!=tail && object.shape!=body) continue;
        std::cout<<" ["<<handle<<" shape="<<object.shape<<" xyz="<<object.world_x<<','
            <<object.world_y<<','<<object.world_z<<" hp="<<unsigned(object.health)
            <<" front="<<front_facing(object)<<" immune="<<bool(object.strategy_flags[2]&0x20)<<']';
    }
    std::cout<<std::endl;
}
}

int main(int argc,char** argv) try {
    if(argc==2 && std::string_view(argv[1])=="--help") {std::cout<<usage;return 0;}
    if(argc!=5 || std::string_view(argv[1])!="--bundle-original")
        throw std::invalid_argument(std::string(usage));
    unsigned limit{};const auto value=std::string_view(argv[3]);
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),limit);
    if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || limit<1 || limit>36000)
        throw std::invalid_argument("MAX_PHASES must be a whole number from 1 through 36000");
    const auto policy=std::string_view(argv[4]);
    if(policy!="center" && policy!="sweep" && policy!="target")
        throw std::invalid_argument("POLICY must be center, sweep or target");
    std::ifstream input(argv[2],std::ios::binary);
    if(!input) throw std::runtime_error("Cannot open companion BIN");
    auto cartridge=read_game_cartridge(input,companion_manifest,simulation::Experience::original);
    GameSessionOptions options;options.preferences=GamePreferences{};options.preferences->god=true;
    GameSession session(std::move(cartridge.rom),std::move(cartridge.symbols),[](auto){},"LEVEL3_3",{},options);
    const auto head=shape(session.symbols(),"BOSS_D_0"),tail=shape(session.symbols(),"BOSS_D_2"),
        body=shape(session.symbols(),"BOSS_D_1");
    const fortuna_route_diagnostic::Inputs guided(session.symbols());
    unsigned boss_phase{},damage_phase{},results_phase{},map_frames{};
    bool visible_results=false,completed_results=false;
    session.advance(0,0);
    for(unsigned phase=1;phase<=limit;++phase) {
        input::ButtonMask held=phase>24?input::y:0;
        if(boss_phase && policy=="target") held=aimed_input(session,head,tail,body);
        if(boss_phase && policy=="sweep") {
            const auto part=((phase-boss_phase)/120)%8;
            constexpr std::array<input::ButtonMask,8> sweep{input::right,input::right|input::up,
                input::up,input::left|input::up,input::left,input::left|input::down,
                input::down,input::right|input::down};
            held|=sweep[part];
        }
        // The original PSTRATS firecnt limits a held burst. Its release path
        // rearms the next burst; keep each release longer than one FX tick.
        if(phase%24>=18) held&=input::ButtonMask(~input::y);
        // Results/map are observation milestones, not another stage launch.
        // A fresh Y press on the map would immediately choose the next stage.
        if(results_phase) held=0;
        if(policy=="target") held=guided.held(session,phase,boss_phase!=0,results_phase!=0);
        const auto advance=session.advance((std::int64_t(phase)*1'000'000'000+59)/60,held);
        if(advance.requested_experience || advance.requested_preview || advance.requested_settings_reset)
            throw std::runtime_error("Unexpected owner replacement during input-only route");
        const auto& game=session.game();const auto meter=game.peek_meter_state();
        if(meter.boss_max_health && meter.boss_health && !boss_phase) {
            boss_phase=phase;describe(session,phase,head,tail,body,held);
        }
        if(boss_phase && meter.boss_max_health && meter.boss_health<meter.boss_max_health && !damage_phase) {
            damage_phase=phase;describe(session,phase,head,tail,body,held);
        }
        if(game.flow_state()==simulation::GameFlowState::stage_results) {
            if(!boss_phase || !damage_phase) throw std::runtime_error("Results without observed live boss/damage");
            if(!results_phase) {results_phase=phase;describe(session,phase,head,tail,body,held);}
            const auto tally=game.stage_results_state();
            visible_results|=tally.active && tally.visible && game.map().display_brightness()>0;
            completed_results|=tally.active && tally.visible && tally.displayed_percentage==tally.percentage;
        }
        if((game.flow_state()==simulation::GameFlowState::planet_select
                || game.flow_state()==simulation::GameFlowState::planet_travel) && results_phase
            && game.map().display_brightness()==15 && ++map_frames>=120) {
            if(!visible_results || !completed_results) throw std::runtime_error("Map without complete visible source results");
            std::cout<<"Input-only Fortuna clear passed: boss="<<boss_phase<<" damage="<<damage_phase
                <<" results="<<results_phase<<" map="<<phase<<"; NOT compositor/ARM/hardware acceptance\n";
            return 0;
        }
        if(game.flow_state()==simulation::GameFlowState::game_over
            || game.flow_state()==simulation::GameFlowState::continue_choice)
            throw std::runtime_error("Input-only route reached death/continue instead of clearing");
        if(phase%600==0) describe(session,phase,head,tail,body,held);
    }
    throw std::runtime_error("Declared input-only route bound exhausted without completing boss/results/map");
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
