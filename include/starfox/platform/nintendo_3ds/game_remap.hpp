#pragma once
#include "starfox/platform/nintendo_3ds/game_input.hpp"
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"

namespace starfox::platform::nintendo_3ds {
// Host editor of native bindings, NOT a replacement for the source pre-game
// menu. Navigation remains fixed; only gameplay and mapped L+R use these binds.
class GameRemap {
public:
    GameRemap();
    void open(const GameBindings&);
    // True means a visible state/binding changed. No source VM/SPC is touched.
    bool update(PadSample);
    void suspend();
    void close() noexcept {active_=false;capturing_=false;candidate_.reset();}
    [[nodiscard]] bool active() const noexcept {return active_;}
    [[nodiscard]] bool capturing() const noexcept {return capturing_;}
    [[nodiscard]] unsigned selection() const noexcept {return selection_;}
    [[nodiscard]] std::optional<input::ButtonMask> highlighted_action() const noexcept {
        return selection_<game_action_bits.size()?std::optional(game_action_bits[selection_]):std::nullopt;
    }
    [[nodiscard]] const GameBindings& bindings() const noexcept {return bindings_;}
    [[nodiscard]] unsigned redraws() const noexcept {return redraws_;}
    [[nodiscard]] PicaFrame frame() const;
    [[nodiscard]] ImageView upper_view() const noexcept {return upper_.view();}
    [[nodiscard]] ImageView lower_view() const noexcept {return lower_.view();}
private:
    void redraw();
    Canvas upper_{top_width},lower_;
    GameBindings bindings_;
    std::array<PicaVertex,6> vertices_{};
    std::array<PicaDraw,1> draws_{};
    std::array<PicaImage,1> images_{};
    input::ButtonMask previous_{};
    std::optional<unsigned> candidate_;
    unsigned selection_{},redraws_{};
    bool active_{},capturing_{},await_release_{},ambiguous_{};
};
} // namespace starfox::platform::nintendo_3ds
