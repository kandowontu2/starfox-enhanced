#include "starfox/simulation/game_simulation.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include <iostream>
#include <algorithm>
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    auto rom = starfox::assets::RomImage::load(argv[1]);
    auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    const bool ex=!symbols.find("M_NANMODE").empty();
    starfox::simulation::GameSimulation game{rom, symbols, "INTROMAP"};
    game.set_experience(ex?starfox::simulation::Experience::starfox_ex
        :starfox::simulation::Experience::original);
    game.set_timing_mode(starfox::simulation::TimingMode::original_speed);
    starfox::audio::Spc700Audio audio;
    auto pending = game.map().take_apu_port_writes();
    (void)audio.prime_upload_sequence(pending);
    pending.clear();
    game.synchronize_apu_output_ports(audio.output_ports());
    unsigned phases=0, ticks=0, last_music_phase=0;
    const auto audible = [&] {
        const auto samples = audio.last_music_samples();
        return std::any_of(samples.begin(), samples.end(),
            [](auto s) { return s > 32 || s < -32; });
    };
    const auto exit = symbols.find("EXITINTRO").front();
    while (phases < 7200 && game.map().read_native_byte(exit)==0) {
        game.present_frame(); ++phases;
        if (game.logic_tick_ready()) {
            auto result=game.tick({}); ++ticks;
            pending.insert(pending.end(), result.audio_port_writes.begin(), result.audio_port_writes.end());
        }
        if (phases%3==0) {
            (void)audio.render_logic_tick(pending);pending.clear();
            if (audible()) last_music_phase=phases;
            game.synchronize_apu_output_ports(audio.output_ports());
        }
    }
    std::cout << "intro seconds=" << phases/60.0 << " updates=" << ticks << '\n';
    for(unsigned i=0;i<40;++i) {
        (void)audio.render_logic_tick({});
        if (audible()) last_music_phase=phases+(i+1)*3;
    }
    std::cout << "music audible until seconds=" << last_music_phase/60.0 << '\n';
    if(ex) {
        bool stopped=false,title_music=false;
        const auto title_command=rom.read8(symbols.find("SNDTBL").at(0)+symbols.find("SND_NEWTITLE").at(0));
        for(unsigned frame=0;frame<480;++frame) {
            game.present_frame();
            if(game.logic_tick_ready()) {
                const auto result=game.tick({});
                for(const auto& write:result.audio_port_writes) {
                    if(write.port==0 && write.value==0xf0) stopped=true;
                    if(stopped && game.flow_state()==starfox::simulation::GameFlowState::title
                        && write.port==0 && write.value==title_command) title_music=true;
                }
                pending.insert(pending.end(),result.audio_port_writes.begin(),result.audio_port_writes.end());
            }
            if(frame%3==2) {
                (void)audio.render_logic_tick(pending);pending.clear();
                game.synchronize_apu_output_ports(audio.output_ports());
            }
            if(stopped && title_music) break;
        }
        if(!stopped || !title_music) {
            std::cerr<<"EX intro/title handoff: stop="<<stopped<<" title_music="<<title_music<<'\n';
            return 1;
        }
    }
    // EX's authored FITUNL music loops; it must continue beyond the visuals.
    // Original's nonlooping intro score instead ends with the choreography.
    // This checks approximate choreography, not cycle accuracy. Neither the
    // old 25/30-second visuals nor slowing the audio to match can satisfy it.
    return phases >= 37*60 && phases <= 39*60
        && (ex ? last_music_phase==phases+40U*3U
            : last_music_phase >= 37*60 && last_music_phase <= 39*60
                && std::abs(static_cast<int>(phases)-static_cast<int>(last_music_phase)) < 60)
        ? 0 : 1;
}
