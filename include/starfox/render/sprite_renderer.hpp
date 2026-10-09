#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/render/hud_layout.hpp"
#include "starfox/render/sprite_selection.hpp"
#include "starfox/simulation/snes_ppu.hpp"

#include <cstdint>
#include <optional>
#include <array>

namespace starfox::simulation {
struct MeterState;
}

namespace starfox::render {
struct MeterRectangle {std::int32_t x,y,width,height;std::uint8_t colour;};
struct MeterRectangles {std::array<MeterRectangle,64> rectangles{};std::size_t count{};};
[[nodiscard]] MeterRectangles meter_rectangles(const simulation::MeterState&,
    std::uint32_t viewport_width,bool anchor_to_edges=false,const HudLayout* layout=nullptr) noexcept;

// The cartridge rebuilds the four-piece reticle only on its 20 Hz logic
// update. Keep the source OAM itself authoritative, but move that rigid group
// between its previous and current source positions for high-rate presents.
void interpolate_crosshair_oam(
    const std::array<std::uint8_t, 544>& previous_oam,
    double interpolation_alpha,
    simulation::SnesPpuState& current_ppu) noexcept;

// Front-end/map screens can inherit the final gameplay OAM image for one
// source frame. Remove only a complete reticle group without disturbing any
// other cartridge sprites.
void suppress_crosshair_oam(simulation::SnesPpuState& ppu) noexcept;

// Extract only a complete retail four-quadrant reticle for a separate eye pass.
// The source OAM is never modified. Offset is in native logical pixels; preserve
// packed X-high/size bits and wrapped sprite coordinates, not unrelated HUD.
[[nodiscard]] std::optional<std::array<std::uint8_t,544>> isolated_crosshair_oam(
    const simulation::SnesPpuState& ppu,std::int32_t horizontal_offset=0) noexcept;

class SpriteRenderer {
public:
    // Retail MSHOWPERCGRAPH: 104x12 frame, 100x8 maximum fill.
    void draw_completion_bar(std::uint8_t percentage, Framebuffer& target) const noexcept;
    void draw_objects(
        const simulation::SnesPpuState& ppu,
        Framebuffer& target,
        std::optional<std::uint8_t> priority = std::nullopt,
        std::int32_t horizontal_origin = 0,
        bool extend_horizontal = true,
        bool anchor_edge_hud = false,
        const HudLayout* hud_layout = nullptr,
        bool suppress_configurable_hud = false,
        const simulation::MeterState* meters = nullptr,
        SpriteSelection selection = SpriteSelection::all) const noexcept;
    void draw_meters(
        const simulation::MeterState& meters,
        Framebuffer& target,
        bool anchor_to_edges = false,
        const HudLayout* hud_layout = nullptr) const noexcept;
};

} // namespace starfox::render
