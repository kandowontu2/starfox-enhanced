# Unreleased artwork and packaging changes

- Shared owner-supplied application icon for iOS, Android/Quest, Windows,
  macOS, Xbox/UWP, Linux, Steam Frame, Switch, Vita and PS5. No redraw or crop.
- Enhanced backgrounds ship as lossless PNGs instead of uncompressed BMPs,
  preserving all pixels, dimensions, registration and lazy upload caching.
- Windows x64 DLSS and DLSS 4.5 share one optional add-on ZIP. The base game
  does not embed the NVIDIA runtime. Extract `dlss` beside `starfox_pc.exe`.
  Game updates preserve the installed add-on; the updater never treats its
  archive as the game package. Existing published releases are unchanged.
- Enhanced ground and enhanced sky/backgrounds remain independent of ray
  tracing. Reflections and ray-traced shadows retain their own requirements.

Nintendo 3DS performance work remains paused. This is not a new release.

Additional local bug fixes:

- #32: share an exterior-cruiser visibility guard across flat Software/GPU and
  the MODERN/stereo/VR scene path. Fresh source playthroughs retain the earlier
  ATZREMOVE fix; legacy retained cruisers cannot draw through the final Armada
  corridor or boss room. Outdoor cruisers and simulation objects remain live.
- #49: automatically use host cartridge composition on Windows Radeon/Vulkan,
  retaining GPU geometry/effects and enhanced backgrounds. Retire prior GPU
  work once when live rendering settings change, not on unchanged frames or
  Preview-OFF menus. This is a compatibility mitigation based on the report's
  isolation results, not a claim that the AMD driver fault has been reproduced
  on the local NVIDIA/Intel test hardware. Radeon acceptance is still needed.
