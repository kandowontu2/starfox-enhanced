# Temporary complete Metal reflection component native linkage

Source-only CI branch for Star Fox Enhanced. No ROMs, private game assets,
credentials or release publication are included.

The actual original sixteen pipeline factory sites now select the correct
twenty-five SDK-specific embedded Metal libraries. Descriptor resources,
uniforms, samplers, workgroups, shader calculations and dispatches stay intact.
The native DXIL/SPIRV path is preserved. The full source transformation must
invert exactly to the pinned original factory implementation.

CI now builds a reusable CMake static component and links a native consumer
against the real pinned SDL implementation for macOS 11 and arm64 iOS 15.
The original entire factory module, provider and all twenty-five unchanged
SDK libraries are linked. macOS checks the real component's idle lifecycle
and complete payload selection/refusal suite, without creating any GPU device.
iOS is linked only. The separate live driver diagnostic is not restarted.
This does not establish integration into the game application, source adoption,
Metal precision, numerical correctness, performance or physical compatibility.
Nothing is published as a release.

The preceding native link attempt failed on both SDKs because the harness
resolved the clang++ executable alias to clang and therefore omitted the C++
runtime at link time. This follow-up retains the actual clang++ invocation
name. That is the only harness-code change; all shader sources, factory sites,
component sources, descriptors, workgroups, strict flags and resource guards
are unchanged. The preceding failed native terminals remain preserved.
