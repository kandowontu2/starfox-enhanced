#pragma once
#include "starfox/vr/openxr_runtime.hpp"
#include "starfox/vr/openxr_input.hpp"
#include <chrono>
#include <functional>
#include <filesystem>
#include <optional>
namespace starfox::vr {
// Callbacks run on the rendering thread. Retain Android JNI global references
// until run_application returns. Zero frame/time limits mean unlimited.
struct ApplicationHost {
    const AndroidXrContext* android{};
    unsigned frame_limit{120};
    std::chrono::seconds time_limit{30};
    std::function<bool()> stop_requested;
    // Optional desktop gamepad input, sampled during the stereo frame. Hosts
    // must set active_actions for each usable action; arbitration ignores
    // unmarked fields and falls back to desktop input per action.
    std::function<VrControls()> desktop_controls;
    // Dual-band SDL fallback for cartridge-authored rumble when the active
    // OpenXR profile has no usable haptic output action.
    std::function<bool()> desktop_rumble_available;
    std::function<bool(std::uint16_t,std::uint16_t,std::uint32_t)> desktop_rumble;
    std::function<void()> stop_desktop_rumble;
    // True only for the native Steam Frame player (set by starfox_steamframe).
    // It selects run_steam_frame_application, which has its own presentation,
    // menu and controls. Every other host keeps the original loop.
    bool steam_frame{};
    std::filesystem::path cartridge_save_path;
    // Optional per-submitted-frame CSV. Supplying a path enables bounded
    // profiling; desktop hosts choose the frame limit explicitly.
    std::optional<std::filesystem::path> profile_csv_path;
};
// Shared experimental loop. Full game presentation parity remains incomplete.
int run_application(int argc, char** argv, const ApplicationHost& host = {});
// The Steam Frame loop, reached through run_application when host.steam_frame is set.
int run_steam_frame_application(int argc, char** argv, const ApplicationHost& host);
}
