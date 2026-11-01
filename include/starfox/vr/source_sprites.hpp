#pragma once
#include "starfox/vr/draw_packet.hpp"
#include "starfox/render/texture_overrides.hpp"
#include "starfox/simulation/snes_ppu.hpp"
#include <optional>
#include <string_view>
namespace starfox::simulation { class TrigTables; struct MeterState; struct CircleEffectState; struct WindowWipeState; struct DialogueState; struct ColourMathEffectState; }
namespace starfox::render { struct HudLayout; class ScaledTextRenderer; class Framebuffer; }
namespace starfox::vr {
struct FrameMenu;
struct GameSceneSnapshot;
[[nodiscard]] std::vector<DrawPacket> layout_a_menu_packets(const assets::RomImage&,const assets::SymbolMap&,
    const FrameMenu&,bool srgb=false);
[[nodiscard]] std::vector<DrawPacket> layout_a_instrument_packets(const assets::RomImage&,const assets::SymbolMap&,
    const GameSceneSnapshot&,render::ScaledTextRenderer&,bool srgb=false);
// all_but_reticle: everything except the first-person reticle's four tile-$61
// objects, for renderers that draw source_vector_reticle_packet instead.
enum class SourceSpritePass {all,world,hud,all_but_reticle};
// Compact band; per-cartridge offsets retain each native group's
// internal composition, including the Super FX meter's +16,+16 origin.
[[nodiscard]] render::HudLayout layout_a_hud(bool extended);
[[nodiscard]] DrawPacket layout_a_surface(bool menu,unsigned visible_selection=0,bool srgb=false,unsigned menu_rows=6);
[[nodiscard]] std::vector<DrawPacket> source_dialogue_packets(const assets::RomImage&,
    const assets::SymbolMap&,const simulation::DialogueState&,render::ScaledTextRenderer&,
    const std::array<uint16_t,256>& cgram,unsigned brightness=15,bool srgb=false,
    const render::TextureOverrides* portraits=nullptr);
[[nodiscard]] DrawPacket source_dialogue_meter_packet(const simulation::DialogueState&,
    const std::array<uint16_t,256>& cgram,unsigned brightness=15,bool srgb=false);
// `at`: the 32x40 frame's top-left in square source pixels (the stage
// tally's draw_face) instead of the communications placement.
[[nodiscard]] DrawPacket source_portrait_packet(const assets::RomImage&,const assets::SymbolMap&,
    uint8_t frame,bool alternate,const std::array<uint16_t,256>& cgram,
    unsigned brightness=15,bool srgb=false,const render::TextureOverrides* portraits=nullptr,
    std::optional<std::array<int,2>> at={});
// Host-drawn indexed ink (CGRAM indices, zero clear) as filtered 2D layer
// quads, in the bitmap's own pixel coordinates.
[[nodiscard]] DrawPacket source_indexed_ink_packet(const render::Framebuffer&,
    const std::array<uint16_t,256>& cgram,unsigned brightness=15,bool srgb=false);
// A portrait frame's identity for replacement art: its ROM address (each
// frame is 640 bytes of 4bpp tiles, 32x40 pixels, from FACEDATA/FACEDATA2).
[[nodiscard]] uint32_t source_portrait_address(const assets::SymbolMap&,uint8_t frame,bool alternate);
// Head-locked normalized-device shutter. Render last, without depth, using
// identity view/projection so each eye's entire viewport is covered.
// Full-screen fixed colour for ColourMathEffectState, in normalized device
// coordinates (draw with an identity camera and the effect's add/subtract,
// optionally halved, blend). Empty when the effect is off.
[[nodiscard]] DrawPacket source_colour_math_packet(const simulation::ColourMathEffectState&,bool srgb=false);
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
    SourceSpritePass pass=SourceSpritePass::all,const render::SpriteOverrides* overrides=nullptr);
// MHUD.MC's first-person indicators (the mirrored wing triangles and the
// vertical pair), a Super FX line pass above world models and below OAM.
// Inner 224x192 Super FX coordinates (place with source_ui_layer_matrix(...,
// true)); lines are one source pixel thick and keep fractional `rotation`
// (0..256, timing::interpolate_cockpit_roll). Damaged wings (M_HUDFLAGS bits
// 0/1) use palette entry 114; otherwise 112 + (M_HUDCOLOUR & 15).
// SPRITES.ASM's first-person reticle (four tile-$61 objects) as resolution-
// independent geometry: each 8x8 quarter's 3-pixel bracket with its rounded
// outer corner and its one-pixel diagonal, in PPU canvas coordinates. The
// rigid group moves between the two ticks' positions at `alpha`. Empty when
// there is no reticle or its tile art is not the retail pattern (callers keep
// the pixel sprite then).
[[nodiscard]] DrawPacket source_vector_reticle_packet(const simulation::SnesPpuState& previous,
    const simulation::SnesPpuState& current,double alpha,unsigned brightness,bool srgb=false);
[[nodiscard]] DrawPacket source_cockpit_hud_packet(const simulation::TrigTables&,double rotation,
    uint8_t colour,uint8_t damage_flags,const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb=false);
[[nodiscard]] DrawPacket source_meter_packet(const simulation::MeterState&,
    const std::array<uint16_t,256>& cgram,unsigned brightness=15,bool srgb=false,
    unsigned viewport_width=256,bool anchor_to_edges=false,const render::HudLayout* layout=nullptr);
}
