#pragma once
#include "starfox/platform/nintendo_3ds/pica_raster.hpp"
#include "starfox/platform/nintendo_3ds/bg2_map_rects.hpp"

namespace starfox::platform::nintendo_3ds {
// Decode authored tilemap texels once, not a finished eye/world image. Scroll,
// HDMA and stereo still use source coordinates and native finite geometry.
// This bounded path declines when coalescing is unhelpful or residency cannot
// fit; the compact character atlas/raster remain complete fallbacks.
class PicaBg2Map {
public:
    std::optional<PicaFrame> prepare(std::shared_ptr<const simulation::SnesPpuState>,
        const PpuBatch&,const FramePlan&,unsigned brightness,unsigned subtract,
        unsigned vertex_budget,unsigned texture_budget,unsigned guard);
    [[nodiscard]] PpuRasterWork work() const noexcept {return work_;}
    [[nodiscard]] unsigned coverage_guard() const noexcept {return guard_;}
private:
    PicaImageContentOwner content_;
    std::shared_ptr<const simulation::SnesPpuState> source_,rejected_;
    PpuBatch batch_,rejected_batch_;
    unsigned guard_{},rejected_guard_{},width_{},height_{},brightness_{},subtract_{};
    std::vector<std::uint8_t> indices_;
    std::vector<std::uint32_t> pixels_;
    std::vector<PicaVertex> vertices_;
    PicaImage image_;
    std::array<PicaDraw,2> draws_{};
    unsigned textured_vertices_{},guard_vertices_{};
    PpuRasterWork work_;
};
}
