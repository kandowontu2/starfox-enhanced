#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/simulation/game_simulation.hpp"

namespace starfox::platform::nintendo_3ds {
struct GameMenuRow {
    std::uint8_t id{};
    std::string label,value;
    bool enabled{};
    bool operator==(const GameMenuRow&) const=default;
};
// This is an observation of the actual host setup state, NOT a second menu
// state machine. Page changes, input latches, cheats and Start remain owned by
// GameSimulation. The cartridge-authored EX menu remains a separate source flow.
struct GameMenuState {
    bool visible{},preview{};
    simulation::PregamePage page{};
    std::uint8_t selection{},language{};
    std::string title;
    std::vector<GameMenuRow> rows;
    bool operator==(const GameMenuState&) const=default;
};
class GameMenu {
public:
    GameMenu(const assets::RomImage&,const assets::SymbolMap&);
    [[nodiscard]] static GameMenuState capture(const simulation::GameSimulation&);
    // Do not let unimplemented desktop controls change a setting that PICA
    // ignores. Navigation/back/Start still use the source menu implementation.
    [[nodiscard]] static input::TickInput filter(const simulation::GameSimulation&,input::TickInput);
    // Rebuild only when the observed page/values change, never per eye/slider.
    bool update(const GameMenuState&);
    [[nodiscard]] PicaFrame frame(const FramePlan&) const;
    [[nodiscard]] ImageView plain_view() const;
    [[nodiscard]] const GameMenuState& state() const noexcept {return state_;}
    [[nodiscard]] unsigned redraws() const noexcept {return redraws_;}
private:
    render::ScaledTextRenderer text_;
    render::Framebuffer indexed_{top_width,screen_height};
    GameMenuState state_;
    bool initialized_{};
    unsigned redraws_{};
    std::vector<std::uint8_t> rgba_,rgb_;
    std::array<PicaVertex,6> vertices_{};
    std::array<PicaDraw,1> draws_{};
    std::array<PicaImage,1> textures_{};
};
} // namespace starfox::platform::nintendo_3ds
