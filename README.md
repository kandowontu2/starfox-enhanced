# Temporary complete Metal reflection offline build integration

Source-only CI branch for Star Fox Enhanced. No ROMs, private game assets,
credentials or release publication are included.

The actual original sixteen pipeline factory sites now select the correct
twenty-five SDK-specific embedded Metal libraries. Descriptor resources,
uniforms, samplers, workgroups, shader calculations and dispatches stay intact.
The native DXIL/SPIRV path is preserved. The full source transformation must
invert exactly to the pinned original factory implementation.

The earlier native component/SDL linkage checks pass for macOS 11 and arm64
iOS 15, including the full library inventory and macOS idle-owner/refusals.
This follow-up adds a source-driven CMake adapter: all25 SDK compiles are
serialized, retain strict flags and the original native resource observer,
preserve failed attempts, and produce the same embedded provider. Changed SDK
roots, versions and tool bytes invalidate cached outputs. No temporary CI
artifact download, GitHub API or runtime network access is part of that adapter.

The preceding native graph configuration passed on both SDKs. This branch's
manual CI now exercises that new source-built target end to end: compile all25
programs from their held Metal source using the same strict compiler/observer,
generate the complete read-only bundle, build the original factory/provider
component, and link real SDL. No previously compiled CI shader artifact is
downloaded. A fresh macOS native consumer verifies idle-owner/full25/refusals;
iOS is compiled/linked only. No GPU, game application, driver restart, Metal
numerical/frame-cost/physical acceptance or production adoption is implied.
No release is published. Actual failures remain preserved, never retried merely
because an observer sees a long-running native operation.

`cmake/BuildMetalReflection.cmake` exposes
`starfox_add_source_built_metal_reflection_history(target source_root sdk output_root)`.
The caller must already supply the real `SDL3::SDL3` target and Apple SDK.
`build-integration/CMakeLists.txt` is the standalone source-build consumer.
This prepares packaging; it does not invent the missing game producer records
or activate unqualified reflection-history reuse.

The preceding native link attempt failed on both SDKs because the harness
resolved the clang++ executable alias to clang and therefore omitted the C++
runtime at link time. This follow-up retains the actual clang++ invocation
name. That is the only harness-code change; all shader sources, factory sites,
component sources, descriptors, workgroups, strict flags and resource guards
are unchanged. The preceding failed native terminals remain preserved.

macOS now passes the real native link and complete payload/idle-owner consumer.
iOS's first successful native build had a subsequent artifact lookup failure
outside its .app bundle. The preceding iOS-only follow-up corrected that lookup
and passed the entire component/SDL build. Failed original receipts remain
preserved. Shader programs, factory code, bindings, strict floating-point flags
and the original resource guards are unchanged by this build-graph addition.
