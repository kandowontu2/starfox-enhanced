#include "desktop_paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() try {
    const auto executable = std::filesystem::absolute("/opt/starfox/bin");
#ifdef _WIN32
    const std::string xdg_home = "C:/frame/.local/share";
    const std::string home_directory = "C:/frame";
    const std::string expected_frame_data = "C:/frame/.local/share/StarFoxEnhanced";
#else
    const std::string xdg_home = "/home/frame/.local/share";
    const std::string home_directory = "/home/frame";
    const std::string expected_frame_data = "/home/frame/.local/share/StarFoxEnhanced";
#endif

    const auto frame_xdg = starfox::vr::resolve_desktop_paths(
        executable, true, xdg_home, home_directory);
    require(frame_xdg.data_directory
            == expected_frame_data,
        "Steam Frame data did not use absolute XDG_DATA_HOME");
    require(frame_xdg.bundle == frame_xdg.data_directory / "Starfox-Assets.BIN",
        "Steam Frame bundle did not use the user data directory");

    const auto frame_fallback = starfox::vr::resolve_desktop_paths(
        executable, true, "relative-xdg", home_directory);
    require(frame_fallback.data_directory == expected_frame_data,
        "relative XDG_DATA_HOME was not ignored in favor of HOME fallback");

    const auto pcvr = starfox::vr::resolve_desktop_paths(
        executable, false, xdg_home, home_directory);
    require(pcvr.data_directory == executable / "vr-data"
            && pcvr.bundle == executable / "Starfox-Assets.BIN",
        "PCVR defaults no longer follow the executable directory");

    starfox::vr::DesktopPathOverrides overrides;
    overrides.bundle = "assets/custom.bin";
    overrides.data_directory = "save-data";
    const auto overridden = starfox::vr::resolve_desktop_paths(
        executable, true, xdg_home, home_directory, overrides);
    require(overridden.bundle == std::filesystem::absolute("assets/custom.bin")
            && overridden.data_directory == std::filesystem::absolute("save-data"),
        "explicit bundle and data-directory overrides were not applied");
    const auto explicit_without_environment = starfox::vr::resolve_desktop_paths(
        executable, true, "relative-xdg", "relative-home", overrides);
    require(explicit_without_environment.bundle == overridden.bundle
            && explicit_without_environment.data_directory == overridden.data_directory,
        "both explicit paths unnecessarily required a valid XDG/HOME default");

    overrides.data_directory.reset();
    const auto independent_bundle_override = starfox::vr::resolve_desktop_paths(
        executable, true, xdg_home, home_directory, overrides);
    require(independent_bundle_override.data_directory == frame_xdg.data_directory,
        "bundle override unexpectedly changed the default save directory");

    overrides.bundle.reset();
    overrides.data_directory = "save-data";
    const auto independent_data_override = starfox::vr::resolve_desktop_paths(
        executable, true, xdg_home, home_directory, overrides);
    require(independent_data_override.data_directory == std::filesystem::absolute("save-data")
            && independent_data_override.bundle == frame_xdg.bundle,
        "data-directory override unexpectedly changed the default bundle path");

    bool rejected_missing_home = false;
    try {
        static_cast<void>(starfox::vr::resolve_desktop_paths(
            executable, true, "relative-xdg", "relative-home"));
    } catch (const std::runtime_error&) {
        rejected_missing_home = true;
    }
    require(rejected_missing_home,
        "Steam Frame accepted a non-absolute XDG/HOME persistent data path");

    std::cout << "Desktop path defaults and overrides passed.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
