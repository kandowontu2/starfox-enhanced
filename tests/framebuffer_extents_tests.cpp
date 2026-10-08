#include "starfox/render/framebuffer.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::uint64_t checks{};
void inspect(const starfox::render::Framebuffer& framebuffer) {
    // Independent pre-optimization formula, including arbitrary repartitions
    // of the same storage. Do not use the fast branch in the oracle.
    ++checks;
    if (framebuffer.draw_scale() == 0U
        || framebuffer.width() != framebuffer.stored_width() / framebuffer.draw_scale()
        || framebuffer.height() != framebuffer.stored_height() / framebuffer.draw_scale()
        || framebuffer.pixels().size()
            != std::size_t(framebuffer.stored_width()) * framebuffer.stored_height())
        throw std::runtime_error{"framebuffer logical extents changed"};
}
void indexed_rows() {
    using namespace starfox::render;
    constexpr std::array<std::uint8_t, 17> row{0, 37, 13, 255, 0, 1, 2, 3, 4, 5, 0, 0, 98, 7, 6, 0, 8};
    constexpr std::array coordinates{std::numeric_limits<std::int32_t>::min(), -32, -17, -1,
        0, 1, 7, 17, 40, std::numeric_limits<std::int32_t>::max()};
    for (unsigned scale = 1; scale <= 4; ++scale) for (unsigned width : {0U, 1U, 7U, 17U, 33U})
        for (unsigned flags = 0; flags < 16; ++flags) for (int x : coordinates) for (int y : {-1, 0, 2, 3}) {
            Framebuffer actual(width, 3, scale), expected(width, 3, scale);
            RasterCommands actual_commands, expected_commands;
            for (auto* frame : {&actual, &expected}) {
                frame->enable_layer_tags((flags & 1U) != 0);
                frame->enable_dither_pairs((flags & 2U) != 0);
                frame->clear(37);
                if (flags & 4U) frame->begin_write_coverage();
                if (flags % 3U == 0) frame->set_layer_override(PixelLayer::background);
                for (unsigned i = 0; i < frame->pixels().size(); ++i)
                    frame->set_dither_alternate(i, std::uint8_t(i % 251U));
            }
            if (flags & 8U) {
                actual_commands.reset(actual.stored_width(), actual.stored_height());
                expected_commands.reset(expected.stored_width(), expected.stored_height());
                actual.record_to(&actual_commands); expected.record_to(&expected_commands);
            }
            actual.set_indexed_row(x, y, row);
            // The unchanged point writer is the reference, including its
            // optional metadata, scaling, clipping and point-command contract.
            for (unsigned i = 0; i < row.size(); ++i) {
                const auto column = std::int64_t(x) + i;
                if (row[i] && column >= std::numeric_limits<std::int32_t>::min()
                    && column <= std::numeric_limits<std::int32_t>::max())
                    expected.set(std::int32_t(column), y, row[i]);
            }
            ++checks;
            if (actual.pixels() != expected.pixels() || actual.layer_tags() != expected.layer_tags()
                || !std::equal(actual.write_coverage().begin(), actual.write_coverage().end(), expected.write_coverage().begin())
                || !std::equal(actual.dither_pairs().begin(), actual.dither_pairs().end(), expected.dither_pairs().begin())
                || actual_commands.commands.size() != expected_commands.commands.size())
                throw std::runtime_error("indexed tile row changed pixels/coverage/tags/dither/command count");
            for (unsigned i = 0; i < actual_commands.commands.size(); ++i) {
                const auto& a = actual_commands.commands[i]; const auto& b = expected_commands.commands[i];
                if (a.left != b.left || a.right != b.right || a.top != b.top || a.bottom != b.bottom
                    || a.even != b.even || a.odd != b.odd || a.tag != b.tag)
                    throw std::runtime_error("indexed tile row changed an ordered point command");
            }
            actual.set_indexed_row(x, y, {});
            if (actual.pixels() != expected.pixels()) throw std::runtime_error("empty indexed row wrote pixels");
        }
}
void solid_rows() {
    using namespace starfox::render;
    constexpr std::array coordinates{std::numeric_limits<std::int32_t>::min(), -32, -1,
        0, 1, 7, 40, std::numeric_limits<std::int32_t>::max()};
    for (unsigned scale = 1; scale <= 4; ++scale) for (unsigned width : {0U, 1U, 7U, 33U})
        for (unsigned flags = 0; flags < 16; ++flags) for (int x : coordinates)
        for (int y : {-1, 0, 2, 3}) for (unsigned count : {0U, 1U, 17U, 100U, std::numeric_limits<unsigned>::max()}) {
            Framebuffer actual(width, 3, scale), expected(width, 3, scale);
            RasterCommands ac, ec;
            for (auto* frame : {&actual, &expected}) {
                frame->enable_layer_tags((flags & 1U) != 0);
                frame->enable_dither_pairs((flags & 2U) != 0);
                frame->clear(37);
                if (flags & 4U) frame->begin_write_coverage();
                frame->set_layer_override(PixelLayer::background);
                for (unsigned i = 0; i < frame->pixels().size(); ++i) frame->set_dither_alternate(i, i % 251U);
            }
            if (flags & 8U) {
                ac.reset(actual.stored_width(), actual.stored_height());
                ec.reset(expected.stored_width(), expected.stored_height());
                actual.record_to(&ac); expected.record_to(&ec);
            }
            const auto ink = std::uint8_t(flags % 3 ? 0 : 255);
            actual.set_solid_indexed_row(x, y, count, ink);
            // Enumerate only visible logical coordinates, including huge
            // caller extents; this is independent of the optimized clipping.
            for (unsigned column = 0; column < width; ++column)
                if (std::int64_t(column) >= x && std::int64_t(column) < std::int64_t(x) + count)
                    expected.set(column, y, ink);
            ++checks;
            if (actual.pixels() != expected.pixels() || actual.layer_tags() != expected.layer_tags()
                || !std::equal(actual.write_coverage().begin(), actual.write_coverage().end(), expected.write_coverage().begin())
                || !std::equal(actual.dither_pairs().begin(), actual.dither_pairs().end(), expected.dither_pairs().begin())
                || ac.commands.size() != ec.commands.size())
                throw std::runtime_error{"opaque row pixels/coverage/tags/dither/command count changed"};
            for (unsigned i = 0; i < ac.commands.size(); ++i) {
                const auto& a = ac.commands[i]; const auto& b = ec.commands[i];
                if (a.left != b.left || a.right != b.right || a.top != b.top || a.bottom != b.bottom
                    || a.even != b.even || a.odd != b.odd || a.tag != b.tag)
                    throw std::runtime_error{"opaque row command order changed"};
            }
        }
}
} // namespace

int main() try {
    indexed_rows();
    solid_rows();
    constexpr std::array<std::uint32_t, 8> sizes{0, 1, 2, 7, 8, 17, 63, 129};
    constexpr std::array<std::uint32_t, 10> partitions{
        0, 1, 2, 3, 4, 5, 6, 16, 65536,
        std::numeric_limits<std::uint32_t>::max()};
    for (const auto width : sizes) for (const auto height : sizes)
        for (std::uint32_t initial = 0; initial <= 6; ++initial) {
            starfox::render::Framebuffer frame{width, height, initial};
            inspect(frame);
            frame.clear(37);
            for (const auto scale : partitions) {
                frame.set_draw_scale(scale);
                inspect(frame);
                const auto copy = frame;
                inspect(copy);
                for (const auto pixel : frame.pixels()) {
                    ++checks;
                    if (pixel != 37) throw std::runtime_error{"extent query altered stored pixels"};
                }
            }
            // Resume native writes, then alternate same-size and changed-size
            // resize paths under both native and noninteger source partitions.
            for (std::uint32_t scale = 0; scale <= 6; ++scale) {
                frame.set_draw_scale(scale);
                frame.resize(width, height);
                inspect(frame);
                frame.resize(width, height);
                inspect(frame);
                frame.resize(height, width);
                inspect(frame);
            }
        }
    std::cout << "Framebuffer extents: " << checks
        << " native/scaled/repartitioned/copy/resize/indexed-row comparisons PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
