#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "../platform/3ds/reference/frame_3ds.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace starfox;
using namespace platform::nintendo_3ds;
int main(int argc,char** argv) try {
    if(argc!=4) throw std::invalid_argument("Use ROM SYMBOLS MAP; host component test, not hardware FPS");
    std::uint64_t blocks=0;
    GameSession session(assets::RomImage::load(argv[1]),assets::SymbolMap::load(argv[2]),
        [&](auto){++blocks;},argv[3]);
    const auto preload_begin=std::chrono::steady_clock::now();
    if(!session.prepare_mono([](unsigned,unsigned){return true;})) throw std::runtime_error("Preload canceled");
    auto& renderer=session.mono_renderer();
    const auto preload_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-preload_begin).count();
    session.advance(0,0);
    std::uint64_t digest=14695981039346656037ULL;unsigned submitted=0,reused=0,world_frames=0,ink_frames=0,fallback_checks=0;
    const auto start=std::chrono::steady_clock::now();
    double draw_ms=0;
    for(unsigned frame=1;frame<=240;++frame) {
        session.advance((std::int64_t(frame)*1000000000+59)/60,
            frame>=120 && frame<180?input::ButtonMask(input::y|input::right):0);
        const auto source=session.presentation(0,true);
        if(!session.game().draw_order().empty()) ++world_frames;
        renderer.set_sprite_selection(source.sprites);
        const bool gpu_bg2=source.raster->ppu->background_mode==2;
        const auto before=frame%30==0?session.game().save_state():std::vector<std::uint8_t>{};
        const auto render_begin=std::chrono::steady_clock::now();
        const bool reuse=renderer.can_reuse_presentation(session.game(),gpu_bg2,source.interpolation_alpha);
        const auto& indexed=reuse?renderer.last_frame():renderer.draw(session.game(),gpu_bg2,source.interpolation_alpha);
        draw_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-render_begin).count();
        if(reuse)++reused;else++submitted;
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
        <<" final_bg="<<session.presentation(0,true).current->background_id
        <<" digest="<<std::hex<<digest<<'\n';
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
