// VideoOut mode selection, shared by sdl_video.c and the host tests.
#pragma once
#include <stdint.h>

#define STARFOX_PS_NO_MODE UINT32_MAX

// Index of the refresh rate closest to target_millihertz (the first on a
// tie), or STARFOX_PS_NO_MODE when there is none. RADV lists 119.88 Hz first
// when the title enables high frame rate; the game is paced for 60 Hz.
static inline uint32_t StarfoxPS5_PickRefresh(const uint32_t *refresh_millihertz, uint32_t count,
                                             uint32_t target_millihertz)
{
    uint32_t best = STARFOX_PS_NO_MODE;
    uint32_t best_distance = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t refresh = refresh_millihertz[i];
        const uint32_t distance = refresh > target_millihertz
            ? refresh - target_millihertz : target_millihertz - refresh;
        if (best == STARFOX_PS_NO_MODE || distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    return best;
}
