# PS5 presentation artwork

The launcher icon now uses the owner's supplied `assets/icons/source.png`,
exported as opaque RGB by `tools/generate_app_icons.py`. `make_artwork.py`
preserves that choice when regenerating artwork. The two backgrounds remain
real in-game frames captured by `capture_frames.sh` and converted by
`make_artwork.py`.

| File | Format | Use |
| --- | --- | --- |
| `icon0.png` | 512x512, 8-bit RGB PNG (colour type 2) | launcher tile |
| `pic0.dds` | 3840x2160 BC7_UNORM, DX10 DDS, 1 mip, straight alpha, 8,294,548 bytes | selected-app background |
| `pic1.dds` | same profile | launch background |
| `pic0.png`, `pic1.png` | 3840x2160 RGB PNG | editable sources of the DDS files |
| `captures/*.png` | 1600x896 RGB PNG, about 150 KB in total | the raw in-game frames |

These files match the reference homebrew templates in `build/console-sdk` (`PS5_Vulkan`, `ProsperoEden`). The 148-byte DDS header is copied byte for byte from the hardware-validated `PS5_Vulkan/sce_sys/pic0.dds`, and the script checks it against that file when it is present. The BC7 payload comes from the mode-6 encoder in the script, with a round-trip PSNR of 51.5 dB (pic0) and 48.4 dB (pic1). `build/console-sdk/PS5_Vulkan/tools/validate-assets.sh platform/ps5/sce_sys` passes. `platform/ps5/package.sh` copies only `icon0.png`, `pic0.dds` and `pic1.dds`, so the captures and scripts are not packaged.

## Provenance

The two backgrounds come from the Linux x64 release (`starfox_pc` sha256 `b7f2d1540a23...43c6`, built 2026-09-14). That release reads `Starfox-Assets.BIN`, which was built from the user's own cartridge. The release was run from a copy of its folder, so the user's settings and saves were never touched. The icon was replaced with the owner's supplied artwork on 2026-11-01.

Capture settings:

- Original experience.
- 16:9 display mode at the maximum internal scale of 4x. This gives a native frame of **1600x896**; the game cannot render larger.
- Anti-aliasing Heavy, lighting and software shadows on.
- Bloom off. No glow is added anywhere.
- HUD and FPS counter hidden, hidden window, god mode, no MSU-1 or audio.

Frames are numbered by the runtime's `STARFOX_CAPTURE_DIR` frame counter.

| Output | Capture | Scene |
| --- | --- | --- |
| `pic0` | `corneria_start_f001694.png`: `LEVEL1_1` from a cold start, frame 1694, no input | The start of Corneria, after the carrier launch: Fox's Arwing in chase view with two wingmen in formation, over the city blocks and the mountain backdrop. |
| `pic1` | `intro_carrier_f002076.png`: `INTROMAP` (attract intro), frame 2076 | The attack carrier above planet Corneria. |
| `icon0` | `assets/icons/source.png` | Owner-supplied shared application icon. |

Several thousand candidate frames were reviewed before these were chosen. They covered the carrier launch, the Corneria opening, arches, the boss, the asteroid field, the Space Armada, the Venom orbit and base, the attract intro and the title screen.

## Processing (`make_artwork.py`)

- **pic0**: centre-cropped from 1600x896 to an exact 16:9 frame (1593x896, losing 3-4 px per side), then resampled with Lanczos to 3840x2160 (2.41x).
- **pic1**: the frame is wrapped 100 px down and 170 px right before the same crop and resample. This has two effects:
  - The carrier and planet move right, away from the left side, where the PS5 Shell draws the title name, Play button and gradient.
  - The carrier's antenna, which touched the top edge, gets some headroom.

  Only empty starfield crosses the wrap seams. The script checks this and refuses to run if any part of the subject would wrap.
- **icon0**:
  - The owner's shared 512×512 artwork is flattened onto black for PS5's
    opaque RGB format. The entire image is preserved without a crop or redraw.
- No colour grading, sharpening, glow or synthetic elements are added.

## Regenerating

```bash
# 1. Optional: re-capture the frames from a release folder. The folder is
#    copied to a temporary directory first and never modified. Takes about 1 min.
platform/ps5/sce_sys/capture_frames.sh /path/to/StarFoxEnhanced-linux-x64
# 2. Build icon0.png, pic0/pic1 .png and .dds. Takes about 20 s.
python3 platform/ps5/sce_sys/make_artwork.py
```

Re-capturing with the same release reproduces the stored captures byte for byte. A different runtime build may render differently. In that case, review the new frames before committing them.

You need:

- bash, a graphical session (the runtime opens a hidden SDL window) and a working GPU driver for the capture step.
- Python 3 with numpy, scipy and Pillow for both steps.
