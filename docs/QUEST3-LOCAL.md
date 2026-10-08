# Star Fox Enhanced on Quest 3

This local build is based on `kandowontu2/starfox-enhanced`, commit
`8fa1aa5e31c9f82beed58ea22b32bdb738b8137b` (0.0.8).
It runs the existing native C++/Vulkan/OpenXR renderer directly on Quest 3.
No PC is required to play.

The Quest presentation uses the existing stereoscopic third-person view with
head tracking. The separate Steam Frame cockpit mode is not enabled here.

## Local additions

- ROM import directly on Quest: supported, unmodified `.sfc`/`.smc` files are
  validated and converted into Original/EX assets compatible with this build.
  A 512-byte copier header is supported.
- Existing `Starfox-Assets.BIN` files are still checked for version compatibility
  and valid checksums. Failed imports do not replace existing game data.
- The desktop asset builder and Quest share the same import implementation.
- The Windows build automatically downloads the pinned SDL archive and verifies
  its SHA-256 checksum. `-BuildRoot` allows a short native build path.
- Fixed Quest action buttons: A fires, Y brakes. The original B/Y remapping in
  control types B/D is compensated for before each game tick.

## Launching and controls

In the Quest library, open **Star Fox Enhanced VR (Development)** under
**Unknown Sources**. Select Original or EX in the startup menu, then choose
**Start Game**.

Starting with version `0.0.8-quest3-local.2`, Quest action buttons have fixed
assignments:

| Button | Action |
|---|---|
| A on the right controller | Fire |
| B on the right controller | Bomb |
| X on the left controller | Boost |
| Y on the left controller | Brake |

This mapping applies to **all control types A/B/C/D**, including when a swap
option was previously saved. The Quest VR options show the fixed label
**A: FIRE / Y: BRAKE**. Types C/D still invert vertical steering.
In the VR startup menu, **A on the right controller** confirms the selection.

| Touch input | Additional actions |
|---|---|
| Left stick | Steering / menu navigation |
| Left / right trigger | Roll left / right |
| Right grip button | Start / pause game |
| Left grip button | Select |
| Both grip buttons | In-game options menu |

Hold both triggers, then press both sticks to reset the current game to the
startup menu. Saved data is preserved.
The Meta button remains reserved for the headset system.

## Importing a ROM or BIN

On a new installation, use the file picker. Extract ZIP files first.
If the headset has no file picker, place your own ROM at `Download/Starfox.sfc`
or `Download/Starfox.smc` and choose **Import ROM or BIN from Downloads**.
The existing Android permission dialog grants access for this fallback.
Alternatively, use `Download/Starfox-Assets.BIN`.

The APK contains neither a ROM nor a BIN generated from a ROM.
App data is stored at:

```text
/sdcard/Android/data/com.starfox.enhanced.quest/files/
```

## Windows build

Requirements: Git, Python 3 on PATH, JDK 17 or 21, Android SDK with platform 35,
Build Tools 34.0.0, CMake 3.22.1, and NDK 28.2.13676358.
The Gradle wrapper and C++ dependencies are downloaded during the first build.

```powershell
.\tools\build_quest.ps1 -JdkRoot 'C:\path\to\jdk' `
    -SdkRoot 'C:\path\to\android-sdk' -BuildRoot 'C:\a\starfox-vr-build'
```

Output: `platform/quest/build/outputs/apk/debug/quest-debug.apk`.
The debug APK is signed, and the native game code is optimized.
Keep the same local Android debug signing key for future updates.

```powershell
adb devices -l
adb -s <QUEST-SERIAL-NUMBER> install -r platform/quest/build/outputs/apk/debug/quest-debug.apk
adb -s <QUEST-SERIAL-NUMBER> shell am start -W -n com.starfox.enhanced.quest/.QuestActivity
```

Android will reject the update if the installed version was signed with a
different key. Back up app data before uninstalling.

## Validation

The new CTest `starfox_runtime_import_tests` checks BIN import, corrupted and
incompatible BIN files, and invalid ROM inputs. With your own USA Rev 2 ROM,
the test also checks copier headers and Original/EX patch parity, and writes
a BIN file:

```text
starfox_runtime_import_tests SOURCE_ROOT USA_REV2_ROM OUTPUT_BIN
```

`tests/QuestNativeImportTest.java` checks the same key import paths using the
APK's actual ARM64 libraries through Android's `app_process`.
Both the test DEX and the installed APK must be on `CLASSPATH` so that SDL can
find its Java classes. The first local attempt omitted the APK from the
classpath and aborted; all import checks passed with the complete classpath.
`tools/check_quest_package.py APK --source-root .` checks the ABI, libraries,
artwork, and exclusion of private ROM, BIN, and signing key files.

A successful build does not establish frame rate, complete level rendering,
or VR comfort. These require gameplay tests while wearing the headset.
The original project credits and third-party notices still apply.

## Local device verification on October 5, 2026

- The optimized ARM64 debug APK was built successfully and installed on the
  connected Quest 3. Current button update: `0.0.8-quest3-local.2`.
- APK SHA-256: `1FFF7A95134FEF42B312933F07CCCC96B08F3220969D2E587D66CC592B0BA943`.
- The APK signature, ARM64 libraries, and 37 embedded background resources were
  verified. No ROM, BIN, or signing keys are included in the package.
- The supplied USA Rev 2 ROM was validated (`CRC32 8fc4e6d0`).
  The host and Quest produce byte-identical BIN files. The device test confirms
  ROM import, copier header support, BIN reimport, and preservation of the
  output when an invalid ROM is supplied.
- Java tests for import copying and session management, and all four package
  tests passed. The embedded VR shaders match their sources.
- OpenXR runtime `Oculus`, Vulkan GPU `Adreno 740`, and both eyes at
  `1680 x 1760` with three swapchain images each initialized successfully.
- The device capture shows the options menu for both eyes. The system log
  reports about 72 FPS at 72 Hz with no stale frames there. This measurement
  applies to the menu; it is not a gameplay or level benchmark.
- During the subsequent test while wearing the headset, the user confirmed
  that the menu and game worked. This does not cover a full playthrough of all
  levels or extended performance and comfort testing.
- The subsequent button update was checked directly on Quest using
  `starfox_vr_face_input_check`: Original and EX, all four control types,
  A/fire, Y/brake, button press/release edges, simultaneous inputs, and saved
  swap options. Vertical inversion for types C/D is preserved.

The local APK is also available at `dist/Quest3/StarFoxEnhanced-Quest3.apk`.
Build and device logs, along with the menu capture, are stored in
`build/verification/`. Private ROM/BIN files are stored separately in
`build/private/` and excluded from Git.
