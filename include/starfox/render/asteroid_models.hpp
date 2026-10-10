#pragma once
#include "starfox/assets/shape.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace starfox::render {
struct RenderPose;

// Optional 3D stand-ins for the cartridge's asteroid sprites, which read as
// flat cut-outs in stereo. The SUPER FX modes draw low-poly models through
// the ordinary shape path, flat shaded and dithered with the level's palette.
// Each model has 476-, 160- and 60-face levels: LOW picks one by on-screen
// size (as retail shapes do through their LOD pointers), MEDIUM always
// uses 160 faces and HIGH always uses the full model.
enum class AsteroidModels : std::uint8_t {
    sprite,
    super_fx_low,
    super_fx_medium,
    super_fx_high,
};
inline constexpr std::uint8_t asteroid_model_mode_count = 4U;
inline constexpr std::array<std::string_view, asteroid_model_mode_count>
    asteroid_model_names{"SPRITE", "SUPER FX LOW", "SUPER FX MEDIUM", "SUPER FX HIGH"};

// When `pose` draws `source` as a recognised asteroid (a whole-object sprite,
// or a lone textured quad such as BIG_METEOR), returns the 3D model and
// rewrites `pose` to draw it at the sprite's size and orientation. Otherwise
// returns nullptr and leaves `pose` untouched. Models are matched by texel
// content, so Original and Star Fox EX share them.
const assets::Shape* substitute_asteroid_model(
    const assets::Shape& source, RenderPose& pose, AsteroidModels mode);

// Exposed for tests.
[[nodiscard]] std::uint32_t asteroid_texture_hash(const assets::TextureImage& texture) noexcept;
// Full-detail model for a texture, or nullptr.
[[nodiscard]] const assets::Shape* asteroid_model_for_texture(const assets::TextureImage& texture);
// Every level of every model, full detail first within each model.
[[nodiscard]] std::span<const assets::Shape> asteroid_model_shapes();
}
