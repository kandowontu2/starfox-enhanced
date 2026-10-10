// Only dispatch placement changes; the precise checked optical writer is shared.
#define STARFOX_SOURCE_OPTICAL_STREAM 1
#define STARFOX_SOURCE_OPTICAL_LANES 64
#define feature_optical_main feature_optical_stream_main
#include "reflection_source_optical.hlsl"
