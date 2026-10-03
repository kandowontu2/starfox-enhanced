# Cockpit C assets

These are the rear cabin pieces for the pilot-view cockpit, made for this
project. There are thirteen modules (floor, side walls, window rims and
insets, a rear bulkhead in three boxes, and a roll hoop in three beams) plus
flat materials. `geometry.json` has the editable pivots, triangles and
per-face materials, the OBJ/MTL files are exports of the same thing, and
`tools/generate_cockpit_assets.py` compiles them into `src/vr/cockpit_assets.inc`.

Nothing from the cartridge is in here, no mesh, texture or pixel data. The front
of the cockpit (the COCKPIT geometry) and the live player ship are decoded from
your own asset bundle at runtime. Original and EX have different descriptor
addresses and palettes but the same front positions and face ranges, so a
geometry signature and exact range checks refuse any bundle that doesn't match
before the materials are applied.

At runtime the upper shell is widened by 70% and lowered by 16 cm, blending out
to the unchanged instrument face, for both the front and rear pieces. The
exports keep the unmodified geometry.
