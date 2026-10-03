#include "starfox/render/asteroid_models.hpp"
#include "starfox/render/face_material.hpp"
#include "starfox/render/software_renderer.hpp"
#include "generated/asteroid_models_data.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace starfox::render {
namespace {

// Synthetic header addresses sit outside the cartridge window so diagnostics
// never confuse a model with a retail shape.
constexpr std::uint32_t kModelAddressBase = 0xff0000U;

// Retail SHADESTAB2 materials step a palette ramp by roughly one colour per
// two light levels and darken with each depth band, dithering the half steps
// between neighbouring colours. Build the same kind of table around each
// model colour so the rock keeps its painted light and dark regions. The
// sprites never darkened with distance, so the models fade more gently than
// retail polygons do.
assets::DiffuseShadeTables shade_tables(const asteroid_data::ModelData& data) {
    assets::DiffuseShadeTables tables{};
    const auto last_step = static_cast<int>(2 * (data.ramp_size - 1));
    for (std::size_t depth = 0; depth < tables.size(); ++depth) {
        for (std::size_t material = 0; material < data.ramp_size; ++material) {
            for (std::size_t light = 0; light < tables[depth][material].size(); ++light) {
                const auto position = static_cast<double>(material)
                    + (static_cast<double>(light) - 4.0) * 0.4
                    - 0.35 * static_cast<double>(depth);
                const auto step = std::clamp(
                    static_cast<int>(std::lround(position * 2.0)), 0, last_step);
                const auto low = data.ramp[static_cast<std::size_t>(step / 2)];
                const auto high = data.ramp[static_cast<std::size_t>((step + 1) / 2)];
                tables[depth][material][light] = static_cast<std::uint8_t>(low | (high << 4U));
            }
        }
    }
    return tables;
}

assets::Shape build_shape(const asteroid_data::ModelData& data, std::size_t index, std::size_t level) {
    const auto& mesh = data.lods[level];
    const auto shape_index = index * asteroid_data::lod_count + level;
    assets::Shape shape;
    shape.name = "ASTEROID_MODEL_" + std::to_string(index) + "_LOD" + std::to_string(level);
    shape.header.address = kModelAddressBase + static_cast<std::uint32_t>(shape_index);
    shape.header.size = static_cast<std::int16_t>(asteroid_data::unit);
    shape.header.radius = static_cast<std::uint16_t>(asteroid_data::unit);
    for (const auto& vertex : mesh.vertices) {
        shape.vertices.push_back({vertex[0], vertex[1], vertex[2]});
    }
    // Lit ramp materials first (SHADESTAB2 lookups), then unlit accents such
    // as the face's glowing eyes as solid COLSMOOTH words.
    for (std::size_t material = 0; material < data.ramp_size; ++material) {
        shape.colour_words.push_back(static_cast<std::uint16_t>(material << 8U));
    }
    for (std::size_t accent = 0; accent < data.accent_count; ++accent) {
        shape.colour_words.push_back(static_cast<std::uint16_t>(0xc000U | data.accents[accent]));
    }
    shape.diffuse_shade_tables = shade_tables(data);
    shape.has_diffuse_shade_tables = true;
    // One visibility triple per face gives each triangle its own facing test,
    // standing in for the retail shapes' BSP trees.
    for (std::size_t face_index = 0; face_index < mesh.faces.size(); ++face_index) {
        const auto& corners = mesh.faces[face_index];
        shape.visibilities.push_back({corners[0], corners[1], corners[2]});
        assets::Face face;
        face.visibility_index = static_cast<std::int16_t>(face_index);
        face.colour_id = mesh.colours[face_index];
        const auto& normal = mesh.normals[face_index];
        face.normal = {normal[0], normal[1], normal[2]};
        face.vertex_indices = {corners[0], corners[1], corners[2]};
        shape.faces.push_back(std::move(face));
    }
    return shape;
}

// Widest front-view extent of a model, in model units.
double front_extent(const assets::Shape& shape) {
    std::array<std::int32_t, 2> low{}, high{};
    for (const auto& vertex : shape.vertices) {
        low = {std::min(low[0], vertex.x), std::min(low[1], vertex.y)};
        high = {std::max(high[0], vertex.x), std::max(high[1], vertex.y)};
    }
    return static_cast<double>(std::max(high[0] - low[0], high[1] - low[1]));
}

// Fraction of the sprite square its opaque texels span, so a model covers
// what the sprite covered rather than the whole (partly empty) square.
double silhouette_fraction(const assets::TextureImage& texture) {
    const auto width = static_cast<std::size_t>(texture.u_mask) + 1U;
    const auto height = static_cast<std::size_t>(texture.v_mask) + 1U;
    std::size_t left = width, right = 0, top = height, bottom = 0;
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            if (y * width + x >= texture.texels.size() || texture.texels[y * width + x] == 0U) continue;
            left = std::min(left, x); right = std::max(right, x + 1U);
            top = std::min(top, y); bottom = std::max(bottom, y + 1U);
        }
    }
    if (right <= left || bottom <= top) return 1.0;
    return std::max(static_cast<double>(right - left) / static_cast<double>(width),
        static_cast<double>(bottom - top) / static_cast<double>(height));
}

const std::vector<assets::Shape>& shapes() {
    static const auto built = [] {
        std::vector<assets::Shape> result;
        for (std::size_t index = 0; index < asteroid_data::models.size(); ++index) {
            for (std::size_t level = 0; level < asteroid_data::lod_count; ++level) {
                result.push_back(build_shape(*asteroid_data::models[index], index, level));
            }
        }
        return result;
    }();
    return built;
}

} // namespace

std::uint32_t asteroid_texture_hash(const assets::TextureImage& texture) noexcept {
    std::uint32_t hash = 2'166'136'261U;
    for (const auto texel : texture.texels) {
        hash = (hash ^ texel) * 16'777'619U;
    }
    return hash;
}

const assets::Shape* asteroid_model_for_texture(const assets::TextureImage& texture) {
    // Every asteroid sprite is a 32x32 or 64x64 texture; skip hashing others.
    if (texture.texels.size() != 32U * 32U && texture.texels.size() != 64U * 64U) return nullptr;
    const auto hash = asteroid_texture_hash(texture);
    for (std::size_t index = 0; index < asteroid_data::models.size(); ++index) {
        if (asteroid_data::models[index]->texture_hash == hash) {
            return &shapes()[index * asteroid_data::lod_count];
        }
    }
    return nullptr;
}

std::span<const assets::Shape> asteroid_model_shapes() {
    return shapes();
}

namespace {

// Recognising a texture costs a full texel hash and a silhouette scan, and
// every sprite or textured quad asks on every frame. Remember each answer,
// including "not an asteroid", by texture address. A decoded-shape cache
// can free a texture and later reuse its address, so each entry also keeps
// a cheap fingerprint that must still match before it is trusted.
struct TextureMatch {
    const assets::TextureImage* texture{};
    std::uint16_t descriptor{};
    std::size_t size{};
    std::array<std::uint8_t, 16> sample{};
    const assets::Shape* model{}; // Full detail; simpler levels follow it.
    // Model scale per world unit of sprite half-size, and the full model's
    // front extent, which every level shares so switching never resizes.
    double fit{};
    double extent{};
};

std::array<std::uint8_t, 16> fingerprint(const assets::TextureImage& texture) {
    std::array<std::uint8_t, 16> sample{};
    for (std::size_t index = 0; index < sample.size() && !texture.texels.empty(); ++index) {
        sample[index] = texture.texels[index * texture.texels.size() / sample.size()];
    }
    return sample;
}

TextureMatch match_texture(const assets::TextureImage& texture) {
    thread_local std::vector<TextureMatch> matches;
    const auto sample = fingerprint(texture);
    for (const auto& match : matches) {
        if (match.texture == &texture && match.descriptor == texture.descriptor
            && match.size == texture.texels.size() && match.sample == sample) return match;
    }
    if (matches.size() >= 64U) matches.clear();
    TextureMatch match{&texture, texture.descriptor, texture.texels.size(), sample,
        asteroid_model_for_texture(texture), 0.0, 0.0};
    if (match.model != nullptr) {
        match.extent = front_extent(*match.model);
        match.fit = 2.0 * silhouette_fraction(texture) / match.extent;
    }
    matches.push_back(match);
    return match;
}

} // namespace

const assets::Shape* substitute_asteroid_model(
    const assets::Shape& source, RenderPose& pose, AsteroidModels mode) {
    if (mode == AsteroidModels::sprite || pose.explosion_progress != 0U) return nullptr;
    const assets::TextureImage* texture = nullptr;
    double half_size = 0.0;
    if (pose.simple_scaled_sprite) {
        texture = texture_for_colour(source, pose.simple_sprite_colour, pose.colour_frame);
        half_size = static_cast<double>(pose.simple_sprite_world_size) / 2.0;
    } else if (source.faces.size() == 1U && !source.faces.front().sprite
        && source.faces.front().vertex_indices.size() == 4U && source.frames.size() <= 1U) {
        // BIG_METEOR is a single textured quad rather than a scaled sprite.
        texture = texture_for_colour(source, source.faces.front().colour_id, pose.colour_frame);
        std::int32_t extent = 0;
        for (const auto& vertex : source.vertices) {
            extent = std::max({extent, std::abs(vertex.x), std::abs(vertex.y), std::abs(vertex.z)});
        }
        half_size = static_cast<double>(extent)
            * static_cast<double>(std::uint32_t{1} << source.header.shift) * pose.scale;
    }
    if (texture == nullptr || half_size <= 0.0) return nullptr;
    const auto match = match_texture(*texture);
    if (match.model == nullptr) return nullptr;
    pose.simple_scaled_sprite = false;
    pose.scale = half_size * match.fit;
    std::size_t level = mode == AsteroidModels::super_fx_medium ? 1U : 0U;
    if (mode == AsteroidModels::super_fx_low && pose.z > 0.0) {
        // On-screen diameter in native pixels (MOBJ projects with 256/z).
        const auto pixels = pose.scale * match.extent * 256.0 / pose.z;
        level = pixels >= 56.0 ? 0U : pixels >= 24.0 ? 1U : 2U;
    }
    return match.model + std::min(level, asteroid_data::lod_count - 1U);
}

}
