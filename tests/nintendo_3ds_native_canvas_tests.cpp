#include "reference/native_canvas.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using starfox::render::Framebuffer;
using starfox::platform_3ds::NativeCanvasStyle;
using starfox::platform_3ds::compose_native_canvas;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

// Independent, deliberately unoptimized pixel oracle for the previous
// gameplay/intro canvas. Bright CGRAM fixtures suppress the separate authored
// intro-margin star decoration; this test isolates the changed composition.
void legacy_canvas(const Framebuffer& source, const Framebuffer& world,
    Framebuffer& target, std::span<const std::uint8_t> left,
    std::span<const std::uint8_t> right) {
    const auto dominant = [&](unsigned x, unsigned first, unsigned end,
        std::span<const std::uint8_t> edge) {
        std::array<unsigned, 256> counts{};
        for (unsigned y = first; y < end; ++y)
            ++counts[edge.size() == 224 ? edge[y] : source.pixels()[y * 256 + x]];
        return std::uint8_t(std::max_element(counts.begin(), counts.end()) - counts.begin());
    };
    for (unsigned y = 0; y < 240; ++y) {
        const auto first = unsigned(std::clamp(int(y) - 8, 0, 223)) & ~7U;
        const auto a = dominant(0, first, first + 8, left);
        const auto b = dominant(255, first, first + 8, right);
        for (unsigned x = 0; x < 400; ++x) {
            const auto i = y * 400 + x;
            const bool centre = y >= 8 && y < 232 && x >= 72 && x < 328;
            auto pixel = centre ? source.pixels()[(y - 8) * 256 + x - 72]
                : x < 328 ? a : b;
            // Outer rows use the left colour across the entire centre.
            if (!centre && world.pixels()[i]) pixel = world.pixels()[i];
            target.pixels()[i] = pixel;
        }
    }
}
std::uint32_t random_word(std::uint32_t& state) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state;
}
void fixture(Framebuffer& source, Framebuffer& world,
    std::array<std::uint8_t, 224>& left, std::array<std::uint8_t, 224>& right,
    unsigned seed) {
    std::uint32_t state = seed + 1;
    for (auto& pixel : source.pixels()) pixel = std::uint8_t(random_word(state));
    for (auto& pixel : world.pixels()) {
        const auto word = random_word(state);
        pixel = (word & 3) ? std::uint8_t(word) : 0;
    }
    for (unsigned y = 0; y < 224; ++y) {
        // Ties, opaque index zero, all palette indices and distinct edge/source
        // bands. Include changes in both clamped endpoint bands.
        left[y] = std::uint8_t((y / 8 + seed * 7 + (y % 8 < 4 ? 0 : 1)) % 256);
        right[y] = std::uint8_t((255 - y / 8 + seed * 11 + (y % 8 < 4 ? 0 : 1)) % 256);
        source.pixels()[y * 256] = std::uint8_t(y % 8 < 4 ? seed : seed + 1);
        source.pixels()[y * 256 + 255] = std::uint8_t(y % 8 < 4 ? seed + 2 : seed + 1);
    }
}
}
int main() try {
    Framebuffer source(256, 224), world(400, 240), expected(400, 240), actual(400, 240);
    starfox::simulation::SnesPpuState ppu;
    ppu.cgram.fill(0x7fff); // No low-luminance cells for the unchanged intro stars.
    std::array<std::uint8_t, 224> left{}, right{};
    unsigned comparisons = 0;
    for (unsigned seed = 0; seed < 64; ++seed) {
        fixture(source, world, left, right, seed);
        for (unsigned edge_case = 0; edge_case < 9; ++edge_case) {
            const auto edge = [&](const auto& values, unsigned choice) -> std::span<const std::uint8_t> {
                return choice == 0 ? std::span<const std::uint8_t>{}
                    : choice == 1 ? std::span<const std::uint8_t>{values}
                    : std::span<const std::uint8_t>{values.data(), 223};
            };
            const auto a = edge(left, edge_case % 3), b = edge(right, edge_case / 3);
            legacy_canvas(source, world, expected, a, b);
            for (const auto style : {NativeCanvasStyle::gameplay, NativeCanvasStyle::intro}) {
                actual.clear(173);
                compose_native_canvas(source, world, actual, ppu, style, a, b, int(seed) - 32);
                require(actual.pixels() == expected.pixels(), "Native canvas changed exact indexed pixels");
                ++comparisons;
            }
        }
    }
    // A subsequent frame must rebuild band colours even with unchanged PPU
    // metadata. No cross-frame image identity or cached palette is assumed.
    source.clear(0); world.clear(0); left.fill(0); right.fill(255);
    compose_native_canvas(source, world, actual, ppu, NativeCanvasStyle::gameplay, left, right);
    require(actual.get(0, 0) == 0 && actual.get(399, 239) == 255, "Zero/end band lost");
    left.fill(91); right.fill(27);
    compose_native_canvas(source, world, actual, ppu, NativeCanvasStyle::gameplay, left, right);
    require(actual.get(0, 0) == 91 && actual.get(399, 239) == 27, "Band cache leaked across frames");
    Framebuffer invalid(255, 224);
    actual.clear(173);
    compose_native_canvas(invalid, world, actual, ppu, NativeCanvasStyle::gameplay);
    require(std::all_of(actual.pixels().begin(), actual.pixels().end(), [](auto p) { return p == 173; }),
        "Invalid canvas size wrote output");
    Framebuffer mode2(400,224);
    for(unsigned y=0;y<224;++y) for(unsigned x=0;x<400;++x)
        mode2.pixels()[y*400+x]=std::uint8_t((y*7+x*3)%256);
    starfox::platform_3ds::compose_mode2_background(mode2,actual);
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<400;++x)
        require(actual.get(x,y)==mode2.get(x,unsigned(std::clamp(int(y)-8,0,223))),
            "Mode-2 source background was shifted, stretched or replaced by ending stars");
    const auto preserved=actual.pixels();
    starfox::platform_3ds::compose_mode2_background(invalid,actual);
    require(actual.pixels()==preserved,"Invalid Mode-2 extent modified native background");
    std::cout << comparisons << " independent whole-image canvas comparisons passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
