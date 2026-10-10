# Temporary Metal reflection shader SDK checks

This source-only development snapshot is not a playable build, release or
production merge. It contains no ROM, asset pack or production Git history.
Its only trigger is manual dispatch; main, tags and releases remain untouched.

The complete 25-program matrix retains all 21 native reflection stages and
four legacy history/path programs. SPIRV-Cross at
`be71ee8c12cd7dc5ca8fa9581f708c2e8561fe2a` translated the pinned SPIRV programs
to MSL 3.0. Resource annotations now use SDL Metal's uniform-first buffer
layout, retaining the original reserved slots and workgroup dimensions.

Generated-code diagnostic fixes retain all unused SSA initializers and calls,
marking only unused return values. Three unsigned self-comparisons use a pure
identity helper; floating-point comparisons are unchanged. Shared values are
read by threadgroup reference after the original lane-zero writes and barrier,
without adding per-lane writes or changing shared storage.

Both SDK jobs attempt all 25 programs with warnings as errors, fast math
disabled and contraction disabled. SDK compilation does not prove runtime
binding behavior, numerical parity, optical quality or acceptable frame cost.
Every operation verifies the complete source hash, byte count and entry point.

The macOS monitor uses native process births/paths and whole-owned-tree
resident memory, not Windows private committed memory. It requires 6 GiB
reclaimable physical and physical-plus-free-swap memory before launch, caps
observed owned-tree residency at 2 GiB, and retains 1 GiB physical/1.5 GiB
physical-plus-swap running floors. It does not kill a compiler merely for
elapsed time, or automatically retry an unchanged compilation.

Original project ownership, licensing and credits apply to the shaders.
SPIRV-Cross is by Khronos Group contributors under Apache 2.0; its unmodified
license is included in `licenses/SPIRV-Cross.txt`.
