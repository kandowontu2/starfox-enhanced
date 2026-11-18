#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/asteroid_models.hpp"
#include "starfox/render/explosion_models.hpp"
#include "starfox/render/face_material.hpp"
#include "starfox/render/software_renderer.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "explosion model test failed: " << message << '\n';
        std::exit(1);
    }
}

using starfox::assets::Shape;
using starfox::render::RenderPose;

// Textured quads of the sprite frame itself, facing the camera with the
// retail winding, one plane each, the back one the sprite's full square.
void check_model(const Shape& shape, const starfox::assets::TextureImage& texture, const std::string& name) {
    require(shape.faces.size() == starfox::render::explosion_layer_count, name + " has the wrong layer count");
    require(shape.faces.size() == shape.visibilities.size(), name + " needs one facing test per face");
    require(shape.bsp_root_address == 0U && shape.face_batches.empty(), name + " must draw from its face list");
    require(shape.colour_words.size() == 1U && shape.colour_words.front() == texture.descriptor,
        name + " must use the sprite frame's own descriptor");
    require(shape.textures.size() == 1U && shape.textures.front().texels == texture.texels
        && shape.textures.front().address == texture.address, name + " must sample the sprite frame's own texels");
    const auto* material_texture = starfox::render::texture_for_colour(shape, 0, 0);
    require(material_texture == &shape.textures.front(), name + " colour does not resolve to its texture");
    const double half = (texture.u_mask + 1.0) / 2.0 * 16.0;
    std::set<long long> depths;
    double largest = 0;
    for (std::size_t index = 0; index < shape.faces.size(); ++index) {
        const auto& face = shape.faces[index];
        require(face.vertex_indices.size() == 4U && !face.sprite, name + " layers must be quads");
        require(face.visibility_index == static_cast<std::int16_t>(index), name + " face lost its own facing test");
        require(face.normal.z == 127 && face.normal.x == 0 && face.normal.y == 0, name + " must face the camera");
        const auto& a = shape.vertices[face.vertex_indices[0]];
        const auto& b = shape.vertices[face.vertex_indices[1]];
        const auto& c = shape.vertices[face.vertex_indices[2]];
        const double winding = double(b.x - a.x) * (c.y - a.y) - double(b.y - a.y) * (c.x - a.x);
        require(winding < 0.0, name + " winding disagrees with its normal");
        for (const auto vertex : face.vertex_indices) {
            const auto& v = shape.vertices[vertex];
            require(v.z == a.z, name + " layer is not flat");
            require(v.z <= 0, name + " layer stands behind the sprite");
            largest = std::max(largest, std::hypot(double(v.x), double(v.y)));
        }
        require(depths.insert(a.z).second, name + " has two layers in one plane");
    }
    require(std::abs(largest - half * std::sqrt(2.0)) < 2.0, name + " back layer is not the sprite's square");
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
    // Unknown textures never substitute, and leave the pose alone.
    starfox::assets::TextureImage blank;
    blank.descriptor = 0x4000U;
    blank.u_mask = blank.v_mask = 31U;
    blank.texels.assign(32U * 32U, 1U);
    require(starfox::render::explosion_model_for_texture(blank) == nullptr, "an arbitrary texture matched a model");
    const auto unknown = sprite_shape(blank);
    RenderPose pose;
    pose.simple_scaled_sprite = true;
    pose.simple_sprite_world_size = 128;
    require(starfox::render::substitute_explosion_model(unknown, pose, true) == nullptr
        && pose.simple_scaled_sprite && pose.scale == 1.0 && !pose.use_rotation_matrix, "an unknown sprite was replaced");

    if (argc == 3) {
        // Every frame of the explosion, smoke and fireball animations has a
        // layered model at the sprite's size, facing the camera.
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        struct Case { const char* shape; const char* colour; unsigned frames; };
        for (const auto& item : std::array<Case, 3>{{{"EXPLOSION", "EXPLODE_C", 0U}, {"SMOKE", "SMOKE_C", 0U},
                 {"FIREBALL", "FIREBALL_C", 3U}}}) {
            const auto source = decoder.decode(low_address(symbols, item.shape, true), {},
                static_cast<std::uint16_t>(low_address(symbols, item.colour, false)));
            std::set<const Shape*> models;
            for (std::uint32_t frame = 0; frame < 16U; ++frame) {
                const std::string name = std::string{item.shape} + " frame " + std::to_string(frame);
                RenderPose sprite;
                sprite.simple_scaled_sprite = true;
                sprite.simple_sprite_world_size = 256;
                sprite.colour_frame = frame;
                const auto* texture = starfox::render::texture_for_colour(source, 0, frame);
                require(texture != nullptr, name + " has no sprite texture");
                auto off = sprite;
                require(starfox::render::substitute_explosion_model(source, off, false) == nullptr && off.simple_scaled_sprite,
                    name + " was replaced with 3D EXPLOSIONS off");
                auto replaced = sprite;
                const auto* model = starfox::render::substitute_explosion_model(source, replaced, true);
                if (model == nullptr) {
                    std::cerr << name << " texel hash " << std::hex << starfox::render::asteroid_texture_hash(*texture)
                              << " descriptor " << texture->descriptor << std::dec << '\n';
                }
                require(model != nullptr, name + " has no 3D model");
                require(model == starfox::render::explosion_model_for_texture(*texture), name + " picked another frame's model");
                require(!replaced.simple_scaled_sprite && replaced.use_rotation_matrix
                    && replaced.rotation_matrix == std::array<std::int16_t, 9>{32767, 0, 0, 0, 32767, 0, 0, 0, 32767},
                    name + " does not face the camera");
                check_model(*model, *texture, name);
                // The back layer covers exactly the sprite's square.
                const double half = (texture->u_mask + 1.0) / 2.0 * 16.0 * replaced.scale;
                require(std::abs(half - 128.0) < 1.0, name + " is not the sprite's size");
                models.insert(model);
            }
            if (item.frames != 0U) require(models.size() == item.frames, std::string{item.shape} + " should have one model per frame");
            else require(!models.empty(), std::string{item.shape} + " has no models");
        }
    }
    std::cout << "explosion model tests passed\n";
    return 0;
}
