#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/grid_line_history.hpp"
#include "starfox/render/grid_projection.hpp"
#include "starfox/simulation/dust_system.hpp"
#include "starfox/timing/fixed_step.hpp"

namespace starfox::render {

class DustRenderer {
public:
    struct DustFrame {
        std::vector<simulation::DustPoint> points;
        std::array<std::uint8_t,64> colours{};
        timing::RenderTransform camera{};
        simulation::MatrixQ15 matrix{};
        std::int32_t offset_x{},offset_y{},exclude_left{},exclude_right{};
    };
    // Own the source points/palette so deferred eye rendering cannot observe
    // the simulation recycling points between recording and submission.
    [[nodiscard]] DustFrame prepare_dust(const simulation::DustSystem&,
        std::size_t active_count,const timing::RenderTransform&,
        const simulation::MatrixQ15&) const;
    static void draw_dust_frame(const DustFrame&,Framebuffer&) noexcept;
    // Binary64 XYZ and padding, matching the portable GPU input record.
    // Only source wrapping happens here; matrix/projection stay on the GPU.
    [[nodiscard]] static std::vector<std::array<double,4>> pack_dust_points(const DustFrame&);
    struct GridLinesFrame {
        ProjectedGrid projected;
        std::array<std::int16_t,2> start{};
    };
    // Capture once in the source camera, then share with both eye draws.
    // Does not draw pixels; repeated presentations retain the same start.
    [[nodiscard]] GridLinesFrame prepare_grid_lines(
        const timing::RenderTransform& camera,const simulation::MatrixQ15& matrix,
        std::uint64_t source_frame,std::uint32_t width,std::uint32_t height) const noexcept;
    static void draw_grid_lines_frame(const GridLinesFrame&,Framebuffer&,
        std::uint8_t colour=126) noexcept;
    DustRenderer(
        const assets::RomImage& rom,
        const assets::SymbolMap& symbols);

    void draw(
        const simulation::DustSystem& dust,
        std::size_t active_count,
        const timing::RenderTransform& camera,
        const simulation::MatrixQ15& view_matrix,
        Framebuffer& target,
        std::int32_t projection_offset_x = 0,
        std::int32_t projection_offset_y = 0,
        std::int32_t exclude_left = 0,
        std::int32_t exclude_right = 0) const noexcept;

    void draw_grid(
        const timing::RenderTransform& camera,
        const simulation::MatrixQ15& view_matrix,
        Framebuffer& target) const noexcept;

    void draw_grid_lines(
        const timing::RenderTransform& camera,
        const simulation::MatrixQ15& view_matrix,
        std::uint64_t source_frame,
        Framebuffer& target) const noexcept;

private:
    const assets::RomImage* rom_{};
    std::uint32_t star_colours_{};
    mutable GridLineHistory grid_line_history_;
};

} // namespace starfox::render
