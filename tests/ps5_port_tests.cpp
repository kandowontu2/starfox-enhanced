// Host checks for the PS5 port's platform policies: the hardware-only
// renderer and the VideoOut mode the SDL backend presents on.
#include "starfox/simulation/game_simulation.hpp"
#include "../platform/ps5/runtime/display_mode.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

using starfox::simulation::GameSimulation;
using starfox::simulation::RendererMode;

void hardware_only_platforms_keep_gpu() {
    static_assert(GameSimulation::constrain_renderer_mode(RendererMode::software, true)
        == RendererMode::gpu);
    static_assert(GameSimulation::constrain_renderer_mode(RendererMode::gpu, true)
        == RendererMode::gpu);
    static_assert(GameSimulation::constrain_renderer_mode(RendererMode::software, false)
        == RendererMode::software);
    // Host builds keep the user's choice; only console builds lock it.
    require(!GameSimulation::hardware_renderer_only,
        "host builds must not be hardware-renderer-only");
}

void videoout_prefers_sixty_hertz() {
    // RADV lists the 119.88 Hz mode first when high frame rate is offered.
    constexpr std::array<std::uint32_t, 2> high_first{119880U, 59940U};
    require(StarfoxPS5_PickRefresh(high_first.data(), 2U, 60000U) == 1U,
        "59.94 Hz must win over 119.88 Hz");
    constexpr std::array<std::uint32_t, 1> standard{59940U};
    require(StarfoxPS5_PickRefresh(standard.data(), 1U, 60000U) == 0U,
        "the only mode must be used");
    constexpr std::array<std::uint32_t, 1> high_only{119880U};
    require(StarfoxPS5_PickRefresh(high_only.data(), 1U, 60000U) == 0U,
        "a single high-rate mode is still a mode");
    constexpr std::array<std::uint32_t, 2> tie{50000U, 70000U};
    require(StarfoxPS5_PickRefresh(tie.data(), 2U, 60000U) == 0U,
        "ties keep the driver's order");
    require(StarfoxPS5_PickRefresh(nullptr, 0U, 60000U) == STARFOX_PS_NO_MODE,
        "no modes must be reported as such");
}

} // namespace

int main() {
    try {
        hardware_only_platforms_keep_gpu();
        videoout_prefers_sixty_hertz();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
