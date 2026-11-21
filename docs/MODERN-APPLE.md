# MODERN on macOS and iOS (Metal): build and test guide

Status (2026-11-18): **built and checked on Windows only. Nobody has run it on Apple hardware yet.** The Metal shaders have not been through Apple's `metal` compiler either, because this Windows PC has no Metal toolchain. Use this guide to finish the job on a Mac.

## What changed

- `tools/generate_modern_shaders.py` now emits Metal (MSL) as well as SPIR-V and DXIL. The DXIL and SPIR-V bytes are unchanged.
  - Each stage's SPIR-V goes through SPIRV-Cross (MSL 2.1, `--msl-decoration-binding`).
  - Before that, every resource's SPIR-V `Binding` is rewritten to the slot SDL_GPU's Metal backend uses. The rules come from `SDL_gpu.h` (SDL 3.4.14) and `SDL_gpu_metal.m`:
    - Graphics stages: `[[buffer]]` holds uniform buffers, then storage buffers. `[[texture]]` holds sampled textures, then storage textures. `[[sampler]]` uses the same index as its texture.
    - Compute stages: `[[buffer]]` holds uniform buffers, then read-only storage buffers, then read-write storage buffers. `[[texture]]` holds sampled textures, then read-only storage textures, then read-write ones.
    - Each range starts after the counts the loader declares. The script therefore repeats each loader's counts and checks the result against SPIRV-Cross reflection.
  - The MSL goes into the headers under `#if defined(__APPLE__)` as `msl_<stage>` and `msl_<stage>_entry`. Entry names stay as in the HLSL, except that `main` becomes `main0`.
  - SPIRV-Cross's `--msl-ios` gives identical output, so macOS and iOS share one payload.
  - `--check` (ctest `starfox_modern_shader_freshness`) now also fails if a header lacks the Metal payloads.
  - Regenerating now needs both tools: `python tools/generate_modern_shaders.py --dxc DXC --spirv-cross SPIRV_CROSS`.
- Loaders: `sdl_scene_packets.cpp`, `sdl_layer_filter.cpp` and `sdl_ray_shadows.cpp` use MSL when the device offers neither DXIL nor SPIR-V. On Apple, `sdl_source_scene.cpp` (the compute route) uses upstream's portable `metal` payloads with entry point `main0`, as desktop `gpu_clip.cpp`, `gpu_projection.cpp` and `gpu_bsp.cpp` already do. Its declared counts match the slots those payloads were built for.
- `cmake/Modern.cmake`: MODERN now defaults ON for `APPLE`. iOS builds the same `starfox_pc` app, so it gets MODERN too.
- New `tools/check_modern_metal_shaders.py` compiles all 11 MSL payloads MODERN loads with `xcrun metal`: 6 of its own and 5 shared compute-route shaders. `tools/build_apple.sh` runs it after upstream's Metal checks.

## 1. Mac prerequisites

- A Mac with Apple Silicon is best. Intel Macs with Metal 2 GPUs should also work.
- Xcode 15 or later. On Xcode 26 the Metal toolchain is a separate download: run `xcodebuild -downloadComponent MetalToolchain`. Check that `xcrun metal --version` works.
- CMake 3.25 or later, Ninja (optional), Python 3 and git, for example from Homebrew: `brew install cmake ninja python`.
- Game data: `Starfox-Assets.BIN` beside the binary (as on Windows), or a build with `-DSTARFOX_EMBED_RUNTIME_ASSETS=ON`. Copy the `textures/` folder too if you want the HD art.
- The code: this work is **uncommitted** on Chris's PC (`fourk-main`). Commit and push it first, then check that branch out on the Mac.

## 2. Check the shaders (about 1 minute)

```sh
python3 tools/check_modern_metal_shaders.py            # macOS and iOS SDKs
```

You should get 11 `ok` lines for each SDK. A `FAIL` line shows the Metal compiler's error. Fix the cause in the HLSL or in the generator, regenerate on Windows, and run the check again.

## 3. Build for macOS

The quick development build is:

```sh
cmake -S . -B build/mac -G Ninja -DCMAKE_BUILD_TYPE=Release -DSTARFOX_BUILD_TESTS=ON
grep STARFOX_ENABLE_MODERN build/mac/CMakeCache.txt     # must say ON
cmake --build build/mac --target starfox_pc
ctest --test-dir build/mac -R 'starfox_modern_|starfox_scene_api_free' --output-on-failure
```

The CI-style universal build (Xcode generator, which also runs every Metal check) is `tools/build_apple.sh "$PWD" macos`.

## 4. Run it

```sh
cd build/mac   # Starfox-Assets.BIN (and textures/) next to starfox_pc
STARFOX_MODERN=1 MTL_DEBUG_LAYER=1 ./starfox_pc 2> modern.log
```

- `STARFOX_MODERN=1` starts in MODERN. You can also choose MODERN on the RENDERER row.
- `MTL_DEBUG_LAYER=1` turns on Metal API validation. A binding mistake shows up as an assertion that names the missing buffer, texture or sampler index.
- Any MODERN problem prints `modern: falling back to the existing renderer (...): <reason>` to stderr. Shader creation failures say `Create MODERN ...`. Send those lines back.

### Same-scene capture to compare with Windows

On the Mac:

```sh
STARFOX_MODERN=1 STARFOX_TEST_RENDERER=GPU_FAST STARFOX_TEST_EXPERIENCE=ORIGINAL \
STARFOX_TEST_MAP=LEVEL1_1 STARFOX_TEST_PREROLL_TICKS=300 STARFOX_TEST_FRAMES=40 \
STARFOX_TEST_PRESENTATION_FPS=20 STARFOX_TEST_HIDDEN=1 \
STARFOX_CAPTURE_DIR=caps STARFOX_CAPTURE_START=39 STARFOX_CAPTURE_INTERVAL=1 \
./starfox_pc LEVEL1_1
```

On Windows, the same scene comes from `cap2.ps1 -Map LEVEL1_1 -Preroll 300 -Frames 40 -Dir <dir> -Start 39`. Flat captures can land a few ticks apart, so compare by eye, not pixel by pixel.

## 5. What to try (each item exercises a different shader)

| Try | Shader / path it exercises |
|---|---|
| Fly LEVEL1_1 for a minute | scene vertex and fragment (`scene_sdl`), uploads, depth |
| LEVEL1_3 (the Space Armada), plus any level with big clipped models | compute route (`spans`, `clip_continuous`, `continuous`, `continuous_visibility`, `bsp`) |
| 2D FILTER: EDGE, xBRZ, SHARP, CRT, SCALEFX | `layer_filter` compute (read-only and write storage textures) and composite (sampler) |
| ANTI-ALIASING light, medium, heavy | MSAA 2x/4x/8x. Apple GPUs may not support 8x; it should drop to 4x without errors |
| DYNAMIC LIGHTING, or `STARFOX_MODERN_RT_SHADOWS=1` / `STARFOX_MODERN_RT_LIGHTS=1` | `ray_shadow` compute (uint atomic min/max) and the fragment shader's ray loop |
| Resize the window, go fullscreen, try a Retina / ProMotion display | target recreation and presentation |
| The ON-SCREEN FPS option | frame time compared with GPU FAST |
| Time from choosing MODERN to the first frame | runtime MSL compile (see the concerns below) |

## 6. iOS

1. Run `tools/build_apple.sh "$PWD" ios`. It makes an unsigned Xcode project in `build/ios` and runs the Metal checks against the iphoneos SDK.
2. Open `build/ios/*.xcodeproj`, set a signing team on `starfox_pc`, and run it on a device. An A12 or newer chip is recommended; the deployment target is iOS 15.
3. Choose MODERN on the RENDERER row. The on-screen buttons should stay visible on MODERN frames.
4. Watch Xcode's memory and energy gauges for a few minutes of play, including a busy explosion scene.

## Known concerns that need Apple hardware

1. **The MSL has never been compiled by Apple's compiler.** Step 2 is the first real check. Everything else so far is structural: slots, entry points, reflection, no fp64 and no auxiliary buffers.
2. **Shader compile time on first use.** SDL compiles MSL source at run time. The scene fragment shader is about 1.5 MB of MSL. Upstream's own `clip_continuous` Metal shader is about 1.7 MB, so this is in line with what already ships, but the first MODERN frame may take seconds. If it is too slow, precompile a `.metallib` at build time (`xcrun metal` plus `metallib`) and load it as `SDL_GPU_SHADERFORMAT_METALLIB`.
3. **Ray-traced shadows and lights are left enabled on Apple.** They are experimental and off unless DYNAMIC LIGHTING or the environment switches turn them on. I found nothing in them that Metal can't do: a 64-thread compute kernel, 5 buffers, relaxed uint `atomic_fetch_min/max` on device memory (the same pattern MoltenVK uses), and storage-buffer reads in the fragment shader. However, the fragment shader's per-pixel ray loop may be too slow on iPhone and iPad GPUs. If they misbehave, turn them off for Apple in `modern_runtime.cpp` rather than ship them broken.
4. **On-screen touch buttons on MODERN frames (fixed 2026-11-18).** `ModernRuntime::present` now takes a `before_present` callback. The app uses it to draw the buttons (`Window::draw_touch_controls_over_frame`) in window and safe-area coordinates, as the existing renderer does. This fixes Android too. It was checked on Windows with `STARFOX_TEST_TOUCH_OVERLAY=1`. On a device, check the safe-area placement (notch, home indicator) in portrait and landscape, and that the buttons still respond.
5. **iOS memory budgets.** These caps were sized for desktop GPUs:
   - Texel words: 32M (128 MB) per frame across packets, plus up to 8M words (32 MB) of backdrop art.
   - Vertices: up to 4M per frame (`SceneVertex` is about 160 bytes).
   - Shadow-caster buffer and source-span arena: up to 256 MB each.
   - All uploads are staged twice: once in a CPU vector, then in an SDL transfer buffer.

   Typical frames are far below these caps, but a 3–4 GB iPhone allows an app roughly 1.5–2.5 GB before iOS kills it. Older A9 and A10 devices, which iOS 15 still supports, cap a single Metal buffer at 256 MB. HD replacement art (512x512 sprites) pushes toward the texel cap fastest. Measure the Xcode memory gauge in a busy explosion with `textures/` present. If it is high, lower the texel cap and the 256 MB caps for iOS (`scene_packet_validation.cpp`, `sdl_ray_shadows.cpp`, `sdl_source_scene.cpp`).
6. **MSAA:** sample counts come from `SDL_GPUTextureSupportsSampleCount`, so unsupported counts (8x on most Apple GPUs) step down automatically. Confirm there are no validation errors.
7. **MoltenVK:** if SDL picks Vulkan on a Mac (`SDL_GPU_DRIVER=vulkan` with MoltenVK installed), the SPIR-V path is used instead. That path is untested on Apple.
