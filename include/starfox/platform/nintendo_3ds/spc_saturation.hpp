#pragma once
#include <cstdint>
#include <limits>
#if defined(__ARM_FEATURE_SAT) && __ARM_FEATURE_SAT
#include <arm_acle.h>
#elif defined(__3DS__)
#error "Native 3DS SPC saturation requires the ARMv6K signed-saturation instruction"
#endif

namespace starfox::platform::nintendo_3ds {
static_assert(std::numeric_limits<int>::digits==31,"Pinned SPC DSP requires 32-bit signed int");
// Same [-32768,32767] clamp as the pinned DSP macro. No rounding, voice
// skipping, sample dropping or filter/state changes. Only the private 3DS DSP
// source selects this operation; upstream files and desktop SPC stay intact.
inline int spc_saturate16(int value) noexcept {
#if defined(__ARM_FEATURE_SAT) && __ARM_FEATURE_SAT
    return __ssat(value,16);
#else
    // Host contract/replay path exercises the same mathematical operation.
    return value < -32768 ? -32768 : value > 32767 ? 32767 : value;
#endif
}
} // namespace starfox::platform::nintendo_3ds
