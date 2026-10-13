#pragma once
#include "starfox/platform/nintendo_3ds/bg2_tile_plan.hpp"
#include "starfox/platform/nintendo_3ds/pica_raster.hpp"

namespace starfox::platform::nintendo_3ds {
// A source-tile atlas and native PICA geometry, not a CPU-rendered scene image.
// Declined frames retain the previous borrowed views; the caller uses PicaRaster.
class PicaBg2Tiles {
public:
    std::optional<PicaFrame> prepare(std::shared_ptr<const simulation::SnesPpuState>,
        const PpuBatch&,const FramePlan&,unsigned brightness,unsigned subtract,unsigned vertex_budget,
        unsigned source_guard=pica_raster_base_guard,bool complete_roll=false,bool indexed_upload=false);
    [[nodiscard]] PpuRasterWork work() const noexcept {return work_;}
    [[nodiscard]] unsigned coverage_guard() const noexcept {return width_>top_width?(width_-top_width)/2:0;}
private:
    std::shared_ptr<const simulation::SnesPpuState> source_;
    PpuBatch batch_;
    unsigned width_{},brightness_{},subtract_{};
    bool complete_roll_{},indexed_upload_{};
    std::vector<Bg2TileRect> rectangles_;
    std::vector<std::uint16_t> keys_;
    std::vector<std::uint8_t> pixels_;
    std::vector<std::uint8_t> indexed_layers_;
    std::array<std::uint32_t,512> indexed_palette_{};
    std::vector<PicaVertex> vertices_;
    PicaDraw draw_;
    PicaImage image_;
    PpuRasterWork work_;
};
} // namespace starfox::platform::nintendo_3ds
