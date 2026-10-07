#include "starfox/platform/nintendo_3ds/game_models.hpp"
#include "starfox/platform/nintendo_3ds/game_assets.hpp"
#include "starfox/platform/nintendo_3ds/game_layers.hpp"
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/platform/nintendo_3ds/game_menu.hpp"
#include "starfox/platform/nintendo_3ds/pica_raster.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include "starfox/platform/nintendo_3ds/game_dots.hpp"
#include "starfox/platform/nintendo_3ds/pica_window.hpp"
#include "starfox/platform/nintendo_3ds/pica_colour.hpp"
#include "starfox/platform/nintendo_3ds/game_effects.hpp"
#include "companion_manifest.hpp"
#include "fortuna_route_inputs.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
std::string context;
void require(bool value,const char* why) {++checks;if(!value) throw std::runtime_error(context+": "+why);}
bool same_draws(std::span<const PicaDraw> a,std::span<const PicaDraw> b) {
    if(a.size()!=b.size()) return false;
    for(unsigned i=0;i<a.size();++i) {
        const auto& x=a[i];const auto& y=b[i];
        if(x.first!=y.first || x.count!=y.count || x.texture!=y.texture || x.model!=y.model
            || x.space!=y.space || x.depth_test!=y.depth_test || x.depth_write!=y.depth_write
            || x.alpha_blend!=y.alpha_blend || x.screen_dither!=y.screen_dither || x.dither_odd!=y.dither_odd || x.clip!=y.clip
            || x.source_layer!=y.source_layer || x.colour_op!=y.colour_op || x.projected_uv!=y.projected_uv) return false;
    }
    return true;
}
std::array<unsigned,5> source_pixel(const PicaFrame& frame,unsigned x,unsigned y) {
    std::array<unsigned,5> result{};
    for(const auto& draw:frame.draws) {
        if(draw.texture==pica_no_texture) continue;
        const auto& image=frame.textures[draw.texture];
        if(draw.space==PicaSpace::world) continue;
        const auto& origin=frame.vertices[draw.first].position;
        const int ix=int(x)+72-int(origin[0]),iy=int(y)+8-int(origin[1]);
        if(ix<0 || iy<0 || ix>=int(image.width) || iy>=int(image.height)) continue;
        const auto at=std::size_t(iy)*image.pitch+unsigned(ix)*4;
        if(image.pixels[at+3]) result={image.pixels[at],image.pixels[at+1],image.pixels[at+2],255,
            image.source_layers.empty()?draw.source_layer:image.source_layers[std::size_t(iy)*image.layer_pitch+unsigned(ix)]};
    }
    return result;
}
std::vector<std::array<unsigned,5>> flat_painter_pixels(const PicaFrame& frame,bool receivers=false) {
    // Independent pixel-centre triangle/nearest-texture oracle. The previous
    // raster-only image-offset sampler cannot validate a compact tile atlas.
    // Include the entire mono LCD, not only its canonical 256x224 window.
    std::vector<std::array<unsigned,5>> result(top_width*screen_height);
    for(const auto& draw:frame.draws) {
        require(receivers || draw.space!=PicaSpace::world,"Flat source oracle received finite geometry");
        if(draw.texture==pica_no_texture) continue;
        const auto& image=frame.textures[draw.texture];
        for(unsigned i=draw.first;i<draw.first+draw.count;i+=3) {
            std::array<std::array<double,2>,3> p{};
            for(unsigned k=0;k<3;++k) {
                const auto& v=frame.vertices[i+k];
                p[k]=draw.space==PicaSpace::world?std::array<double,2>{
                    200+frame.plan.focal_x*v.position[0]/v.position[2],
                    120-frame.plan.focal_y*v.position[1]/v.position[2]}:
                    std::array<double,2>{v.position[0],v.position[1]};
            }
            const auto cross=[](auto a,auto b,auto s){return (b[0]-a[0])*(s[1]-a[1])-(b[1]-a[1])*(s[0]-a[0]);};
            const double area=cross(p[0],p[1],p[2]);if(std::abs(area)<1.e-12) continue;
            const int x0=std::max(0,int(std::floor(std::min({p[0][0],p[1][0],p[2][0]}))));
            const int x1=std::min(int(top_width),int(std::ceil(std::max({p[0][0],p[1][0],p[2][0]}))));
            const int y0=std::max(0,int(std::floor(std::min({p[0][1],p[1][1],p[2][1]}))));
            const int y1=std::min(int(screen_height),int(std::ceil(std::max({p[0][1],p[1][1],p[2][1]}))));
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                const std::array sample{double(x)+.5,double(y)+.5};
                const std::array weights{cross(p[1],p[2],sample)/area,cross(p[2],p[0],sample)/area,cross(p[0],p[1],sample)/area};
                if(std::any_of(weights.begin(),weights.end(),[](double w){return w< -1.e-5;})) continue;
                double u=0,v=0,reciprocal=0;
                for(unsigned k=0;k<3;++k) {
                    const auto& vertex=frame.vertices[i+k];
                    const double weight=draw.space==PicaSpace::world && !draw.projected_uv
                        ?weights[k]/vertex.position[2]:weights[k];
                    reciprocal+=weight;u+=weight*vertex.uv[0];v+=weight*vertex.uv[1];
                }
                const unsigned tx=unsigned(std::clamp(u/reciprocal*image.width,0.,double(image.width-1)));
                const unsigned ty=unsigned(std::clamp(v/reciprocal*image.height,0.,double(image.height-1)));
                const auto at=std::size_t(ty)*image.pitch+tx*4;
                if(image.pixels[at+3]) result[std::size_t(y)*top_width+unsigned(x)]={
                    image.pixels[at],image.pixels[at+1],image.pixels[at+2],image.pixels[at+3],
                    image.source_layers.empty()?draw.source_layer:image.source_layers[std::size_t(ty)*image.layer_pitch+tx]};
            }
        }
    }
    return result;
}
unsigned remaining_layer_vertices(std::initializer_list<PicaFrame> groups) {
    std::size_t occupied=0;for(const auto& group:groups) occupied+=group.vertices.size();
    require(occupied<=pica_vertex_limit,"Non-layer source geometry exceeded whole-scene vertex limit");
    return pica_vertex_limit-unsigned(occupied);
}
void native_menu_composition(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    // A separate bounded integration diagnostic, not a relabeling of the
    // model-stream route. Match the test player's actual owner/order graph,
    // including its real host menu and lower HUD allocation. No PICA device
    // or native audio output is simulated here.
    GameSessionOptions options;options.preferences=GamePreferences{};
    options.preferences->separation=64;options.preferences->convergence=16;
    {
        context="Native composition / actual preview-OFF setup";
        GameSession setup(rom,symbols,[](auto){},"BOOT",{},options);
        GameMenu menu(setup.rom(),setup.symbols());
        setup.advance(0,0);menu.update(GameMenu::capture(setup.game()));
        require(menu.state().visible && !menu.state().preview,"BOOT did not produce the real preview-OFF setup");
        const auto state=setup.save_state(),apu=setup.audio().save_state();
        for(const float slider:{0.F,.5F,1.F,0.F}) {
            const auto source=setup.presentation(slider,true,setup.stereo_settings());
            const auto frame=menu.frame(source.plan);validate_pica_frame(frame,source.dashboard);
            require(frame.vertices.size()==6 && frame.draws.size()==1 && frame.textures.size()==1
                && frame.draws[0].space==PicaSpace::screen && !frame.draws[0].depth_test,
                "Preview-OFF native setup prepared world geometry or lost its menu");
            require(pica_resident_texture_bytes(frame.textures[0])+512U*256U*4U<=pica_texture_budget,
                "Preview-OFF native menu and lower LCD exceeded residency");
        }
        require(setup.save_state()==state && setup.audio().save_state()==apu,
            "Plain menu/sliders advanced the source or SPC");
    }
    context="Native composition / actual preview preroll";
    options.preview=true;
    GameSession preview(rom,symbols,[](auto){},"LEVEL1_1",{},options);
    GameMenu menu(preview.rom(),preview.symbols());GameModels models(preview.rom(),preview.symbols());
    GameDots dots(preview.rom(),preview.symbols());GameLayers layers;
    PicaColourEffects colour;PicaWindow window;PicaComposite composite;
    unsigned peak_bytes=0,peak_vertices=0,peak_draws=0,peak_textures=0,compositions=0;
    preview.advance(0,0);
    for(unsigned phase=1;phase<=24;++phase) {
        context="Native composition / actual preview phase "+std::to_string(phase);
        const auto advance=preview.advance((std::int64_t(phase)*1'000'000'000+59)/60,0);
        require(!advance.requested_experience && !advance.requested_preview && !advance.requested_settings_reset,
            "Preview changed native source owner unexpectedly");
        menu.update(GameMenu::capture(preview.game()));
        require(menu.state().visible && menu.state().preview,"Real native preview lost its host overlay");
        const auto state=preview.save_state(),apu=preview.audio().save_state();
        std::vector<std::uint8_t> menu_pixels;
        // Persistent owners traverse mono, stereo and mono again. Old-model
        // hardware policy remains a separate mono observation, not a claim
        // that this host check measures its speed or actual slider service.
        for(const auto& optics:std::array<std::pair<float,bool>,5>{{{0,true},{.5F,true},{1,true},{0,true},{1,false}}}) {
            const auto source=preview.presentation(optics.first,optics.second,preview.stereo_settings());
            require(source.plan.eye_count==(optics.second && optics.first>0?2U:1U)
                && source.plan.separation<=64 && source.plan.convergence==16,
                "Preview composition departed from supported native optics/old-model mono policy");
            const auto geometry=models.prepare(source),ink=dots.prepare(source);
            const auto policy=game_effect_plan(source);
            const auto math=colour.prepare(source.raster->circle,source.raster->colour_math,source.raster->brightness,source.plan,policy.circle_clip);
            const auto mask=window.prepare(source.raster->wipe,source.plan,policy.window);
            const PicaFrame label{source.plan,{},{},{}}; // Same as TEST_PLAYER.
            const auto overlay=menu.frame(source.plan);
            const auto artwork=layers.prepare(source,remaining_layer_vertices({geometry,ink,math,mask,label,overlay}));
            const auto frame=composite.prepare(source.plan,
                std::array{artwork.before_models,ink,geometry,artwork.after_models,math,mask,label,overlay},
                source.dashboard,artwork.clear);
            validate_pica_frame(frame,source.dashboard);
            const auto offset=artwork.before_models.vertices.size()+ink.vertices.size();
            require(std::equal(geometry.vertices.begin(),geometry.vertices.end(),frame.vertices.begin()+offset),
                "Complete native graph changed/reprojected model geometry");
            const auto& ui=frame.draws.back();
            require(ui.space==PicaSpace::screen && !ui.depth_test && !ui.depth_write && ui.alpha_blend && ui.source_layer==0,
                "Host preview overlay entered cartridge depth/colour/wipe ownership");
            const auto pixels=overlay.textures[0].pixels;
            if(menu_pixels.empty()) menu_pixels.assign(pixels.begin(),pixels.end());
            require(pixels.size()==menu_pixels.size() && std::equal(pixels.begin(),pixels.end(),menu_pixels.begin()),
                "Native slider or old-model policy changed host menu pixels");
            for(unsigned eye=0;eye<source.plan.eye_count;++eye)
                require(pica_draw_matrix(source.plan,eye,ui)==pica_screen_matrix(top_width),
                    "Host preview menu acquired stereo disparity");
            unsigned bytes=512U*256U*4U;
            for(const auto image:frame.textures) bytes+=pica_resident_texture_bytes(image);
            require(bytes<=pica_texture_budget,"Actual complete native preview exceeded padded residency including host UI/lower LCD");
            peak_bytes=std::max(peak_bytes,bytes);peak_vertices=std::max(peak_vertices,unsigned(frame.vertices.size()));
            peak_draws=std::max(peak_draws,unsigned(frame.draws.size()));peak_textures=std::max(peak_textures,unsigned(frame.textures.size()));
            ++compositions;
        }
        require(preview.save_state()==state && preview.audio().save_state()==apu,
            "Native composition/sliders changed source/SPC or advanced either eye");
    }
    require(compositions==120 && peak_vertices>6 && peak_textures>1,"Complete native preview missed real world/UI coverage");
    std::cout<<"Actual native menu/compositor graph: "<<compositions<<" preview compositions, peak padded textures "
        <<peak_bytes<<" bytes including host UI/lower LCD, peak vertices/draws/textures "
        <<peak_vertices<<'/'<<peak_draws<<'/'<<peak_textures
        <<"; real preview-OFF setup, source-state/SPC parity and supported slider/old-model mono policies passed; "
        "HOST owner/resource acceptance only, NOT ARM/PICA pixels, complete process RAM, audio output or device FPS\n";
}
void native_effect_flow(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    // Actual source transitions and PCM service at 60 Hz. Controls and Training
    // are reached using normal Start/buttons, never by changing VM flow/RAM.
    GameSession session(rom,symbols,[](auto){},"TITLEMAP");
    GameModels models(session.rom(),session.symbols()),player_reference(session.rom(),session.symbols());
    GameDots dots(session.rom(),session.symbols());
    GameLayers layers;GameEffects effects;PicaComposite composite;
    std::array<unsigned,15> phases{};unsigned flow_age=0,circles=0,compositions=0,peak_bytes=0,player_phases=0,weapons_phases=0;
    auto previous=session.game().flow_state();session.advance(0,0);
    for(unsigned phase=1;phase<=2400;++phase) {
        const auto flow=session.game().flow_state();
        if(flow!=previous) {flow_age=0;previous=flow;}++flow_age;
        input::ButtonMask held=0;
        using enum simulation::GameFlowState;
        if((flow==title || flow==ex_pregame_menu) && flow_age%90==0) held=input::start;
        else if(flow==controls_type) {
            if(flow_age>=360 && flow_age%90==0) held=input::start;
            // Control A's source diagram assigns Nova Bomb to A (X is brake).
            // Hold through several original-speed FX updates, then release.
            else if(flow_age%90>=15 && flow_age%90<33) held=input::a;
            else if(flow_age%90>=50 && flow_age%90<70) held=input::b; // Control A's blaster.
        } else if(flow==controls_choice && flow_age>=90 && flow_age%90==0) held=input::start;
        session.advance(std::int64_t(phase)*1'000'000'000/60,held);
        const auto source=session.presentation(std::array{0.F,.5F,1.F}[(phase/30)%3],true);
        context="Actual native effect flow phase="+std::to_string(phase)+" flow="+std::to_string(unsigned(source.current->flow));
        ++phases[unsigned(source.current->flow)];
        const auto policy=game_effect_plan(source);
        const auto prepared=effects.prepare(source);
        const auto geometry=models.prepare(source),ink=dots.prepare(source);
        const auto artwork=layers.prepare(source,remaining_layer_vertices({geometry,ink,prepared.colour,prepared.window}));
        const auto complete=composite.prepare(source.plan,
            std::array{artwork.before_models,ink,geometry,artwork.after_models,prepared.colour,prepared.window},
            source.dashboard,artwork.clear);
        validate_pica_frame(complete,source.dashboard);++compositions;
        unsigned bytes=512U*256U*4U;for(const auto image:complete.textures) bytes+=pica_resident_texture_bytes(image);
        peak_bytes=std::max(peak_bytes,bytes);require(bytes<=pica_texture_budget,"Actual flow exceeds whole-scene padded texture residency");
        require(source.raster->final_score==session.game().final_score_active()
            && source.raster->boss_roll==session.game().boss_roll_active(),"Effect extension flags are stale compared with current source raster");
        if(source.current->flow==controls_type || source.current->flow==controls_choice) {
            require(policy.circle_clip==PicaClip{96,32,208,120},"Actual Controls source did not use its flight-panel effect clip");
            for(const auto& draw:geometry.draws)
                require(draw.clip && draw.clip->left>=96 && draw.clip->right<=208
                    && draw.clip->top==32 && draw.clip->bottom==120,
                    "Actual Controls model/particle/text escaped its source flight panel");
            for(const auto& draw:ink.draws)
                require(draw.clip==PicaClip{96,32,208,120},"Actual Controls dust escaped its source flight panel");
            // Compare the full stream with a separately owned player-only
            // observation of this exact completed/interpolated source. The
            // player (including its shadow) is the late demo painter pass,
            // but still uses native finite 3D geometry in both eyes.
            auto isolated=std::make_shared<vr::GameSceneSnapshot>(*source.current);
            std::erase_if(isolated->objects,[&](const auto& object){return object.handle!=isolated->player;});
            auto player_source=source;player_source.current=isolated;
            const auto player=player_reference.prepare(player_source);
            if(!player.vertices.empty()) {
                ++player_phases;
                require(geometry.vertices.size()>=player.vertices.size(),"Controls player was omitted from the full model stream");
                const auto first=unsigned(geometry.vertices.size()-player.vertices.size());
                if(first) ++weapons_phases;
                require(std::equal(player.vertices.begin(),player.vertices.end(),geometry.vertices.begin()+first),
                    "Controls player/shadow are not the final model painter pass");
                for(const auto& draw:geometry.draws) {
                    require(draw.first>=first || draw.first+draw.count<=first,"Controls player merged across its depth-policy boundary");
                    const bool late=draw.first>=first;
                    require(draw.space==PicaSpace::world && draw.source_layer==1
                        && draw.depth_test==!late && draw.depth_write==!late,
                        "Controls player is depth-occluded by the weapons demo or flattened to HUD depth");
                }
                for(unsigned eye=0;eye<source.plan.eye_count;++eye)
                    for(const auto& draw:geometry.draws)
                        require(pica_draw_matrix(source.plan,eye,draw)
                            ==pica_multiply(PicaProjection(source.plan,eye).rows(),draw.model),
                            "Controls player bypassed the active native eye projection");
            }
            if(source.raster->circle.active && source.raster->circle.radius && (source.raster->circle.affected_layers&63)) {
                ++circles;require(!prepared.colour.draws.empty() && prepared.colour.draws.front().clip==policy.circle_clip,
                    "Actual Controls disk lost its live clip before native composition");
                for(unsigned i=0;i<prepared.colour.draws.front().count;++i) {
                    const auto p=prepared.colour.vertices[i].position;
                    require(p[0]>=96 && p[0]<=208 && p[1]>=32 && p[1]<=120,"Actual Controls disk emits ink over instructions/controller artwork");
                }
            }
        } else {
            for(const auto& draw:ink.draws) require(!draw.clip,"Controls dust clip leaked into another actual source flow");
        }
        if(phase%90==0) std::cout<<"  phase "<<phase<<" flow "<<unsigned(source.current->flow)<<" circles "<<circles<<'\n'<<std::flush;
        if(phases[unsigned(training)]>=180) break;
    }
    require(phases[unsigned(simulation::GameFlowState::controls_type)]>=180
        && phases[unsigned(simulation::GameFlowState::controls_choice)]>=60
        && phases[unsigned(simulation::GameFlowState::training)]>=180,
        "Normal source title/Controls/Training flow did not complete required live phases");
    require(circles>0,"Actual Controls route did not exercise a live demonstration circle");
    require(player_phases>180,"Actual Controls route did not exercise its live late player pass");
    require(weapons_phases>=30,"Actual Controls blaster input did not exercise ordinary weapons alongside the late player");
    std::cout<<"Actual native title/Controls/Training effect flow: "<<compositions<<" complete compositions, "<<circles
        <<" live clipped circle phases, "<<player_phases<<" late 3D player phases ("<<weapons_phases
        <<" with ordinary demo geometry), peak padded scene/lower textures "<<peak_bytes
        <<"; HOST source/resource acceptance, NOT ARM/PICA pixels, process RAM or physical FPS\n";
}
void native_landscape_flow(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    // Observe ordinary campaign entry and stage progression. Original Fortuna
    // also uses the existing ordinary-input/GOD diagnostic route to reach its
    // results/map; no teleport or emulated guest-state mutation is involved.
    for(const std::string map:{"TITLEMAP","LEVEL1_1","LEVEL3_3"}) {
        const bool fortuna=map=="LEVEL3_3" && symbols.find("SPECWEPCNTONE").empty();
        if(map=="LEVEL3_3" && !fortuna) continue;
        GameSessionOptions options;
        if(fortuna) {options.preferences=GamePreferences{};options.preferences->god=true;}
        GameSession session(rom,symbols,[](auto){},map,{},options);
        GameModels models(session.rom(),session.symbols());GameDots dots(session.rom(),session.symbols());
        GameLayers layers;GameEffects effects;PicaComposite composite,artwork_composite;PicaRaster priority_oracle,edge_oracle;
        unsigned compositions=0,finite_compositions=0,peak_bytes=0,priority_comparisons=0;
        std::optional<fortuna_route_diagnostic::Inputs> guided;
        if(fortuna) guided.emplace(symbols);
        bool boss=false,results=false;unsigned returned_map=0;
        session.advance(0,0);
        auto previous=session.game().flow_state();unsigned age=0;
        std::map<unsigned,std::array<unsigned,2>> observed;
        for(unsigned phase=1;phase<=(fortuna?36000U:7200U);++phase) {
            const auto flow=session.game().flow_state();
            if(flow!=previous) {age=0;previous=flow;}++age;
            input::ButtonMask held=0;
            using enum simulation::GameFlowState;
            if((flow==title || flow==ex_pregame_menu) && age%90==0) held=input::start;
            else if(flow==controls_type && age>=180 && age%90==0) held=input::start;
            else if(flow==controls_choice) {
                if(age==30) held=input::down; // Select GAME, not Training.
                else if(age>=90 && age%90==0) held=input::start;
            } else if(flow==planet_select && age>=90 && age%90==0) held=input::a;
            else if(flow==gameplay && age>30) held=input::ButtonMask(input::y|input::right);
            if(guided) held=guided->held(session,phase,boss,results);
            session.advance(std::int64_t(phase)*1'000'000'000/60,held);
            const auto source=session.presentation(0,false);
            boss|=session.game().peek_meter_state().boss_max_health!=0;
            results|=source.current->flow==stage_results;
            if(results && (source.current->flow==planet_select || source.current->flow==planet_travel)
                && source.raster->brightness==15) ++returned_map;
            if(source.current->background_landscape && source.current->landscape_grid_height<0
                && source.raster->ppu->background_mode==2 && !source.raster->ppu->tunnel_scene
                && !source.raster->boss_roll) {
                auto& count=observed[unsigned(source.current->flow)];
                if(!count[0]) std::cout<<map<<": first eligible landscape flow "<<unsigned(source.current->flow)
                    <<" phase "<<phase<<" native receiver="<<native_landscape_scene(source)<<std::endl;
                ++count[0];count[1]+=native_landscape_scene(source);
                if(source.current->flow==stage_results && (count[0]==1 || count[0]%30==0)) {
                    context="Actual retained results landscape phase "+std::to_string(phase);
                    const auto state=session.game().save_state(),apu=session.audio().save_state();
                    StereoSettings settings;settings.separation=64;settings.convergence=16;settings.strength=2;
                    for(float slider:{0.F,.5F,1.F}) {
                        const auto optical=session.presentation(slider,true,settings);
                        require(native_landscape_scene(optical),"Actual results landscape lost its finite receiver policy");
                        const auto geometry=models.prepare(optical),ink=dots.prepare(optical);
                        const auto fx=effects.prepare(optical);
                        const auto artwork=layers.prepare(optical,remaining_layer_vertices({geometry,ink,fx.colour,fx.window}));
                        const auto complete=composite.prepare(optical.plan,
                            std::array{artwork.before_models,ink,geometry,artwork.after_models,fx.colour,fx.window},
                            optical.dashboard,artwork.clear);
                        validate_pica_frame(complete,optical.dashboard);++compositions;
                        unsigned bytes=512U*256U*4U;
                        for(const auto image:complete.textures) bytes+=pica_resident_texture_bytes(image);
                        require(bytes<=pica_texture_budget,"Actual results split exceeded whole-scene/lower LCD residency");
                        peak_bytes=std::max(peak_bytes,bytes);
                        finite_compositions+=std::any_of(artwork.before_models.draws.begin(),artwork.before_models.draws.end(),
                            [](const auto& draw){return draw.source_layer==2 && draw.space==PicaSpace::world;});
                        if(slider==0) {
                            const auto policy=game_layer_plan(optical);auto raw=policy.before_models;
                            raw.passes.insert(raw.passes.end(),policy.after_models.passes.begin(),policy.after_models.passes.end());
                            const auto authored=priority_oracle.prepare(optical.raster->ppu,raw,optical.plan,
                                optical.raster->brightness,optical.current->background_colour_subtract);
                            const auto decoded=artwork_composite.prepare_layers(optical.plan,
                                std::array{artwork.before_models,artwork.after_models});
                            const auto actual_pixels=flat_painter_pixels(decoded,true);
                            // BG2 extends its edge scanlines into the native
                            // top/bottom LCD margin. Screen-space OBJ does not.
                            // Compare canonical/source priority with the mixed
                            // raw decoder, and those extra rows independently
                            // with an all-priority BG2-only source decoder.
                            PpuBatch edges{{{PpuLayer::bg2}},PicaSpace::scenery,true};
                            edges.passes[0].scroll=optical.current->background_scroll_override;
                            const auto sky=flat_painter_pixels(edge_oracle.prepare(optical.raster->ppu,edges,optical.plan,
                                optical.raster->brightness,optical.current->background_colour_subtract));
                            auto expected_pixels=flat_painter_pixels(authored);
                            for(unsigned row=0;row<screen_height;++row) if(row<8 || row>=232)
                                std::copy_n(sky.begin()+row*top_width,top_width,expected_pixels.begin()+row*top_width);
                            const auto mismatch=std::mismatch(actual_pixels.begin(),actual_pixels.end(),expected_pixels.begin());
                            if(mismatch.first!=actual_pixels.end()) {
                                const auto at=unsigned(mismatch.first-actual_pixels.begin());
                                std::cout<<"Results pixel mismatch LCD "<<at%top_width<<','<<at/top_width<<" actual/expected ";
                                for(auto value:*mismatch.first) std::cout<<value<<',';
                                std::cout<<" / ";for(auto value:*mismatch.second) std::cout<<value<<',';std::cout<<std::endl;
                            }
                            require(mismatch.first==actual_pixels.end(),
                                "Actual retained results terrain changed mono full-LCD colours, opacity or score/sprite priorities");
                            ++priority_comparisons;
                        }
                    }
                    require(session.game().save_state()==state && session.audio().save_state()==apu,
                        "Results optics/composition changed source or SPC state");
                }
            }
            if(phase%600==0) std::cout<<map<<": observation phase "<<phase<<" flow "
                <<unsigned(source.current->flow)<<std::endl;
            if((source.current->flow==continue_choice && age>=60) || returned_map>=60) break;
        }
        for(const auto& [flow,count]:observed)
            std::cout<<map<<": actual landscape flow "<<flow<<" eligible/native receiver frames "
                <<count[0]<<'/'<<count[1]<<'\n';
        require(observed.contains(unsigned(simulation::GameFlowState::gameplay)),
            "Normal landscape observation failed to reach Corneria gameplay");
        if(fortuna) {
            require(boss && results && returned_map>=60,"Fortuna observation did not complete its ordinary-input boss/results/map route");
            const auto retained=observed.find(unsigned(simulation::GameFlowState::stage_results));
            require(retained!=observed.end() && retained->second[0]>=120 && retained->second[0]==retained->second[1]
                && finite_compositions>0 && compositions>=12 && priority_comparisons>=4,
                "Fortuna check missed real retained results terrain, finite compositions or full-LCD source comparisons");
            std::cout<<map<<": retained results "<<compositions<<" complete supported-optics compositions / "
                <<finite_compositions<<" finite receivers / "<<priority_comparisons<<" full-LCD source priority comparisons; peak "
                <<peak_bytes<<" padded texture bytes including lower LCD; source/SPC parity passed; HOST only, not PICA/device acceptance\n";
        }
    }
}
void auxiliary_checks(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    context="Synthetic particle/transaction fixture using actual cartridge assets";
    GameSession session(rom,symbols,[](auto){},"LEVEL1_1");GameModels models(session.rom(),session.symbols());
    auto source=session.presentation(1,true);
    auto scene=std::make_shared<vr::GameSceneSnapshot>(*source.current);
    scene->objects.clear();scene->transforms.clear();scene->particles={};scene->camera={};
    scene->view_matrix={32767,0,0,0,32767,0,0,0,32767};scene->shadows_enabled=false;
    vr::GameSceneObject item;item.handle=1;item.object.strategy_flags[0]=0x10;
    item.presentation.transform.z=512;item.presentation.rotation_matrix=scene->view_matrix;
    item.source_pose.source_depth=512;item.source_pose.use_source_lighting_state=true;
    item.source_pose.source_lighting_matrix=scene->view_matrix;
    scene->objects.push_back(item);scene->transforms.emplace(item.handle,item.presentation);
    auto& dot=scene->particles[0];dot.owner=1;dot.life=8;dot.colour=5;
    dot.previous_x=32760;dot.x=-32760; // Short signed-word crossing, not a 65k jump.
    scene->particles[1]=dot;scene->particles[1].flags=4;
    scene->particles[2]=dot;scene->particles[2].z=scene->particles[2].previous_z=-400;
    scene->particles[3]=dot;scene->particles[3].owner=2;
    source.previous=source.current=scene;source.interpolation_alpha=.5;
    const auto before=session.game().save_state(),before_audio=session.audio().save_state();
    const auto frame=models.prepare(source);validate_pica_frame(frame,source.dashboard);
    require(models.coverage().particles==2 && frame.vertices.size()==12,
        "Owner/depth rules must keep exactly the valid 2x2 dot and one trail");
    float low=std::numeric_limits<float>::max(),high=-low;
    for(const auto& vertex:frame.vertices) {low=std::min(low,vertex.position[0]);high=std::max(high,vertex.position[0]);}
    require(high-low<32 && low>32750,"Particle interpolation took the long path across signed-word wrap");
    const std::vector<PicaVertex> saved(frame.vertices.begin(),frame.vertices.end());
    auto clipped=std::make_shared<vr::GameSceneSnapshot>(*scene);
    clipped->objects[0].source_pose.effect_clip_left=64;clipped->objects[0].source_pose.effect_clip_right=160;
    auto masked=source;masked.current=masked.previous=clipped;
    const auto masked_frame=models.prepare(masked);validate_pica_frame(masked_frame,source.dashboard);
    require(masked_frame.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),masked_frame.vertices.begin())
        && masked_frame.draws.size()==1 && masked_frame.draws[0].clip==PicaClip{152,0,248,240},
        "Particle source window must become hardware eye-space clip, not mono geometry truncation");
    auto invalid=source;invalid.interpolation_alpha=std::numeric_limits<double>::quiet_NaN();
    bool rejected=false;try {static_cast<void>(models.prepare(invalid));} catch(const std::exception&) {rejected=true;}
    require(rejected,"Invalid interpolation must reject a new source frame");
    require(masked_frame.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),masked_frame.vertices.begin())
        && models.coverage().particles==2,"Failed new scene replaced or invalidated the last complete model stream");
    // Fail AFTER a complete scene's worth of valid conversion, not only in
    // interpolation preflight. The unpublished reusable owner must never
    // invalidate the borrowed last-successful scene or its coverage.
    auto overflow=std::make_shared<vr::GameSceneSnapshot>(*scene);
    overflow->objects.assign(pica_vertex_limit/12+1,item);
    auto oversized=source;oversized.previous=oversized.current=overflow;
    rejected=false;try {static_cast<void>(models.prepare(oversized));}catch(const std::exception&) {rejected=true;}
    require(rejected && std::equal(saved.begin(),saved.end(),masked_frame.vertices.begin(),masked_frame.vertices.end())
        && masked_frame.draws.size()==1 && masked_frame.draws[0].count==12
        && masked_frame.draws[0].clip==PicaClip{152,0,248,240} && models.coverage().particles==2,
        "Late over-budget preparation changed the published native model scene");
    const auto recovered=models.prepare(source);validate_pica_frame(recovered,source.dashboard);
    require(recovered.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),recovered.vertices.begin(),recovered.vertices.end())
        && recovered.draws.size()==1 && recovered.draws[0].count==12 && !recovered.draws[0].clip
        && models.coverage().particles==2,"Retry retained partial geometry/draw state from the unpublished failure");
    auto raster=std::make_shared<GameRasterSnapshot>(*source.raster);raster->brightness=0;source.raster=raster;
    const auto faded=models.prepare(source);
    for(const auto& vertex:faded.vertices) require(vertex.colour[0]==0 && vertex.colour[1]==0 && vertex.colour[2]==0,
        "Native 60Hz fade was delayed until the next model snapshot");
    require(session.game().save_state()==before && session.audio().save_state()==before_audio,
        "Synthetic source observation or fade modified cartridge/audio state");
}
void fixture(const assets::RomImage& rom,const assets::SymbolMap& symbols,const std::string& map,unsigned requested_phases=0,
    bool all_optics=false,bool fortuna_water=false,bool fortuna_complete=false) {
    const bool fortuna=fortuna_water || fortuna_complete;
    GameSessionOptions options;
    if(fortuna) {options.preferences=GamePreferences{};options.preferences->god=true;}
    GameSession session(rom,symbols,[](auto){},map,{},options);GameModels models(session.rom(),session.symbols());
    if(fortuna) require(session.cartridge_experience()==simulation::Experience::original,
        "Fortuna water transition requires the Original cartridge");
    PicaRaster background,objects,native_bitmap,priority_oracle;PicaComposite composite;PicaWindow window;
    PicaColourEffects colour;GameLayers cartridge_layers,reference_layers;GameDots dots(session.rom(),session.symbols());
    unsigned frames{},models_seen{},shadows{},glyphs{},particles{},vertices{},draws{},textures{};
    unsigned dust_points{},grid_points{},connected_points{},combined_vertices{},combined_draws{},combined_textures{};
    unsigned panorama_frames{},combined_resident_bytes{},optical_frames{},optical_resident_bytes{};
    unsigned water_frames{},tunnel_frames{},unique_frames{},orbital_frames{},disabled_dot_frames{},intro_frames{};
    // Observe actual cartridge flows before accepting a receiver policy. A
    // retained landscape is not necessarily gameplay: transition artwork and
    // its OBJ priorities must not silently become a flat screen-space image.
    std::map<unsigned,std::array<unsigned,2>> landscape_flows;
    unsigned finite_water_frames{},water_oracles{},first_water_phase{};
    unsigned first_boss_phase{},first_results_phase{},first_map_phase{},results_frames{},map_frames{},visible_map_frames{};
    unsigned first_boss_damage_phase{};
    std::optional<fortuna_route_diagnostic::Inputs> guided;
    if(fortuna_complete) guided.emplace(symbols);
    bool results_visible=false,results_tallied=false;
    const auto& water_strategies=symbols.find("PLAYERONWATER_STRAT");
    if(fortuna) require(!water_strategies.empty(),"Fortuna cartridge has no on-water player strategy");
    std::optional<decltype(session.game().map().ppu_state().cgram)> land_palette;
    bool sea_palette_changed=false;
    unsigned source_video_phases{},source_logic_ticks{};
    PicaRaster water_before_oracle,water_after_oracle;
    // Retain the actual owner graph through scene changes. Recreating it per
    // maximum-optics sample cannot detect growing cached receiver domains.
    GameLayers wide_layers;GameModels wide_models(session.rom(),session.symbols());
    GameDots wide_dots(session.rom(),session.symbols());PicaColourEffects wide_colour;
    PicaWindow wide_window;PicaComposite wide_composite;
    std::array<bool,2> optical_checked{};
    std::array<bool,4> panorama_modes_checked{};
    std::array<bool,2> unique_halves_checked{};
    session.advance(0,0);
    const unsigned phases=requested_phases?requested_phases:map=="BOOT"?240:1440;unsigned outdoor_frames=0;
    for(unsigned phase=1;phase<=phases;++phase) {
        context=map+" native phase "+std::to_string(phase);
        const auto time=(std::int64_t(phase)*1'000'000'000+59)/60;
        const auto held=guided?guided->held(session,phase,first_boss_phase!=0,first_results_phase!=0)
            :fortuna?input::ButtonMask(phase>24?input::y:0):map=="BOOT" && phase==8?input::start
            :map!="BOOT" && phase>24?input::ButtonMask(input::y|input::right):0;
        const auto advance=session.advance(time,held);
        source_video_phases+=advance.video_phases;source_logic_ticks+=advance.logic_ticks;
        auto source=session.presentation(1,true);const auto state=session.game().save_state(),apu=session.audio().save_state();
        if(source.current->background_landscape && source.current->landscape_grid_height<0
            && source.raster->ppu->background_mode==2 && !source.raster->ppu->tunnel_scene
            && !source.raster->boss_roll) {
            auto& observed=landscape_flows[unsigned(source.current->flow)];
            ++observed[0];observed[1]+=native_landscape_scene(source);
        }
        // Fortuna's mapfadetosea changes its Mode-2 terrain palette and
        // PLAYERONWATER_STRAT, not Titania's Mode-1 BG_2_3B WATER backdrop.
        // Observe actual source execution; never write its strategy/position.
        const bool fortuna_sea=fortuna && session.game().flow_state()==simulation::GameFlowState::gameplay
            && session.game().objects().is_active(session.game().player())
            && std::find(water_strategies.begin(),water_strategies.end(),
                session.game().objects().at(session.game().player()).strategy_address)!=water_strategies.end();
        if(fortuna && native_landscape_scene(source)) {
            if(!land_palette && !fortuna_sea) land_palette=source.raster->ppu->cgram;
            if(fortuna_sea && land_palette) sea_palette_changed|=*land_palette!=source.raster->ppu->cgram;
        }
        const auto meter=session.game().peek_meter_state();
        if(fortuna_complete) {
            if(meter.boss_max_health && meter.boss_health && !first_boss_phase) {
                first_boss_phase=phase;
                std::cout<<"Fortuna reached live source boss at phase "<<phase<<std::endl;
            }
            if(first_boss_phase && meter.boss_max_health && meter.boss_health<meter.boss_max_health
                && !first_boss_damage_phase) {
                first_boss_damage_phase=phase;
                std::cout<<"Fortuna observed ordinary-input boss damage at phase "<<phase<<std::endl;
            }
            const auto flow=session.game().flow_state();
            if(flow==simulation::GameFlowState::stage_results) {
                require(first_boss_phase>0,"Fortuna results appeared without a live source boss");
                if(!first_results_phase) {
                    first_results_phase=phase;
                    std::cout<<"Fortuna entered source results at phase "<<phase<<std::endl;
                }
                const auto tally=session.game().stage_results_state();
                require(tally.active,"Fortuna results flow lost its source tally");
                results_visible|=tally.visible && source.raster->brightness>0;
                results_tallied|=tally.visible && tally.displayed_percentage==tally.percentage;
                ++results_frames;
            }
            if(flow==simulation::GameFlowState::planet_select || flow==simulation::GameFlowState::planet_travel) {
                require(first_results_phase>first_boss_phase,"Fortuna map appeared without completing boss/results flow");
                if(!first_map_phase) {
                    first_map_phase=phase;
                    std::cout<<"Fortuna returned to source map at phase "<<phase<<std::endl;
                }
                ++map_frames;
                visible_map_frames+=source.raster->brightness==15;
            }
            require(flow!=simulation::GameFlowState::game_over && flow!=simulation::GameFlowState::continue_choice,
                "Fortuna full-route diagnostic reached a death/continue screen instead of clearing");
        }
        if(fortuna && phase%600==0) std::cout<<"Fortuna progress phase="<<phase
            <<" video="<<source_video_phases<<" logic="<<source_logic_ticks<<" cursor="<<session.game().map().cursor()
            <<" countdown="<<session.game().map().countdown()<<" sea="<<fortuna_sea
            <<" palette_changed="<<sea_palette_changed<<" flow="<<unsigned(session.game().flow_state())
            <<" boss="<<unsigned(meter.boss_health)<<'/'<<unsigned(meter.boss_max_health)<<std::endl;
        water_frames+=source.current->background_water_surround;
        tunnel_frames+=source.raster->ppu->tunnel_scene;
        unique_frames+=source.current->background_unique_top_rows!=0
            || source.current->background_landscape_unique_half || source.current->background_landscape_unique_right_half;
        orbital_frames+=source.current->background_orbital_planet;
        intro_frames+=source.current->flow==simulation::GameFlowState::intro;
        const auto frame=models.prepare(source);validate_pica_frame(frame,source.dashboard);
        const auto dot_frame=dots.prepare(source);validate_pica_frame(dot_frame,source.dashboard);
        // Resource/ordering bridge check, NOT the final all-flow compositor:
        // native ground, EX spans and menu host UI still remain.
        PpuBatch bg{{{PpuLayer::bg2}},PicaSpace::scenery,true};
        bg.passes[0].scroll=source.current->background_scroll_override;
        PpuBatch obj{{{PpuLayer::objects}}};obj.passes[0].sprites=source.sprites;
        const auto back=background.prepare(source.raster->ppu,bg,source.plan,source.raster->brightness,
            source.current->background_colour_subtract);
        const auto front=objects.prepare(source.raster->ppu,obj,source.plan,source.raster->brightness);
        PpuBatch bitmap{{{PpuLayer::bg1}}};bitmap.passes[0].guard_inset=16;
        bitmap.passes[0].transparent_black=true;bitmap.passes[0].mosaic_inset=true;
        validate_pica_frame(native_bitmap.prepare(source.raster->ppu,bitmap,source.plan,source.raster->brightness),source.dashboard);
        const auto effect_policy=game_effect_plan(source);
        const auto mask=window.prepare(source.raster->wipe,source.plan,effect_policy.window);
        const auto effects=colour.prepare(source.raster->circle,source.raster->colour_math,source.raster->brightness,source.plan,effect_policy.circle_clip);
        const auto composed=composite.prepare(source.plan,std::array{back,frame,front,effects,mask},source.dashboard);
        require(composed.vertices.size()==back.vertices.size()+frame.vertices.size()+front.vertices.size()+effects.vertices.size()+mask.vertices.size()
            && std::equal(frame.vertices.begin(),frame.vertices.end(),composed.vertices.begin()+back.vertices.size()),
            "Native layer composition lost/reprojected cartridge model geometry");
        const auto layer_budget=remaining_layer_vertices({frame,dot_frame,effects,mask});
        const auto ordered=cartridge_layers.prepare(source,layer_budget);
        if(fortuna_sea) {
            require(outdoor_frames>0,"Fortuna skipped its real land-to-water transition");
            require(native_landscape_scene(source),"Fortuna sea lost its authored Mode-2 landscape");
            const auto plane=source_landscape_plane(source);
            const double height=plane.height;
            unsigned finite=0;
            for(const auto* group:{&ordered.before_models,&ordered.after_models}) for(const auto& draw:group->draws) {
                if(draw.space!=PicaSpace::world || draw.source_layer!=2) continue;
                require(draw.projected_uv && draw.depth_test && draw.depth_write,
                    "Actual Fortuna water lost its finite depth/UV receiver");
                for(unsigned at=draw.first;at<draw.first+draw.count;++at) {
                    const auto p=group->vertices[at].position;
                    const double residual=p[1]+plane.slope*source.plan.focal_x/source.plan.focal_y*p[0]
                        +(plane.centre-120)/source.plan.focal_y*p[2]+height;
                    require(std::abs(residual)<std::max(.02,8*std::numeric_limits<float>::epsilon()*height)
                        && p[2]>=source.plan.near_plane-.02 && p[2]<=source.plan.far_plane+.02,
                        "Actual Fortuna water departed from its source plane or near/far clip");
                }
                ++finite;
            }
            require(finite>0,"Actual Fortuna sea remained a flat backdrop without a finite receiver");
            if(!first_water_phase) {first_water_phase=phase;std::cout<<"Fortuna entered source water at phase "<<phase<<std::endl;}
            ++finite_water_frames;
            if(finite_water_frames==1 || finite_water_frames%30==0) {
                const auto policy=game_layer_plan(source);
                const auto authored_before=water_before_oracle.prepare(source.raster->ppu,policy.before_models,
                    source.plan,source.raster->brightness,source.current->background_colour_subtract);
                const auto authored_after=water_after_oracle.prepare(source.raster->ppu,policy.after_models,
                    source.plan,source.raster->brightness,source.current->background_colour_subtract);
                require(flat_painter_pixels(ordered.before_models,true)==flat_painter_pixels(authored_before),
                    "Actual Fortuna water changed mono BG/OBJ colours, holes, margins or source priority before models");
                require(flat_painter_pixels(ordered.after_models,true)==flat_painter_pixels(authored_after),
                    "Actual Fortuna water changed mono BG/OBJ colours, holes, margins or source priority after models");
                ++water_oracles;
            }
        }
        if(native_panorama_scene(source)) {
            ++panorama_frames;
            const auto policy=game_layer_plan(source);std::vector<PpuPass> flattened;
            for(const auto& group:policy.before_model_groups)
                flattened.insert(flattened.end(),group.passes.begin(),group.passes.end());
            require(flattened==policy.before_models.passes,"Actual source panorama reordered interleaved sprite/background priorities");
            for(const auto& draw:ordered.before_models.draws) if(draw.space==PicaSpace::scenery)
                require(draw.source_layer==2 && ordered.before_models.textures[draw.texture].source_layers.empty(),
                    "Actual source panorama pulled mixed sprite ownership into infinity or duplicated A8 residency");
            const auto mode=source.raster->ppu->background_mode;
            if(!panorama_modes_checked[mode] && !ordered.before_models.draws.empty()) {
                const auto mono=reference_layers.prepare(source,0);
                require(flat_painter_pixels(ordered.before_models)==flat_painter_pixels(mono.before_models),
                    "Actual cartridge tile/split scenery changed mono LCD pixels, margins or source ownership");
                panorama_modes_checked[mode]=true;
            }
        }
        if(native_landscape_scene(source)) {
            ++outdoor_frames;
            require(std::any_of(ordered.before_models.draws.begin(),ordered.before_models.draws.end(),[](const auto& draw) {
                return draw.space==PicaSpace::scenery && draw.source_layer==2;}),
                "Actual outdoor cartridge background stayed at HUD depth");
            const auto plane=source_landscape_plane(source);
            const double distance=plane.height*source.plan.focal_y;
            unsigned sky_count=0,receiver_count=0;bool receiver_visible=false;std::set<unsigned> terrain_textures;
            for(const auto& draw:ordered.before_models.draws) {
                if(draw.source_layer!=2) {
                    require(draw.space==PicaSpace::screen && !draw.depth_test && !draw.depth_write,
                        "Actual results sprite/text was projected as terrain");continue;
                }
                terrain_textures.insert(draw.texture);
                if(draw.space==PicaSpace::scenery) {
                    ++sky_count;double low=std::numeric_limits<double>::max(),high=-low;
                    for(unsigned i=draw.first;i<draw.first+draw.count;++i) {
                        const auto& p=ordered.before_models.vertices[i].position;
                        const double d=p[1]-plane.centre-plane.slope*(p[0]-200);
                        low=std::min(low,d);high=std::max(high,d);
                    }
                    receiver_visible|=high>distance/source.plan.far_plane && low<distance/source.plan.near_plane;
                } else {
                    ++receiver_count;
                    require(draw.space==PicaSpace::world && draw.projected_uv && draw.depth_test,
                        "Actual cartridge terrain lost its finite source-owned receiver");
                }
            }
            // A source horizon below the LCD correctly emits sky only; broad
            // guarded receivers can also use more than one borrowed strip.
            // Require finite geometry exactly when the source domain intersects
            // the near/far receiver, not an unconditional two-draw screenshot.
            require(sky_count==terrain_textures.size() && (receiver_count!=0)==receiver_visible,
                "Actual cartridge finite terrain coverage disagrees with its source plane");
            if(source.current->background_landscape_unique_half || source.current->background_landscape_unique_right_half) {
                const bool right=source.current->background_landscape_unique_right_half;
                auto policy=game_layer_plan(source);
                require(std::any_of(policy.before_models.passes.begin(),policy.before_models.passes.end(),[&](const auto& pass) {
                    return pass.single_occurrence_sky_half==PpuUniqueSkyHalf{right,unsigned(source.current->landscape_atlas_origin)+112};
                }),"Actual unique landscape did not reach its native BG2 decoder policy");
                if(!unique_halves_checked[right]) {
                    for(auto& pass:policy.before_models.passes) pass.single_occurrence_sky_half.reset();
                    const auto authored=priority_oracle.prepare(source.raster->ppu,policy.before_models,source.plan,
                        source.raster->brightness,source.current->background_colour_subtract);
                    for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
                        require(source_pixel(ordered.before_models,x,y)==source_pixel(authored,x,y),
                            "Unique-half margin policy changed actual canonical cartridge pixels or opaque ownership");
                    unique_halves_checked[right]=true;
                }
            }
        }
        const auto ordered_frame=composite.prepare(source.plan,
            std::array{ordered.before_models,dot_frame,frame,ordered.after_models,effects,mask},source.dashboard,ordered.clear);
        require(ordered_frame.vertices.size()==ordered.before_models.vertices.size()+dot_frame.vertices.size()+frame.vertices.size()
            +ordered.after_models.vertices.size()+effects.vertices.size()+mask.vertices.size(),
            "Actual cartridge priority adapter lost a model/PPU/effect painter group");
        require(std::equal(dot_frame.vertices.begin(),dot_frame.vertices.end(),ordered_frame.vertices.begin()+ordered.before_models.vertices.size())
            && std::equal(frame.vertices.begin(),frame.vertices.end(),ordered_frame.vertices.begin()+ordered.before_models.vertices.size()+dot_frame.vertices.size()),
            "Actual cartridge dust/grid must precede models, after source scenery");
        combined_vertices=std::max(combined_vertices,unsigned(ordered_frame.vertices.size()));
        combined_draws=std::max(combined_draws,unsigned(ordered_frame.draws.size()));
        combined_textures=std::max(combined_textures,unsigned(ordered_frame.textures.size()));
        unsigned resident=512U*256U*4U;
        for(const auto& image:ordered_frame.textures) resident+=pica_resident_texture_bytes(image);
        combined_resident_bytes=std::max(combined_resident_bytes,resident);
        require(resident<=pica_texture_budget,"Actual source painter separation exceeded total padded native residency including lower LCD");
        const unsigned optical_kind=native_landscape_scene(source)?1:0;
        if(all_optics || fortuna_complete || fortuna_sea
            || ((native_landscape_scene(source) || native_panorama_scene(source)) && !optical_checked[optical_kind])) {
            auto wide=source;auto settings=session.stereo_settings();
            settings.strength=2;settings.separation=64;settings.convergence=16;
            wide.plan=plan_frame(1,true,ScreenUse::world,settings);
            const auto phase_context=context;
            context+=" maximum-optics source layers";
            context=phase_context+" maximum-optics geometry/grid";
            const auto geometry=wide_models.prepare(wide),ink=wide_dots.prepare(wide);
            const auto wide_policy=game_effect_plan(wide);
            const auto tint=wide_colour.prepare(wide.raster->circle,wide.raster->colour_math,wide.raster->brightness,wide.plan,wide_policy.circle_clip);
            const auto wipe=wide_window.prepare(wide.raster->wipe,wide.plan,wide_policy.window);
            const auto layers=wide_layers.prepare(wide,remaining_layer_vertices({geometry,ink,tint,wipe}));
            context=phase_context+" maximum-optics complete composition";
            const auto composed_wide=wide_composite.prepare(wide.plan,
                std::array{layers.before_models,ink,geometry,layers.after_models,tint,wipe},wide.dashboard,layers.clear);
            unsigned wide_bytes=512*256*4;
            for(const auto image:composed_wide.textures) {
                require(image.width<=1024,"Actual optical source strip exceeds PICA texture dimensions");
                wide_bytes+=pica_resident_texture_bytes(image);
            }
            require(wide_bytes<=pica_texture_budget && composed_wide.plan.separation==128,
                "Actual wide source composition exceeded residency or silently reduced optics");
            optical_resident_bytes=std::max(optical_resident_bytes,wide_bytes);++optical_frames;
            optical_checked[optical_kind]=true;
            context=phase_context;
        }
        const auto dot_coverage=dots.coverage();dust_points+=dot_coverage.dust;
        grid_points+=dot_coverage.grid;connected_points+=dot_coverage.connections;
        if(source.current->dots_mode==0) {
            ++disabled_dot_frames;
            require(dot_coverage.dust==0 && dot_coverage.grid==0 && dot_coverage.connections==0
                && dot_frame.vertices.empty() && dot_frame.draws.empty() && dot_frame.textures.empty(),
                "Cartridge-disabled dust/grid produced native geometry or retained ink");
        }
        const auto ordered_work=cartridge_layers.work();
        const auto background_work=background.work(),object_work=objects.work();
        const auto effect_builds=colour.builds();
        const auto count=models.coverage();models_seen+=count.models;shadows+=count.shadows;
        glyphs+=count.text_glyphs;particles+=count.particles;
        vertices=std::max(vertices,unsigned(frame.vertices.size()));draws=std::max(draws,unsigned(frame.draws.size()));
        textures=std::max(textures,unsigned(frame.textures.size()));
        require(count.cached_shape_bytes<=4*1024*1024,"Decoded asset cache grew beyond original-hardware budget");
        // Copy before re-preparing: these are explicitly borrowed spans.
        const std::vector<PicaVertex> saved(frame.vertices.begin(),frame.vertices.end());
        const std::vector<PicaDraw> saved_draws(frame.draws.begin(),frame.draws.end());
        std::vector<std::vector<std::uint8_t>> saved_textures;
        for(const auto image:frame.textures) saved_textures.emplace_back(image.pixels.begin(),image.pixels.end());
        const std::vector<PicaVertex> saved_dots(dot_frame.vertices.begin(),dot_frame.vertices.end());
        const std::vector<PicaDraw> saved_dot_draws(dot_frame.draws.begin(),dot_frame.draws.end());
        std::vector<std::vector<std::uint8_t>> saved_dot_textures;
        for(const auto image:dot_frame.textures) saved_dot_textures.emplace_back(image.pixels.begin(),image.pixels.end());
        for(float slider:{0.F,.5F,1.F}) {
            source=session.presentation(slider,true);const auto other=models.prepare(source);
            const auto other_dots=dots.prepare(source);validate_pica_frame(other_dots,source.dashboard);
            require(other_dots.vertices.size()==saved_dots.size() && std::equal(saved_dots.begin(),saved_dots.end(),other_dots.vertices.begin())
                && same_draws(saved_dot_draws,other_dots.draws) && other_dots.textures.size()==saved_dot_textures.size()
                && dots.coverage().ink_updates==dot_coverage.ink_updates,"Slider changed cartridge dust/grid ink or reran its rasterizer");
            for(unsigned i=0;i<saved_dot_textures.size();++i)
                require(std::equal(saved_dot_textures[i].begin(),saved_dot_textures[i].end(),other_dots.textures[i].pixels.begin()),
                    "Slider changed cartridge connected-grid palette/coverage");
            validate_pica_frame(other,source.dashboard);
            require(other.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),other.vertices.begin())
                && same_draws(saved_draws,other.draws),"Slider/eye projection rebuilt different world geometry or source order");
            require(other.textures.size()==saved_textures.size(),"Slider changed source artwork count");
            background.prepare(source.raster->ppu,bg,source.plan,source.raster->brightness,source.current->background_colour_subtract);
            objects.prepare(source.raster->ppu,obj,source.plan,source.raster->brightness);
            colour.prepare(source.raster->circle,source.raster->colour_math,source.raster->brightness,source.plan,game_effect_plan(source).circle_clip);
            cartridge_layers.prepare(source,layer_budget);
            const auto priority_work=cartridge_layers.work();
            require(priority_work[0].decodes==ordered_work[0].decodes && priority_work[1].decodes==ordered_work[1].decodes
                && priority_work[0].colour_updates==ordered_work[0].colour_updates && priority_work[1].colour_updates==ordered_work[1].colour_updates,
                "Slider reran actual cartridge painter policy rasterization");
            require(colour.builds()==effect_builds,"Slider rebuilt native source colour coverage");
            require(background.work().decodes==background_work.decodes && background.work().colour_updates==background_work.colour_updates
                && objects.work().decodes==object_work.decodes && objects.work().colour_updates==object_work.colour_updates,
                "Slider rerasterized native PPU layers");
            for(unsigned i=0;i<other.textures.size();++i)
                require(std::vector<std::uint8_t>(other.textures[i].pixels.begin(),other.textures[i].pixels.end())==saved_textures[i],
                    "Slider changed source palette/artwork");
        }
        require(session.game().save_state()==state && session.audio().save_state()==apu,"Native model preparation advanced/mutated game or audio");
        ++frames;
        if(fortuna_water && finite_water_frames>=120) break;
        if(fortuna_complete && visible_map_frames>=120) break;
    }
    std::cout<<map<<": "<<frames<<" source frames, "<<models_seen<<" models, "<<shadows<<" shadows, "<<glyphs<<" glyphs, "
        <<particles<<" particles, "<<outdoor_frames<<" terrain frames; peak "<<vertices<<" vertices / "<<draws<<" draws / "<<textures<<" textures\n";
    std::cout<<map<<": dust/grid/connected points "<<dust_points<<" / "<<grid_points<<" / "<<connected_points
        <<"; combined peak "<<combined_vertices<<" vertices / "<<combined_draws<<" draws / "<<combined_textures<<" textures\n";
    std::cout<<map<<": "<<panorama_frames<<" panorama frames; padded texture residency peak "<<combined_resident_bytes<<" bytes including lower LCD\n";
    for(const auto& [flow,observed]:landscape_flows)
        std::cout<<map<<": actual landscape flow "<<flow<<" eligible/native receiver frames "
            <<observed[0]<<'/'<<observed[1]<<'\n';
    std::cout<<map<<": source policy observations water/tunnel/unique/orbital "<<water_frames<<" / "<<tunnel_frames
        <<" / "<<unique_frames<<" / "<<orbital_frames<<"; cartridge-disabled dot frames "<<disabled_dot_frames
        <<"; counts do not prove visual policy acceptance\n";
    std::cout<<map<<": "<<optical_frames<<" actual strength-2 maximum-menu optical fixtures / "<<optical_resident_bytes
        <<" padded GPU bytes including lower LCD; "<<intro_frames<<" observed intro frames; "
        <<(all_optics || fortuna_complete?"every requested source frame checked":fortuna_water?
            "every water frame and first eligible land policy checked":"first eligible policy samples only")
        <<"; not whole-flow peak RAM or hardware acceptance\n";
    require(models_seen>0 && vertices>0,"Actual fixture never produced source geometry");
    if(fortuna) {
        require(outdoor_frames>0 && sea_palette_changed && finite_water_frames>=120 && water_oracles>=5,
            "Fortuna phase bound ended before 120 real source water frames and five full-LCD painter comparisons");
        std::cout<<"Fortuna real land-to-water: "<<finite_water_frames<<" finite receiver frames, "<<water_oracles
            <<" full-LCD BG/OBJ colour/coverage/priority comparisons, maximum menu optics on every water frame; "
            "original pacing/audio retained, diagnostic GOD cheat enabled; "
            <<(fortuna_complete?"full-route checks follow below":"not full stage or hardware acceptance")<<'\n';
    }
    if(fortuna_complete) {
        require(first_boss_phase>first_water_phase && first_boss_damage_phase>first_boss_phase
            && first_results_phase>first_boss_damage_phase && first_map_phase>first_results_phase
            && results_frames>0 && results_visible && results_tallied && visible_map_frames>=120,
            "Fortuna phase bound ended before live boss, visible completed tally and 120 fully visible return-map frames");
        std::cout<<"Fortuna source full route: boss/damage/results/map phases "<<first_boss_phase<<'/'<<first_boss_damage_phase
            <<'/'<<first_results_phase<<'/'<<first_map_phase
            <<", "<<results_frames<<" tally frames, "<<map_frames<<" map frames ("<<visible_map_frames
            <<" fully visible); persistent native renderer owners and maximum menu optics on every phase; "
            "host source/policy acceptance only, not ARM gameplay, PICA pixels or physical-device performance\n";
    }
    // These mandatory-positive checks belong to the original default fixtures.
    // Other stages can legitimately disable dots, or never contain terrain.
    if(map=="LEVEL1_1") require(outdoor_frames>0,"Corneria check stopped before the outdoor terrain actually appeared");
    if(map=="BOOT" || map=="LEVEL1_1")
        require(dust_points+grid_points+connected_points>0,"Default fixture did not exercise cartridge dust/grid");
}
}
int main(int argc,char** argv) try {
    constexpr auto usage="Usage: check_3ds_game_models ROM SYMBOLS [MAP [SOURCE_FRAMES [--all-optics]]]\n"
        "   or: check_3ds_game_models --bundle-original BIN [MAP [SOURCE_FRAMES [--all-optics]]]\n"
        "   or: check_3ds_game_models --bundle-ex BIN [MAP [SOURCE_FRAMES [--all-optics]]]\n"
        "   or: check_3ds_game_models --bundle-original BIN LEVEL3_3 MAX_PHASES --fortuna-water\n"
        "   or: check_3ds_game_models --bundle-original BIN LEVEL3_3 MAX_PHASES --fortuna-complete\n"
        "   or: check_3ds_game_models --bundle-original|--bundle-ex BIN --native-menu-composition\n"
        "   or: check_3ds_game_models ROM SYMBOLS --native-effects-flow\n"
        "   or: check_3ds_game_models --bundle-original|--bundle-ex BIN --native-landscape-flow\n"
        "Default: BOOT (240 frames) and LEVEL1_1 (1440 frames).\n"
        "Optional MAP: an exact cartridge map symbol; SOURCE_FRAMES: 1..3600.\n"
        "--all-optics: check every requested frame at strength 2, separation 64, convergence 16, retaining native owners.\n"
        "--fortuna-water: Original LEVEL3_3 only, MAX_PHASES 1..21600; centre/fire with GOD cheat, no source teleport;\n"
        "stop after land-to-water and 120 finite water frames, with full-LCD painter checks and maximum menu optics.\n"
        "--fortuna-complete: Original LEVEL3_3 only, MAX_PHASES 1..36000; same normal centre/fire and GOD cheat;\n"
        "require water, live boss, visible completed tally and 120 fully visible return-map frames; maximum optics every phase.\n"
        "--native-menu-composition: real plain setup and 24 preview phases through the complete test-player graph, supported optics.\n"
        "--native-effects-flow: normal TITLEMAP/Start/buttons through live Controls circles and Training; complete host compositor.\n"
        "--native-landscape-flow: campaign entry, stage progression and Original Fortuna GOD/input results/map; report real landscape flow/receiver observations.\n"
        "Uses private local assets; host policy/resource checks, not native gameplay or hardware acceptance.\n";
    if(argc==2 && std::string_view(argv[1])=="--help") {std::cout<<usage;return 0;}
    const bool fortuna_water=argc==6 && std::string_view(argv[5])=="--fortuna-water";
    const bool fortuna_complete=argc==6 && std::string_view(argv[5])=="--fortuna-complete";
    const bool native_menu=argc==4 && std::string_view(argv[3])=="--native-menu-composition";
    const bool native_effects=argc==4 && std::string_view(argv[3])=="--native-effects-flow";
    const bool native_landscape=argc==4 && std::string_view(argv[3])=="--native-landscape-flow";
    if(argc<3 || argc>6 || (argc==6 && std::string_view(argv[5])!="--all-optics" && !fortuna_water && !fortuna_complete))
        throw std::invalid_argument(usage);
    if(argc>=4 && ((std::string_view(argv[3])=="--native-menu-composition" && !native_menu)
        || (std::string_view(argv[3])=="--native-effects-flow" && !native_effects)
        || (std::string_view(argv[3])=="--native-landscape-flow" && !native_landscape)))
        throw std::invalid_argument(usage);
    if((fortuna_water || fortuna_complete) && (std::string_view(argv[3])!="LEVEL3_3" || std::string_view(argv[1])=="--bundle-ex"))
        throw std::invalid_argument(std::string(argv[5])+" requires Original LEVEL3_3");
    unsigned phases=0;
    if(argc>=5) {
        const auto value=std::string_view(argv[4]);
        const auto parsed=std::from_chars(value.data(),value.data()+value.size(),phases);
        const unsigned maximum=fortuna_complete?36000:fortuna_water?21600:3600;
        if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || phases<1 || phases>maximum)
            throw std::invalid_argument("SOURCE_FRAMES must be a whole number from 1 through "+std::to_string(maximum));
    }
    const auto cartridge=[&]() -> GameCartridge {
        const std::string_view kind=argv[1];
        if(kind=="--bundle-original" || kind=="--bundle-ex") {
            std::ifstream input(argv[2],std::ios::binary);
            if(!input) throw std::runtime_error(std::string("Cannot open companion BIN: ")+argv[2]);
            // Use the same build-time public-resource manifest and bounded
            // decoder as the console player. The BIN must not choose its own
            // expected checksum, nor bypass ROM/symbol/experience validation.
            return read_game_cartridge(input,companion_manifest,kind=="--bundle-ex"
                ?simulation::Experience::starfox_ex:simulation::Experience::original);
        }
        if(kind.starts_with("--")) throw std::invalid_argument(usage);
        return {assets::RomImage::load(argv[1]),assets::SymbolMap::load(argv[2])};
    }();
    const auto& rom=cartridge.rom;const auto& symbols=cartridge.symbols;
    auxiliary_checks(rom,symbols);
    if(native_landscape) native_landscape_flow(rom,symbols);
    else if(native_effects) native_effect_flow(rom,symbols);
    else if(native_menu) native_menu_composition(rom,symbols);
    else if(argc>=4) fixture(rom,symbols,argv[3],phases,argc==6 && std::string_view(argv[5])=="--all-optics",fortuna_water,fortuna_complete);
    else {fixture(rom,symbols,"BOOT");fixture(rom,symbols,"LEVEL1_1");}
    if(native_menu || native_effects || native_landscape) std::cout<<checks<<" native menu/effect/landscape composition host checks passed; NOT ARM gameplay, PICA pixels or hardware acceptance\n";
    else std::cout<<checks<<" native model-stream checks passed; NOT full compositor, ARM gameplay or hardware acceptance\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<context<<": "<<error.what()<<'\n';return 1;}
