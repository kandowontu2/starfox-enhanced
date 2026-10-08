#include "starfox/render/background_renderer.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
using namespace starfox::render;
using starfox::simulation::SnesPpuState;
std::uint32_t random(std::uint32_t& seed) {
    seed ^= seed << 13U; seed ^= seed >> 17U; seed ^= seed << 5U; return seed;
}
SnesPpuState source(unsigned n) {
    SnesPpuState ppu;
    std::uint32_t seed = 0x659a2217U + n * 0x9e3779b9U;
    for (auto& value : ppu.vram) value = static_cast<std::uint8_t>(random(seed));
    for (auto& value : ppu.cgram) value = static_cast<std::uint16_t>(random(seed) & 32767U);
    for (unsigned i = 0; i < 256; i += 7) ppu.cgram[i] = 0;
    ppu.main_screen = n % 31 ? 2 : 0;
    ppu.tunnel_scene = true;
    ppu.background_mode = n % 3 + 1;
    ppu.bg2_screen_size = n & 3U;
    ppu.bg2_tile_size_16 = (n & 4U) != 0;
    ppu.bg2_character_base = n % 7 ? 0x1000 : 0x7ffe;
    ppu.bg2_screen_base = n % 11 ? 0x6000 : 0x7fff;
    ppu.bg2_scroll_x = static_cast<std::int16_t>(random(seed));
    ppu.bg2_scroll_y = static_cast<std::int16_t>(random(seed));
    ppu.bg2_horizontal_offsets_enabled = n % 3 != 0;
    ppu.bg2_scanline_scroll_enabled = n % 4 != 0;
    ppu.bg2_vertical_offsets_enabled = n % 13 == 0;
    ppu.mosaic = n % 17 ? 0 : 0x32;
    for (unsigned y = 0; y < 224; ++y) {
        ppu.bg2_horizontal_offsets[y] = static_cast<std::int16_t>(random(seed));
        ppu.bg2_scanline_scroll_y[y] = static_cast<std::int16_t>(random(seed));
    }
    if (n % 19 == 0) ppu.vram.fill(0); // Transparent wall and explicit index-zero coverage.
    return ppu;
}
void add(std::uint64_t& hash, std::span<const std::uint8_t> bytes) {
    for (auto value : bytes) { hash ^= value; hash *= 1099511628211ULL; }
}
void number(std::uint64_t& hash, std::uint32_t value) {
    const std::array bytes{std::uint8_t(value), std::uint8_t(value >> 8U),
        std::uint8_t(value >> 16U), std::uint8_t(value >> 24U)};
    add(hash, bytes);
}
std::uint64_t trace() {
    constexpr std::array widths{0U, 1U, 31U, 127U, 256U, 400U, 853U};
    constexpr std::array heights{1U, 17U, 97U, 224U, 240U};
    constexpr std::array origins{-300, -129, -13, 0, 73, 400, 900};
    const BackgroundRenderer renderer;
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned n = 0; n < 210; ++n) {
        const auto ppu = source(n);
        for (const auto pass : {TilePriorityPass::all, TilePriorityPass::low, TilePriorityPass::high}) {
            Framebuffer frame(widths[n % widths.size()], heights[(n / 7) % heights.size()], n % 23 ? 1 : 2);
            frame.enable_layer_tags(true); frame.enable_dither_pairs(n % 5 == 0);
            frame.clear(37); frame.begin_write_coverage();
            for (unsigned i = 0; i < frame.pixels().size(); ++i) frame.set_dither_alternate(i, i % 251);
            const BackgroundUniqueRegion region{0, 0, 192, 224, 1, 127, 4, 128};
            {
                const ScopedLayer tag(frame, PixelLayer::background);
                renderer.draw_bg2(ppu, ppu.bg2_scroll_x, ppu.bg2_scroll_y, frame, pass,
                    origins[(n / 5) % origins.size()], n % 11 != 0, n % 9 != 0, n % 2 != 0,
                    n % 29 ? 0 : 17, n % 37 ? std::span<const BackgroundUniqueRegion>{} : std::span{&region, 1},
                    n % 41 == 0, n % 43 == 0, 73);
            }
            number(hash, n); number(hash, unsigned(pass));
            add(hash, frame.pixels()); add(hash, frame.layer_tags()); add(hash, frame.write_coverage());
            for (const auto pair : frame.dither_pairs()) number(hash, pair);
        }
    }
    // GPU point-command ordering must stay exactly as before the optimization.
    for (unsigned n = 0; n < 18; ++n) {
        const auto ppu = source(n + 1);
        Framebuffer frame(33, 17, n % 3 + 1);
        RasterCommands commands; commands.reset(frame.stored_width(), frame.stored_height());
        frame.record_to(&commands);
        renderer.draw_bg2(ppu, ppu.bg2_scroll_x, ppu.bg2_scroll_y, frame,
            static_cast<TilePriorityPass>(n % 3), n % 2 ? -249 : 17, true, n % 5 != 0, n % 2 != 0);
        number(hash, static_cast<std::uint32_t>(commands.commands.size()));
        for (const auto& c : commands.commands) {
            number(hash, c.left); number(hash, c.top); number(hash, c.right); number(hash, c.bottom);
            number(hash, c.even); number(hash, c.odd); number(hash, c.tag);
        }
    }
    return hash;
}
void benchmark() {
    auto ppu = source(2);
    ppu.main_screen = 2; ppu.mosaic = 0; ppu.background_mode = 1;
    ppu.bg2_vertical_offsets_enabled = false;
    const BackgroundRenderer renderer;
    for (const bool hdma : {false, true}) {
        ppu.bg2_scanline_scroll_enabled = hdma; ppu.bg2_horizontal_offsets_enabled = hdma;
        Framebuffer frame(600, 192); frame.enable_layer_tags(true); frame.begin_write_coverage();
        const auto begin = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 100; ++i)
            renderer.draw_bg2(ppu, static_cast<std::int16_t>(i), ppu.bg2_scroll_y,
                frame, TilePriorityPass::low, 172, true, true, true);
        std::uint64_t hash = 14695981039346656037ULL;
        add(hash, frame.pixels()); add(hash, frame.layer_tags()); add(hash, frame.write_coverage());
        std::cout << "hdma=" << hdma << " ms="
            << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count()
            << " digest=" << std::hex << hash << std::dec << '\n';
    }
}
} // namespace
int main(int argc, char** argv) try {
    if (argc == 2 && std::string_view{argv[1]} == "--benchmark") { benchmark(); return 0; }
    const auto hash = trace();
#ifndef STARFOX_TUNNEL_LEGACY_ORACLE
    // Pin this from the separate pre-optimization executable, not from the new path.
    // Baseline background_renderer.cpp SHA256 D0D8963026E2F5EE01C6F2E4689D9443
    // 09FB1D35F53598D1E44777364810DD16, compiled before changing the renderer.
    constexpr std::uint64_t expected = 0x4085b5e1f2bcb833ULL;
    if (hash != expected) throw std::runtime_error{"tunnel pixels/coverage/tags/dither/commands changed"};
#endif
    std::cout << "PASS 630 tunnel rasters + 18 ordered command streams: " << std::hex << hash << '\n';
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
