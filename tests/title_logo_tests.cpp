#include "starfox/simulation/game_simulation.hpp"
#include "starfox/localization/title_logos.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/assets/decrunch.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/input/buttons.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) try {
    if(argc!=3 && argc!=4) throw std::runtime_error("Usage: starfox_title_logo_check ROM SYMBOLS [NEW_CAPTURE_DIRECTORY]");
    if(argc==4) {
        if(std::filesystem::exists(argv[3])) throw std::runtime_error("Capture directory already exists");
        std::filesystem::create_directories(argv[3]);
    }
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    starfox::simulation::GameSimulation game(rom,symbols,"TITLEMAP");
    starfox::audio::Spc700Audio audio;
    for(unsigned i=0;i<2;++i) {
        auto tick=game.tick({});(void)audio.render_logic_tick(tick.audio_port_writes);
        game.synchronize_apu_output_ports(audio.output_ports());
    }
    const bool ex=!symbols.find("M_NANMODE").empty();
    if(!ex) {
        const auto chars=starfox::assets::decrunch_reverse(rom,symbols.find("BGTI3CCR").at(0)).bytes;
        const auto map=starfox::assets::decrunch_reverse(rom,symbols.find("BGTI3PCR").at(0)).bytes;
        const auto& us=starfox::localization::title_logo_us;
        if(chars.size()!=us.characters.size() || map.size()!=us.tilemap.size()
            || !std::equal(chars.begin(),chars.end(),us.characters.begin())
            || !std::equal(map.begin(),map.end(),us.tilemap.begin()))
            throw std::runtime_error("Embedded title format differs from assembled US source assets");
    }
    for(unsigned language:{1U,2U,3U,4U,5U,0U}) {
        const auto before=game.map().ppu_state();
        game.set_language(uint8_t(language));
        const auto saved=game.save_state();
        const auto restored=game.restored_state(saved);
        if(restored->language()!=language || restored->save_state()!=saved)
            throw std::runtime_error("Regional language changed during save-state restoration");
        game.swap_state(*restored);
        const auto& ppu=game.map().ppu_state();
        if(ex) {
            if(ppu!=before) throw std::runtime_error("Regional Original logo changed EX PPU");
            continue;
        }
        const auto& logo=language==1?starfox::localization::title_logo_japan:
            (language==2 || language==5)?starfox::localization::title_logo_starwing:starfox::localization::title_logo_us;
        if(!std::equal(logo.characters.begin(),logo.characters.end(),ppu.vram.begin()+0xe000)
            || !std::equal(logo.tilemap.begin(),logo.tilemap.end(),ppu.vram.begin()+0xd000)
            || !std::equal(logo.palette.begin(),logo.palette.end(),ppu.cgram.begin()))
            throw std::runtime_error("Regional title tile/map/palette upload mismatch");
        for(unsigned address=0;address<65536;++address)
            if((address<0xd000 || address>=0xd800) && (address<0xe000 || address>=0xe800)
                && ppu.vram[address]!=before.vram[address]) throw std::runtime_error("Regional title overwrote unrelated VRAM");
        for(unsigned i=0;i<40;++i) {
            auto tick=game.tick({});(void)audio.render_logic_tick(tick.audio_port_writes);
            game.synchronize_apu_output_ports(audio.output_ports());
        }
        if(!std::equal(logo.characters.begin(),logo.characters.end(),ppu.vram.begin()+0xe000)
            || !std::equal(logo.tilemap.begin(),logo.tilemap.end(),ppu.vram.begin()+0xd000)
            || !std::equal(logo.palette.begin(),logo.palette.end(),ppu.cgram.begin()))
            throw std::runtime_error("Title ticks overwrote regional logo: language "+std::to_string(language));
        if(argc==4) {
            starfox::render::Framebuffer frame(256,224);starfox::render::BackgroundRenderer renderer;
            renderer.draw_bg3(ppu,frame);
            starfox::render::write_bmp(frame,std::filesystem::path(argv[3])/("logo-"+std::to_string(language)+".bmp"),
                starfox::render::decode_bgr555_palette(ppu.cgram));
        }
        if(language==1 || language==2 || language==5) {
            const auto step=[&](starfox::input::TickInput input) {
                const auto tick=game.tick(input);(void)audio.render_logic_tick(tick.audio_port_writes);
                game.synchronize_apu_output_ports(audio.output_ports());
            };
            unsigned wait=0;
            while(game.flow_state()==starfox::simulation::GameFlowState::title && wait++<1000) step({});
            if(game.flow_state()!=starfox::simulation::GameFlowState::intro)
                throw std::runtime_error("Regional title did not enter attract intro");
            for(unsigned i=0;i<35;++i) step({});
            starfox::input::TickInput skip{};skip.pressed=skip.held=starfox::input::start;
            step(skip);
            wait=0;
            while(game.flow_state()!=starfox::simulation::GameFlowState::title && wait++<120) step({});
            if(game.flow_state()!=starfox::simulation::GameFlowState::title)
                throw std::runtime_error("Regional intro did not return to title");
            for(unsigned i=0;i<40;++i) step({});
            const auto& returned=game.map().ppu_state();
            if(!std::equal(logo.characters.begin(),logo.characters.end(),returned.vram.begin()+0xe000)
                || !std::equal(logo.tilemap.begin(),logo.tilemap.end(),returned.vram.begin()+0xd000)
                || !std::equal(logo.palette.begin(),logo.palette.end(),returned.cgram.begin()))
                throw std::runtime_error("Title re-entry lost regional logo");
        }
    }
    std::cout<<"Regional Original title selection and EX exclusion passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
