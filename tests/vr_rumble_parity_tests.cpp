#include "starfox/audio/spc700_audio.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/rumble_sequencer.hpp"
#include "starfox/simulation/wdc65816.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/input/input_latch.hpp"
#include "starfox/vr/eye_camera.hpp"
#include "starfox/vr/game_frame_driver.hpp"
#include "starfox/vr/game_input.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Observation {
    std::vector<std::uint8_t> game_state,audio_state;
    std::vector<std::int16_t> pcm;
    std::vector<std::array<std::uint8_t,4>> audio_ports;
    std::vector<std::uint8_t> initial_audio_state;
    std::vector<starfox::simulation::ApuPortWrite> apu_trace;
    std::vector<std::optional<starfox::simulation::RumbleEffect>> rumble;
    unsigned video_phases{},logic_ticks{},audio_blocks{},rumble_rasters{};
    unsigned active_rumble_samples{};
    unsigned head_pose_samples{};
    unsigned phases_at_focus{},logic_at_focus{},audio_at_focus{};
    std::uint64_t apu_hash{1469598103934665603ULL};
    unsigned apu_writes{};
};

bool rumble_enabled(const starfox::simulation::GameSimulation& game) {
    return game.rumble()
        && game.experience()==starfox::simulation::Experience::original;
}

std::array<float,7> head_pose(unsigned frame,unsigned eye,unsigned headset_hz) {
    const auto phase=(double(frame)+double(eye)*0.5)/double(headset_hz)*6.283185307179586;
    const auto yaw=phase*0.37;
    return {static_cast<float>(0.03*std::sin(phase)),
        static_cast<float>(0.01*std::cos(phase*0.7)),
        static_cast<float>(-0.02+0.005*std::sin(phase*1.3)),
        0.0F,static_cast<float>(std::sin(yaw*0.5)),0.0F,
        static_cast<float>(std::cos(yaw*0.5))};
}

starfox::vr::VrControls scenario_controls() {
    starfox::vr::VrControls controls;
    controls.steer={-1.0F,0.0F};
    controls.fire=true;
    controls.boost=true;
    controls.roll_left=true;
    return controls;
}

constexpr starfox::input::ButtonMask native_scenario_buttons =
    starfox::input::left | starfox::input::y | starfox::input::x
    | starfox::input::left_shoulder;

void warm_to_live_checkpoint(starfox::simulation::GameSimulation& game,
    starfox::audio::Spc700Audio& audio,const starfox::assets::SymbolMap& symbols) {
    game.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
    const auto checkpoint=symbols.find("MAPRESTART");
    if(checkpoint.empty()) throw std::runtime_error("Live-game warmup needs MAPRESTART symbols");
    for(unsigned tick_count=0;tick_count<3000U
        && game.map().read_native_word(checkpoint.front())==0U;++tick_count) {
        const auto tick=game.tick({});
        static_cast<void>(audio.render_logic_tick(tick.audio_port_writes));
        game.synchronize_apu_output_ports(audio.output_ports());
    }
    if(game.map().read_native_word(checkpoint.front())==0U)
        throw std::runtime_error("Parity fixture did not reach the live-game checkpoint");
}

std::array<starfox::vr::EyeCamera,2> rendered_cameras(
    unsigned frame,unsigned headset_hz) {
    std::array<starfox::vr::EyeCamera,2> cameras{};
    for(unsigned eye=0;eye<2U;++eye) {
        const auto pose=head_pose(frame,eye,headset_hz);
        XrView view{XR_TYPE_VIEW};
        view.pose.position={pose[0]+(eye==0U?-.032F:.032F),pose[1],pose[2]};
        view.pose.orientation={pose[3],pose[4],pose[5],pose[6]};
        view.fov={-.7F,.7F,.7F,-.7F};
        const auto camera=starfox::vr::eye_camera(view,100.0F,10.0F);
        if(!camera) throw std::runtime_error("Synthetic XR pose failed production eye-camera validation");
        cameras[eye]=*camera;
    }
    return cameras;
}

Observation observe(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols,unsigned headset_hz,
    bool interrupt_focus,bool enable_rumble=true,bool sink_available=true) {
    starfox::simulation::GameSimulation game(rom,symbols,"LEVEL1_1",{},true);
    game.set_rumble(enable_rumble);
    const auto controls=scenario_controls();
    starfox::audio::Spc700Audio audio;
    warm_to_live_checkpoint(game,audio,symbols);
    Observation result;
    result.initial_audio_state=audio.save_state();
    starfox::simulation::RumbleSequencer rumble(symbols);
    starfox::vr::GameFrameDriver driver(game,
        [&](auto apu,auto msu) {
            (void)msu;
            for(const auto& write:apu) {
                result.apu_trace.push_back(write);
                result.apu_hash^=write.port;result.apu_hash*=1099511628211ULL;
                result.apu_hash^=write.value;result.apu_hash*=1099511628211ULL;
                result.apu_hash^=write.clock_offset;result.apu_hash*=1099511628211ULL;
                ++result.apu_writes;
            }
            const auto samples=audio.render_logic_tick(apu);
            result.pcm.insert(result.pcm.end(),samples.begin(),samples.end());
            ++result.audio_blocks;
            const auto output_ports=audio.output_ports();
            result.audio_ports.push_back(output_ports);
            return output_ports;
        },nullptr,[&] {
            ++result.rumble_rasters;
            const auto effect=sink_available
                ?rumble.advance(game.map(),rumble_enabled(game)):std::nullopt;
            if(effect && effect->active()) ++result.active_rumble_samples;
            result.rumble.push_back(effect);
        });

    static_cast<void>(driver.advance(1'000'000'000,controls,true));
    std::int64_t paused_offset=0;
    std::optional<std::array<starfox::vr::Matrix4,2>> previous_camera_views;
    for(unsigned frame=1;frame<=headset_hz*2U;++frame) {
        const auto time=1'000'000'000
            +static_cast<std::int64_t>(frame)*1'000'000'000/headset_hz
            +paused_offset;
        // Exercise the production XR pose-to-eye-camera path while gameplay
        // advances once at the shared timestamp. Camera movement is render
        // input and must not become another simulation/source-raster tick.
        const auto cameras=rendered_cameras(frame,headset_hz);
        if(previous_camera_views
            && ((*previous_camera_views)[0]==cameras[0].view
                || (*previous_camera_views)[1]==cameras[1].view))
            throw std::runtime_error("Production eye camera did not respond to the changing head pose");
        previous_camera_views=std::array<starfox::vr::Matrix4,2>{
            cameras[0].view,cameras[1].view};
        const auto step=driver.advance(time,controls,true);
        result.video_phases+=step.video_phases;
        result.logic_ticks+=step.logic_ticks;
        result.head_pose_samples+=2U;
        const auto duplicate=driver.advance(time,controls,true);
        if(!duplicate.duplicate || duplicate.video_phases || duplicate.logic_ticks
            || duplicate.audio_blocks || result.rumble_rasters!=result.video_phases)
            throw std::runtime_error("Duplicate stereo timestamp advanced game/audio/rumble");
        if(interrupt_focus && frame==headset_hz) {
            result.phases_at_focus=result.video_phases;
            result.logic_at_focus=result.logic_ticks;
            result.audio_at_focus=result.audio_blocks;
            const auto pcm_before=result.pcm.size();
            const auto rumble_before=result.rumble.size();
            starfox::vr::VrControls ignored;ignored.fire=true;ignored.menu=true;
            const auto paused=driver.advance(time+5'000'000'000LL,ignored,false);
            const auto resumed=driver.advance(time+5'000'000'000LL,controls,true);
            if(paused.video_phases || paused.logic_ticks || paused.audio_blocks
                || resumed.video_phases || resumed.logic_ticks || resumed.audio_blocks
                || result.video_phases!=result.phases_at_focus
                || result.logic_ticks!=result.logic_at_focus
                || result.audio_blocks!=result.audio_at_focus
                || result.pcm.size()!=pcm_before || result.rumble.size()!=rumble_before)
                throw std::runtime_error("Focus loss advanced game/audio/rumble source time");
            paused_offset=5'000'000'000LL;
        }
    }
    result.game_state=game.save_state();
    result.audio_state=audio.save_state();
    return result;
}

Observation observe_flat(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols,bool enable_rumble=true,
    bool sink_available=true) {
    starfox::simulation::GameSimulation game(rom,symbols,"LEVEL1_1",{},true);
    game.set_rumble(enable_rumble);
    starfox::audio::Spc700Audio audio;
    warm_to_live_checkpoint(game,audio,symbols);
    starfox::simulation::RumbleSequencer rumble(symbols);
    Observation result;
    result.initial_audio_state=audio.save_state();
    std::vector<starfox::simulation::ApuPortWrite> apu;
    std::vector<starfox::simulation::MsuRegisterWrite> msu;
    unsigned audio_phase=0U;
    starfox::input::InputLatch native_input;
    for(unsigned raster=0U;raster<120U;++raster) {
        // The flat host's independent SNES-button representation of the same
        // held steer/fire/boost/roll controls delivered through VrGameInput.
        native_input.sample(native_scenario_buttons);
        game.present_frame();
        ++result.video_phases;
        ++result.rumble_rasters;
        const auto effect=sink_available
            ?rumble.advance(game.map(),rumble_enabled(game)):std::nullopt;
        if(effect && effect->active()) ++result.active_rumble_samples;
        result.rumble.push_back(effect);
        if(game.logic_tick_ready()) {
            const bool options_open=game.runtime_options_open();
            const auto tick=game.tick(native_input.consume());
            ++result.logic_ticks;
            if(!options_open) {
                for(const auto& write:tick.audio_port_writes) {
                    result.apu_trace.push_back(write);
                    result.apu_hash^=write.port;result.apu_hash*=1099511628211ULL;
                    result.apu_hash^=write.value;result.apu_hash*=1099511628211ULL;
                    result.apu_hash^=write.clock_offset;result.apu_hash*=1099511628211ULL;
                    ++result.apu_writes;
                }
                apu.insert(apu.end(),tick.audio_port_writes.begin(),tick.audio_port_writes.end());
            }
            const auto writes=game.map().take_msu_register_writes();
            if(!options_open) msu.insert(msu.end(),writes.begin(),writes.end());
        }
        if(!game.runtime_options_open()&&++audio_phase==3U) {
            const auto samples=audio.render_logic_tick(apu);
            result.pcm.insert(result.pcm.end(),samples.begin(),samples.end());
            ++result.audio_blocks;
            result.audio_ports.push_back(audio.output_ports());
            game.synchronize_apu_output_ports(audio.output_ports());
            apu.clear();msu.clear();audio_phase=0U;
        }
    }
    result.game_state=game.save_state();
    result.audio_state=audio.save_state();
    return result;
}
}

int main(int argc,char** argv) try {
    if(argc<3 || argc>4 || (argc==4 && std::string(argv[3])!="enabled"
        && std::string(argv[3])!="disabled"))
        throw std::runtime_error("Usage: starfox_vr_rumble_parity_check ROM SYMBOLS [enabled|disabled]");
    const bool enable_rumble=argc!=4 || std::string(argv[3])=="enabled";
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    const auto reference=observe_flat(rom,symbols,enable_rumble);
    std::size_t nonzero_pcm_samples=0;
    for(const auto sample:reference.pcm) if(sample!=0) ++nonzero_pcm_samples;
    if(nonzero_pcm_samples==0)
        throw std::runtime_error("Rumble parity fixture produced no nonzero PCM samples");
    if(reference.video_phases!=120 || reference.logic_ticks!=40
        || reference.audio_blocks!=40 || reference.rumble_rasters!=120)
        throw std::runtime_error("Native 60 Hz driver missed its 120-raster reference cadence");
    for(const unsigned headset_hz:{72U,90U,120U}) {
        const auto candidate=observe(rom,symbols,headset_hz,true,enable_rumble);
        if(candidate.video_phases!=reference.video_phases
            || candidate.logic_ticks!=reference.logic_ticks
            || candidate.audio_blocks!=reference.audio_blocks
            || candidate.rumble_rasters!=candidate.video_phases
            || candidate.rumble!=reference.rumble
            || candidate.apu_trace!=reference.apu_trace
            || candidate.audio_ports!=reference.audio_ports
            || candidate.initial_audio_state!=reference.initial_audio_state
            || candidate.game_state!=reference.game_state
            || candidate.audio_state!=reference.audio_state
            || candidate.pcm!=reference.pcm)
        {
            std::string detail;
            if(candidate.audio_state!=reference.audio_state) {
                std::size_t byte=0;
                while(byte<candidate.audio_state.size()&&byte<reference.audio_state.size()
                    &&candidate.audio_state[byte]==reference.audio_state[byte]) ++byte;
                detail+=" first-audio-state-byte="+std::to_string(byte);
            }
            if(candidate.pcm!=reference.pcm) {
                std::size_t sample=0;
                while(sample<candidate.pcm.size()&&sample<reference.pcm.size()
                    &&candidate.pcm[sample]==reference.pcm[sample]) ++sample;
                detail+=" first-pcm-sample="+std::to_string(sample);
            }
            detail+=" initial-audio="+std::to_string(candidate.initial_audio_state==reference.initial_audio_state)
                +" audio-ports="+std::to_string(candidate.audio_ports==reference.audio_ports);
            if(candidate.audio_ports!=reference.audio_ports) {
                std::size_t block=0;
                while(block<candidate.audio_ports.size()&&block<reference.audio_ports.size()
                    &&candidate.audio_ports[block]==reference.audio_ports[block]) ++block;
                detail+=" first-audio-port-block="+std::to_string(block);
                if(block<candidate.audio_ports.size()) {
                    detail+=" candidate";
                    for(const auto value:candidate.audio_ports[block]) detail+=":"+std::to_string(value);
                }
                if(block<reference.audio_ports.size()) {
                    detail+=" reference";
                    for(const auto value:reference.audio_ports[block]) detail+=":"+std::to_string(value);
                }
            }
            detail+=" apu="+std::to_string(candidate.apu_hash==reference.apu_hash)+"/"+std::to_string(candidate.apu_writes==reference.apu_writes);
            detail+=" apu-trace="+std::to_string(candidate.apu_trace==reference.apu_trace);
            if(candidate.apu_trace!=reference.apu_trace) {
                std::size_t write=0;
                while(write<candidate.apu_trace.size()&&write<reference.apu_trace.size()
                    &&candidate.apu_trace[write]==reference.apu_trace[write]) ++write;
                detail+=" first-apu-write="+std::to_string(write);
                if(write<candidate.apu_trace.size()) {
                    const auto& value=candidate.apu_trace[write];
                    detail+=" candidate="+std::to_string(value.port)+":"
                        +std::to_string(value.value)+":"+std::to_string(value.clock_offset);
                }
                if(write<reference.apu_trace.size()) {
                    const auto& value=reference.apu_trace[write];
                    detail+=" reference="+std::to_string(value.port)+":"
                        +std::to_string(value.value)+":"+std::to_string(value.clock_offset);
                }
            }
            detail+=" focus-counts="+std::to_string(candidate.phases_at_focus)+"/"+std::to_string(candidate.logic_at_focus)+"/"+std::to_string(candidate.audio_at_focus);
            std::cerr<<std::to_string(headset_hz)
                +" Hz parity mismatch: raster="+std::to_string(candidate.video_phases)
                +"/"+std::to_string(reference.video_phases)
                +" logic="+std::to_string(candidate.logic_ticks)+"/"+std::to_string(reference.logic_ticks)
                +" audio="+std::to_string(candidate.audio_blocks)+"/"+std::to_string(reference.audio_blocks)
                +" rumble="+std::to_string(candidate.rumble_rasters)+"/"+std::to_string(reference.rumble_rasters)
                +" effects="+std::to_string(candidate.rumble==reference.rumble)
                +" game="+std::to_string(candidate.game_state==reference.game_state)
                +" audio-state="+std::to_string(candidate.audio_state==reference.audio_state)
                +" pcm="+std::to_string(candidate.pcm==reference.pcm)+detail<<'\n';
            if(headset_hz==72U) {
                const auto disabled_reference=observe_flat(rom,symbols,false);
                const auto disabled_candidate=observe(rom,symbols,headset_hz,true,false);
                std::cerr<<"72 Hz rumble-disabled counterfactual: game="
                    <<(disabled_candidate.game_state==disabled_reference.game_state)
                    <<" audio-state="<<(disabled_candidate.audio_state==disabled_reference.audio_state)
                    <<" pcm="<<(disabled_candidate.pcm==disabled_reference.pcm)
                    <<" ports="<<(disabled_candidate.audio_ports==disabled_reference.audio_ports)
                    <<" APU-trace="<<(disabled_candidate.apu_trace==disabled_reference.apu_trace)
                    <<" initial-audio="<<(disabled_candidate.initial_audio_state==disabled_reference.initial_audio_state)
                    <<'\n';
            }
            return 1;
        }
        if(candidate.head_pose_samples!=headset_hz*4U)
            throw std::runtime_error("VR schedule did not sample varied per-eye head poses");
    }
    const auto no_sink_reference=observe_flat(rom,symbols,enable_rumble,false);
    const auto no_sink_candidate=observe(rom,symbols,90U,true,enable_rumble,false);
    if(no_sink_candidate.game_state!=no_sink_reference.game_state
        || no_sink_candidate.apu_trace!=no_sink_reference.apu_trace
        || no_sink_candidate.audio_ports!=no_sink_reference.audio_ports
        || no_sink_candidate.initial_audio_state!=no_sink_reference.initial_audio_state
        || no_sink_candidate.audio_state!=no_sink_reference.audio_state
        || no_sink_candidate.pcm!=no_sink_reference.pcm
        || no_sink_candidate.rumble!=no_sink_reference.rumble)
        throw std::runtime_error("VR advanced authored rumble without an eligible output sink");
    std::cout<<"Rumble "<<(enable_rumble?"enabled":"disabled")
        <<": 60 Hz flat matches 72/90/120 Hz VR game/audio/rumble with held steer/fire/boost/roll; 120 source rasters, "
        <<reference.logic_ticks<<" logic/audio ticks, duplicate eyes, production eye-camera pose variation, focus pause, and no-sink rule passed\n";
    std::cout<<"Active authored-rumble source samples: "<<reference.active_rumble_samples<<'\n';
    std::cout<<"Nonzero PCM samples: "<<nonzero_pcm_samples<<'/'<<reference.pcm.size()<<'\n';
    return 0;
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';
    return 1;
}
