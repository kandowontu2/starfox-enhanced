#include "starfox/render/explosion_models.hpp"
#include "starfox/render/asteroid_models.hpp"
#include "starfox/render/software_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace starfox::render {
namespace {

// render::asteroid_texture_hash of each frame; Original and EX share the
// texels (starfox_texture_dump's textures.csv lists them).
constexpr std::array<std::uint32_t, 19> kKnown{{
    // Large explosion and fire, 64x64 (largeexpl1-8).
    0x8859e220U, 0xf1ff01c5U, 0xd60a4b4bU, 0xb2b3161bU, 0xf9ae3034U, 0xfd14986fU, 0xde63a702U, 0x3b9444b5U,
    // Smoke, 32x32 (smoke1-6).
    0x952b4461U, 0x25e7f383U, 0x330b3db1U, 0xff792c21U, 0xff29ab93U, 0x039c8fffU,
    // Fireball, 32x32 (fireball1-3).
    0xd1734dc4U, 0xbe428f69U, 0x936817f3U,
    // Small explosion, 32x32 (expl9 and expl10: EXPLOSION, EXPLODE_C).
    0x469520f2U, 0xb9807400U,
}};

// A cheap fingerprint of a texture, for remembering recognition per draw.
std::array<std::uint8_t, 16> fingerprint(const assets::TextureImage& texture) {
    std::array<std::uint8_t, 16> sample{};
    for (std::size_t index = 0; index < sample.size() && !texture.texels.empty(); ++index) {
        sample[index] = texture.texels[index * texture.texels.size() / sample.size()];
    }
    return sample;
}

std::optional<ExplosionSpriteFrame> frame_for(const assets::TextureImage& texture) {
    if (texture.u_mask != texture.v_mask || (texture.u_mask != 31U && texture.u_mask != 63U)) return std::nullopt;
    const auto known = std::find(kKnown.begin(), kKnown.end(), asteroid_texture_hash(texture));
    if (known == kKnown.end()) return std::nullopt;
    const auto index = static_cast<std::size_t>(known - kKnown.begin());
    // kKnown's order: 8 large, 6 smoke, 3 fireball, 2 small.
    ExplosionSpriteFrame frame;
    if (index < 8U) frame = {ExplosionSpriteKind::large, static_cast<std::uint8_t>(index), 8};
    else if (index < 14U) frame = {ExplosionSpriteKind::smoke, static_cast<std::uint8_t>(index - 8U), 6};
    else if (index < 17U) frame = {ExplosionSpriteKind::fireball, static_cast<std::uint8_t>(index - 14U), 3};
    else frame = {ExplosionSpriteKind::small, static_cast<std::uint8_t>(index - 17U), 2};
    return frame;
}

// The frame lookup, remembered per draw by texture address and fingerprint.
std::optional<ExplosionSpriteFrame> match_frame(const assets::TextureImage& texture) {
    struct Match {const assets::TextureImage* texture{}; std::uint16_t descriptor{}; std::array<std::uint8_t, 16> sample{}; std::optional<ExplosionSpriteFrame> frame;};
    thread_local std::vector<Match> matches;
    const auto sample = fingerprint(texture);
    for (const auto& match : matches) {
        if (match.texture == &texture && match.descriptor == texture.descriptor && match.sample == sample) return match.frame;
    }
    if (matches.size() >= 64U) matches.clear();
    matches.push_back({&texture, texture.descriptor, sample, frame_for(texture)});
    return matches.back().frame;
}

} // namespace

std::optional<ExplosionSpriteFrame> explosion_frame_for_texture(const assets::TextureImage& texture) {
    return frame_for(texture);
}

std::optional<ExplosionSpriteFrame> explosion_sprite_frame(const assets::Shape& source, const RenderPose& pose) {
    if (pose.explosion_progress != 0U) return std::nullopt;
    const auto [texture, half_size] = whole_object_sprite(source, pose);
    if (texture == nullptr) return std::nullopt;
    auto frame = match_frame(*texture);
    if (frame) frame->half_size = half_size;
    return frame;
}

}
