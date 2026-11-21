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
    // Unknown textures are not fire, smoke or explosions.
    starfox::assets::TextureImage blank;
    blank.descriptor = 0x4000U;
    blank.u_mask = blank.v_mask = 31U;
    blank.texels.assign(32U * 32U, 1U);
    require(!starfox::render::explosion_frame_for_texture(blank), "an arbitrary texture was recognised");
    RenderPose pose;
    pose.simple_scaled_sprite = true;
    pose.simple_sprite_world_size = 128;
    require(!starfox::render::explosion_sprite_frame(sprite_shape(blank), pose), "an unknown sprite was recognised");

    if (argc == 3) {
        // Every frame of the explosion, smoke and fireball animations is
        // recognised as its own frame of the right animation, at the sprite's size.
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        using Kind = starfox::render::ExplosionSpriteKind;
        struct Case { const char* shape; const char* colour; Kind kind; };
        for (const auto& item : std::array<Case, 3>{{{"EXPLOSION", "EXPLODE_C", Kind::large}, {"SMOKE", "SMOKE_C", Kind::smoke},
                 {"FIREBALL", "FIREBALL_C", Kind::fireball}}}) {
            const auto source = decoder.decode(low_address(symbols, item.shape, true), {},
                static_cast<std::uint16_t>(low_address(symbols, item.colour, false)));
            std::set<unsigned> own; // Frames of the animation's own kind.
            for (std::uint32_t frame = 0; frame < 16U; ++frame) {
                const std::string name = std::string{item.shape} + " frame " + std::to_string(frame);
                RenderPose sprite;
                sprite.simple_scaled_sprite = true;
                sprite.simple_sprite_world_size = 256;
                sprite.colour_frame = frame;
                const auto* texture = starfox::render::texture_for_colour(source, 0, frame);
                require(texture != nullptr, name + " has no sprite texture");
                const auto found = starfox::render::explosion_sprite_frame(source, sprite);
                if (!found) {
                    std::cerr << name << " texel hash " << std::hex << starfox::render::asteroid_texture_hash(*texture)
                              << " descriptor " << texture->descriptor << std::dec << '\n';
                }
                require(found.has_value(), name + " was not recognised");
                // The animations mix frame sets: EXPLODE_C ends on the small
                // explosion's frames, SMOKE_C opens on a large then a small
                // explosion frame. FIREBALL_C is fireball frames only.
                const bool expected = found->kind == item.kind
                    || (item.kind != Kind::fireball && (found->kind == Kind::large || found->kind == Kind::small));
                require(expected && found->frame < found->frames, name + " recognised as another animation (kind "
                    + std::to_string(int(found->kind)) + " frame " + std::to_string(int(found->frame)) + ")");
                if (found->kind == item.kind) own.insert(found->frame);
                const auto direct = starfox::render::explosion_frame_for_texture(*texture);
                require(direct && direct->kind == found->kind && direct->frame == found->frame, name + " recognised differently per draw");
                require(std::abs(found->half_size - 128.0) < 1.0, name + " is not the sprite's size");
            }
            require(!own.empty(), std::string{item.shape} + " never shows its own frames");
            if (item.kind == Kind::fireball) require(own.size() == 3U, "FIREBALL should cycle three frames");
            if (item.kind == Kind::smoke) require(own.size() == 6U, "SMOKE should show all six smoke frames");
        }
    }
    std::cout << "explosion model tests passed\n";
    return 0;
}
