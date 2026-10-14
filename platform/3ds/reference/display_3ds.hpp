// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#pragma once

#include "starfox/render/framebuffer.hpp"
#include "bg2_plan.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/simulation/snes_ppu.hpp"

#include <3ds.h>
#include <citro2d.h>

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <vector>

namespace starfox::platform_3ds {

struct DiagnosticSnapshot {
    float fps{}, logic_hz{}, frame_ms{}, game_ms{}, draw_ms{};
    float upload_ms{}, vblank_ms{}, phase_ms{};
    unsigned missed{}, audio_late_packets{}, heap_free_kib{}, heap_total_kib{};
    unsigned linear_free_kib{}, linear_total_kib{}, ppu_mode{};
    unsigned session_seconds{};
    int audio_core{-1};
    const char* flow{"BOOT"};
    bool ex{}, gpu_bg2{}, audio_ready{}, audio_async{}, new_3ds{};
};

// High-resolution wall time for the most recent PICA presentation attempt.
// Values use microseconds from the ARM11 system tick, not millisecond OS time.
struct DisplayFrameTiming {
    std::uint32_t plan_us{}, plan_wait_us{}, cache_validation_us{}, acquire_us{}, build_us{}, flush_us{};
    std::uint32_t transfer_us{}, submit_us{};
    bool reuse_hit{}, bg2_plan_checked{}, plan_cache_hit{};
    bool gpu_begin_failed{}, gpu_submitted{}, bg2_plan_worker_used{};
};

// CPU builds the indexed source raster and palette. PICA200 owns the texture
// transfer and top-screen presentation at native pixel coordinates. RGB565 halves transfer size
// relative to RGBA8; a direct-framebuffer fallback remains available.
class Display3ds {
public:
    Display3ds() = default;
    ~Display3ds();
    Display3ds(const Display3ds&) = delete;
    Display3ds& operator=(const Display3ds&) = delete;

    [[nodiscard]] bool open();
    void close() noexcept;
    // Reactivates the retained owner after source effects/stereo. Returns true
    // on a handoff, requiring the lower-LCD cache to be invalidated.
    bool activate();
    // False means that no frame was submitted (the GPU is busy, or the
    // direct-framebuffer fallback is unavailable).
    // wait_for_gpu joins the prior command queue at acquire, before changing
    // borrowed texture storage; false retains the reference nonblocking mode.
    [[nodiscard]] bool present(const render::Framebuffer& indexed,
        const render::Palette256& palette, bool reuse_source = false,
        const render::Framebuffer* right = nullptr, bool wait_for_gpu = false);
    [[nodiscard]] bool present_black();
    // BG2 tile geometry and palette expansion run on PICA200. The foreground
    // framebuffer must contain only the world, HUD and text; write coverage
    // supplies alpha so an explicitly drawn palette index zero stays opaque.
    // With source_foreground (mono only), foreground is the existing world;
    // its zero-ink and the covered 256x224 source merge during RGB5A1 upload.
    [[nodiscard]] bool present_mode2(const simulation::SnesPpuState& ppu,
        std::uint64_t ppu_vram_revision,
        std::int32_t scroll_x, std::int32_t scroll_y,
        const render::Framebuffer& foreground,
        const render::Palette256& palette, bool reuse_source = false,
        const render::Framebuffer* right = nullptr, bool wait_for_gpu = false,
        const render::Framebuffer* source_foreground = nullptr);
    // Start the New 3DS geometry job before software rendering. The caller
    // must not advance the PPU until present_mode2() has consumed the job.
    void prepare_mode2_plan(const simulation::SnesPpuState& ppu,
        std::uint64_t ppu_vram_revision,
        std::int32_t scroll_x, std::int32_t scroll_y,
        bool core2_available = true);
    [[nodiscard]] bool eligible_mode2(
        const simulation::SnesPpuState& ppu) const noexcept;
    [[nodiscard]] bool bg2_plan_worker_enabled() const noexcept {
        return bg2_plan_worker_ != nullptr;
    }
    [[nodiscard]] const DisplayFrameTiming& last_frame_timing() const noexcept {
        return frame_timing_;
    }
    [[nodiscard]] bool gpu_active() const noexcept { return gpu_active_; }
    void set_wide(bool value) noexcept { wide_ = value; }
    [[nodiscard]] std::size_t bg2_fragment_count() const noexcept {
        return tile_quads_.size();
    }
    [[nodiscard]] std::size_t bg2_source_fragment_count() const noexcept {
        return bg2_plan_.size();
    }
    [[nodiscard]] bool show_full_dump_progress(const char* stage,
        std::uint64_t written, std::uint64_t total);
    const u8* top_framebuffer() const noexcept { return last_top_framebuffer_; }
    void note_top_framebuffer(const u8* pixels) noexcept {
        last_top_framebuffer_ = pixels;
        frame_texture_valid_ = false;
        cpu_frame_valid_ = false;
        right_texture_valid_ = right_overlay_valid_ = false;
    }
    const u8* bottom_framebuffer() const noexcept { return last_bottom_framebuffer_; }
    void note_bottom_framebuffer(const u8* pixels) noexcept {
        last_bottom_framebuffer_ = pixels;
    }

private:
    void suspend() noexcept;
    void prepare_draw_state();
    struct TileQuad {
        std::uint16_t x, y, width, height, slot;
        std::uint8_t first_x, first_y;
        bool reverse_x, reverse_y;
        bool solid;
        std::uint8_t solid_colour;
    };
    struct Bg2PlanKey {
        std::uint8_t background_mode{}, mosaic{}, main_screen{}, bg2_screen_size{};
        bool bg2_tile_size_16{};
        std::uint16_t bg2_character_base{}, bg2_screen_base{};
        bool bg2_vertical_offsets_enabled{};
        std::array<std::int16_t, 224U> bg2_horizontal_offsets{};
        bool bg2_horizontal_offsets_enabled{};
        std::array<std::int16_t, 224U> bg2_scanline_scroll_y{};
        bool bg2_scanline_scroll_enabled{};
    };
    [[nodiscard]] static Bg2PlanKey make_bg2_plan_key(
        const simulation::SnesPpuState& ppu) noexcept;
    [[nodiscard]] bool same_bg2_plan_state(
        const simulation::SnesPpuState& ppu) const noexcept;
    [[nodiscard]] bool build_mode2_quads(
        const simulation::SnesPpuState& ppu,
        std::uint64_t ppu_vram_revision,
        const render::Palette256& palette);
    [[nodiscard]] std::int16_t decode_tile(
        const simulation::SnesPpuState& ppu,
        const render::Palette256& palette,
        std::uint16_t character, std::uint8_t palette_bank);
    [[nodiscard]] bool present_cpu(const render::Framebuffer& indexed,
        const render::Palette256& palette, bool reuse_source);
    void upload_right(const render::Framebuffer& frame,
        const render::Palette256& palette, bool foreground, bool reuse_source);
    static void bg2_plan_worker_main(void* argument);
    void wait_for_bg2_plan() noexcept;
    [[nodiscard]] bool consume_bg2_plan(
        const simulation::SnesPpuState& ppu,
        std::uint64_t ppu_vram_revision,
        std::int32_t scroll_x, std::int32_t scroll_y);

    static constexpr std::uint32_t texture_width_ = 512U;
    static constexpr std::uint32_t texture_height_ = 256U;
    std::uint16_t *linear_pixels_{};
    std::uint16_t *linear_overlay_{};
    std::uint16_t *linear_right_{};
    C3D_Tex texture_{};
    C3D_Tex overlay_texture_{};
    C3D_Tex right_texture_{};
    C3D_Tex right_overlay_texture_{};
    C3D_Tex tile_atlas_{};
    std::array<std::int16_t, 8192U> tile_slots_{};
    std::array<std::int16_t, 8192U> tile_solid_colours_{};
    std::vector<TileQuad> tile_quads_{};
    std::vector<TileQuad> uniform_quads_{};
    std::vector<TileQuad> edge_corrections_{};
    std::vector<Bg2Rect> bg2_plan_{};
    std::vector<Bg2Rect> bg2_worker_plan_{};
    Thread bg2_plan_worker_{};
    LightEvent bg2_plan_requested_{};
    LightEvent bg2_plan_completed_{};
    std::atomic<bool> stop_bg2_plan_worker_{false};
    std::atomic<bool> bg2_plan_request_ready_{false};
    std::atomic<bool> bg2_plan_worker_completed_{false};
    const simulation::SnesPpuState* bg2_worker_ppu_{};
    std::uint64_t bg2_worker_vram_revision_{};
    std::int32_t bg2_worker_scroll_x_{}, bg2_worker_scroll_y_{};
    std::uint32_t bg2_worker_plan_us_{};
    bool bg2_worker_plan_succeeded_{};
    bool bg2_plan_pending_{};
    std::uint16_t tile_slot_count_{};
    Bg2PlanKey cached_tile_plan_key_{};
    std::uint64_t cached_tile_vram_revision_{};
    render::Palette256 cached_tile_palette_{};
    std::int32_t cached_tile_scroll_x_{};
    std::int32_t cached_tile_scroll_y_{};
    bool tile_cache_valid_{};
    std::array<std::uint16_t, 256U> cached_frame_colours_{};
    std::array<std::uint16_t, 256U> cached_cpu_colours_{};
    std::array<std::uint16_t, 256U> cached_overlay_colours_{};
    std::uint16_t cached_frame_width_{}, cached_frame_height_{};
    std::uint16_t cached_cpu_width_{}, cached_cpu_height_{};
    std::uint32_t cached_frame_draw_scale_{}, cached_cpu_draw_scale_{};
    bool cached_frame_wide_{}, cached_cpu_wide_{};
    bool frame_texture_valid_{};
    bool cpu_frame_valid_{};
    bool overlay_texture_valid_{};
    bool right_texture_valid_{}, right_overlay_valid_{}, cached_frame_stereo_{};
    bool cached_overlay_deferred_{};
    std::array<std::uint16_t, 256U> cached_right_colours_{};
    std::array<std::uint16_t, 256U> cached_right_overlay_colours_{};
    C3D_RenderTarget *top_{};
    C3D_RenderTarget *top_right_{};
    bool c3d_ready_{};
    bool c2d_ready_{};
    bool gpu_active_{};
    bool front_buffer_valid_{};
    bool wide_{true};
    DisplayFrameTiming frame_timing_{};
    const u8* last_top_framebuffer_{};
    const u8* last_bottom_framebuffer_{};
};

} // namespace starfox::platform_3ds
