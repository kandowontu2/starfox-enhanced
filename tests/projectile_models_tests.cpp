#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/face_material.hpp"
#include "starfox/render/projectile_models.hpp"
#include "starfox/render/software_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "projectile model test failed: " << message << '\n';
        std::exit(1);
    }
}

using starfox::assets::Shape;
using starfox::render::RenderPose;

// A valid Super FX shape coloured only with slots the sprite frame itself
// uses, wound and lit like retail faces, with faces toward the camera.
void check_model(const Shape& shape, const starfox::assets::TextureImage& texture, const std::string& name) {
    require(!shape.vertices.empty() && shape.vertices.size() <= 256U, name + " exceeds the 8-bit vertex index range");
    require(shape.faces.size() == shape.visibilities.size(), name + " needs one facing test per face");
    require(shape.bsp_root_address == 0U && shape.face_batches.empty(), name + " must draw from its face list");
    std::set<std::uint8_t> sprite_slots;
    for (const auto texel : texture.texels) if (texel != 0U) sprite_slots.insert(static_cast<std::uint8_t>(texel & 0x0fU));
    for (const auto word : shape.colour_words) {
        require((word & 0xc000U) == 0xc000U, name + " colours must be unlit solid slots");
        require(sprite_slots.contains(static_cast<std::uint8_t>(word & 0x0fU)), name + " uses a slot its sprite does not");
    }
    std::size_t toward_camera = 0;
    for (std::size_t index = 0; index < shape.faces.size(); ++index) {
        const auto& face = shape.faces[index];
        require(face.vertex_indices.size() == 3U && !face.sprite, name + " faces must be triangles");
        require(face.visibility_index == static_cast<std::int16_t>(index), name + " face lost its own facing test");
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
        if (face.normal.z > 0) ++toward_camera; // Inward normal pointing away: the face looks at the camera (-z).
    }
    require(toward_camera * 2U > shape.faces.size(), name + " should mostly face the camera");
    // MODERN's solid-decal pass tests faces that share a plane in pairs, every
    // headset frame: a flat halo of ~190 coplanar faces cost ~18 ms a frame
    // for a stream of pulses. Only a quad's two halves may share a plane.
    std::map<std::array<long long, 4>, std::size_t> planes;
    for (const auto& face : shape.faces) {
        const auto& a = shape.vertices[face.vertex_indices[0]];
        const auto& b = shape.vertices[face.vertex_indices[1]];
        const auto& c = shape.vertices[face.vertex_indices[2]];
        const std::array<double, 3> u{double(b.x - a.x), double(b.y - a.y), double(b.z - a.z)};
        const std::array<double, 3> v{double(c.x - a.x), double(c.y - a.y), double(c.z - a.z)};
        std::array<double, 3> n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const auto length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        for (auto& component : n) component /= length;
        if (n[0] < 0 || (n[0] == 0 && (n[1] < 0 || (n[1] == 0 && n[2] < 0)))) for (auto& component : n) component = -component;
        const auto distance = n[0] * a.x + n[1] * a.y + n[2] * a.z;
        const auto plane = std::array<long long, 4>{std::llround(n[0] * 1000.0), std::llround(n[1] * 1000.0),
            std::llround(n[2] * 1000.0), std::llround(distance)};
        require(++planes[plane] <= 2U, name + " has more than two faces in one plane");
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
    // Unknown textures never substitute, and leave the pose alone.
    starfox::assets::TextureImage blank;
    blank.descriptor = 0x4000U;
    blank.u_mask = blank.v_mask = 31U;
    blank.texels.assign(32U * 32U, 1U);
    require(starfox::render::projectile_model_for_texture(blank) == nullptr, "an arbitrary texture matched a model");
    const auto unknown = sprite_shape(blank);
    RenderPose pose;
    pose.simple_scaled_sprite = true;
    pose.simple_sprite_world_size = 128;
    require(starfox::render::substitute_projectile_model(unknown, pose, true) == nullptr
        && pose.simple_scaled_sprite && pose.scale == 1.0 && !pose.use_rotation_matrix, "an unknown sprite was replaced");

    if (argc == 3) {
        // Every frame of the pulse, the Nova Bomb and the plasma ball has a
        // model facing the camera at the sprite's size.
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        struct Case { const char* shape; const char* colour; unsigned frames; };
        for (const auto& item : std::array<Case, 6>{{{"PLAYERBEAM", "PLAYERBEAM_C", 1U}, {"NUKE", "SPECIALWEAPON_C", 3U},
                 {"LARGEPLASMA", "BOUNCYBALL_C", 2U}, {"BOUNCYBALL", "BOUNCYBALL_C", 2U},
                 {"RINGLASER", "RINGLASER_C", 2U}, {"OVALBEAM", "OVALBEAM_C", 3U}}}) {
            const auto source = decoder.decode(low_address(symbols, item.shape, true), {},
                static_cast<std::uint16_t>(low_address(symbols, item.colour, false)));
            std::set<const Shape*> models;
            std::size_t vertex_count = 0, most_vertices = 0, fewest_vertices = 256;
            for (std::uint32_t frame = 0; frame < 8U; ++frame) {
                const std::string name = std::string{item.shape} + " frame " + std::to_string(frame);
                RenderPose sprite;
                sprite.simple_scaled_sprite = true;
                sprite.simple_sprite_world_size = 256;
                sprite.colour_frame = frame;
                const auto* texture = starfox::render::texture_for_colour(source, 0, frame);
                require(texture != nullptr, name + " has no sprite texture");
                auto off = sprite;
                require(starfox::render::substitute_projectile_model(source, off, false) == nullptr && off.simple_scaled_sprite,
                    name + " was replaced with 3D PROJECTILES off");
                auto exploding = sprite;exploding.explosion_progress = 1U;
                require(starfox::render::substitute_projectile_model(source, exploding, true) == nullptr,
                    name + " was replaced while exploding");
                auto replaced = sprite;
                const auto* model = starfox::render::substitute_projectile_model(source, replaced, true);
                require(model != nullptr, name + " has no 3D model");
                require(model == starfox::render::projectile_model_for_texture(*texture), name + " picked another frame's model");
                require(!replaced.simple_scaled_sprite && replaced.scale > 0.0 && replaced.use_rotation_matrix
                    && replaced.rotation_matrix == std::array<std::int16_t, 9>{32767, 0, 0, 0, 32767, 0, 0, 0, 32767},
                    name + " does not face the camera");
                check_model(*model, *texture, name);
                // Inside the square the sprite covered, and filling most of it.
                double reach = 0.0;
                for (const auto& vertex : model->vertices) {
                    reach = std::max(reach, std::hypot(double(vertex.x), double(vertex.y)) * replaced.scale);
                    require(std::abs(vertex.x) * replaced.scale <= 128.0 * 1.05 && std::abs(vertex.y) * replaced.scale <= 128.0 * 1.05,
                        name + " model overflows its sprite");
                }
                require(reach > 128.0 * 0.6, name + " model is much smaller than its sprite");
                // Frames of one animation share a body mesh; only the
                // sprite's stray arcs (a few ribbons) come and go.
                if (vertex_count == 0U) vertex_count = model->vertices.size();
                require(model->vertices.size() + 64U >= vertex_count && model->vertices.size() <= vertex_count + 64U,
                    name + " mesh differs from the first frame's beyond its arcs");
                most_vertices = std::max(most_vertices, model->vertices.size());
                fewest_vertices = std::min(fewest_vertices, model->vertices.size());
                if (std::string_view{item.shape} == "RINGLASER") {
                    // A ring keeps its hole: nothing near the middle.
                    for (const auto& vertex : model->vertices) {
                        require(std::hypot(double(vertex.x), double(vertex.y)) * replaced.scale > 128.0 * 0.25,
                            name + " closes the ring's hole");
                    }
                }
                models.insert(model);
            }
            require(models.size() == item.frames, std::string{item.shape} + " should have one model per animation frame");
            // The plasma ball's red frame keeps its yellow arcs; the blue one has none.
            if (std::string_view{item.shape} == "LARGEPLASMA" || std::string_view{item.shape} == "BOUNCYBALL")
                require(most_vertices > fewest_vertices, std::string{item.shape} + " lost the red frame's arcs");
        }
    }
    std::cout << "projectile model tests passed\n";
    return 0;
}
