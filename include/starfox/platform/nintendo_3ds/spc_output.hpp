#pragma once
#include <cstdint>

namespace starfox::platform::nintendo_3ds {
// The pinned Gaussian interpolator only reads its sample buffer. Noise replaces
// that result, and a zero current envelope annihilates it before any observable
// output. Do not skip voice clocks, decoding, pitch, envelopes or noise updates.
template<class Interpolate>
inline int spc_voice_sample(int current_envelope, bool noise_enabled, int noise,
                            Interpolate interpolate) {
    if (current_envelope == 0) return 0;
    if (noise_enabled) return static_cast<std::int16_t>(noise * 2);
    return interpolate();
}
} // namespace starfox::platform::nintendo_3ds
