#include "../src/render/tilemap_coordinates.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::uint64_t checks{};
void check(std::int32_t value, std::uint32_t dimension) {
    // Keep a separate signed-remainder oracle, not the optimized expression.
    auto expected = value % static_cast<std::int32_t>(dimension);
    if (expected < 0) expected += static_cast<std::int32_t>(dimension);
    const auto result = starfox::render::detail::wrap_tilemap_coordinate(value, dimension);
    ++checks;
    if (result != expected) throw std::runtime_error{"signed tilemap wrap differs"};
    for (const auto shift : {3U, 4U}) {
        ++checks;
        if ((static_cast<std::uint32_t>(result) >> shift)
            != static_cast<std::uint32_t>(expected) / (1U << shift))
            throw std::runtime_error{"tile index shift differs"};
    }
}
} // namespace

int main() {
    try {
        constexpr std::array<std::int32_t, 12> extreme{
            std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::min() + 1,
            std::numeric_limits<std::int32_t>::min() + 1023,
            std::numeric_limits<std::int32_t>::min() + 1024,
            -1, 0, 1, 1023, 1024,
            std::numeric_limits<std::int32_t>::max() - 1024,
            std::numeric_limits<std::int32_t>::max() - 1,
            std::numeric_limits<std::int32_t>::max()};
        for (const auto dimension : {256U, 512U, 1024U}) {
            // Every signed 16-bit scroll plus extended viewport/HDMA guard.
            for (std::int32_t value = -131072; value <= 131072; ++value)
                check(value, dimension);
            for (const auto value : extreme) check(value, dimension);
        }
        std::cout << "Tilemap coordinates: " << checks
            << " signed-wrap/tile-index comparisons PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
