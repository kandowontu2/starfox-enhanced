#pragma once
#include "spc_output.hpp"
#include <array>
#include <stdexcept>

namespace starfox::platform::nintendo_3ds {
// Opaque test-only call boundary: the probe must execute the selector, not
// optimize the candidate and independent oracle into the same expression.
#if defined(__GNUC__) && __GNUC__ >= 9 && !defined(__clang__)
__attribute__((noipa))
#elif defined(__GNUC__)
__attribute__((noinline))
#endif
inline int spc_output_probe_sample(int envelope, bool enabled, int noise,
                                    int gaussian, unsigned& calls) {
    return spc_voice_sample(envelope,enabled,noise,[&] {++calls; return gaussian;});
}
inline unsigned check_spc_output() {
    unsigned checks = 0;
    const auto check = [&](int envelope, bool noise_enabled, int noise, int gaussian) {
        unsigned calls = 0;
        const int selected = spc_output_probe_sample(envelope,noise_enabled,noise,gaussian,calls);
        // Independent original ordering: compute Gaussian unconditionally,
        // replace it with signed 16-bit noise, then apply the CURRENT envelope.
        int original = gaussian;
        if (noise_enabled) original = static_cast<std::int16_t>(noise * 2);
        const int expected = ((original * envelope) >> 11) & ~1;
        const int actual = ((selected * envelope) >> 11) & ~1;
        if (actual != expected || calls != unsigned(envelope != 0 && !noise_enabled))
            throw std::runtime_error("SPC voice output or interpolation-call selection changed");
        ++checks;
    };
    // Full Gaussian int16 domain, including odd values stronger than the
    // actual even Gaussian output; real envelope boundaries and negative
    // bounded restore values. Preserve each signed shift/truncation.
    constexpr std::array envelopes{0,1,15,16,255,256,1023,1024,2046,2047,-1,-2047};
    constexpr std::array noises{0,1,0x1fff,0x3fff,0x4000,0x4001,0x7ffe,0x7fff};
    for (int gaussian = -32768; gaussian <= 32767; ++gaussian)
        for (int envelope : envelopes) for (int noise : noises)
            for (bool enabled : {false,true}) check(envelope,enabled,noise,gaussian);
    // Every reachable noise-LFSR value and zero/nonzero envelope decisions.
    for (int noise=0;noise<32768;++noise) for (int envelope : {0,1,2047})
        check(envelope,true,noise,12345);
    return checks;
}
} // namespace starfox::platform::nintendo_3ds
