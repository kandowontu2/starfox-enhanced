#pragma once
#include "starfox/assets/shape.hpp"
#include <cstdint>

namespace starfox::render {
struct RenderPose;

// 3D PROJECTILES: optional solid stand-ins for the round shot sprites, which
// read as flat discs in stereo. They are the top laser upgrade's twin pulse
// (PLAYERBEAM), the Nova Bomb and its pickup (NUKE, HELPBALL, SHELPBALL:
// three frames) and the enemies' plasma ball (BOUNCYBALL, LARGEPLASMA: two
// frames), the ring laser (RINGLASER: two frames) and the oval beam
// (OVALBEAM: three frames).
//
// Each model is built from the sprite frame's own texels when it is first
// seen, so it carries no game data and keeps the sprite's look. It faces the
// camera like the sprite did: from the front it is the sprite, traced
// outline and all, and it has real depth in stereo. The orbs are a sphere
// for the bright core inside a flat halo for the glow, in the sprite's
// concentric colour bands. The bomb is a low-poly faceted gem standing out
// of a rounded halo ring. The ring laser is a torus, and the oval beam is the
// orb stretched to the sprite's tilted ellipse.
// Stray arcs beside the ball (the plasma ball's red frame) become thin
// ribbons tipped through its depth. Faces are unlit solid palette slots, as
// the glowing sprites were. Every frame of an animation gets its own model
// with the same body mesh, so the cartridge's colour cycle still drives the
// animation; the arcs come and go with their frames.
//
// When `pose` draws `source` as one of those sprite frames, returns the
// model and rewrites `pose` to draw it at the sprite's size, facing the
// camera. Otherwise returns nullptr and leaves `pose` untouched.
const assets::Shape* substitute_projectile_model(const assets::Shape& source, RenderPose& pose, bool enabled);

// Exposed for tests: the model for one sprite frame, or nullptr when the
// texture is not a known projectile frame. Models live for the process.
[[nodiscard]] const assets::Shape* projectile_model_for_texture(const assets::TextureImage& texture);
}
