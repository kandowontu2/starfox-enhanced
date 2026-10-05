#pragma once

namespace starfox::platform::nintendo_3ds {
// Only the pinned core's plain RAM range. I/O ($f0..$ff), high-ROM writes,
// negative/padded/wrapped addresses must retain its original slow path.
constexpr bool spc_plain_ram_read(int address) noexcept {
    return static_cast<unsigned>(address) < 0x10000U
        && (static_cast<unsigned>(address) - 0xf0U) >= 0x10U;
}
constexpr bool spc_plain_ram_write(int address) noexcept {
    return static_cast<unsigned>(address) < 0xffc0U
        && (static_cast<unsigned>(address) - 0xf0U) >= 0x10U;
}
} // namespace starfox::platform::nintendo_3ds
