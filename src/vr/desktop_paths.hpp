#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace starfox::vr {

struct DesktopPathOverrides {
    std::optional<std::filesystem::path> bundle;
    std::optional<std::filesystem::path> data_directory;
};

struct DesktopPaths {
    std::filesystem::path bundle;
    std::filesystem::path data_directory;
};

[[nodiscard]] DesktopPaths resolve_desktop_paths(
    const std::filesystem::path& executable_directory,
    bool steam_frame,
    std::string_view xdg_data_home,
    std::string_view home,
    const DesktopPathOverrides& overrides = {});

} // namespace starfox::vr
