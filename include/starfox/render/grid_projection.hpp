#pragma once
#include "starfox/simulation/math.hpp"
#include "starfox/timing/fixed_step.hpp"
#include <array>

namespace starfox::render {
// Compact camera-space lattice, before projection/clipping. Unlike visible
// screen points this retains geometry needed by both stereo eye frusta.
struct GridLattice {
    std::array<std::int16_t,3> origin{},x_step{},z_step{};
};
[[nodiscard]] GridLattice source_grid_lattice(const timing::RenderTransform&,
    const simulation::MatrixQ15&) noexcept;
struct ProjectedGridPoint { std::int16_t x{},y{},depth{}; };
struct ProjectedGrid {
    std::array<ProjectedGridPoint,225> points{};
    std::size_t count{};
};
// Source-order visible lattice points; no pixels, endpoint history or GPU work.
// Used to advance canonical connected-grid history once per source frame.
[[nodiscard]] ProjectedGrid project_source_grid(const timing::RenderTransform&,
    const simulation::MatrixQ15&,std::uint32_t width,std::uint32_t height) noexcept;
}
