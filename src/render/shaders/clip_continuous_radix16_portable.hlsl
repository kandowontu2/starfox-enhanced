// Only replace the exact binary64 divider. All projection, clipping,
// backface, source transport, scratch capacities and output ABI stay intact.
#define STARFOX_CLIP_RADIX16_DIV 1
#include "clip_continuous_portable.hlsl"
