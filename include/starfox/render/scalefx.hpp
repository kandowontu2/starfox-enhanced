#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include "starfox/render/row_workers.hpp"
namespace starfox::render {
struct ScaleFxScratch {
    std::array<std::vector<std::array<float,4>>,4> passes;
};
// Standard ScaleFX 3x, defaults threshold=.5, AA/corners enabled.
// Packed ARGB output selects source pixels exactly, including their alpha.
void scale_scalefx(std::span<const std::uint32_t> source,
    std::span<std::uint32_t> output,std::uint32_t width,std::uint32_t height,
    ScaleFxScratch&,RowWorkers&);
}
