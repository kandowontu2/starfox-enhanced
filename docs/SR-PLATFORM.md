# Direct SR Platform output

October 5 compatibility fixes against agrofubris's working direct SDK route:
SR Platform selects D3D12 in Windows AUTO, including the initial renderer,
without overriding an explicit backend/environment choice. Selection changes
are deferred while Preview is OFF and do not rebuild the renderer every frame.
The adapter no longer rejects panels using the extra pre-factory EDID whitelist.
It follows the SDK factory/context order, checks the installed runtime's active
display when its optional DisplayManager interface exists, and retains the
older-runtime factory path when that interface does not exist. Confirmed clone
mode and a window on another runtime-reported panel remain rejected.

The optional C-ABI status export preserves V1 compatibility. Failures now identify
the backend, missing adapter/runtime/dependency, service/factory, display or weave
stage in `startup.log`. An initialized factory is not labeled as presented output.
The Windows package includes `Diagnose-Leia-SR.cmd`, an asset-free per-monitor
factory probe, and its instructions. Proprietary SR system software is not bundled.
The separate DisplayXR toggle is labeled **DISPLAYXR LEIA**, not **NATIVE LEIA SR**;
SR Platform alone never satisfies DisplayXR's runtime/interface requirements.

Local missing-runtime and owned-texture checks are not physical SR-panel
acceptance. Use the isolated compatibility test package, not an old `build/release`
executable; installed PC/PCVR builds and public releases were not replaced.
The current compatibility checkpoint records hashes and exact test scope.

Preceding development PC is `B46552B6...`. The optional adapter was unchanged;
renewed factory/owned-weave guards and 36 unavailable-SR Original/EX mono images
pass. Immutable SBS GPU ray-connectivity reuse changes neither SR cameras nor
weaving. All owned checks ended; seven protected hashes unchanged. Physical
panel/lens/head-tracking/resize/refresh acceptance remains open. No proprietary
runtime is newly bundled. The newest renderer report/manifest records exact
current artifacts and scope; full goal remains active.

Preceding development PC is `ED4F4DA0...`. The adapter is unchanged; renewed real
factory/owned-weave guards and 36 unavailable-SR mono images pass. CPU SBS ray
connectivity sharing does not change SR cameras or weaving. All owned checks
ended; physical panel/lens/head-tracking/resize/refresh acceptance remains open.
The newest renderer report/manifest records exact current artifacts and scope.

Windows x64 now has a second optional Leia output route. In Options → Stereo
Options, select **OUTPUT: SR PLATFORM**, use **Renderer: GPU** and **Backend:
D3D12** (Windows Auto selects D3D12 for this route unless explicitly overridden), and enable the preview or start
the game. It requires the SR Platform runtime/service installed for a
compatible physical display, with the game window on that display (not a
cloned/mirrored monitor). The installed SDK runtime/factory qualifies the
hardware, not a separate EDID whitelist; runtime installation alone is not sufficient. It is unavailable
on other hardware/builds. Moving to another monitor disables weaving until
you deselect/reselect the route on a compatible panel.
Switchable panels receive a context-owned 3D lens preference only when weaving;
it is released with the host. Fixed-lens panels do not require this feature.

This is separate from **DISPLAYXR LEIA**, which uses DisplayXR's located,
calibrated physical-panel views on D3D12 or Vulkan. Selecting SR Platform
deselects the DisplayXR request, and enabling DisplayXR Leia deselects SR
Platform; it uses our existing parallel stereo cameras,
live separation/convergence and eye-specific reflections. The SR weaver does
its own tracked panel weaving, but this route does not claim DisplayXR's
head-tracked camera/frustum calibration. Existing stereo IDs 0–8 are unchanged;
`STEREO_OUTPUT 9` selects this route. `LEIA_SR 1` still means DisplayXR.

Both input (2W × H) and woven output (W × H) are app-owned BGRA8 UNORM textures.
The raw SDL swapchain is never handed to the weaver. Extents are even, the
input is rebound every frame, gamma is not converted twice, and a black normal
SDL present settles a size/format change before weaving resumes. Failed
native presentation disables the route and rebuilds the ordinary renderer;
deselect/reselect to retry. Missing software does not show an unwoven SBS
image as successful native output and does not affect normal startup.
Preview OFF releases the direct-SDK host and uses the ordinary plain-menu
path. `--leia-sr` explicitly selects DisplayXR even if SR Platform was saved;
this startup normalization does not write preferences.

## Build and package

The main runtime has no SR SDK import dependency. Windows x64 builds include
the optional loader by default (`STARFOX_ENABLE_LEIASR`); other platforms use
an unavailable stub. Build its adapter with installed MSVC 2022 x64 tools:

```powershell
./tools/build_leia_sr.ps1 -OutputDirectory build/current
```

The helper fetches a commit- and SHA-256-pinned SR-lib SDK archive, builds a
C-ABI adapter with delay-loaded vendor DLLs, and installs it and licenses
beside the executable. It does not install/redistribute SR Platform software.
`cmake --install` includes the optional module/notices when built. The Windows
x64 workflow runs this helper before packaging; x86/UWP/headset builds remain
unchanged. No device driver or app settings are installed by this helper.

## Validation limits

Build, optional-runtime handling and app-owned D3D12 texture tests are local
checks, not evidence of physical SR-panel image quality or refresh stability.
The tester must verify native weave, resize, fullscreen, display changes,
reflections at non-default separation/convergence and recovery on their panel.

### Local checkpoint — October 3, 2026

Compatibility checkpoint executable SHA-256:
`692b1479d38bbd414ef573f9cf28e83c3ef1e0af2518e6d71f08a3702b4e90e4`.
Adapter SHA-256:
`49b72bf97a16d5853321a54306fd815d2dfd013748fe671ba8a55b1d0b9a6327`.

Six stereo/configuration/DisplayXR regression suites pass. The real D3D12
owned-weave fixture passes six exact BGRA outputs at two extents, repeated
commands, cancellation, callback failure and rejection of foreign SDL
renderer textures. It uses a synthetic clear callback, not the vendor weaver;
the real adapter's ABI and missing-runtime creation/re-arm are tested separately.
Normal DLL imports are Windows system libraries only; Core, Displays and
DirectX vendor dependencies are delay-loaded.

On this exact executable/adapter pair, unavailable SR output matches 36 mono
images exactly across Original/EX and D3D12/Vulkan. Evidence:
`D:/SFE-validation/sr-platform-final-module-fallback-oct3/results.json`.
Twenty-nine Preview-OFF menu cases (six with SR output selected), plus the
preview loading capture, pass on the executable. Evidence:
`D:/SFE-validation/sr-platform-owned-guard-menu-oct3/results.json`.
Existing DisplayXR frontend fallback checks also pass. These checks do not
establish sustained performance, physical panel/lens behavior, native image
quality or complete enhancement parity. Release binaries, preferences,
ReShade files and Android APKs were not changed; no workflow was dispatched.

Native liquid/TAA follow-up executable SHA-256:
`9218cb51fb0eb0a0e1526ed2b327fbf44d24b38372035df484fcf50448f588b2`.
The adapter is unchanged. This build also passes the 36-image unavailable-SR
fallback matrix with preferences unchanged:
`D:/SFE-validation/sr-platform-native-liquid-followup-oct3/results.json`.
Its native liquid/TAA post-order checks pass on D3D12 and Vulkan (108 cases
each); see `CURRENT-ACCEPTANCE.md` for the independent fixture and scope.
These follow-ups still do not validate a physical SR panel or publish a release.
The follow-up executable also passes all 29 Preview-OFF menu cases plus the
preview loading capture, with preferences and the ReShade log unchanged:
`D:/SFE-validation/sr-platform-native-liquid-menu-oct3/results.json`.
The timing table from this check is not an isolated performance benchmark.

Preceding SDK liquid-order follow-up executable SHA-256:
`adc89620f76eebfc65fb40c354d3d7f7028a593449a329528d9b3aa0795aa252`.
The adapter remains unchanged. On this pair, the real owned-weave and optional
missing-runtime/re-arm checks, six CPU suites, 36 exact unavailable-SR images
and all 29 Preview-OFF menu cases plus viewed RENDERING/loading pass. Evidence:
`D:/SFE-validation/sr-platform-sdk-liquid-final-oct3/results.json` and
`D:/SFE-validation/sr-platform-sdk-liquid-menu-oct3/results.json`.
Preferences/injector files, release and Android artifacts are unchanged.
This is still not physical-panel, private SDK-quality or sustained-FPS proof;
the separate native SDK liquid-order matrix completed 216 cases and eight
format/model summaries, as recorded in the current renderer report. Its
post-evaluation identity-copy diagnostic does not establish private neural
filter quality.

Preceding small-model follow-up executable SHA-256:
`3f01732c22dea5915557a05659c60758466d77686f8b25998293f60bb2e95993`.
The SR adapter/native library remain unchanged. Owned-weave/factory checks,
six CPU suites, 36 exact unavailable-SR images and all 29 plain-menu/loading
cases pass again on this executable. Evidence:
`D:/SFE-validation/sr-small-model-default-oct3/results.json` and
`D:/SFE-validation/plain-menu-small-model-default-oct3/results.json`.
The optional small-model experiment stays OFF by default; its correctness
checks do not establish physical SR panel behavior or a sustained FPS gain.

Preceding owned-input memo follow-up executable SHA-256:
`42f277d06461d2bb735cd70a0aa5270c9ccf3c180a58697a2151e34012dd7d86`.
The adapter/native library remain unchanged. Six CPU suites, real owned-weave/
factory checks, 36 exact unavailable-SR images, and all 29 Preview-OFF/loading
cases pass again; the loading capture was visually inspected. Evidence:
`D:/SFE-validation/sr-model-memo-default-oct3/results.json` and
`D:/SFE-validation/plain-menu-model-memo-default-oct3/results.json`.
Both APIs also pass 50 exact stereo/mono images with scoped owned-input reuse.
Physical panel/lens/refresh behavior is still unverified; no release or device
installation was performed. The newest renderer report records remaining scope.

Preceding lazy-projection follow-up executable SHA-256:
`bc1927778e9054d314898c4ac2f3d32cf481efa4d8b91b4c5f08747d4040aa12`.
The optional adapter/native library are unchanged. Both APIs pass complete
projection/stereo checks and actual lazy pipeline creation/queued-output guards;
eight rebuilt CPU suites pass. Real owned-weave/factory guards and 36 exact
unavailable-SR images pass again on this executable. Evidence:
`D:/SFE-validation/sr-lazy-projection-default-oct3/results.json`.
The shipped path also passes 30 exact prior-checkpoint full images per API;
embedded mono K/M/K passes 96 evaluations/24 held uses/120 endings. All 29
Preview-OFF menu cases and the visually inspected RENDERING/loading capture
pass: `D:/SFE-validation/plain-menu-lazy-projection-default-oct3/results.json`.
All owned checks ended; all seven protected artifact hashes match.
Physical panel/lens/refresh behavior remains unverified; no runtime,
device or release installation was performed. See the current renderer report.

Preceding lazy-clipping follow-up executable SHA-256:
`369f87e1c53e2fa94c93f5d8d1357a3bbb53724b8bfa21a12b689e345542d625`.
The optional adapter/native library are unchanged. Both GPU APIs pass lazy
clipping/span creation and exact queued-output guards, full clipping and
stereo/MSAA, recycled span-clear and optional colour-tracer checks. Eight
rebuilt CPU suites pass, as do Intel first-use fixtures and Android clipping
syntax. Current default SBS images match the preceding checkpoint exactly
(30/API); real SR guards and 36 unavailable-SR images pass:
`D:/SFE-validation/sr-lazy-clip-default-oct3/results.json`. Mono K/M/K passes
96 evaluations/24 held uses/120 endings. All 29 Preview-OFF menu cases and
the visually inspected RENDERING/loading capture pass:
`D:/SFE-validation/plain-menu-lazy-clip-default-oct3/results.json`.
All owned checks ended; all seven protected artifact hashes match.
Physical panel/lens/refresh behavior remains unverified; no runtime, device
or release installation was performed. See the current renderer report.

Current SBS-material follow-up executable SHA-256:
`d31672b3ff538b7b200b459044f609ab8aa283fb92abb50188a4830f7a2db3c4`.
Ordinary face inputs share preparation/uploads only for matching material
state within one stereo pair; projection, visibility and derived shading are
still independent per eye. Both native stereo/MSAA suites and 60 exact SBS
images pass. Real owned-weave/factory guards and 36 unavailable-SR mono images
pass: `D:/SFE-validation/sr-stereo-faces-oct3/results.json`. Nine CPU suites,
2,697 source-model packets, Android syntax, embedded K/M/K and 29 menu/loading
cases also pass. The optional adapter is unchanged. Loaded timings do not
establish sustained speed or physical panel/lens/headtracking acceptance;
no runtime/device/release installation was performed. See the renderer report.
