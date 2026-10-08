// A half-plane can emit at most floor(3*N/2) corners, even for nonconvex
// source polygons. Four input corners through near + four screen planes have
// bounds 4 -> 6 -> 9 -> 13 -> 19 -> 28. No convexity assumption or truncation.
// Keep the ordinary 129-record output ABI and all original arithmetic.
#define STARFOX_CLIP_SOURCE_CORNERS 4
#define STARFOX_CLIP_CAPACITY 32
#include "clip_portable.hlsl"
