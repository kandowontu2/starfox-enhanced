#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace starfox::render {
// Portable, bounded lossless decoder. No SDL or platform image API.
std::vector<std::uint32_t> decode_backdrop_png(std::span<const std::uint8_t>, unsigned& width, unsigned& height);
}
