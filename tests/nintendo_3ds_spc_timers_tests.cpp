#include "starfox/platform/nintendo_3ds/spc_timers.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace starfox::platform::nintendo_3ds;
std::uint64_t checks{};
void require(bool pass) {
    ++checks;
    if (!pass) throw std::runtime_error("SPC timer quotient/state differs from pinned arithmetic");
}
void quotient(int clocks, int divisor) {
    const int expected = clocks / divisor;
    require(spc_prescaler_divide(clocks, divisor) == expected);
    require(spc_period_divide(clocks, divisor) == expected);
}
struct Timer {
    int next{}, divider{}, counter{};
    bool operator==(const Timer&) const = default;
};
void step(Timer& timer, int time, int prescaler, int period, bool optimized) {
    const int elapsed = (optimized ? spc_prescaler_divide(time - timer.next, prescaler)
        : (time - timer.next) / prescaler) + 1;
    timer.next += prescaler * elapsed;
    const int remain = static_cast<unsigned char>(period - timer.divider - 1) + 1;
    int divider = timer.divider + elapsed;
    const int over = elapsed - remain;
    if (over >= 0) {
        const int count = optimized ? spc_period_divide(over, period) : over / period;
        timer.counter = (timer.counter + 1 + count) & 15;
        divider = over - count * period;
    }
    timer.divider = static_cast<unsigned char>(divider);
}
}
int main() try {
    constexpr std::array boundaries{std::numeric_limits<int>::min(), -65537, -32768, -257, -256,
        -129, -128, -127, -17, -16, -15, -1, 0, 1, 15, 16, 17, 127, 128, 129,
        255, 256, 257, 32767, 65536, std::numeric_limits<int>::max()};
    // Every valid timer period plus arbitrary tempo prescalers, signed edge
    // cases and the exact non-power-of-two fallback. No reciprocal rounding.
    for (int divisor = 1; divisor <= 32768; ++divisor)
        for (int value : boundaries) quotient(value, divisor);
    for (int clocks = -65537; clocks <= 65537; ++clocks)
        for (int divisor : {16, 128, 256, 13, 104}) quotient(clocks, divisor);
    for (int period = 1; period <= 256; ++period) for (int prescaler : {16, 128, 13, 104}) {
        for (int initial = 0; initial < period; ++initial) {
            Timer actual{-100, initial, 13}, reference = actual;
            for (int delta : {0, 1, 15, 16, 127, 128, 255, 256, 32749, 51200}) {
                const int time = actual.next + delta;
                step(actual, time, prescaler, period, true);
                step(reference, time, prescaler, period, false);
                require(actual == reference);
            }
        }
    }
    std::cout << "Exact SPC timer divisions and carry/counter state: " << checks << " checks PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
