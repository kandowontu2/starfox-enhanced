# MSAA integration status

## Native calibrated Leia geometry (October 1)

`GpuCalibratedScene` now has a real hardware 2/4/8-sample colour/depth path.
It retains calibrated per-eye matrices and source geometry, pixel-frequency
material shading, centroid UVs at partially covered edges and native alpha
discard. Flat-model authored dither pairs are smoothed; protected native ink
keeps its source path. Pipelines/shaders are lazy and the default single-sample
path allocates no multisample targets. The caller supplies independent matching
eye targets and optional single-sample resolves. Ordered overlays can load
retained samples only after the source upload is confirmed submitted.

Native Leia MSAA is connected to the frontend: Low/Medium/High selects real
2/4/8 hardware samples at the calibrated output extent, not SSAA or a spatial
substitute. Receiver/surface MRTs preserve exact values at each covered
sample instead of averaging discrete labels or unrelated depths. The pinned
desktop SDL extension permits opt-in Texture2DMS reads on D3D12 and Vulkan with
standard sample locations. Storage access remains forbidden and ordinary
texture validation is unchanged. `GpuCalibratedMsaa` extracts resident colour,
ownership and optional surface planes and resolves finished planes in linear
light without CPU image transport. Shader/pipeline creation is lazy.

Guide shaders carry sample-interpolated geometric forward depth explicitly:
SV_Position.w was centre-evaluated at some D3D12 edges. The native-only guide
vertex/fragment pair fixes that without changing the ordinary/headset ABI.
`calibrated_msaa_sample_camera` shifts only projection centres so single-sample
ray/depth consumers evaluate the matching hardware sample ray. Actual GPU
AO/DOF sample-plane composition and enhanced resolves pass independent plane
and CPU-effect oracles on both backends in four linear/sRGB RGBA/BGRA formats.

Each eye/sample owns its native shadow/reflection producers, temporal trails,
phosphor and exposure history. Depth, material/style, contrast/chromatic, global
and bloom passes consume matching sample planes. Tracked poses, source geometry
and the effects clock remain shared and immutable across the retained frame.
Only accepted presentation commits histories; image waits and failed submissions
do not advance them. Final colour resolves in linear light while centre-raster
protected ink remains exact; ordered translucent UI is drawn afterward.

Unsupported counts/formats and an overflow-safe four-GiB eye/history working
estimate fail explicitly. The estimate is not a free-VRAM guarantee and excludes
geometry/acceleration structures. Normal history/ray owners are released on
entry; OFF restores the ordinary path and releases MSAA resources. Actual
Direct3D/Vulkan GPU fixtures test independent single-sample XR/FOV references,
combined effects/history, held frames, retries and exact OFF restoration. Native
TAA, remaining full-goal parity and physical Leia acceptance remain open.

`tools/check_calibrated_scene.cpp --msaa-only` runs actual D3D12/Vulkan fixtures
(put the backend before the switch). Independent XR/FOV/sample-coverage oracles
check both eyes, all three counts and four linear/sRGB RGBA/BGRA formats. The
fixtures cover partial edges, both windings, shared-quad edges, painter order,
depth, six blends, dither pairs, texture holes, retained overlays, held output,
cancelled-upload recovery, exact OFF restoration and invalid-count/alias/MRT
guards. Full native scene regressions include these tests. This does not certify
physical Leia display composition or every specialized packet's AA behavior.

The additional `--msaa-guides-only` fixture checks exact ownership/colour,
tilted-plane normals/depths, cutout holes, mixed-owner edges, per-sample AO/DOF,
linear enhanced resolve, receiver-only extraction, ordered overlays, empty
frames, held output, cancellation/rejection recovery and exact OFF restoration.
Those guide checks also run in the full native scene fixture. Owner-level tests
in `starfox_displayxr_gpu_presenter_check` additionally exercise combined native
ray/effect histories and ordered UI. Neither fixture certifies a physical Leia
display or every specialized packet/enhancement combination.

## Ordinary flat/stereo compute renderer

The native-compute coverage/resolve core is connected to the game menu as MSAA,
with 2/4/8 samples for Low/Medium/High. It is not an alias for render supersampling.

- Standard 2/4/8 sample positions, stored once in `msaa_samples.inc`.
- Top-left edge ownership, canonical edge evaluation, both polygon windings.
- One flat material shade per primitive/pixel, distributed to covered samples.
- Line primitives preserve thickness and square endpoints, including zero-length
  lines; reversal, diagonal/horizontal coverage and 2/4/8 samples are tested.
- Flat wireframe polygons now emit multisampled boundary strokes with square
  joins, rather than triggering the whole-model fallback. Vulkan/DX12 tests
  cover 2/4/8 samples, reversed winding and empty interiors (no fan diagonals).
  Compact stroke packets cover the full 128-vertex clipping limit; tests include
  4-, 64- and 128-vertex outlines. The native material bit is decoded before
  conversion to raster spans (wireframe mode 1, not the cel-mode bit).
- Model sprites now pack their depth-sized image quad and bounded affine UVs.
  Integer/fractional vertex layouts, transparent texels and no-wrap boundaries
  pass Vulkan/DX12 tests at 2/4/8 samples. Texture shading stays pixel-frequency.
- Cel-style polygons preserve the native one-stored-pixel horizontal inset by
  intersecting left/right translated coverage for the entire polygon. This
  avoids eroding internal fan edges. Vulkan/DX12 tests cover both windings,
  2/4/8 samples and 4/64/128-vertex polygons.
- Continuation wireframe reconstructs the native left/right vertex restart
  events and switches between filled coverage and horizontal boundary coverage.
  Tests include an asymmetric five-vertex shape: the forward winding changes
  to outlines after the left-chain restart, while reverse winding stays filled.
  Both backends pass at 2/4/8 samples, including the filled restart row.
- Sparse wobble now uses previous-row left-boundary sample coverage, retaining
  the one-pixel strip and suppressing first/right-chain restart rows. Its
  fractional boundary fixture passes Vulkan/DX12 at 2/4/8 samples and both
  windings.
- Repeated-row preparation now has a CPU sample-union oracle and Vulkan/DX12
  resolve tests for disjoint intervals, overlap and duplicate spans at 2/4/8
  samples. The native repeated-row tracer emits the selected 2/4/8 bitplanes
  from its fractional fixed-point edge positions.
  Its original binary mask remains intact. Repeated-face packets carry the
  sample-mask location; the MSAA resolve consumes those planes without filling
  the gaps. Combined sparse/repeated, cel, wireframe and wave flags use the
  same native tracer. Allocation is opt-in and remains capped at 256 MiB.
- With MSAA packet generation enabled, 32 real models / 320 wobble-combination
  images retain exact native output on Vulkan and DX12. The resolved 8-sample
  capture was visually inspected: 2330 visible pixels, 15 partial edges, with
  identical image hashes on both backends. Low/medium and allocation/performance
  checks are recorded in the local regression audit below.
- Low/Medium/High repeated-row captures now pass both backends. The 2x fixture
  uses 1,935,360 mask bytes (largest model in this set: 2,580,480). Cold timings
  are explicitly distinguished from 30 warmed-up scene+wait measurements.
  Packed interval writes and parallel sample-mask clearing retain identical
  output hashes while improving the High fixture median from 36.72 to 17.98 ms
  on Vulkan and 45.97 to 3.26 ms on DX12. These include submission/wait and are
  single-model fixture timings, not gameplay FPS. These historical timings
  precede the selected-quality mask optimization recorded below.
- Native wave materials now displace per-sample coverage using the source
  signed-16-bit phase arithmetic and 16-entry displacement table. The wave
  remains constant within each stored pixel column, matching the native raster.
  Vulkan/DX12 tests check the displaced boundary at 2/4/8 samples, both windings
  and 4/64/128 vertices, including a wrapping phase offset.
- Colour-warp packing now consumes the expanded occurrence slots directly,
  preserving their randomized materials without applying the source BSP order
  twice. With packet generation enabled, 32 real models / 576 indexed images
  still match SoftwareRenderer on Vulkan and DX12 (72 nonempty packet outputs).
  A resolved 8-sample resident capture additionally checks that the visible
  colour-warp fixture produces partial coverage (277 edge pixels among 6406
  visible pixels). Vulkan and DX12 produce identical capture hashes; the image
  was visually inspected. `STARFOX_TEST_MSAA_CAPTURE` on the model checker saves
  this final colour output and rejects empty or entirely binary coverage.
- Affine indexed textures retain UVs, scroll/wrap, palette base and transparent
  texels. Shading remains pixel-frequency, with covered-sample centroid
  interpolation on partially covered edges and checked texture addressing.
- Per-sample painter visibility and final sample averaging.
- Optional retained samples across model submissions, preserving
  overlap coverage instead of feeding a prematurely resolved edge into the next
  model. Scene layers retain this storage in place, update only covered pixels,
  and read previous samples only where coverage needs them. This removes a second
  full-frame sample buffer and whole-frame sample copies for each small draw.
  Scene samples hold palette/material tokens; background-based core calls keep
  RGBA16F. The sample storage is allocated only when continuation is requested.
- Authored two-color flat materials resolve their colors rather than emitting
  a checkerboard into the sample buffer.
- CPU reference and Vulkan/DX12 tests cover shared edges, degenerates, partial
  coverage, overlapping faces, dither materials, resize and background retention.

- GPU-resident packing accepts the native clipped-face and material layouts,
  retaining fractional positions, a supplied painter list, and both source
  palette entries. Special primitives are explicitly marked for dispatch.
- Packed-face traversal skips each face's unused clipping capacity. An ordinary
  triangle needs one coverage test per pixel, rather than 128 reserved-slot
  tests; dense traversal remains available as a regression oracle. This is an
  operation-count reduction, not a measured gameplay FPS claim.
- `GpuModel` can optionally emit these packets from its real clipped geometry
  and resident BSP order. GPU result counts, offsets, failed trees and unused
  slots are validated without CPU readback. A 32-model/384-image regression
  check confirms enabling packet generation preserves existing indexed output.

The ordered `GpuScene` submission now optionally consumes model packets and
retains per-sample material identity through each draw. Legacy raster/HUD draws replace only
explicitly covered pixels (including black); indexed layers retain their source
remapping, clipping and mosaic through a separate mapper. Models containing a
invalid or unknown material packet keep their complete native draw rather than
dropping individual faces or changing painter order. This defensive fallback is
not multisampling that packet; the supported special materials listed above have
their own coverage evaluators. Normal AA-disabled submissions allocate no MSAA resources.

Vulkan and DX12 scene tests exercise actual model projection and line edges,
retained samples through HUD and indexed mosaic overlays, and empty/disabled
submission resets. The output is a premultiplied RGBA coverage stream alongside
the indexed scene. `GpuComposite` now accepts this stream for native, late and
background layers and blends coverage against the underlying color rather than
requantizing it to a palette index. Vulkan/DX12 tests verify partial model edges
and opaque black CPU overlays for all three placements. Existing composition
tests continue to exercise the non-MSAA path.

Runtime presentation supplies the finalized CPU palette (including brightness
and tint edits) through a cycled upload. Scene samples retain indexed material
tokens so presentation only recolors coverage; it no longer submits geometry
twice. Provisional palette colors are not exposed to the compositor. Original
geometry/ray metadata remains alive. Native, late and background composition consume the result
before ordinary effects. The menu reports mono/GPU/upscaler restrictions.

Menu cycling, state/config persistence and changing CPU palette uploads have
regression coverage. Initial gameplay captures cover Low at 1x and High at 2x;
they are not comprehensive scene/effects or performance validation.

Validation includes Original and EX intro geometry, EX gameplay, and the combined
enhanced sky/bloom/reflection/ray-water path. Remaining work: special-material
models still fall back as whole native draws instead of receiving MSAA coverage.
Stereo TAA/MSAA and stacking with DLSS/FSR are also unsupported. Passing the
ordinary-geometry tests does not prove those missing paths are finished.

Vulkan/DX12 regression tests also change the palette after an owned scene has
been submitted, verifying recolored samples, unchanged coverage and preserved
geometry output. Generic color and indexed continuation cannot be mixed.

The in-place path is compared against the CPU coverage oracle and separate-buffer
continuation on Vulkan/DX12 for 2/4/8 samples, shared edges, overlap, dither pairs,
degenerate/untouched regions and resize. A High/2x gameplay capture also executes
enhanced sky, bloom, reflection and ray-water paths without fallback. Profiling
uses `capture_lava.ps1 -Profile`; full pass-cost logging avoids the old 20ms-only
log's selection bias. These CPU wall timings are not isolated GPU timestamps.

The optimized coverage shader also rejects disjoint pixel/triangle bounding
boxes before per-sample edge evaluation; inclusive bounds preserve edge ties.
The CPU-reference tests continue to match for all sample counts.

Local 120-frame EX LEVEL6_6 runs at 2x, with the first ten measurements excluded,
gave native-path medians/p95 of 3.403/5.273 ms with AA off and 14.490/17.943 ms
with High MSAA (`tmp/msaa-profile-all-off`, `tmp/msaa-profile-all-high`). These
include synchronization and host submission, exclude other game-loop work, and
are not a measured FPS guarantee or a before/after speedup measurement.

After late palette resolution replaced geometry replay, the equivalent 120-frame
run logs 120 main-scene submissions rather than 240. Its presentation/native
section median/p95 is 4.683/7.158 ms, but MSAA work now starts in the earlier scene
submission, so these numbers must not be presented as an isolated FPS speedup.
The combined sky/bloom/reflection/ray-water capture also completes without
fallback (`tmp/msaa-single-submit-effects`).

Coverage follows the [Direct3D rasterization/sample-pattern specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#19.2.4%20Specification%20of%20Sample%20Positions).

## Local regression audit (2026-09-26)

Repeated-row masks now allocate and generate only the selected 2/4/8 sample
planes plus the original raster plane, instead of all 14 sample patterns.
This reduces that scratch allocation by 80%/67%/40%. All six Vulkan/DX12
quality captures remain byte-identical to the preceding implementation;
32 models / 320 wobble-combination images match the software reference on
each backend and quality. The latest local High Vulkan warm single-model
fixture measured 2.32 ms median (30 samples, render plus wait, not game FPS).
This supersedes the earlier note about still generating all quality masks.

- Rebuilt GPU AA checker passes Vulkan and DirectX 12 for all three SMAA
  qualities, 2/4/8-sample MSAA and TAA GPU/reference comparisons.
- Pixel-filter, MSAA reference, temporal reference, runtime configuration and
  full simulation tests pass, including AA menu cycling and integer scaling's
  main-page position, toggle and save-state round trip.
- The real-model checker matches SoftwareRenderer for 32 models / 384 images.
- Composition suites pass both backends after the palette-zero material-pair
  remapping fix. Native 1x remains unchanged.
- Captures `tmp/aa-final-original-*` and `tmp/aa-final-corneria-*` exercise
  120-frame Original/EX intro runs at High/2x. `tmp/aa-audit-*` exercises EX
  LEVEL6_6 gameplay. Comparison with the original raster path confirms the
  thin ship lines are source geometry, not added temporal trails.
- This is local desktop validation, not mobile/console/device certification
  or an assertion of a minimum FPS on all hardware.
