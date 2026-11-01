#include "starfox/render/exterior_ship_visibility.hpp"
#include "starfox/render/gpu_compatibility.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/audio/spc700_audio.hpp"
#if defined(STARFOX_TEST_SHARED_SCENE)
#include "starfox/vr/game_scene.hpp"
#endif
#include <iostream>
#include <stdexcept>
using namespace starfox;
static void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main(int argc,char** argv) try {
    unsigned checked=0;
    for(bool windows:{false,true}) for(auto vendor:{0U,0x1002U,0x8086U,0x10deU})
        for(auto backend:{"vulkan","direct3d12","metal",""}) {
            require(render::host_cartridge_composition(windows,vendor,backend)
                ==(windows && vendor==0x1002U && std::string_view(backend)=="vulkan"),
                "Radeon workaround escaped its affected Windows/Vulkan combination");++checked;
        }
    render::GpuConfigurationBoundary boundary;
    require(!boundary.changed(1),"First scene unnecessarily drained GPU");
    boundary.commit(1);
    for(unsigned i=0;i<3000;++i) {require(!boundary.changed(1),"Unchanged frame drained GPU");++checked;}
    require(boundary.changed(2),"Live settings change missed retirement");
    require(boundary.changed(2),"Uncommitted failed retirement authorized resource reuse");
    boundary.commit(2);require(!boundary.changed(2),"Retired configuration kept stalling");
    boundary.reset();require(!boundary.changed(3),"Recreated device kept old retirement state");
    const auto symbols=assets::SymbolMap::parse(
        "PLAYERINTUNNEL_STRAT $018100\nPLAYERINTUNNEL_ISTRAT $018200\nPLAYERTOCSLOW_ISTRAT $018300\n"
        "PLAYERWASHENT_STRAT $018400\nPLAYERWASHENT_ISTRAT $018500\nPLAYERINNUCLEUS_STRAT $018600\n"
        "PLAYERINNUCLEUS_ISTRAT $018700\nCRUISER1_STRAT $028100\nCRUISER1_ISTRAT $028200\n"
        "CRUISER1F_ISTRAT $028300\nCRUISER1FALL_STRAT $028400\nCRUISER1FALL_ISTRAT $028500\n");
    render::ExteriorShipVisibility visibility{symbols};
    for(auto player:{0U,0x18888U,0x18100U,0x18200U,0x18300U,0x18400U,0x18500U,0x18600U,0x18700U})
        for(auto object:{0U,0x28888U,0x28100U,0x28200U,0x28300U,0x28400U,0x28500U}) {
            require(visibility.hide(player,object)==(player>=0x18100U && player<=0x18700U
                && object>=0x28100U && object<=0x28500U),"Exterior ship visibility changed unrelated objects or outdoor flight");++checked;
        }
    render::ExteriorShipVisibility empty{assets::SymbolMap::parse("")};
    require(!empty.hide(0,0) && !empty.hide(0x18100U,0x28100U),"Missing source symbols guessed cruiser identities");
    render::ExteriorShipVisibility sparse{assets::SymbolMap::parse(
        "PLAYERINNUCLEUS_ISTRAT $018700\nCRUISER1FALL_ISTRAT $028500\n")};
    require(sparse.hide(0x18700U,0x28500U),"Missing optional strategies shifted remaining source identities");
    if(argc==3) {
        const auto rom=assets::RomImage::load(argv[1]);
        const auto source_symbols=assets::SymbolMap::load(argv[2]);
        const render::ExteriorShipVisibility source_visibility{source_symbols};
        for(bool legacy_retention:{false,true}) {
        simulation::GameSimulation game{rom,source_symbols,"LEVEL1_3"};
        audio::Spc700Audio audio;
        game.set_god_mode(true);
        unsigned outdoor=0,indoor=0,first_hidden=0;
        bool reached_final=false;
        const auto final_background=source_symbols.find("BG_1_3DA").at(0)-source_symbols.find("BGLISTS").at(0);
        for(unsigned tick=0;tick<6000;++tick) {
            if(legacy_retention) for(const auto handle:game.objects().active_handles()) {
                auto& object=game.objects().at(handle);
                if(source_visibility.hide(source_symbols.find("PLAYERINTUNNEL_STRAT").at(0),object.strategy_address))
                    object.type&=~0x08U; // Reproduce the old missing ATZREMOVE flag / legacy saved objects.
            }
            for(unsigned phase=0;phase<3;++phase) game.present_frame();
            const auto result=game.tick({});
            static_cast<void>(audio.render_logic_tick(result.audio_port_writes));
            game.synchronize_apu_output_ports(audio.output_ports());
            if(!game.objects().is_active(game.player())) continue;
            const auto player_strategy=game.objects().at(game.player()).strategy_address;
            reached_final|=game.map().background()==final_background;
            for(const auto handle:game.draw_order()) {
                if(!game.objects().is_active(handle) || handle==game.player()) continue;
                const auto& object=game.objects().at(handle);
                const bool exterior=source_visibility.hide(source_symbols.find("PLAYERINTUNNEL_STRAT").at(0),object.strategy_address);
                if(!exterior) continue;
                if(source_visibility.hide(player_strategy,object.strategy_address)) {
                    ++indoor;
#if defined(STARFOX_TEST_SHARED_SCENE)
                    if(!first_hidden) {
                        // Exercise the real snapshot consumed by MODERN, SR
                        // and VR, not just its helper's classification result.
                        vr::GameSceneHistory history{game,rom,source_symbols,vr::SceneCameraPolicy::source};
                        history.capture();const auto scene=history.current();
                        require(scene && !scene->objects.empty(),"Cruiser guard erased the shared scene");
                        for(const auto& drawn:scene->objects)
                            require(!source_visibility.hide(player_strategy,drawn.object.strategy_address),
                                "Shared stereo/VR scene retained an exterior cruiser inside the ship");
                    }
#endif
                    if(!first_hidden) {first_hidden=tick;std::cout<<"Armada first suppressed cruiser tick="<<tick<<" slot="<<handle
                        <<" player-strategy="<<player_strategy<<" cruiser-strategy="<<object.strategy_address<<'\n';}
                } else ++outdoor;
            }
            if(reached_final && (!legacy_retention || (first_hidden && tick>first_hidden+240U))) break;
        }
        require(outdoor>0,"Armada regression never observed legitimate exterior cruisers");
        require(reached_final,"Armada source playthrough did not reach the boss room");
        if(legacy_retention) require(indoor>0,"Legacy retained cruisers never exercised the inside-ship visibility guard");
        else require(indoor==0,"Fresh source playthrough retained exterior cruisers after entry");
        std::cout<<"Armada source playthrough: legacy-retention="<<legacy_retention
            <<" outdoor-preserved="<<outdoor<<" indoor-hidden="<<indoor<<'\n';
        }
    }
    std::cout<<checked<<" issue 32/49 visibility and GPU safety assertions passed\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
