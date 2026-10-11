# 3D asteroid models

The model files in this directory are copyright (c) 2026 CarltonCracker and
released under the MIT License (see `LICENSE`). They contain no cartridge
image data; colours come from the player's ROM at runtime.

Low-poly stand-ins for the cartridge's asteroid sprites, drawn when
3D OPTIONS > 3D ASTEROIDS is a SUPER FX mode. Each file is the `native/`
export of one model set made for this project (the matching high-detail
`ue5/` meshes and textures are kept outside the repository).

| File | Replaces (texel FNV-1a) | Retail use |
|---|---|---|
| `grey.json` | `$401b` grey rock (`69637efd`) | ASTEROID1/3, CLASTEROID with ASTEROID_C |
| `orange.json` | `$4003` hot rock (`a3b45ac9`) | BREAK_METEOR_C breakable rocks |
| `face.json` | `$401f` face rock (`496c9f82`) | ASTEROID2/4 with ASTEROID2_C |
| `crater.json` | `$6653` large rock (`21683fcc`) | BIG_METEOR (a textured quad) |

The same texels appear in Original and Star Fox EX, so models are matched by
texel content rather than ROM address.

`palette_rgb` lists the sprite colours each model was measured from.
`tools/generate_asteroid_models.py` maps them to the Super FX palette slots
the sprite's texels use (the grey ramp is slots 9-14; the reds and yellows
of the hot rock and the face's eyes are slots 1-4) and writes
`src/render/generated/asteroid_models_data.hpp`. Run it after editing a
model; `starfox_asteroid_models_generated` fails while the header is stale.

`grey.json` shares `orange.json`'s geometry: it was built from a recoloured
orange sprite, not from the real `$401b` grey texture, so its surface detail
does not follow the grey sprite exactly.
