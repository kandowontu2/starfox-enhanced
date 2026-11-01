# Building Star Fox Enhanced

Prebuilt packages do not require these steps. See [the README](../README.md)
for installation and supported ROM revisions.

## Fixed desktop window size

Set `FULLSCREEN 0`, `WINDOW_WIDTH 1280` and `WINDOW_HEIGHT 720` in `pregame.cfg`
to start at that window size. Both dimensions must be present (width 256–16384,
height 224–16384). `WINDOW_WIDTH 0` and `WINDOW_HEIGHT 0` restore automatic sizing.
Non-matching aspect ratios are letterboxed; only FIT SCREEN stretches the image.
Fullscreen keeps the dimensions for the next switch back to windowed mode.
The desktop Alt+1 through Alt+4 shortcuts temporarily select native-size multiples;
Alt+0 restores automatic sizing for the current session.

## Development launcher

### Smaller desktop binaries

Runtime enhanced backgrounds now use lossless PNG payloads (about 61 MB
instead of 179 MB). The BMP authoring references remain in source, but are
not embedded. Resolution and rendered pixels are unchanged; portable decoding
is lazy and cached, with no operating-system image service required.
Desktop flat-runtime builds can use `-DSTARFOX_EMBED_ENHANCED_BACKDROPS=OFF`
to retain embedded import patches/symbols but load artwork from
`assets/enhanced-backdrops/` beside the executable instead (inside Resources
for a macOS `.app`). Use `cmake --install` to install that companion artwork
with the runtime; keep it when distributing
the smaller binary. Normal release builds
still embed it; mobile, console and VR packaging is unchanged.

The standalone asset builder now embeds only the import patches/symbols, not
the unused enhanced artwork. It is not needed after successfully creating
`Starfox-Assets.BIN`. Keep the BIN across updates; it needs regeneration only
when the embedded patch/symbol manifest changes, not for a version-number,
renderer, settings or artwork-only change. Keep your retail ROM available for
that future rebuild. `-DSTARFOX_EMBED_RUNTIME_ASSETS=OFF` also removes embedded
import resources, so it is for external-input development builds rather than
a drop-in small player package.

### Launching from source

Release builds use the checked-in source-built BPS deltas and pinned symbol
maps, so a clean checkout does not need either reconstructed ROM. Developers
regenerating those inputs can build the pinned UltraStarFox and Star Fox EX
sources; their local ROM and symbol outputs remain ignored and untracked:

```text
upstream-ultrastarfox/SF.SFC
upstream-ultrastarfox/SYMBOLS.TXT
upstream-star-fox-ex/SFES/SFES.SFC
upstream-star-fox-ex/SYMBOLS.TXT
```

Then run:

```powershell
.\play-starfox.ps1
```

The launcher configures an optimized build on first use and starts at the
pre-game setup. On the executable's first run, supply any supported unmodified
1 MiB retail Star Fox/Starwing ROM:

```text
Star Fox (Japan), revisions 1.0 or 1.1
Star Fox (USA), revisions 1.0, 1.1, or 1.2
Starwing (Europe), revisions 1.0 or 1.1
Starwing (Germany), revision 1.0
the ROM beside starfox_pc.exe or in C:\NTSC-US Super Nintendo System Roms
the path named by STARFOX_RETAIL_ROM
```

A 512-byte copier header is accepted and removed before validation. Each known
regional revision is checksum-verified and losslessly canonicalized to USA
v1.2 before the source-build patches are applied. Competition cartridges,
betas, hacks, Star Fox 2, modified dumps, and unknown revisions are rejected.
After `Starfox-Assets.BIN` is created, the retail file is no longer read unless
the executable's embedded patch or symbol manifest changes and the companion
must be rebuilt.

A development map can be selected explicitly:

```powershell
.\play-starfox.ps1 LEVEL1_1
```

Explicit external ROM/symbol pairs can still be passed to a development build
for source-to-port comparisons.

The resident source-reflection index is experimental validation code, not a
released visual setting. Normal builds leave it out. Developers running those
diagnostics must explicitly configure
`-DSTARFOX_ENABLE_EXPERIMENTAL_REFLECTION_INDEX=ON` and provide a DXC build with
SPIR-V code generation (the Windows SDK's DXIL-only DXC is insufficient).
This does not disable the normal ray tracing, reflections or Leia integration.

## Regenerating upstream assets

The required Original revision is pinned in `config/upstream.json` and tracked
as a submodule, so `git clone --recurse-submodules` fetches the source with the
repository and the pin keeps the embedded symbol tables bound to the checked-in
BPS deltas:

```powershell
git submodule update --init upstream-ultrastarfox
powershell -ExecutionPolicy Bypass -File tools/build_upstream.ps1
```

The UltraStarFox DOSBox assembler toolchain must be present in that checkout,
as described by its own build instructions. The helper idempotently applies
`config/ultrastarfox-native-runtime.patch`, which enables the authored MSU-1
and rumble events while retaining stock SPC music for the runtime ON/OFF
switch and routing the physical rumble transmitter through SDL.

The Star Fox EX 1.11.03 source revision is here: https://github.com/kandowontu/star-fox-ex

```powershell
git clone https://github.com/sunlitspace542/star-fox-ex.git upstream-star-fox-ex
git -C upstream-star-fox-ex checkout b5e2d837a15a72a532cd019bfe332b7a4b660924
powershell -ExecutionPolicy Bypass -File tools/build_starfox_ex.ps1
```

That checkout also supplies its DOSBox assembler toolchain. Both build helpers
reject a different source revision so the embedded symbol tables remain bound
to the checked-in BPS deltas.

## Build and test

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j 8
ctest --test-dir build/release --output-on-failure
```

Create a portable executable folder (ROM data remains user-supplied):

```powershell
cmake --install build/release --prefix dist/StarFoxEnhanced
```

The default build downloads the pinned music sources and installs the optional
`Starfox-MSU1.PAK` beside `starfox_pc.exe`. Configure with
`-DSTARFOX_PACKAGE_MSU1_MUSIC=OFF` to build and install the game without that
companion; the executable remains fully playable with the original SPC music.

You can launch the binary directly:

```powershell
build/release/starfox_pc.exe
build/release/starfox_pc.exe LEVEL2_3
build/release/starfox_pc.exe path/to/SF.SFC path/to/SYMBOLS.TXT TITLEMAP
```

## Native platform targets

The Windows x64 executable remains the primary release. The same portable SDL3
runtime also has build targets for Windows x86, Linux x64, unsigned universal
macOS, unsigned iOS arm64, Android arm64, Nintendo Switch, PS Vita, and an x64
Xbox UWP package for Developer Mode. Only Android and iOS expose translucent on-screen
controls; physical
SDL-compatible controllers continue to work normally. The Apple and Android
packages show a native file picker on first launch, accept any supported retail
revision, and store the reconstructed asset companion in writable application
storage rather than attempting to modify an application bundle.

```powershell
.\tools\build_windows_x86.ps1 -LlvmMingwRoot C:\path\to\llvm-mingw
.\tools\build_xbox_uwp.ps1
```

```bash
tools/build_linux.sh
tools/build_apple.sh . macos
tools/build_apple.sh . ios
tools/build_android.sh . debug
DEVKITPRO=/opt/devkitpro tools/build_switch.sh
VITASDK=/usr/local/vitasdk tools/build_vita.sh
```

Apple targets require Xcode on macOS, Android requires its SDK/NDK, Switch
requires the devkitPro Switch toolchain, and Vita requires VitaSDK. These
platform SDKs are not vendored.
The `Portable platform builds` GitHub Actions workflow produces a universal
unsigned macOS `.app`, an unsigned iOS arm64 device bundle, an installable
permanently signed Android arm64 development APK, a Nintendo Switch homebrew `.nro`, a PS Vita
homebrew `.vpk`, and a signed x64 UWP package for Xbox Developer Mode. The Xbox
build uses UWP LocalState as its internal writable storage for settings, HUD layouts, `Starfox-Assets.BIN`,
and the optional `Starfox-MSU1.PAK`; both companion names are matched without
regard to case. Packaged application files remain read-only. The iOS
bundle must be signed with the user's own Apple identity or sideloading tool
before installation. It also packages a
standalone Windows x64 `starfox_asset_builder.exe`: run it against a supported
retail ROM on the PC, transfer the resulting `Starfox-Assets.BIN`, and select
that BIN from the mobile app. Mobile can still select the retail ROM directly;
the prebuilt BIN path is an optional convenience.

The Switch runtime finds that same BIN beside the launched NRO, even when its
application folder has been renamed. The packaged default is
`sdmc:/switch/StarFoxEnhanced/Starfox-Assets.BIN`. It includes a local PC script
for generating an optional NSP forwarder with NTON and keys dumped from the
user's own console; console keys are never stored in this repository or in the
public build workflow. See `platform/switch/README.md` for the exact layout and
the forwarder warning.

Linux builds SDL from the pinned source archive. Install the distribution's
SDL build dependencies first; the authoritative Ubuntu/Fedora package lists
are maintained in SDL's
[Linux build documentation](https://github.com/libsdl-org/SDL/blob/main/docs/README-linux.md).
`tools/build_linux.sh` performs an optimized clean build, runs the complete
test set, and installs both the runtime and standalone asset builder.

## Fidelity boundary

Unlocked 20 FPS uses one logic/strategy update for every three fixed 60 Hz
cartridge raster phases. Original Speed additionally retains source frames
according to the measured 10.7 MHz workload schedule, reproducing the
characteristic cartridge slowdown. The independently selected render FPS
changes only how often frames are presented: an exact rational scheduler
services the same raster phases, logic ticks, frontend timing, and audio pace
at 20, 30, 60, 90, 120, 240, 360, and 480 FPS. Object and camera
rotations use normalized matrix interpolation between source updates, while
gameplay state remains fixed-point and unchanged.

The port consumes the exact assembled models and fixed-point state, but it is
not a cycle-accurate SNES emulator. Its software renderer reproduces the
source projection, clipping, face order, scan conversion, sprite priority,
and indexed palette behavior while the extra frames are newly interpolated
presentations. See `docs/ARCHITECTURE.md` for the subsystem boundary and test
strategy.

Useful diagnostics include `starfox_stage_trace`, `starfox_stage_preview`,
`starfox_shape_coverage`, and `starfox_planet_probe`. Third-party revisions
and licenses are recorded in `THIRD_PARTY_NOTICES.md`. The original Nintendo/
Argonaut staff, Star Fox EX team, UltraStarFox contributors, native-port
credit, and MSU-set attribution are recorded separately in `CREDITS.md`.

`starfox_texture_dump` writes every Super FX texture straight from the ROM,
without playing the game:

```sh
starfox_texture_dump Starfox-Assets.BIN textures [--map LEVEL3_7]
starfox_texture_dump SF.SFC SYMBOLS.TXT textures [--map LEVEL3_7]
```

The bundle form writes `original/` and `ex/`. Each holds `atlas/` (every
texture bank as a 256x128 page per 4-bit plane), `textures/` (each texture
that a shape or colour table references) and `textures.csv`, which lists every
size each texture is sampled at, its descriptors and the shapes that use it.
`textures/` uses the names MODERN's replacement loader reads,
`tex_AAAAAA_l.png` / `tex_AAAAAA_h.png` (SNES ROM address, low/high nibble
plane), so an edited copy at any resolution goes into `textures/` next to
`starfox_pc.exe`. The PNGs are RGBA with texel 0 transparent, coloured with
CGRAM palette 7 after the `--map` stage has loaded (default `LEVEL1_1`). When
the game samples one texture at several sizes, the largest is written.

`textures/` also holds every communications portrait frame as
`face_AAAAAA.png` (the frame's ROM address in `FACEDATA`, or EX's
`FACEDATA2`), 32x40 and opaque, listed in `portraits.csv`. MODERN draws a
`face_*.png` in `textures/` next to `starfox_pc.exe` over the whole portrait
box at any resolution (keep 4:5) and applies the screen fade to it. Each
mouth/eye position is its own frame. Original's table ends with a few frames
of unrelated data that the game never shows.

2D sprites (HUD labels, icons and digits, stage title lettering) live in
VRAM, so the tool plays `--map` headlessly, steps `LIVES` to show every digit,
and writes each sprite it sees as `obj_HHHHHHHHHHHHHHHH.png`, a hash of the
sprite's pixels, size and palette (`render::sprite_override_key`), listed in
`sprites.csv` with its tile and screen position. MODERN draws an `obj_*.png`
in `textures/` over that sprite at any square size. With
`STARFOX_MODERN_DUMP_TEXTURES=1` MODERN also writes every sprite it draws.
Never commit the output, since it is ROM data.

The EX regression runs every one of the 40 stage labels shipped through
`PLANETS` and `PLANETS2` for 2,000 deterministic logic ticks. The source-only
`PLANETS3` test campaign is intentionally outside the shipped experience.
