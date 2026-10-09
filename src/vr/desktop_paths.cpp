#include "desktop_paths.hpp"

#include <stdexcept>
#include <string>

namespace starfox::vr {
namespace {

std::filesystem::path absolute_path(const std::filesystem::path& path) {
    return std::filesystem::absolute(path);
}

std::filesystem::path steam_frame_data_directory(
    std::string_view xdg_data_home, std::string_view home) {
    if (!xdg_data_home.empty()) {
        const std::filesystem::path xdg_path{xdg_data_home};
        if (xdg_path.is_absolute()) {
            return xdg_path / "StarFoxEnhanced";
        }
    }

    if (!home.empty()) {
        const std::filesystem::path home_path{home};
        if (home_path.is_absolute()) {
            return home_path / ".local" / "share" / "StarFoxEnhanced";
        }
    }

    throw std::runtime_error{
        "Steam Frame needs an absolute XDG_DATA_HOME or HOME to locate user data"};
}

} // namespace

DesktopPaths resolve_desktop_paths(
    const std::filesystem::path& executable_directory,
    bool steam_frame,
    std::string_view xdg_data_home,
    std::string_view home,
    const DesktopPathOverrides& overrides) {
    const auto executable = absolute_path(executable_directory);
    DesktopPaths paths;
    if (steam_frame) {
        if (overrides.bundle && overrides.data_directory) {
            paths.bundle = absolute_path(*overrides.bundle);
            paths.data_directory = absolute_path(*overrides.data_directory);
        } else {
            const auto default_data_directory =
                steam_frame_data_directory(xdg_data_home, home);
            paths.bundle = overrides.bundle
                ? absolute_path(*overrides.bundle)
                : default_data_directory / "Starfox-Assets.BIN";
            paths.data_directory = overrides.data_directory
                ? absolute_path(*overrides.data_directory)
                : default_data_directory;
        }
    } else {
        paths.data_directory = executable / "vr-data";
        paths.bundle = executable / "Starfox-Assets.BIN";
        if (overrides.bundle) {
            paths.bundle = absolute_path(*overrides.bundle);
        }
        if (overrides.data_directory) {
            paths.data_directory = absolute_path(*overrides.data_directory);
        }
    }
    return paths;
}

} // namespace starfox::vr
