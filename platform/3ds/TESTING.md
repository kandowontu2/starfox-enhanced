# Star Fox Enhanced — New 3DS stereo / original-model mono test candidate

## Quick start for testers

1. Use `StarFoxEnhanced-3ds-test-r180.zip` with its companion
   `StarFoxEnhanced-3ds-test-r180-START-HERE.txt`, or download the experimental
   `StarFoxEnhanced-3ds-test.zip` artifact from
   [ARM run 38000160275](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38000160275).
   Extract the ZIP to the SD card's root, keeping its folder structure.
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

The host audio tests now also compile the actual `native_audio.cpp` NDSP
adapter against explicit libctru test doubles. Its 753 checks cover queued and
playing PCM immutability, buffer exhaustion and wraparound, pause/resume,
worker-before-storage retirement, failed initialization/reset/cache flush,
recovery and the shared NDSP/CSND output lease. This is not firmware, IPC,
audible output or console timing acceptance. No player change or new ARM
package is implied by these host-only tests.

The earlier frontend-only host graph passes its 20 registered tests. The
complete cartridge-core host graph now also passes all 41 registered tests,
including the actual pre-game menu, remapping, HUD editor/routing, raster
coverage and audio adapters. This includes a simultaneous direction+confirm
regression: unsupported desktop graphics settings remain unavailable after
navigation, while supported controls, Back and Start keep their source behavior.
These fixes are now in the R134 native ZIP below. Native console testing is
still required.

## Current build and validation

The current tester package is `build/StarFoxEnhanced-3ds-test-r180.zip`
(1,120,357 bytes), from source `bbc5d96956291e5ffa8647cd462f2d99fb0bced5`.
ZIP SHA256: `2d633592020ee7667d1f659e983ed8f0453c2bdf4c7662cee9c66eccce83d31d`.
Its [source-only ARM run 38000160275](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38000160275)
passes all 43 host tests and seven native targets, including stack and package
checks. The five ZIP members, payload checksums and 81 actual ARM compiler
commands were independently checked. The strict Release-O3 build keeps source
game timing and does not enable fast-math, Ofast or LTO.

This exact package completes all 42 ordinary emulator input events, gameplay,
guest audio/GPU/kernel shutdown and normal host exit, with settings restored.
That is functional evidence, not console FPS, audible output, slider or
Home/sleep acceptance. New 3DS/XL stereo and Original/2DS mono remain the policy.
Use your own BIN and DSP firmware. This is an experimental homebrew 3DSX,
not a CIA or release; full console and campaign testing remains required.
Check `BUILD-INFO.json` for the package's source rather than assuming the current
working tree includes every staged change used by this candidate.

## Earlier build validation

The earlier functional baseline is `build/StarFoxEnhanced-3ds-test-r149.zip`.
It reuses clipped terrain/water corner calculations while keeping the exact
emitted geometry. The complete fresh host build and all 43 tests pass. An
independent comparison against R148 matches every serialized vertex float and
draw/texture field across 2,129,355 vertices, including exact-budget behavior
and previous-frame preservation after rejection. A deliberately shifted
geometry copy fails the same comparison. This is not a measured console speedup.

Source-only ARM
[run 37920848892](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37920848892)
passes for source `48535357fd87828e4d849f5e257d4799b42cd885`, including actual
compile/link, stack/memory and package gates. Independent download checks verify
all five asset-free ZIP members, native 3DSX/SMDH and 81 real SDK compiler commands.
ZIP SHA256: `c1436b4c31492bb66d7975d1fbd571d89035922f305582580bfacdb0aea53ca3`.
Its complete ordinary emulator route also passes all 42 real-menu input edges,
guest DSP/GSP/audio/kernel shutdown and normal host close, with the original
settings/save restored. This is functional evidence, not console FPS or audio.
The unchanged complete default session suites also pass in both Original and
EX: 20,651 checks each for serial audio, and 20,789 each with the actual joined
parallel SPC worker (5,229 Original / 5,301 EX joined blocks). These are host
source/audio/session parity checks, not physical PICA/NDSP or console FPS.
Use the quick-start steps above and your own BIN/DSP firmware. New 3DS/XL stereo
and old-model mono policy remain unchanged. Native full-game, audio, sleep and
sustained performance testing is still required. No release is published.

The earlier strict native Release-O3 experiment is available separately as
`build/StarFoxEnhanced-3ds-test-r153.zip`, from
[run 37936446669](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37936446669)
at source `28a28d010551944eade72d8e432964c69abb07f0`.
ZIP SHA256: `9259bf8bdc02be253a9bba95d0ea31cdcedf7d1b86257d77d62a8e62ee9c1caa`.
The full host tests, ARM/stack/package gates, five-member package audit and all
81 real SDK commands pass. Native Release commands use effective O3, with
strict arithmetic and no fast-math/IPO/LTO. Its ordinary emulator route stops
after 26/42 input edges at the unchanged host-memory floor; it has no full
runtime, console or measured performance verdict. Use R149 as the functional
baseline when comparing that historical experiment, and retain your save backup.

The isolated R148 background follow-up passes all43 current host tests and
24,584,446 BG2 pixel, palette, ownership, HDMA, budget and cache checks. It avoids
replanning an identical rejected tile topology; the accurate raster fallback,
source coverage and public validation stay unchanged. An independent removal of
the cache guard fails the regression as intended. This is not a measured console
speedup. Source-only ARM
[run 37917956715](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37917956715)
passes for source `bf2e4901fbb4432a991d1fa039aba94012fe954e`, including actual
ARM compile/link and stack/memory/package gates. Independent download checks
verify the exact five asset-free ZIP members, native 3DSX/SMDH and all 81 real SDK
compiler commands. Use the separate experimental
`build/StarFoxEnhanced-3ds-test-r148.zip`; previous candidates are preserved.
ZIP SHA256: `d992126e44ecf4966f451abdd499f528506a56d10b39d2b3b6c23266a0edb8fd`.
The attempted new full cartridge suite later stops at the unchanged virtual-
memory floor; the remaining suites never start. They have no complete parity
verdict. No release is published.

The R147 joined-audio follow-up is available as the separate experimental
`build/StarFoxEnhanced-3ds-test-r147.zip`; R134 is preserved. A fresh complete host build passes all 174 steps
and 43 registered tests. The complete independent Original serial suite passes
20,651 checks in both Original and EX. Original's complete parallel suite also
passes 20,789 checks and 5,229 actual joined SPC blocks against the unchanged
independent serial source oracle. EX parallel was stopped by the unchanged
memory-safety floor when free system RAM fell below1GiB; it has no complete
verdict. The fresh R147 whole-CMake cartridge wrapper then refused launch rather
than retrying the failed prerequisite automatically.
A supplemental check confirms actual joined-block dispatch after all six
BOOT/stage partial-audio restores in each cartridge. A deliberate dropped-worker
negative control fails as intended. Real ARM compilation passes in
[run 37913961912](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37913961912)
at source `4276d125bda7fe096bc718b81e5e7c5b55ccc6ca`, including stack/memory/package
gates. Independent download checks verify all five asset-free ZIP members,
checksums, native 3DSX/SMDH and 81 real SDK compiler commands. ZIP SHA256:
`cf3240c9dbcc186429e4bfaf1f7261a305780ec07ef392bd264eb9471994d888`.
New-model core-2 access may be denied; old models and allocation failures retain
accurate serial audio. Host tests do not prove core access, audible output or
console FPS. Read `BUILD-INFO.json` for the exact R147 source: its embedded README
also retains the older build-history notes and the same quick-start steps.

The fresh October 9 R141 complete host build passes all 164 build tasks and
41/41 registered tests. Its exact CMake-built checker also passes 20,651
checks in each complete Original and EX default session suite, including the
newer native settings-import tests. Production inputs match the R134 ARM
package below; only the regression-test file differs. This does not create
a newer console package or establish physical performance, stereo or audio
acceptance. This is historical R134 evidence; use the current candidate section
above when selecting a tester ZIP.

The same fresh executable subsequently passes the complete stage-entry/flow
sweeps: all 19 Original and 40 EX stage entries, plus nine flows in each
variant. Each retains 30 source seconds and suspend/resume, comparing complete
VM state, raster, stereo/mono source state and every PCM block. Original passes
306,401 checks and EX 536,204; all consumed production source hashes and build
inputs are revalidated. This is not completion of every level/boss route,
physical PICA pixels, NDSP/audio playback or stable console FPS.

Full fresh-core route checks also pass Fortuna and both Corneria boss/result/
map routes, followed by natural death/restart and Original GAME OVER -> Continue
YES -> route/gameplay. The latter retains three reserve-consuming deaths,
ordinary choice/Start and a fresh map confirmation; no source writes or survival
cheats. Original Continue NO now also passes through a fully bright title,
and EX natural death/restart passes. EX Continue YES also passes through four
deaths, a fresh map confirmation and 120 fully bright gameplay phases. Eight
completed commands total 536,408 checks. EX Continue NO was stopped by the
unchanged memory-safety floor when free system RAM fell below 1 GiB; its full
route and the nine-command collection are not accepted. These remain host-source/composition checks,
not native PICA pixels, audible output or hardware performance.

### Earlier R134 native FPS/menu candidate (October 9)

Use `build/StarFoxEnhanced-3ds-test-r134.zip`, or **StarFoxEnhanced-3ds-test**
from [run 37889505276](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37889505276).
Source: `6d5e00a3b4a43bafefd5749628bc92b72ad5bead`; ZIP SHA256:
`6ac79746f8ccd80158d023176459c1fee487175ccbeb6de5797dd1666328d96d`.

This keeps the earlier menu/editor/viewer fixes and corrects the native FPS row.
In both pre-game setup and runtime Options, try selecting RENDER FPS while
moving Up/Down and pressing A together. It should toggle between 30 and 60.
Hold a horizontal direction, then change direction without releasing: it must
not toggle again until you release. Leaving the row must not change its value.
Start must retain its ordinary game/menu behavior, including a combined action.

The complete Original and EX host suites each pass 20,277 source/audio/session
checks, including 16 FPS fixtures and the existing 48 editor fixtures. Native
CI, stack/package gates, exact downloaded ZIP and complete ARM compiler/SDK
graph pass. New-model stereo and original-model mono policy is unchanged.
This is still an experimental package, not hardware-verified port completion.
Check `BUILD-INFO.json` for the current source; the ZIP README contains older
build-history notes as well as the unchanged quick-start instructions.

### Earlier R131 native menu/editor candidate (October 9)

Use `build/StarFoxEnhanced-3ds-test-r131.zip`, or **StarFoxEnhanced-3ds-test**
from [run 37885687517](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37885687517).
Source: `8dd11720217c8bbbf697b79303c53daeacff37e6`; ZIP SHA256:
`114dd6d31ee0f11a48327b6f0d3e5739b15907561984ed4b241bdeb2221096f8`.

This includes the capability fix below and fixes combined direction+A
opening the wrong native editor. Try moving onto Controller/Customize Screen
and confirming together; closing the editor should leave the cursor on that
row. Back/Start combinations must not unexpectedly open an editor.
All41 host tests, native ARM and package gates pass. Original and EX each pass
18565 independent source/audio checks, including48 setup/runtime editor cases.
The downloaded package and actual ARM compiler/SDK settings are independently
checked. Physical console performance, pixels, audio, slider and Home/sleep/exit
remain unverified; this is an experimental tester build, not a release.

### Earlier R129 native menu candidate (October 9)

Use `build/StarFoxEnhanced-3ds-test-r129.zip`, or the **StarFoxEnhanced-3ds-test**
artifact from [run 37881321281](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37881321281).
Source: `9d0c46f52c84cee839af2e9a3b74014a459a486a`; ZIP SHA256:
`6a74b23770dff4c5f1dd01f4afc8a928d113ca3e95d87c2c01c86d336a06de8a`.
All 41 asset-free CI tests, native ARM compilation, memory/stack/package gates,
and independent downloaded ZIP/CRC/member/checksum/native-structure checks pass.
The actual downloaded ARM graph contains 80 compile commands and seven real
libctru display targets, using ARMv6K/mpcore/hard-float/O2 without fake SDK
headers, fast-math or LTO. These are build checks, not physical console FPS.

This fixes simultaneous direction+confirm bypassing unavailable menu rows.
Testers should check normal pre-game navigation, Back/Start, and supported
options while pressing direction and confirm together. Desktop-only graphics
settings should remain unavailable. R117's model-viewer and Continue fixes
are retained. The installation steps and stereo/mono hardware policy above
are unchanged. Use the source commit in `BUILD-INFO.json` to identify the app;
the ZIP's README also contains historical R117 information. No release or
private assets are included, and hardware acceptance remains pending.

### Earlier R117 native model-viewer candidate (October 8)

Use `build/StarFoxEnhanced-3ds-test-r117.zip`, or the **StarFoxEnhanced-3ds-test**
artifact from [run 37858079646](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37858079646).
Source: `4189673e8f9d1600365d71602204bbc4e9a0d566`; ZIP SHA256:
`5d8fda65784de4f2e0b996afae7fbe06ae1ed1e2fe79592f66012c10319f9170`.
Native compilation, asset-free checks, memory/stack/package gates pass, with
independent downloaded-package and actual ARM compiler-graph checks.

The Continue screen should now show its rotating Arwing instead of an empty
grid. Its shoulder zoom and EX's native model-viewer page use the source pose,
not a fabricated gameplay object. Original/EX focused host regressions pass;
two actual native Continue captures show the restored Arwing rotating inside
the panel. The ordinary-input run also reaches gameplay, performs guest
audio/graphics/kernel shutdown before planned graceful emulator closure, and
restores the isolated save/settings byte-for-byte. All 19 Original and 40 EX
stage entries plus nine flows each pass host source/audio/raster parity.
This is not a full-stage/boss, physical audio/slider, pixel-equivalence or
console-FPS acceptance. Continue/front-end policy stays mono.
The rebuilt adapter also passes all eight registered Original/EX viewer,
natural death and Continue YES/NO tests without extending their deadlines.
Additional focused host checks verify save/load and suspension at every partial
audio phase, preserving exact PCM and the model/palette. These checks do not
replace a console test of SD state storage, Home/sleep or audio playback.
The host viewer checks also reject damaged/truncated saves and malformed
VM/SPC components without changing the active viewer or emitting PCM. They
continue the replacement for24 ordinary-input frames after destroying the old
decoder/session, checking exact source state, audio and geometry. This does
not change the native tester ZIP or establish console failure-path acceptance.
Installation steps, New-model gameplay slider support and old-model mono
fallback above are unchanged. This is not physical-device acceptance or a release.

### Earlier R90 native texture-cache candidate (October 8)

Use `build/StarFoxEnhanced-3ds-test-r90.zip`, or the **StarFoxEnhanced-3ds-test**
artifact from
[run 37793341673](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37793341673).
Its `BUILD-INFO.json` source is `d0b89a73031f6aca415a1bed07f2f9d582c150c6`.
ZIP SHA256: `abeef63a8dedc473ad4a3964113528e0ab03fda1d43309019fa0e12a398bc636`.
All 40 asset-free checks, native ARM compilation, stack/memory/package gates,
and independent downloaded ZIP/CRC/member/digest/native-structure checks pass.

This removes per-change full-image staging allocations from native texture
updates while preserving colour, ownership and failure-retry behavior. The
shared host test verifies exact packed pixels and zero warmed allocations;
that is not console FPS evidence. Follow the unchanged Quick start above.
Check palette fades, scenery/menus, gameplay, audio and slider behavior on
hardware. No private assets or release are included; hardware acceptance
remains pending.

The unchanged R90 ARM player has also been exercised with ordinary keyboard
input in isolated New-model and original-model emulator profiles. Both reached
gameplay and performed guest DSP/GSP shutdown after Select + Start, before the
test's later host-window cleanup. One New-model test-helper cleanup race is
preserved as a failed helper run; the corrected original-model run closes
without forced termination. This is functional guest exit evidence only, not
physical screen/audio, slider, Home/sleep or console performance acceptance.

The New-model R110 repeat also reaches the actual guest DSP/GSP/audio/kernel
shutdown before the planned host deadline, with the package unchanged and
save/config restored byte-for-byte. The emulator later requires forced host
closure (exit-1), so it is not a clean whole-run pass. Hardware testing below
is still required; an emulator shutdown log does not qualify console speed,
screen output, audible sound or slider behavior.

The R115 native-image follow-up captures the same unchanged ARM player through
its real pre-game menu, Original title, Controls, planet map and launch tunnel
using ordinary inputs in an isolated New-model emulator. The viewed Controls
pose has a uniform dark-blue surround and a demo model confined to its panel;
the launch tunnel has differing eye views and a populated lower HUD. The
emulator's side-by-side display repeats the bottom panel, but the console
presenter draws one physical lower LCD. These five stills are functional
evidence only, not complete pixel/projection equivalence, full gameplay,
physical output/audio/slider or performance acceptance. Guest shutdown is
observed before the host deadline, but emulator closure still requires forced
termination; retain that host failure. Config/save restoration is byte-exact.
The current R90 tester ZIP remains unchanged.

R116 then extends that unchanged New-model ARM run into live Corneria,
multiple natural deaths/restarts and the actual Continue screen. Six reviewed
captures show scenery/models, updating radio/shield/reserves and the lower
HUD switching away from gameplay meters at Continue. The sampled stills do
not capture the death-circle transitions or verify Continue YES/NO. Guest
shutdown precedes planned host closure, which completes gracefully this time;
the earlier forced-closure failures remain recorded. Save/config restoration
is byte-exact. This adds emulator functional coverage, not physical-console
pixels, audio, slider, sustained FPS or complete gameplay-route acceptance.
The R90 player and package have not changed.

The October 8 R98 host-only death/restart follow-up also passes Original and
EX with ordinary stationary flight and no survival cheats or source-state
writes. It follows real health loss, death circles, the black fade, restart
and 120 consecutive bright recovered gameplay phases. Original passed
19,480 checks/84 consecutive VM/SPC/PCM/raster windows; EX passed
30,930 checks/156 windows. The complete native scene-owner graph was checked
at supported maximum stereo and original-model mono for 1,536/1,767
compositions, including the lower-LCD texture budget and source-state purity.
This is host source/audio/resource evidence, not ARM/PICA pixels, audible
console output, game-over/continue, total process RAM or physical FPS. It
does not change the R90 ARM player or tester ZIP.

The same unchanged single-death gates also pass on the R104 host diagnostic
after adding separate natural GAME OVER/Continue route coverage. Original
and EX retain the same source phases, checks, parity windows, compositions
and texture peaks listed above. Both complete R104 Continue-NO routes pass
through the actual title with120 bright recovered phases. The YES test driver
omitted the source-required fresh button press on the returned map; those
owned runs were stopped and remain invalid-driver failures, not passes.
The corrected R106 host driver uses ordinary fresh START there, leaving the
player unchanged. Both unchanged default death regressions also pass on
R106, with identical parity/resource statistics and output hashes to R104.
Original Continue-NO also passes on R106 through the actual title with120
bright recovered phases, preserving200 parity windows/8,934 complete
compositions and3,805,440 padded texture bytes. Original Continue-YES now
also passes through the real route and live/full-shield gameplay plus120
bright rasters (230 parity windows/14,271 complete compositions). EX NO/YES
both pass:703/732 parity windows and11,625/16,980 complete compositions;
peak padded scene/lower textures3,830,016 bytes. The same bounds, source
purity, stereo/mono and complete resource requirements were retained.
All six corrected-driver runs are terminal host passes. These direct runs
do not qualify pending registered CTest deadlines, physical hardware or
change the R90 ARM package.

For a hardware regression, let a ship die normally in Corneria, then check
that the death circle/fade covers the upper screen in both eyes, the lower
HUD remains correct, and controls/audio/scenery recover after the restart.
Report Original/EX, console model, slider position and the package's source
commit. Do not infer sustained console speed from these host tests.

### Earlier R89 merged-source ARM candidate (October 8)

Use `build/StarFoxEnhanced-3ds-test-r89.zip`, or the **StarFoxEnhanced-3ds-test**
artifact from
[run 37788629827](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37788629827).
Its `BUILD-INFO.json` source is `b7ca8f36dee86265e27e8bc74c327a67f717daad`.
All 40 asset-free checks, native ARM compilation, stack/memory/package gates,
and independent ZIP/member/checksum/3DSX/SMDH inspection passed.
ZIP SHA256: `bcfb22a7bbc6968c23298e5d2ba305e53d4ccf314b9fbfb1b4d35b3eb3cff164`.

Follow the Quick start above. Identify the package by `BUILD-INFO.json`, not
the older build-history entries in its bundled README. The pre-game menu,
New-model slider stereo and original-model mono fallback remain. No private
assets or release are included. This is a native-build-qualified candidate,
not a claim of hardware-accepted FPS, audio, PICA output or full gameplay.

### Earlier R74 runtime-remapping audio follow-up (October 8)

The earlier checked candidate is `build/StarFoxEnhanced-3ds-test-r74.zip`, or the
**StarFoxEnhanced-3ds-test** artifact from
[run 37754152691](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37754152691).
Its `BUILD-INFO.json` source is `bbc9d0e4d362e5c4dd3e6584772c7cef7200b2cb`.
ARM compilation, all 40 asset-free checks, native stack/memory/package checks
and independent downloaded ZIP/member/checksum/3DSX/SMDH inspection passed.
ZIP SHA256: `fd7f18a475e218cedc4d6b4a3d026032091f3a330eb017ded7d968b982e5a894`.

This retains R47 and the merged compatibility fixes. Closing controller
remapping into runtime Options now keeps NDSP playback paused, just like the
HUD editor. BOOT pre-game audio still resumes normally. Original and EX each
passed 918 independent VM/SPC/PCM checks across both runtime editors and all
three partial audio phases. That is source-timeline validation, not a physical
NDSP playback result. Keep the Quick start above; identify the downloaded ZIP
by `BUILD-INFO.json`, not the older build-history entries in its README.
Pre-game remains, New 3DS/XL use slider stereo, and old/2DS models stay mono.
No ROM/BIN/DSP firmware is included and no release has been published.
Console FPS, audio, slider, PICA output and complete-route acceptance remain open.

The October 8 R78 host-only route checks also passed full Original Corneria
course 3 and Fortuna boss clears, visible results and bright map return against
independent complete VM/SPC state, exact PCM windows and native raster/fades.
Both require a fully populated native boss-health meter before witnessing
damage. Corneria passed 47,109 checks/302 consecutive parity windows; Fortuna
passed 47,755 checks/306 windows. The drivers emit ordinary mapped inputs with
explicit god survival, never write health/positions/strategies/exits, and check
slider/mono source purity. These are not other-course or physical-console
results and do not change the R74 ARM player or ZIP.

The October 8 R81 host-only first-Corneria route also passed Attack Carrier,
visible completed results and 120 fully bright return-map phases: 41,169 checks
over 263 consecutive independent VM/SPC/PCM/raster/fade windows. It requires a
fully populated 72/72 boss meter before witnessing ordinary-input damage.
Exposed children and the body are targeted with normal Control A buttons and
finite bomb stock; only the normal god-survival preference is enabled. No
health, positions, strategies or exits are written. This is source/audio and
slider/mono purity evidence, not a full native renderer, PICA pixel or physical
console result, and does not replace the R74 package.

The October 8 R85 host renderer-owner route also passed all 15,776 source
phases through Attack Carrier, results and 120 consecutive bright map phases.
Its 641,776 model-stream/source-policy checks include 30 exact full-LCD isolated
BG/OBJ painter comparisons and maximum-menu optics/source-purity/resource checks
every phase. Padded optics residency peaked at 3,883,264 bytes with lower LCD.
This is not ARM gameplay, PICA pixels, full compositor or physical console
performance acceptance. Only the host reference's source coordinate grouping
changed; the R74 native player and tester package remain unchanged.

### R47 source follow-up (October 7)

The source-only ARM CI run has passed, including its 40 asset-free checks and
native package checks. Use `build/StarFoxEnhanced-3ds-test-r47.zip`, or the
**StarFoxEnhanced-3ds-test** artifact from
[native check run 37661588898](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37661588898).
The source commit is `9fad4888fdc06a00f308a90d57ccdc1eafa76895`, recorded in
`BUILD-INFO.json`. Extract the tester ZIP to the SD card's root and follow the
Quick start above. It includes no ROMs/private assets and is not a published
or hardware-accepted release.

The authorized October 8 source-only recheck also passed at
[run 37726898304](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37726898304)
on `codex/3ds-ci-recheck-20261008`, using the same immutable R47 source commit.
Its downloaded tester ZIP is byte-identical to the retained R47 package
(SHA256 `8e3c1f5d2bf8de7b250680f05e00e0071e194c94e5d4eefa04f0a348c8cdf9bd`).
The five allowed members, four payload checksums and native no-RomFS structure
were independently rechecked. This is a fresh ARM/package result, not a new
player revision, physical-console validation or release publication.

The October 8 R62 local host diagnostic also passed all 19 Original stage
entries and nine native flows at 30 source seconds per entry: 306401 checks of
native VM state, source raster, exact SPC/PCM, stereo planning, mono fallback
and suspend/resume parity. EX's separate 40-stage/nine-flow sweep also passed
(536204 checks), and both default session regressions passed. This
diagnostic-only addition is not a new ARM player/package,
complete natural level/boss routes, physical PICA output or console FPS proof.

The October 8 R70 host-only follow-up adds `--fortuna-source-route` to
`starfox_3ds_game_session_check ROM SYMBOLS`. It compares an ordinary-input
Original Fortuna boss/results/map clear against independent complete cartridge
and SPC state, exact PCM windows, native raster/fades and slider/mono source
purity. The normal god preference is explicit; no boss health/position/exit
is written. Its full-route run passed 47755 checks over 306 parity windows,
including live boss/damage/results and fully visible map return at phase18322.
This does not replace the
R47 ARM ZIP or establish other courses, PICA output or physical console FPS.

Landscape, water and tunnel receivers now reuse separate unpublished/published
CPU geometry buffers. Finite-eye coverage and clipped receiver polygons use
bounded stack storage, retaining the original closed edges, source UVs, depth
and painter order. Failed clips, validation and complete-geometry budgets leave
the last published receiver intact. Retained vertex capacity is capped at the
native complete-scene limit; borrowed image pixels are not duplicated.
The warmed six-path fixture changes terrain height, water height, camera and
slider over 1,080 preparations. Its 77,352 previous allocations become zero,
with identical vertex/draw totals and exact geometry digest. Near-limit growth,
late failure/retry and empty-scene tests also cover the reused banks. This is
a host resource/geometry result, not physical console FPS or peak process RAM.
R46 and earlier packages do not include this source follow-up. Identify the
actual candidate using `BUILD-INFO.json`; do not relabel an older ARM package.

### R46 source follow-up (October 7)

The R46 source-only ARM CI build has passed. Use the numbered tester package
`build/StarFoxEnhanced-3ds-test-r46.zip`, or download the
**StarFoxEnhanced-3ds-test** artifact from
[native check run 37603407882](https://github.com/kandowontu2/starfox-enhanced/actions/runs/37603407882).
Its source is `75e7478c37ab19f62a2471318fb9e57a24e42a31`, recorded in
`BUILD-INFO.json`. The CI artifact contains the tester ZIP; extract that ZIP to
the SD card's root and follow the Quick start above. This build supersedes
R45 and earlier candidates. It contains no ROMs/private assets and is not a
published or hardware-accepted release.

Native source textures reuse bounded CPU RGBA buffers instead of allocating a
temporary image for every textured/dithered face. Only the active scene's
textures are published; source texels, palettes and partial alpha are decoded
afresh, and the immutable screen-parity mask is created once per scene. CPU
texture storage has a separate 4.25 MiB bound including inactive slots/workspace;
the existing 4 MiB padded GPU budget including the lower LCD is unchanged.
Changing near-budget sizes/counts, late failure/retry and empty scenes have
regressions. The warmed 180-scene material fixture reduces 40,320 allocations
to zero while independently checking current RGBA, wrapping, sprite and dither
ink. This does not make the complete model-preparation path allocation-free,
nor establish console FPS, peak process RAM or complete native pixel fidelity.
R45 and earlier packages do not include this follow-up. Check the exact source
in `BUILD-INFO.json`; do not relabel an older ARM ZIP as the updated candidate.

### R45 source follow-up (October 7)

Native model conversion reuses polygon workspace, submits triangle fans
directly and uses fixed-size ribbon corners. The unpublished/published model
owners also retain their scene buffers across frames; texture views are prepared
before publication, so a failure cannot replace the last complete scene.
Host checks compare the exact source fan, line, sprite, clipping and EX geometry,
and count allocations only in warmed polygon/ribbon conversion. This does not
claim every game-preparation phase is allocation-free, nor establish console FPS.
R44 and earlier packages do not contain this follow-up. As always, identify the
exact source in `BUILD-INFO.json` and test real audio/controls/stereo/performance.

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
