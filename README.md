# Star Fox Enhanced — native Metal shadow material candidate

This temporary source-only component integrates the separately CPU/SDK-checked
64-byte native RGBA opacity decoder into the actual Metal shadow producer and
its complete runtime shader sources. Native shadows read the resident records
and atlas directly, with declared payload and actual allocation-word bounds.
Legacy indexed and geometry-only paths are retained. Opacity is checked inside
the native intersection query, not by restarting a ray past a cutout.

The candidate embeds the exact decoder in runtime source strings rather than
depending on on-device filesystem includes. Runtime compilation disables unsafe
floating-point optimizations, with the appropriate old/new SDK option selected
by availability. Both primary and shadow rays receive the native material view.

CI compiles the entire Objective-C++ producer against its real project header
closure and original public SDL headers, then compiles/links both full runtime
shadow and reflection shader libraries using the existing Metal3.0 RT language
recipe and strict O3/FP flags. It retains the original native process/memory
observer. No GPU is executed. This is not an abbreviated complete25 test suite.

Native RGBA reflection production still deliberately refuses unsupported input:
its full colour/history/producer contract remains separate work. Empty caster
scenes, GPU execution, numerical/frame cost, full game integration and physical
device acceptance are not proven by compilation or source checks. Do not adopt
or call the whole rendering goal complete from these artifacts.

No main/release/ROM/assets/credentials are changed or published. AI/Codex is used
for programming, testing and documentation. Project sources are GPLv3; the
unmodified SDL public include directory retains its bundled upstream license.
