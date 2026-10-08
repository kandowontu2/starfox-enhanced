#include "starfox/render/framebuffer.hpp"
#include "starfox/render/gpu_image_extent.hpp"
#include "starfox/render/gpu_dispatch.hpp"
#include "starfox/render/span_clear_policy.hpp"
#include "starfox/render/environment_effects.hpp"
#include "starfox/render/fsr1_settings.hpp"
#include "starfox/render/effects.hpp"
#include "starfox/render/global_enhancements.hpp"
#include "starfox/render/scene_enhancements.hpp"
#include "starfox/render/depth_enhancements.hpp"
#include "starfox/render/frame_persistence.hpp"
#include "starfox/render/adaptive_exposure.hpp"
#include "starfox/render/water_caustics.hpp"
#include "starfox/render/water_transmission.hpp"
#include "starfox/render/water_receiver.hpp"
#include "starfox/render/chromatic_aberration.hpp"
#include "starfox/render/hdr_effect.hpp"
#include "starfox/render/bloom.hpp"
#include "starfox/render/colour_math.hpp"
#include "starfox/render/model_smoothing.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/pixel_filter.hpp"
#include "starfox/render/row_workers.hpp"
#include "starfox/assets/shape.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/math.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static_assert(starfox::render::bounded_gpu_image_extent(4800,1344));
static_assert(starfox::render::bounded_gpu_image_extent(4096,4096));
static_assert(starfox::render::bounded_gpu_image_extent(8192,2048));
static_assert(starfox::render::bounded_gpu_image_extent(2048,8192));
static_assert(!starfox::render::bounded_gpu_image_extent(0,224));
static_assert(!starfox::render::bounded_gpu_image_extent(800,0));
static_assert(!starfox::render::bounded_gpu_image_extent(8193,1));
static_assert(!starfox::render::bounded_gpu_image_extent(1,8193));
static_assert(!starfox::render::bounded_gpu_image_extent(8192,2049));
static_assert(!starfox::render::bounded_gpu_image_extent(UINT32_MAX,UINT32_MAX));
static_assert(starfox::render::linear_gpu_dispatch64(0).x==0);
static_assert(starfox::render::linear_gpu_dispatch64(1).row_stride==64);
static_assert(starfox::render::linear_gpu_dispatch64(65535U*64).x==65535);
static_assert(starfox::render::linear_gpu_dispatch64(65535U*64+1).x==32768);
static_assert(starfox::render::linear_gpu_dispatch64(65535U*64+1).y==2);
static_assert(starfox::render::linear_gpu_dispatch64(65535U*64+1).row_stride==2097152);
static_assert(starfox::render::linear_gpu_dispatch64(4096U*4096).x<=65535);
static_assert(starfox::render::linear_gpu_dispatch64(4096U*4096).y<=65535);
static_assert(starfox::render::linear_gpu_dispatch128(0).x==0);
static_assert(starfox::render::linear_gpu_dispatch128(1).row_stride==128);
static_assert(starfox::render::linear_gpu_dispatch128(65535U*128).x==65535);
static_assert(starfox::render::linear_gpu_dispatch128(65535U*128+1).x==32768);
static_assert(starfox::render::linear_gpu_dispatch128(65535U*128+1).y==2);
static_assert(starfox::render::linear_gpu_dispatch128(65535U*128+1).row_stride==4194304);
static_assert(starfox::render::linear_gpu_dispatch128(4096U*4096).x==43691);
static_assert(starfox::render::linear_gpu_dispatch128(4096U*4096).y==3);
using starfox::render::SpanClearMode;
using starfox::render::span_clear_mode;
static_assert(span_clear_mode(0,0)==SpanClearMode::full);
static_assert(span_clear_mode(1,0)==SpanClearMode::full);
static_assert(span_clear_mode(4095,0)==SpanClearMode::full);
static_assert(span_clear_mode(4096,0)==SpanClearMode::parallel);
static_assert(span_clear_mode(256U*1024*1024/96,0)==SpanClearMode::parallel);
static_assert(span_clear_mode(1,1)==SpanClearMode::parallel);
static_assert(span_clear_mode(4096,1,true,true,true)==SpanClearMode::full);
static_assert(span_clear_mode(1,0,false,true,true)==SpanClearMode::parallel);
static_assert(span_clear_mode(4096,1,false,false,true)==SpanClearMode::bounds);

namespace {
using starfox::render::Fsr1Mode;
using starfox::render::Fsr1Extent;
using starfox::render::fsr1_input_extent;
using starfox::render::prefer_fsr1;
static_assert(fsr1_input_extent({1920,1080},Fsr1Mode::off)==Fsr1Extent{1920,1080});
static_assert(fsr1_input_extent({1920,1080},Fsr1Mode::ultra_quality)==Fsr1Extent{1477,831});
static_assert(fsr1_input_extent({1920,1080},Fsr1Mode::quality)==Fsr1Extent{1280,720});
static_assert(fsr1_input_extent({1920,1080},Fsr1Mode::balanced)==Fsr1Extent{1130,636});
static_assert(fsr1_input_extent({1920,1080},Fsr1Mode::performance)==Fsr1Extent{960,540});
static_assert(fsr1_input_extent({3441,1441},Fsr1Mode::performance)==Fsr1Extent{1721,721});
static_assert(fsr1_input_extent({0,0},Fsr1Mode::quality)==Fsr1Extent{});
static_assert(prefer_fsr1(0,true)); // Xbox UWP: desktop vendor metadata is absent.
static_assert(prefer_fsr1(0x1002,true));
static_assert(prefer_fsr1(0x10de,true)); // UWP cannot use the desktop NGX bridge.
static_assert(prefer_fsr1(0x1002,false));
static_assert(!prefer_fsr1(0x10de,false));
static_assert(!prefer_fsr1(0,false));
static_assert(fsr1_input_extent({1,1},Fsr1Mode::performance)==Fsr1Extent{1,1});
static_assert(fsr1_input_extent({1920,1080},static_cast<Fsr1Mode>(255))==Fsr1Extent{1920,1080});
static_assert(fsr1_input_extent({0xffffffffU,0xffffffffU},Fsr1Mode::performance)
    ==Fsr1Extent{0x80000000U,0x80000000U});

using starfox::render::Framebuffer;
using starfox::render::PixelFilterScratch;
using starfox::render::PixelLayer;
using starfox::render::Rgba8;
using starfox::render::TwoDFilter;

static_assert(starfox::render::anti_aliasing_eligible(PixelLayer::three_d));
static_assert(starfox::render::anti_aliasing_eligible(PixelLayer::world_geometry));
static_assert(starfox::render::anti_aliasing_eligible(PixelLayer::terrain_geometry));
static_assert(!starfox::render::anti_aliasing_eligible(PixelLayer::two_d));
static_assert(!starfox::render::anti_aliasing_eligible(PixelLayer::background));
static_assert(!starfox::render::anti_aliasing_eligible(PixelLayer::textured_geometry));

constexpr std::uint32_t source_width = 64U;
constexpr std::uint32_t source_height = 48U;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::vector<Rgba8> make_palette() {
    std::vector<Rgba8> palette(256U);
    for (std::size_t index = 0; index < palette.size(); ++index) {
        // Spread the indices over a wide range so a filtered pixel is easy to
        // tell from an unfiltered neighbour.
        palette[index] = Rgba8{
            static_cast<std::uint8_t>((index * 37U) & 0xffU),
            static_cast<std::uint8_t>((index * 91U) & 0xffU),
            static_cast<std::uint8_t>((index * 173U) & 0xffU),
            255U};
    }
    palette[0] = Rgba8{0U, 0U, 0U, 255U};
    return palette;
}

// A 2D field with hard diagonal edges (the case a filter must smooth) plus a
// rectangular band of Super FX pixels written at stored resolution (the case it
// must leave alone).
void paint(Framebuffer& framebuffer, std::uint32_t scale) {
    framebuffer.clear(0U);
    for (std::uint32_t y = 0; y < source_height; ++y) {
        for (std::uint32_t x = 0; x < source_width; ++x) {
            const auto diagonal = (x + y) / 6U;
            const auto circle = ((x - 32) * (x - 32) + (y - 24) * (y - 24)) < 90
                ? 200U : 0U;
            framebuffer.set(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y),
                static_cast<std::uint8_t>(
                    circle != 0U ? circle : 30U + (diagonal % 5U) * 40U));
        }
    }

    // Scan conversion drops the draw scale to 1 and writes stored pixels.
    const auto previous = framebuffer.draw_scale();
    framebuffer.set_draw_scale(1U);
    const starfox::render::ScopedLayer geometry{framebuffer, PixelLayer::three_d};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto in_band = x >= 10U * scale && x < 22U * scale
                && y >= 8U * scale && y < 40U * scale;
            if (in_band) {
                framebuffer.set(static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y), 250U);
            }
        }
    }
    framebuffer.set_draw_scale(previous);
}

void write_bmp(const std::filesystem::path& path,
    const std::vector<std::uint8_t>& rgba,
    std::uint32_t width, std::uint32_t height) {
    std::ofstream output{path, std::ios::binary};
    const auto row_bytes = ((width * 3U) + 3U) & ~3U;
    const auto pixel_bytes = row_bytes * height;
    const auto put32 = [&output](std::uint32_t value) {
        const std::array<char, 4> bytes{
            static_cast<char>(value & 0xffU),
            static_cast<char>((value >> 8U) & 0xffU),
            static_cast<char>((value >> 16U) & 0xffU),
            static_cast<char>((value >> 24U) & 0xffU)};
        output.write(bytes.data(), bytes.size());
    };
    const auto put16 = [&output](std::uint16_t value) {
        const std::array<char, 2> bytes{
            static_cast<char>(value & 0xffU),
            static_cast<char>((value >> 8U) & 0xffU)};
        output.write(bytes.data(), bytes.size());
    };
    output.write("BM", 2);
    put32(14U + 40U + pixel_bytes);
    put32(0U);
    put32(14U + 40U);
    put32(40U);
    put32(width);
    put32(height);
    put16(1U);
    put16(24U);
    put32(0U);
    put32(pixel_bytes);
    put32(2835U);
    put32(2835U);
    put32(0U);
    put32(0U);
    std::vector<char> row(row_bytes, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto source_y = height - 1U - y;
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto* pixel = rgba.data()
                + (static_cast<std::size_t>(source_y) * width + x) * 4U;
            row[x * 3U + 0U] = static_cast<char>(pixel[2]);
            row[x * 3U + 1U] = static_cast<char>(pixel[1]);
            row[x * 3U + 2U] = static_cast<char>(pixel[0]);
        }
        output.write(row.data(), static_cast<std::streamsize>(row.size()));
    }
}

struct Frame {
    Framebuffer framebuffer;
    std::vector<std::uint8_t> rgba;
};

Frame render(std::uint32_t scale, const std::vector<Rgba8>& palette) {
    Framebuffer framebuffer{source_width, source_height, scale};
    framebuffer.enable_layer_tags(true);
    paint(framebuffer, scale);
    std::vector<std::uint8_t> rgba;
    starfox::render::expand_rgba(framebuffer, rgba, palette);
    return Frame{std::move(framebuffer), std::move(rgba)};
}

void check_tags(std::uint32_t scale) {
    const auto palette = make_palette();
    const auto frame = render(scale, palette);
    const auto& framebuffer = frame.framebuffer;
    auto three_d_seen = std::size_t{0};
    auto two_d_seen = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto in_band = x >= 10U * scale && x < 22U * scale
                && y >= 8U * scale && y < 40U * scale;
            const auto layer = framebuffer.layer_stored(x, y);
            if (in_band) {
                require(layer == PixelLayer::three_d,
                    "stored-resolution writes must tag as 3D");
                ++three_d_seen;
            } else {
                require(layer == PixelLayer::two_d,
                    "source-raster writes must tag as 2D");
                ++two_d_seen;
            }
        }
    }
    require(three_d_seen > 0U && two_d_seen > 0U, "both layers must be present");
}

void check_filter(TwoDFilter filter, std::uint32_t scale,
    const std::filesystem::path& dump_directory) {
    const auto palette = make_palette();
    auto frame = render(scale, palette);
    const auto unfiltered = frame.rgba;

    PixelFilterScratch scratch;
    starfox::render::RowWorkers workers;
    workers.set_worker_count(1U);
    starfox::render::apply_two_d_filter(
        filter, frame.framebuffer, palette, frame.rgba, scratch, workers);
    auto scenery = render(scale, palette);
    for (auto& tag : scenery.framebuffer.layer_tags()) {
        if (tag == std::uint8_t(PixelLayer::two_d)) tag = std::uint8_t(PixelLayer::background);
    }
    starfox::render::apply_two_d_filter(
        filter, scenery.framebuffer, palette, scenery.rgba, scratch, workers);
    require(scenery.rgba == frame.rgba, "world background tags changed 2D filtering");

    const auto& framebuffer = frame.framebuffer;
    const auto stored_width = framebuffer.stored_width();
    auto changed = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < stored_width; ++x) {
            const auto offset =
                (static_cast<std::size_t>(y) * stored_width + x) * 4U;
            const auto same = frame.rgba[offset] == unfiltered[offset]
                && frame.rgba[offset + 1U] == unfiltered[offset + 1U]
                && frame.rgba[offset + 2U] == unfiltered[offset + 2U];
            if (framebuffer.layer_stored(x, y) == PixelLayer::three_d) {
                require(same, "the filter must never touch 3D-owned pixels");
            } else if (!same) {
                ++changed;
            }
        }
    }
    require(changed > 0U, "the filter must resolve some 2D edges");

    // Threading must not change a single pixel.
    auto threaded = render(scale, palette);
    PixelFilterScratch parallel_scratch;
    starfox::render::RowWorkers parallel_workers;
    parallel_workers.set_worker_count(4U);
    starfox::render::apply_two_d_filter(
        filter, threaded.framebuffer, palette, threaded.rgba, parallel_scratch,
        parallel_workers);
    require(threaded.rgba == frame.rgba,
        "threaded and single-threaded output must match exactly");

    if (!dump_directory.empty()) {
        std::filesystem::create_directories(dump_directory);
        const auto name = std::string{starfox::render::two_d_filter_name(filter)}
            + "-" + std::to_string(scale) + "x.bmp";
        write_bmp(dump_directory / name, frame.rgba,
            framebuffer.stored_width(), framebuffer.stored_height());
        write_bmp(dump_directory / ("OFF-" + std::to_string(scale) + "x.bmp"),
            unfiltered, framebuffer.stored_width(),
            framebuffer.stored_height());
    }
}

void check_disabled_paths() {
    const auto palette = make_palette();
    PixelFilterScratch scratch;
    starfox::render::RowWorkers workers;

    // Tags off means no filtering, whatever the scale.
    Framebuffer untagged{source_width, source_height, 4U};
    paint(untagged, 4U);
    std::vector<std::uint8_t> rgba;
    starfox::render::expand_rgba(untagged, rgba, palette);
    const auto untagged_copy = rgba;
    starfox::render::apply_two_d_filter(
        TwoDFilter::edge, untagged, palette, rgba, scratch, workers);
    require(rgba == untagged_copy, "untagged framebuffers must be left alone");

    // OFF is a no-op even with everything else in place.
    auto ready = render(4U, palette);
    const auto ready_copy = ready.rgba;
    starfox::render::apply_two_d_filter(
        TwoDFilter::off, ready.framebuffer, palette, ready.rgba, scratch, workers);
    require(ready.rgba == ready_copy, "OFF must be a no-op");
}

// The draw-scale derivation is only a proxy for layer ownership, and the
// software renderer breaks it on purpose: simple_scaled_sprite shapes and
// sprite faces stay on the source raster, and the cockpit HUD plots there too.
// Those are Super FX output that would otherwise be read as cartridge art and
// handed to the 2D filter, which is what SoftwareRenderer's entry-point
// ScopedLayer exists to prevent. Drive the real renderer rather than a
// hand-tagged scene, so this fails if any of its paths regress.
// Rasterized geometry is the one thing that must never reach the 2D layer.
// Drive a real polygon through the renderer rather than trusting the draw
// scale to speak for it.
void check_polygons_stay_geometry(std::uint32_t scale) {
    constexpr std::uint32_t width = 256U;
    constexpr std::uint32_t height = 224U;
    Framebuffer framebuffer{width, height, scale};
    framebuffer.enable_layer_tags(true);
    framebuffer.clear(0U);
    // Start every cell owned by cartridge art, so a polygon that fails to
    // claim its pixels is visible as a leftover 2D tag.
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            framebuffer.set(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y), 50U);
        }
    }

    starfox::assets::Shape shape;
    shape.colour_words.push_back(0x0202U);
    shape.vertices = {
        starfox::assets::Vec3i{-60, -60, 0},
        starfox::assets::Vec3i{60, -50, 0},
        starfox::assets::Vec3i{0, 60, 0},
    };
    starfox::assets::Face face;
    face.visibility_index = -1;
    face.colour_id = 0U;
    face.vertex_indices = {0U, 1U, 2U};
    shape.faces.push_back(face);

    starfox::render::RenderPose pose;
    starfox::render::RenderSettings settings;
    settings.render_scale = scale;
    const starfox::render::SoftwareRenderer renderer{settings};
    const auto before = framebuffer.pixels();
    renderer.draw(shape, pose, framebuffer, false, nullptr);

    auto polygon_pixels = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto index =
                static_cast<std::size_t>(y) * framebuffer.stored_width() + x;
            if (framebuffer.pixels()[index] == before[index]) continue;
            ++polygon_pixels;
            require(framebuffer.layer_stored(x, y) == PixelLayer::three_d,
                "rasterized polygons must stay in the geometry layer");
        }
    }
    require(polygon_pixels > 0U, "the polygon drew nothing to check");
}

void check_cockpit_hud_is_filtered(std::uint32_t scale) {
    constexpr std::uint32_t width = 256U;
    constexpr std::uint32_t height = 224U;
    Framebuffer framebuffer{width, height, scale};
    framebuffer.enable_layer_tags(true);
    framebuffer.clear(0U);
    // Fill the frame with cartridge 2D art so every pixel starts tagged 2D.
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            framebuffer.set(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y), 50U);
        }
    }
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            require(framebuffer.layer_stored(x, y) == PixelLayer::two_d,
                "cartridge fill should have tagged the whole frame 2D");
        }
    }

    const auto before = framebuffer.pixels();
    const starfox::render::SoftwareRenderer renderer;
    const starfox::simulation::TrigTables trigonometry;
    renderer.draw_cockpit_hud(trigonometry, 0U, 12U, 0U, 0, framebuffer, 0U);

    auto changed = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto index =
                static_cast<std::size_t>(y) * framebuffer.stored_width() + x;
            if (framebuffer.pixels()[index] == before[index]) continue;
            ++changed;
            require(framebuffer.layer_stored(x, y) == PixelLayer::two_d,
                "cockpit HUD lines are authored-resolution art and must be "
                "filtered with the rest of the 2D layer");
        }
    }
    require(changed > 0U, "the cockpit HUD drew nothing to check");
}

// Overlays that composite in RGBA after the frame is expanded never reach the
// tagged framebuffer, so they need their own filtering pass or they stay
// blocky while everything behind them resolves.
void check_overlay_layer_is_filtered(std::uint32_t scale) {
    constexpr std::uint32_t width = 96U;
    constexpr std::uint32_t height = 72U;
    const auto palette = make_palette();

    // A source-raster overlay with hard diagonal edges over index 0, which the
    // overlay pass treats as transparent.
    Framebuffer overlay{width, height};
    overlay.clear(0U);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto dx = static_cast<int>(x) - 48;
            const auto dy = static_cast<int>(y) - 36;
            if (dx * dx + dy * dy < 400) {
                overlay.set(static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y), 150U);
            }
        }
    }

    PixelFilterScratch scratch;
    starfox::render::RowWorkers workers;
    std::vector<std::uint32_t> argb;
    require(starfox::render::filter_overlay_layer(TwoDFilter::edge, overlay,
                palette, scale, argb, scratch, workers),
        "the overlay filter should engage above scale 1");
    require(argb.size()
            == static_cast<std::size_t>(width) * scale * height * scale,
        "the overlay filter must fill the stored-resolution buffer");

    // Transparency must survive, art must survive, and the result must differ
    // from plain block expansion somewhere along the circle's edge.
    auto opaque_pixels = std::size_t{0};
    auto differs_from_nearest = std::size_t{0};
    for (std::uint32_t y = 0; y < height * scale; ++y) {
        for (std::uint32_t x = 0; x < width * scale; ++x) {
            const auto colour =
                argb[static_cast<std::size_t>(y) * width * scale + x];
            if (((colour >> 24U) & 0xffU) != 0U) ++opaque_pixels;
            const auto nearest = overlay.get(x / scale, y / scale);
            const auto nearest_opaque = nearest != 0U;
            if (nearest_opaque != (((colour >> 24U) & 0xffU) == 0xffU)) {
                ++differs_from_nearest;
            }
        }
    }
    require(opaque_pixels > 0U, "the overlay filter dropped all of the art");
    require(scale == 1U || differs_from_nearest > 0U,
        "the overlay filter changed nothing against block expansion");

    // Off must decline so the caller keeps its own path.
    require(!starfox::render::filter_overlay_layer(
                TwoDFilter::off, overlay, palette, scale, argb, scratch, workers),
        "OFF must decline");

}

// composite_transparent_layer has two implementations: a bulk transfer for
// equal-scale layers with no mosaic -- which is the one gameplay actually
// takes when the Super FX world reaches the presented framebuffer -- and a
// generic per-cell path for everything else. Both must move layer tags with
// the pixels, so exercise them both rather than whichever one a default
// LayerCompositeSettings happens to select.
void check_composite_carries_layers(std::uint32_t scale, bool force_generic) {
    constexpr std::uint32_t width = 64U;
    constexpr std::uint32_t height = 48U;

    // Destination: a full screen of cartridge 2D art.
    Framebuffer destination{width, height, scale};
    destination.enable_layer_tags(true);
    destination.clear(0U);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            destination.set(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y), 40U);
        }
    }

    // Source: a Super FX layer holding geometry written at stored resolution,
    // exactly as scan conversion produces it.
    Framebuffer source{width, height, scale};
    source.enable_layer_tags(true);
    source.clear(0U);
    {
        const auto previous = source.draw_scale();
        source.set_draw_scale(1U);
        const starfox::render::ScopedLayer geometry{source, PixelLayer::three_d};
        for (std::uint32_t y = source.stored_height() / 4U;
             y < source.stored_height() * 3U / 4U; ++y) {
            for (std::uint32_t x = source.stored_width() / 4U;
                 x < source.stored_width() * 3U / 4U; ++x) {
                source.set(static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y), 90U);
            }
        }
        source.set_draw_scale(previous);
    }

    starfox::render::LayerCompositeSettings settings;
    if (force_generic) {
        // A mosaic touching this layer disables the bulk transfer.
        settings.mosaic = 0x01U;
        settings.mosaic_layer_mask = 0x01U;
    }
    starfox::render::composite_transparent_layer(source, destination, settings);

    auto geometry = std::size_t{0};
    for (std::uint32_t y = 0; y < destination.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < destination.stored_width(); ++x) {
            const auto index =
                static_cast<std::size_t>(y) * destination.stored_width() + x;
            if (destination.pixels()[index] != 90U) continue;
            ++geometry;
            require(destination.layer_stored(x, y) == PixelLayer::three_d,
                "composited geometry must arrive tagged as geometry");
        }
    }
    require(geometry > 0U, "the composite transferred no geometry to check");
}

// Star Fox draws explosions, asteroids and similar objects as sprites rather
// than polygons: authored texels point-sampled onto the source raster. They
// arrive through the software renderer, but they are cartridge art and belong
// to the 2D layer, or the filter leaves them pixelated while everything around
// them resolves. Drive the real sprite path to hold that classification.
void check_sprites_are_cartridge_art(std::uint32_t scale) {
    constexpr std::uint32_t width = 256U;
    constexpr std::uint32_t height = 224U;
    Framebuffer framebuffer{width, height, scale};
    framebuffer.enable_layer_tags(true);
    framebuffer.clear(0U);

    // A minimal shape whose only content is one solid sprite texture.
    starfox::assets::Shape shape;
    constexpr std::uint16_t descriptor = 0x1234U;
    shape.colour_words.push_back(descriptor);
    starfox::assets::TextureImage texture;
    texture.descriptor = descriptor;
    texture.u_mask = 7U;
    texture.v_mask = 7U;
    texture.texels.assign(64U, 6U);
    shape.textures.push_back(texture);

    starfox::render::RenderPose pose;
    pose.simple_scaled_sprite = true;
    pose.simple_sprite_colour = 0U;
    pose.simple_sprite_world_size = 300;

    starfox::render::RenderSettings settings;
    settings.render_scale = scale;
    const starfox::render::SoftwareRenderer renderer{settings};
    renderer.draw(shape, pose, framebuffer, false, nullptr);

    auto sprite_pixels = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto index =
                static_cast<std::size_t>(y) * framebuffer.stored_width() + x;
            if (framebuffer.pixels()[index] == 0U) continue;
            ++sprite_pixels;
            require(framebuffer.layer_stored(x, y) == PixelLayer::two_d,
                "scaled sprites must reach the 2D layer so they get filtered");
        }
    }
    require(sprite_pixels > 0U, "the sprite drew nothing to check");
}

} // namespace

// Original per-pixel implementation: independent oracle for the separable taps.
static void reference_chromatic(const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba, std::uint8_t intensity) {
    if (!intensity || intensity>3 || !frame.layer_tags_enabled()) return;
    const auto width=frame.stored_width(), height=frame.stored_height();
    if (!width || !height) return;
    const auto source=rgba;
    const auto model=[&](unsigned x,unsigned y) {
        auto layer=frame.layer_stored(x,y);
        return layer==PixelLayer::three_d || layer==PixelLayer::textured_geometry;
    };
    const auto amount=std::array<double,4>{0,.6,1.2,2.4}[intensity]*frame.draw_scale();
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
        if (!model(x,y)) continue;
        const auto pixel=(std::size_t(y)*width+x)*4;
        const auto dx=(2.0*(x+.5)/width-1.0)*amount;
        const auto dy=(2.0*(y+.5)/height-1.0)*amount;
        for (unsigned channel : {0U,2U}) {
            const auto sign=channel==0?1.0:-1.0;
            const auto sx=std::clamp(x+sign*dx,0.0,double(width-1));
            const auto sy=std::clamp(y+sign*dy,0.0,double(height-1));
            const auto x0=unsigned(sx),y0=unsigned(sy);
            const auto x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);
            const auto sample=[&](unsigned px,unsigned py) {
                return double(model(px,py)?source[(std::size_t(py)*width+px)*4+channel]
                    :source[pixel+channel]);
            };
            const auto fx=sx-x0,fy=sy-y0;
            const auto top=sample(x0,y0)*(1-fx)+sample(x1,y0)*fx;
            const auto bottom=sample(x0,y1)*(1-fx)+sample(x1,y1)*fx;
            rgba[pixel+channel]=static_cast<std::uint8_t>(std::lround(top*(1-fy)+bottom*fy));
        }
    }
}

int main(int argc, char** argv) {
    const auto order=starfox::simulation::pregame_menu_order(starfox::simulation::PregamePage::three_d);
    require(std::find(order.begin(),order.end(),31U)==order.end(),"Removed add-on row remains in menu navigation");
    require(order.back()==23U,"Removed add-on row displaced BACK");
    require(std::find(starfox::simulation::three_d_menu_order.begin(),
        starfox::simulation::three_d_menu_order.end(),25U)==starfox::simulation::three_d_menu_order.end(),
        "removed line thickness remains in menu navigation");
    for (const auto dimensions : {std::array<unsigned,2>{0,0}, {1,1}, {1,9}, {13,1}, {37,23}, {256,224}}) {
        for (unsigned scale : {1U,2U,4U}) {
            Framebuffer frame{dimensions[0],dimensions[1],scale};
            frame.enable_layer_tags(true);
            std::vector<std::uint8_t> original(frame.pixels().size()*4),scratch;
            for (std::size_t i=0;i<original.size();++i) original[i]=static_cast<std::uint8_t>(i*73+i/17);
            for (std::size_t i=0;i<frame.pixels().size();++i)
                frame.layer_tags()[i]=static_cast<std::uint8_t>((i/13+i/37)%5);
            for (std::uint8_t level : {0,1,2,3,4}) {
                auto expected=original,actual=original;
                reference_chromatic(frame,expected,level);
                starfox::render::apply_chromatic_aberration(frame,actual,scratch,level);
                require(actual==expected,"separable chromatic taps changed reference output");
            }
        }
    }
    {
        starfox::render::RowWorkers workers;
        const auto colours = make_palette();
        for (const auto width : {256U, 796U}) {
            for (const auto scale : {1U, 2U, 4U}) {
                Framebuffer frame{width, 224U, scale};
                for (std::size_t i = 0; i < frame.pixels().size(); ++i)
                    frame.pixels()[i] = static_cast<std::uint8_t>(i * 73 + i / 97);
                for (const auto count : {1U, 16U, 255U, 256U}) {
                    const std::span<const Rgba8> palette{colours.data(), count};
                    std::vector<std::uint8_t> serial, pooled;
                    starfox::render::expand_rgba(frame, serial, palette);
                    starfox::render::expand_rgba(frame, pooled, palette, workers);
                    require(serial == pooled, "adaptive palette expansion changed pixels");
                }
            }
        }
    }
    {
        starfox::render::Framebuffer frame{256,1};
        frame.enable_layer_tags(true);
        const starfox::render::ScopedLayer model{frame,starfox::render::PixelLayer::three_d};
        std::vector<std::uint8_t> ramp(256*4,255);
        for (unsigned x=0;x<256;++x) {
            frame.set(x,0,1);
            for (unsigned c=0;c<3;++c) ramp[x*4+c]=x;
        }
        auto off=ramp;
        starfox::render::apply_hdr_effect(frame,off,0);
        require(off==ramp,"HDR effect OFF changed colors");
        auto middle=ramp[128*4];
        for (std::uint8_t level=1;level<=3;++level) {
            auto output=ramp;
            starfox::render::apply_hdr_effect(frame,output,level);
            for (unsigned x=0;x<256;++x) {
                const double exposure=std::array<double,4>{1,1.15,1.35,1.65}[level];
                const double contrast=std::array<double,4>{0,.2,.4,.6}[level];
                const double input=x/255.0;
                const double lifted=input*exposure/(1+input*(exposure-1));
                const double shaped=lifted*lifted*(3-2*lifted);
                const auto expected=static_cast<std::uint8_t>(std::lround(
                    255*(lifted*(1-contrast)+shaped*contrast)));
                for (unsigned c=0;c<3;++c)
                    require(output[x*4+c]==expected,"cached HDR curve changed rounding");
            }
            require(output[0]==0 && output[255*4]==255,"HDR effect lifted black or clipped white endpoint");
            require(output[128*4]>middle,"HDR effect levels did not increase midtone brightness");
            middle=output[128*4];
            for (unsigned x=1;x<256;++x)
                require(output[x*4]>=output[(x-1)*4] && output[x*4+3]==255,
                    "HDR tone curve reversed contrast or changed alpha");
        }
        { const starfox::render::ScopedLayer hud{frame,starfox::render::PixelLayer::two_d}; frame.set(128,0,1); }
        auto output=ramp;
        starfox::render::apply_hdr_effect(frame,output,3);
        require(output[128*4]==ramp[128*4],"HDR effect changed HUD text");
    }
    {
        starfox::render::Framebuffer frame{64,8};
        frame.enable_layer_tags(true);
        std::vector<std::uint8_t> original(64*8*4,255), scratch;
        const starfox::render::ScopedLayer model_layer{frame,starfox::render::PixelLayer::three_d};
        for (unsigned y=0;y<8;++y) for (unsigned x=0;x<64;++x) {
            frame.set(x,y,1);
            for (unsigned c=0;c<3;++c) original[(y*64+x)*4+c]=x*4;
        }
        auto off=original;
        starfox::render::apply_chromatic_aberration(frame,off,scratch,0);
        require(off==original,"chromatic aberration OFF changed pixels");
        int previous=original[(4*64+52)*4];
        for (std::uint8_t level=1;level<=3;++level) {
            auto output=original;
            starfox::render::apply_chromatic_aberration(frame,output,scratch,level);
            const auto pixel=(4*64+52)*4;
            require(output[pixel]>previous,"chromatic levels did not increase channel separation");
            require(output[pixel+1]==original[pixel+1] && output[pixel+3]==255,
                "chromatic effect changed green or opacity");
            previous=output[pixel];
        }
        {
            const starfox::render::ScopedLayer hud{frame,starfox::render::PixelLayer::two_d};
            frame.set(52,4,1);
        }
        auto output=original;
        starfox::render::apply_chromatic_aberration(frame,output,scratch,3);
        require(output[(4*64+52)*4]==original[(4*64+52)*4],
            "chromatic aberration affected HUD text");
    }
    starfox::render::RowWorkers restart_workers;
    for (const auto count : {1U, 4U, 2U, 1U, 4U}) {
        restart_workers.set_worker_count(count);
        for (unsigned repeat=0;repeat<8;++repeat) {
            for (unsigned size : {0U,1U,15U,16U,17U,31U,32U,127U,224U,225U}) {
                std::vector<unsigned> rows(size);
                restart_workers.parallel_rows(size,
                    [&](std::uint32_t first, std::uint32_t last) {
                        require(first<=last && last<=rows.size(),"worker partition out of bounds");
                        for (auto row = first; row < last; ++row) ++rows[row];
                    });
                for (const auto visits : rows) {
                    require(visits == 1U, "resized worker pool skipped or repeated rows");
                }
            }
        }
    }
    const auto dump_directory = argc > 1
        ? std::filesystem::path{argv[1]} : std::filesystem::path{};

    for (const auto scale : {1U, 2U, 3U, 4U, 6U, 10U}) {
        check_tags(scale);
        check_polygons_stay_geometry(scale);
        check_cockpit_hud_is_filtered(scale);
        check_overlay_layer_is_filtered(scale);
        check_composite_carries_layers(scale, false);
        check_composite_carries_layers(scale, true);
        check_sprites_are_cartridge_art(scale);
    }
    check_disabled_paths();
    for(unsigned scale:{1U,2U,3U,4U,6U,10U}) {
      for(unsigned spacing:{1U,scale}) {
        Framebuffer scene{12,12,scale};scene.enable_layer_tags(true);
        std::vector<std::uint8_t> rgba(scene.pixels().size()*4,255),scratch;
        std::fill(scene.layer_tags().begin(),scene.layer_tags().end(),std::uint8_t(PixelLayer::three_d));
        for(unsigned y=0;y<scene.stored_height();++y) for(unsigned x=0;x<scene.stored_width();++x)
            for(unsigned c=0;c<3;++c) rgba[(std::size_t(y)*scene.stored_width()+x)*4+c]=((x/spacing)^(y/spacing))&1?200:40;
        const auto original=rgba;
        smooth_models(4,scene,rgba,scratch);
        if(scale==1) require(rgba==original,"native 1x dither changed");
        else for(unsigned y=spacing;y+spacing<scene.stored_height();++y)
            for(unsigned x=spacing;x+spacing<scene.stored_width();++x)
                require(rgba[(std::size_t(y)*scene.stored_width()+x)*4]==120,"upscaled checkerboard was not resolved");
        for(auto protected_layer:{PixelLayer::two_d,PixelLayer::background,PixelLayer::textured_geometry}) {
            std::fill(scene.layer_tags().begin(),scene.layer_tags().end(),std::uint8_t(protected_layer));
            rgba=original;smooth_models(4,scene,rgba,scratch);
            require(rgba==original,"automatic dither resolve altered artwork");
        }
        // Upscaling alone must not turn ordinary hard-edged model shading into
        // a blur. Only an alternating two-colour pattern qualifies for resolve.
        std::fill(scene.layer_tags().begin(),scene.layer_tags().end(),std::uint8_t(PixelLayer::three_d));
        for(unsigned y=0;y<scene.stored_height();++y) for(unsigned x=0;x<scene.stored_width();++x)
            for(unsigned c=0;c<3;++c) rgba[(std::size_t(y)*scene.stored_width()+x)*4+c]=x<scene.stored_width()/2?40:200;
        const auto flat_faces=rgba;
        smooth_models(4,scene,rgba,scratch);
        require(rgba==flat_faces,"automatic dither resolve blurred a model face boundary");
      }
    }
    for(unsigned scale:{1U,2U,4U}) {
        Framebuffer scene{400,224,scale};
        scene.enable_layer_tags(true);
        std::fill(scene.layer_tags().begin(),scene.layer_tags().end(),std::uint8_t(PixelLayer::background));
        std::vector<std::uint8_t> original(scene.pixels().size()*4U,255U), scratch;
        for(unsigned y=80*scale;y<140*scale;++y) for(unsigned x=100*scale;x<200*scale;++x) {
            const auto i=std::size_t(y)*scene.stored_width()+x;
            scene.layer_tags()[i]=std::uint8_t(x<150*scale ? PixelLayer::three_d : PixelLayer::textured_geometry);
            for(unsigned c=0;c<3;++c) original[i*4+c]=x<150*scale?0U:240U;
        }
        unsigned previous=240U;
        starfox::render::RowWorkers workers;
        workers.set_worker_count(4);
        for(std::uint8_t level=0;level<4;++level) {
            auto pixels=original, threaded=original;
            starfox::render::smooth_models(level,scene,pixels,scratch);
            starfox::render::smooth_models(level,scene,threaded,scratch,&workers);
            require(pixels==threaded,"model smoothing differs across worker counts");
            if(level==0) require(pixels==original,"model smoothing Off changed pixels");
            const auto boundary=(std::size_t(110*scale)*scene.stored_width()+150*scale)*4U;
            if(level) require(pixels[boundary]<previous,"model smoothing strength did not increase");
            previous=pixels[boundary];
            for(std::size_t i=0;i<scene.pixels().size();++i) {
                require(pixels[i*4+3]==255U,"model smoothing changed alpha");
                if(scene.layer_tags()[i]==std::uint8_t(PixelLayer::background))
                    require(pixels[i*4]==255U,"model smoothing bled beyond geometry coverage");
            }
        }
    }
    {
        starfox::simulation::SnesPpuState ppu;
        ppu.background_mode=2U;
        ppu.bg2_screen_size=0U;
        ppu.bg2_scanline_scroll_enabled=true;
        ppu.tunnel_scene=true; // water uses scanline scrolling too
        for(unsigned i=0;i<1024;++i) ppu.vram[ppu.bg2_screen_base*2U+i*2U]=1U;
        for(unsigned y=0;y<8;++y) {
            ppu.vram[ppu.bg2_character_base*2U+32U+y*2U]=y<4?255U:0U;
            ppu.vram[ppu.bg2_character_base*2U+33U+y*2U]=y<4?0U:255U;
        }
        starfox::render::BackgroundRenderer renderer;
        for(unsigned width:{400U,512U,768U}) for(unsigned scale:{1U,2U,4U}) {
            Framebuffer wide{width,8,scale}, native{256,8,scale};
            const auto origin=int((width-256U)/2U);
            renderer.draw_bg2(ppu,0,0,wide,starfox::render::TilePriorityPass::all,origin);
            renderer.draw_bg2(ppu,0,0,native);
            for(unsigned y=0;y<8;++y) for(unsigned x=0;x<width;++x) {
                const auto expected=native.get(x*256U/width,y);
                require(wide.get(x,y)==expected,"tunnel did not expand its single authored cross-section");
            }
            require(native.get(0,0)!=0U,"tunnel regression fixture is empty");
            Framebuffer split{width,8,scale};
            renderer.draw_bg2(ppu,0,0,split,starfox::render::TilePriorityPass::low,origin);
            renderer.draw_bg2(ppu,0,0,split,starfox::render::TilePriorityPass::high,origin);
            for(unsigned y=0;y<8;++y) for(unsigned x=0;x<width;++x)
                require(split.get(x,y)==wide.get(x,y),
                    "high-priority tunnel pass erased extended ceiling/floor");
        }
    }
    for (const auto layer : {PixelLayer::three_d, PixelLayer::background}) {
        Framebuffer scene{32,32};
        scene.enable_layer_tags(true);
        std::fill(scene.layer_tags().begin(),scene.layer_tags().end(),std::uint8_t(layer));
        std::vector<std::uint8_t> original(scene.pixels().size()*4U,0U);
        for (std::size_t i=0;i<scene.pixels().size();++i) original[i*4+3]=255U;
        for (unsigned y=12;y<20;++y) for(unsigned x=12;x<20;++x)
            for(unsigned c=0;c<3;++c) original[(y*32+x)*4+c]=255U;
        starfox::render::BloomPass bloom;
        auto wrong=original, right=original;
        bloom.apply(layer==PixelLayer::three_d ? 0U : 3U,
            layer==PixelLayer::three_d ? 3U : 0U,scene,wrong);
        bloom.apply(layer==PixelLayer::three_d ? 3U : 0U,
            layer==PixelLayer::three_d ? 0U : 3U,scene,right);
        require(wrong==original,"disabled bloom layer still emitted light");
        require(right!=original,"enabled bloom layer emitted no light");
        auto base = original, glow = right, final = right;
        // A late host overlay replaces this pixel after scene bloom.
        final[0] = 17U; final[1] = 33U; final[2] = 91U;
        starfox::render::split_bloom_layer(base, glow, final);
        for (std::size_t i = 0; i < final.size(); i += 4U) {
            for (unsigned c = 0; c < 3; ++c)
                require(unsigned(base[i+c])+glow[i+c] == final[i+c],
                    "separate bloom display layer changed native-resolution colors");
        }
        require(glow[0] == 0U && glow[1] == 0U && glow[2] == 0U,
            "scene bloom contaminated a host overlay");
    }
    for (const unsigned scale : {1U,2U,4U}) {
        Framebuffer scene{8,8,scale};
        scene.clear(1U);
        scene.set(4,4,129U);
        std::vector<std::uint8_t> original(scene.pixels().size()*4U,255U);
        for (const auto amount : {31U,30U,16U,2U,0U}) {
            auto pixels=original;
            starfox::simulation::ColourMathEffectState effect{
                true,true,false,0x27U,std::uint8_t(amount),std::uint8_t(amount),std::uint8_t(amount)};
            starfox::render::apply_colour_math(effect,scene,pixels);
            for (std::size_t i=0; i<scene.pixels().size(); ++i) {
                const auto expected=scene.pixels()[i]>=128U ? 255U : 255U-(amount*8U+(amount>>2U));
                for (unsigned c=0;c<3;++c) require(pixels[i*4+c]==expected,
                    "revival colour window changed OBJ or lost native background fade");
                require(pixels[i*4+3]==255U,"colour window changed alpha");
            }
        }
    }
    // Golden output from the pre-optimization bloom, plus threaded/cache reuse
    // coverage at every supported render scale.
    starfox::render::BloomPass cached_bloom;
    starfox::render::RowWorkers bloom_workers;
    bloom_workers.set_worker_count(4);
    for (const unsigned scale : {1U, 2U, 4U, 2U, 1U}) {
        Framebuffer scene{768, 224, scale};
        scene.enable_layer_tags(true);
        std::vector<std::uint8_t> original(scene.pixels().size()*4);
        for (unsigned y=0; y<scene.stored_height(); ++y) for (unsigned x=0; x<scene.stored_width(); ++x) {
            const auto i=std::size_t(y)*scene.stored_width()+x;
            scene.layer_tags()[i]=std::uint8_t(y<16*scale ? PixelLayer::two_d : PixelLayer::background);
            original[i*4]=(x/scale*13+y/scale*3)%256;
            original[i*4+1]=(x/scale*7+y/scale*5)%256;
            original[i*4+2]=(x/scale*3+y/scale*11)%256;
            original[i*4+3]=255;
        }
        auto serial=original, threaded=original;
        cached_bloom.apply(3,scene,serial);
        cached_bloom.apply(3,scene,threaded,&bloom_workers);
        require(serial==threaded,"threaded bloom differs from serial output");
        std::uint64_t hash=1469598103934665603ULL;
        for (auto v:serial) { hash^=v; hash*=1099511628211ULL; }
        const auto expected=scale==1 ? 13590015223919869339ULL
            : scale==2 ? 2401763838851610420ULL : 7426840680921405174ULL;
        require(hash==expected,"optimized bloom changed reference pixels");
    }
    for (const unsigned scale : {1U, 2U, 4U}) {
        Framebuffer scene{32, 32, scale};
        scene.enable_layer_tags(true);
        std::fill(scene.layer_tags().begin(), scene.layer_tags().end(), std::uint8_t(PixelLayer::background));
        std::vector<std::uint8_t> original(scene.pixels().size() * 4, 0);
        for (std::size_t i = 0; i < scene.pixels().size(); ++i) original[i*4+3] = 255;
        for (unsigned y = 12*scale; y < 20*scale; ++y) for (unsigned x = 12*scale; x < 20*scale; ++x) {
            const auto i = std::size_t(y)*scene.stored_width()+x;
            scene.layer_tags()[i] = std::uint8_t(PixelLayer::three_d);
            for (unsigned c=0;c<3;++c) original[i*4+c]=255;
        }
        const auto hud = std::size_t(16*scale)*scene.stored_width()+10*scale;
        scene.layer_tags()[hud] = std::uint8_t(PixelLayer::two_d);
        original[hud*4] = 80;
        starfox::render::BloomPass bloom;
        unsigned previous = 0;
        for (std::uint8_t level=0; level<4; ++level) {
            auto pixels=original;
            bloom.apply(level, scene, pixels);
            if (!level) require(pixels == original, "Bloom Off changed output");
            const auto halo=(std::size_t(16*scale)*scene.stored_width()+7*scale)*4;
            if (level) require(pixels[halo]>previous, "Bloom strength did not increase its broad halo");
            previous=pixels[halo];
            require(std::equal(pixels.begin()+hud*4,pixels.begin()+hud*4+4,original.begin()+hud*4), "Bloom changed HUD");
            for (std::size_t i=3;i<pixels.size();i+=4) require(pixels[i]==255,"Bloom changed alpha");
        }
        std::fill(scene.layer_tags().begin(), scene.layer_tags().end(), std::uint8_t(PixelLayer::two_d));
        auto pixels=original;
        bloom.apply(3,scene,pixels);
        require(pixels==original,"HUD-only frame generated bloom");
    }
    {
        // Odd source dimensions leave partial reduced cells on both edges.
        // This exceeds the parallel-extraction threshold at 4x; the serial
        // raster path remains an independent accumulation-order reference.
        Framebuffer scene{257,257,4};
        scene.enable_layer_tags(true);
        std::vector<std::uint8_t> original(scene.pixels().size()*4);
        for (std::size_t i=0;i<original.size();++i) original[i]=std::uint8_t(i*73+i/17);
        for (std::size_t i=0;i<scene.pixels().size();++i)
            scene.layer_tags()[i]=std::uint8_t((i/13+i/257)%5);
        starfox::render::BloomPass reused;
        for (const auto levels : {std::array<std::uint8_t,2>{3,0},{0,3},{1,3},{3,1},{0,0}}) {
            auto serial=original,threaded=original;
            reused.apply(levels[0],levels[1],scene,serial);
            reused.apply(levels[0],levels[1],scene,threaded,&bloom_workers);
            require(serial==threaded,"parallel bloom extraction changed partial cells or layer strength");
        }
        std::fill(scene.layer_tags().begin(),scene.layer_tags().end(),std::uint8_t(PixelLayer::two_d));
        auto hud=original;
        reused.apply(3,scene,hud,&bloom_workers);
        require(hud==original,"parallel extraction retained stale bloom on HUD-only frame");
    }
    for (const bool world : {false,true}) {
        std::uint8_t style=0;
        for (unsigned i=0;i<starfox::render::effect_count*2;++i) {
            style=starfox::render::next_effect(style,world,false);
            require(starfox::render::selectable_effect(style,world),"selector exposed a removed style");
        }
    }
    {
        Framebuffer hud{256U, 224U};
        hud.enable_layer_tags(true);
        starfox::simulation::SnesPpuState ppu{};
        ppu.main_screen = 0x10U;
        ppu.object_select = 0U;
        ppu.oam[0] = 12U;
        ppu.oam[1] = 16U;
        ppu.vram[0] = 0x80U;
        starfox::render::SpriteRenderer renderer;
        renderer.draw_objects(ppu, hud);
        require(hud.layer_stored(12, 16) == PixelLayer::two_d,
            "1x HUD sprites were tagged as effect-eligible models");
        starfox::simulation::MeterState meters{};
        meters.enabled = true;
        meters.boss_max_health = 100U;
        meters.boss_health = 50U;
        renderer.draw_meters(meters, hud);
        require(hud.layer_stored(118, 2) == PixelLayer::two_d,
            "1x boss meter was tagged as an effect-eligible model");
    }
    {
        using starfox::render::Effect;
        for(bool world:{false,true}) {
            std::array<bool,starfox::render::effect_count> seen{};
            std::uint8_t current=0;
            do {
                require(!seen[current],"effect selector repeats a style before wrapping");
                seen[current]=true;
                require(current!=unsigned(Effect::ice) && current!=unsigned(Effect::bloom),"legacy duplicate remains selectable");
                require(!world || !starfox::render::reflective_material(static_cast<Effect>(current)),"model material leaked into world selector");
                const auto next=starfox::render::next_effect(current,world,false);
                require(starfox::render::next_effect(next,world,true)==current,"effect navigation is not reversible");
                current=next;
            } while(current!=0);
            for(unsigned i=0;i<seen.size();++i)
                require(seen[i]==(starfox::render::selectable_effect(i,world) && i!=unsigned(Effect::ice)
                    && (world || (!starfox::render::manipulation(static_cast<Effect>(i)) && !starfox::render::material(static_cast<Effect>(i))))),"effect missing from grouped selector");
        }
        std::array<bool,starfox::render::effect_count> manipulations{};
        std::uint8_t selected=0;
        do {
            require(!manipulations[selected],"manipulation selector repeats");
            manipulations[selected]=true;
            const auto next=starfox::render::next_manipulation(selected,false);
            require(starfox::render::next_manipulation(next,true)==selected,"manipulation navigation not reversible");
            selected=next;
        } while(selected);
        for(unsigned i=0;i<manipulations.size();++i)
            require(manipulations[i]==starfox::render::valid_manipulation(i),"missing manipulation");
        std::array<bool,starfox::render::effect_count> materials{};
        selected=0;
        do {
            require(!materials[selected],"material selector repeats");materials[selected]=true;
            const auto next=starfox::render::next_material(selected,false);
            require(starfox::render::next_material(next,true)==selected,"material navigation not reversible");selected=next;
        } while(selected);
        for(unsigned i=0;i<materials.size();++i) require(materials[i]==starfox::render::valid_material(i),"missing material");
        static_assert(unsigned(Effect::silver)-unsigned(Effect::duotone)==10,
            "ten new effects must precede the new materials");
        static_assert(unsigned(Effect::molten_glass)-unsigned(Effect::silver)+1==10,
            "the new material group must contain ten entries");
        require(starfox::render::conductor(Effect::silver)==1
            && starfox::render::conductor(Effect::brass)==2
            && starfox::render::conductor(Effect::rose_gold)==3,
            "new metallic materials lost their ray-reflection conductor");
        Framebuffer material_fixture{8U,4U};
        material_fixture.enable_layer_tags(true);
        std::fill(material_fixture.layer_tags().begin(),material_fixture.layer_tags().end(),
            std::uint8_t(PixelLayer::three_d));
        std::vector<std::uint8_t> material_source(material_fixture.pixels().size()*4U), material_scratch;
        for(std::size_t p=0;p<material_fixture.pixels().size();++p) {
            material_source[p*4]=std::uint8_t(31U+p*37U);
            material_source[p*4+1]=std::uint8_t(113U+p*19U);
            material_source[p*4+2]=std::uint8_t(217U-p*5U);
            material_source[p*4+3]=255U;
        }
        std::vector<std::vector<std::uint8_t>> signatures;
        for(unsigned id=unsigned(Effect::duotone);id<=unsigned(Effect::molten_glass);++id) {
            auto pixels=material_source;
            starfox::render::apply_effect(static_cast<Effect>(id),material_fixture,pixels,material_scratch);
            require(pixels!=material_source,"new effect/material is visually inert");
            require(std::find(signatures.begin(),signatures.end(),pixels)==signatures.end(),
                "new effects/materials produce duplicate output");
            signatures.push_back(std::move(pixels));
        }
        require(starfox::render::canonical_effect(unsigned(Effect::ice))==unsigned(Effect::cyanotype),"legacy Ice migration failed");
        Framebuffer frame{2U, 1U};
        frame.enable_layer_tags(true);
        frame.set_layer_override(PixelLayer::three_d);
        frame.set(0, 0, 1);
        frame.set_layer_override(PixelLayer::two_d);
        frame.set(1, 0, 2);
        const std::vector<std::uint8_t> original{180, 100, 40, 255, 12, 34, 56, 255};
        std::vector<std::uint8_t> scratch;
        for (unsigned effect = 0; effect < starfox::render::effect_names.size(); ++effect) {
            auto pixels = original;
            starfox::render::apply_effect(static_cast<starfox::render::Effect>(effect),
                frame, pixels, scratch);
            require(std::equal(pixels.begin() + 4, pixels.end(), original.begin() + 4),
                "effects changed HUD pixels");
            // This one-row fixture is on a bright scanline; dark alternating
            // rows are covered by the multi-row CPU/GPU fixture.
            require(effect>=unsigned(Effect::energy_shield) || (pixels == original) == (effect == 0
                || effect == unsigned(starfox::render::Effect::crosshatch)
                || effect == unsigned(starfox::render::Effect::scanlines)
                || starfox::render::spatial_manipulation(static_cast<Effect>(effect))
                || effect>=unsigned(Effect::energy_shield)
                || starfox::render::persistence_mode(static_cast<Effect>(effect))
                || starfox::render::reflective_material(static_cast<starfox::render::Effect>(effect))), "effect/off output mismatch");
            require(pixels[3] == 255, "effects changed alpha");
            auto disabled = original;
            starfox::render::apply_effect(static_cast<starfox::render::Effect>(effect),
                frame, disabled, scratch, 0U);
            require(disabled == original, "zero intensity changed pixels");
            auto halfway = original;
            starfox::render::apply_effect(static_cast<starfox::render::Effect>(effect),
                frame, halfway, scratch, 50U);
            for (unsigned c = 0; c < 3; ++c) require(
                halfway[c] == (unsigned(original[c]) + pixels[c] + 1U) / 2U,
                "intensity did not blend linearly");
        }
    }
    {
        using namespace starfox::render;
        Framebuffer fx_frame(96,64);fx_frame.enable_layer_tags(true);
        {
            Framebuffer phosphor_frame(3,1);phosphor_frame.enable_layer_tags(true);
            phosphor_frame.layer_tags()[0]=std::uint8_t(PixelLayer::background);
            phosphor_frame.layer_tags()[1]=std::uint8_t(PixelLayer::three_d);
            phosphor_frame.layer_tags()[2]=std::uint8_t(PixelLayer::two_d);
            FramePersistence phosphor;
            std::vector<std::uint8_t> bright(12,192);bright[3]=bright[7]=bright[11]=255;
            phosphor.apply(phosphor_frame,bright,PersistenceMode::phosphor,true,true,0,1,100,2);
            std::vector<std::uint8_t> dark(12,0);dark[3]=dark[7]=dark[11]=255;dark[8]=23;
            phosphor.apply(phosphor_frame,dark,PersistenceMode::phosphor,true,true,.06,1,100,2);
            require(dark[0]==48 && dark[1]==96 && dark[2]==24,"phosphor RGB decay is incorrect");
            require(dark[4]==48 && dark[5]==96 && dark[6]==24,"phosphor missed model pixels");
            require(dark[8]==23 && dark[9]==0 && dark[10]==0 && dark[11]==255,"phosphor changed HUD or alpha");
            phosphor_frame.layer_tags()[0]=std::uint8_t(PixelLayer::two_d);
            std::vector<std::uint8_t> covered(12,0);
            phosphor.apply(phosphor_frame,covered,PersistenceMode::phosphor,true,true,.07,1,100,2);
            phosphor_frame.layer_tags()[0]=std::uint8_t(PixelLayer::background);
            std::vector<std::uint8_t> revealed(12,0);
            phosphor.apply(phosphor_frame,revealed,PersistenceMode::phosphor,true,true,.08,1,100,2);
            require(revealed[0]==0 && revealed[1]==0 && revealed[2]==0,"phosphor reappeared after HUD cover");
            auto cleared=std::vector<std::uint8_t>(12,0);
            phosphor.apply(phosphor_frame,cleared,PersistenceMode::phosphor,true,true,.09,2,100,2);
            require(cleared==std::vector<std::uint8_t>(12,0),"phosphor leaked across scene transition");
            phosphor.apply(phosphor_frame,cleared,PersistenceMode::off,true,true,.10,2);
            require(phosphor.allocated_bytes()==0,"disabled phosphor retained CPU history");
            for(unsigned fps:{60U,120U,240U}) {
                FramePersistence rate;
                auto first=bright;rate.apply(phosphor_frame,first,PersistenceMode::phosphor,true,true,0,1,100,2);
                for(unsigned tick=1;tick<=fps/10;++tick) {
                    std::vector<std::uint8_t> next(12,0);
                    rate.apply(phosphor_frame,next,PersistenceMode::phosphor,true,true,double(tick)/fps,1,100,2);
                    if(tick==fps/10) require(std::abs(int(next[1])-60)<=1,"phosphor decay depends on frame rate");
                }
            }
        }
        std::vector<std::uint8_t> original(96*64*4),scratch;
        for(unsigned y=0;y<64;++y) for(unsigned x=0;x<96;++x) {
            const auto i=y*96+x;fx_frame.set_stored(x,y,1,y<4?PixelLayer::two_d:PixelLayer::three_d);
            original[i*4]=std::uint8_t(x*2);original[i*4+1]=std::uint8_t(y*3);original[i*4+2]=64;original[i*4+3]=255;
        }
        for(unsigned fx=unsigned(Effect::energy_shield);fx<effect_count;++fx) {
            auto first=original,second=original;
            apply_effect(static_cast<Effect>(fx),fx_frame,first,scratch,100,Effect::off,100,.35f);
            apply_effect(static_cast<Effect>(fx),fx_frame,second,scratch,100,Effect::off,100,1.7f);
            require(first!=original,"new FX produces no visual change");
            require(first!=second,"new FX does not animate");
            require(std::equal(first.begin(),first.begin()+96*4*4,original.begin()),"new FX changes HUD");
            for(std::size_t i=3;i<first.size();i+=4) require(first[i]==255,"new FX changes alpha");
        }
        require(!selectable_effect(unsigned(Effect::hologram),false) && valid_special_fx(unsigned(Effect::hologram)),"hologram duplicated in style selector");
        SceneFxTracker tracker;
        std::array<SceneFxEmitter,1> emitter{{{1,{0,0,500},true,false,false}}};
        const auto projection=[](const auto& p){return p;};
        auto first_ring=tracker.update(3,0.,1,emitter,{0,0,0},0,projection,48,32,256,1);
        require(first_ring.camera[3]==1,"explosion did not trigger a shockwave");
        auto paused_ring=tracker.update(3,0.,1,emitter,{0,0,0},0,projection,48,32,256,1);
        require(first_ring.data==paused_ring.data,"paused shockwave advanced or duplicated");
        auto advanced_ring=tracker.update(3,.2,1,emitter,{0,0,0},0,projection,48,32,256,1);
        require(advanced_ring.camera[3]==1 && advanced_ring.data[0][2]>first_ring.data[0][2],"shockwave failed to expand");
        auto expired_ring=tracker.update(3,.9,1,emitter,{0,0,0},0,projection,48,32,256,1);
        require(!expired_ring.active(),"same explosion retriggered after expiry");
        require(!tracker.update(0,1.,1,emitter,{0,0,0},0,projection,48,32,256,1).active(),"disabled scene effects active");
        auto eye=advanced_ring;eye.eye(4,1000);
        require(eye.data[0][0]!=advanced_ring.data[0][0],"shockwave did not receive stereo parallax");
        require(!tracker.update(192,1.,1,{}, {0,0,0},0,projection,48,32,256,1).active(),"weather emitted in non-weather stage");
        SceneFxTracker impact;
        emitter[0]={2,{0,0,500},false,false,false,20};
        require(!impact.update(0,0,1,emitter,{0,0,0},0,projection,48,32,256,1,3).active(),"undamaged object emitted particles");
        emitter[0].health=10;
        auto burst=impact.update(0,.1,1,emitter,{0,0,0},0,projection,48,32,256,1,3);
        require(burst.camera[3]==12,"damage did not emit bounded sparks and debris");
        auto paused_burst=impact.update(0,.1,1,emitter,{0,0,0},0,projection,48,32,256,1,3);
        require(burst.data==paused_burst.data && burst.camera==paused_burst.camera,"paused impact changed or duplicated");
        auto moving_burst=impact.update(0,.3,1,emitter,{0,0,0},0,projection,48,32,256,1,3);
        require(moving_burst.data!=burst.data,"impact particles did not move");
        require(!impact.update(0,.9,1,emitter,{0,0,0},0,projection,48,32,256,1,3).active(),"expired impact was retriggered without damage");
        emitter[0].explosion=true;
        require(impact.update(0,1.,1,emitter,{0,0,0},0,projection,48,32,256,1,3).camera[3]==12,"destruction did not emit particles");
        for(unsigned quality=1;quality<=3;++quality) {
            SceneFxTracker density;
            require(density.update(0,0,1,emitter,{0,0,0},0,projection,48,32,256,1,quality).camera[3]==quality*4,
                "impact quality does not control density");
        }
        std::array<SceneFxEmitter,10> crowd;
        for(unsigned i=0;i<crowd.size();++i) crowd[i]={i,{0,0,500},true,false,false,0};
        SceneFxTracker bounded;
        require(bounded.update(0,0,1,crowd,{0,0,0},0,projection,48,32,256,1,3).camera[3]==24,"impact particle cap exceeded");
        emitter[0].explosion=false;
        require(!impact.update(0,1.1,2,emitter,{0,0,0},0,projection,48,32,256,1,3).active(),"scene change retained impact particles");
        SceneFxTracker heat;
        emitter[0]={3,{0,0,500},false,false,true,20};
        require(!heat.update(0,0,1,emitter,{0,0,0},0,projection,48,32,256,1,12).active(),"heat appeared before an exhaust trail existed");
        auto hot=heat.update(0,.1,1,emitter,{0,0,0},0,projection,48,32,256,1,12);
        require(hot.active() && hot.data[1][0]==8,"exhaust-only heat toggle is inert");
        require(heat.update(0,.1,1,emitter,{0,0,0},0,projection,48,32,256,1,12).data==hot.data,"paused heat drifted");
        require(!heat.update(0,.2,1,emitter,{0,0,0},0,projection,48,32,256,1,0).active(),"disabled heat still rendered");
        SceneFxTracker near_heat;
        emitter[0].position[2]=50;
        near_heat.update(0,0,1,emitter,{0,0,0},0,projection,48,32,256,1,12);
        auto close_plume=near_heat.update(0,.1,1,emitter,{0,0,0},0,projection,48,32,256,1,12);
        require(close_plume.active() && close_plume.data[0][2]<=30,"near-camera exhaust expanded beyond its local bound");
        SurfaceBuffer flat(96,64);
        for(unsigned y=0;y<64;++y) for(unsigned x=0;x<96;++x) flat.set(x,y,{0,0,-1,500,0,true},fx_frame.pixels()[y*96+x]);
        auto hull=original;
        apply_scene_enhancements(hot,fx_frame,hull,scratch,&flat,0,0);
        require(hull==original,"exhaust heat distorts the player's depth band");
        auto focused=original;
        DepthEnhancements depth;depth.modes=15;depth.camera={48,32,256,500};
        apply_depth_enhancements(depth,fx_frame,focused,scratch,&flat,0,0);
        require(focused==original,"flat focused plane was darkened or blurred");
        for(unsigned y=0;y<64;++y) for(unsigned x=0;x<48;++x) flat.set(x,y,{0,0,-1,400,0,true},fx_frame.pixels()[y*96+x]);
        depth.modes=3;auto occluded=original;
        apply_depth_enhancements(depth,fx_frame,occluded,scratch,&flat,0,0);
        require(occluded!=original,"depth discontinuity produced no ambient occlusion");
        for(unsigned i=0;i<original.size();++i) require(occluded[i]<=original[i],"ambient occlusion brightened pixels");
        auto no_surface=original;
        apply_depth_enhancements(depth,fx_frame,no_surface,scratch,nullptr,0,0);
        require(no_surface==original,"depth effects guessed geometry without surface data");
        {
            // Analytic depth step: compare the visible AO profile against a
            // dense disk integral and the former 8-spoke/two-ring kernel.
            // This guards quality, not just agreement between two backends.
            DepthEnhancements ao;ao.modes=3;ao.camera={32,16,256,500};
            const auto geometry=[](float x,float,unsigned field) {
                return field==3?(std::floor(x)>=40?400.f:500.f):field==2?-1.f:0.f;
            };
            const auto reference=[](float x,bool old) {
                const int count=old?16:2048;double sum=0;
                for(int j=0;j<count;++j) {
                    const double angle=old?(j/2)*3.141592653589793/4:j*2.399963229728653;
                    const double r=24*(old?(j%2+1)*.5:std::sqrt((j+.5)/count));
                    const double sx=x+std::cos(angle)*r,sy=16+std::sin(angle)*r;
                    const double z=std::floor(sx)>=40?400:500;
                    const double vx=(sx-32)*z/256-(x-32)*500/256,vy=(sy-16)*z/256,vz=z-500;
                    const double distance=std::sqrt(vx*vx+vy*vy+vz*vz);
                    sum+=std::max(0.,-vz/std::max(1.,distance)-.08)*std::max(0.,1-distance/240);
                }
                return 1-std::min(.7,sum/count*1.8);
            };
            double error=0,old_error=0,jump=0,old_jump=0,previous=1,old_previous=1;
            for(int x=8;x<40;++x) {
                const double value=depth_channel([](float,float){return 1.f;},geometry,float(x),16.f,1.f,ao);
                const double truth=reference(float(x),false),old=reference(float(x),true);
                error+=(value-truth)*(value-truth);old_error+=(old-truth)*(old-truth);
                if(x>8) {jump=std::max(jump,std::abs(value-previous));old_jump=std::max(old_jump,std::abs(old-old_previous));}
                previous=value;old_previous=old;
            }
            require(error<old_error*.5,"AO disk did not improve dense-reference error");
            require(jump<old_jump*.85,"AO disk retained old spoke banding");
            {
                // Calibrated displays can have unequal horizontal/vertical
                // focal lengths. Keep the ordinary square-pixel default exact.
                ao.focal_y=127;double sum=0;
                for(unsigned j=0;j<32;++j) {
                    const double angle=j*2.399963229728653,r=24*std::sqrt((j+.5)/32);
                    const double sx=33+std::cos(angle)*r,sy=16+std::sin(angle)*r;
                    const double z=int(sx)>=40?400:500;
                    const double vx=(sx-32)*z/256-500./256,vy=(sy-16)*z/127,vz=z-500;
                    const double distance=std::sqrt(vx*vx+vy*vy+vz*vz);
                    sum+=std::max(0.,-vz/std::max(1.,distance)-.08)*std::max(0.,1-distance/240);
                }
                const float asymmetric=depth_ambient(geometry,33.F,16.F,1.F,ao);
                require(std::abs(asymmetric-(1-std::min(.7,sum/32*1.8)))<.00002,
                    "AO ignored independent calibrated vertical focal length");
                ao.focal_y=0;
                require(std::abs(asymmetric-depth_ambient(geometry,33.F,16.F,1.F,ao))>.0001,
                    "AO asymmetric fixture did not distinguish square-pixel fallback");
            }
            for(unsigned mode=0;mode<16;++mode) {
                ao.modes=mode;ao.camera[3]=100;
                unsigned queries=0;
                const auto counted=[&](float x,float y,unsigned field) {++queries;return geometry(x,y,field);};
                std::array<float,3> repeated{},shared{};
                for(unsigned channel=0;channel<3;++channel)
                    repeated[channel]=depth_channel([&](float x,float y){return (x+y+channel)/128.f;},counted,33.f,16.f,1.f,ao);
                const auto repeated_queries=queries;queries=0;
                const float ambient=depth_ambient(counted,33.f,16.f,1.f,ao);
                for(unsigned channel=0;channel<3;++channel)
                    shared[channel]=depth_channel([&](float x,float y){return (x+y+channel)/128.f;},counted,33.f,16.f,1.f,ao,ambient);
                require(repeated==shared,"shared AO changed colour or DOF ordering");
                require((mode&3)?queries<repeated_queries:queries==repeated_queries,"shared AO failed to remove redundant surface queries");
                if((mode&3) && !(mode&12)) require(queries*2<repeated_queries,"AO-only did not eliminate most duplicate queries");
            }
        }
        for(unsigned y=4;y<64;++y) for(unsigned x=0;x<96;++x) for(unsigned c=0;c<3;++c)
            original[(y*96+x)*4+c]=std::uint8_t(((x/3+y/5)%2)?255:32);
        auto disabled=original;
        scratch.clear();apply_global_enhancements(0,fx_frame,disabled,scratch,.35f);
        require(disabled==original && scratch.empty(),"disabled global FX did work");
        for(unsigned fx=0;fx<global_enhancement_count;++fx) {
            auto first=original,second=original;
            apply_global_enhancements(3U<<(2*fx),fx_frame,first,scratch,.35f);
            apply_global_enhancements(3U<<(2*fx),fx_frame,second,scratch,.35f);
            require(first!=original,"global enhancement has no visual effect");
            require(first==second,"global enhancement changes while time is frozen");
            require(std::equal(first.begin(),first.begin()+96*4*4,original.begin()),"global enhancement changes HUD");
            for(std::size_t i=3;i<first.size();i+=4) require(first[i]==255,"global enhancement changes alpha");
        }
    }
    for(auto manipulation:{starfox::render::Effect::kaleidoscope,starfox::render::Effect::prism_split,starfox::render::Effect::pixel_sort,
        starfox::render::Effect::shatter,starfox::render::Effect::melt,starfox::render::Effect::ripple_warp,
        starfox::render::Effect::barrel_warp,starfox::render::Effect::venetian,starfox::render::Effect::checker_fold,
        starfox::render::Effect::twist,starfox::render::Effect::ring_ripple,starfox::render::Effect::shard_split}) {
        Framebuffer pattern(32,16);pattern.enable_layer_tags(true);
        std::fill(pattern.layer_tags().begin(),pattern.layer_tags().end(),std::uint8_t(PixelLayer::three_d));
        std::vector<std::uint8_t> input(pattern.pixels().size()*4),scratch;
        for(std::size_t i=0;i<pattern.pixels().size();++i) {
            input[i*4]=std::uint8_t(255-i%32*7);input[i*4+1]=std::uint8_t(i/32*13);
            input[i*4+2]=std::uint8_t(i*17);input[i*4+3]=255;
        }
        pattern.layer_tags()[0]=std::uint8_t(PixelLayer::two_d);
        auto result=input;
        starfox::render::apply_effect(manipulation,pattern,result,scratch);
        require(result!=input,"manipulation failed to transform a patterned surface");
        require(std::equal(result.begin(),result.begin()+4,input.begin()),"manipulation changed HUD");
        for(std::size_t i=3;i<result.size();i+=4)require(result[i]==255,"manipulation changed opacity");
    }
    for(unsigned scale:{1U,2U,4U}) for(auto layer:{PixelLayer::three_d,PixelLayer::background}) {
        Framebuffer flat(8*scale,8*scale);flat.set_draw_scale(scale);flat.enable_layer_tags(true);
        flat.clear(1);std::fill(flat.layer_tags().begin(),flat.layer_tags().end(),std::uint8_t(layer));
        std::vector<std::uint8_t> pixels(flat.pixels().size()*4),scratch;
        for(std::size_t i=0;i<flat.pixels().size();++i) {
            pixels[i*4]=30;pixels[i*4+1]=80;pixels[i*4+2]=110;pixels[i*4+3]=255;
        }
        starfox::render::apply_effect(starfox::render::Effect::comic,flat,pixels,scratch,100,
            starfox::render::Effect::comic,100);
        for(std::size_t i=1;i<flat.pixels().size();++i) for(unsigned c=0;c<4;++c)
            require(pixels[i*4+c]==pixels[c],"Comic added dots to a flat model/world surface");
    }
    for (const auto scale : {1U, 2U, 3U, 4U, 6U, 10U}) {
        check_filter(TwoDFilter::edge, scale, dump_directory);
        check_filter(TwoDFilter::sharp_bilinear, scale, dump_directory);
        check_filter(TwoDFilter::crt, scale, dump_directory);
        check_filter(TwoDFilter::scalefx, scale, dump_directory);
        if (starfox::render::two_d_filter_compiled_in(TwoDFilter::xbrz)) {
            check_filter(TwoDFilter::xbrz, scale, dump_directory);
        }
    }

    {
        Framebuffer frame{5U, 5U};
        frame.enable_layer_tags(true);
        frame.set_layer_override(PixelLayer::two_d);
        frame.set(2, 2, 1);
        std::vector<std::uint8_t> pixels(5U * 5U * 4U, 0U), scratch;
        for (std::size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
        for (unsigned c = 0; c < 3; ++c) pixels[(2 * 5 + 2) * 4 + c] = 255;
        const auto original = pixels;
        starfox::render::apply_effect(starfox::render::Effect::bloom, frame, pixels, scratch);
        require(pixels == original, "bright HUD leaked into bloom");
        frame.set_layer_override(PixelLayer::three_d);
        frame.set(2, 2, 1);
        starfox::render::apply_effect(starfox::render::Effect::bloom, frame, pixels, scratch);
        require(pixels[(1 * 5 + 1) * 4] > original[(1 * 5 + 1) * 4],
            "bright world pixel produced no bloom halo");
    }
    {
        Framebuffer frame{4U, 2U};
        frame.enable_layer_tags(true);
        frame.layer_tags() = {std::uint8_t(PixelLayer::three_d), std::uint8_t(PixelLayer::background),
            std::uint8_t(PixelLayer::two_d), std::uint8_t(PixelLayer::world_geometry),
            std::uint8_t(PixelLayer::three_d),std::uint8_t(PixelLayer::background),
            std::uint8_t(PixelLayer::two_d),std::uint8_t(PixelLayer::world_geometry)};
        const std::vector<std::uint8_t> original{180,100,40,255,180,100,40,255,180,100,40,255,180,100,40,255,
            180,100,40,255,180,100,40,255,180,100,40,255,180,100,40,255};
        std::vector<std::uint8_t> scratch;
        using starfox::render::Effect;
        for (unsigned style = 8; style < unsigned(Effect::energy_shield); ++style) {
            const auto effect = static_cast<Effect>(style);
            for (const bool world : {false, true}) {
                auto pixels = original;
                starfox::render::apply_effect(world ? Effect::off : effect,
                    frame, pixels, scratch, 100, world ? effect : Effect::off, 100);
                for (unsigned pixel = 0; pixel < 8; ++pixel) {
                    const bool selected = (world ? pixel%4 == 1 || pixel%4 == 3 : pixel%4 == 0)
                        && !starfox::render::reflective_material(effect)
                        && effect!=Effect::crosshatch
                        && !starfox::render::spatial_manipulation(effect)
                        && !starfox::render::persistence_mode(effect)
                        && (effect!=Effect::scanlines || pixel>=4);
                    const bool same = std::equal(pixels.begin() + pixel * 4,
                        pixels.begin() + pixel * 4 + 3, original.begin() + pixel * 4);
                    require(same != selected, "new style missed its layer or leaked into another");
                    require(pixels[pixel * 4 + 3] == 255, "new style changed alpha");
                }
            }
        }
        auto model_only = original;
        starfox::render::apply_effect(Effect::monochrome, frame, model_only, scratch);
        require(model_only[0] != original[0] && model_only[4] == original[4]
                && model_only[8] == original[8] && model_only[12] == original[12],
            "model effects leaked into world or HUD");
        auto world_only = original;
        starfox::render::apply_effect(Effect::off, frame, world_only, scratch, 100, Effect::monochrome, 100);
        require(world_only[0] == original[0] && world_only[4] != original[4]
                && world_only[8] == original[8] && world_only[12] != original[12],
            "world effects missed scenery or changed model/HUD");
    }
    {
        using namespace starfox::render;
        require(valid_environment({1,9,3,1,3,2}) && !valid_environment({1,10,0,0,0,0}),"environment setting bounds");
        starfox::simulation::SnesPpuState p;
        p.tunnel_scene=true;
        const auto mask=environment_palette_regions(p,232);
        require(std::all_of(mask.begin(),mask.end(),[](auto v){return v==0;}),"tunnel was treated as landscape");
        Framebuffer f(5,1);f.enable_layer_tags(true);
        for(unsigned i=0;i<5;++i) {f.pixels()[i]=1;f.layer_tags()[i]=i;}
        std::vector<std::uint8_t> rgb(20,100);const auto original=rgb;
        EnvironmentEffects e;e.classes[1]=1;e.modes={6,0,1,0};e.motion={-10,0,0,0};
        apply_environment(e,f,rgb);
        for(unsigned i=0;i<5;++i) if(i!=2) require(std::equal(rgb.begin()+i*4,rgb.begin()+i*4+4,original.begin()+i*4),"environment affected protected ink");
        e.modes={};rgb=original;apply_environment(e,f,rgb);require(rgb==original,"disabled environment changed pixels");
    }
    {
        Framebuffer f(4,4,2);
        require(f.dither_pairs().empty(),"dither provenance allocated without request");
        f.enable_dither_pairs(true);f.set_stored(1,1,17);f.set_dither_alternate(9,53);
        require(f.dither_pairs()[9]==(256|53),"dither alternate missing");
        Framebuffer copy(4,4,2);copy.copy_pixels_from(f);
        require(copy.dither_pairs()[9]==(256|53),"cached frame lost dither provenance");
        f.set_stored(1,1,17,starfox::render::PixelLayer::two_d);
        require(f.dither_pairs()[9]==0,"same-color HUD retained hidden dither material");
        copy.clear();require(copy.dither_pairs()[9]==0,"clear retained dither provenance");
        copy.set_dither_alternate(9,0);copy.resize(5,5);
        require(copy.dither_pairs().size()==100 && copy.dither_pairs()[9]==0,"resize retained stale dither provenance");
        copy.enable_dither_pairs(false);require(copy.dither_pairs().empty(),"disabled dither provenance retained storage");
    }
    for(unsigned size:{2U,300U}) {
        Framebuffer f(size,size,2);f.enable_layer_tags(true);f.enable_dither_pairs(true);
        const std::array<Rgba8,2> palette{{{20,40,60,77},{101,151,201,99}}};
        for(std::size_t i=0;i<f.pixels().size();++i) {
            f.pixels()[i]=i&1;f.set_dither_alternate(i,std::uint8_t((i&1)^1));
        }
        f.layer_tags()[0]=std::uint8_t(starfox::render::PixelLayer::two_d);
        std::vector<uint8_t> serial,parallel;starfox::render::RowWorkers pool;
        starfox::render::expand_rgba(f,serial,palette);
        starfox::render::expand_rgba(f,parallel,palette,pool);
        require(serial==parallel,"parallel material pair resolve mismatch");
        require(serial[0]==20 && serial[1]==40 && serial[2]==60,"material pair touched HUD");
        for(std::size_t i=1;i<f.pixels().size();++i)
            require(serial[i*4]==61 && serial[i*4+1]==96 && serial[i*4+2]==131 && serial[i*4+3]==palette[i&1].a,"source pair did not resolve exactly");
        f.set_draw_scale(1);starfox::render::expand_rgba(f,serial,palette,pool);
        for(std::size_t i=0;i<f.pixels().size();++i)
            require(serial[i*4]==palette[i&1].r && serial[i*4+1]==palette[i&1].g,"native 1x material colors changed");
    }
    {
        Framebuffer source(4,4,2),destination(4,4,2);
        source.enable_dither_pairs(true);source.set_layer_override(starfox::render::PixelLayer::three_d);
        source.set_draw_scale(1);source.set(1,1,0);source.annotate_dither(1,1,0,1);source.set_draw_scale(2);
        const std::array<Rgba8,2> palette{{{0,0,0,255},{100,150,200,255}}};
        for(unsigned mosaic:{0U,0x11U}) {
            destination.clear();starfox::render::LayerCompositeSettings settings;settings.mosaic=mosaic;settings.mosaic_layer_mask=1;
            starfox::render::composite_transparent_layer(source,destination,settings);
            if(!mosaic) {
                require(destination.dither_pairs()[9]==257,"composition lost palette-zero dither material");
                std::vector<uint8_t> rgba;starfox::render::expand_rgba(destination,rgba,palette);
                require(rgba[36]==50 && rgba[37]==75 && rgba[38]==100,"composited source material not resolved");
            }
        }
    }
    {
        using namespace starfox::render;
        Framebuffer frame(20,10);frame.enable_layer_tags(true);
        std::fill(frame.layer_tags().begin(),frame.layer_tags().end(),std::uint8_t(PixelLayer::background));
        frame.layer_tags()[0]=std::uint8_t(PixelLayer::two_d);
        std::vector<std::uint8_t> source(800,40);
        for(unsigned i=0;i<200;++i) source[i*4+3]=123;
        float reference=0;
        for(unsigned fps:{60U,120U,240U}) {
            AdaptiveExposure exposure;
            auto rgba=source;exposure.apply(frame,rgba,3,0,1);
            require(rgba==source,"exposure popped on scene entry");
            for(unsigned t=1;t<=fps;++t) {rgba=source;exposure.apply(frame,rgba,3,double(t)/fps,1);}
            if(reference) require(std::abs(exposure.stops()-reference)<.00001f,"exposure depends on frame rate");
            reference=exposure.stops();
            require(reference>0 && reference<=1.5f && rgba[4]>source[4],"exposure did not brighten dark scene within bounds");
            require(rgba[0]==source[0] && rgba[1]==source[1] && rgba[2]==source[2],"exposure changed HUD");
            for(unsigned i=0;i<200;++i) require(rgba[i*4+3]==123,"exposure changed alpha");
            rgba=source;exposure.apply(frame,rgba,3,1,1);
            require(exposure.stops()==reference,"paused exposure drifted");
            rgba=source;exposure.apply(frame,rgba,3,1,2);
            require(rgba==source && exposure.stops()==0,"exposure leaked across scene epoch");
            exposure.apply(frame,rgba,0,2,2);require(rgba==source,"disabled exposure changed pixels");
        }
        AdaptiveExposure exposure;
        auto black=source;std::fill(black.begin(),black.end(),0);
        exposure.apply(frame,black,3,0,1);exposure.apply(frame,black,3,.5,1);
        require(exposure.stops()==0,"black/transparent frame inflated exposure");
        auto bright=source;
        for(unsigned i=0;i<200;++i) for(unsigned c=0;c<3;++c) bright[i*4+c]=240;
        exposure.reset();auto rgba=bright;exposure.apply(frame,rgba,2,0,1);
        rgba=bright;exposure.apply(frame,rgba,2,.5,1);
        require(exposure.stops()<0 && exposure.stops()>=-1 && rgba[4]<240,"bright-scene exposure did not adapt down");
        AdaptiveExposure baseline,flashes;
        auto flashed=source;
        for(unsigned c=0;c<3;++c) {flashed[c]=255;flashed[4+c]=255;}
        auto a=source,b=flashed;
        baseline.apply(frame,a,3,0,1);flashes.apply(frame,b,3,0,1);
        a=source;b=flashed;baseline.apply(frame,a,3,.5,1);flashes.apply(frame,b,3,.5,1);
        require(std::abs(baseline.stops()-flashes.stops())<.00001f,"HUD or isolated highlights pumped exposure");
        const auto held=baseline.stops();a=bright;
        baseline.apply(frame,a,3,.75,1,true);
        require(baseline.stops()==held,"paused exposure changed with advancing wall clock");
        a=source;baseline.apply(frame,a,3,.1,1);
        require(a==source && baseline.stops()==0,"rewinding exposure retained old adaptation");
    }
    {
        using starfox::render::water_caustic_irradiance;
        require(water_caustic_irradiance(12,23,1,0,1)==0
            && water_caustic_irradiance(12,23,1,-10,1)==0,"caustics lit dry geometry");
        float low=10,high=0,animation=0,filtered_low=10,filtered_high=0;
        for(int z=-250;z<=250;z+=10) for(int x=-250;x<=250;x+=10) {
            const float light=water_caustic_irradiance(float(x),float(z),2,500,1);
            const float later=water_caustic_irradiance(float(x),float(z),2.1f,500,1);
            const float filtered=water_caustic_irradiance(float(x),float(z),2,500,1000);
            require(std::isfinite(light) && light>=0 && light<=6,"unbounded caustic focus");
            low=std::min(low,light);high=std::max(high,light);
            filtered_low=std::min(filtered_low,filtered);filtered_high=std::max(filtered_high,filtered);
            animation+=std::abs(light-later);
        }
        require(high-low>.1f && animation>1,"caustics lack moving light concentration");
        require(filtered_high-filtered_low<.01f,"distant caustics alias instead of filtering out");
        const float a=water_caustic_irradiance(32,52,2,500,1);
        require(a==water_caustic_irradiance(32,52,2,500,1),"caustics are not deterministic");
        require(std::abs(a-water_caustic_irradiance(32,52,2.0001f,500,1))<.01f,"caustics jump at small time steps");
    }
    {
        using namespace starfox::render;
        Framebuffer frame(96,64);frame.enable_layer_tags(true);
        std::fill(frame.pixels().begin(),frame.pixels().end(),7);
        std::fill(frame.layer_tags().begin(),frame.layer_tags().end(),std::uint8_t(PixelLayer::three_d));
        SurfaceBuffer surfaces(96,64);
        for(int y=0;y<64;++y) for(int x=0;x<96;++x) {
            if(x==2 && y==0) continue;
            SurfaceSample s;s.depth=x==3 && y==0?100.f:500.f;
            s.normal_z=x==4 && y==0?1.f:-1.f;surfaces.set(x,y,s,7);
        }
        frame.layer_tags()[0]=std::uint8_t(PixelLayer::two_d);frame.pixels()[1]=8;
        WaterCaustics settings;settings.quality=3;settings.seconds=2;settings.water_y=200;
        settings.projection={48,32,64};
        settings.view_to_world={{{1,0,0,0},{0,0,1,0},{0,-1,0,0}}};
        std::vector<std::uint8_t> source(frame.pixels().size()*4,100);
        for(unsigned i=3;i<source.size();i+=4)source[i]=123;
        const auto water=[](const auto&){return true;};
        const auto lit=[](const auto&,const auto&){return 1.f;};
        auto output=source;apply_water_caustics(settings,frame,output,surfaces,0,0,water,lit);
        unsigned changed=0;
        for(unsigned i=0;i<frame.pixels().size();++i) {
            if(output[i*4]!=source[i*4])++changed;
            require(output[i*4+3]==123,"caustics changed alpha");
        }
        require(changed>100,"submerged receiver did not receive caustics");
        for(unsigned i=0;i<5;++i) require(output[i*4]==source[i*4],"caustics affected HUD, stale, absent, dry or downward-facing surface");
        output=source;apply_water_caustics(settings,frame,output,surfaces,0,0,water,
            [](const auto&,const auto&){return 0.f;});
        require(output==source,"occluded receiver received caustics");
        output=source;apply_water_caustics(settings,frame,output,surfaces,0,0,
            [](const auto&){return false;},lit);
        require(output==source,"receiver outside water coverage received caustics");
        settings.quality=0;output=source;apply_water_caustics(settings,frame,output,surfaces,0,0,water,lit);
        require(output==source,"disabled caustics changed pixels");
    }
    {
        using namespace starfox::render;
        WaterCaustics settings;
        shadows::Scene scene;scene.build();
        const std::array<float,3> receiver{0,300,0},entry{0,0,0};
        require(WaterCausticVisibility(scene,settings)(receiver,entry)==1,"empty light path blocked caustics");
        for(float y:{150.f,-100.f}) {
            scene.clear();scene.add({{-100,y,-100},{100,y,-100},{0,y,100}});scene.build();
            require(WaterCausticVisibility(scene,settings)(receiver,entry)==0,
                "submerged/above-water geometry failed to block caustic light path");
        }
        scene.clear();scene.add({{1000,150,-100},{1200,150,-100},{1100,150,100}});scene.build();
        require(WaterCausticVisibility(scene,settings)(receiver,entry)==1,"off-path object blocked caustics");
        settings.view_to_world={{{1,0,0,10},{0,0,1,20},{0,-1,0,30}}};
        scene.clear();scene.add({{-110,130,130},{90,130,130},{-10,-70,130}});scene.build();
        require(WaterCausticVisibility(scene,settings)(receiver,entry)==0,
            "rotated/translated camera changed caustic occlusion");
    }
    {
        using namespace starfox::render::water_optics;
        require(water_transmitted_channel(.8f,.1f,0,.0025f)==.8f,"zero water path hid receiver");
        require(water_transmittance(100,.0025f)>water_transmittance(1000,.0025f),"deeper water became clearer");
        require(water_transmittance(1000,.0025f)<water_transmittance(1000,.0008f)
            && water_transmittance(1000,.0008f)<water_transmittance(1000,.00035f),"water absorption did not preserve blue longer");
        require(std::abs(water_transmitted_channel(.8f,.1f,65536,.0025f)-.1f)<.0001f,"deep water leaked distant receiver");
        for(float distance:{0.f,1.f,100.f,1000.f,65536.f}) {
            const float result=water_transmitted_channel(.8f,.1f,distance,.0008f);
            require(result>=.1f && result<=.8f,"water transport created energy");
        }
    }
    {
        using namespace starfox::render;
        shadows::Scene scene;
        WaterCaustics settings;settings.water_y=0;settings.quality=3;
        std::array<Rgba8,256> palette{};
        const std::array<float,3> body{.02f,.08f,.14f};
        scene.build();
        const auto bed=water_receiver(scene,settings,{0,1,0},{0,1,0},palette,body,1);
        settings.quality=0;
        const auto unlit=water_receiver(scene,settings,{0,1,0},{0,1,0},palette,body,1);
        require(bed!=unlit,"CPU water bed has no caustic lighting");
        require(water_receiver(scene,settings,{0,1,0},{0,-1,0},palette,body,1)==body,"upward water ray hit bed");
        shadows::Triangle floor{{-1000,100,-1000},{1000,100,-1000},{0,100,1000}};
        floor.reflection_valid=true;floor.reflection_even=2;floor.reflection_odd=2;
        scene.add(floor);scene.build();palette[2]={255,0,0,255};
        const auto red=water_receiver(scene,settings,{0,1,0},{0,1,0},palette,body,1);
        palette[2]={0,255,0,255};
        const auto green=water_receiver(scene,settings,{0,1,0},{0,1,0},palette,body,1);
        require(red[0]>red[1] && green[1]>green[0],"CPU water failed submerged material transmission");
        scene.add({{-1000,-100,-1000},{1000,-100,-1000},{0,-100,1000}});scene.build();
        const auto blocked=water_receiver(scene,settings,{0,1,0},{0,1,0},palette,body,1);
        settings.quality=3;
        require(water_receiver(scene,settings,{0,1,0},{0,1,0},palette,body,1)==blocked,"CPU caustics leaked through overhead geometry");
        const WaterCausticVisibility cached(scene,settings);
        auto surface_settings=settings;surface_settings.water_y=100;surface_settings.projection={0,0,256};
        const WaterCausticVisibility surface_visibility(scene,surface_settings);
        require(!water_surface_sample(scene,surface_settings,palette,0,-10,4,surface_visibility),"water grid traced above horizon");
        require(water_surface_sample(scene,surface_settings,palette,0,100,4,surface_visibility).has_value(),"water grid missed visible water");
        for(unsigned sample=0;sample<32;++sample) {
            const shadows::Vec3 origin{double(sample)*3,1,double(sample)*2};
            require(water_receiver(scene,settings,origin,{0,1,0},palette,body,1,&cached)
                ==water_receiver(scene,settings,origin,{0,1,0},palette,body,1),
                "cached water visibility changed receiver lighting");
        }
    }
    for(unsigned quality:{0U,1U,2U,3U}) for(unsigned scale:{1U,2U,4U}) {
        using namespace starfox::render;
        Framebuffer frame{64,64,scale};frame.enable_layer_tags(true);
        shadows::Scene scene;scene.build();
        EnvironmentEffects effect;effect.modes[0]=6;effect.classes.fill(5);effect.plane[3]=1;
        effect.cpu_water_scene=&scene;effect.cpu_water.quality=quality;effect.cpu_water.water_y=100;
        effect.cpu_water.projection={8.f*scale,0,16.f*scale};
        const std::vector<std::uint8_t> source(frame.pixels().size()*4,173);
        for(auto layer:{PixelLayer::two_d,PixelLayer::three_d,PixelLayer::textured_geometry}) {
            std::fill(frame.layer_tags().begin(),frame.layer_tags().end(),std::uint8_t(layer));
            auto pixels=source;apply_environment(effect,frame,pixels);
            require(pixels==source,"software water grid overwrote HUD/model ownership");
        }
        std::fill(frame.layer_tags().begin(),frame.layer_tags().end(),std::uint8_t(PixelLayer::background));
        auto pixels=source;apply_environment(effect,frame,pixels);
        require(pixels!=source,"software water grid did not shade ground");
        RowWorkers workers;workers.set_worker_count(4);
        auto parallel=source;apply_environment(effect,frame,parallel,&workers);
        require(parallel==pixels,"parallel water grid changed the image");
        for(std::size_t i=3;i<pixels.size();i+=4) require(pixels[i]==173,"software water grid changed alpha");
    }
    for(float depth:{1.f,100.f,640.f,1600.f}) for(float footprint:{0.f,1.f,50.f}) for(unsigned sample=0;sample<32;++sample) {
        using namespace starfox::render::caustics_detail;
        const float x=float(sample)*7.3f,z=float(sample)*-11.7f,t=float(sample)*.2f,h=.05f;
        const auto d=water_light_differential(x,z,t,depth,footprint);
        const auto p=water_light_landing(x,z,t,depth,footprint);
        require(std::abs(d.x-p.x)<.0001f && std::abs(d.z-p.z)<.0001f,"analytic Snell displacement differs");
        const auto a=water_light_landing(x+h,z,t,depth,footprint),b=water_light_landing(x-h,z,t,depth,footprint);
        const auto c=water_light_landing(x,z+h,t,depth,footprint),e=water_light_landing(x,z-h,t,depth,footprint);
        require(std::abs(d.xx-(a.x-b.x)/(2*h))<.002f && std::abs(d.zx-(a.z-b.z)/(2*h))<.002f
            && std::abs(d.xz-(c.x-e.x)/(2*h))<.002f && std::abs(d.zz-(c.z-e.z)/(2*h))<.002f,
            "analytic caustic derivatives disagree with central differences");
    }
    std::cout << "pixel filter tests passed\n";
    return 0;
}
