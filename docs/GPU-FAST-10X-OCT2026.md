# GPU FAST: render scales up to 10× (Phase 1E) — October 3, 2026

## Change

Commits `4b8addc`, `87dadf6`, `738f3f1`, `4ab5540` and `2f341e7`, plus the
benchmark window option `c21e8f4`. SOFTWARE and GPU ACCURATE keep 1×–4×.

- RENDER UPSCALE offers 5×–10× under GPU FAST on desktop builds. Mobile,
  console and UWP builds stay at 4×. At 10× a 16:9 frame is stored at
  4000×2240, which covers 3840×2160. A 32:9 frame is stored at 8000×2240,
  which covers 7680×2160.
- The GPU scene path accepts render scales up to `max_gpu_render_scale` (10).
  The background layer's 4096-pixel limit rises to 8192.
- DLSS and FSR are skipped above 4×. They would render below the output
  again, and their motion surfaces stop at 4096 pixels wide.
- `pregame.cfg` keeps 5×–10× only together with GPU FAST. Older builds
  clamp the value to 4× as before, so the settings revision is unchanged.
- The SOFTWARE line width and dither now scale up to 10× instead of 4×.
  This makes the CPU reference match the GPU spans at 5×–10×. Output at
  1×–4× is unchanged.

### The 10× rounding fix (`738f3f1`)

Spans turn each fractional clipped vertex into a stored pixel with
round-half-away(x × scale). At 1×, 2× and 4× the scale is a power of two,
so the float product is exact. At 5×–10× it is not.

Example: SXPWIRESPACEBAR has an endpoint at x = 107.34999646.
SoftwareRenderer computes 107.34999646 × 10 = 1073.49996 in binary64 and
rounds it to 1073. The GPU's float product came out as exactly 1073.5, so
the whole line moved one pixel. EX LEVEL5_5's 12-pixel diagonal edge
difference at 10× was the same error on a polygon vertex.

The fix has three parts:

- `scaled_round.hlsli` adds an exact (Dekker) product and a tie-aware round.
- Above 4×, `spans` rounds the exact product.
- Above 4×, `clip_continuous` narrows each vertex to the neighbouring float
  whose scaled rounding matches the binary64 value. `GpuClip::enqueue`
  receives the render scale and raster size that `enqueue_spans` will use.

Both stages skip the new code at 1×–4×, so output there is unchanged by
construction. The same latent issue exists at 3×. It was left alone to
keep GPU ACCURATE and 1×–4× GPU FAST bit-identical. An earlier suspicion,
that the CPU reference wrapped 16-bit coordinates past 2048 pixels, was
wrong: the CPU line path is 32-bit and binary64 throughout.

## Correctness evidence

All results are exact against the Software reference, with zero CPU-image
uploads. Evidence is in `tmp/1e/gates-summary.log` and `tmp/1e/gates/`.

- `check_gpu_stage_sweep.ps1 -GpuRenderer FAST -RequireNoCpuUpload`. Every
  Original and EX stage (59) passed in each sweep:
  - Direct3D 12, 32:9: 5×, 8× and 10×.
  - Direct3D 12, 16:9: 5×, 8× and 10×.
  - Vulkan, 32:9: 5×, 8× and 10×.
  - Vulkan, 16:9: 10×.
  - Regression sweeps, Direct3D 12 16:9 at 1× and 4×, for both GPU FAST and
    GPU ACCURATE.
- `starfox_gpu_model_check`: 96 runs, all exact. They cover Direct3D 12 and
  Vulkan; Original and EX; the default scales, 5× and 10×; and eight modes:
  default, scene, mixed batch, recorded batch, lines only, axis, alternate,
  and terrain batch.
- ctest: 88 of 89 pass when run serially, at every 1E commit. The failure is
  the known upstream `starfox_dialogue_catalog_tests`.
- One Vulkan 10× fixture run, before the fix, ended in
  `VK_ERROR_DEVICE_LOST` on MOUSEON. Four reruns after the fix did not
  reproduce it, and neither did the full gate run. The cause is unknown.

## Performance

### Method

- **Machine:** RTX 4080, display 7680×2160 at 119 Hz.
- **Benchmark:** `tools/benchmark_native_defaults.ps1` on Original, with 360
  measured frames for gameplay and 600 for the tunnel, after 60 warm-up
  frames. Runs were unpaced, VSync off, all enhancements off, with a hidden
  window.
- **Window size:** each run used `-WindowSize`, so presentation happened at
  the real output size. The log line `test-window-pixels` confirmed
  3840×2160 and 7680×2160.
- **Scenes:** LEVEL1_1's launch tunnel (preroll 0), and gameplay on
  LEVEL1_1, LEVEL1_2 and LEVEL2_3 (preroll 1000).
- **Repeats:** the full matrix ran twice in a row; the tables show the mean
  of the two. Direct3D 12 repeats agree within about 4%, with one exception:
  ACCURATE LEVEL2_3 16:9 differs by 27% at 1.4–1.7 ms. Vulkan repeats differ
  by up to 24% (FAST 8× LEVEL1_2 16:9).
- **VRAM:** `nvidia-smi` total memory used, sampled every 250 ms. The table
  shows peak minus the reading just before the run, after waiting for the
  previous run's memory to drain. The higher of the two repeats is shown.
- **Evidence:** raw logs are in `tmp/1e/bench/`, and the CSV is
  `tmp/1e/bench/summary.csv`.

An earlier matrix (`tmp/1e/bench`, first run, overwritten) overlapped with
another GPU-heavy application and showed false 8–10 GB VRAM peaks and
p99 spikes up to 475 ms. It was discarded.

#### Direct3D 12, 16:9, 3840×2160 window (10× stores 4000×2240)

| Scene | Renderer | Median ms | p95 ms | p99 ms | VRAM over start, MiB |
| --- | --- | ---: | ---: | ---: | ---: |
| LEVEL1_1 launch tunnel | ACCURATE 4× | 9.75 | 11.61 | 11.83 | 943 |
| LEVEL1_1 launch tunnel | FAST 4× | 8.73 | 10.49 | 10.65 | 922 |
| LEVEL1_1 launch tunnel | FAST 8× | 19.45 | 22.46 | 22.81 | 1685 |
| LEVEL1_1 launch tunnel | FAST 10× | 25.96 | 29.56 | 30.18 | 2261 |
| LEVEL1_1 gameplay | ACCURATE 4× | 3.75 | 4.57 | 5.53 | 984 |
| LEVEL1_1 gameplay | FAST 4× | 3.02 | 3.79 | 4.73 | 1035 |
| LEVEL1_1 gameplay | FAST 8× | 10.90 | 11.78 | 12.30 | 1993 |
| LEVEL1_1 gameplay | FAST 10× | 15.25 | 16.24 | 16.77 | 2721 |
| LEVEL1_2 gameplay | ACCURATE 4× | 2.12 | 2.82 | 8.83 | 919 |
| LEVEL1_2 gameplay | FAST 4× | 1.33 | 2.50 | 7.98 | 970 |
| LEVEL1_2 gameplay | FAST 8× | 5.29 | 6.72 | 10.63 | 1469 |
| LEVEL1_2 gameplay | FAST 10× | 8.28 | 9.44 | 13.78 | 2654 |
| LEVEL2_3 gameplay | ACCURATE 4× | 1.56 | 2.16 | 2.90 | 864 |
| LEVEL2_3 gameplay | FAST 4× | 1.05 | 1.38 | 2.19 | 850 |
| LEVEL2_3 gameplay | FAST 8× | 2.22 | 2.62 | 4.70 | 1369 |
| LEVEL2_3 gameplay | FAST 10× | 8.03 | 8.34 | 8.65 | 2166 |

#### Direct3D 12, 32:9, 7680×2160 window (10× stores 8000×2240)

| Scene | Renderer | Median ms | p95 ms | p99 ms | VRAM over start, MiB |
| --- | --- | ---: | ---: | ---: | ---: |
| LEVEL1_1 launch tunnel | ACCURATE 4× | 13.05 | 14.48 | 14.65 | 1193 |
| LEVEL1_1 launch tunnel | FAST 4× | 10.85 | 12.38 | 12.67 | 1172 |
| LEVEL1_1 launch tunnel | FAST 8× | 26.16 | 31.22 | 31.58 | 2692 |
| LEVEL1_1 launch tunnel | FAST 10× | 35.35 | 43.81 | 44.68 | 3841 |
| LEVEL1_1 gameplay | ACCURATE 4× | 4.95 | 5.67 | 6.88 | 1274 |
| LEVEL1_1 gameplay | FAST 4× | 3.38 | 4.17 | 5.25 | 1363 |
| LEVEL1_1 gameplay | FAST 8× | 17.60 | 18.42 | 18.95 | 3238 |
| LEVEL1_1 gameplay | FAST 10× | 26.38 | 27.46 | 28.15 | 4687 |
| LEVEL1_2 gameplay | ACCURATE 4× | 3.12 | 4.02 | 9.17 | 1196 |
| LEVEL1_2 gameplay | FAST 4× | 1.73 | 2.78 | 9.09 | 1303 |
| LEVEL1_2 gameplay | FAST 8× | 9.00 | 10.22 | 15.34 | 3227 |
| LEVEL1_2 gameplay | FAST 10× | 13.72 | 15.03 | 19.12 | 4675 |
| LEVEL2_3 gameplay | ACCURATE 4× | 2.26 | 3.04 | 3.76 | 932 |
| LEVEL2_3 gameplay | FAST 4× | 1.29 | 1.62 | 2.62 | 1158 |
| LEVEL2_3 gameplay | FAST 8× | 15.83 | 16.05 | 16.29 | 2567 |
| LEVEL2_3 gameplay | FAST 10× | 24.35 | 24.67 | 24.88 | 3500 |

#### Vulkan, 16:9, 3840×2160 window (10× stores 4000×2240)

| Scene | Renderer | Median ms | p95 ms | p99 ms | VRAM over start, MiB |
| --- | --- | ---: | ---: | ---: | ---: |
| LEVEL1_1 launch tunnel | ACCURATE 4× | 14.96 | 17.98 | 18.30 | 3803 |
| LEVEL1_1 launch tunnel | FAST 4× | 13.95 | 16.90 | 17.17 | 3805 |
| LEVEL1_1 launch tunnel | FAST 8× | 29.64 | 34.55 | 34.92 | 4827 |
| LEVEL1_1 launch tunnel | FAST 10× | 39.37 | 45.20 | 45.49 | 5851 |
| LEVEL1_1 gameplay | ACCURATE 4× | 6.24 | 7.36 | 8.07 | 3863 |
| LEVEL1_1 gameplay | FAST 4× | 5.52 | 6.60 | 7.33 | 3929 |
| LEVEL1_1 gameplay | FAST 8× | 18.61 | 20.20 | 20.87 | 5209 |
| LEVEL1_1 gameplay | FAST 10× | 26.39 | 28.82 | 30.04 | 6377 |
| LEVEL1_2 gameplay | ACCURATE 4× | 2.79 | 3.61 | 4.28 | 3866 |
| LEVEL1_2 gameplay | FAST 4× | 2.04 | 2.86 | 3.55 | 3932 |
| LEVEL1_2 gameplay | FAST 8× | 5.71 | 7.08 | 7.71 | 4696 |
| LEVEL1_2 gameplay | FAST 10× | 11.91 | 13.86 | 14.29 | 6235 |
| LEVEL2_3 gameplay | ACCURATE 4× | 1.82 | 2.23 | 2.66 | 3861 |
| LEVEL2_3 gameplay | FAST 4× | 1.77 | 2.12 | 2.45 | 3739 |
| LEVEL2_3 gameplay | FAST 8× | 4.16 | 4.67 | 6.00 | 4757 |
| LEVEL2_3 gameplay | FAST 10× | 12.50 | 12.98 | 13.33 | 5676 |

#### Vulkan, 32:9, 7680×2160 window (10× stores 8000×2240)

| Scene | Renderer | Median ms | p95 ms | p99 ms | VRAM over start, MiB |
| --- | --- | ---: | ---: | ---: | ---: |
| LEVEL1_1 launch tunnel | ACCURATE 4× | 19.43 | 22.04 | 22.43 | 4443 |
| LEVEL1_1 launch tunnel | FAST 4× | 17.25 | 19.93 | 20.35 | 4410 |
| LEVEL1_1 launch tunnel | FAST 8× | 40.39 | 45.31 | 46.00 | 6675 |
| LEVEL1_1 launch tunnel | FAST 10× | 53.26 | 61.69 | 63.86 | 7883 |
| LEVEL1_1 gameplay | ACCURATE 4× | 7.97 | 9.05 | 9.59 | 4507 |
| LEVEL1_1 gameplay | FAST 4× | 6.49 | 7.77 | 8.00 | 4637 |
| LEVEL1_1 gameplay | FAST 8× | 30.19 | 31.81 | 32.27 | 7098 |
| LEVEL1_1 gameplay | FAST 10× | 43.83 | 45.63 | 46.72 | 8969 |
| LEVEL1_2 gameplay | ACCURATE 4× | 4.03 | 4.91 | 5.75 | 4441 |
| LEVEL1_2 gameplay | FAST 4× | 2.42 | 3.38 | 4.02 | 4584 |
| LEVEL1_2 gameplay | FAST 8× | 14.27 | 16.60 | 18.33 | 7875 |
| LEVEL1_2 gameplay | FAST 10× | 21.25 | 23.53 | 24.52 | 9083 |
| LEVEL2_3 gameplay | ACCURATE 4× | 2.05 | 2.62 | 3.74 | 4430 |
| LEVEL2_3 gameplay | FAST 4× | 1.93 | 2.25 | 2.79 | 4373 |
| LEVEL2_3 gameplay | FAST 8× | 20.54 | 20.99 | 21.25 | 5656 |
| LEVEL2_3 gameplay | FAST 10× | 31.58 | 32.10 | 32.41 | 7896 |

### Reading the results

- **The 120 Hz budget (8.3 ms):** at 10× it is out of reach in every scene
  measured. 4K (16:9) gameplay at 10× takes 8–15 ms on Direct3D 12.
  7680×2160 takes 14–26 ms, and the tunnel takes 35 ms.
- **Direct3D 12 against Vulkan:** Direct3D 12 is faster at every 8× and
  10× point measured, by up to 1.7× (LEVEL1_1 gameplay at 10× 16:9).
  Vulkan runs also add about 3 GB more VRAM than Direct3D 12 at every
  scale, even at 4×. That was not investigated.
- **Scaling above 4×:** the cost grows much faster than the pixel count,
  which is 6.25× from 4× to 10×. LEVEL2_3 at 32:9 goes from 1.3 ms at 4×
  to 24.4 ms at 10× on Direct3D 12.

### Where the 10× time goes

`render-profile-us` puts nearly all of the extra time in the presentation
stage, which includes waiting for the GPU. On LEVEL1_1 gameplay at 10× 32:9
that stage takes 20.3 ms, against 2.0 ms at 4×.

A PIX capture of that frame (`tmp/pix/gameplay-10x-32_9.wpix`, events in
`tmp/pix/gameplay-10x-32_9-events.csv`) totals 39.1 ms of GPU time. One
dispatch takes 19.8 ms of it.

That dispatch is the row-span raster of the ground-grid dots: 225 points,
each owning one command slot per stored row. `GpuRaster::enqueue_row_spans`
bins tiles only while the dense tile list,
`tiles × (polygons + 1) × 4 bytes`, fits in 64 MiB (`gpu_raster.cpp`
~L546). For the grid that list is 40 MB at 4× 32:9, so it is tiled and the
pass takes 0.05 ms. It is 253 MB at 10× 32:9, so the raster runs untiled.
The 4× capture (`tmp/pix/gameplay-4x-32_9.wpix`) shows the binning
dispatch; the 10× capture does not.

Two checks confirm the mechanism:

- Forcing the untiled path at 4× (`STARFOX_TEST_DISABLE_TILED_SPANS=1`)
  raises LEVEL1_1 gameplay at 4× 32:9 from 3.71 to 4.89 ms.
- A local build with the cap raised to 512 MiB (not committed) changes
  LEVEL1_1 gameplay at 10× as follows:

  | Output | Committed (64 MiB cap) | Cap raised to 512 MiB |
  | --- | ---: | ---: |
  | 32:9, 7680×2160 | 26.4 ms median | 11.5 ms median (12.5 p95, 13.2 p99) |
  | 16:9, 3840×2160 | 15.3 ms median | 8.3 ms median (9.5 p95, 9.9 p99) |

  These are single runs (`tmp/1e/probe-10x-*`).

The same 64 MiB rule applies to the GPU FAST bounded model raster
(`gpu_raster.cpp` ~L504). At 10× 32:9 a model with more than about 58
polygon slots also loses its tiles.

## Limits

- One machine (RTX 4080) and Original only. EX and the integrated Radeon
  were not benchmarked at 8× or 10×.
- Enhancements were off. Chris's settings (ray tracing, reflections, RTX
  lighting, AA, xBRZ) were not measured at 10×.
- VRAM is a whole-GPU reading. It includes other applications' changes
  during a run and is not a per-process figure.
- The cap experiment is single-run and was not parity-checked. Tiled and
  untiled rasters are designed to give identical output, but that is
  unproven above 4×.
- The PIX pass timings come from PIX's replay and are used to rank work.
  The frame-time tables are the real cost.

## Next steps

1. **Tile lists for row spans at high scales.** Either scale the 64 MiB cap
   with render scale for GPU FAST (quick, at the cost of up to about
   250 MB of scratch at 10× 32:9), or build the compact count → prefix-sum
   → scatter binning from the Phase 1C design and use it here too. The
   compact binning is the better long-term fix, and 1C needs it anyway.
2. **Phase 1C batching**, per `docs/GPU-MODEL-BATCHING-DESIGN.md`. At 4×,
   most of the tunnel's GPU time goes to per-model raster passes, which is
   the cost 1C targets. The tunnel was not profiled at 10×.
