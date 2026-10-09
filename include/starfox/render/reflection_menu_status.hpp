#pragma once
#include <array>
#include <string_view>

namespace starfox::render {
// Software has an independent reflection path. GPU compute shadows alone do
// not supply reflection rays: distinguish missing hardware from disabled RT.
inline constexpr std::string_view reflection_menu_status(unsigned mode,bool software,
    bool hardware_available,bool ray_tracing_enabled) noexcept {
    constexpr std::array<std::string_view,4> modes{"OFF","LOW","MEDIUM","HIGH"};
    if(!software && !hardware_available) return "NEEDS HW RT";
    if(!software && !ray_tracing_enabled) return "RT OFF";
    return mode<modes.size()?modes[mode]:"UNAVAILABLE";
}
}
