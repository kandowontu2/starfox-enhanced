# Side-by-side 3D on a PC-connected TV

Use **Renderer: GPU**, then **Options → Stereoscopic 3D → Output → Half SBS**. Set the TV's
3D mode to side-by-side (left eye first). Half SBS fits both views into the
normal video frame; the TV expands each half. Full SBS retains the full width
of both eyes and needs a player/display mode that accepts double-width input.

Models, particles and the ground grid are projected separately for each eye
using parallel, off-axis cameras—the same stereo projection principle used by
the VR build. This is not two copies of the same finished frame. Near objects
appear in front of the convergence plane and distant objects behind it. HUD
and ordinary raster menu overlays stay at screen depth. Original flat backdrop
art remains a flat layer; it is not converted into a 3D landscape.

The TV rig defaults to 16 game units of eye separation and a convergence distance
of 1024. Both are adjustable in the stereo submenu (separation 1–512;
convergence 16–65535). Start near the defaults and change gradually for viewing
comfort. Geometry, shadows and reflections share those settings. Head tracking
is not used, and these settings do not change Quest/PCVR projection.

Use the native PC GPU backend (D3D12 or Vulkan). Software and legacy D3D11
presentation do not currently provide this stereo path. DLSS/FSR are disabled
while SBS is active. A TV test is still needed to assess viewing comfort and
the TV's eye-order/packing settings; an ordinary monitor cannot validate that.

## Additional outputs

- Crossview: full-width SBS with the right eye on the left and the left eye on
  the right, for cross-eyed viewing on an ordinary monitor. Cameras and effects
  remain associated with their physical eyes; only final placement is reversed.
- Half/full top-bottom: left eye above right eye. Full doubles output height.
- Interlaced L/R: left eye on even physical output rows, right on odd rows.
  Interlaced R/L reverses that assignment. Packing happens at the final window
  pixel size, after render upscaling, with letterbox parity retained. Use a
  fullscreen window and the display's native resolution; external scaling or
  desktop compositor scaling can still destroy the row pattern. Integer
  scaling is deliberately bypassed for this output.
- Anaglyph R/C: left red lens, right cyan lens. This is full-color channel
  selection (left red, right green/blue), not a color-corrected Dubois matrix.
  Eye layers and bloom are composed before channel selection.

`STEREO_OUTPUT` values: 0 off, 1 half SBS, 2 full SBS, 3 half top-bottom,
4 full top-bottom, 5 interlaced L/R, 6 interlaced R/L, 7 red/cyan anaglyph,
8 Crossview. Existing settings IDs are unchanged.
These modes do not provide native LeiaSR calibration or head tracking.

### Stereo fork review (October 2)

Reviewed [agrofubris/starfox-enhanced-stereo](https://github.com/agrofubris/starfox-enhanced-stereo)
at `b4c1e0fed544aa2143dede9016f21c37d46117e6`. Crossview and the single-player
raw-Deck controller preference were missing and have been integrated. Steam's
virtual controller still takes precedence, with physical duplicates suppressed;
explicit multiplayer order is unchanged. Top/bottom, interlaced outputs,
separation/convergence settings, infinity sky placement and screen-depth UI
already exist in this project and were retained.

The fork's optional direct SR weaver references `SRInterfaceDX12`,
`CreateSRInterfaceDX12`, a CMake target and delay-load helper not supplied by the
linked [bo3b/SR-lib](https://github.com/bo3b/SR-lib) checkout reviewed here. Its
source-only default is therefore a Full SBS fallback, not a verified native
Leia implementation. That incomplete bridge was not imported or advertised as
working. Our existing calibrated DisplayXR D3D12/Vulkan path remains separate;
physical Leia display testing is still required.

## Higher render resolution

The render-upscale menu now offers up to **6x** on desktop. For an explicit
high-memory override, `RENDER_SCALE` in `pregame.cfg` is zero-based: `5` selects
6x, `6` selects 7x, through `9` for 10x. Keep a backup of your configuration
before testing the higher values. Pixel and memory costs grow quadratically;
stereo also renders two eyes. Existing size/resource limits still apply, so
10x is not guaranteed to work with every aspect ratio, effect or GPU.
The iOS 2x safety cap is unchanged.

Original BG2 landscape artwork and enhanced skies are placed at infinity
disparity instead of screen depth. This does not create geometry within the
background image. Other original background paths and stage transitions still
need validation.

## Reticle depth

The stereo submenu's **Reticle Depth** controls the original game's four-piece
cockpit reticle independently of HUD text and meters. **Screen** (config value
0, the default) preserves the original placement. Other values are camera-space
world distances (16–65535); matching convergence gives zero disparity, while
greater distances place it behind the screen. The setting is saved as
`STEREO_CROSSHAIR_DEPTH`. This is a static aiming-plane setting, not automatic
target acquisition or dynamic occlusion. EX's model-based reticle already uses
the model projection and is not translated a second time by this control.

## Matching the tester's depth preferences

Open **Options → Stereoscopic 3D**. Increase separation gradually to increase
the near-to-far depth range. Adjust convergence separately to position that
range relative to the screen; reducing convergence moves a given object
farther behind the screen and reduces its pop-out. Reticle Depth controls the
cockpit aiming plane without shifting the ordinary HUD. There is no single
comfortable preset for every monitor size, viewing distance, or converter.

The sky's infinity disparity follows the same separation/convergence rig.
It is not a fixed screen-space offset independent of those controls. Avoid
also applying a large depth shift in an external SBS converter until you have
calibrated the game's controls, since those shifts combine.

Native Leia SR is a separate Windows D3D12/Vulkan option using a compatible,
separately installed DisplayXR runtime and display. It is not these SBS packing
modes. Its actual calibrated GPU-eye path is implemented, but physical display
composition/reconnect acceptance and several enhancement parity items remain
open; see `STEREO-DISPLAY-UPGRADE.md`. Dynamic crosshair targeting is also not
implemented; the available depth is static.

## Current performance work

Compatible opaque models now paint into the existing GPU working image without
copying untouched pixels for every model. Both eyes still have independent
projections and images; quality and completion fences are unchanged. The final
D3D12/Vulkan image matrices include moving-model transitions, texture holes,
black ink, enhanced scenery, ray reflections and failed-eye recovery.
Three interleaved 480-frame paired timing runs show modest, backend-dependent
gains, not a completed SBS lag fix. Full timing and validation scope are recorded
in `STEREO-DISPLAY-UPGRADE.md`; no universal FPS guarantee is implied.
