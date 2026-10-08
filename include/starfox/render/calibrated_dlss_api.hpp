#pragma once
#include "starfox/render/dlss_native.h"

namespace starfox::render {
// Borrow the one trusted SDK opened by the host BEFORE SDL/DXGI. No second
// SDK instance or library search. The provider and device outlive every eye.
// Keep this ABI-only description independent of the optional XR camera/math
// headers: ordinary Android/iOS/UWP/Linux hosts also expose an unavailable stub.
struct CalibratedDlssApi {
    void* module{};
    decltype(&starfox_dlss_bind_device_v1) bind{};
    decltype(&starfox_dlss_configure_v1) configure{};
    decltype(&starfox_dlss_configure_v2) configure_model{};
    decltype(&starfox_dlss_evaluate_v1) evaluate{};
    decltype(&starfox_dlss_release_viewport_v1) release{};
    decltype(&starfox_dlss_finish_frame_v1) finish_frame{};
    decltype(&starfox_dlss_evaluate_v2) evaluate_rejection{};
    bool complete() const noexcept {
        return module && bind && configure && evaluate && release
            && finish_frame && evaluate_rejection;
    }
};
} // namespace starfox::render
