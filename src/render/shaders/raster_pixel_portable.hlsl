// Same source painter, specialized only when no receiver/depth output exists.
// Keep native texture, dither, coverage and bank/wave semantics unchanged.
#define STARFOX_PIXEL_ONLY_RASTER 1
#include "raster_portable.hlsl"
