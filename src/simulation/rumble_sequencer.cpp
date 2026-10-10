#include "starfox/simulation/rumble_sequencer.hpp"
#include "starfox/simulation/map_vm.hpp"

namespace starfox::simulation {
namespace {
std::uint32_t address(const assets::SymbolMap& symbols, const char* name) noexcept {
    const auto found = symbols.find(name);
    return found.empty() ? 0U : found.front();
}
} // namespace

RumbleSequencer::RumbleSequencer(const assets::SymbolMap& symbols) noexcept
    : command_(address(symbols, "RUMBLE_CMD")),
      time_(address(symbols, "RUMBLE_TIME")),
      index_(address(symbols, "RUMBLE_INDEX")),
      table_(address(symbols, "RUMBLE_TABLE")) {}

bool RumbleSequencer::available() const noexcept {
    return command_ != 0U && time_ != 0U && index_ != 0U && table_ != 0U;
}

std::optional<RumbleEffect> RumbleSequencer::advance(
    MapVm& map, bool enabled) const noexcept {
    if (!available() || !enabled) return std::nullopt;

    auto output = std::uint8_t{};
    auto sequence_index = map.read_native_byte(index_);
    for (std::size_t guard = 0U; guard < 4U; ++guard) {
        if (sequence_index == 0U) {
            output = map.read_native_byte(time_) == 0U
                ? 0U : map.read_native_byte(command_);
            break;
        }
        output = map.read_native_byte(
            table_ + static_cast<std::uint32_t>(sequence_index - 1U));
        sequence_index = static_cast<std::uint8_t>(sequence_index + 1U);
        map.write_native_byte(index_, sequence_index);
        if (output == 0x19U) {
            map.write_native_byte(index_, 0U);
            output = 0U;
            break;
        }
        if (output != 0x91U) break;
        sequence_index = 1U;
        map.write_native_byte(index_, sequence_index);
    }

    const auto remaining = map.read_native_byte(time_);
    if (remaining != 0U) {
        map.write_native_byte(time_,
            static_cast<std::uint8_t>(remaining - 1U));
    }
    return RumbleEffect{
        static_cast<std::uint16_t>(((output >> 4U) & 0x0fU) * 0x1111U),
        static_cast<std::uint16_t>((output & 0x0fU) * 0x1111U),
        40U};
}
} // namespace starfox::simulation
