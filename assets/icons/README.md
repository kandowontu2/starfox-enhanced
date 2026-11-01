# Application artwork

`source.png` is the owner's supplied, unchanged 512×512 artwork. All launcher
icons derive from this image, without redrawing or cropping. Regenerate with
`python tools/generate_app_icons.py` (Pillow required for regeneration only).
Builds copy the checked-in exports and do not need Pillow.

- Windows: multi-resolution ICO, plus an embedded SDL window icon that survives
  renderer switches; PCVR also receives the EXE icon.
- macOS: multi-resolution ICNS in the application bundle.
- iOS: opaque RGB iPhone/iPad icons, flattened on black (no pre-rounded corners).
- Android/Quest: density-specific legacy icons and a shared adaptive icon whose
  foreground fits inside the safe zone of circular and other launcher masks.
- Xbox/UWP: square/wide tiles and splash artwork, with aspect ratio preserved.
- Linux/Steam Frame: packaged PNG; the flat runtime also sets its window icon.
- Switch: opaque 256×256 JPEG; Vita: opaque 8-bit indexed PNG artwork.
- PS5: opaque 512×512 RGB `platform/ps5/sce_sys/icon0.png`.

Platform-specific format conversion (such as Vita's 256-colour palette) is
unavoidable. The 3DS performance work remains paused.
