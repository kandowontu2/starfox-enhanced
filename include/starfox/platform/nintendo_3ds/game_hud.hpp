#pragma once
#include "starfox/platform/nintendo_3ds/game_routing.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"

namespace starfox::platform::nintendo_3ds {
// Capture once per presentation snapshot, NOT once per eye. All observation
// uses the VM's non-mutating RAM/PPU accessors. Native counters are authoritative.
struct GameHudFrame {
    GameRouting routing{};
    simulation::MeterState meters{};
    simulation::DialogueState dialogue{};
    std::array<std::uint16_t,256> palette{};
    std::array<std::uint8_t,3> teammate_health{};
    std::uint8_t lives{},bombs{},brightness{},language{};
    // Raw cartridge counts. Only EX supplies these; retail must never read
    // an absent symbol or relabel player-one values as player two.
    struct PlayerTwo {
        std::uint8_t lives{},bombs{};
        bool operator==(const PlayerTwo&) const=default;
    };
    std::optional<PlayerTwo> player_two;
    bool paused{};
};
class GameHud {
public:
    GameHud(const assets::RomImage&,const assets::SymbolMap&);
    [[nodiscard]] GameHudFrame capture(const simulation::GameSimulation&) const;
    // No retained reference to a mutable simulation or to the caller's frame.
    bool update(const GameHudFrame&);
    void set_layout(const CockpitLayout& layout) {
        if(!layout.valid()) throw std::invalid_argument("Invalid native HUD layout");
        layout_=layout;
    }
    [[nodiscard]] ImageView view() const {return dashboard_.view();}
    [[nodiscard]] static render::SpriteSelection top_selection(const GameHudFrame& frame) noexcept {
        return frame.routing.move_hud?render::SpriteSelection::world_only:render::SpriteSelection::all;
    }
    [[nodiscard]] static HudState status(const GameHudFrame&) noexcept;
private:
    render::ScaledTextRenderer text_;
    std::array<std::uint32_t,5> addresses_{}; // P1 lives/bombs, allies, EX P2 lives/bombs.
    CockpitDashboard dashboard_;
    render::Framebuffer portrait_{32,40},radio_{284,56};
    std::vector<std::uint8_t> portrait_rgb_,radio_rgb_;
    std::optional<GameHudFrame> previous_;
    CockpitLayout layout_;
};
} // namespace starfox::platform::nintendo_3ds
