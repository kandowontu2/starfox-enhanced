// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#include "display_3ds.hpp"
#include "stereo_3ds.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace starfox::platform_3ds {
namespace {

// All callers hold an acquired PICA frame. Citro3D's SyncDisplayTransfer
// splits with flags=0; explicitly flush every split command list now that
// frame end flushes only the used Citro2D buffers, rather than the whole heap.
void queue_display_transfer(u32* source, u32 source_size, u32* destination,
    u32 destination_size, u32 flags) {
    C3D_FrameSplit(GX_CMDLIST_FLUSH);
    GX_DisplayTransfer(source, source_size, destination, destination_size, flags);
}

std::uint32_t ticks_to_microseconds(std::uint64_t ticks) noexcept {
    return static_cast<std::uint32_t>(ticks * 1'000'000ULL
        / SYSCLOCK_ARM11);
}

std::uint16_t rgb565(const render::Rgba8& colour) noexcept {
    return static_cast<std::uint16_t>(
        ((static_cast<std::uint16_t>(colour.r) >> 3U) << 11U)
        | ((static_cast<std::uint16_t>(colour.g) >> 2U) << 5U)
        | (static_cast<std::uint16_t>(colour.b) >> 3U));
}

std::uint16_t rgba5551(const render::Rgba8& colour,
    bool opaque = true) noexcept {
    return static_cast<std::uint16_t>(
        ((static_cast<std::uint16_t>(colour.r) >> 3U) << 11U)
        | ((static_cast<std::uint16_t>(colour.g) >> 3U) << 6U)
        | ((static_cast<std::uint16_t>(colour.b) >> 3U) << 1U)
        | (opaque ? 1U : 0U));
}

constexpr std::array<std::uint8_t, 64> tile_morton{{
     0,  1,  4,  5, 16, 17, 20, 21,
     2,  3,  6,  7, 18, 19, 22, 23,
     8,  9, 12, 13, 24, 25, 28, 29,
    10, 11, 14, 15, 26, 27, 30, 31,
    32, 33, 36, 37, 48, 49, 52, 53,
    34, 35, 38, 39, 50, 51, 54, 55,
    40, 41, 44, 45, 56, 57, 60, 61,
    42, 43, 46, 47, 58, 59, 62, 63,
}};

// The diagnostic screen uses an integer 5x7 raster font. Each row stores
// five pixels in its low bits; the table covers exactly its uppercase UI.
struct DiagnosticGlyph {
    char character;
    std::array<std::uint8_t, 7U> rows;
};

constexpr DiagnosticGlyph diagnostic_font[]{
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}},
    {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},
    {'J', {1, 1, 1, 1, 17, 17, 14}},
    {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'+', {0, 4, 4, 31, 4, 4, 0}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
    {'.', {0, 0, 0, 0, 0, 6, 6}},
    {'/', {1, 2, 4, 8, 16, 0, 0}},
    {':', {0, 4, 4, 0, 4, 4, 0}},
    {'%', {17, 2, 4, 8, 17, 0, 0}},
};

} // namespace

Display3ds::~Display3ds() { close(); }

void Display3ds::bg2_plan_worker_main(void* argument) {
    auto& self = *static_cast<Display3ds*>(argument);
    for (;;) {
        LightEvent_Wait(&self.bg2_plan_requested_);
        if (self.stop_bg2_plan_worker_.load(std::memory_order_acquire)) break;
        while (!self.bg2_plan_request_ready_.load(std::memory_order_acquire)) {}
        self.bg2_plan_request_ready_.store(false, std::memory_order_relaxed);
        const auto begin = svcGetSystemTick();
        self.bg2_worker_plan_succeeded_ = self.bg2_worker_ppu_ != nullptr
            && plan_bg2_rects(*self.bg2_worker_ppu_,
                self.bg2_worker_scroll_x_, self.bg2_worker_scroll_y_,
                self.bg2_worker_plan_, 16383U, 400U, 240U, 72, 8);
        self.bg2_worker_plan_us_ = ticks_to_microseconds(
            svcGetSystemTick() - begin);
        self.bg2_plan_worker_completed_.store(true,
            std::memory_order_release);
        LightEvent_Signal(&self.bg2_plan_completed_);
    }
}

void Display3ds::wait_for_bg2_plan() noexcept {
    if (!bg2_plan_pending_) return;
    // Always consume the one-shot event, even if the completion flag is
    // already visible, so a stale signal cannot complete the next request.
    LightEvent_Wait(&bg2_plan_completed_);
    while (!bg2_plan_worker_completed_.load(std::memory_order_acquire)) {}
    bg2_plan_pending_ = false;
}

bool Display3ds::consume_bg2_plan(
    const simulation::SnesPpuState& ppu,
    std::uint64_t ppu_vram_revision,
    std::int32_t scroll_x, std::int32_t scroll_y) {
    if (!bg2_plan_pending_) return false;
    const auto wait_begin = svcGetSystemTick();
    wait_for_bg2_plan();
    frame_timing_.plan_wait_us += ticks_to_microseconds(
        svcGetSystemTick() - wait_begin);
    const bool matches = bg2_worker_ppu_ == &ppu
        && bg2_worker_vram_revision_ == ppu_vram_revision
        && bg2_worker_scroll_x_ == scroll_x
        && bg2_worker_scroll_y_ == scroll_y;
    if (!matches) return false;
    frame_timing_.bg2_plan_worker_used = true;
    frame_timing_.plan_us = bg2_worker_plan_us_;
    if (bg2_worker_plan_succeeded_) bg2_plan_.swap(bg2_worker_plan_);
    return bg2_worker_plan_succeeded_;
}

void Display3ds::prepare_mode2_plan(
    const simulation::SnesPpuState& ppu,
    std::uint64_t ppu_vram_revision,
    std::int32_t scroll_x, std::int32_t scroll_y,
    bool core2_available) {
    if (bg2_plan_pending_) wait_for_bg2_plan();
    if (bg2_plan_worker_ == nullptr || !core2_available
        || !eligible_mode2(ppu)) return;
    const bool reuse_plan = tile_cache_valid_
        && cached_tile_scroll_x_ == scroll_x
        && cached_tile_scroll_y_ == scroll_y
        && cached_tile_vram_revision_ == ppu_vram_revision
        && same_bg2_plan_state(ppu);
    if (reuse_plan) return;

    bg2_worker_ppu_ = &ppu;
    bg2_worker_vram_revision_ = ppu_vram_revision;
    bg2_worker_scroll_x_ = scroll_x;
    bg2_worker_scroll_y_ = scroll_y;
    bg2_plan_worker_completed_.store(false, std::memory_order_relaxed);
    bg2_plan_request_ready_.store(true, std::memory_order_release);
    bg2_plan_pending_ = true;
    LightEvent_Signal(&bg2_plan_requested_);
}

bool Display3ds::open() {
    close();
    gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
    // Bottom telemetry is written directly at native 320x240 pixel positions.
    gfxSetScreenFormat(GFX_BOTTOM, GSP_RGBA8_OES);
    // Mono submissions still duplicate the left buffer through libctru.
    // Only paired gameplay submissions address the right render target.
    gfxSet3D(true);
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
    c3d_ready_ = true;
    // Per-line HDMA on the 400x240 native raster can approach 16k fragments.
    if (!C2D_Init(16384U)) {
        close();
        return false;
    }
    c2d_ready_ = true;
    C2D_Prepare();
    linear_pixels_ = static_cast<std::uint16_t*>(
        linearAlloc(texture_width_ * texture_height_ * sizeof(std::uint16_t)));
    linear_overlay_ = static_cast<std::uint16_t*>(
        linearAlloc(texture_width_ * texture_height_ * sizeof(std::uint16_t)));
    linear_right_ = static_cast<std::uint16_t*>(
        linearAlloc(texture_width_ * texture_height_ * sizeof(std::uint16_t)));
    if (linear_pixels_ == nullptr
        || linear_overlay_ == nullptr
        || linear_right_ == nullptr
        || !C3D_TexInitVRAM(&texture_, texture_width_,
            texture_height_, GPU_RGB565)
        || !C3D_TexInitVRAM(&overlay_texture_, texture_width_,
            texture_height_, GPU_RGBA5551)
        || !C3D_TexInitVRAM(&right_texture_, texture_width_,
            texture_height_, GPU_RGB565)
        || !C3D_TexInitVRAM(&right_overlay_texture_, texture_width_,
            texture_height_, GPU_RGBA5551)
        || !C3D_TexInit(&tile_atlas_, 1024U, 512U, GPU_RGBA5551)) {
        close();
        return false;
    }
    std::memset(linear_pixels_, 0,
        texture_width_ * texture_height_ * sizeof(std::uint16_t));
    std::memset(linear_right_, 0,
        texture_width_ * texture_height_ * sizeof(std::uint16_t));
    C3D_TexSetFilter(&texture_, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&texture_, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    C3D_TexSetFilter(&overlay_texture_, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&overlay_texture_, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    C3D_TexSetFilter(&right_texture_, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&right_texture_, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    C3D_TexSetFilter(&right_overlay_texture_, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&right_overlay_texture_, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    C3D_TexSetFilter(&tile_atlas_, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&tile_atlas_, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    tile_quads_.reserve(16384U);
    uniform_quads_.reserve(4096U);
    edge_corrections_.reserve(512U);
    bg2_plan_.reserve(16384U);
    bg2_worker_plan_.reserve(16384U);
    top_ = C3D_RenderTargetCreate(
        GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP,
        GPU_RB_RGBA8, GPU_RB_DEPTH16);
    top_right_ = C3D_RenderTargetCreate(
        GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP,
        GPU_RB_RGBA8, GPU_RB_DEPTH16);
    if (top_ == nullptr || top_right_ == nullptr) {
        close();
        return false;
    }
    const auto output = GX_TRANSFER_FLIP_VERT(0)
        | GX_TRANSFER_OUT_TILED(0)
        | GX_TRANSFER_RAW_COPY(0)
        | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)
        | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565)
        | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
    C3D_RenderTargetSetOutput(top_, GFX_TOP, GFX_LEFT, output);
    C3D_RenderTargetSetOutput(top_right_, GFX_TOP, GFX_RIGHT, output);
    gpu_active_ = true;
    LightEvent_Init(&bg2_plan_requested_, RESET_ONESHOT);
    LightEvent_Init(&bg2_plan_completed_, RESET_ONESHOT);
    stop_bg2_plan_worker_.store(false, std::memory_order_release);
    bg2_plan_worker_completed_.store(false, std::memory_order_release);
    bool new_3ds = false;
    if (R_SUCCEEDED(APT_CheckNew3DS(&new_3ds)) && new_3ds) {
        // Audio uses core 2 at priority 0x30. This lower-priority worker
        // gets only idle slices while the main core builds the foreground.
        bg2_plan_worker_ = threadCreate(&Display3ds::bg2_plan_worker_main,
            this, 128U * 1024U, 0x31, 2, false);
    }
    return true;
}

void Display3ds::close() noexcept {
    if (bg2_plan_pending_) wait_for_bg2_plan();
    if (bg2_plan_worker_ != nullptr) {
        stop_bg2_plan_worker_.store(true, std::memory_order_release);
        LightEvent_Signal(&bg2_plan_requested_);
        static_cast<void>(threadJoin(bg2_plan_worker_, U64_MAX));
        threadFree(bg2_plan_worker_);
        bg2_plan_worker_ = nullptr;
    }
    gpu_active_ = false;
    // Drain before C2D frees VBOs, and park its borrowed shader while C3D
    // remains active. The next renderer's bind releases just that shader.
    if (c3d_ready_) C3D_FrameSync();
    if (c2d_ready_) C2D_Fini();
    c2d_ready_ = false;
    if (c3d_ready_) C3D_Fini();
    c3d_ready_ = false;
    // C3D_Fini owns render-target deletion.
    top_ = nullptr;
    top_right_ = nullptr;
    if (texture_.data != nullptr) C3D_TexDelete(&texture_);
    texture_ = {};
    if (overlay_texture_.data != nullptr) C3D_TexDelete(&overlay_texture_);
    overlay_texture_ = {};
    if (right_texture_.data != nullptr) C3D_TexDelete(&right_texture_);
    right_texture_ = {};
    if (right_overlay_texture_.data != nullptr) C3D_TexDelete(&right_overlay_texture_);
    right_overlay_texture_ = {};
    if (tile_atlas_.data != nullptr) C3D_TexDelete(&tile_atlas_);
    tile_atlas_ = {};
    if (linear_pixels_ != nullptr) linearFree(linear_pixels_);
    linear_pixels_ = nullptr;
    if (linear_overlay_ != nullptr) linearFree(linear_overlay_);
    linear_overlay_ = nullptr;
    if (linear_right_ != nullptr) linearFree(linear_right_);
    linear_right_ = nullptr;
    tile_quads_.clear();
    tile_slot_count_ = 0U;
    tile_cache_valid_ = false;
    frame_texture_valid_ = false;
    cpu_frame_valid_ = false;
    overlay_texture_valid_ = false;
    right_texture_valid_ = right_overlay_valid_ = false;
    c2d_ready_ = c3d_ready_ = false;
    gfxSet3D(false);
}

bool Display3ds::show_full_dump_progress(const char* stage,
    std::uint64_t written, std::uint64_t total) {
    u16 physical_width = 0, physical_height = 0;
    auto* framebuffer = reinterpret_cast<std::uint32_t*>(
        gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT,
            &physical_width, &physical_height));
    if (!framebuffer || physical_width != 240U || physical_height != 320U)
        return false;
    const auto rgba = [](unsigned r, unsigned g, unsigned b) {
        return static_cast<std::uint32_t>((r << 24U) | (g << 16U)
            | (b << 8U) | 255U);
    };
    const auto ink = rgba(41U, 56U, 74U);
    const auto blue = rgba(139U, 224U, 231U);
    const auto ivory = rgba(239U, 247U, 222U);
    const auto dark = rgba(19U, 27U, 45U);
    std::fill_n(framebuffer, 320U * 240U, ink);
    auto rect = [&](int x, int y, int width, int height, std::uint32_t color) {
        const int left = std::clamp(x, 0, 320);
        const int right = std::clamp(x + width, 0, 320);
        const int top = std::clamp(y, 0, 240);
        const int bottom = std::clamp(y + height, 0, 240);
        for (int px = left; px < right; ++px)
            for (int py = top; py < bottom; ++py)
                framebuffer[static_cast<std::size_t>(px) * 240U
                    + 239U - static_cast<unsigned>(py)] = color;
    };
    auto centered = [&](const char* label, int y, int scale, std::uint32_t color) {
        const int length = label ? static_cast<int>(std::strlen(label)) : 0;
        int x = (320 - (length * 6 - (length ? 1 : 0)) * scale) / 2;
        if (!label) return;
        for (const char* cursor = label; *cursor; ++cursor, x += 6 * scale) {
            const DiagnosticGlyph* glyph = nullptr;
            for (const auto& candidate : diagnostic_font)
                if (candidate.character == *cursor) { glyph = &candidate; break; }
            if (!glyph) continue;
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if (glyph->rows[row] & (1U << (4 - column)))
                        rect(x + column * scale, y + row * scale,
                            scale, scale, color);
        }
    };
    const unsigned percent = total ? static_cast<unsigned>(
        std::min<std::uint64_t>(100U, written * 100U / total)) : 0U;
    char amount[64]{};
    if (total) {
        std::snprintf(amount, sizeof(amount), "%u%%  %llu/%llu MB", percent,
            static_cast<unsigned long long>(written / (1024U * 1024U)),
            static_cast<unsigned long long>(
                (total + 1024U * 1024U - 1U) / (1024U * 1024U)));
    } else {
        std::snprintf(amount, sizeof(amount), "PLEASE WAIT");
    }
    const auto steel = rgba(98U, 139U, 145U);
    const auto shade = rgba(25U, 46U, 58U);
    auto panel = [&](int x, int y, int w, int h) {
        rect(x,y,w,h,shade);
        rect(x+1,y+1,w-2,h-2,steel);
        rect(x+2,y+2,w-4,1,ivory);
        rect(x+2,y+3,1,h-5,ivory);
        rect(x+4,y+4,w-8,h-8,dark);
        rect(x+w-3,y+3,1,h-5,shade);
        rect(x+3,y+h-3,w-6,1,shade);
    };
    panel(8,8,304,28);
    panel(8,43,304,133);
    panel(8,184,304,48);
    centered("FULL MEMORY DUMP", 18, 1, ivory);
    centered(stage ? stage : "PREPARING", 63, 1, blue);
    panel(23,91,274,25);
    rect(29,97,262,13,shade);
    rect(29,97,static_cast<int>(262U*percent/100U),13,steel);
    rect(29,97,static_cast<int>(262U*percent/100U),3,blue);
    centered(amount, 136, 2, ivory);
    centered("WRITING MEMORY", 195, 1, blue);
    centered("DO NOT POWER OFF", 215, 1, ivory);
    GSPGPU_FlushDataCache(framebuffer, 320U * 240U * 4U);
    last_bottom_framebuffer_ = reinterpret_cast<const u8*>(framebuffer);
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
    return true;
}

void Display3ds::upload_right(const render::Framebuffer& frame,
    const render::Palette256& palette, bool foreground, bool reuse_source) {
    auto& valid = foreground ? right_overlay_valid_ : right_texture_valid_;
    auto& cached = foreground ? cached_right_overlay_colours_ : cached_right_colours_;
    auto& texture = foreground ? right_overlay_texture_ : right_texture_;
    std::array<std::uint16_t, 256U> colours{};
    for (unsigned i = 0U; i < colours.size(); ++i)
        colours[i] = foreground ? rgba5551(palette[i]) : rgb565(palette[i]);
    if (reuse_source && valid && cached == colours) return;
    auto begin = svcGetSystemTick();
    const auto coverage = frame.write_coverage();
    const bool covered = foreground && coverage.size() == frame.pixels().size();
    for (unsigned y = 0U; y < frame.height(); ++y) {
        for (unsigned x = 0U; x < frame.width(); ++x) {
            const auto i = static_cast<std::size_t>(y) * frame.width() + x;
            const bool opaque = !foreground || (covered
                ? coverage[i] != 0U : frame.pixels()[i] != 0U);
            linear_right_[static_cast<std::size_t>(y) * texture_width_ + x]
                = opaque ? colours[frame.pixels()[i]] : 0U;
        }
    }
    frame_timing_.build_us += ticks_to_microseconds(svcGetSystemTick() - begin);
    begin = svcGetSystemTick();
    GSPGPU_FlushDataCache(linear_right_, texture_width_ * texture_height_
        * sizeof(std::uint16_t));
    frame_timing_.flush_us += ticks_to_microseconds(svcGetSystemTick() - begin);
    begin = svcGetSystemTick();
    const auto format = foreground ? GX_TRANSFER_FMT_RGB5A1 : GX_TRANSFER_FMT_RGB565;
    queue_display_transfer(reinterpret_cast<u32*>(linear_right_),
        GX_BUFFER_DIM(texture_width_, texture_height_),
        reinterpret_cast<u32*>(texture.data),
        GX_BUFFER_DIM(texture_width_, texture_height_),
        GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1)
            | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(format)
            | GX_TRANSFER_OUT_FORMAT(format) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    frame_timing_.transfer_us += ticks_to_microseconds(svcGetSystemTick() - begin);
    cached = colours;
    valid = true;
}

bool Display3ds::present(const render::Framebuffer& indexed,
    const render::Palette256& palette, bool reuse_source,
    const render::Framebuffer* right, bool wait_for_gpu) {
    frame_timing_ = {};
    if (!gpu_active_) {
        return present_cpu(indexed, palette, reuse_source);
    }
    const auto width = std::min<std::uint32_t>(indexed.width(), 400U);
    const auto height = std::min<std::uint32_t>(indexed.height(), 240U);
    std::array<std::uint16_t, 256U> colours{};
    for (std::uint32_t index = 0U; index < 256U; ++index)
        colours[index] = rgb565(palette[index]);
    if (reuse_source && frame_texture_valid_
        && width == cached_frame_width_
        && height == cached_frame_height_
        && indexed.draw_scale() == cached_frame_draw_scale_
        && wide_ == cached_frame_wide_
        && (right != nullptr) == cached_frame_stereo_
        && (right == nullptr || right_texture_valid_)
        && colours == cached_frame_colours_) {
        // The cached image is still the visible front buffer. Keeping it on
        // screen avoids a redundant PICA acquire, draw and submit.
        frame_timing_.reuse_hit = true;
        return true;
    }
    auto stage_begin = svcGetSystemTick();
    const bool frame_acquired = C3D_FrameBegin(wait_for_gpu ? 0 : C3D_FRAME_NONBLOCK);
    frame_timing_.acquire_us = ticks_to_microseconds(
        svcGetSystemTick() - stage_begin);
    if (!frame_acquired) {
        frame_timing_.gpu_begin_failed = true;
        frame_texture_valid_ = false;
        return false;
    }
    if (!reuse_source || !frame_texture_valid_
        || width != cached_frame_width_
        || height != cached_frame_height_
        || indexed.draw_scale() != cached_frame_draw_scale_
        || colours != cached_frame_colours_) {
        stage_begin = svcGetSystemTick();
        for (std::uint32_t y = 0U; y < height; ++y) {
            auto *row = linear_pixels_ + y * texture_width_;
            for (std::uint32_t x = 0U; x < width; ++x)
                row[x] = colours[indexed.get(x, y)];
        }
        frame_timing_.build_us = ticks_to_microseconds(
            svcGetSystemTick() - stage_begin);
        stage_begin = svcGetSystemTick();
        GSPGPU_FlushDataCache(linear_pixels_,
            texture_width_ * texture_height_ * sizeof(std::uint16_t));
        frame_timing_.flush_us = ticks_to_microseconds(
            svcGetSystemTick() - stage_begin);
        stage_begin = svcGetSystemTick();
        queue_display_transfer(
            reinterpret_cast<u32*>(linear_pixels_),
            GX_BUFFER_DIM(texture_width_, texture_height_),
            reinterpret_cast<u32*>(texture_.data),
            GX_BUFFER_DIM(texture_width_, texture_height_),
            GX_TRANSFER_FLIP_VERT(0)
              | GX_TRANSFER_OUT_TILED(1)
              | GX_TRANSFER_RAW_COPY(0)
              | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565)
              | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565)
              | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
        frame_timing_.transfer_us = ticks_to_microseconds(
            svcGetSystemTick() - stage_begin);
        cached_frame_colours_ = colours;
        frame_texture_valid_ = true;
    }
    if (right != nullptr) upload_right(*right, palette, false, reuse_source);
    stage_begin = svcGetSystemTick();
    C2D_TargetClear(top_, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(top_);
    const Tex3DS_SubTexture subtexture{
        static_cast<u16>(width), static_cast<u16>(height),
        0.0F, 1.0F, static_cast<float>(width) / texture_width_,
        1.0F - static_cast<float>(height) / texture_height_};
    const C2D_Image image{&texture_, &subtexture};
    const auto output_width = wide_ ? static_cast<float>(width) : 274.0F;
    const auto left = (400.0F - output_width) / 2.0F;
    const auto top = static_cast<float>((240U - height) / 2U);
    const C2D_DrawParams params{
        {left, top, output_width, static_cast<float>(height)},
        {0.0F, 0.0F}, 0.0F, 0.0F};
    C2D_DrawImage(image, &params, nullptr);
    if (right != nullptr) {
        C2D_TargetClear(top_right_, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top_right_);
        const C2D_Image right_image{&right_texture_, &subtexture};
        C2D_DrawImage(right_image, &params, nullptr);
    }
    last_top_framebuffer_ = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr);
    // Our pinned Citro2D hook flushes the complete used vertex/index ranges.
    // Every split also flushes its command list before PICA consumes it.
    C3D_FrameEnd(GX_CMDLIST_FLUSH);
    frame_timing_.gpu_submitted = true;
    cached_frame_width_ = static_cast<std::uint16_t>(width);
    cached_frame_height_ = static_cast<std::uint16_t>(height);
    cached_frame_draw_scale_ = indexed.draw_scale();
    cached_frame_wide_ = wide_;
    cached_frame_stereo_ = right != nullptr;
    frame_timing_.submit_us = ticks_to_microseconds(
        svcGetSystemTick() - stage_begin);
    return true;
}

bool Display3ds::present_black() {
    if (gpu_active_) {
        if (!C3D_FrameBegin(0)) return false;
        C2D_TargetClear(top_, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top_);
        last_top_framebuffer_ = gfxGetFramebuffer(
            GFX_TOP, GFX_LEFT, nullptr, nullptr);
        C3D_FrameEnd(GX_CMDLIST_FLUSH);
        frame_texture_valid_ = false;
        cpu_frame_valid_ = false;
        return true;
    }
    u16 width{}, height{};
    auto* screen = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
    if (screen == nullptr || width != 240U || height != 400U)
        return false;
    std::memset(screen, 0, 400U * 240U * sizeof(std::uint16_t));
    last_top_framebuffer_ = screen;
    gfxFlushBuffers();
    gfxSwapBuffers();
    frame_texture_valid_ = false;
    cpu_frame_valid_ = false;
    return true;
}

bool Display3ds::eligible_mode2(
    const simulation::SnesPpuState& ppu) const noexcept {
    // The planner splits horizontal and vertical HDMA at exact scanline
    // boundaries. Mosaic is retained on the software path.
    return gpu_active_ && ppu.background_mode == 2U
        && (ppu.mosaic & 0x02U) == 0U;
}

std::int16_t Display3ds::decode_tile(
    const simulation::SnesPpuState& ppu,
    const render::Palette256& palette,
    std::uint16_t character, std::uint8_t palette_bank) {
    character &= 0x03ffU;
    const auto key = static_cast<std::size_t>(character) * 8U
        + (palette_bank & 7U);
    if (tile_slots_[key] >= 0) return tile_slots_[key];
    if (tile_slot_count_ >= 8192U) return -1;
    const auto slot = tile_slot_count_++;
    tile_slots_[key] = static_cast<std::int16_t>(slot);
    auto* destination = static_cast<std::uint16_t*>(tile_atlas_.data)
        + static_cast<std::size_t>(slot) * 64U;
    const auto base = (static_cast<std::uint32_t>(ppu.bg2_character_base) * 2U
        + static_cast<std::uint32_t>(character) * 32U) & 0xffffU;
    std::array<std::uint16_t, 16U> colours{};
    for (std::uint32_t colour = 1U; colour < 16U; ++colour) {
        colours[colour] = rgba5551(palette[
            static_cast<std::size_t>(palette_bank) * 16U + colour]);
    }
    int uniform_colour = -2;
    for (std::uint32_t y = 0U; y < 8U; ++y) {
        const auto low = ppu.vram[(base + y * 2U) & 0xffffU];
        const auto high = ppu.vram[(base + y * 2U + 1U) & 0xffffU];
        const auto low2 = ppu.vram[(base + 16U + y * 2U) & 0xffffU];
        const auto high2 = ppu.vram[(base + 17U + y * 2U) & 0xffffU];
        for (std::uint32_t x = 0U; x < 8U; ++x) {
            const auto mask = static_cast<std::uint8_t>(0x80U >> x);
            const auto colour = static_cast<std::uint8_t>(
                ((low & mask) != 0U ? 1U : 0U)
                | ((high & mask) != 0U ? 2U : 0U)
                | ((low2 & mask) != 0U ? 4U : 0U)
                | ((high2 & mask) != 0U ? 8U : 0U));
            if (uniform_colour == -2) uniform_colour = colour;
            else if (uniform_colour != colour) uniform_colour = -1;
            destination[tile_morton[y * 8U + x]] = colours[colour];
        }
    }
    tile_solid_colours_[slot] = uniform_colour >= 0
        ? static_cast<std::int16_t>(uniform_colour == 0 ? 0U
            : static_cast<unsigned>(palette_bank & 7U) * 16U
                + static_cast<unsigned>(uniform_colour))
        : static_cast<std::int16_t>(-1);
    return static_cast<std::int16_t>(slot);
}

bool Display3ds::build_mode2_quads(
    const simulation::SnesPpuState& ppu,
    std::uint64_t ppu_vram_revision,
    const render::Palette256& palette) {
    tile_quads_.clear();
    auto& uniform_quads = uniform_quads_;
    auto& edge_corrections = edge_corrections_;
    uniform_quads.clear();
    edge_corrections.clear();
    // The caller acquires the GPU frame before this function can mutate the
    // atlas. Geometric planning (and its failure path) happen before acquire.
    // Scrolling only changes geometry. Preserve expanded characters while
    // tile bytes and palette stay identical, including across HDMA bands.
    const bool reuse_atlas = tile_cache_valid_
        && cached_tile_plan_key_.bg2_character_base == ppu.bg2_character_base
        && cached_tile_vram_revision_ == ppu_vram_revision
        && std::memcmp(cached_tile_palette_.data(), palette.data(),
            sizeof(cached_tile_palette_)) == 0;
    if (!reuse_atlas) {
        tile_slots_.fill(-1);
        tile_slot_count_ = 0U;
    }
    for (const auto& rectangle : bg2_plan_) {
        if (rectangle.solid) {
            edge_corrections.push_back({rectangle.x, rectangle.y,
                rectangle.width, rectangle.height, 0U, 0U, 0U,
                false, false, true, rectangle.solid_colour});
            continue;
        }
        const auto slot = decode_tile(ppu, palette,
            rectangle.character, rectangle.palette_bank);
        if (slot < 0) { tile_quads_.clear(); return false; }
        if (const auto colour = tile_solid_colours_[slot]; colour >= 0) {
            uniform_quads.push_back({rectangle.x, rectangle.y,
                rectangle.width, rectangle.height, 0U, 0U, 0U,
                false, false, true,
                static_cast<std::uint8_t>(colour)});
            continue;
        }
        tile_quads_.push_back({rectangle.x, rectangle.y,
            rectangle.width, rectangle.height, static_cast<std::uint16_t>(slot),
            rectangle.source_x, rectangle.source_y,
            rectangle.reverse_x, rectangle.reverse_y, false, 0U});
    }
    // BG2's 1-pixel vertical-roll strips often sample a flat sky or floor
    // tile. They cover disjoint pixels, so merge equal horizontal spans into
    // one solid GPU draw while preserving the final lower-edge corrections.
    std::sort(uniform_quads.begin(), uniform_quads.end(),
        [](const TileQuad& a, const TileQuad& b) {
            if (a.y != b.y) return a.y < b.y;
            if (a.height != b.height) return a.height < b.height;
            if (a.solid_colour != b.solid_colour)
                return a.solid_colour < b.solid_colour;
            return a.x < b.x;
        });
    for (const auto& quad : uniform_quads) {
        if (!tile_quads_.empty()) {
            auto& last = tile_quads_.back();
            if (last.solid && last.y == quad.y
                && last.height == quad.height
                && last.solid_colour == quad.solid_colour
                && static_cast<unsigned>(last.x) + last.width == quad.x) {
                last.width = static_cast<std::uint16_t>(last.width + quad.width);
                continue;
            }
        }
        tile_quads_.push_back(quad);
    }
    tile_quads_.insert(tile_quads_.end(), edge_corrections.begin(),
        edge_corrections.end());
    return true;
}

Display3ds::Bg2PlanKey Display3ds::make_bg2_plan_key(
    const simulation::SnesPpuState& ppu) noexcept {
    // The planner only observes BG2's screen-enable and mosaic-enable bits.
    // Other layer enables and the mosaic size do not alter its rectangles.
    return {ppu.background_mode,
        static_cast<std::uint8_t>(ppu.mosaic & 0x02U),
        static_cast<std::uint8_t>(ppu.main_screen & 0x02U),
        ppu.bg2_screen_size, ppu.bg2_tile_size_16, ppu.bg2_character_base,
        ppu.bg2_screen_base, ppu.bg2_vertical_offsets_enabled,
        ppu.bg2_horizontal_offsets, ppu.bg2_horizontal_offsets_enabled,
        ppu.bg2_scanline_scroll_y, ppu.bg2_scanline_scroll_enabled};
}

bool Display3ds::same_bg2_plan_state(
    const simulation::SnesPpuState& ppu) const noexcept {
    return cached_tile_plan_key_.background_mode == ppu.background_mode
        && cached_tile_plan_key_.mosaic == (ppu.mosaic & 0x02U)
        && cached_tile_plan_key_.main_screen == (ppu.main_screen & 0x02U)
        && cached_tile_plan_key_.bg2_screen_size == ppu.bg2_screen_size
        && cached_tile_plan_key_.bg2_tile_size_16 == ppu.bg2_tile_size_16
        && cached_tile_plan_key_.bg2_character_base == ppu.bg2_character_base
        && cached_tile_plan_key_.bg2_screen_base == ppu.bg2_screen_base
        && cached_tile_plan_key_.bg2_vertical_offsets_enabled
            == ppu.bg2_vertical_offsets_enabled
        && cached_tile_plan_key_.bg2_horizontal_offsets_enabled
            == ppu.bg2_horizontal_offsets_enabled
        && cached_tile_plan_key_.bg2_scanline_scroll_enabled
            == ppu.bg2_scanline_scroll_enabled
        && (!ppu.bg2_horizontal_offsets_enabled
            || cached_tile_plan_key_.bg2_horizontal_offsets
                == ppu.bg2_horizontal_offsets)
        && (!ppu.bg2_scanline_scroll_enabled
            || cached_tile_plan_key_.bg2_scanline_scroll_y
                == ppu.bg2_scanline_scroll_y);
}

bool Display3ds::present_mode2(
    const simulation::SnesPpuState& ppu,
    std::uint64_t ppu_vram_revision,
    std::int32_t scroll_x, std::int32_t scroll_y,
    const render::Framebuffer& foreground,
    const render::Palette256& palette, bool reuse_source,
    const render::Framebuffer* right, bool wait_for_gpu) {
    frame_timing_ = {};
    if (!eligible_mode2(ppu) || foreground.width() != 400U
        || foreground.height() != 240U || foreground.draw_scale() != 1U) {
        wait_for_bg2_plan();
        return false;
    }

    const auto cache_validation_begin = svcGetSystemTick();
    // The BG2 rectangle plan contains tile/colour indices, not RGB values.
    // A palette-only change requires atlas and quad regeneration, but it
    // must not repeat the 400x240 CPU geometry/ground scan.
    const bool reuse_plan = tile_cache_valid_
        && cached_tile_scroll_x_ == scroll_x
        && cached_tile_scroll_y_ == scroll_y
        && cached_tile_vram_revision_ == ppu_vram_revision
        && same_bg2_plan_state(ppu);
    const bool reuse_tiles = reuse_plan
        && std::memcmp(cached_tile_palette_.data(), palette.data(),
            sizeof(cached_tile_palette_)) == 0;
    if (right != nullptr && reuse_tiles
        && !stereo_bg2_quads_fit(tile_quads_.size())) return false;
    frame_timing_.cache_validation_us = ticks_to_microseconds(
        svcGetSystemTick() - cache_validation_begin);
    frame_timing_.bg2_plan_checked = true;
    frame_timing_.plan_cache_hit = reuse_plan;
    if (!reuse_plan) {
        // Planning is CPU-only. Reject pathological HDMA fragment counts
        // before acquiring a GPU frame so the software fallback can submit
        // its complete image in this same loop.
        // Preserve the prior rendered geometry until acquisition succeeds:
        // a nonblocking miss must leave its old cache key and quads paired.
        bool planned = consume_bg2_plan(ppu, ppu_vram_revision,
            scroll_x, scroll_y);
        if (!frame_timing_.bg2_plan_worker_used) {
            const auto plan_begin = svcGetSystemTick();
            planned = plan_bg2_rects(ppu, scroll_x, scroll_y, bg2_plan_,
                    16383U, 400U, 240U, 72, 8);
            frame_timing_.plan_us += ticks_to_microseconds(
                svcGetSystemTick() - plan_begin);
        }
        if (!planned) {
            tile_cache_valid_ = false;
            overlay_texture_valid_ = false;
            return false;
        }
    }

    auto stage_begin = svcGetSystemTick();
    // A mono retry must wait for queue completion, not a whole extra VBlank.
    // FrameBegin(0) joins the command queue before atlas/texture writes below;
    // C3D_FrameSync alone only waits for display cadence, not GPU retirement.
    const bool frame_acquired = C3D_FrameBegin(wait_for_gpu ? 0 : C3D_FRAME_NONBLOCK);
    frame_timing_.acquire_us = ticks_to_microseconds(
        svcGetSystemTick() - stage_begin);
    if (!frame_acquired) {
        frame_timing_.gpu_begin_failed = true;
        // Planning writes into the working vector before GPU acquisition.
        // If that new plan was not committed, force a fresh plan next time.
        if (!reuse_plan) tile_cache_valid_ = false;
        overlay_texture_valid_ = false;
        return false;
    }
    if (!reuse_tiles) {
        // FrameBegin's GPU-idle acquire protects atlas writes from the
        // preceding submitted frame, which may still have sampled it.
        stage_begin = svcGetSystemTick();
        const bool built = build_mode2_quads(
            ppu, ppu_vram_revision, palette);
        frame_timing_.build_us += ticks_to_microseconds(
            svcGetSystemTick() - stage_begin);
        if (!built) {
            tile_cache_valid_ = false;
            overlay_texture_valid_ = false;
            C3D_FrameEnd(GX_CMDLIST_FLUSH);
            frame_timing_.gpu_submitted = true;
            return false;
        }
        // Slots are packed from zero each rebuild. Flush the used prefix;
        // flushing the entire 1 MiB atlas is unnecessary on Old 3DS.
        if (tile_slot_count_ != 0U) {
            const auto bytes = static_cast<std::size_t>(tile_slot_count_)
                * 64U * sizeof(std::uint16_t);
            stage_begin = svcGetSystemTick();
            if (R_FAILED(GSPGPU_FlushDataCache(tile_atlas_.data, bytes)))
                C3D_TexFlush(&tile_atlas_);
            frame_timing_.flush_us += ticks_to_microseconds(
                svcGetSystemTick() - stage_begin);
        }
        cached_tile_plan_key_ = make_bg2_plan_key(ppu);
        cached_tile_vram_revision_ = ppu_vram_revision;
        cached_tile_palette_ = palette;
        cached_tile_scroll_x_ = scroll_x;
        cached_tile_scroll_y_ = scroll_y;
        tile_cache_valid_ = true;
    }

    if (right != nullptr && !stereo_bg2_quads_fit(tile_quads_.size())) {
        C3D_FrameEnd(GX_CMDLIST_FLUSH);
        frame_timing_.gpu_submitted = true;
        return false;
    }

    std::array<std::uint16_t, 256U> colours{};
    for (std::uint32_t index = 0U; index < 256U; ++index)
        colours[index] = rgba5551(palette[index]);
    if (!reuse_source || !overlay_texture_valid_
        || colours != cached_overlay_colours_) {
        stage_begin = svcGetSystemTick();
        const auto coverage = foreground.write_coverage();
        const auto has_coverage = coverage.size() == foreground.pixels().size();
        const auto& pixels = foreground.pixels();
        for (std::uint32_t y = 0U; y < 240U; ++y) {
            for (std::uint32_t x = 0U; x < 400U; ++x) {
                const auto index = static_cast<std::size_t>(y) * 400U + x;
                linear_overlay_[static_cast<std::size_t>(y)
                    * texture_width_ + x] = has_coverage
                    ? (coverage[index] != 0U ? colours[pixels[index]] : 0U)
                    : (pixels[index] != 0U ? colours[pixels[index]] : 0U);
            }
        }
        frame_timing_.build_us += ticks_to_microseconds(
            svcGetSystemTick() - stage_begin);
        stage_begin = svcGetSystemTick();
        GSPGPU_FlushDataCache(linear_overlay_,
            texture_width_ * texture_height_ * sizeof(std::uint16_t));
        frame_timing_.flush_us += ticks_to_microseconds(
            svcGetSystemTick() - stage_begin);
        stage_begin = svcGetSystemTick();
        queue_display_transfer(
            reinterpret_cast<u32*>(linear_overlay_),
            GX_BUFFER_DIM(texture_width_, texture_height_),
            reinterpret_cast<u32*>(overlay_texture_.data),
            GX_BUFFER_DIM(texture_width_, texture_height_),
            GX_TRANSFER_FLIP_VERT(0)
              | GX_TRANSFER_OUT_TILED(1)
              | GX_TRANSFER_RAW_COPY(0)
              | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB5A1)
              | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB5A1)
              | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
        frame_timing_.transfer_us += ticks_to_microseconds(
            svcGetSystemTick() - stage_begin);
        cached_overlay_colours_ = colours;
        overlay_texture_valid_ = true;
    }

    if (right != nullptr) upload_right(*right, palette, true, reuse_source);
    const auto draw_eye = [&](C3D_RenderTarget* target, C3D_Tex* overlay) {
    C2D_TargetClear(target, C2D_Color32(
        palette[0].r, palette[0].g, palette[0].b, 255));
    C2D_SceneBegin(target);
    stage_begin = svcGetSystemTick();
    for (const auto& quad : tile_quads_) {
        if (quad.solid) {
            const auto colour = palette[quad.solid_colour];
            if (!C2D_DrawRectSolid(static_cast<float>(quad.x),
                static_cast<float>(quad.y), 0.0F,
                static_cast<float>(quad.width),
                static_cast<float>(quad.height),
                C2D_Color32(colour.r, colour.g, colour.b, 255U))) {
                return false;
            }
            continue;
        }
        const auto slot_x = static_cast<std::uint32_t>(quad.slot % 128U) * 8U;
        const auto slot_y = static_cast<std::uint32_t>(quad.slot / 128U) * 8U;
        const auto uv_x = slot_x + quad.first_x
            + (quad.reverse_x ? 1U : 0U);
        const auto uv_y = slot_y + quad.first_y
            + (quad.reverse_y ? 1U : 0U);
        const Tex3DS_SubTexture subtexture{
            quad.width, quad.height,
            static_cast<float>(uv_x) / 1024.0F,
            1.0F - static_cast<float>(uv_y) / 512.0F,
            static_cast<float>(static_cast<std::int32_t>(uv_x)
                + (quad.reverse_x ? -static_cast<std::int32_t>(quad.width)
                    : quad.width)) / 1024.0F,
            1.0F - static_cast<float>(static_cast<std::int32_t>(uv_y)
                + (quad.reverse_y ? -static_cast<std::int32_t>(quad.height)
                    : quad.height)) / 512.0F};
        const C2D_Image image{&tile_atlas_, &subtexture};
        const C2D_DrawParams params{
            {static_cast<float>(quad.x), static_cast<float>(quad.y),
                static_cast<float>(quad.width), static_cast<float>(quad.height)},
            {0.0F, 0.0F}, 0.0F, 0.0F};
        if (!C2D_DrawImage(image, &params, nullptr)) {
            return false;
        }
    }
    const Tex3DS_SubTexture foreground_subtexture{
        400U, 240U, 0.0F, 1.0F,
        400.0F / static_cast<float>(texture_width_),
        1.0F - 240.0F / static_cast<float>(texture_height_)};
    const C2D_Image foreground_image{
        overlay, &foreground_subtexture};
    const C2D_DrawParams foreground_params{
        {0.0F, 0.0F, 400.0F, 240.0F},
        {0.0F, 0.0F}, 0.0F, 0.0F};
    const bool drawn = C2D_DrawImage(
        foreground_image, &foreground_params, nullptr);
    return drawn;
    };
    const auto submit_begin = svcGetSystemTick();
    const bool drawn = draw_eye(top_, &overlay_texture_)
        && (right == nullptr || draw_eye(top_right_, &right_overlay_texture_));
    last_top_framebuffer_ = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr);
    C3D_FrameEnd(GX_CMDLIST_FLUSH);
    frame_timing_.gpu_submitted = true;
    frame_timing_.submit_us += ticks_to_microseconds(
        svcGetSystemTick() - submit_begin);
    if (!drawn) return false;
    return true;
}

bool Display3ds::present_cpu(const render::Framebuffer& indexed,
    const render::Palette256& palette, bool reuse_source) {
    u16 width{}, height{};
    auto *screen = reinterpret_cast<std::uint16_t*>(
        gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height));
    if (screen == nullptr || width != 240U || height != 400U) return false;
    const auto source_width = std::min<std::uint32_t>(indexed.width(), 400U);
    const auto source_height = std::min<std::uint32_t>(indexed.height(), 240U);
    const auto output_width = wide_ ? source_width : 274U;
    std::array<std::uint16_t, 256U> colours{};
    for (std::uint32_t index = 0U; index < 256U; ++index)
        colours[index] = rgb565(palette[index]);
    if (reuse_source && cpu_frame_valid_
        && source_width == cached_cpu_width_
        && source_height == cached_cpu_height_
        && indexed.draw_scale() == cached_cpu_draw_scale_
        && wide_ == cached_cpu_wide_
        && colours == cached_cpu_colours_) {
        // Leave the current front buffer in place; swapping here would expose
        // the older of the two screen buffers even though the source is equal.
        frame_timing_.reuse_hit = true;
        return true;
    }
    const auto left = (400U - output_width) / 2U;
    const auto top = (240U - source_height) / 2U;
    for (std::uint32_t x = 0U; x < 400U; ++x) {
        for (std::uint32_t y = 0U; y < 240U; ++y) {
            const auto target = x * 240U + (239U - y);
            screen[target] = x >= left && x < left + output_width
                && y >= top && y < top + source_height
                ? colours[indexed.get((x-left)*source_width/output_width,
                    y-top)] : colours[0];
        }
    }
    last_top_framebuffer_ = reinterpret_cast<const u8*>(screen);
    gfxFlushBuffers();
    gfxSwapBuffers();
    cached_cpu_colours_ = colours;
    cached_cpu_width_ = static_cast<std::uint16_t>(source_width);
    cached_cpu_height_ = static_cast<std::uint16_t>(source_height);
    cached_cpu_draw_scale_ = indexed.draw_scale();
    cached_cpu_wide_ = wide_;
    cpu_frame_valid_ = true;
    return true;
}

} // namespace starfox::platform_3ds
