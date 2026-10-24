#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/platform/nintendo_3ds/bg1_overlay_dependencies.hpp"
#include "starfox/platform/nintendo_3ds/mode2_foreground_upload.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/platform/nintendo_3ds/game_assets.hpp"
#include "companion_manifest.hpp"
#include "../platform/3ds/reference/frame_3ds.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <tuple>
using namespace starfox;
using namespace platform::nintendo_3ds;
namespace starfox::simulation {
struct ObjectBridgeTestAccess {
    static auto image_hits(const MapVm& map) {return map.object_bridge_->image_hits;}
    static auto transfer(MapVm& map, std::uint32_t address,
        Wdc65816Registers& registers) {
        return map.call_native_routine_reusing_import(address, registers, 5'000'000, true);
    }
};
}
int main(int argc,char** argv) try {
    const bool companion=argc>=5 && std::string_view{argv[1]}=="--companion";
    const unsigned option=companion?5:4;
    const auto mode=unsigned(argc)>option?std::string_view{argv[option]}:std::string_view{};
    const bool bridge_parity=mode=="--bridge-parity";
    const bool foreground_parity=mode=="--foreground-parity";
    const bool scenery_parity=mode=="--scenery-parity";
    if(unsigned(argc)!=option && !(unsigned(argc)==option+1 && (bridge_parity || foreground_parity)))
        throw std::invalid_argument("Use ROM SYMBOLS MAP or --companion EXISTING_BIN original|ex MAP [--bridge-parity|--foreground-parity]; host component test, not hardware FPS");
    auto cartridge=[&]() -> GameCartridge {
        if(!companion) return {assets::RomImage::load(argv[1]),assets::SymbolMap::load(argv[2])};
        const std::string_view experience{argv[3]};
        if(experience!="original" && experience!="ex") throw std::invalid_argument("Expected original or ex");
        std::ifstream input(argv[2],std::ios::binary);
        return read_game_cartridge(input,companion_manifest,experience=="ex"?simulation::Experience::starfox_ex:simulation::Experience::original);
    }();
    std::uint64_t blocks=0;
    GameSessionOptions options;
    if(scenery_parity) {options.preferences=GamePreferences{};options.preferences->level=11;options.preferences->god=true;}
    GameSession session(std::move(cartridge.rom),std::move(cartridge.symbols),
        [&](auto){++blocks;},argv[companion?4:3]);
    const auto symbol = [&](const char* name, bool rom) {
        for (const auto address : session.symbols().find(name)) {
            const auto bank = address >> 16U;
            if (rom ? (address & 0xffffU) >= 0x8000U && bank < 0x7eU
                    : bank == 0U || bank == 0x70U || bank == 0x7eU || bank == 0x7fU)
                return address;
        }
        throw std::runtime_error(std::string{"Missing transfer symbol: "} + name);
    };
    const auto preload_begin=std::chrono::steady_clock::now();
    if(!session.prepare_mono([](unsigned,unsigned){return true;})) throw std::runtime_error("Preload canceled");
    auto& renderer=session.mono_renderer();
    const auto preload_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-preload_begin).count();
    session.advance(0,0);
    std::uint64_t digest=14695981039346656037ULL;unsigned submitted=0,reused=0,world_frames=0,ink_frames=0,fallback_checks=0;
    const auto start=std::chrono::steady_clock::now();
    double draw_ms=0;
    std::uint64_t native_us=0,overlay_us=0,sprites_us=0,canvas_us=0;
    unsigned bg1_mosaic_off=0,bg1_mosaic_one=0,bg1_mosaic_large=0;
    unsigned bg1_dependency_hits=0,bg1_dependency_misses=0;
    std::unique_ptr<simulation::SnesPpuState> previous_bg1;
    unsigned bridge_comparisons{};
    unsigned transfer_comparisons{};
    unsigned foreground_comparisons{};
    std::uint64_t reused_images{};
    std::uint64_t transfer_images{};
    for(unsigned frame=1;frame<=240;++frame) {
        session.advance((std::int64_t(frame)*1000000000+59)/60,
            frame>=120 && frame<180?input::ButtonMask(input::y|input::right):0);
        if(bridge_parity && frame%30==0) {
            auto fast_pool=session.game().objects(),full_pool=fast_pool;
            simulation::MapVm fast{session.rom(),simulation::MapDatabase{session.rom(),session.symbols()},fast_pool,&session.symbols()};
            simulation::MapVm full{session.rom(),simulation::MapDatabase{session.rom(),session.symbols()},full_pool,&session.symbols()};
            const auto state=session.game().map().save_state();fast.load_state(state);full.load_state(state);
            simulation::NativeStrategyScheduler fast_tick{session.symbols(),fast_pool,fast},full_tick{session.symbols(),full_pool,full};
            if(fast_tick.begin_tick()!=full_tick.begin_tick()) throw std::runtime_error("Strategy setup changed instruction count");
            const auto a=fast_tick.tick_all(),b=full_tick.tick_all(true);
            if(a.instructions!=b.instructions || a.objects_run!=b.objects_run || a.objects_removed!=b.objects_removed
                || fast_pool.save_state()!=full_pool.save_state() || fast.save_state()!=full.save_state())
                throw std::runtime_error("Consecutive strategy image differs from full synchronization");
            ++bridge_comparisons;reused_images+=simulation::ObjectBridgeTestAccess::image_hits(fast);
            // Run the actual post-DOSOUNDS palette/sprite/wipe sequence from
            // these cartridge states. Compare after EACH entry, not just the
            // final image: PPU/APU queues, open bus, returned registers, links
            // and generations must all match the defensive path.
            const auto transfer = [&](const char* name, bool reuse = true) {
                const auto address = symbol(name, true);
                simulation::Wdc65816Registers x, y;
                x.status = y.status = 0x24;
                const auto before_hits = simulation::ObjectBridgeTestAccess::image_hits(fast);
                const auto count = reuse
                    ? simulation::ObjectBridgeTestAccess::transfer(fast, address, x)
                    : fast.call_native_routine(address, x, 5'000'000, true);
                const auto reference = full.call_native_routine(address, y, 5'000'000, true);
                if (count != reference
                    || std::tie(x.a, x.x, x.y, x.direct, x.stack, x.data_bank, x.status)
                        != std::tie(y.a, y.x, y.y, y.direct, y.stack, y.data_bank, y.status)
                    || fast_pool.save_state() != full_pool.save_state()
                    || fast.save_state() != full.save_state())
                    throw std::runtime_error(std::string{"Transfer differs from full synchronization: "} + name);
                ++transfer_comparisons;
                transfer_images += simulation::ObjectBridgeTestAccess::image_hits(fast) - before_hits;
            };
            transfer("DOSOUNDS_L", false);
            const bool ex = session.game().experience() == simulation::Experience::starfox_ex;
            if (!ex || fast.read_native_byte(symbol("NOBGMODE", false)) == 0) {
                transfer("PALGOTO_L"); transfer("FADEPALTO_L");
                if (ex) {
                    for (const auto name : {"PALFADETOYAMAO_L", "PALFADETOYAMAB_L", "PALFADETOCORN2_L",
                            "PALFADERANDOM_L", "PALFADETOTITLE_L"}) transfer(name);
                    const auto cadence = [&](const char* name, unsigned threshold, const auto& routines) {
                        const auto address = symbol(name, false);
                        const auto value = fast.read_native_byte(address);
                        if (value != full.read_native_byte(address)) throw std::runtime_error("Transfer cadence differs");
                        const bool due = (static_cast<std::uint8_t>(value - threshold) & 0x80U) == 0;
                        const auto next = due ? 0 : static_cast<std::uint8_t>(value + 1);
                        fast.write_native_byte(address, next); full.write_native_byte(address, next);
                        if (due) for (const auto routine : routines) transfer(routine);
                    };
                    cadence("TEMPVAL5", 3, std::array{"PALFADETOCORNNITE_L", "PALFADETONEWSPACE1_L",
                        "PALFADETONEWSPACE2_L", "PALFADETONEWSPACE3_L", "PALFADETONEWSPACE4_L",
                        "PALFADETONEWSPACE5_L", "PALFADETOBLACK_L", "PALFADETOOOTD_L"});
                    cadence("TEMPVAL6", 10, std::array{"PALFADETOFXGREEN_L", "PALFADETOFXDES_L",
                        "PALFADETOFXPINK_L", "PALFADETOFXBLUE_L"});
                }
            }
            transfer("DO_SPRITES_L"); transfer("DO_WINDOW_WIPE_L");
        }
        const auto source=session.presentation(0,true);
        if (session.game().experience() == simulation::Experience::starfox_ex
            && source.raster->ppu->background_mode == 2U) {
            const auto& ppu = *source.raster->ppu;
            if (previous_bg1 && same_bg1_overlay_source(*previous_bg1, ppu))
                ++bg1_dependency_hits;
            else {
                ++bg1_dependency_misses;
                if (!previous_bg1) previous_bg1 = std::make_unique<simulation::SnesPpuState>();
                *previous_bg1 = ppu;
            }
        }
        const auto mosaic=source.raster->ppu->mosaic;
        if (!(mosaic & 1U)) ++bg1_mosaic_off;
        else if (!(mosaic >> 4U)) ++bg1_mosaic_one;
        else ++bg1_mosaic_large;
        if(!session.game().draw_order().empty()) ++world_frames;
        renderer.set_sprite_selection(source.sprites);
        const bool gpu_bg2=source.raster->ppu->background_mode==2
            && (session.game().flow_state()==simulation::GameFlowState::gameplay
                || session.game().flow_state()==simulation::GameFlowState::training);
        const auto before=frame%30==0?session.game().save_state():std::vector<std::uint8_t>{};
        const auto render_begin=std::chrono::steady_clock::now();
        const bool reuse=renderer.can_reuse_presentation(session.game(),gpu_bg2,source.interpolation_alpha);
        const auto& indexed=reuse?renderer.last_frame():renderer.draw(session.game(),gpu_bg2,source.interpolation_alpha);
        draw_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-render_begin).count();
        if(reuse)++reused;
        else {
            ++submitted;
            const auto timing=renderer.last_draw_breakdown();
            native_us+=timing.native_model_us;
            overlay_us+=timing.overlay_us;
            sprites_us+=timing.sprites_us;
            canvas_us+=timing.canvas_us;
        }
        if(!before.empty()) {
            const auto after=session.game().save_state();
            if(before!=after) {
                unsigned shown=0;
                for(std::size_t i=0;i<std::min(before.size(),after.size()) && shown<12;++i) if(before[i]!=after[i]) {
                    std::cerr<<"state byte "<<i<<": "<<unsigned(before[i])<<" -> "<<unsigned(after[i])<<'\n';++shown;
                }
                throw std::runtime_error("Reference presentation mutated source state");
            }
        }
        if(indexed.width()!=400 || indexed.height()!=240) throw std::runtime_error("Reference output is not native LCD size");
        if(gpu_bg2 && indexed.write_coverage().size()!=indexed.pixels().size()) throw std::runtime_error("Mode2 foreground lacks exact coverage");
        if(std::any_of(indexed.pixels().begin(),indexed.pixels().end(),[](auto p){return p!=0;})) ++ink_frames;
        for(auto byte:indexed.pixels()) {digest^=byte;digest*=1099511628211ULL;}
        if(foreground_parity && gpu_bg2 && !before.empty()) {
            const auto& ppu=session.game().map().ppu_state();
            const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(ppu.cgram),
                session.game().map().display_brightness());
            std::array<std::uint16_t,256> colours{};
            for(unsigned i=0;i<256;++i) colours[i]=std::uint16_t(((palette[i].r>>3U)<<11U)
                |((palette[i].g>>3U)<<6U)|((palette[i].b>>3U)<<1U)|1U);
            std::vector<std::uint16_t> expected(400U*240U),uploaded(512U*256U,0xcafeU);
            const auto coverage=indexed.write_coverage();
            for(unsigned i=0;i<expected.size();++i) expected[i]=coverage[i]?colours[indexed.pixels()[i]]:0U;
            if(renderer.can_reuse_presentation(session.game(),true,source.interpolation_alpha,true))
                throw std::runtime_error("Composed canvas incorrectly reused as deferred world");
            const auto& world=renderer.draw(session.game(),true,source.interpolation_alpha,true);
            if(!renderer.deferred_mode2_foreground()
                || &renderer.last_frame()!=&world
                || !pack_mode2_foreground(world,renderer.source_foreground(),colours,uploaded,512))
                throw std::runtime_error("Deferred native foreground is incomplete");
            for(unsigned y=0;y<240;++y) for(unsigned x=0;x<512;++x)
                if(uploaded[y*512+x]!=(x<400?expected[y*400+x]:0xcafeU))
                    throw std::runtime_error("Actual cartridge direct foreground differs from composed upload");
            if(!std::all_of(uploaded.begin()+512*240,uploaded.end(),[](auto p){return p==0xcafeU;})
                || renderer.can_reuse_presentation(session.game(),true,source.interpolation_alpha,false)
                || session.game().save_state()!=before)
                throw std::runtime_error("Deferred upload changed padding, cache ownership or source state");
            ++foreground_comparisons;
        }
        if(gpu_bg2 && !before.empty()) {
            const auto& complete=renderer.draw(session.game(),false,source.interpolation_alpha);
            if(complete.width()!=400 || complete.height()!=240 || session.game().save_state()!=before)
                throw std::runtime_error("Complete-frame fallback changed source state or LCD extent");
            ++fallback_checks;
        }
    }
    if(!world_frames || !ink_frames) throw std::runtime_error("Fixture did not render visible gameplay");
    std::cout<<"Reference mono source purity/coverage passed; host timing ONLY\npreload_ms="<<preload_ms
        <<" total_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()
        <<" draw_ms="<<draw_ms<<" rendered="<<submitted<<" reused="<<reused<<" audio_blocks="<<blocks
        <<" world_frames="<<world_frames<<" ink_frames="<<ink_frames<<" final_flow="<<unsigned(session.game().flow_state())
        <<" fallback_checks="<<fallback_checks
        <<" native_ms="<<native_us/1000.0<<" overlay_ms="<<overlay_us/1000.0
        <<" sprites_ms="<<sprites_us/1000.0<<" canvas_ms="<<canvas_us/1000.0
        <<" bg1_mosaic_off="<<bg1_mosaic_off<<" bg1_mosaic_one="<<bg1_mosaic_one
        <<" bg1_mosaic_large="<<bg1_mosaic_large
        <<" bg1_dependency_hits="<<bg1_dependency_hits
        <<" bg1_dependency_misses="<<bg1_dependency_misses
        <<" ex_overlay_cache_hits="<<renderer.ex_overlay_cache_hits()
        <<" ex_overlay_cache_misses="<<renderer.ex_overlay_cache_misses()
        <<" bridge_comparisons="<<bridge_comparisons<<" bridge_image_hits="<<reused_images
        <<" transfer_comparisons="<<transfer_comparisons<<" transfer_image_hits="<<transfer_images
        <<" foreground_comparisons="<<foreground_comparisons
        <<" final_bg="<<session.presentation(0,true).current->background_id
        <<" digest="<<std::hex<<digest<<'\n';
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
