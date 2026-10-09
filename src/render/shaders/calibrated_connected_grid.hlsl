// Same compact source and two-pass projection/binning as native PCVR/Quest.
// SDL resource bindings only; do not duplicate or approximate source math.
#define STARFOX_SDL_CONNECTED_GRID
StructuredBuffer<uint> texels : register(t0,space0);
#include "../../vr/shaders/connected_grid.hlsli"
