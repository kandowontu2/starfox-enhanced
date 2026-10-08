#pragma once
#include <cstdint>
#include <optional>

namespace starfox::platform::nintendo_3ds {
struct HardwareProfile {
    bool cpu_speedup{}, stereo{};
    bool operator==(const HardwareProfile&) const=default;
};
// CFGU's model IDs are checked against the actual SDK in native_display.cpp.
// Unknown/failed queries deliberately keep the old-hardware mono baseline.
// New 2DS XL has the faster CPU but never a stereoscopic LCD.
inline constexpr HardwareProfile hardware_profile(std::optional<std::uint8_t> model) noexcept {
    if(!model) return {};
    switch(*model) {
    case 2:case 4:return {true,true}; // New 3DS / New 3DS XL.
    case 5:return {true,false};     // New 2DS XL.
    default:return {};             // Original 3DS/XL, 2DS, unknown.
    }
}
} // namespace starfox::platform::nintendo_3ds
