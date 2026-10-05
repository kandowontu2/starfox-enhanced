#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
std::uint64_t now() noexcept {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
void hash_bytes(std::uint64_t& hash, std::span<const std::uint8_t> bytes) {
    for (auto byte : bytes) { hash ^= byte; hash *= 1099511628211ULL; }
}
void hash_number(std::uint64_t& hash, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) {
        hash ^= std::uint8_t(value >> (i * 8U)); hash *= 1099511628211ULL;
    }
}
void measure(const assets::RomImage& rom, const assets::SymbolMap& symbols, const char* map, bool full_trace) {
    std::uint64_t trace = 14695981039346656037ULL, blocks = 0;
    GameSession session(rom, symbols, [&](auto samples) {
        ++blocks;
        for (auto sample : samples) {
            const auto value = std::uint16_t(sample);
            const std::array bytes{std::uint8_t(value), std::uint8_t(value >> 8U)};
            hash_bytes(trace, bytes);
        }
    }, map);
    FrameProfile profile(&now, 1000000000ULL);
    session.advance(0, 0);
    const unsigned frames = full_trace ? 240U : 2400U;
    unsigned rasters = 0, logic = 0;
    for (unsigned frame = 1; frame <= frames; ++frame) {
        const auto time = (std::int64_t(frame) * 1000000000 + 59) / 60;
        GameAdvance advanced;
        {
            const ScopedFrameProfileActivation active(profile);
            advanced = session.advance(time, 0);
        }
        rasters += advanced.video_phases; logic += advanced.logic_ticks;
        hash_number(trace, advanced.video_phases); hash_number(trace, advanced.logic_ticks);
        hash_number(trace, advanced.audio_blocks);
        // Entire serialized VM and SPC at every source raster, including
        // repeated no-logic phases, plus all emitted PCM and phase counters.
        // Do not wrap these in the native SD-slot envelope: its pending-event
        // limit may reject a direct stage-bank upload. This diagnostic does
        // not alter that production limit or claim native-slot parity.
        if (full_trace || frame == frames) {
            hash_bytes(trace, session.game().save_state());
            hash_bytes(trace, session.audio().save_state());
        }
    }
    if (rasters != frames || blocks != frames / 3U || !logic)
        throw std::runtime_error("Actual source cadence changed in CPU timing fixture");
    for (unsigned index = 0; index < unsigned(FramePhase::count); ++index) {
        const auto phase = FramePhase(index);
        const auto& sample = profile.totals()[unsigned(phase)];
        if (!sample.calls) {
            if (phase == FramePhase::logic || phase == FramePhase::video
                || phase == FramePhase::capture || phase == FramePhase::audio)
                throw std::runtime_error("Missing actual phase timer; enable native frame profiling");
            continue;
        }
        std::cout << map << ',' << frame_phase_names[unsigned(phase)] << ',' << sample.calls
            << ',' << double(sample.ticks) / 1000000.0 << ',' << rasters << ',' << logic << ',' << blocks
            << ',' << std::hex << trace << std::dec << std::endl;
    }
}
} // namespace
int main(int argc, char** argv) try {
    if (argc != 3 && (argc != 4 || std::string_view(argv[3]) != "--timing-only"))
        throw std::invalid_argument("Usage: check_3ds_cpu_timing ROM SYMBOLS [--timing-only]");
#ifndef STARFOX_3DS_PROFILE_FRAMES
    throw std::runtime_error("Compile this diagnostic against the actual profiling-enabled cartridge graph");
#endif
    const auto rom = assets::RomImage::load(argv[1]);
    const auto symbols = assets::SymbolMap::load(argv[2]);
    std::cout << "map,phase,calls,milliseconds,source_rasters,logic_ticks,spc_blocks,complete_state_pcm_digest\n";
    // Full parity is the default (240 rasters). Timing-only runs 2400 rasters,
    // retains complete final VM/SPC, all PCM and raster counters but avoids
    // repeated save-state cache churn; it is not full per-raster parity.
    for (auto map : {"BOOT", "LEVEL1_1", "LEVEL1_2"}) measure(rom, symbols, map, argc == 3);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
