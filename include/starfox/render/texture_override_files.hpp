#pragma once
#include "starfox/render/texture_overrides.hpp"
#include <optional>
#include <string>
namespace starfox::render {
// Replacement artwork from image files, in the layout MODERN's texture dump
// writes (modern::TextureDump) and <exe>/textures/ holds: any size, RGBA
// words with red in the low byte. Shared by the flat renderer and the PCVR app.
// Every tex_AAAAAA_l.png / tex_AAAAAA_h.png in `directory`.
TextureOverrides load_texture_overrides(const std::string& directory);
// Every face_AAAAAA.png in `directory`, keyed by the portrait frame's ROM
// address (vr::source_portrait_address).
TextureOverrides load_portrait_overrides(const std::string& directory);
// Every obj_HHHHHHHHHHHHHHHH.png in `directory`, keyed by
// render::sprite_override_key.
SpriteOverrides load_sprite_overrides(const std::string& directory);
// One image (PNG, BMP), at most max_size on a side; none when absent or unreadable.
std::optional<TextureOverrideImage> load_rgba_image(const std::string& path,unsigned max_size=8192);
}
