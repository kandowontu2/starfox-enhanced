# Star Fox Enhanced — native Metal canonical build integration candidate

This temporary source-only component adds explicit linear/sRGB reflection
transport, seamless resident environment cubes and calibrated mirror/gold/copper
model lobes to the entire native Metal producer and both runtime shaders.
It also implements calibrated transparent water, its optional hidden-world
colour plane and its camera-space normal/depth plane in one retained allocation.

AI/Codex is used for programming, testing and documentation.

The public CMake fragment generates both shared runtime headers directly from
the authoritative liquid and colour equations. It declares both generators,
all scalar kernels, both colour inputs and both templates as regeneration
dependencies. The runtime source-check tool accepts the same build-generated
helper directory. An actual CMake/Ninja NONE-project check verifies byte-exact
headers/runtime assembly and regeneration after changes to temporary canonical
input copies. This is build-source wiring, not full application linking.

The native owner now provides nonblocking completion polling and cleanup,
with output-image capacity accounting (not available VRAM). Pending/failed
completion invalidates borrowed outputs but retains all resources and the
device. A submission without a fence is consumed once; an empty later ordered
marker certifies completion, never replaying or cancelling the submitted work.
Blocking cleanup certifies every slot before releasing any allocation. Failed
waits cannot partially tear down a live owner or free ARC-owned GPU resources.
Ordinary slot reuse retains its existing wait semantics, now with failed-fence
recovery; polling/try-cleanup never wait. Consumers own their SDL device lifetime.

Encoding 0 retains the legacy gamma approximation; 1 transports linear colour;
2 decodes/encodes the canonical sRGB equations. Liquid emission, refraction,
mirror accumulation and model lobes use that negotiated domain. Resident cube
faces follow +X,-X,+Y,-Y,+Z,-Z in the same tracked native material allocation;
bilinear taps crossing a face edge reproject onto the adjacent face. Size,
alignment, format, orthonormal rotation and actual native allocation bounds
are validated. No CPU cube/atlas upload is introduced. Calibrated conductor
model transport is an explicit independent choice; it retains the full eight
rough-lobe samples and four nested mirror bounces of the Vulkan native path.
The native primary ground uses compensated two-float plane evaluation to
retain banked horizon cancellation. Legacy and secondary ray limits remain.

Calibrated water uses the exact shared analytic-liquid geometry, adapted only
for Metal syntax. Live source colours are validated and decoded once. Refraction
uses the correct entering/exiting index, handles total internal reflection,
queries opaque submerged geometry and supplies a virtual bed when appropriate.
Transmission, visibility, caustic focusing, Fresnel and specular response are
computed in linear light. Nested mirrors continue through the same water optics.
Hidden-world water is computed independently of primary opaque visibility;
only visible liquid stores a positive camera depth and surface normal. All
inactive surface data and off-ground world colours are explicitly cleared.
The legacy RGBA row pitch is unchanged. Storage accounts for the full 20/24-byte
per-pixel canonical layer prefix, checks overflow and actual native capacity,
and publishes the layer descriptor only after successful submission.
Calibrated mirror/gold ground keeps its authored diffuse base when reflections
are off, with conductor Fresnel when on. Primary calibrated native models use
the native reflection lobe path independently of nested-model continuation.

Both APIs accept an optional `PrimaryRayRange`. The host rejects invalid ranges
and distinct double planes that collapse to one native float before allocating
or submitting work. The primary direction retains z=1, so the bounds are camera
depth, not normalized ray distance. Primary models and ground use those bounds;
secondary light, reflected, refracted and nested-mirror rays retain their
original independent 65536 limit. Omitting the range preserves legacy limits.
The host and runtime shader uniform layouts are updated together.

Native shadows still read the resident records and atlas directly, with
declared payload and actual allocation-word bounds. Legacy indexed and
geometry-only paths are retained. Opacity is checked inside the native
intersection query, not by restarting a ray past a cutout.

The candidate embeds the exact decoder in runtime source strings rather than
depending on on-device filesystem includes. Runtime compilation disables unsafe
floating-point optimizations, with the appropriate old/new SDK option selected
by availability. Both primary and shadow rays receive the native material view.
Reflected, refracted and nested-mirror queries now use that same resident
coverage view. Native RGBA/indexed-atlas reflected colours use the canonical
calibrated colour equations: already-styled kind2 solids pass through, while
kind3 texture samples are decoded, styled once and encoded according to their
submitted flags.

CI compiles the entire Objective-C++ producer against its real project header
closure and original public SDL headers, then compiles/links both full runtime
shadow and reflection shader libraries using the existing Metal3.0 RT language
recipe and strict O3/FP flags. It retains the original native process/memory
observer. All thirteen prior source assembly/contract checks are retained with
the intentional extended ABI, and four liquid/layout checks are added. A further
check executes the real CMake embedding/regeneration fragment.
No GPU is executed. This is not an abbreviated
complete25 test suite.

This candidate supplies the native material colour/coverage contract for the
existing packed reflection image and canonical current liquid layer prefix.
It does not yet implement the full calibrated reflection-history/ordered
curved-path producer contract. Layer requests without validated calibrated
source colour explicitly refuse rather than returning an incomplete image.
Empty caster
scenes, GPU execution, numerical/frame cost, full game integration and physical
device acceptance are not proven by compilation or source checks. Do not adopt
or call the whole rendering goal complete from these artifacts.

No main/release/ROM/assets/credentials are changed or published. Project sources are GPLv3; the
unmodified SDL public include directory retains its bundled upstream license.
