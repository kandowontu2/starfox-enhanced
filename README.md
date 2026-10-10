# Temporary Metal reflection shader SDK checks

This source-only development snapshot is not a playable build, release or
production merge. It contains no ROM, asset pack or production Git history.
Its only trigger is manual dispatch; main, tags and releases remain untouched.

This follow-up translates the current exact sign-corner native candidate.
It keeps the complete directed interval calculations and original fallbacks;
only guarded mathematical corner extrema avoid redundant products. Six SPIRV
programs changed; nineteen regenerate the identical accepted Metal sources.
Source transformations are exactly reversible, and all 100 changed descriptor,
workgroup and entry-point controls reject. No runtime speedup is inferred.

The complete 25-program matrix retains all 21 native reflection stages and
four legacy history/path programs. SPIRV-Cross at
`be71ee8c12cd7dc5ca8fa9581f708c2e8561fe2a` translated the pinned SPIRV programs
to MSL 3.0. Resource annotations now use SDL Metal's uniform-first buffer
layout, retaining the original reserved slots and workgroup dimensions.

Generated-code diagnostic fixes retain all unused SSA initializers and calls,
marking only unused return values. Typed unsigned self-comparisons use a pure
identity helper; floating-point comparisons are unchanged. Shared values are
read by threadgroup reference after the original lane-zero writes and barrier,
without adding per-lane writes or changing shared storage.

Both SDK matrices attempt all 25 programs with warnings as errors, fast math
disabled and contraction disabled. SDK compilation does not prove runtime
binding behavior, numerical parity, optical quality or acceptable frame cost.
Every operation verifies the complete source hash, byte count and entry point.

The macOS monitor uses native process births/paths and whole-owned-tree
resident memory, not Windows private committed memory. It requires 6 GiB
reclaimable physical and physical-plus-free-swap memory before launch, caps
observed owned-tree residency at the lower of 6 GiB or launch physical
headroom minus 3 GiB, and retains 1 GiB physical/1.5 GiB
physical-plus-swap running floors. It does not kill a compiler merely for
elapsed time, or automatically retry an unchanged compilation.

This cloud-only 14 GiB runner recipe is not a pass of the earlier 2 GiB cap.
Each SDK/program has its own job so a large compiler failure cannot hide the
remaining programs. All 25 source pins are checked in each job; full compiler
coverage requires all 50 SDK/program jobs. The SDK target/root and original
tool name are explicit, linker warnings reject, and each MTLB must contain its
exact expected kernel token. No empty library is accepted as linked success.
Xcode's observed launcher-to-versioned-compiler exec is accepted only for
the same native PID and birth, with the destination binary path and SHA-256
pinned from the selected toolchain before launch. Transitions are recorded;
an unverified path, changed binary or reused incarnation still fails.

This candidate restores static noinline attributes only for functions whose
held original SPIRV OpFunction has DontInline set. SPIRV-Cross had replaced
those boundaries with forced inlining, and the actual jets compiler exceeded
the 6 GiB cap. Function bodies, arguments, all resource slots, work/thread
counts, O3/strict-float flags and resource limits are unchanged. The remaining
scalar-uint self-comparisons use the existing pure identity helper; no
floating-point comparison is rewritten. Exact inverse source audits retain
all original bytes aside from these reviewed annotations/uint expressions.
Successful SDK linking would still not prove runtime behavior or performance.

This follow-up keeps all 25 generated shader files and the source manifest
byte-identical. Only the SDK language/deployment profile changes: platform-
specific Metal 2.3 with explicit macOS 11/iOS 15 targets, matching the app's
deployment minimums. O3, warnings as errors, disabled fast math/contraction,
the complete 50-job matrix, linker checks and native resource guards remain.
No unsupported shader is omitted or replaced. Compilation must validate this
candidate; old-system runtime behavior and numerical parity still need tests.

Original project ownership, licensing and credits apply to the shaders.
SPIRV-Cross is by Khronos Group contributors under Apache 2.0; its unmodified
license is included in `licenses/SPIRV-Cross.txt`.
