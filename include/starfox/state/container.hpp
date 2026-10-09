#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace starfox::state {
// Envelope covers the header and payload with a checksum. Schema and ROM
// identity must match before any component is deserialized.
[[nodiscard]] std::vector<std::uint8_t> pack(std::uint32_t schema,
    std::uint32_t rom_crc, std::span<const std::uint8_t> payload);
[[nodiscard]] std::span<const std::uint8_t> unpack(std::span<const std::uint8_t> file,
    std::uint32_t expected_schema, std::uint32_t expected_rom_crc);
} // namespace starfox::state
