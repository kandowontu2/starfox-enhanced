# Star Fox Enhanced — New Nintendo 3DS-family test candidate

## Quick start for testers

Supported targets are **New 3DS, New 3DS XL and New 2DS XL**. Original
3DS/3DS XL/2DS are unsupported: the owner reports only 2--3 FPS. Please do not
request further old-model performance tests. Legacy mono code and historical
results below are retained, but no longer describe the supported target.
For now, development/regression testing remains emulator-led rather than
requesting a hardware test for each incremental candidate.
The current priority is sustained **60 FPS in mono**, with the slider fully
down. Do not request stereo tuning/acceptance tests until that baseline is
stable. The existing slider functionality is retained, not treated as evidence
that the mono performance target is already met.
Fresh settings use stereo separation 16 and convergence 512; existing saved
values are preserved. The Stereoscopic 3D menu still controls both. The new
default increases distant-background depth without moving HUD/menu text or
changing the one-eye slider-off/New 2DS XL path.

1. The latest source-only candidate is the experimental
   `StarFoxEnhanced-3ds-test.zip` artifact from
   [ARM run 38464851895](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38464851895).
   R376 remains the hardware-tested baseline, not the latest source candidate.
   Extract the ZIP to the SD card's root, keeping its folder structure. When
   updating an earlier build, replace only the `.3dsx` and `.smdh`; preserve the existing
   BIN, settings, saves and diagnostics.
2. Put your own `Starfox-Assets.BIN` in `/3ds/starfox-enhanced/`.
   The final path must be `/3ds/starfox-enhanced/Starfox-Assets.BIN`.
   Do not put a ROM in that folder instead of the generated BIN.
3. Ensure your normal homebrew DSP setup is installed. Audio needs your own
   console's `/3ds/dspfirm.cdc`; it is not included in the download.
4. In Homebrew Launcher, launch **Star Fox Enhanced - TEST**. Keep the
   pre-game menu and choose **START GAME** normally.
5. R376 is confirmed running on New 3DS XL with sound, slider and HOME/exit.
   Its remaining reported problems are ground hiding lights/model bottoms and
   low presentation FPS. Start with Preview OFF and the slider down, then
   keep the slider down and enable the FPS counter. Preserve the newest
   `native-frame-profile-*.csv` for performance investigation. If a hard freeze
   recurs, stop repeating it and send the newest startup/frame CSVs.
6. Once ordinary Start is stable, try Training, Original and EX gameplay with
   the slider fully down. New 2DS XL uses mono with the same faster
   CPU/cache profile. Check sound, pause/resume, and Select + Start to exit.

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

The current frame-pipeline candidate is source
`f635c0d1d85dd0f2973651bb18725abe865d8260`, from
[ARM run 38444509351](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38444509351).
It uses the imported Starwing mono renderer for ordinary mono scenes, retains
the exact native path for cartridge window/colour effects, and removes gameplay
profile SD writes. The final SPC block overlaps presentation and independent
gameplay raster phases, joining before source logic or the next packet; a distinct,
APT-budgeted effects worker can split that complete block on supported models.
Warm GPU handoffs no longer restart startup-trace SD writes. Presentation now
consumes completed source phases instead of independently gating those frames
with a second wall clock. The native ARM, shader and stack gates pass. Eight
Original/EX serial/parallel audio tests passed for the earlier pipeline source
`2780dcaa`, including deliberately
held packets across intervening rasters and exact PCM/VM state at barriers.
Earlier full-suite baselines each made 22,173 checks, including menu/editor
handoffs, disk settings/SRAM reopen, restores and 30/60 output cadence; those
historical full-suite runs are not a new full-suite result for this commit.
No source audio command or simulation phase is dropped. The save codec also
retains legitimate full-bank SPC transfers in partial-phase states within its
unchanged 4-MiB whole-file cap and wire schema.

Accuracy-enabled OpenGL and explicitly selected Vulkan input replays of that earlier
image reach planned DSP shutdown without movie errors. The OpenGL visual samples
retain white ground dots and foreground models. Complete guest GSP retirement
and clean emulator shutdown are not established by every run; stopping the owned
emulator after a bounded replay is not a clean-exit result. The recorded 100%
CPU samples remain far below the mono target. A separately repaired 300% CPU
replay shows sampled completed-presentation HUD readings of 49/56/50 FPS, not
sustained console 60 FPS. Its input ordering was probed and validated separately;
the original 100% movie desynchronizes at that clock and is excluded. The
emulator's unimplemented New-model CPU/cache service makes this sensitivity
experiment distinct from physical New 3DS validation. No incremental tester
request or release is implied by these results.

The follow-up mono queue fix distinguishes a busy acquire from tile rejection:
it retries the same immutable foreground once with a blocking GPU acquire, instead of
immediately rerasterizing the complete scene. Actual atlas/quad rejection still
uses the complete fallback with a blocking acquire before rewriting textures.
The SDK's VBlank sync is not a GPU queue join. Host tests
cover all success/busy/rejection pairs, bounded retry, failure propagation and
retirement order; profiling appends separate `mono_retry` / `mono_fallback` rows.
In the prior commit's 300%-clock Vulkan profile, the background-3 interval has
482 mono draws for 337 mono presentations, consistent with 145 extra fallback
redraws. The follow-up source `490c0f0b` in
[ARM run 38422455437](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38422455437)
records 145 actual fallback calls and no busy-acquire retries in that same
background interval. It did not measurably improve this replay's timing.

The next source follow-up merges opaque uniform BG2 runs **before** the
fragment-budget check. A separate 3,600-phase real EX `LEVEL1_1` host probe
reproduces 679 raw-plan rejections (maximum 22,500 fragments); compaction
eliminates those rejections with at most 1,500 fragments in that background.
Its 728 sampled/previously-rejected frames retain exact indexed pixels and
coverage. The corresponding Original probe retains exact pixels in 43 sampled
frames and does not introduce budget failures. Asset-free tests additionally
cover scanline HDMA, rolled carry, palettes/flips, 16-pixel tiles, character
address wrapping, transparent ink, exact capacity and cleared failure output.
The new asset-free reference-planner test passes 43,613,479 checks, and the
native source-only CI passes all 51 host tests plus ARM/shader/stack/package
gates. The ARM planner's static frame is 81,288 bytes within its 128-KiB worker
reservation; this is not a full call-chain/physical peak-memory result.

The same validated 300%-clock Vulkan movie now records no `mono_fallback` or
`mono_retry` calls. In its background-3 interval, mean inclusive loop time
drops from 38.484 to 23.169 ms, and mono finishing from 11.988 to 5.934 ms/call.
The background-51 interval is broadly unchanged (19.381 to 19.094 ms/loop).
Window labels may include transitions, and presentation counts/poses differ;
these means are not completed-presentation FPS. Unrelated concurrent work was
retained. This removes a reproduced expensive fallback, but does not establish
sustained 60 FPS or native New-model CPU/cache performance. Input replay has no
movie errors and reaches planned DSP shutdown/profile flush; complete guest GSP
retirement remains unverified, so the owned emulator was stopped afterward.
No incremental hardware test or release is implied.

The model-span follow-up replaces point-by-point opaque/two-ink polygon writes
with clipped indexed rows. Absolute screen dithering parity, opaque black,
coverage, layer ownership and recorded/scaled fallbacks are preserved.
47,571,416 framebuffer comparisons and the existing 4,608-frame polygon oracle
pass. Original/EX 240-phase checks keep their previous frame digests and
80 audio blocks. All 51 asset-free host CI tests and the native ARM, shader,
stack and packaging gates pass.

In the same accuracy-enabled Vulkan/300%-clock sensitivity replay, EX
background-3 model rasterization decreases from 5.807 to 3.133 ms/call;
background-51 decreases from 2.896 to 1.755 ms/call. Finishing is broadly
unchanged (5.499 and 5.160 ms/call). Inclusive loop means are 19.431 and
17.688 ms, respectively; they are not completed-presentation FPS. Counts,
poses and window-end labels differ, and unrelated concurrent work remains.
There are no replay errors, fallback or retry calls. Planned DSP shutdown
and profile flush occur; guest GSP retirement remains unverified and the owned
emulator was stopped afterward. Stable mono 60 FPS is still an open gate.

The next validated source adds a bounded draw-local BG1 row cache and bulk
opaque-run world composition. 18,552 independent ordered-point comparisons
cover clipping, scaling, transparency, tags, coverage, dither and in-place
shifts. The background's pinned pre-optimization trace and 192 cached-versus-
point cases also pass. Original/EX retain their prior 240-phase frame digests
and 80 audio blocks. All 52 asset-free CI tests and the ARM/shader/stack/package
gates pass. Two appended finishing subphases retain the fixed 512-window,
512-KiB diagnostic cap and shutdown-only SD serialization.

The same accuracy-enabled 300%-clock replay measures background-3 finishing
at 5.378 ms/call and background-51 at 5.201, versus 5.499/5.160 previously.
This is a small change, not a major native gain. The new child phases place
about 3.45 ms/call in the EX overlay and 1.69--1.86 in final canvas work.
The subsequent tag-free row experiment (`4c308b7f`, ARM run 38437029767) passes
all correctness/native gates but increases overlay timing to 3.677 ms/call.
That specialization is reverted in the current candidate. Its draw-local
opacity masks skip clipped tile rows that cannot write any visible ink, with
all 52 asset-free host CI tests and native ARM/shader/stack/package gates passing.
Original/EX retain their previous 240-phase image digests and 80 audio blocks.
The same accuracy-enabled Vulkan/300%-clock sensitivity replay measures EX
overlay means of 3.182/3.190 ms in backgrounds 3/51, versus 3.454/3.460 for
`502788c5` (about 8% lower). Finishing is 5.105/4.931 ms; canvas remains
1.863/1.694 ms. Inclusive loop means are 19.234/17.557 ms, not completed FPS.
No fallback, retry or movie errors occur; planned DSP shutdown and profile
flush occur. Guest GSP retirement remains unverified, so the owned emulator
was stopped after the flush and its CPU setting restored to 100%.
The sensitivity, window/count/concurrent-load and unverified guest-exit
limitations above still apply. No incremental hardware request, release or
60-FPS acceptance is implied.

The subsequent source-only experiment (`fe4f5f18`,
[ARM run 38440520170](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38440520170))
kept all lazily decoded vertical rows for
each of 32 tile keys, instead of evicting an earlier row when the scanline
changes. Its draw-local payload is bounded to 9,408 bytes, does not survive a
draw, and initializes a row before its validity bit permits any read. The
independent mosaic-size-one decoder comparisons now include deliberate
tile-key collisions. Local background/extents/composition tests and both
240-phase Original/EX image digests pass with 80 audio blocks unchanged.
All 52 CI tests and ARM/shader/stack/package gates pass, with a 9,944-byte
static `draw_bg1` frame. It does not improve the native sensitivity replay:
overlay means are 3.224/3.231 ms in backgrounds 3/51, versus 3.182/3.190 for
the smaller opacity-mask cache. Finishing is 5.146/4.971 ms and inclusive
loop means 19.311/17.466 ms; those loop means are not completed FPS.
No fallback, retry or replay errors occur; planned DSP shutdown/profile flush
occur, but guest GSP retirement remains unverified. The owned emulator was
stopped after the flush and restored to 100% CPU. The larger cache is reverted
because its extra stack is not justified by the measured result; the stronger
independent collision fixture is retained. The current renderer remains the
validated opacity-mask implementation above, not a new 60-FPS result.

The next source-only follow-up checks exact BG1 tilemap/referenced-character
bytes, scroll/geometry and CGRAM black/nonblack membership between draws.
Nonblack RGB-only changes keep the indexed overlay and use the live palette.
The real EX 240-phase host probe finds 197 unchanged-source phases; the
renderer records 190 cache hits and 50 rebuilds, including handoffs through
eight complete CPU-background fallback checks. It borrows the existing
400x240 CPU-background allocation only while BG2 is on PICA; CPU BG2 and EX
BG1 roles invalidate each other before storage is reused. No extra pixel
allocation is introduced. A <=904-byte row index transfers only the source
256x224 rectangle, retaining transparent holes, coverage, tags, dither erasure
and ordered commands. Independent tests cover 576 dependency mutations,
64 borrowed-versus-direct rasters and 64 sparse-transfer mode combinations.
Original/EX retain their prior 240-phase image digests and 80 audio blocks.
All 52 CI tests and ARM/shader/stack/package gates pass. The same
accuracy-enabled Vulkan/300%-clock sensitivity replay records overlay means
of 0.963/1.146 ms in backgrounds 3/51, versus 3.182/3.190 for the earlier
opacity-mask source. Finishing drops to 2.890/2.888 ms/call; inclusive loop
means are 15.744/15.762 ms. These are not completed-presentation FPS.
There are no fallback, retry or replay errors. Planned DSP shutdown/profile
flush occur; guest GSP retirement remains unverified, and the owned emulator
was stopped after the flush with CPU configuration restored to 100%.
Counts/poses, window-end labels and concurrent unrelated work limit timing
comparisons. Retain this exact reuse improvement, but sustained mono 60 FPS
and full native/hardware acceptance remain open gates.

The subsequently rejected experiment (`b22d1c38`,
[ARM run 38448334985](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38448334985))
fuses native world-canvas clearing, coverage reset and
transparent composition into one pass over the existing allocation. It applies
only to independent, equal-size 1x canvases without tags, dither metadata or
recorded commands; rejected cases keep the old compositor. Independent point
comparisons cover 256 canvas fixtures, dirty repeated reuse and 13 rejected
contracts. The source-idle wait also sleeps only until the remaining 60-Hz
source deadline, rather than an unrelated LCD VBlank. Rational partial-phase
tests retain source cadence and bound each sleep to at most 16,666,667 ns.
An appended `presentations` CSV counter follows successful whole-frame returns
(including visible-frame reuse), not loops, source phases, stereo eyes or
physical GPU retirement. It occupies prior padding, retaining the 512-KiB
window buffer and shutdown-only SD writes. All 52 CI tests and the
ARM/shader/stack/package gates pass; eight 240-phase stage probes keep their
prior image digests and 80 audio blocks. In the same accuracy-enabled
Vulkan/300%-clock replay, however, canvas work increases to 2.069/2.073 ms
in backgrounds 3/51 from 1.864/1.693 ms with the previous compositor.
The fused canvas and its now-unused API are reverted; the source deadline
wait and presentation counter remain for isolated follow-up validation.
The combined experiment averages 57.161/55.847 successful presentations per
second over 16/29 adjacent same-label windows, with minimum one-second-window
rates of 48.843/48.914. Inclusive loop means of 13.635/13.729 ms are not FPS.
These successful returns include visible-frame reuse, not physical GPU
retirement. Legacy CSVs lack this counter, so no directly measured success-rate
delta is claimed. Planned DSP shutdown/profile flush occur without retry,
fallback or replay errors; guest GSP retirement remains unverified, and the
owned emulator was stopped after the flush with CPU configuration restored to
100%. The rollback restores the three canvas files exactly to their verified
pre-experiment source. Sustained mono 60 FPS remains open.

The isolated rollback (`1e80339f`, ARM run 38449713288) passes the 52 host
checks plus ARM/shader/stack/package gates. The same accuracy-enabled mono
replay restores canvas means to 1.866/1.694 ms and finish means to
2.859/2.825 ms in backgrounds 3/51. Successful whole-frame returns average
57.519/56.076 per second across 16/29 same-label windows, with minima
48.845/48.753. Source logic overruns and GPU-effects windows remain below
target; inclusive loop means of 13.602/13.407 ms are not FPS. No fallback,
retry or replay errors occur. Planned DSP shutdown and buffered profile flush
occur before the owned emulator is stopped; guest GSP retirement is still
unverified, CPU configuration is restored to 100%, and no assets/saves are
copied or erased. The same sensitivity and concurrent-load limitations apply.

The next source-only experiment gives source-clock-admitted GPU frames
explicit pacing ownership instead of waiting for a second LCD deadline.
Unclocked probes, quick menus and editors retain LCD pacing. Switching back
to LCD ownership rearms its lease. The blocking `C3D_FrameBegin(0)` still
retires every preceding GPU queue before uploads. Citro3D's
[render queue implementation](https://github.com/devkitPro/citro3d/blob/master/source/renderqueue.c)
separates these display and retirement waits. Pacing tests cover 100,000
source-clock counter/wrap cases and ownership handoffs; the three targeted
PICA/frontend/profile checks pass. The source (`5cda2750`, ARM run
38452274142) also passes all 52 CI checks and native gates. In the same
accuracy-enabled mono replay, GPU pacing work drops from 3.833/2.146 ms
to below 0.001 ms in backgrounds 3/51; blocking acquisition remains at
0.001 ms. Successful presentation averages are 57.514/57.065 per second,
with minimum same-label window rates of 48.982/50.813, not stable 60.
The first segment is essentially unchanged; the second was 56.076 in the
isolated rollback. Canvas/overlay/raster costs remain unchanged. No retry,
fallback or replay errors occur, and planned DSP shutdown/profile flush
precede the owned emulator stop. CPU configuration returns to 100%; guest
GSP retirement and physical New-model performance remain unverified. The
next investigation was the upstream runtime's exact object-bridge snapshots
and list reuse.

The exact object-bridge cache (`98adacfda8`, ARM run 38457573595) passes
the 52 CI checks and native gates. An independent bytewise oracle retains
complete pool/CPU state through Original/EX mutations, restores, noncanonical
pointers, stale extended mirrors and I/O ordering. Seven real full-state/audio
checks and both full Original/EX session regressions also pass (501.23/644.25
seconds for those broad sessions). Eight real stage probes retain their prior
240-phase pixel digests and 80 complete audio blocks.

Its accuracy-enabled Vulkan/300%-clock replay averages 57.576/57.198
successful whole-frame returns per second in backgrounds 3/51, with minimum
same-label window rates of 48.993/49.873. Logic means are 10.486/12.171 ms;
strategy means are 3.565/3.954 ms. These are small changes from the previous
cache-free bridge, not stable mono 60 or physical-console results. Planned DSP
shutdown and profile flush occur before the owned host is stopped; guest GPU
retirement remains unverified and CPU configuration returns to 100%. The
next investigation is the reference's consecutive strategy-call batching.

The consecutive-call follow-up (`6188962393`, ARM run 38461773934) reuses
only canonical, complete imports inside the strategy scheduler. Native writes,
host/list changes, pointer/link repair, failures, I/O and restore retain the
defensive bridge. The bytewise oracle and 128 actual interpreter calls per
layout pass. In 64 sampled real strategy ticks across eight Original/EX stages,
964 imported-image reuses retain complete CPU/VM/pool states, source instruction
counts and run/removal counts versus forced full synchronization. All prior
240-phase image digests and 80 audio blocks match. Seven real state/audio checks,
all 52 CI tests and the ARM/shader/stack/package gates pass.

The same accuracy-enabled Vulkan/300%-clock replay records strategy means of
2.705/3.174 ms in backgrounds 3/51, versus 3.565/3.954 for the previous cache.
Logic means are 9.740/11.453 ms. Successful whole-frame return averages are
58.027/58.119 per second, with minimum same-label window rates of 49.024/52.539.
Counts/poses and windows differ; unrelated concurrent work is retained. These
are sensitivity results, not sustained physical New-model 60 FPS. No retry,
fallback or replay errors occur. Planned DSP shutdown/profile flush precede
the verified owned-host stop; guest GSP retirement remains unverified and CPU
configuration returns to 100%. The next measured work is the repeated palette,
sprite and related general-routine synchronization. No incremental hardware
request or release is implied.

The transfer-chain follow-up also borrows canonical imported objects for the
consecutive palette, EX colour-cycle, sprite and window-wipe routines after
DOSOUNDS. It does not skip source routines, palette cadence, native imports or
audio acknowledgements. Independent bytewise tests cover general calls and
failed-call recovery. Eight real Original/EX stage probes match the defensive
path after all 628 sampled named transfer calls, exercising 564 image reuses;
complete CPU/VM/pool states, returned registers and instruction counts match.
The prior 240-phase pixel digests and 80 complete audio blocks remain intact.
The follow-up (`64fd561b87`, ARM run 38464851895) passes all 52 CI tests and
native ARM/shader/stack/package checks. Seven actual Original/EX state and
serial/parallel complete-audio pipeline checks also pass locally (173.97 s,
with the eight stage probes running concurrently).

The exact image's accuracy-enabled Vulkan/300%-clock replay records logic
means of 9.122/10.982 ms in backgrounds 3/51. Successful whole-frame returns
average 58.260/58.502 per second, with minimum adjacent same-label window
rates of 49.057/52.530. Poses/counts and windows differ and unrelated concurrent
work is retained. This is a small measured gain, not stable physical New-model
60 FPS. The 179/245-ms CPU-call maxima occur in scene-boundary windows, excluded
from those adjacent rates; they do not explain the ordinary slow windows.
Those ordinary windows still show source advancement, model/layer work and
audio on the frame path. Investigate that scheduling/critical path next rather
than treating a transition maximum as steady gameplay cost. Parent timings
include children and must not be summed as independent frame costs.

No retry/fallback/replay errors occur. Planned DSP shutdown/profile flush
precede the verified forced owned-host stop; guest GSP retirement is still
unverified. CPU configuration returns to 100%. This does not request an
incremental hardware test or claim completed mono acceptance.

The earlier direct-overlay candidate removes the EX BG1 staging image and applies the same
zero-ink/CGRAM-black transparency directly during decoding. The 57,344-byte
pixel allocation and full-image rescan are gone. All 384 synthetic comparisons
match the removed path's pixels, coverage and tags, and both Original/EX
240-phase source checks retain their prior image digests and 80 audio blocks.
Display retirement now detaches linked outputs to join the GPU command queue
before Citro2D frees vertex/index buffers; `C3D_FrameSync` only waits for VBlank.
The shared retirement helper passes 700 cold/partial/paired/busy-state cycles
plus idempotent closes. All 51 asset-free host CI tests and native ARM,
shader, stack and package gates pass. These host checks do not establish
physical GPU lifetime or full audio-suite acceptance for this commit.

The same accuracy-enabled Vulkan/300% sensitivity replay reports EX background-3
mono finishing at 5.417 ms/call versus 5.934 for `61acaae1`, with inclusive
loop means of 22.296 versus 23.169 ms. Background-51 finishing is 5.151 versus
5.673 ms/call. No fallback or retry occurs. Counts/poses and window-end labels
differ, concurrent unrelated work remains, and these durations are not completed
presentation FPS. Planned DSP shutdown and profile flush occur without replay
errors. Guest GSP exit remains unverified; the owned emulator was stopped after
the flush and its configuration restored to 100%. Model scan conversion and
final composition remain the next measured hot paths. The mono 60 FPS gate is
still open. No incremental hardware request or release is implied.

CI dispatch now accepts `profile_frames=false` for production-style timing.
`BUILD-INFO.json` records the explicit native profiling setting; older packages
without that field have an unspecified setting. With profiling disabled,
there is no frame/startup CSV instrumentation, not an empty performance result.
With it enabled, exit normally before copying the buffered frame-profile CSV.
The diagnostics-free mono baseline is source
`6974f23f8d7a37e6122932f453c84b3457764227`, from
[ARM run 38336169249](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38336169249).
Its package explicitly records `frame_profiling: false`. The accuracy-enabled
Azahar run, with both stereo settings at zero, reaches the real pre-game menu,
Original intro/title/Controls and Training without an out-of-bounds exception.
The completed-frame overlay remains below 60 FPS; this is not performance
acceptance. Azahar also reports the New 3DS CPU configuration service as
unimplemented, so its timing is not a physical New 3DS clock measurement.
No further incremental hardware test is requested for this baseline.
The mono performance gate is still open. Emulator speed is not physical New
3DS speed; no incremental tester request is implied by these source changes.

### Mono landscape work reduction

With the slider at zero, GameLayers now submits the original camera-projected
BG2 artwork once instead of also constructing/drawing its duplicate finite
stereo receiver. It retains the direct atlas and raster fallback, source HDMA,
opacity, colour-math layer ownership and LCD edge clamping. Isolated BG2 needs
no resident A8 ownership copy. Results-screen BG2/OBJ painter groups remain in
their authored order. Nonzero-slider receiver geometry is unchanged.
Independent host checks compare all 400x240 pixels in 27 rolled/unique-sky,
atlas/raster/budget cases, plus black opacity, lights, gate bases and results
sprite priorities. These are correctness checks, not measured FPS gains;
the diagnostics-free ARM/emulator performance check remains required.

### Earlier R412 source candidate

The earlier source candidate (R412) is commit
`e98788bef9dbfbca46cfbe0506b02a356b6a3388`, from
[ARM run 38206689640](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38206689640).
The asset-free ZIP is 1,133,475 bytes, SHA256
`406e3ca195c1edeadde6a476e80347ec21aaea479a4da0e0b40e53fbbd4e607c`.
It retains the R376 shader correction and later ground-depth fixes. Its latest
changes retain the exact-byte RGBA tile expansion and reduce redundant transform
calculations/uploads for adjacent equal model draws within each fixed eye/frame
scope. Every source face, draw, material and clip remains; both eyes, blocking
GPU retirement and cartridge/audio cadence are preserved. The CI run passes
48 asset-free host tests plus the full ARM/shader/stack checks. The new transform
regression checks 1,176 baseline-equality, scope and failure-repair assertions;
these and the earlier upload timings are not console FPS measurements.
Original Preview ON/OFF/ON, ordinary Preview-OFF Start, source intro/Controls,
Training and live steering/roll render in the reused accuracy-enabled New-model
emulator. Select+Start retires the guest, then the same monitored process closes
normally with exit code 0, without a forced stop or restart. Training's sampled
views retain ground lights, visible model bases, both eyes and the lower HUD.
Live logs confirm Vulkan, accurate GPU timing/math, memory exceptions and slider
100. The emulator does not implement the New CPU configuration request, and
HLE DSP is not physical-console audio acceptance. This is not measured New-console
performance or full-port acceptance; development remains emulator-led, without
a request for another minor hardware test. Full current cartridge regression
and sustained New-console performance remain open. R376 remains the only
hardware-tested baseline; R412 is not advertised as hardware-verified.

## Hardware-tested baseline R376

The last hardware-tested candidate is `build/StarFoxEnhanced-3ds-test-r376.zip`
(1,127,314 bytes), source `454e8fb8839d021c7482e41cfe8b224ccba84947`.
ZIP SHA256: `385b9c99de7ed9694c0ffa77828f548a1dfef776bd42c0fc5a6c43fb3fb815d2`.
Its [source-only ARM run 38154475650](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38154475650)
passes all 47 host tests, native compilation, compiled shader output, stack and
package gates. Independent local checks verify all ZIP members, CRCs, native
ELF/3DSX/SMDH and the identical local/CI compiled shader.

The exact native binary passes default Original A-confirmed Start, a minute
of rendering and normal exit in strict Azahar 2126.2. Physical R375 froze at
the second-frame GPU queue wait; the owner now confirms R376 works on New
3DS XL, including sound, the slider and HOME/exit. Ground occlusion and low
presentation FPS still need correction; sleep/resume and long-run stability
are not established by that report.
The same R376 binary also passes EX Preview ON/OFF/ON, ordinary Start,
a minute of rendering and normal guest/host shutdown with exit code 0.
The original-model emulator configuration also passes fresh default
A-confirmed Start, a minute of rendering and normal guest/host exit code 0.
Original-console performance and HOME/sleep remain unverified.

## Earlier R373 validation

The earlier follow-up is `build/StarFoxEnhanced-3ds-test-r373.zip`
(1,125,193 bytes), from source `f5876fc1bfd153a044ea0232c6c9350f2c3b0a33`.
ZIP SHA256: `0adbdd501fe58379972f5e607454741bb57c7eb4b6eebe0545885f680d8d9116`.
Its [source-only ARM run 38148953720](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38148953720)
passes all 46 host tests, native ARM, stack and package gates. Independent
download checks verify the five asset-free members and native 3DSX/SMDH hashes.

Azahar 2126.2 with accurate GPU timings, accurate shader multiplication,
synchronous shader/presentation work and memory exceptions found a null read
inside Citro3D's shader rebinding after Preview. R373 keeps the previous CPU
shader alive until rebinding completes, while still freeing its GPU targets,
textures and vertex buffer. The strict native-binary retest passed Preview
ON/OFF/ON, Start, a minute of post-menu PICA/audio work and normal exit.
This does not establish real-console stability or FPS, or prove that the
tester's initial Preview stall has the same cause.

For equivalent emulator testing, use Azahar 2126.2 or newer; 2125.1.2 ignores
`simulate_3ds_gpu_timings`. Enable `simulate_3ds_gpu_timings` and
`shaders_accurate_mul` under Renderer, and `enable_exception_handler` under
Debugging. Set each corresponding `\default=false` when editing INI directly.
Disable asynchronous shaders/presentation and use New 3DS at 100% CPU clock.
Verify the enabled values in the actual boot log. Unmapped-memory exceptions
do not detect every buffer overrun within otherwise valid guest RAM.

R180 freezes on a tester's real New 3DS XL while displaying the loading image.
The supplied CSV shows loading finished and one GPU presentation returned;
it does not prove GPU completion or identify the precise subsequent stall.
R371/R373 use direct LCD presentation for Preview-OFF menus and initialize PICA
only for a real preview, game or editor. It adds `native-startup-*.csv` phase
receipts. R373 rearms a bounded trace window for each source replacement,
reserves high-level receipts during audio preroll, and separates display-sync
from GPU-queue waits. Preview is session-only and always starts OFF, including
with older saved Preview-ON settings. If startup still freezes, send the
newest startup CSV and `native-frame-profile-*.csv`, along with where it stopped.
Retain existing data and old diagnostics. The ZIP's README contains earlier
history; use `BUILD-INFO.json` and the R373 companion instructions for this build.

## Earlier R180 validation and physical failure

The earlier tester package is `build/StarFoxEnhanced-3ds-test-r180.zip`
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

1. Use a New 3DS, New 3DS XL or New 2DS XL with an existing homebrew setup
   and Homebrew Launcher. Original 3DS/XL/2DS are unsupported; their retained
   legacy mono fallback is not a supported performance target.
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
  game twice. Slider zero skips the right eye. New 2DS XL always uses one
  mono eye. Begin testing at modest separation.
- Select + Y opens the native quick menu (resume/options/save/load).
- Select + Start exits. Home/sleep should suspend and resume safely.
- Pre-game Options includes controller remapping and **CUSTOMIZE SCREEN** for
  the lower HUD. Hold the mapped in-game L+R in setup for five seconds to reset
  settings; cartridge saves are retained.

## New-family acceptance checks

Development remains emulator-led for now; this checklist is for a consolidated
hardware candidate, not a request to test every incremental build.

Please report model, build commit from `BUILD-INFO.json`, Original/EX,
stage/scene, model, render FPS setting and stereo separation/convergence.
On New 3DS/XL compare slider 0, halfway and full. On New 2DS XL verify mono
gameplay. Check Home-menu return, lid-close/wake and clean exit on supported
models. Old-model performance and slider qualification are no longer required.

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
