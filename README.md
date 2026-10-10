# Star Fox Enhanced — native Metal camera clipping candidate

This temporary source-only component connects explicit primary camera-depth
planes to the entire native Metal shadow and packed-reflection producer, on top
of the previously compiled resident RGBA material and colour candidate.

AI/Codex is used for programming, testing and documentation.

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
observer. All five prior source assembly tests are retained and two camera
contract checks are added. No GPU is executed. This is not an abbreviated
complete25 test suite.

This candidate supplies the native material colour/coverage contract for the
existing packed reflection image. It does not yet implement the full calibrated
reflection-history/guide/ordered curved-path producer contract. Unsupported
calibrated liquid layer inputs explicitly refuse rather than returning an
incomplete image as successful. Empty caster
scenes, GPU execution, numerical/frame cost, full game integration and physical
device acceptance are not proven by compilation or source checks. Do not adopt
or call the whole rendering goal complete from these artifacts.

No main/release/ROM/assets/credentials are changed or published. Project sources are GPLv3; the
unmodified SDL public include directory retains its bundled upstream license.
