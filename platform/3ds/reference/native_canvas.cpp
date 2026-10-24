// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#include "native_canvas.hpp"
#include "starfield_3ds.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

namespace starfox::platform_3ds {
namespace {

std::uint8_t dominant_edge(const render::Framebuffer& source,
    std::uint32_t x, std::uint32_t first_y = 0U,
    std::uint32_t last_y = 224U) noexcept {
    std::array<std::uint16_t, 256U> counts{};
    const auto& pixels = source.pixels();
    for (auto y = first_y; y < last_y; ++y)
        ++counts[pixels[static_cast<std::size_t>(y) * 256U + x]];
    return static_cast<std::uint8_t>(std::distance(counts.begin(),
        std::max_element(counts.begin(), counts.end())));
}

std::uint8_t dominant_band(std::span<const std::uint8_t> colours,
    std::uint32_t first_y) noexcept {
    std::array<std::uint8_t, 256U> counts{};
    for (auto y = first_y;
        y < std::min<std::uint32_t>(first_y + 8U, 224U); ++y)
        ++counts[colours[y]];
    return static_cast<std::uint8_t>(std::distance(counts.begin(),
        std::max_element(counts.begin(), counts.end())));
}

std::uint8_t nearest_star_colour(const simulation::SnesPpuState& ppu,
    std::uint32_t target_red, std::uint32_t target_green,
    std::uint32_t target_blue) noexcept {
    std::uint8_t best = 0U;
    std::uint32_t best_distance = 251U;
    for (std::uint32_t index = 1U; index < ppu.cgram.size(); ++index) {
        const auto colour = ppu.cgram[index];
        const auto red = colour & 31U;
        const auto green = (colour >> 5U) & 31U;
        const auto blue = (colour >> 10U) & 31U;
        // The map's authored stars use muted sage-gray arms and a lighter
        // centre. Exclude the white menu font and the dark slate backdrop.
        if (std::min({red, green, blue}) < 9U
            || std::max({red, green, blue}) > 27U) continue;
        const auto delta_red = static_cast<std::int32_t>(red)
            - static_cast<std::int32_t>(target_red);
        const auto delta_green = static_cast<std::int32_t>(green)
            - static_cast<std::int32_t>(target_green);
        const auto delta_blue = static_cast<std::int32_t>(blue)
            - static_cast<std::int32_t>(target_blue);
        const auto distance = static_cast<std::uint32_t>(
            delta_red * delta_red + delta_green * delta_green
                + delta_blue * delta_blue);
        if (distance < best_distance) {
            best_distance = distance;
            best = static_cast<std::uint8_t>(index);
        }
    }
    return best;
}

std::uint8_t nearest_colour(const simulation::SnesPpuState& ppu,
    int red,int green,int blue) noexcept {
    std::uint8_t best=0U;
    unsigned best_distance=~0U;
    for(unsigned i=1;i<ppu.cgram.size();++i) {
        const auto c=ppu.cgram[i];
        const int r=int(c&31U),g=int((c>>5U)&31U),b=int((c>>10U)&31U);
        if(std::max({r,g,b})<8) continue;
        const unsigned distance=unsigned((r-red)*(r-red)
            +(g-green)*(g-green)+(b-blue)*(b-blue));
        if(distance<best_distance) {
            best_distance=distance;best=static_cast<std::uint8_t>(i);
        }
    }
    return best;
}

struct StarPoint {
    std::uint16_t x;
    std::uint8_t y;
    bool cross;
};

// Hand-placed so there are no adjoining dots or accidentally elongated
// clusters. The four crosses on the map follow the source sprite's exact
// five-pixel silhouette; the remaining stars are individual pixels.
constexpr std::array<StarPoint, 22U> map_stars{{
    {12, 19, false}, {49, 30, false}, {25, 53, true},
    {59, 72, false}, {9, 96, false}, {45, 112, false},
    {18, 139, true}, {57, 157, false}, {34, 180, false},
    {7, 209, false}, {56, 222, false},
    {345, 15, false}, {384, 37, false}, {365, 55, true},
    {338, 83, false}, {390, 102, false}, {355, 120, false},
    {379, 145, true}, {341, 170, false}, {392, 185, false},
    {361, 211, false}, {387, 227, false},
}};
constexpr std::array<StarPoint, 8U> controls_stars{{
    {13, 30, false}, {28, 92, true}, {8, 170, false},
    {24, 215, false}, {373, 25, false}, {389, 86, true},
    {367, 160, false}, {382, 220, false},
}};
constexpr std::array<StarPoint, 14U> space_stars{{
    {10, 12, false}, {52, 37, false}, {27, 68, true},
    {61, 101, false}, {14, 151, false}, {46, 186, false},
    {23, 224, false}, {345, 22, false}, {378, 48, false},
    {352, 83, true}, {390, 122, false}, {339, 159, false},
    {374, 194, false}, {395, 225, false},
}};

// Precompute only the native side columns once; the source 256-pixel sky
// remains untouched. This gives both columns the measured cartridge density.
const std::vector<StarPoint>& game_over_stars() {
    static const auto stars = [] {
        std::vector<StarPoint> result;
        for (unsigned y=0;y<240U;++y)
            for (unsigned x=0;x<400U;++x)
                if ((x<72U || x>=328U)
                    && (game_over_star_hash(x,y)&4095U)<11U)
                    result.push_back({static_cast<std::uint16_t>(x),
                        static_cast<std::uint8_t>(y),false});
        return result;
    }();
    return stars;
}

bool is_isolated_map_artifact(const render::Framebuffer& source,
    std::uint8_t backdrop, const simulation::SnesPpuState& ppu) noexcept {
    // Planet selection sometimes leaves one wrapped eight-pixel OBJ scanline
    // at the original raster's top-left corner. Only remove that exact,
    // isolated neon-green row; legitimate planet art remains untouched.
    const auto& pixels = source.pixels();
    const auto stray = pixels[7U * 256U];
    if (stray == backdrop || pixels[7U * 256U + 8U] != backdrop)
        return false;
    const auto colour = ppu.cgram[stray];
    const auto red = colour & 31U;
    const auto green = (colour >> 5U) & 31U;
    const auto blue = (colour >> 10U) & 31U;
    if (green < 20U || green < red * 2U || green < blue * 2U)
        return false;
    for (std::uint32_t x = 0U; x < 8U; ++x) {
        if (pixels[7U * 256U + x] != stray
            || pixels[6U * 256U + x] != backdrop
            || pixels[8U * 256U + x] != backdrop) return false;
    }
    return true;
}

} // namespace

void compose_mode2_background(const render::Framebuffer& source,
    render::Framebuffer& target) noexcept {
    if(source.width()!=400U || source.height()!=224U || source.draw_scale()!=1U
        || target.width()!=400U || target.height()!=240U || target.draw_scale()!=1U) return;
    const auto* pixels=source.pixels().data();auto* output=target.pixels().data();
    std::copy_n(pixels,400U*224U,output+400U*8U);
    for(unsigned y=0;y<8;++y) {
        std::copy_n(pixels,400U,output+y*400U);
        std::copy_n(pixels+223U*400U,400U,output+(232U+y)*400U);
    }
    target.mark_written(0,400U*240U);
}

void compose_native_canvas(const render::Framebuffer& source,
    const render::Framebuffer& world, render::Framebuffer& target,
    const simulation::SnesPpuState& ppu, NativeCanvasStyle style,
    std::span<const std::uint8_t> background_left,
    std::span<const std::uint8_t> background_right,
    int intro_vertical_shift) noexcept {
    if (source.width() != 256U || source.height() != 224U
        || world.width() != 400U || world.height() != 240U
        || target.width() != 400U || target.height() != 240U) return;
    const auto& source_pixels = source.pixels();
    const auto& world_pixels = world.pixels();
    auto& pixels = target.pixels();
    constexpr auto left = 72U;
    constexpr auto right = 328U;
    const auto fill_left = dominant_edge(source, 0U);
    const auto fill_right = dominant_edge(source, 255U);
    const auto spread_controls = style == NativeCanvasStyle::controls
        && fill_left == fill_right
        && [&] {
            for (std::uint32_t y = 0U; y < 224U; ++y) {
                for (std::uint32_t x = 0U; x < 16U; ++x) {
                    if (source_pixels[static_cast<std::size_t>(y) * 256U + x]
                        != fill_left) return false;
                }
            }
            return true;
        }();
    const bool extend_world = style == NativeCanvasStyle::gameplay
        || style == NativeCanvasStyle::intro;
    // A source eight-line band covers eight output rows (sixteen at the
    // clamped ends). Build its histogram once, not once per output row.
    // Recompute every frame: source/edge pixels can change independently of
    // the palette, scroll or scene revision. Lowest-index tie ordering stays
    // identical to the original per-row max_element selection.
    std::array<std::uint8_t, 28U> left_bands{}, right_bands{};
    if (extend_world) for (std::uint32_t band = 0; band < left_bands.size(); ++band) {
        const auto first = band * 8U;
        left_bands[band] = background_left.size() == 224U
            ? dominant_band(background_left, first)
            : dominant_edge(source, 0U, first, first + 8U);
        right_bands[band] = background_right.size() == 224U
            ? dominant_band(background_right, first)
            : dominant_edge(source, 255U, first, first + 8U);
    }
    for (std::uint32_t y = 0U; y < 240U; ++y) {
        auto* row = pixels.data() + static_cast<std::size_t>(y) * 400U;
        auto left_colour = fill_left;
        auto right_colour = fill_right;
        if (extend_world) {
            // Mode 1 can have a moving sky or floor. Continue its edge colour
            // in eight-line bands; an isolated star or model pixel must not
            // become a 72-pixel horizontal streak.
            const auto source_y = std::clamp<std::int32_t>(
                static_cast<std::int32_t>(y) - 8, 0, 223);
            const auto band = static_cast<std::uint32_t>(source_y) / 8U;
            left_colour = left_bands[band];
            right_colour = right_bands[band];
        }
        std::fill_n(row, left, left_colour);
        std::fill_n(row + right, 400U - right, right_colour);
        if (y >= 8U && y < 232U) {
            const auto* source_row = source_pixels.data()
                + static_cast<std::size_t>(y - 8U) * 256U;
            if (spread_controls) {
                // The controls screen has two independent source panels and
                // a solid slate field. Move those panels as intact pixel
                // blocks into the unused native columns; no artwork scales.
                std::fill_n(row + left, 256U, fill_left);
                std::copy_n(source_row + 16U, 136U, row + 48U);
                std::copy_n(source_row + 152U, 104U, row + 256U);
            } else {
                std::copy_n(source_row, 256U, row + left);
                if (style == NativeCanvasStyle::star_map && y == 15U
                    && is_isolated_map_artifact(source, fill_left, ppu))
                    std::fill_n(row + left, 8U, fill_left);
            }
        } else {
            std::fill_n(row + left, 256U,
                (style == NativeCanvasStyle::gameplay
                    || style == NativeCanvasStyle::intro)
                    ? left_colour : y < 8U ? fill_left : fill_right);
        }
        if (extend_world) {
            const auto* world_row = world_pixels.data()
                + static_cast<std::size_t>(y) * 400U;
            const auto copy_world = [&](std::uint32_t begin, std::uint32_t end) {
                for (auto x = begin; x < end; ++x)
                    if (world_row[x] != 0U) row[x] = world_row[x];
            };
            if (y >= 8U && y < 232U) {
                copy_world(0U, left);
                copy_world(right, 400U);
            } else copy_world(0U, 400U);
        }
    }
    if (style == NativeCanvasStyle::intro
        && !std::all_of(source_pixels.begin(), source_pixels.end(),
            [](std::uint8_t pixel) { return pixel == 0U; })) {
        // Sparse stars fill any black margin left empty by the live 3D dust.
        // Their vertical phase follows the camera's projected horizon, so
        // they rise with the centre during the planet approach.
        struct IntroStar {std::uint16_t x,y;std::uint8_t shade;};
        static const auto margin_stars=[] {
            std::vector<IntroStar> result;
            for(unsigned y=8U;y<232U;++y) for(unsigned x=0U;x<400U;++x) {
                if(x>=72U&&x<328U) {x=327U;continue;}
                const auto shade=game_over_star_hash(x,y,0x3c6ef372U);
                if((shade&4095U)<7U)
                    result.push_back({static_cast<std::uint16_t>(x),
                        static_cast<std::uint16_t>(y),
                        static_cast<std::uint8_t>((shade>>12U)%37U)});
            }
            return result;
        }();
        const std::array<std::uint8_t,4> colors{{
            nearest_colour(ppu,27,28,28), nearest_colour(ppu,15,19,27),
            nearest_colour(ppu,28,19,8), nearest_colour(ppu,25,9,8)}};
        for(const auto& star:margin_stars) {
            const auto shifted=int(star.y)-8+intro_vertical_shift;
            const auto y=8+(shifted%224+224)%224;
            const auto offset=std::size_t(y)*400U+star.x;
            if(world_pixels[offset]!=0U) continue;
            const auto existing=pixels[offset];
            const auto colour=ppu.cgram[existing];
            if(std::max({colour&31U,(colour>>5U)&31U,
                    (colour>>10U)&31U})>4U) continue;
            pixels[offset]=colors[star.shade==0U?3U:star.shade<3U?2U:
                star.shade<9U?1U:0U];
        }
    }
    std::span<const StarPoint> stars{};
    if (style == NativeCanvasStyle::star_map) stars = map_stars;
    else if (style == NativeCanvasStyle::controls) stars = controls_stars;
    else if (style == NativeCanvasStyle::space) stars = space_stars;
    else if (style == NativeCanvasStyle::game_over) stars = game_over_stars();
    if (stars.empty()
        || std::all_of(source_pixels.begin(), source_pixels.end(),
            [](std::uint8_t pixel) { return pixel == 0U; })) return;
    if(style==NativeCanvasStyle::game_over) {
        const std::array<std::uint8_t,4> colors{{
            nearest_colour(ppu,12,12,12),nearest_colour(ppu,24,24,24),
            nearest_colour(ppu,28,11,10),nearest_colour(ppu,29,22,9)}};
        for(std::size_t i=0;i<stars.size();++i) {
            const auto& point=stars[i];
            pixels[std::size_t(point.y)*400U+point.x]
                =colors[i%13U==0U?2U:i%17U==0U?3U:i%3U==0U?1U:0U];
        }
        return;
    }
    const auto arm = nearest_star_colour(ppu, 16U, 18U, 16U);
    const auto centre = nearest_star_colour(ppu, 23U, 25U, 23U);
    if (arm == 0U) return;
    for (std::size_t i=0;i<stars.size();++i) {
        const auto& point=stars[i];
        const auto x = static_cast<std::size_t>(point.x);
        const auto y = static_cast<std::size_t>(point.y);
        const auto offset = y * 400U + x;
        if (style == NativeCanvasStyle::game_over) {
            const auto shade = game_over_star_hash(point.x,point.y) >> 12U;
            pixels[offset] = shade % 17U == 0U
                ? nearest_colour(ppu,10,20,29)
                : shade % 2U == 0U && centre != 0U ? centre : arm;
        } else pixels[offset] = point.cross && centre != 0U ? centre : arm;
        if (point.cross) {
            pixels[offset - 400U] = arm;
            pixels[offset + 400U] = arm;
            pixels[offset - 1U] = arm;
            pixels[offset + 1U] = arm;
        }
    }
}

void overlay_source_coverage_on_native(const render::Framebuffer& source,
    render::Framebuffer& target) noexcept {
    if (source.width() != 256U || source.height() != 224U
        || target.width() != 400U || target.height() != 240U) return;
    const auto coverage = source.write_coverage();
    const auto& source_pixels = source.pixels();
    if (coverage.size() != source_pixels.size()) return;
    auto& destination = target.pixels();
    auto& destination_tags = target.layer_tags();
    const auto& source_tags = source.layer_tags();
    const bool tagged = target.layer_tags_enabled()
        && destination_tags.size() == destination.size();
    const bool source_tagged = source.layer_tags_enabled()
        && source_tags.size() == source_pixels.size();
    for (std::size_t y = 0U; y < 224U; ++y) {
        const auto source_row = y * 256U;
        const auto destination_row = (y + 8U) * 400U + 72U;
        for (std::size_t x = 0U; x < 256U;) {
            while (x < 256U && coverage[source_row + x] == 0U) ++x;
            const auto first = x;
            while (x < 256U && coverage[source_row + x] != 0U) ++x;
            if (first == x) continue;
            // Bulk spans avoid one bounds check and coverage fill per pixel.
            std::copy_n(source_pixels.data() + source_row + first,
                x - first, destination.data() + destination_row + first);
            target.mark_written(destination_row + first, x - first);
            if (tagged) {
                if (source_tagged)
                    std::copy_n(source_tags.data() + source_row + first,
                        x - first, destination_tags.data()
                            + destination_row + first);
                else std::fill_n(destination_tags.data()
                    + destination_row + first, x - first,
                    static_cast<std::uint8_t>(render::PixelLayer::three_d));
            }
        }
    }
}

} // namespace starfox::platform_3ds
