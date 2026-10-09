#include "starfox/audio/msu1_audio.hpp"
#include "starfox/audio/stem_mixer.hpp"
#include "starfox/state/container.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    require(argc == 2, "Expected synthetic FLAC fixture path");
    std::ifstream file(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    require(!bytes.empty(), "Missing fixture");
    // Compare the shared tick path against the previous desktop operations.
    // Include looping, stop and failed replacements; SPC must advance in all modes.
    for (bool enabled : {false,true}) {
        auto loader=[&](std::uint16_t track) {return track==50 ? std::vector<std::uint8_t>{} : bytes;};
        starfox::audio::Msu1Audio shared_msu(loader),reference_msu(loader);
        starfox::audio::Spc700Audio shared_spc,reference_spc;
        shared_msu.set_enabled(enabled);reference_msu.set_enabled(enabled);
        for(unsigned tick=0;tick<8;++tick) {
            const std::array writes{
                starfox::simulation::MsuRegisterWrite{0x2004,static_cast<std::uint8_t>(tick==6 ? 50 : 1),0},
                starfox::simulation::MsuRegisterWrite{0x2005,0,0},
                starfox::simulation::MsuRegisterWrite{0x2007,static_cast<std::uint8_t>(tick==4 ? 0 : 3),0}};
            std::vector<std::int16_t> actual;
            starfox::audio::render_mixed_tick(shared_spc,shared_msu,{},writes,73,41,actual);
            (void)reference_spc.render_logic_tick({});
            reference_msu.process_register_writes(writes);
            const auto music=reference_msu.enabled() && !reference_msu.resume_native_music()
                ? reference_msu.render(starfox::audio::Spc700Audio::stereo_frames_per_logic_tick,32000)
                : reference_spc.last_music_samples();
            const auto effects=reference_spc.last_effect_samples();
            require(actual.size()==std::min(music.size(),effects.size()),"Shared mixer changed tick length");
            for(std::size_t i=0;i<actual.size();++i)
                require(actual[i]==std::clamp(int(music[i])*73/100+int(effects[i])*41/100,-32768,32767),
                    "Shared mixer differs from desktop PCM");
            require(shared_spc.output_ports()==reference_spc.output_ports(),"Shared mixer changed SPC timing");
            require(shared_msu.playing()==reference_msu.playing(),"Shared mixer changed MSU state");
        }
    }
    starfox::audio::Msu1Audio audio([&](std::uint16_t track) {
        if (track == 50U) return std::vector<std::uint8_t>{};
        if (track == 51U) return std::vector<std::uint8_t>{1, 2, 3};
        return bytes;
    });
    audio.set_enabled(true);
    auto control = [&](std::uint8_t value, std::uint8_t track = 1) {
        const std::array writes{
            starfox::simulation::MsuRegisterWrite{0x2004, track, 0},
            starfox::simulation::MsuRegisterWrite{0x2005, 0, 0},
            starfox::simulation::MsuRegisterWrite{0x2007, value, 0}};
        audio.process_register_writes(writes);
    };
    auto silent = [](auto samples) {
        return std::all_of(samples.begin(), samples.end(), [](auto v) { return v == 0; });
    };
    require(silent(audio.render(10, 32000)), "Audio started before play command");
    control(1);
    require(!silent(audio.render(319, 32000)), "Play command did not start audio");
    require(audio.playing(), "One-shot ended early");
    audio.set_paused(true);
    require(silent(audio.render(100, 32000)) && audio.playing(), "Pause advanced playback");
    audio.set_paused(false);
    (void)audio.render(1, 32000);
    require(!audio.playing(), "One-shot remained active after its final sample");
    require(silent(audio.render(10, 32000)), "One-shot replayed after completion");
    control(3);
    require(!silent(audio.render(640, 32000)) && audio.playing(), "Loop ended at EOF");
    control(1, 38);
    require(audio.selected_track() == 38 && audio.playing(), "Death did not replace looping track");
    require(!silent(audio.render(320, 32000)) && !audio.playing(), "Death cue did not play once");
    control(0);
    require(!audio.playing() && silent(audio.render(10, 32000)), "Stop failed");
    for (const auto track : {50U, 51U}) {
        control(3);
        control(1, track);
        require(!audio.playing() && silent(audio.render(100, 32000)),
            "Failed replacement retained previous samples");
    }
    control(3, 2);
    require(!silent(audio.render(960, 32000)) && audio.playing(),
        "Short replacement did not loop from zero");
    control(1, 49);
    (void)audio.render(319, 32000);
    require(!audio.resume_native_music(), "Staff roll handed off before EOF");
    audio.set_paused(true);
    (void)audio.render(100, 32000);
    require(!audio.resume_native_music(), "Paused staff roll handed off");
    audio.set_paused(false);
    (void)audio.render(1, 32000);
    require(audio.resume_native_music(), "Staff EOF did not release native music");
    control(0, 49);
    require(!audio.resume_native_music(), "Explicit stop retained staff handoff");
    control(1, 38);
    (void)audio.render(640, 32000);
    require(!audio.resume_native_music(), "Death EOF incorrectly resumed native music");

    // Fractional resampling cursor, pause, loop, and pending track-register
    // writes survive restoration into a fresh device, sample for sample.
    control(3, 2);
    (void)audio.render(73, 44100);
    const std::array pending{starfox::simulation::MsuRegisterWrite{0x2004, 38, 0}};
    audio.process_register_writes(pending);
    audio.set_paused(true);
    const auto saved = audio.save_state();
    starfox::audio::Msu1Audio restored([&](std::uint16_t) { return bytes; });
    restored.load_state(saved);
    require(restored.save_state() == saved, "MSU restore changed playback state");
    require(silent(restored.render(100, 44100)), "MSU restore lost pause");
    audio.set_paused(false);
    restored.set_paused(false);
    for (int i = 0; i < 12; ++i) {
        const auto expected = audio.render(97, 44100);
        const auto actual = restored.render(97, 44100);
        require(std::equal(expected.begin(), expected.end(), actual.begin(), actual.end()),
            "Restored MSU samples diverged");
        require(audio.save_state() == restored.save_state(), "MSU cursor diverged");
    }
    auto invalid = saved;
    invalid.back() ^= 1U;
    const auto before = restored.save_state();
    bool rejected = false;
    try { restored.load_state(invalid); } catch (const std::exception&) { rejected = true; }
    require(rejected && restored.save_state() == before, "Corrupt MSU state mutated playback");
    starfox::audio::Msu1Audio missing;
    const auto empty = missing.save_state();
    rejected = false;
    try { missing.load_state(saved); } catch (const std::exception&) { rejected = true; }
    require(rejected && missing.save_state() == empty, "Missing MSU track accepted or mutated state");
    missing.load_state(empty);
    starfox::audio::Msu1Audio changed([&](std::uint16_t) {
        auto replacement = bytes;
        replacement.push_back(0); // Decodable, but not the saved asset.
        return replacement;
    });
    rejected = false;
    try { changed.load_state(saved); } catch (const std::exception&) { rejected = true; }
    require(rejected && changed.save_state() == empty, "Changed MSU asset accepted");
    auto payload = starfox::state::unpack(saved, 0x4d535501U, 0U);
    const auto truncated = starfox::state::pack(0x4d535501U, 0U,
        payload.first(payload.size() - 1));
    rejected = false;
    try { restored.load_state(truncated); } catch (const std::exception&) { rejected = true; }
    require(rejected && restored.save_state() == before, "Truncated MSU payload mutated playback");
    control(1, 49);
    (void)audio.render(320, 32000);
    restored.load_state(audio.save_state());
    require(restored.resume_native_music() && !restored.playing(), "MSU restore lost staff handoff");
}
