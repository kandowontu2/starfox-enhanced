// Same proven 28-corner upper bound as clip_small_portable, including near
// intersections and the compensated/software-binary64 paths.
#define STARFOX_CLIP_SOURCE_CORNERS 4
#define STARFOX_CLIP_CAPACITY 32
#include "clip_continuous_portable.hlsl"
