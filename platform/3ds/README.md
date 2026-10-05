# Nintendo 3DS frontend

The primary stereo target is **New Nintendo 3DS and New 3DS XL**. Original
3DS/XL and both 2DS models use a mono top screen: the slider is ignored on
original models, and no second-eye rendering is requested. New 3DS/XL and
New 2DS XL request libctru's CPU/cache speedup; old or unknown models do not.
The request is reapplied after Home/sleep and disabled on exit. Cartridge
timing, audio rates and the real pre-game menu remain unchanged.
This directory contains a host-verified cartridge game session and native
LCD/input/audio and PICA200 GPU diagnostics. The new cartridge-core bring-up
target connects the actual simulation, SPC and dashboard to a console entry
point. All three diagnostics, including the actual menu integration below,
compile/link with the actual ARM SDK at the accepted October 4 menu checkpoint
on `codex/3ds-native-bringup`. Physical-device acceptance is still pending.
**There is no hardware-verified 3DS release yet.** An opt-in
`STARFOX_3DS_BUILD_TEST_PLAYER` target builds the real menu/game owner without
the diagnostic banner covering its upper LCD. CI packages it separately as
`StarFoxEnhanced-3ds-test.zip`, explicitly experimental, with native
ELF/3DSX/SMDH validation, source commit and checksums. No ROM/BIN is embedded
or included. [Installation and hardware checks](TESTING.md) describe the SD
layout and controls. This is a test candidate, not full-port acceptance.

This target policy supersedes the earlier Original-3DS stereo experiments
documented below. Historical packages do not gain this change retroactively.
CPU speedup alone does not establish stable 60 FPS; measure New-model stereo
and old-model mono separately before claiming playable performance.

## Opt-in native frame profiling

`STARFOX_3DS_PROFILE_FRAMES=ON` enables separate raw ARM11-clock timing for
source video/logic, scene capture, both SPC drivers plus mixing/output, raster
publication, models, BG layers, dots, composition and GPU presentation. The
native build helper accepts the same environment variable; it defaults OFF.
The bring-up CI candidate enables it for investigation, not an FPS claim.
Profiling does not change source ticks, SPC rates/voices, scene content or
slider projection. The disabled scopes do not read the clock.

The opt-in player writes `native-frame-profile-<startup tick>.csv` in the
existing companion SD folder. It never replaces saves, settings or assets.
Output is limited to 512 one-second windows; unavailable/full storage stops
logging without terminating play. Timing parents include their children:
`frame` includes `advance`, and `advance` includes logic/audio/raster work.
Do not sum those overlapping columns. Source counters are cumulative for the
current owner; flow/BG identify the state sampled at window completion, so a
transition window may contain more than one scene. CSV serialization occurs
after the measured frame, and diagnostic SD traffic can perturb later timings.
Emulator measurements still do not prove physical Original 3DS performance.

## Native IPO performance experiment

`STARFOX_3DS_ENABLE_IPO=ON` asks CMake to verify link-time optimization support
with the active compiler/ABI, then enables it across the actual cartridge CPU,
SPC, source PPU/geometry, frontend and player targets. Unsupported explicit
requests fail configuration; the option defaults OFF. The native helper accepts
the same ON/OFF environment variable. This does not select an approximate DSP,
fast-math, lower audio rate, fewer source ticks, mono output or reduced scenery.

Bring-up CI retains the native compiler commands and cache alongside its
existing tests, linked-stack and package checks. Compare
the same ordinary native route and phase CSV against the non-IPO baseline
before claiming a speed gain. Compilation and emulator timing are not physical
Original 3DS/XL performance acceptance or a verified release.

The first full native IPO candidate passed the unchanged host, ARM stack and
package gates, but its ordinary-route emulator comparison did not establish
a useful overall speed gain. Gameplay still presented at approximately 1 FPS.
Keep the option default OFF; this is not a completed performance fix or a
tester-ready port. Continue with measured source/audio/PPU work rather than
attributing successful linking or a smaller executable to higher frame rates.

Native PPU raster caches now compare the wrapped character/map ranges used by
each painter pass, plus its relevant OAM, scroll, mosaic, palette and Mode-2
offset inputs. Unrelated VRAM writes do not force a background decode/recolour.
Mode-3 BG1 still depends on all 64 KiB; both OBJ banks retain large-sprite tile
carry. Fresh-decode comparisons cover modes 1–3, every map size, 8/16-pixel tiles,
wrapped ranges, sprites, fades, re-enabling layers and tunnel offsets. These
correctness checks do not establish a native frame-rate gain. The candidate
passed the full host/ARM/stack/package gates, then completed the ordinary route
with IPO OFF against the earlier non-IPO baseline. Its dominant gameplay
frame/layer costs remained effectively unchanged and the emulator still showed
approximately 1 FPS. This is not a completed speed fix or a tester-ready port;
continue with changing-layer preparation, accurate SPC and VM costs rather than
removing content or changing source timing to conceal the remaining work.

## Native EX span follow-up

The source renderer's original solid/EX span rules now feed a native camera-
geometry consumer for untextured wireframe, cel, wobble and wave polygons.
It retains sparse ink, authored fan depth and the wave's unwarped source Y;
it does not replace an effect with a filled fan or a projected mono bitmap.
Both eyes use one stream prepared against their union, then hardware projection
and clipping. Source palette pairs, material order, effect windows and atomic
failure/resource limits remain connected to the existing presenter. Particles
and projected text keep their own source routines, rather than inheriting
MDRAWP's global polygon effects.

Local checks compare native ink and reciprocal-depth interpolation across 576
synthetic fixtures/all 48 EX mode combinations, non-planar fan surfaces,
near/far clipping, an eye-only visible face, palette/window ordering and
transaction rollback. All twenty freshly rebuilt host suites pass. Private
local cartridge catalogue checks include 2,697 Original and 3,511 EX models,
335,232 poses including the EX combinations, with no conversion failure;
per-model peaks after coalescing are 9,348 vertices / 125 draws / 39 textures. These are
individual-model resource checks, not a complete-scene memory or performance
claim. The shared generator also retains all three pre-extraction raster
fingerprints across 4,608 frames. R11 predates it and must not be relabeled as
containing it. Corrected R12 ARM/emulator bring-up is described below; full
Original/EX scene/effect and physical-device acceptance remains pending.
The asset-free native GPU diagnostic now exposes these eight span modes via
L/R on a sloping face, alongside unchanged textured cubes. Its small separate
CI artifact allows native effect checks without downloading debug ELFs.

Whole-scene follow-up found two budget failures missed by individual-model
checks: repeated wobble-1 chords and broad near-Z guards emitted off-eye ink.
Equal-plane/displacement runs now union in a bounded seven-row neighbourhood;
exact affine footprint bounds retain only ink that can touch either eye.
Visible source ink/depth is unchanged in regression checks. Seven sampled EX
Corneria snapshots, each with all 48 synthetic presentation modes, pass actual
host layer/model/dot/tint/wipe composition: peak 17,784 vertices / 91 draws /
5 textures / 1,708,288 padded GPU bytes, including the lower LCD. The cartridge
and audio state remain unchanged by these presentation-only stress fixtures.
This does not establish every scene, physical FPS or total-process peak RAM.
The first EX ARM checkpoint `2cb9e0d21c2c211be29071a5162c453d2346a2ab`
predates this resource correction, despite passing build/package gates.

Corrected checkpoint `542464e547faee2e1c946d5136f48d48d64396ea` passes
native CI 37234599486, independent player-package checks, the asset-free native
eight-mode GPU probe and a fresh normal-menu Original map/briefing/campaign-entry
replay in isolated Original-3DS-profile Azahar. Map and briefing outer margins
are uniform in the source colours; the retained entry sample has distinct
upper-eye geometry and exactly matching lower HUDs. The first timed capture
attempt selected emulator popups and was rejected; the fresh replay uses only
the unique owned game window. These are emulator bring-up checks, not full
campaign coverage, physical slider comfort, device FPS or total peak RAM.
The local experimental handoff is `build/StarFoxEnhanced-original-3ds-test-r12.zip`.

## Bounded texture replacements

The source follow-up after R6 releases every obsolete padded colour/A8
allocation after the previous GPU work completes, before creating any new
texture. Two individually valid frames could previously exceed the 4 MiB
texture budget while replacing slots in sequence. Same-size storage remains
resident for palette/scroll changes; removed ownership and inactive-slot CPU
caches are freed rather than retaining their old vector capacity.

Host allocation-ledger tests reproduce a 3.75 MiB -> 3.75 MiB transition with
a former 4.5 MiB intermediate spike. The shared presenter sequence stays within
4 MiB, including the dashboard. Malformed preflight, mask removal, retry after
an injected allocation failure and repeated layout changes pass. All eighteen
rebuilt host suites and strict native-source compilation against the saved
official SDK headers pass. This is not a new ARM build, physical allocation,
total-process peak RAM or performance result; R6 predates the change. The R7
ARM package does contain the residency change, but a later isolated emulator
run found its 32 KiB default main stack overflowing during SPC bank loading.
Do not treat R7's build/package checks as successful game boot.

## Native main-stack follow-up

Source builds after R7 reserve a 256 KiB main stack inside the existing process
heap allocation. The shared SPC snapshot alone exceeds libctru's default
32 KiB stack; the R7 ARM failure starts at `Spc700Audio::Impl::load_driver`,
with writes below the mapped heap/stack base. The native override applies to
all four diagnostics/player executables, without changing SPC data/timing,
Original 3DS memory mode or clocks.

CI validates the initialized strong `__stacksize__` symbol in each linked ARM
ELF and reports compiler stack-usage records. This catches a missing/weak
override and an individual frame exhausting the reservation. It does not prove
whole-call-chain stack depth, total peak RAM or physical hardware performance.
The R8 actual ARM build passes the stack gate (3,296 compiler frames; largest
individual frame 79,280 bytes). Its isolated emulator boot gets past the stack
failure, then exposed a second native error: `C3D_TexBind(1, nullptr)` dereferences
the texture type on Citro3D's secondary units. R8 is not a working tester candidate.

The next source follow-up binds the existing resident 2D dashboard while the
secondary TEV stage is inactive, then selects the actual ownership texture when
needed. No dummy allocation or sampled-colour substitution is added; stale mask
pointers are replaced even after texture layout changes. The R9 ARM build passes
CI and its isolated native run reaches the menu and title/intro without either
earlier memory fault. An actual GPU-window capture then exposed vertically
flipped uploaded artwork on both LCDs; R9 is not a finished tester handoff.

## Uploaded artwork orientation

The next source follow-up stores colour and A8 ownership rows in the same
top-left order. The shader already converts logical V to `1-V`, and PICA's
sampler addresses texture rows bottom-up; reversing upload rows too caused the
observed double flip. Projection, physical eye separation and source coordinates
are unchanged. Regression tests independently follow logical pixel centres
through padded shader UVs, sampler inversion and Morton storage, checking RGBA
and per-pixel ownership together, including 320/400 x 240 LCD artwork.
The R10 actual ARM build and downloaded package pass all gates. Its isolated
Original-3DS-configured emulator shows upright pre-game, title and Controls
artwork, accepting normal recorded Start input with neither earlier memory
fault. A second normal-input run proceeds through the Controls Training choice
into actual Training, with native models/scenery above and live radio, shield,
ally meters and counters below. This is emulator bring-up evidence, not physical
LCD/slider, speed or peak-memory acceptance. A further normal-input run reaches
the Original planet map, travel briefing and initial campaign geometry. At
maximum emulated slider, the world LCD has distinct eyes while the lower HUD
remains identical. This is not physical stereo comfort, sustained performance
or full Original/EX stage/effect coverage.

## Single-occurrence planet-map artwork

The R10 native map/travel capture exposed complete planets and labels repeating
in the outer LCD columns. Mode-3 travel still uses an authored map, not a world
surround. The source follow-up keeps its BG/OBJ artwork within the same canonical
256-pixel panel as planet select; the remaining LCD field uses the source
backdrop. Mode-1/2 world travel surrounds retain their wide/depth behavior.

The independent edge-marker fixture fails on the former travel policy and
passes after the restriction, preserving both canonical edges, both tile
priorities and the other travel modes. R11's source
`033594c200f8625711b310270684bc4ef7c1c8e0` passes the fresh ARM, linked-stack,
eighteen host-suite and package gates. The independently checked package runs
the same normal-input route in the isolated emulator: travel map and briefing
artwork no longer repeat in the native margins. At maximum emulated slider,
initial campaign geometry has distinct eyes and the lower HUD is identical.
R10 does not contain this correction. R11 is still experimental, not physical
LCD/slider, performance, peak-RAM or full Original/EX acceptance.

## Signed tunnel entrance/exit faces

The native receiver now retains authored physical faces when the source camera
is outside or exactly on a tunnel wall, floor or ceiling. Signed plane/frustum
clipping avoids zero-distance division and interpolates continuous wall crossings
on the same source clock as the models. The colony's left side remains open.
The ordinary inside-camera path is unchanged; no camera, optical setting or
cartridge PPU state is clamped or rewritten.

Exterior frames keep their isolated source BG2 far field behind the physical
faces. They do not use a flattened final image containing models or sprites.
The source does not supply a separately identified outside-tube BG2 plate;
disoccluded exterior-artwork treatment and physical LCD acceptance remain open.
Signed receiver coverage is not a claim that every entrance/exit scene is done.

## Final-room raster policy and near-wall stereo

The authored final/vortex backgrounds are distant scenery in both Mode 1 and
Mode 2, even when the cartridge retains INATUNNEL. The native raster now uses
the same verified background-identity policy as the scene snapshot, while
retaining the current video phase's VRAM, OAM, HDMA and palette. Only the
presentation tunnel metadata is cleared; the source VM/SPC remain unchanged.

Controller-driven Original/EX final-tunnel replays each check 61 final-room
frames at default/maximum optics and slider 1, 0.5 and 0: 10,495,112 canonical
pixel/ownership/resource checks each. Padded GPU textures including the lower
LCD peak at 2,097,408 / 2,101,504 bytes. Public corridor regressions now include
near-wall cameras and individual eyes outside small/medium tubes, selecting
the nearest bounded wall intersection and checking source texture/depth
registration. These are host checks, not device pixels, full-route performance
or proof that every exterior camera/EX corridor is covered.

## EX orbital/unique-menu stereo

Verified EX menu atlas metadata now enables the physical 3D slider for orbital,
unique-space and star-surround backgrounds. Only the separate BG2 scenery
groups use infinity disparity and guarded eye coverage; the retained cartridge
BG1 menu text, OBJ and other frontend layers stay at screen depth and keep their
native width and painter order. Ordinary menus, planet maps and Controls are
unchanged. This does not move menu UI onto world geometry or remove setup.

Public regressions cover ordinary/thin/entry orbital metadata, unique space and
star surrounds in both source PPU modes, default/maximum optics and fractional
slider positions. A private real-EX source probe navigates from title through
the shoulder/page/background controls without writing cartridge state. Choices
21, 25, 35, 19, 27, 28, 31 and 2 retain exact canonical foreground/background
pixels and VM/SPC state; landscape choice 0 retains its flat frontend policy.
These are source/resource checks, not native LCD pixels or device performance.
Dedicated orbital hemisphere/body geometry and exterior policies remain open.

## Distant-sky residency at maximum 3D strength

Infinity-only background groups retain the union of both eye-visible source
intervals and the canonical mono LCD. At extreme separation the eyes see
disjoint columns; the unseen gap is no longer uploaded as another large
texture. Colour, alpha, source priority and requested strength/separation are
unchanged. Finite terrain, water and corridors never use this infinity-only
cropping. Ordinary base-guard presentations retain their existing fast path;
slider changes reuse decoded source pixels.

The actual scene checker now supports `--all-optics` after MAP/SOURCE_FRAMES.
It checks every requested source frame with retained owners at strength 2,
separation 64 and convergence 16. Earlier first-policy samples used strength 1
and were not full-intro maximum-strength acceptance. The stronger checker caught
an intro texture-budget failure at phase 24 before the disjoint-frustum fix.
Independent regressions compare all canonical and both-eye LCD pixels against
the uncropped source, including fractional offsets, priorities and opaque black.
This is source/resource verification, not Original 3DS/XL hardware performance.

## Native finite corridor checkpoint

Verified source tunnel IDs now carry dimensions from their own `STUNNEL_`,
`MTUNNEL_` or `LTUNNEL_` symbols: small, medium and large walls are not one
arbitrary screen-depth plane. The native adapter intersects camera rays with
the four authored surfaces, partitions at their nearest intersections and
clips near/far before projection. Camera position and rotation interpolate
with the model source clock; scene/background/bounds discontinuities do not.
Homogeneous source UVs preserve canonical artwork, both BG2 priorities, black
ink and sprite painter order. Only the far-horizon region stays at infinity.

The source decoder already clamps a tunnel's outer cross-section. Analytic
edge strips reuse those pixels at large stereo separation without allocating
thousands of identical columns, copying textures per eye or changing optics.
Unchanged slider presentations reuse decoded artwork and palette conversion.
Independent slab-intersection tests cover small/medium/large bounds, camera
translation, rotated view matrices, near/far, source UVs and both LCD edges.
Maximum strength tests cover eyes inside the physical medium/large tubes;
eyes outside a small tube and exterior exit-camera surrounds remain separate
unfinished policies, not silently clamped projection settings.

All seventeen rebuilt root host suites pass. Source painter/raster checks
total 43,013,740 / 10,848,904. Private unmodified-ROM Armada runs reach 240
corridor frames from source video phases 4,377 (Original) and 5,017 (EX).
Each compares 458,752 canonical colour/opacity/ownership samples at default
and maximum optics while preparing models, dots, colour effects and wipes.
Peak padded texture residency including the lower LCD is 3,150,080 /
3,154,176 bytes. These runs use god/infinite lives/bombs, firing, bomb assistance
and feedback steering toward the centre; they are host correctness tests,
not native device, no-cheat gameplay or frame-time measurements. Initial
straight-edge runs did not reach a corridor and are not acceptance evidence.
The exact native-only export passes all seventeen host suites; corridor source
checkpoint `4a2cb0818f2b36aa221b88e69ee11b697203e4b3` passes ARM CI
37205227789, all three native links and downloaded ELF/3DSX header checks.
EX-specific wider corridors, exterior transitions, full-flow HUD/effects,
physical Original 3DS/XL validation and playable packaging remain unfinished.

## Native water receiver checkpoint

Mode-1 `BG_2_3B` water now uses finite floor and overhead geometry at the
interpolated cartridge camera-to-shadow height, with source-pixel homogeneous
UVs. Its BG3 sky remains at infinity; adjacent OBJ and bitmap priorities remain
screen-space, in their original painter order. This applies to world flows,
including intro/results/credits, but not setup, title, map or Controls screens.
Only the far-horizon band remains at infinity; there is no full flat water
image underneath the finite receiver. Borrowed compact strips and occupied
sprite rectangles avoid padded texture waste without rescaling artwork.

The original depth regression failed before the receiver was implemented.
Both-eye coverage/UV tests cover floor/overhead, low camera heights, slider
motion and strength-2 projection stress. Full native canonical pixels retain
opaque black, all four visible OBJ priorities and both BG3-priority policies.
All seventeen rebuilt root host suites and five strict changed-source checks
pass. A private unmodified-ROM Titania run reaches water at source video phase
10,722 and checks 240 water frames with models, dots and effects at default and
maximum menu optics. Peak padded GPU texture residency is 3,245,312 bytes,
including the lower LCD. That probe uses god mode, infinite lives/bombs and
bomb assistance; it is not a no-cheat gameplay, native-device or performance
claim. Root and exact native-source host runs both pass seventeen suites and
the same natural water probe, including 458,752 canonical pixel/ownership
comparisons each. Water checkpoint `070e93f98afde8b6483ca6e4ea01cf5b6156c24a`
passed ARM CI 37203496561; downloaded ELF/extended-3DSX headers validate.
Full-flow effects/HUD acceptance and playable packaging remain.

## Distant background depth and source priorities

Scenery coverage follows both eye projections instead of assuming a fixed
32-pixel margin. Wide decoded artwork is shared through horizontal GPU texture
strips of at most 1024 texels; no finished-world image is shifted or copied.
Outdoor terrain and connected-grid receivers solve finite-depth eye coverage
as well as the infinity offset. Every infinity strip is submitted before the
finite receivers, so a later sky strip cannot overwrite nearer ground.
Unchanged grid/camera artwork reuses sufficient coverage when the slider moves;
new source camera keys retire excess allocations. Ordinary source-HUD and
screen-space artwork retains its authored margins and original pixel coverage.
Priority-isolated panoramas trim only unoccupied source rows/columns from the
borrowed texture descriptors. Opaque black, source positions, painter order,
palette fades and all visible artwork remain unchanged; no quality downscale.
The existing aggregate 4 MiB padded-texture budget still includes the lower
LCD. Extreme rolled receivers and full-flow peak residency require further
acceptance; coverage/storage failures are explicit, not hidden optics clamps.
The actual menu uses strength 1 and separation up to 64. The separate strength-2
projector stress range passes isolated sky/terrain/grid fixtures, but its
complete intro composition can still exceed the aggregate texture budget;
it is not claimed as a supported full-flow configuration.

Verified unique-left/right landscape metadata now reaches the native BG2
decoder. Beyond the single authored atlas occurrence, only that sky half is
sampled from the opposite, repeatable half. The canonical 256-pixel source
window and ground rows are untouched. This reuses the shared indexed decoder;
there is no sky-image copy, colour averaging or finished-world reprojection.
Palette/subtraction/brightness changes recolour the cached indices. The public
pixel regression failed with duplicated artwork before this wiring and passes
both half orientations, signed scroll, opaque black, ground and slider caches.
Actual stage and native ARM acceptance of this new wiring are separate gates.

Open-world/intro Mode 1/2 BG2 artwork now uses infinite scenery disparity,
separate from screen-space BG3 and OBJ. The adapter splits only contiguous
coordinate-space groups, keeping the exact low/high background and sprite
order around native models. It does not flatten or reproject a completed
game image. Mode 3 map buffers, menus, boss roll, tunnels and water cross-
sections retain their authored policy; verified outdoor ground continues to
use its finite Q15 receiver rather than becoming an infinite sky.

Both eyes share source texture bytes and cached decoded artwork. Isolated
BG2 needs no redundant resident provenance mask; mixed sprite/text groups
retain A8 ownership for colour math. Artwork-only assembly reserves the lower
LCD in the same 4 MiB texture budget as the final compositor. Public fixtures
compare native mono pixels, painter priorities, black coverage, fades, eye
matrices and unchanged source state. Actual cartridge and ARM validation are
separate gates. Unique EX skyline/orbital spans, extreme optical settings,
full-flow resource limits and physical original-console acceptance remain.

The actual-cartridge host checker also accepts targeted stage symbols:

```text
starfox_3ds_game_models_check ROM SYMBOLS [MAP [SOURCE_FRAMES]]
```

The default BOOT/Corneria checks are unchanged. Optional source-frame counts
must be whole numbers from 1 through 3600; omitted counts retain BOOT's 240
or a stage's 1440 frames. For example, `LEVEL1_3 1440` samples Space Armada
and `LEVEL2_2 1440` samples Sector X without requiring them to be outdoor
landscapes. Source water/tunnel/unique/orbital observations are reported
separately; their presence is not proof of visual/depth acceptance. The checks
still assert shared eye source bytes, painter order, resource budgets and
unchanged VM/SPC state. This host tool is not a native gameplay package.
Mandatory-positive dust/grid coverage still belongs to the default BOOT and
Corneria fixtures. Other selectable stages can disable dots entirely: every
such source frame must yield zero dust/grid coverage and no native dot
geometry, draw or texture. Tunnel scenes are not forced to invent ground dots
to satisfy a fixture assumption. Policy counts print before final fixture
assertions, so a failed targeted check retains its observations.

Checkpoint `dda633a6308d37658787ee865478f5919f4fb6a6` passes 15 root/exact
host suites and all three actual ARM links in
[CI 37195278296](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37195278296).
Root/exact Original/EX scene checks pass 117,342 / 124,516, including 217 intro
panorama frames per cartridge. Full VM/SPC/PCM checks remain 13,111 each and
direct audio passes. Combined fixture padded texture peak is 1,835,264 bytes
including lower LCD, not a whole-flow/peak RAM bound. Downloaded core 3DSX is
2,193,552 bytes; ARM headers and unchanged 488-byte shader are verified.
Evidence: `D:/SFE-validation/3ds-panorama-oct4/manifest.json`.

## Native lower-screen HUD customization

Options' **CUSTOMIZE SCREEN** opens a native lower-LCD layout editor from
pre-game setup, the real preview, or paused runtime Game Options. It retains
the shared source menu rather than replacing it. The upper LCD lists controls;
the lower LCD shows a clearly labeled **illustrative** HUD, not a live game.

Touch-drag a widget; L/R cycles radio, portrait, all three allies, shield,
boost, boss, lives/bombs and the three player-two widgets. D-pad moves four
pixels; hold Select for one-pixel adjustments. Hold A + Up/Down to resize in
25% steps (50-200%, bounded to the 320x240 LCD). X hides/shows the selected
widget, Y restores all defaults, Start applies, and B cancels. Hidden widgets
remain selectable. Opening and Home/sleep resume require input/touch release.
Changing selection or size during a drag retires the old touch anchor.

The cartridge, SPC and world preparation remain paused in the editor. Apply
and Cancel rebase time/input without discarding pending APU writes or partial
audio cadence. Runtime Game Options stays paused after returning. The default
cockpit's pixels remain unchanged. Custom panels are independently rendered
HUD artwork with explicit coverage, never cropped from the finished world;
black pixels are opaque, and unchanged contents retain the redraw cache.
The thirteen reused RGB/coverage panels occupy 154,856 bytes when all have
been allocated; this is not the complete editor/game peak-memory measurement.

Settings schema **4** stores a separate native layout in the existing
checksummed SD journal. Schemas 1/2/3 migrate with default placements while
retaining supported FPS, bindings and EX SRAM. Reset restores native layout
defaults without erasing the EX save. Full source-state loads keep the current
native layout, just as they keep the current physical bindings; they do not
rewind a control profile with the cartridge archive. Desktop/mobile layouts
are untouched. Split HUD still applies only on `game_routing(...).move_hud`
routes; title/map/setup source overlays remain on the upper screen.

Host editor/pixel/journal and actual Original/EX cartridge handoff checks are
separate from actual ARM linkage and original-console touch, LCD, NDSP, SD,
allocation and performance acceptance. This feature does not make the rest
of the incomplete compositor or the full rendering-effects goal finished.

## Native rendering FPS

The real pre-game **RENDER FPS** row switches between **30** and **60**.
Game Options' **SHOW FPS** counts completed whole presentations on the lower
LCD, not source ticks, requested rates or separate stereo eyes. The counter
resets across loading, Home/sleep, quick-menu, remapping and state replacement.

The cartridge raster/input clock remains 60 Hz and SPC blocks remain 20 Hz.
At 30 Hz only the expensive scene preparation and whole LCD presentation are
skipped; the source/audio/input loop continues, and both eyes share one game
snapshot. Original FX pacing is a separate setting and remains the default.
Preview OFF still prepares no world geometry. These are output targets, not
a claim that original 3DS hardware maintains either rate.

Settings schema 4 persists both options in the protected SD journal. Schemas
1/2 migrate to 60 Hz / SHOW FPS OFF, preserving bindings and EX SRAM. Valid
30/60 values also survive full-state restore; desktop-only targets are bounded
to the native range without changing the restored VM or partial SPC timeline.

Host cadence/source/PCM checks and the actual ARM link are separate from
original-console pixel, timing, NDSP, SD and performance acceptance.

Checkpoint `72068db49e11be40d1eefcdae835d8beb272bd2b` passes 15 host
suites and all three ARM links in
[CI 37194119246](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37194119246).
Exact Original/EX checks pass 13,111 each (root 13,054), including five-second
BOOT/stage source/PCM comparisons at 30/60 Hz. The full 80-row shared menu
label coverage is checked without private assets. Core 3DSX is 2,182,208
bytes, not peak memory. This is still a bring-up diagnostic, not a playable
release or original-console frame-rate guarantee.

## Full game states and native quick menu

Physical **Select + Y** opens a paused native quick menu: Resume, the actual
shared Game Options, state slot 0-9, Save and Load. This does not replace or
reorder the cartridge pre-game menu. Navigation is fixed physical A/B/D-pad;
gameplay remapping is unchanged. Opening/waking waits for button release.
Overwriting an occupied slot and loading a state require a second A confirmation.
The panel redraws only on changes; paused navigation performs no VM/SPC ticks
or world preparation. Save/load display their operation before SD work begins.

Each state includes the complete game and both SPC stems, pending APU writes,
partial three-raster audio phase, and carried connected-grid history. Load
prepares a new owner with its own ROM/symbol references, validates all components
and prepares native renderer owners before retiring the old run. A corrupt or
incompatible state leaves the old VM/audio/published scene intact. NDSP resets
synchronously only after preparation succeeds; a DSP reset failure remains an
explicit terminal error. Interpolation and held input rebase, rather than
replaying an old pose or catching up the time spent loading.

Files are separate from settings and EX battery saves:
`3ds-state-<ROM CRC>-<0-9>-<0-or-1>.dat` in `/3ds/starfox-enhanced/`.
Each logical slot alternates between two checksummed, companion-bound generations;
close and re-read verifies a write before committing it. The preceding valid
generation survives interruption/write failure. All-corrupt or conflicting
generations are preserved read-only. Different cartridges have different paths.
The explicit state packet bound is 4 MiB, not a whole-flow peak-RAM guarantee;
physical SD/controller power-loss durability and multiple writers are not promised.

The state work also exposed a shared SPC bug: upstream state copying changed
the live CPU input registers while saving. Saving now preserves the live machine,
and an optional trailing archive extension restores the separate input ports.
Old SPC archives still load. A public nine-byte synthetic SMP program tests
different input/output values, repeated read-only snapshots and continuation,
without Nintendo assets. Actual Original/EX BOOT/stage checks cover all audio
phases 0/1/2, SD reopen/interrupted generations, retired-owner continuation,
held-input guards and frozen runtime-options resume. Exact source checkpoint
`6f923360a0336a4b8098e6810b7e0b2543b437c8` passes all 14 host suites,
6,358 Original/EX session checks each and all three actual ARM links in
[CI 37192122255](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37192122255).
The downloaded core 3DSX is 2,179,148 bytes; static sections and packet sizes
are not runtime peak-memory measurements. Original-device acceptance remains.

## EX player-two lower-screen counters

The cartridge HUD observer now exports EX's actual `LIVESTWO` and `SPECCNTTWO`,
in addition to the existing player-one and ally values. The original game has
no player-two symbols and does not read them. Reserve lives follow the source
zero clamp and active-ship subtraction. When EX activates player two, P1 view
shows separate P2 reserve/bomb labels; P2 view selects those counts rather than
relabeling P1 values. Inactive P2 and non-game/menu routing leave them hidden.
Both count changes and view changes invalidate the owned dashboard cache;
unchanged counters do not redraw. Capturing them does not mutate VM/open-bus
state. Exact checkpoint `77715632220963c6c69369a136b8004b2e312dd3` passes
15 host suites and all three ARM links in
[CI 37192771762](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37192771762).
Root/exact cartridge HUD checks pass 154,787/309,543; full session VM/SPC/PCM
checks remain 6,358 each. The core 3DSX is 2,179,300 bytes, not peak RAM.
Natural two-player flow, full upper-screen HUD partition and console acceptance
are still required; this is lower-dashboard/source-counter acceptance only.

## Cartridge dust and connected ground grid

`GameDots` now consumes the actual captured STAR_COLS table, recycled dust
identities, Q15 ground lattice and canonical carried grid-line endpoint. Stars
and ordinary ground dots become angular source-pixel quads at their real source
depth, shared between the independently projected eyes. Interpolation uses the
camera/view only, so recycling a point cannot create a streak across the scene;
pause and scene cuts discard stale camera interpolation. Source Controls-view
offsets, map/Continue suppression, near double dots and depth clamps are retained.

Connected lines preserve the cartridge's unusual asymmetric pixel walk, rather
than replacing it with a generic wire mesh. A single isolated RGBA ink texture
keeps the canonical 224x192 pattern unchanged, with extra LCD/eye guard coverage.
It is projected onto the ground plane derived from the actual Q15 lattice,
with finite depth and homogeneous source UVs. Authored carried ink beyond the
finite receiver stays at infinity, not HUD depth. A zero-distance plane retains
that far-field ink; this edge case still needs physical visual acceptance.
Transparent holes discard before depth/stencil writes; black ink stays opaque
during raster fades. Unchanged cameras/sliders reuse ink; leaving the connected
mode releases its CPU image. Disabled/map dots allocate no vertex buffer.

The native compositor inserts this stream after scenery and before models;
Preview OFF still skips all world preparation. Twelve lean host suites pass,
including 529,492 public dust/grid checks: independent Q15/connected-ink
oracles, signed wrapping, live brightness, stereo disparity, source UV/plane
registration, bounded texture storage, cache retirement and failure preservation.
BOOT/outdoor checks additionally exercise actual Original/EX dust and ground
dots without VM/SPC mutation. Those fixtures do not naturally enable connected
lines; connected-line pixel comparisons use synthetic public snapshots. Native
ARM cross-link and original-console pixel/performance acceptance are separate
gates; a host pass is not a playable-release or whole-goal completion claim.

[Dust/grid CI 37189934817](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37189934817)
at `eda20477fbbd8dd26a4fa6481c702e54e4775198` now passes all twelve host
suites and all three actual ARM links. Downloaded ELF32 ARM/3DSX headers pass;
core 3DSX is 2,005,008 bytes, and the shader remains the accepted 488 bytes.
Static text/data/BSS are 1,943,516 / 9,872 / 33,604, not peak runtime memory.
Exact Original/EX session parity remains 5,896 checks each. Both cartridge
model/dust suites pass (57,663/65,058 across the pair), through 404 outdoor
phases each. Root PC EXE/index and all source VM/SPC states remain unchanged.
Whole-flow/console pixel/optics/memory/performance and the remaining port work
below are still required. Evidence: `D:/SFE-validation/3ds-dots-oct4/manifest.json`.

## Actual pre-game menu integration (ARM-linked, October 4)

`GameMenu` observes `GameSimulation`'s full page/row order and selected row;
it is not another options state machine. The renderer uses the cartridge menu
font and localization, preserves all current pages, and protects its screen-space
text from source windows/colour math/stereo displacement. Native-inapplicable or
not-yet-connected controls remain visible and unavailable rather than silently
changing a setting that PICA ignores. Actual cheats, audio volumes, language,
face swapping and timing use the shared simulation. Separation/convergence use
the existing stereo menu and the real native eye plan (separation bounded at 64).

The entry now loads the real BOOT menu, prepares only a cached screen UI with
Preview OFF, and displays RENDERING while rebuilding a real cartridge preview.
Preview is silently prerolled to the ordinary stable Corneria/chatter frame;
its PICA models and background remain genuine independent eye geometry. Start
from preview rebuilds BOOT and uses the actual source Start/fade/selected-level
path. Experience changes replace cartridge/SPC/model/renderer owners in reference
order rather than ticking an EX selection against Original data. Implemented
settings and per-cartridge SRAM survive these in-process handoffs. The SD
journal below now adds disk settings/SRAM persistence; physical handoff/audio/
SD behavior and full VM state slots remain unverified or unfinished.

Nine lean host suites pass, including a synthetic-public-font menu renderer
contract. The real Original and EX fixtures additionally check every current
source menu page/row, protected/cached glyph drawing, read-only disabled controls,
native stereo settings, real preview geometry, preview Start and silent-load
cancellation. A shared `boss_roll_active()` observation was fixed to use RAM peek,
so it cannot change the emulated open-bus latch during frame capture. Exact BOOT
and direct-stage VM/SPC/PCM parity pass. These are host checks, not console pixels,
physical console acceptance, a playable release, or the full goal.

The exact source-only branch snapshot is now synchronized with the current
menu API while preserving the merged asteroid control, PS5 hardware-only
renderer policy and portable bit casts. AA TYPE keeps row 42; 3D ASTEROIDS uses
row 79, and old asteroid-menu save cursors migrate correctly. Asteroid models
remain unavailable in the native menu until a PICA adapter is connected; the
shared option/action is not deleted. Compatibility calls cannot reactivate the
retired neural/ReShade menu path. Both real cartridge fixtures pass 4,792
session/menu/preview/compatibility checks, and the longer source-model checks
pass 50,941 Original / 58,336 EX with 404 outdoor phases each.

[Native CI run 37185299509](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37185299509)
at `01bfe252b72bc7e3156811144409c81dd4797078` passes nine lean host suites
and all three real devkitARM links. Downloaded ELF32 ARM and 3DSX headers were
checked; the source-core 3DSX is 1,954,004 bytes. Static text/data/BSS are
1,893,064 / 9,872 / 33,604 bytes, not a measurement of peak runtime memory.
The 488-byte shader matches the previous accepted terrain shader. This branch
snapshot is separate from the dirty desktop worktree; it is not a main-branch
release or verification of the old desktop host against the revised menu API.
Full VM state persistence, remaining scene/effect integrations and physical
original 3DS gameplay/optics/audio/APT/memory/performance remain open.

## Native controller remapping

Open **Options → Controller** in the actual pre-game menu. The upper LCD shows
a SNES controller and highlights the logical in-game action being assigned;
the lower LCD lists its physical button or Circle Pad direction. Menu navigation
stays fixed so remapping gameplay cannot make the options screen inaccessible.
Use A to assign, X to clear, Y for controller defaults, and B to return. Capture
commits after release; B+Start cancels a capture. Ambiguous multi-button/axis
input is rejected until release. The final row changes the Circle Pad deadzone.
Restart is not a fictitious SNES button; the native editor exposes the twelve
SNES game actions. The separate face-swap setting still applies during gameplay.

Opening this host editor freezes the cartridge/SPC without world preparation;
returning rebases the clock, suppresses held keys, and retains partial audio
cadence and pending APU writes. Home/sleep cancels an unfinished capture and
requires release on resume. Custom bindings also drive the five-second in-game
L+R reset, including when those shoulders are assigned to physical face buttons.
While that chord is held, it cannot simultaneously confirm or navigate a menu.
The editor's two LCD canvases are allocated only while open and freed on return;
unchanged input redraws no text or controller pixels.

Bindings are persisted in the protected two-slot SD journal. Previous schema-1
settings and EX SRAM load with the original Nintendo bindings; the next changed
save upgrades only the alternate slot to schema 2, retaining the old valid slot.
Invalid sources/deadzones, unknown formats and existing corruption keep the
journal's recovery/read-only protections. These host-checked controls still need
physical original-3DS input/APT/SD and whole-flow acceptance; there is no playable
release or full VM save-state support implied by the editor.

## SD settings and EX save journal

The native source entry reads `/3ds/starfox-enhanced/3ds-save-0.dat` and
`3ds-save-1.dat`. These are a two-generation journal of implemented settings,
last experience/preview selection and EX's real 65,536-byte battery save bank.
Original has no battery bank; generic retail VM RAM is never saved as SRAM.
The journal envelope is tied to the companion manifest, and EX SRAM is also
bound to its cartridge CRC. A different EX cartridge may boot with defaults,
but cannot consume or overwrite that bank. This is not a full VM save state.

Only the older/incomplete journal slot is written, then closed and re-read
before advancing the cached generation. A damaged newest slot recovers the
preceding valid generation. If both files are invalid/incompatible, the app
can use defaults but disables disk writes and shows a lower-screen setup
warning; it does not silently destroy existing files. Back up those files
before manually moving them aside for recovery. File sizes, schema/checksum,
fields and generations are bounded/validated. Unchanged settings perform no
disk work. Changed data is checked at most once per second; cartridge/preview
handoffs, Home/sleep and exit checkpoint immediately. A write failure warns
and disables further disk saves until restart; it does not stop gameplay.

Ten host suites pass. The journal has 70 public synthetic I/O/recovery checks;
real Original/EX sessions each pass 4,802 checks including disk-reopened settings
and the actual EX bank. Native APT/SD behavior and power-loss durability still
need original-console testing; two-slot recovery is not an SD-controller flush
guarantee or a multi-process locking protocol. Full VM/SPC save-state slots,
control remapping/custom HUD, the remaining scene/effect adapters and physical
whole-flow/performance acceptance remain separate unfinished work.

[Native SD CI 37186312347](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37186312347)
at `d4579cbcbcc11c077ce85eda78eef8b4ed542bd1` passes ten host suites and
all three real ARM links, including the journal and APT entry. Downloaded ARM
ELF32/3DSX headers pass; core 3DSX 1,979,076 bytes. Static sections are not peak
RAM or console frame-rate proof. The local current-source host graph also
passes ten suites and its independent 4,745 checks for each cartridge; it is
not identical to the accepted branch snapshot's shared menu API.

## Five-second mapped L+R settings reset

In the actual setup/options menu, hold the **in-game L and R actions** together
for five uninterrupted seconds. The lower LCD shows progress and release-to-
cancel text. This uses the monotonic host clock, not the emulated FX timing or
the number of rendered frames. Either shoulder release, leaving setup, Home/
sleep, or a clock rewind cancels the hold. The same shoulders used to roll in
gameplay never reset settings. Future remapping must supply mapped action bits,
not bypass this hook with libctru's physical key constants.

At the threshold the old source/audio owner stops before another tick. The
entry checkpoints the current battery bank, resets settings, switches to
Original with Preview OFF, and rebuilds actual BOOT. EX's ROM-bound game save
is kept, including when disk writing is unavailable. The new owner suppresses
held input until release. Defaults are written through the same protected SD
journal; damaged/read-only saves are not silently overwritten and write failure
shows a warning. This does not erase EX progress or implement full VM state slots.

Host contracts pass at 20/30/60/120/240/480 Hz, including zero/long uptime,
duplicate samples, release/focus/rewind guards and one-shot firing. Real Original
and EX fixture checks cover the actual Options page, old-owner freeze, default
BOOT rebind, real bank preservation and gameplay exclusion. Native countdown/
APT/SD behavior still requires physical original-3DS testing. Desktop remapping
and rendering-effects acceptance are not renewed by these native checks.

[Native reset CI 37187201296](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37187201296)
at `a1fe2c2f54d48e89d6d85a80fb1e48fc9c5de665` passes ten lean host suites
and all three real ARM links. Downloaded ELF32 ARM/3DSX headers were checked;
source-core 3DSX 1,978,428 bytes, unchanged shader 488 bytes. Static sections
do not prove peak memory. Exact branch Original/EX fixture checks pass 5,040
each; the local dirty desktop worktree's separate shared API passes 4,983 each.
No physical-original-console, full-flow, performance, release or full-goal claim.

## October 4 controller checkpoint

[Native controller CI 37188702923](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37188702923)
at `b9775b82e9f6654d60b28207dc23ef5bd67a49a1` passes eleven lean host
suites and all three actual ARM links. Public remap/journal contracts pass
188/87 checks; exact Original and EX source fixtures each pass 5,896, including
60 independently mapped gameplay rasters and 24 post-editor source/SPC/PCM
comparisons. The separate dirty desktop source passes 5,839 each. Downloaded
ELF32 ARM/3DSX headers pass; source-core 3DSX is 1,993,808 bytes and the shader
remains 488 bytes with the accepted hash. Static text/data/BSS are
1,932,380 / 9,872 / 33,604 bytes, not peak runtime RAM. Physical console input,
SD/APT, optics, audio, memory/performance and whole-flow acceptance remain open.
This is not a playable release or renewed desktop rendering-effects acceptance.

## Screen layout and controls

- Upper LCD: 400×240 per eye. Gameplay world, reticle and warnings stay here.
- Lower LCD: 320×240 cockpit-style dashboard, with radio text/portrait, lives,
  bombs, shield, boost and teammate/boss status. This follows the split in the
  [reference video](https://x.com/estebanpdn_/status/2103302681930662288).
- Keep the actual pre-game settings menu, including Original/EX selection.
  The diagnostic's setup page is **not** a substitute for that menu.
- Keep full menu/map/results/credits artwork on top; do not remove sprites
  from those screens using gameplay-HUD heuristics.
- A/B/X/Y use Nintendo's printed positions, matching the SNES arrangement.
  L/R map to the in-game shoulders, not keyboard letters. Circle Pad and
  D-pad supply directions.
- The physical slider controls parallel, off-axis left/right-eye projections
  from one immutable game snapshot. Zero renders one eye; it does not slow
  down or advance the simulation. Menu text stays mono; its world preview
  can use stereo. A failed hardware query or a 2DS uses mono.
- Infinite-distance scenery must use the per-eye background offset. Copying
  the same flat sky to both eyes would incorrectly put it at screen depth.

`game_routing.hpp` defines the flow policy. `SpriteSelection` in the shared
`sprite_selection.hpp` contract partitions source HUD and world OBJ artwork **before**
composition. It preserves OAM/VRAM, sprite priorities, reticles and warnings;
it does not erase rectangular regions of the completed game image. Native EX
BG1 overlays and the actual game renderer still need dedicated integration.

`GameHud` now captures source health/boost/boss/teammate status, reserve lives,
bombs, radio text, portrait frames, CGRAM and brightness with non-mutating VM
accessors. It uses the cartridge's glyphs and original/EX portrait data (including
alternate EX portraits), not a crop of the upper LCD. Pause/inactive dialogue
retires old communication artwork. Gameplay alone selects `world_only` sprites;
the pre-game menu and other frontend artwork remain intact. The bridge is
connected to `GameSession` and the native-core diagnostic entry point, but
**not yet to the complete console GPU renderer**. EX's
second-player health is supported, but its reserve/bomb export remains pending;
the bridge does not show player-one counters as player two.

`CockpitDashboard` reuses the lower-screen canvas when status, radio text and
visible portrait/radio pixels are unchanged. Moving the slider does not redraw it.
The cache owns its source data and detects in-place portrait/text updates;
changed source padding or pointer addresses alone do not trigger drawing.
The asset-free diagnostic uses this cache. This reduces redundant CPU drawing,
but is not a measured original-3DS frame-rate claim or completed game HUD.

`pica_projection.hpp` supplies the GPU-facing projection contract: the LCD's
clockwise quarter-turn, PICA's reversed `[-w, 0]` homogeneous depth range, and
parallel off-axis cameras. It provides conservative culling against the union
of active eyes, so shared scene preparation does not lose objects visible only
at one eye's edge. The CPU diagnostic clips lines before perspective division;
`NativeGpu` supplies native triangle projection/clipping through Citro3D.
`PicaShapes` now converts shared cartridge primitives while retaining source
visibility/BSP order. Complete background/overlay integration remains; this
is not yet the full game renderer.
When uploading rows to `C3D_Mtx`, assign its named
`x/y/z/w` fields rather than copying raw bytes (Citro3D's vector layout differs).

## Host checks

No Nintendo SDK or ROM is needed for these checks:

```sh
cmake -S platform/3ds -B build/3ds-host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/3ds-host
ctest --test-dir build/3ds-host --output-on-failure
```

`starfox_3ds_frontend_tests --capture <directory>` writes a lower-screen HUD
sample and two geometry-projection BMPs. All sample status values are synthetic;
these captures are not gameplay or proof of physical-console performance.
The same regression targets are included in the root project's host test suite.
The standalone host build also compiles the diagnostic owner as an unlinked
object, without a mock libctru implementation or a fake desktop game executable.

The root build additionally provides `starfox_3ds_game_hud_check`. Its optional
`ROM SYMBOLS [--capture DIRECTORY]` arguments exercise the real cartridge's
font/portraits/palette and verify that observing the VM changes no saved state.
On October 3, the frontend, HUD-routing, status, Original cartridge and EX
cartridge tests all passed. The frontend passed 175,220 assertions; cartridge
checks passed 154,779 (Original) and 309,534 (EX), including source HUD archive
round-trips with the newly linked game-state owner. Their capture palette/status
fixtures are synthetic even though artwork comes from the local cartridges;
they are not gameplay screenshots or hardware performance acceptance.

`starfox_3ds_game_session_check ROM SYMBOLS` exercises the new `GameSession`
against independent, direct cartridge raster execution. It starts with the real
`BOOT` pre-game state, retains quick physical input, and also checks direct
`LEVEL1_1` startup/bank initialization. Original FX pacing uses decoded source
face counts before the pace decision, independently of rendering/culling.
SPC handshakes and mixed native stereo PCM remain at 20 Hz even when model
updates are slower. The test polls at 240 Hz, compares complete game/SPC archives
and PCM, and verifies that repeated slider/eye reads change no source state.
The session's camera policy preserves the cartridge camera instead of applying
the shared scene capture's headset-only follow adjustment.

The session publishes separate immutable native raster state at 60 Hz for
OAM/VRAM/CGRAM, HDMA, brightness, circle/window wipes and colour math. Those
display changes are not delayed until the next FX model update. Both eyes share
the same model/raster snapshots; unchanged PPU storage is reused. The dashboard
view is borrowed until the next advance and must be uploaded/copied before then.
Suspend/resume suppresses held input, resets pose interpolation and preserves
partial audio-block cadence. Long stalls have bounded catch-up; audio failure
rejects further ticks rather than continuing a partly advanced session.
Experience changes stop at an explicit cartridge-handoff request: the native
host still needs to replace the owner and transfer settings, not run one
cartridge's requested experience with another cartridge's data.

On October 3 the seven frontend/HUD/session tests passed. For each cartridge,
the session checker covers 198 source rasters and 66 exact SPC blocks in both
BOOT and direct-stage fixtures, including between-model-tick fades and partial
audio-block suspension. This is host correctness evidence, **not a cross-build,
console frame-rate/memory result, full-flow sweep or playable package**.
Asset loading and the NDSP consumer are now connected in the core diagnostic
described below. Experience/preview handoffs, persistent settings/SRAM/state
files and the complete native graphics/menu entry still remain. The session
does not advertise MSU-1 without a working decoder/streaming adapter.

### Actual cartridge-core bring-up

`STARFOX_3DS_BUILD_GAME_CORE=ON` builds a lean VM/SPC/HUD/model-adapter library
without SDL, Vulkan, desktop codec packages or OpenXR. The standalone host
build can link the same graph and its source-session/model checkers. The pinned
CPU/SPC sources and SPC state extension are the same as the ordinary runtime;
there is no substitute CPU, silent audio sink or fake native SDK implementation.

The native `starfox_3ds_game_core_check.3dsx` entry point reads the standard
companion at `sdmc:/3ds/starfox-enhanced/Starfox-Assets.BIN`. Only a four-byte
manifest derived from public patches/symbols is compiled in, not ROMs or the
public resources themselves. The shared decoder verifies manifest, complete
file checksum, every payload checksum and lengths. Input allocation is bounded
to 12 MiB before reading; unused cartridge and temporary decoding storage are
released before the session is constructed. This bound is not measured peak RAM.

Its guide uses A to load/run the actual selected cartridge's `BOOT`, Y to load
`LEVEL1_1` directly for a source-scene check, X to select Original/EX before
loading, and Select+Start to exit. Mixed source PCM goes to the sole NDSP owner.
With the default PICA diagnostic enabled, source models, ordered cartridge
artwork, colour operations/windows and the separate lower dashboard now feed
the actual Citro3D presenter. A permanent diagnostic strip identifies the
unfinished terrain/menu state. Without PICA, the upper LCD remains a guide.
The combined source-layer/model/colour/window entry passed native CI
[37182647615](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37182647615)
at `184b3245da19495ca481e83a2051d01873679fad`: eight lean host suites and all
three real ARM executables link. Its downloaded core package is 1,899,356 bytes;
the ARM ELF32/3DSX headers and unchanged 440-byte shader were verified. Static
text/data/BSS are 1,843,316 / 9,872 / 33,604 bytes, not peak runtime RAM.
The subsequent finite-terrain/source-projection changes below passed native CI
[37183379840](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37183379840)
at `aa1fd65f603f38c7f36bcbdd0f9aa82cfb97ec09`: all eight host suites and all
three ARM links pass. The downloaded core package is 1,905,812 bytes and the
488-byte native shader matches independent host Picasso assembly. These are
link/contract checks, not physical PICA/NDSP or performance acceptance.
**This is not a completed game or pre-game menu.**
Do not use it as a playable release: remaining panorama policies, EX spans/
grid/dust and host menu/text integration are not complete.
It does not flatten a finished model scene into two eyes or fabricate a menu.

APT Home/sleep hooks pause queued PCM and rebase the source time/input contract;
held input is suppressed until released after resume. Hooks and model references
retire before their cartridge owner, and the session retires before DSP storage.
SD/manifest/core/DSP failures stay visible on the LCD. Native cartridge switching,
settings/SRAM persistence, full scene composition and physical behavior still
require integration/acceptance; a host result or an ELF link alone cannot prove them.

## Cartridge geometry and model adapter

`SoftwareRenderer::prepare_primitives()` exposes the existing camera-space
source geometry/material path without rasterizing a completed image. It retains
Q15 transforms, source visibility/BSP order, animation, clipping, material/UV
selection and explosion transforms. `PicaShapes` converts polygons, lines and
simple/embedded sprites to owned triangle/texture streams. Two-ink faces use
one shared 8x8 binary mask and native screen parity; they do not allocate a
texture for every ink pair. Texture/draw/vertex budgets remain enforced.
EX wire/wobble/wave/cel span conversions are explicitly unsupported, not
silently replaced with ordinary triangles.

`GameModels` connects actual immutable `GamePresentation` object lists to this
converter. It shares source-pose interpolation with desktop VR without loading
OpenXR, selects LOD from completed source depth, retains source material/light
state, and handles source shadows, reticle policy, intro beams, scaled font
glyphs and owner-filtered particles. Native 60 Hz CGRAM/brightness updates do
not wait for a slower FX tick. Its decoded-model cache is limited to 128 entries
and 4 MiB; that separate cache budget is **not** total runtime RAM. Failed
conversion retains the previous complete stream and reports an error; the
host must not present the old frame as if it succeeded. Source effect-window
clipping now travels with each native draw and is applied after each eye's
projection; sprites are not pre-cropped using a mono view.

The root host tool `starfox_3ds_game_models_check ROM SYMBOLS` runs actual
BOOT and direct LEVEL1_1 sessions for 240 native rasters each. On October 4,
Original/EX pass 20,498 / 28,124 checks, including native PPU resource, source
colour coverage and ordered model/OBJ composition checks. Slider 0/0.5/1 leaves geometry, artwork,
draw order and game/SPC archives unchanged. Additional synthetic particle
fixtures with real cartridge assets cover signed-word wrap, owner/depth rules,
60 Hz fades and failed-frame retention. BOOT exercises source pre-game state;
it does not prove the complete menu/background has been rendered on a console.
Peak streams are 1,500 vertices / 96 draws / 36 textures for Original and
1,497 / 135 / 55 for EX across these fixtures, not all-stage peaks.

The standalone `starfox_3ds_source_models_check ROM SYMBOLS` checks the decoded
catalogue independently across six poses and source camera boundaries. Original
passes 2,697 models / 16,182 poses / 157,363 boundary checks; EX passes 3,511 /
21,066 / 393,943. These are geometry/conversion checks, not full composed images,
optical stereo acceptance or physical-device performance.

## Native PPU layers and ordered composition

`GameLayers` now chooses before-model/after-model cartridge groups from the
actual flow, Mode 1/2/3 register state and native raster snapshot. It retains
EX menu BG1 low/high text with the 16-pixel guard inset, Mode 3's eight-bit map
buffer, title foreground priorities and opaque black, late Controls/Continue/
boss-roll frames, EX pause/results bitmap rules and all gameplay world-OBJ
priorities. HUD selection is applied by the source sprite decoder, not by
erasing a finished image. Intro dialogue is not removed before lower-HUD
routing begins. Controls/Continue margin clear comes from the dominant native
right edge rather than the miscolored left demonstration edge.

Two bounded raster caches retain indices/coverage across slider reads and
recolour source palette/brightness fades without repeating tile traversal.
The outdoor Mode 2 gameplay/Training group now uses `GameScenery` to separate
infinite BG2 scenery from its finite terrain receiver. This is not the complete
panorama/all-flow policy, nor the host-owned pre-game UI or missing source
text/grid/span passes.
Host priority-pixel and actual Original/EX BOOT/LEVEL1_1 resource/state tests
exercise the adapter; physical whole-scene acceptance still remains.

### Source landscape receiver

`GameScenery` reconstructs the source horizon from every valid Mode 2 offset
entry, including signed 8192-word wrap. It uses the verified atlas's own ground
origin and captured source ground height. A clipped finite camera plane shares
the source BG2 texture with the infinite background; source color bands, palette
fades, brightness and opaque black are not replaced with invented lighting.
Homogeneous source UV/Q prevents perspective-stretched stripes while each
LCD eye independently projects the ground's actual depth. Ground writes BG2
ownership/depth, not model-sized rectangles or a flattened world image.
The distant far-plane interval stays in the same source artwork under the
receiver. No sky/ground texture duplication or per-eye tile decode is needed;
the isolated one-hot BG2 draw also avoids an unnecessary A8 provenance copy.

This path is enabled only for identified outdoor gameplay/Training with a
valid captured ground height. EX's pre-game backgrounds stay planar and
tunnels/other background types are not guessed to be terrain. The model
fixture now runs 1,440 native phases for direct Corneria and requires that the
outdoor scene actually appeared; the older 240-phase check ended too early.
Current-worktree Original/EX checks pass 50,941 / 58,336 assertions, each with
404 actual outdoor phases and unchanged VM/SPC state across slider reads.
Original-hardware pixels, ground/model occlusion, default-slider edge coverage,
optical comfort and performance still require physical acceptance. Space,
water, tunnel, intro and other panorama policies still need native integration.

`PicaRaster` decodes cartridge BG1/BG2/BG3/OBJ artwork into cached indexed
layers, with separate write coverage and layer provenance. It retains source
tile/sprite priorities, opaque black ink, mode-2 offsets/HDMA, mosaic and the
gameplay-only HUD selection. Palette, source brightness and BG2 subtraction
recolour cached pixels without decoding tiles again. Changes to irrelevant
OAM or background scroll do not invalidate the other layer. Both eyes and
slider changes borrow the same immutable source raster and decoded artwork.

Screen-space layers retain transparent guards around the authored 256×224
image; infinite-distance scenery uses a 464×240 plane with 32-pixel horizontal
guards and the existing per-eye infinity offset. An unsupported eye extent
fails explicitly instead of exposing missing scenery. `PicaComposite` joins
ordered PPU/model/overlay groups without flattening model geometry or changing
their depth/alpha/projection roles. It validates shared frame plans and the
combined padded texture/draw/vertex budgets before publishing a stream.
Raster owners must remain alive and unchanged through native submission.

The six standalone Windows suites pass, including 5,945,439 PPU/cache/order/window/colour
assertions; the PPU suite also passes on Linux. The native GPU diagnostic links
and submits synthetic PPU scenery around actual geometry through Citro3D.
The Original/EX model checker uses real cartridge snapshots to validate PPU
resources and BG/model/OBJ ordering. These checks **do not establish the final
game compositor**: terrain depth, panorama placement, complete source pass
ordering, EX spans, whole-flow colour/death effect placement, grid/dust and the actual
pre-game renderer still need integration. Do not classify every BG2 layer as
infinite scenery or bake artwork across a model boundary.

`PicaWindow` now generates the source's OR/AND/XOR/XNOR colour-window black
mask as coalesced screen-space rectangles after the ordered upper-LCD scene.
Authored-menu coverage retains the centred 192-line FX window; gameplay uses
full-LCD coverage, including every added edge column/row. Horizontal scramble
shutters keep fractional Y edges without interpolating binary X bounds into
slits. Generation visits only the four window boundaries per row, not a full
scene-sized bitmap. Unchanged masks are shared by both eyes and slider reads.
The mask does not touch the separately submitted lower dashboard.

Per-draw source effect clips use the actual Citro3D scissor, mapped from the
400×240 LCD to the rotated 240×400 render target. Clip identity participates
in draw merging and survives composite assembly. The native presenter resets
scissor state before the lower LCD. Simple sprites retain full geometry/UVs
for both eye projections instead of losing regions to a mono crop; the particle
adapter no longer rejects supported source windows. Independent per-pixel
window tests cover all logic modes, wrapped bounds, closed Training coverage,
fractional horizontal shutters, failed-state retention and mask retirement.
The GPU diagnostic exercises both a clipped alpha draw and an X-toggled source
shutter. Black masks use protected source-layer ID zero; later colour math
cannot brighten a closed wipe.

`PicaColourEffects` adds native screen-space coverage for source circles and
global damage/blackfade colour math, in that order. The integer disk is
converted to coalesced scanline rectangles with optional source clipping;
extreme signed centres and 16-bit radii use 64-bit arithmetic. Both eyes share
the coverage, not a completed mono world image. Native fixed-colour brightness,
selected-layer masks and add/subtract/half flags survive the adapter.

The native depth/stencil target records the winning SNES source layer: BG1 for
Super FX models, distinct BG1/BG2/BG3/OBJ bits for PPU artwork, backdrop for
untouched pixels and zero for protected host UI/window black. Mixed PPU groups
carry a single GPU_A8 provenance sidecar with independent texcoord1; opaque
classes are selected by alpha-test subpasses without mixing RGB by layer ID.
This adds one byte per padded pixel, not six copies of the RGBA group, and is
included in the 4 MiB combined resident texture budget. Palette-only changes
retain the decoded mask and resident A8 upload. Transparent pixels never
claim a source layer. Effect passes test the stencil without replacing it;
the lower-LCD submission resets colour/alpha/TEV/stencil/scissor state.

The GPU diagnostic's Y toggle exercises an expanding circle and blackfade over
native models/scenery, with a red source OBJ deliberately excluded from both.
Portable checks independently test exact disk coverage, all source selectors,
mask swizzling/guards/opacity, failed-state retention and effect retirement.
This is **not physical pixel acceptance or completed whole-game effects**:
PICA fixed-function colour blends in RGBA8 and its half constant has 8-bit
rounding, not exact SNES five-bit re-quantization. Whole-flow pass placement and
physical stencil/alpha/depth/colour checks still remain.

## Native audio adapter

`native_audio.cpp` now implements real libctru/NDSP output for the session's
32 kHz, 16-bit interleaved stereo PCM. A source block is 1,600 stereo frames
(3,200 halfwords / 6,400 bytes). It copies borrowed source samples into an
eight-block, 51,200-byte linear-memory pool and flushes the complete byte range
before queueing. Only FREE/DONE descriptors can be reused; a full queue reports
an error instead of overwriting or silently dropping sound. There are no
per-block allocations or unbounded producer waits. The pool capacity is a
catch-up limit, not a target playback delay.

`NativeAudio` is the sole process NDSP owner and must outlive `GameSession`'s
PCM callback. The intended binding is a sink calling `audio.submit(samples)`;
neither eye calls it. `pause(true)` retains queued PCM for focus suspension.
`reset()` is for cartridge/state handoff: it shuts down the DSP worker before
reusing storage. Destruction likewise finalizes NDSP before freeing descriptors
or linear memory. Missing-DSP initialization includes the result code and a
visible diagnostic error screen. No Nintendo DSP component is distributed;
libctru loads it through the homebrew environment or the console owner's
`/3ds/dspfirm.cdc`.

The asset-free diagnostic now offers an X-triggered, low-volume, one-second
test (left 220 Hz, then right 440 Hz), with at most 100 ms queued. This is
separate from cartridge audio and is **not a playable game**. Its native target
now cross-builds and links the adapter, but has not been run on hardware.

On October 3 the portable PCM/ownership test passed 3,509 checks. All eight
frontend/HUD/session/audio host tests passed; both cartridges still match the
independent source-state/SPC oracle after using the adapter's PCM copy contract.
The native adapter also passed host C++20 warnings-as-errors syntax checks
against unmodified official libctru headers at commit
`9b55eda44cf80b971503991e0c78f9bc8fe50425`, without a mocked NDSP implementation.
That validates declarations/ordinary C++, **not ARM ABI, native linkage, DMA,
audible output, suspend behavior or original-console performance**.

## Native diagnostic build

Use devkitPro's current 3DS toolchain with devkitARM, libctru, Citro3D, Picasso, 3ds-tools,
3ds-cmake and the toolchain's dependencies (including 3ds-pkg-config).
From a devkitPro shell, at the repository root:

```sh
bash tools/build_3ds_frontend.sh
```

The CPU diagnostic is `build/3ds-frontend/starfox_3ds_frontend_check.3dsx`.
Put it on an already homebrew-enabled console's SD card under
`/3ds/starfox_3ds_frontend_check/`. A enters the depth diagnostic, B returns to
its setup page, X tests left/right audio, and Select+Start exits. Its cockpit is synthetic.
This diagnostic uses CPU LCD drawing to check input, eye projection and the
display interface; **it is not a PICA200 game renderer or a performance test.**

`build/3ds-frontend/starfox_3ds_gpu_check.3dsx` is the separate native GPU check.
A displays independently projected textured solids and a translucent layer;
Circle Pad/D-pad moves the front solid, B returns to the mono guide, and
X toggles the source shutter, Y toggles circle/blackfade, and Select+Start exits.
It tests the Citro3D presenter rather than copying a finished
image to both eyes. Its synthetic dashboard is not cartridge gameplay.

## Native PICA presenter and current cross-build

The October 4 actual-core build at `93889fafe1cf88d6f7c9828347eb12ed3afebaee`
passes [native CI 37181717395](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37181717395).
It links the real cartridge VM/SPC/HUD/model conversion graph and NDSP consumer
into `starfox_3ds_game_core_check`, alongside both existing diagnostics, using
the pinned official SDK/GCC 16.1.0 image below. The host graph passes all seven
suites, including 28 companion/manifest checks. The current local standard
companion loads both Original and EX into actual BOOT sessions, each producing
60 native rasters, 20 logic ticks and 20 mixed PCM blocks of 3,200 samples over
one host second, with valid source model streams and the lower HUD.

All three downloaded executables have ARM ELF32 little-endian headers and
3DSX package magic. The core `.3dsx` is 1,812,544 bytes; its ELF static segments
are 1,757,188 text / 9,856 data / 29,220 BSS bytes. These are **not peak runtime
RAM or original-device performance measurements**. The 440-byte PICA shader
still matches the accepted independently assembled host shader. This verifies
the lean native link and host cartridge path, not the physical APT/audio/LCD
behavior or a playable upper renderer. The diagnostic does not replace the
real pre-game menu; that menu and the full scene still need native integration.

### Historical native colour checkpoint

`NativeGpu` now records local-space triangles with per-eye off-axis uniforms,
depth testing, ordered opaque/alpha passes, resident power-of-two textures and
one lower-screen dashboard. Screen overlays remain mono; `PicaSpace::scenery`
uses the infinite-distance per-eye offset rather than placing the sky at HUD
depth. The converter must extend scenery coverage across that offset.

RGB/RGBA uploads preserve source alpha and edge padding in PICA's vertically
flipped 8×8 Morton-tiled ABGR storage. Reads respect row pitch. Input validation
rejects incomplete textures, non-finite geometry, omitted/overlapping primitive
ranges and budgets before starting a GPU frame. Native frame limits are 32,766
vertices, 256 draws and 4 MiB of padded texture storage including the dashboard;
over-budget scenes must be handled explicitly, not silently clipped away.

GPU synchronization precedes VBO/texture reuse. Unchanged bytes retain resident
uploads; changing a model/eye uniform does not force a new geometry upload.
The presenter flushes modified storage explicitly and owns display submission.
Do not call the CPU presenter's `gfxSwapBuffers` while it is alive. Teardown
waits for Citro3D before releasing shader/VBO/texture storage. Native render
errors retire the presenter before showing the CPU diagnostic error screen.

On October 4, 2026, the isolated
[native CI build](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37179931495)
passed using the official devkitPro image pinned at
`sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5`
and devkitARM GCC 16.1.0 at commit
`68be3cfdb9326a61d80089ea639bc83e487c75fa`. It compiled and linked both real
native diagnostic targets, including the PPU layer used by the GPU diagnostic,
ran 107,474 PICA, 175 source-geometry and 5,945,439 PPU/cache/composition/window/colour assertions
on Linux and produced `.3dsx` packages. `GameModels` and source interpolation
also compile as ARM objects; this object-library check does not link the full
GameSession/core or create a game executable.
Their downloaded headers were checked as ARM ELF32 / 3DSX, and the native
440-byte shader exactly matches the independently assembled host shader
(`5a91d7299a63eb6aaf77b26b51c940d7440bdd4fbcaca5b81de427c22f1235dc`).
The GPU `.3dsx` is 387,484 bytes; that is **not a runtime RAM measurement**.

All six standalone host suites pass on Windows, including texture swizzling,
ordered draw validation, depth/eye plans and distinct screen/infinite-scenery
projection. The adapter also passes strict syntax against actual libctru and
Citro3D headers, without mock GPU functions. CI artifacts are diagnostics only;
the test branch does not upload ROMs or the full unrelated development tree.
These results establish native compilation/linkage, **not console display,
audio/DMA, suspend, original-hardware speed, complete cartridge composition or a playable
game package**. The cartridge renderer/entry-point work below remains active.

### Historical host-only checkpoint

On October 1, 2026, both host regression targets passed on Windows with GCC
13.2 and warnings treated as errors. The extended frontend suite passed
175,201 assertions, including independent per-eye projection/depth expectations,
one-eye-only edge bounds, near/far/side clipping and malformed-input rejection.
These are host correctness checks, not console performance measurements.
At that October 1 checkpoint the native target had **not** been
cross-compiled or tested on hardware: this machine had no devkitPro 3DS SDK,
and the official package endpoint still returned HTTP 403 on recheck. No WSL installation,
remote CI run, release publication or console installation was performed.

## Remaining port work, in order

1. Run the now-cross-built CPU/audio and PICA diagnostics on
   original 3DS/XL. Check eyes are not reversed, the slider is smooth, stereo
   switches off cleanly, resume works and Circle Pad/face buttons match.
2. Promote the linked `GameSession`/SD/NDSP diagnostic into the actual console
   game entry. Real pre-game menu, experience/preview/Start handoffs, settings,
   EX SRAM, mapped reset and controller remapping are now connected; finish
   native HUD partition and physical acceptance of the connected touch/button
   layout editor. The 30/60 output controls and measured FPS counter
   are connected. Full VM/SPC state slots
   and runtime menu access now have host coverage; verify their native input,
   SD/NDSP handoff/failure paths and allocation peaks on hardware.
   Preserve the full source menu/timing and clear unsupported states for
   desktop-only graphics features; do not silently enable ignored settings.
3. Complete the **PICA200/Citro3D** compositor around the now-converted cartridge
   primitives and cached PPU layers: terrain depth/panorama placement, complete
   game pass order, EX overlays/spans, full-flow circle/colour-math/death effect
   placement and physical pixel fidelity, physical grid/dust depth/alignment,
   clipping and transparency. The GameModels stream is already connected to
   the native owner. Feed both eyes from one interpolated snapshot, with the same
   off-axis projection contract as the diagnostic; no screen-space fake depth.
4. Retain the now-linked `GameHud` bridge in the game presenter, verify EX
   player-two reserve/bomb export and complete native overlay routing. Apply split-HUD
   selection only when `game_routing(...).move_hud` is true.
5. Profile **New 3DS stereo** and **original-model mono** memory/CPU/GPU budgets separately. Reuse buffers; upload static
   geometry/textures once; update the dashboard only when its contents change;
   render no second eye at zero. Keep native resolution and original game timing
   as the baseline. Set a presentation target only after hardware measurements.
6. Run Original/EX title, pre-game setup, training, map, representative stages,
   boss/death transitions, results, game-over and credits. Then package a playable
   `.3dsx` and decide whether a separate CIA package is appropriate.

The existing desktop Vulkan/D3D/Metal and OpenXR paths cannot be enabled on
PICA200 unchanged. SDL's current 3DS renderer is software-only, so merely
compiling the PC SDL runtime would not meet this port's stereo/performance goals.
New 3DS speedup was disabled in the earlier Original-target experiments. The
current model policy enables it only on detected New models, without changing
source game timing or requiring New-only instructions or memory allocation.

The suggested [OpenCTR SDK](https://openctr.github.io/) was evaluated on
October 3. Its [published binaries](https://github.com/OpenCTR/OpenCTR/releases)
are macOS-only packages from 2015; its source toolchain pins
[Clang/LLVM 3.7.1](https://github.com/OpenCTR/OpenCTR/blob/master/toolchain/CMakeLists.txt).
That does not provide a usable Windows/C++20 cross-build for this port, so it
has not been installed or adopted. Its documentation remains a reference;
devkitARM/libctru/Citro3D remains the intended native stack.

## Primary SDK references

- [SDL3 3DS port constraints](https://wiki.libsdl.org/SDL3/README-n3ds)
- [libctru slider API](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/os.h)
- [libctru models and capability query](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/cfgu.h)
- [libctru LCD rotation, buffering and stereo presentation](https://github.com/devkitPro/libctru/blob/master/libctru/source/gfx.c)
- [Official 3DS CMake toolchain/helpers](https://github.com/devkitPro/pacman-packages/tree/master/cmake/3ds)
- [Official 3DS examples](https://github.com/devkitPro/3ds-examples)
- [libctru NDSP channel/buffer API](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/ndsp/channel.h)
- [libctru DSP lifecycle and component loading](https://github.com/devkitPro/libctru/blob/master/libctru/source/ndsp/ndsp.c)
- [Citro3D's rotated PICA projection and depth conventions](https://github.com/devkitPro/citro3d/blob/master/source/maths/mtx_persptilt.c)
## Native EX corridor and open-colony follow-up

Gekkou's `BG_5_2Z` and `BG_5_2A` now use their source-linked `LTUNNEL_`
dimensions. The unused `KTUNNEL_` constants are not substituted for those
backgrounds. Scripted cameras outside the physical tube remain an explicit
unfinished exterior policy, without clamping the camera or stereo controls.

`BG_2_6A` now has its own open-left colony receiver. `COLONY_MINX` is a player
movement limit, not a wall: only the right wall, ceiling and floor receive
finite depth. The source Mode-1 WATER flag, native pixels, OBJ/BG3 priorities
and source camera are retained. Only the explicitly identified colony painter
batch can decode this receiver without INATUNNEL; malformed generic batches
still fail validation. Camera interpolation tests allow travel out of the open
side and reject a camera on a physical wall. Infinity remains in the opening.

Public source-symbol, nearest-surface/UV and canonical painter tests cover both
closed and open-left geometry, including maximum optics, both eyes and slider
0/.5/1. The eighteen root host suites and six changed-source strict warning
checks pass. These are host tests, not physical LCD, sustained FPS, full-flow
RAM or a finished native port. R4 predates this follow-up.
