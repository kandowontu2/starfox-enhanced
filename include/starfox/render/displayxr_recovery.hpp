#pragma once
#include <algorithm>
#include <chrono>

namespace starfox::render {
// Opt-in retry policy, independent of the loader and GPU. Callers drain the
// previous session before attempting discovery/device creation. Ordinary 2D
// launches never request this policy, so they never probe an SR runtime.
class DisplayXrRecovery {
public:
    using Clock=std::chrono::steady_clock;
    void request(Clock::time_point now) noexcept {enabled_=true;connected_=false;failures_=0;next_=now;}
    void stop() noexcept {enabled_=connected_=false;failures_=0;}
    void connected() noexcept {connected_=true;}
    // A session that attaches but never submits a calibrated layer is not
    // healthy; preserve its backoff across repeated READY/mode failures.
    void healthy() noexcept {if(connected_) failures_=0;}
    void failed(Clock::time_point now) noexcept {
        connected_=false;
        const unsigned seconds=std::min(30U,2U<<std::min(failures_,4U));
        next_=now+std::chrono::seconds(seconds);failures_=std::min(failures_+1,5U);
    }
    bool due(Clock::time_point now) const noexcept {return enabled_ && !connected_ && now>=next_;}
    bool retrying() const noexcept {return enabled_ && !connected_;}
private:
    bool enabled_{},connected_{};unsigned failures_{};Clock::time_point next_{};
};
}
