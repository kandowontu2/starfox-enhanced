# Star Fox Enhanced 0.0.8.8.3

Changes since 0.0.8.8.2. Supply your own supported retail ROM; no ROM or
private game data is included. Nintendo 3DS remains paused and is not packaged.

## Smaller downloads and new icons

- Converted enhanced backgrounds to lossless PNG: the embedded artwork drops
  from about 179 MB to 61 MB with the same decoded pixels and dimensions.
- Moved Windows x64 DLSS and DLSS 4.5 runtimes to one optional separate
  **DLSS add-on** download. Extract its contents beside `starfox_pc.exe`, so
  the `dlss` folder sits next to the game. Both DLSS menu options use it;
  the base game works without the add-on. Compatible NVIDIA hardware is still
  required. Existing installed optional runtime files survive game updates.
- Added the new Star Fox Enhanced application icon across desktop, mobile
  and supported console packages.
- Updated Android's embedded-artwork verification for lossless PNG resources,
  and preserved the older Steam Frame toolchain's bit-cast compatibility.

## MODERN renderer

Includes the merged MODERN renderer update by CarltonCracker (PR #64):

- **DYNAMIC LIGHTING** adds coloured weapon/explosion/bomb lights and engine
  glow. Ray-traced sun shadows support the landscape, models, corridors and
  Space Armada boss room. Dynamic lighting is optional and off by default.
- Added optional **HIGH POLY ARWINGS**, including broken-wing variants, without
  changing simulation collision shapes or timing.
- Added MODERN output for SBS, over-under, interlaced, anaglyph and cross-view
  stereo. Leia SR continues to use its existing dedicated presentation path.
- Added MODERN build support for Android and PS5, plus a 3D Space Armada boss
  room, corridor/background accuracy fixes, 2D filters and level-select audio
  corrections. Android/PS5 device acceptance remains hardware-dependent.
- Renamed the renderer **MODERN - VERY FAST** and marked settings it does not
  implement as **GPU ONLY**, rather than suggesting those settings are active.

## Compatibility fixes

- #32: prevented retained exterior cruisers from drawing through Space
  Armada's final corridor/boss room across flat and shared stereo/VR paths.
  Kept the source object-removal correction and preserved outdoor ships.
- #49: added automatic host cartridge composition for Windows AMD/Radeon
  Vulkan while keeping GPU geometry/effects enabled. Live graphics-setting
  changes retire prior GPU work once before reconfiguration. This mitigates
  the reported driver-loss path; confirmation on the affected Radeon hardware
  is still needed, and this is not a claim that every AMD hang is resolved.
- Kept enhanced ground and enhanced sky/backgrounds independent of ray
  tracing. Reflections and ray-traced shadows retain their own requirements.

## Updates and installation

- Preserved the full **0.0.8.8.3** version in the in-game updater, including
  upgrades from 0.0.8.8.2. The updater selects the game package, never the
  optional DLSS add-on. Automatic installation remains for portable Windows
  x64/x86 and Linux x64; other platforms keep their existing update methods.
- Increased native platform build revisions for install-over updates.
  Android and Quest retain the existing release-signing certificate.
- Settings, saves and compatible locally generated game data remain usable.
  Source/build directories do not support in-place auto-update: use an
  extracted release installation.

Platform packages are published only after the release build gates, Windows
Defender package scans and checksum inventory pass. Build success is not a
claim of hardware acceptance for every console, VR headset or graphics driver.
