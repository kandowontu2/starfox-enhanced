# Linux Vulkan ray-tracing status

October 6 live-adapter correctness follow-up PASS: `SdlDxrShadows` now forwards the
Linux/native-probe route to the actual Vulkan owner. Resident uint32 shadows
remain distinct from RGBA/liquid/history buffers. Shared validation and actual
quality are retained. Cleanup failures keep the original owner/device for
retry; nonblocking cleanup cannot free pending native work. Windows continues
using its existing DXR bridge; no installed player has been replaced.

Adapter-routed sharp (384), MODEL compact/ordered (2,592), SCENE (5,184), planar
motion (3,600) and curved liquid motion (3,072) matrices pass on both NVIDIA
and Intel with unchanged independent oracle gates and whole-allocation exact
invalid-request recovery. Current RGBA/world/surface prefixes stay byte-identical
with history ON/OFF. Seventeen focused host regressions pass. Both lifetime
checks pass actual two-device pending/refused and drained replacement, distinct
uint32-shadow/RGBA ownership, 44 consumed-submit failures and 11 deliberately
late recovery markers, without tracked native resource leaks or polling waits.
Both GPUs' full water/lava presentation passes real 4-sample water and 2/4/8-sample
lava MSAA, independent XR sample colour comparisons, protected ink, actual
accepted-bank reuse and held/rejected/reordered/recycled source checks across
RGBA/BGRA linear/sRGB targets. Native queue stalls, waits, cancellation and
uncertain-submission recovery also pass. XR dispatch/compositor remain mocked.
Both GPUs' full model/analytic-plane/curved-liquid lifecycle checks also pass with
AA OFF/spatial/SSAA/TAA, genuine eight-lobe ordered MODEL paths, static/moving
planes and accepted wave clocks/origins (NVIDIA/Intel: 28,327,080/28,327,076 assertions). Protected ink
and actual history-colour reuse retain their original positive gates. These
lifecycle checks supplement the independent producer/MSAA image oracles.
All adapter probe runs have completed successfully; no run remains pending.
The full probe uses the actual XR-created SDL device/required feature chain
and an explicit successful-creation capability marker, not a UI availability
override. No history or quality bypass is added. See CURRENT-ACCEPTANCE.md.

These Windows native probes do not establish Linux/Deck execution, physical
panel compatibility, frame rate or artistic lava acceptance. Those and Apple
platform parity remain open. The producer-only entries below are historical;
their earlier statement that live routing was missing is superseded by this
completed native-probe integration, not by a claim that physical-platform
presentation or performance gates passed.

October 6 curved-liquid follow-up: the native Vulkan owner now produces
water/lava finite-hit motion with the real accepted wave/displacement frame.
Water preserves its canonical prefix (combined 88/104/108 bytes/pixel), lava
uses 88 bytes. CURRENT lighting/transmission/emission/Fresnel remain separate
from incident color; hidden water cannot seed visible model guides. An
optional bounded inverse solver follows RT in the same command/fence, with
an explicit compute-write/read barrier. It does not retrace the scene, submit
another command or read back source geometry. The solver is generated from
the authoritative HLSL include and has Shader-only SPIR-V; old frame missing
means no inverse PSO/dispatch. Liquid variants use a canonical 592-byte UBO;
existing 416/464/496-byte variants and ordinary reflection/shadow shader bytes
are unchanged. Finite old features, explicit IDs and actual old waves are
required; flat-plane or primary-flow approximations are not used.

The 3,072-case liquid matrix passes on NVIDIA RTX 5070 Ti Laptop and Intel
Graphics under the Windows owner probe. Current RGBA/world/surface prefix is
byte-identical ON/OFF in every case. Each GPU checks 9,428,384 records and
519,744 finite witnesses; 48 invalid requests recover the full allocation
exactly, with four release/reinitialize cycles. Lifetime checks pass 44
consumed-submit failures and 11 late markers per GPU, including two-pass
dispatch retention, partial PSO cleanup and old-frame-absent laziness. See
CURRENT-ACCEPTANCE.md for independent oracle gates and regression scope.

This completes this producer slice, NOT live calibrated application routing,
history color reuse, Linux/Deck presentation, FPS or artistic lava acceptance.
The live calibrated owner still routes through SdlDxrShadows; native Linux
Vulkan selection, capability creation and full presentation/accepted-history
tests are the next integration gate. Do not enable it merely by bypassing
history or making the option display available.

October 6 separated planar-motion update: native Vulkan now has canonical
88-byte mirror/gold motion records with actual old-plane/finite-feature
projection and explicit accepted mapping. Current light/highlight and gold
Fresnel are not transported with incident RGB. Sky/multi-hop/invalid old
features retain current colour but cannot enter single-hit reuse. Reflection
OFF keeps visible current ground/base with zero response; clear words are
initialized. The fifth on-demand history pipeline embeds its own shader and
496-byte UBO; existing history remains 464 and ordinary reflection 416 bytes.
No new descriptors, duplicate queries or source readbacks are added.

The 3,600-case `--planar-only` input-only binary64 matrix passes on Windows
NVIDIA/Intel (add `--integrated`), including 32,496 valid ground and 211,488 valid
model motions per GPU. ALL current RGBA pixels are byte-identical ON/OFF.
Current base/response, incident colour, exact IDs/validity and old geometry/
plane projection meet the explicit gates in CURRENT-ACCEPTANCE.md. Sixty-four
invalid requests recover whole allocations exactly, with four release cycles.
The initial fixture failed its model-motion coverage gate despite passing the
numeric checks; the final matrix adds direct finite mirrors, not a weaker gate.

Lifetime checks pass all five partial history variants, 28 missing-fence cases
and seven late recovery markers per GPU without native leaks, waits or replay.
Ordinary mirror/gold/lava regression and 15 host tests pass. Shader consistency
and Vulkan1.2 validation pass without Float64/Int64; ordinary generated shaders
are byte-identical. These are producer/owner checks, not application FPS or
accepted history reuse. Apple source assembly is not Metal compilation.
Compact/ordered MODEL and sharp-motion regressions also pass on both GPUs.
The SCENE recovery slice passes; its full prior matrix is not rerun here.

Curved water/lava history still declines; live calibrated Vulkan integration,
Linux/Deck/platform/Metal, remaining effects and current 3DS package/hardware
acceptance remain open. Installed players are untouched. Exact evidence and
failed initial coverage runs are in the external planar-motion checkpoint.
Older entries below retain their historical scope.


October 6 MODEL/planar scene-history update: native Vulkan now emits canonical
96/460-byte one/eight ordered records with explicit analytic mirror/gold hops,
current primary response and additive direct/base light. Finite hits, sky
escapes and four-hop budget exits are separate. A floor keeps one physical
sharp ray even when MODEL history has eight lobes; its record copies are exact.
Reflection OFF keeps current opaque ground/base lighting and inactive records.
Recording uses the original shading loops without duplicate ray traversal.
A fourth on-demand history pipeline uses the unchanged 464-byte UBO; it skips
creating an unused ordinary planar PSO. No source readbacks or new descriptors
are added; ordinary reflection/shadow generated headers remain byte-identical.

The 5,184-case `--scene-only` input-only binary64 image matrix passes on Windows
NVIDIA/Intel (add `--integrated`): 71,580,672 path records, 26,551,776 ground hops
and 16,995,888 exact sharp-floor record copies per GPU. Current RGBA is byte-
identical with history ON/OFF for ALL pixels. Coverage, identities, hop order,
terminal kind/count and inactive words are exact. RGB/base/response/depth gates
and input-derived barycentric conditioning remain explicit in CURRENT-ACCEPTANCE.
Ninety-six invalid/range requests and exact recovery pass via the added
`--scene-recovery-only` slice; `--scene-only` now includes it too. The producer
did not change between the full matrix and recovery extension.

All four history pipeline fault variants, 24 consumed-submit cases and six late
markers pass on both GPUs without native leaks, polling waits, replay or
cancellation. Compact/ordered MODEL, sharp history and ordinary planar/lava
regressions pass on both GPUs, as do 15 host tests, generated-shader consistency
and Vulkan1.2 SPIR-V validation. Only Shader/RayQueryKHR capabilities are needed.
Apple assembly is not Metal compilation; diagnostic readbacks/waits do not
establish application behaviour or FPS.

Separated motion and curved water/lava history still decline, not approximate
the scene contract. Live calibrated Vulkan history reuse/integration,
Linux/Deck presentation, Metal/platform, remaining effects and current 3DS
package/hardware acceptance remain open. Installed players are untouched.
See CURRENT-ACCEPTANCE.md and the external scene-history checkpoint. Older
entries retain their historical boundaries.

October 6 compact-lobe / ordered MODEL history update: the native Vulkan
producer now emits distinct canonical one/eight-ray compact records (40/124
bytes per pixel) and ordered MODEL specular paths (80/444 bytes per pixel).
Current primary response, unmodified incident RGBA and current secondary
throughput remain separate; finite terminals, sky escapes and four-hop budget
exits have exact kind/count/order. Recording hooks use the original shading
loop, without tracing reflection rays twice. Three history pipeline variants
are embedded and created on demand, with a 464-byte UBO and no new descriptors.
Ordinary reflection/shadow headers remain byte-identical to the sharp checkpoint.

`starfox_vulkan_reflection_history_check --model-only` passes on Windows NVIDIA/
Intel (add `--integrated`): 2,592 input-only binary64 cases and 45,059,664 per-ray
records per GPU. Current history ON/OFF RGBA is byte-identical for ALL pixels;
coverage, identities, terminal kinds/counts/hop order and clear words are exact.
RGB stays within one byte, current response within 2e-5, and depth within
max(.002, abs(expected)*2e-5). Finite barycentric gates now explicitly include an
INPUT vertex-scale/incidence/triangle-altitude float-rounding allowance above
the 2e-5 floor, not a fitted GPU error or new exclusion. See CURRENT-ACCEPTANCE.md
for the formula scope, observed errors and existing 1e-4 boundary band.
Forty-eight invalid/unsupported requests per GPU recover complete records
exactly; four nonblocking release/reinitialize cycles pass.

Sharp history and all five ordinary owner regressions pass on both GPUs.
Lifetime checks additionally cover all three partial history variants, 20
missing-fence cases and five deliberately late recovery markers per GPU.
An initial run failed the one-completion-poll-after-idle assertion; a detail
rerun passed without a renderer change. The final checker forces late markers
and uses bounded nonblocking completion polling, with retained resources,
no native wait/replay/cancel and zero tracked leaks. Fifteen host tests,
generated-shader consistency and Vulkan1.2 SPIR-V validation pass; no Float64/
Int64 capability is needed. Apple assembly is not Metal compilation.

Analytic ground/scene/liquid history still declines. Compact/ordered producer
records do not prove previous-frame correspondence, shared history reuse,
calibrated application integration, Linux/Deck presentation, FPS, Metal/platform,
artistic lava or final 3DS acceptance. Those integration/platform/effect/package
tasks remain open. Installed players are untouched. See CURRENT-ACCEPTANCE.md
and the external MODEL-history checkpoint for exact evidence. Older entries
describe their own checkpoints.

October 6 sharp MODEL history update: the optional native producer emits the
canonical 52-byte current colour/motion/identity/witness layout. Its motion is
the previous finite reflected secondary feature, with explicit accepted IDs,
not inferred order or primary receiver velocity. Previous geometry/mapping are
copied resident-to-resident from the same submitted source. Invalid prior poses
or finite-face/depth/screen correspondence have zero motion; sky escapes retain
current colour without finite-hit witnesses. Separated, rough-lobe, ordered and
liquid history contracts explicitly decline pending their own producer parity.

`starfox_vulkan_reflection_history_check` passes on Windows NVIDIA/Intel (use
`--integrated` for Intel): 384 input-only binary64 cases, 1,177,864 checked pixel
records, exact IDs/coverage/validity and unchanged numeric gates per GPU.
History ON/OFF current RGBA is byte-identical. Twenty-six malformed/unsupported
requests recover all records exactly and four release/reinitialize cycles pass.
All five existing native owner suites pass on both GPUs, including legacy/native
materials, resident sky, mirror/gold/lava, rough conductors and genuine empty
scenes; the independent NVIDIA model shader regression passes too. Existing
RGB/coverage/recovery gates are unchanged.
The expanded lifetime checker also passes temporary history shader/partial
pipeline cleanup and twelve missing-fence cases over shadow/RGBA/history on
both GPUs. Fifteen related rebuilt host tests, generated-shader consistency and
Vulkan1.2 SPIR-V validation pass. No Float64/Int64 capability is needed.

The history pipeline is on-demand with a 464-byte UBO; ordinary reflection and
shadow remain 416/352 bytes with byte-identical shader headers to the preceding
lifetime checkpoint. No source readbacks or new descriptors are added, and
history OFF keeps the original RGBA contract/capacity accounting. Diagnostic
readbacks/waits do not establish app behaviour or FPS. This is not live
calibrated history reuse, Linux/Deck ABI/presentation, Metal, platform, artistic
lava or final 3DS acceptance. Rough/per-lobe/path/liquid parity and application
integration remain open. See CURRENT-ACCEPTANCE.md and the external sharp-history
checkpoint for exact evidence and remaining boundaries.

October 6 native-owner lifetime update: nonblocking completion/cleanup queries
retain native resources while work is pending or fence queries fail. Explicit
blocking cleanup and slot reuse now respect failed waits. Cleanup invalidates
borrowed outputs and certifies every slot before freeing any native resource.
A consumed submission that returns no fence is never cancelled or replayed;
an empty later same-queue marker supplies its recovery fence. Initialization,
reflection-pipeline and buffer-allocation failures roll back transactionally,
including partial variant pipeline handles. Retained larger water/shadow image
capacities are reported separately from source/AS/scratch/driver memory.

`starfox_vulkan_ray_lifetime_check` passes on Windows NVIDIA and Intel (use
`--integrated` for Intel). Diagnostic-only linker wrappers inject allocation,
binding, pipeline, recording, wait/query and consumed-submit fence failures.
A real timeline gate holds three producer slots pending; polling/try-release
does not wait or free live resources. Eight repeated consumed-submit cases
recover without replay, restore all 3,072 baseline pixels exactly and release
all tracked native handles. Full-water plus shadow image capacity is 86,016
bytes and stays accounted after a smaller request. Full populated-scene owner
regressions also pass on both GPUs, including resident sky, mirror/gold/lava and
model conductor transport, with unchanged RGB/coverage gates. Empty-scene,
transition and malformed-recovery regressions pass on both GPUs too. Fifteen rebuilt host tests
and the generated-shader consistency check pass. No shader/uniform change or
player installation is introduced. This is not Linux/Deck ABI, application
cleanup/presentation, device-loss, FPS or final 3DS acceptance. Reflection
history/per-lobe/path outputs and calibrated application integration remain
open, along with Metal/platform, remaining effects and 3DS packaging/device
verification. See CURRENT-ACCEPTANCE.md for evidence and remaining boundaries.

October 6 empty-scene update: complete padded native RGBA frames with zero
vertices now build a real zero-instance TLAS and retain analytic water/mirror/
gold/lava receivers and current resident-cube transport. No dummy caster, BLAS
or source-vertex upload is used. Empty CPU shadow scenes produce clear masks.
Instance addresses are explicitly aligned; empty and populated scratch buffers
use the device's queried acceleration-structure scratch alignment. Malformed
or incomplete native geometry still declines and clears borrowed output.

`starfox_native_rayquery_check --native-empty-only` and
`starfox_vulkan_shadow_owner_check --native-empty-only` pass on Windows NVIDIA/
Intel; add `--integrated` for Intel. The shader checker also accepts
`--shadow-cutouts --native-empty-only`. Each producer checks 672 empty reflection
cases, 25 populated/empty transitions and 288 exact-clear shadow cases; the owner
also checks 10 malformed requests through both producers, recovery and release/
reinitialize. RGB error stays within one byte and water guide/depth gates are
unchanged, without source-boundary exclusions. No shader, uniform or descriptor
change is introduced. Full populated-scene owner regressions pass on both GPUs;
independent NVIDIA shader regressions and 14 rebuilt host tests also pass.
This is not a Linux installation, application integration,
FPS or artistic lava-quality result. See CURRENT-ACCEPTANCE.md and the external
native-empty checkpoint for exact evidence. History/per-lobe/path outputs,
retained failure/completion and calibrated application integration remain open,
along with Metal/platform, remaining effects and current 3DS package/device
verification. Older entries describe their checkpoints, including earlier
empty-scene exclusions.

October 6 native-model update: native linear/sRGB model reflections now match
the calibrated normal-offset bias, stable eight-lobe roughness, linear-light
averaging and material/gold/copper conductor Fresnel. Opt-in secondary model
transport attenuates at each hit and resolves the current sky at the four-hop
budget. Legacy encoding0 remains unchanged. The cached native model PSO uses
constant2; water and planar/lava variants enable it together with their own
constants. No images/descriptors/readbacks or uniform growth are introduced.

`starfox_native_rayquery_check --native-model-only` and
`starfox_vulkan_shadow_owner_check --native-model-only` pass on Windows NVIDIA/
Intel; add `--integrated` for Intel. Each runs 1,728 input-only cases covering
solid/RGBA/native-palette cutouts, linear/sRGB, four conductors, three roughness
values, global transport ON/OFF, fallback/cube, four-bounce/grazing/clipping and
asymmetric-camera witnesses. RGB error is at most one byte; source ownership is
exact. Invalid-specular encoding recovery and release/reinitialize pass. The
frozen legacy shader removes the new function; neither variant needs Float64/
Int64. Fourteen related host tests pass. See CURRENT-ACCEPTANCE.md and the
external native-model checkpoint for exact gates/evidence. All earlier complete
shader/owner regressions also pass on both GPUs, including 26 colour styles,
water/camera ranges, resident cube and planar/lava, malformed recovery and reuse.
This is not an
installation, Linux/Deck/application/FPS or final 3DS acceptance. Reflection
history/per-lobe/path outputs, empty scenes, retained failure/completion and
calibrated application integration remain open, along with Metal/platform,
remaining effects and 3DS package/device verification. Older entries describe
their checkpoints.

October 6 native-surface transport update: native mirror/gold retain their
shadowed authored base with reflection OFF and use linear/sRGB Fresnel and
highlight transport. Native primary/secondary lava decode emission once and
encode the accumulated reflected light once. Legacy encoding0 keeps its
previous recipes and ground/model ownership remains exact. The native planar/
lava pipeline is a cached on-demand specialization, removed from the ordinary
legacy variant; unused indexed-palette uploads are skipped for all native
materials. Neither adds source readbacks, images/descriptors or float64/int64.

`starfox_native_rayquery_check --native-ground-only` and
`starfox_vulkan_shadow_owner_check --native-ground-only` pass on Windows
NVIDIA/Intel; add `--integrated` for the latter adapter. Each runs 1,296
input-only cases over solid/RGBA/native-palette coverage, linear/sRGB, banked/
affine/small-unit transforms, reflection0/.7/1, ordinary/ground-only and
model-mirror transport, fallback/cube illumination and two wave times. RGB
error is at most1 byte. Thirty invalid-transform/recovery and release checks
pass per owner. Complete earlier hardware regressions and14 related host tests
also pass. CURRENT-ACCEPTANCE.md and the external native-ground checkpoint
record exact gates and evidence. Lava's artistic scalar colour recipe is
shared: this does not prove realism, stage appearance or FPS. Installed players
are unchanged; no Linux/Deck/application or final 3DS acceptance is claimed.
Model conductor/history parity, empty scenes, retained failure/completion and
calibrated application integration remain open, along with Metal/platform and
3DS package/hardware verification. Older entries describe their checkpoints.

October 6 resident-environment update: native reflections consume the calibrated
GPU-resident six-face sky and explicit eye-to-cube rotation. Linear/sRGB taps
filter in linear light and reproject across face seams. Cube/material bytes share
one resident range copy; no CPU source-sky readback or new source-image upload,
descriptor or separate cube allocation. Native cube requests skip unused palette
and dummy-image allocations/uploads. Reflection uniforms are now 416 bytes;
shadows remain 352. Exact shader and production SDL/Vulkan owner tests pass on
Windows NVIDIA/Intel: 506 cube and 36 water/cube submissions per device, all six
faces and seam/corner/sharp/rough witnesses, plus 21 malformed/recovery checks.
Both complete legacy/material/water/range regressions and 14 host tests pass.
`starfox_native_rayquery_check --native-environment-only` and
`starfox_vulkan_shadow_owner_check --native-environment-only` run these checks;
add `--integrated` for the integrated adapter. This is not a new Linux gameplay
build, installation or FPS proof. Native conductor/liquid encoding/history,
empty scenes, failure retention and calibrated application integration remain
open, along with Metal/platform acceptance. Details are in CURRENT-ACCEPTANCE.md
and the external resident-cube checkpoint; older entries keep their own counts.

October 6 primary-range update: optional calibrated near/far planes now clip
primary model/ground/water visibility and hidden-water layers using +Z depth.
Secondary casters, reflections/refraction and caustic rays retain their own
transport bounds. Omitted ranges preserve legacy limits; invalid or
float-collapsed ranges clear borrowed outputs. Shadow/reflection uniforms are
now 352 bytes. Exact shader and actual pinned SDL/Vulkan owner range checks
pass on Windows NVIDIA/Intel, including clipped-but-reflected/casting objects,
sub-unit and >65536 receivers, cutouts, asymmetric cameras and newly revealed
native water. Full per-device owner totals are now 1,648 shadow, 3,504 native
reflection, 200 indexed reflection and 936 water submissions, plus malformed
request/reuse checks. Fourteen related host tests pass. Installed players
remain unchanged; Linux/Deck gameplay, calibrated resident environment/history
and compositor integration, Metal parity and performance remain unverified.
See CURRENT-ACCEPTANCE.md and the external primary-range checkpoint for details.
Earlier entries below describe their own checkpoints and counts.

October 6 native material/water update: native resident solid/RGBA/palette
materials now support binary shadow coverage and calibrated colour styles in
reflections. Calibrated water accepts explicit linear/sRGB source colour,
nearer-liquid ownership, optional normal/depth guides and hidden-world layers
in one resident buffer. The water pipeline is specialized on demand; surface-only
requests omit hidden-water tracing. Release/reinitialize now resets destroyed
native handles. Exact shader and actual pinned SDL/Vulkan owner tests pass on
Windows NVIDIA/Intel, with 864 water, 3,504 native reflection and 1,408 shadow
submissions per device. See CURRENT-ACCEPTANCE.md for independent-reference
scope, failure recovery and remaining Linux/Deck/application/history/Metal
boundaries. This is not a new installed Linux build or performance acceptance.
The older unsupported-native-RGBA statements below describe earlier checkpoints.

October 6 shadow-material update: native shadows now receive current indexed
material records and source byte texels, including the GPU-resident record
offset/range path. Cutout-aware primary and light rays share coverage semantics
with reflections. Geometry-only/solid scenes retain their opaque fast path;
native RGBA materials are explicitly declined. Both NVIDIA and Intel pass the
independent HARD/LOW/MEDIUM/HIGH shader masks and640 submissions through the
actual `VulkanHardwareRt`/pinned SDL owner, with separate CPU-record and poisoned
CPU/GPU-offset tests. The checker-only Windows probe does not enable this
producer in Windows players and does not establish Linux/Deck acceptance or
FPS. `starfox_vulkan_shadow_owner_check [--integrated]` runs the owner test;
`starfox_native_rayquery_check [--integrated] --shadow-cutouts` runs the separate
shader test. Current counts and limitations are in `CURRENT-ACCEPTANCE.md`.

October 6 indexed-material update: primary and reflected ray-query traversal
now tests source texture coverage instead of forcing every triangle opaque.
Transparent source texel zero is skipped before colour-base wrapping; solid
ink zero and opaque texels that wrap to palette zero are retained. Ordinary
reflections, bounded mirror bounces, underwater receivers and caustic visibility
use the same coverage rule. Candidate traversal does not fetch palette colours
until the committed surface is shaded. The current native Vulkan checker
passes independent binary64 ray/UV cutout comparisons on NVIDIA and Intel,
alongside the existing liquid/planar-ground checks. This is Windows native
Vulkan shader validation, NOT a new Deck/Linux owner, installation, shadow
cutout, full-stage or performance result. See `CURRENT-ACCEPTANCE.md` for scope.

October 6 current-source note: the checked-in GLSL/SPIR-V now resolves actual
animated displaced lava at primary and secondary hits and bounded mirror/gold
transport, rather than the original flat floor or the water-like mirror tint.
The exact native shader runs directly (without DXR/SDL interop) on Windows
NVIDIA/Intel Vulkan devices. Both pass the independent secondary and primary
ground checks indexed in `CURRENT-ACCEPTANCE.md`, including exact no-ground
coverage. This is shader correctness, not a new Linux SDL ownership/lifetime,
Deck installation, whole-stage or performance acceptance. Enhanced-sky sampling
also exists in the current shader; the earlier no-photographic-sky statement
below describes its older checkpoint, not current source. Full photographic
Linux/device reflection parity still needs a scoped actual-native run.

The Linux SDL GPU path can now request Vulkan 1.2 buffer device addresses,
`VK_KHR_acceleration_structure`, `VK_KHR_ray_query`, and their supporting
extension through the pinned SDL 3.4.14 build. It falls back to an ordinary
Vulkan GPU device if that request fails. The bridge and capability probe are
implemented in `src/render/vulkan_ray_support.cpp` and
`tools/check_vulkan_ray_support.cpp`.

The native path uses SDL's pinned Vulkan command bridge to build BLAS/TLAS
from GPU-resident model geometry and dispatch ray-query shadows and reflections
to SDL GPU buffers consumed by the ordinary compositor. The menu reports `ON`
and offers reflective surfaces only when the ray-query device and bridge are
available; otherwise it retains the portable compute-shadow fallback.

`tools/check_vulkan_ray_support.cpp` probes the hardware path, dispatches
CPU-upload and GPU-resident shadow geometry, then dispatches a GPU-resident
reflection and checks its center pixel. On the Steam Deck (2026-09-25), it
reported `Vulkan ray-query device ready`, center shade `160`, reflection
RGBA `0xff302010`, and a reflected GPU-resident palette material of
`0xff11aa22`. The latter exercises the live material-buffer offset rather
than only a CPU-uploaded synthetic scene. A separate 30-frame Corneria GPU
gameplay run used both GPU-resident Vulkan ray-query shadows and reflections
without a declined ray pass. This proves the integration on that device but
is not a full-stage or 90-FPS performance guarantee.

LOW/MEDIUM/HIGH use one, two, or four ray queries per pixel. The reflection
pass samples model materials from the live palette and supports a ground
receiver. Background misses now sample an authored BG2 panorama rasterized
on the GPU on the same command stream. A Deck ray-query probe returned the
authored tile's palette colour (`0xffdd6633`) instead of the flat environment
fallback. Photographic enhanced-sky artwork is not yet mirrored in this
Linux pass; it reflects the underlying cartridge BG2.
