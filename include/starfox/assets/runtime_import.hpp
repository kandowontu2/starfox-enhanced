#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace starfox::assets {

struct RuntimeImport {
    std::vector<std::uint8_t> bundle;
    std::string source_name;
};

// Accept a verified retail ROM (optionally copier-headered) or a compatible
// BIN. Uses the same embedded patches as the runtime; never modifies input.
// All validation completes before callers publish bundle to app storage.
[[nodiscard]] RuntimeImport prepare_runtime_input(
    std::span<const std::uint8_t> input,
    std::span<const std::uint8_t> (*resource)(int));

} // namespace starfox::assets
