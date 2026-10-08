#pragma once
#include <cstdint>

namespace starfox::render {
// Wide outputs need not fit a 4096-pixel square. Keep the previous maximum
// pixel count (and allocation budget) while accepting the renderer's 8192
// per-axis limit. Widen before multiplying so hostile extents cannot wrap.
constexpr bool bounded_gpu_image_extent(std::uint32_t width,std::uint32_t height) noexcept {
    return width && height && width<=8192 && height<=8192
        && std::uint64_t(width)*height<=4096ULL*4096;
}
}
