#pragma once
#include "starfox/input/buttons.hpp"
#include <cstdint>
#include <optional>

namespace starfox::platform::nintendo_3ds {
// The caller supplies mapped in-game actions, not KEY_L/KEY_R or keyboard
// letters. Host monotonic time, not VM ticks or frame count, measures the hold.
class SettingsResetHold {
public:
    static constexpr std::int64_t duration=5'000'000'000LL;
    bool update(bool in_menu,input::ButtonMask mapped,std::int64_t now) noexcept {
        constexpr auto chord=input::left_shoulder|input::right_shoulder;
        if(now<0 || (previous_ && now<*previous_)) {cancel();return false;}
        previous_=now;
        if(!in_menu || (mapped&chord)!=chord) {
            started_.reset();elapsed_=0;fired_=false;return false;
        }
        if(!started_) started_=now; // Zero is a valid uptime, not a sentinel.
        elapsed_=now-*started_;
        if(fired_ || elapsed_<duration) return false;
        fired_=true;return true;
    }
    void cancel() noexcept {started_.reset();previous_.reset();elapsed_=0;fired_=false;}
    [[nodiscard]] bool active() const noexcept {return started_.has_value();}
    [[nodiscard]] std::int64_t elapsed() const noexcept {return elapsed_;}
private:
    std::optional<std::int64_t> started_,previous_;
    std::int64_t elapsed_{};
    bool fired_{};
};
} // namespace starfox::platform::nintendo_3ds
