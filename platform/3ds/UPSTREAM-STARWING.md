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

Our reference planner also merges opaque uniform scanline fragments before
its capacity check. This avoids rejecting a compact flat EX background solely
because its unmerged HDMA representation exceeds the draw budget. Transparent
characters remain textured; mixed tiles, palette banks, source flips and rolled
lower-edge corrections retain exact indexed-pixel/coverage parity. The fragment
limit itself is unchanged, and genuine over-budget scenes still use the complete
fallback. This is an adaptation of the credited planner, not replacement artwork.

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
continue across the LCD idle interval and ordinary gameplay/training raster
updates that do not consume SPC acknowledgements. The source tick and the
next SPC packet remain barriers, as do frontend/native handoffs, duplicate or
rewound timestamps, focus loss and runtime-options entry. It cannot overlap a
later source logic tick, state save or session replacement. Packets now start
at their producing source phase, including inside a catch-up batch; the same
APU-independence predicate and source/new-packet barriers guard later phases.
Executors that refuse asynchronous dispatch retain joined intermediate packets.
Denied core-2 allocation retains a serial fallback, and the default host session path
is unchanged. Original and EX parity checks compare every PCM sample and the
complete VM/SPC timeline against that synchronous path. This is not a measured
console performance claim or permission to drop sound commands.

The cross-raster follow-up uses the later reference's
`game_3ds.cpp` at `f357f9369cc1ab674fb3a87066434fd61c34bbf1`, which waits
before a source tick or new audio packet rather than every intervening display
phase. Our conservative simulation predicate enables this only in ordinary
gameplay/training with no frontend, preview, runtime-options or deferred-MSU
handoff. The `--audio-video-pipeline` oracle deliberately holds packets until
owner completion to prove raster calls do not wait, then compares every PCM
sample and full VM/SPC state with the synchronous owner at explicit barriers.
The parallel variant exercises the actual executor and its distinct effects
worker. Both Original and EX and both timing modes remain test targets; no
source/PCM cadence is traded for output rate.

The packet-boundary dispatch follow-up also uses `game_3ds.cpp` at
`01bf4925951515244cdb03d58ad722417919d6ba`: its audio worker starts inside
the source-phase loop, rather than after snapshot/render preparation. Our
implementation keeps both accurate stems, captures volumes at that packet's
boundary and submits PCM only on the owner thread after joining. The held-packet
and real-parallel oracles now exercise multi-phase catch-up batches in Original
and EX, under both timing modes, comparing complete PCM and saved VM/SPC state
at the explicit completion barriers. This does not establish physical 60 FPS.

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

## Object-bridge follow-up

The same reference at `01bf4925951515244cdb03d58ad722417919d6ba`
(`src/simulation/map_vm.cpp` and `object_pool.cpp`) caches host/native object
records and reuses exact active/free lists. Our first adaptation keeps every
routine bridge, compares complete host objects and live WRAM payloads, and
retains native-pointer fixups. It does not assume that a dirty generation
identifies every host/native write. Reserved scratch lists avoid repeated allocations; unchanged
lists do not rebuild pool topology. Record scratch is bounded and heap-owned,
not placed on `load_state`'s native stack or included in saved states.

Two fidelity guards are additional to the reference idea: the first import
after export must normalize semantic/extended mirrors, and imported pointer
words must match a canonical export before that export can be reused. Active
membership/order changes invalidate snapshots, as do slot reuse and VM restore.
Only plain WRAM records may be reused. I/O records retain their original access
order, and every reused record retains the old final open-bus byte.

An independent bytewise bridge checks complete Original/EX pool and CPU states
through 256 mutation sequences per layout, full/empty pools, list reordering,
native writes, pointer normalization, inconsistent mirrors, slot reuse, restores
and malformed lists. Separate I/O fixtures prove that those records never enter
the cache. Eight real stage probes retain their prior 240-phase pixel digests,
80 complete audio blocks and source-purity checks. These are correctness checks,
not a physical-console FPS claim; native timing and sustained mono acceptance
remain required.

## Consecutive strategy-call follow-up

The same reference's `strategy_scheduler.cpp` retains the just-imported native
image for consecutive strategy calls inside one tick. Our adaptation limits
that path to the scheduler's private bridge. Standalone calls retain
their full defensive export, and every completed routine still imports its
complete native result before the scheduler selects the next object.

Reuse requires exact active/free order, all complete host objects, canonical
native object references and PREV links. Native record/list writes, including
low-bank WRAM aliases, revoke the imported image. I/O writes revoke it because
they can launch DMA/HLE work. Failed native calls and VM restore revoke it too.
The omitted export's last open-bus byte is retained. Recovery host removals and
slot reuse are detected by the existing exact list/object checks. No simulation
phase, source instruction, audio acknowledgement or list generation is dropped.

The independent bytewise oracle now also exercises this complete-image reuse
through 256 mutation sequences per layout and 128 actual interpreter calls per
layout. Real-cartridge diagnostics compare complete CPU/VM and pool states,
instruction counts and run/removal counts against full synchronization at
eight sampled strategy ticks in each of eight Original/EX stages. All 64 paired
ticks match while exercising 964 complete-image reuses. Their 240-phase image
digests and 80 complete audio blocks remain unchanged. This establishes sampled
behavioral equivalence, not sustained native 60 FPS or full hardware acceptance.

## Consecutive transfer-call follow-up

The same pinned reference's `game_simulation.cpp` avoids redundant object
bridges for palette fades and window wipes. Our transfer chain instead borrows
the existing canonical import after DOSOUNDS, with the same complete host,
list, native-pointer/link and mapped-write guards as the strategy chain.
Palette routines, EX colour cycles, sprite building and window wipes still
execute in their original order and cadence. Every completed call imports its
full result; public/general callers outside the chain remain defensive. The
DOSOUNDS, dialogue, collision and audio acknowledgement paths are unchanged.

The bytewise oracle also compares 128 actual general interpreter calls per
layout, including returned registers, PPU/APU writes, host/native edits, slot
reuse, restores, transfer servicing on/off and failed-call recovery. Eight
real Original/EX stage probes compare 628 named transfer calls against full
synchronization after each call, with 564 imported-image reuses. Complete
CPU/VM/pool states, registers and instruction counts match. All eight prior
240-phase image digests and 80 complete audio blocks remain unchanged.
These correctness results do not establish sustained native mono 60 FPS;
the exact ARM image still needs timing and full hardware acceptance.
