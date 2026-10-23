#include "starfox/render/background_renderer.hpp"
#include "starfox/render/row_workers.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "tunnel_margin_cases.hpp"
#include "starfox/platform/nintendo_3ds/bg1_overlay_dependencies.hpp"
#include "starfox/platform/nintendo_3ds/indexed_overlay_rows.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
using namespace starfox::render;
using starfox::simulation::SnesPpuState;

std::uint32_t random_word(std::uint32_t& seed) {
    seed ^= seed << 13U; seed ^= seed >> 17U; seed ^= seed << 5U;
    return seed;
}
void word(SnesPpuState& ppu, unsigned address, std::uint16_t value) {
    const unsigned byte = (address & 32767U) * 2U;
    ppu.vram[byte] = std::uint8_t(value);
    ppu.vram[byte + 1U] = std::uint8_t(value >> 8U);
}
SnesPpuState source(unsigned n) {
    SnesPpuState ppu;
    std::uint32_t seed = 0x6842de19U + n * 0x9e3779b9U;
    for (auto& byte : ppu.vram) byte = std::uint8_t(random_word(seed));
    for (unsigned i = 0; i < ppu.cgram.size(); ++i)
        ppu.cgram[i] = i % 13U == 0 ? 0 : std::uint16_t(random_word(seed) & 32767U);
    // Transparent ink, opaque CGRAM black, all palettes/priorities/flips and
    // sub-character carry, including maps and bitplanes wrapping at 64 KiB.
    ppu.main_screen = n % 29U == 0 ? 0 : 7;
    ppu.background_mode = std::uint8_t(1U + n % 3U);
    ppu.bg1_character_base = n % 5U ? 0x1000 : 0x7ffe;
    ppu.bg2_character_base = n % 7U ? 0x1800 : 0x7ff9;
    ppu.bg3_character_base = n % 11U ? 0x2800 : 0x7ffd;
    ppu.bg1_screen_base = n % 9U ? 0x6000 : 0x7ffb;
    ppu.bg2_screen_base = n % 13U ? 0x6800 : 0x7ffa;
    ppu.bg3_screen_base = n % 17U ? 0x7000 : 0x7ffc;
    ppu.bg1_screen_size = std::uint8_t(n & 3U);
    ppu.bg2_screen_size = std::uint8_t((n >> 2U) & 3U);
    ppu.bg3_screen_size = std::uint8_t((n >> 4U) & 3U);
    ppu.bg1_tile_size_16 = (n & 4U) != 0;
    ppu.bg2_tile_size_16 = (n & 16U) != 0;
    ppu.bg3_tile_size_16 = (n & 64U) != 0;
    ppu.bg1_scroll_x = std::int16_t(random_word(seed));
    ppu.bg1_scroll_y = std::int16_t(random_word(seed));
    ppu.bg2_scroll_x = std::int16_t(random_word(seed));
    ppu.bg2_scroll_y = std::int16_t(random_word(seed));
    ppu.bg3_scroll_x = std::int16_t(random_word(seed));
    ppu.bg3_scroll_y = std::int16_t(random_word(seed));
    ppu.mosaic = n % 4U ? 0 : std::uint8_t(((n % 16U) << 4U) | 7U);
    ppu.tunnel_scene = n % 6U == 0;
    ppu.bg2_horizontal_offsets_enabled = n % 3U == 1;
    ppu.bg2_scanline_scroll_enabled = n % 3U != 0;
    ppu.bg2_vertical_offsets_enabled = n % 5U != 0;
    for (unsigned y = 0; y < 224; ++y) {
        ppu.bg2_horizontal_offsets[y] = std::int16_t(int(y) / 7 - 31);
        ppu.bg2_scanline_scroll_y[y] = std::int16_t(y < 112U ? 137 : -97);
    }
    for (unsigned x = 0; x < 32; ++x) {
        const bool valid = n % 4U == 1 || (n % 4U == 2 && x % 3U != 0);
        word(ppu, 0x2fa0U + x, std::uint16_t((valid ? 0x4000U : 0)
            | (unsigned(int(x) * 7 - 119) & 8191U)));
    }
    return ppu;
}
void add(std::uint64_t& hash, std::span<const std::uint8_t> bytes) {
    for (const auto byte : bytes) { hash ^= byte; hash *= 1099511628211ULL; }
}
void number(std::uint64_t& hash, std::uint32_t value) {
    const std::array bytes{std::uint8_t(value), std::uint8_t(value >> 8U),
        std::uint8_t(value >> 16U), std::uint8_t(value >> 24U)};
    add(hash, bytes);
}
void draw(const SnesPpuState& ppu, Framebuffer& frame, unsigned n,RowWorkers* workers=nullptr) {
    BackgroundRenderer renderer;renderer.row_workers=workers;
    const auto priority = static_cast<TilePriorityPass>(n % 3U);
    const int origin = int(frame.width()) / 2 - 128 + int(n % 9U) - 4;
    const bool extend = n % 4U != 1;
    const BackgroundUniqueRegion region{48, 8, 208, 192, 1, 127, 4,
        int(128 | (n % 2U ? BackgroundUniqueRegion::suppress_every_copy
            : BackgroundUniqueRegion::suppress_all_side_copies))};
    {
        const ScopedLayer tag(frame, PixelLayer::background);
        renderer.draw_bg2(ppu, ppu.bg2_scroll_x, ppu.bg2_scroll_y, frame,
            priority, origin, extend, n % 5U != 0, n % 3U == 0,
            n % 7U == 0 ? 112U : 0U,
            n % 8U == 0 ? std::span(&region, 1) : std::span<const BackgroundUniqueRegion>{},
            n % 19U == 0, n % 23U == 0, n % 11U == 0 ? 72U : 0U);
    }
    {
        const ScopedLayer tag(frame, PixelLayer::two_d);
        renderer.draw_bg1(ppu, frame, priority, origin, extend, n % 19U,
            n % 4U == 0, n % 9U == 0, n % 13U == 0);
    }
    renderer.draw_bg3(ppu, frame, priority, origin, extend);
}
std::uint64_t oracle_trace(RowWorkers* workers=nullptr) {
    std::uint64_t hash = 14695981039346656037ULL;
    constexpr std::array widths{0U, 1U, 127U, 256U, 400U, 853U};
    constexpr std::array heights{1U, 97U, 192U, 224U};
    for (unsigned n = 0; n < 192; ++n) {
        auto ppu = source(n);
        const unsigned scale = n % 17U == 0 ? 2U : 1U;
        Framebuffer frame(widths[n % widths.size()], heights[(n / 6U) % heights.size()], scale);
        frame.enable_layer_tags(true); frame.enable_dither_pairs(true);
        frame.clear(37); frame.begin_write_coverage();
        for (unsigned i = 0; i < frame.pixels().size(); ++i)
            frame.set_dither_alternate(i, std::uint8_t(i % 251U));
        draw(ppu, frame, n,workers);
        // The old point-writer trace below is pinned independently. Native
        // bulk writes must reproduce that same covered/tagged source image.
        Framebuffer native(widths[n % widths.size()], heights[(n / 6U) % heights.size()], scale);
        native.enable_layer_tags(true); native.clear(37); native.begin_write_coverage();
        draw(ppu, native, n,workers);
        if (native.pixels() != frame.pixels() || native.layer_tags() != frame.layer_tags()
            || !std::equal(native.write_coverage().begin(), native.write_coverage().end(),
                frame.write_coverage().begin()))
            throw std::runtime_error("native bulk tile-row writes differ from the pinned point-writer image");
        number(hash, n); add(hash, frame.pixels()); add(hash, frame.layer_tags());
        add(hash, frame.write_coverage());
        for (auto pair : frame.dither_pairs()) number(hash, pair);
    }
    // Command recording must retain its exact ordered point writes, rather
    // than silently replacing the GPU's source-raster interface with a span.
    for (unsigned n = 0; n < 12; ++n) {
        auto ppu = source(n + 9U);
        Framebuffer frame(33, 29, n % 2U + 1U);
        RasterCommands commands; commands.reset(frame.stored_width(), frame.stored_height());
        frame.record_to(&commands); draw(ppu, frame, n + 9U,workers);
        number(hash, std::uint32_t(commands.commands.size()));
        for (const auto& command : commands.commands) {
            number(hash, unsigned(command.left)); number(hash, unsigned(command.top));
            number(hash, unsigned(command.right)); number(hash, unsigned(command.bottom));
            number(hash, command.even); number(hash, command.odd); number(hash, command.tag);
        }
        add(hash, frame.pixels());
    }
    return hash;
}
void benchmark() {
    using Clock = std::chrono::steady_clock;
    const BackgroundRenderer renderer;
    auto ppu = source(2);
    ppu.main_screen = 7; ppu.mosaic = 0; ppu.background_mode = 2;
    ppu.bg1_tile_size_16 = ppu.bg2_tile_size_16 = ppu.bg3_tile_size_16 = false;
    ppu.bg2_vertical_offsets_enabled = false; ppu.bg2_scanline_scroll_enabled = false;
    ppu.bg2_horizontal_offsets_enabled = false; ppu.tunnel_scene = false;
    Framebuffer frame(400, 224); frame.enable_layer_tags(true); frame.begin_write_coverage();
    std::uint64_t hash = 14695981039346656037ULL;
    std::cout << "layer,draws,milliseconds,pixel_digest\n";
    for (unsigned layer = 1; layer <= 4; ++layer) {
        ppu.bg2_vertical_offsets_enabled = layer == 4;
        constexpr unsigned draws = 200;
        const auto start = Clock::now();
        for (unsigned tick = 0; tick < draws; ++tick) {
            ppu.bg1_scroll_x = ppu.bg2_scroll_x = ppu.bg3_scroll_x = std::int16_t(tick);
            if (layer == 1) renderer.draw_bg1(ppu, frame);
            else if (layer == 3) renderer.draw_bg3(ppu, frame);
            else renderer.draw_bg2(ppu, ppu.bg2_scroll_x, ppu.bg2_scroll_y, frame);
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        add(hash, frame.pixels()); add(hash, frame.write_coverage()); add(hash, frame.layer_tags());
        std::cout << layer << ',' << draws << ',' << elapsed << ',' << std::hex << hash << std::dec << '\n';
    }
}
void large_recording_parity() {
    RowWorkers workers;workers.set_worker_count(8);
    BackgroundRenderer serial,parallel;parallel.row_workers=&workers;
    auto ppu=source(2);ppu.main_screen=2;ppu.background_mode=2;ppu.tunnel_scene=false;
    for(bool offsets:{false,true}) {
        ppu.bg2_vertical_offsets_enabled=offsets;
        Framebuffer a(400,224),b(400,224);
        RasterCommands left,right;left.reset(400,224);right.reset(400,224);
        a.record_to(&left);b.record_to(&right);
        serial.draw_bg2(ppu,0,0,a,TilePriorityPass::all,72);
        parallel.draw_bg2(ppu,0,0,b,TilePriorityPass::all,72);
        if(left.commands.size()<32768 || left.commands.size()!=right.commands.size()
            || left.texels!=right.texels
            || std::memcmp(left.commands.data(),right.commands.data(),left.commands.size()*sizeof(RasterCommand)))
            throw std::runtime_error("Large BG2 recording lost serial painter order");
    }
}
void parallel_benchmark(const char* rom_path=nullptr,const char* symbols_path=nullptr) {
    using Clock=std::chrono::steady_clock;
    RowWorkers workers;workers.set_worker_count(4);
    BackgroundRenderer serial,parallel;parallel.row_workers=&workers;
    auto ppu=source(2);
    if(rom_path && symbols_path) {
        const auto rom=starfox::assets::RomImage::load(rom_path);
        const auto symbols=starfox::assets::SymbolMap::load(symbols_path);
        auto game=std::make_unique<starfox::simulation::GameSimulation>(rom,symbols,"LEVEL1_1",
            std::span<const std::uint8_t>{},true);
        game->set_god_mode(true);
        for(unsigned tick=0;tick<1000;++tick) static_cast<void>(game->tick({}));
        ppu=game->map().ppu_state();
        std::cout<<"Actual Corneria BG2 after 1000 source ticks (background-only benchmark)\n";
    } else {
        ppu.main_screen=2;ppu.background_mode=2;ppu.mosaic=0;
        ppu.bg2_tile_size_16=false;ppu.tunnel_scene=false;
        ppu.bg2_scanline_scroll_enabled=false;ppu.bg2_horizontal_offsets_enabled=false;
    }
    for(unsigned width:{256U,400U}) for(bool offsets:{false,true}) {
        ppu.bg2_vertical_offsets_enabled=offsets;
        Framebuffer a(width,224),b(width,224);
        for(auto* f:{&a,&b}) {f->enable_layer_tags(true);f->begin_write_coverage();}
        const auto run=[&](const BackgroundRenderer& renderer,Framebuffer& frame) {
            const auto start=Clock::now();
            for(unsigned tick=0;tick<160;++tick)
                renderer.draw_bg2(ppu,int(tick),17,frame,TilePriorityPass::all,int(width)/2-128,
                    true,true,false,0,{},false,false,0,true);
            return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        };
        run(serial,a);run(parallel,b); // Warm caches and persistent workers.
        std::array<double,5> solo{},shared{};
        for(unsigned sample=0;sample<5;++sample) {
            if(sample&1) {shared[sample]=run(parallel,b);solo[sample]=run(serial,a);}
            else {solo[sample]=run(serial,a);shared[sample]=run(parallel,b);}
            if(a.pixels()!=b.pixels() || a.layer_tags()!=b.layer_tags()
                || !std::equal(a.write_coverage().begin(),a.write_coverage().end(),b.write_coverage().begin()))
                throw std::runtime_error("BG2 benchmark changed output");
        }
        std::sort(solo.begin(),solo.end());std::sort(shared.begin(),shared.end());
        std::cout<<"BG2 width="<<width<<" offsets="<<offsets<<" draws=160 serial-ms="<<solo[2]
            <<" four-worker-ms="<<shared[2]<<" speedup="<<solo[2]/shared[2]<<'\n';
    }
}
void source_parallel(const char* rom_path,const char* symbols_path) {
    const auto rom=starfox::assets::RomImage::load(rom_path);
    const auto symbols=starfox::assets::SymbolMap::load(symbols_path);
    const bool ex=!symbols.find("PLANETSEQ2_L").empty();
    RowWorkers workers;workers.set_worker_count(4);
    BackgroundRenderer serial,parallel;parallel.row_workers=&workers;
    unsigned samples=0;
    for(const char* level:ex?std::array{"LEVEL5_1","LEVEL6_2","LEVEL1_3"}
                            :std::array{"LEVEL1_1","LEVEL3_3","LEVEL1_3"}) {
        auto game=std::make_unique<starfox::simulation::GameSimulation>(rom,symbols,level,
            std::span<const std::uint8_t>{},true);
        game->set_god_mode(true);
        for(unsigned tick=0;tick<=1100;++tick) {
            static_cast<void>(game->tick({}));
            if(tick%100) continue;
            const auto& p=game->map().ppu_state();
            for(unsigned width:{256U,400U,853U}) for(unsigned scale:{1U,2U}) for(bool ground:{false,true}) {
                Framebuffer a(width,224,scale),b(width,224,scale);
                for(auto* f:{&a,&b}) {f->enable_layer_tags(true);f->clear(37);f->begin_write_coverage();}
                const auto draw_frame=[&](const BackgroundRenderer& renderer,Framebuffer& frame) {
                    ScopedLayer tag(frame,PixelLayer::background);
                    for(auto pass:{TilePriorityPass::low,TilePriorityPass::high})
                        renderer.draw_bg2(p,p.bg2_scroll_x,p.bg2_scroll_y,frame,pass,int(width)/2-128,
                            true,true,false,0,{},false,false,0,ground,true);
                };
                draw_frame(serial,a);draw_frame(parallel,b);
                if(a.pixels()!=b.pixels() || a.layer_tags()!=b.layer_tags()
                    || !std::equal(a.write_coverage().begin(),a.write_coverage().end(),b.write_coverage().begin()))
                    throw std::runtime_error(std::string("Source BG2 worker mismatch: ")+level+" tick="+std::to_string(tick));
                ++samples;
            }
        }
    }
    std::cout<<(ex?"EX":"Original")<<" live BG2 worker parity: "<<samples<<" native/wide/ultrawide, 1x/2x and ground-history samples\n";
}
void native_row_contract() {
    const auto require = [](bool value) {
        if (!value) throw std::runtime_error("native indexed row ownership/eligibility changed");
    };
    Framebuffer frame(19, 3);
    require(!frame.native_indexed_row(0));
    frame.enable_layer_tags(true);
    require(!frame.native_indexed_row(0));
    frame.clear(37); frame.begin_write_coverage();
    const ScopedLayer tag(frame, PixelLayer::background);
    const auto row = frame.native_indexed_row(1);
    require(bool(row) && row.pixels.size() == 19 && row.coverage.size() == 19 && row.tags.size() == 19);
    row.set(0, 0); row.set(18, 129); // Opaque zero and the final bounded pixel.
    for (unsigned y = 0; y < 3; ++y) for (unsigned x = 0; x < 19; ++x) {
        const bool written = y == 1 && (x == 0 || x == 18);
        require(frame.get(x, y) == (written ? (x == 0 ? 0 : 129) : 37));
        require(frame.write_coverage()[y * 19 + x] == unsigned(written));
        require(frame.layer_tags()[y * 19 + x] == unsigned(written ? PixelLayer::background : PixelLayer::three_d));
    }
    require(!frame.native_indexed_row(3));
    frame.enable_dither_pairs(true); require(!frame.native_indexed_row(1));
    frame.enable_dither_pairs(false);
    RasterCommands commands; commands.reset(19, 3);
    frame.record_to(&commands); require(!frame.native_indexed_row(1));
    frame.record_to(nullptr); frame.set_draw_scale(2); require(!frame.native_indexed_row(0));
    frame.set_draw_scale(1); frame.end_write_coverage(); require(!frame.native_indexed_row(0));
    frame.begin_write_coverage(); frame.enable_layer_tags(false); require(!frame.native_indexed_row(0));
    Framebuffer empty(0, 3); empty.enable_layer_tags(true); empty.begin_write_coverage();
    require(!empty.native_indexed_row(1));
}
} // namespace

namespace {
void reference_priority_split() {
    BackgroundRenderer renderer;
    for(unsigned n=0;n<64;++n) {
        auto ppu=source(n);ppu.background_mode=1;
        for(bool bg2:{false,true}) for(bool tracked:{false,true}) {
            Framebuffer low(256,224),high(256,224),expected_low(256,224),expected_high(256,224);
            for(auto* frame:{&low,&high,&expected_low,&expected_high}) {
                frame->clear(199);
                if(tracked) {frame->enable_layer_tags(true);frame->begin_write_coverage();}
            }
            if(bg2) {
                renderer.draw_bg2(ppu,ppu.bg2_scroll_x,ppu.bg2_scroll_y,expected_low,TilePriorityPass::low);
                renderer.draw_bg2(ppu,ppu.bg2_scroll_x,ppu.bg2_scroll_y,expected_high,TilePriorityPass::high);
                renderer.draw_bg2_mode1_split(ppu,ppu.bg2_scroll_x,ppu.bg2_scroll_y,low,high);
            } else {
                renderer.draw_bg3(ppu,expected_low,TilePriorityPass::low);
                renderer.draw_bg3(ppu,expected_high,TilePriorityPass::high);
                renderer.draw_bg3_split(ppu,low,high);
            }
            if(low.pixels()!=expected_low.pixels() || high.pixels()!=expected_high.pixels())
                throw std::runtime_error("Reference priority split differs from exact source planes");
            if(tracked && (!std::ranges::equal(low.layer_tags(),expected_low.layer_tags())
                || !std::ranges::equal(high.layer_tags(),expected_high.layer_tags())
                || !std::ranges::equal(low.write_coverage(),expected_low.write_coverage())
                || !std::ranges::equal(high.write_coverage(),expected_high.write_coverage())))
                throw std::runtime_error("Reference priority split loses layer/coverage metadata");
        }
    }
}
}
namespace {
void reference_ex_direct_overlay() {
    const BackgroundRenderer renderer;
    // The removed path is the oracle: decode into a cleared source image,
    // then copy only nonzero ink whose CGRAM RGB is not black.
    for (unsigned n = 0; n < 192; ++n) {
        auto ppu = source(n);
        if (n % 17U == 0) ppu.background_mode = 0;
        for (unsigned ink = 0; ink < ppu.cgram.size(); ++ink) {
            if (ink % 7U == 0) ppu.cgram[ink] = 0;
            else if (ink % 11U == 0) ppu.cgram[ink] = 0x8000U;
            else if (ink % 13U == 0) ppu.cgram[ink] |= 0x8000U;
        }
        for (bool tracked : {false, true}) {
            Framebuffer staged(256, 224), expected(256, 224), direct(256, 224);
            for (auto* frame : {&expected, &direct}) {
                frame->enable_layer_tags(tracked);
                frame->clear(199);
                for (std::size_t i = 0; i < frame->pixels().size(); ++i)
                    frame->pixels()[i] = std::uint8_t(i * 19U + n);
                if (tracked) frame->begin_write_coverage();
            }
            renderer.draw_bg1(ppu, staged, TilePriorityPass::all, 0, false, 16U);
            for (unsigned y = 0; y < 224; ++y) for (unsigned x = 0; x < 256; ++x) {
                const auto ink = staged.get(x, y);
                if (ink != 0 && (ppu.cgram[ink] & 0x7fffU) != 0)
                    expected.set(x, y, ink);
            }
            renderer.draw_bg1(ppu, direct, TilePriorityPass::all, 0, false, 16U, true);
            if (expected.pixels() != direct.pixels()
                || expected.layer_tags() != direct.layer_tags()
                || !std::equal(expected.write_coverage().begin(),
                    expected.write_coverage().end(), direct.write_coverage().begin()))
                throw std::runtime_error("Direct EX BG1 overlay changed pixels, coverage or tags");
        }
    }
}
void bg1_cached_rows() {
    const BackgroundRenderer renderer;
    for (unsigned n = 0; n < 48; ++n) for (unsigned scale : {1U, 2U})
        for (bool tagged : {false, true}) {
            auto ppu = source(n);
            ppu.main_screen = 1;
            ppu.background_mode = n & 1U ? 3U : 2U;
            ppu.mosaic = 0;
            // Repeated vertical rows and adversarial tile-key patterns,
            // including palette/flip variants and direct-map collisions.
            // The next invocation changes VRAM/CGRAM but reuses tile IDs.
            if (n % 3U == 0) {
                ppu.bg1_screen_base = 0x6000;
                for (unsigned entry = 0; entry < 4096; ++entry)
                    word(ppu, 0x6000U + entry, std::uint16_t((entry % 31U) * 33U
                        | ((entry % 8U) << 10U) | ((entry % 4U) << 14U)));
            }
            Framebuffer actual(400, 224, scale), expected(400, 224, scale);
            for (auto* frame : {&actual, &expected}) {
                frame->enable_layer_tags(tagged);
                frame->enable_dither_pairs(true);
                frame->clear(199); frame->begin_write_coverage();
            }
            const auto priority = static_cast<TilePriorityPass>(n % 3U);
            renderer.draw_bg1(ppu, actual, priority, 72, n % 4U != 0, 16, n & 2U);
            // Mosaic size one preserves coordinates but selects the original
            // independent per-pixel/bitplane decoder instead of cached rows.
            ppu.mosaic = 1;
            renderer.draw_bg1(ppu, expected, priority, 72, n % 4U != 0, 16, n & 2U);
            if (actual.pixels() != expected.pixels()
                || actual.layer_tags() != expected.layer_tags()
                || !std::ranges::equal(actual.write_coverage(), expected.write_coverage())
                || !std::ranges::equal(actual.dither_pairs(), expected.dither_pairs()))
                throw std::runtime_error("Draw-local BG1 cache differs from original point decoder");
        }
}

void bg1_overlay_dependencies() {
    using starfox::platform::nintendo_3ds::same_bg1_overlay_source;
    const BackgroundRenderer renderer;
    unsigned hits = 0;
    for (unsigned n = 0; n < 32; ++n) {
        auto before = source(n);
        before.main_screen = 1;
        before.background_mode = n & 1U ? 3U : 2U;
        Framebuffer cached(256, 224);
        renderer.draw_bg1(before, cached, TilePriorityPass::all, 0, false, 16, true);
        // The native cache borrows a 400x240 background allocation. Its crop
        // must be identical to decoding directly into the 256x224 foreground.
        Framebuffer borrowed(400, 240);
        renderer.draw_bg1(before, borrowed, TilePriorityPass::all, 0, false, 16, true);
        starfox::platform::nintendo_3ds::IndexedOverlayRows rows;
        rows.prepare(borrowed);
        for (unsigned scale : {1U, 2U}) {
            Framebuffer actual(256, 224, scale), direct(256, 224, scale);
            for (auto* frame : {&actual, &direct}) {
                frame->enable_layer_tags(true); frame->enable_dither_pairs(true);
                frame->clear(199); frame->begin_write_coverage();
                for (std::size_t i = 0; i < frame->pixels().size(); ++i)
                    frame->set_dither_alternate(i, std::uint8_t(i));
            }
            rows.apply(borrowed, actual);
            renderer.draw_bg1(before, direct, TilePriorityPass::all, 0, false, 16, true);
            if (actual.pixels() != direct.pixels() || actual.layer_tags() != direct.layer_tags()
                || !std::ranges::equal(actual.write_coverage(), direct.write_coverage())
                || !std::ranges::equal(actual.dither_pairs(), direct.dither_pairs()))
                throw std::runtime_error("Borrowed native BG1 cache differs from direct source decode");
        }
        for (unsigned mutation = 0; mutation < 18; ++mutation) {
            auto after = before;
            switch (mutation) {
            case 0: break;
            case 1: ++after.bg2_scroll_x; break;
            case 2: after.main_screen ^= 6; break;
            case 3: after.mosaic ^= 6; break;
            case 4: after.cgram[0] ^= 31775; break;
            case 5:
                for (auto& colour : after.cgram)
                    if (colour & 32767U) colour = 0x8001;
                break;
            case 6: for (auto& colour : after.cgram) colour ^= 0x8000; break;
            case 7: ++after.bg2_character_base; break;
            case 8: ++after.bg1_character_base; break;
            case 9: ++after.bg1_scroll_x; break;
            case 10: after.bg1_tile_size_16 = !after.bg1_tile_size_16; break;
            case 11: after.vram[(unsigned(after.bg1_screen_base) * 2U) & 65535U] ^= 1; break;
            case 12: {
                const unsigned at = (unsigned(after.bg1_screen_base) * 2U) & 65535U;
                const unsigned tile = (after.vram[at]
                    | (unsigned(after.vram[(at + 1U) & 65535U]) << 8U)) & 1023U;
                const unsigned depth = after.background_mode == 3U ? 8U : 4U;
                after.vram[(unsigned(after.bg1_character_base) * 2U + tile * depth * 8U) & 65535U] ^= 128;
                break;
            }
            case 13: after.cgram[1] = (after.cgram[1] & 32767U) ? 0x8000 : 1; break;
            case 14: after.background_mode = after.background_mode == 2 ? 3 : 2; break;
            case 15: after.main_screen ^= 1; break;
            case 16: ++after.bg1_scroll_y; break;
            case 17: after.mosaic ^= 0x11; break;
            }
            const bool same = same_bg1_overlay_source(before, after);
            if (mutation < 8 && !same)
                throw std::runtime_error("BG1 overlay rejected an irrelevant register/RGB change");
            if (mutation >= 8 && mutation <= 16 && same)
                throw std::runtime_error("BG1 overlay failed to invalidate a source dependency");
            if (same) {
                ++hits;
                Framebuffer expected(256, 224);
                renderer.draw_bg1(after, expected, TilePriorityPass::all, 0, false, 16, true);
                if (cached.pixels() != expected.pixels())
                    throw std::runtime_error("BG1 dependency reuse changed exact indexed pixels");
            }
        }
        before.main_screen = 0;
        auto disabled = before;
        disabled.background_mode = 0;
        disabled.vram.fill(255); disabled.cgram.fill(31);
        if (!same_bg1_overlay_source(before, disabled))
            throw std::runtime_error("Two disabled BG1 overlays are not identically empty");
    }
    if (hits < 256) throw std::runtime_error("BG1 reuse fixture did not exercise source hits");
}

void indexed_overlay_rows() {
    using starfox::platform::nintendo_3ds::IndexedOverlayRows;
    Framebuffer source_frame(400, 240);
    for (unsigned y = 0; y < 240; ++y) for (unsigned x = 0; x < 400; ++x) {
        if (x >= 256 || y >= 224 || (y % 7 != 0 && (x % 13 == 0 || x == 255)))
            source_frame.set(x, y, std::uint8_t((x * 17U + y) % 255U + 1U));
    }
    const auto unchanged = source_frame.pixels();
    IndexedOverlayRows rows;
    rows.prepare(source_frame);
    for (unsigned scale : {1U, 2U}) for (bool tagged : {false, true})
        for (bool covered : {false, true}) for (bool annotated : {false, true})
        for (bool recorded : {false, true}) for (unsigned width : {219U, 320U}) {
            const unsigned height = width == 219 ? 171 : 240;
            Framebuffer actual(width, height, scale), expected(width, height, scale);
            RasterCommands actual_commands, expected_commands;
            actual_commands.reset(actual.stored_width(), actual.stored_height());
            expected_commands.reset(expected.stored_width(), expected.stored_height());
            for (auto* frame : {&actual, &expected}) {
                frame->enable_layer_tags(tagged); frame->enable_dither_pairs(annotated);
                frame->clear(199);
                if (covered) frame->begin_write_coverage();
                if (annotated) for (std::size_t i = 0; i < frame->pixels().size(); ++i)
                    frame->set_dither_alternate(i, std::uint8_t(i));
            }
            if (recorded) { actual.record_to(&actual_commands); expected.record_to(&expected_commands); }
            rows.apply(source_frame, actual);
            for (unsigned y = 0; y < 224; ++y) for (unsigned x = 0; x < 256; ++x) {
                const auto ink = source_frame.get(x, y);
                if (ink) expected.set(x, y, ink);
            }
            if (actual.pixels() != expected.pixels() || actual.layer_tags() != expected.layer_tags()
                || !std::ranges::equal(actual.write_coverage(), expected.write_coverage())
                || !std::ranges::equal(actual.dither_pairs(), expected.dither_pairs())
                || actual_commands.commands.size() != expected_commands.commands.size())
                throw std::runtime_error("Sparse BG1 transfer changed pixels or ownership");
            for (std::size_t i = 0; i < actual_commands.commands.size(); ++i) {
                const auto& a = actual_commands.commands[i]; const auto& b = expected_commands.commands[i];
                if (a.left != b.left || a.top != b.top || a.right != b.right || a.bottom != b.bottom
                    || a.even != b.even || a.odd != b.odd || a.tag != b.tag || a.dither != b.dither
                    || a.textured != b.textured)
                    throw std::runtime_error("Sparse BG1 transfer changed ordered point commands");
            }
        }
    if (source_frame.pixels() != unchanged)
        throw std::runtime_error("Sparse BG1 transfer mutated its immutable source");
    const auto rejects = [](auto&& operation) {
        try { operation(); } catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("Incomplete BG1 cache source was accepted");
    };
    IndexedOverlayRows empty;
    Framebuffer destination(256, 224), narrow(255, 224), short_frame(256, 223), scaled(256, 224, 2);
    rejects([&] { empty.apply(source_frame, destination); });
    rejects([&] { rows.prepare(narrow); });
    rejects([&] { rows.prepare(short_frame); });
    rejects([&] { rows.prepare(scaled); });
    source_frame.resize(256, 240);
    rejects([&] { rows.apply(source_frame, destination); });
}
} // namespace

int main(int argc, char** argv) try {
    if (argc == 2 && std::string_view(argv[1]) == "--benchmark") { benchmark(); return 0; }
    if (argc == 2 && std::string_view(argv[1]) == "--parallel-benchmark") { parallel_benchmark(); return 0; }
    if (argc == 4 && std::string_view(argv[1]) == "--source-benchmark") { parallel_benchmark(argv[2],argv[3]); return 0; }
    if (argc == 4 && std::string_view(argv[1]) == "--source-parallel") { source_parallel(argv[2],argv[3]); return 0; }
    native_row_contract();
    large_recording_parity();
    check_tunnel_margins([](const auto& p,unsigned width,unsigned scale,bool extend) {
        Framebuffer f(width,224,scale);
        BackgroundRenderer{}.draw_bg2(p,p.bg2_scroll_x,p.bg2_scroll_y,f,TilePriorityPass::all,int(width)/2-128,
            true,true,false,0,{},false,false,0,true,extend);
        return f.pixels();
    });
    {
        // A bright orbital/star texel immediately before an atlas wrap is not
        // a floor. The old wide continuation extruded it down to the HUD.
        SnesPpuState ppu;
        ppu.background_mode=2;ppu.main_screen=2;
        ppu.bg2_screen_base=0x4000;ppu.bg2_character_base=0x1000;
        ppu.bg2_vertical_offsets_enabled=true;
        for(unsigned x=0;x<32;++x) {
            word(ppu,0x2fa0+x,0x4000|100);
            word(ppu,0x4000+31*32+x,1);
        }
        for(unsigned y=0;y<8;++y) {
            ppu.vram[0x2000+32+y*2]=255;
            ppu.vram[0x2000+32+y*2+16]=255;
            ppu.vram[0x2000+32+y*2+17]=255; // Ink 13, vivid magenta.
        }
        ppu.cgram[13]=0x7c1f;
        for(unsigned width:{256U,400U,853U}) {
            Framebuffer floor(width,224),space(width,224);
            const auto origin=int(width)/2-128;
            BackgroundRenderer renderer;
            renderer.draw_bg2(ppu,0,100,floor,TilePriorityPass::all,origin);
            renderer.draw_bg2(ppu,0,100,space,TilePriorityPass::all,origin,
                true,true,false,0,{},false,false,0,false);
            for(unsigned x=0;x<width;++x) for(unsigned y=156;y<224;++y) {
                if(space.get(x,y)!=0) throw std::runtime_error("Starfield texel extruded into a ground block");
                if(width==256 && floor.get(x,y)!=space.get(x,y))
                    throw std::runtime_error("No-ground continuation changed native-width pixels");
            }
            if(width>256 && floor.get(width/2,200)!=13)
                throw std::runtime_error("Ground continuation control did not exercise the previous extrusion");
        }
        std::cout<<"Space background continuation: native/wide/ultrawide atlas-wrap blocks suppressed; real ground continuation retained\n";
    }
    {
        // Mode 2 offset-per-tile columns follow BG2's tiles as fetched, i.e.
        // the fine scroll of each scanline (HDMA), not one frame-wide phase.
        // Each tile row is one solid colour and the columns alternate by a
        // whole tile row, so every colour change must lie on a tile edge of
        // that line. A frame-wide phase split tiles: vertical seams.
        SnesPpuState ppu;
        ppu.background_mode=2;ppu.main_screen=2;
        ppu.bg2_screen_base=0x4000;ppu.bg2_character_base=0x1000;
        ppu.bg2_vertical_offsets_enabled=true;ppu.bg2_horizontal_offsets_enabled=true;
        for(unsigned ink=1;ink<=3;++ink) {
            for(unsigned y=0;y<8;++y) {
                const unsigned row=0x2000+ink*32+y*2;
                ppu.vram[row]=(ink&1U)?255:0;ppu.vram[row+1]=(ink&2U)?255:0;
            }
            ppu.cgram[ink]=std::uint16_t(ink*0x1234U&32767U);
        }
        for(unsigned ty=0;ty<32;++ty) for(unsigned tx=0;tx<32;++tx) word(ppu,0x4000+ty*32+tx,1+ty%3);
        for(unsigned x=0;x<32;++x) word(ppu,0x2fa0+x,std::uint16_t(0x4000|(x%2?8:0)));
        for(unsigned y=0;y<224;++y) ppu.bg2_horizontal_offsets[y]=std::int16_t(3+8*(y%5));
        Framebuffer frame(256,224);
        BackgroundRenderer{}.draw_bg2(ppu,0,0,frame,TilePriorityPass::all,0);
        for(unsigned y=0;y<224;++y) for(unsigned x=1;x<256;++x)
            if(frame.get(x,y)!=frame.get(x-1,y) && (x+3)%8!=0)
                throw std::runtime_error("Mode 2 offset-per-tile column split a BG2 tile (per-scanline fine scroll)");
        std::cout<<"Mode 2 offset-per-tile: columns follow each scanline's fine scroll\n";
    }
    reference_priority_split();
    reference_ex_direct_overlay();
    bg1_cached_rows();
    bg1_overlay_dependencies();
    indexed_overlay_rows();
    const auto digest = oracle_trace();
    RowWorkers workers;
    for(unsigned count:{1U,2U,4U,8U}) {
        workers.set_worker_count(count);
        if(oracle_trace(&workers)!=digest)
            throw std::runtime_error("Threaded BG2 changed pixels/tags/coverage/dither or ordered commands");
    }
    std::cout<<"BG2 worker parity: serial/1/2/4/8 workers match the independent source trace\n";
    // Filled from a separate executable built BEFORE changing the renderer:
    // background_renderer.cpp SHA256 450af058b47c38fd096b7416782f1b3ab
    // dcdd428b847388391a68958e7d587d0, GNU 13.2 / -O2 / c++20.
    // This is not a comparison between two wrappers of the optimized routine.
#ifndef STARFOX_BACKGROUND_LEGACY_ORACLE
    // Updated for Mode 2 offset-per-tile columns that follow each scanline's
    // fine scroll (the varied rasters with per-scanline BG2 HOFS changed).
    constexpr std::uint64_t expected = 0xc18cad0e9a7ec6c8ULL;
    if (digest != expected) throw std::runtime_error("background scanline pixels/coverage/tags/commands changed");
#endif
    std::cout << "Background source trace: 192 varied rasters + 12 ordered command streams, digest "
        << std::hex << digest << " PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
