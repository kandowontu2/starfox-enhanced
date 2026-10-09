#include "starfox/platform/nintendo_3ds/game_models.hpp"
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/platform/nintendo_3ds/game_state.hpp"
#include "starfox/assets/bps.hpp"
#include "starfox/compat/bit_cast.hpp"
#include "starfox/state/container.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <iostream>
#include <vector>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
void require(bool value,const char* message) {
    ++checks;if(!value) throw std::runtime_error(message);
}
std::int64_t timestamp(unsigned phase) {return (std::int64_t(phase)*1'000'000'000+59)/60;}
std::uint32_t ram(const assets::SymbolMap& symbols,const char* name) {
    for(auto address:symbols.find(name)) if((address>>16)==0x70) return address;
    return 0;
}
bool same_draws(std::span<const PicaDraw> a,std::span<const PicaDraw> b) {
    if(a.size()!=b.size()) return false;
    for(unsigned i=0;i<a.size();++i) {
        const auto& x=a[i];const auto& y=b[i];
        if(x.first!=y.first || x.count!=y.count || x.texture!=y.texture || x.model!=y.model
            || x.space!=y.space || x.depth_test!=y.depth_test || x.depth_write!=y.depth_write
            || x.alpha_blend!=y.alpha_blend || x.screen_dither!=y.screen_dither || x.dither_odd!=y.dither_odd
            || x.clip!=y.clip || x.source_layer!=y.source_layer || x.colour_op!=y.colour_op
            || x.projected_uv!=y.projected_uv) return false;
    }
    return true;
}
void source_pose(const GameSession& session,const GamePresentation& source) {
    const auto& native=session.game().map().native_model_draw();
    require(native.active && native.shape && source.raster->native_model.has_value(),
        "Active source model launch was not captured in the raster");
    const auto& viewer=*source.raster->native_model;
    const auto word=[&](const char* name) {
        const auto address=ram(session.symbols(),name);
        return address?session.game().map().peek_ram_word(address).value():std::uint16_t{};
    };
    const auto byte=[&](const char* name) {
        const auto address=ram(session.symbols(),name);
        return address?session.game().map().peek_ram_byte(address).value():std::uint8_t{};
    };
    require(viewer.shape==native.shape && viewer.colour_table==native.colour_table,
        "Viewer changed the authored shape or colour table");
    const auto& pose=viewer.pose;
    require(pose.x==native.x && pose.y==native.y && pose.z==starfox::bit_cast<std::int16_t>(word("M_BIGZ")),
        "Viewer did not snapshot the latest shoulder zoom/source position");
    require(pose.pitch==((native.rotation_x&255U)<<8) && pose.yaw==((native.rotation_y&255U)<<8)
        && pose.roll==((native.rotation_z&255U)<<8),"Viewer lost native byte-angle conversion");
    const auto trig=simulation::TrigTables::load(session.rom(),session.symbols());
    const auto matrix=simulation::rotation_matrix_q15(trig,
        starfox::bit_cast<std::int16_t>(std::uint16_t(pose.pitch)),
        starfox::bit_cast<std::int16_t>(std::uint16_t(pose.yaw)),
        starfox::bit_cast<std::int16_t>(std::uint16_t(pose.roll)));
    require(pose.use_rotation_matrix && pose.rotation_matrix==matrix && pose.scale==1,
        "Dedicated viewer applied normal object scaling or approximate rotations");
    require(pose.vanish_x==native.vanish_x && pose.vanish_y==native.vanish_y,
        "Viewer doubled the canonical bitmap guard or lost its panel origin");
    require(pose.animation_frame==native.animation_frame && pose.colour_frame==native.colour_frame,
        "Viewer lost source animation/colour frames");
    require(pose.wireframe_mode==byte("M_WIREMODE") && pose.wobble_mode==byte("M_WOBBLEMODE")
        && pose.wave_mode==(byte("M_WABBLEMODE")!=0) && pose.cel_mode==(byte("M_CELMODE")!=0)
        && pose.wave_offset==starfox::bit_cast<std::int16_t>(word("M_SINEOFFSET"))
        && pose.colour_warp==(word("M_COLORWARP")!=0),"Viewer lost authored EX scan-converter modes");
    const auto projected=ram(session.symbols(),"M_PROJPNTS");
    require(pose.projected_points_address==(projected?std::uint16_t(projected):0x0b9f),
        "Viewer lost variant-specific projected-point seed");
    render::RenderPose depth;
    render::apply_source_depth_tables(session.rom(),session.symbols().find("DEPTHTABLES").front(),
        word("M_DEPTHSTAB"),word("M_DEPTHTABLE"),0,depth);
    require(pose.has_depth_colour_tables==depth.has_depth_colour_tables
        && pose.depth_thresholds==depth.depth_thresholds && pose.depth_colour_tables==depth.depth_colour_tables,
        "Viewer replaced the source depth palette");
}
void exact_geometry(const GameSession& session,const GamePresentation& source,GameModels& models) {
    source_pose(session,source);
    const auto vm=session.game().save_state(),spc=session.audio().save_state();
    const auto actual=models.prepare(source);validate_pica_frame(actual,source.dashboard);
    const auto& viewer=*source.raster->native_model;
    assets::ShapeDecoder decoder(session.rom(),session.symbols());
    const auto shape=decoder.decode(viewer.shape,{},viewer.colour_table);
    render::RenderSettings settings;settings.colour_index_base=112;
    render::SoftwareRenderer renderer(settings);
    const auto primitives=renderer.prepare_primitives(shape,viewer.pose);
    auto words=source.raster->ppu->cgram;
    std::copy(source.current->model_palette.begin(),source.current->model_palette.end(),words.begin()+112);
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(words),source.raster->brightness);
    PicaShapes oracle;oracle.append(primitives,palette,{112,96},&source.plan,PicaShapeOrder::painter);
    const auto expected=oracle.frame(source.plan);
    require(std::ranges::equal(actual.vertices,expected.vertices) && same_draws(actual.draws,expected.draws),
        "Native adapter changed the dedicated shape/pose, material, painter order or projection");
    require(actual.textures.size()==expected.textures.size(),"Viewer changed source texture count");
    for(unsigned i=0;i<actual.textures.size();++i) {
        const auto& a=actual.textures[i];const auto& b=expected.textures[i];
        require(a.width==b.width && a.height==b.height && a.pitch==b.pitch && a.repeat==b.repeat
            && std::ranges::equal(a.pixels,b.pixels),"Viewer changed palette/texture pixels");
    }
    require(models.coverage().models==1 && models.coverage().shadows==0
        && models.coverage().primitives==primitives.primitives.size(),
        "Dedicated viewer fabricated an object, LOD or shadow");
    require(std::all_of(actual.draws.begin(),actual.draws.end(),[](const auto& draw) {
        return draw.space==PicaSpace::world && !draw.depth_test && !draw.depth_write && draw.source_layer==1;
    }),"Viewer lost finite stereo geometry/BG1 ownership or BSP painter order");
    require(session.game().save_state()==vm && session.audio().save_state()==spc,
        "Preparing either viewer eye changed cartridge or SPC state");
}
void restored_viewer_geometry(GameSession& source,GameSession& restored,
    GameModels& source_models,GameModels& restored_models) {
    require(source.game().save_state()==restored.game().save_state(),"Restored viewer changed VM state");
    require(source.audio().save_state()==restored.audio().save_state(),"Restored viewer changed SPC state");
    const auto a=source.presentation(1,true),b=restored.presentation(1,true);
    require(a.raster->native_model.has_value() && b.raster->native_model.has_value(),
        "Save/load lost the active dedicated native viewer");
    require(*a.raster->ppu==*b.raster->ppu && a.raster->brightness==b.raster->brightness,
        "Restored viewer lost source raster/palette/fade");
    const auto af=source_models.prepare(a),bf=restored_models.prepare(b);
    require(std::ranges::equal(af.vertices,bf.vertices) && same_draws(af.draws,bf.draws),
        "Restored viewer changed primitive/material stream");
    require(af.textures.size()==bf.textures.size(),"Restored viewer changed texture count");
    for(unsigned i=0;i<af.textures.size();++i) {
        const auto& x=af.textures[i];const auto& y=bf.textures[i];
        require(x.width==y.width && x.height==y.height && x.pitch==y.pitch && x.repeat==y.repeat
            && std::ranges::equal(x.pixels,y.pixels),"Restored viewer changed texture colours");
    }
}
void check_viewer_state(const assets::RomImage& rom,const assets::SymbolMap& symbols,unsigned checkpoint) {
    std::array<std::vector<std::int16_t>,2> pcm;
    unsigned sink_owner=0;
    // The restored owner deliberately inherits the platform sink. Select its
    // comparison destination outside the cartridge, never through VM writes.
    GameSession source(rom,symbols,[&](auto block) {
        auto& output=pcm[sink_owner];output.insert(output.end(),block.begin(),block.end());
    },"CONTINUE");
    GameModels source_models(rom,symbols);source.advance(0,0);
    for(unsigned phase=1;phase<=checkpoint;++phase) source.advance(timestamp(phase),0);
    require(source.game().flow_state()==simulation::GameFlowState::continue_choice
        && source.presentation(0,false).raster->native_model,"Source Continue viewer missing");
    const auto archive=source.save_state();
    const auto vm=source.game().save_state(),spc=source.audio().save_state();
    for(auto& output:pcm) output.clear();
    auto restored=source.restored_state(archive);
    require(pcm[0].empty() && pcm[1].empty(),"Preparing state replacement emitted unwanted PCM");
    require(source.save_state()==archive && source.game().save_state()==vm
        && source.audio().save_state()==spc,"Preparing restored owner mutated live viewer");
    require(restored->save_state()==archive,"Viewer state archive did not round-trip exactly");
    auto restored_models=std::make_unique<GameModels>(restored->rom(),restored->symbols());
    restored_viewer_geometry(source,*restored,source_models,*restored_models);
    // Loading intentionally rebases host time. Use the public focus API to
    // rebase the comparison owner too, preserving VM/SPC and partial APU phase.
    source.advance(timestamp(checkpoint)+1,0,false);
    const auto origin=timestamp(checkpoint)+1'000'000'000;
    source.advance(origin,0,true);
    require(source.save_state()==archive,"Reference focus rebase mutated saved viewer state");
    restored->advance(0,0);
    for(unsigned step=1;step<=60;++step) {
        for(auto& output:pcm) output.clear();
        const auto held=step<=8?input::right_shoulder:input::left;
        sink_owner=0;source.advance(origin+timestamp(step),held);
        sink_owner=1;restored->advance(timestamp(step),held);
        require(pcm[0]==pcm[1],"Restored viewer changed consecutive PCM bytes or block cadence");
        restored_viewer_geometry(source,*restored,source_models,*restored_models);
    }
    const auto saved=restored->save_state();
    const auto prepared=restored_models->prepare(restored->presentation(0,false));
    const std::vector<PicaVertex> vertices(prepared.vertices.begin(),prepared.vertices.end());
    const std::vector<PicaDraw> draws(prepared.draws.begin(),prepared.draws.end());
    // Host Home/sleep clock contract, not physical APT service acceptance.
    restored->advance(timestamp(61),input::right_shoulder,false);
    restored->advance(60'000'000'000LL,input::right_shoulder,false);
    require(restored->save_state()==saved,"Suspension advanced viewer VM or SPC");
    const auto after=restored_models->prepare(restored->presentation(1,true));
    require(std::ranges::equal(after.vertices,vertices) && same_draws(after.draws,draws),
        "Suspension replaced completed native viewer geometry");
    restored->advance(61'000'000'000LL,0,true);
    require(restored->save_state()==saved,"Focus regain caught up suspended viewer or SPC");
    // Failed outer checks and late VM/SPC component decode must not retire the
    // active viewer, change its published snapshot, or send boot/preroll PCM.
    const auto retained_raster=restored->presentation(1,true).raster;
    const auto crc=assets::crc32(restored->rom().bytes());
    for(unsigned fault=0;fault<4;++fault) {
        for(auto& output:pcm) output.clear();
        auto damaged=saved;
        if(fault==0) damaged.back()^=1;
        else if(fault==1) damaged.pop_back();
        else {
            auto fields=decode_game_state(saved,crc);
            if(fault==2) fields.game=state::pack(0x47414d01U,crc,{});
            else fields.audio=state::pack(0x53504301U,0,{});
            damaged=encode_game_state(fields,crc);
        }
        bool refused=false;
        try {static_cast<void>(restored->restored_state(damaged));}
        catch(const std::exception&) {refused=true;}
        require(refused,"Damaged Continue state was accepted");
        require(restored->save_state()==saved && restored->presentation(1,true).raster==retained_raster,
            "Refused Continue load changed the live owner or raster");
        require(pcm[0].empty() && pcm[1].empty(),"Refused Continue load emitted PCM");
        restored_viewer_geometry(source,*restored,source_models,*restored_models);
    }
    auto roundtrip=restored->restored_state(saved);
    auto roundtrip_models=std::make_unique<GameModels>(roundtrip->rom(),roundtrip->symbols());
    restored_viewer_geometry(*restored,*roundtrip,*restored_models,*roundtrip_models);
    // Follow the native handoff: retire the old decoder/views before the old
    // ROM/symbol/VM owner, then continue the replacement, not both owners alive.
    restored_models.reset();
    restored=std::move(roundtrip);
    restored_models=std::move(roundtrip_models);
    const auto continued_origin=origin+10'000'000'000;
    source.advance(origin+timestamp(61),0,false);
    source.advance(continued_origin,0,true);
    restored->advance(0,0);
    require(source.save_state()==saved && restored->save_state()==saved,
        "Retired-owner continuation changed the saved source before input");
    for(unsigned step=1;step<=24;++step) {
        for(auto& output:pcm) output.clear();
        const auto held=step<=8?input::left_shoulder:input::right;
        sink_owner=0;source.advance(continued_origin+timestamp(step),held);
        sink_owner=1;restored->advance(timestamp(step),held);
        require(pcm[0]==pcm[1],"Retired viewer owner changed PCM cadence or samples");
        restored_viewer_geometry(source,*restored,source_models,*restored_models);
    }
}
void check_continue(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    GameSession session(rom,symbols,[](auto){},"CONTINUE");
    GameModels models(session.rom(),session.symbols());
    session.advance(0,0);
    for(unsigned phase=1;phase<=120;++phase)
        session.advance(timestamp(phase),0);
    require(session.game().flow_state()==simulation::GameFlowState::continue_choice,
        "CONTINUE did not enter the actual source model viewer");
    const auto native=session.game().map().native_model_draw();
    require(native.active && native.shape,"Source viewer did not launch MSHOWOBJ3");
    const auto vm=session.game().save_state(),spc=session.audio().save_state();
    const auto source=session.presentation(1,true);
    const auto frame=models.prepare(source);
    validate_pica_frame(frame,source.dashboard);
    require(models.coverage().models>0 && !frame.vertices.empty() && !frame.draws.empty(),
        "Active Continue viewer model is absent from the native PICA stream");
    require(session.game().save_state()==vm && session.audio().save_state()==spc,
        "Preparing the viewer changed cartridge or SPC state");
    const auto frozen=source;
    const std::vector<PicaVertex> frozen_vertices(frame.vertices.begin(),frame.vertices.end());
    const std::vector<PicaDraw> frozen_draws(frame.draws.begin(),frame.draws.end());
    const auto first_zoom=frozen.raster->native_model->pose.z;
    bool rotated=false,zoomed=false;
    for(unsigned phase=121;phase<=180;++phase) {
        session.advance(timestamp(phase),phase<=140?input::right_shoulder:input::left);
        const auto fresh=session.presentation(phase%2?0:1,phase%3!=0);
        exact_geometry(session,fresh,models);
        zoomed|=fresh.raster->native_model->pose.z!=first_zoom;
        rotated|=fresh.raster->native_model->pose.rotation_matrix!=frozen.raster->native_model->pose.rotation_matrix;
    }
    require(zoomed && rotated,"Ordinary source buttons did not exercise viewer zoom and rotation");
    const auto retained=models.prepare(frozen);
    require(std::ranges::equal(retained.vertices,frozen_vertices) && same_draws(retained.draws,frozen_draws),
        "Retained viewer snapshot read newer mutable VM registers");
    auto broken=frozen;
    auto bad=std::make_shared<GameRasterSnapshot>(*frozen.raster);
    bad->native_model->pose.vanish_x=std::numeric_limits<double>::quiet_NaN();broken.raster=bad;
    bool rejected=false;try {static_cast<void>(models.prepare(broken));}catch(const std::exception&){rejected=true;}
    require(rejected && std::ranges::equal(retained.vertices,frozen_vertices) && same_draws(retained.draws,frozen_draws),
        "Invalid dedicated viewer partly replaced the published geometry");
    auto inactive=frozen;auto no_viewer=std::make_shared<GameRasterSnapshot>(*frozen.raster);
    no_viewer->native_model.reset();inactive.raster=no_viewer;
    require(models.prepare(inactive).vertices.empty() && models.coverage().models==0,
        "Inactive native viewer retained old geometry");
    auto other_flow=std::make_shared<vr::GameSceneSnapshot>(*frozen.current);
    other_flow->flow=simulation::GameFlowState::intro;inactive.current=inactive.previous=other_flow;inactive.raster=frozen.raster;
    require(models.prepare(inactive).vertices.empty(),"Dedicated viewer leaked into another source flow");
    // Source NO choice/fade/title: no register injection or direct state change.
    for(unsigned phase=181;phase<=480;++phase) session.advance(timestamp(phase),
        phase==182?input::down:phase==184?input::b:0);
    require(session.game().flow_state()!=simulation::GameFlowState::continue_choice
        && !session.presentation(0,false).raster->native_model,"Leaving Continue did not retire the viewer snapshot");
    std::cout<<"Continue: 60 actual zoom/rotation frames, varying slider/hardware requests (front-end policy remains mono), exact stream/material, immutable/transactional state and source NO cleanup\n";
}
void check_ex_viewer(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    if(symbols.find("SPECWEPCNTONE").empty()) return;
    GameSession session(rom,symbols,[](auto){},"TITLEMAP");
    GameModels models(session.rom(),session.symbols());session.advance(0,0);
    bool entered=false;unsigned phase=1;
    // Documented title L+Select shortcut into the native EX model-test page.
    // No menu-register writes, archive injection or direct flow transitions.
    for(;phase<=2400;++phase) {
        session.advance(timestamp(phase),phase>=60?input::left_shoulder|input::select:0);
        const auto source=session.presentation(1,true);
        if(source.current->flow==simulation::GameFlowState::ex_pregame_menu
            && source.raster->native_model && source.raster->brightness==15) {entered=true;break;}
    }
    require(entered,"Documented EX title shortcut did not reach the bright native model viewer");
    session.advance(timestamp(++phase),0);
    const auto first=session.presentation(1,true).raster->native_model->pose.z;
    bool zoomed=false;
    for(unsigned sample=0;sample<30;++sample) {
        session.advance(timestamp(++phase),sample<8?input::right_shoulder:input::left);
        const auto source=session.presentation(sample%2?0:1,true);
        exact_geometry(session,source,models);zoomed|=source.raster->native_model->pose.z!=first;
    }
    require(zoomed,"EX native viewer did not preserve live post-launch M_BIGZ shoulder zoom");
    std::cout<<"EX: documented L+Select title shortcut, 30 source viewer frames and live shoulder zoom\n";
}
}
int main(int argc,char** argv) try {
    if(argc!=3) throw std::invalid_argument("Usage: check_3ds_native_model ROM SYMBOLS");
    const auto rom=assets::RomImage::load(argv[1]);const auto symbols=assets::SymbolMap::load(argv[2]);
    check_continue(rom,symbols);check_ex_viewer(rom,symbols);
    for(unsigned phase:{120U,121U,122U}) check_viewer_state(rom,symbols,phase);
    std::cout<<"Continue state: three partial-audio checkpoints, 60 replay plus 24 retired-owner frames each, four damaged-save refusals, exact VM/SPC/PCM/geometry/colours and focus rebase\n";
    std::cout<<checks<<" native viewer host checks passed; not ARM/PICA or hardware acceptance\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
