# Star Fox Enhanced on the Steam Frame

I got Star Fox Enhanced running natively on the Valve Steam Frame. It runs on
the headset itself, with no PC and no streaming, and draws through OpenXR and
Vulkan. It reuses the OpenXR and Vulkan code in `src/vr`, built for Linux on
ARM64, plus what's needed to build, package and launch it there. The player is
only built with `STARFOX_BUILD_STEAM_FRAME`, which is off by default. As with
the rest of the project you bring your own ROM, and nothing here ships one.

## Tech stack

The game side is the same C++20 code as everywhere else. The VR side is the
OpenXR and Vulkan renderer that was already in `src/vr`, with the OpenXR loader
(release 1.1.63) and Vulkan headers pinned in `cmake/OpenXR.cmake` and SDL3
3.4.14 linked statically for audio and gamepads. On the Frame, SteamVR on
SteamOS provides the OpenXR runtime and the Vulkan driver, so the package
doesn't ship either of them.

The binary is Linux ARM64. I cross-build it on an x64 Linux machine with Clang
and LLD against the Steam Runtime "Sniper" ARM64 sysroot (snapshot
3.0.20260415.224995, checked against a pinned SHA-256). On the device it runs
natively with Steam Play turned off, under `SteamLinuxRuntime_4-arm64`. The
plain `SteamLinuxRuntime_sniper` wrapper is x86-64, so it's the wrong one. I
build against the sysroot so the binary links the same libraries the device
runtime provides, and a normal x64 CI runner can do it without an ARM machine.

## Building

On a Linux machine with CMake, Ninja, Clang, LLD, Python 3, curl, tar and
binutils:

```
tools/build_steam_frame.sh [source-root] [build-root] [package-root]
```

The script downloads and checks the sysroot, builds the flat `starfox_pc` as an
ARM64 baseline, builds `starfox_steamframe` and the `starfox_vr_runtime_check`
diagnostic, installs the `steamframe` component and then runs the package
checks. It refuses to reuse an existing package directory. macOS can't do this
cross-build, so on a Mac you only get the host VR targets and tests:

```
cmake -S . -B build/host-vr -G Ninja -DSTARFOX_BUILD_VR=ON
cmake --build build/host-vr
ctest --test-dir build/host-vr
```

`portable-builds.yml` has two Frame jobs that do the same thing in CI: the host
VR tests with a software Vulkan driver, and the ARM64 package build with its
checks. Like the other jobs in that file they run on release tags and manual
dispatch.

The Sniper archive has 38 absolute symlinks in its linker directories, so
`tools/prepare_steam_frame_sysroot.py` rewrites them as relative links in the
extracted copy (the download stays untouched). A standalone VR build also
fetches the same pinned SDL3 the flat runtime uses, instead of needing an
installed one.

## What's in the package

The runtime package has `starfox_steamframe`, `LAUNCH-STEAM-FRAME.sh`,
`STEAM-FRAME-START-HERE.txt`, a `vrpreferences.json`, `BUILD-METADATA.json`
(source revision, tool versions, pinned dependencies and a SHA-256 for every
file) and the licence files. A separate diagnostics set has the flat
`starfox_pc`, `starfox_vr_runtime_check` and a report that resolves every ELF's
dynamic dependencies against the sysroot.

There's no ROM, no `Starfox-Assets.BIN`, no soundtrack, no saves and no Vulkan
loader or driver in either set, and the validation scripts fail if any of
those turn up. `vrpreferences.json` asks SteamVR for 2160 resolution, a 90 Hz
minimum, no half framerate and motion smoothing off. Those are requests, and I
haven't checked that SteamVR applies them all.

## Installing and launching

Build `Starfox-Assets.BIN` on a desktop from your own ROM with
`starfox_asset_builder`, the same as for any other platform. Copy it to
`~/.local/share/StarFoxEnhanced/` on the Frame (`$XDG_DATA_HOME/StarFoxEnhanced`
if that's set). Saves, preferences and the shader cache live in the same
folder, so the package itself can stay read-only.

Then put the package folder on the Frame and register it as a devkit title with
the SteamOS Devkit Client. To push updates I use `rsync -a package/
steamos@<frame-address>:<install-dir>/`. Set the title's launch command to
`LAUNCH-STEAM-FRAME.sh`, which works from any directory. If you
want another bundle, data folder or MSU-1 pack, pass `--bundle PATH`,
`--data-dir PATH` or `--msu PATH`. `starfox_steamframe --help` lists the rest.

## Controls

The Frame profile (`/interaction_profiles/valve/frame_controller_valve`) is only
enabled when the runtime advertises `XR_VALVE_frame_controller_interaction`.
The other OpenXR profiles and the gamepad fallback keep working.

| Frame input | Game |
| --- | --- |
| Left stick or D-pad | Steer |
| A / B / X / Y | Brake / bomb / shoot / boost |
| Bumpers | Roll left and right |
| Right Menu | Start and pause, and confirm in menus |
| Left View | Select |

I picked that layout to match the on-screen control settings. My first attempt
(A fire, X boost, Y brake) disagreed with them and felt wrong straight away.
Cartridge control type and the face-button swap setting still apply.

The Frame also gets a small system layer, which is the same for every game I
port.

| Input | What it does |
| --- | --- |
| Left View, short press | Select, and back in menus (sent on release) |
| Left View, hold 1 s | Recentre, keeping your height. Both controllers buzz |
| Left View, hold 3 s | Recentre and recalibrate height. Second buzz |
| Menu + View, hold 0.5 s | Opens the game's runtime menu, which is its VR settings |
| B | Back on every menu page |

On the Frame the runtime menu has RESET GAME (back to the start menu, saves are
kept) and QUIT TO STEAM, which ends the OpenXR session cleanly, and the old
four-input reset chord is off (those inputs are plain game buttons). Controls
held through a focus change or at launch are ignored until you let go.

## Settings

The Frame's runtime menu has a VR PRESENTATION page. Settings are saved in
`vr-preferences.bin`, and older preference files load with the defaults.

| Setting | Notes |
| --- | --- |
| Camera | Existing (default) or pilot, with X, Y, Z cockpit offsets in centimetres |
| World scale | Bounded range, cockpit offsets stay in real centimetres |
| Head translation | 0, 50, 100, 150 or 200%. Changes the head centre only, so IPD is untouched |
| Follow ship rotation | Off by default. Only does anything in the pilot view |
| Refresh rate | 90 Hz (default), 120 Hz or SYSTEM |

Haptics strength is on the OPTIONS page: 0 to 100% in 10% steps, default 60%.
It scales OpenXR haptics only, not desktop gamepad rumble.

Profiling is opt-in. `--profile-csv FILE` writes one row per submitted stereo
frame (host cadence, CPU stage times, per-eye submit-to-fence time, and Vulkan
GPU timestamps where the queue supports them), and `--profile-frames N`
(default 120) stops after N frames. Empty GPU cells mean unavailable.

There are also a few environment overrides, read at startup. They beat the
saved setting and are never written to the preferences file, and they do
nothing unless you set them.

| Variable | Meaning |
| --- | --- |
| `SFX_VR_HAPTICS` | Haptics strength, 0 to 1 |
| `SFX_VR_TIMING_GPU` | `1` turns on GPU timestamp queries |
| `SFX_VR_REFRESH_RATE` | Target refresh in Hz (72 to 144), default 90 |

On the Frame the game prints one `[vr-perf]` line every 10 seconds (fps, missed
frames, CPU stage times, GPU time or `n/a`, display rate). It and the refresh
decisions also go to `vr-session.log` in the data folder, since stdout isn't
kept on the Frame.

## Design decisions

Why native Linux and not Proton or FEX? The game is already portable C++ and
SDL3 and already builds for Linux. The Frame is ARM64 Linux, so a native binary
is the straightforward path, and I'd rather not put a translation layer or a
Vulkan wrapper between a game and a VR compositor that needs two eyes at 90 Hz.
I haven't benchmarked the alternatives, I just didn't want to start there.

Why reuse `src/vr`? It already builds the stereo scene from the game's own
state, and the Frame is just another OpenXR runtime. So the target is a new
platform entry (`STARFOX_STEAM_FRAME`, a path layout, a package) and not a
second renderer.

The Frame has its own loop and menu, so that PCVR and Quest stay exactly as they
are. `src/vr/steam_frame_application.cpp` and `include/starfox/vr/frame_menu.hpp`
are copies of `application.cpp` and `startup_menu.hpp` with the Frame changes.
`run_application` hands over to the Frame loop only when `host.steam_frame` is
set, which only `starfox_steamframe` does. The cost is duplication. Diffing each
copy against its original shows what the Frame changes, if you'd like to merge
them back later.

The HUD is the part I changed most. Menus, pause, map and briefing go on a
1024x896 mono quad layer, 1.15 m wide and 1.75 m away. The gameplay HUD and
dialogue sit at 0.75 m with the same angular size, because with the HUD at
1.75 m the player's ship could be nearer than the HUD that should be in front
of it (the player ship reaches about 0.87 m from the eye). The title and
controls screens use one flat 1.75 m quad on purpose, so their animated ship
previews are flat. Reticle and warning sprites keep their original 2 m plane.
I also removed the dark 254x78 panel that the first VR HUD drew behind the
gameplay HUD. It was opaque, and it covered a big part of the view. The 0.75 m
distance is a starting value and isn't comfort tuned.

Rumble uses the cartridge's own authored sequence. I moved the `RUMBLE_*`
register sequencer out of `starfox_pc.cpp` into `src/simulation/` so the flat
build and VR share it. In VR it advances once per 60 Hz source raster (not per
eye submission), turns the two bands into one OpenXR amplitude with
`max(low, high)` for 40 ms, and prefers OpenXR haptics over SDL rumble but
never plays both. It only runs for Original with the rumble setting on. EX has
no authored rumble. With no output to send to, the registers aren't touched, so
game state is the same as the flat build.

The cockpit view uses the cartridge's own COCKPIT geometry, decoded from your
asset bundle at runtime (122 triangles, with a topology check before my
materials go on), so no Nintendo geometry is in the repo. The rear of the cabin
is authored by me as OBJ files in `assets/vr/cockpit-c`, turned into
`src/vr/cockpit_assets.inc` by `tools/generate_cockpit_assets.py` (a test
checks it's current). The pilot sits in the cutscene Arwing (`MY_DEMOS`) at
24x, with the world scaled to match. The seat position is a fit I tuned by
hand, not where the original game puts the camera.

With Follow ship rotation on, steering is relative to the view. Once per logic
tick I project the world X/Y plane through the same ship basis the view uses
and pick the closest of the eight directions, so left on the stick stays left
when you're banked 90 degrees. Turning also felt jerky, because the source
moves in steps every 20 Hz tick. In pilot view the camera now follows a
quadratic B-spline through the last three ticks (about half a tick behind), and
Follow's rotation eases toward its target with a 0.1 s time constant and a 90
degree cap, so fast barrel rolls keep their direction.

The shared conventions (L View timing, haptics vocabulary, setting names, the
`[vr-perf]` line) live in a small C99 library called sfvr, vendored in
`third_party/sfvr`. I use it across my Steam Frame ports so a player doesn't
have to relearn anything between games, and its `STANDARD.md` describes the
conventions. A vendored copy follows the host project's licence, so here it's
GPL-3.0. `include/starfox/vr/system_layer.hpp` is a thin adapter on top that
adds only the Menu + View chord.

The refresh rate is requested with `XR_FB_display_refresh_rate`. By default I
ask for the highest rate the runtime offers at or below 90 Hz, because the
Frame's system 120 Hz setting doesn't apply to the game on its own (the
runtime offered 72 to 144 and the request was logged). If the focused frame
rate stays under 90% of the current rate for two 10 second windows, the game
steps down one offered rate at a time to 72 Hz and stays there. Choosing
SYSTEM stops the requests, though a rate that's already been requested can't be
withdrawn mid-session.

## Files outside src/vr

Flat builds shouldn't change, apart from the items marked below.

`include/starfox/compat/bit_cast.hpp` plus about two dozen call sites in
`src/render`, `src/simulation`, `src/app`, `src/timing` and the state archive
swap `std::bit_cast` for `starfox::bit_cast`. Where the standard library has
`std::bit_cast` that's what it calls. The builtin is only the fallback for GCC
10 headers. `tests/bit_cast_compat_tests.cpp` forces the fallback and checks
it. Two Windows-only files now define `NOMINMAX` before `windows.h`, as the
other Windows files already do. `CMakeLists.txt` enables Objective-C++ early on
Apple, which newer CMake needs before SDL's nested project, and adds the
Steam Frame option with guards, the SDL fetch for standalone VR builds and the
new tests. `.gitignore` gets `.DS_Store`.

`src/app/starfox_pc.cpp` now builds its rumble output on the shared sequencer.
The behaviour is meant to be identical.

Everything VR-facing is Frame-only. `application.cpp` has two added lines (the
hand-over) and `startup_menu.hpp` is untouched. The shared classes the Frame
loop also uses keep their original behaviour unless the Frame player asks for
more: `OpenXrInput::set_frame_player` adds the Index profile, the D-pad and
rumble actions, `OpenXrRuntime::set_frame_extensions` enables the two Frame
extensions, and `source_ui_text_packet` only draws ( ) + for the Frame menu.
`starfox_pcvr` has no profiling options and finds its folders the way it did.

`set_inset_decals` switches on the inset-sign decal rule, and only the Frame
loop calls it. `C_TYPE` is optional in the scene symbols.

`OpenXrInput::set_system_layer` and `DesktopControlEdges` do the View hold, the
Menu + View chord and the buzz only for the Frame, and keep the original
Select-on-press and four-input reset chord otherwise. PCVR and Quest keep their
original 20-byte preferences file. The Frame writes its own versioned one (v9)
into its data folder, so don't point both players at the same `--data-dir`.

## What I've tested

Host side, the ctest suite passes on macOS apart from a few tests that already
fail or abort there on a clean upstream checkout (more on that in the PR). The
two new CI jobs run the Linux host tests and the ARM64 package build, and the
package passes the hash, allowlist and ELF checks.

On the Frame itself I've run the flat ARM64 build (menus, audio and gameplay
are fine) and the native VR build, which gets to `FOCUSED` with two eye
swapchains and lets me navigate the menus and play.

I haven't run PCVR or Quest on hardware for this. Their code is unchanged, and
host tests cover the original paths (the original menu, input, decals and
runtime setup).

I've also confirmed on the headset that the controller profile loads and the
buttons respond, that the gameplay HUD is fine without the black backing, that
Original's authored rumble reacts to boosting and destroying things, and that
the closer HUD fixes the ship-over-HUD problem.

The tower logo decal fix looked stable. Follow ship rotation was rough and hard
to steer when banked at first, and with the view-relative steering and the
smoothing it feels good now. I've worn the cockpit view too. The cockpit
geometry and input tests need your own Original and EX bundles, so CI doesn't
run them.

The system layer, haptics strength, the env overrides, the `[vr-perf]` line and
the refresh rate request are covered by host tests with a fake OpenXR runtime.
I haven't tried them in the headset yet, so I don't know how the buzz feels or
whether the Frame's compositor honours every request.

## Known limitations

You need your own ROM and a devkit-enabled Frame, and there's no store
packaging. I haven't measured frame times on the device. The profiler is there
for that, but I haven't captured a run, and I haven't compared against Proton
or FEX.

The cockpit is new, and other people may want the seat position adjusted.

Menu labels added here have no translations yet and show in English. The
desktop gamepad fallback has no haptic buzz, and EX has no authored rumble.
