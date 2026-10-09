#pragma once
#include "starfox/vr/draw_packet.hpp"
#include "starfox/simulation/snes_ppu.hpp"
#include <optional>
#include <string_view>
namespace starfox::simulation { struct MeterState; struct CircleEffectState; struct WindowWipeState; struct DialogueState; }
namespace starfox::render { struct HudLayout; class ScaledTextRenderer; }
namespace starfox::vr {
struct FrameMenu;
struct GameSceneSnapshot;
[[nodiscard]] std::vector<DrawPacket> layout_a_menu_packets(const assets::RomImage&,const assets::SymbolMap&,
    const FrameMenu&,bool srgb=false);
[[nodiscard]] std::vector<DrawPacket> layout_a_instrument_packets(const assets::RomImage&,const assets::SymbolMap&,
    const GameSceneSnapshot&,render::ScaledTextRenderer&,bool srgb=false);
enum class SourceSpritePass {all,world,hud};
// Compact band; per-cartridge offsets retain each native group's
// internal composition, including the Super FX meter's +16,+16 origin.
[[nodiscard]] render::HudLayout layout_a_hud(bool extended);
[[nodiscard]] DrawPacket layout_a_surface(bool menu,unsigned visible_selection=0,bool srgb=false,unsigned menu_rows=6);
[[nodiscard]] std::vector<DrawPacket> source_dialogue_packets(const assets::RomImage&,
    const assets::SymbolMap&,const simulation::DialogueState&,render::ScaledTextRenderer&,
    const std::array<uint16_t,256>& cgram,unsigned brightness=15,bool srgb=false);
[[nodiscard]] DrawPacket source_dialogue_meter_packet(const simulation::DialogueState&,
    const std::array<uint16_t,256>& cgram,unsigned brightness=15,bool srgb=false);
[[nodiscard]] DrawPacket source_portrait_packet(const assets::RomImage&,const assets::SymbolMap&,
    uint8_t frame,bool alternate,const std::array<uint16_t,256>& cgram,
    unsigned brightness=15,bool srgb=false);
// Head-locked normalized-device shutter. Render last, without depth, using
// identity view/projection so each eye's entire viewport is covered.
[[nodiscard]] DrawPacket source_shutter_packet(const simulation::WindowWipeState& previous,
    const simulation::WindowWipeState& current,double alpha);
[[nodiscard]] DrawPacket source_unicode_ui_text_packet(const assets::RomImage&,const assets::SymbolMap&,
    std::u32string_view text,int x,int y,uint8_t ink,const std::array<uint16_t,256>& cgram,
    unsigned brightness=15,bool srgb=false);
[[nodiscard]] DrawPacket source_circle_packet(const simulation::CircleEffectState& previous,
    const simulation::CircleEffectState& current,double alpha,unsigned brightness=15,bool srgb=false);
[[nodiscard]] DrawPacket source_ui_text_packet(const assets::RomImage&,const assets::SymbolMap&,
    std::string_view text,int x,int y,int right_clip,uint8_t ink,
    const std::array<uint16_t,256>& cgram,unsigned brightness=15,bool srgb=false,
    bool frame_glyphs=false); // frame_glyphs: draw ( ) + too (the Frame menu); the original menu leaves them as they were
[[nodiscard]] std::vector<DrawPacket> source_mode3_packets(
    const simulation::SnesPpuState&,unsigned brightness=15,bool srgb=false,
    std::optional<std::array<int16_t,2>> bg2_scroll={});
// Native variable-width briefing font; CPU lays out glyphs, GPU decodes bits.
[[nodiscard]] DrawPacket source_game_text_packet(const assets::RomImage&,
    const assets::SymbolMap&,uint32_t address,int x,int y,int right_clip,
    size_t characters,uint8_t palette_index,const std::array<uint16_t,256>&,
    unsigned brightness=15,bool srgb=false);
// Native 256x224 sprite coordinates (+Y down), clipped to that viewport.
// Caller supplies a pixel-to-world model transform and ordered layer pass.
// Optional group offsets affect HUD only; source reticle positions stay unchanged.
[[nodiscard]] DrawPacket source_sprite_packet(const simulation::SnesPpuState&,
    unsigned brightness=15,std::optional<unsigned> priority={},bool srgb=false,
    const simulation::MeterState* meters=nullptr,const render::HudLayout* layout=nullptr,
    SourceSpritePass pass=SourceSpritePass::all);
[[nodiscard]] DrawPacket source_meter_packet(const simulation::MeterState&,
    const std::array<uint16_t,256>& cgram,unsigned brightness=15,bool srgb=false,
    unsigned viewport_width=256,bool anchor_to_edges=false,const render::HudLayout* layout=nullptr);
}
