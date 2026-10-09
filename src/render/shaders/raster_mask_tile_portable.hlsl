// Two ordered 32-bit face masks replace each tile's variable-length list.
// The host guarantees ordinary non-wave row spans and at most 64 faces.
#define STARFOX_MASK_TILE_RASTER 1
#include "raster_portable.hlsl"
