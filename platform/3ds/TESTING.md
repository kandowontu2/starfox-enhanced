# Star Fox Enhanced — New 3DS stereo / original-model mono test candidate

## Quick start for testers

1. Download the current `StarFoxEnhanced-3ds-test.zip` experimental player
   (or its numbered tester ZIP) and extract it to the SD card's root.
2. Put your own `Starfox-Assets.BIN` in `/3ds/starfox-enhanced/`.
   The final path must be `/3ds/starfox-enhanced/Starfox-Assets.BIN`.
   Do not put a ROM in that folder instead of the generated BIN.
3. Ensure your normal homebrew DSP setup is installed. Audio needs your own
   console's `/3ds/dspfirm.cdc`; it is not included in the download.
4. In Homebrew Launcher, launch **Star Fox Enhanced - TEST**. Keep the
   pre-game menu and choose **START GAME** normally.
5. Try Training and Corneria. On New 3DS/XL, start with the slider low, then
   compare slider off and halfway. Original 3DS/XL and 2DS intentionally stay
   mono. Check sound, pause/resume, and Select + Start to exit.

Back up existing `/3ds/starfox-enhanced/` saves/settings first. If boot fails,
photograph the on-screen error; it should remain visible. Report console model,
Original or EX, stage/scene, and the source commit in `BUILD-INFO.json`.
This is a test candidate, not a promise of stable console FPS.

## Current build and validation

### R44 source follow-up (October 7)

Native changed-scene vertex uploads now use the borrowed scene directly and
reuse one CPU comparison cache. Held scenes still skip uploads; an unsuccessful
flush invalidates residency so retrying the previous scene repairs the VBO.
This removes the former per-change complete-scene temporary vector without
changing geometry, stereo, source timing or effects. Host regressions cover
steady updates, held/empty/shrinking/restored scenes and failed-upload recovery.
Use the package's `BUILD-INFO.json` to identify its exact source. R43 and earlier
packages do not contain this optimization. Native ARM and real-device tests are
required before treating it as a playable/performance-accepted improvement.

### R43 source follow-up (October 7)

Native saved-state loading now clears unsupported desktop renderer/upscale,
AA, lighting, material, environment and post-processing preferences. It also
keeps MSU-1 unavailable until a native streaming adapter exists. This does not
remove the cartridge's own EX effects or alter its VM/SPC/grid timeline.
Supported timing, audio volumes, cheats and controls remain intact. Real
Original/EX BOOT and stage import regressions pass on the host; this is not
physical-console validation. Use `BUILD-INFO.json` for the exact binary's
source commit; the older R42 ZIP does not contain this fix.

### Previous R42 build and validation

The R42 source-only CI build has passed: use the local
`build/StarFoxEnhanced-3ds-test-r42.zip` or the experimental player artifact from
[native check run 37518401745](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37518401745).
Its source is `10d57b0a2afdd13142aa4da48430e54940269fa0`.
Fresh native emulator checks reach the Original title, Controls, launch and
Corneria in both old-model mono and New-model stereo. New-model upper-eye
images differ while the lower HUD stays identical, and its bounded run closes
normally. The earlier old-model movie needs a planned forced stop after reaching
gameplay; that historical run does not verify clean exit. A fresh R42
Original-model mono run without movie replay also reaches gameplay through
pre-game, title/intro, Controls and map/travel, then closes gracefully at the
planned 261-second limit. Its isolated save/config are restored byte-exactly;
no new pixel or audible-audio acceptance is claimed from this route.
These are smoke checks, not sustained console FPS, physical
slider/audio/Home/sleep or complete stage/results acceptance. No release or
private assets were published.

The current Windows-host regression run also passes all 27 configured 3DS
tests, including Original/EX menu composition, source/effect integration,
remapping, quick menu, HUD, audio ownership and settings/state storage. Native
landscape-flow checks are now registered in CTest instead of relying on manual
invocation. Original Fortuna's ordinary-input/GOD route covers its retained
results and return map with source/SPC and full-LCD priority parity. These are
host checks, not ARM/PICA pixel, audible-audio or console-performance proof;
the R42 binary/ZIP has not changed.

October 6 R42 source follow-up: retained Mode-2 landscapes now keep their
finite terrain and distant sky during stage results. Source BG2 low/high and
OBJ priorities remain separate: score/sprite artwork does not acquire ground
depth. Occupied receiver rectangles avoid duplicating guarded texture pages,
while budget failure retains the complete source raster fallback. The actual
Original Fortuna ordinary-input/GOD diagnostic observes 411 retained results
phases, 42 sampled supported-optics compositions, 14 full-LCD source-priority
comparisons and source/SPC-state parity. These are host checks, not physical
PICA pixels, total process RAM or console FPS. R41 native screenshots below
predate this results fix. Check real stage clears/results on both New-model
stereo and old-model mono, including slider changes during the score tally.

This is an experimental native player, not a verified release. It contains
the real pre-game menu, cartridge simulation, SPC audio, native 3D geometry,
slider-controlled stereoscopic top screen and configurable lower-screen HUD.
New 3DS/XL is now the stereo/performance target. Its normal CPU/cache speedup
is requested automatically. Original 3DS/XL ignores the slider and renders
only one eye; 2DS is also mono. New 2DS XL uses the CPU speedup but stays mono.
The same package supports all models, with no changed cartridge timing.
Physical performance and source scenery/effect transitions still need testing;
this is not a promise of stable 60 FPS.

For EX menu testing, also try Background choices 21, 25 and 35 (orbital),
19/27/28/31 (unique space) and 2 (stars). On New 3DS/XL, the 3D slider should move their
background into depth while the native menu text remains at screen depth.
Other menu/map/Controls routing remains unchanged. Source checks pass; the
physical LCD result still needs testing.

The next EX span follow-up needs native tests of the cartridge's wireframe,
cel, wobble and wave choices: preserve their deliberate holes/deformation,
palette changes and effect-window boundaries while the slider changes actual
object depth. Text and particles must not inherit polygon-only effects.
R11 predates this implementation. Host ink/depth and cartridge-catalogue
checks are not proof of native pixels, complete-scene budgets or device FPS.
Use the follow-up after EX checkpoint `2cb9e0d2` for effect testing: that first
build predates the repeated-chord/off-eye geometry resource correction found
by whole-scene host stress checks. Its green ARM build is not gameplay acceptance.

The source follow-up also fixes final-room panorama depth when the cartridge
retains its tunnel flag. Check final-tunnel exits and near-wall stereo at slider
maximum, including camera banks. Host canonical/resource tests do not replace
physical LCD checks. Older R3 packages predate this production follow-up and
must not be relabeled as containing it.

## Install

Use a candidate after R9: the first isolated native emulator boot uncovered a
32 KiB main-stack overflow during cartridge audio loading in R7. Its earlier
host/link/package checks did not establish successful boot. The source follow-up
reserves a bounded native stack, validated in the R8 ARM build. R8 then exposed
a null secondary-texture binding in the native presenter. R9 fixes both and
reaches the native menu/intro, but actual GPU-window inspection exposed vertically
flipped uploaded artwork. R10's colour/A8 upload follow-up passes the actual ARM
build/package gates and isolated native pre-game/title/Controls/Training visual
check, including live lower-LCD radio/meters. A normal campaign recording also
reaches the Original map/travel briefing and initial gameplay geometry, with
distinct upper-screen eyes at maximum emulated slider and an unchanged lower
HUD. R10's travel map repeats artwork in the outer columns. R11 restricts its
Mode-3 menu panel and passes fresh ARM/package gates plus the normal native
emulator route: map and briefing margins no longer duplicate their artwork,
and initial campaign geometry retains distinct eyes with an unchanged lower
HUD. Full Original/EX stage-flow and physical-device checks remain in progress.
Do not treat an earlier package's host/link checks as proof that its displayed
artwork is correct. The historical R12 candidate is
`build/StarFoxEnhanced-original-3ds-test-r12.zip`; its source commit is
`542464e547faee2e1c946d5136f48d48d64396ea` in `BUILD-INFO.json`.
R12 adds the EX span consumer and bounded-run resource corrections. Its ARM
build/package gates pass, and fresh native emulator tests cover the asset-free
eight-mode effect probe and normal Original menu/map/briefing/campaign entry.
An earlier capture attempt selected emulator popup windows and was rejected;
the fresh actual-game-window captures pass margin and eye/HUD checks. These
are not complete-stage, physical-device performance or total-memory results.

For the retained-results follow-up, use **R42 / StarFoxEnhanced-3ds-test**
from the source-only **3DS native bring-up checks** on
`codex/3ds-followup-r42-20261006`; its ARM/package gates passed in
[run 37518401745](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37518401745).
Its `BUILD-INFO.json` source must be `10d57b0a2afdd13142aa4da48430e54940269fa0`.
R41's independently checked earlier candidate is available from
[native check run 37514141658](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37514141658)
on `codex/3ds-followup-r41-20261006`. Its ARM/package checks passed. The local
tester copy is `build/StarFoxEnhanced-3ds-test-r41.zip`; its `BUILD-INFO.json` source must be
`ed7733fb11963420b62d3573a034f2465d72c473`; do not substitute an older branch's
"latest" artifact. R41 keeps native Controls dust, models, shadows, particles
and text inside the black flight panel without flattening their stereo depth.
The earlier R40 candidate is `build/StarFoxEnhanced-3ds-test-r40.zip`
from run 37508739190, source `a69e729707b35bf57c336661ecaa1cca58d82253`.
Physical console and full-flow acceptance remain open for both candidates.
Check `BUILD-INFO.json`: its target must say New Nintendo 3DS stereo with
original-model mono, and its hardware policy must match the description above.
R38 and older packages predate this change. No ROM, asset BIN or DSP firmware
is bundled. If an artifact has expired, request a current package.

The following R35 details are historical Original-target measurements, not the
current New-model candidate or evidence of its performance. R35 used artifact
`StarFoxEnhanced-original-3ds-test` from
[native check run 37304639533](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37304639533).
Its ZIP contains `BUILD-INFO.json` with source commit
`14326c557288ddda2e4a431e7f1638d9237e6bef`. Do not substitute an older ZIP.
R35 retains the real menu/audio/stereo/HUD and includes the source timing,
object synchronization, page transfers and tunnel-row follow-ups. It adds
packed exact RGBA writes and row-level crop-bound updates; palette-only fades
retain decoded layer ownership. Independent scalar and frozen-renderer checks
preserve colours, transparency and bounds, and real Original/EX route snapshots
retain their complete RGBA/ownership/VM/SPC digest at slider 0/0.5/1. These are
not complete-stage or native-pixel acceptance. Exact signed saturation remains
in the private native DSP copy, without removing voices, samples, filters or
either accurate SPC stem. All 35 host tests and the native ARM/stack/package
checks pass. R35's bounded native emulator replay reduces corridor colour
conversion from 24.413 to 14.391 ms/call (168 calls each), with unchanged
decode/source/audio costs. Matching window-end flow/background is not a
physical-console or exact-pose whole-game FPS result. Its separate asset-free diagnostic adds
an opt-in CSND compatibility test; **gameplay still uses NDSP**. The separate
R34 native arithmetic probe passed 4,326,406 exact inputs under emulation. This
does not establish console audio or gameplay speed. R34's bounded same-input
native emulator replay shows a modest reduction in audio component cost,
with exact host source/audio parity. Window source poses/call counts differ;
this is not a sustained-console or exact-pose whole-frame FPS result. The
earlier bounded R32 replay reaches
Corneria's launch tunnel and outdoor section with reduced decode/logic cost;
it is **not** proof of sustained Original-3DS FPS, complete stages or physical
audio/slider/sleep behavior. This remains a test candidate, not a release.
If the CI artifact has expired, request a current package instead of assuming
an older build contains these fixes. No ROM, asset BIN or DSP firmware is bundled.

For a console with an NDSP initialization crash, the separate small
`StarFoxEnhanced-3ds-audio-compatibility-check` artifact from that same run
offers a Y-triggered CSND left/right test without game data. See
[CSND-CHECK.md](CSND-CHECK.md) before running it. That is a compatibility check,
not proof that the full player uses CSND or that streaming/sleep behavior works.

The separate `StarFoxEnhanced-3ds-spc-saturation-check` artifact from R34 is
an optional **arithmetic check, not a game or sound-output test**. Copy its
`.3dsx` and `.smdh` together into their own folder under the SD card's `/3ds/`.
It needs no cartridge assets or DSP firmware. It reports PASS/FAIL and writes
one uniquely named `starfox-spc-saturation-*.txt` to the SD root; share that
small report if the result fails. Select + Start exits. Do not mistake this
result for sustained FPS, streaming audio, full process-RAM or hardware
acceptance of the player.

1. Prefer a New 3DS/New 3DS XL with an existing homebrew setup and Homebrew Launcher.
   Original 3DS/XL and 2DS use the same package in mono.
   This package does not modify firmware or install a CIA.
2. Extract the ZIP to the SD card root. The program is
   `/3ds/starfox-enhanced/starfox-enhanced.3dsx`.
3. Copy your own current `Starfox-Assets.BIN` into that same directory:
   `/3ds/starfox-enhanced/Starfox-Assets.BIN`.
   Use the asset builder from the current PC package with your own game data.
   ROMs, BIN data, patches and music are not bundled in this test ZIP.
4. Launch **Star Fox Enhanced - TEST** in Homebrew Launcher. It opens the
   pre-game setup, not a forced direct-to-stage diagnostic.

Audio requires your console's DSP firmware at `/3ds/dspfirm.cdc`, as with
other NDSP homebrew. If initialization reports it missing, provide your own
console's dump through your existing homebrew setup; firmware is not bundled.

If assets are missing or incompatible, the error stays on screen. Correct the
SD file and press A to retry. Y on this error screen starts a direct Corneria
test; it is not the normal boot route. X selects Original/EX for the retry.
Back up existing `starfox-enhanced` settings/save files before testing.

## Controls

- Face buttons and L/R follow the SNES/Nintendo physical arrangement.
- Circle Pad and D-pad steer. Start pauses the game.
- On New 3DS/XL the 3D slider changes stereo strength without advancing the
  game twice. Slider zero skips the right eye. On Original 3DS/XL the slider
  is ignored, including during preview and gameplay; the top screen stays mono.
  Slider-off and 2DS use one mono eye. Begin testing at modest separation.
- Select + Y opens the native quick menu (resume/options/save/load).
- Select + Start exits. Home/sleep should suspend and resume safely.
- Pre-game Options includes controller remapping and **CUSTOMIZE SCREEN** for
  the lower HUD. Hold the mapped in-game L+R in setup for five seconds to reset
  settings; cartridge saves are retained.

## Check on New 3DS/XL; separately check old-model mono

Please report model, build commit from `BUILD-INFO.json`, Original/EX,
stage/scene, model, render FPS setting and stereo separation/convergence.
On New 3DS/XL compare slider 0, halfway and full. On Original 3DS/XL verify
both slider extremes stay mono. On New 2DS XL verify mono gameplay. Check
Home-menu return, lid-close/wake and clean exit on each model. Do not compare
New and old emulator timings as though they were the same hardware baseline.

- Preview OFF should keep the plain menu responsive; preview ON should show
  RENDERING during preparation. Start Game, experience switching and restart
  must retain the real pre-game options.
- Play Corneria and Training, then an Armada tunnel and Titania water section.
  Check both slider extremes, LCD edges, sprite/model overlap and palette fades.
- In source builds after R4, check the Original/EX colony's open left side and
  EX Gekkou's entry/inner tunnels. The colony must not acquire a left wall;
  Gekkou's inside camera must have tunnel depth. The signed-face follow-up also
  handles outside/on-wall source cameras without clamping their view or dividing
  by zero. Check entry/exit occlusion and disoccluded artwork particularly closely;
  their full visual/device acceptance is still pending. R4/R5 predate the signed-
  exterior follow-up (R5 does include the inside Gekkou/open-colony receivers).
- Check pause/resume, portrait/dialogue and meters on the lower LCD, fades,
  explosions/death, stage results, map, Controls, game over and end/credits.
  In Controls, stars and demo models must stay inside the black flight panel;
  outside instructions and the controller artwork must remain unobstructed.
- Check audio, remapping, HUD editing, save/load, clean exit, SD persistence,
  Home and sleep. Note sustained FPS, slowdowns, crashes or memory errors.
- In builds after R6, repeatedly switch preview, game scenes and the quick menu.
  Texture replacement now frees all obsolete colour/ownership allocations before
  creating new ones; inactive pixel caches are also freed. Host tests prove the
  bounded replacement sequence, not total device RAM or sustained performance.

Some EX-specific/orbital/exterior scenery-depth policies and full-flow resource
limits are still under validation. Advanced desktop effects and MSU-1 are not
advertised in this lean native port. This package is not proof of hardware
performance, completed rendering coverage or a finished 3DS port.
