# Effect categories and ray-tracing quality

Color / Style, Materials, Distortion / Motion, and Special FX are independent
choices. 3D Materials retains its ray-tracing/reflection requirement. 2D surface
materials remain the existing Ground Material controls. Bloom keeps its existing
separate control rather than appearing twice.

Hologram now belongs to Special FX. Kaleidoscope, Prism Split, Pixel Sort,
Shatter, Melt, Ripple Warp, Barrel Warp, Venetian, Checker Fold, Twist,
Ring Ripple and Shard Split belong to Distortion. 3D Trails and Long Exposure
remain in Distortion / Motion. Legacy settings migrate the old choices to their
new selectors without renumbering serialized effect IDs.

New animated screen-space effects:

- Energy Shield: hex lattice, edge glow and travelling recharge band.
- Arc Lightning: moving branching electrical arcs.
- Hyperspace: radial travelling light streaks.
- Dissolve/Rebuild: cycling cellular breakup with glowing borders.
- Radar Sweep: travelling scan band and edge illumination.
- Frost Growth: spreading crystalline branches.
- Heat Wake: animated refractive distortion.
- Gravity Lens: animated radial lens distortion.

The first six are Special FX; the last two are Distortions. These are visual
post-processes, not new collision meshes or simulation objects. Each targets
its chosen model/world layers; HUD/portraits and alpha are protected. CPU and
GPU implementations share the procedural functions. Distortion uses filtered
sampling to avoid hard pixel stepping. Disabled extra categories submit no
extra effects passes.

Ray Tracing now cycles OFF / LOW / MEDIUM / HIGH. Low uses four shadow samples,
Medium retains the original eight, and High uses sixteen. All use the same
angular light size and full receiver resolution. More samples refine penumbrae;
they do not increase darkness or change scene geometry. Reflective Surfaces
keeps its independent quality selector. Old RAY_TRACING ON settings default
to Medium. Quality is persisted in config and save states. DXR, portable
compute, Vulkan hardware ray queries and Metal hardware paths use the quality
count; software Enhanced Shadows retains its prior setting and default samples.

Desktop DXR and portable Vulkan shadow/reference tests cover all three levels.
Metal and Linux hardware runtime validation requires their respective devices.
