# PC/Android renderer report — October 1–3

## October 3 — immutable SBS GPU ray-connectivity uploads

Current development PC: `B46552B6F6FE3ABF140967764CFB2A49EA9CA3701F48BF515F55EFAE080FE775`.
Core library: `7A8A8E3FFA1163FCC6C3995185C8BA56CB1A46DAF3A6E5A8C711A8DCC0E2EEC0`.
Native display library: `4F3DB0AA9F061F89D5780A37DDAE30F8A53B663BEA204A7F72B80CE8BA60F422`.
Ray checker: `040F22FB9BEE8F4790569843E804F4A5A3CD18C511C2DEE5804EA63AA94499D8`.
Stereo checker: `9E66E9602529D1CD81BDC5E40331F62B57AEBF5B3A238A00C476F01CD96EB1F9`.
Optional SR adapter/checker unchanged. Earlier broad native SDK/TAA/clipping
matrices remain historical, not renewed by this relink.

Eligible ordinary SBS models now upload immutable ray triangle vertex indices
and material-corner connectivity once per exact matching source within one
pair. The existing source pool retains its 8 MiB GPU plus 8 MiB transfer budget.
Both inputs require matching prepared-face/ray identity on the same device and
an encoded packet. Unencoded, mismatched, unsupported or over-budget packets
use the ordinary full-quality path. Shadow-only users omit material connectivity;
ray-OFF and emissive exclusions do not allocate it. Mutable effects retain their
specialized paths. No source address is cached across pairs.

The first command cycles the immutable inputs before either eye binds them;
both encoders join before identities expire. Transformed points, expanded
materials, acceleration structures, histories and ray results stay eye-owned.
No shader payload, quality, saved preference, submission ordering or fence
changed. Scene-owned ray copies are now included in upload accounting; earlier
checkpoint totals omitted them and must not be compared directly with these
totals. `STARFOX_TEST_DUPLICATE_STEREO_RAY_UPLOADS` isolates only the new GPU
sharing policy while preserving shared CPU ray preparation.

Completed evidence on this executable:

- Both D3D12 and Vulkan pass 20 new GPU-borrowed native/continuous/wave/wobble
  fixtures and ten independent CPU-packed material comparisons per API. Twelve
  hand-transformed stereo pairs per API cover split/joined/parallel recording,
  changing connectivity/palette, shadow-only/ray-OFF storage and cancellation/
  retry. Position tolerance is 0.0001; material records and sharing-policy
  comparisons are byte-exact. Three actual queued owned pairs preserve changed
  source data and separate eye results. Borrowed-source cancellation, unencoded
  fallback and asymmetric vertex/corner connectivity are separately exercised.
  Existing 2,925 vertex checks and full native stereo/MSAA suites pass. Native
  checkers reject known ReShade modules. Accepted logs are in
  `D:/SFE-validation/stereo-ray-gpu-sources-oct3`.
- Seventy full-SBS images exactly match GPU-duplicate uploads on the SAME
  binary: water, asteroids, EX lava, MSAA/rays, ray-OFF, parallel and joined,
  five images/case/API. Each policy records 32 actual complete eye pairs with
  identical logical demand and no fallback. Eligible cases reduce actual bytes
  and copies; asteroid/ray-OFF cases add no sources or copies. Fifty additional
  images exactly match the preceding ED4 checkpoint (25/API). These are separate
  prior-checkpoint comparisons, not extra same-binary cases. Image evidence:
  `stereo-ray-gpu-images-direct3d12-oct3/results.json` and
  `stereo-ray-gpu-images-vulkan-oct3/results.json`, beneath `D:/SFE-validation`.
- Nine rebuilt CPU suites pass; 2,697 decoded source models retain independent
  BSP triangle/corner comparisons (7,052 polygons, 808 lines and 31 sprites).
  Android ARM64/API 26 syntax passes GPU model and GPU scene. No authorized ADB
  device is present; syntax is not APK/device exit-relaunch acceptance.
- Embedded mono K/M/K passes 96 evaluations, 24 held uses and 120 once-only
  endings. All 29 plain-menu/loading cases pass, including both actual neural
  Software-to-GPU transitions. RENDERING/loading and full-SBS lava were viewed.
  SR factory/owned-weave/missing-runtime/re-arm guards and 36 exact unavailable-
  SR Original/EX mono images pass. No physical Leia panel is certified.

Manifest: `D:/SFE-validation/stereo-ray-gpu-sources-oct3/manifest.json`.
The quiet same-binary 180-pair D3D12 ABBA workload keeps authored demand at
26,003,600 bytes per trial. Duplicate/shared uploads are 13,209,544/12,867,944
bytes and 33,331/31,999 copies: reductions of 2.586% and 3.996%. Frame medians
are 8.893/8.812/8.855/8.721 ms; timing is effectively unchanged and mixed,
not sustained FPS acceptance. Other projects remain running. Vulkan functional
checks pass, but no new Vulkan timing run is claimed. Profile:
`D:/SFE-validation/stereo-ray-gpu-profile-d3d12-oct3/results.json`.

Initial test-only failures remain in diagnostic logs: missing packing argument/
index type, a hand oracle's default pose offset, a split cancellation hook that
does not exist, and attempting unpublished borrowed ray output. Fixtures were
corrected and rerun; production publication/quality was not weakened to pass.
All owned checks ended. Seven protected settings/release/injector-config/log/
APK hashes match this turn's initial values. The historical ReShade-log exception
below remains recorded. No device installation, release copy or publication;
the development executable embeds private assets and must not be published.
The small-model experiment stays OFF. Physical Leia/Android, true secondary
motion, broader finishing/peak-memory, sustained SBS speed and packaging remain
open; Ally troubleshooting deferred, full goal active.

## October 3 — immutable SBS CPU ray connectivity

Preceding development PC: `ED4F4DA05535BA06AE21A8DBC92E8C60CBB50A6B418094ECA41FBF16879D5633`.
Core library: `92388B84CA692EA5EA765356DDCE8E4455A721B61EDB87C545378EF32711C89F`.
Native display library: `4F3DB0AA9F061F89D5780A37DDAE30F8A53B663BEA204A7F72B80CE8BA60F422`.
Ray checker: `A43714EA2040D5AC7006B00B954CBD22D3782BCE725C10CB12DDDF963A5470C0`.
Stereo checker: `19476D6FE5472D8CF1075251CFF8B9B90E1874CEFA6A9D7DD013058C1C96AF49`.
Optional SR adapter/checker unchanged; earlier broad native SDK/TAA/clipping
matrices are historical, not renewed by this relink.

Eligible ordinary SBS ray consumers borrow immutable triangle IDs and material-
corner connectivity prepared once per matching face source within a pair.
Both encoders join before packets die. Transformed points, GPU materials,
acceleration structures and ray results remain per eye. Existing GPU ray
topology cache/copy behavior is unchanged: this removes repeated CPU packing,
not more GPU uploads. Shadow-only packets omit material connectivity. Emissive
objects and scenes without ray requests prepare none. Explosions, colour warp,
axes and billboards retain specialized paths. The common retained CPU source
budget remains 8 MiB; overflow uses ordinary full-quality packing. No source
address is cached across pairs. Wrong material policy rejects/clears ray output.

Completed evidence on this executable:

- Nine rebuilt CPU suites pass. All 2,697 decoded models compare ray triangle
  and corner IDs against independently traversed original BSP faces. Pair tests
  cover mixed shadow/reflection users, emissive/mutable exclusions, fresh source
  mutation, shadow-only storage, ray-OFF and budget fallback.
- Each API's ray checker passes 20 borrowed native/continuous/wave/wobble model
  cases, wrong-policy rejection/retry and ten GPU material packets against CPU
  packing. Existing scene/ray tests pass 2,925 vertex checks and independent
  stereo shadows/reflections. Both full stereo/MSAA suites pass, including
  joined/parallel queued ownership/cancellation. Native checkers reject known
  ReShade modules. Logs: `D:/SFE-validation/stereo-ray-sources-oct3/*-rays.log`
  and `*-stereo.log`.
- Fifty full-SBS images exactly match duplicate-ray preparation on the SAME
  binary: water, asteroids, EX lava, MSAA/rays and ray-OFF, five images/case/API.
  Actual completed-pair accounting validates each policy. Over 32 pairs water
  prepares 270 packets for 860 ordinary eye-model users; lava 219 for 576;
  MSAA/rays has the same water counts. The selected asteroid recording has no
  eligible ordinary ray-source users; zero extra preparation and its existing
  specialized/fallback output pass. The initial D3D12 harness wrongly required
  positive asteroid preparation and stopped after water passed. That failure
  is retained; the corrected four-case follow-up passed. Completed water output
  was revalidated/collected without restarting it. Image roots:
  `stereo-ray-images-d3d12-oct3`, `stereo-ray-images-d3d12-followup-oct3`,
  `stereo-ray-images-vulkan-oct3`, beneath `D:/SFE-validation`.
- Android ARM64/API 26 syntax passes packed faces, GPU model and GPU scene;
  source checks are not a new APK/device-relaunch acceptance.
- SR factory/ownership/missing-runtime/re-arm guards and 36 exact unavailable-
  SR Original/EX mono images pass. Mono embedded K/M/K passes 96 evaluations,
  24 held uses and 120 endings. All 29 plain-menu/loading cases pass, including
  actual renderer transitions. RENDERING and the lava stereo capture were viewed.

Manifest: `D:/SFE-validation/stereo-ray-sources-oct3/manifest.json`. Loaded
same-binary 180-pair D3D12 ABBA medians are 8.791/6.839/7.088/6.766 ms, with
unchanged authored source demand/uploads. Other projects remain running;
timing drift does not establish isolated or sustained FPS improvement.
Evidence: `stereo-ray-profile-d3d12-oct3/results.json`.

All owned checks ended. Seven protected settings/release/injector-config/log/
APK hashes match this turn's initial values. The preceding ReShade-log exception
below remains recorded, not erased. No effects, quality, saved preferences,
device APK or published build changed. The small-model experiment stays OFF.
Physical Leia/Android, true secondary motion, broader finishing/peak-memory,
sustained SBS speed and packaging remain open; Ally troubleshooting deferred,
full goal active.

## October 3 — immutable SBS face/material preparation and uploads

Preceding development PC: `D31672B3FF538B7B200B459044F609AB8AA283FB92ABB50188A4830F7A2DB3C4`.
Core library: `6F2B5F663E79DE547A3F7EB69E2A979CCFF18D0D14244380D8BFDCD3B1DA4183`.
Clean-runtime stereo checker: `23B1264A1E7E88004A14053D8D3D3837DD9DC8CEDD98A377EF94AE723F4D3976`.
Native display library: `79CAD75E0EE1A713446750F36DFCDE2F7DBCDCE8059898EC3164EEC4BDCDDCEC`.
The optional SR adapter/checker are unchanged. Earlier full native SDK/TAA,
clipping and tracer matrices are historical, not renewed by this relink.

Ordinary SBS models now prepare immutable face corners, polygon descriptors,
materials and texels once per actual matching source state in one pair. The
key includes derived source depth/light, animated colour frame, depth tables,
palette/forced colours, texture scroll, world/terrain tags, wave/cel/wobble/
wireframe state, colour base and culling. Eye projection is not a material key.
Different eye material states receive different packets. Source shapes remain
immutable while both encoders read them; no address cache survives the pair.
Explosions, active colour warp, axes and billboards keep their specialized
paths. Budget exhaustion keeps ordinary full-quality packing.

Identical ordinary face inputs also upload once through the existing bounded
source pool; geometry, visibility, derived surface shading, depth, motion,
ray outputs and painter targets remain eye-owned. The surface shader now binds
the selected inputs for this draw instead of assuming every input is private.
This binding correction was found by a failing native check before acceptance.
No shader payload, arithmetic, quality setting, submission or fence changed.

Completed evidence on this executable:

- Nine rebuilt CPU suites pass. Prepared-face guards exercise every material
  dependency, independent camera changes and mixed polygon/line/sprite data.
  All 2,697 decoded source models match ordinary packing exactly (7,052
  polygons, 808 lines and 31 sprites).
- Both APIs' full stereo suites pass. The expanded source-pool fixture checks
  90 independent SoftwareRenderer images including texture holes, changed
  artwork/scroll/depth colours, unready packets, cancellation and recovery.
  Joined and parallel paths each preserve 6,266,880 exact colour/surface/depth/
  motion bytes, including queued source-mutating pairs. All 54 real MSAA cases,
  162 retained palette resolves and 96 sparse-to-motion batches pass.
  Clean-process logs: `D:/SFE-validation/stereo-faces-oct3/*-clean.log`.
  Each verifies absence of the known ReShade injector after renderer/device
  creation and after the full suite. Earlier diagnostic logs are retained.
- Thirty default full-SBS images per API exactly match `369F87E1...`, including
  enhanced water/reflections, banked 3x terrain, Venom motion, high MSAA and EX
  lava: `stereo-faces-images-{d3d12,vulkan}-oct3/results.json`.
- Actual ARM64/API 26 Android syntax compilation passes for packed faces,
  GPU model and GPU scene translation units. This is not APK/device acceptance.
- SR ownership/factory guards pass six outputs and resize/failure/cancellation/
  foreign-device/missing-runtime/re-arm cases. Unavailable SR passes 36 exact
  Original/EX mono images on both APIs: `sr-stereo-faces-oct3/results.json`.
- Embedded mono K/M/K passes 96 evaluations, 24 held uses and 120 once-only
  presentation endings: `dlss-stereo-faces-oct3/toggle.log`. All 29 Preview-OFF
  menu cases and the viewed RENDERING capture pass:
  `plain-menu-stereo-faces-oct3/results.json`.

The same-binary ABBA workload retains 31,128,464 input bytes across each 180-pair
run. On both APIs, shared faces reduce actual uploads from 23,217,592 to
15,534,888 bytes (33.09%) and copies from 46,859 to 36,341 (22.45%). Pool copies
are included, not inferred from borrowed pointers. Quiet frame medians in ABBA
order are D3D12 6.546/5.030/4.622/3.889 ms and Vulkan
5.409/5.278/5.392/6.698 ms. Timing drifts with concurrent builds; these establish
fewer uploads, not a reproducible sustained FPS gain. Per-model CPU clocks
exclude the now-shared preparation and must not stand in for total CPU savings.
Evidence: `stereo-faces-profile-d3d12-oct3/results.json` and
`stereo-faces-quiet-{d3d12,vulkan}-oct3/results.json`. The first quiet D3D12
benchmark completed; a later read-only log hash hit a transient writer lock.
That completed trial was collected without rerunning it; the helper now shares
the log handle for hashing after complete-trace checks.

All owned checks ended; six protected release/settings/injector-config/APK
hashes match. The initial native checker picked up the installed system-wide
ReShade Vulkan layer and updated `build/current/ReShade.log`; its header names
the checker, not the game. Subsequent game tests already used clean processes.
The native checker was rerun with that layer disabled for the test process and
now rejects the known ReShade module explicitly. The changed diagnostic log is
preserved rather than overwritten; no injector configuration was changed.
The single-pass small-model experiment remains OFF. Physical Leia and
Android relaunch, true secondary motion, broader finishing/peak-memory,
sustained SBS speed and packaging remain open. Ally troubleshooting is deferred;
the full goal stays active. Other projects' compiler jobs remain running.

## October 3 — independently lazy clipping and span preparation

Preceding development PC: `369F87E1C53E2FA94C93F5D8D1357A3BBB53724B8BFA21A12B689E345542D625`.
Core library: `066A10C1712E13AD94C79B446B5BE9B39E94C50A693E1FF35277865D85A51E12`.
Clip checker: `0301BF4861509E375CE61E118903C288243CB211A7FFD908F659D2AD77E12A08`.
Stereo checker: `A22036E29040B4D02082C3961E655D490B393207516C42CC1B7CC6183F76AAD1`.
Native owner checker: `8161D814683258E350860CAD968DF8831F9617417CFB94E3181B05EC94A5B2D1`.
SR adapter/native library are unchanged; prior full SDK/TAA matrices and
loaded ABBA timings are historical, not renewed by this relink.

GpuClip used to compile native clipping, continuous clipping and span emission
together. These stages now prepare independently: clip-only work creates one
pipeline; an ordinary continuous model plus spans does not compile unused
native clipping. Same-device policy changes retain existing pipelines, clipped
polygons, spans and recorded consumers. Specialized span tracing prepares the
standard span shader only if its MSAA mask-clear pass actually needs it.
The Intel compact DXIL selection, all shader payloads, binding counts, source
arithmetic, quality, submissions and user settings are unchanged.

`lazy-clip-init-components-verified-oct3` completed on both D3D12 and Vulkan:

- Actual preparation-job counts verify native-first and continuous-first
  lifetimes, separate span preparation, warm policy reuse, invalid requests,
  cancellation and release/recreate. Independently authored polygon/row
  expectations are downloaded before switches in one unsubmitted command.
  SNES solid coverage retains the inclusive right edge and exclusive bottom.
- Complete clipping suites pass: 25,362 exact native and 50,724 fractional
  polygons, 31,418,112 native and 57,003,776 fractional SoftwareRenderer pixels,
  near-plane/residual/line/binary64 cases and 24 word near-plane comparisons.
- Recycled span-clear tests pass 192 cases and 1,375,631,424 exact live command/
  color/surface bytes. Minimal parallel clearing passes nine poisoned cases,
  wide dispatch and mask-prefix/tail guards. The colour-only optional tracer
  passes 384 cases, including clear-mode/full overrides and MSAA storage.
- Complete stereo/source/memo/MSAA checks pass, including 96 changing sparse-
  to-motion batches and independently authored eye motion/depth expectations.
- Eight rebuilt CPU suites pass: DisplayXR, joined preparation, simulation,
  stereo, executing-thread clock, GPU preparation, controller and UWP input.

The native/continuous-first preparation fixtures also pass on Intel integrated
graphics with both D3D12 and Vulkan (`lazy-clip-init-low-power-oct3`); these are
not full-scene or sustained-performance tests on that adapter. Android NDK
ARM64/API 26 syntax compilation of the actual GPU clipping translation unit
passes; no Android device is attached and this is not APK/device acceptance.

The current default path passes 30 byte-identical full SBS images per API
against `BC192777...`, across six scenarios including enhanced water, banked
3x terrain, Venom motion, high MSAA and EX lava:
`lazy-clip-init-images-{d3d12,vulkan}-oct3/results.json`. Real SR ownership/
factory checks pass six exact outputs plus resize, failure/cancellation,
foreign-device and missing-runtime/re-arm guards. Unavailable SR passes 36
exact mono images across Original/EX and both APIs:
`sr-lazy-clip-default-oct3/results.json`. Actual embedded mono K/M/K passes
96 evaluations, 24 held uses and 120 once-only endings:
`dlss-lazy-clip-default-oct3/toggle.log`. All 29 Preview-OFF menu cases and
the visually inspected RENDERING/loading capture pass:
`plain-menu-lazy-clip-default-oct3/results.json`. Enhancements, SSAA and SR
output do not enter scene rendering or change the plain display. Both neural-
model Software-to-GPU transitions restore D3D12 directly. Menu work timings
are loaded diagnostics, not isolated performance comparisons. All owned checks
have ended; all seven protected artifact hashes match.
This removes proved unnecessary pipeline creation, but does not
establish sustained FPS improvement or eliminate every first-use stall.
The single-pass small-model experiment stays OFF. Physical Leia/Android,
true secondary motion, broader finishing/peak-memory, sustained SBS speed and
packaging remain open; Ally deferred, full goal active. Other compiler jobs
keep running. No release, device install, preferences, injector or APK changes.

## October 3 — independently lazy projection stages and executing-thread costs

Preceding development PC: `BC1927778E9054D314898C4AC2F3D32CF481EFA4D8B91B4C5F08747D4040AA12`.
Core library: `2F947D91BDA9C69B53930FB4E300F9A9DA88D23F1004EBCF7D1E2A793EFA0B62`.
Stereo checker: `4D83D5F81ADF23944FE556715315B2B35000DF2FBBBB70F0AC8C1E7B2A9AB902`.
Native owner checker: `7268D388F0376AB66A31C6CD16C06B98F25052A70BE4F2B4FE4E4F2B6C570D6E`.
The SR adapter/native library are unchanged. This relink does not renew the
historical full native SDK/TAA matrices.

GpuProjection previously created five word/visibility/transform pipelines
when any operation initialized its device, including motion, grid, dust,
particles or text that did not use those pipelines. Device setup is now
separate from independently lazy stage creation. An ordinary projection,
continuous projection or visibility operation creates only its requested
pipeline. Same-device policy changes preserve other pipelines and recorded
buffer outputs; release/device changes retire the complete owner. Existing
joined preparation/event pumping and error propagation remain in use.
No shader arithmetic, geometry, quality, queue ownership or user setting was
changed. The bounded single-pass small-model experiment still defaults OFF.

Completed current-artifact checks:

- `lazy-projection-init-components-oct3`: D3D12 and Vulkan pass exact queued
  motion, word, continuous, visibility and text outputs. Actual preparation
  job counts prove that the first motion operation creates one pipeline,
  rather than motion plus five unused stages. Warm switches do not recreate
  shaders; invalid requests, cancellation and release/recreate are covered.
- Both backends' complete projection suites pass: 393,987 exact native point/
  face checks, 131,329 continuous points, text/particles/dust/grid, per-pixel
  motion, source cancellation and residual-aware axis checks.
- Both complete stereo suites pass, including the independent source/memo,
  billboard, painter, parallel failure/retry, MSAA and 96 changing sparse-to-
  motion batches. No tolerance was relaxed.
- Eight rebuilt CPU suites pass: controller, UWP, stereo, simulation,
  DisplayXR, joined preparation, GPU preparation and executing-thread clock.
- Android NDK ARM64/API 26 syntax compilation of the actual GPU projection
  translation unit passes. This is not APK/device/Metal execution acceptance.

The new optional `STARFOX_TRACE_MODEL_THREAD_COST` diagnostic requires model
cost tracing. It records current-thread kernel/user CPU time on Windows or
POSIX thread CPU time where supported, with actual scene batch/draw indices.
It is disabled without OS clock queries on the ordinary path. Deferred output
is written once per thread, after drawing, instead of thousands of individual
stderr writes. The reader rejects unavailable/invalid clocks, dropped or
unpaired records, and changed authored workloads; unsupported is not zero work.
CPU accounting is OS-quantized and excludes preparation/driver/other workers.
Individual zero buckets are not evidence of free work, and the 100 ns unit is
not a claim of 100 ns clock resolution. The clock test observes busy work and
excludes sleep; its Android ARM64/API 26 syntax check also passes.

The preceding diagnostic-only PC was
`C295FFEBE89833430A67854694145DDC73984408DA2A559691BF5A7EEAFC111E`.
`small-stage-thread-clock-images-{d3d12,vulkan}-oct3` passes 30 exact complete
SBS images per backend on that checkpoint. Its complete loaded ABBA traces
have 6,134 ordinary model records per run, 4,058 after 60 warmup batches,
13,440,320 measured upload bytes in every variant, and identical authored
draw order/annotations. Model GPU intervals also include billboards not
covered by these ordinary-model CPU records. Both APIs show less GPU
preparation work in the single-pass variant, but executing CPU estimates are
coarse/mixed and late preparation stalls remain; it is not default/FPS rollout.
Raw results: `small-stage-thread-clock-profile-{d3d12,vulkan}-oct3/results.json`.

Current checkpoint loaded ABBA profiles complete on D3D12 and Vulkan:
`lazy-projection-init-profile-{d3d12,vulkan}-oct3/results.json`. Every variant
retains 6,134 ordinary model records, 4,058 after warmup and 13,440,320 uploaded
bytes. The single-pass experiment reduces measured GPU preparation work,
but CPU/frame timing is mixed and first-use stalls remain; it stays OFF.
These loaded measurements do not establish a causal cross-checkpoint or
sustained FPS gain. Other projects remain running.

The shipped separate-stage path passes 30 byte-identical complete images per
API against the preceding `C295FFEB...` checkpoint, across six scenarios:
`lazy-projection-init-images-{d3d12,vulkan}-oct3/results.json`. These renew the
default path, not the current experimental path's full-image acceptance.
Real owned-SR/factory checks pass six exact outputs plus resize, cancellation,
foreign-device and missing-runtime/re-arm guards. Unavailable SR preserves
36 exact mono images across Original/EX and both APIs:
`sr-lazy-projection-default-oct3/results.json`. Actual embedded mono K/M/K
passes 96 evaluations, 24 held uses and 120 once-only endings:
`dlss-lazy-projection-default-oct3/toggle.log`. All 29 Preview-OFF menu cases
and the visually inspected RENDERING/loading capture pass:
`plain-menu-lazy-projection-default-oct3/results.json`. Heavy enhancements,
SSAA and SR output do not enter scene rendering or change the plain display;
both Software-to-GPU neural-model transitions restore D3D12 directly.
All owned checks have ended and all seven protected artifact hashes match.
Physical Leia/Android, true secondary motion, broader finishing/
peak-memory, sustained SBS speed and packaging remain open. Ally deferred,
full goal active. Other compiler jobs remain running; no quiet-host assumption.
Release/settings/injector/APKs and device installations are untouched.

## October 3 — owned-input reuse alongside shared stereo sources

Preceding development PC: `42F277D06461D2BB735CD70A0AA5270C9CCF3C180A58697A2151E34012DD7D86`.
Core library: `D79CE7F35321A84D049ED50BA008A8E46D635720930E38AC26E37B989F3B256C`.
Stereo checker: `75173BAE164AA314A6763D4EA69FA1AC6DDA66AE42E776BECCF3F58B4394D496`.
Native owner checker: `5F539C9042CB9A1E2D4E733EC1B5F94DA297E3F96553100F6C5A8B3C3417C53F`.
The SR adapter/native owner library are unchanged from the preceding checkpoint.

Borrowing any stereo source buffer previously disabled the entire model's
upload memo. Owned material, texture and pose inputs could therefore be
uploaded again even when their bytes were unchanged. The memo now records
owned-versus-borrowed provenance per slot: borrowed bytes never seed owned
storage, returning a borrowed slot to owned storage forces its upload, and
unchanged owned slots remain reusable. GpuScene enables this exact-byte memo
within each command recording by default. It ends before submission or
cancellation, resets for a new recording/device, and retains at most 1 MiB
of CPU snapshot capacity per GpuModel. Billboard writes invalidate it.
There is no cross-frame, pointer-only or projected-image cache, extra submit,
readback, wait, quality reduction or sharing of eye-specific results.
`STARFOX_TEST_DUPLICATE_MODEL_UPLOADS=1` remains the same-binary reference.
The separate bounded small-model GPU-stage experiment still stays OFF.

Completed on these artifacts:

- `shared-owned-memo-components-oct3`: D3D12 and Vulkan each pass 144 new
  independent SoftwareRenderer images, with 61,068 owned bytes avoided.
  Native/matrix/Euler projection, 1x/2x/4x, borrowed-to-owned transitions,
  live pose/texel mutation, billboard overwrite, metadata changes, cold
  scopes, cancellation and recovery are covered. The complete existing
  stereo ownership, independent-eye, billboard, painter and MSAA suites pass.
- `shared-owned-memo-full-images-{d3d12,vulkan}-verified-oct3/results.json`:
  ten scenarios and 50 byte-identical complete images per API. Includes
  banked 3x terrain, water/rays/reflections/bloom, Venom motion blur, high MSAA,
  EX lava, billboards, joined/parallel stereo and mono water. Ordinary model
  accounting avoids 1,832,120 bytes and 6,844 copies per API over the logged
  runs. Parallel accounting separately avoids 8,016 bytes/32 copies in its
  first completed pair only; it is not extrapolated. Asteroid billboards
  deliberately bypass this memo and have unchanged all-borrowed accounting.
- Six rebuilt controller/stereo/simulation/DisplayXR CPU suites pass.
- Real owned-SR/factory checks again pass six exact BGRA outputs and failure,
  resize, foreign-device, cancellation and missing-runtime/re-arm guards.
- `sr-model-memo-default-oct3/results.json`: 36 exact unavailable-SR images.
- `dlss-model-memo-default-oct3/toggle.log`: real embedded mono K/M/K passes
  96 evaluations, 24 held previews and 120 once-only endings.
- `plain-menu-model-memo-default-oct3/results.json`: all 29 Preview-OFF cases
  and the visually inspected RENDERING/loading capture pass.

The first full-image runner stopped after matching the asteroid images
because it incorrectly required ordinary-model upload records for an
all-billboard scene. Its original logs remain in
`shared-owned-memo-full-images-d3d12-oct3`; the corrected runner requires the
actual all-borrowed source accounting and both fresh matrices pass. No image
tolerance or production rendering was changed to resolve that runner error.

Loaded ABBA diagnostics completed on both APIs in
`shared-owned-memo-profile-{d3d12,vulkan}-oct3/results.json`. Each of the
eight 180-pair runs has the same 6,134 authored model draws and 5,458 measured
post-warmup model intervals. Duplicate runs upload 23,677,048 bytes; memo runs
upload 20,775,312 bytes, reproducibly avoiding 2,901,736 bytes (12.2555%).
No per-draw stderr logging runs inside this timing workload: host records are
bounded and dumped afterward. Host cost sums still include cold setup/all
draws; frame and GPU distributions discard 60 warmup pairs. Other projects
remain running. Read these as loaded diagnostics, not isolated acceptance:

| Backend/trial, in order | Uploaded bytes | Frame work median/p95 (us) | Model GPU interval per pair (us) |
| --- | ---: | ---: | ---: |
| D3D12 duplicate 1 | 23,677,048 | 13,359 / 23,981 | 3,800.79 |
| D3D12 memo 1 | 20,775,312 | 8,710 / 16,329 | 2,362.85 |
| D3D12 memo 2 | 20,775,312 | 6,751 / 15,170 | 2,361.71 |
| D3D12 duplicate 2 | 23,677,048 | 7,023 / 12,168 | 2,361.79 |
| Vulkan duplicate 1 | 23,677,048 | 6,059 / 8,281 | 3,800.31 |
| Vulkan memo 1 | 20,775,312 | 5,313 / 7,833 | 3,764.87 |
| Vulkan memo 2 | 20,775,312 | 5,277 / 7,844 | 3,758.61 |
| Vulkan duplicate 2 | 23,677,048 | 5,317 / 7,969 | 3,819.90 |

The first/last controls drift materially. Comparing the later memo/control
runs, D3D12 upload-recording host sums are 328,076/396,369 us and Vulkan
38,030/41,736 us, but encoding/frame-tail results are mixed. GPU intervals
exclude scene preamble, native ray queues, CPU encoding and presentation.
The supported result is fewer exact input transfers with unchanged imagery,
not a claimed sustained FPS gain. The small-model stage remains OFF.
Full prior native SDK/TAA matrices are
historical, not renewed by this relink. Physical Leia/Android, true secondary
motion, broader finishing/peak-memory, sustained SBS speed and packaging
remain open. Ally deferred, full goal active. Other projects and all seven
protected release/settings/injector/APK hashes are unchanged.

## October 3 — bounded small-model stage (opt-in)

Preceding development PC: `3F01732C22DEA5915557A05659C60758466D77686F8B25998293F60BB2E95993`.
Native owner checker: `4E01AC000EBBAA46147609356DB18E91755253B4E24B709C3A6FD5C4711D0A65`.
Core library: `AE1D011494872D9F29D77A2095902C387B41761A2C298933E70312E1A0095A60`.
Small-stage/BSP checker: `57A476E49ADC298B87CC00EC52277774F51082182E049CA0D86357AAB8F4F9AE`.
Model checker: `3434A919FB24A88C0EB3C4B71998833672398DCAFB3BF6068882CEEEF5DDEA46`.
The native owner library and optional SR adapter remain unchanged from the
preceding checkpoint; the native owner executable was relinked, not yet renewed
by the full SDK/TAA matrices below.

The previous real 180-pair SBS host trace (`sbs-host-cost-current-oct3`) recorded
6,134 model draws: packing 38,029 us, preparation 68,959 us, upload recording
589,825 us and command encoding 1,452,615 us. These are summed host wall-clock
buckets under simultaneous checks/other compiler jobs, not GPU durations or
isolated/sustained FPS. They point to pass/command overhead in this scene, not
proof that all scenes are CPU-bound or that an optimization improves FPS.

`STARFOX_TEST_FUSED_SMALL_MODEL=1` combines continuous transform/projection,
source visibility and authored painter traversal into one resident 64-thread
group for at most 128 vertices and 128 visibility faces. It reuses the existing
continuous arithmetic without reducing geometry or sharing eye-specific
results. Larger/native/axis/destruction policies keep ordinary stages. The
default stays OFF; `STARFOX_TEST_SEPARATE_SMALL_MODEL` overrides the experiment.
The capture/benchmark helpers expose scoped switches and require an actual
single-pass marker. No CPU projection, readback or implicit submit is added.

Completed on the rebuilt artifacts:

- `small-model-components-oct3`: D3D12 and Vulkan on the RTX 5070 Ti each pass
  294 new cases, 1,287,552 exact separate-stage point/residual bytes, 40,236
  independent CPU visibility flags and 6,444 independent painter entries.
  FP32/Q15/Euler, both residual policies, counts 1/31/63/64/65/127/128, invalid
  poses, active cycles/shared subtrees, statuses 0..4, aliases/bounds, queued
  small/projected/generic switches and cancelled/recreated producers are covered.
  Existing 112 projected-BSP cases, 128 synthetic trees/four failure statuses
  and resident order-to-raster reuse also pass on each API.
- `small-model-real-images-oct3`: 64 real models and 768 images per GPU API,
  exact SoftwareRenderer coverage/palettes, normal error <=5.96046e-8 and
  depth error <=0.00012207. Actual small-stage markers are present. This
  independently covers rendered output, not only shared shader arithmetic.
- Six controller/stereo/simulation/DisplayXR CPU suites pass after relink.
- The unchanged real SR adapter/owned-weave checker again passes six exact
  outputs, resize/repeat, foreign-device rejection, failure restoration,
  cancellation and missing-runtime/re-arm. No physical vendor weave is tested.

- `small-model-full-sbs-{d3d12,vulkan}-oct3/results.json`: six scenarios and
  30 byte-identical complete SBS images per API, separate versus actual
  single-pass producers. Includes water/reflections/bloom, banked 3x terrain,
  Venom motion blur, high MSAA and EX 6-6 lava; five moving frames per scenario.
  Both real eye outputs and the candidate marker are required; no partial
  fallback pair is accepted. The D3D12 water frame was visually inspected.
- `sr-small-model-default-oct3/results.json`: 36 exact unavailable-SR images
  across Original/EX and D3D12/Vulkan on the rebuilt default route.
- `dlss-small-model-default-oct3/toggle.log`: actual embedded mono K/M/K,
  96 evaluations, 24 held previews and 120 once-only endings, no SDK failure.
- `plain-menu-small-model-default-oct3/results.json`: all 29 Preview-OFF
  cases and RENDERING/loading capture pass; preferences/injector log unchanged.

All owned runs ended normally. Current default settings are unchanged.
The preceding C41 SDK post-order matrix completed
216 cases/eight summaries; it is evidence for the preceding binary's real SDK
post-evaluation identity-copy diagnostic, not renewed current private neural
quality or current full SDK owner acceptance.

Physical Leia/Android, true secondary motion, broader finishing/peak-memory,
sustained SBS performance and packaging remain open; Ally deferred, full goal
active. Other projects, release/preferences/injector/APKs are untouched.

Loaded cost diagnostics, not a default rollout: two 180-pair D3D12 runs in
`small-model-profile-d3d12-{separate,single}-oct3` each retain 6,160 valid
draw intervals (5,458 post-warmup model samples; 4,058 phased model samples).
`draws.json` reports model preparation 640.43 -> 369.82 us per pair and
complete model intervals 2,726.75 -> 2,239.38 us per pair. These GPU intervals
exclude scene preamble, ray queues, CPU encoding and presentation. Host
encoding sums for 6,134 model draws instead rise 1,186,127 -> 1,260,365 us,
and frame-work median/p95 rise 4,402/9,379 -> 6,143/14,870 us. Upload bytes
remain exactly 23,677,048. Other compiler jobs and some menu checks overlapped;
this is mixed diagnostic evidence, not an isolated or sustained FPS gain.
The experiment therefore stays OFF by default. No full GPU/default acceptance
is inferred from the smaller preparation interval.

Diagnostic runner correction: the first generic timestamp summarizer rejected
the separate run because it requires both scene and effect streams; this run
intentionally requested scene/draw tracing only. The appropriate strict
per-draw summarizer completed on both original logs without changing them.

## October 3 — direct SR compatibility and raw liquid post order

Preceding development PC: `ADC89620F76EEBFC65FB40C354D3D7F7028A593449A329528D9B3AA0795AA252`.
Native owner checker: `C41C23E7E38E7C5FBEE500F0A5C983DDF3BA55C7E830AAD0161BEF872446C9A3`.
Native library: `344345D4E57BB94D72EB0CFF337B97488CD7B5968A5C8DCD8B8B439E8AA168B6`.
Optional SR adapter: `49B72BF97A16D5853321A54306FD815D2DFD013748FE671BA8A55B1D0B9A6327`.
SR checker: `7B3B8DAFE633FAFB805E1BB6ED9A2D9D84CC667C6CF84415F732A3284DBA9147`.

The direct SR Platform route complements DisplayXR. It uses app-owned BGRA
input/output textures, delay-loaded vendor dependencies, recognized-panel and
clone-display qualification, optional switchable-lens preference, retained
failure cleanup and one ordinary black settling frame after layout changes.
See `SR-PLATFORM.md`: this is parallel stereo plus tracked weaving, not a claim
of DisplayXR's located camera/frustum calibration. No vendor runtime is bundled.
The real D3D12 owned-weave fixture passes six exact BGRA outputs at two extents,
repeated/cancelled commands, foreign-renderer rejection and callback-failure
restoration. Its synthetic callback is not the vendor weaver. The real optional
adapter separately passes missing-runtime factory/re-arm checks.

The preceding native TAA liquid fix also removes deferred palette styles from
ray-material/environment packet bindings. Otherwise reflected/submerged
radiance could contain a style before the complete centre-grid post chain
applied it again. This checkpoint extends reconstruct-first to all traced SDK
water, including simple palette effects and the unstyled baseline. Raw optical
radiance is reconstructed before complete ordered post styles/finishing.

Completed on current artifacts:

- Six controller/stereo/simulation/DisplayXR CPU regression suites.
- `taa-sdk-shared-liquid-post-oct3/results.json`: 108 independent full-image
  liquid post-order cases per D3D12/Vulkan, four formats and three TAA qualities.
  Separate unstyled timelines, authored centre-plane ownership, submerged/dry
  actors, banked/moving eyes, exact protected ink, held/rejected/wait and actual
  queue cancellation remain covered without relaxed tolerances.
- `sr-platform-sdk-liquid-final-oct3/results.json`: 36 exact ordinary/SR-unavailable
  images across Original/EX and D3D12/Vulkan; saved preferences unchanged.
- `sr-platform-sdk-liquid-menu-oct3/results.json`: all 29 Preview-OFF cases,
  including six SR selections; the RENDERING/loading image was also viewed.
  Preferences and ReShade log unchanged. Loaded timings are not isolated FPS
  or performance/default acceptance while other projects remain running.
- `sdk-liquid-mono-model-cycle-oct3/toggle.log`: real embedded mono K/M/K,
  96 evaluations, 24 held uses and 120 once-only frame ends, no SDK warning/error.

The K/M SDK liquid post-order matrix completed successfully on its original
owned process: `sdk-liquid-full-post-raw-oct3/results.json`, exit zero,
216 cases and eight K/M/format summaries. The stronger SDK fixture first
reproduced a real mismatch
in `sdk-liquid-full-post-before-oct3` (model 0, quality 1, selection 0, frame 0,
eye 0, byte 188416: actual 127 versus expected 147). Preserve that failure.
That failed evidence is retained separately from the complete passing result.
The diagnostic calls the real SDK, then
copies its equal-size DLAA input to output on the actual command list to expose
the post-order contract. It does not independently reproduce a neural filter,
optical-water integrator or depth-finishing image. True secondary motion,
broader finishing/SDK/driver/AS peak VRAM, physical Leia/Android, sustained SBS
speed and packaging remain open; Ally is deferred, full goal active.

The previous TAA edge renewal on PC `9218CB51...` and checker `4CAB32A1...`
also completed: `taa-liquid-post-edge-regression-oct3/summary.json`, eight
four-format component groups plus 108 native owner edge cases per GPU API.
It is historical evidence, not renewed acceptance of this SDK policy change.
All evidence paths above are under `D:/SFE-validation`. Other projects remain
running and untouched; no isolated timing/FPS/default acceptance, release,
device install or saved preference/injector/APK change is authorized here.
All seven protected release/preferences/injector/APK hashes match after the
completed checks. The SDK matrix ended normally on its original owned process;
it was not restarted or killed for elapsed time.

## October 3 — physical liquid guides on the reconstructed panel

Historical development PC: `8D6D2226510D1EA8697BA83C74A1502F41BA9FE145DCE575D975235B024DC12C`.
Native owner checker: `0D0CB987864AE2E34FA7BBD9248B1FBA4070ECE6201C84E165BE0041773BD251`.
Native library: `1E1CA08067F02029AFA5C4F8910F4ADB0736930C2727B34038F80A93BEA836C7`.
SDK component checker: `7ED5322655CF62E75D96C51DCCF7F1C53124A517E750D95593277DA4BBA1CB14`.
Ray component checker: `FDC0510677895C7B6A583C2AEE81E4639A394D567037EDAD5CAF9934CA3AA391`.
CPU checker: `8099BC62305B5146372DDE1443BEC5B8CDBA49769694BB6E14DF1AF1C0A4FF11`.

The native SDK post path now evaluates visible water ownership, wave normals
and positive forward depth at the unjittered physical panel grid. Previously
that grid retained the primary submerged model surface: source-grid liquid
guides did not update the independent full-panel raster. This could apply
model styles or AO/DOF/scene/fog depth to the wrong physical surface.

The new optional MRT pass uses the retained ground plane, exact asymmetric eye
projection and affine view/world transform, source wave clock and the native
RT water normal field. It does not upscale source-grid labels/depth, retrace
reflections, project vertices on the CPU, read back images, submit or wait.
Nearer opaque models, protected/unknown owners, dry sky and AA alpha remain
untouched; water is classified as WORLD before the complete post chain. Its
normal/depth feed the existing depth-based finish. SDK raw history and current
secondary rejection remain independent. Only selected reconstructed RT water
allocates the extra full-panel owner/surface pair. The combined four-GiB guard
now includes old/new liquid pairs: panel allowance rounds 317 B/pixel UP to
320, source allowance remains 576. This is not complete peak VRAM/SDK/AS proof.

New source checks: twelve independently derived asymmetric/affine/wave-clock/
near/far cases per format/API, with visible water, nearer opaque geometry and
protected pixels; exact ownership/alpha, geometric normal/depth limits,
invalid inputs and cancelled retries. Actual K/M owners additionally select
drawn/world styles, contrast, bloom and AO/DOF with centre/SSAA/MSAA rasters,
checking stereo, opacity, exact held images and moving accepted history.
The rebuilt implementation passes `panel-water-components-compensated-oct3`:
96 distinct panel geometry cases across four formats and D3D12/Vulkan, plus
all source liquid MRT/style/AO/DOF regressions. A normalized float ray amplified
rounding near the horizon; the shader now uses the existing native axis
shader's compensated-float arithmetic for plane depth/position. No shaderFloat64
requirement, numerical tolerance relaxation, extra ray trace or CPU pixels.
The independent double oracle evaluates the exact single-precision authored
inputs, rather than a different pre-conversion double plane.

Preserved attempts: `panel-water-components-oct3` reported a stale error string;
the diagnostic attempt established that the fixture supplied an unconverted
OpenXR projection. Its native-eye correction then exposed real plane-depth
rounding at pixel 1648, case zero (`8006.837402` versus `8006.808105` after exact
float plane/projection inputs). Compensated GPU arithmetic fixed that mismatch
without changing the normal/depth tolerances or removing the near-horizon case.

The first analytic implementation (`9592FA14...` PC / `5EE646BA...` owner) passed
all four original SDK owner groups including the six new panel-liquid finishing
cases per format, sixteen full-HD cases and seventy-two sample-alignment cases
in `panel-water-sdk-owner-oct3`. The newest precision correction independently
completed the same whole run in `panel-water-compensated-sdk-owner-oct3` on
`0D0CB987...`: all four original groups, all 24 new panel-liquid cases, sixteen
full-HD cases and seventy-two sample-alignment cases, with no SDK warning/error.
All eight legacy native TAA routes also completed 296 cases on that checker.
Newest completed evidence:

- `panel-water-history-renewal-oct3`: CPU 6,570 assertions; both APIs' four-format
  local ray-history oracles; 112 genuine SDK ray/history cases and 152 existing
  SDK components. Exact current checker hashes appear above.
- `panel-water-mono-dlss-oct3`: normal embedded mono K/M/K, 96 actual evaluations
  and 24 exact held reuses. No force-availability override.
- `panel-water-plain-menu-oct3`: all 23 Original/EX Software/D3D12/Vulkan
  baseline/heavy/source-AA/model-toggle/navigation cases; plain UI identical,
  RENDERING presented, preview finishes, settings/injector log unchanged.
- `panel-water-sbs-{d3d12,vulkan}-oct3`: selected water-rays, banked-upscale and
  asteroids, five exact paired images each, 30 total. Asteroids source transfers
  remain 49,152 -> 3,072 bytes and 48 -> 3 copies. The previous eight-case/80-image
  checkpoint is not a new complete SBS renewal. No sustained speed claim.
- Current SBS water and preview RENDERING captures visually inspected.
- `panel-water-existing-taa-oct3`: 296 old raw/edge/pattern/local-layer cases
  across D3D12/Vulkan and four formats, complete. This does not establish the
  still-missing native TAA centre-liquid integration.
- `panel-water-compensated-sdk-owner-oct3`: genuine K/M owner lifecycle, selected
  liquid post styles/depth, full-HD raw/post/held images and sample alignment,
  complete on the current checker. The six liquid cases per format are owner
  lifecycle/opacity/stereo smoke; independent geometry is established by the
  component oracle, not an independent full owner AO/DOF image oracle.

All owned build/check processes ended. All seven protected hashes match:
current/release settings, release executable,
ReShade settings/log, original APK and separately staged Android candidate.
Shader stamp gate and focused tracked diff check pass. No release/device install,
saved preference, injector, APK, WSL or other-project mutation.

The previous `DCDF4F36...` checkpoint completed all four original SDK owner
groups, 296 legacy native TAA cases, 80 exact D3D12/Vulkan SBS images, mono
K/M/K and all 23 preview-off/menu/loading checks. Its corrected checker
`C5FADDAD...` also completed 128 actual SDK pattern/liquid cases: 4,352 real
evaluations, 4,352 held reuses, 119,721,600 guide pixels and 440,138,880 exact
protected/opacity bytes. Fresh 112 ray and 152 old component cases pass on
that checker. The preserved failing pattern attempt expected a liquid hit on
an omitted class-zero raster sample to replace depth (pixel 82,5, frame 2).
The independent oracle now distinguishes input hits from actual visible
consumption, retaining those unused hits as negative controls; no shader or
numerical tolerance was relaxed. All seven protected hashes matched before
this new rebuild. Its premature build attempt was rejected by the stale-shader
gate before compilation; the same generation process was allowed to finish.

Physical Leia/Android, true secondary/liquid temporal correspondence, broader
finishing and peak-memory coverage, sustained SBS speed and final packaging
remain open. Native TAA still needs the corresponding centre-liquid integration;
the new analytic panel merge is currently in the native SDK post path only.
ADB currently reports no attached device; generic Windows monitor enumeration
does not prove a physical Leia display. Ally deferred. Other projects remain
untouched. Full goal active.

## October 3 — actual secondary RGB and accepted dry-history footprints

Previous development PC: `DCDF4F362EB44ACA61BD6D93E13E5AB71341CDFFE3718C976EDC1E5399162D90`.
Native owner checker: `D3F5E9B74FF82750F5DAF705AE14503FD9D4EC2A945D590867BB17EA16FBE7BF`.
Native library: `755E26FD82A7B94BF19D5A41AD05D2BA0EA448CDDACEB01D6100DB83E5BAF16E`.
SDK component checker: `B0B0603334D3A6214784B99BB94A1516369DD69F706A8A8C585C1FB058A98BB0`.
TAA component checker: `89082538548F53862BA9294850BFAF87913211B26A07B16D1B14745FB9EFD4A0`.

Native TAA and K/M SDK inputs now match the ray compositor's actual RGB
consumers: liquid sentinel 253; analytic floor sentinel 254 only on a floor
receiver; model sentinel 255 only with a nonzero selected reflection strength
and receiver mix. Protected artwork and unused reflection words are not
mistaken for reflected pixels. RGBA-only native outputs are supported without
inventing a liquid aux plane; malformed nonempty aux layouts still fail.
Liquid RGB stays reactive even when its auxiliary normal/depth is invalid;
only the replacement depth requires a validated physical surface.

Each actual MSAA/SSAA ray sample uses its matching resident raster ownership,
not the centre or an averaged tag. The SDK rejects mismatched sample grids.
TAA stores secondary pixels as ineligible. SDK reuses the existing R32
double phase/witness targets (8 B/source pixel) for ray-selected eligibility
even without a patterned style. A later dry pixel must match the complete
positive previous bilinear footprint before reusing primary motion. Failed
submissions retain the existing conservative reset/transaction rules. Valid
local ray rejection no longer discards both entire eye histories. No extra
render pass, CPU projection, GPU readback or synchronization is introduced.
This is correct local rejection, NOT reconstructed secondary-ray motion or
knowledge of the SDK's private neural filter. That broader work remains open.

Completed evidence:

- `D:/SFE-validation/secondary-history-taa-final-oct3/results.json`: CPU 6,570
  assertions; D3D12/Vulkan four-format independent CPU history oracles,
  4,452,096 compared bytes, consumed/unconsumed reflections, reflections OFF,
  three output layouts, wet/reflected-to-dry same-depth transitions, local
  masks, invalid aux normals, malformed inputs, opacity and cancellation.
- `D:/SFE-validation/secondary-history-sdk-coherent-oct3/results.json`: 112
  genuine K/M ray/coverage/history cases across four formats, RGBA-only output,
  model reflections ON/OFF, source 1x/2x/3x/4x and readable MSAA 2/4/8,
  1,120 SDK evaluations, 672 held reuses, 13,762,560 scalar guide pixels,
  38,599,680 exact protected/opacity bytes. Same-depth dry recovery checks
  independently derive the full previous footprint and require no global
  reset. The 152 existing SDK component cases also pass (488 evaluations,
  912 held uses); no history/filter tolerances were relaxed.
- Current actual owner ray alignment: 72 cases/four formats pass. Current
  full-HD owner: 16 genuine two-eye 1080p cases/four formats pass.

The full owner SDK regressions, 296 original native TAA owner cases, 80 exact
SBS comparisons and mono/menu/loading renewal completed on this checkpoint.
Their completed records are under `secondary-history-sdk-owner-oct3`,
`secondary-history-existing-taa-oct3`, `secondary-history-sbs-{d3d12,vulkan}-oct3`,
`secondary-history-mono-dlss-oct3` and `secondary-history-plain-menu-oct3`.
Two earlier SDK fixture failures are preserved under
`secondary-history-first-oct3` (deliberate malformed preflights forced reset
immediately before a purported valid warm sequence) and
`secondary-history-sdk-oct3` (alternating labels left no coherent region for
fractional dry recovery). The fixture now retains all cold invalid-input
checks and includes coherent AND mixed regions, without weakening rejection.

Release/settings/injector/APKs remained unchanged; all seven protected hashes
matched. Other projects keep running untouched. No sustained FPS claim.
Physical Leia/Android, true secondary/liquid correspondence, broader finishing
and peak-memory coverage, SBS speed and final packaging remain open. Ally is
deferred at the user's request; full goal remains active.

## October 3 — SDK outlines, sample-aligned rays and retained image layouts

Previous development PC: `8FCE5087AB488FED24E50AE9D93BFEACF73154FDAF9B99BE505E4DC0E6895CED`.
Native checker: `83290328CD0FF2B7ED9220CDA567AA39E93D5E0AC4D322CF10D40E4EDEF283DF`.
Native library: `AB49E6B49DD225B6F23B13B872FC49BA7AFF46D8E91A2E6377C3B58226624AD3`.

Thirteen drawn/outline-only SDK styles now follow raw reconstruction, with their
complete authored chain at panel resolution. SDK has pattern-phase guides but
no native edge witnesses; native TAA retains its separately witnessed route.
MSAA ray cameras now include source SDK jitter before hardware sample offsets,
matching raster/geometry. No invented secondary reflected flow or relaxed
liquid/shutter guard is used. The assertion counter is now 64-bit.

The four-GiB combined working-image guard charges max(576 B/source pixel,
selected MSAA/shutter/liquid/fog bytes), 288 B/panel pixel and 64 MiB. Panel
resources total 277 B rounded UP: centre/output guides, transactional SDK/post
targets, scratch, double float histories, bloom, fog and exposure tiles. It
admits genuine two-eye 1080p DLAA and rejects invalid/UHD/heavy/overflowing
layouts. It is not free VRAM, SDK/driver/AS storage or a complete peak-memory
guarantee. AA replacement also charges ACTUAL old attachments, histories,
meters, native ray capacity and SDL copy/download images, including retained
larger liquid outputs and alignment. Imported aliases are not charged twice;
failed-import old allocations are retained and charged. CPU metadata adds no
render pass, readback or GPU synchronization. Once source-layout work drains
and replacements allocate, obsolete ray/trail/meter/bloom/depth/scene/fog/AA
caches are released rather than only clearing readiness flags.

Preceding PC `306E7681...` / checker `97E9924C...` completes
`D:/SFE-validation/sdk-outlines-memory-final-oct3/results.json`: 2,688 actual
SDK post cases across 48 selections, 14 K/M/source-AA layouts and four formats,
5,284,823,040 compared bytes, 148,087,296 exact protected bytes, 10,752 held
frames, 56 rejected/wait recoveries and 1,736 static/448 animated clock checks.
Separately evaluated unstyled SDK timelines feed independent CPU effects/
history/exposure and centre-label oracles. Exact linear edge checks and existing
sRGB limits are unchanged. That artifact also passes original SDK regressions,
72 sample-aligned native-ray cases and 16 genuine 1080p panel cases; expanded
TAA (816 cases) and all eight old TAA routes (296) pass on D3D12/Vulkan. These
preceding broad effects/TAA matrices are not claimed as renewed on today's hash.

Current `retained-image-layout-final-oct3/results.json` completes CPU runtime
(6,570), twelve real resized native/SDL imports checked against actual D3D
allocation descriptions, and both APIs' four-format float-history/exposure
pixel oracles. Retained resize images, two histories/stops, packed histogram
padding, cancellation/release and zero unused storage are checked. Current
SDK renewal completes `retained-image-sdk-final-oct3/results.json`: 72
sample-aligned ray cases, 16 full-HD cases and all four original SDK quality/
raster/liquid/palette/lifetime matrices. The ray oracle
does genuine SDK evaluation before an identity diagnostic output override;
independently shifted single-sample XR images test actual production MSAA/DXR,
including positive visible ray shading, not opaque neural-filter parity.

Current mono K/M/K passes 96 evaluations, 24 retained-preview uses and 120
once-only pair endings; all 23 plain-menu cases and the inspected RENDERING
image pass. Evidence: `retained-image-mono-dlss-oct3` and
`retained-image-plain-menu-oct3` under `D:/SFE-validation`. Preview-OFF has no
hidden scene/effect/SDK work or effect-dependent artwork. Ordinary SBS renewal
completes in `retained-image-sbs-sprites-{d3d12,vulkan}-oct3/results.json`:
eight cases/five exact full-SBS images per API, 80 comparisons in total, including
water/rays, banked upscaling, Venom shutter, MSAA, Asteroids and joined/parallel
encoding. Asteroids independently records 49,152 -> 3,072 bytes and 48 -> 3
copies on each API. Water and Asteroids captures were visually inspected.
Its earlier attempt
passed five cases then rejected Asteroids' stale zero-helper-upload assumption:
billboard texels now use this pool too. Actual duplicate/shared markers were
49,152/49,152 vs 49,152/3,072 bytes. The corrected manifest requires positive
reduction and unchanged exact-image checks, not a waived missing marker.

Earlier failed fixture attempts are preserved: `sdk-outlines-ray-samples-oct3`
skipped genuine SDK retirement; `sdk-outlines-ray-samples-lifecycle-oct3` used
one invalid test-only slot. Genuine evaluation before identity copying and a
legal chain fixed the fixtures without relaxing production validation/tolerances.

All owned checks ended, current binary hashes match and all seven protected
release/settings/injector/APK hashes remain unchanged. Physical Leia/Android,
wider finishing/secondary/liquid and peak-memory auditing, and sustained SBS
speed remain open. Release/settings/injector/APKs unchanged, other jobs left
running; no quiet-host retry, FPS/default/backend-ranking claim. Ally deferred,
full goal active. See exact completed/live records above rather than older heads.

## October 3 — SDK reconstruction before full-panel post effects

Previous development PC:
`6FFEDA4377B6ED9D91B922A67A86302EBDB9E43A8B78AD28029447D5B81188C1`.
Native owner checker:
`C3138DB4398E6461ADE336C6F2F622BDF37D3CCF634CE580754E36C9C428AE40`.

Native K/M DLSS now reconstructs geometric/ray/material radiance before the
complete three-slot complex style chain and selected finishing. Effects run
at the actual XR panel extent, not the SDK's smaller source extent. Genuine
source FXAA/SSAA/MSAA remain separate; another TAA history is not stacked on
DLSS. Full-panel centre ownership, optional surfaces and raw/scratch targets
are independent of source-grid guides. Finishing uses an explicit grid rather
than changing the owner's extent/settings. Fog uses that grid's projection.
Late HUD/portraits/menu artwork remains outside reconstruction and effects.
Secondary-reflection and physical-shutter guards, local liquid/current-colour
bias, independent viewports 100/101 and once-only pair cleanup remain.
Palette/pattern and separately guarded edge-only chains retain their existing
source route; this is not a claim of opaque neural-filter or secondary-flow parity.

Selected full-panel targets allocate transactionally only after pending work
retires. The combined conservative limit charges 576 B/source pixel plus
576 B/panel pixel and 64 MiB for both eyes together, capped at four GiB;
individual panels also retain the 64-megapixel/16,384-axis limit. This accounts
for retained finishing/history/shutter images, not a promise of free VRAM or
driver/acceleration-structure storage. Source MSAA no longer releases the
panel-sized histories every frame. No production readback, additional
submission/wait, replacement filter or rendering-quality default is introduced.

Static multi-tap/coordinate styles and static global kernels no longer consume
an unused host clock. The eight authored animated FX, trails, exposure/phosphor
and global Heat Haze/Film Grain remain time-driven. CPU policy checks cover
every authored ID/intensity plus all thirteen global selections/qualities.
Pending persistence/exposure cancellation is now distinct from explicit reset:
rejecting a presentation retains accepted colour/stops/timing until a genuine
accepted replacement. This fixes a real rejected-frame recovery jump exposed
by the combined temporal oracle; queued resources still retire before reuse.

`D:/SFE-validation/sdk-post-taa-accepted-history-oct3/results.json` passes both
APIs: 816 owner cases across 34 selections, three qualities and four formats.
Independent CPU styles, bloom, trails/phosphor and exposure consume separately
reconstructed raw timelines and centre labels. Alpha/ink remain exact; existing
linear/sRGB/combined numerical limits are unchanged. Static and animated clocks,
held frames, rejected presentations and image/GPU waits pass.
`D:/SFE-validation/sdk-post-existing-taa-oct3/results.json` passes all eight old
raw/edge/pattern/local-layer owner routes (296 cases), including their independent
motion/history oracles and cancellation/retirement. CPU runtime passes 5,999
assertions. Final mono K/M/K passes 96 evaluations, 24 retained-preview uses and
120 completed/attempted pair cleanups; all 23 plain-menu cases and the viewed
RENDERING image pass on this PC hash. Evidence: `sdk-post-mono-dlss-oct3` and
`sdk-post-plain-menu-oct3` under `D:/SFE-validation`.

`D:/SFE-validation/sdk-post-accepted-history-oct3/results.json` completes both
real SDK suites. The new full-image oracle passes 1,904 cases: 34 selections,
four qualities for each K/M model, six additional K/M source-FXAA/SSAA/MSAA
layouts, and four linear/sRGB RGBA/BGRA formats. Separately evaluated unstyled
SDK timelines feed independent CPU styles/bloom/trails/phosphor/exposure;
independent centre labels and final overlays prevent sharing private history
or accidentally treating post-ray UI as an earlier neighbour. It compares
3,743,416,320 bytes, including 104,895,168 exact protected bytes, 7,616 held
frames, 56 rejected/wait recoveries, 952 static and 448 animated clock checks.
The original four-format SDK suite also passes: 160 quality/AA/ray cases,
72 finite-floor cases, 600 palette cases, 192 mixed/local-post cases and 48
water/lava cases, plus later-eye failure/reset, exactly-once frame-end retry,
real GPU queue stalls, retained images and nonblocking close/disconnect.
These are application ordering/lifetime oracles, not an emulation or a proof
of the private neural algorithm. The generic 32-bit assertion display wraps
in the large new SDK run; per-format byte/case counters and explicit outcomes
are used instead, and no wrapped assertion total is claimed.
The fixture uses the integrity-verified
`build/dlss-embedded-stage/dlss` package actually embedded by `starfox_dlss.rc`.
The stale loose `build/current/dlss` package has no configure-v2 export and is
NOT the normal embedded app's SDK. The first explicit-directory attempt failed
on that missing export, before rendering. Another fixture compared a genuine
post-rejection SDK reset against uninterrupted neural history; moving rejection
after the compared timeline corrects lifecycle equivalence without relaxing
pixel tolerances. `sdk-post-full-panel-oct3`/`sdk-post-taa-full-panel-oct3` retain
the actual temporal recovery failure that prompted the cancellation fix.

All owned checks ended and all seven protected hashes still match.
Physical Leia/Android, broader finishing/edge/secondary/liquid correspondence
and sustained SBS speed remain open. Ordinary SBS/model matrices are not renewed
by these native tests. No release/device install, preference/injector/APK change,
isolated retry, other-job intervention or FPS/default acceptance. Ally remains
deferred and the FULL goal stays active.

## October 3 — native TAA reconstructs before multi-source effects

Previous development PC:
`90224D33D2C5A602713B49E467F771480638A2029B983F207D602233B8C1FD51`.
Native owner checker:
`EF71018221466D2B06F63A0CB3B7012C56090BF42757235BD3356C830AB3FFB4`.

Native TAA no longer feeds neighbourhood averages or coordinate/animated warps
to history with the primary fragment's rigid motion. It reconstructs the real
geometric/ray/material source first, then applies the COMPLETE ordered style
chain to the unjittered eye using independently rasterized centre ownership.
Depth, scene FX, contrast/chromatic, global enhancements, bloom and fog also
select this reconstruction-first ordering before trails/exposure/phosphor.
The ordinary pointwise/pattern/edge-only chains retain their witnessed route.
The existing accepted-frame transaction, wet-sample exclusion, conservative
secondary-reflection guard and physical-blur guard remain. No post-effect colour
is written into this raw history. The two eyes do not share colour or history.
No effect is removed, recoloured or substituted, no raw motion is assigned to a
warped pixel, and no production readback/submit/wait or new shader is added.
The existing scratch/centre allocations are reused; complex chains do not need
the earlier separate styled-centre/edge-atlas composition.

`D:/SFE-validation/reconstructed-post-legal-slots-oct3/results.json` completes
744 native owner cases: each D3D12/Vulkan route has 31 selections x three TAA
qualities x four RGBA/BGRA linear/sRGB formats. The selections cover all five
multi-tap styles, twelve static transforms, eight animated FX, three ordered
mixed chains and three finishing combinations (including pure finishing).
Each case has four changing presentations and both geometric eyes. Independent
CPU effects consume a separately rendered UNSTYLED TAA timeline and independently
rasterized centre labels, not private owner colour/history/ownership. Final
post-ray artwork is referenced separately, not used as an earlier neighbour.
Across both APIs the oracle checks 219,414,528 bytes and 6,151,392 exact protected
bytes, plus 2,976 held presentations and 24 rejected/wait recoveries. Linear
single static selections are exact; sRGB, animated and combined selections
allow at most two RGB codes; alpha and protected artwork are always exact.

Two failed fixture attempts are retained, not counted as acceptance:
`reconstructed-post-oct3` incorrectly included final post-ray UI in the earlier
neighbour source; `reconstructed-post-overlay-fixed-oct3` put Heat Wake and
Gravitational Lens in Special FX slots rather than their legal manipulation
slots. Separating the independently rendered overlay and correcting those
fixture fields fixes the references; numerical tolerances were not relaxed.

`D:/SFE-validation/reconstructed-post-regressions-oct3/results.json` completes
all eight old native-owner routes: D3D12/Vulkan raw (4 cases each), edge (108),
pattern (20), local-layer/liquid (16). These renew the independent rigid
motion/resolve/presentation oracles, all qualities and four formats, recycled
generations/morph/cuts, exact centre ink, held/OFF, rejected frames, actual GPU
stalls, image waits and nonblocking retirement. CPU runtime tests pass 5,960
assertions; both new PowerShell helpers parse.

Final mono K/M/K passes 96 SDK evaluations, 24 retained-preview uses and the
actual `completed=120 attempts=120` lifecycle summary without SDK warnings.
An initial ad-hoc summary regex mistakenly expected parentheses around those
numbers; the actual log was inspected and the corrected summary validated,
without rerunning the SDK. All 23 plain-menu cases and the separately viewed
`RENDERING...` loading image pass on this PC hash. Evidence:
`D:/SFE-validation/reconstructed-post-dlss-oct3/toggle.log` and
`D:/SFE-validation/reconstructed-post-menu-oct3/results.json`.

This is actual native GPU/owner evidence with mocked XR dispatch/compositor,
not physical Leia or a renewed native SDK matrix. Native DLSS reconstruction
still uses its previous ordering/guards; it needs its own panel-resolution
post chain. Secondary/refraction correspondence, centre liquid guides,
static complex-style clock classification, further finishing combinations,
physical Leia/Android and sustained SBS performance remain open. Ordinary
SBS/model/image matrices are not renewed by these native tests; the previous
paired upload implementation remains present. No new FPS/default acceptance,
isolated retry, other-project intervention, release/install or saved-setting
change. All owned checks ended; all seven protected hashes match. Ally remains
deferred and the FULL goal stays active.

## October 3 — paired billboard sources and broader span-cost evidence

Previous development PC:
`ABA7D0C17894E1F440F5F1510D47CDE0084789F43220D9A8A1D0831E2FCE08A4`.
Ordinary stereo can now share the selected immutable whole-object sprite
texture between eyes and repeated instances in the same recording. It never
shares eye position, depth, palette, material, output or history. Animated
texture mismatch and unready packets use ordinary full-quality uploads.
Texture identities expire after encoding; every later pair/retry uploads its
current bytes, including reused addresses. Four-byte padding stays inside the
existing 8 MiB source/8 MiB transfer/2,048-handle bounds. Source and transfer
backings cycle before binding either eye; joined, split and parallel submission
retain their existing ownership order. No extra submit, wait, readback, CPU
projection or screen-image reuse is added. The stale pre-game MSAA/SBS
"MONO REQUIRED" label is also corrected; native MSAA behavior is unchanged.

NVIDIA D3D12, NVIDIA Vulkan and Intel Vulkan each pass 128 independent Software
sprite images, with exact ordinary/shared packed pixels and separate depth.
Coverage includes 1/2/4x, own eye/palette/depth, live same-address texel changes,
animation mismatch, unready recovery, bounded/padded uploads, cancellation,
malformed payload and eight changing-eye images recorded before submission.
All three full stereo suites pass the original 45-image immutable geometry
oracle, joined/parallel retained ownership, native MSAA and sparse/motion
fixtures. Both NVIDIA real-model suites pass 64 models/768 images and the
changing queued 12-image scene oracle. Runtime-input ordering/fault tests and
non-SDL source syntax pass. Stereo checker:
`554268C9D6331626A5EF1685D4651C44EF3D14E2DBF78A38EA446F62AA95F941`;
model checker:
`AC8C8FFDBD824287E79D95551F1ADF1B8DB39428EABE99F9775DCFA54B3CBB49`.
Evidence: `stereo-billboard-components-oct3/results.json`,
`stereo-billboard-full-suite-oct3/results.json` and
`stereo-billboard-{models,queued}-{direct3d12,vulkan}-oct3.log` under
`D:/SFE-validation`.

The actual-game integration matrix completes 22 reference/shared cases and
66 exact complete-image comparisons (frames 8/16 and final aliases): nine cases
on each NVIDIA API, plus four on Intel Vulkan. This covers billboard-only
Asteroids, Corneria 4x, water/lava with resident rays/reflections, EX combined
rows, joined/parallel encoding, actual 4x native MSAA on all three routes, and
incomplete-left-eye cancellation/mono recovery on both NVIDIA APIs. Every
ordinary run proves 16 native pairs, direct eye output, positive actual shared
inputs, reduced pair transfer/copy demand and bounded pool storage. Eight
completed ray logs were separately verified for actual hardware-shadow GPU
casters, resident reflection scenes, 16 native pairs and no CPU caster/replay
fallback; a future-run helper assertion was also added. Shader selection stays
full-reference by default. Asteroids transfers 49,152 -> 3,072 bytes and
48 -> 3 copies on all three actual routes. This isolates the new sprite sharing
in a scene that had no polygon-source pool work; polygon cases also retain the
already-existing immutable geometry sharing. These are source-transfer
accounting, not whole-frame or sustained FPS claims. Evidence:
`stereo-source-integration-oct3/results.json` and retained images/raw logs.

Normal mono DLSS K/M/K passes 96 evaluations, 24 retained-preview uses and 120
once-only frame ends with no SDK warnings/errors. All 23 plain-menu cases plus
the separately captured RENDERING/loading indicator pass on this final PC hash,
including both models' Software-to-D3D12 restoration and no hidden Preview-OFF
scene/effect/SDK work. Evidence: `stereo-billboard-dlss-oct3/toggle.log` and
`stereo-billboard-menu-oct3/results.json`. All nine shader-helper tests and
updated capture/benchmark/validation script parsing pass. An initial ad-hoc
summary searched a nonexistent per-frame log label and returned zero; the
actual completed/attempted lifecycle summaries were then independently checked
as 120/120, matching the successful helper. No GPU tests were repeated for that
summary mistake.

The diagnostic predecessor PC
`B0CD7F7A3679741D4FE35BB7F04F049CCA8F050AFB30056E4A3AAE07D857D640`
passes 144 exact full-SBS reference/candidate/override comparisons across
eight real stage/style/scale workloads and all three actual GPU/API routes.
Polygon cases prove real span submission, EX repeated/sparse/combined styles
prove activation throughout the workload, and Asteroids is an explicitly
billboard-only negative control, not polygon-shader coverage. Evidence:
`cooperative-broad-parity-verified-oct3/results.json`.

All 48 broader short loaded GPU-cost runs finish: eight workloads, both span
writers and three routes, 120 pairs/60 warmup per run. NVIDIA's selected span
intervals are lower in all seven polygon workloads on both APIs. Intel is
mixed, including higher candidate intervals in lava and combined-row cases;
other phases and the unchanged billboard control vary as well. The cooperative
writer therefore remains opt-in, not a portable default or sustained FPS
claim. Other projects are untouched; no quiet-host gate/retry. A checker-only
CPU compilation overlapped some resumed NVIDIA samples and is part of that
uncontrolled host load. Evidence:
`cooperative-broad-cost-verified-oct3/results.json` and original raw logs.
The original batch's ten completed records were independently re-summarized
and revalidated before resuming only unfinished cases. The initial parity
runner incorrectly assumed polygon work in Asteroids; the initial cost runner
rejected valid CRLF style-count records. Both partial attempts are retained,
not counted as complete batches; the corrected checks keep coverage thresholds.
An initial summary-only PowerShell pipeline syntax error did not rerun or
invalidate GPU diagnostics.

All owned checks are terminal and no owned game/model/stereo process remains.
All seven protected release/settings/injector/original-APK/candidate-APK hashes
match their recorded values. Physical Leia/Android, wider native/private-SDK
effects correspondence and sustained SBS speed remain open, Ally deferred and
full goal active. No release, installation, saved-setting or injector change;
Android candidate unchanged.
The source-sharing optimization adds no effects or calibrated correspondence.
Older checkpoints below describe earlier executables.

## October 3 — cooperative row writing (correctness and loaded-cost checkpoint)

Previous development PC:
`030619084EA0C15AAD643100F3AFA1787AECF3DA1FDD38674027DE00C16BFC41`.
The opt-in `STARFOX_TEST_COOPERATIVE_SPAN_TRACE` keeps the exact serial edge/
UV stepping in one leader and distributes bounded 32-row command writes over
the group. Shared storage is bounded to 32 row inputs and two counters (backend
alignment may pad the scalar payload); storage ABI, painter ordering, coverage
and materials are unchanged. Repeated-row accumulations, lines,
sprites and rejected descriptors retain the original serial helper. Initial
UAV clears are device-barrier ordered before live writers; shared inputs are
group-barrier ordered before reads and reuse. No new GPU buffer, pass, submit,
fence, readback or CPU geometry is added. Full-tracer override wins; the lazy
experiment is selectable only on executed SPIRV/DXIL APIs, not unexecuted Metal.
Ordinary launches still use the original tracer.

NVIDIA D3D12/Vulkan and Intel Vulkan each pass the 384 reference/candidate/
override recycled-buffer, clear-policy, style, scale and native/MSAA cases:
41,488 live commands, 4,546,032 poisoned empty slots and 7,343,052,288 checked
command/colour/surface/mask bytes per route. New real-host dispatch oracles
add nine cases at 65,534/65,535/65,536 polygon slots per route, including 18
independent live sentinels and 56,622,240 fully checked poisoned command bytes.
This exercises the second dispatch row and surplus-group guard, not just a
formula or direct shader invocation. All three independent full CPU clip/span
oracles pass; both NVIDIA APIs' real-model, queued changing-input, full stereo/
MSAA/lifetime suites and runtime-input checks pass. Final clipping checker:
`DA41DEE05099EE16386604965DBEF34765EC411BCEEC53949109DD86660EF801`.

Both full-game matrices complete eight cases each / 160 exact complete-SBS
candidate/override image comparisons on this PC hash: ordinary terrain,
reflective water/rays/sky/bloom, banked upscale, Venom motion, MSAA, parallel,
joined and native EX menu. Actual shader selection, full override and direct
eye outputs are checked, with no CPU replay or changed preferences. Source
freshness, generated Metal bindings, nine shader-helper cases, script parsing
and non-SDL C++ syntax pass; this does not prove Apple compiler/device behavior.
Evidence uses `cooperative-spans-*-verified2-oct3.log` and
`cooperative-spans-native-{direct3d12,vulkan}-oct3/results.json` under
`D:/SFE-validation`. The initial runner treated a present low-power flag with
value `0` as enabled; those successful Intel runs are not counted as NVIDIA.
An explicit Windows PowerShell 5 invocation then failed on native stderr before
completion; its evidence is preserved. The final PowerShell 7 runner verifies
actual adapter names and all routes complete.

Six loaded GPU-cost diagnostics complete, each with 180 ordinary Corneria 1x
SBS pairs, 60 warmup pairs, 6,907 measured draw samples and 6,284 model samples.
Actual adapter/API, shader choice and complete query ownership are verified.
Selected model span-writing and complete-scene intervals, full/cooperative:

| Actual route | Spans, us/pair | Complete scene, us/pair |
| --- | --- | --- |
| NVIDIA D3D12 | 643.73 / 263.94 | 4123.74 / 3473.55 |
| NVIDIA Vulkan | 867.41 / 479.24 | 5839.00 / 5756.77 |
| Intel Vulkan | 2578.69 / 1795.30 | 19210.37 / 17250.47 |

The selected component is lower on all three routes, unlike the earlier UV-only
experiment. This is encouraging structural evidence, not an isolated A/B,
normalized GPU-clock comparison, sustained FPS result, backend ranking or
portable-default acceptance. Other phase/host costs vary (especially Vulkan
whole-frame CPU timing), and this cost matrix covers one stage/scale only.
The next performance step is broader stage/scale coverage, including rejection-
heavy, textured/small-face, repeated-row and high-upscale workloads, before
ordinary/default adoption. Evidence:
`cooperative-spans-loaded-diagnosis-oct3/results.json` and its phase/draw logs.
Normal mono DLSS K/M/K passes 96 evaluations, 24 retained-preview uses and 120
once-only frame ends without SDK errors/warnings. All 23 plain-menu cases plus
RENDERING/loading pass, including both models' Software-to-D3D12 transitions.
Preview OFF has no hidden scene/effect/SDK work and unchanged artwork/settings.
The initial renewal wrapper incorrectly checked native `$LASTEXITCODE` after
a successful PowerShell script (which validates its own child exit/status).
Its outer failure is preserved; it is not a DLSS runtime failure. The completed
helper's actual 96/24/120 records were independently inspected, and the plain-
menu script was then executed without that invalid wrapper condition.
Evidence: `cooperative-spans-dlss-oct3/toggle.log` and
`cooperative-spans-menu-oct3/results.json`.
All owned checks ended. Protected release EXE, both preferences, ReShade files,
original APK and separate Android candidate retain their recorded hashes.
Other projects are untouched; no quiet-host
retry, release/device installation or saved-setting change. Physical Leia/
Android, broader native/private-SDK effects correspondence and sustained SBS
speed remain open, Ally deferred and the full goal active. ADB is empty and the
separate Android candidate remains `5EA2FFB4...`. Older checkpoints below are
historical, not acceptance of this new executable.

## October 3 — colour-only span tracer (experimental checkpoint)

Previous development PC:
`5B03564C18C567C806286EC3C8FBF21D6ED01726D765BE93C6E9152FD55E53FF`.
The opt-in `STARFOX_TEST_COLOUR_SPAN_TRACE` shader skips unused UV reads,
segment interpolation and per-row UV advancement only for untextured polygon
spans. Geometric traversal, reciprocal/word arithmetic, winding, material bytes,
wireframe/sparse/repeated-row/wave coverage, masks and all textured/line/sprite
paths retain their reference behavior. It adds no pass, buffer, submit, fence,
readback or CPU geometry work. `STARFOX_TEST_FULL_SPAN_TRACE` takes precedence.
The optional pipeline is lazy and selectable only on executed SPIRV/DXIL APIs;
ordinary launches still use the original tracer. Generated MSL passes source/
binding checks; no Apple compiler or Metal device was executed, and no Metal
runtime acceptance or new default is claimed.

Each of NVIDIA D3D12/Vulkan and Intel Vulkan passes 384 paired fixture cases
across four clear policies, scales 1/2/4, native/MSAA masks, poisoned recycled
storage, mixed textures, arbitrary wrapped solid UVs, wireframes and wobble/wave
flags. Each route compares 41,488 live commands, 4,546,032 cleared slots and
7,343,052,288 live-command/color/surface/mask bytes against the full tracer,
including explicit override. The first attempt exposed a fixture assumption:
sparse wobble may author a zero-width command with nonzero row bounds, distinct
from a completely cleared slot. Such commands now compare in full, not as empty
storage. The failed fixture log remains; no shader behavior was changed to pass.
Final clipping fixture:
`2B142DFF221DA1E7B4946E56040A1D25A1C41B094D2BCF5FEEA5AF1D2D7CDB5E`.
All three routes' independent full clipping/span CPU oracles pass, including
57,003,776 1x/2x/4x pixels each. Both NVIDIA APIs' 64-model/768-image, queued
changing-input, full stereo/MSAA/lifetime suites pass. Runtime-input checks,
source freshness, generated Metal bindings, nine shader-helper tests and
PowerShell parsing and non-SDL C++ syntax pass. Both full-game matrices pass all
eight cases each / 160 exact full-SBS candidate/override image comparisons,
including banked upscale, reflective water, motion blur, MSAA, parallel/joined
submission and the no-model native EX menu. Actual shader/override markers and
both direct eyes are verified, with no CPU replay and unchanged preferences.

The previous interior-output experiment completes four short instrumented
180-pair traces (60 warmup) using unchanged `6D11265F...`. It does not demonstrate
a useful clipping reduction: selected D3D12 clip cost is 818/836 us per pair,
Vulkan 1239/1253 us, full/interior respectively. These uncontrolled-host command
intervals are not isolated A/B, sustained FPS or a backend ranking. The interior
shortcut remains opt-in. Evidence: `interior-loaded-diagnosis-oct3/results.json`.
No other project's jobs were stopped, paused or messaged; no quiet-host retry.
Six colour-tracer GPU-cost diagnostics also complete: 180 pairs each, 60 warmup,
6,907 measured draw samples / 6,284 model samples each, complete query identities
and actual NVIDIA D3D12/Vulkan or Intel Vulkan adapters. Selected span cost,
full/colour in us per pair: D3D12 848/802; NVIDIA Vulkan 876/906; Intel Vulkan
8690/9449. Scene costs are respectively 5046/4670, 5864/6008 and 61755/66361 us.
They do not support a portable default: D3D12 is encouraging, the Vulkan traces
are not, and unrelated phases/host conditions also vary. These are not isolated
A/B, normalized clock measurements, sustained FPS or an optimization gain.
Evidence: `colour-spans-loaded-diagnosis-oct3/results.json`, with per-draw/phase
summaries and logs. No quiet-host retries or other-job intervention.

The next targeted work is cooperative row writing rather than just removing UV
arithmetic: the current one-thread-per-polygon tracer writes a height-strided
96-byte command per row, so adjacent lanes target different polygons' distant
storage. A group-per-polygon, bounded row-chunk path could preserve exact serial
edge stepping while distributing contiguous row command writes. This is a next
action, NOT an implemented/validated optimization; original word-wrap/clamp,
UV, repeated-row, wireframe, line/sprite and ordering rules must remain covered.
Ordinary mono DLSS K/M/K renewal passes 96 evaluations, 24 retained-preview uses
and once-only frame-end checks; all 23 plain-menu cases plus RENDERING/loading
pass, including both SDK models' Software-to-D3D12 transitions. Preview OFF
does not run hidden scene/effect/SDK work or change appearance with enhancements.
All owned checks ended. Protected release EXE, both preferences, ReShade files,
original APK and separate Android candidate retain their recorded hashes.
No quality/default change,
device installation or release is performed. Physical Leia/Android, broader
effects correspondence and sustained SBS speed remain open; Ally deferred and
the full goal active. Android candidate `5EA2FFB4...` is unchanged. Native/private-
SDK correspondence matrices are not renewed by these legacy span checks.

## October 3 — exact interior output shortcut (experimental checkpoint)

Previous development executable:
`6D11265F78C04DF0956D60E2E6D67D18AB825F4C7A509B8A61BBA1F78414D125`.
Ordinary launches still select the full reference clipper. The explicit
`STARFOX_TEST_INTERIOR_CLIP` experiment bypasses the four identity viewport
clipping/copy passes only after authoritative software-binary64 coordinates
pass the original half-open predicates. Backface validation, UV bits, raster
boundary-side adjustment and the 129-record ABI are unchanged. Near-plane,
boundary, invalid, line and billboard cases keep their original paths; there
is no convexity assumption, scratch reduction, extra pass, submission, fence,
readback, CPU projection or reduced rendering quality. Full-reference selection
wins. Metal payloads are generated and their bindings validate, but unexecuted Metal
cannot select the experiment. No general/vendor-specific default is promoted.

Final clipping fixture:
`5D6F48290BC43C9B40890A3456C796BD33DB0CEB3710B57CBE5EDA83BCADFEEF`.
On NVIDIA D3D12/Vulkan and Intel Vulkan, each route passes 5,560 bit-exact
headers / 136,472 consumed XYUV words, 60 independently computed triangles
and 1,296 independent interior polygon records. The added packets cover all
3–32 source-corner counts, concavity, raw residuals and both sides of the exact
viewport boundaries. Full clipping/span oracles pass on all three routes:
50,724 fractional polygons, 57,003,776 1x/2x/4x span pixels, near-plane order,
15 residual/invalid/line fixtures and 24 native-word SoftwareRenderer images.
Both NVIDIA APIs additionally pass 64 real models / 768 images, queued changing
model inputs, full stereo ownership/failure/retry/lifetime checks, 54 real MSAA
cases / 162 resolves and 96 sparse-to-motion helper batches. None is an FPS test.

The first D3D12 app matrix passed all seven gameplay cases, then rejected a bad
test setup that combined the host frozen-model preview with a native EX-menu
fixture. That run is not a complete matrix. The corrected test enters the real
EX menu without the unrelated preview. Both renewed D3D12/Vulkan matrices pass
all eight cases each / 160 total full-SBS candidate/override image comparisons.
Mono DLSS K/M/K passes 96 evaluations / 24 retained-preview uses. All 23 plain-
menu cases plus RENDERING/loading pass, including both SDK models' Software-to-
D3D12 transitions; Preview-OFF remains unchanged by heavy enhancements.
All owned checks ended. Evidence uses `interior-*` under `D:/SFE-validation`, including
the discarded setup error. No other project's jobs were paused, stopped or
messaged; no quiet-host retry or sustained speed claim. Shader generators,
source/Metal-binding checks, helper unit tests, PowerShell parsing, non-SDL
syntax and the rebuilt runtime-input suite pass.

Release, settings, injector files and the original APK retain their recorded
hashes. The separately verified Android candidate `5EA2FFB4...` is unchanged;
it is not installed or relabeled as including this optional PC shader checkpoint.
Physical Leia/Android acceptance, broader correspondence and sustained SBS
speed remain open, Ally troubleshooting stays deferred and the full goal is
active. Earlier native/private-SDK correspondence matrices are not renewed.

## October 3 — isolated Android test APK; loaded SBS GPU-cost diagnosis

The Android window-handoff fix below is now packaged in a separate candidate:
`D:/SFE-validation/android-handoff-package-oct3/outputs/apk/debug/app-debug.apk`.
SHA-256:
`5EA2FFB49E8CDC1D3C08CC7EA7E69ABCD2A3FDF4A7CC9D6E6D01C4FCE7B097FC`.
The actual project `:app:assembleDebug` completes offline using the existing
JDK/SDK and a validation-only init script redirecting the ordinary app's output.
No Quest task, WSL, reinstall, device launch or release. The original ordinary
APK remains `5E96D652...`; only the new candidate contains this fix.
The staged unstripped native library is `384B77E8...` (different build/debug path
from the previous standalone native compile), not the old `6E066C86...` artifact.

Package checks verify ELF64/arm64 runtimes, all 38 declared embedded backdrops,
archive integrity and absence of packaged ROM/BIN/signing files/docs/system
link stubs/Quest entry. Three exact native diagnostics identify the new EGL
handoff, recovery journal and deferred plain-menu startup. `--native-marker`
checks only `libmain.so`, not logs or archive metadata; five fixture groups pass
including missing/empty/case-mismatched markers and a marker present only in an
asset log. The unchanged old APK is an actual negative control and correctly
fails this new feature-presence gate. The gate proves compiled-feature presence,
not that Android has executed the handoff. Evidence under `D:/SFE-validation`:
`android-handoff-package-oct3.log`, `android-handoff-package-verify-oct3.log`,
`android-handoff-package-tests-oct3.log`, `android-handoff-stale-package-oct3.log`.

Official `apksigner` verifies v2 signatures for both APKs and their certificates
match (`00da83b3...`). `aapt2` confirms ordinary application/activity identity,
0.0.8/code 13, minimum SDK 26, target SDK 35 and arm64-only native code. This is
compatibility with the existing LOCAL test APK's signer, not a claim about every
distributed release. Signature evidence:
`android-handoff-signature-{candidate,existing}-oct3.log`. ADB is empty; physical
saved-GPU close/relaunch, failed entry recovery and GLES/Vulkan switching remain
unverified. No existing APK, settings or installed app was replaced.

Two short instrumented ordinary-SBS traces use the unchanged `E98BF0AD...` PC,
the actual NVIDIA RTX 5070 Ti Laptop GPU, 180 complete eye pairs each, Original
Corneria at 1x and the existing split/direct-eye route. No other project's jobs
are stopped, paused or messaged; no quiet-host gate or isolated A/B retry is
used. Both D3D12/Vulkan streams have contiguous complete query identities with
no drops/cancellations/errors. Strict summaries exclude 60 warmup pairs and
cover 6,160 actual draw intervals per API, including 4,058 phased model samples.
Model clipping and span generation are substantial selected GPU costs (about
0.87/0.62 ms on D3D12 and 1.47/0.99 ms on Vulkan per measured pair); background
production is also material. These are instrumented command intervals under
uncontrolled/concurrent host load, excluding native-ray queues, CPU encoding,
idle time and presentation. They are NOT sustained FPS, a backend ranking or
an optimization gain. No rendering default/quality/resolution is changed here.
The result directs further work toward geometry clipping/span setup or the
existing calibrated graphics route, not disabling effects or promoting the
previous mixed-performance clipping experiments without acceptance.
Evidence: `sbs-under-load-gpu-cost{-vulkan}-oct3` logs and `phases.json` /
`draws.json` summaries. Previous graphics/SDK/Leia matrices are not renewed.

All owned processes ended. Current/release EXEs, both preference files, ReShade
files and original APK retain their prior hashes. Physical Leia/Android, wider
correspondence and sustained SBS speed remain open, Ally stays deferred and
the full goal stays active.

## October 3 — Android releases its EGL window before Vulkan opt-in

The Android-only renderer transition now replaces a retained GLES window before
the SDL GPU presenter claims it. Inspection of the pinned SDL 3.4.14 Android
window/GLES/Vulkan implementations found that destroying a GLES renderer releases
its context, not the window-owned EGL surface; the GPU window claim calls Vulkan
surface creation directly. The deferred Software/GLES startup therefore needs an
explicit window handoff, as does a later GLES-to-GPU renderer selection. This is
a concrete transition defect, not proof of the reported device boot failure.

All existing renderer owners are released first. Android's single-window rule
requires destroying the old window before creating its replacement, unlike the
Windows flip-swapchain helper. The replacement preserves title, size, fullscreen,
hidden and ordinary presentation flags, but not connected graphics flags. A
creation failure nulls the window handle and retains the armed GPU journal, so
next-launch Software recovery remains available. Same-API changes and other
platforms do not use this handoff. No assets/preferences/cache deletion or
renderer-quality reduction; no new per-frame rendering work.

The current arm64/API-26 optimized native Android target compiles and links;
a final incremental build reports no remaining work. `libmain.so` is ELF64
AArch64 and contains the new handoff diagnostic. Native SHA-256:
`6E066C86C484BE21C11BF8B866A6A396FA8C9B3E7E38FCEB353D57F3CD181B4C`.
The rebuilt runtime-input suite passes the transition predicate, actual SDL
destroy-before-create lifetime under a single-window creation seam, four retries,
fullscreen/hidden preservation, failed creation with no dangling handle, and
retained GPU recovery marker. It also passes its existing settings/input checks.
These are host ownership/policy tests, NOT Android EGL/Vulkan device tests.
Test SHA-256:
`AFB7F6460D9679A8A6288FD2580F6BA2A47AD24E48B642B482AE3897D396C3B3`.
Evidence under `D:/SFE-validation`: `android-current-native-oct3.log`,
`android-window-final-native-oct3.log`,
`android-window-final-runtime-build-oct3.log`,
`android-window-final-runtime-oct3.log`. Scoped whitespace checks pass.

ADB still lists no device. The ordinary APK remains `5E96D652...` and DOES NOT
contain this newly built native-library fix; no APK replacement, installation
or release was performed. Current PC remains `E98BF0AD...`; its previous GPU,
DLSS, stereo/menu and native-correspondence evidence is not renewed by these
Android-only changes. Both saved-setting files, release EXE and ReShade files
retain their prior hashes. All owned checks ended; other compiler jobs kept
running, with no isolated timing retries or performance acceptance. Physical
Android exit/relaunch and Leia, wider correspondence and sustained SBS speed
remain open; Ally stays deferred and the full goal stays active.

## October 3 — ordinary stereo shares immutable GPU model uploads

Current development PC:
`E98BF0ADBE0BB6F343DB95763E1BE776A9B176DF7BA3137616EB174E69F70013`.
Ordinary SBS now uploads immutable source vertices, source visibility descriptors,
BSP nodes and face IDs once per stereo recording, sharing them across both eyes
and repeated model instances. Camera transforms, visibility results, materials,
normals, raster targets, depth, motion and ray outputs remain eye/draw-owned.
Explosions keep their mutable expanded vertices/visibility and share only their
already-flattened immutable topology. Axis effects, billboards and empty helpers
keep their original paths; empty helpers request no source upload pass.

`GpuModelSourcePool` rebuilds source identities for EVERY pair, including failed
or cancelled retries and recycled addresses. It deduplicates immutable prepared
packets, never mutable shape pointers or GPU results. One copy pass on the left
or sole command precedes both consumers; existing split submissions remain the
default. Parallel recording uploads before starting its worker and submits left
before right; joined recording uses the same command. No additional submit,
readback, wait, CPU geometry transform, simulation tick, quality or resolution
cut. Both encoders join/cancel before borrowed packet identities are cleared.
SDL source/transfer backings cycle each new recording so queued consumers retain
their original bytes. Device release follows existing eye-fence retirement.

The pool caps logical read-buffer payload to eight MiB, transfer capacity to
eight MiB, and read-buffer handles to 2,048. Changing layouts shrink excessive
high-water allocations when necessary. Budget/handle/allocation failure uses
the original full-quality uploads. These are logical capacity bounds, not total
VRAM guarantees: SDL may retain backing versions until existing in-flight eye
fences retire. Host accounting separates actually uploaded bytes from shared
consumer demand, reports retained pool capacity even when inactive, counts memo
capacity per producer rather than per draw, and clears released scene counters.
The checked Corneria pair uploads 139,744 instead of 161,888 model-input bytes,
about 13.7% less, with 14,000 bytes of logical pool storage. This is structural
upload reduction, NOT measured FPS improvement under concurrent compiler load.

Final GPU fixtures:
`1A4D0F54883010F4E32D07788CE14C2BA85A2C3063454BA831DFE2FB74B9A18E`
(stereo),
`BE55A9A8E645AB1472250318C9D75D11E82D3C24E670F494A4AB1F4043CAA65C`
(ray geometry). Both D3D12/Vulkan complete fixtures pass: 45 independent Software
source images per API, native/continuous/Euler mutation, budget/handle fallback,
unencoded packets, deduplicated uploads, ordinary recovery and cancellation.
Joined and parallel owners each match 6,266,880 duplicate-upload color/surface/
depth/motion bytes per API, with independent CPU eye coverage, resize/covered
black writes, partial failure/retry, release, borrowed retirement and twelve
queued source-mutating pairs. Real 2/4/8 MSAA, retained palette resolves and sparse
to motion transitions also pass. Ray geometry passes native/fractional/residual
coordinates, materials, both-eye resident reflective underlays, reuse and
invalidation. This is not a new native Vulkan hardware-RT acceptance claim.
Evidence: `source-upload-accounting-{stereo,rays}-{direct3d12,vulkan}-oct3.log`
under `D:/SFE-validation`.

The graphics checkpoint `B782E8F6...` passes 80 byte-identical full-game SBS
comparisons (160 captures) across both APIs: Corneria, water/RT/reflections,
banked upscale, Venom motion, MSAA, asteroid zero-source fallback, parallel and
joined owners. The final rebuild only corrects host accounting and renews 50
selected comparisons for Corneria, water/rays, asteroid zero-source, parallel
and joined recording. Each actual producer marker confirms shared uploads or
zero unused source work, with both eyes retained directly and no replay fallback.
Reports: `source-upload-final-native-{direct3d12,vulkan}-oct3/results.json`
and `source-upload-retained-native-{direct3d12,vulkan}-oct3/results.json`.

Thirteen rebuilt CPU suites and no-SDL compilation of both changed implementation
units pass (`source-upload-accounting-cpu-oct3.log`). Both capture scripts parse;
the scoped source/test/script whitespace checks pass. Embedded mono K/M/K passes
96 evaluations, 24 exact held uses and 120 once-only frame ends without SDK
warnings (`source-upload-accounting-dlss-oct3/toggle.log`). All 23 plain-menu
cases plus RENDERING/loading pass, including both models' Software-to-D3D12
transitions and no hidden Preview-OFF scene/effect/SDK work:
`source-upload-accounting-menu-oct3/results.json`. The native calibrated TAA/
SDK correspondence matrices are not renewed by this ordinary-stereo change;
the earlier edge-witness evidence below remains its own checkpoint.

All owned checks ended. Other projects' compiler jobs stayed running as the user
requested; no isolated timing retry or sustained FPS claim. Physical Leia and
Android saved-GPU/relaunch checks, wider temporal/secondary correspondence and
sustained SBS/GPU speed remain open; ADB lists no device and Ally stays user-
deferred. Full goal active. Release EXE, both preference files, ReShade settings/
log and ordinary APK retain their prior hashes. No WSL, push, release, device
install or saved-setting change.

## October 3 — witnessed native TAA edge styles

Previous development PC:
`F2B5FC6EF395069FBC5D4E36BA72D379EC065F8B81FDC63729FD7B9A0BA97DA0`.
Native calibrated TAA now tracks the actual pre-effect edge branch for Cel,
Ink, Neon, Blueprint, Comic, Vaporwave, Chalk, Stained Glass, Hologram, Pencil,
Woodcut, Xray and Pop Art. Blueprint's grid and Hologram's scanline branch are
included. The witness is captured from the actual resolved input in each
ordered post pass, not reconstructed from the final styled RGB. Original pass
slots, receiver class, intensity and grid scale remain in accepted descriptors.
Every positive history tap and neighbourhood clamp sample must match both the
edge witness and existing screen-pattern phase. De-jittering compares the AA
taps against the independently styled centre witness; a changed branch keeps
the exact centre artwork. Invalid/missing witnesses cannot seed history, and
rejected/cancelled candidates cannot replace accepted descriptors or colour.
Static edge styles no longer advance sample phase just because the unused FX
clock ticked. Neighbour averages, warps, animation, liquid and secondary ray
transport retain their existing conservative guards. Mixing an untracked pass
clears edge correspondence for that layer, not the other layer.

Capture uses an optional lazy second MRT in the existing composite pass, only
for selected native TAA with supported edge styles. Four independent banks
retain two R32_FLOAT ping-pong atlases for each eye's AA/centre inputs, including
equal eye extents. This is up to 16 bytes per eye pixel; the conservative native
working bound includes it. Retained atlases across extents/formats have a
separate one-GiB payload cap, not a free-VRAM guarantee. Accepted keys use the
existing metadata.w: no additional TAA history image, render pass, submission,
production readback or GPU wait. Ordinary rendering, SDK upscaling, MSAA/SSAA
and Effects OFF do not request these atlases. Normal composite bindings remain
unchanged; native TAA's optional samplers and liquid-buffer binding are renewed.

`tools/check_native_edge_taa.ps1` passes real D3D12/Vulkan components and game
owners in four negotiated RGBA/BGRA linear/sRGB formats. Each component format
checks 7,365,072 scalar-history bytes, 1,007,556 observable stable responses,
72,202 crossed footprints and 489,136 centre-branch fallbacks. All 13 styles,
two authored grids, both receivers, 35/100 intensity, original/non-contiguous
slots, ordered mixed passes, a guarded WORLD warp, cancelled/rejected candidates,
wet/UI/alpha, NaN/fractional/out-of-range/missing/aliased witnesses, recovery and
four non-overwriting equal-extent banks pass. Both AA and centre branches are
checked independently from their PRE-style inputs. MRT capture is byte-identical
to normal composition; 1x RGB also matches the independent flat-effects oracle.
The native game owner passes 108 cases per API (216 total), all three qualities,
independent eyes and rigid-motion/scalar-history/centre reconstruction, recycled
generations/morph/cuts, held/OFF, unused-clock ticks, rejected presentations, XR
waits, real queue stalls, in-flight retirement and combined enhancements. Full
intensity constant-colour Ink/Blueprint/Chalk need not change RGB when reusing
history; their partial-intensity cases prove reuse is not blanket-rejected.
Evidence: `native-edge-taa-oct3/summary.json` in `D:/SFE-validation`.

Final fixtures:
`92971CF09F8C3719CF02AED1DE4FE51AFFB83C6C08EA0B8BD4D20993F652D1A3`
(composite),
`E596F9BB84F9B598E43F6D1ACB9250D313A28B3107700DB685AF3C1DBDB58025`
(native owner). Both full compositor runs pass all four formats, 67 style/warp/
animated selections, 17 decorative finishes, global/appearance/bloom/history/
exposure/spatial-AA/SSAA/FSR/core/pattern/liquid/edge TAA and native scene checks:
`edge-witness-full-composite-{direct3d12,vulkan}-oct3.log`. D3D12 also renews
resident DXR; the Vulkan compositor fixture does not certify native Vulkan RT.
All four legacy TAA component/liquid-owner runs pass:
`edge-witness-legacy-taa-oct3/summary.json`.

Thirteen selected CPU suites and nine portable-source/binding tests pass, with
fresh generated DXIL/SPIR-V, clean C++ builds, script parsing and all 22 changed
source/test/script whitespace checks. Logs: `edge-witness-cpu-oct3.log`,
`edge-witness-shader-source-tests-oct3.log`, `edge-witness-*-build-oct3.log`.
Embedded mono K/M/K passes 96 evaluations, 24 exact held uses and 120 once-only
frame ends without SDK warnings (`edge-witness-mono-dlss-oct3/toggle.log`). All
23 plain-menu cases plus RENDERING/loading pass, including both models' direct
Software-to-D3D12 transitions and no hidden Preview-OFF scene/effect/SDK work:
`edge-witness-menu-oct3/results.json`. These are correctness checks under other
compiler load, not isolated timing or sustained FPS acceptance.

All owned checks ended; other projects' builds remain running as requested.
Native SDK edge correspondence/private-filter parity is NOT renewed by native
TAA. Wider neighbour/warp/secondary transport, physical Leia/Android and sustained
SBS/GPU performance remain open; ADB lists no device, Ally stays user-deferred,
full goal active. Release EXE, both preference files, ReShade configuration/log
and ordinary Android APK hashes are unchanged. No WSL, push, release, device
install or settings change. Previous source-sharing evidence remains historical.

## October 3 — ordinary stereo shares animation-frame projection sources

Previous development PC:
`523EEB2DE4CCEFD0E637CA48CC459662F63596E2CF629455D4FF352CFFAD3ED3`.
Ordinary stereo now prepares immutable vertex coordinates and source visibility
descriptors once per unique shape/effective animation frame/representation within
one pair, including repeated instances. Continuous coordinates stay unscaled;
each draw's GPU transform still applies its own scale. Native byte coordinates
retain exact prescale/word rounding and require the matching scale; word-only
coordinates bypass unused scale exactly as before. Eye pose constants, GPU
transforms, BSP/visibility evaluation, clipping, materials, uploads, ray geometry
and output/history remain independent. Explosion/axis expansion materializes
writable arrays before modifying them, including the public fragment helper.
No cache survives source mutation or a frame. The existing common 8 MiB budget
bounds retained topology/projection array payload, not temporary construction or
lookup-map overhead; overflow retains full-quality independent packing. Encoders
join before the source packets die. No new GPU image, pass, production readback,
submission or wait is added.

Diagnostic-only `STARFOX_TEST_DUPLICATE_STEREO_PROJECTION=1` restores per-draw
coordinate/visibility preparation while leaving the earlier BSP sharing active.
Actual Corneria accounting is 8 source preparations serving 42 model draws versus
42 preparations in that control. This is only the coordinate/visibility work,
not all pose packaging, total frame time or an FPS result. App comparisons assert
the topology-policy marker is identical between both projection routes.

Final GPU stereo fixture:
`0DD9FB06745610630E1E801F24D5FF467E88EE6D186FF8C4D568FA7EF01B7247`.
All four D3D12/Vulkan shared/duplicated component runs pass independent CPU-eye,
mutation/reuse, source-policy rejection, upload-owned retirement, joined/parallel
ownership, cancellation/retry and real MSAA checks. Each joined/parallel route
retains 20 CPU eye images and 6,266,880 exact colour/surface/depth/motion bytes;
MSAA retains 54 split/joined/parallel cases and 162 palette resolves. Logs:
`shared-projection-component-{direct3d12,vulkan}-{shared,duplicate}-oct3.log`.
These ordinary GPU stereo checks do not certify physical/native Leia transport.

All 13 selected CPU suites pass (`shared-projection-edge-verified-cpu-oct3.log`).
Cartridge source-equivalence tests cover 6,208 models / 9,378 animation frames:
`shared-projection-final-{original,ex}-catalog-oct3.log`. Added cases check frame
modulo, representation/native scale keys, partial budget, source rejection and
copy-before-fragment-mutation. One new fixture incorrectly expected native
integer keys after stereo_scene_eye, which deliberately promotes subpixel
projection. It now asserts that production policy first and checks native keys
on direct native encoder recordings; no production projection policy changed.
The initial failed fixture and wrong EX input-filename logs are retained.
Final GPU-disabled source syntax, script parsing and all 12 changed source/test/
script whitespace checks pass. No shader changed. Native DisplayXR was rebuilt
for the public source-pointer layout; calibrated native owner matrices are not
renewed by these ordinary tests.

Both APIs' full app comparisons pass 100 exact images in 20 cases each (200
total). All 80 runtime logs confirm the requested API and NVIDIA RTX 5070 Ti
Laptop. Each API retains 19 isolated projection-route comparisons and failed-
left mono recovery; formats, water/bloom, upscale/AA, MSAA2/4/8, reflective EX
lava, banked ground, fog, production motion and separated-model fallback pass.
Evidence: `shared-projection-app-{direct3d12,vulkan}-oct3/results.json`.
Final embedded mono K/M/K passes 96 evaluations, 24 exact held uses and 120
once-only frame ends without SDK warnings/errors:
`shared-projection-mono-dlss-oct3/toggle.log`. All 23 plain-menu cases plus
RENDERING/loading pass, including navigation and both models' direct Software-
to-D3D12 transitions. Heavy/SSAA settings leave Preview-OFF output identical and
run no hidden scene/effect/SDK work: `shared-projection-menu-oct3/results.json`.
Menu times collected under other compiler load are not isolated acceptance.

All owned checks ended. Other projects' builds remain running, as requested;
no isolated timing attempt or sustained FPS claim. Physical Leia/Android, wider
effects correspondence and sustained SBS performance remain open; ADB lists no
device, Ally remains user-deferred, full goal active. Release EXE, both preference
files, ReShade configuration/log and ordinary Android APK hashes are unchanged.
Evidence is in `D:/SFE-validation`; no push, release, install or settings change.

## October 3 — ordinary stereo shares model-source preparation

Previous development PC:
`C95CC05E18751DEC309D2B7AE6C2F2E60865667911CDB55F83FF3916699BD501`.
Ordinary stereo now prepares each unique source shape/authoritative explosion
policy once within a pair. Both eyes and repeated instances borrow the immutable
face/BSP graph and normal table instead of repeating deep copies, address maps
and traversal-bound scans. Transforms, visibility/BSP order, materials, clipping,
GPU uploads/ray geometry and temporal outputs remain per draw/eye. Nothing is
cached across source mutation or frames; encoder readers join before packet
destruction. An 8 MiB retained-source-payload budget falls back to the original
full-quality packing, not simplified geometry. Axes/billboards keep their paths.
No new GPU image, pass, production readback, submission or wait is added.

The diagnostic-only previous route is
`STARFOX_TEST_DUPLICATE_STEREO_TOPOLOGY=1`. Actual Corneria pair accounting is
8 unique preparations serving 42 model draws, versus 42 preparations previously.
This counts that source work only, not a total frame-time or FPS improvement.
The same-binary app matrices pass 100 byte-exact packed images per API (200 total):
eight formats, reflective water/bloom, upscale/AA, MSAA2/4/8, reflective EX lava,
banked ground, fog, production motion, separated-model fallback and failed-left
mono recovery. All 80 runtime logs confirm NVIDIA RTX 5070 Ti Laptop and the
requested D3D12/Vulkan API. Each API has 19 source-route comparisons plus the
failed-left mono comparison. Evidence:
`shared-topology-app-{direct3d12,vulkan}-oct3/results.json`.
Final embedded mono K/M/K preview passes 96 evaluations, 24 exact held uses and
120 once-only frame ends without SDK warnings:
`shared-topology-mono-dlss-oct3/toggle.log`. All 23 final plain-menu cases plus
RENDERING/loading pass, including live navigation and both models' direct
Software-to-D3D12 transitions. Heavy/SSAA settings leave Preview-OFF identical
and run no scene/effect/SDK work. Preferences and injector log are unchanged:
`shared-topology-menu-oct3/results.json`. Recorded menu times under other
compiler load are not isolated performance acceptance.

Final GPU stereo fixture:
`A017577206E777871A621B77B4E274CD6F93E830C67A6A4DDB4EE89142B59686`.
Shared and duplicated routes pass on actual D3D12 and Vulkan. Each retains the
independent SoftwareRenderer eye oracle, source/layout/material mutation checks,
source/policy rejection and upload-owned retirement after preparation destruction.
Joined and parallel routes each pass 20 independent eye images and 6,266,880
exact colour/surface/depth/motion bytes; cancellation/retry, empty/resize,
borrowed retirement and 12 queued underlay pairs pass. Real stereo MSAA covers
54 split/joined/parallel 2/4/8-sample cases and 162 retained palette resolves;
96 changing helper batches and depth/motion checks also pass. Logs:
`shared-topology-component-{direct3d12,vulkan}-{shared,duplicate}-oct3.log`.
These ordinary stereo components are not calibrated native SDK/Leia owner tests.

Eleven final-linked CPU suites pass (`shared-topology-verified-cpu-oct3.log`).
The new source fixture initially assigned a fractional phase to the authoritative
byte counter, unintentionally selecting normal BSP mode. The fixture now uses
byte progress 1 plus separate fractional phase .5, and explicitly verifies that
phase alone does NOT flatten the graph. Initial failed diagnostic logs remain;
no production destruction policy was changed to satisfy the test. Budget,
fresh in-place source mutation and partial-bind exception cleanup also pass.
Affected source files compile with GPU support disabled; changed scripts parse
and all eleven changed source/test/script files pass whitespace checks. No GPU
shader source changed; earlier shader/physical-platform checks are not renewed.
The native DisplayXR library was recompiled for the public draw-layout change,
but the calibrated native TAA/blur/SDK owner matrices were not rerun.

Other projects' builds stay running, as requested. No timing batch or isolated/
sustained FPS claim. Physical Leia/Android, wider native effects correspondence
and sustained SBS performance remain open; ADB lists no device, Ally stays
user-deferred and the full goal stays active. All owned checks ended. Release EXE, both preferences,
ReShade configuration/log and ordinary Android APK remain unchanged. No push,
release, install or device/settings change. Evidence is in `D:/SFE-validation`.

## October 3 — ordinary stereo shares its fog source geometry

Previous development PC:
`AE3F4755A70385A64837A880129C4910A4A33685CC2E1D003B1AC037B72F33FE`.
Ordinary stereo no longer copies/translates all fog triangles and rebuilds a CPU
BVH per eye. It packs/uploads the existing central scene once per pair, and both
eyes trace from their own source-space origin/off-axis projection. Primary and
light rays use that origin; tilted ground intersections subtract it correctly.
Both motion-blur fog underlays borrow the same resident geometry too. Source
packing/uploads reduce from two to one, or four to one with both blur underlays;
this is source-work accounting, not a total frame-time or FPS claim. Outputs,
qualities, pipelines and eye histories remain independent. No new GPU image,
production readback, submission or wait is added. Borrowing expires with the
pair, on the same ordered SDL device queue; no cross-frame identity cache.

The former translated-scene route remains a diagnostic-only control. The initial
graphics checkpoint `2F4F7A347D1AF38450A6CAE227CE317D745BD78B248230D5E1D45D74CC7E5F2D`
passes 75 byte-exact images per API (150 total) on NVIDIA RTX 5070 Ti Laptop
D3D12/Vulkan: all eight stereo formats, all three fog qualities, banked ground,
reflective water/production motion, reflective EX lava and failed-left mono
recovery. All 60 runtime logs confirm the actual requested API and adapter.
Evidence: `shared-fog-app-{direct3d12,vulkan}-oct3/results.json`.
The final rebuild adds only an underlay source diagnostic; it renews 25 exact
images per API (50 total) for high fog, banked ground, water/motion, lava and
recovery. Successful source markers prove BOTH eye underlays use resident
geometry, versus independent uploads in the control:
`shared-fog-final-app-{direct3d12,vulkan}-oct3/results.json`.

Final GPU fog fixture:
`999F88E14187A5FF370E64724A48B035A429097DF77889B54D28F71F471E4E0E`.
Both APIs pass 96 translated-scene/CPU-integral cases each, also checked through
resident reuse, including empty scenes, primary/underlay modes, ground and
1/16/64/128 samples. Maximum error is `1.34493e-06` (limit `1e-4`). Additional
three-axis origins/tilted ground, distinct eye outputs, donor republish/release,
malformed descriptors, alias rejection and invalid-origin recovery pass.
D3D12 additionally passes donor destruction with two GPU readers genuinely
stalled behind an unsignalled queue fence. That deliberate stall test is D3D12
only; Vulkan passes the submitted-reader lifecycle checks, not that stall test.
Logs: `shared-fog-final-component-{direct3d12,vulkan}-oct3.log`.

Eleven selected final-linked CPU suites pass (`shared-fog-final-linked-cpu-oct3.log`;
`shared-fog-final-cpu-link-oct3.log` records final-core relinking).
Portable shader suites pass all 3 source and 6 binding tests; DXIL/SPIR-V/MSL
generated fog freshness, PowerShell parsing and changed-source whitespace pass. MSL
generation is not physical Metal validation. Final embedded mono K/M/K preview
passes 96 evaluations, 24 held uses and 120 once-only frame ends without SDK
warnings (`shared-fog-final-mono-dlss-oct3/toggle.log`). The broader calibrated
native temporal/SDK owner matrices are not renewed by these ordinary app tests.
All 23 final plain-menu cases plus RENDERING/loading pass, including both models'
direct Software-to-D3D12 transitions and live options navigation. Heavy/SSAA
settings leave Preview-OFF output identical and run no scene/effect/SDK work;
preferences and the injector log stay unchanged. Evidence:
`shared-fog-final-menu-oct3/results.json`. Menu work times recorded under other
compiler load are not isolated performance acceptance. All owned checks ended.

Other projects' builds remain running, as requested; no timing batch or isolated/
sustained FPS claim. Physical Leia/Android, wider native effects correspondence
and sustained SBS performance remain open. ADB lists no device; actual Android
saved-GPU exit/relaunch remains unverified. Ally stays user-deferred and the full
goal stays active. Release EXE `828D98F4...`, both preferences, ReShade configuration/
log and ordinary Android APK `5E96D652...` remain unchanged. No push, release,
install or device/settings change. Evidence files are in `D:/SFE-validation`.

## October 3 — native temporal source preparation shared across the eye pair

Previous development PC:
`491003E4790ECB931C223A39D53076525944CB921A081A6892CC5549B113709F`.
Native Leia TAA, physical motion blur and SDK reconstruction now prepare entity
identity, topology and accepted primitive correspondence once per history source
for the whole eye pair. Current packet descriptors are reused; accepted packet
descriptors, key maps and primitive scans are no longer rebuilt for each eye.
SDK and blur/TAA timelines remain separate. Each eye still validates its own
camera/FOV/model mapping and produces independent motion/depth/history guides.
The stack-local preparation borrows retained immutable frame geometry and is
never retained across waits, cancellation or a different source frame. No CPU
vertex projection, new GPU image/pass, production readback or quality reduction.
This change targets native calibrated temporal pipelines, not ordinary SBS.

Ten rebuilt current-core CPU suites pass, including asymmetric cameras, malformed
current/previous projections without poisoning the other eye, distinct accepted
SDK/blur sources, packet descriptor validation, rigid/line/billboard order and
borrowed-geometry ownership. Existing generation/destruction/morph/floor guards
remain in force. A new malformed floor-with-lines fixture initially expected
triangle history to survive; the existing floor policy rejects that entire
packet. The expectation was repaired to preserve this fail-closed behavior and
the unrelated actor's correspondence. Preliminary failed logs are retained;
`native-motion-verified-cpu-oct3.log` is the final passing CPU run.

Rebuilt native owner:
`794D463E81E5D9C71E30AC5AD52973C75B2D6C785352AE9EDF5D6DE0033E2C7B`.
Final actual-queue native TAA and blur runs pass on D3D12 and Vulkan, four
linear/sRGB RGBA/BGRA formats each. Independent scalar motion/resolve/shutter
oracles cover both eyes, all qualities, 60/120/240 blur clocks, actual
destruction/billboard correspondence, recycled entities/morph/cuts, protected
ink, held/OFF, rejected presentation, GPU/XR waits and in-flight cancellation.
`native-motion-verified-{taa,blur}-{d3d12,vulkan}-oct3.log` retain these runs.
The real D3D12 K/M SDK native owner also passes four formats: base quality/AA/ray
cases, 25 static palettes, finite original/gradient/red-sand ground, ownership-
local posts and four point patterns, water/lava local rejection, held/static
sources, resets, failure/retry/reconnect and resource lifetime. It records 5,270
evaluation attempts, 950 viewport retirements and 5,891 final frame ends in
5,895 attempts (intentional finish retries); `native-motion-verified-dlss-oct3.log`.
The XR dispatch/panel is mocked in these fixtures: real GPU/SDK execution is not
physical Leia certification or proof of the SDK's private filter parity.

Ordinary app regression checks separately pass 20 byte-exact images per API
(40 total) on NVIDIA RTX 5070 Ti Laptop: reflective water/bloom, MSAA4, production
motion and failed-left mono recovery. Evidence:
`native-motion-app-{d3d12,vulkan}-oct3/results.json`. Embedded mono K/M/K preview
passes 96 evaluations, 24 held uses and 120 once-only frame ends without SDK
warnings; `native-motion-mono-dlss-oct3/toggle.log`. Changed-source whitespace
passes. The independent SDK component, full ordinary format/briefing and 23
plain-menu/loading matrices were not rerun by this checkpoint.

Other builds remain running; no timing batch was launched and no isolated or
sustained FPS gain is claimed. Physical Leia/Android, broader native effects
correspondence and sustained SBS performance remain open. ADB currently lists
no device; actual Android saved-GPU exit/relaunch remains unverified. Ally stays
user-deferred and the full goal stays active. All owned checks ended. Release
EXE `828D98F4...`, both preferences, ReShade configuration/log and ordinary
Android APK `5E96D652...` remain unchanged. No release, push, install or device/
settings change.

## October 3 — quiet fixed-layer timing witness; overlapping batch rejected

Previous development PC:
`7A6ED2FA1E2D4A39A7FC5E707E413F70A280CDDA5B9A2AEB606F06312634CC5D`.
The only application change since `ABC68124...` is a test-only, one-time marker
after a successful eligible right-eye producer: actually duplicated or actually
pair-shared. Rendering/quality, source borrowing, native SDK/Leia transport and
production resource ownership are unchanged. The benchmark accepts the existing
duplicate-source override; its guarded comparison can now omit per-frame GPU
queries/logging and readbacks. Actual adapter/backend, full pair counts, exact
producer route, unchanged EXE/preferences and both trial orderings are required.
The process observer checks immediately and every half second without stopping
any observed process. Quiet results are hidden-window/unpaced pair work, not
visible/paced display FPS or device acceptance.

One Vulkan batch was attempted after the process preflight found no compilers.
Runs one through five completed, but another project's CMake/Ninja/GCC build
started during run six. The live benchmark was awaited, the whole batch rejected
and no partial rows accepted as speed evidence. There is no complete results
file: `fixed-layers-quiet-vulkan-oct3/rejected.json` records the overlap, while
the driver/runtime logs retain the rejected attempt. Other builds were never
paused/stopped or messaged; no second timing batch was launched this turn.

Final same-EXE duplicate/shared comparisons pass 25 exact images per API (50
total) on actual NVIDIA RTX 5070 Ti Laptop D3D12/Vulkan: reflective water/bloom,
MSAA4, banked ground, production motion blur and failed-left mono recovery.
Each API's four active cases reduce eligible producer submissions from 256 to
128 without changing packed output;
`fixed-layers-quiet-check-{d3d12,vulkan}-oct3/results.json`. These are correctness
and workload results under normal host load, not isolated performance acceptance.
Ten existing, current-core CPU suites pass; only the app required rebuilding.
Embedded mono K/M/K passes 96 evaluations, 24 held uses and 120 once-only frame
ends without SDK warnings (`fixed-layers-quiet-dlss-oct3/toggle.log`). PowerShell
parsing and changed-file whitespace also pass.

The earlier full 160-image format/briefing, 23 plain-menu/loading and calibrated
native component/owner matrices are not renewed by this narrower marker-only
checkpoint. Physical Leia/Android, broader correspondence and sustained SBS
performance remain open; Ally is deferred and the full goal remains active.
All owned checks ended. Release EXE `828D98F4...`, both preferences, ReShade
configuration/log and ordinary Android APK `5E96D652...` remain unchanged.
No release, push, install, runtime/settings change or new GPU allocation.

## October 3 — pair-shared fixed stereo source layers

Previous development PC:
`ABC6812462C3F2844CEF1695E79D8C1F2BDC78DA8CA0FF79511C7F9B257D78AD`.
Ordinary stereo now renders unchanged indexed screen-space producers once per
pair: compatible late cartridge/HUD, fixed backdrop and isolated portrait/text
planes. A stack-local, four-entry borrow expires on every success or failure;
source identity/device/extent must match the left producer. No output is retained
across frames, no extra persistent GPU allocation, shader or production readback.
The following pair uses the existing producer buffer cycling/queue ownership.
Models, grid, projected text, dust/particles, displaced sky and marked depth-
adjusted reticles stay eye-specific. Sharing is before palette/fades/composition/
effects: neither eye's composed colour, reflection, motion/depth nor temporal
history is shared. Mono bypasses eligibility scans. Shared right-eye producers
also avoid constructing another late-layer draw vector.

Final same-binary duplicate-versus-shared comparisons pass on NVIDIA D3D12 and
Vulkan, all eight formats plus reflective water/bloom, MSAA4, reflective lava/
exposure, banked ground and production motion blur. Failed-left-eye mono recovery
also matches exactly. Each API passes 70 packed images over 14 cases;
`fixed-layers-final-{d3d12,vulkan}-oct3/results.json`. Each active 32-pair case
reduces eligible HUD producer submissions from 64 to 32; over the 13 active cases
per API that is 832 to 416. This is producer-work accounting, NOT a 50% total
render-time/FPS claim.

Original and EX actual PLANETSELECT/Start briefing sequences additionally pass
400 complete GPU pairs each, duplicate and shared, on both APIs. Twenty packed
captures match exactly through entry/fades/dialogue. Each isolated portrait/text
producer reduces 502 submissions to 251 in each sequence;
`fixed-layers-briefing-{d3d12,vulkan}-oct3/results.json`. The final EX Vulkan image
was inspected: both portraits, heading, planet and dialogue remain visible.
CPU eligibility regressions reject mixed projected lists, missing source data,
sky parallax and reticles, while allowing fixed/mapped raster and UI tile planes.
The rebuilt ten CPU suites, PowerShell parsing and changed-file whitespace pass.

Embedded mono K/M/K preview is renewed on this EXE: 96 SDK evaluations, 24 exact
held uses and 120 once-only frame ends, without SDK warnings
(`fixed-layers-dlss-oct3/toggle.log`). All 23 Preview-OFF/plain-menu cases plus
RENDERING/loading pass, including both models' Software-to-D3D12 transitions;
`fixed-layers-plain-menu-oct3/results.json`. No hidden preview work or saved-
preference/injector-log change. Raw timing rows from these correctness runs are
not isolated speed evidence; the user's other builds remain running.

The first `02C29377...` graphics checkpoint passed 20 selected D3D12 images;
final `ABC68124...` only moves source-vector construction inside the producer
and gates classification off in mono. The larger final matrices above supersede
that preliminary scope. Calibrated native/SDK shaders and eye-owner implementation
are unchanged; their full component/owner matrix below was not rerun by these
ordinary-stereo tests. Physical Leia/Android, broader native correspondence and
sustained performance remain open. Ally is deferred, full goal active. ADB has
no connected Android device. No release/push/install or Android APK change.
Release EXE `828D98F4...`, both preferences, ReShade configuration/log and ordinary
Android APK `5E96D652...` are unchanged. All owned diagnostics ended.

## October 3 — native SDK point-pattern rejection witnesses

Previous development PC:
`258CA386ADBA9D93F62EE42785468C996A7588887F8F8218264CC1B463DC9BF8`.
Native calibrated DLSS now supplies phase-aware rejection guides for Dithered,
Night Vision, Scanlines and CRT Phosphor. The actual contributing SSAA/MSAA
ownership samples must agree before their resolved colour can receive a phase
witness. Every positive tap of the accepted previous input-grid footprint must
match, including when clean geometry moves over previously patterned radiance.
Geometric motion includes both jitters for this comparison; only the SDK vector
has jitter removed. Missing depth/flow, unknown receivers, actual wet samples
and untracked posts retain rejection. Empty far-world/protected-ink handling
remains distinct from foreground without valid correspondence.

Two private R32_FLOAT input-grid phase images are allocated lazily for selected
patterns (eight bytes per input pixel per eye, included in the existing working
bound). Integer phases are represented exactly. They are written as a fifth
target of the existing guide pass: no additional guide pass, CPU mapping or
readback in production, no change to the SDK's RG32_FLOAT motion or R8_UNORM bias
formats, signed payload, quality or RGB/pass order. Pattern-OFF uses the original
four-target guide shader. Pattern descriptors/intensities/grid/sample layout
commit with the accepted colour pair; cancelled/submitted-rejected candidates
cannot replace them and still force the next SDK attempt to reset.

This proves the application's current-AA/accepted-footprint rejection guide,
not knowledge of the SDK's private neural reconstruction/history filter. It
does not assert complete point-pattern sharpness/parity at arbitrary motion or
relax neighbouring/warped/secondary-reflection correspondence guards.

The final pattern component checker is
`B4F1371F64E3DEF40633CD982670B835B9BD23FF991B3C7CB8840F319CB60DAF`.
Its independent phase/coverage/motion/bias oracle exercises 128 real-SDK cases:
both K/M models, all four point patterns and SDK modes, WORLD/MODEL/both and OFF,
35/100 intensity, ordered mixed passes, layout/scale/intensity changes,
positive same-phase reuse, positive fractional/cross-phase rejection,
zero-weight taps, boundaries, missing flow/depth, actual liquid in the final
AA sample, protected full-panel ink/opacity, held images and cancelled or
submitted-rejected candidates. All four linear/sRGB RGBA/BGRA formats cover
centre, 2x SSAA and MSAA4; RGBA additionally covers SSAA3/4 and MSAA2/8.
Final evidence: `dlss-pattern-component-accepted-final-oct3.log`.
It passes 4,352 actual SDK evaluations, 4,352 exact held-eye reuses,
119,721,600 independently checked guide pixels (103,758,720 phase witnesses),
440,138,880 exact protected/opacity bytes and 1,502,048,587 assertions.
The unchanged ordinary component matrix is renewed separately in
`dlss-pattern-baseline-component-final-oct3.log`: 488 SDK evaluations, 912 exact
held uses, 10,618,320 scalar guide pixels and 304 retired viewports.

The final actual D3D12 native owner checker is
`2E5517442E302337B490A28979AB3860A98CDEB791E3A59BAEFBD502B43CFDA0`.
All four target formats pass the existing K/M/all-mode AA/ray owner matrix,
finite floors, all 25 palettes, liquid/current-bias, frame-end/failure/held/OFF,
wait/reconnect and retained submission checks. Each format adds 24 point-pattern
owner cases at Balanced across K/M, WORLD/MODEL/both and centre/SSAA3/MSAA4:
static clocks stay held, movement preserves global history, intensity changes
reset, stereo remains distinct and protected ink/opacity exact. Neighbour/warp/
animated post cases and chromatic/secondary-reflection guards still pass.
The owner ends with 95,321,556 assertions
(`dlss-pattern-owner-final-oct3.log`). GPU queues are real; XR dispatch is mocked,
not physical Leia/panel acceptance. The SDK does not run on Vulkan here.

Ten CPU suites, shader stamps and changed-file whitespace checks pass. The final
PC renews 40 exact ordinary stereo/recovery images across NVIDIA D3D12/Vulkan
(`dlss-pattern-app-accepted-{d3d12,vulkan}-oct3/results.json`), plus embedded mono
K/M/K preview (`dlss-pattern-dlss-oct3/toggle.log`). Plain-menu/loading evidence
is separate (`dlss-pattern-plain-menu-oct3/results.json`): all 23 Preview-OFF
cases plus RENDERING/loading, heavy/SSAA pixel equality, no hidden scene/effect/
SDK work and direct Software-to-D3D12 transitions for both models pass.
Mono K/M/K passes 96 evaluations, 24 held uses and 120 once-only frame ends
without SDK warnings. Plain-menu raw work-time rows are not isolated speed
acceptance. No speed/FPS claim is made while other projects' builds continue.
No further pause is requested.

Rejected attempts remain separate: an obsolete loose adapter lacked the V2
model entry point; an integer phase target was unsupported by SDL and only that
owned diagnostic was stopped; early scope/OFF scalar-oracle policies were
corrected; intermediate diagnostic runs were stopped before replacing their
shader. The invalid ordinary-app case name failed preflight without GPU tests.
None is accepted evidence. Full native effect/secondary correspondence,
physical Leia/Android and sustained performance remain open; Ally is deferred,
the full goal active. No release/install/push/SDK-payload change.
Release EXE `828D98F4...`, both preferences, ReShade configuration/log and the
ordinary Android APK `5E96D652...` remain unchanged. All owned diagnostics ended.

## October 3 — phase-aware native TAA for point patterns

Previous development PC:
`8EB8241CBAD178107C65F0858AD29FD3685E7D6B435D53009FF83A66D761AFB9`.
Native calibrated TAA now tracks Dithered, Night Vision, Scanlines and CRT
Phosphor by the actual post-pass order, receiver, material grid and intensity.
The existing accepted metadata image stores the exact per-pixel phase: no new
GPU image, readback or history pass is added. Any positive eligible history tap
with a different phase rejects the entire footprint. Clamp neighbours also need
the same phase. Centre reconstruction retains the independently styled centre
raster when de-jittering would mix phases. Empty pattern descriptors retain the
ordinary path and bypass phase reads. Static point-pattern clocks no longer
advance held preview jitter/history merely because unused time ticks.

This is not a blanket relaxation of temporal guards. Native SDK/DLSS still
rejects these patterns until its full input-AA/previous-footprint correspondence
is implemented. Neighbouring/warped styles, actual liquid and untracked reflected
transport retain their existing rejection. Native source/settings validation
was not weakened; the intensity/grid/pass descriptor is also an accepted-history
key, and cancelled/rejected candidates cannot replace it.

The final component checker is
`E213300BDDFC18ECB8091193557A59F8F6E6CA3BA0B5FCE84CF5C1102A342402`.
Both actual NVIDIA D3D12 and Vulkan pass RGBA/BGRA linear/sRGB, all four patterns,
1/2/4 material grids, WORLD/MODEL/both, 35/100% intensity, ordered mixed passes,
same-phase reuse, fractional/cross-phase rejection, descriptor changes, wet/UI
protection, opacity and cancelled/submitted-rejected candidates. Each format
checks 2,772,000 independent pattern-history bytes; selected patterned receivers
must demonstrate positive reuse at the larger grids. Existing geometry/liquid/
presentation TAA oracles pass too
(`taa-pattern-component-accepted-{d3d12,vulkan}-oct3.log`).

The final real-queue native owner checker is
`62A422D913E602AE6C9008D6022C6060BC4D9AAACAA3D8A7EB174CB96A33C7EF`.
Both NVIDIA APIs pass 20 pattern/format cases each: four MODEL styles plus legal
Night Vision MODEL / CRT WORLD, all three qualities and both eyes. Independent
FOV/motion/phase/centre-image oracles, recycled generations/morph/cuts, static
clock ticks, held/OFF, rejected presentations, XR/image/GPU waits and in-flight
cancellation pass (`taa-pattern-owner-accepted-{d3d12,vulkan}-oct3.log`). The mixed
WORLD case's analytic motion oracle covers dry foreground, not liquid or complete
world transport. Component tests cover WORLD phase eligibility separately.
Earlier illegal Manipulation/FX selections and a Vulkan fixture extent changed
after discovery were rejected; they are not accepted owner runs. The corrected
fixture fixes discovery order and asserts input image extents explicitly.

Ten CPU suites pass on the current sources (`taa-pattern-final-cpu-oct3.log`).
The current PC renews 40 exact ordinary app images across NVIDIA D3D12/Vulkan:
format 1, water/reflections/bloom, MSAA4 and failed-left mono recovery
(`taa-pattern-app-accepted-{d3d12,vulkan}-oct3/results.json`). Mono embedded K/M/K
preview passes 96 evaluations, 24 held uses and 120 once-only frame ends without
SDK warnings (`taa-pattern-dlss-oct3/toggle.log`). This is not a renewed full
native two-eye SDK matrix, all-effects parity or a physical Leia test.
All 23 Preview-OFF menu cases and separate RENDERING/loading pass on this hash,
including heavy/SSAA settings and direct Software-to-D3D12 transitions for both
SDK models (`taa-pattern-plain-menu-oct3/results.json`). Plain UI pixels remain
unchanged with no hidden scene/effect/SDK work; preferences and the injector log
are unchanged. The menu tool's raw work-time rows are not isolated performance
acceptance and are not used for an FPS/speed claim.

No timing claim is made. Other projects' builds are left running, and no further
pause is requested. Broader native effect/secondary correspondence, physical
Leia/Android and sustained performance remain open; Ally remains deferred and
the full goal active. No release, install, push, signing-key or SDK-payload change.
Release EXE `828D98F4...`, both preferences, ReShade configuration/log and the
preceding ordinary Android APK `5E96D652...` remain unchanged this turn.

## October 3 — default bounded ordinary-stereo submissions

Previous development PC:
`51EF42F129F0167878C0E1144F13F605957B950133FBD7F0982E7B822038193A`.
The full graphics checkpoint is
`C8E28DE9A41A6744A03A371A6758A30A0F870BC39AD209B57A93829ED709D3BC`;
the final rebuild only enables the one-time policy marker on the existing quiet
stereo-result channel, rather than requiring verbose per-frame GPU tracing.
Independent ordinary-stereo compositor/effects owners now use the retained,
bounded submission queue automatically. All consumers are submitted on SDL's
same queue before the next frame can overwrite backing resources. Each owner
retains at most two older submissions plus the next encoded frame; completed
fences are joined before release. Explicit captures, policy changes and teardown
drain pending work. Shared owners, mono and separated-model fallback retain
synchronous reuse. `STARFOX_TEST_STEREO_WAIT` is the isolated synchronous control;
owner isolation still requires the explicit diagnostic queue override.
No geometry, projection, effect shader, quality setting or simulation was changed.

On the graphics checkpoint, both NVIDIA backends pass the full automatic-queue/
synchronous app comparison:
19 cases and 95 byte-identical images per backend, 190 total, across all eight
stereo formats, reflective water/bloom, 4x upscale, upscale AA, 2/4/8 MSAA,
lava/exposure, banked ground, fog, production motion and failed-left mono recovery
(`sbs-default-queue-{vulkan,direct3d12}-oct3/results.json`). Successful candidates
require automatic policy and both actual per-eye owners; device loss, incomplete
pairs, replay or effects fallback cannot qualify. Saved preferences are unchanged.
The final diagnostic rebuild renews format 1, reflective water/bloom, 4x MSAA
and failed-left mono recovery: 20 exact images per NVIDIA API, 40 total
(`sbs-default-queue-quiet-final-{vulkan,direct3d12}-oct3/results.json`).

The strengthened linked checker is
`9B5FB15351718DD1F00512686354BB36F567743D2490A08FC7471B11D1BB496F`.
NVIDIA D3D12 and Intel D3D12/Vulkan each pass 96 retained 512x448 and 128 retained
360x270 frames against synchronous output. These checks change queue policy on
the same owners, inject malformed composition/effects requests, recreate effects
with pending submissions, animate resident geometry/palettes/coverage/history,
and verify explicit final readback. No per-frame image readback hides reuse.
Evidence is `sbs-default-queue-transitions-nvidia-d3d12-r2-oct3.log` and
`sbs-default-queue-transitions-intel-{d3d12,vulkan}-final-oct3.log`.
The earlier NVIDIA-labelled D3D12 attempt actually selected Intel; the attempted
NVIDIA Vulkan route also selected Intel. Their hardware assertions rejected them;
neither is NVIDIA fixture acceptance. Actual adapter names are logged, not inferred
from requested preferences. App-level NVIDIA Vulkan acceptance above is separate.

Ten CPU suites pass, including automatic/shared-owner/wait policy and retained-fence
regressions (`sbs-default-queue-cpu{,-extra,-msaa}-oct3.log`). Stereo/helper targets
were rebuilt; unchanged CPU targets were executed, not all rebuilt. Conflicting
queue comparison switches reject before output creation or launching the app.
The final rebuild also passes nine CPU suites plus the separately named calibrated
DLSS ABI suite (`sbs-default-queue-quiet-final-{cpu,dlss-api-cpu}-oct3.log`).
Final mono K/M/K preview passes 96 evaluations, 24 held uses and 120 once-only
frame ends without SDK warnings (`sbs-default-queue-quiet-final-dlss-oct3/toggle.log`).
The first plain-menu run matched its pixels but is rejected as clean-isolation
evidence: its script omitted the installed Vulkan ReShade layer's per-process
opt-out. That run rewrote `build/current/ReShade.log`, not the configuration.
The script now opts out, restores the caller's environment and rejects any
injector-log change. Only its clean rerun qualifies; the original run and copied
injector logs remain retained. No ReShade manifest or configuration was edited.
The final isolated rerun passes all 23 Preview-OFF cases and the separate
RENDERING/loaded-preview capture
(`sbs-default-queue-quiet-final-plain-menu-oct3/results.json`). Heavy settings and
SSAA preserve plain-menu pixels; scene/effects/SDK work is absent and unchanged
UI textures are retained. Both Software-to-GPU SDK-model transitions restore
direct D3D12. Its preference and injector-log hash checks pass. The already
rewritten log remains `BBF6761E...`; it was not restored or deleted to conceal
the rejected run. Menu work-time rows are diagnostic, not isolated performance.
The first timing batch completed its synchronous frames but rejected the missing
quiet policy marker (`sbs-default-queue-timing-d3d12-oct3`); the diagnostic rebuild
fixes that mismatch without per-frame logging. Its second batch was refused before
launch for live C++/CMake/Ninja work (`sbs-default-queue-timing-d3d12-r2-oct3/rejected.json`).
The guard also now recognizes C's `cc1`, C++ driver wrappers and MSBuild. Its
classification regression includes those compilers and excludes the driver shell.
The third batch reached its last run but observed C/CMake/GCC/Ninja overlap
(`sbs-default-queue-timing-d3d12-r3-oct3/rejected.json`). Its same live app was
awaited to completion, not terminated. The whole batch is rejected, including
the seven earlier timing rows; there is no accepted `results.json`. Completed
quiet-channel runs do verify the one-time automatic/forced policy and actual
per-eye submission markers, but their partial statistics are not speed evidence.
No timing is accepted from this checkpoint and no competing job was stopped.
Physical Leia/Android, broader native
effect correspondence and sustained performance remain open. Ally remains
user-deferred and the complete goal stays active. Release/install/push is not part
of this checkpoint. Final hashes confirm the release executable, both preference
files, ReShade configuration and preceding ordinary APK are unchanged. The
generated injector log changed only in the rejected first menu run and stayed
unchanged through the final clean checks. All owned game/checker runs are terminal.

## October 3 — native DXGI DLSS presentation and cancelled-token recovery

Previous development PC:
`807730CC783AD9C6EC52A3BC3F54DAF9D5E04D409505D1E0791B420B8D125DCC`.
Ordinary mono DLSS now keeps SDL's original DXGI swapchain and invokes the
existing signed-runtime adapter's common-plugin frame-end ABI explicitly.
This uses the same ABI as native XR, not a second SDK or dummy presentation.
Only real SDK attempts and submitted held reconstructions create tickets;
plain menus/black startup frames do not. Reconfiguration and teardown drain
pending tickets before freeing resources. Older adapters without the ABI retain
the wrapper fallback; `STARFOX_TEST_DLSS_PRESENT_PROXY` is the isolated A/B control.
The historical refcount-1 warning is absent on the new path, not suppressed.
The proxy control still reports it while producing identical pixels.

A real post-evaluation command-cancellation test exposed a separate token bug:
the mono frame index advanced only after successful GPU submission. The SDK had
already consumed that CPU token, so the next attempt reused it with different
constants and repeatedly failed with `eErrorDuplicatedConstants`. Indices now
advance at each actual SDK call; accepted preview samples still advance only
after successful submission. History resets after cancellation, without treating
that discarded evaluation as an accumulated sample.

`dlss-explicit-presentation-r2-oct3/results.json` passes eight real SDK cases:
24 byte-identical proxy/explicit images across Original moving scenes and EX
held previews, using both K/Quality and M/DLAA; four additional Original/EX,
K/M cancellations match the first fallback exactly to unjittered mono and each
resume 39 evaluations with reset only on the recovery frame. All eight cases
finish exactly 40 SDK tickets each, 320 in total, without SDK errors/warnings on
the explicit path. Recovered Original/K and EX/M final images were inspected.
This is scoped reconstruction/lifecycle evidence, not all-effects image quality.

Rejected attempts are retained: `FCFAA368...` matched the normal presentation
images but failed the new cancellation test with repeated duplicate constants
(`dlss-explicit-presentation-final-oct3`); it is not recovery acceptance. The
embedded-only checker initially rejected `build/current` because it contains an
adjacent `dlss` directory. No user folder was removed. A same-hash EXE-only host
under `dlss-explicit-embedded-host-oct3` then passes all eight K/M quality modes
with 16 real evaluations/frame ends each and both D3D12/Vulkan OFF controls
(`dlss-explicit-embedded-isolated-oct3`). It cannot load loose adjacent SDK files.

Nine CPU suites plus the separate DisplayXR suite pass, including the freshly
linked no-XR ABI/lifecycle target (idle/once-only/held/cancelled, failed/throwing
notifications and restart). Unchanged CPU targets were executed, not all relinked.
The native calibrated SDK owner implementation and shader payloads are unchanged;
this does not renew its full hardware-owner matrix or physical panel acceptance.
Final `dlss-explicit-model-final-oct3` passes K/M/K menu preview with 96 real
evaluations, 24 held uses and 120 once-only frame ends. Final OFF/ON/OFF passes
32 evaluations, 8 held uses and 40 frame ends, no upgraded/restored proxy chains,
and the final OFF backend returns to native Vulkan
(`dlss-explicit-off-on-off-final-oct3`). Both checks use no image readback.
`dlss-explicit-plain-menu-final-oct3/results.json` passes all 23 Preview-OFF
cases plus the separate RENDERING/loaded-preview capture. Heavy effects/SSAA
preserve plain-menu pixels, SDK notifications and scene/effect work stay absent,
and both Software-to-GPU model transitions remain direct D3D12. Saved preferences
are unchanged. Its diagnostic work-time rows are not isolated timing evidence.
Final ordinary SBS checks on this same `807730CC...` binary pass 20 exact images
per backend, 40 total, across format 1, reflective water/bloom, 4x MSAA and
failed-left mono recovery (`dlss-explicit-stereo-{vulkan,direct3d12}-oct3/results.json`).
These use the normal synchronous path; the queued experiment remains OFF.
They do not renew every stereo format or the calibrated native SDK matrix.
All owned game/checker processes are terminal. Final hashes confirm the release
executable, both saved preference files, current ReShade files and ordinary
Android APK are unchanged.
Other compiler jobs stay running. No new timing result/default queued-SBS
promotion, physical Leia/Android acceptance, release or installation is claimed.
Broader native correspondence and sustained SBS performance remain open; Ally
stays user-deferred and the full goal remains active.

## October 3 — queued SBS fence lifetime

Previous development PC:
`A97EB3CB8F1CC6866C3C82EA89FBB4794BA879272D2FF1EC3ADA972440909960`.
The graphics-fix checkpoint before adding timing markers is
`12905742F81B7197F4B611F606496C508140D5477CF48356B9666A4B867D834C`.
Normal rendering still does not enable `STARFOX_TEST_STEREO_ORDERED_QUEUE`.

The old experiment released acquired fences before their commands completed.
The pinned SDL Vulkan backend returns released fences to its pool and resets
them on reacquisition; ordering GPU buffer consumers does not make that pending
fence reusable. The compositor and effects owner both had this lifetime error.
Each now retains every pending fence, retires completed work after joining
backend cleanup, and waits for the oldest only when the queue exceeds two older
submissions. Explicit readback, disabling reuse and destruction drain retained
work. Effects timestamp tickets retire with their real fences rather than
controlling whether fences are kept alive.

The pre-fix small fixture passed, but the full reflective-water/bloom app test
failed with Vulkan device loss. Separate compositor-only and effects-only
captures also failed. Those rejected attempts are
`ordered-reuse-app-vulkan-oct3` and `ordered-reuse-water-{validation,composite,effects}-vulkan-oct3`;
they are not accepted renderings or measurements. The stronger component fixture
retains all images before inspection, updates resident geometry without CPU
readback, and changes palettes, foreground coverage, uniform/striped/dense
uploads, bloom, exposure, phosphor and history modes. Both backends pass 96
512x448 and 128 360x270 frames compared exactly with synchronous rendering
on the final linked checker
`56DB765ED87D160010DBCDDFAC232AF99F2EF7A99B52A6EBCBA3A0F18BFA9202`
(`ordered-reuse-final-stress-{vulkan,direct3d12}-oct3.log`).
Ten CPU suites pass, including a regression that rejects premature fence release,
checks bounded queue pressure and preserves entries on a failed wait
(`ordered-reuse-fence-cpu-oct3.log`).

On `12905742...`, the full app compares queued and synchronous output exactly:
42 images per backend across format 1, real 4x MSAA, reflective water/bloom,
banked ground, production motion and failed-left mono recovery. Every successful
candidate requires both actual compositor/effects owners and every requested
complete GPU pair; device loss/replay/fallback cannot qualify as success
(`ordered-reuse-fence-app-{vulkan,d3d12}-oct3/results.json`).

Final `A97EB3CB...` repeats reflective water/bloom, 4x MSAA, production motion
and failed-left mono recovery: 20 exact images per backend, 40 in total
(`ordered-reuse-final-app-{vulkan,direct3d12}-oct3/results.json`). The candidate
must report both actual queued owners; the reference must report neither.
These narrower final checks do not renew all eight stereo formats or the full
calibrated SDK matrix. Ordinary embedded Standard/4.5/Standard menu preview
passes 96 evaluations / 24 held uses, resetting history on each model change
(`ordered-reuse-final-dlss-oct3/toggle.log`). The known mono SDK shutdown/refcount
warning remains; this fence change does not fix it.

The first automatic-span timing attempt on the preceding `D6E2BF61...` was
rejected before launch for other compiler jobs. Those jobs were not stopped or
paused. The guarded harness now also supports an isolated ordered-queue A/B
and records preflight rejections; it rejects observed compilation overlap and
never terminates other work. The final Vulkan A/B batch is rejected at
`05-ordered-queue` after observing `cmake`, `g++`, `gcc` and `ninja`
(`ordered-reuse-fence-timing-vulkan-oct3/rejected.json`). The first four partial
rows are not accepted timing evidence. All owned test processes are terminal;
other compiler jobs were left running, as requested.
No new sustained FPS, mobile/physical-display performance or default promotion
is established by the image checks. Native effect correspondence and physical
Leia/Android acceptance remain open; Ally troubleshooting stays user-deferred.
The full goal remains active. Release, settings, ReShade and the ordinary APK
are unchanged; no push/install.

## October 3 — interruption-safe settings and mobile build recovery

Previous development PC:
`D6E2BF6155CAD34E703CFE1EA212509E77170FBA5F450C9B86D7C42F3DE2026B`.
Ordinary Android APK (`platform/android/app/build/outputs/apk/debug/app-debug.apk`):
`5E96D652092DBDB543243C4983B0101835918F1D977FB38A50D326571E4DB7B0`.

Settings previously truncated the live file and reported success before flush
and close. Pregame settings and GPU session/policy journals now use an
exclusively created sibling temporary, check write/flush/close, and replace the
destination only after success. Failed or abandoned writes preserve the last
complete file. Unrelated collision files, directories and symlinks are not
overwritten; stale temporaries are not treated as settings or deleted globally.
This protects process interruption, not arbitrary power/filesystem failure.

Nine CPU suites pass, including runtime input, UWP input and a standalone DLSS
ABI target without XR include paths. Real-file tests cover abandonment, five
I/O failure stages, overlapping transactions, Unicode paths and exclusive-create
collisions. `atomic-recovery-interruption-oct3/results.json` confirms a real
forced process close during a temporary settings write preserves the original
file, which then loads and saves successfully with unrelated options intact.
Fourteen real app recovery cases pass across D3D12/Vulkan, including forced
GPU close, next-boot Software recovery, explicit retry, blocked journals and
no per-frame retry loop (`atomic-recovery-{d3d12-r2,vulkan}-oct3`). These are
host recovery checks, not reproduction or acceptance on an Android device.

The Android build exposed an accidental dependency from the host's DLSS API
description to OpenXR camera math. That small ABI now has its own header;
ordinary mobile builds no longer need XR headers just to return the unavailable
DLSS stub. The first build's missing-OpenXR error was fixed, not bypassed.
The offline Gradle build completes and the ordinary APK passes arm64/runtime,
private-payload exclusion and all 38 backdrop checks; four package negative
fixtures pass. An earlier package check against the September 25 APK was
rejected for missing Kazaru artwork; only the newly built APK above is accepted.
The aggregate Gradle task also built Quest, but Quest packaging/device behavior
is not renewed by the ordinary APK check.

Ordinary embedded K/M/K preview still passes 96 evaluations / 24 held uses
with history reset on model changes (`atomic-recovery-dlss-model-oct3`). The
known mono SDK shutdown/refcount warning remains. Graphics implementation is
unchanged from the preceding ordinary-MSAA checkpoint. Fresh D3D12/Vulkan
snapshot/direct MSAA and failed-left mono comparisons pass 40 exact images
(`atomic-recovery-msaa-{d3d12,vulkan}-oct3/results.json`). Both routes require
actual 2/4/8 sample and late-palette markers. All 30 direct MSAA images also
match the preceding accepted per-backend `F3066240...` baseline exactly
(`atomic-recovery-msaa-baseline-oct3.json`). They do not renew the full calibrated
SDK matrix, three-route numerical coverage oracle or physical Leia acceptance.

The `F3066240...` automatic-clear timing batch was rejected on its second run
when unrelated compiler jobs appeared
(`default-spans-timing-d3d12-venom-1x-f306-oct3/rejected.json`). Those jobs were
left running as requested; partial rows are not accepted timing evidence.
ADB has no attached device. Physical Android relaunch/Leia, broader native
effect correspondence and sustained SBS performance remain open; Ally remains
user-deferred and the full goal remains active. Release EXE, both real saved
configurations and ReShade INI/log keep their previous hashes. No push/install.

## October 3 — real ordinary SBS MSAA

Previous development PC:
`F3066240890A7736A58454456268A2FADA96F1B8B59C0E8BBF51457ABFE20D4E`.
Stereo checker:
`8196C6DF1DE43298148515710E5F47442091DB1E37FCBDC978FBB1BBFEA6891D`.
Clip checker is unchanged from the automatic-clear checkpoint below.

Ordinary SBS now requests independent 2/4/8 indexed coverage samples for
Low/Medium/High MSAA, using the existing portable producer. Split, joined,
parallel and borrowed encoders all forward the sample settings. Both eyes
retain their own sample identities and resolve the final presentation palette
without replaying geometry. Held sample-count changes, including OFF, rebuild
coverage when needed. A failed resolve invalidates publication of the entire
pair, even if the left eye recolored successfully. FSR/DLSS incompatibility
gates remain unchanged. This is not the separate calibrated hardware-MSAA path.

`stereo-msaa-{d3d12,vulkan,vulkan-intel}-r2-oct3.log` pass the independent
hand-projected coverage oracle on NVIDIA D3D12/Vulkan and Intel Vulkan. Each
tests 54 real 2/4/8 cases across split/joined/parallel owners, 162 retained
palette resolves and 10,450,944 numeric RGBA bytes, plus recovery/cancellation,
empty clears, OFF, resize and borrowed encoding. The old independent CPU eye,
motion and recycled-owner checks also pass in each full stereo run. The first
failed fixture incorrectly treated the source's packed `0x11` material as
host palette index 17; source material decoding requires solid index 1.
Correcting that expectation did not weaken geometric coverage or byte bounds.

`stereo-msaa-presentation-{d3d12,vulkan}-r2-oct3/results.json` each pass
15 exact snapshot/direct MSAA images and 5 existing failed-left-eye mono
images. Actual per-eye sample count and late-palette activation are required;
the first capture without the activation marker was rejected. The new
`stereo-msaa-live-{d3d12,vulkan}-oct3/results.json` each prove all 15 active
images differ from matching OFF images and that all 32 injected right-eye
palette failures decline stereo; 5 captured fallback frames match the actual
MSAA mono reference byte-for-byte. This is not selected-AA/no-op acceptance.

Six freshly linked CPU suites pass (MSAA, stereo output, pixel filters,
temporal AA, source materials and DisplayXR). Ordinary embedded K/M/K menu
preview passes 96 evaluations / 24 held uses in
`stereo-msaa-dlss-model-oct3/toggle.log`. The previous full calibrated native
SDK matrix and physical Leia/Android acceptance are not renewed by this work.
The non-MSAA all-format automatic-clear acceptance below retains its older
binary scope; the implementation remains unchanged.

An automatic-clear 1x Venom timing attempt on the preceding `33103828...`
binary was rejected during its first run when unrelated cmake/gcc/ninja jobs
restarted (`default-spans-timing-d3d12-venom-1x-oct3/rejected.json`). Those jobs
remain running as requested. No new default-policy or MSAA speed, sustained
FPS or Android/Metal performance claim is made. The full goal remains active;
broader native effect correspondence and device acceptance remain open, and
Ally troubleshooting remains user-deferred. Release, both saved configurations
and ReShade INI/log retain their previous hashes; no push or install.

## October 3 — automatic large-batch span clearing

Previous development PC:
`331038289A2B2C468332385077830DEF51A93DE562AA236BD8800571A9AD5C94`.
Clip checker:
`476D3E0825844D3579981BF4DE2481E9F4B33843E9A1AE00F6E97D40A9DA2AC8`.
Stereo checker:
`110252EBB80958C7D1EC10DE238D7C3A83D31B2607F83F0F264BE64495F6ED2F`.

The dedicated clearer is now selected automatically for at least 4096 span
rows, or any MSAA sample-mask batch. Small non-MSAA batches keep the original
single tracer dispatch. The threshold is a dispatch-overhead guard, not a
measured optimum for every device. Explicit full-clear overrides win, followed
by forced parallel and legacy serial-bounds overrides. Automatic selection
does not change geometry, painter order, scratch budgets or consumer lifetime.
Optional diagnostics report each observed mode once per owner, avoiding log
churn when small and large batches alternate.

Fresh `default-spans-reuse-{direct3d12-nvidia,vulkan-nvidia,vulkan-intel}-oct3.log`
checks four independent full/bounds/parallel/automatic owners across 192
poisoned/recycled cases per adapter. Each covers 40 automatic full and 152
automatic parallel cases, both sides of the 4096-row boundary, copied prefixes,
repeated rows, 2/4/8 sample masks and all live command/raster/surface bytes.
There are 1,375,631,424 compared bytes plus the existing nine independent
minimal-writer cases (167,789,168 bytes, including two large 2D dispatches)
per adapter. Six CPU suites, nine shader-source/binding units, both clear
shader payload checks and edited PowerShell syntax pass.

`default-spans-presentation-{d3d12,vulkan}-oct3/results.json` each pass 80 exact images
across all eight stereo formats, reflective water/bloom, 4x, SMAA, EX lava,
camera banking, fog, production motion blur and failed-left-eye mono fallback.
Candidate captures assert automatic selection without a forced-clear flag;
references force the original full clear. Both full clipping/stereo oracles
also pass on NVIDIA D3D12/Vulkan (`default-spans-full-*-oct3.log`), including
independent CPU coverage/motion, near-plane/residual boundaries and 96 changing
sparse-to-motion helper batches. Ordinary embedded K/M/K preview passes 96
real evaluations / 24 held uses (`default-spans-dlss-model-oct3-driver.log`);
the separate OFF/ON/OFF preview also passes 32 evaluations / 8 held uses.
The previous full native calibrated SDK matrix is not renewed by these mono
and span checks; its existing scope remains in the stereo checkpoint.

The earlier E241 1x Venom r2 batch was rejected when other compiler jobs
restarted before run seven. Six partial rows are not accepted performance
evidence. Other builds stay running as requested. The two accepted 2x timings
below measure forced clearing on E241, **not this new automatic threshold**;
no new default-policy FPS or Android/Metal performance claim is made yet.
Fresh automatic-policy timing was not launched while cmake/gcc/ninja were
active after the correctness runs. The timing harness can now compare
`default-spans` against full clear and rejects forced-clear flags in the
automatic arm. Correctness captures are not promoted into timing evidence.
Ordinary SBS MSAA remains a real compatibility gap, not a passing no-op test.
The broader Leia/effects/device goal remains active. Release, both saved
configurations and ReShade INI/log are unchanged; no push or install.

## October 2 — dedicated span/mask clear and first accepted SBS timings

Previous development PC:
`E2413182262F58E5C3B99FC950F8AD10517C83CE3BE4CF13312AC58BF82693A2`.
Clip checker:
`DAE9F110B4E4CFC17099A34267037C26D48B0DCEA21103FFB032C2F57D682887`.
Stereo checker:
`5DC9B7454B0C7D2FE4648A64D01DE46D6FC8B36362712A60DEF2557D556DF9F6`.

The parallel-clear experiment now uses a dedicated 128-lane byte writer with
no clipping/material/order reads. It clears only the complete validity uint4
of each 96-byte command. When MSAA row masks exist, the same pass clears their
sample planes after the preserved source-texel prefix, eliminating the second
mask-clear pass. Non-MSAA mask cycling remains owned by the normal tracer.
The minimal pipeline is lazy and released with its device. The general span
shader no longer contains the unused bounds-clear mode. A balanced 2D dispatch
passes its actual row stride to the shader rather than assuming 65535 groups.
The experiment remains test-only/OFF; reference and explicit overrides remain.

Two complete eight-run ABBA/BAAB batches have **accepted, scoped timing evidence**.
Each uses this same executable, full SBS, 2x scale, 240 complete pairs per run,
60-pair query warmup, four repetitions per arm and no observed compiler/test
overlap. Actual adapter identity is now required and constant across the batch.
Other builds were neither stopped nor modified.

| Adapter/API and workload | Mean GPU scene per pair, full → parallel | Mean host median, full → parallel | Mean host p95, full → parallel |
| --- | --- | --- | --- |
| NVIDIA RTX 5070 Ti Laptop / D3D12, Original Corneria | 5.523 → 3.762 ms (-31.9%) | 5.720 → 4.028 ms | 8.862 → 6.830 ms |
| Intel Graphics / Vulkan, Original Venom | 12.412 → 11.555 ms (-6.9%) | 13.414 → 12.425 ms | 15.686 → 15.219 ms |

Evidence: `D:/SFE-validation/minimal-spans-timing-d3d12-oct2/results.json`
and `minimal-spans-timing-intel-vulkan-venom-oct2/results.json`. These are hidden,
unpaced instrumented workloads, not sustained visible FPS, ray-queue timings,
every stage/scale or every device. Integrated Venom no longer repeats the
earlier serial-bounds experiment's regression in this tested workload. The
older C119 `parallel-spans-timing-d3d12-oct2-r3` batch was rejected when its
eighth run overlapped cmake/gcc/ninja; its first seven rows are not promoted.

Fresh `minimal-spans-reuse-{direct3d12-nvidia,vulkan-nvidia,vulkan-intel}-oct2.log`
checks each actual adapter/API: 96 independent full/bounds/parallel recycled
command/raster/mask cases plus nine minimal-shader byte-oracle cases. The latter
include two large 2D mask dispatches, preserved command payload/unused rows and
source-prefix/tail guards, checking 167,789,168 bytes per adapter. All pass.
Both full NVIDIA clipping/stereo oracles also pass; six CPU suites, nine shader
units, 61 common shader payload checks and edited PowerShell syntax pass.
`minimal-spans-presentation-{d3d12,vulkan}-oct2/results.json` adds 35/25 exact
app images on E241, including reflective water/bloom, AA, banking and failed-eye
mono recovery; D3D12 also covers 4x and production motion blur. Ordinary embedded
K/M/K preview passes 96 real evaluations / 24 held uses
(`minimal-spans-dlss-oct2-driver.log`). The Vulkan checker completed normally;
a surrounding shell incorrectly treated its unset `$LASTEXITCODE` as failure.
Its completed result file was inspected, not replaced by a rerun. The SDK test
then ran separately. The existing mono shutdown warning remains.

A proposed live SBS/MSAA capture was correctly rejected for missing actual
sample-mask activation (`minimal-spans-msaa-d3d12-oct2`). Source inspection
confirms ordinary SBS excludes MSAA in `prepare_msaa`/`use_msaa`; calibrated
Leia's hardware MSAA is a different path. The provisional capture cases were
removed, not weakened into passing no-op tests. Component mask tests prove the
new clearer, **not ordinary SBS MSAA support**. That compatibility gap remains.

Broader scale/stage/backend performance and default rollout still need work.
The later NVIDIA 1x Venom attempt (`minimal-spans-timing-d3d12-venom-1x-oct2`)
refused to launch because cmake/gcc/ninja were active. No timing rows exist for
that attempt; the successful two batches above are not broadened to 1x.
The complete Leia/effects/device goal remains active; the existing native
SDK owner matrix is not renewed by these clear tests. Release, saved preferences
and ReShade INI/log remain unchanged. No push or install.

## October 2 — parallel span-clear candidate; correctness only

Previous development PC:
`C119BF6F6F7C818E8F1777D7B9250BF62601C07EF5BBEAB9A62317D3B47BB2A9`.
Clip checker:
`F333BF281D64B371FE0A9991F9C9CBEBB03D3014F5A679238C8FB5C0B183FBDC`.
Stereo checker:
`115970AAFC3522FB8BE810FB5C6C3605C9075E34ACB2A8A2C8D0FC95ECBE7352`.

The test-only `STARFOX_TEST_PARALLEL_SPAN_CLEAR=1` candidate clears each row's
four validity bounds in a parallel ordered pass, then runs the unchanged span
tracer. It preserves projection/UV stepping, painter order, scratch capacity,
source texel prefixes and MSAA masks. The first pass owns any span cycling;
subsequent passes reuse those same buffers. It adds one compute pass, not a
submission, fence, readback or resource allocation. Full-clear override wins.
The default remains full clear; the previously rejected serial bounds-clear
and grouped-divider experiments remain separate and OFF.

Optional draw diagnostics now separate preparation, clipping, span emission
and painting. Query storage still retires on the original submission fence.
No additional timestamp resources or GPU operations exist with diagnostics
disabled. Summarizers explicitly sort numeric hashtable serial keys and avoid
the .NET Core-only `IsFinite` API, fixing Windows PowerShell 5 behavior without
relaxing duplicate/missing serial, phase-total, batch or cancellation checks.
Synthetic fixtures pass 19 assertions each on PowerShell 5.1 and 7.6.

Fresh correctness evidence under `D:/SFE-validation/`:

- `parallel-spans-masks-{d3d12,vulkan,intel}-oct2.log`: each actual adapter/API
  passes 96 poisoned/recycled-buffer cases, 0/2/4/8-sample masks and 1x/2x/4x
  scaling. Full, serial-bounds and parallel-bounds owners compare 5,186 live
  and 109,502 empty rows, raster output/metadata and complete mask storage.
  This does not exercise the clear dispatch's two-dimensional group boundary.
- `parallel-spans-full-{clip,stereo}-{direct3d12,vulkan}-oct2.log`: both NVIDIA
  APIs pass full independent clipping/raster/metadata and eye/motion oracles,
  including allocation reuse and sparse-to-motion helper transitions.
- `parallel-spans-presentation-{d3d12,vulkan}-oct2/results.json`: respectively
  35 and 25 byte-identical app images, isolating full versus parallel clear.
  Ordinary SBS, reflective water/bloom, AA, banked ground and failed-eye mono
  recovery are covered; D3D12 also covers 4x upscale and production motion blur.
  This is not an all-format/all-effect presentation matrix.
- `parallel-spans-dlss-oct2-driver.log`: ordinary embedded K/M/K preview passes
  96 real evaluations and 24 held uses. Five CPU suites and nine shader source/
  binding units pass. The prior full native-SDK owner matrix below is not renewed
  by this ordinary mono preview check; its known shutdown warning remains.

There is **no accepted performance A/B**. `parallel-spans-timing-d3d12-oct2`
and `-r2` refused to launch while external compiler jobs ran. Earlier
`clip-split-timing-d3d12-oct2-r2/rejected.json` records an overlapped run; its
timestamps validate parsing/phase ownership only, not performance. No partial
batch is promoted. The user requested that other builds keep running, so no
further quiet-window requests or build interruptions were made.

Release `828D98F4...`, both saved preferences and ReShade INI/log retain their
hashes/timestamps. No push or device install was made. Physical Leia/Android,
remaining native temporal correspondence and sustained SBS improvement remain
open; Ally troubleshooting is user-deferred. The complete goal stays active.

## October 2 — native ownership-local temporal rejection, not a speed claim

Previous PC is `9B43BBC5...`. Native untracked post styles no longer discard
clean-layer temporal motion. TAA stores no rejected samples as eligible history;
SDK packing checks actual SSAA/MSAA ownership coverage in its existing pass.
RT-OFF water retains genuine dry-model TAA. Cross-layer transport/reflection
guards remain. Both TAA APIs/four formats and real-SDK component guide oracles
pass, as do the final four-format SDK owner matrix (96 new post cases and
existing quality/AA/ray/ground/liquid/lifecycle coverage) and ordinary embedded
K/M/K preview (96 evaluations / 24 held uses). See the
newest stereo checkpoint for hashes, scope, fixture repairs and evidence.
No FPS benchmark was run for this change. The exact grouped clipping candidate
remains OFF, and the earlier contaminated timing batches remain rejected.
Physical Leia/Android and sustained SBS speed remain open; the full goal is active.

## October 2 — exact grouped-divider candidate; timing rejected

Previous development PC:
`32B1F50FCFE883B27249C4C22A109849A9FB5BB36A95C9ABE6409534BD368B04`.
Clip checker:
`DEAFA998E71944F8588FE47C42E936BE29DAAAC0F60B89965B554177D74DEDCE`.
Stereo checker:
`9A6E85626B0EEA48A4A474B52B8BE2FBC350F2D28B6DB7DC02D8E628929AB32D`.

The exact divider's 56 single-bit iterations can be grouped into fourteen
base-16 iterations. Each digit subtracts precomputed 8D/4D/2D/D multiples from
8R; the remaining numerator is shifted once to reproduce the reference's next
remainder. All intermediates fit uint32 limb pairs; the same 53 significand,
three rounding and sticky bits, nearest-even rule, signed zeros and unsupported
input/result sentinel remain. No native float64 capability, approximate
reciprocal, precision reduction, scratch-capacity change or geometry ABI change
is introduced. All backface, near/intersection, XYUV and order logic is shared
with the reference. The normal/default shader is unchanged in behaviour.

`STARFOX_TEST_RADIX16_CLIP=1` lazily selects only this continuous-clip variant;
`STARFOX_TEST_FULL_CLIP=1` wins. The other experiments remain separate/off.
The optional `clip-radix16: exact binary64 division` marker proves dispatch,
not a speed gain. Production has no new projection/readback/submission/wait.
DXIL, Intel DXIL, SPIR-V and Metal payloads are fresh; generated Metal is not
physical Apple verification.

Accepted correctness evidence in `D:/SFE-validation/radix16-clip-oct2/`:

- `geometry-independent.log`: 1,006,458 candidate and reference divisions
  independently match native double bits, with exponent, rounding, limb,
  sign/zero and invalid-boundary coverage. Five freshly linked CPU CTests pass
  (`ctest.log`); DisplayXR still has 3,372 mocked-dispatch assertions.
- `{direct3d12,vulkan}-radix16-r2.log` and `intel-radix16.log`: each actual
  adapter/API has 5,560 exact headers, 104,424 consumed XYUV words and 60
  independently expected triangles. Raw/mixed/tiny-negative/invalid/near/line/
  billboard/32-corner/reuse/cancellation/explicit-reference override pass.
- Both NVIDIA full clip oracles pass 25,362 integer / 50,724 fractional
  polygons, independent span/raster/metadata and source-near fixtures. Both
  full stereo checks retain independent CPU eye/motion references,
  joined/parallel ownership, cancellation/retry and 96 helper transitions.
- Live app `presentation-direct3d12/results.json`: 35 exact images (ordinary
  full SBS, reflective water, AA, banked ground, EX lava/explosion shutter,
  production motion blur and failed-eye mono recovery). Vulkan counterpart:
  25 images (ordinary/water/AA/banking and failed-eye recovery). Comparisons
  isolate the divider; actual route markers are required.
- Ordinary/default embedded SDK `mono-toggle-driver.log`: K/M/K preview passes
  96 real evaluations / 24 held reuses on this executable. This is not new
  full native owner-matrix or experimental-divider SDK acceptance. The known
  mono swapchain refcount-1 shutdown warning remains.
- All 61 common portable headers match recursive source digests; nine binding/
  include-source units pass. Edited PowerShell scripts parse. Current game
  preferences, release executable/preferences and ReShade INI/log retain their
  hashes/timestamps. No external push or device install was made.

Three profiling attempts have **no accepted timing outcome**. D3D12
`radix16-timing-d3d12-oct2/rejected.json` and its `-r2` counterpart reject runs
06 and 05 after observing cmake/gcc/ninja and gcc respectively. Vulkan
`radix16-timing-vulkan-oct2/rejected.json` rejects its first run. The latter
captures compiler PID/parent information and gcc source paths under
`C:/Users/kando/UrbanRecompBuild/`; the observer initially treated lowercase
`-c` as `-C` when labeling paths, and now uses case-sensitive matching with
separate source-directory fields. This label repair does not alter the
observed-overlap rejection. Earlier rows from partial batches are not promoted
into an A/B/B/A claim. No build/test process was killed or restarted on an
observation timeout. Each child was awaited on its original process handle.

The candidate stays OFF by default; there is no accepted FPS gain or release
promotion. A quiet performance window is still needed. Further meaningful
performance work must separate clipping from span/material setup, or evaluate
the existing calibrated graphics path against the compute painter, not infer a
speed gain from iteration count. Native Leia world/post/fluid/reflection/
secondary correspondence, physical Leia/Android and sustained SBS improvement
remain open. Ally troubleshooting is user-deferred; the full goal stays active.

## October 2 — exact raw-camera projection reuse candidate

Measured development PC (prior clipping checkpoint):
`3CC74B317CD8CEA8386387F0C6E16CE86340C262078B23BEB2A7E85C8AE264FB`.
Stereo checker:
`A3EAECDAB141718587C1B42D8015B8FD34DF4315060301C3096CC7CD5E0CB046`.
Clip checker:
`15413CF7012B5987831C647CC637FA00EF9C4788234C73354BBC68D2650D5B41`.

The prior model-phase diagnosis identified clipping/span preparation as the
large sampled model cost. The continuous raw-camera path projects front source
vertices once for its compensated working coordinates, then projects them
again for exact backface/boundary processing. The new candidate retains the
first binary64 results in the existing pre-screen arrays. It reuses them only
when every source corner has raw-camera tag 3, exact nonnegative depth and no
behind-camera classification, and the packet is not a line. In particular,
negative tiny depths narrowed to float -0 cannot take the cached route.
Near intersections, mixed tags, invalid data, lines, ordering, XYUV/status ABI,
half-open boundaries, precision and full scratch capacities remain as before.
No occupied-tile, identity-plane or other shader experiment is combined with it.

`STARFOX_TEST_CACHED_PROJECTION_CLIP=1` selects the lazy candidate pipeline;
`STARFOX_TEST_FULL_CLIP=1` wins and the normal default remains unchanged. The
optional `clip-projection-cache` marker confirms pipeline dispatch, not a
measured number of cache hits. Clip diagnostic selection now consistently uses
SDL's environment snapshot, allowing the in-process oracle to switch routes
without relying on Windows CRT environment synchronization. Generated DXIL,
SPIR-V and Metal payloads are fresh; Metal generation is not Apple execution.

Completed checks for that hash (artifacts under `D:/SFE-validation/`):

- `cached-projection-clip-component-oct2`: `--projection-cache` passes on actual
  NVIDIA D3D12/Vulkan and Intel D3D12. Each run compares 5,560 headers and
  104,424 consumed XYUV words bit-for-bit, with 60 independently expected exact
  triangles. It exercises raw/mixed/near/invalid/tiny-negative inputs, 32 source
  corners, lines, billboards, four viewport sizes, growth/shrink/reuse,
  cancellation/retry and explicit reference override on the candidate owner.
  Unused stale payload slots are not treated as defined output.
- `cached-projection-clip-health-oct2`: both NVIDIA APIs pass the full clip
  oracles (25,362 integer / 50,724 fractional polygons, independent software
  span/raster/metadata and source-near fixtures) and full stereo checkers,
  including CPU eye/motion references, joined/parallel ownership,
  cancellation/retry and 96 sparse-to-motion helper batches.
- `cached-projection-presentation-d3d12-oct2/results.json`: 35 byte-identical
  live app images. Vulkan counterpart: 25. Ordinary SBS, reflective water,
  banked terrain and AA pass both APIs; D3D12 additionally covers EX lava/shutter
  and production motion blur. The main comparisons change only the clipping
  candidate; both APIs also pass failed-eye-to-mono parity with it enabled.
- `cached-projection-dlss-oct2/toggle.log`: the normal-default real embedded
  SDK K/M/K preview passes 96 evaluations and 24 held reuses. This is not
  candidate SDK acceptance. The known mono swapchain refcount-1 shutdown
  warning remains. Four freshly linked CPU suites pass in
  `cached-projection-cpu-oct2/ctest.log`; DisplayXR's 1,746 assertions use mocked
  dispatch, not a physical Leia panel.
- All 59 common portable headers and the effects shader are fresh; six Metal
  binding and three include-digest tests pass. Edited PowerShell scripts parse,
  and the normal whitespace check passes.

`cached-projection-timing-d3d12-oct2/rejected.json` explicitly rejects the
first 360-pair reference run: an unrelated compiler was observed at its end.
Its frame/GPU values are not accepted and no candidate timing conclusion is
drawn from it. The r2 pre-launch guard also detected a concurrent build and
started no timed app run; its `rejected.json` records that separately. Quiet
sequential A/B/B/A and reversed B/A/A/B comparisons now completed on the same
`3CC74B31...` executable. Both discard the first 60 of 360 full 2x SBS pairs
per run, use Original 1-1, unpaced 120-FPS scheduling, 1,000 preroll ticks,
no DLSS/rays/other enhancements and strict route/query guards. No builds,
other GPU checks or benchmarks overlapped these accepted runs.

| Order/route | Scene GPU mean (us) | Frame median (us) | Frame p95 (us) |
| --- | ---: | ---: | ---: |
| A1 reference | 5897.28 | 6328 | 13058 |
| B1 cached | 5674.52 | 5876 | 8263 |
| B2 cached | 5442.66 | 5436 | 8585 |
| A2 reference | 5635.92 | 5698 | 8941 |
| B3 cached | 5528.07 | 5612 | 8750 |
| A3 reference | 5546.47 | 5592 | 8055 |
| A4 reference | 5743.10 | 6114 | 10121 |
| B4 cached | 5686.43 | 5990 | 11741 |

Evidence is `cached-projection-timing-d3d12-oct2-r4/results.json` and
`cached-projection-timing-d3d12-oct2-baab/results.json`. The reversed order does
not sustain the apparent initial whole-frame gain: B3 has a slightly worse
median than A3, and both cached tails are worse than their neighbouring
references. Drift/noise is substantial. No reliable frame-time improvement,
new production default or sustained SBS acceptance is claimed. The candidate
remains disabled by default. These timings do not apply to a later executable;
Vulkan/Intel performance and uninhibited sustained play remain unverified.

Release executable/settings, current preferences and ReShade INI/log remain
unchanged. Native Leia liquid/world/post/secondary-hit correspondence, physical
Leia/Android acceptance and sustained SBS improvement remain open; Ally
troubleshooting stays user-deferred. These checks do not renew the older native
SDK evidence under this new executable hash. The full goal stays active.

## October 2 — model-phase diagnosis and interior-plane experiment

Prior development PC:
`D18A5D96C222AA4CC8882CD5F53AE385F4044253ACB4D8CFDB3E9A063E47108A`.
Stereo checker:
`FD13A8D638406B3D73DD8F06F2CDCB6F17A01811041A8A85A017D7AFB1DA26E1`.
Clip checker:
`3A145AC51CC1B3B60FB2F2A28129417A6E11E73569F541BE861A30BD0B5E998E`.
No clipping or raster experiment is promoted to the normal default.

`STARFOX_TRACE_SCENE_GPU_DRAWS=1` measures each owned scene draw through its
normal merge/MSAA/ray-input packing, on the existing submission and exact fence.
Draw kind, ordinal, owner-local batch and authored work units are retained with
the query sample. Model hooks mark upload/projection/visibility/BSP preparation,
surface/warp/clipping/span preparation, and final painting/composition. Early
model shortcuts retain only their total interval, not invented phase timings.
Parallel eyes use separate thread-local hooks. Borrowed commands are not timed.
No production image download, additional submission or query fence wait is
introduced; the diagnostic collectors allocate nothing when disabled. Owners
have bounded query slots/sample storage and emit verbose samples at shutdown.
These diagnostics are not normal-play performance measurements.

The measured diagnostic predecessor was
`9ADB319C12A7CB21744D6ED919E2F3E0FDE16ECECBA6131F275D50472337F851`,
not the current executable. `scene-draw-d3d12-phases-oct2/draw-summary.json`
records 180 complete 2x SBS pairs, Original 1-1, unpaced 120 FPS schedule,
1,000 preroll ticks and first 60 pairs excluded on actual NVIDIA D3D12.
There are 7,050 measured draws, including 6,284 model draws / 3,998 phased
model draws. Mean model total is 4,615 us/pair; phased preparation, clipping/
spans and painting are 677 / 3,411 / 341 us/pair respectively. Remaining early
model shortcuts have no phase split. Grid is 159 us/pair; background and raster
draws are 767 / 412 us/pair. The scope excludes scene preamble/empty clearing,
the separate native hardware-ray queue, CPU encoding and presentation. Units
are authored faces/commands, not expanded GPU face counts. The result targets
model preparation rather than another grid-bin or effects-copy experiment.

An explicit `STARFOX_TEST_IDENTITY_CLIP=1` selects a lazy continuous-clip shader
variant. It uses the reference's exact half-open predicate, and skips a plane
only if all current vertices are inside: copying them to scratch and back is
then an identity with the same order and XYUV bits. Mixed planes, intersections,
near/line/invalid paths, status records and output ABI retain their reference
logic. Scratch capacities, including the existing Intel payload, are unchanged.
`STARFOX_TEST_FULL_CLIP=1` wins; default and noncontinuous clipping remain as
before. Generated DXIL/SPIR-V/Metal payloads are fresh, but generated Metal is
not a claim of execution on Apple hardware.

Completed current-hash verification:

- `identity-clip-component-oct2`: actual NVIDIA D3D12/Vulkan clip oracles pass
  25,362 integer and 50,724 fractional polygons per API, source/near/invalid/
  binary64/line/reuse fixtures, independent span pixels and retained metadata.
- `identity-clip-presentation-d3d12-oct2/results.json`: 35 byte-identical app
  images. `identity-clip-presentation-vulkan-oct2/results.json`: 25 images.
  Both cover ordinary SBS, water/reflections, banked terrain, AA and failed-eye
  mono recovery; D3D12 also covers EX lava/shutter and production motion blur.
  The main comparisons change only the clipping variant.
- `scene-draw-presentation-d3d12-oct2-r2/results.json`: 20 byte-identical app
  images; Vulkan counterpart: 15. Timing ON/OFF comparisons cover ordinary
  SBS/water, D3D12 motion and failed-eye recovery. Completed ordinary query
  streams also pass batch/serial/count/backend/phase-sum checks. The first
  D3D12 harness attempt omitted the presentation counter and was rejected;
  it is not an accepted timing result. The corrected capture helper requests
  that counter, and all r2 checks pass without altering saved preferences.
- `scene-draw-summary-negative-oct2`: missing frame counts, allocation bounds,
  query errors, malformed phases and inconsistent phase sums are rejected.
- `identity-clip-health-oct2`: full instrumented D3D12/Vulkan stereo checkers
  pass, including independent CPU eye/motion/metadata, joined/parallel ownership,
  cancellation/retry and sparse-to-motion coverage. Per-draw query errors and
  allocation-bound counts are zero; deliberate component cancellation is not
  mistaken for a zero-loss performance run. The same clipping oracle also
  passes on actual Intel integrated D3D12, with its existing payload capacity.
- `identity-clip-dlss-oct2/toggle.log`: normal-default embedded real SDK K/M/K
  preview passes 96 evaluations / 24 held reuses. This is a default-path
  regression, not SDK acceptance of the opt-in clipping experiment. The known
  mono swapchain refcount-1 shutdown warning remains. `identity-clip-cpu-oct2`
  has four freshly linked passing DisplayXR/stereo/input/simulation suites;
  DisplayXR has 1,746 assertions on mocked dispatch, not a physical panel.
- 55 common portable headers, effects shader freshness, six Metal-binding and
  three include-digest tests pass. Edited PowerShell scripts parse successfully.

`identity-clip-timing-d3d12-oct2` is a sequential A/B/B/A comparison on the
same current binary/actual NVIDIA adapter: 360 complete 2x SBS pairs per run,
Original 1-1, unpaced 120 FPS schedule, first 60 pairs excluded, scene/effects
timestamps enabled but per-draw profiling OFF. No build, other GPU test or
benchmark overlaps the runs. Route/counter/query/runtime markers are required.

| Run | Scene mean us/pair | Effects mean us/pair | Frame-work median / p95 us |
| --- | ---: | ---: | ---: |
| Reference A1 | 6635.04 | 39.03 | 13085 / 19694 |
| Candidate B1 | 6414.88 | 43.12 | 16403 / 22256 |
| Candidate B2 | 6691.38 | 36.45 | 11196 / 15337 |
| Reference A2 | 6421.01 | 32.02 | 11921 / 18350 |

The direction reverses between pairs; whole-frame results also vary widely.
This is not an accepted speedup or an uninstrumented FPS measurement. The
interior-plane variant stays opt-in. Further work should address the measured
model-preparation cost, with independent coverage/metadata and live app checks.
Sustained SBS improvement, native Leia liquid/world/post/secondary-hit
correspondence and physical Leia/Android acceptance remain open. Ally diagnosis
is user-deferred. Release binaries and saved preferences remain unchanged.

## October 2 — compact ordered mask bins remain experimental

Current development PC:
`84715F0118EA3A77EC9932B8C00F5EE15AB74B16660B941796B8A7A4F55205F8`.
Stereo checker:
`2483D3EB10E4F7F933FFBB0CD9C7B873CA13FE24CC12F54C086E845AF64EE270`.
The original list painter remains the default. This checkpoint does not
establish a rendering speed improvement or promote a release package.

An explicit `STARFOX_TEST_MASK_TILE_RASTER=1` uses two ordered 32-bit face masks
instead of variable-length lists for ordinary non-wave row spans with 1–64
faces. Each 64-pixel row tile requests 8 scratch bytes, rather than
`4 * (face_count + 1)` bytes. The existing grow-only scratch cache is unchanged;
switching a previously larger owner does not free its old allocation. This
is a requested-layout reduction, not measured freed GPU memory. Empty inputs,
waves, larger face counts and conflicting raster experiments keep their
original paths. The explicit disable switch wins. A once-per-owner diagnostic
marker verifies that the requested compact kernel actually ran.

The same descending painter order and shared material/receiver/depth code are
retained. There are still two GPU passes; no CPU projection, image download,
extra submission or new queue wait was added. Pipeline creation remains lazy
and opt-in. Generated DXIL/SPIR-V/Metal headers are fresh; Metal source
generation is not a device execution claim.

Verification on the current hashes:

- `D:/SFE-validation/mask-tile-component-oct2`: standalone and full D3D12/Vulkan
  stereo checkers exit 0 on the actual NVIDIA adapter. Each backend has 144
  metadata cases / 161,928 independently checked pixels and 360 colour cases /
  458,316 pixels. Both outputs also match byte-exact packed/surface/depth
  planes. The fixtures cover partial tiles, the 64/65-face boundary, transparent
  texture holes, black/dither ink, inherited receivers, emissive/world ownership,
  copying/in-place targets, waves, custom extents/jitter, empty inputs and cache
  reuse. Full checks also retain independent CPU stereo eyes, joined/parallel
  ownership, right-eye cancellation/retry and sparse-to-motion transitions.
  The same 504 compact-mask cases also pass on actual Intel integrated D3D12
  (`intel-direct3d12.{out,err}.log`); this is pixel/metadata correctness, not
  an integrated-GPU speed measurement or a physical Android test.
- `mask-tile-presentation-d3d12-oct2/results.json`: 35 byte-identical app images
  from seven comparisons. `mask-tile-presentation-vulkan-oct2/results.json`:
  25 images from five comparisons. Ordinary SBS, enhanced water/reflections,
  banked terrain, AA and rejected-eye mono recovery run on both APIs. D3D12
  additionally covers EX lava/exposure/particle shutter and saved-setting
  production motion blur. Main stereo reference/candidate runs differ only
  in the compact-bin representation; their actual-route marker is required.
  The separate recovery comparison intentionally injects a failed stereo eye
  and verifies that the resulting mono image retains the ordinary mono result.
- `mask-tile-dlss-oct2/toggle.log`: the real embedded SDK passes K/M/K menu
  preview switching, 96 evaluations and 24 held reuses. The existing mono
  swapchain refcount-1 shutdown warning remains; it is not hidden or fixed by
  this experiment. `mask-tile-cpu-oct2`: four freshly linked CPU suites pass.

Actual NVIDIA D3D12, Original 1-1, 2x full SBS, 360 complete pairs, unpaced
120 FPS schedule, 1,000 preroll ticks and first 60 pairs excluded:

| Route | SDL scene mean (us/pair) | Effects mean (us/pair) | Frame-work median (us) |
| --- | ---: | ---: | ---: |
| Established lists | 5737.25 | 38.45 | 6027 |
| Compact masks | 5817.56 | 42.46 | 6154 |

Evidence is `mask-tile-d3d12-{baseline,candidate}-oct2`, including complete
bottom/bottom query logs and `gpu-timing.json`. Both runs retire every query
without drops/errors and use the same executable and actual adapter. This one
instrumented pair does not establish a sustained regression or improvement;
it supplies no reason to enable the candidate by default. Scene time still
dominates effects copies. No cross-platform or uninstrumented FPS gain is
accepted. Performance work should separate model and terrain/grid phases
before trying to optimize the heavier batches.

Diagnostic children exclude the installed global Vulkan ReShade layer; normal
user applications and its installation are untouched. Game preferences,
ReShade INI and the release executable retain their recorded hashes. No
physical Leia/Android or current native liquid/world/post/secondary acceptance
is inferred from these portable raster tests. The full goal remains active;
Ally troubleshooting remains user-deferred.

## October 2 — native GPU command phases, not copy-policy FPS inference

Previous development PC is `F7D4B00DA0DBB9D49F12EF9D8411E464C8063AF4601D43DCAF5665B0D62C07C1`.
Effects checker is `54DD10ABEDBBFF498C220FC980DA9DA79AB4E8BAA5EFE5574628AB07B7A8FF9E`;
D3D12 interop checker is `4B65399717DB6870481D3F8698B180FD4ACBD5772C6BFB2C55AC378422B5FD18`.
This adds opt-in instrumentation, not an accepted rendering speed improvement.
The D3D12 copied-input default and owned-capture policy remain unchanged.

The pinned SDL backend exposes a separate optional timestamp bridge. It owns
only a query heap and a 256-byte timestamp readback per diagnostic owner, not
scene pixels. Native queue frequency converts ticks to elapsed time, following
[Microsoft's timestamp semantics](https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing).
The same fence-retired collector now covers D3D12 scene commands and effects
commands on D3D12/Vulkan. Effects record input-copy/upload, effect work, and
capture/extraction/output boundaries. Vulkan now also starts at bottom-of-pipe:
TOP/BOTTOM scopes can include preceding submissions, so summing them need not
represent exclusive work. See the
[Vulkan synchronization scope](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp.html).
Final logs identify both actual backend and `boundary=bottom-bottom`.

Only diagnostic runs allocate queries. Ordered effects reuse retains diagnostic
fences, polls them without blocking, and releases their tiny resources after
retirement; it adds no frame wait/submission or scene download. Pending queries
drain at existing owner shutdown. Query slots cannot be reused before their
exact fence retires. Logs are buffered until shutdown. The benchmark exposes
`-TraceEffectsGpuTimestamps` and checks backend, complete sample counts, and
zero dropped/cancelled/error queries. The new summary helper checks serial
uniqueness, phase sums and fixed per-frame multiplicity; it sorts retirement
order before discarding 60 warmup frames, rather than assuming retirement order
is recording order. It refuses unmarked TOP/BOTTOM aggregation.

Actual NVIDIA RTX 5070 Ti Laptop GPU, Original 1-1, 2x full SBS, 360 complete
pairs, unpaced 120 FPS schedule and 1,000 preroll ticks: mean instrumented
elapsed microseconds per complete pair, after warmup:

| Current build / scene | SDL scene | Effects total | Input | Effect work | Output |
| --- | ---: | ---: | ---: | ---: | ---: |
| D3D12 / plain | 5665.96 | 39.55 | 8.48 | 24.21 | 6.85 |
| Vulkan / plain | 10828.34 | 31.56 | 0.11 | 20.39 | 11.05 |

Evidence is `D:/SFE-validation/gpu-phases-{d3d12,vulkan}-bottom-oct2`, including
source logs and `gpu-summary.json`. These are command intervals, not a complete
frame budget: native DXR queue work, CPU encoding, presentation and queue-idle
gaps outside the intervals are not measured. Instrumentation can perturb
scheduling; these are not uninstrumented FPS comparisons.

The predecessor `B8DE1C991E99661AEBB7C00162DEFFDA0D23ADB5995EFE369F86F5DC5EF0C81D`
has four D3D12 probes under `gpu-phases-d3d12-{plain-copied,water-copied,
water-borrowed,plain-fused}-oct2`. D3D12 already used bottom-of-pipe. Water
copied/borrowed effects total is 223.09/222.71 us per pair; input is 8.70/4.81 us,
yet frame medians differ by milliseconds. Copying cannot account for that gap.
Plain ordered world/model-scene owners contain 21–28 draws and cost about 2.3 ms each;
the fused visibility experiment reduces total scene mean from 5982.66 to
5722.10 us, but whole-frame median does not improve. One instrumented pair
does not establish a general gain, so the experimental default stays off.
The predecessor's `gpu-phases-vulkan-plain-oct2` is a TOP/BOTTOM probe and is
superseded for aggregate comparison, not relabeled as bottom/bottom evidence.

Current D3D12 and Vulkan effects components each pass 40 cases, 30,838,500 exact
byte comparisons and 55 deferred captures with timing enabled and no query
loss/errors. The D3D12 bridge passes 32 retired query sets, cancellation/reuse
and invalid bounds, plus existing DXR/texture interop checks. An initial interop
invocation inherited the Vulkan-only driver setting and was rejected at device
creation; the explicit D3D12 rerun is the passing result. Current app passes 20
exact comparison images (water, AA, banked ground and rejected-eye mono recovery)
with timing disabled, and actual SDK K/M/K preview passes 96 evaluations/24
held images. Its already-known mono swapchain refcount warning remains. Four
freshly linked CPU suites and generated shader freshness pass. Evidence is
`gpu-phases-final-{component,presentation,dlss}-oct2` under the validation directory.
Global Vulkan ReShade is excluded only in diagnostic children; saved game
preferences and the release executable remain unchanged.

Next performance work breaks down the ordered world/model batch, including
projection/visibility/BSP/clip/raster and terrain/grid composition, plus the
separate native ray queue, not more input-copy FPS matrices.
The full goal remains active: native Leia liquid/world/post/secondary
correspondence, physical Leia/Android acceptance and sustained SBS improvement
are not proven by these diagnostics. Ally troubleshooting remains user-deferred.

## October 2 — isolate input borrowing from capture ownership

Development PC is `5F29D055A0B8EB98E620DF668682F29A5292366DAB6FD5027B6E2C66AB9A159C`;
effects checker is `61848206E4974768E33BDD523ED13F2E7F92ED61661C7DA18468A6F3AED7D0DE`.
D3D12 now retains the established resident GPU input copy by default. Other
SDL backends keep direct resident input sampling. The owned-scratch capture
path remains: plain output needs no separate full-image snapshot, while split
bloom/model extraction retains one. Fences, queue submission, visual effects
and protected ownership are unchanged. This does not select CPU effects or
download the compositor. Backend policy is queried once at owner initialization.

`STARFOX_TEST_BORROW_EFFECTS_INPUT` explicitly restores borrowed input on
D3D12; the copied-input reference takes precedence. The benchmark exposes both
routes, rejects contradictory selections, and can emit one diagnostic line
when the actual resident/input/capture/split route changes. Normal play does
not emit it. This avoids incorrectly assuming a requested switch was used.

Actual traces confirm that ray-water/bloom already needs a separate capture
in both input modes. The previous water timing difference could not have come
from snapshot removal. Eighteen longer D3D12 runs completed with 1,200 full SBS
pairs each, first 60 excluded, counterbalanced route order, same 2x targets,
NVIDIA adapter and unpaced 120 FPS schedule. Both water input variants and all
four plain input/capture combinations executed their requested routes. Each log
contains only two route lines, not per-frame stderr output. Runtime, game
preferences and ReShade INI/log hashes remained fixed throughout.

| Scene/actual route | Trial medians (us) |
| --- | --- |
| Water copied/separate, split=1 | 14415 / 17499 / 15551 |
| Water borrowed/separate, split=1 | 9281 / 15640 / 14467 |
| Plain copied/separate | 5023 / 10625 / 5442 |
| Plain copied/owned | 5325 / 12054 / 9216 |
| Plain borrowed/separate | 5108 / 12817 / 11409 |
| Plain borrowed/owned | 11847 / 6167 / 5325 |

Evidence is `D:/SFE-validation/effects-copy-isolation-oct2/{results,summary}.json`
with the actual route and terminal state recorded per run. Longer water pairs
favour borrowing and contradict the short-run water result. Plain runs vary
too widely to infer a reliable capture-copy speed difference. The D3D12 default
therefore restores the pre-candidate input route conservatively, **not as a
verified speed improvement**. Neither route is accepted as universally faster;
this is not sufficient evidence to promote an FPS claim. Further performance
work needs controlled CPU/GPU/queue-phase measurement, not another relabeling of
these noisy whole-frame medians.

Default, forced-copied and forced-borrowed effects components pass on both
D3D12 and Vulkan: 40 cases, 30,838,500 exact byte comparisons and 55 deferred
captures per run (six terminal runs). Actual SDK K/M/K preview passes 96
evaluations/24 retained frames; its known mono shutdown warning remains. Four
CPU suites pass. Five live D3D12 app comparisons pass 25 exact images: water,
lava, banked ground, AA and rejected-eye mono recovery. Evidence is
`effects-copy-policy-{direct3d12,vulkan}-{default,copied,borrowed}-oct2.*`,
`effects-copy-policy-dlss-oct2`, and
`effects-copy-policy-presentation-d3d12-oct2/results.json` under the validation
directory. The release executable and game preferences are unchanged. The
full goal remains active, including native liquid/world/secondary correspondence,
physical Leia/Android acceptance and actual SBS throughput improvement.

## October 2 — legacy injector cleanup and clean validation

Development PC is `1BAB73F77F6FAB93FDE3F8C6CC831D4738979F4453A37460FB90A8203FAF7D9B`.
The three obsolete loose development DLLs were moved, not deleted, to
`D:/SFE-validation/obsolete-dlss5-runtime-oct2/`; each destination matches its
source SHA-256. The release executable and both game preference files remain
unchanged. Ordinary embedded DLSS/DLSS 4.5 do not depend on these files.

This machine also has the globally registered `VK_LAYER_reshade` in
`C:/ProgramData/ReShade/ReShade64.json`. Removing the loose proxy does not
disable that layer. Diagnostic helpers now use its documented manifest key
`DISABLE_VK_LAYER_reshade_1=1` for their child processes only and restore the
caller's environment. No registry entry, system DLL or other application's
configuration was removed. Normal user ReShade installations remain allowed.

`STARFOX_TEST_REQUIRE_CLEAN_RUNTIME` checks known loaded ReShade exports and
legacy neural modules after renderer/SDK binding. Profiling, capture and DLSS
helpers require its marker. The benchmark also rejects known loose sidecars
before changing its environment. Both a deliberately loaded local proxy and
the actual global Vulkan layer were rejected, rather than silently measured.

Clean D3D12 and Vulkan effects checks pass 40 cases, 30,838,500 exact byte
comparisons and 55 deferred captures per backend. The initial Vulkan component
run without the layer opt-out is superseded, not accepted as clean. Seven app
comparisons per backend pass 70 exact images total: water, lava, 4x upscale,
AA, banked ground, production motion and rejected-eye mono fallback. Both
backends retain the copied-input/capture reference and directly presented
candidate. Ordinary SDK preview K/M/K passes 96 evaluations and 24 retained
frames; its previously recorded mono shutdown warning remains. Four CPU suites,
shader freshness and helper parsing pass.

Evidence: `tmp/clean-runtime-negative-oct2/results.json`,
`D:/SFE-validation/sbs-copy-clean-owned-direct3d12-oct2.*`,
`sbs-copy-clean-owned-vulkan-optout-oct2.*`,
`sbs-copy-clean-presentation-direct3d12-oct2/results.json`,
`sbs-copy-clean-presentation-vulkan-optout-oct2/results.json`, and
`sbs-copy-clean-dlss-mono-oct2`. The rejected global-layer run is retained under
`sbs-copy-clean-presentation-vulkan-oct2`, not included in passing results.

Before the layer was excluded, it automatically rewrote/expanded `ReShade.ini`
from `1DFC3DE7...` to `6559A593...`. All settings in the preserved
`ReShade.ini.before-starfox-dlss5` still match. No byte-exact copy of the
immediate pre-test serialization was found, so it was not replaced with an
older config. Disabled-layer tests no longer rewrite it or `ReShade.log`.
Game settings remain byte-identical.

Quiet, alternating copy/direct trials completed: 24 terminal runs on the
NVIDIA RTX 5070 Ti Laptop GPU, 360 full SBS pairs each, 2x scene targets, 120 FPS
schedule unpaced, no captures or per-pass traces, first 60 pairs excluded. The
same executable and settings served both routes. The global layer was excluded;
the current ReShade INI/log and executable hashes stayed fixed during all runs.
Results and per-run logs are in
`D:/SFE-validation/sbs-copy-clean-timing-oct2/{results,summary}.json`.

| Backend/scene | Copied reference medians (us) | Direct candidate medians (us) | Change in median-of-three frame work |
| --- | --- | --- | --- |
| D3D12 plain | 5570 / 6317 / 5966 | 6118 / 6032 / 5705 | +1.11% |
| D3D12 ray water/bloom | 10019 / 9930 / 9892 | 10110 / 10170 / 10285 | +2.42% |
| Vulkan plain | 10341 / 10285 / 10493 | 11465 / 10235 / 10401 | +0.58% |
| Vulkan ray water/bloom | 15876 / 13059 / 13688 | 13160 / 12973 / 13903 | -3.86% |

Positive means slower. D3D12 water is slightly slower in all three pairs;
Vulkan water is mixed, with one large reference outlier. These clean timings are
accepted as measurements, **not as a successful general speed optimization**.
The copy candidate remains development-only, not promoted to release. Next
performance work must isolate borrowed input from the capture-copy removal and
address the D3D12 concern before claiming a throughput benefit. Verified pixel
ownership and lower copy/allocation counts do not establish higher FPS. The
complete goal remains active, including native effect parity, sustained
performance and physical-device acceptance.

## October 2 — resident effects-copy candidate; timings not accepted

Development PC `675E429A34636DBC3CB780CB7CD859316FB0111190ABDAD299308B78490578AE`
reads the immutable compositor image directly for the first effect, then uses
owned ping-pong scratch. Deferred screenshots retain owned scratch without a
separate snapshot unless split model/bloom extraction needs one. Existing
submission and retirement fences remain; ordered-queue reuse stays opt-in.
Diagnostic copied-input/snapshot reference switches remain available.

Effects checker `95D50C3BA3169DC5FEAFA187D00EB91CCC9ABB1BA15765304E33EF4248BAF84E`
passes 30,838,500 byte checks, 40 scale/pattern cases and 55 deferred captures
on D3D12 and Vulkan. OFF screenshots survive a subsequently overwritten
compositor source; plain capture avoids one full-frame snapshot allocation.
The application comparisons pass 85 D3D12 and 45 Vulkan pre-injector images,
including water/lava, AA, motion, banked ground and rejected-eye mono fallback.
Ordinary DLSS K/M/K preview switching passes 96 evaluations and 24 retained
frames; its previously recorded mono shutdown warning remains. Four CPU suites
and shader freshness pass. Release/preferences are unchanged.

Evidence is `D:/SFE-validation/sbs-copy-owned-final-{direct3d12,vulkan}-oct2.*`,
`sbs-copy-presentation-{d3d12,vulkan}-oct2/results.json`, and
`sbs-copy-dlss-mono-oct2`. These are scoped correctness checks, not a claim of
general SBS speed improvement or final post-injector visual acceptance.

**The quiet timing matrix is not accepted.** Investigation of the user's
ReShade report found an old `build/current/dxgi.dll` ReShade proxy, RenoDX
DLSS5 add-on and neural runtime still beside the executable. `ReShade.log`
confirms the proxy actually loads, even though these are not in the standard
embedded runtime or `build/release`. The interrupted timing orchestrator's
results remain under `sbs-copy-timing-*-oct2`, but cannot establish clean
normal-build throughput. Proxy cleanup and clean reruns are pending; no files
were removed in response to the diagnostic question. The complete goal remains
active, including sustained SBS, native effect parity and physical devices.

## Earlier measurements

Current development PC is `286B78B1...`. Native FSR1 uses actual lower-resolution
eye scene/ray/effect targets, then full-panel ink and EASU/RCAS reconstruction.
Correctness/lifecycle checks pass on real D3D12/Vulkan, with a mocked runtime;
the extra ink raster/images/passes have not been accepted by quiet timing or
physical/large-eye tests. This is not a general GPU/SBS speed improvement claim.
SDK/menu and ordinary stereo regressions pass. Exact scope and remaining native
DLSS/liquid/device work are in the newest stereo checkpoint. Every timing below
keeps its measured executable hash rather than being relabeled for this PC.

Previous development PC is `14E7AD30...`. The latest change makes native TAA
liquid-specific instead of globally dropping dry foreground correspondence.
Its real D3D12/Vulkan correctness checks add no extra eye image/render pass,
but they do not establish an FPS improvement. See the newest stereo checkpoint.
All timings below keep their measured hashes; none is relabeled as this build.

## October 2 — fuse the enhanced environment and physical ground resolve

Current development PC is
`6263A3729708B8AEC1DF384E2E4FD4023B05FF4B124EF932797668792A7AC47C`.
SDL GPU stage 31 now consumes a valid physical ground-ray result directly,
instead of shading a disposable procedural floor and then resolving the same
ray in a second full-image stage 30. The existing ray buffer, command owner,
queue and fences are unchanged. Both eyes keep their independent rays and
images. Missing/out-of-bounds rays, non-ground ray markers and protected pixels
still use the original environment path. Model reflections and later
flash/wipe, shadow and post-effect ordering are unchanged. The D3D11 host keeps
its previous separate-pass route.

The normal SDL D3D12 route uses the fusion. Vulkan and other SDL backends keep
their previous separate pass after mixed timing results. The backend policy is
cached once at owner initialization, not queried in the eye loop. The diagnostic
`STARFOX_TEST_SEPARATE_ENVIRONMENT_REFLECTION` retains the immediate two-pass
reference; `STARFOX_TEST_FUSE_ENVIRONMENT_REFLECTION` explicitly enables fusion
on other backends, and `STARFOX_TEST_ENVIRONMENT_REFLECTION_RESULT` records which route
actually executed. This removes one full-image dispatch/read/write per eye
when enhanced ray ground is active, not the physical trace or selected effects.
It adds no new buffer, upload, submission, fence, wait or production readback.

Current focused checks pass on actual NVIDIA D3D12 and Vulkan:
`D:/SFE-validation/environment-reflection-final-component-oct2`. Per backend, 45
Auto/water/mirror/gold/lava cases compare 524,160 RGBA bytes exactly against the
two-pass reference. Independent decoding checks 30,741 ground hits; the exact
reference covers 25,880 missing/bounded/wrong-marker fallbacks. Independent
source-byte checks protect 45,045 pixels. Scales
1/2/4, both banks, fractional scrolling, alpha and owner release/recreation
are included. The effects checker is
`D272CD670C723FD2D0D62E517FA28BA738DD8A55DC931DEB306DA6F6A13841AD`.

The preceding timed PC
`F470E68E1E1C52ED48E73024E38FE41B80F68B6557A91F13575875B2415B77C1`
passes actual cartridge-backed SBS checks on both backends in
`environment-reflection-sbs-{d3d12,vulkan}-oct2`: four exact packed images each
for enhanced water/sky/rays/bloom and EX 6-6 lava/exposure/shutter, plus four
exact mono images after an injected partial-eye failure. The harness requires
the actual separate/fused resolve and eye/producer markers, not just equal
files. Three freshly rebuilt CPU suites (input, stereo output and terrain),
portable-effects freshness and all four changed PowerShell parsers pass.
These captures retain that preceding hash; final-policy acceptance is below.

### Quiet paired timings and resulting policy

Both complete matrices belong to the preceding F470E68E executable, before
limiting the default policy to D3D12. All 36 actual-adapter processes complete
240 native eye pairs each. Three interleaved trials per adapter/stage reverse
order in trial 2. They use visible 4:3 / 2x / full SBS, Original 1-1 or 3-5,
enhanced water/sky, RT/High reflections, bloom 2, original logic timing,
120 target unpaced, VSync off, 1,000 preroll ticks, God Mode and 60 warmup
frames. There are no image captures, per-frame diagnostic writes, overlapping
GPU checks or compilation. Both executable/settings hashes and actual backend,
complete-pair and separate/fused markers are mandatory. Logs:
`D:/SFE-validation/environment-reflection-quiet-d3d12-intel-oct2` (24 records)
and `environment-reflection-quiet-nvidia-vulkan-oct2` (12); both `complete=true`.

The table uses median-of-three per-run medians and p95s, in microseconds. It is
frame work, not displayed FPS, isolated shader time or sustained acceptance.

| Actual adapter/backend and stage | Separate / fused median | Separate / fused p95 |
| --- | ---: | ---: |
| RTX 5070 Ti Laptop / D3D12, Corneria | 10,417 / 10,157 | 16,393 / 14,370 |
| RTX 5070 Ti Laptop / D3D12, Venom | 7,050 / 7,078 | 10,973 / 11,198 |
| RTX 5070 Ti Laptop / Vulkan, Corneria | 13,121 / 13,185 | 14,481 / 14,511 |
| RTX 5070 Ti Laptop / Vulkan, Venom | 9,362 / 9,464 | 10,265 / 10,405 |
| Intel Graphics / forced Vulkan, Corneria | 32,620 / 39,578 | 35,596 / 43,913 |
| Intel Graphics / forced Vulkan, Venom | 27,485 / 26,730 | 29,879 / 30,069 |

D3D12 Corneria median improves in all three pairs (7.07%, 2.12%, 2.50%);
its aggregate improvement is 2.50%, with lower p95. Venom's aggregate is
essentially unchanged, with mixed pairs and no broad tail improvement.
NVIDIA Vulkan has no reliable gain. Intel Vulkan changes substantially between
trials: Corneria trial 2 is 39,925 us fused versus 32,620 us separate, while
trial 3 is 31,667 versus 32,337. Do not infer causality from the aggregate alone.
Its large stalls remain, including a 553,657-us reference maximum and a
452,133-us candidate maximum. Candidate tails also worsen in several NVIDIA
cases. These results support only the narrow D3D12 policy, not enabling fusion
for Vulkan/Android or claiming that general GPU/SBS lag is fixed. Windows Intel
AUTO already uses D3D12; the Intel Vulkan runs explicitly override it.

The final policy changes no shader arithmetic or ray resources. Current
6263A372's three 120-pair real frontend probes pass actual-adapter and normal
default-policy assertions: NVIDIA D3D12 is fused; NVIDIA/Intel Vulkan are
separate, without either force override. Evidence is
`environment-reflection-final-default-{d3d12,vulkan,intel}-oct2`. These are
functional probes, not new three-trial timings. The preceding timing results
are not silently relabeled as a measurement of the final hash.

Current final D3D12 and Vulkan full-SBS water/sky/rays/bloom each pass four
byte-identical packed images against the separate/snapshot route, plus four
exact EX-lava mono images after injected left-eye rejection:
`environment-reflection-final-sbs-{d3d12,vulkan}-oct2`. The diagnostic opt-in
still proves Vulkan fusion parity while its normal default stays separate.
Current SDK K/M/K passes 96 evaluations and 24 retained previews; all 23
Preview-OFF menu cases, renderer navigation and RENDERING loading pass in
`environment-reflection-final-{dlss,menu}-oct2`. The SDK's known swap-chain
release/refcount warning remains; these checks are not warning-free. Three
freshly relinked CPU suites and shader freshness pass. Both saved preference
hashes remain A2B8BDBA / E0F794E8, and release remains 828D98F4.

### Rejected single-face experiments

A generic direct one-face row painter and its later compile-time-specialized
shader both pass exact raster references but do not establish a speed gain.
They remain diagnostic-only/OFF. The specialized shader uses the same existing
resource ABI and preserves source arithmetic; only the one-face loop and its
dead tile/sparse/wave branches are specialized. Normal rasterization still
uses the prior binned policy.

The generic candidate PC was
`D25EDF1C4977EEE800B9BA5EFAB852F3F60E95E8F161EA3AFC6D7BB170A1066A`.
Its quiet matrix was deliberately canceled after an actual Intel Vulkan
regression: trial-1 Corneria median/p95 frame work went from 41,765/46,026 us
(binned) to 97,122/170,915 us (generic direct). NVIDIA D3D12 was mixed. All 11
finished records are retained under `single-face-spans-quiet-d3d12-intel-oct2`,
with `complete=false`; this is not a completed timing matrix. Its component
checks retain their own hash in `single-face-spans-component-oct2`.

The specialized OFF-policy PC was
`519363BE603A4E28BF22E86D364968C772F444EC9CE9AC35A703C0925F7570BE`;
stereo checker was
`5D2F0CA55A5CAEB6AF470360B7E3A069D93E2ABB7D2FE8BD9353863A9A997906`.
All three actual adapter/backend component suites pass in
`single-face-specialized-component-oct2`, including 24/48 single-face dispatch
cases, independent painter/metadata references and queued stereo ownership.
Six individual quiet Corneria probes in `single-face-specialized-probe-oct2`
are mixed: Intel Vulkan median 43,067 -> 39,804 us, NVIDIA Vulkan 14,225 ->
14,666 us, NVIDIA D3D12 10,492 -> 11,334 us. Candidate maximums worsen to
331,510 / 288,524 / 341,285 us respectively. These are single probes, not
multi-trial acceptance. Do not enable the specialization from that evidence.

Release `828D98F4...` and both preferences remain unchanged. Physical Leia,
Android saved-GPU relaunch, remaining native enhancement parity and sustained
SBS performance acceptance remain open; Ally troubleshooting is user-deferred.
Earlier checkpoints below keep their original executable/test scope.

## October 2 — native water shading without added CPU pixel work

Current PC is `925102D7F78765FDE88AE45AC5C4122C7D8F48746BC722910D779EF82AA9289E`.
The calibrated water receiver now shades RT-OFF waves/glints at actual GPU
world points, with footprint filtering. Native Auto ramp extraction admits
water explicitly; the CPU retains palette/clock metadata rather than sampling
one frozen wave into its endpoints. The ray-replaced receiver skips decorative
wave work. No extra submits, readbacks or render targets are introduced.
This is a native functionality correction, not a measured FPS improvement.

D3D12/Vulkan four-format component and retained-owner checks pass, including
independent optical references, protected opacity, AA and real-queue lifecycle.
Actual Original/EX source integration, two rebuilt CPU suites, freshness,
96 SDK evaluations / 24 retained previews, all 23 plain-menu cases and loading
pass. Exact scopes, hashes and logs are in the newest stereo checkpoint.
The earlier Intel/NVIDIA timestamps below retain their separate executable
scope; default parallel/joined/cache experiments are still OFF. Android-device,
physical Leia, broader native enhancement and sustained SBS acceptance remain
open. Release/preferences are unchanged; Ally remains user-deferred.

## October 2 — GPU timestamps separate scene work from Vulkan submission

Current diagnostic PC is
`49C2FA300EFC22C08B5E0FD66888CEFC3EF39A96E2BABE6E5A5E73B1A5D7430D`;
stereo checker is `63EF7B398D060CFB3AC22BD39C593A3448CCAEF5670901AE2F6326237CC17411`.
The GPU-timing change is diagnostic-only, not a shipped FPS improvement.
All previous experimental default policies remain unchanged.

`STARFOX_TRACE_SCENE_GPU_TIMESTAMPS` records native Vulkan TOP/BOTTOM timestamps
around owned scene batches, including empty clears, independent split/parallel
eyes and joined commands. A bounded per-owner pool associates each slot with
its exact borrowed SDL fence. Results are read only after an existing successful
retirement/finish wait, before releasing that fence; availability alone cannot
permit reuse while a queued reset still exposes old timestamps. No new wait,
device idle, external queue or normal-path readback is added. Cancellation frees
unsubmitted tickets. Values wrap at the actual queue's valid-bit width and use
its timestamp period. Unsupported backends keep rendering and report the
diagnostic unavailable. These intervals include GPU dependencies, not isolated
shader occupancy. The relevant primary references are
[Vulkan timestamps](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp.html)
and [query results](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetQueryPoolResults.html).

`STARFOX_TRACE_VULKAN_SUBMIT_TIMESTAMPS` separately records bounded host intervals
for lock acquisition, preparation, command finalization, fence setup, queue
submit and presentation/cleanup. Writes remain under SDL's existing submit lock;
all reporting is deferred until device teardown. The patch is idempotent on the
pinned SDL source and changes no interop ABI, queue or fence ownership. Both
options are exposed by `benchmark_native_defaults.ps1`, which rejects missing,
dropped or failed diagnostic samples rather than accepting a partial trace.

The measurements below belong to the preceding nonempty-scene diagnostic PC
`0A78D5E157BC6DE19DA415F96272BE35306C276D9823C2E0C4D38FA62D530C53`.
Each completes 240 native pairs: actual adapter, Original Corneria, visible
2x/full SBS/4:3, enhanced water/sky, bloom 2, original timing, 120 target unpaced,
VSync off, 1,000 preroll ticks, 60 warmup frames, God Mode. Vulkan ray-on uses
RT/High reflections; ray-off sets both to zero. There are no per-frame log
writes or image captures. These are individual instrumented probes, not quiet
multi-trial performance acceptance or displayed FPS.

| Actual adapter/backend and mode | Main-eye GPU intervals (median us, L/R) | Host scene encode / submit (median us) | Frame work median / p95 (us) |
| --- | ---: | ---: | ---: |
| Intel Graphics / Vulkan, ray on | 6,056.77 / 6,793.825 | 2,739 / 33,068 | 40,483 / 43,799 |
| Intel Graphics / Vulkan, ray off | 17,576.65 / 16,252.65 | 1,830 / 59,662 | 63,295 / 90,810 |
| RTX 5070 Ti Laptop / Vulkan, ray on | 3,397.085 / 2,976.975 | 4,087.5 / 215.5 | 14,037 / 16,429 |

Each Vulkan probe records 1,440 scene intervals with zero drops/cancellations/
errors. The ray-on host traces contain 4,605 complete submissions; ray-off has
2,685, with zero drops. Grouping the last 180 complete presentation buckets,
Intel ray-on's median summed host queue-submit time is 33,284.95 us, versus
20.2-us command finalization, 8.6-us fence setup and 28.2-us tail work. The large
cost is inside `vkQueueSubmit`, not `vkEndCommandBuffer`, logging, cleanup or
AS-size queries. This does not distinguish driver CPU work from GPU/interop
back-pressure. Removing rays does not eliminate the issue; it changes the
material path and the GPU intervals, so no causal clock/occupancy claim follows.
Logs are `D:/SFE-validation/sbs-submit-complete-{intel-ray-on,intel-ray-off,nvidia-ray-on}-oct2`.

The earlier 72793C0F host-only trace filled its 4,096-record limit and dropped
509 submissions; it is retained only as an incomplete exploratory trace. The
bound is now 8,192, and the later complete probes above supersede it. The first
ray-on harness attempt finished the game but rejected CRLF summary lines; the
parser was fixed and its full 1,440/4,605 raw sample counts independently checked.
The ray-off and NVIDIA runs pass the corrected assertions.

Joining the pair on earlier 6A7A23E4 still leaves 33,173-us median host submission:
`sbs-scene-gpu-timestamps-intel-joined-oct2`. Its split reference also completes
all queries. Joining remains OFF; it is not the fix. A 0A78D5E1 actual Intel
D3D12 ray-on probe completes 240 pairs with median/p95 work 38,393/48,820 us,
but a worse 405,903-us maximum: `sbs-submit-intel-d3d12-ray-oct2`. This is not a
new backend-selection policy. Windows AUTO already switches Intel to D3D12;
the Vulkan probes explicitly override it. Next optimization work must target
the actual backend/material dispatches and GPU dependencies, not assume every
adapter has this Vulkan stall or reduce the eye/effect workload.

Current 49C2FA30's NVIDIA/Intel Vulkan ownership fixtures pass 20 independent
CPU eye images and 6,266,880 exact split-path color/surface/depth/motion bytes
for each joined/parallel mode, plus resize, covered black ink, empty clears,
cancellation/retry, borrowed retirement, queued pairs and 96 changing painter
batches. The final query summaries have no drops/errors, with injected
cancellations retained: `scene-timestamps-final-stereo-*-oct2`. The preceding
0A78D5E1 fixtures exposed four missing empty-clear ending timestamps; those
diagnostic errors were fixed, not relabeled as clean. Its D3D12 fixture renders
correctly with the optional Vulkan diagnostic unavailable; its three ordinary
ray/retained-eye suites pass in `scene-timestamps-rays-*-oct2`.
Current SDK K/M/K passes 96 evaluations / 24 retained previews in
`scene-timestamps-final-dlss-oct2`. All 23 current Preview-OFF cases and the
RENDERING loading gate pass in `scene-timestamps-final-menu-oct2` with unchanged
preferences. Three rebuilt selected CPU suites and script parsing pass.
The final 49C2FA30 frontend also passes a 120-pair NVIDIA Vulkan 2x/full-SBS
water/sky/bloom/RT/High-reflection probe with BOTH native timing assertions:
`scene-timestamps-final-native-oct2`. Every reported scene/host sample is
present, with no drops or query errors. This is a current-hash instrumentation
gate, not a replacement for the preceding 240-frame measurements above.
Release `828D98F4...` and both saved preferences
remain unchanged. Broad performance/native-enhancement and physical Leia/
Android acceptance are still open; no Android device is attached, and Ally
troubleshooting remains user-deferred.

## October 2 — exact-layout ray AS size cache

Final OFF-policy PC is
`630671F5A44D024EC4D1590684FC8B54BFD516391ED0872F13E8304B3528B652`;
checker is
`5E40B45F04221D6748293405B5CF48D463C1B1D68B13915BA19C4B6596E79D73`.
Its current-hash gates pass as scoped below. Preceding corrected-timer PC is
`9D98BF511B36FBEB7A4F967DD3311D36F0991A7344F6427D538D56750BEBE840`;
expanded growth/shrink/revisit checker is
`5FFD08F8A7EA368258BADBE2C6D966C31B5699B3B98691CF78CDE03F3560A160`.
The immediate reference is
`7AD645473727449264D245857CF4DCC8287191007EFB1CEED1C52D7F1055F479`,
preserved at `D:/SFE-validation/ray-prebuild-cache-baseline-oct2/starfox_pc.exe`.

The reference's actual Intel Vulkan Original Corneria, 2x/full SBS, enhanced
water/sky, bloom, RT/High reflections trace completes 240 eye pairs in
`ray-as-preparation-intel-visible-oct2`. The other three producers independently
query the same 306/16 layout. There are no logged producer-fence waits. The
600-pair follow-up in
`ray-as-preparation-intel-long-oct2` records nine distinct count/stride layouts,
each queried four times. The original AS/wait timer sampled the clock AFTER
writing part of the log prefix. Its apparent 692–2,567-us warm query costs
include diagnostic output and are REJECTED as query-latency evidence, including
the corresponding C38375CD profile. Corrected timing stops before any logging;
fresh cached/uncached probes are required. Counts of driver calls and image/
component equality are unaffected. None of these traces measure GPU timestamps,
controlled cold compilation or displayed frame rates.

The new bounded 64-entry weak-registry cache holds only immutable prebuild
sizes, keyed by canonical actual-device identity and exact count/stride for
this function's fixed opaque float3, one-geometry, array, FAST_TRACE layout.
It neither guesses allocations from another count nor shares AS/geometry,
output, upload, queue, fence or command buffers. Each producer still rebuilds
external animated geometry every frame. Invalid/empty driver answers are not
cached. Ready hits avoid both the driver query and preparation-worker sleep;
first misses retain responsive preparation. The last producer drops its pool
before unloading device/system DLLs; the global registry holds only weak refs.
`STARFOX_TEST_DXR_PREBUILD_CACHE` / `-CachedRayPrebuildSizes` opt into it;
`STARFOX_TEST_DISABLE_DXR_PREBUILD_CACHE` overrides it for an identical-binary
uncached reference. Size caching, PSO sharing and stable-hit selection remain
OFF after the mixed performance result below.

At C38375CD/20256B75 all six ordinary component suites (cached/uncached on actual
NVIDIA D3D12, NVIDIA Vulkan and Intel Vulkan) pass, including revisited counts,
CPU float3/resident float4 transport, retained foreground/eyes, rejection and
peer release/recreation. The live cached/uncached 32-pair Corneria checks also
pass on all three adapters, with all 15 images exact:
`ray-prebuild-live-*-oct2`. Intel visual repeatability uses stable-hit selection
only for those images; the ordinary PSOs stay the default. Three selected CPU
suites and three PowerShell parsers pass at that preceding hash.

After stopping the clock before logging, current 9D98BF51's actual Intel Vulkan
ordinary-ray cached/uncached 240-pair probes both pass:
`ray-prebuild-corrected-intel-{cached,uncached}-oct2`. Each records four exact
layouts. Uncached makes sixteen driver calls; cached makes four, with twelve
size hits. Warm uncached queries take 8–30 us, and ready hits take 3–10 us. The
three redundant first-use joined preparations take 11,685–12,916 us uncached
versus 5–10-us peer hits cached. First preparation remains expensive
(1,629,924 us uncached / 1,861,022 us cached); these sequential runs do not
control driver cache state or prove a cold-time improvement. Both traces have
no producer-fence waits. This narrows the optimization to duplicate first-use
preparation and small query overhead, not the sustained frame-rate bottleneck.
All 24 trace-free ordinary-ray 9D98BF51 timings complete in
`ray-prebuild-visible-timing-2x-oct2/results.json`, with three trials/reversed
trial 2, actual NVIDIA D3D12 and Intel Vulkan, Original Corneria/Venom,
2x/full SBS/4:3, water/sky/bloom/RT/High reflections, 120 target unpaced,
1,000 preroll ticks, 60 warmup frames and 360/240 total frames respectively.
Complete native pairs, no replay, executable hashes and unchanged preferences
are asserted. This preceding hash enabled size caching by default; its uncached
mode used the disable override. The final policy restores OFF by default, and
the harness now explicitly requests the candidate. Median-of-trial host work:

| Adapter/backend | Stage | Median uncached → cached (us) | Change | p95 uncached → cached (us) |
| --- | --- | --- | --- | --- |
| NVIDIA D3D12 | Corneria | 10,932 → 10,846 | −0.79% | 22,339 → 19,210 |
| NVIDIA D3D12 | Venom | 8,094 → 8,468 | +4.62% | 15,318 → 17,554 |
| Intel Vulkan | Corneria | 41,566 → 39,551 | −4.85% | 45,489 → 43,200 |
| Intel Vulkan | Venom | 35,571 → 36,200 | +1.77% | 41,865 → 42,467 |

These mixed observations do not establish a general speedup or attribute median
changes to rare microsecond-sized warm queries. Separate current 9D98BF51
deferred host profiles (no per-frame writes) complete in
`ray-prebuild-host-profile-{intel,nvidia}-oct2`: Intel's 180 post-warmup samples
have medians of 38,025-us world work, 2,493-us native scene encoding,
34,508.5-us scene submission and 77.5-us retirement. NVIDIA's 300 samples have
4,364-us world, 2,930.5-us encoding, 209-us submission and 26.5-us retirement.
This identifies Intel submission/back-pressure as the next measurement target;
it does not prove CPU versus GPU/driver causation. Per-frame instrumented
compose/scene traces remain separate from quiet distributions.

The same 9D98BF51 PC passes standard/4.5/standard SDK switching (96 evaluations,
24 retained previews), 23 Preview-OFF cases and the RENDERING loading indicator:
`ray-prebuild-{dlss,menu}-oct2`. Final OFF-policy 630671F5 / checker 5E40B45F
also pass all six ordinary default/cache component suites and all three
120-sample focused suites on the three actual adapters:
`ray-prebuild-final-*-{default,cached,focused}-oct2.{log,stderr}`. These assert
that default never reuses a pooled size, while opt-in cache really does; all
growth/shrink/revisit and peer-release image/handle invariants pass. Current
K/M/K (96 evaluations / 24 retained previews), 23 Preview-OFF cases and RENDERING
loading pass in `ray-prebuild-final-{dlss,menu}-oct2`, with unchanged settings.
The three rebuilt selected CPU suites pass (2.99 s), and all three PowerShell
parsers pass. No current-hash live image sweep or new quiet speed claim is
inferred from those component/menu passes. Release/preferences remain unchanged.

## October 2 — device-scoped immutable ray-pipeline sharing (opt-in)

Preceding diagnostic PC is
`B2E4CC4601C6C1F740F16D2CFEFDA5839EEFEBD2B1065483329B41DF456937A2`;
checker before the added peer-release fixture is
`CDB41B8E6B0EA900FFA3AA8ABFB1A184D1BC3D095FAEC1512AAEB7321E66FCEC`.
`STARFOX_TEST_SHARE_DXR_PIPELINES` / capture `-SharedRayPipelines` opt into
sharing root signatures and shader PSOs only. Ordinary and experimental
embedded shader arrays remain distinct keys; canonical device identity,
exact serialized root bytes, shader-array identity/length, node mask and PSO
flags must match. Cached-blob descriptors bypass sharing. Buffers, AS objects,
uploads, command objects, queues, timelines and eye outputs stay producer-local.

The registry keeps weak references and removes expired entries. Each producer
holds its pool until its recorded command objects/PSO references are released,
then drops the pool before its device/system DLL references. A ready pool/PSO
hit uses a nonblocking lookup, without a preparation worker or UI sleep. A miss
or busy cache still takes joined responsive preparation; failed creations are
never inserted. Nothing changes ray depth, bounds, geometry or material math.

The preceding cache implementation at PC CEB83A47 / checker 1138D20C passes
all three actual-adapter ordinary component suites and 120 focused samples
with sharing ON (`ray-shared-pipelines-*-{ordinary,focused}-oct2.log`). After
the ready-hit improvement, current B2E4CC46 OFF/ON live Corneria captures each
pass 32 complete GPU-caster eye pairs and all five images match on each actual
NVIDIA D3D12, NVIDIA Vulkan and Intel Vulkan pair: `ray-shared-live-*-oct2`.
Intel uses opt-in stable hits only for visual repeatability; ordinary ray PSOs
remain the production default. No readback of raw ray inputs contaminates this
creation diagnostic. The traces prove both eye producers use the same PSO
object in ON, rather than merely recording matching shader hashes.

For example, NVIDIA D3D12's second ordinary indexed-fluid creation takes
15,429 us with sharing OFF versus a 3-us cache hit with ON; its other shadow
hits are 1–2 us. First indexed-fluid preparation is 951,576 us OFF / 1,166,875 us
ON in those runs. These are instrumented host create/join times, not fair cold
driver compilation comparisons or GPU timestamps; cache state is not reset.
The first expensive creation remains an open startup concern. All 24 quiet
ordinary-ray OFF/ON timings complete at this B2E4CC46 hash in
`ray-shared-pipeline-visible-timing-2x-oct2/results.json`, using the same visible
2x/full-SBS water protocol as the caster comparison below. Median-of-trial
host distributions are mixed:

| Adapter/backend | Stage | Median OFF → ON (us) | Change | p95 OFF → ON (us) |
| --- | --- | --- | --- | --- |
| NVIDIA D3D12 | Corneria | 10,712 → 10,638 | −0.69% | 20,668 → 17,931 |
| NVIDIA D3D12 | Venom | 7,542 → 8,100 | +7.40% | 16,423 → 13,982 |
| Intel Vulkan | Corneria | 40,510 → 39,860 | −1.60% | 44,793 → 42,757 |
| Intel Vulkan | Venom | 35,960 → 36,419 | +1.28% | 42,458 → 40,571 |

All twelve ordinary/focused OFF/ON suites pass on the three actual adapters
at checker `164181F8C447DC5B147D1E21A2E07A1DE0A345B1F6CFBBFC43FD5DFB957E712D`:
`ray-shared-lifecycle-*-{ordinary,focused}-oct2.log`. These include new peer
release/recreation assertions preserving the surviving eye's handle and bytes,
and 120 focused hit samples per suite. Three selected CPU suites pass. Despite
reuse and lifecycle correctness, quiet medians are not a consistent win, so
sharing stays OFF. This is not broad frame-rate acceptance. Release/settings
stay unchanged.

## October 2 — active stereo ray-caster selection

Preceding diagnostic PC is
`16D3040D31FFE6FFAEE2A7650DA4783FAA0898E156F9DEEE5B14C3C65D815CBE`;
unchanged checker is
`B0CD74A7881D59F96B00CC0767EAC776EFDF7B0F55507D2E09C22A65EB6A01DB`.
The preserved immediate PC reference is
`43332D0E574EF31A0B4D6F6BD547C4CCAC260B50C59D99689C13B409468BAAE2`,
at `D:/SFE-validation/sbs-active-casters-baseline-oct2/starfox_pc.exe`.

`Window::ray_caster_vertices()` previously inspected only the mono scene.
`submit_scene()` instead produces the stereo pair when SBS succeeds, leaving
the mono buffer empty or retained from a different frame. The readiness check
now requires complete, nonempty geometry from both active eyes; mono readiness
also requires its completed current batch. No caster, eye, geometry precision,
ray bound or effect is removed. Failed stereo still rebuilds the mono scene.

In actual Intel Vulkan Original Corneria, 2x/full SBS, water/sky/bloom, RT and
High reflections, the immediate reference uploads CPU casters on all 32 frames.
The candidate uses stereo GPU casters with no CPU-caster or presentation
fallback on all 32 frames. Five BMPs match exactly. Intel uses the opt-in stable
hit resolver solely to make the before/after water comparison deterministic;
it remains OFF in production. The ordinary NVIDIA D3D12 and Vulkan paths also
each pass 32 complete GPU-caster pairs and five exact before/after captures.
All captures use 1,000 preroll ticks, fixed temporal clock, 120 target FPS and
frames 8/16/24/32; directories are `sbs-active-casters-{before,after}-*-oct2`
under D:/SFE-validation. Earlier 60-target captures were initially compared
against these 120-target captures and differed; those mismatched protocols are
not accepted as regression evidence. The matched-protocol comparisons pass.

Injected failure after the left eye finishes the current D3D12 capture with
GPU mono casters and rebuilt mono reflections: `sbs-active-casters-fallback-
d3d12-oct2`. All five images exactly match a same-protocol forced-fallback
43332D0E reference in `sbs-active-casters-fallback-reference-d3d12-oct2`.
This is a fallback-path check, not physical stereo/display proof.
The new `capture_lava.ps1 -CheckResidentRayCasters` rejects CPU caster replay,
any unintended presentation fallback, missing stereo GPU-caster status, and
anything other than the requested complete-pair count. Quiet performance is
measured separately; no FPS gain is inferred from removing the fallback.

All 24 ordinary-ray old/new quiet visible 2x water runs finish in
`sbs-active-casters-visible-timing-2x-oct2/results.json`: three trials with
trial 2 reversed, Original Corneria/Venom, full SBS/4:3, 120 target unpaced,
VSync OFF, 1,000 preroll ticks, 60 warmup frames, 360 NVIDIA / 240 Intel measured
frames. No diagnostic tracing, readbacks or resource sampling overlaps timing.
Actual adapters, all complete eye pairs, both binary hashes and unchanged
preferences are verified. Median-of-trial distributions, in microseconds:

| Adapter/backend | Stage | Median old → new | Change | p95 old → new |
| --- | --- | --- | --- | --- |
| NVIDIA D3D12 | Corneria | 10,297 → 10,428 | +1.27% | 16,109 → 18,311 |
| NVIDIA D3D12 | Venom | 7,511 → 7,355 | −2.08% | 12,643 → 13,997 |
| Intel Vulkan | Corneria | 41,325 → 40,710 | −1.49% | 57,575 → 47,263 |
| Intel Vulkan | Venom | 36,628 → 34,528 | −5.73% | 41,465 → 38,609 |

NVIDIA tails worsen. The first and third Intel Corneria reference trials have
multi-second stalls (maximum 4,977,059 / 4,853,501 us); their candidate maxima
are 61,469 / 71,262 us. These outliers are retained, not treated as disposable
warmup. Driver/cache state is not reset, so this is not proof that the fix alone
eliminates cold compilation cost. This is two-scene host frame-work evidence,
not GPU timestamps, displayed FPS, all-device or release acceptance. Broader
performance remains open; ordinary visual parity is not a speed guarantee.

On this same 16D3040D executable, actual standard/4.5/standard switching passes
96 SDK evaluations and 24 retained preview frames, with model-change resets.
All 23 Preview-OFF cases and the RENDERING loading transition pass:
`sbs-active-casters-{dlss,menu}-oct2`. These are ordinary desktop checks, not
native Leia upscaler or physical-panel proof. No forced render upscale or
release copy is introduced.

Current B0CD74A7 checker ordinary suites and all 120 focused samples pass on
each actual NVIDIA D3D12, NVIDIA Vulkan and Intel Vulkan adapter. Ordinary
coverage includes material/model ray transport, independent reflective
underlays, both-eye/foreground retention and 2,925 expanded vertices. Logs:
`sbs-active-casters-ray-*-{ordinary,focused}-oct2.log`. Two selected CPU
arithmetic/stereo suites pass again (0.88 s), all four capture/timing/comparison
script parsers pass, and scoped diff checks are clean. Current/release settings
remain A2B8BDBA / E0F794E8; release executable remains 828D98F4. No device is
attached to ADB. Physical Android GPU relaunch and Leia reconnect/presentation,
native enhancement/upscaler parity and wider sustained performance are still
unverified. Ally work stays user-deferred. The full goal remains active.

## October 2 — looped inline experimental resolver and resource diagnostics

Before the active-caster fix, looped inline depth math was validated at PC
`D280CED2C74E038F0E740BEF0A3ED385780E7A8E588690E188AE5CA46939FD95`
and checker
`AF4F4E30D9C5CD6A03D2004C5D3AA4E0C5A678ADE18E1F9DAABD068DE7FDCD1C`.
Axis loops retain the same portable binary64 operations and association.
Seven fixtures / 120 focused samples now include sloped duplicate planes and
strict one-ULP farther sloped planes. All pass in isolation on actual Intel
Vulkan, NVIDIA D3D12 and NVIDIA Vulkan (`ray-loop-inline-ties-*-oct2.log`).
An all-scalar noinline selected-word alternative compiled but failed Intel's
first duplicate fixture (actual zero, expected two); it was discarded.

Two sequential Intel live repeats at D280 match all 224 input records, all
64 output hashes, all 30 dumped raw files and five BMPs, including against the
preceding 9F7AD791 capture. The linked five experimental-array extent falls
18.36%; this is static binary extent, not driver memory or speed improvement.
The new optional process-resource recorder waits on the same live handle.
First run: 278.0704 s whole-process wall time, 450.25 sampled CPU seconds,
6,762,254,336-byte sampled peak working set. Repeat: 14.8566 s, 6.375 CPU s,
1,246,572,544 bytes. Both terminate normally with no measurement errors.
These are Windows-process metrics, not GPU memory or per-frame timings, and
driver/cache state was not reset; cold-start/memory acceptance remains open.

All 24 D280 quiet visible 2x stable-hit OFF/ON runs finish in
`ray-loop-inline-visible-timing-2x-oct2/results.json`. Median-of-trial frame
work increases 6.74% / 5.79% in NVIDIA D3D12 Corneria / Venom and 2.71% / 2.46%
in Intel Vulkan; several tails change in either direction. This resolver is
not accepted as a performance optimization and stays opt-in.

Opt-in pipeline identity probes at 6EC4E5F4 and 43332D0E record identical root
serialization and separate PSO objects for the same shader on a canonical
device. This does not prove duplicated driver compilation. No shared PSO cache
has been implemented. The no-readback 43332D0E probe exposed persistent CPU
caster fallback, leading to the active-stereo-buffer fix above. Raw diagnostic
readbacks change synchronization and must not substitute for quiet-path tests.
No release, deployment, settings change or full-goal completion is claimed.

## October 2 — inlined depth oracle passes the sampled Intel repeat

Preceding diagnostic PC is
`9F7AD79130456C0ACDE58545C367846430B2905E1AC08536C2102B8D80A6ED93`;
focused checker is
`119A65A2D3C3867A52B4B2265E088F0943DC638F1233E117309A64AAF47F8679`.
Only the experimental scalar-ABI arithmetic helpers lose their noinline
attributes. The ordinary ray PSOs still exclude the resolver and its extra
query/arithmetic; stable hits remain opt-in, not a production default.

The focused checker runs in isolation, with no other GPU workload. All 88
duplicate/farther/strict-one-ULP/transparent/secondary-colour samples pass on
actual Intel Vulkan, NVIDIA D3D12 and NVIDIA Vulkan. Logs are
`ray-inline-ties-{intel-vulkan,direct3d12,vulkan}-oct2.log` under D:.
This does not prove whether the preceding failure was a compiler/ABI issue
or resource competition; its focused and live Intel jobs had overlapped.

Sequential actual Intel Vulkan Original Corneria runs at 2x, full SBS,
water/reflections/bloom, frames 8/16/24/32, now finish successfully:
`intel-water-inline-hits-t{1,2}-oct2`. All 224 CPU/resident input records and
raw position/material bytes match; all 64 native output hashes match, all ten
dumped native output buffers match, and all five full-SBS captures match
byte-for-byte. No tolerance, original ray bound, returned depth or geometry
precision is relaxed. This closes the **sampled repeat**, not every scene.

The first live process continues expensive CPU work after producing its
captures; one sampled working set reaches 6,864,556,032 bytes. It is waited on
to terminal completion, not killed/restarted, and the repeat begins only
afterward. This cold compile/shutdown cost is an acceptance concern separate
from steady-frame timing. A smaller noinline uint2 interface is rejected by
DXIL validation (vector instructions); it is discarded, not checked in as
a usable alternative.

All 24 quiet visible 2x water OFF/ON runs finish in
`ray-inline-visible-timing-2x-oct2/results.json`, on this same PC hash. Three
trials reverse order in trial 2; each uses Original timing, full SBS/4:3,
120 target/unpaced/VSync OFF, 1,000 preroll ticks, 60 warmup frames and
360 measured NVIDIA / 240 Intel frames. No tracing or readbacks contaminate
this comparison. Actual adapters, complete native eye pairs and unchanged
executable/preferences are checked. These are host frame-work distributions,
not GPU timestamps, displayed FPS, all-stage/device or release acceptance.
The table is the median of each trial's median/p95, in microseconds:

| Adapter/backend | Original stage | Median OFF → ON | Median change | p95 OFF → ON |
| --- | --- | --- | --- | --- |
| NVIDIA D3D12 | Corneria | 8,268 → 8,623 | +4.29% | 11,933 → 11,139 |
| NVIDIA D3D12 | Venom | 6,321 → 6,303 | −0.28% | 8,523 → 9,475 |
| Intel Vulkan | Corneria | 29,553 → 30,591 | +3.51% | 31,627 → 32,531 |
| Intel Vulkan | Venom | 24,657 → 25,500 | +3.42% | 26,429 → 27,385 |

Some tails worsen: NVIDIA Corneria median-of-trial maxima rises from 73,790
to 78,979 us, and NVIDIA Venom p95 rises by 11.17%. The first ordinary Intel
Corneria run itself has a 43,049-us median, versus 29,448/29,553 in its other
trials, and has substantial post-render CPU work too. All raw trials are
retained; no poor result is discarded or attributed solely to the resolver.
Small steady-frame cost does not remove the cold-start/memory acceptance
concern. Stable hits remain OFF and are not a production-ready optimization.

With the experiment OFF, this current PC also passes five byte-identical
full-SBS water captures on each NVIDIA backend against CDDE775F. Both
`ray-inline-default-{direct3d12,vulkan}-oct2/results.json` are complete with
unchanged executable/settings and native eye pairs. Ordinary ray component
suites pass on all three actual adapter/backend pairs, including material
coverage, scene/model transport, independent underlays/eye retention and
2,925 expanded vertices; logs are `ray-inline-default-geometry-*-oct2.log`.
The two selected arithmetic/stereo CPU tests and four timing/capture script
parser checks pass. Current DLSS/Preview-OFF acceptance is not rerun here;
the older SDK/menu evidence remains attached to its own CB42012F hash.
Release `828D98F4...` and
preferences A2B8BDBA / E0F794E8 remain unchanged. Nothing is promoted,
published or deployed. Sustained GPU/SBS performance, enhancement parity and
physical Leia/Android acceptance remain open; Ally work stays deferred.

## October 2 — hit-selection experiments; production path remains separate

Preceding diagnostic PC is
`2628AD563E06D6F2EF067EC00FD245AE7841796958525AFE045638F00324CDCA`.
The ray checker is
`0C5429C73C671364BF92885D6E773C09AB10B51987A7746DE03AC0D593421785`.
The proposed hit resolver is **not enabled by default**. Its five experimental
indexed/native/model/water shaders are separate lazy-created PSOs. Ordinary
shader binaries contain neither its second ray query nor its portable binary64
depth arithmetic. No production input readback, geometry precision reduction,
image tolerance, settings or release change is made.

Three iterations are retained rather than treated as an accepted fix:

- `1B43B4CF...` enumerates the native nearest-distance interval without
  committing/pruning its candidates and selects the later source triangle on
  equal returned depths. The actual Intel Corneria control repeat differs in
  39/64 native output hashes; the experimental repeat matches all 64 and all
  five full-SBS captures. All 224 CPU/resident input records and raw dumped
  positions/materials match within each repeat. Its hit-ID repeat is exact,
  and its distance-mode OFF/ON and ON/ON repeats are exact too. This apparently
  successful live result is **not acceptance**: the one-ULP displaced-plane
  regression fails on Intel. Rounded native depths alone are not a sufficient
  physical tie criterion.
- `96D83DCA...` restricts substitution to identical vertex sets. Its 224 input
  records match, but 22/64 native output hashes still differ; sampled frames
  8/24 differ by up to 34/36 channel levels. All three actual-adapter component
  suites and 80 focused samples per adapter pass, but the live water gate
  remains open. The one-ULP case at this checkpoint checks returned distance,
  not a guaranteed native primitive ID; the farther-ID case uses 16 ULPs.
- `2628AD56...` refines rounded-distance ties using the shared portable
  binary64 plane-depth arithmetic, retaining original returned distance and
  ray bounds. Duplicate vertices use a cheap source-order path. Scalar-ABI
  wrappers avoid DXIL's rejected noinline struct-return parameters. All eleven
  ordinary/experimental DXR shader variants compile. The restored strict
  one-ULP primitive test plus transparent coverage and secondary source colour
  pass 88 samples on each actual NVIDIA backend. Intel's first identical-face
  sample instead returns zero, and its live water attempt fails before complete
  captures, with a GPU buffer-memory binding failure/CPU replay. These are
  failed checks, not a driver-root-cause diagnosis or an accepted fallback.

Evidence directories under D: use `intel-water-stable-{0,1}-t{1,2}-oct2`,
`intel-water-stable-hit{1,2}-t*-oct2`, `intel-water-duplicate-hits-t{1,2}-oct2`,
`ray-duplicate-geometry-*-oct2.log`, `ray-precise-ties-*-oct2.log` and
`intel-water-precise-hits-t1-oct2`. Every launched process reaches a terminal
result; incomplete/failing captures are retained. The checker changes its
diagnostic environment through its own CRT, because changing SDL's environment
did not update the DXR code's `getenv`; the initial harness failure is not
counted as a rendering regression.

With experiments disabled, that PC passes five byte-identical full-SBS
reflected-water captures on each NVIDIA backend against CDDE775F, with native
eye pairs and executable/preferences unchanged. Results are
`ray-precise-default-{direct3d12,vulkan}-oct2/results.json`. The two selected
arithmetic/stereo CPU suites pass. Ordinary material/model/scene transport,
independent underlays, both-eye retention and 2,925 expanded vertices also pass
on actual NVIDIA D3D12, NVIDIA Vulkan and Intel Vulkan. Those component checks
are recorded separately as `ray-precise-default-geometry-*-oct2.log`;
experimental focused tests are opt-in and cannot accidentally enable the
rejected PSO in normal CI. Component checks do not close the live water gate.

The timing helpers now support a quiet reflected-water OFF/ON comparison, but
no ray-hit timing matrix is run after the failed correctness checks. Historical
72 hidden plus 72 visible timings below still belong only to CB42012F. Release
`828D98F4...` and preferences A2B8BDBA / E0F794E8 are unchanged. Nothing is
published or deployed. The live Intel gate, sustained performance, enhancement
parity and physical Leia/Android acceptance remain open; Ally work is deferred.

## October 2 — live Intel ray-output inconsistency isolated further

The preceding diagnostic is
`7A4F03666B4E7599B68194916522C9A3BD9B0A8F4E861F8E9684800C387FF8EE`.
It retains the exact multiplier below and adds opt-in DXR input/output and
hit-selection diagnostics. Normal runs do not read resident inputs back,
write diagnostic files or enable the canonical-normal experiment. The earlier
72 hidden and 72 visible quiet timing records belong to `CB42012F...`, not
this new instrumented executable. No new performance claim is made for it.

Two actual Intel Vulkan 32-frame Original Corneria reflected-water runs have
matching 160 CPU-input fingerprints. A second pair compares 224 CPU/resident
records, including 64 queue-ordered resident geometry/material snapshots;
their input fingerprints all match while their final images still differ.
The output-enabled pair localizes differences before SDL import/composition:
all input fingerprints match, but 47 of 64 native ray-output hashes differ.
Optional raw input dumps subsequently confirm identical first-eye/second-eye
position and material bytes (14,688/19,584 bytes per eye). These diagnostics
add synchronization/readback overhead; they are not timing measurements or
proof that every possible resident input in every scene is identical.

Separate raw hit diagnostics find 974/893 first-frame primary triangle-ID
differences across the two eyes, while both primary-distance images are
byte-identical in the distance-mode repeat. The scene contains 40 exact
duplicate vertex-set groups (80 of 306 triangles), including oppositely
ordered Arwing faces. Most observed alternate triangle pairs share their
vertex set. The normal-X repeat differs in 17 of 64 output hashes, although
its first two raw normal images match. This establishes varying hit selection
and a normal-output variation, not the cause of every reflected-water pixel.
DXR does not guarantee a unique incident triangle at a shared edge; the
[Microsoft intersection specification](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#fixed-function-ray-triangle-intersection-specification)
documents that limitation. No image tolerance or intersection bound is relaxed.

An opt-in experiment sorts a triangle's vertices before normal subtraction,
so reversed copies use the same arithmetic order. It changes neither shape,
intersection traversal nor precision. It does **not** pass the full repeat:
with matching inputs, the control has 39/64 different output hashes and the
experiment 35/64. Several model-reflection samples become identical, but
sampled water still differs by up to 39 channel levels. Canonical normals
remain OFF by default; this is not a completed flicker fix or release gate.

Evidence under D: is `intel-water-input-trace-t{1,2}-oct2` (diagnostic
`2E141C93...`), `intel-water-resident-input-trace-t{1,2}-oct2` (`DB4B92EB...`),
`intel-water-output-input-trace-t{1,2}-oct2` (`9DF3C54A...`),
`intel-water-hit-mode{1,2}-t{1,2}-oct2` (`0003F00C...`),
`intel-water-normal-x-t{1,2}-oct2` (`ED86700D...`) and
`intel-water-canonical-{0,1}-t{1,2}-oct2` (current `7A4F0366...`). The CPU-only
first pair captured frames 1/9/17/25 rather than the historical 8/16/24/32;
all subsequent pairs use the historical capture positions. Every process
finishes normally. Helper syntax and scoped whitespace checks pass; the six
DXR shader variants rebuild. The release remains `828D98F4...`, preferences
remain A2B8BDBA / E0F794E8, and nothing is published or deployed.

With diagnostics/experiments disabled, actual NVIDIA D3D12 and Vulkan each
pass five byte-identical full-SBS reflected-water captures against CDDE775F,
without replay/fallback and with executable/preferences unchanged. Results:
`ray-input-diagnostic-default-{direct3d12,vulkan}-oct2/results.json` under D:.
Ray checker `F56467E8...` also passes material/model/scene transport, independent
reflective underlays, both-eye retention and 2,925 expanded vertices on actual
NVIDIA D3D12, NVIDIA Vulkan and Intel Vulkan. Logs use
`ray-input-diagnostic-rays-*-oct2.log`. The two selected arithmetic/stereo CPU
suites pass. These checks protect the unchanged default path; they do not
close the Intel live-water gate, validate all effects or accept a release.

## October 2 — exact fixed-limb multiplication; Intel reflection gate open

The arithmetic checkpoint diagnostic is
`CB42012FD7AECCE1087522BCC6339F671063C43248048648A65FA9A394807E04`.
The shared portable binary64 multiplier now uses fixed 32-bit limb products,
explicit carries and a direct guard/round/sticky extraction. It preserves the
53-bit significand, signed-zero handling, ties-to-even rounding and existing
unsupported-value contract; there is no float32 substitution or quality cut.
It removes the indexed product-building and per-bit extraction/sticky loops.
Ten affected portable artifacts and the VR ray include graph are regenerated.
SPIR-V has no Float64/Int64 capability, and Metal binding conversion passes;
this is not an Apple hardware execution claim.

The host oracle passes 3,016,257 add/sub pairs, 1,006,158 divisions, 1,006,458
binary64 products and 1,000,064 full-width uint32 products, including every
normal exponent and the named near-clipping/quarter-pixel regressions. The
three selected CPU suites pass. Both old and new standalone arithmetic shaders
each pass 262,144 native-double bit comparisons on actual NVIDIA D3D12,
NVIDIA Vulkan and Intel(R) Graphics Vulkan adapters. Independent stereo
ownership/coverage/guide and queued-underlay checks pass on all three adapters,
as do native ray inputs/expansion and 32 Original plus 32 EX real models per
adapter (384 exact indexed images each, bounded normal/depth errors).

Both dedicated-GPU application matrices pass 30 byte-identical full-SBS
presentations, covering original scenery, reflected water, banked 3x grass,
motion blur, MSAA and the native EX menu. All five non-water Intel cases pass
25 exact captures, but its water matrix stops on a real mismatch. A repeated
**unchanged CDDE775F baseline**
also differs from itself: 450/978 pixels in sampled frames, maximum channel
error 10/30. The candidate-vs-baseline difference is similarly localized to
reflective regions, not a screen shift. The independent geometry checks do
not establish the cause of this live reflection nondeterminism. It remains
an open gate; no tolerance is relaxed and the failed matrix is preserved.

All 72 quiet old/new timing processes finish normally: three trials per stage,
scale and adapter, with order reversed in trial two. The complete matrices
verify executable/preference hashes, actual adapters, native stereo pair counts
and absence of fallback. They use Original Corneria / Venom, full SBS, 4:3,
1x/2x, a 120 FPS target with pacing disabled, 1,000 source preroll ticks and
60 warmup presentations. Each trial measures 360 NVIDIA / 240 Intel frames.
These are hidden-window **whole-frame host work** distributions, not GPU
timestamps, occupancy, displayed FPS or a physical-panel result.

Each table entry is the median of three per-run medians or p95 values, in
microseconds; negative change means less work. Raw individual trials and all
outliers remain in the matrices.

| Adapter/backend | Scale | Stage | Old median | New median | Change | Old p95 | New p95 |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| NVIDIA D3D12 | 1x | Corneria | 4,940 | 4,839 | -2.04% | 8,478 | 8,442 |
| NVIDIA D3D12 | 1x | Venom | 3,742 | 2,882 | -22.98% | 6,281 | 6,240 |
| Intel Vulkan | 1x | Corneria | 14,659 | 14,497 | -1.11% | 18,376 | 22,417 |
| Intel Vulkan | 1x | Venom | 12,378 | 9,772 | -21.05% | 14,291 | 11,854 |
| NVIDIA D3D12 | 2x | Corneria | 6,620 | 6,025 | -8.99% | 9,041 | 9,129 |
| NVIDIA D3D12 | 2x | Venom | 4,591 | 3,908 | -14.88% | 6,701 | 7,264 |
| Intel Vulkan | 2x | Corneria | 20,407 | 16,927 | -17.05% | 22,538 | 18,705 |
| Intel Vulkan | 2x | Venom | 15,545 | 12,398 | -20.24% | 17,410 | 14,427 |
| NVIDIA Vulkan | 1x | Corneria | 7,932 | 7,096 | -10.54% | 11,036 | 9,501 |
| NVIDIA Vulkan | 1x | Venom | 6,363 | 5,135 | -19.30% | 7,002 | 5,617 |
| NVIDIA Vulkan | 2x | Corneria | 11,057 | 10,102 | -8.64% | 14,228 | 12,733 |
| NVIDIA Vulkan | 2x | Venom | 7,919 | 6,745 | -14.83% | 8,645 | 7,362 |

The repeated Venom medians improve on all three adapters at both scales;
NVIDIA Vulkan and 2x Corneria improve as well. The small 1x D3D12/Intel Corneria
changes are within trial variation, and Intel's 1x Corneria p95 is worse.
NVIDIA D3D12 still has roughly half-second outliers; Intel candidate Corneria
also has 274/253 ms maxima at 1x/2x, versus 38/46 ms in the old runs. Some
D3D12 p95 values worsen despite lower medians. This is useful arithmetic
progress, not stutter resolution or a general performance-acceptance claim.

Evidence under `D:/SFE-validation` uses `fp64-multiply-*`: arithmetic logs,
selected CPU results, model/stereo/ray component logs, the two completed
dedicated presentation matrices, the failed Intel matrix, the separate passing
Intel non-water continuation and both water difference reports. The exact
baseline executable and copied preferences are in `fp64-multiply-baseline-oct2`.
Completed timing matrices are `fp64-multiply-timing-{1x,2x}-oct2/results.json`
and `fp64-multiply-nvidia-vulkan-timing-{1x,2x}-oct2/results.json`.
The final arithmetic checker `431D7EAA...` repeats all six old/new GPU bit
checks successfully; its logs use `fp64-multiply-final-probe-*`.
The unchanged CB42012F executable also passes actual K/M/K SDK switching
(96 evaluations / 24 retained previews), all 23 Preview-OFF cases and the
separate RENDERING loading capture. Evidence is `fp64-multiply-dlss-oct2`
and `fp64-multiply-menu-oct2` under D:. The three helper parsers and scoped
source/generated-artifact whitespace checks pass.
No sustained-FPS or release acceptance is claimed at this checkpoint.

The subsequent instrumented D3D12 diagnostic reproduces two Corneria
498/494 ms frames and one Venom 500 ms frame. Their presentation phase takes
494/491/496 ms while scene encoding is 1.8/1.3/1.4 ms and retirement is only
16/11/10 microseconds. The trace therefore does not support model-cache
retirement or model encoding as the source of these particular spikes.
The stereo packing/final presentation path is not individually timed yet.
Both visible-window counterparts finish without a 100 ms frame: maxima are
36 ms (Corneria) / 21 ms (Venom). Disabling the embedded SDK in a hidden run
still reproduces 499/488 ms Corneria frames in presentation; that Venom run
has a 25 ms maximum. These checks point to hidden/occluded presentation
behavior, not an established gameplay or SDK stall. They do not isolate the
specific swapchain call or establish general visible performance. Logs use
`fp64-multiply-slow-frame-{d3d12,visible-d3d12,no-dlss-d3d12}-oct2` under D:.
These diagnostics contain tracing overhead and are not additional quiet
timing trials. The quiet helper now accepts and records explicit `-Visible`
for a separate old/new visible-window comparison; its hidden default and
the completed 72-record historical evidence remain unchanged.

The separate visible comparison is now terminal: all 72 old/new processes
pass the same hash/preference, adapter and complete-native-pair checks. Method,
warmup, source preroll and trial ordering are unchanged; every record explicitly
has `visible=true`. These whole-frame host measurements are not pooled with
the hidden trials, and still are not GPU timestamps or physical-display FPS.

| Adapter/backend | Scale | Stage | Old median | New median | Change | Old p95 | New p95 |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| NVIDIA D3D12 | 1x | Corneria | 5,087 | 4,257 | -16.32% | 7,411 | 7,061 |
| NVIDIA D3D12 | 1x | Venom | 3,724 | 2,999 | -19.47% | 6,181 | 6,119 |
| NVIDIA D3D12 | 2x | Corneria | 6,460 | 5,931 | -8.19% | 8,122 | 8,853 |
| NVIDIA D3D12 | 2x | Venom | 4,557 | 3,793 | -16.77% | 6,591 | 6,759 |
| NVIDIA Vulkan | 1x | Corneria | 8,520 | 7,537 | -11.54% | 11,581 | 9,984 |
| NVIDIA Vulkan | 1x | Venom | 6,680 | 5,466 | -18.17% | 7,412 | 6,053 |
| NVIDIA Vulkan | 2x | Corneria | 11,230 | 10,369 | -7.67% | 14,472 | 13,027 |
| NVIDIA Vulkan | 2x | Venom | 8,211 | 6,901 | -15.95% | 8,995 | 7,683 |
| Intel Vulkan | 1x | Corneria | 14,675 | 12,572 | -14.33% | 18,128 | 14,368 |
| Intel Vulkan | 1x | Venom | 12,731 | 9,286 | -27.06% | 14,789 | 10,190 |
| Intel Vulkan | 2x | Corneria | 19,836 | 17,457 | -11.99% | 23,061 | 19,214 |
| Intel Vulkan | 2x | Venom | 15,815 | 12,455 | -21.25% | 17,997 | 13,613 |

Medians improve in every tested visible cell. NVIDIA D3D12 2x p95 is slightly
worse in both stages; all individual trials and maxima remain recorded. The
half-second hidden-window spikes are not reproduced in these visible trials:
the worst D3D12 old/new maxima are 73/68 ms, NVIDIA Vulkan 47/44 ms and Intel
43/43 ms. This supports a measured improvement in these two sampled SBS scenes,
not general stutter resolution, all-effect/device coverage or release acceptance.
Results are `fp64-multiply-visible-timing-{1x,2x}-oct2/results.json` and
`fp64-multiply-visible-nvidia-vulkan-timing-{1x,2x}-oct2/results.json` under D:.

Parallel eye encoding and the earlier approximate/scratch/queue experiments
remain disabled by default. The release stays `828D98F4...` and preferences
stay A2B8BDBA / E0F794E8; nothing is published or deployed. Full-goal acceptance
still requires sustained GPU/SBS performance, the remaining enhancement
parity, physical Leia/reconnect and Android-device relaunch. Ally remains
user-deferred. The timing helper now observes a slow live process on the same
handle rather than stopping its matrix merely because an observation expired.

## October 2 — shared row-table race fixed; parallel default still off

Expanded opt-in checks expose an intermittent D3D12 mixed-layer mismatch on
the earlier `14EA53D3...` candidate; its serial baseline passes. Legacy CPU
binning calls `RasterCommands::bin_rows()`, which mutates vectors shared by
the two eye views. Concurrent rebuilds could race each other's staging copy.
Both eyes now consume tables prepared once before the worker starts, without
duplicating source arrays. GPU-binned chunks retain their original GPU path.
Projection, arithmetic, quality, queue order and fence lifetime are unchanged.

The corrected PC diagnostic hash is
`CDDE775F6818E5757A9F4F00493D8C709108F9C21AC5255C700A7A1837BBC22C`.
The stereo checker now consumes the low-power flag and logs actual adapter
identity. NVIDIA D3D12/Vulkan and actual Intel(R) Graphics Vulkan each pass
20 independent CPU eye images / 6,266,880 exact serial-path guide bytes,
failure/retry, worker recreation, 12 queued pairs with changing shared
CPU-binned underlays and 96 mixed
odd/even helper transitions with shared CPU-binned, changing colored underlays
and covered black ink. D3D12 passes two additional complete checker repeats.
Both dedicated-GPU backends pass native ray geometry, including independently
referenced per-eye shadows/reflections and 2,925 expanded vertices. Component
hashes are `DBBF0E57...` (final queued stereo) and `E9CFD462...` (ray geometry); logs use
`parallel-stereo-immutable-rows-{ownership,rays}-*-oct2` under D:.
The final strengthened queued-underlay checker passes all three recorded
adapters, an additional D3D12 repeat and the serial D3D12 baseline; logs use
`parallel-stereo-immutable-rows-queued-*-oct2`.
The two selected CPU suites, non-SDL scene/raster syntax and all six relevant
PowerShell parsers pass. Final D3D12 and Vulkan application suites each pass
30 byte-identical full-SBS presentations across all six cases, without scene
replay or MSAA fallback. Actual K/M/K switching passes 96 SDK evaluations /
24 retained previews. All 23 Preview-OFF cases and the separate RENDERING
loading capture pass. Those application/SDK/menu results use the exact
corrected CDDE775F hash, with settings unchanged. Evidence is
`parallel-stereo-immutable-rows-present-{direct3d12,vulkan}-oct2/results.json`,
`parallel-stereo-immutable-rows-dlss-oct2/toggle.log` and
`parallel-stereo-immutable-rows-menu-oct2/results.json` under D:.
One outer wrapper incorrectly tested stale native `$LASTEXITCODE` after the
passing D3D12 PowerShell matrix. Its complete six-case result and underlying
terminal process checks are verified; that wrapper stop is not a rendering
failure, and the subsequent Vulkan/SDK/menu continuation completes normally.
Capture/menu/SDK helpers now retain live handles and wait in 30-second chunks
rather than treating an observation delay as completion or killing a process.

The 72 earlier timing records below remain historical evidence of `14EA53D3...`,
not performance acceptance of this corrected binary. The old candidate's
broader correctness failure prevents promotion regardless of timings.
Serial recording remains default; no backend/vendor heuristic is enabled.
The verified release remains
`828D98F478FAADE2D0B30FC6732CBC1E3038A83AB2BA7B0E1BBE42BEBDB9201C`.
Current/release preferences remain A2B8BDBA / E0F794E8. No release copy,
publication, device deployment or WSL recreation occurs. This is a corrected
experimental checkpoint, not sustained-FPS or full-goal acceptance.

## October 2 — initial parallel candidate: timings do not justify promotion

A fresh 180-presentation Vulkan full-SBS host trace records 6,074 model draws.
Median packing/preparation/upload/encoding costs are 4/0/8/31 microseconds per
draw; p95 is 12/0/17/63. The apparent ZACO_A outlier is four cold encodes near
170–186 ms, not steady per-model work. The raw host/pass traces are
`D:/SFE-validation/native-host-triage-oct2` and `native-pass-triage-oct2`.
They are diagnostic host costs, not uncontested performance comparisons or
GPU timestamps; cold costs are not counted as sustained bottleneck evidence.

The next experiment keeps two original queue submissions but records the
independent eyes concurrently. A persistent right-eye worker acquires,
records and submits/cancels its own SDL command; command buffers never move
between threads. Both encodes must succeed before the left is submitted;
the right is then submitted on its worker after the left, preserving queue
order. Failure joins/cancels the borrowed worker job before returning and
publishes neither eye. Existing fence retirement and buffer cycling remain;
there is no ordered-reuse bypass, changed projection, quality reduction,
simulation tick or production readback. Waits still service preparation events.
`STARFOX_TEST_PARALLEL_STEREO_ENCODING` opts in;
`STARFOX_TEST_SERIAL_STEREO_ENCODING` wins. Serial split remains the default.

The earlier diagnostic executable was
`14EA53D35BE29FDCACC37D911AEE48A9DD2FB2C2393B1F7388899B783D211206`.
D3D12/Vulkan ownership checks pass 20 independent CPU eye images and exact
serial-path color/surface/depth/motion comparisons per run, with covered
black ink, resize, cancellation after left encoding,
right-worker failure/retry, same-owner serial/parallel switching, worker
release/recreation, empty clear, borrowed retirement and 12 changing queued
pairs. Logs are `parallel-stereo-ownership-{d3d12,vulkan,intel}-oct2.log` on D:.
Those standalone logs did not record actual adapter identity and their
checker did not consume the low-power selection flag. The `intel` filename
is therefore not accepted as integrated-GPU ownership evidence. Corrected
explicit-selection, logged-identity evidence is in the follow-up above. The separate finite
application timing harness does apply and record actual adapter selection.
Real-game D3D12 and Vulkan full-SBS suites each pass 30 byte-identical
presentations across six cases: Corneria 1x, reflected enhanced water 2x,
banked grass 3x, Venom motion blur 2x, MSAA 2x and a native EX menu control.
Both independent eyes are presented without scene replay or MSAA fallback.
Results are `parallel-stereo-present-{d3d12,vulkan}-oct2/results.json` on D:.
All comparisons use the diagnostic hash above. All 72 quiet timing runs are
complete: three trials per mode, two stages, 1x/2x, NVIDIA D3D12/Vulkan and
Intel Vulkan. Each records actual adapter/backend identity, all complete
native eye pairs and the requested encoding policy. The binary/settings do
not change; compilation, captures and other GPU checks do not overlap timing.
Full SBS, Original Corneria/Venom, original timing, target 120, 1000 preroll
ticks and 60 warmup presentations are unchanged; NVIDIA uses 360 presentations
and Intel uses 240. Trial two reverses the serial/parallel order. These hidden,
unpaced whole-frame host work costs are not GPU timestamps, occupancy or a
physical-display sustained-FPS result. Median-of-medians work and median p95
across three trials, in milliseconds:

| Adapter / backend / scale / workload | Serial median | Parallel median | Serial / parallel p95 |
| --- | ---: | ---: | ---: |
| NVIDIA / D3D12 / 1x / Corneria | 4.988 | 5.021 | 8.264 / 6.952 |
| NVIDIA / D3D12 / 1x / Venom | 3.866 | 3.655 | 7.024 / 5.722 |
| NVIDIA / D3D12 / 2x / Corneria | 6.718 | 6.630 | 10.102 / 9.189 |
| NVIDIA / D3D12 / 2x / Venom | 4.759 | 4.468 | 8.625 / 6.798 |
| NVIDIA / Vulkan / 1x / Corneria | 7.912 | 7.963 | 10.863 / 10.830 |
| NVIDIA / Vulkan / 1x / Venom | 6.276 | 6.308 | 7.005 / 6.898 |
| NVIDIA / Vulkan / 2x / Corneria | 10.963 | 10.979 | 14.227 / 14.260 |
| NVIDIA / Vulkan / 2x / Venom | 7.959 | 7.945 | 8.663 / 8.660 |
| Intel / Vulkan / 1x / Corneria | 14.661 | 15.314 | 17.820 / 17.892 |
| Intel / Vulkan / 1x / Venom | 13.963 | 13.767 | 16.895 / 16.041 |
| Intel / Vulkan / 2x / Corneria | 21.873 | 20.381 | 23.649 / 23.031 |
| Intel / Vulkan / 2x / Venom | 17.865 | 17.973 | 21.362 / 21.088 |

NVIDIA D3D12 Venom median work improves 5.46%/6.11%, with lower p95s in
every paired trial. D3D12 Corneria medians are mixed. NVIDIA Vulkan median
changes stay within 0.65%, not a useful demonstrated improvement. Intel 1x
Corneria regresses 4.45%, while 2x Corneria improves 6.82%; its other trials
vary substantially (1x Venom trial-two p95 is 23.45% worse). All raw outliers,
including cold pipeline spikes after warmup, remain in the results. This
limited two-stage evidence does not justify a universal default, backend or
vendor heuristic. Serial recording remains default; parallel encoding stays
opt-in. Complete matrices on D:/SFE-validation are
`parallel-stereo-timing-{1x,2x}-oct2/results.json` and
`parallel-stereo-nvidia-vulkan-timing-{1x,2x}-oct2/results.json`.
The verified release (`828D98F4...`) and saved settings remain unchanged.

## October 2 — compact clipping: Vulkan-only experiment

The small-face experiment keeps the 129-record output ABI and original word,
compensated and software-binary64 arithmetic. For any cyclic polygon, one
half-plane emits at most floor(3*N/2) corners. Four source corners through near
plus four viewport planes therefore have bounds 4 -> 6 -> 9 -> 13 -> 19 -> 28.
Its 32-entry scratch does not assume convexity or truncate geometry. The model
encoder derives the bound from all uploaded descriptors; larger, unknown and
GPU-expanded colour-warp topology use generic clipping. Defaults remain generic.
`STARFOX_TEST_SMALL_CLIP` opts in; `STARFOX_TEST_FULL_CLIP` wins.

Standalone small shaders passed native/fractional pixel and metadata oracles on
NVIDIA D3D12/Vulkan and Intel Vulkan. Nevertheless the D3D12 executable's 2x
reflected-water SBS case repeatedly loses its device (0x887A0006), while full
clipping succeeds. Its first three captured frames match before the failure;
that is not a passing run. A debug-mode repeat and a repeat without the DLSS
SDK also fail. The causal mechanism is not proven. Small clipping is now
restricted to Vulkan; neither D3D12 nor unexecuted Metal can select it.

The final gated executable is
`7D01400603FA17A2890737988472C66BEA7D0E411ABEEDA6D8128F9FB6AD3234`.
It passes 30 byte-identical Vulkan full-SBS presentations across six cases,
including reflected water, banked 3x, motion blur, MSAA and a no-model EX menu
control. Requesting small clipping on D3D12 retains full clipping, completes
the reflected-water case without replay, and matches all five successful
reference images. Component/model oracles preceded the backend gate on
`2E5D55EF...`; failed executable reproductions use `DB537A56...`, not the final
gated hash. Evidence on D:/SFE-validation uses `small-clip-*` names. Finite
benchmark logs now record the actual adapter for both high/low-power devices.
All 48 quiet three-trial Vulkan timings complete on the gated executable,
with actual NVIDIA GeForce RTX 5070 Ti Laptop GPU and Intel(R) Graphics adapter
names recorded. Full SBS, Original Corneria/Venom, original timing, target 120,
1000 preroll ticks and 60 warmup presentations are unchanged; NVIDIA uses 360
presentations and Intel uses 240. Trial two reverses the full/small order.
No compilation or other GPU checks overlap either timing matrix. These are
whole-frame host work costs, not GPU timestamps or GPU occupancy measurements.
Median-of-medians work and median p95 across three trials, in milliseconds:

| Vulkan adapter / scale / workload | Full median | Small median | Full / small p95 |
| --- | ---: | ---: | ---: |
| NVIDIA / 1x / Corneria | 7.984 | 7.953 | 10.988 / 10.990 |
| NVIDIA / 1x / Venom | 6.342 | 6.363 | 6.997 / 7.042 |
| Intel / 1x / Corneria | 16.043 | 15.730 | 20.969 / 19.082 |
| Intel / 1x / Venom | 13.830 | 12.479 | 16.209 / 16.443 |
| NVIDIA / 2x / Corneria | 11.106 | 11.075 | 14.377 / 14.400 |
| NVIDIA / 2x / Venom | 8.008 | 7.994 | 8.727 / 8.732 |
| Intel / 2x / Corneria | 22.039 | 21.458 | 27.365 / 24.782 |
| Intel / 2x / Venom | 17.847 | 16.753 | 23.468 / 18.683 |

NVIDIA median differences are below 0.4%, not a demonstrated useful speedup.
Intel median work improves 1.95–9.77%, but its 1x Venom p95 is 1.44% worse,
and individual Intel trials have substantial variation. All outliers remain
in the raw results. This limited two-stage SBS evidence does not justify a
general default or a vendor heuristic; compact clipping remains opt-in.
Complete matrices are `D:/SFE-validation/small-clip-vulkan-timing-1x-oct2/results.json`
and `small-clip-vulkan-timing-2x-oct2/results.json`. The verified release remains
`828D98F4...` and is not replaced.

Final rebuilt component checks also pass: generic clipping on NVIDIA
D3D12/Vulkan and Intel Vulkan, compact clipping on both Vulkan adapters,
64 real models / 768 images and queued FRIENDSHIP_4 / 180 images on NVIDIA
Vulkan. Unknown-size inputs retain generic clipping even with the opt-in.
The final component hashes are `7EF05A9C...` (clip) and `7AAC79CB...` (model);
logs use `small-clip-final-{oracle,models,queued}-*-oct2` under D:.
The unchanged gated PC executable passes actual K/M/K DLSS switching with
96 SDK evaluations and 24 retained preview frames; all 23 Preview-OFF cases
skip scene/effect work and retain the unchanged display, while Preview ON
presents RENDERING and then the scene. Both selected input/stereo CPU suites,
four shader source/Metal-binding checks, helper unit tests, non-SDL syntax,
PowerShell parsing and scoped whitespace checks pass. Current/release settings
remain `A2B8BDBA...` / `E0F794E8...`; no release copy or publication occurs.
This is a validated experimental checkpoint, not completion of the full goal
or evidence of a shipped general GPU/SBS performance improvement.

## October 2 — smaller span clears: correctness passes, default rejected

Model span emission initializes a 96-byte command for every painter slot and
screen row. An opt-in path clears only its four coverage bounds (16 bytes),
then overwrites the entire command for each live row. Empty payload is not a
material/geometry result. Buffer sizes, ABI, projection, painter order, source
arithmetic, uploads, submissions, fences and image quality are unchanged.
`STARFOX_TEST_SPAN_BOUNDS_CLEAR` opts in; `STARFOX_TEST_FULL_SPAN_CLEAR` wins.
Ordinary launches still clear the full command. No adapter/stage heuristic is
enabled, and this is not a shipped FPS improvement.

The measured current-folder candidate is
`5716E2044326005D4BB8EB4E9C1FB0FBEC09AE607DB6CD10A953AF36B9BE6615`.
The verified release stays `828D98F4...`; it was not replaced by this experiment.
Three quiet interleaved trials reverse order on trial two, at both 1x and 2x:
Original Corneria/Venom, full SBS, original timing, target 120, God Mode,
1000 preroll ticks and 60 warmup presentations. Default D3D12 uses 360 total
presentations and actual Intel Vulkan uses 240. Each run verifies the actual
span mode, all complete native pairs and absence of CPU replay. All 48 runs
complete; no compilation or other GPU checks overlap the timing matrices.
These are whole-frame host work costs, not GPU timestamps. The default D3D12
adapter name is not recorded by the frontend; do not confuse the separately
identified NVIDIA component device with direct adapter evidence for that run.
Median-of-medians work and median p95 across three trials, in milliseconds:

| Backend / scale / workload | Full median | Bounds median | Full / bounds p95 |
| --- | ---: | ---: | ---: |
| Default D3D12 / 1x / Corneria | 5.492 | 5.060 | 9.799 / 8.364 |
| Default D3D12 / 1x / Venom | 3.817 | 3.712 | 7.369 / 8.865 |
| Intel Vulkan / 1x / Corneria | 15.297 | 13.614 | 18.191 / 16.674 |
| Intel Vulkan / 1x / Venom | 14.175 | 13.783 | 20.552 / 18.964 |
| Default D3D12 / 2x / Corneria | 6.547 | 5.665 | 9.672 / 9.311 |
| Default D3D12 / 2x / Venom | 4.688 | 4.595 | 7.263 / 7.947 |
| Intel Vulkan / 2x / Corneria | 19.257 | 16.997 | 22.685 / 20.364 |
| Intel Vulkan / 2x / Venom | 15.773 | 16.901 | 17.835 / 19.922 |

Corneria improves at both scales, but Intel 2x Venom regresses 7.15% in median
work and 11.71% in p95; default D3D12 Venom p95 also regresses. Large stalls
occur in both modes and remain in the raw results, not discarded or attributed
to this change without evidence. Fewer shader stores do not establish a safe
general default. Raw complete matrices are
`D:/SFE-validation/span-clear-timing-oct2/results.json` and
`span-clear-timing-2x-oct2/results.json`.

Recycled storage is explicitly poisoned before every emission, without cycling
away the poisoned target. Each NVIDIA D3D12/Vulkan and Intel Vulkan fixture
passes 24 cases: 1,358 live commands, 12,978 poisoned empty rows and 20,774,208
exact live-command/colour/surface bytes, including 1x/2x/4x, clipping, invisible
faces, textures and wave lookup. Independent software references pass 25,362
word and 50,724 fractional polygons, 31,418,112 native and 57,003,776 scaled
span pixels per NVIDIA backend, plus boundary/invalid/near-plane cases.
Each backend also passes 768 images of 64 model entries, 18 wave-batch images
and 180 queued wobble/wire/cel/wave images of the visible FRIENDSHIP_4 fixture,
with exact ownership/palettes and checked normal/depth error bounds.
The first six-model wave fixture produced no visible pixels and is rejected,
not counted; the named visible rerun is retained separately.

Actual full-SBS comparisons retain 30 identical presentations per backend:
25 exercise model-span emission (including enhanced water/sky with RT, banked
3x, Venom motion and MSAA); five are a no-span native EX menu control. The
first D3D12 harness incorrectly required a span marker in that no-span menu;
its failed result is retained and the corrected suite reruns all cases.
Final results are `span-clear-present-d3d12-r2-oct2/results.json` and
`span-clear-present-vulkan-oct2/results.json` under D:/SFE-validation.
Raw/component evidence uses `span-clear-{reuse,clip,models,live-model}-*-oct2.log`.
Generated DXIL/SPIR-V/MSL freshness/bindings and non-SDL syntax pass; this is
not Metal/mobile execution, Linux Vulkan acceptance or physical Leia evidence.

The final candidate also passes the actual SDK standard/4.5/standard preview
toggle: 96 evaluations and 24 retained reconstructed presentations. All 23
Preview-OFF menu cases retain the plain path without scene/effect work, and
the separate Preview-ON case presents RENDERING before the rendered scene.
The selected runtime-input and stereo-output CPU suites pass 2/2. Evidence is
`span-clear-dlss-models-oct2/toggle.log`, `span-clear-menu-oct2/results.json`
and `span-clear-selected-cpu-oct2.log` under D:/SFE-validation. These are
regression checks, not a controlled menu-performance comparison.
Final executable hashes still match the candidate/release identities above;
both saved pregame configurations are unchanged. The release is not replaced.

## October 2 — compositor recovery and specialization experiment

Compositor initialization now rolls back partially created pipelines/textures.
A failed initialization leaves no published output and no usable device identity;
the same owner can retry without restarting the borrowed GPU device. Previously
the next call could skip initialization and bind missing resources. This is a
specific failure-recovery fix, not proof of the reported Android boot cause.

The compositor's rare margin histogram contains a 256-element local array.
Separate edge/pixel shaders share the original arithmetic and descriptor ABI;
they add no uploads, submissions, readbacks or waits. Whether that separation
improves occupancy was a hypothesis, not a GPU timestamp measurement. The
specialization remains opt-in (`STARFOX_TEST_SPLIT_COMPOSITE_PIPELINES`);
`STARFOX_TEST_UNIFIED_COMPOSITE_PIPELINE` wins. Normal launches stay unified.

Three quiet interleaved trials, reversed on trial two, use Original Corneria and
Venom, 1x full SBS, original timing, target 120, God Mode, 1000 preroll ticks and
60 warmup frames. NVIDIA D3D12 has 360 total presentations; Intel Vulkan has 240.
All 24 runs verify the actual compositor mode and every complete native eye
pair. No compilation or other GPU tests overlap these timings. The timed binary
is `4E18A2A69595538563D96FD230406FF46E497AEA5FEE6193EC12E868B1061C7B`,
not the later recovery-check binary. Median-of-medians frame work / median p95,
in milliseconds:

| Adapter / workload | Unified median | Split median | Unified / split p95 |
| --- | ---: | ---: | ---: |
| NVIDIA D3D12 / Corneria | 5.265 | 5.636 | 9.799 / 10.489 |
| NVIDIA D3D12 / Venom | 3.686 | 3.740 | 9.332 / 7.582 |
| Intel Vulkan / Corneria | 18.647 | 17.467 | 29.902 / 27.655 |
| Intel Vulkan / Venom | 15.497 | 15.135 | 24.474 / 22.057 |

The dedicated-GPU medians regress; integrated-GPU medians improve, with
variation between trials. A separate quiet Intel 2x pilot pair gives 23.835 vs
24.191 ms (p95 32.759 vs 28.634): no median improvement. These do not establish
a safe cross-adapter/scaling default. Raw results and all outliers are retained
in `D:/SFE-validation/composite-paired-timing-oct2.json` and
`composite-2x-paired-timing-oct2.json`. The later 2x trial-two observation timeout
is not a passed benchmark suite; its process subsequently finished all 240
presentations. That failed harness log is retained. Benchmark observation now
uses a longer cold-start allowance and bounded waits, without killing or
restarting a still-running process.

The final recovery-check binary is
`828D98F478FAADE2D0B30FC6732CBC1E3038A83AB2BA7B0E1BBE42BEBDB9201C`.
On NVIDIA D3D12/Vulkan and Intel Vulkan, each raw fixture compares 36 resized,
jittered, world/HUD, margin, late-overlay and MSAA cases: 5,677,056 exact
packed/normal/depth/motion/RGBA bytes and 22,672 populated depth guides. Both
pipeline modes also reject an injected partial initialization and recover
exactly against a fresh owner, without a device restart. Logs are
`composite-final-guides-{d3d12,vulkan,intel}-oct2.log` on D:. This paired raw
fixture is supplemented by the independent CPU composition oracle; it is not
itself an independent geometry oracle. Generated DXIL/SPIR-V/MSL freshness and
Metal bindings pass, but Metal/mobile execution is not certified.

The final executable also passes 30 identical full-SBS captures per Windows
backend across six cases, including MSAA and the native EX menu, with actual
native eye markers and no CPU replay or partial fallback. Both full independent
CPU composition suites pass, including 108 base cases and the extended window,
wipe, fade, colour-math, protected-overlay and direct-model/glow references.
Two selected CPU suites and the non-SDL syntax check pass. Final evidence is
`composite-final-present-{d3d12,vulkan}-oct2/results.json`,
`composite-final-full-{d3d12,vulkan}-oct2.log` and
`composite-selected-cpu-oct2.log` under D:/SFE-validation.

Both local PC folders now contain the final recovery-check hash above. Each
passes standard/4.5/standard switching with 96 SDK evaluations / 24 retained
previews. The separate OFF/ON/OFF lifecycle check passes; all 23 Preview-OFF
cases and the RENDERING loading presentation pass. Only the EXE was copied,
and preference hashes remain A2B8BDBA / E0F794E8. The previous release is backed
up at `D:/SFE-validation/composite-previous-release-908BABE0-oct2.exe`.
No runtime download, publication, device deployment or WSL recreation occurred.

Full goal completion remains open: sustained GPU/SBS performance, Android
device relaunch acceptance, physical Leia, and the enhancement-parity gaps
tracked in `ALL-EFFECTS-TRACKING.md`. Ally troubleshooting stays user-deferred.

## SBS submission experiment: keep the existing default

The opt-in joined main-scene submission retains independent eye resources and
one shared completion fence. A failed second-eye encoding cancels the whole
command and invalidates both published outputs. Borrowed recordings must retire
both owners first; mixed split/joined use retains a single bounded outstanding
submission budget. It does not change simulation, projection or image quality.
`STARFOX_TEST_JOINED_STEREO_SUBMISSIONS` opts in; the split flag wins. Ordinary
launches retain the preceding split submission policy.

Three quiet, interleaved same-binary timing trials reverse order on trial two.
The timed executable is
`CFFF075DE318E035ED40076491DDC2A19BBF003E2D85A3B9DE833153B789EB50`.
Original Corneria/Venom, 1x, enhancements OFF, original timing, God Mode,
1000 preroll ticks, target 120 FPS and 60 warmup frames are identical between
modes. NVIDIA D3D12 has 360 **total** presentations per case; Intel Vulkan has
240. Every run reports all expected complete native eye pairs. Compilation and
other GPU work did not overlap these comparisons. Median-of-medians frame work
and median p95 across three trials, in milliseconds:

| Adapter / workload | Split median | Joined median | Split / joined p95 |
| --- | ---: | ---: | ---: |
| NVIDIA D3D12 / Corneria SBS | 5.193 | 5.663 | 10.377 / 10.611 |
| NVIDIA D3D12 / Venom SBS | 3.617 | 4.210 | 8.001 / 8.209 |
| Intel Vulkan / Corneria SBS | 17.976 | 18.784 | 28.655 / 31.283 |
| Intel Vulkan / Venom SBS | 16.029 | 15.292 | 21.428 / 19.333 |

This regresses both dedicated-GPU aggregates and gives mixed integrated-GPU
results. The first Intel split Corneria trial's 102.252-ms p95 outlier remains
in the raw results; it is not discarded. Joining is **not enabled by default**
or presented as a speedup. All 24 runs and log paths are in
`D:/SFE-validation/gpu-sbs-joint-paired-timing-oct1.json`.
The preceding 120-frame scene trace reports six owned scene submissions per
Corneria presentation, not just the main two eyes. Joining the main pair removes
only one; delaying its first-eye submit can also reduce CPU/GPU overlap. That
overlap explanation is an inference, not a GPU timestamp measurement.

### Correct cost reporting and deferred model traces

The slow-frame report now sums the actual two-eye scene costs when SBS is active,
instead of reading stale mono-scene counters. `-TraceModelCost` records normal
model host packing, preparation, upload and command encoding; whole-object
billboards bypass this instrumentation. Records are bounded to 32768 per thread,
with an explicit dropped count, and are printed only at shutdown. This avoids
the substantial Windows per-draw stderr disturbance seen in the rejected first
trace. `-TraceNativeCost` remains available for native presentation stages.

On current executable
`908BABE0F8B1EC9323945DFF5FF57B7211659EAC387D7E77B197BCC44CC2E280`,
120-frame normal SBS traces retain 4154 Corneria and 1712 Venom model records,
with zero dropped records, on each adapter. Per-normal-model median / p95 host
costs in microseconds (including startup records, not a warmup-filtered GPU time):

| Adapter / workload | Pack | Upload | Encode |
| --- | ---: | ---: | ---: |
| NVIDIA D3D12 / Corneria | 4 / 14 | 15 / 56 | 27 / 60 |
| NVIDIA D3D12 / Venom | 3 / 11 | 12 / 42 | 27 / 80 |
| Intel Vulkan / Corneria | 4 / 17 | 10 / 21 | 9 / 21 |
| Intel Vulkan / Venom | 4 / 19 | 12 / 27 | 11 / 26 |

The Intel frame-phase averages charge 17.343 ms / 15.412 ms to completion and
presentation, versus 2.182 ms / 0.815 ms to world preparation/submission, for
Corneria / Venom. Individual slow frames still have encoding spikes. These
instrumented runs do not establish an FPS improvement or pure GPU execution
time, and cannot characterize the unknown PC/Android testers' hardware.
Logs are under `gpu-sbs-host-buffer-{d3d12,intel}-oct1` on D:.

Both final D3D12/Vulkan stereo component suites pass 20 independent CPU eye
images, 6266880 exact split-reference color/surface/depth/motion bytes, changed
dimensions, covered black ink, second-eye cancellation/retry, empty clears,
borrowed retirement and 12 queued changing pairs. Both actual ray suites retain
2925 native/fractional/compensated vertices and independently checked reflections
for both eyes. Logs are `gpu-sbs-final-buffer-{stereo,rays}-{direct3d12,vulkan}-oct1.log`
on D:. Selected runtime-input and stereo-output CPU suites pass (2/2); a prior
unprefixed CTest selector matched zero tests and is not counted as validation.
Full executable split/joined captures also pass all four cases on both backends:
Corneria, 2x enhanced water/sky with RT/reflections/bloom, 3x enhanced grass with
camera banking, and 2x Venom with motion blur. Each backend has 20 byte-identical
full-SBS presentations, with actual direct-eye/producer markers and no partial
or CPU-replay fallback. Final capture and selected sequence captures are checked
separately. The initial count validator incorrectly treated the legitimate final
capture as an extra image; that rejected checker log is retained. Final results
are `gpu-sbs-final-buffer-present-{direct3d12,vulkan}-oct1/results.json` on D:;
both the executable and saved preferences are unchanged by the diagnostics.
The non-SDL source syntax check passes. These are correctness/profiling changes,
not physical Leia, Android-relaunch or sustained GPU/SBS acceptance.

Historical ground-shutter executable is `60C8BCAD0B2566393012DAF14B14440B9266FFC3212B12CE37F2E649A2FF2436`.
Newer correctness/local-build checkpoints are in `GPU-MIGRATION-STATUS.md`;
the measurements here do not establish performance of those newer binaries.
It adds optional native ground-surface shading to the joint particle/model
shutter path, plus joined Windows SDL presenter-PSO preparation and destruction-
only fence retirement. Basic native gradients retain their inexpensive shader;
new variants are lazy with constant-time validated packet selection. No new
production readback, submission or ordinary per-frame synchronization is added.
It retains independent DLSS/DLSS 4.5 render-scale selection and fixes explicit
wide-output dispatch/resampling failures within the existing total-pixel budget.
Validation and backup are recorded at the top of `PC-DLSS-STATUS.md`.
Raster/upload/visibility experiments remain opt-in. Measurements below identify their
specific executables; this is not a claim that ordinary GPU/SBS performance is fixed.

The optional native particle payload is projected on GPU per eye/sample,
without CPU point projection/readback or additional submissions/waits. Its
candidate masks are accounted for in resident shutter/MSAA working bounds.
D3D12/Vulkan component/game-owner tests are correctness/lifetime evidence,
not sustained FPS measurements. See the newest stereo-display checkpoint.
The timing tables below remain scoped to their original executable hashes;
they do not measure this native-ground executable. All 32 SDK scale/mode
cases, K/M/K, 23 plain-menu cases and loading presentation pass on this hash.
Installed-folder K/M/K passes as well. Ground correctness/AA and real Original/
EX owner checks pass both backends; the newest GPU/stereo checkpoints record
exact scope, backup and unchanged settings. No universal GPU/SBS gain is claimed.

Historical `879BD987...` candidate-location injected bootstrap evidence passes the
two-second responsiveness limit (longest sampled streak 1,485 ms), while the
exact installed-folder run hits 2,141 ms after presenter preparation. The
installed K/M/K check passes; startup acceptance is not closed. Evidence,
failure/backup/preferences are in the
corresponding stereo-display checkpoint; no broad FPS gain is inferred from these
correctness/lifecycle checks.
The Ally tester reports successful boot at 60 FPS. Further Ally troubleshooting
is deferred at the user's request; this is not a verified 120-FPS fix.

The Discord report concerns PC and Android; the testers' adapters, settings,
backends and exact binaries are unknown. Do not dismiss it as insufficient GPU
power or infer that all cases are CPU-bound. Android still needs device-specific
timing; the measurements below are Windows, not Android acceptance.

## SDL presenter bootstrap: responsiveness, not an FPS fix

SDL's built-in GPU presenter shaders previously compiled synchronously inside
renderer creation, before the game's responsive shader wrappers could run.
The pinned Windows desktop renderer now has a private opt-in joined callback.
Borrowed arguments stay alive until the worker finishes; worker-local SDL
errors/exceptions return to the owner. Pumping stays on the owner and does
not consume queued input. Device/window, window-claim, command-buffer,
swapchain and presentation operations do not move threads.
[SDL renderer creation remains main-thread-only](https://wiki.libsdl.org/SDL3/SDL_CreateRendererWithProperties).
No adapter policy/external device is substituted. Absent hooks and unknown
versions preserve ordinary SDL behavior. UWP/mobile/Linux bootstrap is unchanged.

Four native presenter checks (NVIDIA/Intel × D3D12/Vulkan) each pass 9,216
actual pixels, TLS errors/exceptions, failed initialization followed by retry,
owner pumps and retained input. Logs on D: are
`sdl-presenter-preparation-{dedicated,integrated}-{direct3d12,vulkan}-final-oct1.log`.
The initial 80-ms fixture expected ten native SDL pumps but observed six;
it is retained as rejected fixture evidence. The final 500-ms fixture retains
that assertion and is supplemented by the executable-level test below.

An injected eight-second delay before actual presenter shader creation tests
a slow bootstrap, not real compiler duration. Windows WM_NULL probes enforce
a two-second maximum continuous unresponsive streak. The blocking policy
fails at 2,140 ms while its log is in that exact bootstrap phase. Responsive
preparation passes on default D3D12 (174 responses / 509 pumps / longest streak
1,154 ms), forced Intel D3D12 (193 / 568 / 1,323 ms), and the exact installed
release (175 / 512 / 1,349 ms). All complete 16 Preview-OFF frames and preserve
settings. Records: `sdl-presenter-{delayed-nvidia,delayed-intel,installed-startup}-oct1/results.json`
on D:, and rejected `sdl-presenter-blocking-control-oct1`. The delay applies
only to finite diagnostic runs, not normal launches.

Four ordinary 64-frame Intel startup/exit cases pass (longest sampled streak
1,307 ms, not zero): `sdl-presenter-normal-intel-oct1/results.json`. Actual SDK
checks pass all 32 Original/EX × standard/4.5 × four modes × selected 1x/2x
cases (1,024 evaluations plus retained previews), K/M/K (96 evaluations /
24 retained frames), 23 Preview-OFF cases plus the RENDERING indicator.
The installed binary passes K/M/K again. Evidence on D: is
`sdl-presenter-dlss-scale-r2-oct1/results.json`,
`sdl-presenter-{dlss-model-toggle,installed-model-toggle}-oct1/toggle.log` and
`sdl-presenter-plain-menu-oct1/results.json`. No compilation/other GPU work
overlaps the responsiveness checks. The first scale invocation used exactly
32 frames, ending at the accumulation boundary without a retained frame;
the checker now requires 33 or more, and the successful matrix uses 40.

Installed executable is `2BE878CE...`, settings remain `E0F794E8...`, and previous
`1D9634FD...` is backed up as `sdl-presenter-previous-release-1D9634FD-oct1.exe`
on D:. This closes the missing presenter-shader preparation hook, not every
startup stall. Device/renderer initialization, first-present graphics PSOs,
SDK setup and other owner-thread work may still block and require profiling
on the affected ROG. Android saved-GPU restart and physical ROG/Leia acceptance
remain unproven. Upload/raster/fused-visibility experiments stay off by default;
no sustained-FPS or completed-GPU/SBS claim is made.

## Wide-output fix: default-path regression timing

Installed 524DD82F versus candidate/current 1D9634FD, three quiet interleaved
trials with reversed order on trial two: Original Corneria, 1x, enhancements
off, 200 preroll ticks, 60 warmup frames and 240 total frames. No compilation
or other GPU checks overlapped the timings; upload/raster prototypes are off.
Every full-SBS run presents all 240 complete pairs. Median-of-medians frame
work and median p95 across the three trials, in milliseconds:

| Adapter / workload | Previous median | Current median | Previous / current p95 |
| --- | ---: | ---: | ---: |
| NVIDIA D3D12 / mono | 2.609 | 2.189 | 4.937 / 3.254 |
| NVIDIA D3D12 / full SBS | 4.511 | 4.525 | 8.156 / 7.550 |
| Intel Vulkan / mono | 10.963 | 10.559 | 13.202 / 14.326 |
| Intel Vulkan / full SBS | 18.890 | 19.003 | 21.138 / 21.700 |

These are scoped regression measurements, not reliable speedup evidence.
Large mono outliers occur in both versions (Intel previous 25.636 ms in trial
two, current 16.194 ms in trial three). NVIDIA SBS regresses in one pair and
Intel SBS in two, but the aggregate SBS median differences are 0.3%/0.6%,
with mixed tails. Neither a consistent large new regression nor a universal
improvement is established. GPU/SBS/Android performance remains open.
`D:/SFE-validation/dlss-wide-default-regression-oct1/results.json` preserves
all 24 runs, hashes, parameters and per-trial distributions. Saved preferences
are unchanged. This is not actual Android/ROG/Leia-device acceptance.

## Reproduction and cost split

Plain Original Corneria, 1×, no enhancements, finite hidden/unpaced presentation,
60 warmup frames and 240 total frames on installed executable
`53656336D945E1F084DE2A70B58CDB3F3E69BC100A8D40113B16D511D9A1B16D`:

| Renderer / adapter | Median frame work | p95 |
| --- | ---: | ---: |
| Software | 2.674 ms | 5.071 ms |
| NVIDIA / D3D12 | 1.908 ms | 2.695 ms |
| NVIDIA / Vulkan | 3.081 ms | 3.555 ms |
| Intel integrated / Vulkan | 8.376 ms | 12.061 ms |

Evidence is `D:/SFE-validation/gpu-report-baseline-{d3d12,vulkan}-driver-oct1.log`
and `gpu-report-integrated-vulkan-driver-oct1.log`. Actual adapter/driver names
are recorded in each runtime log. The low-power diagnostic requests an integrated
GPU through SDL properties, not a vendor-name guess.

The Intel scene-cost trace reports roughly 0.304–0.471 ms CPU encoding, 0.002–0.003
ms fence retirement and 0.007–0.024 ms submission for the sampled 19-draw scene.
Most frame work is charged to completion/presentation. This fixture is therefore
not predominantly CPU model encoding; other scenes may be. Per-pass stderr
tracing affects timing and is not used for headline paired comparisons. The
initial LEVEL2_3 sample did not show a stage HUD and is not a busy-space proof.

`tools/benchmark_native_defaults.ps1` now offers `-LowPowerGpu` and
`-TraceSceneCost`, plus LEVEL1_2, LEVEL3_5 and EX LEVEL5_1 workloads. It explicitly
disables saved enhancements, restores environment variables, preserves settings
and reports live PIDs on timeouts rather than killing/restarting a test.

## Shared row-tile experiment: not enabled by default

A small-model shader loads at most 64 row spans into 6 KiB of workgroup storage,
builds two coverage masks and visits live faces in exact reverse painter order.
This removes the preceding global bin dispatch. All partial lanes reach the
barriers; empty, >64-face, wave, resized and jittered inputs retain the ordinary
path. Texture holes, opaque black, zero-valued dither alternates, surface
inheritance, emissive/world ownership and visible geometric depth are preserved.
It adds no CPU projection, readback, submission or wait.

D3D12 and Vulkan component references pass 120 cases / 134,940 independent pixel
checks and byte-exact old/new packed, surface and depth planes, plus native
stereo/motion and sparse-to-moving-model transition checks. Logs:
`gpu-row-tile-{d3d12,vulkan}-oct1.log` on D:. Subsequent final-code checks must
identify the final binary separately.

Quiet, interleaved comparisons use intermediate executable
`04DBAAEE1E86BC39415626EBCD2CFBB03C43D8984D66BCDA89568F7EA21748BB`.
On Intel Vulkan, three 480-frame runs per path give median-of-medians:

| Layout | Ordinary | Shared row tile |
| --- | ---: | ---: |
| Mono | 7.629 ms | 7.772 ms |
| Full SBS | 18.130 ms | 18.705 ms |

Runs reverse ordering on alternating trials, verify every complete SBS pair,
and do not capture images or trace every pass. Evidence directories are
`D:/SFE-validation/gpu-row-tile-pair-intel-{0,2}-{1,2,3}-{separate,fused}-oct1`.
The results do not justify a default change. NVIDIA's two paired trials also
vary by API/layout; one favorable mono timing is not universal acceptance.
The shader remains opt-in through `STARFOX_TEST_ROW_TILE_RASTER` /
`-RowTileRaster`; the disable switch wins. Ordinary rendering does not create
its pipeline or dispatch it. The existing occupied-tile experiment also stays
off. No frame-rate fix or quality reduction is claimed.

## Reflection menu

Software reflections are independent of the GPU hardware-RT prerequisite.
GPU compute shadow rays alone do not provide reflections. The menu now reports
`RT OFF` when supported GPU reflections require enabling RT, and `NEEDS HW RT`
when that implementation is unavailable, instead of ambiguous `LOCKED` /
`UNAVAILABLE`. OFF/LOW/MEDIUM/HIGH remain unchanged on usable paths.
The prerequisite matrix is covered by runtime-input tests. This wording change
does not add compute reflections or prove the reporters' hardware supports RT.

The sustained GPU/SBS slowdown and Android-specific performance remain open.
Native Leia effects/upscalers and physical Leia/Android/ROG acceptance in
STEREO-DISPLAY-UPGRADE.md also remain part of the active goal. WSL was not used,
source was not published and no device build was installed for this report.

## Verified PC checkpoint

Final executable SHA-256:
`857B00B92775E7E3EDF50A9912BFD9B5FBD2DF3911A30D38D32B04B40B5CD84A`.
This version keeps both tile experiments off by default. The only normal-menu
behavior change from the previous checkpoint is the reflection prerequisite
wording; this is not a shipping FPS improvement.

- Native CMake build succeeds without WSL; all three affected generated shader
  headers pass freshness checks. `gpu-report-final-build-oct1.log` on D:.
- Final D3D12 and Vulkan row-tile/stereo checks pass in
  `gpu-row-tile-final-{direct3d12,vulkan}-oct1.log`.
- Final 64-model mixed checks each pass 768 Software/GPU image comparisons,
  exact receiver coverage/palettes, bounded normal/depth error and five-layer
  mixed painter reuse: `gpu-report-final-model-{direct3d12,vulkan}-oct1.log`.
- Runtime-input tests pass, including every software/hardware/RT/quality menu
  prerequisite combination: `gpu-report-runtime-input-oct1.log`.
- All 23 Preview-OFF/loading cases pass with no scene/effect work on the plain
  path. `gpu-report-final-plain-menu-oct1/results.json` identifies this binary.
- Actual ordinary PC DLSS OFF/ON/OFF passes 33 SDK evaluations; standard K /
  4.5 M / K passes 96 evaluations plus 24 retained preview frames. Logs:
  `gpu-report-final-dlss-{toggle,model}-driver-oct1.log`. This is not native Leia
  neural integration or a repeat of every quality-mode image reference.

`build/release/starfox_pc.exe` was replaced after the executable was closed
and candidate/previous hashes were checked. The previous 53656336 executable is
recoverable at `D:/SFE-validation/gpu-report-previous-release-53656336-oct1.exe`.
Saved preferences remain
`E0F794E81E64295A34AC93043E4B8D123EA7481C9937CDB13C7784FF072B95B4`.
`build/current`, publications and device installations remain unchanged.
The installed release-folder K/M/K check also passes 96 SDK evaluations / 24
retained preview frames with that exact hash and unchanged preferences:
`D:/SFE-validation/gpu-report-release-dlss-model-driver-oct1.log`.

## Follow-up: colour-only specialization stays diagnostic-only

The raster now has a separate colour-only shader for row spans with neither
requested nor inherited receiver/depth outputs. It removes unused shader work
and bindings, not an effect or a quality setting. Draws with either metadata
output retain the full shader. The specialization is opt-in through
`STARFOX_TEST_PIXEL_ONLY_RASTER` / `-PixelOnlyRaster`; `-GenericRaster` wins.
It adds no CPU projection, image upload, submission or fence.

Three interleaved runs per path, reversing order on the second trial, used
intermediate executable SHA-256
`F1A3012149DFABA0155D1E9535F4EDB33098C29A862C18DE3259646320CA885F`.
Plain Original at 1×, all enhancements off, 200 preroll ticks and 60 warmup
frames; Intel runs have 480 frames, NVIDIA runs 360. Every SBS run presented
the requested number of complete eye pairs. Median-of-medians in milliseconds:

| Adapter / workload | Full shader | Colour-only |
| --- | ---: | ---: |
| Intel Vulkan / Corneria mono | 8.808 | 9.003 |
| Intel Vulkan / Corneria full SBS | 18.240 | 18.763 |
| NVIDIA D3D12 / asteroids mono (1-2) | 2.154 | 2.186 |
| NVIDIA D3D12 / Venom mono (3-5) | 2.694 | 2.667 |
| NVIDIA D3D12 / Corneria full SBS | 4.702 | 4.719 |

There is run-to-run drift and an Intel mono/NVIDIA asteroid outlier. These
numbers do not establish a repeatable overall gain or a causal regression;
one marginally faster workload does not justify changing the default. Logs
are `D:/SFE-validation/gpu-pixel-pair-intel-{0,2}-{1,2,3}-{generic,pixel}-oct1`
and `gpu-pixel-pair-nvidia-d3d12[-sbs]-{1,2,3}-{generic,pixel}-oct1`.

The initial installed-857B00B9 busy-stage baseline also shows why GPU cannot
be assumed universally faster. At 1-2, Software/GPU medians are 2.486/1.796 ms;
at 3-5 they are 2.636/2.787 ms. Both have 360 HUD-bearing gameplay frames after
200 preroll ticks. See `gpu-busy-baseline-nvidia-d3d12-driver-oct1.log` on D:.
This is Windows hidden/unpaced frame work, not an Android or physical-display
FPS guarantee, and does not prove every complex asteroid is CPU-bound.

Final gated candidate SHA-256:
`E8DCC03C888F4C904EF09798398DB479BFD312A596EEB4F7652550307B1006E1`.
Its expanded D3D12 and Vulkan checks each pass 144 metadata cases / 161,928
independent pixels and 360 colour-only cases / 458,316 independent pixels,
with byte-exact full/specialized output parity. Cases cover empty and 64/65-face
inputs, black/dither, wrapping texture holes, inherited receivers, emissive
ownership, cache reuse, copy/in-place, signed waves, custom extents and positive
and negative jitter. Stereo/motion and sparse-to-moving transitions also pass.
Logs are `gpu-pixel-gated-oracle-{direct3d12,vulkan}-oct1.log` on D:. Both earlier
backend real-model checks pass 768 Software/GPU images each; those checks
identify the intermediate, not final gated, binary. All four affected portable
shader headers pass freshness/binding checks.

The candidate remains on D: and is not installed or published. The installed
release executable remains 857B00B9, with unchanged saved preferences. The
sustained slowdown, Android-device timing and ROG Xbox Ally device acceptance
are still open. Existing Windows responsive-preparation/recovery safeguards
are included in the installed build, but are not a confirmed Ally-device fix.

## Follow-up: ordered model-input upload reuse

The model producer now has an explicit single-command-recording scope for
byte-exact read-only input reuse. It compares packed bytes rather than shape
addresses or hashes; only changed buffers are uploaded. Artwork changes,
layout changes and interleaved billboard uploads invalidate the appropriate
snapshot. Every scope ends before submission/cancellation; a new recording
uploads again, even when SDL recycles its command-buffer address. The memo
budget is 1 MiB per producer. There are no extra GPU reads, fences or CPU
projection. Standalone model calls remain uncached.

This is still opt-in via `STARFOX_TEST_MODEL_UPLOAD_REUSE` /
`benchmark_native_defaults.ps1 -ModelUploadReuse`; `-DuplicateModelUploads`
wins. Explicit per-draw diagnostics (`-TraceModelUploads`) are excluded from
timing. In three 120-frame Original traces, input bytes avoided are 30.41%
in Corneria (1-1), 20.11% in asteroids (1-2), and 22.67% in Venom (3-5).
These are upload-accounting results, not FPS gains.

Three interleaved NVIDIA D3D12 timing trials reverse order on trial two.
All enhancements are off, 1x, 200 preroll ticks, 60 warmup frames and 360
measured frames. Median-of-medians frame work, in milliseconds:

| Workload | Original uploads | Byte-exact reuse |
| --- | ---: | ---: |
| Corneria mono | 2.553 | 2.371 |
| Asteroids mono | 2.539 | 2.492 |
| Venom mono | 2.789 | 2.797 |
| Corneria full SBS | 4.854 | 4.757 |

SBS median improves in all three pairs, but mono reverses in some pairs and
p95 results are mixed. The run-to-run drift and modest workload-dependent
improvement do not justify enabling this globally. Integrated/device timing
is still pending; the prototype remains off while the explicit DLSS-scale
request is addressed. Logs are on D: under
`gpu-model-upload-pair-nvidia-{mono,sbs}-{1,2,3}-{duplicate,reuse}-oct1`.
The timed candidate is SHA-256
`A7BDF0B30EABCDF3E9494770C950FDB91F81BD56A6648D76632CAFDA551D79A7`.

Both D3D12/Vulkan independent upload oracles pass 39 images / 1,886,976
SoftwareRenderer pixels at 1x/2x/4x, avoiding 210 read-only copies / 14,748
bytes. They cover same-address texture mutations, pose/vertex/material/layout
changes, opaque black, billboard interleave, metadata switching, cancellation
and standalone recovery. Both real-model mixed checks pass 768 images with
exact coverage/palettes. Existing row-span and stereo/motion checks also pass.
`gpu-model-upload-{oracle,reuse}-{direct3d12,vulkan}-oct1.log` records these
checks; they do not certify Android/ROG/physical Leia behavior or complete
the full goal.

### Bounded snapshot capacity and integrated-GPU cross-check

The memo now retains its bounded high-water capacity when a model layout
shrinks/regrows, avoiding repeated host allocations within a recording. The
1 MiB cap is unchanged; release frees that capacity. The expanded D3D12 and
Vulkan oracles each pass 45 images / 2,177,280 independent SoftwareRenderer
pixels, including explicit shrink/regrow capacity assertions. Both real-model
checks pass 64 models / 768 images with exact coverage and palette identities.
Evidence is `gpu-model-upload-capacity-{oracle,models}-{direct3d12,vulkan}-oct1.log`
on D:. This is not an across-recording or pointer-keyed upload cache.

Quiet interleaved timing on candidate SHA-256
`FB551B3FA965B38CE91AC2F9EC19B542E57830B5C8C7B696CF5238A6115AED68`
used three trials, reversed order on trial two, Original 1x with enhancements
off, 200 preroll ticks and 60 warmup frames. Intel Vulkan runs have 480 frames;
NVIDIA D3D12 runs have 360. Every full-SBS run reports that many complete pairs.
Median-of-medians frame work in milliseconds:

| Adapter / workload | Original uploads | Byte-exact reuse |
| --- | ---: | ---: |
| Intel Vulkan / Corneria mono | 9.356 | 8.054 |
| Intel Vulkan / Corneria full SBS | 19.494 | 16.135 |
| NVIDIA D3D12 / Corneria mono | 2.797 | 2.398 |
| NVIDIA D3D12 / Asteroids mono | 2.190 | 1.959 |
| NVIDIA D3D12 / Venom mono | 2.742 | 2.723 |
| NVIDIA D3D12 / Corneria full SBS | 4.678 | 4.807 |

Results still drift materially. Intel mono reverses on trial three, and NVIDIA
SBS is slower in two of three trials, with worse median p95 (6.267 -> 7.413 ms).
The favorable aggregate Intel numbers do not establish a universal gain. The
experiment remains opt-in/off by default, not installed as a performance fix.
Logs: `gpu-model-upload-capacity-intel-{0,2}-{1,2,3}-{duplicate,reuse}-oct1`
and `gpu-model-upload-capacity-nvidia-{mono,sbs}-{1,2,3}-{duplicate,reuse}-oct1`
on D:. No compilation or other GPU checks overlapped these timings.

## Fused source visibility and single-model BSP painter experiment

The opt-in model path computes source face visibility in 64 parallel lanes,
then walks the authored BSP in the same group/dispatch. The active recursion
stack is group-shared; legal shared subtrees still repeat and active cycles
are skipped. Native signed-word arithmetic and compensated continuous face
tests are shared with the existing separate visibility shaders. This removes
one dispatch/pass boundary per eligible model/eye without CPU projection,
GPU readback, altered painter order or quality reduction. Larger than 4096
visibility-face inputs keep the existing path. `STARFOX_TEST_FUSED_VISIBILITY_BSP`
or `benchmark_native_defaults.ps1 -FusedVisibilityBsp` opts in;
`-SeparateVisibilityBsp` wins. A once-per-producer success marker verifies
that the timings actually use the path; per-draw tracing is not enabled.

Three quiet interleaved NVIDIA trials reverse order on trial two: Original
Corneria, 1x, enhancements off, 200 preroll ticks, 60 warmup frames, 360 total
frames. Every SBS run presents 360 complete pairs, and no run replays the
frame through SoftwareRenderer. Median-of-medians frame work and median p95,
in milliseconds:

| Backend / workload | Separate median | Fused median | Separate / fused p95 |
| --- | ---: | ---: | ---: |
| D3D12 / mono | 2.281 | 2.186 | 3.327 / 3.388 |
| D3D12 / full SBS | 4.779 | 4.585 | 7.596 / 8.258 |
| Vulkan / mono | 3.678 | 3.640 | 4.452 / 4.310 |
| Vulkan / full SBS | 7.740 | 7.607 | 9.240 / 9.085 |

The D3D12 SBS median improves in all three pairs, but p95 worsens in two.
The Vulkan SBS improvement is under 2%, with one pair nearly unchanged.
Single-trial busy-stage checks are also mixed: D3D12 Venom regresses from
2.669 to 3.075 ms, while D3D12 asteroids improves from 3.128 to 2.957 ms but
has worse p95. Vulkan asteroids regresses from 2.925 to 3.002 ms; Venom is
almost unchanged (4.612 to 4.584 ms). These single trials do not establish
stage-specific gains. Initial single-pair Intel Vulkan mono/SBS checks were
nearly unchanged (7.955 -> 7.931 ms / 16.167 -> 16.118 ms).

The optimization therefore remains **off by default and uninstalled**.
The timing executable SHA-256 is
`7FCBAFFA353C1026C6200E9BD33CB65134A95177FD5A5D0DD3611582FCB8738B`.
All 32 repeated/busy NVIDIA run logs and parameters are retained under
`D:/SFE-validation/gpu-fused-visibility-bsp-nvidia-pairs-oct1`;
the four initial adapter/workload pairs are under
`D:/SFE-validation/gpu-fused-visibility-bsp-triage-oct1`.
No compilation or other GPU work overlapped these timings.

The subsequent resource-lifetime refinement initializes the two pipelines
independently and preserves resident visibility when one recording switches
from fused to generic BSP. It does not silently create the unused generic
pipeline first. This final diagnostic executable is
`C4CC6C9AB15D9CA4EA328C7312E66613F5806C6BD6620B8E28E6176C56DE95A4`;
it is not the timed binary above. On NVIDIA D3D12, NVIDIA Vulkan and Intel
Vulkan, its independent binary64/wrapped-word source oracle each passes
112 cases / 71,152 visibility flags / 1,212 authored order entries, including
same-recording path switches, failures, alias/bounds rejection, shared
subtrees, active cycles and canceled-command reuse. Existing 128 generic
BSP trees / four failure cases / resident order-to-raster fixtures pass too.
Each adapter also passes 64 real models / 768 SoftwareRenderer reference
images with exact coverage/palettes, mixed-layer composition and cancellation
recovery. Logs are `gpu-fused-visibility-bsp-switch-{direct3d12,vulkan,intel-vulkan}-oct1.log`
and `gpu-fused-visibility-bsp-final-models-{direct3d12,vulkan,intel-vulkan}-oct1.log`
on D:. Generated DXIL/SPIR-V/MSL freshness/binding checks pass for all three
affected shaders; Metal/mobile execution is not certified by those checks.
Seven freshly rebuilt CPU tests pass: packed faces/projection/BSP, runtime
input, stereo output, GPU preparation and responsive preparation. Evidence:
`D:/SFE-validation/gpu-fused-visibility-bsp-cpu-tests-oct1.log`.

The earlier diagnostic also passes laser-axis, colour-warp-axis and fractional
destruction references on D3D12/Vulkan, plus projection/stereo/depth oracles.
The generic native-axis model selection produced no visible pixels and was
correctly rejected, not counted as a pass; the named ELASER2 fixture passes
both separate and fused paths. All failed evidence is retained.
Installed release executable `1D9634FD...` and saved settings `E0F794E8...`
remain unchanged. This experiment does not close GPU/SBS/Android performance,
Android saved-GPU restart, ROG startup or physical Leia acceptance.
