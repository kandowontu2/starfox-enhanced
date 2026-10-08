#pragma once
#include "starfox/platform/nintendo_3ds/game_input.hpp"
#include "starfox/platform/nintendo_3ds/game_state_storage.hpp"
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"

namespace starfox::platform::nintendo_3ds {
enum class QuickAction {resume,options,save,load};
// Physical Select+Y opens this paused host panel. Neither navigation nor image
// generation touches the source VM/SPC; shared source options remain separate.
class GameQuickMenu {
public:
    GameQuickMenu();
    void open(unsigned slot,bool states,bool options);
    bool update(PadSample);
    void suspend();
    void set_info(GameStateInfo);
    void message(std::string);
    [[nodiscard]] std::optional<QuickAction> take_action() noexcept {auto result=action_;action_.reset();return result;}
    [[nodiscard]] unsigned slot() const noexcept {return slot_;}
    [[nodiscard]] unsigned selection() const noexcept {return selection_;}
    [[nodiscard]] unsigned redraws() const noexcept {return redraws_;}
    [[nodiscard]] bool confirming() const noexcept {return confirming_;}
    [[nodiscard]] PicaFrame frame() const;
    [[nodiscard]] ImageView upper_view() const noexcept {return upper_.view();}
    [[nodiscard]] ImageView lower_view() const noexcept {return lower_.view();}
private:
    void redraw();
    Canvas upper_{top_width},lower_;
    std::array<PicaVertex,6> vertices_{};
    std::array<PicaDraw,1> draws_{};
    std::array<PicaImage,1> images_{};
    GameStateInfo info_;
    std::string message_;
    std::optional<QuickAction> action_;
    input::ButtonMask previous_{};
    unsigned slot_{},selection_{},redraws_{};
    bool states_{},options_{},await_release_{true},confirming_{};
};
} // namespace starfox::platform::nintendo_3ds
