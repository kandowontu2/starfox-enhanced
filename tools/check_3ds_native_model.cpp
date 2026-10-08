#include "starfox/platform/nintendo_3ds/game_models.hpp"
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/compat/bit_cast.hpp"
#include <algorithm>
#include <limits>
#include <iostream>

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
    std::cout<<"Continue: 60 actual zoom/rotation frames, mono/stereo plans, exact stream/material, immutable/transactional state and source NO cleanup\n";
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
    std::cout<<checks<<" native viewer host checks passed; not ARM/PICA or hardware acceptance\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
