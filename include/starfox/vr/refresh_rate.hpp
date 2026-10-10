#pragma once
#include <openxr/openxr.h>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace starfox::vr {
// Display refresh request through XR_FB_display_refresh_rate (standard,
// section 9). The target comes from the REFRESH RATE setting (90 by default,
// 120, or none for the system rate); the request picks the highest rate the
// runtime offers that is at most the target. After two consecutive 10 s windows
// below 90% of the current rate while the session is focused, the governor steps
// down to the next lower offered rate, and so on until the 72 Hz floor.
// Everything the runtime does is behind an injectable function table.
struct RefreshApi {
    PFN_xrEnumerateDisplayRefreshRatesFB enumerate{};
    PFN_xrGetDisplayRefreshRateFB get{};
    PFN_xrRequestDisplayRefreshRateFB request{};
    // Resolves the three entry points with xrGetInstanceProcAddr; any missing
    // one leaves the whole table empty (no refresh control).
    static RefreshApi from_instance(XrInstance instance);
    [[nodiscard]] bool available() const noexcept {return enumerate && get && request;}
};

// The highest offered rate <= target. When the runtime offers nothing at or
// below the target, the lowest offered rate (the closest above it). Non-finite
// and non-positive offers are ignored; nullopt when nothing usable is offered.
[[nodiscard]] std::optional<float> choose_refresh_rate(
    std::span<const float> offered,float target) noexcept;

class RefreshRate {
public:
    static constexpr float default_target=90.F;
    static constexpr float fallback_rate=72.F;
    static constexpr double window_seconds=10.0;
    static constexpr double low_fraction=.9;
    static constexpr unsigned low_windows_before_fallback=2;

    explicit RefreshRate(RefreshApi api={}):api_(api) {}
    // Enumerate, choose and request for `target` Hz. False when the extension is
    // unavailable, nothing is offered, or the runtime refuses (status() says why).
    bool request(XrSession session,float target);
    // Stop requesting and governing; the runtime's own rate applies from the
    // next session (a request already made cannot be withdrawn).
    void release() noexcept {requested_.reset();fell_back_=false;status_="Display refresh rate left to the system";}
    // One call per submitted stereo frame. Returns the next lower offered rate
    // to pass to request() when the governor steps down; nothing once at 72 Hz.
    [[nodiscard]] std::optional<float> observe(double now_seconds,bool focused) noexcept;
    [[nodiscard]] std::optional<float> requested() const noexcept {return requested_;}
    // The rate the runtime reports now, refreshed at each window end.
    [[nodiscard]] std::optional<float> current() const noexcept {return current_;}
    // True once the governor has reached the 72 Hz floor.
    [[nodiscard]] bool fell_back() const noexcept {return fell_back_;}
    [[nodiscard]] const std::vector<float>& offered() const noexcept {return offered_;}
    [[nodiscard]] const std::string& status() const noexcept {return status_;}
private:
    RefreshApi api_;
    XrSession session_{};
    std::vector<float> offered_;
    std::optional<float> requested_,current_;
    bool fell_back_{};
    std::string status_{"Display refresh rate not requested"};
    // Governor window.
    bool started_{},tainted_{};
    double window_start_{};
    unsigned long frames_{};
    unsigned low_windows_{};
};
}
