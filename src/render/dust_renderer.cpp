#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/grid_projection.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string>

namespace starfox::render {
namespace {

constexpr std::int16_t kGridSize = 15;
constexpr std::int16_t kGridWidth = 256;
constexpr std::int16_t kGridHalfExtent = kGridWidth * kGridSize / 2;
constexpr std::int16_t kMaximumReciprocalDepth = 12 * 1'024;

std::int16_t camera_word(double value) noexcept {
    return simulation::wrap16(static_cast<std::int64_t>(std::trunc(value)));
}

std::int16_t grid_start(std::int16_t camera) noexcept {
    const auto phase = static_cast<std::uint16_t>(camera) & 0xffU;
    return simulation::wrap16(
        static_cast<std::int32_t>(phase ^ 0xffU) - kGridHalfExtent);
}

std::int16_t matrix_grid_step(std::int16_t value) noexcept {
    return simulation::wrap16(simulation::arithmetic_shift_right(value, 7));
}

std::int16_t grid_projection(std::int16_t coordinate, std::int16_t depth) noexcept {
    const auto even_depth = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(depth) & 0xfffeU);
    const auto reciprocal = static_cast<std::int16_t>(
        (32'767 * 256) / even_depth);
    return simulation::multiply_q15(coordinate, reciprocal);
}

double source_word_difference(double value, double origin) noexcept {
    auto difference = std::fmod(value - origin, 65'536.0);
    if (difference > 32'767.0) difference -= 65'536.0;
    else if (difference < -32'768.0) difference += 65'536.0;
    return difference;
}

std::uint32_t rom_symbol(
    const assets::SymbolMap& symbols, const char* name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U
            && ((address >> 16U) & 0xffU) < 0x7eU) return address;
    }
    throw std::runtime_error{std::string{"missing dust ROM symbol: "} + name};
}

} // namespace

GridLattice source_grid_lattice(const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix) noexcept {
    GridLattice result;
    result.origin=simulation::transform_q15(view_matrix,{
        grid_start(camera_word(camera.x)),simulation::wrap16(-int32_t(camera_word(camera.y))),
        grid_start(camera_word(camera.z))});
    result.x_step={matrix_grid_step(view_matrix[0]),matrix_grid_step(view_matrix[1]),matrix_grid_step(view_matrix[2])};
    result.z_step={matrix_grid_step(view_matrix[6]),matrix_grid_step(view_matrix[7]),matrix_grid_step(view_matrix[8])};
    return result;
}
ProjectedGrid project_source_grid(const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix,std::uint32_t width,std::uint32_t height) noexcept {
    ProjectedGrid result;
    const auto lattice=source_grid_lattice(camera,view_matrix);
    auto row=lattice.origin;
    const auto& x_step=lattice.x_step;
    const auto& z_step=lattice.z_step;
    for(unsigned z=0;z<15;++z) {
        auto point=row;
        for(unsigned x=0;x<15;++x) {
            if(point[2]>256) {
                const auto depth=std::min<int16_t>(point[2],kMaximumReciprocalDepth-1);
                const auto sx=simulation::add16(grid_projection(point[0],depth),static_cast<int16_t>(width/2));
                const auto sy=simulation::add16(grid_projection(point[1],depth),static_cast<int16_t>(height/2));
                if(static_cast<uint16_t>(sx)<width && static_cast<uint16_t>(sy)<height)
                    result.points[result.count++]={sx,sy,point[2]};
            }
            for(unsigned axis=0;axis<3;++axis) point[axis]=simulation::add16(point[axis],x_step[axis]);
        }
        for(unsigned axis=0;axis<3;++axis) row[axis]=simulation::add16(row[axis],z_step[axis]);
    }
    return result;
}

DustRenderer::DustRenderer(
    const assets::RomImage& rom,
    const assets::SymbolMap& symbols)
    : rom_(&rom), star_colours_(rom_symbol(symbols, "STAR_COLS")) {}

DustRenderer::DustFrame DustRenderer::prepare_dust(const simulation::DustSystem& dust,
    std::size_t active_count,const timing::RenderTransform& camera,
    const simulation::MatrixQ15& matrix) const {
    DustFrame result;
    active_count=std::min(active_count,dust.points().size());
    result.points.assign(dust.points().begin(),dust.points().begin()+active_count);
    for(std::size_t i=0;i<result.colours.size();++i)
        result.colours[i]=rom_->read8(star_colours_+static_cast<std::uint32_t>(i));
    result.camera=camera;result.matrix=matrix;
    return result;
}

std::vector<std::array<double,4>> DustRenderer::pack_dust_points(const DustFrame& frame) {
    if(frame.points.size()>simulation::kMaximumDustPoints || !std::isfinite(frame.camera.x)
        || !std::isfinite(frame.camera.y) || !std::isfinite(frame.camera.z))
        throw std::invalid_argument("Invalid dust snapshot");
    std::vector<std::array<double,4>> result;result.reserve(frame.points.size());
    for(const auto& p:frame.points) result.push_back({source_word_difference(p.x,frame.camera.x),
        source_word_difference(p.y,frame.camera.y),source_word_difference(p.z,frame.camera.z),0});
    return result;
}

void DustRenderer::draw_dust_frame(const DustFrame& frame,Framebuffer& target) noexcept {
    const ScopedLayer layer{target,PixelLayer::world_geometry};
    const auto put=[&](int x,int y,std::uint8_t colour) {
        if(x<frame.exclude_left || x>=frame.exclude_right) target.set(x,y,colour);
    };
    for(std::size_t i=0;i<frame.points.size();++i) {
        const auto& p=frame.points[i];const auto& m=frame.matrix;
        const auto x=source_word_difference(p.x,frame.camera.x);
        const auto y=source_word_difference(p.y,frame.camera.y);
        const auto z=source_word_difference(p.z,frame.camera.z);
        const auto cx=(x*m[0]+y*m[3]+z*m[6])/32768.;
        const auto cy=(x*m[1]+y*m[4]+z*m[7])/32768.;
        const auto cz=(x*m[2]+y*m[5]+z*m[8])/32768.;
        if(cz<256.) continue;
        const auto depth=std::min(cz,4095.);
        const int sx=int(target.width()/2)+frame.offset_x+int(std::trunc(cx*256./depth));
        const int sy=int(target.height()/2)+frame.offset_y+int(std::trunc(cy*256./depth));
        if(sx<0 || sy<0 || sx>=int(target.width()) || sy>=int(target.height())) continue;
        const auto shade=std::clamp(int(depth)>>8,0,15);
        const auto colour=std::uint8_t(112+frame.colours[((frame.points.size()-i)&3U)*16+unsigned(shade)]);
        put(sx,sy,colour);if(cz<1024.) put(sx-1,sy+1,colour);
    }
}

void DustRenderer::draw(
    const simulation::DustSystem& dust,
    std::size_t active_count,
    const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix,
    Framebuffer& target, std::int32_t projection_offset_x,
    std::int32_t projection_offset_y, std::int32_t exclude_left,
    std::int32_t exclude_right) const noexcept {
    // Dust is world-space geometry that happens to address the source raster,
    // so it opts out of 2D presentation filtering and keeps its crisp
    // block-replicated specks.
    const ScopedLayer layer{target, PixelLayer::world_geometry};
    const auto put = [&](std::int32_t x, std::int32_t y, std::uint8_t colour) {
        if (x < exclude_left || x >= exclude_right) target.set(x, y, colour);
    };
    constexpr auto q15 = 32'768.0;
    active_count = std::min(active_count, dust.points().size());
    std::size_t index = 0;
    for (const auto& point : std::span{dust.points()}.first(active_count)) {
        const auto x = source_word_difference(point.x, camera.x);
        const auto y = source_word_difference(point.y, camera.y);
        const auto z = source_word_difference(point.z, camera.z);
        const auto camera_x = (x * view_matrix[0] + y * view_matrix[3]
            + z * view_matrix[6]) / q15;
        const auto camera_y = (x * view_matrix[1] + y * view_matrix[4]
            + z * view_matrix[7]) / q15;
        const auto camera_z = (x * view_matrix[2] + y * view_matrix[5]
            + z * view_matrix[8]) / q15;
        if (camera_z < 256.0) {
            ++index;
            continue;
        }
        const auto clipped_z = std::min(camera_z, 4'095.0);
        const auto screen_x = static_cast<std::int32_t>(target.width() / 2U)
            + projection_offset_x
            + static_cast<std::int32_t>(
                std::trunc(camera_x * 256.0 / clipped_z));
        const auto screen_y = static_cast<std::int32_t>(target.height() / 2U)
            + projection_offset_y
            + static_cast<std::int32_t>(
                std::trunc(camera_y * 256.0 / clipped_z));
        if (screen_x < 0 || screen_x >= static_cast<std::int32_t>(target.width())
            || screen_y < 0 || screen_y >= static_cast<std::int32_t>(target.height())) {
            ++index;
            continue;
        }
        const auto depth = static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(clipped_z) >> 8, 0, 15));
        const auto remaining = active_count - index;
        const auto colour = rom_->read8(star_colours_
            + static_cast<std::uint32_t>((remaining & 3U) * 16U + depth));
        put(screen_x, screen_y,
            static_cast<std::uint8_t>(7U * 16U + colour));
        if (camera_z < 1'024.0) {
            put(screen_x - 1, screen_y + 1,
                static_cast<std::uint8_t>(7U * 16U + colour));
        }
        ++index;
    }
}

void DustRenderer::draw_grid(
    const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix,
    Framebuffer& target) const noexcept {
    const ScopedLayer layer{target, PixelLayer::world_geometry};
    const auto camera_x = camera_word(camera.x);
    const auto camera_y = camera_word(camera.y);
    const auto camera_z = camera_word(camera.z);
    auto row = simulation::transform_q15(view_matrix, {
        grid_start(camera_x),
        simulation::wrap16(-static_cast<std::int32_t>(camera_y)),
        grid_start(camera_z),
    });

    const std::array<std::int16_t, 3> x_step{
        matrix_grid_step(view_matrix[0]),
        matrix_grid_step(view_matrix[1]),
        matrix_grid_step(view_matrix[2]),
    };
    const std::array<std::int16_t, 3> z_step{
        matrix_grid_step(view_matrix[6]),
        matrix_grid_step(view_matrix[7]),
        matrix_grid_step(view_matrix[8]),
    };

    for (std::int16_t grid_z = 0; grid_z < kGridSize; ++grid_z) {
        auto point = row;
        for (std::int16_t grid_x = 0; grid_x < kGridSize; ++grid_x) {
            const auto original_z = point[2];
            if (original_z > 256) {
                const auto depth = std::min<std::int16_t>(
                    original_z, kMaximumReciprocalDepth - 1);
                const auto screen_x = simulation::add16(
                    grid_projection(point[0], depth),
                    static_cast<std::int16_t>(target.width() / 2U));
                const auto screen_y = simulation::add16(
                    grid_projection(point[1], depth),
                    static_cast<std::int16_t>(target.height() / 2U));
                if (static_cast<std::uint16_t>(screen_x) < target.width()
                    && static_cast<std::uint16_t>(screen_y) < target.height()) {
                    constexpr auto colour = static_cast<std::uint8_t>(
                        7U * 16U + 14U);
                    target.set(screen_x, screen_y, colour);
                    if (original_z < 512) {
                        target.set(screen_x - 1, screen_y + 1, colour);
                    }
                }
            }
            for (std::size_t axis = 0; axis < 3U; ++axis) {
                point[axis] = simulation::add16(point[axis], x_step[axis]);
            }
        }
        for (std::size_t axis = 0; axis < 3U; ++axis) {
            row[axis] = simulation::add16(row[axis], z_step[axis]);
        }
    }
}

DustRenderer::GridLinesFrame DustRenderer::prepare_grid_lines(
    const timing::RenderTransform& camera,const simulation::MatrixQ15& matrix,
    std::uint64_t source_frame,std::uint32_t width,std::uint32_t height) const noexcept {
    const auto history=grid_line_history_.begin(source_frame);
    GridLinesFrame result{project_source_grid(camera,matrix,width,height),history.start};
    auto endpoint=history.start;
    if(result.projected.count) {
        const auto& last=result.projected.points[result.projected.count-1];
        endpoint={static_cast<std::int16_t>(last.x-1),last.y};
    }
    grid_line_history_.finish(history,endpoint);
    return result;
}

void DustRenderer::draw_grid_lines(
    const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix,
    std::uint64_t source_frame,
    Framebuffer& target) const noexcept {
    const auto frame = prepare_grid_lines(camera,view_matrix,source_frame,target.width(),target.height());
    draw_grid_lines_frame(frame,target);
}
void DustRenderer::draw_grid_lines_frame(const GridLinesFrame& frame,
    Framebuffer& target,std::uint8_t colour) noexcept {
    const ScopedLayer layer{target, PixelLayer::world_geometry};
    auto previous_x = frame.start[0];
    auto previous_y = frame.start[1];
    const auto source_line = [&target,colour](
                                 std::int16_t current_x,
                                 std::int16_t current_y,
                                 std::int16_t old_x,
                                 std::int16_t old_y) {
        // MSHOWGRID2 starts at the new point and walks left using DX as its
        // loop counter. PLOT advances X, so the pair of DECs before each PLOT
        // has a net one-pixel leftward step. Negative DX deliberately emits
        // only the first pixel at a projected row wrap.
        auto x = static_cast<std::int32_t>(current_x);
        auto y = static_cast<std::int32_t>(current_y);
        const auto dx = static_cast<std::int32_t>(current_x) - old_x;
        const auto absolute_dx = std::abs(dx);
        const auto absolute_dy = std::abs(
            static_cast<std::int32_t>(current_y) - old_y);
        const auto y_step = current_y < old_y ? 1 : -1;
        auto error = absolute_dx;
        auto remaining = dx;
        do {
            target.set(x - 2, y, colour);
            --x;
            error -= absolute_dy;
            if (error < 0) {
                y += y_step;
                error += absolute_dx;
            }
            --remaining;
        } while (remaining >= 0);
    };

    const auto& projected=frame.projected;
    for(std::size_t i=0;i<projected.count;++i) {
        const auto& point=projected.points[i];
        // Source marker, then asymmetric connection back to M_PREVX/M_PREVY.
        const auto adjusted_x=static_cast<std::int16_t>(point.x-1);
        target.set(adjusted_x,point.y+2,colour);
        source_line(adjusted_x,point.y,previous_x,previous_y);
        previous_x=adjusted_x;previous_y=point.y;
        if(point.depth<512) target.set(adjusted_x-1,point.y+1,colour);
    }
}

} // namespace starfox::render
