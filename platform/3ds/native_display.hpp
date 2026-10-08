#pragma once
#include "starfox/platform/nintendo_3ds/frontend.hpp"
#include "starfox/platform/nintendo_3ds/hardware_profile.hpp"

namespace starfox::platform::nintendo_3ds {
struct NativeInput {
    input::ButtonMask held{};float slider{};bool running{},stereoscopic_hardware{};
    input::ButtonMask physical{};int circle_x{},circle_y{};
    bool touching{};int touch_x{},touch_y{};
};
// LCD/input adapter only. The game renderer must supply independent projected
// eyes. This does not pretend that SDL's software presenter is a PICA renderer.
class NativeDisplay {
public:
    NativeDisplay();~NativeDisplay();
    NativeDisplay(const NativeDisplay&)=delete;
    NativeDisplay& operator=(const NativeDisplay&)=delete;
    NativeInput poll();
    [[nodiscard]] HardwareProfile profile() const noexcept {return profile_;}
    void present(const FramePlan&,ImageView left,ImageView right,ImageView lower);
private:
    struct Runtime;
    HardwareProfile profile_{};
    std::unique_ptr<Runtime> runtime_;
};
} // namespace starfox::platform::nintendo_3ds
