#pragma once

namespace starfox::platform::nintendo_3ds {
// The original ARM11 has no integer divide instruction. Keep normal SPC
// prescalers as constant divisions so the compiler can use exact shifts/bias;
// arbitrary restored tempos retain the original division. In particular, use
// C++ division (not a signed shift) to preserve truncation for negative values.
inline int spc_prescaler_divide(int clocks, int prescaler) noexcept {
    if (prescaler == 128) return clocks / 128;
    if (prescaler == 16) return clocks / 16;
    return clocks / prescaler;
}
inline int spc_period_divide(int clocks, int period) noexcept {
    if (clocks >= 0 && clocks < period) return 0;
    if (period == 256) return clocks / 256;
    return clocks / period;
}
}
