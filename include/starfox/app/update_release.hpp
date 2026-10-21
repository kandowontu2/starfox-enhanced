#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace starfox::app {
inline constexpr std::string_view update_repository = "https://github.com/kandowontu2/starfox-enhanced";
inline constexpr std::string_view update_api = "https://api.github.com/repos/kandowontu2/starfox-enhanced/releases/latest";
// Four numeric components are intentional: 0.0.8.10 must sort after 0.0.8.9.
using UpdateVersion = std::array<std::uint32_t, 4>;
std::optional<UpdateVersion> parse_update_version(std::string_view text);
struct UpdateRelease {
    std::string version;
    std::string page;
    std::string asset_url;
    std::string sha256;
    std::uint64_t bytes{};
    bool newer{};
};
// Throws on malformed/untrusted metadata; an absent platform asset is not an
// error and still permits reading the release notes/manual installation.
UpdateRelease parse_update_release(std::string_view json, std::string_view installed,
    std::string_view package_suffix);
} // namespace starfox::app
