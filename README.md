# Temporary Metal reflection SDK checks

This is a source-only development snapshot, not a playable build or release.
It contains no ROM, private assets or credentials. Main and releases are unchanged.

All 25 reflection programs are compiled separately for both macOS and iOS.
The current stream root solver uses eight threads per group, with the matching
SDL resource layout. A 64-query batch still processes all 64 queries; optical
calculations, work bounds, ordering and deadlines are unchanged by scheduling.

The generated sources preserve the original SPIRV DontInline boundaries and
SDL Metal buffer bindings. Exact inverse checks preserve the original bodies;
100 negative descriptor controls reject altered bindings, entry points and
workgroup sizes. Nineteen programs match the previous accepted MSL byte-for-byte.

The compiler workflow and monitor are unchanged: Metal 3.0, O3, warnings as
errors, fast math disabled, and contraction disabled. It requires 6 GiB launch
headroom, caps whole-owned-tree residency at the lower of 6 GiB or launch
headroom minus 3 GiB, and keeps 1 GiB physical/1.5 GiB physical-plus-swap floors.
Every program's source hash, byte count and entry point are checked. Neither
empty libraries nor ambiguous process identities qualify as successful links.

These SDK checks target macOS 13/iOS 16. They do not raise the application's
deployment minimums, or establish runtime compatibility on older operating systems.
Compiler success alone does not prove GPU output, visual quality or frame cost.

Original project ownership, licensing and credits apply. SPIRV-Cross is by
Khronos Group contributors under Apache 2.0; its license is included in
`licenses/SPIRV-Cross.txt`.
