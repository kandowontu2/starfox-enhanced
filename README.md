# Temporary complete Metal reflection factory compilation

Source-only CI branch for Star Fox Enhanced. No ROMs, private game assets,
credentials or release publication are included.

The actual original sixteen pipeline factory sites now select the correct
twenty-five SDK-specific embedded Metal libraries. Descriptor resources,
uniforms, samplers, workgroups, shader calculations and dispatches stay intact.
The native DXIL/SPIRV path is preserved. The full source transformation must
invert exactly to the pinned original factory implementation.

CI compiles the actual factory module and its complete program provider for
macOS 11 and arm64 iOS 15, using the already qualified complete shader family
and the actual SDL interface. This does not launch a GPU or restart the
separate live driver diagnostic. Compile results do not establish application
linking/adoption, Metal precision, numerical correctness, performance or
physical hardware compatibility. Nothing is published as a release.
