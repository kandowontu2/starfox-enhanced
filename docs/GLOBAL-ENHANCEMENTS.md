# Global Enhancements

The main setup menu places **Global Enhancements** immediately below 3D Options.
Ray Tracing, Reflective Surfaces, Enhanced Lighting, Contrast (formerly HDR
Effect), and Chromatic Aberration live here rather than being duplicated in 3D
Options. Existing preferences retain their values and config keys.

Shadow Softness sits directly below Ray Tracing: Hard / Low / Medium / High.
It changes the angular size of the light, so softness grows with the distance
between a caster and its receiver rather than applying a fixed screen blur.
Medium retains the previous appearance. Hard uses one visibility ray; ray
tracing quality controls sampling for the other levels. It applies to software
Enhanced Shadows and GPU shadows, including both SBS eyes. The saved config
key is `SHADOW_SOFTNESS` (0–3, default 2); this control does not enable shadows
when the relevant shadow renderer is off.

Water Caustics is below Reflective Surfaces (Off / Low / Medium / High).
It lights submerged geometry and the water bed when enhanced ground is Water
or Auto selects water. Reflections are optional. Software uses a bounded ray
grid, and turning caustics Off leaves water transmission enabled. Auto on dry
ground does not collect geometry for water. GPU mode currently requires hardware ray tracing enabled. The menu shows
`NEEDS HW RT` or `RT OFF` when that GPU path cannot run. Compute-only GPU support
and native Apple/device validation remain unfinished.

Thirteen additional independent controls offer Off / Low / Medium / High:

| Control | Implementation |
| --- | --- |
| Light Shafts | Bright-pixel radial integration toward an upper-screen light source; screen-space, not volumetric geometry. |
| Anamorphic Flare | Wide horizontal streaks sourced from bright scene pixels. |
| Halation | Warm highlight diffusion. |
| Soft Focus | Spatial diffusion, not depth-of-field. |
| Radial Blur | Center-out zoom blur, not velocity-based motion blur. |
| Heat Haze | Animated bright-area refraction. |
| Sharpen | Local unsharp masking. |
| Vignette | Graduated corner shading. |
| Film Grain | Deterministic, time-stepped monochrome grain. |
| CRT Scanlines | Native-raster horizontal modulation. |
| Phosphor Mask | RGB column mask. |
| CRT Curvature | Barrel-shaped scene sampling; interface remains flat/readable. |
| Lens Ghosts | Reflected bright-pixel ghosts around the image center. |

All new controls default Off. They are fused into one optional GPU pass, with
sampling branches skipped for disabled controls. With all Off, no global pass or
CPU scratch copy is submitted. Expensive combinations still cost more: light
shafts, flares and ghosts require multiple texture samples. This is not a promise
that enabling every effect is free, especially on mobile hardware.

The shared CPU/GPU kernel preserves alpha and protects HUD/menu/portrait pixels,
including source taps, and has no frame-history feedback. Settings persist in
the normal config and save states; old settings default the new controls Off.
Actual volumetrics and velocity-based motion blur remain separate pending work.

## Surface-depth controls

Ambient Occlusion and Depth of Field are independent Off / Low / Medium / High
controls below Enhanced Lighting. AO is screen-space geometric occlusion: it
reconstructs neighboring surface positions from actual depth, evaluates them
against the receiver normal, and applies a bounded local shading term. It is not
ray-traced/off-screen occlusion and does not guess contacts from image brightness.

Depth of Field computes a blur radius from actual surface distance relative to
the player's camera-space depth (500 units when there is no player). A focus
dead band keeps the player sharp; depth-discontinuity rejection prevents nearby
objects and HUD ink bleeding into distant surfaces. Flat background artwork
without geometry metadata remains unchanged rather than receiving invented
depth. Both controls need captured surfaces and do no work when disabled.
The DOF gather uses dense, depth-rejected Gaussian samples rather than a sparse
ring, avoiding repeated silhouettes on low-resolution polygon edges.

## Gameplay-driven additions

Six further Off / Low / Medium / High controls sit above the lens controls:

- Explosion Shockwaves: expanding refractive rings triggered by destruction
  flags and object-generation identities, not a repeating screen animation.
- Weapon Lighting: nearby emissive projectiles and explosions provide additive
  point lighting to captured polygon surfaces using their depth and normals.
  These extra lights do not cast additional ray-traced shadows. Background art
  without surface metadata is not a lighting receiver.
- Exhaust Trails: short, fading blue exhaust puffs retain previous player world
  positions and are projected through the current camera, with surface-depth
  occlusion. These are lightweight particles, not newly modelled engine meshes.
- Stage Weather: world-space snow on Titania while its live ground palette is
  snowy; rain on EX's night-storm background; ash on EX's ember/volcanic
  backgrounds. Unlisted stages, menus, space and tunnels do not get generic
  weather. Density increases with quality.
- Impact Sparks / Debris: health decreases and newly destroyed object identities
  emit a short burst at the object's world position. Low/Medium/High emit 4/8/12
  particles per event, with at most 24 retained. Sparks glow; debris is opaque,
  rotating billboard flecks, not replacement object meshes. Surface depth hides
  particles behind nearer models. Gravity applies on planets, not space scenes.
- Exhaust Heat Distortion: localized refraction follows recent player positions,
  independently of Exhaust Trails. Smooth bilinear samples and a fading envelope
  avoid nearest-pixel shimmer. The ship and other nearer model surfaces occlude
  the heat; it does not tint the image or distort protected HUD pixels.

Transient histories clear on scene changes, setting changes and time rewinds.
Animation uses the source environment clock and freezes on pause. SBS eye
projection adjusts particle/ring positions and light positions independently.
There is a hard cap of 48 active projected effects (including at most six point
lights and four shockwaves), and no scene-effect pass when the list is empty.
HUD and alpha remain protected. Device validation outside the desktop build
remains separate from the portable shader generation checks.

`HDR_EFFECT` remains the legacy configuration key for the renamed Contrast
control. It is tone adjustment, not HDR monitor output.

## CRT phosphor persistence

CRT Phosphor Persistence is an independent Off / Low / Medium / High control,
separate from Phosphor Mask and model Trails/Long Exposure. It retains a short
afterglow across both world and model pixels, with green decaying more slowly
than red and blue. The green half-life is 30/60/90 milliseconds; red is half
that and blue one third. Decay uses elapsed presentation time, not frame count.

HUD/menu/portrait pixels are neither captured nor changed. Covering an old
afterimage with HUD ink clears that pixel's history. Mono and both stereo eyes
have independent histories; dimensions, scene/setting changes and time jumps
reset them. The software fallback and portable GPU path use the same decay.
Combining it with model Trails uses a separate history for each effect.

## Adaptive exposure

Adaptive Exposure is separate from Contrast, with Off / Low / Medium / High
limiting compensation to ±0.5 / ±1 / ±1.5 stops. It meters linear scene
luminance through a 64-bin log histogram, discarding the darkest and brightest
5% of eligible samples. HUD, transparent pixels and near-black letterboxes do
not drive the meter. Darkening responds faster than brightening; adaptation
uses elapsed time and freezes while paused. Scene changes restart at neutral.

The portable GPU path builds tile histograms, reduces them and retains exposure
on-device without a CPU readback. Software/legacy rendering has a matching
reference with lookup tables. The option defaults off and allocates no exposure
textures when disabled. Config key: `ADAPTIVE_EXPOSURE`, values 0–3.

## Camera response (development build)

Impact Shake, Weapon Recoil and Camera Banking are independent Off / Low /
Medium / High controls in Global Enhancements. All default Off. They rotate
the composed world rather than moving HUD elements, with a small bounded crop
to avoid exposed edges. Impact follows shield damage; recoil follows newly
observed player-owned laser volleys; banking follows the player's roll.
Pause freezes response, and scene or strength changes reset it.

Config key `CAMERA_RESPONSE` packs impact, recoil and banking into successive
two-bit fields (bits 0–1, 2–3 and 4–5). Config and save-state support are wired.
Desktop software and resident GPU gameplay have been tested. Temporal
upscaling, portrait/setup/wipe combinations, stereo and device coverage remain
unfinished; this is not yet deployed to `build/current`.

## Volumetric fog (development build)

Global Enhancements now includes Volumetric Fog: Off / Low / Medium / High,
default Off. Density and integration sample count rise together. Fog uses model
occlusion and physical ground depth, not a radial screen filter; its light is
integrated along the view ray. The HUD and comms artwork remain protected.

Config key `VOLUMETRIC_FOG` accepts 0–3 and save states retain the choice.
Older settings/states default to Off. Disabling fog releases its GPU resources.
Vulkan and Direct3D 12 reference/compositing tests pass, and actual menu-driven
activation has been captured on Direct3D 12. Upscaler/stereo compatibility,
broader scene coverage, performance and mobile validation remain unfinished.
This has not been deployed to `build/current`.
