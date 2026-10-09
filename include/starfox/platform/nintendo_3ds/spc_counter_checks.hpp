#pragma once
#include "spc_counters.hpp"
#include <array>
#include <limits>
#include <stdexcept>

namespace starfox::platform::nintendo_3ds {
// Independent pinned rates and offsets: do not derive the reference divisor
// from the reciprocal table under test. Shared by host and actual ARM probe.
inline unsigned check_spc_counters() {
    constexpr std::array<unsigned, 32> rates{
        30721, 2048, 1536, 1280, 1024, 768, 640, 512,
        384, 320, 256, 192, 160, 128, 96, 80,
        64, 48, 40, 32, 24, 20, 16, 12,
        10, 8, 6, 5, 4, 3, 2, 1};
    constexpr std::array<unsigned, 32> offsets{
        1, 0, 1040, 536, 0, 1040, 536, 0,
        1040, 536, 0, 1040, 536, 0, 1040, 536,
        0, 1040, 536, 0, 1040, 536, 0, 1040,
        536, 0, 1040, 536, 0, 1040, 0, 0};
    static_assert(std::numeric_limits<unsigned>::digits == 32);
    unsigned checks = 0;
    const auto check = [&](unsigned counter, unsigned rate) {
        // This addition intentionally wraps exactly like the pinned DSP.
        const unsigned value = counter + offsets[rate];
        const unsigned expected = value % rates[rate];
        if (spc_dsp_counter_remainder(value, rates[rate],
                spc_dsp_counter_reciprocals[rate]) != expected)
            throw std::runtime_error("Native SPC counter remainder changed an exact result");
        ++checks;
    };
    for (unsigned rate = 0; rate < 32; ++rate) {
        if (spc_dsp_counter_divisors[rate] != rates[rate])
            throw std::runtime_error("Native SPC reciprocal rates differ from pinned DSP");
        // Every normal counter and every actual envelope/noise rate. Include
        // the counter wrap and 'never fires' rate, not just common ADSR rates.
        for (unsigned counter = 0; counter < 30720; ++counter) check(counter, rate);
        for (unsigned edge : {0U, 1U, rates[rate] - 1, rates[rate],
                0x7fffffffU, 0x80000000U, 0xfffffc00U, 0xffffffffU})
            for (unsigned delta = 0; delta < 129; ++delta) {
                check(edge + delta, rate);
                check(edge - delta, rate);
            }
    }
    // Full uint32 coverage is also relevant to restored negative counters.
    // Keep the oracle's division live (different input per call).
    std::uint32_t state = 0xa831ed57;
    for (unsigned index = 0; index < 2'097'152; ++index) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        check(state, index & 31U);
    }
    return checks;
}
} // namespace starfox::platform::nintendo_3ds
