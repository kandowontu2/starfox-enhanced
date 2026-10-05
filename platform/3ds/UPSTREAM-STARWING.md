# Starwing 3DS integration

The BG2 tile-rectangle planning algorithm is adapted from
[Esteban PDN's Starwing 3DS](https://github.com/EstebanPdN/starwing-3ds), pinned at
`753952be5b170059d95068350d37759a966a3bd5` (2026-10-02).
The project maintainer supplied the author's permission to reuse the port code
in this chat on 2026-10-04. This records that permission and attribution; it
does not invent a blanket license or extend permission to cartridge assets.

Original sources: `platform/3ds/source/bg2_plan.cpp`, `bg2_plan.hpp` and the
tile-atlas/HDMA composition in `display_3ds.cpp`. Esteban PDN is credited for
the 3DS adaptation. Existing third-party notices retain their own scope.
No ROM, generated game artwork, firmware or private UI resources are imported.

Our integration uses our existing Citro3D owner rather than importing a second
Citro2D presenter. Tile colour zero is transparent; opaque black stays opaque.
Palette fades, source BG2 ownership and RGB8 brightness/subtraction are retained.
It consumes one immutable source mesh for both eyes and does not change game
timing, source audio, the pre-game menu or native model stereo projection.

The first enabled path covers isolated, ordinary Mode-2 BG2 at screen depth or
our existing infinity projection. Extra LCD rows clamp the same source scanline;
both eye frusta retain their full source-derived coverage.
Complete constant vertical-offset tables are supported with their actual source
offset, not BG2VOFS. The second enabled path covers isolated Mode-2 outdoor
landscapes, including all-sample fitted roll, gapped tables/register fallback,
scanline offsets and the original lower wrap/transparent/priority-hole colour
continuation. Uniform source characters and carried colours are merged into
rectangles; no invented ground colour or completed model image enters the atlas.
Its finite quads retain our source camera plane, near/far clipping, homogeneous
UV/Q pixel registration and both eye frusta. The entire infinity artwork is
drawn before every finite receiver, with the same source BG2 stencil/depth.

Isolated Mode-1 BG2 panorama groups now use their own source tile atlas,
including per-scanline horizontal/vertical offsets and the original clamped
wide-margin bridge cross-section. This does not turn a panorama into a water
plane or change its existing infinity projection. Both source priorities remain
in their original position among the screen-space OBJ/BG3 groups.
Mode-1 water receivers can also use this atlas. Their source camera height,
both signed finite planes, subpixel infinity band and near/far clipping retain
the same semantics as the raster receiver. Homogeneous source UV/Q registration
and source-layer/depth ownership remain exact; a complete receiver must fit
before the raster owner is retired. Palette-only changes recolour without
replanning, while character changes rebuild any uniform-character merging.

Source mosaic, corridors/tunnels, unique moons/planets/sky halves and
mixed-priority painter groups retain the existing exact raster path. The flat
screen/infinity path still declines nonconstant roll and lower ground carry;
the complete Mode-2 terrain planner is explicitly selected for outdoor receivers.
Capacity or atlas-budget rejection also falls back to the complete reference
scene before publishing partial geometry, including the finite vertices in the
caller's remaining whole-scene budget. We retain our finite-ground/sky stereo
semantics rather than copying the upstream flat BG2 depth policy.

In particular, Original Corneria's early BG3/Mode-1 launch sequence is an
authored tunnel (`tunnel_scene`), not a water receiver or panorama. That sequence
still uses the corridor raster. An enabled synthetic Mode-1 atlas does not prove
that this real tunnel has been accelerated.

Host equivalence and ARM compilation do not establish physical-console speed.
