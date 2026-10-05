#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/platform/nintendo_3ds/raster_coverage.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include <memory>

namespace starfox::platform::nintendo_3ds {
enum class PpuLayer {bg1,bg2,bg3,objects};
struct PpuUniqueSkyHalf {
    bool right{};
    unsigned rows{}; // Atlas rows above the receiver; ground still wraps.
    bool operator==(const PpuUniqueSkyHalf&) const=default;
};
struct PpuPass {
    PpuLayer layer{PpuLayer::bg2};
    int priority{-1}; // -1 all; BG 0 low / 1 high; OBJ 0..3.
    std::optional<std::array<std::int16_t,2>> scroll{};
    bool extend_horizontal{true},wrap_horizontal{true},transparent_black{},mosaic_inset{};
    unsigned guard_inset{},single_occurrence_top_rows{};
    render::SpriteSelection sprites{render::SpriteSelection::all};
    std::optional<PpuUniqueSkyHalf> single_occurrence_sky_half{};
    bool operator==(const PpuPass&) const=default;
};
struct PpuBatch {
    // A batch is one contiguous source painter group. Never bake models or
    // combine passes that belong on opposite sides of a native geometry pass.
    std::vector<PpuPass> passes;
    PicaSpace space{PicaSpace::screen};
    bool expand_horizontal{};
    unsigned first_row{},last_row{224};
    bool water_receiver{}; // Isolated BG2; GameLayers supplies finite surface geometry.
    bool compact_strips{}; // Preserve pixels while reducing power-of-two GPU padding.
    bool corridor_receiver{}; // Isolated BG2, not every source INATUNNEL scene.
    bool corridor_open_left{}; // Explicit source WATER colony, not a generic flag bypass.
    // Infinity-only painter groups can have disjoint eye-visible columns at
    // large separation. Borrow their visible rectangles plus the canonical
    // mono LCD, rather than uploading the unseen gap. Never use for finite
    // receivers: their source intervals depend on depth, not infinity offsets.
    bool visible_scenery_only{};
    bool operator==(const PpuBatch&) const=default;
};
struct PpuRasterWork {std::uint64_t decodes{},colour_updates{};};

// CPU decodes only cartridge-authored 2D artwork. GPU projection, native model
// depth and both eye views stay separate; this never reprojects a finished
// world image. One owner per painter group, not one per eye.
class PicaRaster {
public:
    // Immutable PPU storage must outlive preparation through the shared owner.
    // Spans remain valid until the next successful prepare; failures retain
    // the complete previous layer. Eye/receiver coverage may grow beyond the
    // normal 32px guard; oversized artwork is borrowed as native GPU strips.
    PicaFrame prepare(std::shared_ptr<const simulation::SnesPpuState>,const PpuBatch&,
        const FramePlan&,unsigned brightness=15,unsigned bg2_subtract=0,
        unsigned receiver_guard=pica_raster_base_guard,bool trim_transparent=false);
    [[nodiscard]] PpuRasterWork work() const noexcept {return work_;}
    [[nodiscard]] unsigned coverage_guard() const noexcept {
        return indexed_ && indexed_->width()>top_width?unsigned(indexed_->width()-top_width)/2:0;
    }
private:
    std::shared_ptr<const simulation::SnesPpuState> source_;
    PpuBatch batch_;
    std::unique_ptr<render::Framebuffer> indexed_;
    // Naturally aligned, typed word writes; public images borrow its exact
    // byte representation. Never cast a byte vector to an aliased word array.
    std::vector<std::uint32_t> rgba_;
    std::vector<std::uint8_t> layers_;
    std::array<PicaVertex,pica_raster_max_strips*6> vertices_{};
    std::array<PicaDraw,pica_raster_max_strips> draws_{};
    std::array<PicaImage,pica_raster_max_strips> images_{};
    std::array<std::array<unsigned,4>,pica_raster_max_strips> occupied_{};
    std::array<std::uint16_t,256> palette_{};
    unsigned brightness_{},subtract_{};
    bool visible_{};
    PpuRasterWork work_;
};
} // namespace starfox::platform::nintendo_3ds
