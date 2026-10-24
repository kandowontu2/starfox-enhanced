#include "starfox/assets/rom.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/audio/parallel_stem_executor.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/state/container.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric>
#include <span>
#include <string_view>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::uint64_t pcm_difference(
    std::span<const std::int16_t> left,
    std::span<const std::int16_t> right) {
    return std::inner_product(left.begin(), left.end(), right.begin(),
        std::uint64_t{}, std::plus<>{},
        [](std::int16_t a, std::int16_t b) {
            return static_cast<std::uint64_t>(
                std::abs(static_cast<int>(a) - static_cast<int>(b)));
        });
}

void check_good_luck_tail(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    auto game = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "PLANETSELECT", std::span<const std::uint8_t>{}, true);
    starfox::audio::Spc700Audio live, reference;
    const auto boot = game->map().take_apu_port_writes();
    (void)live.prime_upload_sequence(boot);
    (void)reference.prime_upload_sequence(boot);
    game->synchronize_apu_output_ports(live.output_ports());
    int cue = -1;
    bool heard_tail = false, finished = false;
    for (int tick = 0; tick < 2000; ++tick) {
        const auto result = game->tick({0, starfox::input::start, 0});
        if (cue < 0 && std::find(result.sound_effect_commands.begin(),
                result.sound_effect_commands.end(), 0x13U) != result.sound_effect_commands.end()) cue = tick;
        (void)live.render_logic_tick(result.audio_port_writes);
        // Leave the reference SPC untouched after dispatch, allowing the
        // complete recorded voice to play without the next stage's bank upload.
        (void)reference.render_logic_tick(cue < 0 || tick == cue
            ? std::span<const starfox::simulation::ApuPortWrite>{result.audio_port_writes}
            : std::span<const starfox::simulation::ApuPortWrite>{});
        game->synchronize_apu_output_ports(live.output_ports());
        if (cue < 0) continue;
        const auto expected = reference.last_effect_samples();
        const bool audible = std::any_of(expected.begin(), expected.end(), [](auto v) { return v != 0; });
        if (audible) {
            require(pcm_difference(live.last_effect_samples(), expected) == 0,
                "stage transition cut off Good luck voice");
            if (tick - cue >= 12) heard_tail = true;
        } else if (heard_tail) finished = true;
        if (tick - cue >= 30) {
            require(game->flow_state() == starfox::simulation::GameFlowState::gameplay,
                "Good luck voice guard blocked stage entry");
            break;
        }
    }
    require(cue >= 0 && heard_tail && finished, "Good luck tail regression did not exercise complete cue");
}

void check_audio_restore(starfox::audio::Spc700Audio& live) {
    const auto saved = live.save_state();
    starfox::audio::Spc700Audio restored;
    restored.load_state(saved);
    require(restored.save_state() == saved, "SPC fresh restore changed state");
    for (int tick = 0; tick < 24; ++tick) {
        const std::array writes{starfox::simulation::ApuPortWrite{3, 0x0d, 512}};
        const auto commands = tick == 4
            ? std::span<const starfox::simulation::ApuPortWrite>{writes}
            : std::span<const starfox::simulation::ApuPortWrite>{};
        const auto expected = live.render_logic_tick(commands);
        const auto actual = restored.render_logic_tick(commands);
        require(expected == actual, "SPC restored PCM differs");
        require(live.save_state() == restored.save_state(), "SPC continuation state differs");
        require(live.output_ports() == restored.output_ports(), "SPC restored ports differ");
    }
    live.load_state(saved);
    require(live.save_state() == saved, "SPC same-device restore differs");
    auto invalid = saved;
    invalid.back() ^= 1U;
    bool rejected = false;
    try { live.load_state(invalid); } catch (const std::exception&) { rejected = true; }
    require(rejected && live.save_state() == saved, "Corrupt SPC state changed audio");
    const auto payload = starfox::state::unpack(saved, 0x53504301U, 0U);
    const auto short_state = starfox::state::pack(0x53504301U, 0U,
        payload.first(payload.size() / 2));
    rejected = false;
    try { live.load_state(short_state); } catch (const std::exception&) { rejected = true; }
    require(rejected && live.save_state() == saved, "Truncated SPC state changed audio");
}

void check_original_selected_level_bank(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    // Stage 3-5's bank is an overlay. Normal progression has already
    // installed route 3's common first-stage effects before reaching it.
    auto selected = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "BOOT");
    selected->set_selected_level(35U);
    starfox::audio::Spc700Audio audio;
    (void)audio.prime_upload_sequence(selected->map().take_apu_port_writes());
    for (int tick = 0; tick < 30; ++tick) (void)audio.render_logic_tick({});
    selected->synchronize_apu_output_ports(audio.output_ports());
    for (int tick = 0; tick < 120
            && selected->flow_state()
                != starfox::simulation::GameFlowState::gameplay; ++tick) {
        for (int phase = 0; phase < 3; ++phase) selected->present_frame();
        const auto result = selected->tick(tick == 0
            ? starfox::input::TickInput{0, starfox::input::start, 0}
            : starfox::input::TickInput{});
        (void)audio.render_logic_tick(result.audio_port_writes);
        selected->synchronize_apu_output_ports(audio.output_ports());
    }
    require(selected->flow_state() == starfox::simulation::GameFlowState::gameplay
            && selected->map().apu_upload_generation() >= 6U
            && audio.driver_loaded(),
        "level 3-5 shortcut skipped one or more preceding sound banks");
    for (int tick = 0; tick < 40; ++tick) (void)audio.render_logic_tick({});
    constexpr std::array laser{starfox::simulation::ApuPortWrite{3U, 0x35U, 0U}};
    bool heard_laser{};
    for (int tick = 0; tick < 24; ++tick) {
        (void)audio.render_logic_tick(tick == 0
            ? std::span<const starfox::simulation::ApuPortWrite>{laser}
            : std::span<const starfox::simulation::ApuPortWrite>{});
        heard_laser = heard_laser || std::any_of(
            audio.last_effect_samples().begin(),
            audio.last_effect_samples().end(),
            [](std::int16_t sample) { return sample != 0; });
    }
    require(heard_laser, "level 3-5 shortcut silenced the laser effect");
    // Enemy e-lasers use a different effect family ($44-$48). The stage 3-5
    // bank alone does not include the sound1 samples from earlier stages.
    constexpr std::array e_laser{starfox::simulation::ApuPortWrite{3U, 0x44U, 0U}};
    bool heard_e_laser{};
    for (int tick = 0; tick < 24; ++tick) {
        (void)audio.render_logic_tick(tick == 0
            ? std::span<const starfox::simulation::ApuPortWrite>{e_laser}
            : std::span<const starfox::simulation::ApuPortWrite>{});
        heard_e_laser = heard_e_laser || std::any_of(
            audio.last_effect_samples().begin(),
            audio.last_effect_samples().end(),
            [](std::int16_t sample) { return sample != 0; });
    }
    require(heard_e_laser, "level 3-5 shortcut silenced the e-laser effect");
}

void check_original_batched_music_banks(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    auto game = std::make_unique<starfox::simulation::GameSimulation>(rom, symbols, "BOOT");
    starfox::audio::Spc700Audio batched, separate, settled;
    const auto boot = game->map().take_apu_port_writes();
    (void)batched.prime_upload_sequence(boot);
    (void)separate.prime_upload_sequence(boot);
    (void)settled.prime_upload_sequence(boot);
    std::vector<starfox::simulation::ApuPortWrite> combined;
    for (const auto* name : {"DO_BGM_30", "DO_BGM_31", "DO_BGM_32", "DO_BGM_33", "DO_BGM_34", "DO_BGM_35"}) {
        starfox::simulation::Wdc65816Registers registers;
        registers.status = 0x24U;
        game->map().call_native_routine(symbols.find(name).front(), registers, 50'000'000U, true);
        const auto writes = game->map().take_apu_port_writes();
        combined.insert(combined.end(), writes.begin(), writes.end());
        (void)separate.render_logic_tick(writes);
        (void)settled.render_logic_tick(writes);
        if (std::string_view{name} != "DO_BGM_35")
            for (unsigned tick = 0; tick < 30; ++tick) (void)settled.render_logic_tick({});
    }
    (void)batched.render_logic_tick(combined);
    require(batched.save_state() == separate.save_state(),
        "Real level-select SPC banks differ from separately initialized route banks");
    const std::array start_music{starfox::simulation::ApuPortWrite{0, 3, 0}};
    for (unsigned tick = 0; tick < 60; ++tick) {
        const auto commands = tick == 0 ? std::span<const starfox::simulation::ApuPortWrite>{start_music}
            : std::span<const starfox::simulation::ApuPortWrite>{};
        (void)batched.render_logic_tick(commands);
        (void)settled.render_logic_tick(commands);
        require(std::equal(batched.last_music_samples().begin(), batched.last_music_samples().end(),
            settled.last_music_samples().begin()), "Level-select music differs from settled route progression");
    }
}

void check_selected_orchestra_bank(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols, std::uint8_t level) {
    using Write = starfox::simulation::ApuPortWrite;
    // Native PLANETSEQ installs orchestra instruments before each mission.
    // The asteroid bank supplies a score, not those instruments. Compare the
    // actual shortcut PCM against that independent source-bank prerequisite.
    auto bank_source = std::make_unique<starfox::simulation::GameSimulation>(rom, symbols, "BOOT");
    (void)bank_source->map().take_apu_port_writes();
    starfox::simulation::Wdc65816Registers registers;
    registers.status = 0x24U;
    bank_source->map().call_native_routine(symbols.find("DO_BGM_MAP").front(), registers, 50'000'000U, true);
    const auto orchestra = bank_source->map().take_apu_port_writes();
    auto game = std::make_unique<starfox::simulation::GameSimulation>(rom, symbols, "BOOT");
    game->set_selected_level(level);
    starfox::audio::Spc700Audio actual, expected;
    const auto boot = game->map().take_apu_port_writes();
    (void)actual.prime_upload_sequence(boot);
    (void)expected.prime_upload_sequence(boot);
    bool inserted = false, heard_music = false;
    for (unsigned tick = 0; tick < 150; ++tick) {
        for (unsigned phase = 0; phase < 3; ++phase) game->present_frame();
        const auto result = game->tick(tick == 0
            ? starfox::input::TickInput{0, starfox::input::start, 0} : starfox::input::TickInput{});
        auto reference_writes = result.audio_port_writes;
        const auto restart = std::find_if(reference_writes.rbegin(), reference_writes.rend(),
            [](const Write& write) { return write.port == 0 && write.value == 0xff; });
        if (!inserted && restart != reference_writes.rend()) {
            reference_writes.insert(restart.base() - 1, orchestra.begin(), orchestra.end());
            inserted = true;
        }
        (void)actual.render_logic_tick(result.audio_port_writes);
        (void)expected.render_logic_tick(reference_writes);
        game->synchronize_apu_output_ports(actual.output_ports());
        if (!inserted) continue;
        const auto music = actual.last_music_samples();
        heard_music |= std::any_of(music.begin(), music.end(), [](auto sample) { return sample != 0; });
        require(std::equal(music.begin(), music.end(), expected.last_music_samples().begin()),
            "Level-select score uses the wrong orchestra instruments/pitch");
    }
    require(inserted && heard_music, "Orchestra shortcut fixture never exercised stage music");
}

void check_parallel_spc(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    starfox::audio::ParallelStemExecutor executor(true);
    require(executor.parallel_available(), "Real-driver audio test did not start the worker");
    for(const auto* level:{"LEVEL1_1","LEVEL3_5"}) {
        auto game=std::make_unique<starfox::simulation::GameSimulation>(
            rom,symbols,level,std::span<const std::uint8_t>{},true);
        game->set_god_mode(true);
        starfox::audio::Spc700Audio serial,parallel;
        const auto boot=game->map().take_apu_port_writes();
        (void)serial.prime_upload_sequence(boot);
        (void)parallel.prime_upload_sequence(boot);
        game->synchronize_apu_output_ports(serial.output_ports());
        bool audible{};
        for(unsigned tick=0;tick<240;++tick) {
            const auto result=game->tick({});
            serial.render_stems_logic_tick(result.audio_port_writes);
            parallel.render_stems_logic_tick(result.audio_port_writes,&executor);
            require(std::equal(serial.last_music_samples().begin(),serial.last_music_samples().end(),
                parallel.last_music_samples().begin(),parallel.last_music_samples().end()),
                "Threaded real SPC music differs from serial");
            require(std::equal(serial.last_effect_samples().begin(),serial.last_effect_samples().end(),
                parallel.last_effect_samples().begin(),parallel.last_effect_samples().end()),
                "Threaded real SPC effects differ from serial");
            require(serial.output_ports()==parallel.output_ports(),"Threaded real SPC handshake changed");
            audible|=std::any_of(serial.last_music_samples().begin(),serial.last_music_samples().end(),
                [](auto sample){return sample!=0;});
            if(tick%24==0) require(serial.save_state()==parallel.save_state(),"Threaded real SPC machine state changed");
            if(tick==120) {
                starfox::audio::Spc700Audio restored;
                restored.load_state(parallel.save_state());
                parallel=std::move(restored);
            }
            game->synchronize_apu_output_ports(serial.output_ports());
        }
        require(audible,"Real-driver parallel audio fixture remained silent");
    }
}

void check_selected_level_map_samples(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    // Every stage bank leaves instrument samples to the planet map's bank
    // (orchestra's tail and sound6, ARAM $dac0-$dd0d). LEVEL SELECT skips the
    // map, so it must rebuild that state: compare with a start from the map.
    const auto music_at_gameplay = [](starfox::simulation::GameSimulation& game,
            bool press_every_tick) {
        starfox::audio::Spc700Audio audio;
        (void)audio.prime_upload_sequence(game.map().take_apu_port_writes());
        for (int tick = 0; tick < 30; ++tick) (void)audio.render_logic_tick({});
        game.synchronize_apu_output_ports(audio.output_ports());
        for (int tick = 0; tick < 2000
                && game.flow_state() != starfox::simulation::GameFlowState::gameplay; ++tick) {
            for (int phase = 0; phase < 3; ++phase) game.present_frame();
            const auto result = game.tick(press_every_tick || tick == 0
                ? starfox::input::TickInput{0, starfox::input::start, 0}
                : starfox::input::TickInput{});
            (void)audio.render_logic_tick(result.audio_port_writes);
            game.synchronize_apu_output_ports(audio.output_ports());
        }
        require(game.flow_state() == starfox::simulation::GameFlowState::gameplay,
            "map-sample fixture did not reach gameplay");
        for (int tick = 0; tick < 20; ++tick) {
            const auto result = game.tick({});
            (void)audio.render_logic_tick(result.audio_port_writes);
            game.synchronize_apu_output_ports(audio.output_ports());
        }
        return audio.music_ram();
    };
    auto normal = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "PLANETSELECT", std::span<const std::uint8_t>{}, true);
    const auto expected = music_at_gameplay(*normal, true);
    auto selected = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "BOOT");
    selected->set_selected_level(11U);
    const auto actual = music_at_gameplay(*selected, false);
    require(expected.size() == 0x10000U && actual.size() == 0x10000U,
        "music ARAM unavailable");
    require(std::equal(expected.begin() + 0xdac0, expected.begin() + 0xdd0e,
                actual.begin() + 0xdac0),
        "LEVEL SELECT left the planet map's instrument samples out of ARAM");
    // Later stages follow a return to the map, whose orchestra instruments
    // (directory $3c60-$3c9f, samples $b360-$dcef) replace stage 1's band.
    // Asteroid (1-2) uploads neither region: it must match the map screen.
    auto map_screen = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "PLANETSELECT", std::span<const std::uint8_t>{}, true);
    std::vector<std::uint8_t> map_ram;
    {
        starfox::audio::Spc700Audio audio;
        (void)audio.prime_upload_sequence(map_screen->map().take_apu_port_writes());
        map_screen->synchronize_apu_output_ports(audio.output_ports());
        for (int tick = 0; tick < 120; ++tick) {
            for (int phase = 0; phase < 3; ++phase) map_screen->present_frame();
            const auto result = map_screen->tick({});
            (void)audio.render_logic_tick(result.audio_port_writes);
            map_screen->synchronize_apu_output_ports(audio.output_ports());
        }
        map_ram = audio.music_ram();
    }
    auto second = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "BOOT");
    second->set_selected_level(12U);
    const auto stage_two = music_at_gameplay(*second, false);
    require(std::equal(map_ram.begin() + 0x3c60, map_ram.begin() + 0x3ca0, stage_two.begin() + 0x3c60)
            && std::equal(map_ram.begin() + 0xb360, map_ram.begin() + 0xdcf0, stage_two.begin() + 0xb360),
        "LEVEL SELECT 1-2 kept stage 1's band instead of the map's orchestra");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: starfox_direct_audio_tests ROM SYMBOLS\n";
        return 2;
    }
    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    check_parallel_spc(rom,symbols);
    {
        starfox::audio::Spc700Audio fresh;
        check_audio_restore(fresh);
    }
    check_good_luck_tail(rom, symbols);
    if (symbols.find("BGMPLAYLIST").empty()) {
        check_original_batched_music_banks(rom, symbols);
        for (const auto level : {12U, 13U, 22U, 24U, 32U, 34U, 35U, 36U, 37U})
            check_selected_orchestra_bank(rom, symbols, static_cast<std::uint8_t>(level));
        check_original_selected_level_bank(rom, symbols);
        check_selected_level_map_samples(rom, symbols);
    }
    auto effect = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "LEVEL1_1", std::span<const std::uint8_t>{}, true);
    auto control = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "LEVEL1_1", std::span<const std::uint8_t>{}, true);
    starfox::audio::Spc700Audio effect_audio;
    starfox::audio::Spc700Audio control_audio;

    // The desktop runtime services the cartridge's boot bank before its
    // first direct-level tick. Keep the two IPL transfers in separate SPC
    // frames: the stage bank is an overlay on sound0's initialized workspace.
    const auto effect_boot_writes = effect->map().take_apu_port_writes();
    const auto control_boot_writes = control->map().take_apu_port_writes();
    require(!effect_boot_writes.empty() && !control_boot_writes.empty(),
        "direct-level startup omitted the base SPC bank");
    {
        starfox::audio::Spc700Audio partial, resumed;
        const std::span boot{effect_boot_writes};
        const auto split = boot.size() / 2;
        (void)partial.render_logic_tick(boot.first(split));
        require(!partial.driver_loaded(), "Partial upload fixture already finished");
        resumed.load_state(partial.save_state());
        const auto expected = partial.render_logic_tick(boot.subspan(split));
        const auto actual = resumed.render_logic_tick(boot.subspan(split));
        require(partial.driver_loaded() && resumed.driver_loaded(), "Restored upload failed");
        require(expected == actual && partial.save_state() == resumed.save_state(),
            "Restored partial SPC upload diverged");
    }
    const auto effect_upload_frames =
        effect_audio.prime_upload_sequence(effect_boot_writes);
    const auto control_upload_frames =
        control_audio.prime_upload_sequence(control_boot_writes);
    effect->synchronize_apu_output_ports(effect_audio.output_ports());
    control->synchronize_apu_output_ports(control_audio.output_ports());
    require(effect_audio.driver_loaded() && control_audio.driver_loaded(),
        "base SPC bank did not initialize before direct-level entry");
    check_audio_restore(effect_audio);
    // Constructor startup contains the boot sound0 bank. The first source
    // update requests the stage overlay after the desktop has synchronized
    // the live SPC ports, so this must be one completed frame here—not zero,
    // and not a combined base+stage transfer.
    require(effect_upload_frames == 1U && control_upload_frames == 1U,
        "direct-level startup did not isolate the base SPC bank");
    require(effect->map().apu_upload_generation() == 1U
                && control->map().apu_upload_generation() == 1U,
        "stage SPC bank was uploaded before the base driver could initialize");

    // The packaged CLI runtime performs its ordinary 1.5-second black preroll
    // after construction. Unlike a title/menu launch, a direct map otherwise
    // has no time for sound0 to finish initializing the ARAM workspace that
    // the stage bank overlays.
    constexpr std::size_t direct_entry_settle_ticks = 30U;
    for (std::size_t tick = 0U; tick < direct_entry_settle_ticks; ++tick) {
        static_cast<void>(effect_audio.render_logic_tick({}));
        static_cast<void>(control_audio.render_logic_tick({}));
    }
    effect->synchronize_apu_output_ports(effect_audio.output_ports());
    control->synchronize_apu_output_ports(control_audio.output_ports());

    const auto enqueue_laser_effect = [&]() {
        const auto sound_read = symbols.find("SDGPT3").front();
        const auto sound_write = symbols.find("SDSPT3").front();
        effect->map().write_native_byte(sound_read,
            effect->map().read_native_byte(sound_write));
        effect->map().write_native_byte(
            symbols.find("SDPCK3").front(), 0U);
        effect->map().write_native_byte(0x002143U, 0U);
        for (const auto* optional_flag : {
                 "NOSFX", "BGMSFX", "NOSETPORT3"}) {
            const auto addresses = symbols.find(optional_flag);
            if (!addresses.empty()) {
                effect->map().write_native_byte(addresses.front(), 0U);
            }
        }
        starfox::simulation::Wdc65816Registers registers;
        registers.a = 0x35U;
        registers.status = 0x24U;
        effect->map().call_native_routine(
            symbols.find("SETPORT3_L").front(), registers, 5'000'000, true);
    };

    auto saw_stage_upload = false;
    auto early_effect_queued = false;
    auto early_effect_command = false;
    auto early_effect_ack = false;
    auto early_effect_audible = false;
    auto early_music_isolated = true;
    for (std::size_t tick = 0; tick < 360U; ++tick) {
        const auto effect_tick = effect->tick({});
        const auto control_tick = control->tick({});
        saw_stage_upload = saw_stage_upload
            || effect->map().apu_upload_generation() >= 2U;
        static_cast<void>(effect_audio.render_logic_tick(
            effect_tick.audio_port_writes));
        static_cast<void>(control_audio.render_logic_tick(
            control_tick.audio_port_writes));
        effect->synchronize_apu_output_ports(effect_audio.output_ports());
        control->synchronize_apu_output_ports(control_audio.output_ports());
        if (early_effect_queued) {
            early_effect_command = early_effect_command || std::find(
                effect_tick.sound_effect_commands.begin(),
                effect_tick.sound_effect_commands.end(), 0x35U)
                    != effect_tick.sound_effect_commands.end();
            early_effect_ack = early_effect_ack
                || effect_audio.output_ports()[3] == 0x35U;
            early_effect_audible = early_effect_audible || pcm_difference(
                effect_audio.last_effect_samples(),
                control_audio.last_effect_samples()) != 0U;
            early_music_isolated = early_music_isolated && std::equal(
                effect_audio.last_music_samples().begin(),
                effect_audio.last_music_samples().end(),
                control_audio.last_music_samples().begin(),
                control_audio.last_music_samples().end());
        } else if (effect->map().apu_upload_generation() >= 2U
                   && effect_audio.driver_loaded()) {
            // Exercise port 3 immediately after the first stage bank replaces
            // sound0. This is the precise startup window that the packaged CLI
            // path used to leave half-initialized.
            enqueue_laser_effect();
            early_effect_queued = true;
        }
    }
    require(effect_audio.driver_loaded() && control_audio.driver_loaded(),
        "direct-level startup did not leave a running SPC driver");
    require(effect->map().apu_upload_generation() >= 2U
                && control->map().apu_upload_generation() >= 2U,
        "direct-level startup did not replace the base SPC bank");
    require(saw_stage_upload,
        "direct-level startup never submitted its stage SPC overlay");
    require(early_effect_queued && early_effect_command && early_effect_ack
                && early_effect_audible,
        "first direct-entry stage SFX was not audible and acknowledged");
    require(early_music_isolated,
        "first direct-entry stage SFX modified the music stem");

    enqueue_laser_effect();

    const auto fired = effect->tick({});
    const auto idle = control->tick({});
    auto fired_pcm = effect_audio.render_logic_tick(fired.audio_port_writes);
    auto idle_pcm = control_audio.render_logic_tick(idle.audio_port_writes);
    effect->synchronize_apu_output_ports(effect_audio.output_ports());
    control->synchronize_apu_output_ports(control_audio.output_ports());
    auto saw_command = std::find(fired.sound_effect_commands.begin(),
        fired.sound_effect_commands.end(), 0x35U)
        != fired.sound_effect_commands.end();
    auto saw_ack = effect_audio.output_ports()[3] == 0x35U;
    auto heard_difference = pcm_difference(fired_pcm, idle_pcm) != 0U;
    auto heard_effect = std::any_of(
        effect_audio.last_effect_samples().begin(),
        effect_audio.last_effect_samples().end(),
        [](std::int16_t sample) { return sample != 0; });
    auto effect_stem_differs = pcm_difference(
        effect_audio.last_effect_samples(),
        control_audio.last_effect_samples()) != 0U;
    auto music_is_isolated = std::equal(
        effect_audio.last_music_samples().begin(),
        effect_audio.last_music_samples().end(),
        control_audio.last_music_samples().begin(),
        control_audio.last_music_samples().end());
    for (std::size_t tick = 0; tick < 50U; ++tick) {
        const auto effect_tick = effect->tick({});
        const auto control_tick = control->tick({});
        fired_pcm = effect_audio.render_logic_tick(
            effect_tick.audio_port_writes);
        idle_pcm = control_audio.render_logic_tick(
            control_tick.audio_port_writes);
        effect->synchronize_apu_output_ports(effect_audio.output_ports());
        control->synchronize_apu_output_ports(control_audio.output_ports());
        saw_command = saw_command || std::find(
            effect_tick.sound_effect_commands.begin(),
            effect_tick.sound_effect_commands.end(), 0x35U)
                != effect_tick.sound_effect_commands.end();
        saw_ack = saw_ack || effect_audio.output_ports()[3] == 0x35U;
        heard_difference = heard_difference
            || pcm_difference(fired_pcm, idle_pcm) != 0U;
        heard_effect = heard_effect || std::any_of(
            effect_audio.last_effect_samples().begin(),
            effect_audio.last_effect_samples().end(),
            [](std::int16_t sample) { return sample != 0; });
        effect_stem_differs = effect_stem_differs || pcm_difference(
            effect_audio.last_effect_samples(),
            control_audio.last_effect_samples()) != 0U;
        music_is_isolated = music_is_isolated && std::equal(
            effect_audio.last_music_samples().begin(),
            effect_audio.last_music_samples().end(),
            control_audio.last_music_samples().begin(),
            control_audio.last_music_samples().end());
    }
    if (!(saw_command && saw_ack && heard_difference && effect_stem_differs
            && heard_effect && music_is_isolated)) {
        std::cerr << "direct SFX command=" << saw_command
                  << " acknowledgement=" << saw_ack
                  << " audible=" << heard_difference
                  << " effect-stem=" << heard_effect
                  << " effect-difference=" << effect_stem_differs
                  << " music-isolated=" << music_is_isolated << '\n';
    }
    require(saw_command && saw_ack && heard_difference
                && heard_effect && effect_stem_differs,
        "direct-level laser did not produce an isolated SPC effect stem");
    require(music_is_isolated,
        "sound effect command modified or consumed a music-channel sample");

    // SOUND.ASM's PLAYERSND writes the continuous Arwing engine to port 1;
    // NEAROBJS/DO_OBSTACLES use port 2 for other engines and positional
    // loops. MSU replaces only port-0 BGM, so both continuous buses must be
    // present in the independently mixed effect stem as well as port 3.
    auto continuous_effect_changed = false;
    auto continuous_music_unchanged = true;
    for (std::size_t tick = 0U; tick < 12U; ++tick) {
        // Repeated writes match the cartridge's per-raster engine update.
        constexpr std::array writes{
            starfox::simulation::ApuPortWrite{1U, 0xc0U, 0U},
            starfox::simulation::ApuPortWrite{2U, 0x60U, 0U},
        };
        static_cast<void>(effect_audio.render_logic_tick(writes));
        static_cast<void>(control_audio.render_logic_tick({}));
        continuous_effect_changed = continuous_effect_changed || pcm_difference(
            effect_audio.last_effect_samples(),
            control_audio.last_effect_samples()) != 0U;
        continuous_music_unchanged = continuous_music_unchanged && std::equal(
            effect_audio.last_music_samples().begin(),
            effect_audio.last_music_samples().end(),
            control_audio.last_music_samples().begin(),
            control_audio.last_music_samples().end());
    }
    require(continuous_effect_changed,
        "continuous engine ports did not reach the isolated effect stem");
    require(continuous_music_unchanged,
        "continuous effect bus modified the isolated music stem");

    // PAUSESND is carried over the nominal SFX port, but $01/$02 are global
    // controls in Nintendo's driver. The two host SPC stems must therefore
    // diverge when only one receives PAUSE ON; ordinary SFX above must not.
    constexpr std::array pause_on{
        starfox::simulation::ApuPortWrite{3U, 0x02U, 0U}};
    auto pause_changed_music = false;
    static_cast<void>(effect_audio.render_logic_tick(pause_on));
    static_cast<void>(control_audio.render_logic_tick({}));
    for (std::size_t tick = 0U; tick < 8U; ++tick) {
        static_cast<void>(effect_audio.render_logic_tick({}));
        static_cast<void>(control_audio.render_logic_tick({}));
        pause_changed_music = pause_changed_music || pcm_difference(
            effect_audio.last_music_samples(),
            control_audio.last_music_samples()) != 0U;
    }
    require(pause_changed_music,
        "global PAUSE ON command did not reach the isolated music driver");

    // A boss BGM must be replaced by the cartridge's short Player Down cue.
    constexpr std::array resume{
        starfox::simulation::ApuPortWrite{3U, 0x01U, 0U}};
    (void)effect_audio.render_logic_tick(resume);
    constexpr std::array boss{
        starfox::simulation::ApuPortWrite{0U, 5U, 0U}};
    (void)effect_audio.render_logic_tick(boss);
    for (int tick = 0; tick < 20; ++tick) (void)effect_audio.render_logic_tick({});
    require(std::any_of(effect_audio.last_music_samples().begin(),
        effect_audio.last_music_samples().end(), [](auto sample) { return sample != 0; }),
        "SPC audible restore fixture is silent");
    check_audio_restore(effect_audio);
    constexpr std::array death{
        starfox::simulation::ApuPortWrite{0U, 0x11U, 0U}};
    (void)effect_audio.render_logic_tick(death);
    auto heard_death = false;
    for (int tick = 0; tick < 160; ++tick) {
        (void)effect_audio.render_logic_tick({});
        const auto samples = effect_audio.last_music_samples();
        const auto audible = std::any_of(samples.begin(), samples.end(),
            [](auto sample) { return std::abs(static_cast<int>(sample)) > 8; });
        if (tick < 20) heard_death = heard_death || audible;
        if (tick >= 140) require(!audible, "Boss music continued after the death cue");
    }
    require(heard_death, "Player Down music was silent");
}
