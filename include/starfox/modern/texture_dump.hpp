#pragma once
#include "starfox/render/texture_override_files.hpp"
#include "starfox/render/texture_overrides.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_set>

namespace starfox::modern {
// Writes the game's source artwork as PNGs in the layout MODERN loads
// replacements from (STARFOX_MODERN_DUMP_TEXTURES=1). Edit a dumped file at
// any resolution and copy it, same name, into <exe>/textures/. Each texture is
// written once, the first time it is drawn; README.txt explains every kind.
class TextureDump {
public:
    explicit TextureDump(std::string directory);
    const std::string& directory() const noexcept {return directory_;}
    // A Super FX model texture with the 16 live model palette colours
    // (BGR555; index 0 is transparent).
    void model_texture(const assets::TextureImage&,const std::array<uint16_t,16>& palette);
    // A planet background's art: bg_XXXX_sky.png (replacement geometry),
    // bg_XXXX_ground.png (template) and bg_XXXX_atlas.png (reference only).
    // `horizon` is the level horizon art row; `haze` the 112 ground colours
    // below it (RGBA, red low byte).
    void landscape(uint16_t background,const simulation::SnesPpuState&,float horizon,std::span<const uint32_t> haze);
    // Every visible OBJ sprite (HUD labels, icons, digits) as
    // obj_HHHHHHHHHHHHHHHH.png in its live OBJ palette.
    void sprites(const simulation::SnesPpuState&);
    std::size_t written() const noexcept {return written_;}
private:
    bool write(const std::string& name,unsigned width,unsigned height,std::span<const uint32_t> rgba,const std::string& note);
    std::string directory_;
    std::unordered_set<uint32_t> models_;
    std::unordered_set<uint16_t> backgrounds_;
    std::unordered_set<uint64_t> sprites_;
    std::size_t written_{};
};
// Loading the replacements (shared with the PCVR app).
using render::load_texture_overrides;
using render::load_portrait_overrides;
using render::load_sprite_overrides;
}
