// One native painter pass for small models: share each row's authored command
// across its 64 pixel lanes instead of constructing/scanning global tile bins.
#define STARFOX_ROW_TILE_RASTER 1
#include "raster_portable.hlsl"
