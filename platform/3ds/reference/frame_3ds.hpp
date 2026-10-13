// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#pragma once

#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/object_snapshot.hpp"
#include "starfox/render/particle_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/simulation/game_simulation.hpp"

#include <cstdint>
#include <array>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace starfox::platform_3ds {

// The cartridge layers retain their 256x224 raster. Gameplay geometry uses
// the complete 400x240 top screen with the source raster centred at (72, 8).
class Frame3ds {
public:
    struct DrawBreakdown {
        float background_ms{};
        float world_ms{};
        float finish_ms{};
        std::uint32_t world_prepare_us{};
        std::uint32_t world_raster_us{};
        std::uint32_t world_composite_us{};
        std::uint32_t native_model_us{};
        std::uint32_t finalize_us{};
        std::uint32_t overlay_us{};
        std::uint32_t sprites_us{};
        std::uint32_t canvas_us{};
    };

    Frame3ds(const assets::RomImage& rom, const assets::SymbolMap& symbols);
    void set_sprite_selection(render::SpriteSelection selection) noexcept {
        if (selection != sprite_selection_) last_draw_valid_=false;
        sprite_selection_=selection;
    }
    void set_stereo_strength(std::uint8_t strength) noexcept;
    [[nodiscard]] bool stereo_active() const noexcept { return last_draw_stereo_; }
    [[nodiscard]] std::uint8_t stereo_strength() const noexcept { return stereo_strength_; }
    [[nodiscard]] const render::Framebuffer& right_frame() const noexcept {
        return last_draw_stereo_ ? right_frame_ : last_frame();
    }
    void set_top_hud_visible(bool visible) noexcept {
        if(show_top_hud_!=visible) {
            show_top_hud_=visible;
            last_draw_valid_=false;
        }
    }
    [[nodiscard]] const render::Framebuffer& draw(
        const simulation::GameSimulation& game, bool gpu_bg2 = false,
        double interpolation_alpha = 1.0);
    [[nodiscard]] bool can_reuse_presentation(
        const simulation::GameSimulation& game, bool gpu_bg2,
        double interpolation_alpha = 1.0) const;
    void capture_after_tick(const simulation::GameSimulation& game);
    [[nodiscard]] const render::Framebuffer& last_frame() const noexcept {
        return last_frame_wide_ ? wide_frame_ : frame_;
    }
    // Decode the stage's currently active geometry while the source display
    // is black, before the first visible gameplay presentation.
    [[nodiscard]] unsigned preload_active_shapes(
        const simulation::GameSimulation& game);
    struct PreloadStats { unsigned candidates{}, decoded{}, rejected{}; std::size_t bytes{}; bool limited{}; };
    using PreloadProgress = bool (*)(void*, unsigned, unsigned);
    [[nodiscard]] PreloadStats preload_catalog(std::size_t budget,
        PreloadProgress progress = nullptr, void* context = nullptr);
    [[nodiscard]] std::int16_t background_scroll_x() const noexcept {
        return background_scroll_x_;
    }
    [[nodiscard]] std::int16_t background_scroll_y() const noexcept {
        return background_scroll_y_;
    }
    [[nodiscard]] std::pair<std::int16_t, std::int16_t>
    current_background_scroll(const simulation::GameSimulation& game) const;
    [[nodiscard]] std::uint64_t world_cache_hits() const noexcept {
        return world_cache_hits_;
    }
    [[nodiscard]] std::uint64_t world_cache_misses() const noexcept {
        return world_cache_misses_;
    }
    [[nodiscard]] std::uint64_t top_hud_cache_hits() const noexcept {
        return top_hud_cache_hits_;
    }
    [[nodiscard]] std::uint64_t top_hud_cache_misses() const noexcept {
        return top_hud_cache_misses_;
    }
    [[nodiscard]] std::uint64_t background_cache_hits() const noexcept {
        return background_cache_hits_;
    }
    [[nodiscard]] std::uint64_t background_cache_misses() const noexcept {
        return background_cache_misses_;
    }
    [[nodiscard]] DrawBreakdown last_draw_breakdown() const noexcept {
        return last_draw_breakdown_;
    }
    [[nodiscard]] std::uint64_t world_cache_validation_us() const noexcept {
        return world_cache_validation_us_;
    }
    [[nodiscard]] std::uint64_t world_cache_snapshot_us() const noexcept {
        return world_cache_snapshot_us_;
    }

private:
    struct Mode1BackgroundCache {
        render::Framebuffer bg3_low{256U, 224U};
        render::Framebuffer bg3_high{256U, 224U};
        render::Framebuffer bg2_low{256U, 224U};
        render::Framebuffer bg2_high{256U, 224U};
        render::Framebuffer bg1{256U, 224U};
        simulation::SnesPpuState ppu{};
        std::array<std::uint16_t, 256U> cgram{};
        std::uint64_t ppu_vram_revision{};
        std::int16_t scroll_x{};
        std::int16_t scroll_y{};
        bool valid{};
    };

    struct RenderItem {
        simulation::ObjectHandle handle{};
        std::array<std::int16_t, 3> position{};
        simulation::MatrixQ15 rotation_matrix{};
        std::int16_t world_x{};
        std::int16_t world_z{};
        friend bool operator==(const RenderItem&, const RenderItem&) = default;
    };

    [[nodiscard]] std::uint32_t ram_symbol(const char *name) const;
    [[nodiscard]] std::uint32_t fx_symbol(const char *name) const;
    [[nodiscard]] std::uint16_t colour_symbol(const char *name) const;
    [[nodiscard]] const assets::Shape* load_base_shape(
        std::uint16_t shape, std::uint16_t colour);
    [[nodiscard]] const assets::Shape* load_lod_shape(const assets::Shape& base,
        std::uint16_t pointer, std::uint16_t colour);

    const assets::RomImage& rom_;
    const assets::SymbolMap& symbols_;
    assets::ShapeDecoder decoder_;
    simulation::TrigTables trigonometry_;
    render::Framebuffer frame_{256U, 224U};
    render::Framebuffer wide_frame_{400U, 240U};
    // Native 400x240 world raster: wider view, rather than a scaled 256x224
    // texture. The software model renderer clips to these physical pixels.
    render::Framebuffer world_{400U, 240U};
    render::Framebuffer right_world_{400U, 240U};
    render::Framebuffer right_frame_{400U, 240U};
    std::uint8_t stereo_strength_{};
    bool last_draw_stereo_{};
    // Indexed world pixels are independent of PPU composition and CGRAM.
    // Key reuse from the quantized poses and view matrix actually consumed by
    // the rasterizer; interpolation alpha alone is not a pixel dependency.
    std::array<std::int32_t, 17U> world_cache_key_{};
    simulation::MatrixQ15 world_cache_view_{};
    std::vector<RenderItem> world_cache_render_items_{};
    std::uint64_t world_cache_scene_revision_{};
    std::uint64_t last_ppu_memory_revision_{};
    std::uint64_t world_cache_validation_us_{};
    std::uint64_t world_cache_snapshot_us_{};
    std::vector<simulation::ObjectHandle> world_cache_draw_order_{};
    std::vector<std::optional<simulation::GameObject>> world_cache_objects_{};
    std::array<simulation::ParticleState, simulation::kMaximumParticles>
        world_cache_particles_{};
    std::array<simulation::DustPoint, simulation::kMaximumDustPoints>
        world_cache_dust_{};
    bool world_cache_valid_{};
    std::uint64_t world_cache_hits_{};
    std::uint64_t world_cache_misses_{};
    std::uint64_t background_cache_hits_{};
    std::uint64_t background_cache_misses_{};
    DrawBreakdown last_draw_breakdown_{};
    render::Framebuffer mode2_background_cache_{400U, 240U};
    std::array<Mode1BackgroundCache, 2U> mode1_caches_{};
    std::uint8_t mode1_next_victim_{};
    simulation::SnesPpuState mode2_background_ppu_{};
    std::array<std::uint16_t, 256U> mode2_background_cgram_{};
    std::uint64_t mode2_background_ppu_vram_revision_{};
    std::uint64_t mode2_background_scene_revision_{};
    std::int16_t mode2_background_x_{};
    std::int16_t mode2_background_y_{};
    std::int16_t background_scroll_x_{};
    std::int16_t background_scroll_y_{};
    bool mode2_background_valid_{};
    simulation::SnesPpuState last_ppu_{};
    simulation::GameFlowState last_flow_{};
    std::uint64_t last_scene_revision_{};
    bool last_frame_gpu_bg2_{};
    bool last_frame_wide_{};
    bool last_draw_valid_{};
    double last_interpolation_alpha_{1.0};
    timing::TransformSnapshot previous_camera_{};
    timing::TransformSnapshot current_camera_{};
    render::ObjectSnapshotMap previous_objects_{};
    render::ObjectSnapshotMap current_objects_{};
    std::uint64_t snapshot_scene_revision_{};
    simulation::GameFlowState snapshot_flow_{};
    bool snapshots_ready_{};
    render::BackgroundRenderer backgrounds_;
    render::SpriteRenderer sprites_;
    render::Framebuffer corner_hud_{256U,224U};
    render::Framebuffer meter_hud_{256U,224U};
    render::Framebuffer top_hud_cache_{400U,240U};
    std::vector<std::uint32_t> top_hud_occupied_indices_{};
    std::uint64_t top_hud_ppu_memory_revision_{};
    simulation::MeterState top_hud_meters_{};
    std::uint8_t top_hud_object_select_{};
    bool top_hud_cache_valid_{};
    std::uint64_t top_hud_cache_hits_{};
    std::uint64_t top_hud_cache_misses_{};
    render::software_renderer_detail::Scratch model_scratch_;
    render::SpriteSelection sprite_selection_{render::SpriteSelection::all};
    render::SoftwareRenderer models_;
    render::ParticleRenderer particles_;
    render::ScaledTextRenderer text_;
    bool show_top_hud_{true};
    render::DustRenderer dust_;
    std::unordered_map<std::uint32_t, assets::Shape> shapes_;
    // Compact LODs inherit scale/material/bounds from their parent. They
    // cannot share the base cache or a key containing only the child address.
    std::unordered_map<std::uint64_t, assets::Shape> lod_shapes_;
    std::unordered_set<std::uint64_t> invalid_lods_;
    std::unordered_set<std::uint32_t> invalid_shapes_;
    std::vector<RenderItem> items_;
};

} // namespace starfox::platform_3ds
