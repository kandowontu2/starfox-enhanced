#pragma once
#include <array>
#include <cstdint>

namespace starfox::platform::nintendo_3ds {
// The pinned DSP has 32 immutable counter rates. These are used only to
// generate reciprocals at compile time; the live DSP still supplies its own
// original rate and preserves the unsigned offset addition before modulo.
inline constexpr std::array<std::uint32_t, 32> spc_dsp_counter_divisors{
    30721, 2048, 1536, 1280, 1024, 768, 640, 512,
    384, 320, 256, 192, 160, 128, 96, 80,
    64, 48, 40, 32, 24, 20, 16, 12,
    10, 8, 6, 5, 4, 3, 2, 1};
inline constexpr auto spc_dsp_counter_reciprocals = [] {
    std::array<std::uint32_t, 32> result{};
    for (unsigned rate = 0; rate < result.size(); ++rate) {
        const auto divisor = spc_dsp_counter_divisors[rate];
        // 2^32 is not representable as a uint32 reciprocal for divisor one.
        if (divisor != 1)
            result[rate] = std::uint32_t((std::uint64_t{1} << 32) / divisor);
    }
    return result;
}();

// For floor(2^32 / divisor), the high product is either the exact quotient or
// one below it, over the entire uint32 domain. q*divisor cannot overflow since
// q <= value/divisor. One subtraction therefore restores the exact remainder.
// ARMv6K lowers the high product to UMULL, not a software division. This is
// not an approximation or a restriction to the normal 0..30719 DSP counter.
inline std::uint32_t spc_dsp_counter_remainder(std::uint32_t value,
        std::uint32_t divisor, std::uint32_t reciprocal) noexcept {
    if (divisor == 1) return 0;
    const auto quotient = std::uint32_t((std::uint64_t{value} * reciprocal) >> 32);
    const auto remainder = value - quotient * divisor;
    return remainder >= divisor ? remainder - divisor : remainder;
}
} // namespace starfox::platform::nintendo_3ds
