#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/audio/stem_mixer.hpp"
#include "starfox/platform/nintendo_3ds/audio_pcm.hpp"
#include "starfox/platform/nintendo_3ds/game_menu.hpp"
#include "starfox/platform/nintendo_3ds/game_storage.hpp"
#include "starfox/platform/nintendo_3ds/game_remap.hpp"
#include "starfox/platform/nintendo_3ds/game_hud_editor.hpp"
#include "starfox/platform/nintendo_3ds/game_state_storage.hpp"
#include "starfox/state/container.hpp"
#include "starfox/assets/bps.hpp"
#include "starfox/platform/nintendo_3ds/game_models.hpp"
#include "starfox/platform/nintendo_3ds/game_layers.hpp"
#include "starfox/platform/nintendo_3ds/game_dots.hpp"
#include "starfox/platform/nintendo_3ds/game_effects.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include "starfox/platform/nintendo_3ds/presentation_clock.hpp"
#include "fortuna_route_inputs.hpp"
#include "corneria_route_inputs.hpp"
#include "attack_carrier_route_inputs.hpp"
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F f,const char* message) {
    bool rejected=false;try {f();} catch(const std::exception&) {rejected=true;}
    require(rejected,message);
}
std::int64_t timestamp(unsigned frame,unsigned fps=60) {
    return (std::int64_t(frame)*1'000'000'000+fps-1)/fps;
}
// Independent cartridge oracle: direct native raster phases, not GameSession,
// FixedStepClock, GameSceneHistory or either presentation-eye implementation.
struct SourceOracle {
    assets::ShapeDecoder decoder;
    std::unordered_map<std::uint32_t,std::uint32_t> counts;
    std::unordered_set<std::uint32_t> invalid;
    simulation::GameSimulation game;
    audio::Spc700Audio spc;
    input::InputLatch input;
    unsigned phase{},ticks{},blocks{};
    std::vector<std::int16_t> pcm;
    SourceOracle(const assets::RomImage& rom,const assets::SymbolMap& symbols,const std::string& map)
        :decoder(rom,symbols),game(rom,symbols,map,{},true) {
        game.set_experience(symbols.find("SPECWEPCNTONE").empty()
            ?simulation::Experience::original:simulation::Experience::starfox_ex);
        game.set_timing_mode(simulation::TimingMode::original_speed);game.set_shape_face_counts(&counts);
        game.set_msu1_available(false); // Same platform capability, not source timing.
        const auto uploads=game.map().take_apu_port_writes();
        if(!uploads.empty()) {
            static_cast<void>(spc.prime_upload_sequence(uploads));
            if(map!="BOOT") for(unsigned i=0;i<30;++i) static_cast<void>(spc.render_logic_tick({}));
            game.synchronize_apu_output_ports(spc.output_ports());
        }
    }
    std::vector<simulation::ApuPortWrite> pending;
    void raster() {
        for(auto handle:game.draw_order()) if(game.objects().is_active(handle)) {
            const auto id=game.objects().at(handle).shape;
            if(counts.contains(id) || invalid.contains(id)) continue;
            try {counts.emplace(id,std::uint32_t(decoder.decode(id).faces.size()));}
            catch(const std::exception&) {invalid.insert(id);}
        }
        game.present_frame();
        if(game.logic_tick_ready()) {
            const auto tick=game.tick(input.consume());++ticks;
            pending.insert(pending.end(),tick.audio_port_writes.begin(),tick.audio_port_writes.end());
            static_cast<void>(game.map().take_msu_register_writes());
        }
        if(++phase%3==0) {
            static_cast<void>(spc.render_logic_tick(pending));pending.clear();
            std::vector<std::int16_t> next;
            audio::mix_stems(spc.last_music_samples(),spc.last_effect_samples(),
                game.music_volume(),game.sfx_volume(),next);
            pcm.insert(pcm.end(),next.begin(),next.end());
            game.synchronize_apu_output_ports(spc.output_ports());++blocks;
        }
    }
};
void parity(const assets::RomImage& rom,const assets::SymbolMap& symbols,const std::string& map,unsigned polls=720) {
    require(polls>=720 && polls<=14400 && polls%120==0,"Invalid native session parity duration");
    std::vector<std::int16_t> pcm;
    unsigned blocks{},phases{},ticks{},raster_only_updates{};
    GameSession session(rom,symbols,[&](auto samples) {
        require(samples.size()==AudioPcm::samples,"SPC block is not exactly 50ms of stereo PCM");
        std::array<std::int16_t,AudioPcm::samples> copied{};
        copy_audio_pcm(samples,copied); // Same copy contract as the real NDSP sink.
        pcm.insert(pcm.end(),copied.begin(),copied.end());++blocks;
    },map);
    SourceOracle source(rom,symbols,map);
    const auto initial_owner_state=session.game().save_state(),initial_source_state=source.game.save_state();
    if(initial_owner_state!=initial_source_state) {
        const auto a=state::unpack(initial_owner_state,0x47414d01U,assets::crc32(rom.bytes()));
        const auto b=state::unpack(initial_source_state,0x47414d01U,assets::crc32(rom.bytes()));
        std::cerr<<"Initial state payload bytes owner/source: "<<a.size()<<'/'<<b.size()<<"; differing offsets:";
        unsigned shown=0;
        for(std::size_t i=0;i<std::min(a.size(),b.size()) && shown<12;++i)
            if(a[i]!=b[i]) {std::cerr<<' '<<i;++shown;}
        std::cerr<<'\n';
    }
    require(initial_owner_state==initial_source_state,"Initial native owner changed cartridge state");
    require(session.audio().save_state()==source.spc.save_state(),"Initial SPC bank/preroll differs from source");
    if(map=="BOOT") {
        require(session.game().flow_state()==simulation::GameFlowState::pregame_menu,"Real pre-game menu was skipped");
        const auto frame=session.presentation(1,true);
        require(!frame.plan.stereo && frame.sprites==render::SpriteSelection::all,"Setup text/artwork was routed into gameplay");
    }
    require(session.game().timing_mode()==simulation::TimingMode::original_speed,"Original FX timing not baseline");
    require(blocks==0,"Silent bank initialization queued startup audio");
    const auto initial_snapshot=session.presentation(0,true).current;
    const auto initial_raster=session.presentation(0,true).raster;
    const auto initial_native_ppu=*initial_raster->ppu;
    const auto initial_vram=initial_snapshot->ppu->vram;
    const auto initial_palette=initial_snapshot->cgram;
    session.advance(0,0);
    for(unsigned frame=1;frame<=polls;++frame) {
        // 240 Hz polling with one immutable scene for any number of eye reads.
        // A short Start tap at 133ms must reach the source tick from the latch.
        const input::ButtonMask held=map=="BOOT" && frame==32?input::start
            :map!="BOOT" && frame>=120 && frame<400?input::ButtonMask(input::y|input::up|input::right):0;
        source.input.sample(held);
        const auto result=session.advance(timestamp(frame,240),held);
        phases+=result.video_phases;ticks+=result.logic_ticks;
        require(!result.time_clamped && !result.requested_experience,"Normal source clock clamped/switched cartridge");
        if(frame%4==0) {
            source.raster();
            const auto published=session.presentation(0,true);
            require(*published.raster->ppu==source.game.map().ppu_state()
                && published.raster->brightness==source.game.map().display_brightness(),
                "Native video/fade was delayed until a slower model tick");
            if(published.current->display_brightness!=published.raster->brightness) ++raster_only_updates;
        }
        if(frame%120==0) {
            require(phases==frame/4,"High presentation rate altered native 60Hz raster count");
            require(ticks==source.ticks && blocks==source.blocks,"Source/20Hz audio cadence differs");
            require(session.game().save_state()==source.game.save_state(),"3DS source state differs from independent native phases");
            require(session.audio().save_state()==source.spc.save_state() && pcm==source.pcm,"SPC PCM/handshakes differ");
            const auto old= session.presentation(0,true);
            const auto source_coordinate=[&](const char* name) {
                for(auto address:symbols.find(name)) if(address>>16==0 || address>>16==0x7e)
                    return std::bit_cast<std::int16_t>(session.game().map().peek_ram_word(address).value());
                throw std::runtime_error("Camera coordinate missing in cartridge fixture");
            };
            require(old.current->camera.x==source_coordinate("VIEWPOSX")
                && old.current->camera.y==source_coordinate("VIEWPOSY")
                && old.current->camera.z==source_coordinate("VIEWPOSZ"),"Console snapshot inherited a headset-only camera adjustment");
            const auto game_before=session.game().save_state(),apu_before=session.audio().save_state();
            for(float slider:{0.F,.2F,.5F,1.F,0.F}) {
                const auto plan=session.presentation(slider,true);
                require(plan.current==old.current && plan.previous==old.previous && plan.raster==old.raster,
                    "Slider/eyes published a different game or display tick");
                require(valid_image(plan.dashboard,bottom_width,screen_height),"Cartridge lower HUD is invalid");
                require(plan.plan.eye_count==(plan.plan.stereo?2U:1U),"Physical slider eye count mismatch");
            }
            require(session.game().save_state()==game_before && session.audio().save_state()==apu_before,"Eye projection mutated game/audio");
            require(!session.presentation(1,false).plan.stereo,"Mono hardware fabricated a second eye");
        }
    }
    require(phases==polls/4 && blocks==polls/12,"Requested duration did not retain source/audio cadence");
    require(initial_snapshot->ppu->vram==initial_vram && initial_snapshot->cgram==initial_palette,
        "Retained stereo scene referenced mutable cartridge video memory");
    require(*initial_raster->ppu==initial_native_ppu,"Retained native raster referenced mutable cartridge video memory");
    if(map=="BOOT") require(raster_only_updates>0,"Fade fixture never exercised native raster updates between model ticks");
    if(map=="BOOT") require(session.game().flow_state()!=simulation::GameFlowState::pregame_menu,"Quick physical Start tap was lost");
    if(session.game().flow_state()==simulation::GameFlowState::gameplay)
        require(session.presentation(1,true).sprites==render::SpriteSelection::world_only,"Gameplay source HUD was not partitioned");
    // Suspend with a partially accumulated 50ms audio block, not just at its
    // convenient three-raster boundary. Resume must not discard that cadence.
    session.advance(timestamp(polls+8,240),0);
    source.input.sample(0);source.raster();source.raster();
    const auto retained=session.presentation(1,true).current;
    const auto saved=session.game().save_state(),saved_audio=session.audio().save_state();
    const auto duplicate=session.advance(timestamp(polls+8,240),0);
    require(duplicate.duplicate && !duplicate.video_phases && !duplicate.audio_blocks,"Duplicate time ticked source/audio");
    rejects([&]{session.advance(-1,0);},"Negative time accepted");
    session.advance(timestamp(polls+9,240),input::start,false);
    session.advance(900'000'000'000LL,input::start,true);
    require(session.game().save_state()==saved && session.audio().save_state()==saved_audio,"Suspend/resume advanced or reprised source state");
    require(session.presentation(1,true).previous==retained,"Resume retained an interpolated old pose");
    // All buttons are released on resume before accepting a fresh Start.
    session.advance(900'000'000'001LL,0,true);
    const auto clamped=session.advance(901'000'000'001LL,0,true);
    require(clamped.time_clamped && clamped.video_phases==15 && clamped.audio_blocks==5,"Long stall not bounded to 250ms of native phases");
    source.input.reset();for(unsigned i=0;i<15;++i) source.raster();
    require(session.game().save_state()==source.game.save_state() && pcm==source.pcm,"Suspension or clamped catch-up changed cartridge/audio");
    const auto last=session.advance(901'000'000'001LL+timestamp(1),0);
    source.raster();
    require(last.video_phases==1 && last.audio_blocks==1 && blocks==polls/12+6
        && session.game().save_state()==source.game.save_state() && pcm==source.pcm,
        "Suspend/resume discarded a partial source audio block");
    const auto rewind=session.advance(100,0);
    require(!rewind.video_phases && session.presentation(1,true).previous==session.presentation(1,true).current,"Clock rewind replayed a source pose");
    std::cout<<"  "<<map<<": "<<polls/4+18<<" source rasters, "<<ticks<<" initial logic ticks, "<<blocks
        <<" exact SPC blocks; state/PCM oracle matches\n";
}
void stage_sweep(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    std::cout.setf(std::ios::unitbuf);
    const bool ex=!symbols.find("SPECWEPCNTONE").empty();
    unsigned stages=0,flows=0;
    for(const std::string map:{"BOOT","INTROMAP","TITLEMAP","CONTMAP","PLANETSELECT","GAMEOVER","CONTINUE","CREDITSMAP","TRAININGMAP"}) {
        std::cout<<"Native flow sweep begins "<<map<<" (30 source seconds plus suspend/resume)\n";
        try {parity(rom,symbols,map,7200);}
        catch(const std::exception& e){throw std::runtime_error(map+": "+e.what());}
        ++flows;
    }
    // Exact public cartridge labels, not a fabricated level sequence or a
    // copied save state. Every available stage starts through the native
    // initializer. Missing entire courses cannot silently count as coverage.
    for(unsigned course=1;course<=(ex?7U:3U);++course)for(unsigned stage=1;stage<=7;++stage) {
        const auto map="LEVEL"+std::to_string(course)+"_"+std::to_string(stage);
        if(symbols.find(map).empty())continue;
        std::cout<<"Native stage sweep begins "<<map<<" (30 source seconds plus suspend/resume)\n";
        try {parity(rom,symbols,map,7200);}
        catch(const std::exception& e){throw std::runtime_error(map+": "+e.what());}
        ++stages;
    }
    require(stages==(ex?40U:19U) && flows==9,"Native stage sweep did not cover the complete cartridge stage/flow list");
    std::cout<<"3DS stage/flow sweep: "<<stages<<" exact stages and "<<flows
        <<" native flows passed 30-second VM/SPC/PCM/raster/slider/mono/suspend parity."
        <<" Not complete level/boss routes, PICA pixels or physical console FPS/NDSP/slider acceptance.\n";
}
template<class Inputs>
void natural_source_route(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    const char* map,const char* name,const Inputs& guided) {
    require(symbols.find("SPECWEPCNTONE").empty(),"Natural source route requires the Original cartridge");
    std::cout.setf(std::ios::unitbuf);
    std::vector<std::int16_t> pcm;
    unsigned blocks{},phases{},ticks{};
    GameSessionOptions options;options.preferences=GamePreferences{};options.preferences->god=true;
    GameSession session(rom,symbols,[&](auto samples) {
        require(samples.size()==AudioPcm::samples,"Natural-route SPC block changed native PCM extent");
        pcm.insert(pcm.end(),samples.begin(),samples.end());++blocks;
    },map,{},options);
    SourceOracle source(rom,symbols,map);
    // The existing diagnostic uses the normal persisted god preference to
    // survive the route. Apply exactly those user settings to the independent
    // cartridge oracle; never copy an owner state or force a boss/flow exit.
    const auto& prefs=*options.preferences;
    source.game.set_presentation_fps(prefs.render_fps);source.game.set_show_fps(prefs.show_fps);
    source.game.set_timing_mode(prefs.timing);source.game.set_music_volume(prefs.music);source.game.set_sfx_volume(prefs.sfx);
    source.game.set_language(prefs.language);source.game.set_swap_face_buttons(prefs.swap);
    source.game.set_god_mode(prefs.god);source.game.set_infinite_bombs(prefs.bombs);source.game.set_infinite_boost(prefs.boost);
    source.game.set_infinite_lives(prefs.lives);source.game.set_planet_select_cheat(prefs.planet_cheat);
    source.game.set_default_laser(prefs.laser);source.game.set_selected_level(prefs.level);
    source.game.set_stereo_separation(prefs.separation);source.game.set_stereo_convergence(prefs.convergence);
    require(session.game().save_state()==source.game.save_state() && session.audio().save_state()==source.spc.save_state(),
        "Natural-route owner/source preferences or initial cartridge/SPC state differ");
    unsigned boss_phase{},damage_phase{},results_phase{},map_phases{},parity_windows{};
    bool visible_results=false,completed_results=false,full_boss_health_seen=false;
    session.advance(0,0);
    for(unsigned phase=1;phase<=36000;++phase) {
        const auto held=guided.held(session,phase,boss_phase!=0,results_phase!=0);
        source.input.sample(held);
        const auto step=session.advance(timestamp(phase),held);
        phases+=step.video_phases;ticks+=step.logic_ticks;
        require(!step.time_clamped && !step.requested_experience && !step.requested_preview && !step.requested_settings_reset,
            "Natural input-only route clamped time or requested an owner replacement");
        source.raster();
        const auto& game=session.game();const auto meter=game.peek_meter_state();
        if(meter.boss_max_health && meter.boss_health && !boss_phase)boss_phase=phase;
        // Boss child HP is summed during strategy initialization. A partial
        // first tally is NOT a laser damage witness. Observe a fully populated
        // native meter before accepting a later decrease as ordinary damage.
        if(boss_phase && meter.boss_max_health && meter.boss_health==meter.boss_max_health)full_boss_health_seen=true;
        if(full_boss_health_seen && meter.boss_max_health && meter.boss_health<meter.boss_max_health && !damage_phase)damage_phase=phase;
        if(game.flow_state()==simulation::GameFlowState::stage_results) {
            require(boss_phase && damage_phase,"Natural source results appeared without observed live boss/damage");
            if(!results_phase)results_phase=phase;
            const auto tally=game.stage_results_state();
            visible_results|=tally.active && tally.visible && game.map().display_brightness()>0;
            completed_results|=tally.active && tally.visible && tally.displayed_percentage==tally.percentage;
        }
        if(results_phase && (game.flow_state()==simulation::GameFlowState::planet_select
            || game.flow_state()==simulation::GameFlowState::planet_travel) && game.map().display_brightness()==15)++map_phases;
        else map_phases=0;
        const bool finished=map_phases>=120;
        if(phase%60==0 || finished) {
            require(phases==phase && ticks==source.ticks && blocks==source.blocks,
                "Full boss/results/map route changed source raster/logic/SPC cadence");
            require(game.save_state()==source.game.save_state(),"Full natural route diverged from independent cartridge state");
            require(session.audio().save_state()==source.spc.save_state() && pcm==source.pcm,
                "Full natural route changed SPC/handshakes or any emitted PCM sample");
            pcm.clear();source.pcm.clear(); // Exact consecutive windows, bounded host-only storage.
            const auto native=session.presentation(0,true);
            require(*native.raster->ppu==source.game.map().ppu_state()
                && native.raster->brightness==source.game.map().display_brightness(),
                "Full natural route delayed or replaced the cartridge raster/palette/fade");
            const auto before=game.save_state(),before_audio=session.audio().save_state();
            for(float slider:{0.F,.2F,.5F,1.F,0.F}) {
                const auto eye=session.presentation(slider,true);
                require(eye.current==native.current && eye.previous==native.previous && eye.raster==native.raster,
                    "Natural boss/results/map route used a different source tick per slider/eye");
                require(valid_image(eye.dashboard,bottom_width,screen_height)
                    && eye.plan.eye_count==(eye.plan.stereo?2U:1U),"Natural route emitted invalid lower HUD/eye planning");
            }
            require(!session.presentation(1,false).plan.stereo && game.save_state()==before
                && session.audio().save_state()==before_audio,"Natural-route slider/mono inspection changed cartridge/SPC state");
            ++parity_windows;
        }
        require(game.flow_state()!=simulation::GameFlowState::game_over
            && game.flow_state()!=simulation::GameFlowState::continue_choice,"Natural-route diagnostic reached death/continue instead of clearing");
        if(phase%600==0) {
            std::cout<<name<<" source parity phase="<<phase<<" boss="<<boss_phase<<" damage="<<damage_phase
                <<" results="<<results_phase<<" flow="<<unsigned(game.flow_state())<<" windows="<<parity_windows;
            std::cout<<" boss-hp="<<unsigned(meter.boss_health)<<'/'<<unsigned(meter.boss_max_health)
                <<" full-hp-seen="<<full_boss_health_seen;
            if constexpr(requires {guided.describe(session,std::cout);})if(boss_phase)guided.describe(session,std::cout);
            std::cout<<'\n';
        }
        if(finished) {
            require(visible_results && completed_results,"Natural route returned to map without complete visible results");
            std::cout<<"3DS "<<name<<" full source route PASS: boss="<<boss_phase<<" damage="<<damage_phase
                <<" results="<<results_phase<<" map="<<phase<<" windows="<<parity_windows
                <<"; exact VM/raster/SPC/PCM and eye/mono source purity. God preference only; ordinary input."
                <<" Not all courses, PICA/ARM pixels or physical console FPS/audio/stereo acceptance.\n";
            return;
        }
    }
    throw std::runtime_error(std::string("Natural ")+name+" route bound exhausted without boss/results/map completion");
}
void natural_death_restart(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    std::optional<bool> continue_yes={}) {
    // Ordinary stationary flight, no god preference, source-memory writes,
    // checkpoint import or synthetic death. The independent cartridge/SPC
    // owner sees precisely the same input and every source raster.
    std::cout.setf(std::ios::unitbuf);
    std::vector<std::int16_t> pcm;
    unsigned phases=0,ticks=0,blocks=0,windows=0,compositions=0;
    GameSession session(rom,symbols,[&](auto samples) {
        require(samples.size()==AudioPcm::samples,"Death-route native PCM extent changed");
        pcm.insert(pcm.end(),samples.begin(),samples.end());++blocks;
    },"LEVEL1_1");
    SourceOracle source(rom,symbols,"LEVEL1_1");
    std::optional<std::uint32_t> flags;
    for(auto address:symbols.find("GAMEFLAGS")) if(address>>16==0 || address>>16==0x7e) {flags=address;break;}
    require(flags.has_value(),"Death-route source GAMEFLAGS is missing");
    require(session.game().save_state()==source.game.save_state()
        && session.audio().save_state()==source.spc.save_state(),"Death-route initial cartridge/SPC state differs");
    require(!session.game().god_mode() && !session.game().infinite_lives(),"Death-route enabled survival cheats");
    GameModels models(rom,symbols);GameDots dots(rom,symbols);GameLayers layers;
    GameEffects effects;PicaComposite composite;
    unsigned damaged=0,dying=0,circles=0,black=0,restored=0,bright=0,peak_bytes=0,full_health=0;
    unsigned deaths=0,game_over=0,continue_screen=0,continue_bright=0,choice=0,confirm=0,returned=0,return_bright=0;
    unsigned arrival_bright=0,arrival_confirm=0;
    bool previous_dead=false,route_seen=false;
    std::optional<std::uint32_t> option;
    if(continue_yes) {
        for(auto address:symbols.find("FOXY_OPTION"))if(address>>16==0 || address>>16==0x7e) {option=address;break;}
        require(option.has_value(),"Natural Continue route has no source option RAM");
    }
    session.advance(0,0);
    // Keep the original single-death 18,000-phase gate unchanged. These are
    // separate complete natural-reserve-exhaustion routes with a fixed bound;
    // no lives/health, flow, audio bank or checkpoint is written by the test.
    const unsigned bound=continue_yes?60000U:18000U;
    for(unsigned phase=1;phase<=bound;++phase) {
        input::ButtonMask held=0;
        if(choice && phase>=choice && phase<choice+6)held=*continue_yes?input::up:input::down;
        if(confirm && phase>=confirm && phase<confirm+6)held=input::start;
        if(arrival_confirm && phase>=arrival_confirm && phase<arrival_confirm+6)held=input::start;
        source.input.sample(held);
        const auto step=session.advance(timestamp(phase),held);
        phases+=step.video_phases;ticks+=step.logic_ticks;
        require(!step.time_clamped && !step.requested_experience && !step.requested_preview
            && !step.requested_settings_reset,"Natural death route changed native owner or clamped its clock");
        source.raster();
        const auto& game=session.game();
        const auto native=session.presentation(0,true);
        // A bus read updates the cartridge open-bus latch even through its
        // const API. Diagnostic observations must use the non-mutating RAM
        // peek; source and native timelines otherwise stop being comparable.
        const auto raw_flags=game.map().peek_ram_byte(*flags);
        require(raw_flags.has_value(),"Death-route GAMEFLAGS is not mapped RAM");
        const bool dead=(*raw_flags&0x42U)!=0;
        if(game.flow_state()==simulation::GameFlowState::gameplay && dead && !previous_dead)++deaths;
        previous_dead=dead;
        const auto meter=game.peek_meter_state();
        // The historically named M_DAMAGE is remaining shield/health, not
        // accumulated damage. The source meter caps it at player_health_max;
        // a fresh ship may have40 health while its displayed full gauge is36.
        // Observe a populated full meter before accepting a decrease/restart.
        const unsigned health=std::min(meter.damage,meter.player_health_max);
        if(meter.enabled && !dead && health==meter.player_health_max)full_health=health;
        if(full_health && meter.damage<full_health && !damaged)damaged=phase;
        if(damaged && dead && !dying)dying=phase;
        if(dying && dead && native.raster->circle.active && native.raster->circle.radius
            && (native.raster->circle.affected_layers&63))++circles;
        if(dying && !native.raster->brightness && !black)black=phase;
        if(black && !dead && game.flow_state()==simulation::GameFlowState::gameplay
            && native.raster->brightness==15 && meter.enabled && health==full_health) {
            if(!restored)restored=phase;
            ++bright;
        }else bright=0;
        if(continue_yes) {
            if(game.flow_state()==simulation::GameFlowState::game_over && !game_over) {
                require(damaged && dying && circles && black && deaths>1,
                    "Natural game-over omitted real reserve-consuming deaths/circles/fade");
                require(!native.raster->circle.active,"Final death circle leaked into GAME OVER");
                game_over=phase;
            }
            if(game.flow_state()==simulation::GameFlowState::continue_choice) {
                require(game_over,"Continue screen appeared without natural GAME OVER");
                if(!continue_screen)continue_screen=phase;
                if(!choice && native.raster->brightness==15)++continue_bright;
                if(continue_bright>=30 && !choice) {choice=phase+1;confirm=choice+12;}
                if(choice && phase==choice+10) {
                    const auto selected=game.map().peek_ram_byte(*option);
                    require(selected && *selected==(*continue_yes?0U:0xffU),
                        "Ordinary Continue UP/DOWN did not select the requested source option");
                }
            }
            if(confirm && phase>=confirm+6) {
                route_seen|=game.flow_state()==simulation::GameFlowState::planet_travel;
                // MAIN/PLANETSEQ waits in .rotateforabit for a NEW button
                // press after Continue returns to stage zero. Do not change
                // the player to auto-launch or fabricate an arrival flag.
                // Let the real source consume an ordinary fresh START after
                // its returned map is fully visible for thirty rasters. If
                // travel is not actually ready it refuses that press, and
                // the unchanged full-route recovery requirement still fails.
                if(*continue_yes && game.flow_state()==simulation::GameFlowState::planet_travel
                    && native.raster->brightness==15) {
                    if(!arrival_confirm && ++arrival_bright>=30)arrival_confirm=phase+1;
                }else if(!arrival_confirm)arrival_bright=0;
                const bool terminal=*continue_yes
                    ?game.flow_state()==simulation::GameFlowState::gameplay && route_seen && !dead
                        && meter.enabled && health==full_health && game.objects().is_active(game.player())
                    :game.flow_state()==simulation::GameFlowState::title;
                if(terminal && native.raster->brightness==15) {
                    if(!returned)returned=phase;
                    ++return_bright;
                }else return_bright=0;
            }
        }
        const bool finished=continue_yes?return_bright>=120:bright>=120;
        // Inspect every live death-effect phase, plus consecutive exact
        // source/audio windows. Maximum supported optics and original-model
        // mono use the same immutable completed source, never two ticks.
        const bool frontend=continue_yes && game_over && (!returned || return_bright<120);
        if(phase%60==0 || (dying && (!restored || dead)) || frontend || finished) {
            if(phase%60==0 || finished) {
                require(phases==phase && ticks==source.ticks && blocks==source.blocks,
                    "Death/restart changed source raster/logic/SPC cadence");
                require(game.save_state()==source.game.save_state(),"Natural death/restart cartridge state differs");
                require(session.audio().save_state()==source.spc.save_state() && pcm==source.pcm,
                    "Natural death/restart changed SPC handshakes or consecutive PCM");
                require(*native.raster->ppu==source.game.map().ppu_state()
                    && native.raster->brightness==source.game.map().display_brightness(),
                    "Natural death/restart replaced or delayed source raster/fade");
                pcm.clear();source.pcm.clear();++windows;
            }
            const auto state=game.save_state(),apu=session.audio().save_state();
            StereoSettings settings;settings.separation=64;settings.convergence=16;settings.strength=2;
            for(const auto& optics:std::array<std::pair<float,bool>,3>{{{0,true},{1,true},{1,false}}}) {
                const auto eye=session.presentation(optics.first,optics.second,settings);
                require(eye.current==native.current && eye.previous==native.previous && eye.raster==native.raster,
                    "Death/restart eye projection changed the completed source tick");
                const auto geometry=models.prepare(eye),ink=dots.prepare(eye);
                const auto fx=effects.prepare(eye);
                const auto occupied=geometry.vertices.size()+ink.vertices.size()+fx.colour.vertices.size()+fx.window.vertices.size();
                require(occupied<=pica_vertex_limit,"Death effects exceeded complete native scene geometry budget");
                const auto artwork=layers.prepare(eye,unsigned(pica_vertex_limit-occupied));
                const auto frame=composite.prepare(eye.plan,
                    std::array{artwork.before_models,ink,geometry,artwork.after_models,fx.colour,fx.window},
                    eye.dashboard,artwork.clear);
                validate_pica_frame(frame,eye.dashboard);++compositions;
                unsigned bytes=512U*256U*4U;
                for(const auto image:frame.textures)bytes+=pica_resident_texture_bytes(image);
                peak_bytes=std::max(peak_bytes,bytes);
                require(bytes<=pica_texture_budget,"Natural death/restart exceeded padded scene/lower-LCD residency");
                if(dying && dead && native.raster->circle.active && native.raster->circle.radius
                    && (native.raster->circle.affected_layers&63))
                    require(!fx.colour.draws.empty() && !fx.colour.draws.front().clip,
                        "Natural death circle was omitted or inherited the Controls demo clip");
                for(const auto& draw:fx.colour.draws)
                    require(draw.space==PicaSpace::screen && !draw.depth_test && !draw.depth_write,
                        "Death colour math entered finite model depth instead of screen-layer coverage");
                require(optics.second || !eye.plan.stereo,"Original-model death route fabricated a stereo eye");
            }
            require(game.save_state()==state && session.audio().save_state()==apu,
                "Death/restart composition or slider reads mutated source/SPC");
        }
        if(phase%600==0 || finished) {
            std::cout<<"Death/restart phase="<<phase<<" damage="<<damaged<<" dying="<<dying
                <<" circles="<<circles<<" black="<<black<<" restored="<<restored<<" bright="<<bright
                <<" health="<<unsigned(meter.damage)<<'/'<<full_health<<'\n';
            if(continue_yes)std::cout<<"Natural Continue "<<(*continue_yes?"YES":"NO")<<" deaths="<<deaths
                <<" game-over="<<game_over<<" continue="<<continue_screen<<" choice="<<choice<<" confirm="<<confirm
                <<" returned="<<returned<<" bright="<<return_bright<<" route-seen="<<route_seen
                <<" arrival-confirm="<<arrival_confirm
                <<" flow="<<unsigned(game.flow_state())<<'\n';
        }
        if(finished) {
            require(damaged && dying && circles && black && restored,
                "Natural death/restart lacked damage, live circle, black fade or recovery");
            if(continue_yes) {
                require(game_over && continue_screen && choice && confirm && returned,
                    "Natural Continue route omitted full reserve exhaustion/prompt/input/recovery");
                require(!*continue_yes || arrival_confirm,
                    "Continue YES omitted the source-required fresh map confirmation");
                std::cout<<"3DS natural GAME OVER -> Continue "<<(*continue_yes?"YES -> route/gameplay":"NO -> title")
                    <<" PASS: "<<windows<<" consecutive cartridge/SPC/PCM/raster parity windows, "
                    <<compositions<<" complete supported-eye/mono compositions, peak padded scene/lower textures "<<peak_bytes
                    <<" bytes. Ordinary stationary flight and source UP/DOWN/START only; no survival cheats or source writes. "
                    <<"HOST source/resource acceptance, not ARM/PICA pixels or physical audio/FPS acceptance.\n";
                return;
            }
            std::cout<<"3DS natural death/restart PASS: "<<windows<<" consecutive cartridge/SPC/PCM parity windows, "
                <<compositions<<" complete supported-eye/mono compositions, peak padded scene/lower textures "<<peak_bytes
                <<" bytes. Ordinary stationary flight, no source mutations or survival cheats. HOST source/resource checks; "
                "not game-over/continue, ARM/PICA pixels or physical audio/FPS acceptance.\n";
            return;
        }
    }
    throw std::runtime_error(continue_yes?"Natural game-over/Continue bound exhausted without full bright recovery"
        :"Natural death/restart bound exhausted without 120 bright recovered gameplay phases");
}
void handoff(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    GameSession session(rom,symbols,[](auto){ });
    const auto cartridge=session.cartridge_experience();
    session.advance(0,0);
    require(session.advance(0,input::a).duplicate && session.advance(0,0).duplicate,
        "Duplicate time did not preserve quick physical menu samples");
    const auto result=session.advance(50'000'000,0);
    require(result.requested_experience && *result.requested_experience!=cartridge,"Experience row did not request a cartridge handoff");
    const auto game=session.game().save_state(),apu=session.audio().save_state();
    const auto next=session.advance(5'000'000'000LL,input::start);
    require(next.requested_experience && !next.video_phases && !next.audio_blocks,"Wrong cartridge kept running after experience selection");
    require(game==session.game().save_state() && apu==session.audio().save_state(),"Pending cartridge handoff changed source/audio");
}
void presentation_cadence(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    for(const auto map:{"BOOT","LEVEL1_1"}) for(const unsigned rate:{30U,60U}) {
        std::vector<std::int16_t> pcm;unsigned phases{},blocks{},ticks{},renders{};
        GameSessionOptions options;options.preferences=GamePreferences{};
        options.preferences->render_fps=static_cast<std::uint8_t>(rate);options.preferences->show_fps=true;
        GameSession session(rom,symbols,[&](auto samples){pcm.insert(pcm.end(),samples.begin(),samples.end());++blocks;},map,{},options);
        SourceOracle source(rom,symbols,map);
        // Loading SD defaults also applies source-side god/laser/language
        // setters (including cartridge RAM writes and pending laser state).
        // Give the independent oracle those same user choices, not just FPS.
        source.game.set_god_mode(false);source.game.set_default_laser(0);source.game.set_language(0);
        source.game.set_selected_level(0);source.game.set_stereo_separation(16);source.game.set_stereo_convergence(1024);
        source.game.set_presentation_fps(static_cast<std::uint16_t>(rate));source.game.set_show_fps(true);
        require(session.game().save_state()==source.game.save_state(),"Output cadence fixture initialized different source preferences");
        PresentationClock gate;PresentationRate measured;session.advance(0,0);
        const auto present=[&](std::int64_t now) {
            const auto first=session.presentation(.5F,true);
            const auto second=session.presentation(1,true);
            require(first.current==second.current && first.raster==second.raster,
                "Native render-rate gate split the source timeline between eye reads");
            require(valid_image(first.dashboard,bottom_width,screen_height),"FPS overlay source dashboard invalid");
            measured.completed(now);++renders;
        };
        require(gate.due(0,rate),"New native output gate did not render its first frame");present(0);
        for(unsigned poll=1;poll<=1200;++poll) {
            const input::ButtonMask held=std::string_view(map)=="BOOT" && poll==32?input::start
                :std::string_view(map)!="BOOT" && poll>=120 && poll<400?input::ButtonMask(input::y|input::up|input::right):0;
            const auto now=timestamp(poll,240);source.input.sample(held);
            const auto result=session.advance(now,held);
            phases+=result.video_phases;ticks+=result.logic_ticks;
            require(!result.time_clamped && !result.requested_experience && !result.requested_preview,
                "Ordinary native cadence fixture clamped or changed source owner");
            if(poll%4==0) source.raster();
            if(gate.due(now,rate)) present(now);
            if(poll%240==0) {
                require(phases==poll/4 && ticks==source.ticks && blocks==source.blocks,
                    "30/60 presentation target altered source raster, logic or SPC cadence");
                require(session.game().save_state()==source.game.save_state(),"Output-rate gate changed independent source VM state");
                require(session.audio().save_state()==source.spc.save_state() && pcm==source.pcm,
                    "Output-rate gate changed independent SPC state, handshakes or PCM");
                require(renders==1+rate*(poll/240) && measured.fps()==rate,
                    "Native counter counted requested FPS, source rasters or stereo eyes instead of completed frames");
            }
        }
        require(phases==300 && blocks==100,"Five-second output fixture lost the fixed source/audio clock");
        const auto saved=session.save_state();auto restored=session.restored_state(saved);
        require(restored->save_state()==saved && restored->preferences().render_fps==rate && restored->preferences().show_fps,
            "Native full state lost supported output FPS/show-FPS preferences");
        std::cout<<"  "<<map<<" output "<<rate<<" Hz: "<<renders<<" whole presentations, 300 source rasters, 100 unchanged SPC blocks\n";
    }
    // A real decoded GAME archive, not a guessed byte offset, supplies the
    // cross-platform rates. The native candidate must bound only that setting.
    GameSession session(rom,symbols,[](auto){},"LEVEL1_1");session.advance(0,0);session.advance(timestamp(1),0);
    const auto saved=session.save_state();const auto crc=assets::crc32(rom.bytes());
    const auto packet=decode_game_state(saved,crc);
    for(const std::uint16_t rate:{0,20,30,60,90,480,65535}) {
        auto candidate=packet;auto desktop=session.game().restored_state(packet.game);
        desktop->set_presentation_fps(rate);desktop->set_show_fps(true);candidate.game=desktop->save_state();
        auto native=session.restored_state(encode_game_state(candidate,crc));
        const unsigned expected=rate<=30?30:60;
        require(native->game().presentation_fps()==expected && native->preferences().render_fps==expected
            && native->preferences().show_fps,"Valid desktop state leaked unsupported native output rate");
        const auto after=decode_game_state(native->save_state(),crc);
        require(native->game().map().save_state()==desktop->map().save_state() && after.audio==packet.audio
            && after.audio_phase==packet.audio_phase && after.pending_audio==packet.pending_audio,
            "Bounding desktop FPS changed the restored cartridge/SPC timeline");
        require(session.save_state()==saved,"Candidate output-rate normalization mutated the running owner");
    }
    GameSessionOptions invalid;invalid.preferences=GamePreferences{};invalid.preferences->render_fps=90;
    rejects([&]{GameSession rejected(rom,symbols,[](auto){},"BOOT",{},invalid);},"Invalid native SD FPS accepted by session");
}
void native_capability_restore(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    // Import a real decoded GAME packet with supported cartridge settings and
    // desktop-only rendering enabled. Do not edit guessed serialized offsets.
    for(const auto map:{"BOOT","LEVEL1_1"}) {
        GameSession session(rom,symbols,[](auto){},map);
        session.advance(0,0);session.advance(timestamp(1),0);
        const auto saved=session.save_state();const auto crc=assets::crc32(rom.bytes());
        const auto packet=decode_game_state(saved,crc);
        auto desktop=session.game().restored_state(packet.game);
        desktop->set_display_mode(simulation::DisplayMode::super_ultrawide_32_9);
        desktop->set_renderer_mode(simulation::RendererMode::software);
        desktop->set_render_scale(simulation::RenderScale::scale_6x);
        desktop->set_anti_aliasing_mode(simulation::AntiAliasingMode::heavy);
        desktop->set_aa_type(3);desktop->set_integer_scaling(true);
        desktop->set_enhanced_graphics(true);desktop->set_smooth_polys(true);
        desktop->set_rtx_lighting_intensity(3);desktop->set_two_d_filter(simulation::TwoDFilterMode::scalefx);
        desktop->set_effect(1);desktop->set_world_effect(2);desktop->set_bloom(3);desktop->set_bloom_2d(3);
        desktop->set_material(static_cast<std::uint8_t>(render::Effect::mirror));
        desktop->set_manipulation(static_cast<std::uint8_t>(render::Effect::twist));
        desktop->set_extra_effects({static_cast<std::uint8_t>(render::Effect::heat_wake),
            static_cast<std::uint8_t>(render::Effect::energy_shield),static_cast<std::uint8_t>(render::Effect::hologram)});
        desktop->set_enhanced_shadows(true);desktop->set_ray_tracing_quality(3);
        desktop->set_chromatic_aberration(3);desktop->set_hdr_effect(3);
        desktop->set_global_enhancements(0x03ffffff);desktop->set_scene_enhancements(255);
        desktop->set_depth_enhancements(15);desktop->set_particle_enhancements(15);
        desktop->set_phosphor_persistence(3);desktop->set_adaptive_exposure(3);
        desktop->set_water_caustics(3);desktop->set_camera_response(63);
        desktop->set_volumetric_fog(3);desktop->set_motion_blur(3);
        desktop->set_environment({1,1,1,1,1,1});desktop->set_vsync(true);
        require(desktop->effect() && desktop->world_effect() && desktop->material() && desktop->manipulation()
            && std::ranges::all_of(desktop->extra_effects(),[](auto value){return value!=0;})
            && desktop->environment()!=std::array<std::uint8_t,6>{},"Desktop capability fixture silently disabled its effects");
        desktop->set_msu1_available(true);desktop->set_msu1_music(true);
        desktop->set_music_volume(35);desktop->set_sfx_volume(55);
        desktop->set_timing_mode(simulation::TimingMode::unlocked_20_fps);
        desktop->set_show_fps(true);desktop->set_presentation_fps(90);
        desktop->set_swap_face_buttons(true);desktop->set_infinite_bombs(true);
        auto candidate=packet;candidate.game=desktop->save_state();
        const auto native=session.restored_state(encode_game_state(candidate,crc));
        const auto& game=native->game();
        require(!game.anti_aliasing() && !game.aa_type() && !game.integer_scaling()
            && !game.enhanced_graphics() && !game.smooth_polys() && !game.rtx_lighting()
            && game.two_d_filter()==simulation::TwoDFilterMode::off
            && !game.effect() && !game.world_effect() && !game.bloom() && !game.bloom_2d()
            && !game.enhanced_shadows() && !game.ray_tracing() && !game.reflective_surfaces_setting()
            && !game.chromatic_aberration() && !game.hdr_effect(),
            "Restoring desktop state silently enabled unsupported native rendering");
        require(!game.global_enhancements() && !game.scene_enhancements() && !game.depth_enhancements()
            && !game.particle_enhancements() && !game.phosphor_persistence() && !game.adaptive_exposure()
            && !game.water_caustics() && !game.camera_response() && !game.volumetric_fog() && !game.motion_blur()
            && game.environment()==std::array<std::uint8_t,6>{} && !game.material() && !game.manipulation()
            && game.extra_effects()==std::array<std::uint8_t,3>{},
            "Restoring desktop state retained ignored native material/environment/post effects");
        require(game.display_mode()==simulation::DisplayMode::standard_4_3
            && game.renderer_mode()==simulation::RendererMode::gpu
            && game.render_scale()==simulation::RenderScale::scale_1x && !game.vsync()
            && !game.msu1_available() && !game.msu1_music(),
            "Restoring desktop state advertised unsupported LCD/renderer/audio capabilities");
        require(game.presentation_fps()==60 && game.show_fps() && game.music_volume()==35 && game.sfx_volume()==55
            && game.timing_mode()==simulation::TimingMode::unlocked_20_fps
            && game.swap_face_buttons() && game.infinite_bombs(),
            "Native capability normalization erased supported preferences");
        const auto after=decode_game_state(native->save_state(),crc);
        require(game.map().save_state()==desktop->map().save_state() && after.audio==packet.audio
            && after.audio_phase==packet.audio_phase && after.pending_audio==packet.pending_audio
            && after.grid==packet.grid && after.scene_revision==packet.scene_revision,
            "Native capability normalization changed cartridge/SPC/grid timeline");
        require(session.save_state()==saved,"Preparing native capability candidate mutated the running session");
        const auto roundtrip=native->restored_state(native->save_state());
        require(roundtrip->save_state()==native->save_state(),"Normalized native state did not round-trip exactly");
    }
    std::cout<<"  Native capabilities: real BOOT/stage state imports clear desktop-only graphics/audio, preserve supported controls and VM/SPC/grid\n";
}
struct MenuDriver {
    GameSession& session;std::int64_t time{};GameAdvance last;
    explicit MenuDriver(GameSession& source):session(source) {session.advance(0,0);}
    void tap(input::ButtonMask button) {
        time+=50'000'000;last=session.advance(time,button);
        time+=50'000'000;const auto release=session.advance(time,0);
        if(release.requested_experience || release.requested_preview || release.requested_controller_remap || release.requested_hud_customization || release.requested_settings_reset) last=release;
    }
    void select(unsigned id) {
        const auto order=simulation::pregame_menu_order(session.game().pregame_page());
        require(std::find(order.begin(),order.end(),id)!=order.end(),"Fixture selected a row absent from source order");
        for(std::size_t i=0;session.game().pregame_selection()!=id && i<order.size();++i) tap(input::down);
        require(session.game().pregame_selection()==id,"Actual menu navigation did not reach requested source row");
    }
};
void fps_navigation_parity(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    constexpr std::array<input::ButtonMask,4> navigation{0,input::up,input::down,input::ButtonMask(input::up|input::down)};
    unsigned fixtures{};
    for(bool runtime:{false,true})for(unsigned rate:{30U,60U})for(auto nav:navigation) {
        std::vector<std::int16_t> pcm;
        const auto map=runtime?"LEVEL1_1":"BOOT";
        GameSessionOptions options;options.preferences=GamePreferences{};
        options.preferences->render_fps=static_cast<std::uint8_t>(rate);
        GameSession session(rom,symbols,[&](auto samples){pcm.insert(pcm.end(),samples.begin(),samples.end());},map,{},options);
        SourceOracle source(rom,symbols,map);
        // SD defaults invoke source-side setters, including EX's pending
        // laser/cartridge writes. Match those actual user choices in the
        // independent owner before comparing any input action.
        source.game.set_god_mode(false);source.game.set_default_laser(0);source.game.set_language(0);
        source.game.set_selected_level(0);source.game.set_stereo_separation(16);source.game.set_stereo_convergence(1024);
        source.game.set_presentation_fps(static_cast<std::uint16_t>(rate));
        require(session.game().save_state()==source.game.save_state(),"FPS fixture initialized different source/SD preferences");
        std::int64_t time{};session.advance(time,0);
        const auto compare=[&] {
            require(session.game().save_state()==source.game.save_state(),"Native FPS action changed independent source state");
            require(session.audio().save_state()==source.spc.save_state() && pcm==source.pcm,
                "Native FPS action changed source SPC handshakes or PCM");
        };
        if(runtime) {
            require(session.toggle_runtime_options() && source.game.toggle_runtime_options(),"FPS fixture could not open actual runtime options");
            source.input.reset();session.advance(++time,0);compare();
        }
        const auto step=[&](input::ButtonMask held) {
            time+=50'000'000;source.input.sample(held);
            const auto result=session.advance(time,held);
            require(result.video_phases>0 && result.video_phases<=3,"FPS action lost ordinary source raster timing");
            for(unsigned phase=0;phase<result.video_phases;++phase) {
                const auto before=source.game.presentation_fps();
                if(!runtime)source.raster();
                else {
                    const bool was_runtime=source.game.runtime_options_open();
                    source.game.present_frame();
                    if(source.game.logic_tick_ready()) {
                        const auto tick=source.game.tick(source.input.consume());
                        if(!was_runtime)source.pending.insert(source.pending.end(),tick.audio_port_writes.begin(),tick.audio_port_writes.end());
                        static_cast<void>(source.game.map().take_msu_register_writes());
                    }
                    if(!source.game.runtime_options_open() && ++source.phase%3==0) {
                        source.spc.render_stems_logic_tick(source.pending);source.pending.clear();
                        std::vector<std::int16_t> samples;
                        audio::mix_stems(source.spc.last_music_samples(),source.spc.last_effect_samples(),
                            source.game.music_volume(),source.game.sfx_volume(),samples);
                        source.pcm.insert(source.pcm.end(),samples.begin(),samples.end());
                        source.game.synchronize_apu_output_ports(source.spc.output_ports());
                    }
                }
                // Independent direct-source owner plus the native two-rate
                // policy. Do not call the production input/rate interceptor.
                if(source.game.presentation_fps()!=before)source.game.set_presentation_fps(before==30?60:30);
            }
            compare();return result;
        };
        const auto tap=[&](input::ButtonMask held) {step(held);step(0);};
        const auto select=[&](unsigned id) {
            const auto order=simulation::pregame_menu_order(source.game.pregame_page());
            for(std::size_t row=0;source.game.pregame_selection()!=id && row<order.size();++row)tap(input::down);
            require(source.game.pregame_selection()==id,"FPS fixture could not navigate to actual source row");
        };
        const auto order=simulation::pregame_menu_order(simulation::PregamePage::main);
        const auto target=std::size_t(std::find(order.begin(),order.end(),2U)-order.begin());
        const auto origin=nav?order[(target+((nav&input::up)?1U:order.size()-1U))%order.size()]:2U;
        select(origin);tap(input::ButtonMask(nav|input::a));
        unsigned expected=rate==30?60:30;
        require(session.game().pregame_selection()==2 && session.game().presentation_fps()==expected,
            "Direction+confirm did not toggle the destination native FPS row");
        for(auto action:{input::a,input::b,input::select,input::left,input::right}) {
            tap(action);expected=expected==30?60:30;
            require(session.preferences().render_fps==expected,"Native FPS action did not use the same 30/60 list");
        }
        step(input::right);expected=expected==30?60:30;
        require(session.game().presentation_fps()==expected,"Horizontal neutral tap lost FPS action");
        step(input::ButtonMask(input::left|input::right));step(input::left);
        require(session.game().presentation_fps()==expected,"Held horizontal direction bypassed source neutral-release guard");
        step(0);tap(input::left);expected=expected==30?60:30;
        require(session.game().presentation_fps()==expected,"Released horizontal direction could not toggle FPS again");
        tap(input::ButtonMask(input::up|input::a));
        require(session.game().pregame_selection()==1 && session.game().presentation_fps()==expected,
            "Leaving FPS row applied a rate change to the old row");
        select(2);step(input::ButtonMask(input::a|input::start));
        expected=expected==30?60:30;
        require(session.game().presentation_fps()==expected && !session.game().runtime_options_open(),
            "Combined FPS/Start action lost source ordering or leaked a desktop rate on menu exit");
        ++fixtures;
    }
    std::cout<<"  Native FPS: "<<fixtures<<" setup/runtime 30/60 fixtures, destination navigation, all actions, horizontal neutral release, old-row departure and combined Start ordering; exact source/SPC/PCM checked\n";
}
void actual_menu(const assets::RomImage& rom,const assets::SymbolMap& symbols,const std::filesystem::path& captures) {
    GameSessionOptions native;native.preferences=GamePreferences{};
    native.preferences->hud_layout.widgets[unsigned(HudWidget::shield)].x=20;
    GameSession session(rom,symbols,[](auto){},"BOOT",{},native);MenuDriver controls(session);
    GameMenu menu(rom,symbols);
    const auto observe=[&] {
        const auto before=session.game().save_state(),spc=session.audio().save_state();
        const auto state=GameMenu::capture(session.game());menu.update(state);
        const auto order=simulation::pregame_menu_order(session.game().pregame_page());
        require(state.rows.size()==order.size(),"Native UI replaced full source menu with a reduced menu");
        for(std::size_t i=0;i<order.size();++i) require(state.rows[i].id==order[i] && !state.rows[i].label.empty(),"Source menu row was missing/reordered/unlabelled");
        require(state.selection==session.game().pregame_selection(),"Rendered cursor was detached from source navigation");
        const auto source=session.presentation(1,true);validate_pica_frame(menu.frame(source.plan),source.dashboard);
        require(before==session.game().save_state() && spc==session.audio().save_state(),"Menu/font capture mutated source VM/SPC state");
        const auto redraws=menu.redraws();require(!menu.update(state) && menu.redraws()==redraws,"Unchanged source menu was rasterized again");
        return state;
    };
    observe();
    if(!captures.empty()) {
        std::filesystem::create_directories(captures);Canvas canvas(top_width);canvas.image(0,0,menu.plain_view());
        canvas.write_bmp((captures/"actual-main-menu.bmp").string());
    }
    controls.select(2);
    const auto fps=session.game().presentation_fps();controls.tap(input::a);
    require(fps==60 && session.game().presentation_fps()==30 && session.preferences().render_fps==30,
        "Native 30 Hz target did not reach source preferences/menu");
    auto render_row=GameMenu::capture(session.game());
    const auto shown_rate=std::find_if(render_row.rows.begin(),render_row.rows.end(),[](const auto& row){return row.id==2;});
    require(shown_rate!=render_row.rows.end() && shown_rate->enabled && shown_rate->value=="30 FPS","Native FPS row advertised the wrong target");
    controls.tap(input::b);require(session.game().presentation_fps()==60,"B did not toggle the native target safely");
    controls.tap(input::select);require(session.game().presentation_fps()==30,"Select leaked a desktop-only native target");
    controls.tap(input::left);require(session.game().presentation_fps()==60,"Left did not toggle native 30/60 target");
    controls.tap(input::up);controls.tap(input::down|input::a);
    require(session.game().pregame_selection()==2 && session.game().presentation_fps()==30,
        "Simultaneous menu navigation/action did not toggle the native render target");
    controls.select(14);controls.tap(input::a);
    require(session.game().pregame_page()==simulation::PregamePage::options,"Source Options action not used");observe();
    controls.select(1);controls.tap(input::a);
    require(session.game().show_fps() && session.preferences().show_fps,"Source show-FPS control was not persisted natively");observe();
    controls.select(6);const auto volume=session.game().music_volume();controls.tap(input::left);
    require(session.game().music_volume()<volume,"Source music volume did not change");observe();
    controls.select(12);controls.tap(input::right);
    require(session.game().language()==1,"Actual source language did not change");observe();
    controls.select(5);controls.tap(input::a);
    require(session.game().swap_face_buttons(),"Actual face-swap setting did not change");observe();
    controls.select(9);controls.tap(input::a);
    require(session.game().pregame_page()==simulation::PregamePage::stereo,"Source stereo submenu did not open");observe();
    controls.select(1);const auto separation=session.stereo_settings().separation;controls.tap(input::right);
    require(session.stereo_settings().separation>separation,"Native projection did not consume source separation control");observe();
    controls.select(2);const auto convergence=session.stereo_settings().convergence;controls.tap(input::right);
    require(session.stereo_settings().convergence>convergence,"Native projection did not consume source convergence control");observe();
    const auto optical=session.presentation(1,true,session.stereo_settings());
    require(!optical.plan.stereo && optical.plan.eye_count==1
        && optical.plan.convergence==session.stereo_settings().convergence,"Plain setup text acquired world stereo disparity");
    controls.tap(input::b);
    require(session.game().pregame_page()==simulation::PregamePage::options,"Source stereo Back failed");
    controls.select(0);controls.tap(input::a);
    require(session.game().pregame_page()==simulation::PregamePage::cheats,"Source Cheats action not used");
    for(auto id:simulation::pregame_menu_order(simulation::PregamePage::cheats)) {controls.select(id);observe();}
    controls.select(1);controls.tap(input::right);
    require(session.game().selected_level()==11,"Source level-selection list not used");
    controls.select(3);controls.tap(input::a);
    require(session.game().infinite_bombs(),"Source infinite-bombs action not used");
    controls.tap(input::b);controls.tap(input::b);
    require(session.game().pregame_page()==simulation::PregamePage::main,"Actual menu Back transition failed");
    for(unsigned page_row:{20U,21U,47U}) {
        controls.select(page_row);controls.tap(input::a);observe();
        const auto order=simulation::pregame_menu_order(session.game().pregame_page());
        for(auto id:order) {controls.select(id);observe();}
        controls.select(order.front());const auto state=observe();controls.tap(input::a);
        require(GameMenu::capture(session.game())==state,"Unavailable PICA effect silently changed its source setting");
        controls.tap(input::b);
        require(session.game().pregame_page()==simulation::PregamePage::main,"Graphics-page source Back failed");
    }
    const auto prefs=session.preferences();
    controls.select(16);controls.tap(input::a);
    require(controls.last.requested_preview==true && !controls.last.start_after_preview,"Preview did not request a real stage-owner restart");
    const auto frozen=session.game().save_state(),frozen_spc=session.audio().save_state();
    const auto pending=session.advance(controls.time+1'000'000'000,0);
    require(pending.requested_preview==true && !pending.video_phases && !pending.audio_blocks
        && frozen==session.game().save_state() && frozen_spc==session.audio().save_state(),"Pending preview ticked the old cartridge/audio");
    GameSessionOptions options;options.preferences=prefs;options.preview=true;unsigned progress{};
    options.preview_progress=[&](unsigned){++progress;return true;};
    const auto source_ram=session.cartridge_ram();
    require(session.cartridge_experience()==simulation::Experience::starfox_ex || source_ram.empty(),
        "Retail generic VM RAM was misidentified as battery-backed SRAM");
    const std::vector<std::uint8_t> saved_ram(source_ram.begin(),source_ram.end());
    GameSession preview(rom,symbols,[](auto){},"LEVEL1_1",saved_ram,options);MenuDriver preview_controls(preview);
    require(progress>0 && preview.preferences()==prefs,"Preview lost settings or skipped bounded source preroll");
    require(std::equal(preview.cartridge_ram().begin(),preview.cartridge_ram().end(),saved_ram.begin(),saved_ram.end()),
        "Preview source preroll changed the user's preserved cartridge SRAM");
    require(preview.game().menu_preview() && preview.game().peek_meter_state().enabled,"Preview froze the empty source initializer instead of gameplay");
    GameMenu preview_menu(rom,symbols);preview_menu.update(GameMenu::capture(preview.game()));
    const auto scene=preview.presentation(1,true,preview.stereo_settings());
    require(scene.plan.stereo,"Hardware slider was ignored in the real menu preview");
    require(scene.plan.separation==preview.stereo_settings().separation
        && scene.plan.convergence==preview.stereo_settings().convergence,"Configured native depth detached from preview eye matrices");
    GameModels models(preview.rom(),preview.symbols());GameLayers layers;PicaComposite composite;
    const auto shapes=models.prepare(scene);
    const auto art=layers.prepare(scene);
    const auto frame=composite.prepare(scene.plan,std::array{art.before_models,shapes,art.after_models,preview_menu.frame(scene.plan)},scene.dashboard,art.clear);
    validate_pica_frame(frame,scene.dashboard);
    require(!shapes.vertices.empty() && !art.before_models.textures.empty(),"Preview substituted a mono placeholder for real PICA models/backgrounds");
    const auto game=preview.game().save_state(),apu=preview.audio().save_state();
    for(float slider:{0.F,.5F,1.F}) {
        const auto eye=preview.presentation(slider,true);
        preview_menu.update(GameMenu::capture(preview.game()));validate_pica_frame(preview_menu.frame(eye.plan),eye.dashboard);
    }
    require(game==preview.game().save_state() && apu==preview.audio().save_state(),"Preview/slider changed frozen cartridge state");
    preview_controls.tap(input::start);
    require(preview_controls.last.requested_preview==false && preview_controls.last.start_after_preview,"Preview Start did not request the real BOOT/Start path");
    options.preview=false;options.start_after_preview=true;
    GameSession started(rom,symbols,[](auto){},"BOOT",{},options);started.advance(0,0);
    for(unsigned phase=1;phase<=180;++phase) started.advance(timestamp(phase),0);
    require(!started.game().in_setup_menu() && started.preferences()==prefs,"Preview Start failed to launch through source fade/selected level");
    options.preview=true;options.start_after_preview=false;options.preview_progress=[](unsigned){return false;};
    rejects([&]{GameSession cancelled(rom,symbols,[](auto){},"LEVEL1_1",{},options);},"Cancelled preview was published as a playable owner");
    std::cout<<"  Actual setup: all source pages/rows, font/cache/protected UI, real preview geometry, settings and Start handoff checked\n";
}
}
namespace {
void actual_disk_handoff(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    struct Temp {
        std::filesystem::path path;
        Temp() {
            const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
            for(unsigned i=0;i<64;++i) {
                const auto candidate=std::filesystem::temp_directory_path()/("sfe-3ds-real-save-"+std::to_string(stamp)+"-"+std::to_string(i));
                if(std::filesystem::create_directory(candidate)) {path=candidate;return;}
            }
            throw std::runtime_error("Cannot create local cartridge-save fixture directory");
        }
        ~Temp() {
            std::error_code ignored;
            for(unsigned i=0;i<2;++i) std::filesystem::remove(path/("3ds-save-"+std::to_string(i)+".dat"),ignored);
            std::filesystem::remove(path,ignored);
        }
    } temp;
    constexpr std::uint32_t manifest=0x76543210;
    GamePreferences prefs{simulation::TimingMode::original_speed,35,55,2,2,35,true,true,true,true,true,true,32,4096,30,true};
    prefs.hud_layout.widgets[unsigned(HudWidget::radio)]={44,12,3,true};
    prefs.hud_layout.widgets[unsigned(HudWidget::portrait)]={108,120,5,false};
    GameSessionOptions options;options.preferences=prefs;
    GameSession source(rom,symbols,[](auto){},"BOOT",{},options);
    require(source.preferences()==prefs,"Real cartridge did not accept persisted supported settings");
    const auto vm=source.game().save_state(),spc=source.audio().save_state();
    GameSaveData record;record.experience=source.cartridge_experience();record.preferences=source.preferences();
    const auto ram=source.cartridge_ram();record.ex_sram.assign(ram.begin(),ram.end());
    if(!ram.empty()) record.ex_rom_crc=assets::crc32(rom.bytes());
    GameStorage storage(temp.path.generic_string(),manifest);static_cast<void>(storage.load());
    require(storage.save(record),"Actual cartridge settings/SRAM did not reach disk");
    require(vm==source.game().save_state() && spc==source.audio().save_state(),"Disk checkpoint mutated actual source VM/SPC");
    GameStorage reopened(temp.path.generic_string(),manifest);
    const auto loaded=reopened.load();require(loaded.found && loaded.data==record,"Reopening lost real cartridge save data");
    options.preferences=loaded.data.preferences;
    GameSession resumed(rom,symbols,[](auto){},"BOOT",loaded.data.ex_sram,options);
    require(resumed.preferences()==prefs && resumed.cartridge_experience()==record.experience,
        "Disk settings were not applied to the real native session");
    require(std::equal(resumed.cartridge_ram().begin(),resumed.cartridge_ram().end(),record.ex_sram.begin(),record.ex_sram.end()),
        "Real EX boot changed/ignored the loaded SRAM bank");
    if(source.cartridge_experience()==simulation::Experience::starfox_ex)
        require(record.ex_sram.size()==65'536 && record.ex_rom_crc==assets::crc32(rom.bytes()),"Native EX save bank lost its cartridge identity");
    else require(record.ex_sram.empty() && record.ex_rom_crc==0,"Retail generic VM RAM leaked into disk SRAM");
    auto newer=record;newer.preferences.music=75;require(reopened.save(newer),"Second actual settings generation failed");
    {
        std::ofstream partial(storage.slot_path(1),std::ios::binary|std::ios::trunc);
        partial.exceptions(std::ios::badbit|std::ios::failbit);partial.write("bad",3);partial.close();
    }
    GameStorage recovered(temp.path.generic_string(),manifest);const auto fallback=recovered.load();
    require(fallback.found && fallback.writable && !fallback.warning.empty() && fallback.data==record,
        "Interrupted actual settings save did not retain the preceding cartridge save");
    options.preferences=fallback.data.preferences;
    GameSession old_valid(rom,symbols,[](auto){},"BOOT",fallback.data.ex_sram,options);
    require(old_valid.preferences()==prefs,"Recovered settings did not rebind the actual native owner");
    std::cout<<"  Disk handoff: actual settings/ROM-bound EX SRAM reopen and interrupted-slot recovery checked\n";
}
void actual_settings_reset(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    constexpr auto chord=input::ButtonMask(input::left_shoulder|input::right_shoulder);
    GameSessionOptions options;
    options.preferences=GamePreferences{simulation::TimingMode::unlocked_20_fps,35,55,2,2,35,true,true,true,true,true,true,32,4096};
    GameSession menu(rom,symbols,[](auto){},"BOOT",{},options);MenuDriver controls(menu);
    controls.select(14);controls.tap(input::a);controls.select(12); // Real Options page.
    const auto prefs=menu.preferences();const auto start=controls.time+1;
    require(!menu.advance(start,chord).requested_settings_reset && menu.settings_reset_hold().active(),"Real setup did not arm mapped reset");
    // Poll independent of the source clock; the source must not mutate its
    // settings or request a replacement until the complete five-second hold.
    for(unsigned frame=1;frame<100;++frame)
        require(!menu.advance(start+std::int64_t(frame)*50'000'000,chord).requested_settings_reset,"Source settings reset fired early");
    require(!menu.advance(start+4'999'999'999LL,chord).requested_settings_reset && menu.preferences()==prefs,"Incomplete hold changed settings");
    const auto before=menu.game().save_state(),spc=menu.audio().save_state();
    const auto bank=std::vector<std::uint8_t>(menu.cartridge_ram().begin(),menu.cartridge_ram().end());
    const auto fired=menu.advance(start+SettingsResetHold::duration,chord);
    require(fired.requested_settings_reset && !fired.video_phases && !fired.logic_ticks && !fired.audio_blocks,"Reset boundary advanced the owner being retired");
    require(before==menu.game().save_state() && spc==menu.audio().save_state(),"Reset request partly reset source VM/SPC before handoff");
    const auto pending=menu.advance(start+20'000'000'000LL,input::start|chord);
    require(pending.requested_settings_reset && !pending.video_phases && !pending.audio_blocks
        && before==menu.game().save_state() && spc==menu.audio().save_state(),"Old source/audio kept running after settings-reset request");
    GameSaveData record;record.experience=menu.cartridge_experience();record.preview=true;record.preferences=menu.preferences();
    record.ex_sram=bank;if(!bank.empty()) record.ex_rom_crc=assets::crc32(rom.bytes());
    const auto defaults=default_game_settings(record);
    require(defaults.experience==simulation::Experience::original && !defaults.preview
        && defaults.ex_sram==record.ex_sram && defaults.ex_rom_crc==record.ex_rom_crc,"Real reset record erased game progress or retained preview/EX setup");
    options.preferences=defaults.preferences;
    GameSession rebuilt(rom,symbols,[](auto){},"BOOT",bank,options);
    require(rebuilt.preferences()==GamePreferences{} && rebuilt.game().in_setup_menu() && !rebuilt.game().menu_preview(),"Fresh actual BOOT retained settings/preview instead of defaults");
    require(std::equal(bank.begin(),bank.end(),rebuilt.cartridge_ram().begin(),rebuilt.cartridge_ram().end()),"Default source reconstruction discarded real game progress");
    rebuilt.advance(0,0,false);
    require(!rebuilt.advance(9'000'000'000LL,chord).requested_settings_reset && !rebuilt.settings_reset_hold().active(),"Held buttons leaked into replacement BOOT");
    rebuilt.advance(10'000'000'000LL,0);
    require(!rebuilt.advance(11'000'000'000LL,chord).requested_settings_reset && rebuilt.settings_reset_hold().active(),"Released reset could not rearm on new BOOT");
    rebuilt.advance(15'000'000'000LL,chord);
    rebuilt.advance(15'500'000'000LL,0,false);
    require(!rebuilt.settings_reset_hold().active(),"Home/sleep did not cancel the live menu hold");
    require(!rebuilt.advance(30'000'000'000LL,chord).requested_settings_reset,"Resume turned a suspended hold into reset");
    rebuilt.advance(30'500'000'000LL,0);rebuilt.advance(31'000'000'000LL,chord);
    rebuilt.advance(31'000'000'000LL,input::left_shoulder);
    require(!rebuilt.settings_reset_hold().active(),"Duplicate-time shoulder release did not cancel actual menu hold");
    rebuilt.advance(32'000'000'000LL,chord);rebuilt.advance(31'000'000'000LL,chord);
    require(!rebuilt.settings_reset_hold().active(),"Rewound clock retained a native reset hold");
    GameSession stage(rom,symbols,[](auto){},"LEVEL1_1");stage.advance(0,chord);
    require(!stage.game().in_setup_menu(),"Direct-stage reset exclusion fixture is not a real stage");
    for(unsigned frame=1;frame<=120;++frame) {
        const auto next=stage.advance(std::int64_t(frame)*50'000'000,chord);
        require(!next.requested_settings_reset && !stage.settings_reset_hold().active(),"In-game roll shoulders triggered menu factory reset");
    }
    std::cout<<"  Settings reset: mapped five-second menu chord, pure owner handoff, defaults/game-save preservation, focus/release/clock guards checked\n";
}
void actual_controller_remap(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    GameSession session(rom,symbols,[](auto){});MenuDriver controls(session);
    rejects([&]{session.finish_controller_remap();},"Unrequested controller editor acknowledged");
    controls.select(14);controls.tap(input::a);controls.select(8);
    const auto menu=GameMenu::capture(session.game());
    const auto row=std::find_if(menu.rows.begin(),menu.rows.end(),[](const auto& item){return item.id==8;});
    require(row!=menu.rows.end() && row->enabled && row->value=="A  OPEN","Actual Controller option remains unavailable");
    controls.tap(input::a);
    require(controls.last.requested_controller_remap && session.controller_remap_pending()
        && session.game().pregame_page()==simulation::PregamePage::options && session.game().pregame_selection()==8,
        "Controller action did not freeze at the actual source Options row");
    const auto before=session.game().save_state(),spc=session.audio().save_state();
    const auto waiting=session.advance(controls.time+10'000'000'000LL,input::a|input::left_shoulder|input::right_shoulder);
    require(waiting.requested_controller_remap && !waiting.video_phases && !waiting.audio_blocks
        && before==session.game().save_state() && spc==session.audio().save_state(),"Controller host screen ticked old source/audio/reset");
    GameRemap editor;editor.open({});editor.update({});
    const auto tap=[&](input::ButtonMask physical) {editor.update({physical});editor.update({});};
    for(unsigned i=0;i<8;++i) tap(input::down);
    tap(input::a);editor.update({input::a});editor.update({});
    tap(input::down);tap(input::a);editor.update({input::b});editor.update({});
    const auto bindings=editor.bindings();
    require(bindings.sources[8]==8 && bindings.sources[9]==0,"Real host editor did not map shoulders to face buttons");
    require(before==session.game().save_state() && spc==session.audio().save_state(),"Editing bindings mutated the source VM/SPC");
    session.finish_controller_remap();
    require(!session.controller_remap_pending(),"Controller screen could not return to actual Options");
    const auto base=controls.time+20'000'000'000LL;
    auto result=session.advance(base,input::a|input::b,true,mapped_buttons(bindings,{input::a|input::b}));
    require(!result.requested_controller_remap && !result.requested_settings_reset && !session.settings_reset_hold().active(),"Captured physical input leaked out of controller screen");
    session.advance(base+50'000'000,0,true,0);
    const auto begin=base+100'000'000;const auto prefs=session.preferences();
    session.advance(begin,input::a|input::b,true,mapped_buttons(bindings,{input::a|input::b}));
    for(unsigned frame=1;frame<100;++frame) {
        result=session.advance(begin+std::int64_t(frame)*50'000'000,input::a|input::b,true,mapped_buttons(bindings,{input::a|input::b}));
        require(!result.requested_controller_remap && !result.requested_settings_reset
            && session.game().pregame_selection()==8 && session.preferences()==prefs,"Mapped reset chord also changed fixed menu settings/navigation");
    }
    require(session.advance(begin+SettingsResetHold::duration,input::a|input::b,true,mapped_buttons(bindings,{input::a|input::b})).requested_settings_reset,
        "Remapped in-game L/R did not trigger the five-second reset");
    // Independent real cartridge oracle for custom gameplay bindings. No
    // production mapping helper is used to build the expected logical inputs.
    std::vector<std::int16_t> pcm;
    GameSession stage(rom,symbols,[&](auto block){pcm.insert(pcm.end(),block.begin(),block.end());},"LEVEL1_1");
    SourceOracle direct(rom,symbols,"LEVEL1_1");GameBindings custom;
    custom.sources[0]=12;custom.sources[4]=0;custom.sources[5]=255;custom.sources[6]=255;custom.sources[8]=8;custom.sources[9]=9;
    stage.advance(0,0);
    for(unsigned poll=1;poll<=240;++poll) {
        PadSample pad;input::ButtonMask expected{};
        if(poll>=16 && poll<44) {pad.physical=input::b;expected=input::a;}
        if(poll>=64 && poll<92) {pad.physical=input::a;expected=input::left_shoulder;}
        if(poll>=112 && poll<140) {pad.physical=input::x;expected=input::right_shoulder;}
        if(poll>=160 && poll<188) {pad.circle_y=80;expected=input::up;}
        if(poll>=208 && poll<236) {pad.circle_x=80;expected=input::right;}
        require(mapped_buttons(custom,pad)==expected,"Custom sampler differs from independent native logical-input oracle");
        const auto advanced=stage.advance(timestamp(poll,240),fixed_menu_buttons(pad),true,mapped_buttons(custom,pad));
        direct.input.sample(expected);if(poll%4==0) direct.raster();
        require(!advanced.requested_settings_reset && !advanced.requested_controller_remap,"Gameplay remapping opened a host editor/reset");
        if(poll%4==0) require(stage.game().save_state()==direct.game.save_state() && stage.audio().save_state()==direct.spc.save_state()
            && pcm==direct.pcm,"Actual remapped gameplay VM/SPC/PCM diverged from direct cartridge inputs");
    }
    std::cout<<"  Controller: actual source row/editor freeze/return, mapped reset and 60 independent gameplay rasters/SPC/PCM checked\n";
}
void full_state_parity(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    const auto crc=assets::crc32(rom.bytes());std::size_t maximum_bytes{};
    struct Temp {
        std::filesystem::path path;std::uint32_t crc;
        explicit Temp(std::uint32_t cartridge):crc(cartridge) {
            const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
            for(unsigned i=0;i<64;++i) {
                const auto candidate=std::filesystem::temp_directory_path()/("sfe-3ds-real-states-"+std::to_string(stamp)+"-"+std::to_string(i));
                if(std::filesystem::create_directory(candidate)) {path=candidate;return;}
            }
            throw std::runtime_error("Cannot create isolated actual state fixture directory");
        }
        ~Temp() {
            std::error_code ignored;GameStateStorage storage(path.generic_string(),0x76543210,crc);
            for(unsigned slot=0;slot<6;++slot) for(unsigned gen=0;gen<2;++gen) std::filesystem::remove(storage.slot_path(slot,gen),ignored);
            std::filesystem::remove(path,ignored);
        }
    } temp(crc);
    for(const auto map:{"BOOT","LEVEL1_1"}) for(unsigned partial=0;partial<3;++partial) {
        std::vector<std::int16_t> pcm;
        auto session=std::make_unique<GameSession>(rom,symbols,[&](auto samples){pcm.insert(pcm.end(),samples.begin(),samples.end());},map);
        SourceOracle oracle(rom,symbols,map);session->advance(0,0);
        const auto before_rasters=30+partial;
        for(unsigned i=1;i<=before_rasters;++i) {session->advance(timestamp(i),0);oracle.raster();}
        require(session->game().save_state()==oracle.game.save_state() && session->audio().save_state()==oracle.spc.save_state()
            && pcm==oracle.pcm,"State fixture diverged before save");
        const auto game=session->game().save_state(),audio=session->audio().save_state();
        const auto scene=session->presentation(1,true);const auto state=session->save_state();maximum_bytes=std::max(maximum_bytes,state.size());
        const auto decoded=decode_game_state(state,crc);
        require(decoded.audio_phase==partial && decoded.pending_audio==oracle.pending,"Full state omitted pending APU writes/partial audio phase");
        require(decoded.game==game && decoded.audio==audio && session->presentation(1,true).current==scene.current,
            "State capture mutated live VM/SPC/published scene");
        // Corrupt outer/component data and a checksummed invalid native field
        // must leave the old owner, its PCM sink and immutable raster untouched.
        auto corrupt=state;corrupt.back()^=1;
        rejects([&]{static_cast<void>(session->restored_state(corrupt));},"Corrupt state replaced live source");
        auto wrong_audio=decoded;wrong_audio.audio.back()^=1;
        rejects([&]{static_cast<void>(encode_game_state(wrong_audio,crc));},"Corrupt SPC component accepted");
        // Valid checksums are not a substitute for complete component decode.
        auto invalid_game=decoded;invalid_game.game=state::pack(0x47414d01U,crc,{});
        const auto malformed_game=encode_game_state(invalid_game,crc);
        rejects([&]{static_cast<void>(session->restored_state(malformed_game));},"Checksummed invalid VM was loaded");
        auto invalid_spc=decoded;invalid_spc.audio=state::pack(0x53504301U,0,{});
        const auto malformed_spc=encode_game_state(invalid_spc,crc);
        rejects([&]{static_cast<void>(session->restored_state(malformed_spc));},"Checksummed invalid SPC partly replaced live VM");
        rejects([&]{static_cast<void>(decode_game_state(state,crc^1));},"Foreign cartridge state accepted");
        require(session->game().save_state()==game && session->audio().save_state()==audio
            && session->presentation(1,true).current==scene.current && pcm==oracle.pcm,"Rejected state changed the running owner");
        const auto slot=(std::string_view(map)=="BOOT"?0U:3U)+partial;
        GameStateStorage disk(temp.path.generic_string(),0x76543210,crc);static_cast<void>(disk.load(slot));
        require(disk.save(slot,state),"Actual VM/SPC state did not reach disk");
        {
            std::ofstream interrupted(disk.slot_path(slot,1),std::ios::binary|std::ios::trunc);interrupted<<"partial";
        }
        GameStateStorage reopened(temp.path.generic_string(),0x76543210,crc);const auto loaded=reopened.load(slot);
        require(loaded.bytes==state && loaded.info.found && loaded.info.writable && !loaded.info.warning.empty(),
            "Interrupted SD state failed to retain actual prior VM/SPC timeline");
        auto next=session->restored_state(loaded.bytes);
        require(next->save_state()==state && pcm==oracle.pcm,"Prepared restore changed state or queued boot/preroll PCM");
        require(next->presentation(1,true).previous==next->presentation(1,true).current,
            "State load interpolated against the discarded run");
        session=std::move(next); // Destroy all old ROM/symbol/VM owners before continuation.
        pcm.clear();oracle.pcm.clear();oracle.input.reset();
        constexpr std::int64_t resumed=90'000'000'000LL;
        session->advance(resumed,input::start);session->advance(resumed+1,0);
        require(session->game().save_state()==oracle.game.save_state() && session->audio().save_state()==oracle.spc.save_state(),
            "State load caught up SD time or accepted held Start");
        for(unsigned i=1;i<=24;++i) {
            const auto step=session->advance(resumed+1+timestamp(i),0);oracle.raster();
            require(step.video_phases==1 && session->game().save_state()==oracle.game.save_state(),"Restored VM/source pace differs from independent continuation");
            require(session->audio().save_state()==oracle.spc.save_state() && pcm==oracle.pcm,"Restored partial SPC block duplicated/dropped handshakes or PCM");
        }
        // Carry is a source presentation checkpoint, not a pointer into a retired owner.
        auto carried=decode_game_state(session->save_state(),crc);
        carried.scene_revision=300;carried.grid={true,true,300,{17,-9},{-23,12}};carried.grid_start={-23,12};
        const auto ink=encode_game_state(carried,crc);auto with_ink=session->restored_state(ink);
        require(with_ink->save_state()==ink && with_ink->presentation(1,true).current->grid_line_start==carried.grid_start,
            "State discarded asymmetric source grid carry");
    }
    // The host freezes source state at the real audio partial phase even while
    // its shared runtime options animate/navigate. Closing rebases wall time.
    for(unsigned partial=0;partial<3;++partial) {
        std::vector<std::int16_t> pcm;
        GameSession session(rom,symbols,[&](auto samples){pcm.insert(pcm.end(),samples.begin(),samples.end());},"LEVEL1_1");
        SourceOracle oracle(rom,symbols,"LEVEL1_1");session.advance(0,0);
        for(unsigned i=1;i<=30+partial;++i) {session.advance(timestamp(i),0);oracle.raster();}
        const auto map=session.game().map().save_state(),audio=session.audio().save_state();
        require(session.toggle_runtime_options() && session.game().runtime_options_open(),"Native runtime options are not accessible");
        rejects([&]{static_cast<void>(session.save_state());},"Transient runtime menu saved as an incompatible source timeline");
        session.advance(1'000'000'000,0);
        for(unsigned i=1;i<=60;++i) session.advance(1'000'000'000+timestamp(i),0);
        require(session.game().map().save_state()==map && session.audio().save_state()==audio && pcm==oracle.pcm,
            "Runtime options advanced cartridge/SPC or queued audio");
        require(session.toggle_runtime_options() && !session.game().runtime_options_open(),"Native runtime options did not close");
        constexpr std::int64_t resumed=3'000'000'000;
        session.advance(resumed,0);pcm.clear();oracle.pcm.clear();
        for(unsigned i=1;i<=24;++i) {
            const auto step=session.advance(resumed+timestamp(i),0);oracle.raster();
            if(session.game().map().save_state()!=oracle.game.map().save_state()
                || session.audio().save_state()!=oracle.spc.save_state() || pcm!=oracle.pcm) {
                const auto a=session.game().map().save_state(),b=oracle.game.map().save_state();
                std::cerr<<"Runtime resume mismatch: phase "<<partial<<", raster "<<i
                    <<", advance phases/ticks/audio "<<step.video_phases<<'/'<<step.logic_ticks<<'/'<<step.audio_blocks
                    <<", oracle phase "<<oracle.phase%3<<", native phase "<<unsigned(decode_game_state(session.save_state(),crc).audio_phase)
                    <<", map/audio/PCM "<<(a==b)<<'/'<<(session.audio().save_state()==oracle.spc.save_state())<<'/'<<(pcm==oracle.pcm)<<"; map offsets:";
                unsigned printed=0;for(std::size_t j=0;j<std::min(a.size(),b.size()) && printed<16;++j)
                    if(a[j]!=b[j]) {std::cerr<<' '<<j<<':'<<unsigned(a[j])<<'/'<<unsigned(b[j]);++printed;}
                const auto sa=session.audio().save_state(),sb=oracle.spc.save_state();
                std::cerr<<"; SPC offsets:";printed=0;
                for(std::size_t j=0;j<std::min(sa.size(),sb.size()) && printed<24;++j)
                    if(sa[j]!=sb[j]) {std::cerr<<' '<<j<<':'<<unsigned(sa[j])<<'/'<<unsigned(sb[j]);++printed;}
                std::cerr<<'\n';
            }
            require(session.game().map().save_state()==oracle.game.map().save_state()
                && session.audio().save_state()==oracle.spc.save_state() && pcm==oracle.pcm,
                "Runtime options resume lost partial source/audio phase");
        }
    }
    std::cout<<"  Full states: BOOT/stage, audio phases 0/1/2, retired-owner continuation and runtime options; largest fixture "<<maximum_bytes<<" bytes\n";
}
void editor_resume_parity(const assets::RomImage& rom,const assets::SymbolMap& symbols,bool hud_editor=false) {
    std::vector<std::int16_t> pcm;
    GameSession session(rom,symbols,[&](auto samples){pcm.insert(pcm.end(),samples.begin(),samples.end());});
    SourceOracle source(rom,symbols,"BOOT");
    std::int64_t time{};session.advance(time,0);
    const auto compare=[&] {
        require(session.game().save_state()==source.game.save_state(),"Controller editor return changed independent source state");
        require(session.audio().save_state()==source.spc.save_state() && pcm==source.pcm,
            "Controller editor discarded/replayed partial SPC cadence or pending handshakes");
    };
    const auto step=[&](input::ButtonMask held,bool editor_action=false) {
        time+=50'000'000;
        source.input.sample(editor_action?0:held);
        const auto result=session.advance(time,held);
        require(result.video_phases>0 && result.video_phases<=3,"Editor navigation lost its ordinary raster clock");
        for(unsigned i=0;i<result.video_phases;++i) source.raster();
        compare();return result;
    };
    const auto tap=[&](input::ButtonMask held) {step(held);step(0);};
    const auto select=[&](unsigned id) {
        const auto order=simulation::pregame_menu_order(session.game().pregame_page());
        for(std::size_t i=0;session.game().pregame_selection()!=id && i<order.size();++i) tap(input::down);
        require(session.game().pregame_selection()==id,"Independent resume fixture could not select source menu row");
    };
    select(14);tap(input::a);select(hud_editor?3:8);
    const auto opening=step(input::a,true);
    require(hud_editor?opening.requested_hud_customization:opening.requested_controller_remap,"Independent resume fixture did not open native editor");
    source.input.reset();
    const auto frozen_phase=source.phase;
    const auto wait=session.advance(time+60'000'000'000LL,input::start);
    require(!wait.video_phases && !wait.audio_blocks && source.phase==frozen_phase,"Editor wait advanced source audio/raster");
    compare();
    if(hud_editor) {
        auto layout=session.preferences().hud_layout;layout.widgets[unsigned(HudWidget::shield)].x=20;
        session.finish_hud_customization(layout);
    } else session.finish_controller_remap();
    time+=60'000'000'001LL;
    require(!session.advance(time,input::a).video_phases,"Editor close caught up wall-clock time");
    source.input.reset();session.advance(++time,0);compare();
    // Resume one raster at a time, including across every partial 50ms audio
    // boundary. The independent oracle retains its own phase and APU writes.
    const auto resumed=time;
    for(unsigned raster=1;raster<=24;++raster) {
        const auto result=session.advance(resumed+timestamp(raster),0);
        require(result.video_phases==1,"Editor return duplicated/dropped source raster");
        source.raster();compare();
    }
    std::cout<<(hud_editor?"  HUD editor":"  Controller")<<" resume: independent VM/SPC/PCM parity through partial audio phase "<<frozen_phase%3
        <<" and 24 post-editor rasters checked\n";
}
void editor_navigation_parity(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    // Independent cartridge/SPC owner, normal input only. Exercise both sides
    // of each native editor and both Up+Down/Back priorities in setup/runtime.
    constexpr std::array<input::ButtonMask,4> navigation{0,input::up,input::down,input::ButtonMask(input::up|input::down)};
    constexpr std::array<input::ButtonMask,3> modifiers{0,input::b,input::start};
    unsigned fixtures{};
    for(bool runtime:{false,true})for(unsigned target:{3U,8U})for(auto nav:navigation)for(auto modifier:modifiers) {
        std::vector<std::int16_t> pcm;
        const auto map=runtime?"LEVEL1_1":"BOOT";
        GameSession session(rom,symbols,[&](auto samples){pcm.insert(pcm.end(),samples.begin(),samples.end());},map);
        SourceOracle source(rom,symbols,map);std::int64_t time{};session.advance(time,0);
        const auto compare=[&] {
            require(session.game().save_state()==source.game.save_state(),"Editor navigation changed independent source state");
            require(session.audio().save_state()==source.spc.save_state() && pcm==source.pcm,
                "Editor navigation lost source handshakes, partial SPC phase or PCM");
        };
        if(runtime) {
            require(session.toggle_runtime_options() && source.game.toggle_runtime_options(),"Editor chord fixture could not open runtime options");
            source.input.reset();session.advance(++time,0);compare();
        }
        const auto step=[&](input::ButtonMask physical,input::ButtonMask expected) {
            time+=50'000'000;source.input.sample(expected);
            const auto result=session.advance(time,physical);
            require(result.video_phases>0 && result.video_phases<=3,"Editor chord lost its ordinary source raster clock");
            for(unsigned phase=0;phase<result.video_phases;++phase) {
                if(!runtime)source.raster();
                else {
                    source.game.present_frame();
                    if(source.game.logic_tick_ready())static_cast<void>(source.game.tick(source.input.consume()));
                }
            }
            compare();return result;
        };
        const auto tap=[&](input::ButtonMask held) {step(held,held);step(0,0);};
        const auto select=[&](unsigned id) {
            const auto order=simulation::pregame_menu_order(session.game().pregame_page());
            for(std::size_t row=0;session.game().pregame_selection()!=id && row<order.size();++row)tap(input::down);
            require(session.game().pregame_selection()==id,"Editor chord fixture could not select source row");
        };
        select(14);tap(input::a);
        const auto order=simulation::pregame_menu_order(simulation::PregamePage::options);
        const auto destination=std::size_t(std::find(order.begin(),order.end(),target)-order.begin());
        const auto origin=nav?order[(destination+((nav&input::up)?1U:order.size()-1U))%order.size()]:target;
        select(origin);
        const bool opens=modifier==0;
        const auto chord=input::ButtonMask(nav|input::a|modifier);
        const auto opening=step(chord,opens?nav:chord);
        require(opening.requested_hud_customization==(opens && target==3)
            && opening.requested_controller_remap==(opens && target==8),
            "Same-tick direction/confirm opened the wrong native editor or stole Back/Start");
        if(opens) {
            require(session.game().pregame_page()==simulation::PregamePage::options && session.game().pregame_selection()==target,
                "Native editor froze before source cursor/navigation reached its destination");
            const auto state=session.game().save_state(),apu=session.audio().save_state();
            const auto waiting=session.advance(time+1'000'000'000,chord);
            require(!waiting.video_phases && !waiting.logic_ticks && !waiting.audio_blocks
                && state==session.game().save_state() && apu==session.audio().save_state(),"Editor chord ticked the frozen cartridge/SPC owner");
            if(target==3)session.finish_hud_customization();else session.finish_controller_remap();
            source.input.reset();time+=1'000'000'001;session.advance(time,chord);
            require(session.game().pregame_selection()==target,"Returning from native editor restored the old source row");
            source.input.reset();session.advance(++time,0);compare();
            for(unsigned raster=1;raster<=6;++raster)step(0,0);
        }
        // Moving AWAY must act on the new ordinary source row, not reopen the
        // old editor. Use the controller's supported volume/stereo neighbours.
        if(opens && nav==0) {
            const auto away=target==8?input::up:input::down;
            if(target==3)select(8); // controller -> volume, avoiding disabled desktop controls
            const auto leaving=step(input::ButtonMask(away|input::a),input::ButtonMask(away|input::a));
            require(!leaving.requested_controller_remap && !leaving.requested_hud_customization,
                "Leaving a native editor row opened the old editor");
        }
        ++fixtures;
    }
    std::cout<<"  Native editor chords: "<<fixtures<<" setup/runtime fixtures, destination/Back/Start/Up priority and exact VM/SPC/PCM navigation/freeze/return checked\n";
}
void runtime_editor_resume_parity(const assets::RomImage& rom,const assets::SymbolMap& symbols,bool hud_editor) {
    for(unsigned partial=0;partial<3;++partial) {
        std::vector<std::int16_t> pcm;
        GameSession session(rom,symbols,[&](auto samples){pcm.insert(pcm.end(),samples.begin(),samples.end());},"LEVEL1_1");
        SourceOracle source(rom,symbols,"LEVEL1_1");std::int64_t time{};session.advance(time,0);
        const auto compare=[&] {
            require(session.game().save_state()==source.game.save_state(),"Runtime editor changed independent cartridge state");
            require(session.audio().save_state()==source.spc.save_state() && pcm==source.pcm,"Runtime editor lost partial SPC/PCM phase");
        };
        for(unsigned raster=1;raster<=partial;++raster) {time=timestamp(raster);session.advance(time,0);source.raster();compare();}
        require(session.toggle_runtime_options() && source.game.toggle_runtime_options(),"Stage fixture could not open runtime options");
        source.input.reset();session.advance(++time,0);compare();
        const auto menu_step=[&](input::ButtonMask held,bool host_action=false) {
            time+=50'000'000;source.input.sample(host_action?0:held);
            const auto result=session.advance(time,held);
            require(result.video_phases>0 && result.video_phases<=3 && !result.audio_blocks,"Runtime editor menu clock resumed audio prematurely");
            // Menu input still uses source raster/logic timing, but its paused
            // cartridge and SPC have no clock or handshake service.
            for(unsigned raster=0;raster<result.video_phases;++raster) {
                source.game.present_frame();
                if(source.game.logic_tick_ready()) static_cast<void>(source.game.tick(source.input.consume()));
            }
            compare();return result;
        };
        const auto tap=[&](input::ButtonMask button) {menu_step(button);menu_step(0);};
        const auto select=[&](unsigned id) {
            const auto order=simulation::pregame_menu_order(session.game().pregame_page());
            for(std::size_t i=0;session.game().pregame_selection()!=id && i<order.size();++i) tap(input::down);
            require(session.game().pregame_selection()==id,"Runtime editor fixture could not select source option");
        };
        select(14);tap(input::a);select(hud_editor?3:8);
        const auto opening=menu_step(input::a,true);
        require(hud_editor?opening.requested_hud_customization:opening.requested_controller_remap,
            "Runtime Options did not open the selected native editor");
        source.input.reset();const auto map=session.game().map().save_state(),spc=session.audio().save_state();
        const auto wait=session.advance(time+60'000'000'000LL,input::start);
        require(!wait.video_phases && !wait.audio_blocks && map==session.game().map().save_state() && spc==session.audio().save_state(),
            "Runtime editor advanced paused cartridge/audio");compare();
        if(hud_editor) {
            auto layout=session.preferences().hud_layout;layout.widgets[unsigned(HudWidget::shield)].x=20;
            session.finish_hud_customization(layout);
        } else session.finish_controller_remap();
        compare();
        require(session.game().runtime_options_open(),"Editor return unexpectedly closed source runtime menu");
        // The player must retain NDSP pause on this return: spend additional
        // ordinary menu rasters here without any SPC service or new PCM. This
        // verifies source cadence, not the physical DSP's paused DMA status.
        source.input.reset();time+=60'000'000'001LL;
        require(!session.advance(time,0).video_phases,"Runtime editor return caught up paused wall time");compare();
        for(unsigned menu_frame=0;menu_frame<6;++menu_frame)menu_step(0);
        require(session.game().runtime_options_open() && session.audio().save_state()==spc,
            "Returning to runtime Options prematurely resumed source audio");
        require(session.toggle_runtime_options() && source.game.toggle_runtime_options(),"Runtime editor options could not close");
        source.input.reset();time+=60'000'000'001LL;session.advance(time,0);compare();
        for(unsigned raster=1;raster<=24;++raster) {
            const auto resumed=session.advance(time+timestamp(raster),0);
            require(resumed.video_phases==1,"Runtime editor resume duplicated/dropped source raster");source.raster();compare();
        }
    }
    std::cout<<(hud_editor?"  Runtime HUD":"  Runtime controller")
        <<" resume: paused Options return and independent VM/SPC/PCM continuation for all three partial audio phases checked\n";
}
void actual_hud_customization(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    GameSession session(rom,symbols,[](auto){});MenuDriver controls(session);
    rejects([&]{session.finish_hud_customization();},"Unrequested HUD editor acknowledged");
    controls.select(14);controls.tap(input::a);controls.select(3);
    const auto menu=GameMenu::capture(session.game());
    const auto row=std::find_if(menu.rows.begin(),menu.rows.end(),[](const auto& item){return item.id==3;});
    require(row!=menu.rows.end() && row->enabled && row->value=="A  OPEN","Actual Customize Screen option remains unavailable");
    const auto saved_before=session.save_state();const auto preferences=session.preferences();
    controls.tap(input::a);require(controls.last.requested_hud_customization && session.hud_customization_pending(),"Real Customize Screen action did not freeze native owner");
    const auto vm=session.game().save_state(),spc=session.audio().save_state();
    for(auto held:std::array<input::ButtonMask,3>{input::a,input::start,input::ButtonMask(input::left_shoulder|input::right_shoulder)}) {
        const auto wait=session.advance(controls.time+10'000'000'000LL,held);
        require(wait.requested_hud_customization && !wait.video_phases && !wait.logic_ticks && !wait.audio_blocks
            && vm==session.game().save_state() && spc==session.audio().save_state(),"HUD editor advanced cartridge/SPC or reset settings");
    }
    require(!session.state_available(),"Transient HUD screen exposed save-state action");
    rejects([&]{static_cast<void>(session.save_state());},"Transient HUD state saved");
    rejects([&]{static_cast<void>(session.restored_state(saved_before));},"Transient HUD state replaced owner");
    rejects([&]{session.toggle_runtime_options();},"Transient HUD editor admitted another menu");
    GameHudEditor editor;editor.open(preferences.hud_layout);editor.update({});
    for(unsigned id=0;id<hud_widget_count;++id) {
        if(id) {editor.update({input::right_shoulder});editor.update({});}
        editor.update({input::left});editor.update({});
    }
    const auto layout=editor.layout();require(layout!=preferences.hud_layout && layout.valid(),"Actual editor did not prepare independent profile");
    require(vm==session.game().save_state() && spc==session.audio().save_state(),"Moving native HUD mutated cartridge/audio");
    auto malformed=layout;malformed.widgets[0].quarters=9;
    rejects([&]{session.finish_hud_customization(malformed);},"Malformed applied HUD accepted");
    require(session.hud_customization_pending() && session.preferences()==preferences,"Invalid HUD edit partly changed profile or acknowledged handoff");
    editor.update({input::start});require(editor.applied(),"Actual HUD editor did not Apply");
    session.finish_hud_customization(editor.layout());
    auto edited=preferences;edited.hud_layout=layout;
    require(!session.hud_customization_pending() && session.preferences()==edited && session.state_available(),"HUD profile did not commit/return to source Options");
    require(vm==session.game().save_state() && spc==session.audio().save_state(),"Applying native layout changed source VM/SPC");
    // Layout is a current native control profile, not rewound by the source
    // GAME archive. Save/load still preserves the complete source/audio packet.
    const auto restored=session.restored_state(saved_before);
    require(restored->preferences().hud_layout==layout && restored->save_state()==saved_before,"State restore rewound HUD profile or changed source archive");
    const auto resumed=controls.time+20'000'000'000LL;
    require(!session.advance(resumed,input::start).video_phases,"HUD close caught up long editor time");
    require(!session.advance(resumed+1,0).requested_hud_customization,"Held editor Start reopened editor");
    for(unsigned raster=1;raster<=6;++raster) session.advance(resumed+1+timestamp(raster),0);
    const auto no_hud=session.presentation(0,false).dashboard;
    GameSessionOptions defaults;defaults.preferences=preferences;
    GameSession pristine(rom,symbols,[](auto){},"BOOT",{},defaults);
    require(std::ranges::equal(no_hud.pixels,pristine.presentation(0,false).dashboard.pixels),"Native layout changed setup-only lower-screen status");
    controls.time=resumed+1+timestamp(6);controls.tap(input::a);
    require(session.hud_customization_pending(),"Customize Screen could not reopen after Apply/release");
    session.finish_hud_customization();require(session.preferences()==edited,"Cancel did not preserve applied native layout");
    GameSessionOptions options;options.preferences=edited;
    GameSession stage(rom,symbols,[](auto){},"LEVEL1_1",{},options);
    require(stage.preferences()==edited && !stage.hud_customization_pending(),"Native profile lost at actual stage-owner handoff");
    GameHud expected(rom,symbols);expected.set_layout(layout);expected.update(expected.capture(stage.game()));
    require(std::ranges::equal(stage.presentation(1,true).dashboard.pixels,expected.view().pixels),"Native stage did not apply customized HUD to split-screen route");
    options.preferences->hud_layout=malformed;
    rejects([&]{GameSession bad(rom,symbols,[](auto){},"BOOT",{},options);},"Malformed persisted HUD accepted by game owner");
    std::cout<<"  HUD customization: actual source option, paused editor, Apply/Cancel, source-state purity, persistent native layout and stage routing checked\n";
}
}
int main(int argc,char** argv) {
    try {
        const bool states_only=argc==4 && std::string_view(argv[3])=="--states-only";
        const bool capabilities_only=argc==4 && std::string_view(argv[3])=="--capabilities-only";
        const bool sweep=argc==4 && std::string_view(argv[3])=="--stage-sweep";
        const bool fortuna_route=argc==4 && std::string_view(argv[3])=="--fortuna-source-route";
        const bool corneria_route=argc==4 && std::string_view(argv[3])=="--corneria-source-route";
        const bool carrier_route=argc==4 && std::string_view(argv[3])=="--attack-carrier-source-route";
        const bool runtime_editors=argc==4 && std::string_view(argv[3])=="--runtime-editor-parity";
        const bool editor_chords=argc==4 && std::string_view(argv[3])=="--editor-chords";
        const bool fps_chords=argc==4 && std::string_view(argv[3])=="--fps-chords";
        const bool death_route=argc==4 && std::string_view(argv[3])=="--death-source-route";
        const bool continue_yes=argc==4 && std::string_view(argv[3])=="--continue-yes-source-route";
        const bool continue_no=argc==4 && std::string_view(argv[3])=="--continue-no-source-route";
        if(argc!=3 && !states_only && !capabilities_only && !sweep && !fortuna_route && !corneria_route && !carrier_route && !runtime_editors && !editor_chords && !fps_chords && !death_route && !continue_yes && !continue_no && !(argc==5 && std::string_view(argv[3])=="--capture"))
            throw std::invalid_argument("Usage: game_session_check ROM SYMBOLS [--capture DIRECTORY | --states-only | --capabilities-only | --stage-sweep | --fortuna-source-route | --corneria-source-route | --attack-carrier-source-route | --runtime-editor-parity | --editor-chords | --fps-chords | --death-source-route | --continue-yes-source-route | --continue-no-source-route]");
        const auto rom=assets::RomImage::load(argv[1]);const auto symbols=assets::SymbolMap::load(argv[2]);
        if(continue_yes || continue_no) {
            natural_death_restart(rom,symbols,continue_yes);
            std::cout<<"3DS natural game-over/Continue source/composition parity: "<<checks<<" checks passed\n";return 0;
        }
        if(death_route) {
            natural_death_restart(rom,symbols);
            std::cout<<"3DS death/restart source/composition parity: "<<checks<<" checks passed\n";return 0;
        }
        if(fortuna_route || corneria_route || carrier_route) {
            if(fortuna_route) natural_source_route(rom,symbols,"LEVEL3_3","Fortuna",fortuna_route_diagnostic::Inputs(symbols));
            else if(carrier_route) natural_source_route(rom,symbols,"LEVEL1_1","Corneria 1",attack_carrier_route_diagnostic::Inputs(symbols));
            else natural_source_route(rom,symbols,"LEVEL3_1","Corneria 3",corneria_route_diagnostic::Inputs(symbols));
            std::cout<<"3DS full source route parity: "<<checks<<" checks passed\n";return 0;
        }
        if(runtime_editors) {
            runtime_editor_resume_parity(rom,symbols,false);runtime_editor_resume_parity(rom,symbols,true);
            std::cout<<"3DS runtime editors: "<<checks<<" checks passed; source parity, not physical NDSP pause acceptance\n";return 0;
        }
        if(editor_chords) {
            editor_navigation_parity(rom,symbols);
            std::cout<<"3DS native editor chords: "<<checks<<" checks passed; source parity, not physical device acceptance\n";return 0;
        }
        if(fps_chords) {
            fps_navigation_parity(rom,symbols);
            std::cout<<"3DS native FPS chords: "<<checks<<" checks passed; source parity, not physical device acceptance\n";return 0;
        }
        if(sweep) {stage_sweep(rom,symbols);std::cout<<"3DS stage/flow parity: "<<checks<<" checks passed\n";return 0;}
        if(capabilities_only) {
            native_capability_restore(rom,symbols);
            std::cout<<"3DS native capabilities: "<<checks<<" checks passed; host state imports, not console acceptance\n";return 0;
        }
        if(states_only) {
            full_state_parity(rom,symbols);
            std::cout<<"3DS actual full states: "<<checks<<" checks passed; host source parity, not console acceptance\n";return 0;
        }
        parity(rom,symbols,"BOOT");parity(rom,symbols,"LEVEL1_1");handoff(rom,symbols);
        presentation_cadence(rom,symbols);
        native_capability_restore(rom,symbols);
        fps_navigation_parity(rom,symbols);
        actual_menu(rom,symbols,argc==5?std::filesystem::path(argv[4]):std::filesystem::path{});
        actual_disk_handoff(rom,symbols);
        actual_settings_reset(rom,symbols);
        actual_controller_remap(rom,symbols);
        editor_navigation_parity(rom,symbols);
        editor_resume_parity(rom,symbols);
        actual_hud_customization(rom,symbols);
        editor_resume_parity(rom,symbols,true);
        runtime_editor_resume_parity(rom,symbols,false);
        runtime_editor_resume_parity(rom,symbols,true);
        full_state_parity(rom,symbols);
        GameSession failed(rom,symbols,[](auto){throw std::runtime_error("PCM device failed");});
        failed.advance(0,0);rejects([&]{failed.advance(50'000'000,0);},"PCM failure ignored");
        rejects([&]{failed.advance(100'000'000,0);},"Failed source tick retried against partly advanced state");
        rejects([&]{static_cast<void>(failed.presentation(0,true));},"Failed audio/source session presented stale geometry");
        std::cout<<"3DS actual game session: "<<checks<<" checks passed; host source parity, not PICA/NDSP or hardware acceptance\n";
    } catch(const std::exception& error) {std::cerr<<"3DS actual game session: "<<error.what()<<'\n';return 1;}
}
