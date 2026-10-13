# Starwing 3DS integration

## Reference mono pipeline (October 13 follow-up)

To establish a meaningful mono baseline, `reference/` adapts Esteban PDN's
actual `Frame3ds`, native canvas and Citro2D display implementation at
`01bf4925951515244cdb03d58ad722417919d6ba`. This supersedes the earlier
tile-planner-only integration **for ordinary mono gameplay**. Slider stereo,
source window/color effects and host menu previews retain our native PICA
path. The real pre-game menu, lower-screen HUD, preferences, save transactions,
VM cadence and accurate SPC audio are not replaced by the reference frontend.

The reference's reusable model scratch, immutable BSP address tables, PPU
memory-change revisions and one-pass priority background decoder are also
adapted, with exact-raster/state checks. Presentation RAM reads use non-mutating
peeks. Cached CPU data is invalidated across state restores. GPU completion
precedes freeing linear textures/C2D buffers; the two GPU owners are exclusive.

Citro2D v1.7.0 comes from the official devkitPro source archive with a pinned
SHA-256. Its altered draw hook explicitly flushes used vertex/index ranges and
split command lists; kernel fallback flushes both buffers or refuses submission.
The accuracy-enabled emulator caught a stale shader borrow when an authored
effect switched back to our native presenter. The patched C2D owner now parks
one CPU shader until the next valid program bind; it retains no render targets,
textures or vertex buffers. Queue completion precedes either owner's teardown.
Its original notice is retained in `reference/CITRO2D-LICENSE.txt`. This imports
code only, under the author permission recorded below, not ROMs or game artwork.
Compilation and emulator checks do not establish sustained hardware 60 FPS.

The mono lower LCD uses a bounded two-buffer residency cache, validated against
actual RGB bytes and committed only after a successful SDK cache flush. Changed
HUD pixels invalidate both physical buffers. A 16-pixel tiled transpose replaces
the strided full-screen conversion; LCD cache ownership ends with the presenter.
The native profile records mono background, preparation, raster, finishing and
LCD-copy costs separately, without SD writes during gameplay. The reference
display target also receives the same Release `-O3` policy as the source core.

Diagnostic renderer handoffs no longer rearm startup SD writes: only the first
native GPU initialization per cartridge owner can open that receipt window.
Ordinary Start still opens its own bounded trace, and replacing the cartridge
resets the first-GPU guard. Repeated damage/wipe transitions must remain free of
startup I/O once that bounded window expires. Failed SD streams stay disabled.

## Earlier tile-planner integration

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

That earlier integration uses our existing Citro3D owner rather than importing a second
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

## Audio compatibility investigation

The same pinned reference's `audio_3ds.cpp` documents a retail NDSP startup
abort and implements a CSND output alternative. That motivates our separate
asset-free CSND check (`CSND-CHECK.md`), not a claim that our own NDSP owner
fails identically. Esteban PDN is credited for identifying this compatibility
path. We do not copy its estimated looping-ring cursor, midpoint frame insertion,
dropped-packet policy or SPC worker scheduling. The gameplay sink and both
accurate SPC stems remain unchanged until a streaming adapter is accepted.

## Frame-pipeline follow-up

The later reference at `01bf4925951515244cdb03d58ad722417919d6ba`
(`game_3ds.cpp`) overlaps a complete audio update with rendering.
That scheduling idea motivates our optional `GameSession::run_frame_work`
path; no additional reference implementation or assets are copied. Our
persistent executor runs the complete, unchanged SPC block in its background
task and presentation on the owner thread, then joins both before submitting
PCM or acknowledging the source ports. The original joined mode remains the
default host API. Native presentation now permits that same complete block to
continue across the LCD idle interval; the next `advance()` joins it before
any video phase, including duplicate/focus-loss calls. It cannot overlap a
later source phase, state save or session replacement. Earlier audio blocks
in a source catch-up batch retain their synchronous acknowledgements. Denied
core-2 allocation retains a serial fallback, and the default host session path
is unchanged. Original and EX parity checks compare every PCM sample and the
complete VM/SPC timeline against that synchronous path. This is not a measured
console performance claim or permission to drop sound commands.

The reference's APT-budgeted core-1 stem also motivates an optional, distinct
effects worker within our joined frame-audio task. Core 2 renders music, that
worker renders effects and the owner renders the scene. Budget/allocation
denial retains serial stems; the previous APT budget is restored after join.
This keeps our exact source acknowledgements, complete PCM blocks, restore
ownership and failure retirement. No reference ring-buffer packet dropping or
midpoint sample insertion is adopted, and no console speed gain is claimed
without a native timing run.

The reference's source-phase presentation and phase-debt VBlank wait are now
also used by our frontend gate. Completed 60 Hz video phases drive 60/30 Hz
output directly, rather than a second wall clock sampled after varying source
work. Catch-up still submits only its latest complete scene, never replayed
frames. A wait includes the time spent since the source-clock sample so a
newly due phase is not slept through. The cartridge/audio clock is unchanged.
Host tests cover 100 seconds of alternating 2/12-ms draw jitter, zero-phase
retries, 30 Hz cadence, catch-up, reset, clock rewind and long uptime. Native
speed acceptance remains outstanding.
