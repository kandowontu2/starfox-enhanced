#pragma once
#include "starfox/simulation/snes_ppu.hpp"
#include <cstdint>
#include <vector>

namespace starfox::platform::nintendo_3ds {
// Adapted from Esteban PDN's starwing-3ds bg2_plan.cpp, revision 753952be.
// See platform/3ds/UPSTREAM-STARWING.md for provenance and permission.
struct Bg2TileRect {
    unsigned x{},y{},width{},height{};
    std::uint16_t character{};
    std::uint8_t bank{},source_x{},source_y{};
    bool reverse_x{},reverse_y{};
    std::uint8_t solid_index{}; // Nonzero source CGRAM index; merged uniform/carry colour.
};
// The first integration accepts ordinary Mode-2 artwork, including complete
// constant offset tables whose lower source characters need no ground carry.
// Rolled ground/corridors/mosaic need our finite-depth/unique-region semantics,
// not the upstream flat-screen approximation. Failure publishes no rectangles.
bool plan_bg2_tiles(const simulation::SnesPpuState&,int scroll_x,int scroll_y,
    unsigned width,int origin,int priority,std::vector<Bg2TileRect>&,
    unsigned capacity);
// Full fitted Mode-2 roll/HDMA and source ground continuation. Uniform source
// characters become merged colour rectangles, not an invented smooth floor.
bool plan_rolled_bg2_tiles(const simulation::SnesPpuState&,int scroll_x,int scroll_y,
    unsigned width,int origin,int priority,std::vector<Bg2TileRect>&,unsigned capacity);
} // namespace starfox::platform::nintendo_3ds
