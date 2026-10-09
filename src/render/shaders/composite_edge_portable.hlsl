// Keep the margin histogram out of the per-pixel compositor's bytecode.
// Both specializations share the reference shader's arithmetic and ABI.
#define STARFOX_COMPOSITE_EDGE_ONLY
#include "composite_portable.hlsl"
