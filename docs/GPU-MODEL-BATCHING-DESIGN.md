# GPU FAST: batching consecutive models into one raster pass (Phase 1C design)

Status: **design for review; no production code**. Phase 1A (static packing
cache) is folded in as an optional later step (section 9).

## 1. Why: what the measurements say

All numbers: RTX 4080, Direct3D 12, GPU FAST, Original LEVEL1_1, 4× 32:9
(3200×896 stored), 120 Hz presentation target, unpaced. Raw logs are under
`tmp/static-drop`, `tmp/fps-check`, `tmp/pix`.

| Scene | Everything | Models dropped from the GPU scene (experiment) | Background layers dropped |
| --- | ---: | ---: | ---: |
| Launch tunnel, enhancements off | 7.18 ms | 2.12 ms | 6.66 ms |
| Launch tunnel, Chris's settings | 10.16 ms | 3.50 ms | 9.42 ms |

The drop experiment produces wrong pictures and was never committed. It
shows that in the tunnel **model rasterization costs about 5–6.5 ms per
frame**, so Chris's 120 Hz budget (8.3 ms) is out of reach without fixing
it. The tunnel draws 18–20 models per frame, and each wall segment covers
most of the screen. That is why Phase 1B's screen box barely helps there
(bounded vs full-frame raster: −4%).

An earlier version of this experiment wrongly suggested the models were
*not* the cost. A CPU-side compositor loop (≈4 ms per frame proving an
unchanged CPU layer is unchanged) masked the GPU time. That loop is fixed
separately; see the static CPU layer change in the Status log.

PIX (2603.25, capture `tmp/pix/tunnel-plain.wpix`, enhancements off) shows
the structure of one tunnel frame: 154 dispatches, 30 indirect dispatches
and **1,682 resource barriers**. Each bounded model is a chain of about nine
dependent passes: projection → visibility → BSP → clip → spans → box reduce
→ box arguments → tile binning → raster. SDL inserts barriers around each
pass, so these chains serialize on the GPU. PIX's per-event times are
inflated by its fixed-clock serialized replay and are used here only to
rank work, not as real costs.

## 2. Options

| | (a) Instanced front-end chain | (b) CPU-merged shapes (terrain-style) | **(c) Per-model front end, one batched raster** |
| --- | --- | --- | --- |
| What runs once per batch | Everything from projection to raster | Everything (one merged shape) | Box reduce, binning, raster |
| What stays per model | Nothing | Nothing | Projection, visibility, BSP, clip, spans (small dispatches) |
| Painter order | Draw index in every stage | Depends on sort order; the merged BSP is not the same as separate BSPs | Global record index (proof below) |
| Shader changes | Every front-end shader gains instance tables | None, but `Face::vertex_indices` → 16-bit and new CPU merging | `raster_bins` (sorted binning for a batch), one small append/rebase shader |
| CPU cost | Lowest | **Higher**: vertex transform moves back to the CPU | Unchanged (front end still per model) |
| Exactness risk | High (largest rewrite) | High (merging changes BSP semantics; needs decoder-identity proofs) | **Low**: the raster already resolves "last covering command wins" |
| Expected tunnel win | Largest | Unclear (CPU-bound) | Most of the raster cost; front-end passes remain |

**Recommendation: (c).** It goes after the measured cost (≈20 near
full-screen raster/binning passes per tunnel frame) with the smallest change
to proven code. (a) can follow later if the remaining per-model front-end
passes show up as the next bottleneck. (b) is rejected: it puts the vertex
transform back on the CPU and breaks the per-model BSP painter semantics
that GPU ACCURATE reproduces exactly.

## 3. Design (c) in detail

### 3.1 Batch formation (GpuScene::enqueue_batch, GPU FAST only)

A batch is a maximal run of **consecutive** `GpuModelDraw`s in the scene's
draw order that would each take the 1B bounded in-place path today, and
that share:

- raster size (no custom raster), render scale, and zero jitter;
- `surface_metadata` (`take_surface`) and pixel-coverage mode;
- planar-depth eligibility and the same depth projection (focal x/y,
  vanish/centre). Every world model in a frame uses the camera's
  projection; HUD or cockpit models that differ end the batch.

A model is excluded from batches (and keeps the 1B or full-frame path)
when it uses wave rows, wobble/repeated rows (masked texels), colour warp,
or explosion fragments, or when it is not fused (emissive, world sprite,
motion history, first model after a non-model layer). Anything that ends a
batch flushes it first, so draw order is never changed.

### 3.2 Per-model front end, appended into a batch arena

Each model in the batch runs its existing front end unchanged (projection,
visibility, BSP, clip, spans). After `enqueue_spans`, a new tiny **append**
pass copies the model's span records into a per-batch arena at
`base_k = Σ_{j<k} slots_j × height` and rebases two model-local fields:

- `texture_offset += texel_base_k` for textured records. The model's
  texels are copied into a batch texel arena at `texel_base_k` with
  `CopyBufferRegion`.
- the depth-plane id in `has_surface >> 1 += plane_base_k` when it is
  non-zero. Planes are copied into a batch plane arena at `plane_base_k`.

This is needed because the two `GpuModel` renderers alternate, and each
reuses its span, texel and plane buffers for the next model. The arena
keeps every model's records alive until the batch raster.

### 3.3 One box, one binning, one raster

1. **Box:** `raster_bins` stage 7 over all `Σ slots × height` records gives
   the union screen box (same shader, larger dispatch).
2. **Binning:** today's tile lists (stage 6/9) store every polygon slot per
   tile (`tiles × (slots + 1) × 4` bytes). For a 20-model batch at 4× 32:9
   that would be ~107 MB, so batches use **compact ordered lists** over the
   box's tiles, sized by actual overlaps.

   *As built (raster_bins stages 10–14):* one 64-lane group per box row
   stages the row's polygon spans through groupshared memory, 64 at a time.
   Each lane owns two tile columns and counts (stage 10) and later fills
   (stage 14) its tiles by walking polygons in ascending order. So every
   list is in painter order without a sort or atomics, and matches stage 6
   entry for entry. Stages 11–13 are an exclusive scan of the box tile counts
   into CSR offsets. The raster reads `[offset[t], offset[t+1])` and keeps
   its "walk from the top, stop at the first covering command" early exit.
   If a fill would overflow the 64 MiB list budget, the raster detects it on
   the GPU and walks every polygon, which is slow but exact.

   The same lists replace the dense lists wherever a dense list would
   exceed its 64 MiB cap under GPU FAST: single bounded models, and the
   full-frame grid, dust and particle row spans. That fixes the 10× cliff in
   `docs/GPU-FAST-10X-OCT2026.md`.
3. **Raster:** one 1B-style in-place indirect dispatch over the union box,
   with `command_count = Σ slots` and the global arena as the command
   buffer.

### 3.4 Painter-order argument

Sequential GPU FAST drawing (1B) produces, for each pixel, the result of
applying models 1..n in order. For model k the raster takes the colour of
model k's **last** covering command, or keeps the background. It takes the
surface of model k's last covering surface-bearing command, falling back to
the background's surface bit. The depth comes from the winning colour
command's plane, or the background's depth when nothing covers.

In the batch, records are ordered by (model order, then the model's own
polygon/row order), so the global record index is a total order consistent
with sequential drawing. Walking a pixel's tile list from the highest
global index down:

- the first covering command is the last covering command of the last
  model that covers the pixel, which is exactly what sequential drawing
  leaves;
- continuing to the first surface-bearing covering command gives the last
  such command over all models, which is what sequential fallback-to-
  background chains produce, model by model;
- depth is computed from the same winning command's plane with the same
  (required-equal) projection, so it matches too;
- pixels no command covers keep the background (with the 1B skip, they are
  not even written).

So batched output equals sequential output bit for bit, provided every
model in the batch would have taken the in-place bounded path, which the
batch rules guarantee.

### 3.5 Interactions

- **Ray-traced shadows and reflections:** ray geometry and materials come
  from each model's front end (projection points, clip records), which is
  unchanged, so casters and materials are identical. Models flagged
  emissive never batch.
- **Temporal (DLSS/FSR):** models with motion history are not fused today
  and so never batch. The temporal raster (custom raster size and jitter)
  is excluded by rule 3.1.
- **Enhanced terrain:** its merged patch shapes are ordinary fused models
  and can batch like any other (the terrain-batching fixture must still
  pass).
- **3D asteroids (PR #11), Phase 1D:** substituted rocks routed back
  through `draw_model()` become ordinary fused models and batch.

### 3.6 Fallbacks

Every reason a model leaves a batch, or a batch is flushed early, is
reported under `STARFOX_TRACE_GPU_FAST_FALLBACK`, as 1B does. A/B switch:
`STARFOX_TEST_UNBATCHED_MODEL_RASTER=1` keeps 1B behaviour.

### 3.7 Memory budget

The span arena needs `Σ slots × height × 96 B`. The tunnel's ~20 models
with 20–34 slots at 896 rows need about 50 MB. At 10× (2240 rows) that is
about 130 MB. A batch therefore closes when its arena would exceed **64 MiB**
(configurable for tests). Ordered bins need about `(box tiles + overlaps)
× 4 B`, far below the dense lists. Arena and bin buffers are reused across
frames, with SDL cycling on the first write per command, as for scene
scratch, to bound in-flight copies.

A compact span format that stores only the rows each polygon covers would
shrink the arena several-fold. It is a possible follow-up if 10× memory
becomes a problem.

## 4. Expected counter changes (tunnel, 4× 32:9)

| Counter | Today (1B) | With batching |
| --- | ---: | ---: |
| Bounded raster dispatches | ~19 | 1–2 (one per batch) |
| Box/bin passes | ~57 (3 per model) | ~4–6 per batch |
| Front-end passes | ~95 | ~95 + 1 append per model |
| Resource barriers (PIX) | ~1,682 | roughly −40% |

A new counter, `model-batches`, will report batches per frame and models
per batch.

## 5. Test plan

1. **New fixture `starfox_gpu_model_batches_check`**, modelled on
   `starfox_gpu_terrain_batches_check`. It runs the same model lists
   batched and unbatched (1B), and compares pixels, tags, coverage,
   surfaces and depth exactly at 1×/2×/4×, with flat, banked and
   near-plane views, overlapping and non-overlapping models, textured,
   surface-bearing and line/sprite faces, and forced batch splits by
   arena size. It runs on D3D12 and Vulkan.
2. `starfox_gpu_model_check` batch modes with the batched opt-in, requiring
   a non-zero batch count (as `f205c0b` does for bounded dispatches).
3. Stage sweeps under GPU FAST for all 59 stages (both drivers, 1×/4×,
   16:9/32:9), and `check_gpu_fast_ab.ps1` including the launch tunnel
   (`-Ticks 0`).
4. Benchmarks: the tunnel and the baseline stages, RTX 4080 and the
   integrated Radeon, interleaved repeats. Target: tunnel GPU FAST under
   8.3 ms p95 at 4× 32:9 with enhancements off, and as close as possible
   with Chris's settings.
5. The Android arm64 build and package check.

## 6. Implementation outline (after approval)

1. Fixture first (step 5.1), running against the unbatched path.
2. Append/rebase shader and per-batch arenas; batch formation in
   `GpuScene::enqueue_batch` behind GPU FAST and the A/B switch.
3. Ordered binning stages in `raster_bins`, regenerated with the pinned
   toolchain.
4. Batched box + raster using the existing 1B in-place raster.
5. Fallback tracing, the counter, gates, benchmarks, results doc.

## 7. Risks

- Ordered binning is the subtle part. The fixture must cover tiles where
  many models overlap, and the unordered-plus-owner alternative is the
  fallback design if the sort is too costly.
- Arena copies add GPU work per model. They are small next to a near
  full-screen raster pass, but must be measured in sparse scenes (the
  asteroid field) so batching never regresses them.
- Metal is generated but untested (no Apple device).

## 8. Out of scope

Instanced front-end chains (option a), the compact span format, and 10×
(Phase 1E). 1E builds on this design because it multiplies every per-pixel
cost by 6.25.

## 9. Folded-in Phase 1A (optional)

The CPU encode of the whole scene is 0.2–0.8 ms per frame. Caching packed
static topology per `Shape` (BSP, topology, normals, texels; see the 1A
audit in the Status log) would trim part of that. It stays optional, and
it should follow batching only if CPU encode becomes the limit.
