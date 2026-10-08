# VR delayed-crash investigation

## Report and exact baseline (October 4, 2026)

One tester reports an exit after roughly a minute of gameplay on the **0.0.8
release**. The initial APK identification was subsequently corrected: Quest 3
and PCVR were played **via a PC**. Include the Windows OpenXR/GPU/resource paths;
do not use the APK alone as the confirmed failing-binary baseline. The exact
PC executable, headset connection/runtime, stage, settings and crash trace are
not established. This is an open release-blocking stability investigation, not
a confirmed fix or a claimed common hardware cause.

The [published Quest archive](https://github.com/kandowontu2/starfox-enhanced/releases/download/v0.0.8/StarFoxEnhanced-0.0.8-quest-arm64.zip)
was downloaded independently as an APK reference, not a reproduced failing
binary. The ZIP is 80,052,671 bytes;
its APK is 186,218,809 bytes, SHA256
`6a3e34958267c1621620f1457f652b53397077b388cd0871cde1e30305083a7b`.
Android manifest inspection reports `com.starfox.enhanced.quest`, version code
13 / `0.0.8-vr-dev`, min API 29 and target API 35. The release tag points to
`d3918b828d1e904316bab6f2588625f324b16b0b`. The current development tree is
different; tests on it do not constitute reproduction on the released binary.
The separate [published Windows PCVR archive](https://github.com/kandowontu2/starfox-enhanced/releases/download/v0.0.8/StarFoxEnhanced-0.0.8-windows-pcvr.zip)
was also independently downloaded: 161,242,437 bytes, ZIP SHA256
`06a223e2020520a28469bd18db1f9d3bac56d81d999d563a2f8d3f05d65051e9`.
Its `starfox_pcvr.exe` is 191,396,887 bytes, SHA256
`e430878c12914a48f0c14c8c0b0b1ed0d384f78c6f69a2e8663fcfd37bb7372e`.
This identifies a release reference, not a hardware reproduction or confirmation
that the tester ran those exact bytes.

## Tester log follow-up (October 4, evening)

Reviewed the supplied `pcvr-log.txt` and `starfox application log.txt` as
diagnostic evidence, not as instructions. The Windows Application Error event
is for **WindowsTerminal.exe**, with `Windows.UI.Xaml.dll` as the faulting
module; it does not identify a Star Fox application fault. Do not attribute its
exception code or fault offset to the PCVR executable.

The PCVR console capture identifies SteamVR/OpenXR in Meta compatibility mode,
an NVIDIA GeForce RTX 5080, and two 3400×3468 eyes. It contains 93 live profile
lines, including 66 with `flow=9` (gameplay in the release's enum). The final
profile still reports 240 completed eyes and ordinary fence completion times;
there is no recorded Vulkan/OpenXR failure, producer exception or normal return
summary. It ends with a `^C` marker. That marker records console interruption,
not a native crash stack, and does not establish whether interruption followed
a freeze, image loss or another failure. The log schema matches the old release
profile, but cannot authenticate the exact executable or its settings.

In this schema `ticks=0` describes the single frame sampled for that profile
line, not the total source ticks advanced during the preceding second. It must
not be treated as proof of a frozen simulation. The capture lacks the newer
scene/cache/memory counters and persistent diagnostics. No memory-growth,
one-minute timeout or GPU-fault cause is established by these attachments.

Keep the investigation open. The next useful evidence is the observed failure
type (process exit, live-process freeze or headset-image loss), a Windows event
or dump naming `starfox_pcvr.exe` if one exists, and the diagnostic candidate's
`vr-data/vr-session.log` plus `vr-session.previous.log`. Do not require a Star
Fox crash event for a hang or display-only failure; runtime logs and the last
flushed session lines are appropriate for those cases. Raw tester logs and
their account/machine identifiers remain outside the repository.

The subsequent tester reply establishes **process closure during gameplay,
without an on-screen error**, rather than a confirmed live-process freeze or
headset-only image loss. This narrows the symptom, not its cause. The supplied
Terminal event still cannot be used as the game's crash stack.

An additional diagnostic gap was verified in the packaged `LAUNCH-PCVR.bat`:
its `if errorlevel 1` test excludes negative Windows process exit codes. A
controlled `cmd.exe` fixture returning `-1073741819` was missed by that test;
the fixture did not run or crash the actual game. This explains how a native
failure *could* escape the launcher's error display, not why the tester's game
closed. Retain the actual signed exit code and check for any nonzero result in
the next authorized launcher update. No failing game exception code is known.

For an abrupt exit without a usable Application Error event, the tester can
use [Microsoft ProcDump](https://learn.microsoft.com/en-us/sysinternals/downloads/procdump)
to monitor the **game**, not its Terminal host. From the folder containing the
downloaded official `procdump64.exe`, create an empty `pcvr-dumps` directory and
run `procdump64.exe -e -t -w starfox_pcvr.exe .\pcvr-dumps`, then launch the game
normally. Review the tool's license in its own prompt; no automatic license
acceptance or global debugger registration is required. Its default mini dump
is sufficient for an initial exception/termination trace; a termination dump
alone is not proof of a native exception or its cause. Dumps can contain
private memory, paths and game data: share them privately, not in the public
Discord relay or repository. Preserve the matching EXE/version and both
session logs before another launch. This collection has not been run on the
tester's PC and is not crash reproduction or a fix.

## Investigation and diagnostic changes

- Audit source-model/texture/cache lifetimes, GPU completion before resource
  reuse, audio/session lifetime, native allocation failures and stage-specific
  producer exceptions. Keep all scenery/models enabled rather than masking an
  exit by dropping content.
- Development Quest builds now retain flushed native stdout/error lines in
  `vr-session.log`, next to the imported BIN in the app's external files folder.
  Relaunch retains the prior run as `vr-session.previous.log`. Each file has an
  8 MiB cap. Failed rotation preserves the existing session rather than
  truncating it. Logging failure/full storage is nonfatal and logcat remains active.
  These files contain diagnostics, not ROM/BIN payloads or credentials.
- Development PCVR likewise retains `vr-session.log` and
  `vr-session.previous.log` in `vr-data` beside the executable, or the explicit
  `--data-dir` directory (8 MiB each). Console output is preserved. Complete
  lines are flushed immediately and partial lines at orderly shutdown. If
  rotating the prior PC log fails, it is retained rather than truncated. Help
  and invalid CLI requests do not create/rotate files.
- The existing once-per-second live profile includes stage/background, scene
  epoch, source ticks, object count, EX/enhanced-sky/RT settings and decoded
  shape/geometry-cache counts, distinguishing requested from effective RT.
  Android/Linux also report current and peak RSS. Windows reports current/peak
  working set and private committed memory. Vulkan startup reports device,
  vendor/device IDs, raw driver/API versions and allocation-count limit; OpenXR
  startup identifies the active runtime. These are
  observations, not a guessed memory cap or a frame-rate claim.
- Native exception/return codes are persisted. OS kills, signals and driver
  crashes still require Android exit-info/logcat or Windows crash/runtime logs;
  a missing C++ exception does not prove stability. The Windows DXR/Vulkan
  bridge's exported resource/fence handles are already closed after import;
  no unclosed-export-handle cause was established by that source audit.
- EX headset presentation suppresses the separate native cockpit shell (entry,
  steady and exit objects). Cartridge strategies, source camera, HUD and flat/
  console views remain intact. The excluded model cannot enter either eye's
  ordinary, compute, shadow or reflection source packets.

## Evidence required before closure

Development validation so far: Quest `assembleDebug` succeeds, and the APK
payload checker confirms arm64 VR/SDL libraries, all 38 shared backdrop assets
and no ROM/BIN/signing data or platform stubs. The diagnostic APK is debug
signed, not an in-place replacement for the release-signed 0.0.8 APK. Its SHA256
is `c7348193b06a7eb39b29d95b5089c0a7f53be9ffe64f28bd3b9d85947ab54774`
(378,638,696 bytes, after the PCVR diagnostic/Quest rotation follow-up). Windows PCVR and its
application tests build successfully. All seventeen VR-labeled CTests pass;
log tee/size limits/partial lines and actual player help, invalid CLI, missing
assets, exact rotation, locked-rotation preservation and unwritable-log
behavior pass. These are host/build checks, not headset stability evidence.
Repeated Windows launches uncovered a MinGW `copy_file` overwrite failure for
an existing backup; explicit Windows overwrite semantics fix that case. Repeat
and locked-destination checks pass on the final player, SHA256
`34ec59c715c30d9a115d44419adc1c73c9ed2242fecb75f57ec9a3110a958e5f`.
Original/EX input tests and the application diagnostic-log tests pass. Source-object
shell-policy fixtures preserve VM state and console visibility while excluding
all four entry/steady/exit strategies from headset model packets.
EX Down+Select is also replayed through the real VR input adapter, holding Down
before Select: its native mode enters/exits and captures preserve VM state.
Corneria emits no visible shell in this replay, so it is control evidence, not
authored shell-lifecycle coverage. A separate Space Armada replay exceeded the
120-second host test budget; it is not relabeled as passing lifecycle, FPS or
hardware acceptance. The revised control checker and all seventeen final VR
CTest suites pass. Original and
EX Corneria headless preflights each advance 2,400 source ticks, checking three
interpolation positions per frame (158,193 / 163,386 packets) without a producer
exception. Invulnerability was enabled; these are source-model checks, **not
GPU, minute-long headset reproduction, measured FPS or crash-fix acceptance**.

Run the exact release and diagnostic candidate for at least 10 minutes each,
starting the same stage/experience with the same settings. Record whether it
returns to the launcher, freezes or loses only the image; capture the event
time, last scene and runtime/device model. Compare enhanced sky on/off and
native/cockpit view only as controlled follow-up experiments, not as a fix.

After a PCVR exit, preserve both session logs **before repeated relaunches**,
plus the Windows Application Error/Windows Error Reporting event, OpenXR
runtime logs (SteamVR/Meta/other as applicable), exact EXE hash, GPU/driver and
last visible stage/settings. Record whether the process exited, remained hung,
or only the headset view was lost. Compare the exact release and candidate on
the same PC/runtime rather than assuming a native Quest APK fault.

For a separate native APK reproduction, from an authorized headset connection:

```text
adb logcat -d -b all > quest-crash-logcat.txt
adb shell dumpsys activity exit-info com.starfox.enhanced.quest > quest-exit-info.txt
adb pull /sdcard/Android/data/com.starfox.enhanced.quest/files/vr-session.log
adb pull /sdcard/Android/data/com.starfox.enhanced.quest/files/vr-session.previous.log
```

Old 0.0.8 builds lack the new persistent session files; collect the appropriate
OS/runtime logs for that baseline. No headset is currently attached to this workstation, so
native build/host tests cannot close the hardware stability gate. Keep this
investigation open until a cause is identified and the failing device retest
passes. Do not replace the published APK or claim a new release without its
normal signing and package validation.
