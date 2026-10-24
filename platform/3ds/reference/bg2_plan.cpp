// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#include "bg2_plan.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace starfox::platform_3ds {
namespace {

std::uint16_t vram_word(const simulation::SnesPpuState& ppu,
    std::uint32_t address) noexcept {
    const auto byte = (address & 0x7fffU) * 2U;
    return static_cast<std::uint16_t>(ppu.vram[byte])
        | (static_cast<std::uint16_t>(ppu.vram[byte + 1U]) << 8U);
}

} // namespace

bool plan_bg2_rects(const simulation::SnesPpuState& ppu,
    std::int32_t scroll_x, std::int32_t scroll_y,
    std::vector<Bg2Rect>& output, std::size_t capacity,
    std::uint32_t target_width, std::uint32_t target_height,
    std::int32_t origin_x, std::int32_t origin_y,
    bool cache_rolled_ground_samples, bool compact_uniform_tiles) {
    output.clear();
    if (ppu.background_mode != 2U || (ppu.mosaic & 0x02U) != 0U)
        return false;
    if (target_width == 0U || target_height == 0U
        || target_width > 400U || target_height > 240U) return false;
    if ((ppu.main_screen & 0x02U) == 0U) return true;

    const auto width_tiles = (ppu.bg2_screen_size & 1U) != 0U ? 64U : 32U;
    const auto height_tiles = (ppu.bg2_screen_size & 2U) != 0U ? 64U : 32U;
    const auto pages_wide = width_tiles / 32U;
    const auto tile_edge = ppu.bg2_tile_size_16 ? 16U : 8U;
    const auto tile_shift = ppu.bg2_tile_size_16 ? 4U : 3U;
    const auto x_mask = width_tiles * tile_edge - 1U;
    const auto y_mask = height_tiles * tile_edge - 1U;
    std::array<std::uint16_t, 32U> vertical_offsets{};
    if (ppu.bg2_vertical_offsets_enabled) {
        for (std::uint32_t index = 0U; index < vertical_offsets.size(); ++index)
            vertical_offsets[index] = vram_word(ppu, 0x2fa0U + index);
    }
    std::array<std::int32_t, 240U> row_scroll_x_values{};
    std::array<std::int32_t, 240U> row_scroll_y_values{};
    for (std::uint32_t y = 0U; y < target_height; ++y) {
        const auto source_y = static_cast<std::int32_t>(y) - origin_y;
        row_scroll_x_values[y] = ppu.bg2_horizontal_offsets_enabled
                && source_y >= 0 && source_y < 224
            ? static_cast<std::int32_t>(ppu.bg2_horizontal_offsets[
                static_cast<std::size_t>(source_y)]) : scroll_x;
        const auto clamped_source_y = std::clamp<std::int32_t>(
            source_y, 0, 223);
        row_scroll_y_values[y] = ppu.bg2_scanline_scroll_enabled
            ? static_cast<std::int32_t>(ppu.bg2_scanline_scroll_y[
                static_cast<std::size_t>(clamped_source_y)]) : scroll_y;
    }

    // The cartridge provides a per-column roll table only for its original
    // 256-pixel window. Leaving the new side columns on BG2VOFS produces a
    // visible 256-pixel rectangle. Continue the PC port's fitted horizon
    // through the full native viewport, at pixel precision.
    const bool wide_roll = target_width > 256U
        && ppu.bg2_vertical_offsets_enabled;
    double sum_x = 0.0, sum_y = 0.0, sum_xx = 0.0, sum_xy = 0.0;
    std::size_t fitted_samples = 0U;
    std::int32_t previous_raw = 0, previous_unwrapped = 0;
    bool have_previous = false;
    if (wide_roll) {
        for (std::size_t index = 0U; index < vertical_offsets.size(); ++index) {
            const auto entry = vertical_offsets[index];
            if ((entry & 0x4000U) == 0U) continue;
            const auto period = ppu.tunnel_scene ? 8192 : static_cast<int>(y_mask + 1U);
            const auto raw = static_cast<std::int32_t>(entry & (period - 1));
            auto difference = (raw - previous_raw) & (period - 1);
            if (difference > period / 2 - 1) difference -= period;
            const auto unwrapped = have_previous
                ? previous_unwrapped + difference : raw;
            const auto column = static_cast<double>(index + 1U);
            sum_x += column;
            sum_y += unwrapped;
            sum_xx += column * column;
            sum_xy += column * unwrapped;
            ++fitted_samples;
            previous_raw = raw;
            previous_unwrapped = unwrapped;
            have_previous = true;
        }
    }
    double fitted_slope = 0.0, fitted_intercept = 0.0;
    if (fitted_samples != 0U) {
        const auto count = static_cast<double>(fitted_samples);
        const auto denominator = count * sum_xx - sum_x * sum_x;
        fitted_slope = fitted_samples > 1U && denominator != 0.0
            ? (count * sum_xy - sum_x * sum_y) / denominator : 0.0;
        fitted_intercept = (sum_y - fitted_slope * sum_x) / count;
    }
    constexpr auto no_column_offset = std::numeric_limits<std::int32_t>::min();
    // The shared renderer resolves offsets at each scanline's fine horizontal
    // phase. A frame-wide phase and signed lround disagree at rolled map seams.
    // Lazily resolve only phases actually requested, retaining bounded scratch.
    std::array<std::array<std::int32_t, 400U>,8> phase_columns;
    std::array<bool,8> phase_ready{};
    const auto columns_for = [&](int row_scroll_x) -> const std::array<std::int32_t,400U>& {
        const auto phase=unsigned(row_scroll_x)&7U;
        auto& column_offsets=phase_columns[phase];
        if(phase_ready[phase]) return column_offsets;
        phase_ready[phase]=true;column_offsets.fill(no_column_offset);
        if (fitted_samples != 0U) {
        for (std::uint32_t x = 0U; x < target_width; ++x) {
            const auto logical_x = static_cast<std::int32_t>(x) - origin_x;
            const auto visible_column = static_cast<double>(logical_x
                + int(phase)) / 8.0;
            const auto fitted=fitted_intercept+fitted_slope*visible_column;
            const auto magnitude=std::abs(fitted);
            auto rounded=static_cast<std::int32_t>(magnitude);
            const auto fraction=magnitude-rounded;
            rounded+=!ppu.tunnel_scene && fitted<0 ? fraction>.5 : fraction>=.5;
            auto offset=fitted<0 ? -rounded : rounded;
            offset %= 8192;
            if (offset < 0) offset += 8192;
            column_offsets[x] = offset;
        }
        }
        return column_offsets;
    };

    auto& planned = output;
    planned.reserve(std::min<std::size_t>(capacity, 16384U));
    // Rejecting the raw HDMA strips before merging them forced flat EX
    // backgrounds through a second, complete CPU raster. Decode each observed
    // character once, and merge only opaque identical horizontal spans. Zero
    // stays a textured fragment, retaining the atlas's transparency contract.
    std::array<std::int8_t, 1024U> uniform_characters;
    uniform_characters.fill(-2);
    const auto uniform_character = [&](std::uint16_t character) {
        auto& cached = uniform_characters[character];
        if (cached != -2) return cached;
        const auto base = static_cast<std::uint32_t>(ppu.bg2_character_base)
            * 2U + static_cast<std::uint32_t>(character) * 32U;
        unsigned colour = 0U;
        constexpr std::array<std::uint32_t, 4U> planes{0U, 1U, 16U, 17U};
        for (unsigned plane = 0U; plane < planes.size(); ++plane) {
            const auto first = ppu.vram[(base + planes[plane]) & 0xffffU];
            if (first != 0U && first != 255U) { cached = -1; return cached; }
            for (unsigned row = 1U; row < 8U; ++row) {
                if (ppu.vram[(base + planes[plane] + row * 2U) & 0xffffU]
                    != first) { cached = -1; return cached; }
            }
            if (first != 0U) colour |= 1U << plane;
        }
        cached = static_cast<std::int8_t>(colour);
        return cached;
    };
    constexpr auto no_run = std::numeric_limits<std::size_t>::max();
    std::array<std::size_t, 240U> uniform_run_by_y;
    uniform_run_by_y.fill(no_run);
    for (std::uint32_t band_top = 0U; band_top < target_height;) {
        const auto band_scroll_x = row_scroll_x_values[band_top];
        const auto band_scroll_y = row_scroll_y_values[band_top];
        const auto offset_phase=unsigned(band_scroll_x)&7U;
        const auto& column_offsets=columns_for(band_scroll_x);
        auto band_end = band_top + 1U;
        while (band_end < target_height
            && (!ppu.bg2_horizontal_offsets_enabled
                || row_scroll_x_values[band_end] == band_scroll_x)
            && (!ppu.bg2_scanline_scroll_enabled
                || row_scroll_y_values[band_end] == band_scroll_y)) {
            ++band_end;
        }

        for (std::uint32_t x = 0U; x < target_width;) {
            const auto logical_x = static_cast<std::int32_t>(x) - origin_x;
            const auto source_x = static_cast<std::uint32_t>(logical_x
                + band_scroll_x) & x_mask;
            const auto column_coordinate = static_cast<std::uint32_t>(
                logical_x) + offset_phase;
            const auto column = logical_x >= 0
                ? (static_cast<std::uint32_t>(logical_x) + offset_phase) / 8U
                : 33U;
            const auto index = column == 0U ? 0U : column - 1U;
            auto column_scroll_y = band_scroll_y;
            if (column_offsets[x] != no_column_offset) {
                column_scroll_y = column_offsets[x];
            } else if (ppu.bg2_vertical_offsets_enabled
                && index < vertical_offsets.size()
                && (vertical_offsets[index] & 0x4000U) != 0U) {
                column_scroll_y = vertical_offsets[index] & 0x1fffU;
            }
            auto width = std::min<std::uint32_t>({target_width - x,
                8U - (source_x & 7U), 8U - (column_coordinate & 7U)});
            if (fitted_samples != 0U) {
                for (std::uint32_t dx = 1U; dx < width; ++dx) {
                    if (column_offsets[x + dx] != column_scroll_y) {
                        width = dx;
                        break;
                    }
                }
            }
            if (logical_x < 0 && origin_x > 0)
                width = std::min(width,
                    static_cast<std::uint32_t>(origin_x - static_cast<std::int32_t>(x)));
            for (auto y = band_top; y < band_end;) {
                const auto logical_y = static_cast<std::int32_t>(y) - origin_y;
                const auto source_y = static_cast<std::uint32_t>(logical_y
                    + column_scroll_y) & y_mask;
                const auto height = std::min(band_end - y,
                    8U - (source_y & 7U));
                const auto tile_x = source_x >> tile_shift;
                const auto tile_y = source_y >> tile_shift;
                const auto page = (tile_x >> 5U)
                    + (tile_y >> 5U) * pages_wide;
                const auto entry = page * 0x400U
                    + (tile_y & 31U) * 32U + (tile_x & 31U);
                const auto tile = vram_word(ppu,
                    static_cast<std::uint32_t>(ppu.bg2_screen_base) + entry);
                const auto reverse_x = (tile & 0x4000U) != 0U;
                const auto reverse_y = (tile & 0x8000U) != 0U;
                auto character = static_cast<std::uint16_t>(tile & 0x03ffU);
                if (ppu.bg2_tile_size_16) {
                    const auto sub_x = reverse_x
                        ? 15U - (source_x & 15U) : (source_x & 15U);
                    const auto sub_y = reverse_y
                        ? 15U - (source_y & 15U) : (source_y & 15U);
                    character = static_cast<std::uint16_t>((character
                        + (sub_x >= 8U ? 1U : 0U)
                        + (sub_y >= 8U ? 16U : 0U)) & 0x03ffU);
                }
                Bg2Rect rectangle{
                    static_cast<std::uint16_t>(x),
                    static_cast<std::uint16_t>(y),
                    static_cast<std::uint16_t>(width),
                    static_cast<std::uint16_t>(height),
                    character,
                    static_cast<std::uint8_t>((tile >> 10U) & 7U),
                    static_cast<std::uint8_t>(reverse_x
                        ? 7U - (source_x & 7U) : (source_x & 7U)),
                    static_cast<std::uint8_t>(reverse_y
                        ? 7U - (source_y & 7U) : (source_y & 7U)),
                    reverse_x, reverse_y};
                if (compact_uniform_tiles) {
                    const auto uniform = uniform_character(character);
                    if (uniform > 0) {
                        rectangle.solid = true;
                        rectangle.solid_colour = static_cast<std::uint8_t>(
                            rectangle.palette_bank * 16U + uniform);
                        auto& last_index = uniform_run_by_y[y];
                        if (last_index != no_run) {
                            auto& last = planned[last_index];
                            if (last.x + last.width == x
                                && last.height == height
                                && last.solid_colour == rectangle.solid_colour) {
                                last.width = static_cast<std::uint16_t>(
                                    last.width + width);
                                y += height;
                                continue;
                            }
                        }
                        if (planned.size() < capacity) last_index = planned.size();
                    }
                }
                if (planned.size() == capacity) {
                    output.clear();
                    return false;
                }
                planned.push_back(rectangle);
                y += height;
            }
            x += width;
        }
        band_top = band_end;
    }

    // A low camera can scroll the authored green floor past the 512-line
    // tilemap boundary. The next wrapped rows are blue sky. Match the CPU
    // renderer's per-column carry only at the lower edge, after all ordinary
    // BG2 fragments, so foreground models and sprites retain their priority.
    if (wide_roll && target_height > 192U) {
        // The carry resolver may sample the same map entry and 8x8 character
        // dozens of times while it walks down each rolled column. These
        // caches are scoped to one immutable PPU snapshot, so no revision or
        // cross-frame invalidation is needed. Pixel storage stays
        // uninitialized until its validity bit is set.
        std::array<std::uint16_t, 4096U> sampled_tile_words;
        std::array<std::uint64_t, 64U> sampled_tile_words_valid{};
        std::array<std::uint8_t, 1024U * 64U> sampled_character_pixels;
        std::array<std::uint64_t, 16U> sampled_characters_valid{};
        // -2: not decoded; -1: mixed; 0..15: identical texels. Ground and
        // sky characters are often flat even when the camera rolls, so their
        // sample is independent of source coordinates and tile flips.
        // Consecutive pixels in a rolled column often share one map entry.
        std::uint32_t last_tile_entry = 4096U;
        std::uint16_t last_tile_word = 0U;
        const auto sample_colour = [&](std::uint32_t source_x,
                                       std::uint32_t source_y, bool* uniform_sample = nullptr) {
            if (uniform_sample) *uniform_sample = false;
            const auto tile_x = source_x >> tile_shift;
            const auto tile_y = source_y >> tile_shift;
            const auto page = (tile_x >> 5U) + (tile_y >> 5U) * pages_wide;
            const auto entry = page * 0x400U
                + (tile_y & 31U) * 32U + (tile_x & 31U);
            std::uint16_t tile{};
            if (cache_rolled_ground_samples) {
                if (entry == last_tile_entry) {
                    tile = last_tile_word;
                } else {
                    const auto word_mask = std::uint64_t{1} << (entry & 63U);
                    auto& word_bits = sampled_tile_words_valid[entry >> 6U];
                    if ((word_bits & word_mask) != 0U) {
                        tile = sampled_tile_words[entry];
                    } else {
                        tile = vram_word(ppu,
                            static_cast<std::uint32_t>(ppu.bg2_screen_base)
                                + entry);
                        sampled_tile_words[entry] = tile;
                        word_bits |= word_mask;
                    }
                    last_tile_entry = entry;
                    last_tile_word = tile;
                }
            } else {
                tile = vram_word(ppu,
                    static_cast<std::uint32_t>(ppu.bg2_screen_base) + entry);
            }
            // Full 8x8 characters of one colour do not depend on pixel
            // coordinates or flips. Their palette still belongs to this tile.
            if (cache_rolled_ground_samples && !ppu.bg2_tile_size_16) {
                const auto uniform = uniform_characters[tile & 0x03ffU];
                if (uniform >= 0) {
                    if (uniform_sample) *uniform_sample = true;
                    return uniform == 0 ? std::uint8_t{0U}
                        : static_cast<std::uint8_t>(
                            ((tile >> 10U) & 7U) * 16U + uniform);
                }
            }
            auto pixel_x = source_x & (tile_edge - 1U);
            auto pixel_y = source_y & (tile_edge - 1U);
            if ((tile & 0x4000U) != 0U) pixel_x = tile_edge - 1U - pixel_x;
            if ((tile & 0x8000U) != 0U) pixel_y = tile_edge - 1U - pixel_y;
            auto character = static_cast<std::uint32_t>(tile & 0x03ffU);
            if (ppu.bg2_tile_size_16) character = (character
                + (pixel_x >= 8U ? 1U : 0U)
                + (pixel_y >= 8U ? 16U : 0U)) & 0x03ffU;
            pixel_x &= 7U;
            pixel_y &= 7U;
            std::uint8_t colour{};
            if (cache_rolled_ground_samples) {
                const auto character_mask = std::uint64_t{1}
                    << (character & 63U);
                auto& character_bits =
                    sampled_characters_valid[character >> 6U];
                if ((character_bits & character_mask) == 0U) {
                    int uniform = -2;
                    for (std::uint32_t row = 0U; row < 8U; ++row) {
                        const auto base = (static_cast<std::uint32_t>(
                            ppu.bg2_character_base) + character * 16U + row)
                            * 2U;
                        const auto low_0 = ppu.vram[base & 0xffffU];
                        const auto low_1 = ppu.vram[(base + 1U) & 0xffffU];
                        const auto high_0 = ppu.vram[(base + 16U) & 0xffffU];
                        const auto high_1 = ppu.vram[(base + 17U) & 0xffffU];
                        for (std::uint32_t column = 0U; column < 8U;
                             ++column) {
                            const auto mask = static_cast<std::uint8_t>(
                                0x80U >> column);
                            sampled_character_pixels[character * 64U
                                + row * 8U + column] =
                                static_cast<std::uint8_t>(
                                    ((low_0 & mask) != 0U ? 1U : 0U)
                                    | ((low_1 & mask) != 0U ? 2U : 0U)
                                    | ((high_0 & mask) != 0U ? 4U : 0U)
                                    | ((high_1 & mask) != 0U ? 8U : 0U));
                            const auto decoded = sampled_character_pixels[
                                character * 64U + row * 8U + column];
                            if (uniform == -2) uniform = decoded;
                            else if (uniform != decoded) uniform = -1;
                        }
                    }
                    uniform_characters[character] = static_cast<std::int8_t>(uniform);
                    character_bits |= character_mask;
                }
                colour = sampled_character_pixels[character * 64U
                    + pixel_y * 8U + pixel_x];
            } else {
                const auto base = (static_cast<std::uint32_t>(
                    ppu.bg2_character_base) + character * 16U + pixel_y) * 2U;
                const auto mask = static_cast<std::uint8_t>(0x80U >> pixel_x);
                const auto bit = [&](std::uint32_t offset) {
                    return (ppu.vram[(base + offset) & 0xffffU] & mask) != 0U;
                };
                colour = static_cast<std::uint8_t>(
                    (bit(0U) ? 1U : 0U) | (bit(1U) ? 2U : 0U)
                    | (bit(16U) ? 4U : 0U) | (bit(17U) ? 8U : 0U));
            }
            if (uniform_sample && cache_rolled_ground_samples)
                *uniform_sample = uniform_characters[character] >= 0;
            return colour == 0U ? std::uint8_t{0U}
                : static_cast<std::uint8_t>(((tile >> 10U) & 7U) * 16U
                    + colour);
        };
        std::array<std::size_t, 240U> solid_run_by_y{};
        solid_run_by_y.fill(std::numeric_limits<std::size_t>::max());
        const auto append_solid = [&](std::uint32_t x, std::uint32_t y,
                                      std::uint32_t height,
                                      std::uint8_t colour) {
            if (height == 0U) return true;
            // Corrections are produced in column order, with several runs
            // possible in one column. Remember the most recent correction
            // beginning at each row so matching runs from adjacent columns
            // can merge even when another run was emitted between them.
            constexpr auto no_solid_run =
                std::numeric_limits<std::size_t>::max();
            auto& last_index = solid_run_by_y[y];
            if (last_index != no_solid_run) {
                auto& last = planned[last_index];
                if (last.x + last.width == x && last.y == y
                    && last.height == height
                    && last.solid_colour == colour) {
                    ++last.width;
                    return true;
                }
            }
            if (planned.size() == capacity) return false;
            Bg2Rect rect{};
            rect.x = static_cast<std::uint16_t>(x);
            rect.y = static_cast<std::uint16_t>(y);
            rect.width = 1U;
            rect.height = static_cast<std::uint16_t>(height);
            rect.solid = true;
            rect.solid_colour = colour;
            last_index = planned.size();
            planned.push_back(rect);
            return true;
        };
        const auto first_ground_y = std::min(target_height,
            static_cast<std::uint32_t>(std::max<std::int32_t>(
                origin_y + 144, 0)));
        const bool constant_phase=std::all_of(row_scroll_x_values.begin(),row_scroll_x_values.begin()+target_height,
            [&](int value){return (unsigned(value)&7U)==(unsigned(row_scroll_x_values[0])&7U);});
        for (std::uint32_t x = 0U; x < target_width; ++x) {
            const auto logical_x = static_cast<std::int32_t>(x) - origin_x;
            const auto offset_phase=unsigned(row_scroll_x_values[0])&7U;
            const auto& column_offsets=columns_for(row_scroll_x_values[0]);
            const auto column = logical_x >= 0
                ? (static_cast<std::uint32_t>(logical_x) + offset_phase) / 8U
                : 33U;
            const auto index = column == 0U ? 0U : column - 1U;
            const bool fitted_column =
                column_offsets[x] != no_column_offset;
            const bool table_column = index < vertical_offsets.size()
                && (vertical_offsets[index] & 0x4000U) != 0U;
            const bool fixed_vertical_scroll = constant_phase && (fitted_column || table_column
                || !ppu.bg2_scanline_scroll_enabled);
            const auto fixed_scroll_y = fitted_column
                ? column_offsets[x]
                : table_column
                    ? static_cast<std::int32_t>(vertical_offsets[index]
                        & 0x1fffU)
                    : scroll_y;
            const auto scroll_at = [&](std::uint32_t y) {
                const auto& columns=columns_for(row_scroll_x_values[y]);
                if (columns[x]!=no_column_offset) return columns[x];
                if (table_column) return fixed_scroll_y;
                return row_scroll_y_values[y];
            };
            const auto source_y_at = [&](std::uint32_t y) {
                return static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(y) - origin_y
                    + scroll_at(y)) & y_mask;
            };
            const auto fixed_source_y_at = [&](std::uint32_t y) {
                return static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(y) - origin_y
                    + fixed_scroll_y) & y_mask;
            };
            const auto colour_at = [&](std::uint32_t y,
                                       std::uint32_t source_y, bool* uniform_sample = nullptr) {
                const auto source_x = static_cast<std::uint32_t>(logical_x
                    + row_scroll_x_values[y]) & x_mask;
                return sample_colour(source_x, source_y, uniform_sample);
            };
            auto rolling_source_y = fixed_vertical_scroll
                ? fixed_source_y_at(first_ground_y) : 0U;
            std::int32_t previous_source_y = first_ground_y == 0U ? -1
                : static_cast<std::int32_t>(fixed_vertical_scroll
                    ? (rolling_source_y - 1U) & y_mask
                    : source_y_at(first_ground_y - 1U));
            std::uint8_t last_opaque = 0U;
            bool prior_resolved = false;
            const auto recover_prior = [&](std::uint32_t y,
                                           std::uint32_t current_source_y) {
                if (prior_resolved) return;
                prior_resolved = true;
                if (fixed_vertical_scroll) {
                    auto source_y = current_source_y;
                    while (y > 0U) {
                        --y;
                        source_y = (source_y - 1U) & y_mask;
                        const auto sampled = colour_at(y, source_y);
                        if (sampled != 0U) {
                            last_opaque = sampled;
                            return;
                        }
                    }
                } else {
                    while (y > 0U) {
                        --y;
                        const auto sampled = colour_at(y, source_y_at(y));
                        if (sampled != 0U) {
                            last_opaque = sampled;
                            return;
                        }
                    }
                }
            };
            bool wrapped = false;
            std::uint32_t run_y = 0U, run_height = 0U;
            std::uint8_t run_colour = 0U;
            const auto flush = [&] {
                const auto ok = append_solid(x, run_y, run_height, run_colour);
                run_height = 0U;
                return ok;
            };
            for (std::uint32_t y = first_ground_y; y < target_height;) {
                const auto logical_y = static_cast<std::int32_t>(y) - origin_y;
                const auto source_y = fixed_vertical_scroll
                    ? rolling_source_y : source_y_at(y);
                if (logical_y >= 144 && previous_source_y >= 0
                    && source_y < static_cast<std::uint32_t>(previous_source_y)
                    && static_cast<std::uint32_t>(previous_source_y)
                        - source_y > (y_mask + 1U) / 2U)
                    wrapped = true;
                previous_source_y = static_cast<std::int32_t>(source_y);
                bool override_pixel = wrapped;
                auto colour = last_opaque;
                std::uint32_t sample_height = 1U;
                if (!wrapped) {
                    bool uniform_sample = false;
                    const auto sampled = colour_at(y, source_y, &uniform_sample);
                    // A uniform 8x8 character cannot change until its next
                    // sub-tile row or horizontal HDMA change. Resolve that
                    // run once while retaining palette, transparency and wrap
                    // boundaries; the uncached path remains a pixel oracle.
                    if (uniform_sample && fixed_vertical_scroll) {
                        const auto limit = std::min(target_height - y,
                            8U - (source_y & 7U));
                        while (sample_height < limit
                            && row_scroll_x_values[y + sample_height]
                                == row_scroll_x_values[y]) ++sample_height;
                    }
                    if (sampled != 0U) {
                        last_opaque = sampled;
                        prior_resolved = true;
                    } else {
                        recover_prior(y, source_y);
                        if (last_opaque != 0U) override_pixel = true;
                    }
                    colour = last_opaque;
                } else {
                    recover_prior(y, source_y);
                    colour = last_opaque;
                }
                if (override_pixel) {
                    if (run_height != 0U && run_y + run_height == y
                        && run_colour == colour) run_height += sample_height;
                    else {
                        if (!flush()) { output.clear(); return false; }
                        run_y = y;
                        run_height = sample_height;
                        run_colour = colour;
                    }
                } else if (!flush()) { output.clear(); return false; }
                if (fixed_vertical_scroll)
                    rolling_source_y = (rolling_source_y + sample_height) & y_mask;
                previous_source_y = static_cast<std::int32_t>(
                    (source_y + sample_height - 1U) & y_mask);
                if (wrapped) {
                    // The carry colour is sticky after the first wrapped row.
                    // Fold the remaining viewport into this run without
                    // repeating scanline coordinate and wrap checks.
                    run_height += target_height - y - sample_height;
                    break;
                }
                y += sample_height;
            }
            if (!flush()) { output.clear(); return false; }
        }
    }
    return true;
}

} // namespace starfox::platform_3ds
