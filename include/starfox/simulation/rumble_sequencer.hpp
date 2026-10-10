#pragma once

#include "starfox/assets/rom.hpp"
#include <cstdint>
#include <optional>

namespace starfox::simulation {
class MapVm;

struct RumbleEffect {
    std::uint16_t low_frequency{};
    std::uint16_t high_frequency{};
    std::uint32_t duration_ms{40U};
    [[nodiscard]] bool active() const noexcept {
        return low_frequency != 0U || high_frequency != 0U;
    }
    friend bool operator==(const RumbleEffect&, const RumbleEffect&) = default;
};

// Advances the cartridge-authored RUMBLE_* register sequencer once per native
// source raster. SDL and OpenXR only adapt the resulting effect to hardware.
class RumbleSequencer {
public:
    explicit RumbleSequencer(const assets::SymbolMap& symbols) noexcept;
    [[nodiscard]] bool available() const noexcept;
    // A disabled experience or missing cartridge symbols leaves the native
    // registers untouched and tells the output adapter to stop its actuator.
    [[nodiscard]] std::optional<RumbleEffect> advance(
        MapVm& map, bool enabled) const noexcept;

private:
    std::uint32_t command_{};
    std::uint32_t time_{};
    std::uint32_t index_{};
    std::uint32_t table_{};
};
} // namespace starfox::simulation
