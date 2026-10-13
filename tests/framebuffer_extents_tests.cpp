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
bool same_canvas(const starfox::render::Framebuffer& a, const starfox::render::Framebuffer& b) {
    return a.pixels() == b.pixels() && a.layer_tags() == b.layer_tags()
        && std::equal(a.write_coverage().begin(), a.write_coverage().end(),
            b.write_coverage().begin(), b.write_coverage().end())
        && std::equal(a.dither_pairs().begin(), a.dither_pairs().end(),
            b.dither_pairs().begin(), b.dither_pairs().end())
        && a.tracks_write_coverage() == b.tracks_write_coverage()
        && a.command_buffer() == b.command_buffer()
        && a.layer_override() == b.layer_override()
        && a.stored_width() == b.stored_width() && a.stored_height() == b.stored_height()
        && a.draw_scale() == b.draw_scale();
}
void transparent_native_canvases() {
    using namespace starfox::render;
    for (unsigned width : {0U, 1U, 17U, 400U}) for (unsigned height : {0U, 1U, 5U, 240U})
        for (unsigned prior = 0; prior < 2; ++prior) for (unsigned pattern = 0; pattern < 8; ++pattern) {
            Framebuffer source(width, height), actual(width, height), expected(width, height);
            for (std::size_t i = 0; i < source.pixels().size(); ++i) {
                const unsigned x = unsigned(i % width), y = unsigned(i / width);
                source.pixels()[i] = pattern == 0 ? 0 : pattern == 1 ? 255 : pattern == 2 ? 37
                    : pattern == 3 ? std::uint8_t(i % 256)
                    : pattern == 4 ? std::uint8_t((x % 2) ? 0 : 13)
                    : pattern == 5 ? std::uint8_t((y % 2) ? 98 : 0)
                    : pattern == 6 ? std::uint8_t((i % 97) ? 0 : 1)
                    : std::uint8_t((i % 97) ? 7 : 0);
            }
            const auto source_before = source;
            for (auto* canvas : {&actual, &expected}) {
                canvas->begin_write_coverage(); canvas->clear(37);
                if (!prior) canvas->end_write_coverage();
                canvas->set_layer_override(PixelLayer::terrain_geometry);
            }
            // Independent ordered point oracle for the old clear+coverage+
            // transparent compositor contract, including previously dirty rows.
            expected.clear(0); expected.begin_write_coverage();
            for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x)
                if (const auto ink = source.get(x, y))
                    expected.set_stored(x, y, ink, PixelLayer::three_d);
            ++checks;
            if (!actual.reset_transparent_native_canvas(source) || !same_canvas(actual, expected)
                || !same_canvas(source, source_before))
                throw std::runtime_error("Fused native canvas changed pixels/coverage/source/state");
            // Reuse the existing allocation and overwrite all stale coverage.
            source.clear(0); expected.clear(0); expected.begin_write_coverage();
            ++checks;
            if (!actual.reset_transparent_native_canvas(source) || !same_canvas(actual, expected))
                throw std::runtime_error("Fused native canvas retained a dirty transparent cell");
        }
    for (unsigned reason = 0; reason < 12; ++reason) {
        Framebuffer source(17, 5), destination(17, 5);
        RasterCommands source_commands, destination_commands;
        destination.begin_write_coverage(); destination.clear(37);
        source.clear(98);
        switch (reason) {
        case 0: source.set_draw_scale(2); break;
        case 1: destination.set_draw_scale(2); break;
        case 2: source.resize(18, 5); break;
        case 3: source.resize(17, 6); break;
        case 4: source.enable_layer_tags(true); break;
        case 5: destination.enable_layer_tags(true); break;
        case 6: source.enable_dither_pairs(true); break;
        case 7: destination.enable_dither_pairs(true); break;
        case 8: source.record_to(&source_commands); break;
        case 9: destination.record_to(&destination_commands); break;
        case 10: source.pixels().resize(1); break;
        case 11: destination.pixels().resize(1); break;
        }
        const auto source_before = source, destination_before = destination;
        ++checks;
        if (destination.reset_transparent_native_canvas(source) || !same_canvas(source, source_before)
            || !same_canvas(destination, destination_before) || !source_commands.commands.empty()
            || !destination_commands.commands.empty())
            throw std::runtime_error("Rejected fused canvas changed either owner or recorded commands");
    }
    Framebuffer alias(17, 5); alias.begin_write_coverage(); alias.clear(13);
    const auto alias_before = alias;
    ++checks;
    if (alias.reset_transparent_native_canvas(alias) || !same_canvas(alias, alias_before))
        throw std::runtime_error("Fused native canvas accepted or changed an aliased source");
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
void dithered_rows() {
    using namespace starfox::render;
    constexpr std::array coordinates{std::numeric_limits<std::int32_t>::min(), -32, -1,
        0, 1, 7, 40, std::numeric_limits<std::int32_t>::max()};
    constexpr std::array<std::array<std::uint8_t, 2>, 3> inks{{{0, 255}, {17, 98}, {37, 37}}};
    for (unsigned scale : {1U, 2U, 4U}) for (unsigned width : {0U, 1U, 7U, 33U})
        for (unsigned flags = 0; flags < 16; ++flags) for (int x : coordinates)
        for (int y : {-1, 0, 2, 3}) for (unsigned count : {0U, 1U, 17U, 100U, std::numeric_limits<unsigned>::max()})
        for (auto ink : inks) {
            Framebuffer actual(width, 3, scale), expected(width, 3, scale);
            RasterCommands ac, ec;
            for (auto* frame : {&actual, &expected}) {
                frame->enable_layer_tags((flags & 1U) != 0);
                frame->enable_dither_pairs((flags & 2U) != 0);
                frame->clear(37);
                if (flags & 4U) frame->begin_write_coverage();
                if (flags % 3U == 0) frame->set_layer_override(PixelLayer::three_d);
                else if (flags % 3U == 1) frame->set_layer_override(PixelLayer::background);
                for (unsigned i = 0; i < frame->pixels().size(); ++i)
                    frame->set_dither_alternate(i, std::uint8_t(i % 251U));
            }
            if (flags & 8U) {
                ac.reset(actual.stored_width(), actual.stored_height());
                ec.reset(expected.stored_width(), expected.stored_height());
                actual.record_to(&ac); expected.record_to(&ec);
            }
            actual.set_dithered_indexed_row(x, y, count, ink[0], ink[1]);
            // Independently enumerate visible source coordinates, preserving
            // point-command order and the existing provenance annotation rule.
            for (unsigned column = 0; column < width; ++column)
                if (std::int64_t(column) >= x && std::int64_t(column) < std::int64_t(x) + count) {
                    expected.set(column, y, ((column ^ y) & 1) ? ink[1] : ink[0]);
                    expected.annotate_dither(column, y, ink[0], ink[1]);
                }
            ++checks;
            if (actual.pixels() != expected.pixels() || actual.layer_tags() != expected.layer_tags()
                || !std::equal(actual.write_coverage().begin(), actual.write_coverage().end(), expected.write_coverage().begin())
                || !std::equal(actual.dither_pairs().begin(), actual.dither_pairs().end(), expected.dither_pairs().begin())
                || ac.commands.size() != ec.commands.size())
                throw std::runtime_error{"two-ink row pixels/coverage/tags/dither/command count changed"};
            for (unsigned i = 0; i < ac.commands.size(); ++i) {
                const auto& a = ac.commands[i]; const auto& b = ec.commands[i];
                if (a.left != b.left || a.right != b.right || a.top != b.top || a.bottom != b.bottom
                    || a.even != b.even || a.odd != b.odd || a.tag != b.tag)
                    throw std::runtime_error{"two-ink row command order changed"};
            }
        }
}
} // namespace

int main() try {
    transparent_native_canvases();
    indexed_rows();
    solid_rows();
    dithered_rows();
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
