# GPU FAST: bounded in-place model raster (Phase 1B) — October 2, 2026

## Change

Commit `9768731` (fixture support `f205c0b`, benchmark switches `4d46b41`,
`ab854b6`). GPU ACCURATE is untouched.

GPU ACCURATE gives every model a raster pass dispatched over the whole
output. That pass reads the running scene and writes a full copy (colour,
plus surface metadata and depth when present) into the other ping-pong
buffer, even when the model covers a few pixels. Under GPU FAST, a model
fused onto the running scene now:

1. reduces its clip-stage row spans to a screen box on the GPU
   (`raster_bins` stage 7);
2. turns the box into indirect dispatch arguments and resets it (stage 8);
   an empty box dispatches nothing;
3. bins only the box's 64-pixel tiles (stage 9);
4. rasters only the box, writing directly into the running scene's
   buffers. `raster_portable` offsets thread IDs by the box origin and
   reads the background from its own output. Uncovered pixels inside the
   box rewrite their existing value, so the output is identical by
   construction, and pixels outside the box are never touched.

Empty models keep the background without any pass. Wave rows, raster
jitter, custom raster sizes, and backgrounds missing a surface or depth
plane keep the full-frame path, reported under
`STARFOX_TRACE_GPU_FAST_FALLBACK`. Models that are never fused (emissive
beams, world sprites, models with motion history, the first model after a
non-model layer) are unchanged. A/B switch:
`STARFOX_TEST_FULL_FRAME_MODEL_RASTER=1`.

The shaders were regenerated for SPIR-V, DXIL and MSL with DXC v1.9.2607
and SPIRV-Cross `be71ee8c`. Regenerating the unchanged shaders first
reproduced the checked-in payloads exactly. The generated headers also pick
up the current generator's formatting, which differs only in whitespace.

## Correctness evidence

All exact against the Software reference, with zero CPU-image uploads:

- `check_gpu_stage_sweep.ps1 -GpuRenderer FAST -RequireNoCpuUpload`:
  - every Original and EX stage (59) at 1× and 4×, 16:9, on Direct3D 12
    (236 comparisons, `tmp/1b-d3d12-16_9-all.log`);
  - every stage at 2× 32:9 on Vulkan (118 comparisons,
    `tmp/1b-vulkan-32_9-all-2x.log`);
  - LEVEL1_1/1_2/2_3 for both experiences at 1×/4× on Direct3D 12 32:9
    and Vulkan 16:9/32:9.
- `check_gpu_fast_ab.ps1`: 48 GPU ACCURATE/GPU FAST pairs with
  byte-identical native and final captures (`tmp/1b-ab-*.log`).
- `starfox_gpu_model_check ... 128` with `STARFOX_TEST_BOUNDED_MODEL_RASTER=1`:
  mixed, billboard, terrain, submitted, queued and recorded batches on
  Direct3D 12 and Vulkan. Each run reports 408–4,609 bounded dispatches,
  so none silently fell back (`tmp/1b/fixture-proof-*.log`).
- In game, 240 frames at 4× 32:9: no fallbacks traced on Original/EX
  LEVEL1_1, 1_2, 2_3 and 3_1 or EX LEVEL5_1. 69–82% of model draws
  took the bounded path (`tmp/1b/fallbacks`).
- ctest: 88/89 when run serially. The failure is the known upstream
  `starfox_dialogue_catalog_tests`. The 20 ROM-dependent tests that CMake
  enabled once `SF.SFC` was present all pass.
- The Android arm64 debug build compiles and the package check passes
  (`tmp/android-build-1b.log`). That is compile evidence only.

## Scene counters (Direct3D 12, 4× 32:9, median per frame)

| Experience | Stage | Full-frame dispatches (HUD-only FAST → bounded) | Bounded dispatches | Compute passes |
| --- | --- | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 27 → 11 | 13 | 128 → 148 |
| ORIGINAL | LEVEL1_2 | 25 → 7 | 12 | 66 → 84 |
| ORIGINAL | LEVEL2_3 | 14 → 8 | 2 | 38 → 38 |
| EX | LEVEL1_1 | 31 → 8 | 18 | 154 → 186 |
| EX | LEVEL1_2 | 28 → 11 | 13 | 107 → 129 |
| EX | LEVEL2_3 | 36 → 9 | 23 | 181 → 223 |

Each bounded model adds three small passes, so compute passes go up while
full-frame work goes down.

## Matched runtime measurements

Method as in `GPU-BATCHING-BASELINE-OCT2026.md`: unpaced CPU frame work,
360 presentations, 60 warm-up frames excluded, enhancements off. Values are
median / p95 in ms.

### Integrated GPU (AMD Radeon, 9800X3D iGPU, Direct3D 12)

Selected with `STARFOX_TEST_LOW_POWER_GPU=1`; the log line
`test-gpu-adapter: AMD Radeon(TM) Graphics driver=direct3d12` confirms the
adapter. Three repetitions per configuration, interleaved; each cell is the
median of the three run medians, and run-to-run spread was within about 2%.
Raw logs: `tmp/1b/igpu`.

| Stage | Scale | Display | GPU ACCURATE | GPU FAST, full-frame | GPU FAST, bounded | Bounded vs full-frame | Bounded vs ACCURATE |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| ORIGINAL LEVEL1_1 | 1× | 16:9 | 4.05 / 4.61 | 4.14 / 4.69 | 3.59 / 4.09 | -13% | -11% |
| ORIGINAL LEVEL1_1 | 4× | 32:9 | 34.83 / 39.95 | 36.97 / 41.89 | 19.61 / 23.15 | -47% | -44% |
| ORIGINAL LEVEL1_2 | 1× | 16:9 | 2.62 / 3.27 | 2.69 / 3.36 | 2.19 / 2.64 | -18% | -16% |
| ORIGINAL LEVEL1_2 | 4× | 32:9 | 29.32 / 38.02 | 31.59 / 40.32 | 17.81 / 19.57 | -44% | -39% |
| EX LEVEL2_3 | 1× | 16:9 | 5.32 / 5.78 | 5.42 / 5.86 | 4.54 / 4.94 | -16% | -15% |
| EX LEVEL2_3 | 4× | 32:9 | 48.78 / 52.23 | 50.85 / 54.10 | 24.00 / 25.71 | -53% | -51% |

On this GPU the bounded raster halves frame work at 4× 32:9. The HUD
change on its own (the GPU FAST full-frame column) was 2–8% *slower* than
GPU ACCURATE here, because its GPU indexed-layer passes cost more than the
upload they replace. With the bounded raster, GPU FAST is 39–51% faster
than GPU ACCURATE at 4× and 11–16% faster at 1×.

### NVIDIA RTX 4080 (single runs; raw logs in `tmp/gpu-fast-1b-oct2026`)

#### Direct3D 12

| Experience | Stage | Scale | Display | GPU ACCURATE | GPU FAST, full-frame | GPU FAST, bounded | Bounded vs full-frame |
| --- | --- | ---: | --- | ---: | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 1× | 16:9 | 1.72 / 2.35 | 1.62 / 2.32 | 1.66 / 2.46 | +3% |
| ORIGINAL | LEVEL1_1 | 1× | 32:9 | 1.74 / 2.47 | 1.64 / 2.42 | 1.69 / 2.40 | +3% |
| ORIGINAL | LEVEL1_1 | 4× | 16:9 | 3.81 / 5.30 | 3.30 / 4.29 | 3.10 / 4.26 | -6% |
| ORIGINAL | LEVEL1_1 | 4× | 32:9 | 6.03 / 7.12 | 4.13 / 5.62 | 4.21 / 5.34 | +2% |
| ORIGINAL | LEVEL1_2 | 1× | 16:9 | 0.93 / 1.89 | 0.96 / 1.92 | 0.90 / 1.87 | -6% |
| ORIGINAL | LEVEL1_2 | 1× | 32:9 | 0.96 / 2.00 | 0.93 / 1.92 | 0.98 / 2.02 | +6% |
| ORIGINAL | LEVEL1_2 | 4× | 16:9 | 3.18 / 4.21 | 2.50 / 3.66 | 2.69 / 3.71 | +8% |
| ORIGINAL | LEVEL1_2 | 4× | 32:9 | 5.72 / 6.94 | 4.14 / 5.28 | 4.32 / 5.55 | +4% |
| ORIGINAL | LEVEL2_3 | 1× | 16:9 | 0.60 / 1.07 | 0.59 / 0.96 | 0.60 / 0.99 | +3% |
| ORIGINAL | LEVEL2_3 | 1× | 32:9 | 0.69 / 1.08 | 0.64 / 1.08 | 0.64 / 1.10 | +1% |
| ORIGINAL | LEVEL2_3 | 4× | 16:9 | 3.23 / 3.64 | 2.12 / 2.64 | 2.13 / 2.66 | +0% |
| ORIGINAL | LEVEL2_3 | 4× | 32:9 | 5.05 / 5.85 | 3.74 / 4.32 | 3.70 / 4.32 | -1% |
| EX | LEVEL1_1 | 1× | 16:9 | 2.09 / 2.98 | 2.05 / 3.22 | 2.07 / 3.14 | +1% |
| EX | LEVEL1_1 | 1× | 32:9 | 2.16 / 3.29 | 2.08 / 3.09 | 2.08 / 3.30 | +0% |
| EX | LEVEL1_1 | 4× | 16:9 | 4.79 / 6.42 | 4.18 / 5.70 | 4.03 / 5.52 | -4% |
| EX | LEVEL1_1 | 4× | 32:9 | 6.30 / 8.00 | 5.24 / 6.55 | 4.54 / 6.31 | -13% |
| EX | LEVEL1_2 | 1× | 16:9 | 1.39 / 2.31 | 1.39 / 2.32 | 1.41 / 2.28 | +2% |
| EX | LEVEL1_2 | 1× | 32:9 | 1.49 / 2.51 | 1.40 / 2.46 | 1.42 / 2.45 | +1% |
| EX | LEVEL1_2 | 4× | 16:9 | 3.41 / 4.83 | 2.91 / 3.92 | 2.60 / 3.97 | -10% |
| EX | LEVEL1_2 | 4× | 32:9 | 5.90 / 7.55 | 4.00 / 5.69 | 4.16 / 5.69 | +4% |
| EX | LEVEL2_3 | 1× | 16:9 | 2.17 / 3.14 | 2.23 / 3.12 | 2.18 / 3.20 | -2% |
| EX | LEVEL2_3 | 1× | 32:9 | 2.18 / 3.28 | 2.15 / 3.18 | 2.27 / 3.21 | +6% |
| EX | LEVEL2_3 | 4× | 16:9 | 5.05 / 6.08 | 4.51 / 5.33 | 4.26 / 4.88 | -5% |
| EX | LEVEL2_3 | 4× | 32:9 | 6.46 / 8.47 | 5.48 / 6.49 | 4.69 / 6.62 | -14% |

#### Vulkan

| Experience | Stage | Scale | Display | GPU ACCURATE | GPU FAST, full-frame | GPU FAST, bounded | Bounded vs full-frame |
| --- | --- | ---: | --- | ---: | ---: | ---: | ---: |
| ORIGINAL | LEVEL1_1 | 1× | 16:9 | 2.60 / 3.36 | 2.58 / 3.52 | 2.67 / 3.53 | +3% |
| ORIGINAL | LEVEL1_1 | 1× | 32:9 | 2.63 / 3.28 | 2.56 / 3.20 | 2.61 / 3.26 | +2% |
| ORIGINAL | LEVEL1_1 | 4× | 16:9 | 6.05 / 7.34 | 5.41 / 6.67 | 5.35 / 6.51 | -1% |
| ORIGINAL | LEVEL1_1 | 4× | 32:9 | 7.28 / 8.53 | 6.07 / 7.21 | 5.77 / 6.91 | -5% |
| ORIGINAL | LEVEL1_2 | 1× | 16:9 | 1.33 / 1.76 | 1.29 / 1.72 | 1.36 / 1.89 | +5% |
| ORIGINAL | LEVEL1_2 | 1× | 32:9 | 1.41 / 1.95 | 1.34 / 1.82 | 1.35 / 1.87 | +1% |
| ORIGINAL | LEVEL1_2 | 4× | 16:9 | 3.21 / 4.40 | 2.33 / 3.36 | 2.47 / 3.61 | +6% |
| ORIGINAL | LEVEL1_2 | 4× | 32:9 | 6.14 / 7.34 | 3.84 / 5.02 | 4.24 / 5.60 | +10% |
| ORIGINAL | LEVEL2_3 | 1× | 16:9 | 0.90 / 1.05 | 0.89 / 1.01 | 0.92 / 1.07 | +3% |
| ORIGINAL | LEVEL2_3 | 1× | 32:9 | 0.91 / 1.10 | 0.90 / 1.09 | 0.90 / 1.08 | +0% |
| ORIGINAL | LEVEL2_3 | 4× | 16:9 | 2.81 / 3.33 | 2.01 / 2.42 | 2.15 / 2.55 | +7% |
| ORIGINAL | LEVEL2_3 | 4× | 32:9 | 5.29 / 5.99 | 3.57 / 4.18 | 3.83 / 4.81 | +7% |
| EX | LEVEL1_1 | 1× | 16:9 | 3.10 / 4.43 | 3.11 / 4.42 | 3.20 / 4.50 | +3% |
| EX | LEVEL1_1 | 1× | 32:9 | 3.15 / 4.36 | 3.10 / 4.28 | 3.16 / 4.37 | +2% |
| EX | LEVEL1_1 | 4× | 16:9 | 7.64 / 9.61 | 7.01 / 9.11 | 6.90 / 8.82 | -2% |
| EX | LEVEL1_1 | 4× | 32:9 | 8.90 / 10.74 | 7.79 / 9.72 | 7.29 / 9.11 | -6% |
| EX | LEVEL1_2 | 1× | 16:9 | 2.16 / 2.58 | 2.13 / 2.70 | 2.21 / 2.81 | +4% |
| EX | LEVEL1_2 | 1× | 32:9 | 2.21 / 2.57 | 2.14 / 2.52 | 2.23 / 2.56 | +4% |
| EX | LEVEL1_2 | 4× | 16:9 | 4.95 / 5.49 | 4.39 / 5.24 | 4.25 / 4.79 | -3% |
| EX | LEVEL1_2 | 4× | 32:9 | 6.25 / 7.68 | 5.00 / 5.62 | 4.77 / 5.90 | -5% |
| EX | LEVEL2_3 | 1× | 16:9 | 3.45 / 4.08 | 3.43 / 4.07 | 3.50 / 4.16 | +2% |
| EX | LEVEL2_3 | 1× | 32:9 | 3.46 / 4.34 | 3.38 / 4.28 | 3.46 / 4.36 | +2% |
| EX | LEVEL2_3 | 4× | 16:9 | 8.21 / 9.14 | 7.61 / 8.55 | 7.50 / 8.38 | -1% |
| EX | LEVEL2_3 | 4× | 32:9 | 9.52 / 10.74 | 8.31 / 9.57 | 7.95 / 9.16 | -4% |

On the RTX 4080 the bounded raster is roughly neutral against full-frame
GPU FAST (−14% to +10% at 4×, −6% to +6% at 1×; single runs with about ±5%
noise). This GPU has headroom: utilization averaged 59% (peak 88%) during
a 1,200-frame 4× 32:9 GPU FAST run (`nvidia-smi`). The remaining 4× cost
on it sits in the presentation stage (`present` ≈ 3.3 ms at 4× 32:9 against
≈ 0.9 ms at 1×, independent of model count), not in model rasterization.

## Limits / follow-up

- Two GPUs on one machine. CPU frame work only; no GPU timestamps.
- On the low-power adapter the process sometimes failed to exit after
  finishing its frames, under GPU ACCURATE as well (two early probe runs;
  none of the 54 measured runs). Not investigated yet.
- The presentation stage is now the main 4× cost on the RTX 4080 and the
  next profiling target. It also matters for Phase 1E (10×).
- Metal output is generated but not run (no Apple device).
