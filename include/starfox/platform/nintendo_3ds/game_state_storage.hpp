#pragma once
#include "starfox/platform/nintendo_3ds/game_state.hpp"
#include <optional>
#include <string>

namespace starfox::platform::nintendo_3ds {
struct GameStateInfo {
    bool found{},writable{true};
    std::uint64_t generation{};
    std::string warning;
};
struct GameStateLoad {
    GameStateInfo info;
    std::vector<std::uint8_t> bytes;
};
// Ten cartridge-bound logical slots, each with two alternating generations.
// Separate from settings and EX battery RAM. Only explicit Save writes a state.
// Close/re-read before committing; preserve the newest valid generation on
// failure. No promise of physical SD power-loss durability or multi-writer lock.
class GameStateStorage {
public:
    static constexpr unsigned slots=10;
    static constexpr std::size_t maximum_file_bytes=maximum_game_state_bytes+64;
    GameStateStorage(std::string directory,std::uint32_t companion_manifest,std::uint32_t rom_crc);
    [[nodiscard]] GameStateLoad load(unsigned slot);
    bool save(unsigned slot,std::span<const std::uint8_t> state);
    [[nodiscard]] const GameStateInfo& current(unsigned slot) const;
    [[nodiscard]] std::string slot_path(unsigned slot,unsigned generation_slot) const;
private:
    struct Cached {
        GameStateInfo info;
        std::optional<unsigned> newest;
        std::uint32_t crc{};
        std::size_t size{};
        bool initialized{};
    };
    std::string directory_;
    std::uint32_t manifest_,rom_crc_;
    std::array<Cached,slots> cached_{};
};
} // namespace starfox::platform::nintendo_3ds
