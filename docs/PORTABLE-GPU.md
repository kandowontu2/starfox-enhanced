# Portable GPU effects

The SDL GPU backend uses Vulkan on Linux/Android and Metal on macOS/iOS.
It shares the HLSL effect algorithms with Windows D3D11. Shader translation is
offline, not a download or dependency added to the running game.

Coverage: model/world effects and intensity, smoothing, 2D polygon/overlay
filters, Enhanced Lighting, HDR, chromatic aberration, both bloom controls,
anti-aliasing, shadow-mask blending, bloom layer splitting, and legacy 1440p
model-layer separation. Supported paths present GPU textures directly, with
capture/history readback on demand. Unsupported devices explicitly log fallback.

The connected pipeline also supports resident native geometry, rasterization
and composition; see GPU-MIGRATION-STATUS.md for current evidence and limits.
Switch/Vita GPU effects remain outside this SDL backend: SDL GPU does not
provide those consoles' backend paths.
Metal and Android runtime validation requires their devices/toolchains; passing
Linux tests is not evidence of those platforms working.

## Rebuilding shaders

Validated generators: DXC v1.9.2607 with SPIR-V enabled and SPIRV-Cross commit
`be71ee8c12cd7dc5ca8fa9581f708c2e8561fe2a`. The Windows SDK DXC used for DXR
does not necessarily include SPIR-V code generation.

```
python tools/generate_portable_effects.py --dxc PATH_TO_DXC --spirv-cross PATH_TO_SPIRV_CROSS
python tools/generate_portable_effects.py --check
python tools/generate_portable_shadows.py --dxc PATH_TO_DXC --spirv-cross PATH_TO_SPIRV_CROSS
python tools/generate_portable_shadows.py --check
python tools/generate_portable_shadows.py --shader raster_portable --dxc PATH_TO_DXC --spirv-cross PATH_TO_SPIRV_CROSS
python tools/generate_portable_shadows.py --shader raster_portable --check
```

Both xBRZ-enabled and minimal shader variants are generated. Preserve the
generated header files in source distributions; GPU xBRZ carries the same GPLv3
requirements as the CPU implementation.

## Checks

Build/run `starfox_gpu_effects_check` with `SDL_GPU_DRIVER=vulkan` or `metal`.
The check compares effects with CPU references and batched versus separate GPU
passes. It requires a working SDL GPU device; do not silently skip failures.
Windows builds with SDL GPU support now select Vulkan by default. Set
`SDL_GPU_DRIVER=direct3d12` to check the D3D12 backend; builds without that
support retain the D3D11 path. `STARFOX_TRACE_GPU=1` reports direct presentation.
The effects diagnostic accepts SPIR-V, MSL and DXIL, matching the renderer.

Screen-space water/mirror/gold reflections now resolve after environment
replacement, so their immutable source contains the enhanced sky, live palette
and fractional scrolling. The resolve reuses the existing GPU ping-pong images
and command; it does not add readback, another texture, or a second physical-ray
finish. CPU row workers use the same two-pass ownership. Excluded HUD taps retain
their bilinear coverage rather than normalizing a tiny edge tap into a bright
full-strength reflection. Basic nonreflective surfaces and the physical-ray
path do not run this extra screen resolve.

The bounded `STARFOX_TEST_SCREEN_REFLECTION_ONLY=1` diagnostic checks Auto water,
water, mirror and gold at 1×/3×/6× with banked horizons, procedural/photographic
sky, fractional scrolling, foreground models, HUD exclusion and source alpha.
It separately qualifies the environment shade, then independently mixes that
renderer’s completed UNORM source with a one-channel-unit bound; it does not
raise the bound to conceal accumulated quantization. On Windows,
`STARFOX_TEST_D3D11_SCREEN_REFLECTION_ONLY=1` runs the same fixture against the
legacy D3D11 effects implementation. Neither is a whole-game performance or
mobile-device acceptance test.

Verified locally: Vulkan effect comparisons and captured gameplay smoke runs on
Windows and Linux/WSL; the Linux regression suite passed 38/38 after integration.
Floating-point effects permit at most one channel unit
of difference in the test fixtures; styles and shadow blending match exactly.

## Portable shadow tracing (post-0.0.6.5, unreleased)

`PortableShadows` traverses a packed BVH in a Vulkan/Metal compute shader when
DXR is unavailable. It traces the same eight fixed area-light rays at full
resolution, including sloped model receivers and an optional ground plane.
This is GPU compute ray tracing, not use of dedicated ray-tracing units.
Geometry is rebuilt from the current frame; float BVH bounds round outward.
The connected compositor consumes the mask on the GPU. Readback remains for
explicit diagnostics and fallback paths.

`starfox_portable_shadows_check` compares masks, measures warmed isolated
timings, and tests changes in geometry, resolution, light, winding, ground and
empty scenes. Normal operation requires hardware Vulkan; software Vulkan
drivers are slower than the native CPU tracer and must not be selected as an
optimization. Set `STARFOX_TEST_SOFTWARE_GPU=1` only for shader parity testing.
`STARFOX_DISABLE_PORTABLE_SHADOWS=1` retains the CPU fallback; on Windows,
`STARFOX_DISABLE_DXR=1` exercises the portable path instead of DXR.

Verified on Windows/NVIDIA Vulkan and Linux/WSL software Vulkan. The hidden
Windows gameplay smoke used portable shadows plus direct GPU bloom presentation,
captured a frame and exited cleanly. Metal device testing remains outstanding.

## Native raster pipeline

Build `starfox_gpu_raster_check` for exact CPU/GPU comparisons and isolated
overdraw timings. `starfox_raster_commands_tests` runs the CPU command replay
checks without a GPU. `tools/check_gpu_native.ps1` alternates real hidden-window
CPU/GPU capture paths at three render scales and rejects differing captures.
`STARFOX_TEST_GPU_RASTER=1` tests the older readback path. The connected
native path keeps output on the GPU through composition and effects on
supported SDL GPU Vulkan/Metal renderers by default. Use
`check_gpu_native.ps1 -DefaultPipeline` to verify real captures without test
enable flags. `-RayTracing` also checks the resident shadow backend.
Full scene/transition and physical-device acceptance remain outstanding.
`starfox_gpu_composite_check` verifies 108 composition/effect cases, including
early shadow ordering, resident shadow buffers and all four existing 2D filters.

Portable shadow tracing now borrows the presentation device. Its uint32 mask
is bound directly to the effects shader; normal resident frames do not allocate
a shadow-download buffer, wait for mask readback, repack bytes, or upload the
mask again. Geometry packing/BVH construction remain CPU-side. Empty scenes
invalidate the previous output. Renderer changes release borrowed resources
before SDL destroys the device. Explicit readback remains available for CPU
fallback and diagnostic comparisons. Windows DXR masks now use same-adapter
GPU resource/fence interop for both Vulkan and D3D12; see the current migration
checkpoint for transfer and live-render evidence.
