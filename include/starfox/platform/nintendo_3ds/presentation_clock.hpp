#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace starfox::platform::nintendo_3ds {
// Host presentation only. Never feed this gate to the 60 Hz cartridge/SPC
// clock or advance it independently for the second eye.
class PresentationClock {
public:
    void reset() noexcept {previous_.reset();credit_=0;rate_=0;}
    [[nodiscard]] bool due(std::int64_t now,unsigned rate) {
        if(now<0 || (rate!=30 && rate!=60)) throw std::invalid_argument("Invalid native presentation clock");
        if(!previous_ || now<*previous_ || rate!=rate_) {
            previous_=now;credit_=0;rate_=rate;return true;
        }
        const auto elapsed=now-*previous_;previous_=now;
        // Long stalls render once, never a burst of catch-up frames. Splitting
        // before multiplication keeps arbitrarily long uptime bounded.
        if(elapsed>=1'000'000'000LL) {credit_=0;return true;}
        credit_+=static_cast<std::uint64_t>(elapsed)*rate;
        if(credit_<1'000'000'000ULL) return false;
        credit_%=1'000'000'000ULL;return true;
    }
private:
    std::optional<std::int64_t> previous_;
    std::uint64_t credit_{};
    unsigned rate_{};
};
// Count completed whole presentations, not requested FPS, source ticks or
// individual stereo eyes. Loading, suspend and editor time must be reset.
class PresentationRate {
public:
    void reset() noexcept {origin_.reset();frames_=0;fps_=0;}
    void completed(std::int64_t now) noexcept {
        if(now<0) {reset();return;}
        if(!origin_ || now<*origin_) {origin_=now;frames_=0;fps_=0;return;}
        if(frames_<65'535) ++frames_;
        const auto elapsed=now-*origin_;
        if(elapsed<1'000'000'000LL) return;
        fps_=elapsed>2'000'000'000LL?0:static_cast<unsigned>(
            (std::uint64_t(frames_)*1'000'000'000ULL+std::uint64_t(elapsed)/2)/std::uint64_t(elapsed));
        origin_=now;frames_=0;
    }
    [[nodiscard]] unsigned fps() const noexcept {return fps_;}
private:
    std::optional<std::int64_t> origin_;
    unsigned frames_{},fps_{};
};
} // namespace starfox::platform::nintendo_3ds
