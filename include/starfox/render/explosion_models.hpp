#pragma once
#include "starfox/assets/shape.hpp"
#include <cstdint>
#include <optional>

namespace starfox::render {
struct RenderPose;

// 3D EXPLOSIONS: the fire, smoke and explosion sprites, which read as flat
// cards in stereo, become particles in MODERN (vr::ExplosionParticles). They
// are the large explosion and fire frames (largeexpl1-8: EXPLOSION*, FIRE,
// FIREBREATH, BONFIRE), the smoke frames (smoke1-6: SMOKE, LSMOKE, FOLSMOKE),
// the fireball frames and the small explosion frame, recognised by their
// texels (asteroid_texture_hash).

// Which animation a fire, smoke or explosion sprite frame belongs to, and
// its frame within that animation.
enum class ExplosionSpriteKind : std::uint8_t {
    large,    // largeexpl1-8: EXPLOSION*, FIRE, FIREBREATH, BONFIRE (64x64)
    smoke,    // smoke1-6: SMOKE, LSMOKE, FOLSMOKE (32x32)
    fireball, // fireball1-3 (32x32)
    small,    // expl9, expl10: EXPLOSION, EXPLODE_C (32x32)
};
struct ExplosionSpriteFrame {
    ExplosionSpriteKind kind{};
    std::uint8_t frame{}, frames{};
    // The sprite's half width in its pose's units (source units).
    double half_size{};
};
// When `pose` draws `source` as one of those sprite frames, which one.
[[nodiscard]] std::optional<ExplosionSpriteFrame> explosion_sprite_frame(const assets::Shape& source, const RenderPose& pose);
// Exposed for tests: the frame for one texture, without the size.
[[nodiscard]] std::optional<ExplosionSpriteFrame> explosion_frame_for_texture(const assets::TextureImage& texture);
}
