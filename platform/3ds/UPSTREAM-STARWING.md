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
offset, not BG2VOFS. Sloped/gapped tables, lower tilemap wraps, priority holes and
transparent lower characters that need reference ground continuation fall back intact.
Source mosaic, rolled/finite terrain, tunnel geometry, unique moons/planets and
mixed-priority painter groups retain the existing exact raster path. Capacity
or atlas-budget rejection also falls back before publishing partial geometry.
Further adaptation must preserve our finite-ground/sky stereo semantics rather
than copy the upstream flat BG2 depth policy.

Host equivalence and ARM compilation do not establish physical-console speed.
