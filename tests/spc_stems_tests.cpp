#include "starfox/audio/spc700_audio.hpp"
#if defined(STARFOX_SPC_PARALLEL_ORACLE)
#include "native_stem_executor.hpp"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace {
std::atomic<std::size_t> allocations{};
bool count_allocations{};
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
using Audio = starfox::audio::Spc700Audio;
using Write = starfox::simulation::ApuPortWrite;

// An original, tiny SPC noise driver, not cartridge bytes or an IPL ROM.
// It echoes all four CPU ports and uses ports 0/3 to vary its DSP output.
std::vector<Write> upload(bool restart) {
    std::vector<std::uint8_t> program;
    const auto dsp = [&](std::uint8_t reg, std::uint8_t value) {
        program.insert(program.end(), {0x8f, reg, 0xf2, 0x8f, value, 0xf3});
    };
    dsp(0x6c, 0x3f); // Echo disabled; no reset/mute; audible noise rate.
    dsp(0x0c, 0x7f); dsp(0x1c, 0x7f);
    dsp(0x00, 0x60); dsp(0x01, 0x60);
    dsp(0x04, 0x00); dsp(0x05, 0x00); dsp(0x07, 0x7f);
    dsp(0x5d, 0x02); dsp(0x3d, 0x01); dsp(0x5c, 0x00); dsp(0x4c, 0x01);
    const auto loop = program.size();
    program.insert(program.end(), {0xe4, 0xf4, 0x28, 0x1f, 0x08, 0x30,
        0x8f, 0x6c, 0xf2, 0xc4, 0xf3,
        0xe4, 0xf7, 0x28, 0x7f, 0x8f, 0x01, 0xf2, 0xc4, 0xf3});
    for (std::uint8_t port = 0; port < 4; ++port) {
        const auto address = static_cast<std::uint8_t>(0xf4 + port);
        program.insert(program.end(), {0xe4, address, 0xc4, address});
    }
    const auto branch = static_cast<int>(loop) - static_cast<int>(program.size() + 2);
    require(branch >= -128, "Synthetic SPC loop is too large");
    program.insert(program.end(), {0x2f, static_cast<std::uint8_t>(branch)});
    std::vector<Write> writes;
    if (restart) writes.push_back({0, 0xff, 0});
    const auto block = [&](std::uint16_t address, std::span<const std::uint8_t> bytes) {
        writes.insert(writes.end(), {{2, static_cast<std::uint8_t>(address), 0},
            {3, static_cast<std::uint8_t>(address >> 8), 0}, {1, 1, 0}, {0, 0xcc, 0}});
        for (auto byte : bytes) writes.push_back({1, byte, 0});
    };
    constexpr std::array<std::uint8_t, 4> directory{0, 3, 0, 3};
    constexpr std::array<std::uint8_t, 9> sample{3, 0, 0, 0, 0, 0, 0, 0, 0};
    block(0x0200, directory); block(0x0300, sample); block(0x0400, program);
    writes.insert(writes.end(), {{2, 0, 0}, {3, 4, 0}, {0, 0xcc, 0}, {1, 0, 0}});
    return writes;
}

void advance(Audio& audio, std::span<const Write> writes) {
#if defined(STARFOX_AUDIO_LEGACY_ORACLE)
    // Compile this fixture against the unchanged pre-optimization source and
    // preserve its executable separately to compare complete state/PCM traces.
    static_cast<void>(audio.render_logic_tick(writes));
#else
#if defined(STARFOX_SPC_PARALLEL_ORACLE)
    static starfox::platform::nintendo_3ds::NativeStemExecutor executor;
    require(executor.parallel_available(), "Parallel oracle accidentally used serial fallback");
    audio.render_stems_logic_tick(writes, &executor);
#else
    audio.render_stems_logic_tick(writes);
#endif
#endif
}
std::uint64_t digest = 14695981039346656037ULL;
void record(std::ostream* output, std::span<const std::uint8_t> bytes) {
    for (auto byte : bytes) { digest ^= byte; digest *= 1099511628211ULL; }
    if (output) output->write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
void record(std::ostream* output, const Audio& audio) {
    const auto state = audio.save_state();
    record(output, state); // Includes both PCM stems, DSP/filter/carry and CPU ports.
}
void check_tick(Audio& stems, Audio& mixed, std::span<const Write> writes,
    std::ostream* trace, bool& audible) {
    advance(stems, writes);
    const auto samples = mixed.render_logic_tick(writes);
    const auto music = stems.last_music_samples(), effects = stems.last_effect_samples();
    require(samples.size() == Audio::stereo_frames_per_logic_tick * 2,
        "Mixed block did not retain exactly 50 ms of stereo PCM");
    require(music.size() == samples.size() && effects.size() == samples.size(),
        "Stem block size changed");
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto expected = std::clamp(std::int32_t(music[i]) + effects[i],
            std::int32_t(std::numeric_limits<std::int16_t>::min()),
            std::int32_t(std::numeric_limits<std::int16_t>::max()));
        require(samples[i] == expected, "Legacy saturated mix differs from stems");
        audible |= samples[i] != 0;
    }
    require(stems.save_state() == mixed.save_state(), "Stem-only SPC state changed");
    require(stems.output_ports() == mixed.output_ports(), "Stem-only handshakes changed");
    record(trace, stems);
}
}

// Count steady-state C++ heap allocations around just the audio call. Recording
// and state comparison allocate outside this scope; no timing claim is made.
void* operator new(std::size_t count) {
    if (count_allocations) ++allocations;
    if (auto* memory = std::malloc(std::max(count, std::size_t{1}))) return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t count) { return ::operator new(count); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main(int argc, char** argv) {
    try {
        require(argc == 1 || argc == 2, "Usage: spc_stems_tests [private trace file]");
        std::ofstream file;
        if (argc == 2) {
            file.open(argv[1], std::ios::binary);
            require(bool(file), "Cannot open private trace output");
        }
        auto* trace = argc == 2 ? static_cast<std::ostream*>(&file) : nullptr;
        Audio stems, mixed;
        bool audible{};
        record(trace, stems);
        check_tick(stems, mixed, {}, trace, audible); // Not loaded: both silent.
        auto bank = upload(false);
        const auto split = bank.size() / 2;
        check_tick(stems, mixed, std::span(bank).first(split), trace, audible);
        require(!stems.driver_loaded(), "Partial upload unexpectedly loaded a driver");
        Audio restored;
        restored.load_state(stems.save_state());
        stems = std::move(restored);
        check_tick(stems, mixed, std::span(bank).subspan(split), trace, audible);
        require(stems.driver_loaded(), "Synthetic upload did not load the SPC driver");
        const auto* music_buffer = stems.last_music_samples().data();
        const auto* effects_buffer = stems.last_effect_samples().data();
        for (unsigned tick = 0; tick < 96; ++tick) {
            const std::array writes{
                Write{0, static_cast<std::uint8_t>(tick % 31), 17},
                Write{1, static_cast<std::uint8_t>(tick * 3), 512},
                Write{2, static_cast<std::uint8_t>(tick * 7), 51'199},
                Write{3, static_cast<std::uint8_t>(tick % 8 == 0 ? 2 : tick % 8 == 1 ? 1 : 96), 13},
                Write{3, 64, 70'000}}; // Rewound/out-of-frame timestamps must clamp as before.
            check_tick(stems, mixed, writes, trace, audible);
#if !defined(STARFOX_AUDIO_LEGACY_ORACLE)
            require(music_buffer == stems.last_music_samples().data()
                && effects_buffer == stems.last_effect_samples().data(),
                "Steady stem output was reallocated");
#endif
            if (tick == 20) {
                Audio next;
                next.load_state(stems.save_state());
                stems = std::move(next);
                music_buffer = stems.last_music_samples().data();
                effects_buffer = stems.last_effect_samples().data();
            }
        }
        require(audible, "Synthetic fixture never exercised audible DSP/filter samples");
        auto replacements = upload(true);
        const auto second = upload(true);
        replacements.insert(replacements.end(), second.begin(), second.end());
        const auto stem_frames = stems.prime_upload_sequence(replacements);
        require(stem_frames == mixed.prime_upload_sequence(replacements) && stem_frames == 3,
            "Multi-bank restart changed SPC initialization cadence");
        require(stems.last_music_samples().empty() && stems.last_effect_samples().empty(),
            "Inaudible upload priming exposed startup samples");
        require(stems.save_state() == mixed.save_state(), "Primed SPC state changed");
        record(trace, stems);
        for (unsigned tick = 0; tick < 24; ++tick) check_tick(stems, mixed, {}, trace, audible);
        allocations = 0;
        count_allocations = true;
        for (unsigned tick = 0; tick < 32; ++tick) advance(stems, {});
        count_allocations = false;
#if !defined(STARFOX_AUDIO_LEGACY_ORACLE)
        require(allocations == 0, "Warmed stem-only audio still allocates per tick");
#endif
        record(trace, stems);
        // Captured from a separate executable compiled before the stem/buffer
        // refactor (pinned accurate core, original synthetic driver above).
        // Keep an independent PCM/state oracle, not just two new API wrappers.
        require(digest == 0xe7cddb7195a79cc2ULL, "Pre-refactor state/PCM trace changed");
        if (trace) { file.flush(); require(bool(file), "Private trace write failed"); }
        std::cout << "SPC stem-only: partial uploads, 120 audible ticks, timestamp clamping,\n"
            << "restore, saturated mix, multi-bank priming and exact state/PCM checked; digest "
            << std::hex << digest << std::dec << "; warmed allocations " << allocations << '\n';
        return 0;
    } catch (const std::exception& error) {
        count_allocations = false;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
