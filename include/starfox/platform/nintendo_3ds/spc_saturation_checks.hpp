#pragma once
#include "spc_saturation.hpp"
#include <array>
#include <stdexcept>

namespace starfox::platform::nintendo_3ds {
// Host and native probe use identical inputs and an independent 64-bit oracle.
// Unsigned conversion is explicitly decoded, not an implementation-defined
// uint32->int32 conversion or the same int16/sign-xor expression being tested.
inline unsigned check_spc_saturation() {
    unsigned checks=0;
    const auto check=[&](std::int64_t value) {
        const auto expected=value < -32768 ? -32768 : value > 32767 ? 32767 : value;
        if(spc_saturate16(static_cast<int>(value))!=expected)
            throw std::runtime_error("Native SPC signed saturation changed an exact 32-bit result");
        ++checks;
    };
    for(std::int64_t value=-65536;value<=65536;++value) check(value);
    for(const std::int64_t edge:{std::int64_t{-2147483648},std::int64_t{-32768},
            std::int64_t{0},std::int64_t{32767},std::int64_t{2147483647}})
        for(int offset=-128;offset<=128;++offset) {
            const auto value=edge+offset;
            if(value>=-2147483648LL && value<=2147483647LL) check(value);
        }
    // Include representative Gaussian/BRR/voice/echo accumulator boundaries,
    // along with non-repeating coverage of the full 32-bit domain.
    std::uint32_t state=0x51a7c39b;
    for(unsigned i=0;i<4'194'304;++i) {
        state=state*1664525U+1013904223U;
        const std::int64_t value=state<=2147483647U ? std::int64_t(state) : std::int64_t(state)-4294967296LL;
        check(value);
    }
    return checks;
}
} // namespace starfox::platform::nintendo_3ds
