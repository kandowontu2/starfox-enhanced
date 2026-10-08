#pragma once
#include "starfox/platform/nintendo_3ds/game_input.hpp"
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"

namespace starfox::platform::nintendo_3ds {
struct HudTouch {bool held{};int x{},y{};};
// Separate native lower-LCD profile. No cartridge/world ticks in this editor.
class GameHudEditor {
public:
    GameHudEditor();
    void open(const CockpitLayout&);
    bool update(PadSample,HudTouch={});
    void suspend() noexcept {await_release_=true;dragging_=false;previous_=0;}
    [[nodiscard]] bool active() const noexcept {return active_;}
    [[nodiscard]] bool applied() const noexcept {return applied_;}
    [[nodiscard]] const CockpitLayout& layout() const noexcept {return layout_;}
    [[nodiscard]] unsigned selection() const noexcept {return selection_;}
    [[nodiscard]] unsigned redraws() const noexcept {return redraws_;}
    [[nodiscard]] PicaFrame frame() const;
    [[nodiscard]] ImageView upper_view() const {return upper_.view();}
    [[nodiscard]] ImageView lower_view() const {return lower_.view();}
private:
    void redraw();
    Canvas upper_{top_width},lower_;
    CockpitDashboard preview_;
    CockpitLayout layout_;
    input::ButtonMask previous_{};
    unsigned selection_{},redraws_{};
    int drag_x_{},drag_y_{};
    bool active_{},applied_{},await_release_{},dragging_{},touch_blocked_{};
    std::array<PicaVertex,6> vertices_{};
    std::array<PicaDraw,1> draws_{};
    std::array<PicaImage,1> images_{};
};
} // namespace starfox::platform::nintendo_3ds
