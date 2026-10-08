# GPU batching baseline — October 1, 2026

## Purpose

Phase 0 of the GPU FAST work: frame-work numbers and scene work counters
for the existing GPU renderer (to be renamed GPU ACCURATE) and the Software
renderer, recorded before any optimisation. Later phases compare against
these tables on the same machine and settings.

## Method

Commit `5eacb34` (upstream `main` `e88c2ad` plus the scene counters and the
widened benchmark script). Windows 11, AMD Ryzen 7 9800X3D, 32 GB, NVIDIA
GeForce RTX 4080 (driver 32.0.16.1088), llvm-mingw Release build. The SDL GPU
device used its default high-performance adapter preference. The adapter name
is not logged, so the RTX 4080 is expected but not proven by these logs.

`tools/benchmark_native_defaults.ps1`, defaults otherwise: hidden window,
unpaced, 60 Hz presentation target, original source timing, 1,000 preroll
ticks, 360 presentations with the first 60 excluded from distributions, every
enhancement off, no captures, VSync off. Frame work is CPU time per
presented frame excluding pacing; it is not a GPU-time or physical-display
frame-rate claim. One run per configuration, so single-run noise applies,
especially to p99.

```powershell
tools/benchmark_native_defaults.ps1 -Experiences ORIGINAL,EX -Levels LEVEL1_1,LEVEL1_2,LEVEL2_3 `
  -Renderers SOFTWARE,GPU -GpuDriver direct3d12,vulkan -RenderScale 1,4 -DisplayMode 16_9,32_9 `
  -OutputDirectory tmp/baseline-oct2026/timing
tools/benchmark_native_defaults.ps1 -Experiences ORIGINAL,EX -Levels LEVEL1_1,LEVEL1_2,LEVEL2_3 `
  -Renderers GPU -GpuDriver direct3d12,vulkan -RenderScale 1,4 -DisplayMode 16_9,32_9 `
  -Frames 180 -TraceSceneCost -OutputDirectory tmp/baseline-oct2026/counters
```

Counter runs are separate from timing runs because tracing writes a line per
frame. Raw logs and `summary.csv` files: `tmp/baseline-oct2026/{timing,counters}`.

**Scene caveat:** after 1,000 preroll ticks, ORIGINAL LEVEL2_3's measured
window is a cutscene (`hud=0`, cut counters `1/1/1` in `render-profile-us`).
Every other stage is gameplay (`hud=360`). Upstream's earlier LEVEL2_3
measurements use the same window, so it is kept for comparability.

## Frame work: Direct3D 12

| Experience | Stage | Scale | Display | GPU median / p95 / p99 (ms) | Software median / p95 / p99 (ms) | GPU ÷ Software |
| --- | --- | ---: | --- | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 1× | 16:9 | 1.68 / 2.46 / 4.03 | 2.35 / 4.15 / 4.29 | 0.71 |
| ORIGINAL | LEVEL1_1 | 1× | 32:9 | 1.73 / 2.62 / 4.21 | 2.27 / 4.83 / 5.27 | 0.76 |
| ORIGINAL | LEVEL1_1 | 4× | 16:9 | 3.87 / 4.88 / 7.18 | 4.71 / 7.28 / 9.39 | 0.82 |
| ORIGINAL | LEVEL1_1 | 4× | 32:9 | 5.43 / 6.86 / 9.82 | 6.43 / 12.96 / 15.90 | 0.84 |
| ORIGINAL | LEVEL1_2 | 1× | 16:9 | 1.00 / 1.88 / 7.78 | 2.32 / 4.25 / 5.38 | 0.43 |
| ORIGINAL | LEVEL1_2 | 1× | 32:9 | 0.97 / 1.97 / 8.77 | 2.18 / 4.82 / 5.10 | 0.44 |
| ORIGINAL | LEVEL1_2 | 4× | 16:9 | 2.90 / 4.05 / 7.64 | 4.17 / 6.52 / 6.81 | 0.70 |
| ORIGINAL | LEVEL1_2 | 4× | 32:9 | 5.04 / 6.14 / 10.33 | 5.29 / 8.82 / 9.14 | 0.95 |
| ORIGINAL | LEVEL2_3 (cutscene) | 1× | 16:9 | 0.63 / 1.04 / 1.35 | 3.48 / 3.86 / 4.25 | 0.18 |
| ORIGINAL | LEVEL2_3 (cutscene) | 1× | 32:9 | 0.72 / 1.16 / 1.64 | 4.75 / 5.34 / 6.60 | 0.15 |
| ORIGINAL | LEVEL2_3 (cutscene) | 4× | 16:9 | 3.53 / 4.00 / 5.40 | 5.82 / 6.25 / 6.73 | 0.61 |
| ORIGINAL | LEVEL2_3 (cutscene) | 4× | 32:9 | 5.56 / 6.26 / 8.50 | 8.71 / 9.28 / 9.95 | 0.64 |
| EX | LEVEL1_1 | 1× | 16:9 | 2.03 / 3.26 / 7.45 | 2.72 / 4.88 / 5.12 | 0.75 |
| EX | LEVEL1_1 | 1× | 32:9 | 2.13 / 3.19 / 7.82 | 2.61 / 5.57 / 5.78 | 0.81 |
| EX | LEVEL1_1 | 4× | 16:9 | 4.71 / 6.01 / 9.57 | 6.53 / 9.26 / 10.27 | 0.72 |
| EX | LEVEL1_1 | 4× | 32:9 | 6.18 / 7.44 / 12.95 | 8.21 / 12.67 / 13.60 | 0.75 |
| EX | LEVEL1_2 | 1× | 16:9 | 1.39 / 2.26 / 2.97 | 2.63 / 4.71 / 4.91 | 0.53 |
| EX | LEVEL1_2 | 1× | 32:9 | 1.42 / 2.37 / 3.01 | 2.49 / 5.40 / 5.79 | 0.57 |
| EX | LEVEL1_2 | 4× | 16:9 | 3.19 / 4.72 / 5.46 | 5.99 / 8.82 / 10.51 | 0.53 |
| EX | LEVEL1_2 | 4× | 32:9 | 5.22 / 6.81 / 7.30 | 7.16 / 12.04 / 15.60 | 0.73 |
| EX | LEVEL2_3 | 1× | 16:9 | 2.22 / 3.04 / 4.61 | 2.67 / 5.02 / 5.50 | 0.83 |
| EX | LEVEL2_3 | 1× | 32:9 | 2.20 / 3.21 / 5.18 | 2.57 / 5.67 / 6.08 | 0.86 |
| EX | LEVEL2_3 | 4× | 16:9 | 5.07 / 5.74 / 8.45 | 6.31 / 9.16 / 9.88 | 0.80 |
| EX | LEVEL2_3 | 4× | 32:9 | 6.45 / 7.59 / 12.40 | 7.66 / 11.66 / 13.58 | 0.84 |

## Frame work: Vulkan

| Experience | Stage | Scale | Display | GPU median / p95 / p99 (ms) | Software median / p95 / p99 (ms) | GPU ÷ Software |
| --- | --- | ---: | --- | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 1× | 16:9 | 2.58 / 3.35 / 3.83 | 2.35 / 4.19 / 4.34 | 1.10 |
| ORIGINAL | LEVEL1_1 | 1× | 32:9 | 2.62 / 3.28 / 3.56 | 2.24 / 4.87 / 5.17 | 1.17 |
| ORIGINAL | LEVEL1_1 | 4× | 16:9 | 6.01 / 7.17 / 7.76 | 4.74 / 7.22 / 10.30 | 1.27 |
| ORIGINAL | LEVEL1_1 | 4× | 32:9 | 7.29 / 8.34 / 8.64 | 6.35 / 11.97 / 15.37 | 1.15 |
| ORIGINAL | LEVEL1_2 | 1× | 16:9 | 1.33 / 1.80 / 2.22 | 2.32 / 4.10 / 4.39 | 0.58 |
| ORIGINAL | LEVEL1_2 | 1× | 32:9 | 1.38 / 1.83 / 2.32 | 2.17 / 4.85 / 5.22 | 0.64 |
| ORIGINAL | LEVEL1_2 | 4× | 16:9 | 2.80 / 3.88 / 4.37 | 4.19 / 6.44 / 6.75 | 0.67 |
| ORIGINAL | LEVEL1_2 | 4× | 32:9 | 4.92 / 6.00 / 6.44 | 5.28 / 8.73 / 9.12 | 0.93 |
| ORIGINAL | LEVEL2_3 (cutscene) | 1× | 16:9 | 0.89 / 1.04 / 1.54 | 3.47 / 4.00 / 4.86 | 0.26 |
| ORIGINAL | LEVEL2_3 (cutscene) | 1× | 32:9 | 0.91 / 1.10 / 1.53 | 4.72 / 5.26 / 5.51 | 0.19 |
| ORIGINAL | LEVEL2_3 (cutscene) | 4× | 16:9 | 3.05 / 3.48 / 4.99 | 5.81 / 6.26 / 6.62 | 0.52 |
| ORIGINAL | LEVEL2_3 (cutscene) | 4× | 32:9 | 5.45 / 6.14 / 8.15 | 8.69 / 9.25 / 9.42 | 0.63 |
| EX | LEVEL1_1 | 1× | 16:9 | 3.09 / 4.41 / 4.60 | 2.71 / 4.87 / 5.17 | 1.14 |
| EX | LEVEL1_1 | 1× | 32:9 | 3.15 / 4.35 / 4.77 | 2.61 / 5.60 / 5.90 | 1.21 |
| EX | LEVEL1_1 | 4× | 16:9 | 7.55 / 9.56 / 9.87 | 6.57 / 9.20 / 10.11 | 1.15 |
| EX | LEVEL1_1 | 4× | 32:9 | 8.81 / 10.70 / 11.12 | 8.19 / 12.41 / 13.77 | 1.08 |
| EX | LEVEL1_2 | 1× | 16:9 | 2.14 / 2.59 / 2.87 | 2.63 / 4.73 / 4.96 | 0.81 |
| EX | LEVEL1_2 | 1× | 32:9 | 2.20 / 2.59 / 2.82 | 2.49 / 5.39 / 5.80 | 0.88 |
| EX | LEVEL1_2 | 4× | 16:9 | 4.95 / 5.47 / 5.91 | 5.99 / 8.74 / 10.89 | 0.83 |
| EX | LEVEL1_2 | 4× | 32:9 | 6.20 / 6.80 / 7.11 | 7.14 / 12.05 / 15.40 | 0.87 |
| EX | LEVEL2_3 | 1× | 16:9 | 3.43 / 4.08 / 4.29 | 2.67 / 5.04 / 5.46 | 1.28 |
| EX | LEVEL2_3 | 1× | 32:9 | 3.43 / 4.32 / 4.67 | 2.57 / 5.71 / 6.28 | 1.34 |
| EX | LEVEL2_3 | 4× | 16:9 | 8.19 / 9.09 / 9.36 | 6.28 / 9.06 / 9.91 | 1.30 |
| EX | LEVEL2_3 | 4× | 32:9 | 9.48 / 10.65 / 11.02 | 7.65 / 11.66 / 13.55 | 1.24 |

## Scene work counters (GPU renderer)

Median per frame over 120 measured frames, p95 in brackets. Model, pass,
dispatch and command counts were identical on Direct3D 12 and Vulkan and at
every scale and display mode; only upload bytes changed, so one row per
stage is shown.

| Experience | Stage | Models | Compute passes | Full-frame dispatches | Raster commands | Upload KiB 1× 16:9 / 1× 32:9 / 4× 16:9 / 4× 32:9 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 18 [21] | 120 [137] | 25 [28] | 11 | 329 / 479 / 2579 / 4979 |
| ORIGINAL | LEVEL1_2 | 18 [23] | 58 [73] | 23 [28] | 15 | 298 / 448 / 2548 / 4948 |
| ORIGINAL | LEVEL2_3 | 6 [6] | 38 [38] | 14 [14] | 0 | 277 / 277 / 277 / 277 |
| EX | LEVEL1_1 | 22 [25] | 146 [161] | 29 [31] | 13 | 425 / 575 / 2675 / 5075 |
| EX | LEVEL1_2 | 18 [19] | 99 [105] | 26 [28] | 52 | 330 / 480 / 2580 / 4980 |
| EX | LEVEL2_3 | 27 [29] | 173 [187] | 34 [36] | 308 | 397 / 547 / 2647 / 5047 |

Definitions (`include/starfox/render/gpu_scene_counters.hpp`): *models* are
`GpuModelDraw` entries; *compute passes* are passes begun by the scene
encoders; *full-frame dispatches* are raster or painter-merge passes
dispatched over the whole output; *raster commands* are CPU-recorded
`RasterCommand`s; *upload* is CPU→GPU bytes copied by the scene encoders.

## Findings

1. **At 4× the GPU renderer barely beats Software, and on Vulkan it is
   often slower.** Direct3D 12 GPU/Software median ratios rise from
   0.15–0.86 at 1× to 0.53–0.95 at 4×. On Vulkan, LEVEL1_1 and EX LEVEL2_3
   are 1.08–1.34× *slower* than Software at both scales.
2. **Full-frame work scales with model count, not layer count.** Gameplay
   stages run 23–34 full-frame dispatches per frame for 18–27 models: about
   one per model plus the layers. This is the target of Phase 1B.
3. **Upload grows with output pixels: about 1.7 bytes per output pixel.**
   ORIGINAL LEVEL1_2 goes from 298 KiB per frame at 1× 16:9 to 4,948 KiB
   at 4× 32:9. Temporary traces (not committed) attributed 4,865 KiB/frame
   of ORIGINAL LEVEL1_2 at 4× 32:9 (Direct3D 12) to the texel payload of the
   scene's recorded raster layer. Its commands are 1.4 KB and already use
   GPU binning. Almost all of the payload is one snapshot from
   `composite_transparent_layer`: the 800×192 `superfx_hud` layer (cockpit
   HUD lines, comms face, meters) is cleared and drawn on the CPU at stored
   resolution (3200×768), then uploaded as pixels plus layer tags (2.4 MB
   each) every gameplay frame. The remaining 64 KiB is a VRAM snapshot. The
   ORIGINAL LEVEL2_3 cutscene has no gameplay HUD, and its upload stays at
   277 KiB at every scale.

   *Correction:* the first version of this finding blamed per-frame CPU
   binning (`RasterCommands::bin_rows()`). That was wrong. Scene raster
   draws are created with `gpu_binning` set (`GpuSceneRecording::flush`).

4. **Per-model mesh packing is small in the asteroid field.** In the same
   trace, all of LEVEL1_2's ~18 uploads per frame came from the whole-object
   billboard texture upload (`gpu_model.cpp`, ~18 KiB per frame in total).
   None came from the mesh packing upload in `GpuModel::enqueue`. Phase
   1A's static-data cache will matter more on mesh-heavy stages such as LEVEL1_1 and EX LEVEL2_3, which show the
   highest compute-pass counts (120–173 per frame).

## Limits

- One machine, one run per configuration, unpaced CPU frame work. No GPU
  timestamps, so GPU-bound cost is only visible indirectly.
- 1× and 4× only; 2×/3× and 4:3 were not measured.
- No ray tracing, DLSS/FSR, enhanced terrain or other enhancements.
- The 3D asteroid option (PR #11) is not on this branch, so
  `-AsteroidModels` was not exercised.
- Android, Steam Deck, VR and low-end GPUs were not measured.
