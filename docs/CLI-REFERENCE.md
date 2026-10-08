# Runtime command-line reference

These are the supported command lines for the distributed desktop executables.
The flat game's menu settings are not CLI flags; use the in-game menu. Launch
a map by its uppercase label for testing, not as a substitute for a natural
playthrough (the latter supplies preceding-stage state and transitions).

## Flat PC runtime

```text
starfox_pc [--fullscreen] [MAP]
starfox_pc [--fullscreen] ROM SYMBOLS [MAP]
```

Without `MAP`, the game enters `BOOT` (the pre-game setup menu). The
two-file form loads an explicitly supplied development ROM and symbol table.
The embedded-asset build needs no external ROM or symbols. `--fullscreen`
starts the desktop game in fullscreen and may appear before or after the
positional arguments. The executable has no `--help` option.

Options includes a Renderer (GPU/Software) picker and platform-supported GPU
Backend choices (AUTO by default). These are menu preferences, not CLI switches.
In Windows desktop builds, DLSS and DLSS 4.5 are separate 3D Options rows; either
requires the D3D12 backend and compatible NVIDIA hardware. They share the
embedded runtime but select different models, so enabling one disables the
other. Their config keys are `DLSS_MODE` and `DLSS45_MODE` (0=OFF, 1=Quality,
2=Balanced, 3=Performance, 4=DLAA). `RENDERER_BACKEND` is 0=AUTO, 1=Vulkan,
2=D3D12, 3=D3D11, 4=Metal, 5=OpenGL ES; only supported choices are offered on
each platform. An interrupted GPU session on Android/Windows recovers with
Software on the next launch without deleting assets.

With DLSS enabled for ordinary mono rendering, output resolution follows the
window/display's actual pixel size and chosen aspect; Render Upscale acts as
a minimum, and its row shows the effective DLSS output scale. Automatic sizing
is bounded to 6x/4096 pixels per axis. Integer Scaling retains fixed-raster
presentation. These are rendering preferences, not additional CLI switches.

Development desktop Windows builds with `STARFOX_ENABLE_DISPLAYXR` also support:

```text
starfox_pc --leia-sr-check [DISPLAYXR_RUNTIME_DIRECTORY]
```

This separate, asset-free diagnostic reads the installed DisplayXR directory
(or the explicit directory), negotiates its native display interfaces and checks
for a confirmed physical Leia panel. It does not change the system OpenXR
runtime, environment variables or game settings. Exit 0 means panel discovery
passed, 3 means unavailable, and 2 means invalid usage. It does not create a
graphics session, render a frame or prove calibrated native presentation.
The default negotiation is D3D12; `SDL_GPU_DRIVER=vulkan` selects Vulkan2.
Development Windows builds also accept `--leia-sr` before or after the ordinary
positional arguments, for example `starfox_pc --fullscreen --leia-sr`. This is
equivalent to requesting DISPLAYXR LEIA in Stereo Options (`LEIA_SR` config key).
It requires an installed DisplayXR runtime and a confirmed physical Leia panel.
D3D12 uses the exact runtime adapter; Vulkan creates its instance/device through
the runtime's Vulkan2 callbacks before SDL allocates the renderer. Select Vulkan
through GPU Backend or `SDL_GPU_DRIVER=vulkan`; an explicit incompatible backend
remains unavailable. It does not bundle the vendor SR runtime or alter global
OpenXR selection. No qualifying hardware means unavailable, not simulated native
3D. Native game/UI rendering and real-queue mocked presentation are tested, but
  requested sessions retry after disconnect with capped backoff and safe
  nonblocking GPU cleanup. Turning native SR off or manually changing the
  renderer/backend/fullscreen cancels recovery. Physical presentation/reconnect
  and complete enhancement/RT/material parity remain unverified/incomplete.
  Native D3D12 and Vulkan eye presentation are implemented. Real-GPU fixtures
  exercise both with mocked XR composition; physical Leia acceptance is still
  required. Windows Vulkan ray rendering uses the existing DXR interop queues,
  not a claim of native Linux Vulkan RT validation.

Windows x64 also has an optional direct **SR PLATFORM** Stereo Output choice
(`STEREO_OUTPUT 9`). It requires GPU/D3D12, the optional adapter beside the
executable, and the installed SR runtime on a recognized physical panel.
AUTO selects D3D12 for this route; an explicit GPU backend/environment choice
is not silently overridden. The runtime/factory validates the panel without
the extra pre-factory EDID whitelist. The Windows x64 package includes
`Diagnose-Leia-SR.cmd` (or `starfox_leia_sr_check.exe --probe`) to check native
SDK initialization on each monitor without assets. It records exact unavailable
reasons, not a claim that a native game image was presented. SDK initialization
failures during normal play are also written to `startup.log`.
It does not use the `--leia-sr` switch, which still selects DisplayXR. See
[SR Platform setup and validation limits](SR-PLATFORM.md). Missing or
incompatible hardware leaves ordinary 2D output working.

Host flow entries: `BOOT`, `PLANETSELECT`, `TITLEMAP`, `INTROMAP`,
`CONTMAP`, `GAMEOVER`, `CONTINUE`, `CREDITSMAP`. `TRAININGMAP` is
a ROM-backed training entry.

Playable Original stage labels:

```text
LEVEL1_1 LEVEL1_2 LEVEL1_3 LEVEL1_4 LEVEL1_5 LEVEL1_6
LEVEL2_1 LEVEL2_2 LEVEL2_3 LEVEL2_4 LEVEL2_5 LEVEL2_6
LEVEL3_1 LEVEL3_2 LEVEL3_3 LEVEL3_4 LEVEL3_5 LEVEL3_6 LEVEL3_7
LEVEL_SPECIAL LEVEL_BLACKHOLE LEVEL1_END
```

Star Fox EX adds the following labels to the Original stage labels:

```text
LEVEL4_1 LEVEL4_2 LEVEL4_3 LEVEL4_4 LEVEL4_5
LEVEL5_1 LEVEL5_2 LEVEL5_3 LEVEL5_4 LEVEL5_5
LEVEL6_1 LEVEL6_2 LEVEL6_3 LEVEL6_4 LEVEL6_5 LEVEL6_6
LEVEL7_1 LEVEL7_2 LEVEL7_3 LEVEL7_4 LEVEL7_5
LEVEL_COMET
```

EX also contains `LEVEL_CREDTEST` and `LEVEL_CREDTEST2`; they are
diagnostic credits entries, not campaign stages. A map label must exist in the
selected cartridge's symbol table.

## PCVR player

```text
starfox_pcvr [--bundle PATH] [--data-dir DIRECTORY] [--msu PACK] [--enhanced-sky]
starfox_pcvr --help
```

The default bundle is `Starfox-Assets.BIN` beside the executable. Save files
and preferences default to its `vr-data` directory. An optional
`Starfox-MSU1.PAK` beside the executable is auto-detected; `--msu` overrides
it. `--enhanced-sky` enables that option at startup. The headset must have
an active OpenXR runtime, even when using a standard gamepad.

## Asset builder

```text
starfox_asset_builder RETAIL_ROM [Starfox-Assets.BIN]
```

The builder accepts a supported unmodified retail Star Fox/Starwing dump,
validates the output against the current build's asset manifest, and does not
ship a ROM. The PCVR ZIP includes `BUILD-ASSETS.bat` and
`LAUNCH-PCVR.bat` wrappers with persistent, readable errors.
