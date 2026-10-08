# PC DLSS work — September 13

October 6 real-player rejected-command follow-up: the ordinary mono DLSS host
still cancelled an SDK-encoded command, unlike the calibrated renderer's
accepted drain policy. A separate actual player reproduces five D3D12 state
errors (ID 527) after that rejection. The fixed host submits the recorded work
in queue order, rejects its image/history and preserves the unjittered native
fallback; ordinary/held frames gain no new submission or wait. Failed draining
declines this device's SDK evaluations rather than attempting uncertain reuse.

`tools/check_dlss_rejected_work.ps1` passes Original/EX with both actual K/M
SDK models: one real post-evaluation rejection each, exact native first-frame
fallback, eleven recovered frames and twelve once-only frame ends per case.
Diagnostic capture retains ERROR/CORRUPTION messages without clearing or
enlarging the queue; critical AND discarded counts are zero after SDK shutdown
and SDL renderer destruction. Capture is enabled only by explicit player-test
flags, never ordinary settings. The negative executable/log remain separate.

The separate fixed candidate is
`D:/SFE-validation/rendering-player-rejected-drain-oct6/starfox_pc.exe`, SHA256
`C8F9654B6A7F7A9AF9D96DFE8973BA3DB44518D927C3A0A6039A40CCABB5DF36`.
All eight quality/model preview combinations pass in each experience, with
stationary native sky/HUD exact and frozen model/terrain correspondence valid.
This is normal PC mono-player ownership/recovery and scoped preview acceptance,
not neural image quality, Linux/Metal, physical stereo or whole-game FPS.
Installed current/PCVR players, main README, primary index and R42 are unchanged.
See CURRENT-ACCEPTANCE.md for adjacent player composition checks and remaining work.

October 3 current PC is `7DB600A7...`. ReShade/RenoDX/DLSS5 support and its
optional menu row are removed. Process-local ReShade Vulkan-layer suppression
runs before any graphics probe, and remains in force through renderer changes.
All eight Original/EX D3D12/Vulkan cases pass four live switches each with no
legacy module. Settings are unchanged; seven obsolete current-folder injector
config/log files were deleted after checks. The actual embedded standard K and
alternate M models still initialize/evaluate all four modes: eight frames each,
64 evaluations and 64 once-only endings. OFF retains both ordinary swapchains.
No claim about neural image quality or renewed full preview/SR matrices follows
from these lifecycle checks. SBS work is stopped; full effects/3DS goal active.
Current evidence: `D:/SFE-validation/no-reshade-oct3/manifest.json` and
`D:/SFE-validation/no-reshade-dlss-oct3`.

Historical accepted PC is `B46552B6...`. GPU ray-connectivity sharing changes no
DLSS selection, quality or render scale. Renewed embedded mono K/M/K passes 96
evaluations, 24 held uses and 120 once-only endings. All 29 menu/loading cases
pass, including both actual Software-to-GPU neural transitions. Both native GPU
APIs, 70 same-binary SBS images and 50 prior-checkpoint comparisons pass. All
owned checks ended; seven protected hashes unchanged. Prior broad SDK/TAA
matrices remain historical. Loaded timing does not establish sustained FPS;
physical, secondary-motion, finishing/memory and packaging scope remains open.
The newest renderer report records exact artifacts; full goal active.

Preceding development PC is `ED4F4DA0...`. CPU ray-connectivity sharing changes no
DLSS selection/quality. Renewed embedded mono K/M/K passes 96 evaluations,
24 held uses and 120 endings; all 29 menu/loading cases pass, including both
actual Software-to-GPU neural transitions. Both ray/stereo APIs and 50 exact
SBS images pass. All owned checks ended; seven protected hashes unchanged this
turn. Prior broad native SDK/TAA matrices are historical; physical/secondary-
motion/finishing/performance scope stays open, full goal active. See the report.

Preceding development PC is `D31672B3...`. SBS immutable face/material reuse
reduces actual upload traffic without changing 60 tested gameplay images;
both native stereo/MSAA APIs, nine CPU suites and source packing pass. Actual
embedded mono K/M/K still passes 96 evaluations/24 held uses/120 endings, and
all 29 plain-menu/loading cases pass, including both neural-model Software-to-
GPU transitions. SR guards and 36 unavailable-SR images pass. No DLSS selection
or quality changes. Earlier full native SDK/TAA matrices are historical; loaded
SBS timing does not prove sustained FPS. Physical/secondary-motion/finishing/
performance scope remains open, small-model stage OFF, full goal active.
The renderer report records exact current evidence and hashes.

Preceding development PC is `369F87E1...`; native owner checker `8161D814...`.
Clipping/span shader stages now compile independently. Both GPU APIs pass
actual creation-count/queued-output guards, full clipping and stereo/MSAA,
span-clear and colour-tracer suites; eight rebuilt CPU suites pass. Current
default SBS images match the preceding checkpoint exactly (30/API); SR guards
and 36 unavailable-SR images pass. Actual embedded mono K/M/K passes 96
evaluations/24 held uses/120 endings. All 29 Preview-OFF menu cases and the
visually inspected RENDERING/loading capture pass, including direct D3D12
restoration from Software for both neural models. Owned checks ended; seven
protected artifact hashes match.
Intel first-use fixtures and Android ARM64/API 26 clipping syntax pass. No
DLSS selection or quality changes; prior broad SDK/TAA/timing checks historical.
Physical/finishing/performance acceptance stays open; small-model stage OFF.
Exact artifacts and scope are in the renderer report; full goal stays active.

Preceding development PC is `BC192777...`; native owner checker `7268D388...`.
Projection preparation is independently lazy; both GPU APIs' complete
projection/stereo and exact preparation-count checks, eight rebuilt CPU suites
and Android ARM64/API 26 projection syntax compilation pass. The shipped path
passes 30 exact prior-checkpoint full images per API, renewed SR guards and
36 unavailable-SR images. Actual embedded mono K/M/K passes 96 evaluations,
24 held uses and 120 once-only endings. All 29 Preview-OFF menu cases and the
visually inspected RENDERING/loading capture pass, including direct D3D12
restoration from Software for both neural-model selections. All owned checks
ended; all seven protected artifact hashes match.
Both loaded ABBA profiles complete, with mixed CPU/frame timing rather than
sustained FPS acceptance. No DLSS quality/model selection
changed; prior full native SDK/TAA matrices are historical. Physical/finishing/
performance acceptance stays open, small-model stage OFF, full goal active.
The renderer report records hashes, diagnostic scope and unfinished checks.

Preceding development PC is `42F277D0...`; native owner checker `5F539C90...`.
Same-recording owned input reuse now works alongside shared stereo sources.
Both GPU APIs pass the new independent memo/stereo components and 50 exact
full stereo/mono images each. Six CPU suites and renewed SR guards/fallback
pass. Actual embedded mono K/M/K again passes 96 evaluations, 24 held uses and
120 once-only endings; all 29 Preview-OFF menu/loading cases also pass.
No prior full native SDK/TAA matrix is claimed renewed by this relink.
Loaded ABBA memo diagnostics complete on both APIs: uploaded bytes fall
12.2555%, while timing drifts/mixes. This is not isolated FPS acceptance.
Physical/finishing/performance acceptance stays open; small-model stage OFF.
The newest renderer report has exact artifacts and scope.

Preceding development PC is `3F01732C...`; native owner checker `4E01AC00...`.
This relinks an OFF-by-default bounded model-stage experiment; both APIs'
294 component cases and 768 real-model images each, plus six CPU suites and
owned-SR guards, pass. Actual embedded mono K/M/K preview passes 96 evaluations,
24 held uses and 120 once-only endings. Default-SR renewal passes 36 exact
images, full-SBS parity passes 30 per API, and all 29 plain-menu/loading cases
pass. Loaded cost evidence is mixed; no default/FPS rollout. The preceding
SDK/TAA matrices are historical, not
renewed current-binary acceptance. The newest renderer report has precise
hashes and evidence. Physical/finishing/performance acceptance remains open.

Preceding development PC is `ADC89620...`; native owner checker `C41C23E7...`.
Traced SDK water reconstructs raw radiance before supported post styles;
palette styles no longer enter a different pipeline order than complex effects.
The genuine K/M SDK full-image post-order matrix passes all 216 cases and eight
format/model summaries. It preserves
actual evaluation/lifecycle, then uses an equal-size identity-copy diagnostic;
this is not private neural-filter quality acceptance. D3D12/Vulkan TAA liquid
order (108 cases each), six CPU suites and 36 exact unavailable-SR images pass
on current artifacts. Embedded mono K/M/K passes 96 actual evaluations,
24 held uses and 120 once-only frame ends. All 29 Preview-OFF menu cases and
the viewed RENDERING/loading capture pass, without preferences/injector edits.
Exact checkpoint evidence and remaining scope are in
`RENDERER-PERFORMANCE-OCT1.md`. Physical Leia/Android, true secondary motion,
broader finishing/peak-memory, sustained SBS speed and packaging remain open;
Ally is deferred, full goal active. Release/settings/injector/APKs unchanged.

Historical development PC `8D6D2226...`. Full-panel liquid classification,
normals and depth now precede native SDK post styles/depth effects. Compensated
GPU plane arithmetic fixes near-horizon float rounding without retracing,
readback, shaderFloat64 or relaxed tolerances. D3D12/Vulkan independently pass
96 distinct geometric cases and the source liquid/style/AO/DOF components;
CPU, ray-history components, 112 SDK ray and 152 legacy SDK cases, mono K/M/K,
all 23 plain-menu/loading cases and 30 selected SBS images pass. Native SDK
original owner/liquid/full-HD, all 72 SDK sample-alignment and 296 legacy TAA
cases pass. All owned checks ended; all seven protected hashes match.
The first analytic implementation passed all original
SDK owner groups, new liquid finishing, full-HD and sample-alignment cases.
Previous DCDF acceptance and C5FADDAD's 128 SDK pattern cases are historical,
not newest-binary acceptance. Native TAA centre-liquid integration, true
secondary motion, broader finishing/peak-memory, physical Leia/Android,
sustained SBS speed and packaging remain open, Ally deferred, goal active.
ADB sees no device. Latest renderer report has exact artifacts and scope.

Previous PC `DCDF4F36...` passes 112 actual K/M consumed-ray/accepted-footprint
cases, 152 existing SDK components and owner ray/full-HD checks. SDK local
secondary rejection preserves dry motion/global eye history; it is not true
secondary motion or private neural-filter parity. Its owner regressions
and mono/menu/loading renewal completed. Physical Leia/Android, broader
finishing/liquid/peak-memory, SBS speed and packaging stay open; goal active,
Ally deferred. Release/settings/injector/APKs stayed unchanged, protected hashes
matched; other projects untouched. Newest report has exact acceptance scope.

Previous PC `8FCE5087...` retains embedded mono K/M/K: 96 evaluations, 24 exact
held uses and 120 once-only pair endings; all 23 plain-menu/loading checks pass.
SDK outlines now follow reconstruction and native MSAA ray samples include SDK
jitter. Actual old ray/history images are charged during layout replacement;
source-grid changes retire obsolete caches. Current SDK sample/full-HD checks
and original SDK regressions pass; both APIs' SBS renewal passes 80 exact images.
All owned checks ended; seven protected hashes match. The preceding broad 2,688 SDK effects matrix
passed on `306E7681...`; it is not a private-neural-filter parity claim. Wider
finishing/secondary/liquid/peak-memory, physical Leia/Android and sustained SBS
speed remain open; goal active, Ally deferred. Release/settings/injector/APKs
unchanged, other jobs untouched. See the newest renderer report for exact scope.

Previous PC is `E98BF0AD...`. Its new change shares immutable ordinary-stereo GPU
source uploads, not SDK formats, quality, history or calibrated correspondence.
Embedded mono K/M/K passes 96 evaluations, 24 exact held uses and 120 once-only
frame ends without SDK warnings. All 23 plain-menu cases plus RENDERING/loading
and both models' Software-to-D3D12 transitions pass, with no hidden Preview-OFF
work. Both APIs' stereo/ray fixtures, CPU suites and renewed SBS comparisons
pass. Native calibrated correspondence matrices are not renewed here. All owned
checks ended; other builds stay running, no isolated/sustained FPS claim. Wider
transport/private SDK parity, physical Leia/Android and SBS speed remain open,
Ally deferred, goal active; release/preferences/injector/APK unchanged. See the
newest renderer report for exact hashes and acceptance boundaries.

Previous PC is `F2B5FC6E...`. Its edge-witness implementation is for native TAA,
NOT the opaque native SDK history filter. SDK edge styles keep their conservative
rejection; no new SDK correspondence/private-filter parity claim. Embedded mono
K/M/K still passes 96 evaluations, 24 exact held uses and 120 once-only frame
ends without SDK warnings. All 23 plain-menu cases plus RENDERING/loading and
both models' Software-to-D3D12 transitions pass, with no Preview-OFF SDK/effect/
scene work. Native TAA components/live owners, full compositors, legacy TAA, CPU
and shader checks pass. All owned checks ended; other builds stay running, no
isolated/sustained speed claim. Wider transport and physical Leia/Android stay
open, Ally deferred, full goal active; release/preferences/injector/APK unchanged.
The newest renderer report records exact native/SDK boundaries and hashes.

Previous PC is `523EEB2D...`. Its new change shares ordinary-stereo immutable
animation-frame coordinates/visibility only, not SDK formats, quality, history
or native calibrated output. Four GPU component routes, 13 selected CPU suites,
cartridge source equivalence and 200 exact D3D12/Vulkan app images pass. Embedded
mono K/M/K passes 96 evaluations, 24 held uses and 120 once-only frame ends
without SDK warnings. All 23 plain-menu cases plus RENDERING/loading pass,
including both models' Software-to-D3D12 transitions and no hidden Preview-OFF
scene/effect/SDK work. Calibrated native SDK owner matrices are not renewed.
All owned checks ended; other builds stay running, no isolated/sustained speed
claim. Physical Leia/Android, private-filter parity and wider native transport
remain open, Ally deferred, full goal active. Release/preferences/injector/APK
unchanged. See the newest renderer report for hashes and precise renewed scope.

Previous PC is `C95CC05E...`. Its change shares immutable model-local source
preparation only within ordinary stereo pairs, not SDK quality/payloads,
temporal history or native Leia eye output. Full GPU stereo components on both
APIs, eleven CPU suites and 200 exact D3D12/Vulkan app images pass. Final embedded
mono K/M/K passes 96 evaluations, 24 held uses and 120 once-only frame ends
without SDK warnings. All 23 plain-menu cases plus RENDERING/loading pass,
including both models' direct Software-to-D3D12 transitions and no Preview-OFF
scene/effect/SDK work. The broader calibrated SDK owner matrices are not renewed.
All owned checks ended; other builds stay running,
no isolated/sustained speed claim. Physical Leia/Android, private-filter parity
and wider native correspondence remain open, Ally deferred, full goal active.
Release/preferences/injector/APK unchanged. See the newest renderer report for
hashes, evidence and precise renewed scope.

Previous PC is `AE3F4755...`. Its new change shares ordinary-stereo fog geometry,
not SDK formats, signed payloads, quality or native temporal history. Final mono
K/M/K preview passes 96 evaluations, 24 held uses and 120 once-only frame ends
without SDK warnings (`shared-fog-final-mono-dlss-oct3/toggle.log`). All 23 plain-
menu cases and RENDERING/loading pass, including both models' Software-to-D3D12
transitions; Preview-OFF does no scene/effect/SDK work. Fog components,
eleven CPU suites and exact ordinary stereo comparisons are separate evidence.
The broader native two-eye SDK owner matrix is not renewed. Physical Leia,
private-filter parity and sustained speed remain open; other builds stay running,
Ally deferred, full goal active. Release/preferences/injector/APK unchanged.
See the newest renderer report for full hashes and precise renewed scope.

Previous PC is `491003E4...`. Native independent SDK eyes reuse only source
identity/topology preparation for their accepted frame, with eye-specific
camera/motion/depth guides and separate blur/TAA history. The full real D3D12
K/M four-format native owner passes, including palette/pattern/local-reactivity,
held/reset and cancellation/reconnect/resource lifetime. Embedded ordinary mono
K/M/K separately passes 96 evaluations / 24 held uses / 120 once-only frame ends
without SDK warnings. Native TAA/blur on both APIs, CPU suites and 40 selected
ordinary app images also pass. Signed payload/SDK formats and RGB order are
unchanged; independent SDK component and 23 plain-menu/loading matrices were
not rerun. No physical Leia/private-filter parity or sustained speed claim.
Other builds remain running, Ally deferred, full goal active; release/settings/
injector/APK unchanged. See the newest renderer report for hashes and scope.

Previous PC is `7A6ED2FA...`: the only app change is a quiet ordinary-stereo route
marker. Embedded mono K/M/K passes 96 evaluations, 24 held uses and 120 once-only
frame ends without SDK warnings. Fifty selected ordinary D3D12/Vulkan images and
ten CPU suites also pass. Native SDK/Leia shaders and transport are unchanged;
broader matrices below are not renewed. The single quiet speed batch is rejected
for external compiler overlap, not an accepted FPS result; other builds were
never interrupted. Full goal active, physical Leia/Android and broader transport/
speed remain open, Ally deferred, release/settings/injector/APK unchanged.

Previous PC is `ABC68124...`. The new optimization is ordinary-stereo fixed source
sharing, not a native SDK shader/owner change. Embedded mono K/M/K is renewed:
96 evaluations, 24 exact held uses and 120 once-only frame ends without SDK
warnings. All 23 plain-menu/loading cases and both models' Software-to-D3D12
transitions pass; Preview-OFF does no hidden scene/effect/SDK work. Both APIs'
ordinary stereo and Original/EX briefing checks are separate. No private neural-
filter, physical Leia/Android or sustained speed claim; other builds stay running,
Ally deferred, full goal active. See the newest renderer report for full hashes
and scope. The preceding full native SDK component/owner matrix is not renewed
by these ordinary app tests.

Previous PC is `258CA386...`. Native independent SDK eyes now supply AA/accepted-
phase rejection guides for Dithered, Night Vision, Scanlines and CRT Phosphor,
including clean surfaces over previously patterned pixels. The R8 current-bias
and RG32 motion SDK contract, signed runtime and RGB/post order are unchanged.
Extra float phase images are lazy; pattern-OFF uses the original guide shader.
Real K/M/four-format components and the complete D3D12 native owner matrix pass,
including 96 added Balanced pattern owner cases, held clocks, movement, intensity
resets, exact protected ink/opacity and rejected/failing attempts. Mono K/M/K
preview and ordinary app/menu evidence are separate. See the newest renderer
report for full hashes and matrix scope. This is not private neural-filter parity,
broader post/secondary correspondence, physical Leia/Android or sustained speed
acceptance. Other builds stay running, Ally deferred, full goal active.

Previous PC is `8EB8241C...`. The new phase witness is for native calibrated TAA,
not opaque SDK history: the four point-pattern styles remain rejected in native
DLSS until input-AA/previous-footprint correspondence is implemented. Ordinary
embedded K/M/K preview renews 96 evaluations / 24 held uses / 120 once-only frame
ends without SDK warnings. Signed payloads and mono SDK presentation lifecycle
are unchanged. All 23 plain-menu cases and RENDERING/loading pass, including both
SDK models' direct Software-to-D3D12 transitions, with no Preview-OFF scene/SDK
work and unchanged preferences/injector log. Native TAA component/owner and
ordinary stereo comparisons pass;
see the newest renderer report for hashes and scope. Full two-eye SDK, physical
Leia/Android and broader effect/performance acceptance remain open; goal active.

Previous PC is `51EF42F1...`. The newest change makes ordinary-stereo per-eye
composition/effects queue reuse automatic; mono DLSS lifecycle and signed SDK
payloads are unchanged. See the newest renderer report for separate stereo
acceptance. Current K/M/K mono menu preview passes 96 evaluations, 24 held uses
and 120 once-only frame ends without SDK warnings (`sbs-default-queue-quiet-final-dlss-oct3`).
The clean plain-menu rerun passes all 23 Preview-OFF cases and RENDERING/loading,
including direct Software-to-D3D12 transitions for both models; saved preferences
and the injector log remain unchanged during that rerun. The first menu run
omitted the per-process Vulkan injector opt-out and is not clean-isolation
acceptance. See the newest renderer report for retained evidence and scope.
This does not renew the calibrated native two-eye SDK matrix or
physical panel acceptance; full goal remains active and other builds stay running.

Previous PC is `807730CC...`. Mono DLSS retains SDL's native DXGI swapchain and
uses explicit common-plugin frame-end tickets; the proxy's refcount warning is
absent, not filtered. Both models match 24 proxy-control images exactly across
the selected moving/held cases. Four real post-SDK command cancellations now
match mono fallback and recover: consumed SDK frame indices advance even on
cancelled submissions instead of trapping later frames in duplicate constants.
The isolated same-hash EXE-only host passes all eight embedded K/M quality modes
and both OFF backends. Final K/M/K (96 evaluations/24 held uses/120 frame ends),
OFF/ON/OFF (32/8/40), all 23 plain-menu cases and RENDERING/loading pass without
SDK warnings on the explicit path. Final ordinary SBS checks pass 40 exact
D3D12/Vulkan images across selected water, MSAA and mono recovery cases.
See the newest renderer report for hashes,
evidence and scope. This does not certify all-effects reconstruction,
native effect correspondence or physical Leia/Android. Full goal remains active;
other builds remain running, release/preferences/ReShade unchanged.

Previous PC is `A97EB3CB...`. The newest change fixes fence retention in the
opt-in ordinary SBS experiment; it does not alter the SDK API or DLSS projection.
Final embedded Standard/4.5/Standard menu preview passes 96 evaluations / 24
held uses and resets history on model changes
(`ordered-reuse-final-dlss-oct3/toggle.log`). See the newest renderer-performance
checkpoint for the separate SBS checks and rejected timing batch.
The known mono SDK shutdown/refcount warning, full native correspondence and
physical Leia acceptance remain open. Release/preferences/ReShade unchanged.

Previous PC `D6E2BF61...` again passes embedded Standard/4.5/Standard preview:
96 evaluations / 24 held uses and reset on each model switch. The host's ABI
description is now independent of optional XR camera headers; a standalone
no-XR target and ordinary Android build pass. No SDK behavior or camera math
was changed by that split. Settings/journals now commit completed temporaries.
The known mono shutdown warning and physical Leia/full calibrated SDK acceptance
remain open. See the latest renderer-performance checkpoint for exact scope;
release/preferences/ReShade remain unchanged.

Previous PC `F3066240...` retains working embedded Standard/4.5/Standard
preview reconstruction: 96 evaluations and 24 held uses, with reset on each
model switch. Ordinary SBS MSAA is now connected separately; existing
neural/spatial-upscaler incompatibility gates remain. This mono check does not
renew the prior full calibrated native SDK matrix or physical Leia acceptance.
See the newest renderer-performance checkpoint for exact scope. The known mono
shutdown warning remains; release/settings/ReShade are unchanged.

Previous PC `33103828...` retains working embedded Standard/4.5/Standard
preview reconstruction: 96 real evaluations and 24 held images, with history
reset on each model change. OFF/ON/OFF preview separately passes 32 real
evaluations and 8 held images. This is ordinary mono preview acceptance, not
renewal of the previous calibrated native SDK owner matrix. The dedicated
span-clear rollout is covered by the latest renderer-performance checkpoint.
The known mono shutdown warning remains; release/settings/ReShade are unchanged.

Previous PC `90488E3F...` adds native SDK current-colour bias with actual liquid
coverage/depth, preventing liquid/new geometry from resetting the entire eye.
Real SDK four-format components and retained owners pass both models/all modes,
SSAA/MSAA footprints, dry motion, animated water/lava, protected ink, held
frames, failures and HWND reconnect. Ordinary K/M/K passes 96 evaluations/24
retained previews. Old adapters lacking the V2/native lifecycle entry points
correctly retain native FSR fallback; mono V1 is unchanged. Exact acceptance
scope/hashes are in the newest stereo checkpoint. Fluid/world/post/secondary
parity, physical Leia and sustained performance remain open. All 23 plain-menu/
loading, four CPU and four off/incompatible/backend-fallback checks pass. The mono shutdown
warning remains; release and saved preferences are unchanged.

Previous PC `BFBBF140...` fixes the native OpenXR SDK frame-end/garbage-collection
gap through the real common before/after-present hooks. Four-format/cartridge
and HWND reconnect checks pass 875 cleanups plus four rejected-and-retried
cleanup calls without reevaluation, early retirement or SDK errors/warnings.
The standalone component, ordinary K/M/K (96 evaluations/24 held previews), all
23 plain-menu/loading cases, CPU suites and no-panel recovery pass. Exact
binary scope is in the newest STEREO-DISPLAY-UPGRADE.md checkpoint. Physical
Leia, liquid/reactive/secondary parity and sustained performance remain open;
the separate mono swapchain shutdown warning remains. Release/settings unchanged.

Previous PC `233633B5...` has real native Leia D3D12 SDK eye integration (viewports
100/101, not mono 99). Its retained-owner/cartridge checks pass both models/all
modes, real input rasters, full-panel ink, held source, cancellation/reset and
desktop reconnect. SDK frame tokens survive owner replacement without duplicate
constants. Native Vulkan/unsupported devices keep FSR; ordinary SBS still does
not use mono DLSS. Current ordinary K/M/K passes 96 evaluations/24 held previews
in `D:/SFE-validation/native-dlss-final-sdk-oct2`, and all 23 plain-menu/loading/
renderer cases pass in `native-dlss-final-menu-oct2`. The known mono swapchain
shutdown warning remains. See STEREO-DISPLAY-UPGRADE.md for exact scope: physical
Leia, large-panel/sustained performance and liquid/reactive parity are not proven.
Release and settings are unchanged; the full goal remains open.

Previous PC `0C99E5AC...` passes 96 K/M/K SDK evaluations and 24 retained previews
in `D:/SFE-validation/stereo-fork-dlss-oct2`. All 23 Preview-OFF cases, renderer
navigation and RENDERING loading pass in `stereo-fork-menu-oct2`. The known SDK
shutdown/refcount warning remains. Crossview is an ordinary stereo output;
it does not enable mono SDK DLSS in stereo. Native FSR component/owner
regressions pass both backends, but native Leia SDK DLSS remains unfinished.
Release and preferences are unchanged; see the newest stereo checkpoint.

Previous PC `286B78B1...` passes 96 K/M/K SDK evaluations and 24 retained previews
in `D:/SFE-validation/native-fsr-delivery-dlss-oct2`, and all 23 plain-menu cases,
renderer navigation and RENDERING loading in `native-fsr-delivery-menu-oct2`.
The existing SDK shutdown/refcount warning remains. Native Leia now exposes its
actual per-eye FSR1 path on every supported vendor, not the ordinary mono SDK
host; native SDK DLSS remains unfinished. Leaving Leia restores vendor-specific
ordinary upscaler selection. Release and both preferences are unchanged.

Previous PC `14E7AD30...` passes 96 actual K/M/K SDK evaluations and 24 retained
previews after the native liquid/TAA change. Evidence is
`D:/SFE-validation/native-liquid-taa-final-dlss-oct2`. The known shutdown
swap-chain/refcount warning remains; release is unchanged. The latest stereo
checkpoint covers native rendering scope without borrowing older-hash proof.
All 23 current plain-menu cases, renderer navigation and RENDERING loading pass
in `D:/SFE-validation/native-liquid-taa-final-menu-oct2`; both saved preferences
are unchanged.

## October 2 — backend-qualified environment build

Current PC `6263A3729708B8AEC1DF384E2E4FD4023B05FF4B124EF932797668792A7AC47C`
passes standard/4.5/standard switching: 96 actual SDK evaluations, 24 retained
preview frames and history resets at model changes. All 23 plain-menu cases,
renderer navigation and the RENDERING loading gate pass. Evidence is
`D:/SFE-validation/environment-reflection-final-{dlss,menu}-oct2`.
The SDK still emits its known swap-chain release/refcount warning; these checks
are not claimed warning-free. Both preferences and release `828D98F4...` are
unchanged. The ray-ground pass optimization changes neither discovery, model
selection nor requested upscale. Its D3D12-only default and limited timing
benefit are recorded in RENDERER-PERFORMANCE-OCT1.md. Physical Leia/native
upscaler parity and broader enhancement/performance acceptance remain open.
Older checkpoints below retain their own executable scope.

## October 2 — native water build retains SDK/menu acceptance

Current PC `925102D7F78765FDE88AE45AC5C4122C7D8F48746BC722910D779EF82AA9289E`
passes standard/4.5/standard K/M/K switching, 96 real SDK evaluations and
24 retained preview frames with model-change history resets. All 23
Preview-OFF cases, renderer navigation and the RENDERING loading gate pass.
Logs are in `D:/SFE-validation/native-water-ripples-oct2/{dlss,menu}`.
Native procedural-water/Auto-ramp changes do not change SDK discovery,
requested scale, model selection or backend policy. Both saved preferences
and release `828D98F4...` remain untouched. Physical Leia/native-upscaler,
additional enhancement parity and sustained SBS performance remain open;
the full goal is not complete.

## October 2 — optional GPU timing retains native SDK switching

Current diagnostic `49C2FA300EFC22C08B5E0FD66888CEFC3EF39A96E2BABE6E5A5E73B1A5D7430D`
passes actual standard/4.5/standard switching: 96 SDK evaluations, 24 retained
preview frames and history resets on model changes, in
`D:/SFE-validation/scene-timestamps-final-dlss-oct2`. GPU timing is opt-in; no
runtime discovery, SDK model, requested scale or backend policy is changed.
All 23 current Preview-OFF cases and the RENDERING loading gate pass in
`scene-timestamps-final-menu-oct2`, with unchanged preferences. The preceding
`0A78D5E1...` separately passes those cases in `scene-timestamps-menu-oct2`.
Release `828D98F4...` and saved preferences remain
untouched. Physical Leia/native-upscaler and broad performance acceptance remain
open; GPU/host timestamps are diagnostic evidence, not a claimed speedup.

## October 2 — OFF-policy ray preparation build retains SDK switching

Current-folder diagnostic is
`630671F5A44D024EC4D1590684FC8B54BFD516391ED0872F13E8304B3528B652`.
Actual standard/4.5/standard switching passes 96 SDK evaluations and 24 retained
previews, with model-change resets: `D:/SFE-validation/ray-prebuild-final-dlss-oct2`.
All 23 current Preview-OFF cases and RENDERING loading pass in
`ray-prebuild-final-menu-oct2`, with unchanged preferences. The preceding 9D98BF51
build separately passes those cases in `ray-prebuild-menu-oct2`. Neither new
ray cache is enabled by default; neither changes runtime discovery, model
selection or requested render scale. Release `828D98F4...` and saved settings
stay untouched. Physical Leia/native-upscaler and full performance acceptance
remain open; see the current GPU/performance checkpoints.

## October 2 — active-stereo-caster build retains DLSS/menu acceptance

Current-folder diagnostic is
`16D3040D31FFE6FFAEE2A7650DA4783FAA0898E156F9DEEE5B14C3C65D815CBE`.
Actual standard/4.5/standard switching passes 96 SDK evaluations / 24 retained
preview frames with resets on model changes. All 23 Preview-OFF cases and the
RENDERING loading transition pass, with unchanged saved settings. Evidence:
`D:/SFE-validation/sbs-active-casters-{dlss,menu}-oct2`. No SDK/runtime model
change or forced render upscale is introduced. Release `828D98F4...` remains
untouched. This does not establish physical Leia/native-upscaler parity or
whole-goal performance acceptance; the active-caster timing report records
mixed NVIDIA results, sampled Intel improvements and remaining stalls.

## October 2 — exact-arithmetic diagnostic retains verified DLSS

The preceding current-folder diagnostic is
`CB42012FD7AECCE1087522BCC6339F671063C43248048648A65FA9A394807E04`.
Actual standard/4.5/standard switching passes 96 SDK evaluations / 24 retained
previews, with history resets on each model change. All 23 Preview-OFF cases
and the RENDERING loading capture pass. Evidence on D:/SFE-validation uses
`fp64-multiply-{dlss,menu}-oct2`; preferences remain A2B8BDBA / E0F794E8.
The release stays the previously verified `828D98F4...`, with no runtime/model
change, forced Render Upscale, copy, publication or device deployment.
The GPU/performance checkpoints record the arithmetic evidence, timing limits,
open Intel reflection mismatch and unchanged full-goal scope. Passing these
ordinary desktop checks does not establish native Leia upscaler/device parity.

## October 2 — stereo diagnostic retains verified DLSS

The current-folder diagnostic is
`CDDE775F6818E5757A9F4F00493D8C709108F9C21AC5255C700A7A1837BBC22C`.
Actual standard/4.5/standard switching passes 96 SDK evaluations / 24 retained
previews. All 23 Preview-OFF cases and RENDERING loading pass; current/release
preferences remain A2B8BDBA / E0F794E8. Evidence on D: uses
`parallel-stereo-immutable-rows-{dlss,menu}-oct2`.
The release folder remains the previously verified `828D98F4...`; no copy,
runtime/model change, forced Render Upscale or publication occurs. The stereo
worker experiment stays opt-in. The newest GPU/performance checkpoints record
its fixed shared-row race, rejected older timing candidate and remaining goal.
Earlier identical-folder notes below are historical installation checkpoints.

## October 2 — compositor recovery build retains verified DLSS

Both local PC folders contain SHA-256
`828D98F478FAADE2D0B30FC6732CBC1E3038A83AB2BA7B0E1BBE42BEBDB9201C`.
Current and installed release each pass standard/4.5/standard SDK switching:
96 actual evaluations, 24 retained preview frames and correct history resets.
Logs are `composite-final-dlss-models-oct2/toggle.log` and
`composite-installed-dlss-models-oct2/toggle.log` under D:/SFE-validation.
The separate OFF/ON/OFF check also passes (`composite-final-dlss-oct2/toggle.log`),
with 32 SDK evaluations, eight retained previews and renderer restoration.

All 23 Preview-OFF cases and the RENDERING loading presentation pass. Only the
EXE was copied; preferences remain A2B8BDBA / E0F794E8. Previous release backup:
`D:/SFE-validation/composite-previous-release-908BABE0-oct2.exe`.
No runtime/model change, forced Render Upscale, download or publication occurred.
The compositor recovery fix is independent of DLSS; shader specialization stays
opt-in because its timings do not justify a default performance claim.

## October 1 — SBS profiling build retains verified DLSS

Both local PC executable folders now contain SHA-256
`908BABE0F8B1EC9323945DFF5FF57B7211659EAC387D7E77B197BCC44CC2E280`.
The current-folder build passes standard/4.5/standard switching with 96 actual
SDK evaluations / 24 retained previews. All 23 Preview-OFF cases and the
RENDERING indicator pass; preferences remain A2B8BDBA / E0F794E8. Logs on D:
are `gpu-sbs-final-buffer-{dlss,menu}-oct1`.
Only the verified executable was copied into the release folder. The previous
release is recoverable at
`D:/SFE-validation/gpu-sbs-previous-release-73EDE4A7-oct1.exe`.
The installed release-folder K/M/K retest also passes 96 SDK evaluations / 24
retained previews (`gpu-sbs-final-buffer-installed-dlss-oct1.log` on D:), with
its saved settings unchanged.
There is no DLSS runtime/model change or forced Render Upscale. The separate
SBS submission experiment remains off because its timings do not justify a
default change; accurate cost reporting is not a sustained-FPS fix.

## October 1 — liquid-depth/style release retains verified DLSS

Both local PC executable folders contain SHA-256
`73EDE4A755CAAF40CA95B85C87A24462D1014F1ACB7CDE872D17F897F62EAAE0`.
The final candidate passes standard/4.5/standard switching with 96 actual SDK
evaluations / 24 retained preview frames. All 23 Preview-OFF cases and the
RENDERING indicator pass; preferences remain A2B8BDBA / E0F794E8. Logs on D:
are `native-water-guides-final-pc-{dlss,menu}-oct1`. The previous `63F29CF3...`
release executable is recoverable at
`D:/SFE-validation/native-water-guides-previous-release-63F29CF3-oct1.exe`.
The installed release-folder K/M/K retest passes the same 96 SDK evaluations /
24 retained previews (`native-water-guides-installed-dlss-oct1.log` on D:).
No runtime download, model change or forced Render Upscale was introduced.
The newest stereo/GPU checkpoints record the native depth/style scope;
physical acceptance, native upscaler parity and sustained performance remain
separate, still-open full-goal requirements.

## October 1 — liquid shutter release retains verified DLSS

Both local PC executable folders contain SHA-256
`63F29CF3D9DE1EDAEFB55169DF64F4EE20A823A322258304DBDC1405ED4C2A52`.
The rebuilt candidate and installed release each pass standard/4.5/standard
switching with 96 SDK evaluations / 24 retained preview frames. Logs on D:
are `native-water-layers-pc-dlss-oct1.log` and
`native-water-layers-installed-dlss-oct1.log`. All 23 Preview-OFF cases and
the RENDERING indicator also pass; preferences remain A2B8BDBA / E0F794E8.
No runtime download/model change or forced Render Upscale is introduced.
The prior `3CB14B61...` executable is recoverable at
`D:/SFE-validation/native-water-layers-previous-current-3CB14B61-oct1.exe`.
The newest stereo/GPU checkpoints document native liquid-shutter scope and
the still-open full goal; native neural/FSR parity and physical/performance
acceptance remain separate requirements.

## October 1 — native water release retains verified DLSS

Current and release executable SHA-256 is
`3CB14B61D3CAB28660AB6A049C9BBC7D7CA613B567D391B524A8F8F7368BFB30`.
The final candidate and installed release each pass standard/4.5/standard
switching with 96 actual SDK evaluations and 24 retained preview frames.
Logs: `D:/SFE-validation/native-water-affine-pc-dlss-oct1.log` and
`native-water-installed-dlss-oct1.log`. All 23 final Preview-OFF cases and the
RENDERING transition pass. Current/release preferences remain A2B8BDBA / E0F794E8.

The preceding F92664B6 water candidate separately passes all eight embedded
standard/4.5 modes in a standalone folder without loose DLLs, OFF restoration
on D3D12/Vulkan, and seven recovery cases per backend. Those logs are retained
under `native-water-standalone-dlss-oct1` / `native-water-pc-*` on D: and remain
attributed to that earlier executable. The final change corrects native-water
affine lighting/caustic transforms; it does not change runtime packaging or
force a higher Render Upscale. No new SDK download or DLSS5 embedding occurred.

The E2B4DC87 predecessor is recoverable as
`D:/SFE-validation/native-water-previous-current-E2B4DC87-oct1.exe`.
Native Leia neural/FSR parity, device acceptance and broader SBS performance
remain open. The newest stereo/GPU checkpoints record the water scope and
remaining goal; this is not full migration completion.

## October 1 — current-tree embedded runtime corrected

The rebuilt `build/current` tree was still configured without embedded DLSS
and used its older loose adapter. Standard reconstruction worked, but 4.5
failed because that adapter lacks the model-selection v2 export, not because
this GPU is incompatible. The failed `915C70E9...` candidate was never installed
as the release. The tree now uses the same verified staged production package
as release via `STARFOX_DLSS_RUNTIME_DIRECTORY`; the missing-export diagnostic
also identifies the loaded runtime rather than blaming the adapter/GPU.

Installed `build/release` and `build/current` hash `E2B4DC87...` passes
standard/4.5/standard switching with 96 actual
SDK evaluations and 24 retained preview frames, all eight modes in a standalone
folder without loose DLLs, OFF restoration on D3D12/Vulkan, plain-menu/loading
and GPU recovery regressions. All 32 Original/EX × standard/4.5 × four modes ×
selected 1x/2x cases pass 1,024 real SDK evaluations; scale is preserved, never
automatically promoted to 6x. The old `3C104DE2...` executable is backed up on D:.
The final installed-folder K/M/K check also passes 96 actual SDK evaluations
and 24 retained preview frames on that same hash
(`D:/SFE-validation/native-metal-installed-dlss-oct1.log`). The newest GPU
checkpoint records final installation status and exact evidence. No new SDK
download or DLSS5/ReShade embedding is claimed. Native Leia neural/FSR parity,
physical-device validation and broader SBS performance remain open.

## October 1 — installed recovery release: final scale matrix passed

Installed executable hash is
`3C104DE218605C989F542D4CC7EB6295385D8804B73C1D0DD9B0634A1ED8161F`.
`D:/SFE-validation/gpu-recovery-installed-scales-oct1/results.json` records
all 32 Original/EX, standard/4.5, four-quality and selected 1x/2x combinations,
with 1,024 real SDK evaluations and the requested output scale unchanged.
Installed K/M/K passes 96 evaluations / 24 retained preview frames;
23 Preview-OFF/loading cases pass on the identical candidate hash. Preferences
are preserved. This proves local SDK availability/evaluation and scale/history
regressions; it does not claim physical Leia neural-upscaler parity or an SBS
FPS improvement. Native reflective-ground work is a subsequent source candidate,
not part of this installed executable's evidence.

## October 1 — native ground-surface release: ordinary DLSS unchanged

Installed local release hash is
`60C8BCAD0B2566393012DAF14B14440B9266FFC3212B12CE37F2E649A2FF2436`.
All 32 Original/EX × standard/4.5 × four modes × selected 1x/2x cases pass
1,024 actual SDK evaluations and retained reconstructed previews; requested
upscale is preserved, never automatically promoted to 6x. K/M/K passes 96
evaluations / 24 retained frames in both candidate and installed folders.
All 23 Preview-OFF cases and the RENDERING indicator pass, with unchanged
settings. Evidence is under `D:/SFE-validation/native-ground-surfaces-*`.
The previous `879BD987...` binary is backed up as
`native-ground-surfaces-previous-release-879BD987-oct1.exe` on D:.
Release/current preferences remain E0F794E8 / A2B8BDBA; build/current, devices
and publications are unchanged. Native neural/FSR parity and broader GPU/SBS
performance remain open. At the user's request, Ally diagnosis is deferred;
these SDK/menu checks do not verify its 120-FPS startup path.

## October 1 — native joint particle release: scale policy retained

Installed local release hash:
`879BD987A2B8A37D9AD9FD2463FBF70ECF201AC04AF8CCB8B104D89E17FB3938`.
Its native Leia particle/shutter integration does not change ordinary DLSS
selection or automatically promote Render Upscale. All 32 Original/EX ×
standard/4.5 × four modes × selected 1x/2x cases pass 1,024 actual SDK
evaluations at 800x224 / 1600x448, followed by retained reconstructed previews.
Evidence: `D:/SFE-validation/native-joint-particle-dlss-scale-oct1/results.json`.
The earlier explicit 6x ultrawide correction is retained; its older-binary
evidence below is not relabeled as a new 6x matrix on this executable.
Native Leia neural/FSR/upscale parity is still separate and open.

K/M/K passes 96 evaluations and 24 retained-preview frames in
`native-joint-particle-model-toggle-oct1/toggle.log` on D:. All 23 Preview-OFF
cases and the RENDERING transition pass. Candidate-location slow-bootstrap
responsiveness passes, but the installed-folder probe hits a 2,141-ms stall
after presenter preparation. Installed-folder K/M/K also passes 96 evaluations
and 24 retained frames. The stereo checkpoint retains the failure and its
scope; this is not a blanket startup/ROG acceptance claim.
Release preferences retain E0F794E8. The previous 2BE878CE executable is backed
up at `D:/SFE-validation/native-joint-particle-previous-release-2BE878CE-oct1.exe`.
Current-folder preferences are preserved at their observed A2B8BDBA hash;
current executables, devices and published releases are unchanged.

## October 1 — SDL presenter bootstrap responsiveness

Installed local release hash is now
`2BE878CEC73D67137E606DD9203CAE13456CDBAD51F686B6B9BEE852E9EADED6`.
The pinned Windows SDL GPU renderer joins a worker for its built-in presenter
shader compile, extending the existing game-shader preparation policy. Device,
window/swapchain and presentation ownership do not move threads. This does
not change SDK selection, render upscale, temporal sampling or scene quality.

All 32 Original/EX × standard/4.5 × four modes × selected 1x/2x cases pass
1,024 actual SDK evaluations at the requested output size, followed by held
previews (`sdl-presenter-dlss-scale-r2-oct1/results.json` on D:). K/M/K passes
96 evaluations / 24 retained frames; 23 Preview-OFF cases plus preview-loading
pass with unchanged preferences. The installed binary passes K/M/K again and
an injected eight-second bootstrap responsiveness check. Shader-bootstrap
stress/ordinary startup tests and remaining synchronous/device limits are
recorded in `RENDERER-PERFORMANCE-OCT1.md`; ROG acceptance is still unproven.

The first scale-check invocation ended exactly at the 32-sample accumulation
boundary and correctly lacked any subsequent retained frame. Its failure is
retained, not counted as a product regression/pass. The tool now requires at
least 33 frames; the successful matrix uses 40. The explicit 6x ultrawide
ownership/dispatch correction below is retained; its older-binary evidence
is not relabeled as a fresh full 6x matrix on this executable.

Saved preferences retain SHA-256
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
Previous 1D9634FD binary backup:
`D:/SFE-validation/sdl-presenter-previous-release-1D9634FD-oct1.exe`.
build/current and published releases are unchanged. The broader goal remains open.

## October 1 — explicit ultrawide scale and temporal ownership

Installed local `build/release/starfox_pc.exe` SHA-256:
`1D9634FDBFA65A7F3B22F371CA2E938F63FA8ABEC0179A2F4D9F38455A5F0802`.
The independent Render Upscale policy below is retained: neither DLSS variant
automatically selects 6x. An explicitly selected 6x 32:9 output now passes all
16 actual SDK cases (Original/EX, both variants, all four modes), with 512
evaluations at 4800x1344 and retained reconstructed previews. Evidence is
`D:/SFE-validation/dlss-wide-selected6-final-matrix-oct1/results.json`.

Background/temporal extents allow up to 8192 on either axis while retaining
the former 4096x4096 total-pixel budget. Large per-pixel motion and scene merges
split over legal dispatch rows rather than exceeding 65,535 groups on X.
Temporal resampling uses exact rational footprints: zero-area neighbors can
no longer donate foreground depth or motion, including at odd equal widths.
D3D12 and Vulkan each pass 8,388,608 independently checked motion/merge pixels
across the dispatch boundary, wide/portrait background and guide fixtures,
exact color/depth/motion ownership, reset and invalid-sentinel checks. The five
CPU suites and all 11 captured-preview checker negative controls pass.

All eight 1x reconstructed previews settle with zero physical model variation;
text/HUD error is zero and models genuinely differ from native. The initial
capture invocation incorrectly combined native-identity and reconstruction-
difference checks; retained captures pass the correct held-model check, and
the script now rejects that contradictory combination before launching.
`dlss-wide-dispatch-held-preview-oct1/stability.json` records this distinction.
Representative images/logs remain; 193,573,260 bytes of redundant generated
frames were removed and can be regenerated. All 23 plain-menu cases and the
RENDERING/loading transition pass, as does candidate K/M/K (96 evaluations,
24 retained frames). Default-path paired timings are recorded in the renderer
report; they are not a broad GPU/SBS performance fix.
The installed-folder K/M/K check also passes 96 evaluations and 24 retained
frames in `dlss-wide-dispatch-installed-model-toggle-oct1/toggle.log` on D:.

Preferences remain `E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
The previous 524DD82F executable is recoverable at
`D:/SFE-validation/dlss-wide-previous-release-524DD82F-oct1.exe`.
`build/current`, publications and device installations are unchanged.
Native Leia neural parity and physical Leia/Android/ROG acceptance remain open.

## October 1 — selected Render Upscale is authoritative

Installed local `build/release/starfox_pc.exe` SHA-256:
`524DD82F13A48BFFFFE6C37911A258B1C0FDE005A6FB5FA716FF0C370727A343`.
This supersedes the September 30 automatic display-sized scale policy below.
DLSS and DLSS 4.5 no longer promote the renderer/output to a display-derived
scale (up to 6x), nor relabel Render Upscale as `X DLSS OUTPUT`. The selected
render upscale controls actual framebuffer/compositor/SDK output dimensions.
SDK quality still selects the lower-resolution input; DLAA uses the chosen
output extent. SSAA's separately selected effective scale is unchanged.
Window size/DPI cannot silently change Render Upscale.

`D:/SFE-validation/dlss-selected-scale-matrix-oct1/results.json` passes 32
real-SDK cases: both variants, all four modes, Original/EX and selected 1x/2x
at 32:9. Output is exactly 800x224 / 1600x448, with 1,024 SDK evaluations and
retained actual reconstructed previews. Eight explicit 6x cases at 16:9 also
pass (Quality/DLAA, both variants/experiences; 256 evaluations). Cartridge-backed
simulation tests exercise independent scale/mode settings; four CPU suites pass.

The matched 1x 32:9 full-cycle image check covers all eight SDK selections and
the native reference. Text/HUD error is zero; settled model variation is zero;
actual models differ from native, excluding a silent native bypass. Reports and
representative captures remain in `dlss-selected-scale-held-preview-oct1` on D:;
193,573,260 bytes of redundant generated frames were removed and can be regenerated.
Candidate OFF/ON/OFF passes 32 evaluations. K/M/K passes 96 evaluations plus
24 retained frames, including the installed release-folder check in
`dlss-selected-scale-installed-model-driver-oct1.log`.

Preferences remain `E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
The previous 857B00B9 executable is recoverable at
`D:/SFE-validation/dlss-scale-previous-release-857B00B9-oct1.exe`.
`build/current`, publications and device installations are unchanged.
The upload/raster experiments remain off by default; no global FPS fix is claimed.

An additional manual 6x 32:9 case exposed the existing 4,096-pixel background/
temporal-path limits: the 4800x1344 output was configured but did not evaluate.
That failed check is retained in `dlss-selected-scale-manual6-oct1` on D: and
is not represented as a passing case. Wider explicit-scale parity, native Leia
upscalers, GPU/SBS performance and physical Leia/Android/ROG acceptance remain open.

## October 1 — renderer-report rebuild

Latest local release executable:
`857B00B92775E7E3EDF50A9912BFD9B5FBD2DF3911A30D38D32B04B40B5CD84A`.
All 23 plain-menu/loading regressions pass. Actual OFF/ON/OFF passes 33 SDK
evaluations; standard K / 4.5 M / K passes 96 SDK evaluations and 24 retained
preview frames. This is not native Leia neural completion or sustained FPS
acceptance. GPU reflection status wording is clarified, and both experimental
raster paths remain off by default. Evidence, backup and unchanged settings
are recorded in [the renderer report](RENDERER-PERFORMANCE-OCT1.md).

## October 1 — installed native blur owner and ordinary DLSS regression

Installed `build/release/starfox_pc.exe` SHA-256:
`53656336D945E1F084DE2A70B58CDB3F3E69BC100A8D40113B16D511D9A1B16D`.
`D:/SFE-validation/native-live-blur-plain-menu-oct1/results.json` passes all 23
Preview-OFF cases, including heavy motion/fog/AA/upscale settings. Menu pixels
are unchanged and no scene effects execute; RENDERING and the completed preview
were inspected. Actual standard/4.5 SDK evaluation and stationary native sky/HUD
pass all four modes in both `native-live-blur-dlss-{original,ex}-driver-oct1.log`.
This is ordinary PC DLSS validation, not native Leia neural-upscaler completion.
Installed-folder OFF/ON/OFF and K/M/K pass in
`native-live-blur-release-dlss-{toggle,model}-driver-oct1.log`. Two saved-profile
startup and four ordinary/native fallback cases pass; the longest sampled
unresponsive stretch is 1,541 ms, not zero. Preferences remain E0F794E8 and
the previous executable is preserved on D:. The newest stereo-display
checkpoint records validation scope and the remaining full goal.

## October 1 — installed native fog owner and menu/DLSS regression

Current `build/release/starfox_pc.exe` SHA-256:
`C1A5A74D7B460FB47B571C16B6B2C46C40E847261672A589FD554CA9A5800994`.
This native fog-owner checkpoint supersedes the executable statuses below;
it does not establish native Leia DLSS support. Remaining scope and native
validation are in `STEREO-DISPLAY-UPGRADE.md`.

All 23 Preview-OFF cases pass in
`D:/SFE-validation/native-fog-plain-menu-oct1/results.json`. Heavy effects now
explicitly include volumetric fog, as well as 10x upscale, AA and neural modes.
They do not change menu pixels or execute scene/ray/neural/fog work; unchanged
UI is retained. The RENDERING-before-preparation and completed-preview images
were visually inspected. This is not a literal zero-CPU/all-hardware guarantee.

All four actual SDK modes of standard DLSS and DLSS 4.5 pass Original and EX in
`native-fog-dlss-{original,ex}-driver-oct1.log`. Reconstruction/correspondence is
required; checked native sky and shield HUD remain stationary and pixel-exact.
Representative captures were visually inspected. Installed-release OFF/ON/OFF
and K/M/K checks are recorded in
`native-fog-release-dlss-{toggle,model}-driver-oct1.log`.

Two saved-profile GPU startup cases pass in `native-fog-startup-oct1/results.json`,
with a longest measured unresponsive stretch of 1,425 ms, not zero. Four ordinary/
native fallback cases pass in `native-fog-frontend-driver-oct1.log`.
These local tests do not substitute for physical ROG/Android/Leia acceptance or
universal neural image-quality/performance certification. Saved preferences
remain `E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
The previous executable is recoverable on D:. `build/current`, publications and
device installations are unchanged; no commit/publication occurred.

## October 1 — installed scene/particle owner (historical menu/DLSS regression)

Current `build/release/starfox_pc.exe` SHA-256:
`7BAC87AED80BBA04E989F465B4A679C840B695444EF0C3AFE6D06A785C11C620`.
This is the tested native primary scene/particle owner plus Vulkan allocation-
lifetime/compute-sampler fixes. It does not establish native Leia DLSS support;
remaining calibrated/upscaler/platform requirements are in
`STEREO-DISPLAY-UPGRADE.md`. Earlier executable checkpoints below are historical.

All 23 Preview-OFF cases pass in
`D:/SFE-validation/native-scene-fx-plain-menu-oct1/results.json`: heavy settings
do not change the plain menu or run scene/ray/neural evaluation, and unchanged
UI is retained. Preview ON presents RENDERING before scene preparation; the
loading and completed-preview captures were visually inspected.

Actual DLSS and DLSS 4.5 evaluation/correspondence passes all four modes in
Original and EX in `native-scene-fx-dlss-{original,ex}-driver-oct1.log`.
Checked native sky and shield HUD regions stay pixel-exact and stationary;
representative captures were visually inspected. On the installed executable,
OFF/ON/OFF passes with 33 SDK evaluations and K/M/K passes with 96 evaluations
plus 24 retained previews in
`native-scene-fx-release-dlss-{toggle,model}-driver-oct1.log`.
This is not universal neural image-quality or performance certification.

Both saved-profile startup cases pass in
`native-scene-fx-startup-oct1/results.json`; the longest measured unresponsive
streak is 1,254 ms, not zero. Four ordinary/native fallback cases also pass.
These local checks do not substitute for physical ROG/Android/Leia acceptance.
Saved preferences remain
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
The previous executable is preserved on D:. `build/current`, published releases
and device installations are unchanged; no commit/publication occurred.

## October 1 — native TAA owner and current release regression

Current `build/release/starfox_pc.exe` SHA-256:
`45452E5306E5DFCD94E57FD6656BD6434DCAA14C75D00EBD7DD9AA169DB879DD`.
The native Leia TAA owner and frontend are now connected; its two-eye GPU
fixtures do not establish native DLSS support. Remaining calibrated enhancement,
upscaler, SBS and device-acceptance scope is in `STEREO-DISPLAY-UPGRADE.md`.

On this exact executable, all 23 Preview-OFF cases pass in
`D:/SFE-validation/native-taa-owner-plain-menu-oct1/results.json`. Heavy settings
leave the menu image unchanged, retained UI avoids repeated uploads, and no
game/effect/neural scene evaluation runs. The RENDERING loading capture was
visually inspected. Actual SDK OFF/ON/OFF passes with 33 evaluations in
`native-taa-owner-dlss-preview-driver-oct1.log`; standard K / 4.5 M / K passes
with 96 evaluations and 24 retained previews in
`native-taa-owner-dlss-model-driver-oct1.log`.
The installed release also passes all four modes of both models in Original
and EX in `native-taa-owner-release-dlss-stability-{original,ex}-driver-oct1.log`.
Actual reconstruction/correspondence is required; protected native sky and
shield HUD stay pixel-exact and stationary. Captures were visually inspected.
This does not certify every model pixel or every device's image quality.

Both saved-profile GPU startup scenarios pass in
`native-taa-owner-startup-oct1/results.json`, with a longest measured unresponsive
streak of 1,398 ms. This is local bounded startup validation, not a zero-stall or
physical ROG/Android/Leia acceptance claim. Four ordinary/native fallback cases
also pass. Saved preferences are unchanged at
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
The previous executable is recoverable on D:. `build/current`, published
releases and device installations are unchanged; no commit/publication occurred.
Earlier checkpoint statuses below are historical.

## October 1 — native temporal core follow-up (source/fixtures only)

Native calibrated motion/depth, temporal-history resolve and jitter-removal
presentation cores now pass D3D12/Vulkan four-format references. Native Leia
TAA frontend/owner integration remains open. This does not change the ordinary
DLSS path or constitute native DLSS support.

The verified `build/release/starfox_pc.exe` below is unchanged at
`E8A2D2A555BDD4D8DA8E2977BA06726A287F0F492F3DC6C63600D840CF3899B2`;
its plain-menu, loading-indicator, startup and actual SDK-evaluation records
still apply. New test executables use `D:/SFE-validation/native-temporal-bin`
because C: is nearly full. Preferences, current/device/published builds remain
unchanged. No new broad performance/device-acceptance claim is made.

## October 1 — rebuilt native-MSAA/menu/DLSS checkpoint

Current `build/release/starfox_pc.exe` SHA-256:
`E8A2D2A555BDD4D8DA8E2977BA06726A287F0F492F3DC6C63600D840CF3899B2`.
All 23 Preview-OFF cases pass in
`D:/SFE-validation/msaa-owner-plain-menu-oct1/results.json`: heavy settings do
not change plain menu pixels or run scene/effect/neural evaluation. Static menu
uploads are retained, and the RENDERING indicator precedes preview preparation
(capture visually inspected).

The explicitly targeted release executable passes OFF/ON/OFF with 33 actual
SDK evaluations (`msaa-owner-dlss-preview-driver-oct1.log`) and standard K / 4.5 M /
K with 96 evaluations and 24 retained previews (`msaa-owner-dlss-model-driver-oct1.log`).
Both saved-profile GPU startup cases pass local bounded responsiveness checks;
the longest measured startup stall is 1,923 ms, not zero. Four ordinary/native
fallback cases pass. This is not physical Android/ROG/Leia acceptance.

Native Leia MSAA is now wired to real 2/4/8 hardware samples with independent
ray/effect histories and sharp ink/UI; the native presenter fence-lifetime bug
is fixed. Native TAA and the remaining full-goal requirements stay open in
`STEREO-DISPLAY-UPGRADE.md`. No new broad SBS performance improvement is claimed.
Preferences remain `E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
`build/current`, published releases and device installations are unchanged.
Earlier checkpoint statuses below are historical.

## October 1 — current release menu/startup/DLSS checkpoint

Release test executable SHA-256:
`544343B024C85F0A8AD1E7C351A0782488D7711E986AA25C84536CD43AF91902`.
`D:/SFE-validation/msaa-guides-plain-menu-oct1/results.json` passes all 23
Preview-OFF cases and the separate RENDERING capture. Heavy enhancements,
render scale, SSAA and both selected DLSS models do not alter the plain menu
image or run scene/effect/neural evaluation. Unchanged menu pixels are retained
instead of uploaded again. The loading indicator was also visually checked.

The explicitly targeted release executable passes OFF/ON/OFF with 33 actual
SDK evaluations (`msaa-guides-dlss-oct1.log`) and standard K / 4.5 M / K with
96 actual evaluations plus 24 retained previews (`msaa-guides-dlss-model-oct1.log`).
Both saved-profile GPU startup/preview scenarios pass bounded responsiveness
checks in `msaa-guides-startup-oct1/results.json`; four ordinary/incompatible/
no-panel fallback cases pass in `msaa-guides-frontend-oct1.log`. This is local
NVIDIA validation, not proof of physical ROG/Android/Leia acceptance.

Full native scene and composition regressions pass on D3D12/Vulkan, including
the new resident native MSAA sample-guide/linear-resolve core. That core is not
finished Leia MSAA frontend support. The complete remaining goal scope is in
`STEREO-DISPLAY-UPGRADE.md`; no new SBS performance gain is claimed.
Preferences remain `E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
build/current, published releases and device installations are unchanged.

## October 1 — final menu/DLSS checks after raster experiment

Release test executable SHA-256:
`D864E8F4FD48CAA741D053889D8FDE5511DFDCB848FD9116E350ABB8271AA773`.
All 23 Preview-OFF/loading checks pass in
`D:/SFE-validation/occupied-isolated-plain-menu-oct1/results.json`.
Heavy settings leave the plain UI unchanged; no scene/effect/neural evaluation
runs with preview disabled. RENDERING is presented before preview preparation.
`D:/SFE-validation/occupied-isolated-dlss-driver-oct1.log` explicitly targets
build/release and passes 96 actual K/M/K evaluations and 24 retained previews.
Both GPU backends also pass 160 total exact stereo/recovery images, with four
rebuilt CPU/preparation tests passing. Preferences remain unchanged.

Occupied-tile rasterization was tested but did not show a repeatable timing win.
It is isolated in diagnostic-only shaders/resources; ordinary rendering keeps
its previous kernels/bindings and does not allocate/dispatch the experiment.
This is not a completed SBS performance fix. Detailed scope, timing hashes and
remaining native Leia/platform requirements are in `STEREO-DISPLAY-UPGRADE.md`.
build/current, published releases and device installations were not changed.

## October 1 — dedicated cleanup and explicitly identified DLSS test binary

Current release test executable SHA-256:
`41AE20FF01D5242080F3461AC9F0910C5149B147B523EBDA516AE4FB2EDC436E`.
The small native upscale-checkerboard cleanup matches CPU/general-shader
oracles for all 1x–10x factors. D3D12/Vulkan stereo/recovery captures remain
exact; paired timing shows only small, workload-dependent gains. Full scope and
results are in `STEREO-DISPLAY-UPGRADE.md`; broader SBS performance stays open.

`D:/SFE-validation/dedither-plain-menu-oct1/results.json` passes all 23 plain-menu
and RENDERING cases on this hash. The explicitly targeted release executable
passes 96 actual standard K / 4.5 M / standard K SDK evaluations and 24 retained
previews in `D:/SFE-validation/dedither-dlss-release-driver-oct1.log`.
The test helper now prints the absolute tested executable/hash and rejects a
binary changed mid-run. The first invocation defaulted to old build/current
and failed its model-toggle assertions; it is excluded from release evidence.
Four CPU/preparation regressions pass, source/header freshness and whitespace
checks pass, and saved preferences remain
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
build/current/releases are unchanged; all remaining full-goal requirements stay
active. No files were deleted or device build installed.

## October 1 — sparse stereo painter and completed frontend regressions

Current release test executable SHA-256:
`F2E3C1F3FB37A9485E60192AE848097E0FAAECF2F582FDF8936A00925CC530A1`.
The compatible SBS model painter skips full-image copies, retaining untouched
pixels and their normal/depth/ownership guides. A producer-selection guard and
independent CPU-eye regression protect transitions back to moving models.
Both D3D12/Vulkan full frontend matrices pass 70 exact image comparisons each;
measured speed gains are modest and broader SBS performance remains open.
Implementation, final logs and paired timing are in `STEREO-DISPLAY-UPGRADE.md`.

`D:/SFE-validation/inplace-plain-menu-oct1/results.json` passes all 23 plain-menu
cases on this hash: no scene/effect/neural evaluation with Preview OFF, unchanged
pixels under heavy settings, retained static UI, and RENDERING before preview.
`D:/SFE-validation/inplace-dlss-driver-oct1.log` passes 96 actual SDK evaluations
across standard K / 4.5 M / standard K, history resets and 24 retained previews.
These are real neural evaluations, not a bypass to make the menu appear stable.
The four final CPU/preparation regressions pass. Preferences remain SHA-256
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
build/current, published releases and the remaining full-goal scope are unchanged.
No cleanup was performed; new diagnostic outputs are on D:.

## October 1 — native SSAA and completed plain-menu retry

Current release test executable SHA-256:
`4C3F9F8910C0471A8FC53BA19675B5C1783AB2E4FE87A3DF9AB20DAE18AFB995`.
Native Leia now supports real SSAA at 4/9/16 samples per output pixel, with
independent calibrated high-resolution eyes, linear-light resolve and sharp
native HUD/emissive coverage. Native translucent menu panels dim the finished
enhanced scene, rather than restoring an unprocessed background. Component,
Original/EX D3D12/NVIDIA Vulkan and Intel non-ray Vulkan owner suites pass;
scope, resource bounds and remaining parity requirements are recorded in
`STEREO-DISPLAY-UPGRADE.md`. Physical composition is still mocked.

`D:/SFE-validation/plain-menu-native-ssaa-retry-oct1/results.json` passes all
23 Preview-OFF cases, including six new heavy-SSAA cases, plus the preview's
RENDERING loading transition. No scene, enhancement or neural evaluation runs
in plain menus; heavy settings do not change their pixels. Twenty static cases
upload once/retain 79 frames; three navigation/renderer transitions upload
eight/retain 72. Device-creation latency and performance on every machine are
not guaranteed by these path/pixel checks.
`tmp/dlss-native-ssaa-driver-oct1.log` passes 96 real standard K / 4.5 M /
standard K SDK evaluations and 24 retained preview frames. All four ordinary
Leia fallback cases pass in `tmp/leia-native-ssaa-driver-oct1.log`.
Both final SBS runs in
`D:/SFE-validation/sbs-native-ssaa-{d3d12,vulkan}-oct1/results.json` pass 15
byte-identical presentation/fallback comparisons each. No new SBS speed gain
is claimed by this parity check.
Preferences remain SHA-256
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.

The first plain-menu attempt stopped with SDL/DIB allocation failure while C:
was full and virtual memory low. Shader freshness also encountered MemoryError
during that pressure; its checker now decodes the large header once, with a
bounded stamp-prefix split, and the completed freshness check passes. The
interrupted frontend attempt is not a pass. Disk/memory recovered externally;
no files were deleted or old artifacts moved. New frontend captures are on D:.
build/current and published releases remain unchanged. The complete goal remains
active, including native TAA/MSAA/scene/particle/volumetric/motion/analytic-ground
and secondary-hit parity, native upscaler/render-scale audit, broader SBS speed,
and physical Leia/reconnect/Android saved-GPU relaunch/ROG startup acceptance.
ADB currently reports no attached Android device.

## September 30 — native spatial AA and final ordinary-menu regressions

Current release test executable SHA-256:
`892ADA7FAC4CB11292BCDD55534218149F4C8F191DD08B3AB74751C15E6E6419`.
Native Leia FXAA/Sharp Edge/Soft Edge/SMAA now use raster-owned ink protection
and actual independent eyes, after appearance/global/bloom and before history.
Native SSAA/TAA/MSAA remain explicitly unavailable, not approximated. Four-format
component/order, Original/EX D3D12 and NVIDIA Vulkan owner suites, Intel non-ray
Vulkan EX and CPU/preparation regressions pass; exact scope/evidence is recorded
in `STEREO-DISPLAY-UPGRADE.md`. Physical Leia composition is still mocked.

`tmp/plain-menu-native-aa-final-sep30/results.json` passes all 17 cases on this
executable: Preview OFF has no scene/neural/effect work and heavy settings do not
change its displayed pixels. Static menus upload once and retain 79 frames;
navigation uploads only eight changed frames. Preview ON presents RENDERING
before the actual scene, then retains it. These are path/pixel regressions, not
a no-slowdown guarantee for every device or GPU creation transition.
`tmp/dlss-native-aa-final-driver-sep30.log` passes standard K / 4.5 M / standard K
with 96 actual SDK evaluations and 24 retained preview frames, not a DLSS bypass.
All four native-OFF/incompatible/D3D12-no-panel/Vulkan-no-panel fallbacks exit
cleanly in `tmp/leia-native-aa-final-driver-sep30.log`.
Both final SBS runs in `tmp/sbs-native-aa-final-{d3d12,vulkan}-sep30/results.json`
pass 15 byte-identical image comparisons each, including banked ground,
production motion and mono recovery after a left-eye failure. This verifies
presentation parity, not a new SBS performance improvement.
Saved preferences retain SHA-256
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.

Disk space recovered before validation resumed; no files were deleted. ADB
currently reports no attached Android device. The complete goal stays active:
remaining native AA/scene/particle/volumetric/motion and analytic ground/liquid/
secondary-hit parity, broader SBS performance, physical Leia/reconnect and
Android saved-GPU relaunch / ROG startup acceptance. The renderer picker remains
implemented. build/current and published releases are unchanged; earlier
checkpoint executable hashes are historical.

## September 30 — responsive GPU preparation and first AS query

Current release executable SHA-256:
`4604D4D8E43911AAA58505D4A6F5D4EE08C3F824FC9E838C6A768F3A9D814DD2`.
Windows resource compilation, DXR capability preparation and first BLAS/TLAS
size queries now use joined preparation workers while the owner pumps events
without consuming queued input. Recording/submission/presentation stay on their
owner thread. Cached shaders and warmed AS-count/layout queries retain their
existing frame path. An atomic completion latch avoids an owner-thread timed
future wait. Borrowed create-info/device lifetimes and compiler errors are
preserved, including exceptions.

A late, non-invasive thread snapshot caught Intel's driver inside
`GetRaytracingAccelerationStructurePrebuildInfo`; earlier shader-only changes
did not close the stall. The completed same-executable comparison uses native
file handles for logs, avoiding redirected-pipe backpressure, and no debugger.
`tmp/gpu-startup-prebuild-{responsive,blocking}-sep30/results.json` records:

| Saved-profile scenario | Blocking maximum streak | Responsive maximum streak |
| --- | ---: | ---: |
| Plain menu | 573 ms | 625 ms |
| Saved preview | 2,021 ms | 548 ms |
| Standard-DLSS preview | 3,272 ms | 613 ms |
| Native preview | 3,352 ms | 596 ms |

These are bounded 500-ms WM_NULL probes on this PC's low-power Intel D3D12
adapter, not physical ROG acceptance or frame-rate gains. Initial SDL renderer
creation remains synchronous. Plain menus compile zero game shaders, upload
once and retain 63 frames. Three captures are byte-identical; the native-preview
pair differs at eight model pixels (at most three RGB codes), not UI/coverage.
All profiles complete and preserve saved settings.

Two preparation unit tests cover frequent owner-thread pumps, joined results,
exceptions, nested/reentrant scopes, retained user events and SDL error transfer.
`tmp/calibrated-scene-responsive-{d3d12,vulkan}-sep30.log` passes the full
four-format rendering/oracle suites with preparation enabled: 532/64 joined
jobs and 2,524/64 pumps respectively. This includes actual GPU rays, materials,
terrain and AO/DOF; XR composition is still mocked, not panel certification.
The 17 ordinary menu cases, RENDERING-before-preview, 96 actual standard/4.5/
standard SDK evaluations, four Leia fallback cases, and both 15-comparison SBS
regressions pass in `tmp/*responsive*sep30` artifacts. Shader freshness and
whitespace checks pass. Saved preferences retain SHA-256
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.

build/current and published releases are unchanged. The full goal remains
active: remaining native AA/scene/particle/volumetric/motion and liquid/metal
ground/secondary-hit parity, broader SBS performance, physical Leia/reconnect,
Android saved-GPU relaunch and ROG startup acceptance are still required. The
renderer picker is implemented; this checkpoint does not replace that remaining
scope with a startup-only objective. Earlier checkpoints below are historical.

## September 30 — native AO/DOF rebuild and ordinary frontend regression

Current release executable SHA-256:
`B1826AED2587424C33525DA4DDED8565794EB4CEB7531E3CF424B80926BE2576`.
Calibrated Leia eyes now honor Ambient Occlusion and Depth of Field. Real
D3D12/Windows Vulkan component and Original/EX frame-owner checks pass;
geometry/colour tolerances, protected-ink and lifetime checks, and the remaining
native/physical-device gaps are in `ALL-EFFECTS-TRACKING.md` and
`STEREO-DISPLAY-UPGRADE.md`. This is not a physical Leia acceptance claim.

All 17 ordinary menu cases pass in
`tmp/plain-menu-depth-verified-sep30/results.json`. Preview-OFF heavy selections
preserve baseline display pixels and encode no scene/effect/neural work. Static
cases upload once and retain 79 frames; navigation/renderer changes upload eight
changed menus and retain 72. Preview ON presents RENDERING before scene setup.
Standard/4.5/standard switching passes 96 actual SDK evaluations and 24 retained
preview frames in `tmp/dlss-depth-verified-sep30-driver.log`. The four native
OFF/incompatible/D3D12-no-panel/Vulkan-no-panel frontend fallbacks pass in
`tmp/leia-frontend-depth-verified-sep30-driver.log`.

`tmp/sbs-depth-{d3d12,vulkan}-verified-sep30/results.json` each passes 15 exact
direct-versus-independent snapshot comparisons: banked ground, production
motion blur and mono recovery after left-eye rejection. This verifies parity,
not a new measured SBS performance improvement. Seven CPU regressions pass in
`tmp/calibrated-depth-cpu-final-sep30.log`; calibrated shader/include freshness
and whitespace checks also pass.

Four 64-frame Intel startup/exit profiles complete with live bounded WM_NULL
probes in `tmp/gpu-startup-depth-verified-sep30/results.json`. They record
13/60/79/73 responsive samples; the plain case uploads once and retains 63.
The maximum observed unresponsive streak is 1,641 ms plain and 2,975–3,760 ms
during preview startup. Intel performs zero unavailable NVIDIA SDK evaluations.
These measurements do NOT establish stall-free startup or ROG-device acceptance;
the initial GPU/preview preparation still needs responsiveness work. They are
not a controlled cross-build performance benchmark.

Saved preferences retain SHA-256
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
build/current and published releases are unchanged. The full goal remains
active: native AA/scene/particle/volumetric/motion and liquid/metal ground/
secondary-hit parity, broader SBS performance, physical Leia/reconnect and
Android GPU-relaunch / ROG startup device acceptance are still required.
Earlier checkpoint lists below are historical.

## September 30 — native camera response and temporal-pause rebuild

Current release executable SHA-256:
`DD3E1CF07A5850C7390874D9343EDB5686FEF2F3374FE6236122D8B1CEA5B252`.
Native camera response now moves world/models/shadows/reflections together,
without moving runtime eye poses or HUD. Flat/software/SBS/native paths share
event observation, with a separate camera-event reset epoch so AA/DLSS pause
resets do not discard an active impulse. Native real-GPU/mocked-XR tests and
remaining parity scope are documented in `ALL-EFFECTS-TRACKING.md` and
`STEREO-DISPLAY-UPGRADE.md`.

`tmp/camera-observer-final-oct1/results.json` passes 300 gameplay observations
across software, D3D12, Vulkan/TAA, DLSS 4.5 and SBS: actual player recoil,
exactly retained nonzero paused pose, second-shot detection after resume and
world/HUD composition. The first one-frame input fixture missed the cartridge
fire sampling window; the successful fixture holds each input for three frames.

All 17 menu cases pass in `tmp/plain-menu-camera-pause-final-oct1/results.json`.
Preview-OFF heavy settings preserve baseline pixels and encode no scene/effect/
neural work. Static cases upload once and retain 79 frames; changed menus upload
only changed artwork. Preview ON presents RENDERING before scene preparation.
`tmp/dlss-camera-pause-final-oct1-driver.log` passes standard/4.5/standard
switching with 96 actual SDK evaluations and 24 retained preview frames.
Four native OFF/incompatible/D3D12-no-panel/Vulkan-no-panel fallbacks pass in
`tmp/leia-frontend-camera-pause-final-oct1-driver.log`.
`tmp/sbs-camera-final-{d3d12,vulkan}-oct1/results.json` each passes 15 exact
direct-versus-independent snapshot image comparisons: banked ground, production
motion blur and mono recovery after left-eye rejection. This is parity testing,
not a new measured performance improvement. Six CPU regressions and calibrated
shader/include freshness checks also pass (`tmp/leia-camera-cpu-final-oct1.log`).

Four 64-frame Intel low-power startup/exit profiles pass with actual bounded
WM_NULL window probes in `tmp/gpu-startup-camera-pause-final-oct1/results.json`.
They record 20/78/81/84 responsive samples. The plain case uploads once and
retains 63 frames. Maximum observed unresponsive streak is 792 ms plain and
2,767–3,309 ms during enhanced-preview startup. This is NOT stall-free startup
or ROG-device acceptance. Intel correctly runs no unavailable NVIDIA evaluations.
An earlier diagnostic attempt (`tmp/gpu-startup-camera-final-oct1`) was incomplete:
a finite child exit raced its cleanup and masked the original diagnostic error.
Cleanup now tolerates that missing-PID race without masking other failures;
only the complete later run above is accepted as evidence.

Saved preferences retain SHA-256
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
build/current and published releases are unchanged. The full goal remains active.

Full-goal gates at this checkpoint:

- Effects: desktop families have implementation checkpoints; native AA/depth/
  scene and liquid/metal/secondary-hit parity remain incomplete.
- Leia: D3D12 and Windows Vulkan device/eye/lifetime paths pass real GPU fixtures,
  but physical panel/reconnect acceptance is still required.
- DLSS availability: embedded standard/4.5 discovery and actual evaluation pass
  on the local NVIDIA GPU; general device acceptance is not implied.
- SBS performance: direct eye targets/shared inputs/combined passes are in place;
  prior measured improvements are modest, not a completed lag-free performance fix.
- Android GPU relaunch / ROG boot: recovery safeguards and local CPU/startup
  checks exist; the reported device cases are not yet certified fixed.
- Renderer picker: present under Options; live navigation/Software-to-GPU checks
  pass. Backend selection does not bypass hardware/SDK prerequisites.

## September 30 — native adaptive exposure rebuild

Release SHA-256:
`74958E6DF7460B7FCC6A625FE3A294F124B2E7A8FF35DDDC827CA5736375B6D7`.
Calibrated Leia eyes now honor the existing adaptive exposure control; the
implementation and actual GPU/mocked-XR validation scope are documented in
`ALL-EFFECTS-TRACKING.md` and `STEREO-DISPLAY-UPGRADE.md`. Ordinary DLSS/SBS
algorithms are unchanged by this addition.

All 17 ordinary menu regression cases pass in
`tmp/plain-menu-exposure-final-oct1/results.json`: Preview OFF heavy selections
preserve baseline pixels and encode no scene/effect/neural work. Static cases
upload once and retain 79 frames; navigation and renderer-switch cases upload
only their changed menu frames. Preview ON presents RENDERING before preparing
the scene. Standard/4.5/standard switching passes 96 actual SDK evaluations and
24 retained preview frames in `tmp/dlss-exposure-final-oct1-driver.log`.

Four native OFF/incompatible-backend/D3D12-no-panel/Vulkan-no-panel frontend
fallback cases pass in `tmp/leia-frontend-exposure-final-oct1-driver.log`.
Four 64-frame Intel low-power startup/exit profiles pass with bounded window
responsiveness probes in `tmp/gpu-startup-exposure-final-oct1-driver.log`:
the static plain profile uses one upload/63 retained frames; preview profiles
render normally without unavailable NVIDIA neural evaluation.

Saved preferences keep SHA-256
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
build/current and published releases are unchanged. These local checks do not
certify Android GPU relaunch, ROG Ally startup or physical Leia acceptance;
native enhancement parity and broader SBS/device performance remain required.
The full goal remains active.

## September 30 — native Vulkan presenter rebuild

Release SHA-256:
`38C54A41E4B3E63B676AC0D643EF5389613E5A9335B4DF1BDD9FC170AAFA1A15`.
Native Vulkan Leia now connects runtime-created SDL devices to calibrated
native eye swapchains and the desktop/recovery owner. Real GPU session,
transport, game effects and lifetime checks are in `STEREO-DISPLAY-UPGRADE.md`.
The ordinary DLSS/SBS algorithms are unchanged. This is not physical Leia,
Android relaunch or ROG-device acceptance; the full goal remains active.

All 17 plain-menu cases pass in `tmp/plain-menu-vulkan-presenter-final-oct1`:
heavy settings leave Preview-OFF pixels unchanged, encode no scene/effect/neural
work, upload once and retain 79 frames; Preview ON presents RENDERING first.
Standard/4.5/standard preview switching passes 96 actual SDK evaluations and
24 retained preview frames in `tmp/dlss-vulkan-presenter-final-oct1-driver.log`.
Native OFF, explicit incompatible backend, D3D12 no-panel and Vulkan no-panel
fallback pass in `tmp/leia-frontend-vulkan-presenter-final-oct1-driver.log`.
Saved preferences retain their prior hash. build/current and published releases
are unchanged. These tests do not claim stall-free startup on every device.

## September 30 — Vulkan creation/lifetime rebuild

Release SHA-256:
`25B1D818E30B118A3B99E1492D485403C0C73AD81BDF76E14322AA62FE3BE6ED`.
The optional native Vulkan2 creation component and its limits are documented in
`STEREO-DISPLAY-UPGRADE.md`; the user-facing Leia presenter remains D3D12.

All 17 Preview-OFF menu cases pass in
`tmp/plain-menu-vulkan-creation-final-oct1`: heavy enhancement/DLSS selections
leave pixels unchanged, use no scene/effect/neural processing, and retain 79
frames after one upload. Preview ON still presents RENDERING before preparation.
Standard/4.5/standard preview switching passes 96 SDK evaluations and 24 retained
frames in `tmp/dlss-vulkan-creation-final-oct1-driver.log`. Native OFF,
explicit-incompatible and no-panel frontend fallback pass in
`tmp/leia-frontend-vulkan-creation-final-oct1-driver.log`. Saved preferences are
unchanged. build/current and published releases are unchanged; these checks do
not certify physical Leia, Android relaunch, ROG startup, or zero startup stalls.

## September 30 — native appearance rebuild and window responsiveness

Release SHA-256:
`0D58AB2EF0535844B20883EE9A6D37B415E5B2AB7C0996D53151EF47883BA2B9`.
Native Leia contrast/channel separation is documented in
`STEREO-DISPLAY-UPGRADE.md`; ordinary SBS/DLSS algorithms are unchanged.

All 17 plain-menu scenarios pass in `tmp/plain-menu-native-appearance-oct1`:
heavy settings preserve Preview-OFF pixels without scene/effect/neural work,
static menus upload once and retain 79 frames, and Preview ON presents
RENDERING before scene preparation. Standard/4.5/standard preview switching
performs 96 SDK evaluations and retains 24 frames in
`tmp/dlss-native-appearance-oct1-driver.log`. Explicitly incompatible/no-panel
Leia starts fall back to ordinary 2D and exit cleanly in
`tmp/leia-frontend-native-appearance-oct1-driver.log`. All checks preserve saved
preferences; build/current and published releases remain unchanged.

`check_gpu_startup_profile.ps1 -CheckResponsiveness` now samples the actual
process-owned SDL window using bounded, read-only WM_NULL requests instead of
equating eventual process exit with a responsive window. Eight 64-frame
startup/exit profiles pass: four on Intel Graphics and four on the default
NVIDIA adapter. Results:
`tmp/gpu-responsive-low-power-appearance-oct1/results.json` and
`tmp/gpu-responsive-appearance-oct1/results.json`. Plain setup evaluates no
neural frames and retains 63 menu frames after one upload. Selected supported
preview profiles still evaluate DLSS.

These runs include transient response timeouts: maximum sampled unresponsive
streaks are 2,973 ms on Intel and 2,359 ms on NVIDIA. None reaches the test's
five-second failure threshold. This is not stall-free startup, proof of the
ROG Ally report being fixed, or Android relaunch/physical Leia acceptance.
Those device checks and the remaining native enhancement/session-binding
requirements remain open.

## September 30 — model-pass rebuild regression

Release SHA-256:
`DA43C4D2C8E4C6DD5B9D860D982631EB32CCCD96D934AEEA2F2DA75A5E795843`.
Per-model motion and depth/world ownership are consolidated without removing
history validity, fences or independent eye projection. Component parity,
140 complete stereo/recovery comparisons and modest effect-heavy timing gains
are documented in `STEREO-DISPLAY-UPGRADE.md`; plain SBS is essentially unchanged.

All 17 plain-menu cases pass in `tmp/plain-menu-world-motion-merge-oct1`:
Preview OFF bypasses scene/effect/neural work, heavy settings preserve the same
UI pixels, static menus upload once and retain 79 frames, and Preview ON
presents RENDERING before preparing the scene. Standard/4.5/standard preview
switching evaluates 96 actual SDK frames and retains 24 in
`tmp/dlss-world-motion-merge-oct1-driver.log`. Four 64-frame Intel GPU startup/exit
cases pass with the embedded runtime in `tmp/gpu-intel-world-motion-merge-oct1`;
the plain menu uploads once and retains 63 frames with zero SDK evaluations.
Shared-input component checks also pass 96 retained frames on each backend in
`tmp/world-motion-merge-shared-{direct3d12,vulkan}-oct1.log`.
Preferences and build/current remain unchanged. Android/ROG/physical Leia
acceptance and the broader native enhancement/session-binding goal remain open.

## September 30 — shared stereo CPU-input rebuild regression

Release SHA-256:
`3F3AD9399B9C8829412DD2FB1252D03D7397BD8F845CE938712A6CF6FE98555B`.
SBS shares only its identical CPU artwork/masks/palette; per-eye scene, depth,
motion, reflections and histories remain independent. Scope, exact-image checks
and quiet timing (no meaningful median FPS gain yet) are documented in
`STEREO-DISPLAY-UPGRADE.md`.

All 17 plain-menu cases pass again in `tmp/plain-menu-sbs-shared-oct1`, including
heavy settings with Preview OFF, retained UI textures, renderer/neural switching,
and RENDERING presented before preview preparation. Standard/4.5/standard model
preview switching evaluates 96 SDK frames and retains 24 in
`tmp/dlss-sbs-shared-oct1-driver.log`. The embedded-runtime Intel startup/exit
suite passes all four saved-profile scenarios with 64 frames each in
`tmp/gpu-intel-sbs-shared-oct1`; the plain menu uploads once and retains 63 frames,
without evaluating DLSS or creating an enhancement scene. Preferences and
build/current remain unchanged. This is local regression evidence, not physical
Android/ROG/Leia certification or a completed full goal.

## September 30 — direct stereo-eye rebuild regression

Release SHA-256:
`909C24FBD36532D02B3FD3F4D642430E649558883E4CA94236128FCF5C2351AF`.
The ordinary stereo eye-copy reduction is documented in
`STEREO-DISPLAY-UPGRADE.md`; it does not promote unsafe asynchronous buffer reuse.
The same binary passes all 17 plain-menu cases under `tmp/plain-menu-sbs-direct-oct1`:
heavy enhancement settings do not alter Preview OFF pixels or invoke scene,
effect or neural rendering; unchanged menus retain the uploaded UI texture;
Preview ON presents RENDERING before preparing the scene.

`tmp/dlss-sbs-direct-oct1-driver.log` confirms 96 actual SDK evaluations and
24 retained frames across standard/4.5/standard preview switching.
`tmp/gpu-intel-sbs-direct-oct1` passes four 64-frame saved-profile startup/exit
cases with the embedded runtime, including the plain menu and three preview
profiles. This is local regression evidence, not universal crash certification.
Preferences remain unchanged and build/current remains older. Broader SBS
performance, physical Leia and Android/ROG recovery acceptance remain open.

## September 30 — native bloom rebuild regression

Release SHA-256:
`F72689B772C51EC15E31A42EF69F051E6EC41F1BDC166A947FA92FFAA739B2F9`.
The separate native Leia bloom addition passes the D3D12/Vulkan four-format
component and Original/EX real-queue owner suites; scope is documented in
`ALL-EFFECTS-TRACKING.md`. The same executable passes all 17 plain-menu checks
(`tmp/plain-menu-calibrated-bloom-sep30`), with byte-identical display captures
to the preceding release, retained native UI and the preview loading indicator.
Standard/4.5/standard preview switching still evaluates 96 SDK frames and
retains 24 (`tmp/dlss-calibrated-bloom-model-toggle-sep30`). The 64-frame saved
heavy Intel startup/exit suite passes with the embedded runtime enabled
(`tmp/gpu-intel-calibrated-bloom-sep30`). Preferences are unchanged;
`build/current` remains older. Physical Leia and broader SBS/device recovery
acceptance remain open; the complete goal is not finished.

## September 30 — retained plain menu and GPU shutdown ordering

Release SHA-256:
`5950BBDE05FA103523F7CC4165BA73298CA96174C90E28A2DF4B1180094DF1B2`.
Preview OFF retains an opaque native-sized UI texture. Exact indexed pixels,
palette RGB and dimensions invalidate it; unchanged frames do not reconvert or
upload it. Device recreation invalidates the cache. Touch controls remain live.
All 17 menu scenarios (1,360 frames) pass, with unchanged captures compared with
the preceding build, no scene/effect/ray/neural evaluation, and `RENDERING...`
presented before preview preparation. Static cases upload once and retain the
next 79 presentations. Evidence: `tmp/plain-menu-sdk-order-final-sep30`.

The saved heavy GPU profile reproduced an Intel D3D12 exit hang while destroying
ray pipeline state after the embedded SDK had shut down. Application-owned ray
and effect resources now release before SDK shutdown, both at exit and renderer
replacement, while the SDL device is still alive. Shadow-only and model-only
ray shaders are also specialized, with fluid tracing retained separately.
The 64-frame saved-profile suite now renders and exits on Intel Graphics and
NVIDIA RTX 5070 Ti: `tmp/gpu-intel-sdk-order-long-sep30` and
`tmp/gpu-nvidia-sdk-order-final-sep30`. NVIDIA evaluates both selected DLSS models;
unsupported Intel selections retain the normal GPU fallback. This fixes the
locally reproduced hang, not every historic driver crash.

Standard/4.5/standard preview switching passes 96 SDK evaluations and 24 retained
frames (`tmp/dlss-sdk-order-final-model-toggle-sep30`). Hardware DXR shadow,
reflection, water/transmission/caustics, SDL ray-resource interop and native
calibrated four-format D3D12/Vulkan tests pass, as do the five targeted CPU
regressions. Saved preferences are unchanged.
Physical Leia and Android/ROG recovery remain unverified. `build/current` is
still older; test `build/release/starfox_pc.exe`.

## September 30 — native basic-ground rebuild regression

Release SHA-256:
`8590BA7F8CD6B33AF9DD31D244B63C63524BD6B11C002D82D1F756775E8D5A89`.
The native basic-terrain addition passes all 17 plain-menu scenarios under
`tmp/plain-menu-native-ground-final-sep30`: unchanged baseline/heavy captures,
no scene/effect/ray/neural work with Preview OFF, the loading indicator before
Preview ON, and preserved preferences. Standard/4.5/standard preview switching
passes 96 neural evaluations and 24 retained frames under
`tmp/dlss-native-ground-final-model-toggle-sep30`. Native D3D12/Vulkan components,
Original/EX retained-source owner suites and five CPU regressions pass. Physical
Leia, Android/ROG recovery and the GPU startup hang remain unverified.
`build/current` is still older; use `build/release/starfox_pc.exe`.

## September 30 — native global/phosphor rebuild regression

Current `build/release/starfox_pc.exe` SHA-256:
`144AA75FD142E7DE112532E07D3B9AB912652D18CE14126A66AFC9FC0FB523CC`.
Native Leia now supports the 13 existing global quality selections and
independent LOW/MED/HIGH CRT persistence (details in `ALL-EFFECTS-TRACKING.md`).
This executable passes all 17 scenarios in
`tmp/plain-menu-native-global-sep30/results.json`: 1,360 Preview-OFF frames use
the native-sized UI-only path, baseline/heavy captures are identical, no scene/
ray/neural work runs, live AA and Software-to-GPU transitions pass, and Preview
ON presents `RENDERING...` before scene preparation. Saved preferences are
unchanged by the tests. This is not a promise of zero OS/driver delays.

Standard/4.5/standard frozen-preview switching passes 96 actual SDK evaluations
and 24 retained reconstructed frames with fresh model-switch history, under
`tmp/dlss-native-global-model-toggle-sep30/toggle.log`. The DLSS checker now also
guards saved preferences explicitly. The native additions do not replace flat
DLSS reconstruction with a bypass. Earlier GPU startup crashes, Android/ROG
device recovery and physical Leia remain unverified. `build/current` has not
been replaced; use the release executable for this checkpoint.

## September 30 — temporal native rebuild: menu/DLSS verification

The rebuilt `build/release/starfox_pc.exe`, SHA-256
`CBF4AD4A653FE9DBF0F6E9406578F704A0679B74EC7858E3105078128B159808`,
passes the 17 checks in `tmp/plain-menu-native-persistence-sep30/results.json`.
Original/EX Software, D3D12 and Vulkan baseline/heavy captures match exactly.
All 1,360 checked Preview-OFF frames use the native-sized UI-only path;
upscale, scene effects, ray rendering and neural evaluation are absent.
Live AA navigation and Software-to-GPU restoration of both DLSS models pass.
The separate loading capture shows `RENDERING...` before the rendered preview.
Saved preferences are unchanged. These local checks do not promise zero
operating-system/driver delays on every device or certify the reported GPU
startup crash. `build/current` is still the older executable.

The same native-temporal rebuild also passes standard/4.5/standard frozen
preview switching: 96 actual neural evaluations and 24 retained frames, with a
fresh history on each model switch. Evidence:
`tmp/dlss-native-persistence-model-toggle-sep30/toggle.log`. The Leia additions
do not substitute a native bypass for flat DLSS reconstruction.

## September 30 — native spatial-effects rebuild regression

Release SHA-256
`106E1AED72538E4AA8ED8A793772F38488631BBE67A8338FCB41AAAF3C92FD16`
adds native calibrated-eye spatial/animated effects (see
`ALL-EFFECTS-TRACKING.md`). The same executable passes the 17 plain-menu checks
under `tmp/plain-menu-native-warp-sep30`, with effects/upscale/rays/neural work
absent when Preview is OFF, identical baseline/heavy display captures and
unchanged saved preferences. `RENDERING...` is captured before the preview.
Standard/4.5/standard model switching also passes 96 actual neural evaluations
and 24 retained preview frames under `tmp/dlss-native-warp-model-toggle-sep30`.
These are regression checks, not proof that every GPU startup hang is fixed.
`build/current` remains unchanged; use the rebuilt release executable.

## September 30 — Preview OFF is a plain host menu

With Preview OFF, setup/options use a native-sized UI-only blit. Saved upscale,
AA, filters, materials, lighting, reflections and global enhancements do not
render a scene or affect the menu. No DLSS viewport is evaluated or allocated.
DXR capability labels no longer create rendering queues or shader pipelines.
Native Leia's plain menu likewise contains only host UI, with scene effects and
rays disabled; this is not a new physical-panel validation.

Turning Preview ON presents `RENDERING...` before cartridge preroll and scene
preparation. The preroll pumps window events, and the normal preview replaces
the indicator when its first rendered frame is ready. Existing preferences are
preserved. FSR1/neural capability metadata remains available without rendering
the preview.

Software -> GPU now restores the selected DLSS model/quality before creating
the GPU renderer, avoiding an unnecessary Vulkan device followed immediately
by a second SDK/device recreation for D3D12. Unavailable DLSS selections show
their actual prerequisite (`GPU REQUIRED`, `DX12 REQUIRED`, etc.), rather than
the same generic label for every cause. Interrupted-GPU Software recovery is
still enabled; the earlier reported GPU hang is not certified fixed.

Release SHA-256
`43F56A15833A1EB7AC3DF12BE3936CB8D006AFDDF623E1737B89236D9C2A33B4`
passes 17 plain-menu scenarios (1,360 frames): Original/EX, Software/D3D12/
Vulkan, baseline/heavy 10x settings, both neural models, live AA navigation,
and Software -> GPU transitions. Baseline/heavy display captures match exactly,
and no scene/effect/DXR rendering or neural evaluation executes. Loading and
the following genuine preview are captured separately. Preferences are
unchanged. Results are in `tmp/plain-menu-ready-sep30/results.json`; rerun with
`tools/check_plain_menu.ps1` and a new output directory.

The same binary passes evaluated gameplay GPU/Software/GPU cycles with exactly
one D3D12 creation per return to GPU, under
`tmp/dlss-direct-gpu-recovery-ready-sep30`. DXR hardware rendering/reflection
regressions, unsupported-DXR, runtime-input, simulation and DisplayXR tests pass.
These local checks do not certify arbitrary gameplay settings or every device.
`build/current` and saved preferences were not replaced.

## September 30 — actual reconstructed setup preview (current)

DLSS and DLSS 4.5 now process the frozen setup/options preview with the selected
SDK model and quality, rather than bypassing reconstruction. It accumulates
32 jittered samples, then retains that actual neural output and its last raster
phase. This keeps a settled preview from repeatedly changing its model edges.
The retained image is invalidated by scene/settings, quality/model, resolution,
device and menu-context changes. Gameplay never uses this frozen-image policy;
resuming it starts fresh temporal history and evaluates each presented frame.
The upscale row now says `X DLSS OUTPUT`, not `X NATIVE PREVIEW`.

Native menu text, HUD, portraits and backdrop artwork remain outside neural
reconstruction. Changing a DLSS mode is consequently expected to affect model
edges, not soften the text or redraw the sky. This is anti-aliasing/upscaling,
not a material or palette effect; a good native reference can look similar.

Rechecked after the September 30 native drawn/palette-parity rebuild:
release SHA-256
`904FBA8306411C4BA212472909F81A093BA5A871211BE1BCA6E0276EADCFFFDC`
passes the paced actual-window test for all eight modes. The 27,504 tested glyph
pixels and shield sample remain exact, 1,755–3,061 model-region pixels differ
from OFF, and the settled model has zero variation across 32 consecutive frames.
The report and first/last comparisons are under
`tmp/dlss-native-drawn-parity-final-sep30`; redundant intermediate diagnostic
BMPs were deleted after verification (774,181,260 bytes), leaving logs/results.
Saved settings and `build/current` remain unchanged.

The same executable also passes live frozen-preview switching: standard → 4.5
→ standard reconstructs 32 new samples after each switch (96 evaluations and
24 retained frames), and OFF → ON → OFF restores native presentation. Reports
are in `tmp/dlss-native-drawn-parity-model-toggle-sep30` and
`tmp/dlss-native-drawn-parity-preview-toggle-sep30`. The eleven captured-image
negative-control tests and runtime-input tests pass too.

The paced 1600x448 actual-window comparison passes all eight SDK model/quality
combinations. All 27,504 checked menu glyph pixels and the sampled shield HUD
are exact. Settled model pixels have zero consecutive-frame variation, while
1,755–3,061 model-region pixels per mode differ from OFF by more than one code
value. Each run records 32 SDK evaluations followed by 64 retained frames,
so the difference is not simulated by a native bypass. Eleven image-checker
tests include both bypass and moving-held-image negative controls. The runtime
input suite checks partial accumulation, reset/epoch boundaries and continued
gameplay sampling.

The 2560x1600 fullscreen drawable passes the same eight combinations: all
69,315 checked glyph pixels and the shield sample are exact, settled model
variation is zero, and 2,231–4,478 model pixels per mode differ from OFF.
`tmp/dlss-reconstructed-held-fullscreen-verified-sep30/stability.json` records
the executable SHA-256
`A14CB33B51ED76A0C8923C62941B8A0702A63EB48C6A562E86F6FB235AE50CAD`.
The K/M/K frozen-menu switch verifies three new 32-sample reconstructions
and 24 retained frames. OFF/ON/OFF also verifies a reset when SDL's new
drawable dimensions settle. Real F1 sequences in Original and EX each record
20 gameplay, 70 reconstructed/retained preview and 30 resumed gameplay frames,
with a reset on resume. Performance mode of both SDK models also preserves
17,228 white/gray/yellow options glyph pixels exactly with the heavier ray,
bloom, exposure, scene, particle and camera enhancements enabled.

One initial fullscreen diagnostic run exited with D3D12 allocation error
`0x8007000E` at presentation frame 69. An identical fresh launch and the
complete eight-mode rerun succeeded. Its failure log is retained under
`tmp/dlss-reconstructed-held-fullscreen-sep30/k-mode1`; this is not evidence
that general allocation failures have been fixed. Saved settings and
`build/current` are unchanged; use the rebuilt release executable.

Use `tools/check_dlss_display_cycle.ps1 -Menu Main -PlainScene -CaptureDrawable
-Paced` to compare settled reconstruction with matched time-dependent effects
disabled. `-Fullscreen` exercises the final letterboxed drawable. Other preview
enhancements are still available; a changing post-effect need not yield a
byte-identical final image. These local checks are not a blanket gameplay image
quality or performance certification.

## September 30 — native frozen setup preview (superseded)

The preceding attempt rendered the frozen setup/options preview unjittered at its full output
resolution, including models. It does not reduce that scene to a DLSS input
grid or evaluate neural history. The selected standard/4.5 mode remains
enabled for gameplay; preview entry/exit changes the temporal context and
resets gameplay history rather than carrying a frozen model into it. Other
preview enhancements still run. The upscale row explicitly says
`X NATIVE PREVIEW` while this workspace is shown.

That attempt superseded the reconstructed-preview approach below, whose residual
edge variation was still visible. The main-menu drawable check at 1600x448
passes all eight quality/model combinations: native text/HUD and model-region
pixels match exactly across 32 consecutive frames, with zero model variation.
`tools/check_dlss_display_cycle.ps1 -Menu Main -NativeModelCheck -CaptureDrawable`
reproduces it; time-dependent effects are disabled only in this strict
reference fixture. Nine checker tests include a stationary blurred-model
negative control, which frame-to-frame stability alone could not reject.
The result is `tmp/dlss-native-preview-main-sep30/stability.json`.

The actual 2560x1600 fullscreen drawable also passes all eight combinations
with zero checked model/text variation. All eight modes still evaluate every
frame in the separate 24-frame gameplay checks. Real F1 open/resume/reopen
sequences pass in Original (standard Quality) and EX (4.5 DLAA): 70 native
preview frames, 20 preceding and 30 resumed SDK frames, with the first resumed
frame resetting history. `tools/check_runtime_options.ps1 -Binary
build/release/starfox_pc.exe -Dlss 1` tests that boundary; use `-Experience EX
-Dlss45 4` for the other case. K/M/K reconfiguration still passes. The release
EXE SHA-256 is `9DC5B0F5CE21E8C51B00A86FE7A4DC783F5BB080E6BAB86D1913D7CD23BC2117`.
These are local RTX checks, not a general gameplay motion-quality or FPS
certification. Saved settings and `build/current` remain unchanged.

## September 30 — final drawable verification

`tools/check_dlss_display_cycle.ps1 -Menu Main -CaptureDrawable -Paced`
now checks the actual window output, not only the internal canvas. Its optional
`-Fullscreen` check includes final sampling, fractional viewport edges and
letterboxing. Readback occurs only in the explicit diagnostic fixture.
The checker requires matching logged drawable sizes/viewports; eight unit
checks include missing evidence, invalid bounds and blurred letterboxed text.
Windowed tests of both models at all four qualities preserve all 27,568 sampled
glyph pixels and HUD pixels exactly. Neural model-edge variation remains
measurable, so this does not establish an artifact-free reconstructed preview.
The full 2560x1600 fullscreen run also passes all eight combinations, including
69,465 checked glyph pixels per frame across 32 consecutive frames per case.
Its report is `tmp/dlss-fullscreen-verified-sep30/stability.json`, tied to EXE
SHA-256 `5DAD92E98FDDCB07A72FA0E7A878AF927B76C0D77D056FBE78787C4A917D92AB`.
Physical ship-region mean change is 0.037–0.127 on the 0–255 scale; occasional
individual edge differences reach 59. The initial fullscreen fixture read
only SDL's clipped logical viewport and is discarded, not acceptance evidence.
Saved preferences and `build/current` are unchanged.

## September 30 — live preview poses and final native menu composition

The user's report that every quality wobbles and blurs models/text exposed two
additional gaps in the earlier verification. Unpaced captures held a repeatable
interpolation fraction, and sampling every fourth frame skipped 24 of the 32
jitter phases. Those checks did not establish live preview stability.

Menu input continues ticking while the captured simulation is frozen. Its video
phase counter previously cycled between the last two source poses, despite
supplying DLSS with explicitly frozen zero-motion guides. A preview now always
uses the completed source pose (`logic_interpolation_alpha == 1`). Ordinary
gameplay interpolation is unchanged. The stationary audit also checks raw
model motion *before* the zero-motion guide conversion, so that override can no
longer hide a moving model input.

Consecutive-frame capture found another actual failure in standard Performance:
at frame 91, world exposure changed 53,024 ordinary-label pixels to a different
ink. Setup/touch UI had been painted before exposure, trails, phosphor and camera
effects, without ownership tags for the new overlay. These overlays now render
after world history/camera processing, matching the software path. Grey, yellow
and white text retain their native colours and positions, and do not enter the
world's exposure/afterimage history. The underlying scene still runs DLSS;
subpixel sampling and both independent K/M models remain enabled.

The rebuilt `build/release/starfox_pc.exe` passes the actual 3D-options screen
in all eight model/quality combinations with paced 240 FPS requested. Each
case completes 96 SDK evaluations and captures every frame from 65 through 96.
All 68,976 checked glyph pixels per sample and the sampled shield artwork are
byte-exact, not merely within a rounding tolerance. Raw model motion is below
0.001 input pixel. Separate EX paced/Balanced-4.5 captures also preserve text
and HUD exactly. The completed-pose regression passes the cartridge-backed
simulation tests; the host-UI/world-effects fixture and the full effects suite
pass on D3D12 and Vulkan.

This is still not zero neural variation. The display-sized Original ship region
has roughly 0.04–0.12 mean physical-pixel RGB change (0–255 scale); the maximum
99th percentile is 3, with occasional individual edge-pixel differences up to
59. At the deliberately smaller 800x448 EX output, variation is larger. The
checker reports physical-pixel changes alongside downsampled averages instead
of hiding these outliers. No blanket motion-quality or performance acceptance
is implied.

Evidence: `tmp/dlss-adjacent-cycle-sep30/regression.json` retains the original
failure, `tmp/dlss-adjacent-cycle-fixed-sep30` verifies corrected UI ordering,
and `tmp/dlss-live-preview-fixed-sep30/stability.json` records the latest paced
build. EX comparisons are `tmp/dlss-ex-live-native-sep30` and
`tmp/dlss-ex-live45-sep30`.
`tools/check_dlss_display_cycle.ps1 -Paced -PresentationFps 240` reproduces the
full cycle with bounded capture windows; redundant generated BMPs are removed
only after successful analysis, keeping logs, reports and first/last samples.
Saved preferences and `build/current` remain unchanged.

The current release executable was rechecked after the user's all-qualities
report: SHA-256
`A4FADE1A675454486350D336D0FB61CCBD31934CA2C25CDF767453D39E3D481C`.
All eight combinations pass both the 3D submenu at a requested 240 FPS and
the main preview at a requested 120 FPS, capturing 32 consecutive late frames
per case. Every checked text/HUD pixel is exact (68,976 submenu glyph pixels;
110,388 main-menu glyph pixels). The capture script now accepts `-Menu Main`
to check the complete main-menu labels and values, not only submenu labels.
Evidence is `tmp/dlss-current-release-recheck-sep30/stability.json` and
`tmp/dlss-main-release-recheck-sep30/stability.json`. Model-edge variation remains
measurable; these checks do not certify zero neural shimmer or achieved FPS.
The artwork checker, simulation tests and cartridge-backed frozen-pose
regression also pass. Saved preferences and `build/current` are unchanged.

## September 30 — display-sized reconstruction and frozen-preview motion

The user still observed blurry text/models and continuous preview wobble in
every mode after the September 29 change. The earlier sky-only checks did not
establish model stability or proper final presentation resolution.

DLSS previously reconstructed the logical canvas times the saved render
upscale, then SDL magnified that result to the window. At 1x/32:9 this meant
an 800x224 output (Quality input 533x149), even on a much larger screen.
DLSS output now follows the actual drawable size, preserving the selected
aspect. The saved render upscale is a minimum. Automatic selection is bounded
to 6x and 4096 pixels per axis; explicit higher requests retain their existing
limits. Integer Scaling keeps its existing fixed-raster behavior. The upscale
row reports the effective `X DLSS OUTPUT` scale when this path is active.
Native GPU presentation also tracks that scale so a window resize cannot
feed back into an ever-growing output size. DLSS OFF and SBS are unchanged.

Only the explicitly frozen setup preview supplies zero physical motion for
every input sample; previously unknown stationary sky/sprite samples were
marked invalid. Reset frames remain invalid, ordinary gameplay retains real
per-pixel motion, and subpixel projection jitter remains enabled. Lighting
and reflection ownership/normals are resolved from the original SDK sample
grid onto the neural output rather than nearest-enlarging jittered metadata.
Native artwork protection no longer treats stale earlier-model coverage as
visible model ownership, so overlaid text is not classified as a neural edge.

Local verification uses the user's 32:9, Original/1x, enhanced-sky, high bloom
and exposure, all scene/particle enhancements, hardware rays/reflections and
camera-response settings. Both models reconstruct all 64 frames in each of
Quality/Balanced/Performance/DLAA at 3200x896. In 64 sampled images, all 29,748
tested setup-menu glyph pixels retain exact coverage, with at most one RGB
code-value backend rounding difference; shield artwork is byte-exact.
`tools/check_dlss_display_preview.py` checks these independently of model
pixels. Matched whole-image change, normalized back to 800x224, falls from
0.214 to 0.041 for standard Quality and from 0.272 to 0.045 for 4.5 Quality.
Small reconstructed model variation remains; this is not a claim of zero
neural artifacts, general performance acceptance or completed world inputs.

Evidence: `tmp/dlss-display-preview-native`, `tmp/dlss-display-preview-k-quality`
and `tmp/dlss-display-preview-{k,m}-mode{1,2,3,4}`. A real fullscreen launch with
automatic selection, not a fixed test override, chooses 3200x896 output and
2133x597 Quality input; its text/HUD checks also pass. Hidden automatic-window
selection chooses 1600x448 instead, confirming the actual drawable affects
the plan. GPU guide/metadata/artwork fixtures pass on D3D12 and Vulkan,
including unequal source/output extents and frozen/reset motion. Runtime input,
startup, exit and embedded-runtime smoke tests pass; all three changed shader
payloads pass freshness checks. The rebuilt EXE is `build/release/starfox_pc.exe`;
saved settings, `build/current`, embedded runtime bytes and publication are
unchanged.

The actual 3D-options submenu has also been exercised, using controller menu
navigation with both models at all four qualities. Each mode reconstructs all
96 frames. Across eight late samples per mode, 53,024 unchanged label pixels
retain exact glyph coverage (maximum one RGB code-value rounding difference),
and sampled shield artwork remains byte-exact. The intentionally different
quality and effective-output-scale values are excluded, not compared as
unchanged labels. The display-preview checker now accepts repeatable label
regions to cover the full submenu. Evidence: `tmp/dlss-submenu-*-sep30`.
Model variation is measured separately and remains small but nonzero.

Follow-up to the user's report that every quality blurs models and text:
repeated the full 3D-options capture on the latest desktop EXE, with all
768 neural evaluations succeeding. The artwork checker now tests all repeated
white, gray and yellow inks rather than just the most common white ink.
Across eight samples per mode, all 68,976 tested glyph pixels preserve their
exact coverage (maximum one RGB code-value rounding difference); shield
artwork is byte-exact. Five negative-control tests reject blur in each ink,
gray-only movement and mismatched extents, and are registered with CTest
(explicitly skipped where Pillow/NumPy are absent). Standard/4.5/standard
history resets and OFF/ON/OFF native-renderer restoration also pass.
Mean ship-region variation is approximately 0.031–0.119 on the 0–255 scale;
the largest individual pair is 0.234. This is not zero reconstructed-model
variation or a claim that every visual artifact has been eliminated.
Results/logs and first/last comparison samples are retained in
`tmp/dlss-user-allqualities-sep30`; redundant generated frames were removed.
The retest EXE SHA-256 is
`E22ABE7763A8BE5F639A19F3CE24DE524FDA2CBDF73CF19489CB8B29C2F3D352`.
Saved preferences and `build/current` remain unchanged.

## September 29 — preview artwork reference correction

This supersedes the earlier blur-correction claim below: protecting artwork
was insufficient because the reference compositor still sampled DLSS's
reduced, jittered late/background layers. Restoring those pixels retained
softness and frame-to-frame movement, even in a completely frozen preview.

DLSS now keeps separate full-resolution, unjittered artwork producers and
reduced/jittered SDK-input producers. The final sky, HUD and world sprites
use genuinely native artwork; models and generated terrain remain neural
output. Camera-response world underlays use the same corrected reference,
without importing HUD into the world. A narrow optional model-edge guard
avoids clipping reconstructed sky silhouettes with a single jittered mask.
The normal disabled-DLSS path is unchanged, and there is no second model
render or production motion readback. Standard K and 4.5 M remain separate.

`tools/check_dlss_preview.ps1` now checks a frozen preview against DLSS OFF,
requires actual SDK evaluation with valid stationary model/terrain motion,
and tests both exact native sky pixels and zero frame-to-frame sky movement.
All eight Quality/Balanced/Performance/DLAA combinations pass in Original at
1x and EX at 2x with camera-response effects, using D3D12 on the local RTX
5070 Ti Laptop. These captures enable enhanced sky; all eight combinations
also pass Original at 4x with the original backdrop. Sampled shield-HUD
pixels match the native reference exactly in all 24 combinations.
EX's sky-only measurement excludes
the tall left-side pillar; model pixels are not mislabeled as sky artwork.
Evidence: `tmp/dlss-preview-final-1x`, `tmp/dlss-preview-final-ex-2x`
and `tmp/dlss-preview-final-original-4x`.
The complete GPU temporal fixtures, including exact HUD/world-sprite
protection and guarded model edges, pass on D3D12 and Vulkan. Five serial
input/stereo/startup/exit/embedded-runtime smoke tests pass; generated HUD
shader freshness passes as well.
Both models also reconstruct all 60 frames of moving Original gameplay at
2x with enhanced ground/sky, hardware DXR shadows/reflections and camera
response enabled. K/M/K switching resets then reuses history correctly;
OFF/ON/OFF returns to ordinary Vulkan presentation without reconstruction.

This corrected the measured sky/HUD region, but the subsequent user report
and September 30 investigation showed further presentation/model defects.
Residual reconstructed model/terrain variation remained:
the matched frozen Original Quality sequence falls from about 1.20 to 0.15
mean whole-image 8-bit channel change, while the measured sky goes from
1.34 to exactly zero. This is neither a blanket visual-quality certification
nor a performance claim. The SDK jitter sign follows NVIDIA's sample;
disabling jitter would hide the input defect at the expense of reconstruction.
The rebuilt executable is `build/release/starfox_pc.exe`. `build/current`,
user-installed add-ons/settings, runtime package bytes and publication stay
unchanged.

## September 29 — separate DLSS models and blur correction

DLSS and DLSS 4.5 now have separate adjacent rows in 3D OPTIONS. Both offer
OFF/Quality/Balanced/Performance/DLAA, default OFF, and select alternate models
of the same official embedded runtime: preset K for standard DLSS, preset M
for DLSS 4.5. Enabling either disables the other. `DLSS_MODE` remains the
standard preference; new `DLSS45_MODE` records the explicit second-generation
selection. Ambiguous files enabling both are rejected without partially loading
settings. The private configure-v2 ABI selects K/M; configure-v1 stays standard.
Settings revision 14 accepts older files, defaulting the new fields to AUTO/OFF.
The preceding embedded-only implementation inadvertently replaced standard
DLSS with M. That behavior is superseded by these separate controls.

Blur correction: camera-response effects formerly borrowed raw neural output,
bypassing native-artwork protection. Their world underlay now restores native
tilemaps and world OAM sprites, while leaving HUD out of the world layer.
Final composition protects native HUD as well. Models and generated terrain
remain temporally reconstructed. Their subpixel sampling is enabled again;
the previously disabled jitter could only supply repeated undersampled pixels.
Native art is restored unjittered rather than fabricating its camera motion.
No extra sharpening or second neural pass is used to conceal bad inputs.

Verification: both private-adapter models build with the packaged NVIDIA
runtime; all eight mode/model combinations evaluate 32 frames on RTX 5070 Ti
Laptop/D3D12 in `tmp/dlss-separated-jitter-sep29`. OFF preserves ordinary D3D12
and Vulkan presentation. The GPU fixture verifies byte-exact native artwork
and independent HUD inclusion/exclusion on both backends, alongside the full
TAA/SMAA/MSAA/motion fixtures. Original cartridge menu tests verify mutual
exclusion, separate row navigation and held-button behavior; config round trips,
invalid-file rejection and startup/exit smoke tests pass. Camera-response
Quality captures in `tmp/dlss-separated-camera-sep29` have been visually
inspected against native rendering. These are local execution and regression
checks, not broad hardware/performance acceptance or a claim that every
temporal-world input is complete.

Final live checks: `tmp/dlss-separated-model-toggle-final-sep29` evaluates all
24 K/M/K frames, resets on each model change and subsequently reuses history;
`tmp/dlss-separated-off-toggle-final-sep29` returns to unwrapped Vulkan after
OFF/ON/OFF. The revised menu was captured and inspected in
`tmp/dlss-separated-menu-sep29`. Original asteroid/cockpit and EX enhanced-cloud
camera-response captures also evaluate successfully. Versioned-cache repair
and unexpected-DLL refusal pass for the final embedded package. Six serial
config/stereo/Original-cartridge/startup/exit tests pass. The tested EXE is
`build/release/starfox_pc.exe`; `build/current`, user add-ons and release/CI
publication remain unchanged.

The Options menu also exposes GPU/Software and platform-supported GPU backend
selection. Interrupted Android/Windows GPU sessions retain a recovery journal
through GPU teardown instead of clearing it early; a subsequent launch selects
Software without deleting assets or requiring reinstall. Guard/config unit
tests pass. This is a recovery safeguard; the reported Android relaunch and
ROG Ally freeze still need device-specific reproduction and validation.
`tmp/renderer-picker-live-final-sep29` confirms actual Vulkan/D3D12/D3D11
device selection through menu input and GPU/Software switching. An initial
D3D12 selection was overridden by SDL's prior AUTO/Vulkan hint; the picker
now refreshes both the hint and device property while retaining environment
override priority. Diagnostic menu changes do not overwrite saved preferences.

## September 29 — embedded DLSS 4.5

The Windows x64 executable can embed the five-file production runtime plus
all notices and its integrity manifest. Enable this source-build option with
`-DSTARFOX_DLSS_RUNTIME_DIRECTORY=<verified package>/dlss`. Configure verifies
hashes, x64 architecture, the exact DLL set, NVIDIA signatures and notices;
the Windows x64 workflow stages that package before rebuilding the final EXE.
No experimental neural-rendering DLL, ReShade proxy or add-on is embedded.

Windows still requires DLL files to load them. On launch the EXE restores its
exact embedded bytes into `%LOCALAPPDATA%/StarFoxEnhanced/dlss/<package-id>`.
The cache is versioned, checks every file before loading, repairs changed or
missing files atomically, refuses substituted directories/files and unexpected
DLLs, and keeps ordinary rendering available if optional initialization fails.
No installation-folder DLLs or separate downloads are required. Redistribution
notices also install into `licenses/dlss`; the SDK licensing review described
below remains separate from technical verification.

The adapter explicitly requests DLSS 4.5's second-generation transformer,
preset M, in Quality/Balanced/Performance/DLAA. NVIDIA supports M in all these
modes, although its recommended performance/quality balance uses M for
Performance and older K for Quality/Balanced/DLAA. Using M in the latter modes
can increase GPU cost, particularly on RTX 20/30. DLSS remains optional and OFF
by default. This change adds neither Frame Generation nor Ray Reconstruction.

Sources: [NVIDIA's DLSS 4.5 announcement](https://www.nvidia.com/en-eu/geforce/news/dlss-4-5-dynamic-multi-frame-gen-6x-2nd-gen-transformer-super-res/),
[Streamline DLSS guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.1/docs/ProgrammingGuideDLSS.md).

This does not close the existing world-input, visual-quality, older-GPU or
release-approval gaps. The hardware check is `tools/check_embedded_dlss.ps1`;
it requires an NVIDIA RTX/D3D12 device, asserts preset M and frame evaluations
without an adjacent `dlss` directory, and checks that OFF does not wrap native
D3D12/Vulkan presentation.

Local verification: `build/release/starfox_pc.exe` embeds Streamline 2.14.1's
unmodified DLSS 310.9.1 runtime and the rebuilt static-CRT adapter. On the RTX
5070 Ti Laptop, all four modes evaluate all 16 frames, with explicit preset-M
requests; DLSS OFF leaves D3D12/Vulkan unwrapped. OFF→DLAA→OFF passes with eight
evaluated frames and returns to normal Vulkan presentation. A changed cached
notice is restored byte-for-byte; an unexpected uppercase `.DLL` is rejected
before SDK loading while ordinary GPU play succeeds. Five targeted input,
stereo and startup/exit smoke tests pass when run serially (two smoke tests hit
their 10-second timeout during an earlier concurrent profiling run). The Quality
capture was visually inspected; it retains native HUD/artwork. Existing
world-input and broader visual/performance acceptance limits remain open.
Evidence directories: `tmp/dlss45-embedded-sep29`, `tmp/dlss45-toggle-sep29`,
`tmp/dlss45-cache-sep29`. No release/CI run was published or launched, and
`build/current` plus its user-installed add-ons/settings were left unchanged.

September 19 menu prerequisite update deployed locally: copied the desktop
executable tested by the 120-frame high-resolution neural/control comparisons
into `build/current` while no current game was running; hashes match. Previous
executable retained at `tmp/pc-before-neural-status-sep19/starfox_pc.exe`.
An enabled startup preference now reads ENABLE DLSS when ordinary DLSS is off,
or UNAVAILABLE for unsupported renderer/stereo/capability. Restart and save-error
labels remain intact. This is prerequisite reporting, NOT live evaluation or
photorealism confirmation. No settings, add-on DLLs or VR artifacts changed.

September 19 user-session correction: archived the normal installation's latest
log and settings in `tmp/dlss5-user-session-sep19`. The 18:19 session has
NeuralUplift=1 and DLSS_MODE=1; the log confirms signed NR initialization,
feature 18 creation and successful inline evaluations at counts 1 and 60.
Its output is 720x448 with 480x299 guides. The user reports no realistic visual
transformation despite this. Therefore disabled DLSS does NOT explain this
session, and successful execution does NOT establish visual acceptance.
Do not present the experimental integration as completed photorealism.
Controlled follow-up at 4x render scale / DLSS Quality / 120 frames:
`tmp/dlss5-high-resolution-sep19/ORIGINAL-on.bmp` (neural ON) versus
`tmp/dlss5-high-resolution-control-sep19/ORIGINAL-on.bmp` (neural OFF).
Both retain ordinary DLSS; only the isolated add-on startup preference changes.
ON logs confirm feature-18 success at 1600x896 with 1067x597 guides. Both
captures were visually inspected: ON changes ground/building shading and softens
some edges, but retains the low-poly scene without a photorealistic makeover.
Thus higher resolution alone does not resolve the user's complaint. This is
evidence of visible processing, not realism acceptance. The isolated installation
was left neural OFF; normal user settings were not modified.
Additional supported-control investigation: the installed add-on's own strings
expose three presets, Natural/Cinematic style and intensity controls. Baseline
runtime logs already report intensity=1 and global_tone=1. Tested preset=2,
style=1 (third preset / Cinematic) in the isolated installation; logs confirm
those settings and successful evaluations. Visually inspected
`tmp/dlss5-cinematic-preset3-sep19/ORIGINAL-on.bmp`: altered tone and subtle
surface detail, still not the requested realistic transformation. Restored the
isolated configuration to OFF with preset/style overrides removed. No unsupported
intensity values, patched runtime, or normal-install changes were introduced.
Repeated feature creation also appears in this session; whether that is normal
scene-transition behavior or unnecessary recreation remains to be investigated.

September 19 local current-install follow-up: `build/current` now contains the
same tested ReShade proxy, RenoDX V4.7 add-on and NVIDIA neural DLL as
`tmp/dlss5-gameplay-sep19`. Previously the normal build deliberately lacked those
files, which hid its conditional DLSS5 menu row. Add-on/neural DLL hashes match
the retained author downloads; NVIDIA's neural DLL signature is valid. The
three files were added only where absent, without replacing user settings or
copying the isolated installation's gameplay configuration.

Initialized NeuralUplift OFF. Fresh installed-runtime Original checks detect
the add-on and validate regular DLSS Quality while neural processing stays OFF:
`tmp/dlss5-current-installed-off-sep19`. A subsequent ON test was not started
because a user game opened; the configurator correctly refused the running
installation. A later live launch reports enabled=ON and its config contains
NeuralUplift=1; that user state was preserved. Startup ON alone is not proof
of neural evaluation. Existing isolated positive feature-18 evidence remains
documented below. Local installation is not release redistribution clearance.

September 19 package gate: `tools/package_dlss.ps1` now invokes the read-only
`tools/verify_dlss_package.ps1` before reporting success (therefore the existing
Windows packaging workflow also checks before creating its archive). It verifies
the exact five-DLL manifest, sizes/hashes, x64 PE/DLL headers, NVIDIA Authenticode
signatures and nonempty required notices, rejecting unexpected DLLs. The current
local package passes. `tools/check_dlss_package.ps1` also passes disposable-copy
negative tests for hash corruption, duplicate manifest entries, unexpected DLLs,
and x86 machine headers with an otherwise matching hash. No DLL is loaded by
these checks. The normal local runtime folder was repackaged with updated notices.

Release review remains OPEN, not satisfied by these technical checks. The pinned
SDK's `bin/x64/nvngx_dlss.license.txt` requires protective distribution terms,
and its supplement describes NVIDIA attribution/mark placement and approval.
Official reference: https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt .
The application's GPLv3 xBRZ component also requires compatibility review; do
not assume dynamic loading alone resolves that question. No NVIDIA approval,
end-user licensing review, or RenoDX/neural-DLL redistribution permission is
established by the evidence collected here. Do not describe the package as
legally cleared or publish the experimental DLLs on that assumption.

September 19 PC menu integration: compatible already-loaded RenoDX installations
with an explicit NeuralUplift setting expose `DLSS5 (EXP.)` beneath ordinary
DLSS in 3D OPTIONS. Absent add-ons leave the normal menu/VR navigation unchanged.
The row changes the add-on's own persisted startup preference and displays
`ON - RESTART` / `OFF - RESTART` when it differs from this launch. It does not
claim live switching or automatically load unsigned code. Host preference and
capability survive experience reconstruction and save-state restoration without
becoming emulated state. Ordinary DLSS remains separately controlled; successful
neural reconstruction still requires a compatible D3D12/DLSS path.

Verified in the isolated installation: `tmp/neural-menu-on-sep19` saves ON from
an OFF launch; `tmp/neural-menu-restarted-sep19` reads ON and confirms neural
feature 18 evaluation, then saves OFF; `tmp/neural-menu-disabled-sep19` reads
OFF and confirms no feature 18 evaluation while ordinary DLSS still evaluates.
The latter run explicitly asserts the disabled state and absence of neural
evaluation. Pixel/menu availability tests and the Original simulation substrate
tests (including press-versus-hold behavior) pass. Redistribution/legal review,
broader hardware compatibility and visual/performance acceptance remain open;
this is an experimental optional integration, not a production release sign-off.
Actual button navigation also selects/toggles the new row in a 4:3 capture:
`tmp/neural-menu-visual-sep19/menu.bmp` (visually inspected: all rows fit, selected
ON - RESTART is readable). The isolated installation is left OFF afterward.

September 19 startup-control implementation: `tools/configure_dlss5.ps1`
configures NeuralUplift in an explicitly selected, already-installed experimental
installation. Defaults OFF; refuses the matching running game; checks companion
files, preserves other sections/keys, rejects duplicate sections/keys, and keeps
the original ini backup. It never loads or downloads add-ons. This is a startup
configuration utility, not the still-pending in-game menu integration or a claim
of supported hardware. `tools/check_dlss5_configuration.ps1` passes ON/OFF round
trip, default OFF, unrelated-setting preservation, backup preservation, missing
section/key insertion, idempotence and ambiguous-input rejection with inert
temporary fixtures. No normal installation or running game was changed.

September 19 live-control result: used the public ReShade global configuration
ABI documented in https://github.com/crosire/reshade/blob/main/include/reshade.hpp
through an already-loaded proxy (no additional DLL loading). Diagnostic-only
calls set NeuralUplift=1 at presentation frame 8 and 0 at frame 24; getter
readback confirms both values. V4.7's active state stays OFF and no feature-18
evaluation occurs. Ordinary DLSS evaluates all 40 frames successfully. Evidence:
`tmp/dlss5-live-control-sep19`. Therefore writing live configuration is NOT a
working neural toggle for this build. Use explicit restart-required semantics
unless a supported live interface is established. Diagnostic is opt-in through
`-TestNeuralControl`; production does not write ReShade settings. Isolated
configuration remains OFF after the test.

September 19 DLSS5 control investigation: inspected installed V4.7 exports;
only NAME and DESCRIPTION are exported, not a supported direct toggle API.
Binary configuration strings identify `[RenoDX.DLSS5] NeuralUplift`. Setting
`NeuralUplift=0` in only the isolated installation's ReShade.ini was verified
on fresh launch: add-on logs report enabled=OFF, no neural evaluation succeeds,
while regular DLSS still evaluates 24 Original frames successfully. Evidence:
`tmp/dlss5-disabled-proof-sep19`. Earlier enabled runs provide the positive case.
The isolated configuration is intentionally left OFF. This establishes a
startup configuration control, NOT a verified live-reload interface. A menu
integration must either verify live control or explicitly require restart;
do not claim a working live toggle by writing an ini value alone.

September 19 history/DLSS5 follow-up: added GPU fixtures for unchanged static
texture with advancing animation counter, changed texture, previous near-plane
clipping and previous non-billboard type. D3D12/Vulkan pass; rejected history
does not change current colour/depth. Inspected the older controlled DLSS5/plain
EX comparison: visible changes are modest, not evidence of a realism overhaul.
Updated only the authorized isolated add-on installation with the current exe
and static-CRT runtime. Original/EX each pass 32 Quality frames and the archived
add-on logs confirm neural feature 18 evaluation in `tmp/dlss5-current-proof-sep19`.
Inspected current EX output. This proves execution, not product integration:
no user-facing DLSS5 toggle, redistribution approval or broad quality/performance
acceptance yet. The normal installation still contains no unsigned add-on.

September 19 sprite correspondence: added per-pixel motion for whole-object
camera-facing sprites, using the source's rounded/truncated screen rectangles
and normalized texture position rather than rigid polygon transforms. Texture
changes reject correspondence; unchanged texture selection is valid even when
the raw animation counter advances. Transparent texels remain invalid. The
GPU translation-plus-depth/size-change fixture passes D3D12 and Vulkan at a
fractional raster size. Original/EX Quality space runs each evaluate 32 frames
and match normal GPU queue ordering against forced serialization exactly in
`tmp/dlss-sprite-motion-sep19`. Native artwork preservation and default no-jitter
behavior are unchanged. Other sprite-face types/particles and full visual
acceptance remain; this does not establish complete DLSS5 integration.

September 19 packaged runtime verification: rebuilt the native adapter with
MSVC static CRT; `dumpbin /dependents` lists only WINTRUST.dll and KERNEL32.dll.
Repackaged the adapter plus four signature-verified NVIDIA production DLLs into
`build/current/dlss`. Original/EX installed-runtime Quality runs each evaluate
24 frames successfully without explicit adapter/binary/backend environment
paths (`tmp/dlss-static-runtime-sep19`). Packaging now writes a five-DLL SHA-256,
size and version manifest with no machine paths; all five installed hashes
verified. No release uploaded; licensing/publication review remains separate,
and the unsigned DLSS5 add-on is not included.

September 19 expanded scene checks: Original/EX each evaluate all 48 frames in
Training (Quality, 60 FPS) and asteroid space (Performance, 240 FPS), with
installed runtime discovery. Captures/logs: `tmp/dlss-training-quality-sep19`
and `tmp/dlss-space-240-sep19`; inspected the latter EX final image. These are
evaluation/lifecycle checks, not performance benchmarks or motion-vector proof.
Fixed shared one-plane GPU buffer allocation to include compute-write usage:
polygon generation can reuse the buffer previously uploaded by a sprite.
The direct sprite test now requests depth before the same model instance draws
a one-face polygon. D3D12/Vulkan targeted depth tests both pass after rebuild.

September 19 sprite-depth follow-up: Original asteroid scenes had only
whole-object sprites, which supplied no temporal depth and skipped DLSS despite
configuration. Added constant-Z GPU depth for visible sprite texels (transparent
texels remain unknown); kept lighting metadata disabled. Independent raster
remapping explicitly supports these screen-aligned planes, while general
plane restrictions remain. Corrected raster receiver gating to use flag bit 0,
not the packed plane index. Depth/colour/ownership tests pass D3D12 and Vulkan;
Original/EX Quality 32-frame space runs now both evaluate all frames in
`tmp/dlss-space-depth-verified-sep19`. Sprite object-motion vectors still need
implementation; this is not full temporal acceptance.

The first new test incorrectly read an absent buffer and exposed an SDL assert
dialog to the user. The process is gone, test readback now rejects absent
buffers before SDL, and corrected runs pass. No user game was terminated.
The preceding configured-but-unused SDK viewport cleanup crash is fixed by
tracking successful evaluation separately from options configuration. Its
Original no-evaluation regression passed before sprite depth was enabled;
EX evaluated-space regression also passed. Current regular executable rebuilt.
User reports the prior jitter issue appears fixed; preserve the current
native-artwork restoration/no-jitter behavior while completing other inputs.

## Additional queued reports — September 19

User supplied Discord screenshot `C:/Users/kando/AppData/Local/Temp/codex-clipboard-581bae5c-618b-474f-b7ea-64adc066ddf3.png`
(reports dated September 16; build/platform not established). Investigate after
the current DLSS work; these reports are not yet reproduced or fixed:

- Sector Z: tan/peach stepped artifact at the bottom-left of the gameplay view.
- Corridors and Atomic Base: review incomplete widescreen background extensions.
  Preserve the user's earlier constraint against duplicated tunnel elements;
  do not crop gameplay to the original viewport.
- Sector Z victory: last speaking pilot's portrait, static-box texture and
  dialogue remain visible until the map loads instead of clearing.
- Victory presentation: investigate the unexpected top-only letterbox band;
  compare with source behavior before deciding how to correct framing.

September 19 lifecycle follow-up: switching GPU -> software -> GPU previously
closed the SDK permanently. The host now retains its verified runtime location,
reopens after old-device destruction and before replacement-device creation,
and binds each replacement renderer. Empty environment overrides are treated
as unset. New `check_dlss_lifecycle.ps1 -RendererCycle` checks fresh SDK startup,
swapchain upgrade, initial history reset and evaluation in every GPU segment.
Original/EX installed-runtime runs each pass four renderer switches over 40
frames (24 evaluated GPU frames, 16 software frames), evidence in
`tmp/dlss-renderer-cycle-verified-sep19`. Initial harness assumed exactly one
swapchain per renderer; SDL resize legitimately creates additional swapchains,
so assertions now validate each restart segment instead of that false count.
This is lifecycle coverage, not proof of complete DLSS motion quality.

September 19 next pass: preserve native non-terrain background artwork after
DLSS, alongside the existing HUD restoration. These are screen-space tilemaps,
not surfaces with the pinhole-camera motion assumed by reconstruction. Explicit
terrain bit 27 remains eligible, as do model/textured geometry and world sprites.
The restoration remains GPU-resident and precedes normal presentation effects.
Focused D3D12/Vulkan tests verify ownership, opaque black and alias rejection.
Installed-runtime Original/EX 32-frame gameplay checks pass in
`tmp/dlss-artwork-stability-sep19`; inspected EX frame 22. The matched EX upper
region mean frame difference is now 0.183 versus 0.687 before this change and
0.211 with DLSS off. This supports improvement for the tested background, not
complete temporal acceptance. Jitter remains temporarily disabled; model-edge,
moving terrain and wider-scene acceptance are still outstanding.

September 19 follow-up: laser shape headers are explicitly excluded from
PC lighting receiver metadata and ray caster collection (Original and EX).
Emissive GPU draws also clear receiver metadata underneath opaque beam pixels,
without removing temporal depth or changing palette colour. Focused GPU depth
tests cover that ownership and pass on D3D12 and Vulkan.

DLSS wobble is NOT resolved. Default subpixel jitter is temporarily disabled
pending reliable screen-space background correspondence; diagnostic jitter
remains available. Fresh installed-runtime Original/EX 32-frame runs pass in
`tmp/dlss-beams-stability-sep19`. EX upper-region frame-change measurement is
0.687 with this mitigation versus 2.809 with centered jitter, and 0.211 with
DLSS off. These are matched-scene difference measurements, not a general
quality score. Reconstruction remains enabled but loses jitter-based sampling
benefits. Do not label DLSS motion release-ready yet.

PRIORITY REGRESSION: user reports DLSS extremely wobbly/shaky and unplayable.
Do not equate successful SDK evaluation or still images with acceptable motion.
Pause DLSS5 expansion and investigate live temporal stability, especially the
reported 1x render-upscale configuration. Added configurable test render scale
and an explicit zero-jitter diagnostic override to separate sampling from
motion/depth errors. NVIDIA's official sample applies positive pixel-offset
projection translation and sends the same offset to Streamline, matching our
model sign convention; no unsupported sign flip applied. No fix yet claimed.

Code audit found an additional concrete failure: rejected temporal frames
could present raw jitter, and transition readback could reuse jittered native
pixels. Added projection-consistency preflight before resizing/jitter and
unjittered recorded-scene replay whenever jittered reconstruction fails or
transition composition falls back. Building; not yet proven to resolve all
reported shaking. Added sequence capture and explicit no-jitter comparison
options to the targeted harness. DLSS5 work stays secondary to this regression.

1x Performance reproduction completed for Original/EX with per-frame captures
in `tmp/dlss-motion-1x-sep19` and `tmp/dlss-motion-1x-nojitter-sep19`.
All frames evaluated, so fallback alone does not explain instability.
New `tools/check_dlss_sequence.py` measures matched frame-pair background
change, not overall visual quality. EX frames 16–32: mean absolute sky change
0.211 with DLSS off, 2.746 with jittered DLSS, 0.687 without jitter (0–255
channel units). This reproduces excessive temporal variation and implicates
background sampling/correspondence; disabling jitter reduces but does not
resolve it and is not being substituted as the final fix. Input is 200x112
for 400x224 output at this setting. Consecutive jittered EX frames inspected.

Sampling audit: jittered 2D producers used pixel-edge inverse sampling and
edge-based scatter bounds, while model/depth reconstruction uses pixel
centers. Updated shared fixed-point helpers with explicit centered sampling
and enabled it for jittered backgrounds, raster/text, span producers and CPU
composition. Non-jitter legacy lookup stays unchanged. Regenerated all six
affected portable shaders successfully. CPU expectation fixtures and combined
motion validation are next; not claiming the visual regression resolved yet.

Latest integration evidence (September 19): menu-selected Quality completed
32 frames each for Original and EX, initial-only reset, matching queued and
serialized captures: `tmp/dlss-menu-batch-fixed-sep19`. Captures inspected.
The user's UNAVAILABLE report exposed a real deployment gap: Windows still
defaulted to Vulkan. Installed SDK now selects D3D12 at initial device creation
(explicit backend overrides remain honored). Added an installed-runtime test
that removes all runtime paths and backend overrides. Current build compiled.

Windows x64 packaging now downloads checksum-pinned official SDK 2.14.1,
builds the native adapter with static CRT, verifies NVIDIA runtime signatures,
and includes production DLLs and full notices under `dlss`. Local packaging
passed. CI itself has not run. SDK attribution/marketing and other applicable
distribution obligations still need release review; no release pushed.

Isolated third-party gameplay experiment: Original/EX SDK evaluation completed
in `tmp/dlss5-gameplay-proof-sep19`. EX's final ReShade log confirms feature 18
creation and successful inline evaluation. This is genuine game input, not
only the earlier analytical fixture. EX capture inspected, but comparison with
the normal build is confounded by different portable settings; no neural
quality/performance claim yet. Add-on logs must be preserved per process for
stronger Original evidence. Add-on redistribution and user-facing control
are not implemented. Files remain isolated in `tmp/dlss5-gameplay-sep19`.

Priority note: after DLSS/DLSS5 integration, investigate reported black-screen
startup in release 0.0.6.7. Platform/GPU/logs are not yet available. Do not
interrupt the integration to chase this report unless new evidence makes it
an integration blocker.

Additional user queue, after integration and startup investigation: fix EX god
nuke still killing the player; add a reflective complete metal/mirror model
effect; research and implement roughly ten additional 2D/3D effects. These are
queued requests, not implemented features or permission to change focus now.

Also queued: a Double Rendering Distance option. Display objects twice as
early, but keep their routines/animation static until the original spawn time.
Early visual presence must not advance gameplay routines or collision state.

Also queued: improve VR pre-game menu directional precision. Pressing Right
to adjust an option too easily also navigates Up/Down. Add deliberate axis
selection/hysteresis so horizontal adjustments do not accidentally change
rows; preserve intentional vertical navigation. Menu-only behavior, not a
change to gameplay stick precision. Investigate and verify after DLSS work.

End-of-work cleanup requested: inspect and remove obsolete builds and truly
unneeded temporary files only after integration. Preserve current/platform
toolchains, required SDKs, assets, saves, signing keys, and useful proof.

## Connected integration batch in progress (September 19)

Native resized scene sampling now maps output pixel centers directly into
the source, avoiding integer reference-canvas double rounding. Colour,
surface metadata, depth and motion share that lookup; original-size and
mosaic paths retain their established sampling. Added a native identity-grid
fixture, not yet run. Regenerated the portable compositor shader.

DLSS host fallback and preparation failure now invalidate all temporal
history consistently. Both focal axes and projection centers are validated
before evaluation; preparation/evaluation share mode parsing. Successful
shutdown clears cached device and render-plan state. The SDK swapchain
warning corresponds to the intentionally retained native swapchain reference
used to restore SDL ownership; no speculative reference-count change made.

Per user instruction, regression runs are deferred until this larger connected
batch is ready. These newest changes are not yet runtime-verified.

Capability handling now caches the actual device support result and exposes
availability/reason for subsequent menu integration. Unsupported hardware
keeps its native SDL swapchain rather than failing the optional presentation
hook, and skips DLSS preparation/evaluation. This is an integration safeguard,
not a verified explanation or fix for reported 0.0.6.7 black-screen startups.
The alignment/history batch compiled successfully; capability changes are
under compilation. No regression suite has been run for this pending batch.

Capability build passed. Added default-off persisted DLSS quality preference
(Off/Quality/Balanced/Performance/DLAA), strict read/write range validation,
and pending round-trip fixtures. PC settings snapshots preserve this value.
Menu selection and use of the saved preference by the evaluator are not yet
wired; existing diagnostic environment controls remain the active path.

Follow-up: connected the DLSS row in 3D Options, saved quality restoration,
and runtime mode changes to temporal history, native-resolution scene drawing,
jitter, terrain tagging, pre-HUD background capture and SDK evaluation.
Explicit installed adapter/runtime paths now allow capability discovery
without test switches. Missing runtime, incompatible renderer and stereo
output display UNAVAILABLE; the stored preference is preserved. Normal
installation/path discovery and complete input coverage remain unfinished.
Updated the diagnostic off-control to omit installed runtime paths.
Compilation caught a missing background callback capture; corrected it and
restarted compilation. No runtime verification of this combined batch yet.

Next connected changes: executable-local optional runtime discovery, keeping
DLSS quality when restoring save states, and a menu-selection harness mode
that removes all temporal/evaluation diagnostic enable switches. The runtime
layout is `<executable directory>/dlss/starfox_dlss_native.dll` alongside the
official SDK runtime DLLs; explicit paired `STARFOX_DLSS_ADAPTER` and
`STARFOX_DLSS_BINARIES` paths still override it. Existing signature checks
remain active for SDK binaries; no proprietary DLLs bundled or redistributed.
Missing runtime leaves normal rendering available. Full input/visual quality
and DLSS5 integration are still incomplete; this is not release acceptance.

Combined validation: Windows build, runtime settings tests, and D3D12/Vulkan
compositor checks pass, including direct native sample mapping and reduced
early/late enlargement. First menu-path gameplay run failed: clearing process
variables through .NET left an empty mode override, and SDK stderr split the
restoration log line. Corrected empty-mode handling, harness variable removal,
and single-write presentation logging. Rebuild/retry pending; no gameplay
pass claimed for this batch yet (`tmp/dlss-menu-batch-sep19`).

## Reduced layers and world-sprite ownership (September 19)

Early background and late scene layers now use the SDK render extent when
scene conversion supports it, with original-size fallback for unsupported
draws. Composition samples their actual dimensions directly, rather than
rounding through the CPU reference canvas. World billboards retain their
2D styling tag but carry a separate world-sprite marker so temporal world
composition includes them and HUD restoration does not overwrite them.
CPU gameplay HUD overlays remain on the full-resolution presentation path;
complete native HUD/reticle separation still needs acceptance verification.

Focused D3D12/Vulkan depth and composition checks passed. Live Original and
EX 32-frame Quality runs in `tmp/dlss-reduced-layers-sep19` pass native
533x299 evaluation, changing jitter phases, initial-only history reset and
identical queued/serialized captures. Both final captures visually inspected.
This proves execution and synchronization, not final temporal image quality
or a measured performance gain. SDK shutdown still emits a swap-chain
reference-count warning; investigate lifecycle ownership before release.

Remaining: full world correspondence, native HUD acceptance, capability/menu
integration, performance and visual acceptance, and DLSS5 gameplay hookup.
These remain opt-in diagnostic paths; no release pushed.

## Scene jitter batch (September 19)

Opt-in `STARFOX_TEST_DLSS_JITTER=1` with native raster evaluation now drives
a deterministic 32-phase Halton sequence through scene models, billboards,
raster commands, projected text, grid/particle/dust spans, early/late GPU
layers and CPU world composition. Actual input-pixel jitter reaches temporal
terrain reconstruction and the SDK; history poses remain unjittered. Native
and presentation draw scales are accounted for independently.

Added shared 1/256-phase integer sampling for 2D producers. Initial floating
mapping disagreed at exact boundaries; initial signed remainder also differed
on Vulkan. Both were replaced with bounded unsigned quotient/remainder
arithmetic. Zero-jitter output retains its established sampling. BG2 scatter
fills disjoint shifted intervals and clamps outer cells, without stale edges.

Windows builds. D3D12 and Vulkan projection/raster/text, background, depth/
motion and composition checks pass. Added 16 raster and 48 text fixtures plus
27 background size/phase cases, with direct/scene comparisons, and sequence/
invalid-jitter checks. Existing particle/grid tests pass; dedicated jittered
particle/grid fixtures still need strengthening. Live Original/EX 32-frame
Quality runs pass changing phases, native 533x299 inputs, initial-only history
reset and exact queued/serialized final captures in
`tmp/dlss-scene-jitter-fixed-sep19`; both final captures visually inspected.

This is still diagnostic-only. Remaining integration includes full-resolution
native HUD separation, reduced early/late layers, broader world motion/depth
coverage, capability/menu controls, visual/performance acceptance and DLSS5
gameplay hookup. No release or unrelated bug work was performed.

## Native scene/composition batch (September 19, later)

`STARFOX_TEST_DLSS_NATIVE_RASTER=1` now selects the SDK render plan before
the main ordered scene is submitted. Original and EX both evaluate native
533x299 inputs for 800x448 Quality output; this is no longer the previous
full-resolution main-scene render followed by input resampling. Preparation,
source projection dimensions, output composition dimensions and focal X/Y
are connected. CPU, early background and late overlay inputs retain their
reference coordinates. Those layers still render at their original sizes.

Added independent command-raster and whole-object billboard output, a copied
scene conversion preserving the original fallback recording, and independent
compositor output with correctly scaled motion. Legacy motion arithmetic is
preserved exactly (an initial shader reassociation changed its last bits;
the corrected shader passes exact legacy and fractional motion tests).
Wave-mode models still decline scene conversion and use the existing path.

Focused checks: D3D12/Vulkan raster/projection, compositor and depth/motion
checks pass, including fractional raster/billboard coverage, textures,
palette, offsets, mosaic, HUD writes and temporal ownership. PC builds.
Live Quality Original/EX 16-frame runs pass continuous history and identical
queued/serialized captures in `tmp/dlss-native-raster-billboards-sep19`.
Both final captures inspected. The earlier EX run intentionally failed the
new native-size assertion and exposed the billboard restriction now fixed.
No full-game regression suite run for this batch.

Still not a finished user-facing DLSS feature: full-scene jitter, complete
world correspondence, full-resolution native HUD separation, reduced early/
late layers, capability/menu integration and performance measurement remain.
DLSS5 remains isolated-test-only; this batch does not integrate that add-on
with gameplay or redistribute its binaries. No release pushed.

## Render-plan/evaluation batch (September 19)

Particle, dust and grid scene records now carry optional logical viewports;
their shared span shader maps logical cells directly into independent output
dimensions, including signed edge coordinates. CPU replay rejects resized
records before clearing its target. Projection batch builds and passes D3D12
and Vulkan, including added 149x127 dust direct/scene reference comparisons.
Existing particle/grid tests pass, but dedicated fractional particle/grid
fixtures and gameplay render-plan routing still remain. No native performance
benefit is claimed until the application actually selects this path.

Preparation now exposes SDK render dimensions separately from evaluation.
Evaluation accepts native input extents independently from final/HUD extents,
rejects mismatched plans/devices, supports independent focal X/Y scaling and
passes actual raster jitter to terrain reconstruction and SDK constants.
Input-extent changes and failed evaluations invalidate history; failed texture
allocation cannot reuse a stale configured plan. Adapter explicitly declares
unjittered motion. These APIs are not yet selected by native scene rendering.

Windows application and MSVC adapter compile. Projection tests pass, including
rounded SDK ratios. One consolidated Quality lifecycle run, Original and EX
16 frames each, passes actual SDK evaluation, continuous history and exact
queued/serialized captures in tmp/dlss-plan-batch-sep19. This still uses the
full-resolution diagnostic resample path. Full-scene jitter, native reduced
scene routing and DLSS5 gameplay integration remain incomplete.

## Independent background raster dimensions

Projected text also supports independent output sizing, preserving its original
projection and glyph sampling while dispatching only the target pixel count.
Twelve added fixtures compare 299x255 direct/scene output to the original
1x/2x/4x reference. Both D3D12 and Vulkan pass these and existing projection,
stereo text, particle, dust and grid checks. This is not yet host-enabled.

All three background layers now accept a logical viewport independently from
their output texture dimensions, including through GpuScene composition.
Nine fixtures cover 267x149, 533x299 and 800x448 output from a 400x224 canvas;
packed pixels and coverage agree with logical reference sampling and direct
versus scene rendering on D3D12 and Vulkan. Existing 432 background cases pass.
CPU replay rejects these resized records before clearing the destination;
recovery must retain the original-resolution recording.

This removes a scene integration restriction, not the remaining gameplay host,
particle/raster sizing or full-scene jitter work. DLSS is still unfinished.

## Direct model raster dimensions

GpuModel and GpuScene now accept output dimensions independent of the logical
projection viewport. Span generation allocates/emits the requested row count;
depth projection and both motion projections use independent X/Y scale factors.
Raster jitter remains in output pixels. This permits direct geometry rendering
at reduced SDK sizes instead of resizing a full-resolution model image.

D3D12/Vulkan tests render 149x127, 299x255 and 533x299 from a 224x192 logical
viewport, with/without fractional jitter. Analytical translation motion passes;
scene composition preserves exact pixel/depth/motion outputs. Existing depth,
motion, resampling and HUD tests also pass. The span tests additionally verify
1.5x raster sizing against the software renderer.

This API is not yet selected by the gameplay DLSS host. Mixed background,
particle and raster chunks still need independent sizing. Whole-object
billboards and wave effects reject custom model sizing; CPU recovery must use
the original recording. Full-scene jitter and DLSS5 gameplay remain unfinished.

## Quality/Balanced/Performance gameplay connection (after 0.0.6.7)

The diagnostic PC host now requests all three SDK super-resolution modes in
addition to DLAA, uses the SDK's optimal input dimensions, and keeps final
output/HUD at the original presentation size. Mode changes release/reconfigure
the viewport and reset history. The checker accepts `-DlssMode` and verifies
the requested mode was actually evaluated.

A GPU-only preparation pass area-filters world color to the requested size,
selects the nearest depth and its paired motion, scales motion to input-pixel
units, and preserves invalid motion sentinels. Fractional/integer ratios,
constant-color preservation, depth/motion pairing, resize/reuse and alias
rejection pass on D3D12 and Vulkan. Generated DXIL/SPIR-V/MSL freshness passes.

Important: this currently reduces an already-rendered full-resolution scene.
It enables actual SDK SR evaluation but does NOT yet provide the main native
low-resolution rendering performance benefit. Full-scene jitter, direct
lower-resolution scene rendering, remaining world correspondence and normal
menu/capability integration remain. Do not label this finished DLSS.

`tmp/dlss-quality-gameplay` passes Original/EX, 16 frames each, initial reset
only and exact queued/serialized output. Quality uses 533x299 -> 800x448.
The Original screenshot was inspected with native HUD restored. These changes
are after the 0.0.6.7 release tag and have not been published.

`tmp/dlss-balanced-gameplay` and `tmp/dlss-performance-gameplay` each pass
32 frames per Original/EX, initial reset only and identical queued/serialized
final images, with clean SDK shutdown. Balanced uses 464x260 and Performance
400x224 for 800x448 output. Existing 5120 guide and 366183 model-depth sample
checks still pass alongside the new resampling fixtures on both GPU backends.

## Direct gameplay terrain-motion verification

The opt-in terrain audit now also downloads the RG32 motion texture actually
supplied to DLSS. For each classified ground pixel it independently intersects
the camera ray with the ground in double precision, transforms the point to
the previous camera, and compares previous-minus-current pixel displacement
against the GPU result (0.02 pixel tolerance). Missing motion and mismatches
fail the diagnostic. Samples include frame 1 and every subsequent 16th frame,
not merely an initially stationary frame. This adds no normal rendering stalls.

`tmp/dlss-terrain-motion-sampled` passes 64 evaluated frames per Original/EX at
60 FPS, initial history reset only. All classified pixels in each of five
sampled frames have usable depth and matching motion, including nonzero camera
movement. This verifies the terrain-plane reprojection transport for these
scenes; it does not establish all background artwork/scroll motion or finish
full-scene jitter, SR modes, or neural gameplay integration.

`tmp/dlss-terrain-motion-sampled-240` also passes 96 frames per experience at
240 FPS, queued and serialized. Seven sampled frames in each run have zero
motion mismatches, including moving/interpolated frames; serialized final
images match exactly, only the initial history reset occurs, and SDK shutdown
is clean. The Windows executable includes these default-off diagnostic checks.

## Terrain ownership survives intermediate GPU merges

Fixed scene merge and fused raster shaders dropping terrain bit 27. Ownership
now follows visible background color through these passes and is cleared by
covering models or opaque HUD pixels. Regenerated DXIL, SPIR-V and MSL assets.
New raw-metadata fixtures exercise both fused and separate scene merge paths;
D3D12 and Vulkan background/depth suites pass, including 183997440 background
samples and 5120 temporal-guide samples.

Added opt-in `-AuditTerrain` to the gameplay checker. It downloads the actual
world ownership buffer and the depth texture supplied to DLSS on the second
evaluated frame, reporting classified versus usable terrain depth pixels.
Readback is diagnostic-only; ordinary rendering gains no fence or CPU transfer.
The checker rejects missing/empty depth evidence or evaluation failure.

Correction to earlier sections: matched profile logs and color screenshots
alone did not prove terrain reached DLSS. The intermediate metadata loss above
meant those earlier gameplay runs could not establish that claim. Direct depth
auditing is required in addition to those existing checks. This remains a
diagnostic DLAA path, not completed SR modes or a release-ready DLSS option.

Direct gameplay evidence after the fix:
- `tmp/dlss-terrain-merge-audit`: 32 frames per Original/EX and queued/serialized
  mode, initial reset only, exact serialized output match. All 143669 Original
  and 163287 EX classified ground pixels have usable depth on the audited frame.
- `tmp/dlss-terrain-merge-level1-4`: 8 frames per experience; all 156258/163219
  classified terrain pixels have usable depth.
- `tmp/dlss-terrain-merge-level1-6`: 8 frames per experience; all 166819/166838
  classified terrain pixels have usable depth.

All runs evaluated successfully and shut down cleanly. These depth counts prove
transport and usable depth for the sampled frames, not full background motion
accuracy or quality across every stage. Windows executable rebuilt; generated
scene/raster shader freshness and whitespace checks passed.

## Additional authored terrain profiles

Added 1-4.SCR and F-1.SCR definitions (ground rows 360..511), using their
complete source fingerprints 80d53f12 and 33b82640 with the same uniform
tile-relocation normalization. Source BGS.ASM selects these maps for ground
scenes; their lower tile rows contain the authored ground gradients. Changed
maps remain unknown, and first-word palette/flip bits cheaply reject unrelated
profiles before hashing.

Both assets pass original/relocated and rejection fixtures on D3D12/Vulkan.
The gameplay checker now accepts a validated `-Level` argument.
`tmp/dlss-terrain-level1-4` and `tmp/dlss-terrain-level1-6` each pass all 16
Original/EX evaluations, log matched terrain rows, and shut down without SDK
errors. Original screenshots for both were visually inspected with comms/HUD
present. These runs do not prove all other ground or EX-specific backgrounds.

## Authored ST-P terrain enabled in diagnostic gameplay

Added a source definition for ST-P.SCR's rows 360..511, the ground-gradient
rows after the sky/cloud/mountain artwork. Classification requires the complete
8192-byte tilemap fingerprint (FNV-1a 536ac185), normalized for the loader's
uniform character-index relocation, while retaining palette/priority/flip bits.
Direct VRAM comparison in `tmp/dlss-terrain-vram` found every one of 4096 words
relocated by +192 in both Original and EX. Arbitrary modified maps do not match.
Tunnel/wrong-mode/wrong-layout maps remain excluded.

The PC diagnostic path now assigns this profile to matching GPU BG2 draws,
connecting authored coverage through composition to the camera-plane converter.
`tmp/dlss-gameplay-relocated-terrain` logs rows=360:512 for both experiences and
passes 64 evaluations each, initial reset only, clean shutdown and exact
queued/serialized comparison. `ORIGINAL-on.png` was inspected with HUD intact.
The background checker accepts the authored ST-P.SCR as an optional fixture;
original/relocated matches and modified/tunnel/tile-size rejection pass on
D3D12/Vulkan. Other tilemaps remain unclassified: this does not complete all
backgrounds, full-scene jitter, SR modes or release acceptance.

## PC terrain-plane handoff

The diagnostic PC DLSS host now consumes packed terrain ownership with the
actual camera-space ground plane, current/previous pixel projections and
camera mapping. The plane is sourced independently of whether ray tracing is
enabled; source shadow availability and tunnel exclusion gate it. Changing
ground height, losing the plane or a history reset invalidates terrain motion.

`tmp/dlss-gameplay-terrain-plane-handoff` passes 64 evaluated frames each in
Original/EX, only the initial global reset, clean shutdown, no evaluation errors,
and exact queued/serialized output comparison. Authored terrain row ranges are
still empty by default, so this proves the handoff's regression behavior, not
that gameplay terrain pixels now have complete depth. Per-background source
classification remains necessary; unknown pixels have not been guessed.

## Source-row terrain ownership transport

GpuBackgroundSettings accepts optional authored BG2 terrain source-row ranges.
The GPU marks qualifying visible pixels with bit 27, following scroll/mosaic
sampling and continued ground rows; tunnels are excluded. GpuComposite retains
this background ownership only while visible: native models, CPU foreground,
late overlays and margin replacements clear it. The terrain converter can now
consume that packed ownership directly, avoiding a separate mask readback.

D3D12/Vulkan background checks cover scrolling at 1x/2x/4x, tunnel exclusion,
and an actual background->compositor test with model and opaque-black HUD
occlusion. Existing 183997440 background samples remain unchanged; D3D12's full
composition/effects suite passes. Temporal guide checks pass 5120 cases with
both packed and standalone masks on both backends. PC builds.

Ranges remain empty by default. Authored per-background ranges and their plane
association still need integration/validation before gameplay terrain guides
are enabled. This implements mask transport, not all-stage terrain completion.

## Cartridge coverage evidence

Added `-CaptureBackground` to the gameplay checker to preserve expanded,
unscrolled and complete tilemap images alongside evaluation logs. The run in
`tmp/dlss-terrain-source-layers` passes Original/EX evaluation and shutdown.
Inspected `ORIGINAL-on-bg2-tilemap.png`: Corneria's sky, mountains and ground
occupy one 512x512 BG2 tilemap (map 28672, characters 20480, scroll Y 232).
Terrain classification must follow source sampling coordinates, not a fixed
screen-space horizon or the shared background layer tag.

Important source clarification: WORLD.ASM's `if_ground` branch selects the
ground-dot mode (`dotsflag=1`); it does not emit per-pixel terrain coverage.
That flag alone is not sufficient to enable the planar terrain converter.
No terrain mask has been inferred from the screenshot's colours.

## Explicitly masked GPU terrain guides

Terrain sampling now accepts current-frame raster jitter, reconstructing the
plane at the displaced sample while excluding jitter from motion vectors.
D3D12/Vulkan checks pass 2560 samples including zero/fractional jitter on
sloped planes. Nonfinite jitter is rejected. This is converter support, not
yet a connected full-scene gameplay jitter sequence.

Follow-up validation rejects degenerate plane normals and non-affine camera
history. The GPU suite now passes 1280 guide samples on D3D12 and Vulkan,
including sloped planes, terrain behind either camera, exact coverage value 1
(other values are not terrain), existing model depth and reset behavior.
This strengthens the converter; terrain-mask integration is still outstanding.

GpuTemporalInputs now accepts optional visible-terrain coverage, a camera-space
plane and current/previous projection/camera data. On-device ray/plane
intersection provides depth and reprojection provides motion only for explicitly
covered pixels without valid model depth. Uncovered artwork stays unknown;
resets preserve depth but invalidate motion. This path has no CPU pixel readback.

D3D12/Vulkan checks pass 512 depth/motion cases spanning masked/unmasked pixels,
model ownership, resets, invalid source guides and exposure. Generated DXIL,
SPIR-V and Metal bindings validate (no Metal execution claimed). PC builds.
The terrain API is not yet enabled in gameplay: reliable terrain-only coverage
must still be carried from the cartridge renderer into composition. Full-world
DLSS is therefore still incomplete; a plane alone does not classify artwork.

## Camera axes and terrain ownership follow-up

SDK camera direction vectors are now normalized after inversion of the
Q15-derived view matrix; the exact quantized matrix still drives reprojection.
Unit tests cover quantized axes, up-sign conversion and degenerate/nonfinite
rejection. `tmp/dlss-gameplay-camera-unit-axes` passes 16 Original/EX evaluation
frames with only the initial reset and clean shutdown; PC build passes.

Terrain input investigation: the PC scenery pass rewrites all scenery tags to
PixelLayer::background, and GpuBackground preserves no terrain-only ownership.
The optional shadow receiver plane therefore cannot by itself identify terrain
pixels: sky, planet art and tunnel art share that tag. Full-world temporal depth
needs an explicit terrain coverage source carried through composition, not a
blanket plane intersection applied to all background pixels. No guessed terrain
depth or motion has been enabled.

## Interpolated gameplay camera history

The PC host now forwards the actual interpolated gameplay camera and Q15-derived
view matrix to DLSS. Clip history combines projection changes with current-view
to previous-view mapping, and SDK camera position/basis follow that camera.
Mapping inverts the quantized matrix rather than assuming an exact orthonormal
rotation; translations follow the game's 65536-unit coordinate wrap. Singular
or nonfinite camera transforms fail back to the complete original frame.

Math tests cover rotation/translation, wrap crossing, combined clip mapping and
invalid camera input. `tmp/dlss-gameplay-camera-history` passes 96 frames each
of Original/EX at 240 FPS with exactly one initial reset, no evaluation errors,
clean shutdown and exact queued/serialized comparison. `ORIGINAL-on.png` was
visually inspected: world and restored HUD remain present. This supersedes the
earlier missing-rigid-camera-history limitation for the diagnostic PC gameplay
path. Background/ground per-pixel guides and full-scene jitter are still missing;
camera constants alone do not implement them. Broader scene/cut coverage remains.

## Static-model history continuity

Fixed a real periodic-reset bug: the application gives static models the global
animation counter, but both ModelMotionHistory and GpuModel compared its raw
value. Each simulation tick therefore discarded unchanged geometry history.
Both now compare selected geometry frames (static shapes always match; animated
shapes compare modulo the decoded frame count). Different geometry frames still
invalidate correspondence, as do entity changes, missing frames and scene cuts.

History unit tests cover static tick changes, distinct animated frames and
wrapped frames. D3D12/Vulkan motion checks cover different static counters at
1x/2x/4x, stationary/moving and jittered/un-jittered geometry. Runtime proofs:
`tmp/dlss-gameplay-static-history` has 64 evaluated frames at 60 FPS, and
`tmp/dlss-gameplay-static-history-240` has 96 at 240 FPS, for each experience.
All have exactly one initial reset, clean shutdown, no evaluation errors, and
byte-identical queued/serialized captures. Earlier runs reset every few frames.
The test runner now asserts the evaluation count and optionally requires
continuous history for these stable-scene fixtures. This is not full DLSS
completion: world inputs, host jitter and user-facing modes remain outstanding.

## Projection history

The host now uses a validated, shared perspective/inverse calculation and carries
projection-only clip reprojection across successfully submitted evaluations.
Changed focal length or principal point no longer silently receives identity
clip transforms; reset frames still do. Invalid/nonfinite projection inputs fail
back to the complete original image. This assumes the same camera coordinates:
it does not yet supply the missing rigid camera-motion history.

`starfox_temporal_projection_tests` passes 144 samples spanning aspect ratios,
focal lengths, off-center views and depths, checking pixel projection and both
reprojection directions to 1e-5 normalized-device tolerance. Invalid inputs are
also rejected. `tmp/dlss-gameplay-projection-history` passes Original/EX real
evaluation and exact queued/serialized output comparison, with clean shutdown.
These runtime scenes are regression coverage, not proof of every camera cut.

## Queue-ordered evaluation

Removed the host's per-frame CPU fence wait. The pinned SDL D3D12 backend submits
guide conversion, native evaluation, HUD restoration, effects and presentation
on one command queue; transitions and submission order protect resident texture
reuse. Resize/reconfiguration and shutdown still wait for GPU idle. Failed
output allocation now triggers reconfiguration on the next attempt.

`tools/check_dlss_lifecycle.ps1 -Evaluate -CompareSerialized -OutputDirectory
tmp/dlss-gameplay-queue-ordered` passes Original and EX, with byte-identical final
captures between queue-ordered and explicitly GPU-idle-serialized evaluation,
distinct DLSS-off captures, no SDK evaluation errors, and clean shutdown. This
is synchronization regression evidence, not a measured FPS improvement or
completion of temporal reconstruction quality. PC build and diff checks pass.

## Model raster jitter plumbing

GpuModel/GpuScene now accept an explicit output-pixel raster displacement without
mutating temporal history poses. Planar depth follows the displaced projection;
motion removes the displacement when reconstructing current camera positions.
Nonfinite offsets and jitter on non-subpixel geometry are rejected. The default
zero displacement preserves existing callers.

The geometry-depth check passes on D3D12 and Vulkan with stationary and translated
models, zero/nonzero jitter, 1x/2x/4x scales, and scene/compositor HUD ownership.
The PC target builds. This is renderer plumbing, not completed DLSS jitter:
the host sequence, SDK sign convention, and non-model world jitter remain to be
connected and validated. No menu/release readiness is implied.

## Gameplay world/HUD split (latest)

The diagnostic gameplay path now retains the CPU backdrop at native-layer
insertion, before later HUD writes. It composes a separate world-only GPU input,
excluding two_d-tagged CPU/native/late artwork while retaining subsequent non-HUD
world writes. DLSS evaluates that world input; a new GPU pass restores exact
two_d-tagged pixels from the original final compositor output before normal
effects/presentation. Host overlays already applied afterward remain afterward.
Failure falls back to the original complete frame, not the HUD-free input.

Evidence: D3D12/Vulkan HUD restoration checks cover 128 exact pixels across all
five layer tags, opaque black and output-alias rejection. The compositor suite
adds world-only native/CPU HUD exclusion at multiple scales; all 108 existing
composition fixtures and effects/overlay suites still pass on D3D12.
`tmp/dlss-gameplay-world-separated` passes Original/EX actual evaluation/display
and clean SDK shutdown without SDK errors. Inspected `ORIGINAL-world.png` and
`EX-world.png` contain no gameplay HUD; `ORIGINAL-on.png` restores the HUD over
the evaluated world. These are Corneria captures, not all-scene acceptance.

This currently duplicates composition and snapshots CPU backdrop data only in
the opt-in diagnostic path. Native HUD coverage can hide earlier native geometry
before the scene reaches composition; complete occlusion/disocclusion behavior
still needs broader auditing. World motion, jitter/camera history, SR modes,
capability-gated settings and performance/quality acceptance remain unfinished.
No release pushed; the earlier note that all HUD is still sent to DLSS is
superseded for this newly split diagnostic gameplay path.

## First actual gameplay evaluation and display

`STARFOX_TEST_DLSS_EVALUATE=1`, together with the lifecycle and temporal-input
switches, now runs diagnostic DLAA on actual PC gameplay textures. GpuComposite
color and resident guides flow through GPU guide conversion, the scoped native
SDL command bridge, the C SDK evaluator, and back into the resident effects/
presentation path. No gameplay color/depth/motion CPU readback is used. The host
checks a common model projection, supplies explicit perspective parameters,
resets on history loss/epoch/gaps, and releases the viewport before shutdown.
Renderer changes shut down this experimental session instead of reusing stale
device resources. Normal launches do not activate any of this.

Evidence: `tools/check_dlss_lifecycle.ps1 -Evaluate -OutputDirectory <new-dir>`.
`tmp/dlss-gameplay-sdk-checked` passed 16 real evaluated/displayed frames each in
Original and EX at 800x448, distinct off/on captures, clean shutdown, and no SDK
errors (SDK warning/error logging is now captured). Initial captures in
`tmp/dlss-gameplay-first/ORIGINAL-on.png` and `EX-on.png` were visually inspected.
Warnings about ignored unrequested plugins and the temporarily retained native
swapchain reference during restoration are not treated as SDK errors.

**Not release-ready:** only equal-resolution diagnostic DLAA is connected, not
the game's SR quality selectors. Game geometry is not yet jittered; non-model
world motion/depth and full camera-history matrices remain incomplete. HUD is
still in this diagnostic input and must be separated before shipping. Current
acceptance proves evaluation/display/lifetime, not reconstruction quality or
performance. No DLSS menu choice, NVIDIA-only Quest option, neural runtime
redistribution or release push was added. DLSS5-style gameplay is not proven.

## Native command and real presentation hooks

The SDL D3D12 bridge now scopes native compute work between SDL passes, passing
borrowed native textures/list with explicit read/UAV states and restoring SDL
resource states/descriptor heaps afterward. The caller must bind a new pipeline
before later drawing. Nine RGBA8/R32/RG32 native-command/round-trip checks pass,
including alias and output-index rejection. This is not yet a gameplay DLSS call.

The default-off lifecycle path now upgrades SDL's actual swapchains immediately
after creation, so real SDL presentation reaches Streamline bookkeeping. It
restores the original owned interface before swapchain destruction and before
SDK shutdown. An initial test uncovered a leaked native reference preventing
SDL swapchain recreation and causing cleanup failure: `slUpgradeInterface`
retains a native reference but does not consume the original caller reference.
Fixed that ownership in both the adapter and analytical probe.

`tmp/dlss-game-present-hooks-fixed` proves Original/EX startup, three successive
swapchain upgrade/restore cycles, gameplay, clean SDK shutdown and unchanged
off/on pixel captures. The prior `dlss-game-present-hooks` and GDB log are failed
diagnostics, not acceptance. `check_dlss_lifecycle.ps1` now requires actual
presentation upgrade/restore evidence as well as device binding/shutdown.

Remaining: invoke evaluation with complete gameplay color/temporal textures,
world motion and jitter, preserve HUD separately, expose gated settings, and
verify evaluated gameplay quality/performance. No release pushed.

## GPU temporal texture conversion

`GpuTemporalInputs` now converts resident camera-Z and float4 motion buffers to
the exact DLSS formats: projected R32 depth, RG32 pixel motion, and a 1x1 R32
exposure texture. It records a compute pass without submission/readback. Near/
far are explicit camera inputs. Unknown/nonfinite/out-of-frustum depth maps to
far depth; invalid, reset or mismatched-depth motion maps to the SDK's -FLT_MAX
sentinel, not valid zero motion. Reset accepts a missing motion buffer without
reading it. This does not synthesize missing background/ground correspondence.

The depth checker adds 256 conversion samples covering valid/invalid camera Z,
near/far bounds, nonfinite values, rejected inputs, reset without previous motion,
depth/motion ownership and exposure. D3D12 and Vulkan checks pass. DXIL/SPIR-V
compile and generated Metal bindings validate; no Apple execution claim.
Native SDL/D3D12 texture interop now checks all three required formats (RGBA8,
R32, RG32): nine byte-exact round trips at widths 37/128/259, with all previous
DXR mask/effects checks still passing. No CPU readback was added to production.

This converter is ready for the host's frame handoff, not yet called by gameplay.
Complete world inputs, jitter and evaluated-gameplay integration remain required.

## SDK lifecycle connected to PC (latest)

The native adapter now provides trusted initialization, actual-device LUID
capability checks/binding, viewport release and shutdown. Official Streamline
secondary signatures and NGX Authenticode are verified before secure absolute-
path loading. Initialization is single-instance, downloads are disabled, and
the bound device is retained through SDK shutdown. The probe uses this lifecycle
and completed another 128 evaluated frames in `tmp/dlss-native-lifecycle-evaluation`.

PC `DlssHost` uses that same C ABI behind `STARFOX_TEST_DLSS_LIFECYCLE`, requiring
explicit absolute `STARFOX_DLSS_ADAPTER` and `STARFOX_DLSS_BINARIES` paths. Default
launches load nothing. Initialization precedes SDL. The first actual-game smoke
test exposed a shutdown access violation when SDL destroyed its renderer first;
fixed by waiting for GPU idle and shutting down SDK before Window destruction.
`tools/check_dlss_lifecycle.ps1` now passes Original and EX startup, actual GPU
binding, 16 gameplay presentations, and clean shutdown. Captures with lifecycle
off/on are identical (`tmp/dlss-game-lifecycle-fixed`). Earlier failed smoke logs
are diagnostic only. This is lifecycle integration, not gameplay evaluation.

Remaining: resident guide textures, complete world motion/depth and jitter,
evaluation/presentation wiring, HUD separation, capability-gated menu settings,
and actual evaluated-gameplay quality/performance acceptance. No release pushed.

## Reusable native evaluator (latest)

`starfox_dlss_native.dll` now separates real DLSS configuration/evaluation from
the analytical fixture. `include/starfox/render/dlss_native.h` is a versioned
plain-C boundary usable by the MinGW game and MSVC SDK adapter: no SDK/STL types,
exceptions or ownership transfer cross it. The caller supplies actual D3D12
textures, states, matrices, jitter, frame identity and reset. The adapter checks
ABI size, finite camera inputs, resource formats/extents/device ownership,
aliasing and expected states before tagging/evaluating. Errors are bounded
strings and failure codes; malformed input is rejected before evaluation.

Evidence: `tmp/dlss-native-abi-evaluation` completed 128 real GPU-evaluated frames
across four modes and all 16 saved output images match the pre-refactor fixture
byte-for-byte. Aliased resource and ABI-size rejection checks run before each
mode. `tools/check_dlss_native_abi.cpp`, compiled with the game's MinGW compiler,
loads the MSVC DLL and passes cross-compiler frame-size, failure and bounded-error
checks. Configuration now also passes through the C interface; the follow-up
fixture is `tmp/dlss-native-config-evaluation`.

This adapter records evaluation only. Trusted SDK startup before DXGI, adapter
capability checks, device binding, queue synchronization, presentation hooks,
viewport release and SDK shutdown remain responsibilities of the integrating
host. The existing probe owns those today; the game does not yet call the DLL.
No NVIDIA binaries are copied into the game or release by this change.

## Native texture handoff verified (latest)

The pinned SDL D3D12 backend now exposes an optional versioned texture bridge.
It copies matching single-mip, single-sample 2D textures in either direction
between SDL and native D3D12 resources without CPU mapping. It validates device
ownership, dimensions, format and aliasing before recording commands, restores
native COMMON/SDL default states, and tracks SDL resource lifetimes. External
resources remain caller-owned through GPU completion; separate-queue evaluation
must use the existing fence bridges. Existing older buffer bridge ABIs remain
unchanged. This does not itself initialize or evaluate DLSS.

`starfox_sdl_d3d12_interop_check` passes exact RGBA round trips at widths 37,
128 and 259 (height 23), rejects mismatched/null inputs, and still passes all
12 existing animated/resized DXR buffer/effects comparisons. Diagnostic
readback is confined to the checker. The new source compiles with the current
MinGW PC toolchain; official NVIDIA SDK code remains in the separate MSVC probe.

Still required before release: native SDK lifecycle/evaluation integration,
complete temporal world inputs/jitter, HUD separation, capability-gated UI,
and evaluated gameplay acceptance. Do not label the texture bridge as finished
DLSS or a measured performance improvement.

## Gameplay history and final composition connected (latest)

`STARFOX_TEST_TEMPORAL_INPUTS=1` now connects validated presentation history to
the PC mono GPU scene. `ModelMotionHistory::prepare` rejects duplicate current
identities as well as previous duplicates. Only a successful GPU presentation
and successful SDL present commit the new poses. Scene/camera cuts, save-state
loads, renderer recreation, context/stereo changes and failed/native-fallback
presentations invalidate history. The production menu does not expose this
test switch as DLSS; no NVIDIA evaluation is yet called by gameplay.

The first live comparison caught a depth-only request incorrectly publishing
lighting metadata on native shadows. Fixed by separating geometry-plane
preparation from effects metadata publication. Enabling temporal inputs now
leaves captured game pixels byte-identical to the disabled path.

`GpuCompositeOutput` now carries resident camera depth and float4 motion through
native viewport offsets, source/destination scaling, clipping and foreground
coverage. Opaque black/same-color HUD writes, post-late CPU writes, late GPU
overlays and solid margins invalidate underlying guides. Mosaic remains unknown
rather than publishing a false pinhole correspondence. Data stays GPU-resident.

Evidence:
- `tmp/temporal-gameplay-240-fixed`: Original/EX 240Hz, initial reset and later
  resident depth/motion, identical enabled/disabled captures.
- `tmp/temporal-gameplay-state`: D3D12 240Hz Original/EX, saved frame 4, loaded
  frame 10, asserted no previous pose on presentation 11, identical captures.
- `tmp/temporal-gameplay-60-vulkan`: Vulkan 60Hz Original/EX, same input/pixel checks.
- D3D12 and Vulkan model-depth checker: previous plane/motion tests plus final
  composite offset, differing render scales and explicit CPU coverage checks.
- Full existing D3D12 compositor checker passes, including 108 base fixtures and
  its overlay/effect coverage suites. Stereo/history unit test passes.

Remaining: complete world/non-model temporal inputs, jittered game rendering,
native NVIDIA texture and evaluation handoff, HUD exclusion from the neural
pass, capability-gated settings, and actual evaluated gameplay acceptance.
The new test switch is input-generation evidence, not a finished DLSS option.

## Actual DLSS and neural evaluation verified (evening update)

The optional second argument of `starfox_streamline_probe` now runs an actual
D3D12 evaluation fixture, not just capability queries. It supplies analytical
moving foreground/background color, perspective depth, previous-minus-current
motion, subpixel jitter, explicit exposure, and two history resets per mode.
Quality, Balanced, Performance and DLAA each completed 32 frames on the RTX
5070 Ti Laptop: 128 evaluated, fence-completed frames with changing output and
foreground-area checks. Output is 1280x720; input dimensions were respectively
853x480, 742x418, 640x360 and 1280x720.

The initial evaluation exposed a missing frame-tagging preference and then a
missing manual-integration presentation hook. Both were fixed. The clean run
upgrades its own hidden swapchain, presents once per frame, and has no SDK
error messages. Proof: `tmp/dlss-evaluation-present/evaluation.txt`, PPM images
and `tmp/dlss-evaluation-present.log`. The earlier `first` and `second` runs are
diagnostics, not clean acceptance results.

The same evaluator was copied into `tmp/renodx-dlss-evaluation`, with the
previously authorized author add-on and signed NR runtime. ReShade.log now
explicitly reports **inline feature 18 evaluation succeeded**, including count
60, rather than merely reporting a loaded DLL. Its first evaluation was skipped
because host-state tracking was incomplete; subsequent evaluations succeeded.
GPU-read-back output differs from plain DLSS, and the final Quality image was
visually inspected. This verifies the neural path on the analytical fixture;
it is NOT evidence of neural gameplay, visual quality across levels, or FPS.
No neural binaries were added to the normal game or release package.

Reproduce plain DLSS (new output directory required):

```powershell
build/streamline-probe-msvc/starfox_streamline_probe.exe tmp/streamline-sdk-2.14.1/sdk tmp/dlss-evaluation-new
```

## Per-pixel model motion integration

`GpuProjection::enqueue_motion_surface` now reconstructs a visible camera-space
point from planar depth and reprojects it through rigid-object previous pose.
It returns pixel-space XY, camera Z and explicit validity. It handles projection
changes and current jitter; reset, unknown/nonfinite depth and previous-near
clipping invalidate motion instead of fabricating valid zero vectors. All 728
analytical samples pass on D3D12 and Vulkan.

`GpuModel::enqueue` accepts an optional preceding pose and chains this pass
directly after its resident raster depth. Packed current/previous transforms
produce the mapping without CPU per-vertex projection. Changed coordinates,
singular/mismatched byte/word transforms and native word-wrapped paths reject
correspondence. Destruction, folded faces and screen-space deformation retain
unknown motion. `GpuScene` carries motion according to visible color ownership;
opaque HUD writes invalidate it, even when black. Motion-bearing draws bypass
the fused raster merge so no prior object's motion is silently discarded.
Stereo copies transform both current and previous poses into the same eye.

Model translation and opaque HUD ownership tests pass at 1x/2x/4x on D3D12 and
Vulkan alongside the existing 366,183 planar-depth samples. Normal gameplay
still does not request temporal data: history/scene resets, non-model world
inputs, jittered rendering and native texture/evaluation handoff remain to be
connected before a working DLSS option can ship. Earlier sections below record
the progression and should not be mistaken for the latest evaluation status.

Final checks for this pass: Windows executable rebuilt; six targeted runtime,
stereo, core and raster checks passed (27.55 seconds). Linux software Vulkan
also passed both the model-depth/motion and full projection checkers. DXIL,
SPIR-V and generated Metal source pass freshness/binding checks; Apple GPU
execution and Android rebuild are not included in this pass. No release was
published and no new PC/Quest handoff archive was made.

## Planar model depth integration

`GpuModel::enqueue(..., geometry_depth=true)` now produces a separate float
camera-Z buffer. GPU surface preparation computes planes from actual transformed
vertices; source face IDs survive BSP/span ordering in the existing command ABI.
Rasterization evaluates the plane at each covered pixel centre, after texture
transparency, using the model's projection. Native rounded vanishing points and
fractional/high-resolution projection are distinguished. Existing mean-depth
effects metadata is unchanged. Unknown depth is zero; folded/nonplanar faces,
lines/sprites, colour-warp, wave and wobble paths do not fabricate valid depth.

`GpuModelDraw::geometry_depth` carries this opt-in through mixed scenes and
stereo draw copies. Fused raster composition and separate scene merge preserve
depth according to visible colour ownership (not effects surface ownership).
An opaque black/HUD overlay invalidates its pixels' depth; untouched pixels
retain the background depth. No per-frame readback is used in production.

`starfox_gpu_depth_check` passes on D3D12 and Vulkan: 366,183 analytical sloped
plane samples at native/fractional 1x/2x/4x, including fractional vanishing
points; exact unchanged colour/effects output; folded-face invalidation; mixed
scene painter ownership; byte-identical fused/separate depth composition.
Additional existing checks pass: 192 Original real-model mixed-batch images
on D3D12, 12 EX Arwing and 12 EX BOXXIE images on Vulkan, and mixed raster
binning comparisons on both backends. An initial EX first-16-header sample
failed the nonempty-fixture check and is not counted as passing evidence.

This is not a complete DLSS input set: per-pixel motion, ground/background and
other nonplanar geometry depth, jitter/history integration, GPU texture handoff
and actual evaluation remain. The normal game's draw records do not yet opt
into this buffer. DXIL/SPIR-V compile and execute; Metal source is generated
and binding-validated, not Apple-compiled or hardware-tested.

After the final shader change, all Windows targets rebuilt successfully and
both backend depth checks passed again. Eight targeted CTest checks passed
(11.49 s): packed faces/projection, raster commands, core, stereo output,
Original/EX runtime smoke, and isolated effects. This is not a fresh full
55-test run or headset acceptance.

```powershell
cmake --build build/current --target starfox_gpu_depth_check
$env:SDL_GPU_DRIVER='direct3d12' # repeat with vulkan
build/current/starfox_gpu_depth_check.exe
```

## Authorized isolated add-on test

User explicitly approved loading the unsigned author V4.7 add-on in isolation.
`tmp/renodx-isolated-d3d12` contains a copy of the Windows executable/assets,
the author add-on/NR DLL, official SDK SR DLL, and ReShade64.dll extracted from
the official ReShade 6.8.0 Addon installer and named dxgi.dll. No installer was
run and the normal game directory was not modified. No Vulkan/global layer was
installed. Both the setup-menu run and a 180-frame Original Corneria run exited
0 on D3D12. ReShade.log confirms V4.7 registration and the signed NR runtime's
reference hash match, but supplies no evidence of neural frame evaluation.
Native capture `corneria-capture.bmp` was inspected; it occurs before ReShade
and is NOT photographic proof of neural output. The game still has no native
DLSS evaluation path, so loading successfully does not make realism functional.

## Verified capability probe

User approved the official SDK license. The isolated Windows/MSVC target in
`tools/streamline_probe` now initializes the approved SDK and checks actual DXGI
adapter LUIDs. NVIDIA's secondary signatures are validated for Streamline DLLs;
NGX's different signing scheme is checked through standard Authenticode. DLL
search is restricted, OTA/downloaded plugins are disabled, and no game files
are replaced. This is not renderer integration or a working menu option.

Built and executed against official v2.14.1: `slInit` and DLSS requirements
return `eOk`; the primary RTX 5070 Ti Laptop adapter returns `eOk`. Intel and
the other exposed adapters return `eErrorAdapterNotSupported`, including a
second adapter bearing the NVIDIA name. This demonstrates why capability must
be checked per adapter, not inferred from its name. No frame was evaluated.

Device follow-up: the probe now creates a D3D12 device on the supported adapter
and resolves DLSS's optimal-settings API after `slSetD3DDevice`. A locally
generated custom-engine project GUID was required; without it NGX unloaded the
feature despite the earlier support result. With that identity the device and
all four queries pass: 1920x1080 output requests 1280x720 Quality, 1114x626
Balanced, 960x540 Performance, and 1920x1080 DLAA. Device lifetime extends past
Streamline shutdown. This is still not a rendered/evaluated DLSS frame.

Reproduce from a VS x64 developer shell (paths can be absolute):

```powershell
cmake -S tools/streamline_probe -B build/streamline-probe-msvc -G Ninja -DCMAKE_CXX_COMPILER=cl -DSTREAMLINE_SDK="<extracted official SDK>"
cmake --build build/streamline-probe-msvc
build/streamline-probe-msvc/starfox_streamline_probe.exe "<extracted official SDK>"
```

After the user explicitly requested joining, the RenoDX wiki's
`https://discord.com/invite/renodx` invite unlocked the server. Download channel:
https://discord.com/channels/1408098019194310818/1545049227321810974

Downloaded the channel's forwarded V4.7 `renodx-dlss5.addon64` and companion
`DLSS310.8.0-Streamline2.13.zip` into `tmp/renodx-author-downloads`; extracted only
`nvngx_dlssnr.dll`. Subsequently loaded only in the authorized isolated test above.
The add-on is unsigned; the neural DLL has valid NVIDIA Authenticode. This
establishes the download source and DLL signature, not add-on safety, bridge
compatibility, or redistribution rights. The archive contains DLLs but no
license files. The channel also offers a separate ShortFuse `renodx-dlss.addon64`
variant; its posted instructions say the two add-ons cannot be used together.

SHA-256 evidence:
- V4.7 add-on: `D5ADF82EB44B065F4C590AC91FE824BAB07AFEA0EB9F994BDE936710C8593952`
- Companion ZIP: `3FDB7CB25250259332550F419DBAC516A2EBA778D8592DA75CB2FE8ABFC781D8`
- Neural DLL: `E16BCF15E16E13F527491CDF7845B2FE6521A738D8F7C9C721866A8496E1FC8E`

## User-provided experimental bridge

https://github.com/NIGos/dlss5-bridge provides a possible unofficial ReShade
route; absence from Streamline's public API does not rule this out. Inspected
README and MIT license. The bridge itself does not perform neural rendering:
it requires a separate compatible neural add-on and nvngx_dlssnr.dll (README
points to RenoDX's Discord distribution), plus ReShade and the SR runtime.
Those neural components have now been obtained as recorded above; the bridge's
MIT license does not establish redistribution rights for them.

Native DLSS inputs are preferred. The optical-flow substitute requires usable
depth and approximate motion and documents soft text/smearing. Vulkan mirror
also documents synchronization stalls; stereo/multiple-view compatibility is
not established for this game. Any eventual support must be experimental,
default off, and preserve HUD outside the neural pass. No bridge or third-party
neural DLL has been added to the normal game installation. The authorized
isolated test is documented above. The official Streamline v2.14.1
SDK is extracted in tmp and used by the isolated capability probe above,
not bundled or integrated into the game.

Requested: DLSS Super Resolution and an AI-realism option. Neither is
implemented or exposed as a working menu choice yet. Both must default off
and be capability-gated; Quest must not display NVIDIA-only features.

Verified local adapter: NVIDIA GeForce RTX 5070 Ti Laptop GPU, driver 595.79.
Public NVIDIA-RTX/Streamline release: v2.14.1. Its public include directory
exposes DLSS SR, Ray Reconstruction and Frame Generation; no DLSS 5 neural
rendering header/API was found. NVIDIA's September 1 DLSS 5 research page
describes RTX 50-series appearance generation, but is not an integration SDK.
Do not substitute Ray Reconstruction or a color-grading preset and label it
DLSS 5. That portion needs an available official integration interface.

Super Resolution integration prerequisites (official ProgrammingGuideDLSS):
render-resolution color, depth, motion vectors, output-resolution color,
camera matrices/jitter, history-reset handling and feature capability checks.
GpuSceneDraw now carries optional object identity (slot, pool generation,
shape, strategy and type), populated by the PC object's actual draw path and
preserved across stereo eye copies. Unidentified helper/shadow draws remain
explicitly unidentified. The stereo regression passes, including recycled-slot
and changed-shape inequality. Previous presentation poses, scene/reset epochs,
and motion-vector surfaces are not connected to the renderer yet.

`ModelMotionHistory` now implements the previous-pose store for successful
presentation submissions, with explicit serial/epoch/dimension checks. Tests
cover recycled entities, topology animation/explosion changes, skipped frames,
duplicate identities, disappearance, projection changes and reset. It is not
enabled in normal rendering: the future caller must supply scene/load-state
epochs, reject ambiguous current identities, and commit only submitted frames.
This is CPU pose metadata, not CPU-generated motion-vector pixels.

GPU vertex motion is now implemented by `GpuProjection::enqueue_motion` and
`motion_portable.hlsl`: paired unjittered projected vertices produce
previous-minus-current render-pixel XY, linear camera Z, and validity. The
pass records into a caller-owned command with no submission/readback. GPU
checks pass on D3D12 and Vulkan for movement direction/scale, stationary valid
vertices, history reset, behind-camera points, NaN, overflow, and alias rejection.
DXIL/SPIR-V compile and generated Metal source/bindings validate; Metal execution has
not been tested. This is not yet wired into the normal model draw path, and
per-pixel interpolation, clipping correspondence, surface composition and DLSS
evaluation are still required. Do not expose a working DLSS menu choice yet.

Portability follow-up corrected the motion pipeline's Metal entry point to
`main0`, matching SPIRV-Cross output, and capped dispatches to the portable
65535-workgroup limit. Generated MSL is not evidence of compilation by Apple's
Metal compiler or of execution on a Mac.
Motion-vector generation and disocclusion/history correctness therefore need
real renderer work before a quality selector is useful. Do not fake zero
motion vectors or upscale the HUD as though it were world geometry.

Sources:
- https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.1
- https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.1/docs/ProgrammingGuideDLSS.md
- https://research.nvidia.com/labs/adlr/DLSS5/

## September 24 default-path performance correction

The above historical scaffold status predates the current opt-in gameplay
integration. In the current worktree, DLSS OFF no longer upgrades SDL's D3D12
swapchain merely because the optional SDK is installed. It also retains the
ordinary Vulkan GPU backend on Windows instead of forcing D3D12. Turning DLSS
ON selects D3D12 and recreates the renderer once; turning it OFF restores the
native presentation path. Quality changes while ON do not rebuild the device.
The optional RenoDX/ReShade proxy is not included in the release workflow.

An installed-runtime lifecycle check on the local NVIDIA adapter confirms
that OFF initializes the adapter but makes no `dlss-presentation: upgraded`
call, while ON upgrades, evaluates eight frames and restores. This removes a
default-path cost but is not a GTX 750 Ti performance measurement; the reported
weak-system slowdown still needs hardware validation.

The 180-frame, 1×/unenhanced, hidden/unpaced Original 1-1 check on this
NVIDIA laptop measured 3.578 ms frame-work p99 on the restored default
backend, versus 3.216 ms with D3D12 explicitly forced. These are local
renderer-work measurements, not on-screen FPS or evidence for GTX 750 Ti;
the native-default behavior is about preserving the pre-DLSS backend rather
than claiming a universal speedup.

A 24-frame diagnostic toggled OFF→DLAA→OFF without restarting the process:
eight DLAA frames evaluated, the wrapped swapchain was restored, and the
final renderer returned to the native non-D3D12 backend. The script is
`tools/check_dlss_toggle.ps1`.
