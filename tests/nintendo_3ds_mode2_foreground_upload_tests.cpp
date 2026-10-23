#include "starfox/platform/nintendo_3ds/mode2_foreground_upload.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

using starfox::render::Framebuffer;
using starfox::platform::nintendo_3ds::pack_mode2_foreground;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main() try {
    Framebuffer world(400, 240), source(256, 224);
    std::array<std::uint16_t, 256> palette{};
    std::vector<std::uint16_t> actual(512 * 256), expected(actual.size());
    std::uint32_t random = 0x87654321U;
    const auto next = [&] {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        return random;
    };
    for (unsigned fixture = 0; fixture < 256; ++fixture) {
        for (unsigned i = 0; i < 256; ++i)
            palette[i] = std::uint16_t((next() & 0xfffeU) | 1U);
        palette[0] = 1U; // Opaque RGB black, not transparent zero-ink.
        for (auto& pixel : world.pixels())
            pixel = fixture == 0 ? 0 : fixture == 1 ? 255
                : fixture == 2 ? std::uint8_t(next())
                : (next() & 3U) ? std::uint8_t(next()) : 0;
        source.begin_write_coverage();
        for (unsigned y = 0; y < 224; ++y) for (unsigned x = 0; x < 256; ++x) {
            const auto word = next();
            const auto colour = fixture == 0 || !(word & 7U) ? 0 : std::uint8_t(word);
            // Keep deliberately nonzero stale ink in uncovered cells too.
            source.pixels()[y * 256 + x] = colour;
            const bool covered = fixture == 1 || (fixture != 0 && (word & 1U));
            if (covered) source.set(x, y, colour);
        }
        source.end_write_coverage(); // Frozen coverage remains consumable.
        const auto old_world = world.pixels(), old_source = source.pixels();
        const std::vector<std::uint8_t> old_coverage(source.write_coverage().begin(),
            source.write_coverage().end());
        std::fill(actual.begin(), actual.end(), 0xcafeU);
        expected = actual;
        // Independent old painter: world nonzero ink, then every covered
        // source write at (72,8), finally palette conversion using coverage.
        for (unsigned y = 0; y < 240; ++y) for (unsigned x = 0; x < 400; ++x) {
            auto colour = world.pixels()[y * 400 + x];
            bool opaque = colour != 0;
            if (x >= 72 && x < 328 && y >= 8 && y < 232) {
                const auto i = (y - 8) * 256 + x - 72;
                if (old_coverage[i]) { colour = source.pixels()[i]; opaque = true; }
            }
            expected[y * 512 + x] = opaque ? palette[colour] : 0;
        }
        require(pack_mode2_foreground(world, source, palette, actual, 512), "Valid upload rejected");
        require(actual == expected, "Direct upload changed pixels, alpha or texture padding");
        require(world.pixels() == old_world && source.pixels() == old_source
            && std::equal(old_coverage.begin(), old_coverage.end(), source.write_coverage().begin()),
            "Upload mutated its source rasters");
    }
    Framebuffer absent_coverage(256, 224), wrong_source(255, 224), scaled(256, 224, 2);
    wrong_source.begin_write_coverage(); scaled.begin_write_coverage();
    for (const auto* invalid : {&absent_coverage, &wrong_source, &scaled}) {
        std::fill(actual.begin(), actual.end(), 0xbeefU);
        require(!pack_mode2_foreground(world, *invalid, palette, actual, 512), "Invalid source accepted");
        require(std::all_of(actual.begin(), actual.end(), [](auto p) {return p == 0xbeefU;}),
            "Rejected source wrote output");
    }
    require(!pack_mode2_foreground(world, source, palette, actual, 399), "Short stride accepted");
    require(!pack_mode2_foreground(world, source, palette, std::span{actual.data(), 512 * 239}, 512),
        "Short output accepted");
    require(!pack_mode2_foreground(world, source, palette, actual, std::size_t(-1)), "Overflow stride accepted");
    require(std::all_of(actual.begin(), actual.end(), [](auto p) {return p == 0xbeefU;}),
        "Rejected output layout wrote storage");
    std::cout << "256 independent full texture/alpha comparisons and fail-closed extents passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
