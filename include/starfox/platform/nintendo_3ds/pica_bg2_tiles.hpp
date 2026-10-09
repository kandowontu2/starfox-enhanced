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
        unsigned source_guard=pica_raster_base_guard,bool complete_roll=false);
    [[nodiscard]] PpuRasterWork work() const noexcept {return work_;}
    [[nodiscard]] std::uint64_t planning_attempts() const noexcept {return planning_attempts_;}
    [[nodiscard]] unsigned coverage_guard() const noexcept {return width_>top_width?(width_-top_width)/2:0;}
private:
    std::shared_ptr<const simulation::SnesPpuState> source_;
    PpuBatch batch_;
    unsigned width_{},brightness_{},subtract_{};
    bool complete_roll_{};
    std::vector<Bg2TileRect> rectangles_;
    std::vector<std::uint16_t> keys_;
    std::vector<std::uint8_t> pixels_;
    std::vector<PicaVertex> vertices_;
    PicaDraw draw_;
    PicaImage image_;
    PpuRasterWork work_;
    // Only one exact declined topology is retained. It must not replace the
    // successful borrowed atlas, nor turn a larger budget into a cached miss.
    struct RejectedPlan {
        std::shared_ptr<const simulation::SnesPpuState> source;
        PpuBatch batch;
        unsigned width{},vertex_budget{};
        bool complete{};
    };
    std::optional<RejectedPlan> rejected_;
    std::uint64_t planning_attempts_{};
};
} // namespace starfox::platform::nintendo_3ds
