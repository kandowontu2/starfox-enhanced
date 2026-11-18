#include "starfox/render/explosion_models.hpp"
#include "starfox/render/asteroid_models.hpp"
#include "starfox/render/software_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <string>
#include <vector>

namespace starfox::render {
namespace {

// Synthetic header addresses sit outside the cartridge window, below the
// projectile models ($fe0000) and the asteroid models ($ff0000).
constexpr std::uint32_t kModelAddressBase = 0xfd0000U;
// Model units per sprite texel, as the projectile models use.
constexpr double kTexelUnits = 16.0;

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

// Each layer: size relative to the sprite, distance toward the camera in
// sprite half widths, and turn about the view axis. Turns are irregular so
// no two layers repeat the art's features in line.
struct Layer {double size, toward, turn;};
constexpr std::array<Layer, explosion_layer_count> kLayers{{
    {1.00, 0.00, 0.0},
    {0.72, 0.30, 137.0},
    {0.45, 0.55, 263.0},
}};

assets::Shape build(const assets::TextureImage& texture, std::uint32_t address) {
    assets::Shape shape;
    // The frame's own texels, mapped corner to corner. Corners go round
    // top-left, bottom-left, bottom-right, top-right so the quad's winding
    // faces the camera with the retail inward normal (+Z, away from it).
    auto image = texture;
    const auto right = static_cast<std::uint8_t>(texture.u_mask), bottom = static_cast<std::uint8_t>(texture.v_mask);
    image.coordinates = {{{0, 0}, {0, bottom}, {right, bottom}, {right, 0}}};
    shape.colour_words = {texture.descriptor};
    shape.textures = {std::move(image)};
    const double half = (static_cast<double>(texture.u_mask) + 1.0) / 2.0 * kTexelUnits;
    std::int32_t extent = 1;
    for (std::size_t layer = 0; layer < kLayers.size(); ++layer) {
        const auto& [size, toward, turn] = kLayers[layer];
        const double a = turn * std::numbers::pi / 180.0, c = std::cos(a), s = std::sin(a);
        const double r = half * size, z = -toward * half;
        const std::array<std::array<double, 2>, 4> corners{{{-r, -r}, {-r, r}, {r, r}, {r, -r}}};
        assets::Face face;
        for (const auto& [x, y] : corners) {
            const auto vx = static_cast<std::int32_t>(std::lround(x * c - y * s));
            const auto vy = static_cast<std::int32_t>(std::lround(x * s + y * c));
            const auto vz = static_cast<std::int32_t>(std::lround(z));
            extent = std::max({extent, std::abs(vx), std::abs(vy), std::abs(vz)});
            face.vertex_indices.push_back(static_cast<std::uint8_t>(shape.vertices.size()));
            shape.vertices.push_back({vx, vy, vz});
        }
        shape.visibilities.push_back({face.vertex_indices[0], face.vertex_indices[1], face.vertex_indices[2]});
        face.visibility_index = static_cast<std::int16_t>(shape.visibilities.size() - 1U);
        face.colour_id = 0;
        face.normal = {0, 0, 127};
        shape.faces.push_back(std::move(face));
    }
    shape.name = "EXPLOSION_MODEL_" + std::to_string(address - kModelAddressBase);
    shape.header.address = address;
    shape.header.size = static_cast<std::int16_t>(extent);
    shape.header.radius = static_cast<std::uint16_t>(extent);
    shape.header.x_max = shape.header.y_max = shape.header.z_max = static_cast<std::int16_t>(extent);
    return shape;
}

const assets::Shape* model_for(const assets::TextureImage& texture) {
    if (texture.u_mask != texture.v_mask || (texture.u_mask != 31U && texture.u_mask != 63U)) return nullptr;
    const auto hash = asteroid_texture_hash(texture);
    const auto known = std::find(kKnown.begin(), kKnown.end(), hash);
    if (known == kKnown.end()) return nullptr;
    static std::mutex mutex;
    // One model per frame and descriptor: the descriptor picks the texel
    // plane and the replacement art, so frames shared at two descriptors
    // keep both.
    static std::map<std::pair<std::uint32_t, std::uint16_t>, std::unique_ptr<assets::Shape>> models;
    const std::lock_guard lock{mutex};
    auto& model = models[{hash, texture.descriptor}];
    if (!model) {
        const auto address = kModelAddressBase + static_cast<std::uint32_t>(models.size() - 1U) * 0x100U;
        model = std::make_unique<assets::Shape>(build(texture, address));
    }
    return model.get();
}

// Texture recognition per draw, remembered by texture address with a cheap
// fingerprint, as for the asteroids and projectiles.
struct TextureMatch {
    const assets::TextureImage* texture{};
    std::uint16_t descriptor{};
    std::array<std::uint8_t, 16> sample{};
    const assets::Shape* model{};
};

std::array<std::uint8_t, 16> fingerprint(const assets::TextureImage& texture) {
    std::array<std::uint8_t, 16> sample{};
    for (std::size_t index = 0; index < sample.size() && !texture.texels.empty(); ++index) {
        sample[index] = texture.texels[index * texture.texels.size() / sample.size()];
    }
    return sample;
}

const assets::Shape* match_texture(const assets::TextureImage& texture) {
    thread_local std::vector<TextureMatch> matches;
    const auto sample = fingerprint(texture);
    for (const auto& match : matches) {
        if (match.texture == &texture && match.descriptor == texture.descriptor && match.sample == sample) return match.model;
    }
    if (matches.size() >= 64U) matches.clear();
    matches.push_back({&texture, texture.descriptor, sample, model_for(texture)});
    return matches.back().model;
}

} // namespace

const assets::Shape* explosion_model_for_texture(const assets::TextureImage& texture) {
    return model_for(texture);
}

const assets::Shape* substitute_explosion_model(const assets::Shape& source, RenderPose& pose, bool enabled) {
    if (!enabled || pose.explosion_progress != 0U) return nullptr;
    const auto [texture, half_size] = whole_object_sprite(source, pose);
    if (texture == nullptr) return nullptr;
    const auto* model = match_texture(*texture);
    if (model == nullptr) return nullptr;
    pose.simple_scaled_sprite = false;
    // The sprite's world square spans the whole texture.
    pose.scale = half_size / (static_cast<double>(texture->u_mask + 1U) / 2.0 * kTexelUnits);
    // Face the camera, as the sprite did.
    pose.pitch = pose.yaw = pose.roll = 0.0;
    pose.rotation_matrix = {32767, 0, 0, 0, 32767, 0, 0, 0, 32767};
    pose.use_rotation_matrix = true;
    return model;
}

}
