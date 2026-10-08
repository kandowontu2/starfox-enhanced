#pragma once
#include <array>
#include <string_view>

namespace starfox::render {
// An unavailable selection is not necessarily unsupported hardware. In
// particular, crash recovery deliberately starts Software until the user
// chooses GPU again. Expose that prerequisite without changing preferences.
inline constexpr std::string_view dlss_menu_status(unsigned mode,bool gpu_renderer,
    bool stereo,bool d3d12_renderer,bool runtime_loaded,bool supported,bool model_available=true) noexcept {
    constexpr std::array<std::string_view,5> modes{"OFF","QUALITY","BALANCED","PERFORMANCE","DLAA"};
    if(!mode) return modes[0];
    if(!gpu_renderer) return "GPU REQUIRED";
    if(stereo) return "MONO ONLY";
    if(!runtime_loaded) return "NO RUNTIME";
    if(!d3d12_renderer) return "DX12 REQUIRED";
    if(!supported) return "UNSUPPORTED";
    if(!model_available) return "4.5 MISSING";
    return mode<modes.size()?modes[mode]:"UNAVAILABLE";
}
}
