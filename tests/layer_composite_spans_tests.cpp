#include "starfox/render/framebuffer.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using namespace starfox::render;
std::uint64_t checks{};

// Independent pre-span equal-scale implementation, intentionally one write
// at a time. In-place source/destination reads must remain ordered too.
void old_composite(const Framebuffer& source, Framebuffer& destination,
    const LayerCompositeSettings& settings) {
    const auto scale = source.draw_scale();
    const auto pairs = source.dither_pairs();
    const bool carry = !pairs.empty() && scale > 1 && destination.draw_scale() > 1;
    if (carry) destination.enable_dither_pairs(true);
    for (std::uint32_t y = 0; y < source.stored_height(); ++y)
        for (std::uint32_t x = 0; x < source.stored_width(); ++x) {
            const auto dx = std::int64_t(x) + std::int64_t(settings.offset_x) * scale;
            const auto dy = std::int64_t(y) + std::int64_t(settings.offset_y) * scale;
            if (dx < 0 || dy < 0 || dx >= destination.stored_width()
                || dy >= destination.stored_height()
                || dx < std::int64_t(settings.clip_left) * scale
                || dx >= std::int64_t(settings.clip_right) * scale
                || dy < std::int64_t(settings.clip_top) * scale
                || dy >= std::int64_t(settings.clip_bottom) * scale) continue;
            const auto input = std::size_t(y) * source.stored_width() + x;
            const auto colour = source.pixels()[input];
            if (colour == 0 && !(carry && (pairs[input] & 256U))) continue;
            const auto output = std::size_t(dy) * destination.stored_width() + std::size_t(dx);
            destination.pixels()[output] = colour;
            destination.mark_written(output);
            if (carry && (pairs[input] & 256U))
                destination.set_dither_alternate(output, std::uint8_t(pairs[input]));
            if (destination.layer_tags_enabled())
                destination.layer_tags()[output] = source.layer_tags_enabled()
                    ? source.layer_tags()[input] : std::uint8_t(PixelLayer::three_d);
        }
}
void compare(const Framebuffer& a, const Framebuffer& b) {
    ++checks;
    if (a.pixels() != b.pixels() || a.layer_tags() != b.layer_tags()
        || !std::ranges::equal(a.write_coverage(), b.write_coverage())
        || !std::ranges::equal(a.dither_pairs(), b.dither_pairs()))
        throw std::runtime_error("Opaque span composite differs from ordered point oracle");
}
void populate(Framebuffer& frame, unsigned flags, unsigned phase, bool target) {
    frame.enable_layer_tags(flags & 1U);
    if (flags & 2U) frame.enable_dither_pairs(true);
    auto& pixels = frame.pixels();
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const bool gap = phase % 4U == 0 ? true : phase % 4U == 1 ? false
            : phase % 4U == 2 ? i % 2U == 0 : i % 29U < 7U;
        pixels[i] = target ? std::uint8_t(i * 17U + 13U)
            : gap ? 0 : std::uint8_t(1U + i % 255U);
        if (frame.layer_tags_enabled()) frame.layer_tags()[i] = std::uint8_t(i % 4U);
        if (flags & 2U) frame.set_dither_alternate(i, std::uint8_t(i * 13U + 1U));
    }
    if (flags & 4U) frame.begin_write_coverage();
}
}
int main() try {
    for (unsigned n = 0; n < 96; ++n)
        for (unsigned scale : {1U, 2U, 4U}) for (unsigned flags = 0; flags < 32; ++flags) {
            Framebuffer source(n & 1U ? 33U : 7U, n % 17U ? 9U : 0U, scale);
            Framebuffer actual(n & 2U ? 42U : 11U, 11U, scale);
            populate(source, (flags & 1U) | ((flags >> 2U) & 2U), n, false);
            populate(actual, ((flags >> 1U) & 1U) | ((flags >> 3U) & 2U)
                | ((flags >> 0U) & 4U), n, true);
            auto expected = actual;
            const auto source_before = source;
            LayerCompositeSettings settings;
            constexpr std::array xs{-73, -7, -1, 0, 3, 40};
            constexpr std::array ys{-12, -1, 0, 4};
            settings.offset_x = xs[(n / 4U) % xs.size()];
            settings.offset_y = ys[(n / 24U) % ys.size()];
            if (n & 8U) { settings.clip_left = 2; settings.clip_right = 29; }
            if (n & 16U) { settings.clip_top = 1; settings.clip_bottom = 8; }
            old_composite(source, expected, settings);
            composite_transparent_layer(source, actual, settings);
            compare(actual, expected);
            compare(source, source_before);
        }
    for (unsigned scale : {1U, 2U, 4U}) for (int offset : {-3, -1, 0, 1, 3})
        for (unsigned flags = 0; flags < 8; ++flags) {
            Framebuffer actual(33, 9, scale);
            populate(actual, flags, 3, false);
            auto expected = actual;
            LayerCompositeSettings settings;
            settings.offset_x = offset; settings.offset_y = offset;
            old_composite(expected, expected, settings);
            composite_transparent_layer(actual, actual, settings);
            compare(actual, expected);
        }
    std::cout << "Layer composite: " << checks
        << " point-oracle transparency/clip/scale/coverage/tag/dither/in-place comparisons PASS\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
