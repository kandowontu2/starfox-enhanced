#pragma once
#include "starfox/assets/shape.hpp"
#include <cstdint>

namespace starfox::render {
struct RenderPose;

// 3D EXPLOSIONS: optional layered stand-ins for the fire, smoke and explosion
// sprites, which read as flat cards in stereo. They are the large explosion
// and fire frames (largeexpl1-8: EXPLOSION*, FIRE, FIREBREATH, BONFIRE), the
// smoke frames (smoke1-6: SMOKE, LSMOKE, FOLSMOKE), the fireball frames and
// the small explosion frame.
//
// Each model is a small cluster of camera-facing quads textured with the
// sprite frame itself (so replacement art in textures\ applies to every
// layer): the back quad is the sprite at full size in the sprite's own plane,
// so from the front the silhouette is unchanged, and smaller copies, each
// turned about the view axis, stand nearer the camera like the swell of a
// fireball or a smoke puff. Every frame of an animation gets its own model
// with the same mesh layout, so the cartridge's animation still drives it.
//
// When `pose` draws `source` as one of those sprite frames, returns the model
// and rewrites `pose` to draw it at the sprite's size, facing the camera.
// Otherwise returns nullptr and leaves `pose` untouched.
const assets::Shape* substitute_explosion_model(const assets::Shape& source, RenderPose& pose, bool enabled);

// Exposed for tests: the model for one sprite frame, or nullptr when the
// texture is not a known fire, smoke or explosion frame.
[[nodiscard]] const assets::Shape* explosion_model_for_texture(const assets::TextureImage& texture);

// Exposed for tests: how many layers each model has, the size of each layer
// relative to the sprite and its distance toward the camera relative to the
// sprite's half width.
inline constexpr std::size_t explosion_layer_count = 3;
}
