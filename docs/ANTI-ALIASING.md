# Anti-aliasing

In 3D Options, **AA Type** selects the algorithm and **AA Quality** selects
OFF / LOW / MEDIUM / HIGH. Existing settings default to FXAA.

- **FXAA:** existing fast directional contrast filter.
- **Sharp Edge:** chooses the lower-curvature axis to preserve straight edges.
- **Soft Edge:** blends four neighboring samples at detected contrast edges.
- **SSAA:** renders at least 2×, 3× or 4× the native dimensions, then averages
  samples into native-resolution cells. These correspond to 4, 9 or 16 samples
  per source pixel. An independently selected higher Render Upscale is retained.
  iOS stays capped at 2×. SSAA is substantially more expensive than spatial AA.

- **TAA:** jittered native model samples, reprojected history and depth rejection.
  Low/Medium/High use 65%/80%/90% history weight. Ordinary screen output requires
  the GPU path and mono output; calibrated Leia support is described below.
  The menu identifies unavailable configurations.
- **SMAA:** upstream SMAA 1x color-edge detection, area/search lookup blending
  weights, and neighborhood blending. Low/Medium/High increase search length;
  High also handles diagonal patterns and corners. Requires the native GPU path.

- **MSAA:** 2/4/8 coverage samples for Low/Medium/High, with pixel-frequency
  material shading and retained samples across model draws. Requires native GPU
  mono output without DLSS/FSR1. HUD and legacy sprite layers retain their native
  pixel coverage. Polygon, line, model-sprite, wireframe, cel, wave, colour-warp
  and wobble coverage is implemented and verified on Vulkan and DirectX 12;
  see `MSAA-INTEGRATION.md` for validation and platform limits.

HUD/portrait layers are excluded from AA filtering. Unsupported
TAA/SMAA/MSAA configurations do not silently substitute another AA algorithm.

## Native Leia AA

The calibrated Windows D3D12/Vulkan eye renderer supports FXAA, Sharp Edge,
Soft Edge and upstream SMAA 1x at LOW/MED/HIGH. It filters resident, independently
rendered eyes after appearance/global/bloom and before persistence/exposure.
An opt-in raster-owned mask excludes HUD, portraits, emissive ink and textured
art, including texture cutouts and later overlays. Alpha and OFF stay exact.
SMAA's area/search tables and edge/weight images are cached; production does not
read back eye pixels or upload a CPU-filtered image.

Native Leia SSAA renders each actual calibrated eye at 2x/3x/4x its recommended
dimensions for LOW/MED/HIGH (4/9/16 raster samples per output pixel). Ray,
appearance, depth and history passes use those working images; the final resolve
averages in linear light. A separate native-resolution coverage/ink raster keeps
HUD, portraits, wipes and emissive strokes sharp, including source opacity.
This is not integer enlargement or filtering a previously rendered SBS image.
SSAA costs substantially more GPU work and memory; it does not improve frame rate.

Native working images are bounded to 16,384 pixels per dimension and 64 megapixels
per eye. Larger requests/allocation failures report an error, rather than silently
substituting spatial AA. Resize/quality changes are transactional and cannot
replace pending compositor images. OFF restores the original native extent;
no supersampling targets/passes are created by a fresh OFF launch. These native
Windows limits do not alter the ordinary renderer or the iOS 2x cap.

Native Leia MSAA is connected to real 2/4/8 hardware samples; see
`MSAA-INTEGRATION.md`. Native Leia TAA is now connected to the actual eye owner
and frontend at LOW/MED/HIGH (65%/80%/90% history weight). It is not a spatial
filter substituted for TAA. The ordinary screen renderer's mono/upscaler
restrictions above still apply to that separate path.
See `STEREO-DISPLAY-UPGRADE.md` for native image-oracle/lifecycle evidence and
physical-display limitations.

The native TAA core uses each rigid surface's current/previous model and actual
calibrated eye projections, including sample jitter. It produces previous-minus-
current pixel motion, previous-camera depth and explicit correspondence validity
through the same raster visibility/depth/discard path. Unknown correspondence
is reactive rather than guessed from camera motion or draw-list position.
The game owner matches accepted unique entity keys, nonzero generations and
shape/strategy/type with unchanged source geometry. Recycled, deformed,
destruction, billboard and eye-facing line primitives are reactive; active
styled/distorted composition is conservatively reactive too. This does not
claim valid temporal motion for every primitive or reflected surface.
The resident temporal resolve rejects depth/visibility mismatches, clips history
to the eligible current neighbourhood and preserves source alpha/protected ink.
Each eye owns independent accepted history. Cancelled/rejected work preserves it,
even across candidate resizes; a reset during a pending presentation cannot be
undone by that old work. Per-eye history pairs and transactional resize
candidates have a one-GiB working bound, not a guarantee about free GPU memory.
The owner also applies a conservative four-GiB combined working estimate for
both eyes, guides, centre raster, scratch and potentially retained ray/trail/
phosphor/exposure images. Geometry/AS/driver storage is separate. These bounds
do not change the SSAA working-image limits above.

A separate presentation pass removes current sample jitter without feeding the
extra reconstruction back into history. It uses unjittered centre-raster
ink/ownership to retain protected artwork. All four RGBA/BGRA linear/sRGB
formats pass Direct3D12/Vulkan CPU-reference resolve and stationary-ramp checks.
The full game-owner sequence also passes independent moving-model/eye references,
generation/morph/cut rejection, combined ray/depth/global/bloom, real GPU waits,
rejected presentation, in-flight cancellation and exact OFF. Held frames retain
accepted colour/phase without accumulating rounding drift; an unused host FX
clock cannot keep a static preview jittering. A physical Leia panel has not been
validated, and the remaining enhancement/platform gaps are tracked separately.

## TAA integration and validation

`STARFOX_TEST_TAA=1` forces the native GPU temporal resolve for diagnostic
captures. It uses jittered model samples, native
motion/depth, previous-camera depth rejection, neighbourhood history clipping,
and exact HUD restoration. It does not stack with DLSS or FSR1, and stereo
views are not enabled for this path yet.

The resolved history remains on its jittered sampling grid, but a separate
presentation reconstruction cancels the current projection offset. This avoids
exposing the sampling sequence as preview wobble without feeding the extra
filter back into history. Non-geometry regions and protected HUD pixels remain
unshifted. An alternating-offset stationary-ramp regression verifies this on
Vulkan and DX12; the EX main-menu preview also executes the corrected path.

`starfox_gpu_temporal_aa_check vulkan` and
`starfox_gpu_temporal_aa_check direct3d12` compare the GPU resolve against its
CPU reference for motion, jitter, disocclusion, camera depth, HUD/alpha,
resizing, scene resets and cancellation. An in-game capture also confirms
resident execution. Original and EX intro geometry and EX gameplay captures
have been visually checked, including thin moving ship edges. This does not
establish validation on every device or in every stage.

The same GPU checker verifies SMAA's three quality levels, flat-color stability,
diagonal smoothing, HUD/alpha protection and intermediate-buffer clearing on
resize and reuse. The composition checker also compares SMAA's resident input
against its uploaded-input path.
