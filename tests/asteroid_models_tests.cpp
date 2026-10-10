#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/asteroid_models.hpp"
#include "starfox/render/software_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "asteroid model test failed: " << message << '\n';
        std::exit(1);
    }
}

using starfox::assets::Shape;
using starfox::render::AsteroidModels;
using starfox::render::RenderPose;

void check_model(const Shape& shape) {
    const auto& name = shape.name;
    require(!shape.vertices.empty() && shape.vertices.size() <= 256U, name + " exceeds the 8-bit vertex index range");
    require(shape.faces.size() == shape.visibilities.size(), name + " needs one facing test per face");
    require(shape.has_diffuse_shade_tables, name + " is missing Super FX shade tables");
    require(shape.bsp_root_address == 0U && shape.face_batches.empty(), name + " must draw from its face list");
    std::size_t materials = 0;
    std::set<std::uint8_t> slots;
    for (const auto word : shape.colour_words) {
        if ((word & 0xc000U) == 0xc000U) {
            const auto slot = static_cast<std::uint8_t>(word & 0x0fU);
            require(slot >= 1U && slot <= 4U, name + " has an accent outside the sprite's eye colours");
        } else {
            require(word == (materials << 8U), name + " materials must be sequential SHADESTAB2 lookups");
            ++materials;
        }
    }
    require(materials >= 2U && materials <= shape.diffuse_shade_tables[0].size(), name + " has an invalid ramp");
    for (const auto& depth : shape.diffuse_shade_tables) {
        for (std::size_t material = 0; material < materials; ++material) {
            for (const auto byte : depth[material]) {
                slots.insert(static_cast<std::uint8_t>(byte & 0x0fU));
                slots.insert(static_cast<std::uint8_t>(byte >> 4U));
            }
        }
    }
    for (const auto slot : slots) {
        // Asteroid texels use slots 1-4 (reds/yellows) and 9-14 (the ramp).
        require((slot >= 1U && slot <= 4U) || (slot >= 9U && slot <= 14U), name + " shades outside the sprite palette");
    }
    for (std::size_t index = 0; index < shape.faces.size(); ++index) {
        const auto& face = shape.faces[index];
        require(face.vertex_indices.size() == 3U && !face.sprite, name + " faces must be triangles");
        require(face.visibility_index == static_cast<std::int16_t>(index), name + " face lost its own facing test");
        const auto& visibility = shape.visibilities[index];
        require(visibility.a == face.vertex_indices[0] && visibility.b == face.vertex_indices[1]
            && visibility.c == face.vertex_indices[2], name + " facing test does not match its face");
        require(face.colour_id < shape.colour_words.size(), name + " face colour is outside its table");
        const auto length = std::sqrt(static_cast<double>(face.normal.x * face.normal.x
            + face.normal.y * face.normal.y + face.normal.z * face.normal.z));
        require(length > 120.0 && length < 128.5, name + " normals must use the retail 127 scale");
        const auto& a = shape.vertices[face.vertex_indices[0]];
        const auto& b = shape.vertices[face.vertex_indices[1]];
        const auto& c = shape.vertices[face.vertex_indices[2]];
        const std::array<double, 3> u{double(b.x - a.x), double(b.y - a.y), double(b.z - a.z)};
        const std::array<double, 3> v{double(c.x - a.x), double(c.y - a.y), double(c.z - a.z)};
        const std::array<double, 3> winding{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        // Retail shapes store inward normals: (b-a)x(c-a) opposes them.
        require(winding[0] * face.normal.x + winding[1] * face.normal.y + winding[2] * face.normal.z < 0.0,
            name + " face winding disagrees with its normal");
    }
}

Shape sprite_shape(const starfox::assets::TextureImage& texture) {
    Shape shape;
    shape.colour_words = {texture.descriptor};
    shape.textures = {texture};
    starfox::assets::Face face;
    face.sprite = true;
    face.vertex_indices = {0};
    face.visibility_index = -1;
    shape.faces = {face};
    shape.vertices = {{0, 0, 0}};
    return shape;
}

std::uint32_t low_address(const starfox::assets::SymbolMap& symbols, const char* name, bool bank_zero) {
    for (const auto address : symbols.find(name)) {
        if (!bank_zero || (address >> 16U) == 0U) return address & 0xffffU;
    }
    require(false, std::string{"missing symbol "} + name);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const auto shapes = starfox::render::asteroid_model_shapes();
    require(shapes.size() == 12U, "expected three levels of grey, orange, face and crater models");
    for (std::size_t model = 0; model < 4U; ++model) {
        require(shapes[model * 3U].faces.size() > shapes[model * 3U + 1U].faces.size()
            && shapes[model * 3U + 1U].faces.size() > shapes[model * 3U + 2U].faces.size()
            && shapes[model * 3U + 2U].faces.size() <= 64U, "levels of detail must get simpler");
    }
    for (const auto& shape : shapes) check_model(shape);

    // Unknown textures and SPRITE mode never substitute, and leave the pose alone.
    starfox::assets::TextureImage blank;
    blank.descriptor = 0x4000U;
    blank.u_mask = blank.v_mask = 31U;
    blank.texels.assign(32U * 32U, 1U);
    require(starfox::render::asteroid_model_for_texture(blank) == nullptr, "an arbitrary texture matched a model");
    const auto unknown = sprite_shape(blank);
    RenderPose pose;
    pose.simple_scaled_sprite = true;
    pose.simple_sprite_world_size = 128;
    require(starfox::render::substitute_asteroid_model(unknown, pose, AsteroidModels::super_fx_high) == nullptr
        && pose.simple_scaled_sprite && pose.scale == 1.0, "an unknown sprite was replaced");

    if (argc == 3) {
        // Every retail asteroid texture, in Original and EX alike, has a model
        // that stays inside the square its sprite covered.
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        struct Case { const char* shape; const char* colour; };
        for (const auto& item : std::array<Case, 4>{{{"ASTEROID1", "ASTEROID_C"}, {"ASTEROID1", "BREAK_METEOR_C"},
                 {"ASTEROID2", "ASTEROID2_C"}, {"BIG_METEOR", "BIG_METEOR_C"}}}) {
            auto source = decoder.decode(low_address(symbols, item.shape, true), {},
                static_cast<std::uint16_t>(low_address(symbols, item.colour, false)));
            RenderPose sprite;
            const bool whole_sprite = source.faces.size() == 1U && source.faces.front().sprite;
            sprite.simple_scaled_sprite = whole_sprite;
            sprite.simple_sprite_world_size = static_cast<std::int16_t>(source.header.size * 2);
            auto untouched = sprite;
            require(starfox::render::substitute_asteroid_model(source, untouched, AsteroidModels::sprite) == nullptr,
                std::string{item.colour} + " was replaced in SPRITE mode");
            auto replaced = sprite;
            const auto* model = starfox::render::substitute_asteroid_model(source, replaced, AsteroidModels::super_fx_high);
            require(model != nullptr, std::string{item.colour} + " has no 3D model");
            require(!replaced.simple_scaled_sprite && replaced.scale > 0.0, std::string{item.colour} + " pose was not converted");
            const auto square = whole_sprite ? double(sprite.simple_sprite_world_size)
                : double(source.header.size) * 2.0;
            for (const auto& vertex : model->vertices) {
                require(std::abs(vertex.x) * replaced.scale <= square * 0.6 && std::abs(vertex.y) * replaced.scale <= square * 0.6,
                    std::string{item.colour} + " model overflows its sprite");
            }
            // The per-texture match is cached; repeats must agree with the first.
            auto repeated = sprite;
            require(starfox::render::substitute_asteroid_model(source, repeated, AsteroidModels::super_fx_high) == model
                && repeated.scale == replaced.scale, std::string{item.colour} + " cached match changed");
            // HIGH is always full detail and MEDIUM always the middle level;
            // LOW follows on-screen size and drops to the simplest far away.
            require(model == starfox::render::asteroid_model_for_texture(source.textures.front()),
                std::string{item.colour} + " HIGH did not use the full model");
            auto medium = sprite;
            require(starfox::render::substitute_asteroid_model(source, medium, AsteroidModels::super_fx_medium) == model + 1,
                std::string{item.colour} + " MEDIUM did not use the middle level");
            auto near = sprite;near.z = 64.0;
            require(starfox::render::substitute_asteroid_model(source, near, AsteroidModels::super_fx_low) == model,
                std::string{item.colour} + " LOW did not use full detail up close");
            auto far = sprite;far.z = 30000.0;
            require(starfox::render::substitute_asteroid_model(source, far, AsteroidModels::super_fx_low) == model + 2,
                std::string{item.colour} + " LOW did not use the simplest level far away");
            require(near.scale == replaced.scale && far.scale == replaced.scale && medium.scale == replaced.scale,
                std::string{item.colour} + " changing detail level resized the rock");
            // A texture rewritten at the same address must not reuse a stale match.
            auto& texels = source.textures.front().texels;
            texels.front() = static_cast<std::uint8_t>(texels.front() ^ 0x0fU);
            auto rewritten = sprite;
            require(starfox::render::substitute_asteroid_model(source, rewritten, AsteroidModels::super_fx_high) == nullptr,
                std::string{item.colour} + " matched a rewritten texture from cache");
        }
    }
    std::cout << "asteroid model tests passed\n";
    return 0;
}
