# Temporary Metal reflection shader SDK checks

This is a source-only development snapshot, not a playable build or a release.
It is not a production merge candidate. Its only trigger is manual dispatch.

All25 standalone Metal sources were translated from the exact SPIRV shader
programs bound by the current native reflection candidate:21 native stages
and4 legacy history/path stages. Native stages retain the complete current
polynomial, stream, root, guide, publish and diagnostic programs.

SPIRV-Cross at `be71ee8c12cd7dc5ca8fa9581f708c2e8561fe2a` produced MSL3.0
with only `--msl --msl-version 30000`. No source arithmetic rewriting or
invariant-float-math substitution was enabled. `shaders.json` records exact
input/output hashes and the full matrix; every compiler operation checks it.

Both macOS and iPhone SDKs compile and link all25 programs with warnings as
errors, fast math disabled and contraction disabled. Local translation and
SDK compilation do not prove SDL resource-binding ABI, runtime portability,
signed-zero/subnormal parity, reflection quality or acceptable frame cost.

The macOS resource monitor uses native microsecond process births/paths and
whole-owned-tree resident memory. This is explicitly not Windows committed
private memory. It requires6GiB reclaimable physical memory and physical plus
free swap before every launch, caps observed owned-tree residency at2GiB,
and retains1GiB physical/1.5GiB physical-plus-swap running floors. It performs
no elapsed native kill or automatic unchanged retry.

No ROMs, asset packs, player binaries, private transcripts, credentials or
inherited repository history are included. No main branch, tags or releases
are modified by this check.

Original project ownership, licensing and credits apply to the shaders;
this compilation snapshot does not grant a different license. SPIRV-Cross
is by the Khronos Group contributors under Apache2.0; its license is included
in `licenses/SPIRV-Cross.txt`.
