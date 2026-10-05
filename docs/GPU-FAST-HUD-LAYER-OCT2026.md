# GPU FAST: Super FX HUD layer — October 2, 2026

## Change

First GPU FAST change (commits `1f96ac9`, `1b5b10e`). GPU ACCURATE is untouched.

- **Record `superfx_hud`.** This 800×192 layer holds the cockpit HUD lines,
  the comms face and the meters. It was the one gameplay host layer still
  drawn into a CPU image. Every frame it was cleared, drawn at stored
  resolution and uploaded whole as pixels plus layer tags: 4.9 MB at 4× 32:9
  (3200×768). Under GPU FAST it records host-ink raster commands, like
  `superfx_ui` and `comms_hud` already do, and composites as a GPU indexed
  layer. A/B switch: `STARFOX_TEST_UNRECORDED_SUPERFX_HUD=1`.
- **Skip empty host-ink layers.** In flows without a GPU destination layer
  (for example the ORIGINAL LEVEL2_3 cutscene), an empty recording was
  replayed into a fresh stored-resolution CPU image every frame. That made
  the first version of this change 16–22% *slower* than GPU ACCURATE in
  that cutscene at 4× (`tmp/hud-fast/timing`). An empty recording is a fully
  transparent layer, so GPU FAST skips it. A/B switch:
  `STARFOX_TEST_KEEP_EMPTY_HOST_INK=1`.

## Correctness evidence

- `tools/check_gpu_stage_sweep.ps1 -GpuRenderer FAST -RequireNoCpuUpload`
  passes for every Original and EX stage (59) at 1× and 4×, 16:9, on
  Direct3D 12: 236 exact native and final comparisons against the Software
  reference (`tmp/hud-fast2-d3d12-16_9-all.log`). LEVEL1_1/1_2/2_3 for both
  experiences at 1× and 4× also pass on Direct3D 12 32:9 and Vulkan
  16:9/32:9 (`tmp/hud-fast2-{direct3d12-32_9,vulkan-16_9,vulkan-32_9}.log`).
- `tools/check_gpu_fast_ab.ps1`: 48 GPU ACCURATE/GPU FAST pairs (both
  experiences, three stages, 1×/4×, 16:9/32:9, both drivers) have
  byte-identical native and final captures (`tmp/hud-fast2-ab-*.log`).
- ctest: 68/69 when run serially. The failure is the known upstream
  `starfox_dialogue_catalog_tests`.

**Not covered:** scripted input never reached cockpit view (Select presses
at several frames did not switch it), so the cockpit HUD lines under GPU
FAST have no automated check yet. They use the same recorded line path as
the other host-ink layers, but a manual check in cockpit view is still
needed.

## Matched runtime measurements

Method, machine and settings as in `GPU-BATCHING-BASELINE-OCT2026.md`
(RTX 4080 expected, 9800X3D, unpaced CPU frame work, 360 presentations, 60
warm-up frames excluded, enhancements off, one run per configuration). Both
renderers were measured in the same matrix on commit `1b5b10e`. Raw logs:
`tmp/gpu-fast-hud-oct2026/timing`.

Median frame-work change, GPU FAST vs GPU ACCURATE, across the six
stage/experience pairs and both display modes:

| Driver | 1× | 4× |
| --- | --- | --- |
| Direct3D 12 | -5% to +1% | -33% to -11% |
| Vulkan | -4% to +0% | -24% to -8% |

At 1× the HUD layer is small (800×192), so differences there are mostly
run-to-run noise. The benefit grows with stored resolution.

### Direct3D 12

| Experience | Stage | Scale | Display | GPU ACCURATE median / p95 / p99 (ms) | GPU FAST median / p95 / p99 (ms) | Median change |
| --- | --- | ---: | --- | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 1× | 16:9 | 1.70 / 2.38 / 3.92 | 1.71 / 2.33 / 3.81 | +0% |
| ORIGINAL | LEVEL1_1 | 1× | 32:9 | 1.73 / 2.51 / 4.00 | 1.73 / 2.45 / 3.35 | +0% |
| ORIGINAL | LEVEL1_1 | 4× | 16:9 | 3.87 / 4.86 / 7.71 | 3.38 / 4.35 / 5.91 | -13% |
| ORIGINAL | LEVEL1_1 | 4× | 32:9 | 5.55 / 7.00 / 8.33 | 4.18 / 5.87 / 9.57 | -25% |
| ORIGINAL | LEVEL1_2 | 1× | 16:9 | 0.99 / 1.85 / 7.99 | 0.99 / 1.92 / 7.77 | +0% |
| ORIGINAL | LEVEL1_2 | 1× | 32:9 | 0.94 / 2.07 / 8.38 | 0.94 / 1.97 / 8.04 | -0% |
| ORIGINAL | LEVEL1_2 | 4× | 16:9 | 3.17 / 4.48 / 7.73 | 2.73 / 3.86 / 7.44 | -14% |
| ORIGINAL | LEVEL1_2 | 4× | 32:9 | 5.69 / 6.99 / 10.81 | 4.32 / 5.53 / 9.00 | -24% |
| ORIGINAL | LEVEL2_3 (cutscene) | 1× | 16:9 | 0.60 / 1.02 / 1.33 | 0.61 / 0.96 / 1.30 | +1% |
| ORIGINAL | LEVEL2_3 (cutscene) | 1× | 32:9 | 0.69 / 1.17 / 1.65 | 0.65 / 1.15 / 1.49 | -5% |
| ORIGINAL | LEVEL2_3 (cutscene) | 4× | 16:9 | 3.25 / 3.75 / 5.13 | 2.19 / 2.74 / 3.89 | -33% |
| ORIGINAL | LEVEL2_3 (cutscene) | 4× | 32:9 | 5.17 / 5.75 / 8.31 | 3.89 / 4.37 / 6.80 | -25% |
| EX | LEVEL1_1 | 1× | 16:9 | 2.07 / 3.31 / 7.22 | 2.06 / 3.12 / 8.10 | -0% |
| EX | LEVEL1_1 | 1× | 32:9 | 2.15 / 3.13 / 8.09 | 2.09 / 3.15 / 7.92 | -3% |
| EX | LEVEL1_1 | 4× | 16:9 | 4.83 / 6.34 / 10.40 | 4.23 / 5.58 / 9.42 | -12% |
| EX | LEVEL1_1 | 4× | 32:9 | 6.49 / 8.85 / 14.71 | 5.23 / 6.79 / 11.80 | -19% |
| EX | LEVEL1_2 | 1× | 16:9 | 1.41 / 2.29 / 2.79 | 1.42 / 2.32 / 2.86 | +1% |
| EX | LEVEL1_2 | 1× | 32:9 | 1.49 / 2.38 / 2.84 | 1.43 / 2.47 / 3.04 | -4% |
| EX | LEVEL1_2 | 4× | 16:9 | 3.44 / 4.98 / 5.86 | 2.88 / 4.00 / 4.73 | -16% |
| EX | LEVEL1_2 | 4× | 32:9 | 6.04 / 7.88 / 8.81 | 4.17 / 5.86 / 7.00 | -31% |
| EX | LEVEL2_3 | 1× | 16:9 | 2.24 / 3.15 / 4.74 | 2.18 / 3.20 / 4.99 | -3% |
| EX | LEVEL2_3 | 1× | 32:9 | 2.22 / 3.26 / 5.46 | 2.23 / 3.21 / 5.15 | +0% |
| EX | LEVEL2_3 | 4× | 16:9 | 5.09 / 6.00 / 9.30 | 4.52 / 5.19 / 7.82 | -11% |
| EX | LEVEL2_3 | 4× | 32:9 | 6.61 / 8.73 / 13.59 | 5.52 / 6.92 / 11.87 | -17% |

### Vulkan

| Experience | Stage | Scale | Display | GPU ACCURATE median / p95 / p99 (ms) | GPU FAST median / p95 / p99 (ms) | Median change |
| --- | --- | ---: | --- | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 1× | 16:9 | 2.70 / 3.57 / 3.96 | 2.59 / 3.52 / 3.94 | -4% |
| ORIGINAL | LEVEL1_1 | 1× | 32:9 | 2.65 / 3.36 / 3.69 | 2.58 / 3.26 / 3.69 | -2% |
| ORIGINAL | LEVEL1_1 | 4× | 16:9 | 6.12 / 7.32 / 7.90 | 5.50 / 6.72 / 7.34 | -10% |
| ORIGINAL | LEVEL1_1 | 4× | 32:9 | 7.34 / 8.52 / 8.82 | 6.06 / 7.35 / 7.77 | -18% |
| ORIGINAL | LEVEL1_2 | 1× | 16:9 | 1.39 / 1.79 / 2.42 | 1.33 / 1.81 / 2.24 | -4% |
| ORIGINAL | LEVEL1_2 | 1× | 32:9 | 1.39 / 1.75 / 2.58 | 1.37 / 1.81 / 2.50 | -2% |
| ORIGINAL | LEVEL1_2 | 4× | 16:9 | 2.76 / 3.81 / 4.58 | 2.31 / 3.36 / 3.83 | -16% |
| ORIGINAL | LEVEL1_2 | 4× | 32:9 | 5.02 / 6.21 / 6.78 | 3.88 / 5.03 / 5.38 | -23% |
| ORIGINAL | LEVEL2_3 (cutscene) | 1× | 16:9 | 0.90 / 1.09 / 1.31 | 0.90 / 1.11 / 1.41 | +0% |
| ORIGINAL | LEVEL2_3 (cutscene) | 1× | 32:9 | 0.90 / 1.16 / 1.46 | 0.90 / 1.08 / 1.59 | -1% |
| ORIGINAL | LEVEL2_3 (cutscene) | 4× | 16:9 | 2.63 / 3.20 / 4.63 | 1.99 / 2.45 / 3.83 | -24% |
| ORIGINAL | LEVEL2_3 (cutscene) | 4× | 32:9 | 4.49 / 5.41 / 7.70 | 3.60 / 4.61 / 6.35 | -20% |
| EX | LEVEL1_1 | 1× | 16:9 | 3.16 / 4.52 / 4.72 | 3.13 / 4.52 / 4.70 | -1% |
| EX | LEVEL1_1 | 1× | 32:9 | 3.26 / 4.47 / 4.60 | 3.19 / 4.39 / 4.71 | -2% |
| EX | LEVEL1_1 | 4× | 16:9 | 7.70 / 9.70 / 9.90 | 7.08 / 9.11 / 9.38 | -8% |
| EX | LEVEL1_1 | 4× | 32:9 | 9.03 / 10.88 / 11.33 | 7.76 / 9.76 / 9.99 | -14% |
| EX | LEVEL1_2 | 1× | 16:9 | 2.17 / 2.73 / 2.92 | 2.12 / 2.75 / 2.88 | -2% |
| EX | LEVEL1_2 | 1× | 32:9 | 2.23 / 2.60 / 2.89 | 2.14 / 2.57 / 2.79 | -4% |
| EX | LEVEL1_2 | 4× | 16:9 | 4.99 / 5.77 / 6.05 | 4.35 / 5.04 / 5.39 | -13% |
| EX | LEVEL1_2 | 4× | 32:9 | 6.24 / 6.94 / 7.47 | 5.05 / 5.71 / 6.24 | -19% |
| EX | LEVEL2_3 | 1× | 16:9 | 3.51 / 4.19 / 4.46 | 3.44 / 4.07 / 4.29 | -2% |
| EX | LEVEL2_3 | 1× | 32:9 | 3.49 / 4.37 / 4.64 | 3.42 / 4.34 / 4.54 | -2% |
| EX | LEVEL2_3 | 4× | 16:9 | 8.35 / 9.19 / 9.57 | 7.72 / 8.66 / 9.01 | -8% |
| EX | LEVEL2_3 | 4× | 32:9 | 9.60 / 10.81 / 11.65 | 8.33 / 9.61 / 10.22 | -13% |

## Scene counters (Direct3D 12, median per frame)

GPU ACCURATE values are from the baseline; GPU FAST values are from
`tmp/gpu-fast-hud-oct2026/counters`. Pass and dispatch counts are shown at
4× 32:9 (they don't depend on scale or display).

| Experience | Stage | Compute passes | Full-frame dispatches | Upload KiB, 1× 16:9 | Upload KiB, 4× 32:9 |
| --- | --- | ---: | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 120 → 128 | 25 → 27 | 329 → 223 | 4979 → 223 |
| ORIGINAL | LEVEL1_2 | 58 → 66 | 23 → 25 | 298 → 195 | 4948 → 958 |
| ORIGINAL | LEVEL2_3 | 38 → 38 | 14 → 14 | 277 → 277 | 277 → 277 |
| EX | LEVEL1_1 | 146 → 154 | 29 → 31 | 425 → 343 | 5075 → 343 |
| EX | LEVEL1_2 | 99 → 107 | 26 → 28 | 330 → 249 | 4980 → 249 |
| EX | LEVEL2_3 | 173 → 181 | 34 → 36 | 397 → 299 | 5047 → 299 |

The HUD now costs about 8 compute passes and 2 full-frame dispatches on the
GPU, in exchange for no longer uploading the full-resolution image.

## Limits / follow-up

- One machine, single runs, CPU frame work only, no GPU timestamps.
- ORIGINAL LEVEL1_2 still uploads 958 KiB per frame at 4× 32:9 against
  195 KiB at 1× 16:9. Another resolution-dependent upload remains there and
  has not been traced yet.
- Cockpit view is not covered by automated checks (see above).
- Android: the arm64 debug build of `1b5b10e` compiles and passes
  `tools/check_android_package.py --source-root .` (`tmp/android-build-hud.log`).
  That is compile evidence only; no device was run.
