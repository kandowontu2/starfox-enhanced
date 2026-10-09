#pragma once
#include "starfox/render/grid_line_history.hpp"
#include "starfox/simulation/wdc65816.hpp"
#include <span>
#include <vector>

namespace starfox::platform::nintendo_3ds {
// Explicit native timeline fields, not object memory or platform resources.
// The outer checksum binds the actual cartridge; component envelopes retain
// their own schemas. The SD journal separately binds the companion manifest.
struct GameStateData {
    std::vector<std::uint8_t> game,audio;
    std::vector<simulation::ApuPortWrite> pending_audio;
    std::uint8_t audio_phase{};
    render::GridLineHistory::State grid;
    std::uint64_t scene_revision{};
    std::array<std::int16_t,2> grid_start{};
};
inline constexpr std::uint32_t game_state_schema=0x33445310U;
inline constexpr std::size_t maximum_game_state_bytes=4U*1024U*1024U;
inline constexpr std::size_t maximum_pending_audio_writes=4096;
[[nodiscard]] std::vector<std::uint8_t> encode_game_state(const GameStateData&,std::uint32_t rom_crc);
[[nodiscard]] GameStateData decode_game_state(std::span<const std::uint8_t>,std::uint32_t rom_crc);
} // namespace starfox::platform::nintendo_3ds
