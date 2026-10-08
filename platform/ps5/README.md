# PS5 homebrew build

The PS5 build is a native homebrew title (a fake-signed title folder) that runs
the same `starfox_pc` runtime as the desktop ports, including its SDL GPU
renderer and GPU raster pipeline. It needs a PS5 that you own with a homebrew
environment that launches folder titles. It was tested with kstuff and
[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus).

Verified on a console: boot, menu and gameplay at 60 fps, DualSense input,
audio, MSU-1 music and saves.

## Installing

1. Copy the title folder `PPSA99764/` to `/data/homebrew/PPSA99764/` (for
   example over FTP). Let ShadowMountPlus register it.
2. The game data is not included. On a PC, use `starfox_asset_builder` with a
   supported clean retail ROM to create `Starfox-Assets.BIN` (see
   `ASSET_BUILDER.md`). Copy it into the title folder, beside `eboot.bin`.
3. Optional music: copy `Starfox-MSU1.PAK` into the title folder too.

The game looks for both files in the title folder and in
`/data/StarFoxEnhanced/`, case-insensitively.

Settings, saves and logs are written to the first writable place among
`/data/StarFoxEnhanced/`, the title folder itself (the usual case with
ShadowMountPlus), and the title's sandbox storage
`/download0/StarFoxEnhanced/`. That folder also holds RADV's shader cache.
These logs help when reporting a problem:

- `startup.log`: launch stages with timings.
- `stderr.log`: runtime diagnostics. The previous run's is kept as
  `stderr.prev.log`.
- `sdl.log`: SDL's log, including the GPU in use and the pads opened.

Controls are the same as on PC with a gamepad. The touchpad and Create both act
as Select. Up to four signed-in users get a pad each: player 1 is the user who
launched the game.

## How it works

```
starfox_pc -> SDL3 GPU (Vulkan) -> RADV, linked into the title -> AGC / VideoOut
```

- **GPU:** Mesa RADV from Mihawk's
  [PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa), built with its PS5
  winsys and linked statically. SDL gets Vulkan from RADV's
  `vk_icdGetInstanceProcAddr`; there is no loader.
- **Display:** `runtime/sdl_video.c` creates a `VK_KHR_display` surface on
  RADV's VideoOut WSI. The display is 3840x2160 and the mode nearest 60 Hz is
  chosen (`runtime/display_mode.h`). VideoOut presents with vsync only, so the
  game's vsync-off option has no effect.
- **Hardware only:** the console build has no CPU presentation path. The
  renderer is locked to GPU (`GameSimulation::hardware_renderer_only`), and a
  GPU failure is reported as an error instead of falling back to CPU rendering.
- **Audio:** `runtime/sdl_audio.c`, AudioOut at 48 kHz, 16-bit stereo.
- **Input:** `runtime/gamepad.c` reads ScePad into SDL virtual gamepads.
- **Process setup:** `runtime/process.c` chooses the data folder, keeps RADV's
  shader cache there, redirects stderr, and hides the system splash screen on
  the first presented frame.

## Building

### Requirements

On the build machine (Linux; any distribution):

- **Docker**, with a running daemon your user can reach. The compilers, Meson,
  LLVM 18 and Python packages all live in the image; nothing else is
  installed on the host.
- **git** and **git-lfs** (`git lfs install` once, before cloning), so the
  launcher backgrounds (`sce_sys/*.dds`) check out as real files. Without
  them the build stops and asks for `git lfs pull`. The
  `upstream-ultrastarfox` submodule is not needed.
- **Internet access on the first build**: the Ubuntu 24.04 image and its
  packages, Meson from PyPI, Mihawk's three repositories (pinned commits from
  GitHub), the ps5-payload-dev SDK v0.42 release and zlib 1.3.2
  (checksum-verified), and the CMake dependencies (SDL 3.4.14, retro_cpu,
  snes_spc, dr_libs, xBRZ).
- **About 10 GB of free disk** (roughly 2 GB image, 2 GB `build/console-sdk`,
  2 GB `build/ps5`). The first build compiles Mesa and takes a while; later
  builds reuse `build/console-sdk/`.

To play, you also need `Starfox-Assets.BIN` from your own cartridge (see
Installing), and a console with the homebrew environment described above.
No Sony SDK, ROM or console is needed to build.

### Build

From the repository root:

```sh
tools/build_ps5.sh
```

1. The script builds the `platform/ps5/Dockerfile` toolchain image.
2. `bootstrap.sh` fetches Mihawk's
   [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan), PS5_Mesa and
   [PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK) at pinned
   revisions into `build/console-sdk/`. It builds the payload SDK, the release
   RADV archive, `ps5-native-tool`, and `libc.prx`, which is checked against
   PS5_Vulkan's recorded SHA-256.
3. The game is configured with `cmake/toolchains/ps5-radv.cmake` and built in
   `build/ps5/`.
4. The result is copied to `dist/StarFoxEnhanced-ps5/`: the `PPSA99764/` title
   folder (`eboot.bin`, `sce_sys/`, `sce_module/libc.prx`) and these notes.

The first run downloads and compiles Mesa and takes a while. Later runs reuse
`build/console-sdk/`. Set `STARFOX_PS5_TITLE_ID` to use another title id.

### Files

| Path | Purpose |
|---|---|
| `cmake/PS5.cmake` | Console configuration, SDL options, the title package target. |
| `cmake/toolchains/ps5-radv.cmake` | Compilers from the payload SDK fork; the link rule. |
| `tools/prepare_ps5_sdl.py` | Patches the pinned SDL 3.4.14: registers the drivers, accepts RADV (it reports no CTS conformance), builds virtual joysticks without HIDAPI. |
| `platform/ps5/runtime/` | The SDL backend and the process setup. |
| `platform/ps5/link-title.sh` | The executable link: PS5_Vulkan's title CRT, AGC stubs and `tools/radv-link.sh` recipe, then `ps5-native-tool link`. |
| `platform/ps5/package.sh` | Signs `eboot.bin` and lays out the title folder. |
| `platform/ps5/param.json.in` | Title metadata. |
| `platform/ps5/sce_sys/` | Launcher artwork and the scripts that make it (`ARTWORK.md`). |

## Tests

Host tests (`STARFOX_BUILD_TESTS=ON`):

- `starfox_ps5_port_tests`: the hardware-only renderer policy and the VideoOut
  mode choice.
- `starfox_ps5_sdl_patch`: the SDL patcher's changes, that it is idempotent,
  and that it fails on an SDL source it does not recognise.

The PS4 is not supported.
