#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace starfox::state {
// Slots are replaced only after the entire temporary file was written and
// closed successfully. This is atomic replacement, not a power-loss guarantee.
void write_atomic(const std::filesystem::path& path, std::span<const std::uint8_t> bytes);
[[nodiscard]] std::vector<std::uint8_t> read_file(const std::filesystem::path& path);
} // namespace starfox::state
