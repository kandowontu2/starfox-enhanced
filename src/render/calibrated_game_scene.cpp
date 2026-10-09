#include "starfox/render/calibrated_game_scene.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/global_enhancements.hpp"
#include "starfox/render/calibrated_ground.hpp"
#include "starfox/render/calibrated_game_scene_fx.hpp"
#include "starfox/render/camera_response.hpp"
#include "starfox/render/environment_effects.hpp"
#include "starfox/vr/source_models.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/vr/enhanced_landscape.hpp"
#include "starfox/vr/scene_packet_validation.hpp"
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>

namespace starfox::render {
namespace {
constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
bool empty(const vr::DrawPacket& p) {return p.geometry.vertex_view().empty() && p.geometry.line_view().empty();}
bool photograph(const vr::DrawPacket& p) {
    const auto vertices=p.geometry.vertex_view();
    return !vertices.empty() && (vertices.front().texture[3]&vr::backdrop_texture_flag)==vr::backdrop_texture_flag;
}
}
DisplayXrRigSettings calibrated_game_rig(float convergence) {
    if(!std::isfinite(convergence) || convergence<16 || convergence>65535)
        throw std::invalid_argument("Invalid calibrated game convergence");
    DisplayXrRigSettings rig;
    rig.meters_to_virtual=1;rig.convergence=convergence/256.F;rig.near_plane=1.F/256.F;
    return rig;
}
std::optional<std::array<double,3>> calibrated_game_light(const vr::EyeCamera& camera,
    std::array<double,3> source,std::array<double,3> angles) {
    double length=0;
    for(double value:source) {if(!std::isfinite(value) || std::abs(value)>1.e6) return {};length+=value*value;}
    if(length<1.e-12) return {};
    const auto response=camera_response_world_transform({angles[0],angles[1],angles[2]});
    if(!response) return {};
    const auto rotated=vr::model_eye_camera(camera,*response);if(!rotated) return {};
    const double native[]{source[0],-source[1],-source[2]};std::array<double,3> result{};
    for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) result[r]+=rotated->view[c*4+r]*native[c];
    result[1]=-result[1];result[2]=-result[2];return result;
}
std::vector<CalibratedScenePacket> CalibratedGameFrame::packets() const {
    const auto& angles=settings.camera_response_pose;
    const auto response=camera_response_world_transform({angles[0],angles[1],angles[2]});
    if(!response || (settings.camera_response_modes&~63U)) throw std::invalid_argument("Invalid calibrated camera response");
    const bool moving=angles!=std::array<double,3>{};
    std::vector<CalibratedScenePacket> result;result.reserve(draws.size());
    for(const auto& draw:draws) {
        CalibratedScenePacket p{&draw.packet,draw.blend,draw.depth_test};
        p.effects_override=draw.layer==CalibratedGameLayer::world?settings.world_effects:
            draw.layer==CalibratedGameLayer::model?settings.model_effects:std::array<unsigned,4>{};
        if(draw.layer==CalibratedGameLayer::screen) p.camera_override=vr::EyeCamera{identity,identity};
        p.ray_caster=draw.ray_caster;p.after_rays=draw.after_rays;
        // Rotate native world geometry once, before both eye views. This also
        // reaches native ray geometry and environment captures through the
        // same retained model binding. Emissive world models move too; only
        // native UI/screen artwork and the post-world boundary are protected.
        if(moving && !draw.after_rays && (draw.layer==CalibratedGameLayer::world || draw.layer==CalibratedGameLayer::model)) {
            const auto transformed=vr::model_eye_camera(vr::EyeCamera{*response,identity},draw.packet.model);
            if(!transformed) throw std::invalid_argument("Invalid calibrated camera-response model");
            p.model_override=transformed->view;
        }
        p.reflection_environment=draw.reflection_environment;
        p.reflective_material=draw.ray_caster && settings.ray_tracing && settings.reflections
            && reflective_material(static_cast<Effect>(settings.material));
        const auto ground=calibrated_ground_material(draw.packet);
        p.ground_receiver=calibrated_ground_ray_surface(ground) && draw.layer==CalibratedGameLayer::world
            && !draw.after_rays && settings.ray_tracing && (ground==5 || ground==9 || settings.reflections);
        if(!draw.after_rays && !draw.packet.preserve_native_colour)
            p.effect_layer=draw.layer==CalibratedGameLayer::world?1:draw.layer==CalibratedGameLayer::model && draw.ray_caster?2:0;
        result.push_back(p);
    }
    return result;
}
vr::Matrix4 CalibratedGameFrame::scene_fx_rig() const {
    const auto& angles=settings.camera_response_pose;
    const auto response=camera_response_world_transform({angles[0],angles[1],angles[2]});
    if(!response) throw std::invalid_argument("Invalid native scene-effect camera response");
    const auto transformed=vr::model_eye_camera({*response,identity},scene_fx_source_to_rig);
    if(!transformed) throw std::invalid_argument("Invalid native scene-effect source view");
    return transformed->view;
}
void append_calibrated_host_ui(CalibratedGameFrame& frame,const Framebuffer& bitmap,
    std::span<const Rgba8> palette,unsigned brightness,std::optional<std::array<int,4>> dim_rect) {
    if(!bitmap.width() || !bitmap.height() || bitmap.width()>1024 || bitmap.height()>1024
        || brightness>15 || palette.size()!=256) throw std::invalid_argument("Invalid calibrated host UI");
    const auto rig=calibrated_game_rig(frame.settings.convergence_source_units);
    const auto transform=vr::source_layer_matrix(float(bitmap.width())*.5F,float(bitmap.height())*.5F,rig.convergence).value();
    const auto add_rectangle=[](vr::DrawPacket& p,int left,int top,int right,int bottom,const std::array<float,4>& color) {
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            vr::SceneVertex v{};v.position[0]=float((corner==1 || corner==2)?right:left);
            v.position[1]=float(corner>=2?bottom:top);
            std::copy(color.begin(),color.end(),v.color);std::copy(color.begin(),color.end(),v.odd_color);
            p.geometry.vertices.push_back(v);
        }
    };
    if(dim_rect) {
        const auto r=*dim_rect;
        if(r[0]<0 || r[1]<0 || r[2]>int(bitmap.width()) || r[3]>int(bitmap.height()) || r[0]>=r[2] || r[1]>=r[3])
            throw std::invalid_argument("Invalid calibrated host UI dim rectangle");
        vr::DrawPacket p;p.model=transform;p.preserve_native_colour=true;
        add_rectangle(p,r[0],r[1],r[2],r[3],{0,0,0,.75F});
        frame.draws.push_back({std::move(p),CalibratedGameLayer::native_ui,vr::SceneBlend::alpha,false,0,false,true});
    }
    vr::DrawPacket p;p.model=transform;p.preserve_native_colour=true;
    const auto channel=[&](unsigned value) {
        const float encoded=float(value*brightness/15)/255.F;
        return !frame.settings.srgb?encoded:encoded<=.04045F?encoded/12.92F:std::pow((encoded+.055F)/1.055F,2.4F);
    };
    // Coalesce equal indexed ink horizontally. Only menu/font artwork is CPU
    // authored, just as in the desktop frontend; no world frame is reprojected.
    for(unsigned y=0;y<bitmap.height();++y) for(unsigned x=0;x<bitmap.width();) {
        const auto ink=bitmap.get(int(x),int(y));const auto left=x++;
        while(x<bitmap.width() && bitmap.get(int(x),int(y))==ink) ++x;
        if(!ink) continue;
        const auto c=palette[ink];
        add_rectangle(p,int(left),int(y),int(x),int(y+1),{channel(c.r),channel(c.g),channel(c.b),1});
    }
    if(!p.geometry.vertices.empty()) frame.draws.push_back({std::move(p),CalibratedGameLayer::native_ui,vr::SceneBlend::opaque,false,0,false,true});
}
struct CalibratedGameScene::State {
    const assets::RomImage& rom;const assets::SymbolMap& symbols;
    vr::SourceModels models;vr::EnhancedLandscape landscape;ScaledTextRenderer text;
    std::unordered_set<std::uint16_t> emissive;
    std::unordered_map<std::uint16_t,std::string_view> ground_names;
    std::shared_ptr<const vr::GameSceneSnapshot> ground_source;
    std::array<std::uint8_t,256> ground_regions{};
    SceneFxTracker scene_fx;
    std::shared_ptr<const vr::GameSceneSnapshot> fx_source;
    std::uint64_t fx_host_epoch{},fx_epoch{};
    void capture_fx(CalibratedGameFrame& frame,const simulation::GameSimulation& game) {
        const auto& scene=*frame.current;const auto& settings=frame.settings;
        // No emitter scan, camera interpolation, weather lookup or tracker copy
        // on the unenhanced path. Clear once on OFF/menu entry, preserving the
        // tracker's monotonic birth sequence for a later re-enable.
        if(!(settings.scene_enhancements|settings.particle_enhancements) || !calibrated_scene_fx_flow(scene.flow)) {
            if(fx_source) {
                (void)scene_fx.update_world(0,settings.effect_seconds,fx_epoch,{}, {},0);
                fx_source.reset();
            }
            return;
        }
        const auto inputs=calibrated_scene_fx_inputs(frame,models.source_interpolation_rules(false),
            [&](unsigned shape){return emissive.contains(std::uint16_t(shape));},
            [&](std::uint32_t strategy){return models.source_centre_scale(scene,strategy,false);});
        const bool reset=!fx_source || fx_host_epoch!=settings.history_epoch || fx_source->scene_epoch!=scene.scene_epoch
            || fx_source->flow!=scene.flow || timing::camera_transform_is_discontinuous(fx_source->camera,scene.camera);
        const auto epoch=fx_epoch+unsigned(reset);
        unsigned weather=0;
        const bool active=calibrated_scene_fx_flow(scene.flow);
        const bool space=scene.dots_mode<0 || scene.background_star_sphere || scene.background_space_horizon || scene.background_unique_space;
        const auto named=ground_names.find(scene.background_id);const auto name=named==ground_names.end()?std::string_view{}:named->second;
        if(active && !space && !scene.ppu->tunnel_scene && scene.flow!=simulation::GameFlowState::intro && (settings.scene_enhancements>>6)) {
            if(name=="BG_2_3A") {
                const auto regions=environment_palette_regions(*scene.ppu,scene.landscape_atlas_origin);
                unsigned snow=0,dirt=0;
                for(unsigned ink=1;ink<256;++ink) if(regions[ink]==1 && (scene.cgram[ink]&0x7fff)) {
                    if(titania_ground_material(scene.cgram[ink])==4) ++snow;else ++dirt;
                }
                if(snow>dirt) weather=1;
            }
            if(game.experience()==simulation::Experience::starfox_ex) {
                if(name=="BG_6_4") weather=2;
                if(name=="BG_6_5" || name=="BG_6_6") weather=3;
            }
        }
        // Capture publication is transactional. A failed model/background/
        // packet or malformed effect frame cannot consume a source event.
        auto next=scene_fx;
        frame.scene_fx=next.update_world(active?settings.scene_enhancements:0,settings.effect_seconds,epoch,
            inputs.emitters,inputs.origin,weather,active?settings.particle_enhancements:0,space?0.F:400.F);
        if(!valid_scene_fx_world_frame(frame.scene_fx)) throw std::invalid_argument("Invalid captured native scene effects");
        frame.scene_fx_origin=inputs.origin;frame.scene_fx_source_to_rig=inputs.source_to_rig;
        scene_fx=std::move(next);fx_source=frame.current;fx_host_epoch=settings.history_epoch;fx_epoch=epoch;
    }
    State(const assets::RomImage& r,const assets::SymbolMap& s)
        :rom(r),symbols(s),models(r,s,true,false,false),landscape(s),text(r,s) {
        // Exact source shape headers, matching the flat renderer's policy.
        // Do not match similarly named face/vertex symbols in another bank.
        for(const auto* name:{"LASERLINE","LASER_0","ELASER2","ELASER2_S2","ELASER2A",
                "PLAYERBEAM","RINGLASER","OVALBEAM","BOSS_8_0"})
            for(const auto address:s.find(name)) if(address>=0x8000U && address<=0xffffU)
                emissive.insert(static_cast<std::uint16_t>(address));
        const auto lists=s.find("BGLISTS");
        if(!lists.empty()) for(const auto name:{"BG_2_3A","BG_3_5","BG_7_2","BG_5_5","BG_6_4","BG_6_5","BG_6_6",
                "BG_5_1","BG_6_1","BG_7_1"}) {
            const auto values=s.find(name);
            if(!values.empty() && values.front()>=lists.front() && values.front()-lists.front()<=65535
                && (values.front()&0xff0000U)==(lists.front()&0xff0000U))
                ground_names.emplace(std::uint16_t(values.front()-lists.front()),name);
        }
    }
    bool enhance_ground(vr::DrawPacket& packet,const std::shared_ptr<const vr::GameSceneSnapshot>& source,
        const simulation::GameSimulation& game,unsigned material,unsigned motion,float seconds,
        const timing::RenderTransform& camera) {
        const auto& scene=*source;const auto& ppu=*scene.ppu;
        const auto named=ground_names.find(scene.background_id);
        const std::string_view name=named==ground_names.end()?"":named->second;
        const bool ex=game.experience()==simulation::Experience::starfox_ex;
        // In particular a shared BG_6_6 alias must not enable lava on Venom.
        if(!material && ex) {
            const auto stage=symbols.find("NEWMAP"),lava=symbols.find("LEVEL6_6");
            if(!stage.empty() && !lava.empty()) {
                const auto launched=std::uint32_t(game.map().read_native_word(stage.front()))
                    |(std::uint32_t(game.map().read_native_byte(stage.front()+2))<<16);
                if(launched==lava.front()) material=9;
            }
        }
        if(!material && (name=="BG_3_5" || name=="BG_5_5")) material=8;
        const auto same=[&] {
            if(!ground_source || ground_source->background_id!=scene.background_id
                || ground_source->landscape_atlas_origin!=scene.landscape_atlas_origin) return false;
            const auto& old=*ground_source->ppu;
            return old.bg2_screen_base==ppu.bg2_screen_base && old.bg2_screen_size==ppu.bg2_screen_size
                && old.bg2_character_base==ppu.bg2_character_base && old.bg2_tile_size_16==ppu.bg2_tile_size_16
                && old.vram==ppu.vram;
        };
        if(!same()) {
            ground_regions=environment_palette_regions(ppu,scene.landscape_atlas_origin);
            if(ex) correct_ex_landscape_palette(name,ground_regions);
            ground_source=source;
        }
        std::array<std::uint32_t,256> classes{};
        const bool palette_fading_ground=ex && (name=="BG_5_1" || name=="BG_6_1" || name=="BG_7_1");
        for(unsigned ink=1;ink<256;++ink) if(ground_regions[ink]==1)
            classes[ink]=palette_fading_ground?4:name=="BG_7_2"?3:name=="BG_2_3A"?titania_ground_material(ppu.cgram[ink])
                :automatic_ground_material(ppu.cgram[ink]);
        // Retain automatic water as a real liquid receiver, not a basic floor.
        if(!material) {
            std::array<unsigned,6> population{};
            for(unsigned ink=1;ink<256;++ink) if(classes[ink] && (ppu.cgram[ink]&0x7fff)) ++population[classes[ink]];
            if(population[5] && std::max_element(population.begin()+1,population.end())==population.begin()+5) material=5;
        } else for(auto& kind:classes) if(kind) kind=1;
        const auto endpoints=source_ground_gradient(ppu.cgram,classes,true);
        if(!endpoints[1][3]) return false;
        EnvironmentEffects effects;effects.modes[0]=material+1;
        CalibratedGroundGradient gradient;
        gradient.material=material>=5?material:0;
        gradient.origin={float(camera.x),float(camera.z)};
        gradient.seconds=seconds;gradient.motion=motion;
        gradient.brightness=float(scene.display_brightness)/15.F;
        for(unsigned end=0;end<2;++end) {
            const auto& raw=endpoints[end];
            // Lava radiance is evaluated on the native GPU receiver, not once
            // on the CPU and stretched across its entire floor. Retain the
            // live original ramp for its tightly bounded horizon seam.
            const float light=raw[0]*.3F+raw[1]*.59F+raw[2]*.11F;
            const auto colour=material==9?std::array<float,3>{raw[0],raw[1],raw[2]}:
                material==8?std::array<float,3>{light*1.27F,light*.58F,light*.43F}:
                // Water's live ramp is metadata, not one CPU wave sample
                // stretched over both eyes. Ripples run at each native point;
                // the RT finish separately owns physical optical transport.
                material==5?std::array<float,3>{light*.46F,light*.95F,light*1.50F}:
                environment_colour({raw[0],raw[1],raw[2]},unsigned(endpoints[0][3]),0,224,effects);
            auto& result=end?gradient.near:gradient.far;
            for(unsigned c=0;c<3;++c) result[c]=std::clamp(colour[c],0.F,255.F)/255.F*float(scene.display_brightness)/15.F;
        }
        return apply_calibrated_ground(packet,gradient);
    }
};
CalibratedGameScene::CalibratedGameScene(const assets::RomImage& r,const assets::SymbolMap& s)
    :state_(std::make_unique<State>(r,s)) {}
CalibratedGameScene::~CalibratedGameScene()=default;
CalibratedGameFrame CalibratedGameScene::assemble(std::shared_ptr<const vr::GameSceneSnapshot> before,
    std::shared_ptr<const vr::GameSceneSnapshot> current,const simulation::GameSimulation& game,
    double alpha,const CalibratedGameSettings& settings,const CalibratedBackdropLoader& loader) {
    if(!before || !current || !before->ppu || !current->ppu || !std::isfinite(alpha) || alpha<0 || alpha>1)
        throw std::invalid_argument("Missing/invalid calibrated game snapshots");
    const auto rig=calibrated_game_rig(settings.convergence_source_units);
    if(settings.world_effects[0]>=effect_count || settings.model_effects[0]>=effect_count
        || settings.world_effects[1]>100 || settings.model_effects[1]>100 || settings.manipulation_intensity>100
        || !valid_manipulation(settings.manipulation) || !valid_manipulation(settings.extra_effects[0])
        || !valid_special_fx(settings.extra_effects[1]) || !valid_special_fx(settings.extra_effects[2])
        || !std::isfinite(settings.effect_seconds) || settings.effect_seconds<0 || settings.phosphor>3
        || settings.bloom_model>3 || settings.bloom_world>3 || settings.contrast>3 || settings.chromatic>3
        || (settings.global_enhancements&~global_enhancement_mask)!=0 || settings.ground_material>9 || settings.ground_motion>3
        || settings.depth_enhancements>15 || settings.volumetric_fog>3 || settings.motion_blur>3 || settings.scene_enhancements>255 || settings.particle_enhancements>15)
        throw std::invalid_argument("Invalid calibrated game effect intensity");
    if(settings.ray_tracing>3 || settings.reflections>3 || settings.shadow_softness>3 || settings.water_caustics>3)
        throw std::invalid_argument("Invalid calibrated ray quality");
    if(!valid_material(settings.material)) throw std::invalid_argument("Invalid calibrated model material");
    if(settings.fsr1_mode>4 || settings.dlss_mode>4 || settings.dlss_model>1 || (settings.fsr1_mode && settings.dlss_mode))
        throw std::invalid_argument("Invalid calibrated upscaler selection");
    // Cross-scene jumps, pauses and resumed/rebased clocks cannot replay an old
    // geometry/PPU state into the new frame. Both eyes retain this choice.
    if(before->scene_epoch!=current->scene_epoch || before->flow!=current->flow || current->paused
        || timing::camera_transform_is_discontinuous(before->camera,current->camera)) before=current;
    CalibratedGameFrame frame{before,current,{}, {},settings,alpha};
    const auto& scene=*current;const auto& previous=*before;const auto& ppu=*scene.ppu;
    auto world=state_->models.assemble_world_interpolated(previous,scene,alpha,settings.srgb,false);
    if(!world.pending.empty()) throw std::runtime_error("Calibrated source model "+
        std::to_string(world.pending.front().handle)+": "+world.pending.front().reason);
    if(!world.compute_models.empty() || world.handles.size()!=world.packets.size())
        throw std::runtime_error("Unassembled calibrated game compute pass");
    const bool controls=scene.flow==simulation::GameFlowState::controls_type
        || scene.flow==simulation::GameFlowState::controls_choice;
    frame.clear=controls || scene.flow==simulation::GameFlowState::continue_choice
        ?vr::source_menu_background_colour(ppu,scene.display_brightness,settings.srgb)
        :scene.flow==simulation::GameFlowState::game_over || ppu.tunnel_scene
            || scene.background_unique_top_rows || scene.background_star_sphere || scene.background_space_horizon
        ?vr::source_background_border_colour(scene.cgram,scene.display_brightness,settings.srgb)
        :vr::source_backdrop_colour(scene.cgram[0],scene.display_brightness,settings.srgb);
    bool after_rays=false;
    const auto append=[&](vr::DrawPacket p,CalibratedGameLayer layer,vr::SceneBlend blend=vr::SceneBlend::opaque,
                          bool depth=false,std::uint32_t key=0) {
        if(empty(p)) return;
        bool caster=false;
        if(layer==CalibratedGameLayer::model && key<=0xffffU && !p.preserve_native_colour && !after_rays) {
            const auto object=std::find_if(scene.objects.begin(),scene.objects.end(),
                [key](const auto& item){return item.handle==key;});
            if(object==scene.objects.end()) throw std::runtime_error("Calibrated model lacks source caster identity");
            caster=!state_->emissive.contains(object->object.shape);
        }
        frame.draws.push_back({std::move(p),layer,blend,depth,key,caster,after_rays,
            layer==CalibratedGameLayer::world && !after_rays});
    };
    const auto dust=[&] {
        for(std::size_t i=0;i<world.packets.size();++i) if(world.handles[i]==0x20000U)
            append(world.packets[i],CalibratedGameLayer::native_ui);
    };
    if(!controls) dust();
    const bool fixed_menu=scene.flow==simulation::GameFlowState::ex_pregame_menu;
    const float vx=fixed_menu?112.F:float(scene.source_vanishing_point[0]);
    const float vy=fixed_menu?96.F:float(scene.source_vanishing_point[1]);
    const auto ui=vr::source_layer_matrix(vx+16,vy+16,rig.convergence).value();
    const auto meter=vr::source_layer_matrix(vx,vy,rig.convergence).value();
    vr::BackgroundTileOptions options;
    options.brightness=scene.display_brightness;options.colour_subtract=scene.background_colour_subtract;
    options.scroll_override=scene.background_scroll_override;
    options.single_occurrence_top_rows=scene.background_unique_top_rows;
    options.ex_twin_planets=scene.background_ex_twin_planets;options.ex_face_planets=scene.background_ex_face_planets;
    options.ex_ocean_island=scene.background_ex_ocean_island;options.ex_volcanic_horizon=scene.background_ex_volcanic_horizon;
    options.ex_city_planets=scene.background_ex_city_planets;options.transparent_black=scene.background_unique_top_rows!=0;
    if(ppu.background_mode==2) {options.expanded_horizontal=true;options.horizontal_bounds={-384,640};}
    const bool intro_sky=!ppu.tunnel_scene && scene.flow==simulation::GameFlowState::intro
        && !scene.meters.extended && scene.background_unique_top_rows==224;
    std::vector<vr::DrawPacket> backgrounds;
    if(intro_sky) backgrounds.push_back(vr::intro_star_sphere_packet(ppu,scene.display_brightness,settings.srgb));
    if(!ppu.tunnel_scene && scene.background_unique_space)
        backgrounds.push_back(vr::intro_star_sphere_packet(ppu,scene.display_brightness,settings.srgb,false,true));
    if(ppu.background_mode>=1 && ppu.background_mode<=2) {
        auto bg2=vr::background_tile_packet(ppu,vr::BackgroundLayer::bg2,options,settings.srgb);
        if(intro_sky) bg2=vr::intro_planet_packet(ppu,scene.display_brightness,settings.srgb);
        else if(!ppu.tunnel_scene && scene.background_unique_space)
            bg2=vr::unique_planet_packet(ppu,options,scene.background_planet_rect,settings.srgb);
        else if(!ppu.tunnel_scene && scene.background_star_sphere) {
            bg2=vr::intro_star_sphere_packet(ppu,scene.display_brightness,settings.srgb,true,false,scene.background_retain_sky_scroll);
            if(scene.background_ex_face_planets && !bg2.geometry.texels.empty()) bg2.geometry.texels[7]|=256U;
        } else if(!ppu.tunnel_scene && scene.background_space_horizon)
            bg2=scene.background_orbital_planet?vr::orbital_planet_sphere_packet(ppu,options,settings.srgb,
                scene.background_orbital_thin,scene.background_orbital_entry,
                scene.flow==simulation::GameFlowState::gameplay && game.experience()==simulation::Experience::starfox_ex)
                :vr::space_horizon_sphere_packet(ppu,options,settings.srgb);
        else if(!ppu.tunnel_scene && scene.background_landscape) {
            bg2=vr::landscape_sphere_packet(ppu,options,112.F,settings.srgb,scene.background_landscape_unique_half,
                scene.landscape_atlas_origin,scene.background_landscape_unique_right_half);
            if(scene.landscape_grid_height<0)
                vr::place_landscape_ground(bg2,std::clamp(float(scene.landscape_grid_height)/256.F,-8.F,-.001F),true);
        } else if(scene.background_water_surround)
            bg2=vr::water_surface_packet(ppu,options,std::clamp(std::abs(float(scene.camera.y-scene.shadow_height))/256.F,.01F,8.F),settings.srgb);
        else bg2.model=ui;
        std::optional<vr::DrawPacket> photo;
        if(settings.enhanced_sky) {
            if(!loader) throw std::invalid_argument("Enhanced calibrated sky has no asset loader");
            photo=state_->landscape.prepare(scene,game,loader,settings.srgb);
        }
        const bool enhanced=photo.has_value();
        if(enhanced) state_->landscape.retain_native_ground(bg2,ppu);
        if(settings.enhanced_ground && !fixed_menu && !ppu.tunnel_scene && scene.background_landscape
            && scene.landscape_grid_height<0)
            state_->enhance_ground(bg2,current,game,settings.ground_material,settings.ground_motion,
                settings.effect_seconds,timing::interpolate(previous.camera,scene.camera,alpha));
        if(!enhanced && scene.flow==simulation::GameFlowState::game_over)
            backgrounds.push_back(vr::game_over_star_sphere_packet(ppu,scene.display_brightness,scene.background_colour_subtract,settings.srgb));
        if(photo && scene.flow==simulation::GameFlowState::game_over) {backgrounds.push_back(*photo);photo.reset();}
        const auto bg2_index=backgrounds.size();backgrounds.push_back(std::move(bg2));
        if(photo) {
            backgrounds.push_back(*photo);
            for(const auto& body:state_->landscape.bodies()) backgrounds.push_back(body);
        }
        if(!enhanced && !ppu.tunnel_scene && scene.background_ex_city_planets) {
            auto planet=options;planet.scroll_override=std::array<int16_t,2>{0,248};
            backgrounds.push_back(vr::unique_planet_packet(ppu,planet,{384,208,56,48},settings.srgb));
        }
        if(!enhanced && !ppu.tunnel_scene && scene.background_orbital_entry) {
            auto planet=options;planet.scroll_override=std::array<int16_t,2>{0,312};
            backgrounds.push_back(vr::unique_planet_packet(ppu,planet,{336,320,56,64},settings.srgb));
        }
        const bool continuous=previous.flow==scene.flow && previous.background_id==scene.background_id;
        const auto scroll=[](const auto& s) {return s.background_scroll_override?float((*s.background_scroll_override)[1]):float(s.ppu->bg2_scroll_y);};
        for(std::size_t i=0;i<backgrounds.size();++i) {
            auto& p=backgrounds[i];
            if(scene.background_orbital_planet && !ppu.tunnel_scene) {
                const bool valid=continuous && previous.background_orbital_planet;
                if(i==bg2_index || (enhanced && state_->landscape.orbital_body(p)))
                    p.model=vr::orbital_horizon_motion(scroll(valid?previous:scene),scroll(scene),valid?alpha:1.,
                        scene.background_orbital_thin,scene.background_orbital_entry,
                        i==bg2_index && scene.flow==simulation::GameFlowState::gameplay && game.experience()==simulation::Experience::starfox_ex);
            }
            if(scene.background_landscape && (i==bg2_index || scene.background_ex_city_planets
                || (enhanced && (state_->landscape.landscape_body(p) || photograph(p)))))
                p.model=vr::landscape_camera_motion(previous,scene,vr::same_landscape_mapping(previous,scene)?alpha:1.);
            if(scene.background_water_surround) {
                const auto data=p.geometry.texel_view();
                const auto height=[](const auto& s) {return std::clamp(std::abs(float(s.camera.y-s.shadow_height))/256.F,.01F,8.F);};
                if(data.size()>15 && (data[15]&32U)) p.model=vr::water_height_motion(
                    height(continuous && previous.background_water_surround?previous:scene),height(scene),continuous?alpha:1.);
            }
            if(enhanced && state_->landscape.scrolling_pattern() && state_->landscape.pattern_panorama(p))
                p.model=state_->landscape.pattern_motion(previous,scene,alpha);
            if(enhanced) if(const auto body=state_->landscape.tracked_body(p))
                p.model=state_->landscape.body_motion(previous,scene,alpha,*body);
            if(intro_sky && i==bg2_index) p.model=vr::intro_planet_motion(
                std::int16_t(scroll(continuous?previous:scene)),std::int16_t(scroll(scene)),continuous?alpha:1.,vy+16);
        }
        if(ppu.background_mode==1) {
            options.scroll_override.reset();
            auto bg3=scene.background_water_surround?vr::water_surround_packet(ppu,options,settings.srgb)
                :vr::background_tile_packet(ppu,vr::BackgroundLayer::bg3,options,settings.srgb);
            if(!scene.background_water_surround) bg3.model=ui;
            if(scene.background_water_surround && !backgrounds.empty()) backgrounds.insert(backgrounds.end()-1,std::move(bg3));
            else backgrounds.push_back(std::move(bg3));
        }
    }
    for(auto& p:backgrounds) {
        const auto blend=photograph(p)?vr::SceneBlend::alpha:vr::SceneBlend::opaque;
        const auto key=calibrated_ground_motion_source(p)?calibrated_ground_source_key:0U;
        append(std::move(p),CalibratedGameLayer::world,blend,false,key);
    }
    if(ppu.tunnel_scene) {
        const auto color=[&](unsigned y,unsigned x) {return vr::source_backdrop_colour(ppu.cgram[tunnel_border_index(ppu,y,x)],scene.display_brightness,settings.srgb);};
        auto surround=vr::tunnel_surround_packet(color(112,0),color(4,128),color(219,128));surround.model=ui;
        append(std::move(surround),CalibratedGameLayer::world);
    }
    if(controls) dust();
    for(std::size_t i=0;i<world.packets.size();++i) if(world.handles[i]!=0x20000U) {
        auto& p=world.packets[i];
        if(vr::is_source_shadow_pass(world.handles[i])) p.preserve_native_colour=true;
        append(std::move(p),world.handles[i]==0x30000U?CalibratedGameLayer::world:CalibratedGameLayer::model,
            vr::SceneBlend::opaque,true,world.handles[i]);
    }
    // An ordered boundary, not a sort by layer: early dust deliberately stays
    // before backgrounds. Native circle effects, sprites, portraits, meters,
    // text and the final screen shutter are all composed after traced surfaces.
    after_rays=true;
    auto circle=vr::source_circle_packet(previous.circle,scene.circle,alpha,scene.display_brightness,settings.srgb);
    circle.model=ui;
    const auto circle_flags=scene.circle.affected_layers;
    const auto circle_blend=(circle_flags&0x80)?((circle_flags&0x40)?vr::SceneBlend::half_subtract:vr::SceneBlend::subtract)
        :((circle_flags&0x40)?vr::SceneBlend::half_add:vr::SceneBlend::add);
    if(!(circle_flags&0x10)) append(circle,CalibratedGameLayer::native_ui,circle_blend);
    const auto append_ui=[&](vr::DrawPacket p) {p.model=ui;p.preserve_native_colour=true;append(std::move(p),CalibratedGameLayer::native_ui);};
    if(scene.flow==simulation::GameFlowState::title)
        for(auto& p:vr::title_foreground_packets(ppu,scene.display_brightness,!scene.ex_title_logo_screen,scene.background_scroll_override,settings.srgb)) append_ui(std::move(p));
    const bool dialogue=vr::replace_native_dialogue(scene);
    if(ppu.background_mode==3) {
        for(auto& p:vr::source_mode3_packets(ppu,scene.display_brightness,settings.srgb,scene.background_scroll_override)) append_ui(std::move(p));
    } else {
        if(scene.native_ex_bitmap && !dialogue) {
            vr::BackgroundTileOptions bitmap;bitmap.brightness=scene.display_brightness;bitmap.guard_inset=16;bitmap.transparent_black=true;
            append_ui(vr::background_tile_packet(ppu,vr::BackgroundLayer::bg1,bitmap,settings.srgb));
        }
        append_ui(vr::source_sprite_packet(ppu,scene.display_brightness,{},settings.srgb,&scene.meters));
        auto meters=vr::source_meter_packet(scene.meters,ppu.cgram,scene.display_brightness,settings.srgb);
        meters.model=meter;meters.preserve_native_colour=true;append(std::move(meters),CalibratedGameLayer::native_ui);
    }
    if(dialogue) {
        state_->text.set_language(settings.language);
        for(auto& p:vr::source_dialogue_packets(state_->rom,state_->symbols,scene.dialogue,state_->text,ppu.cgram,scene.display_brightness,settings.srgb)) append_ui(std::move(p));
    }
    if(scene.briefing.active) {
        const auto text=[&](std::uint32_t address,int x,int y,int right,std::size_t count,std::uint8_t ink) {
            append_ui(vr::source_game_text_packet(state_->rom,state_->symbols,address,x,y,right,count,ink,ppu.cgram,scene.display_brightness,settings.srgb));
        };
        const auto& brief=scene.briefing;
        text(brief.message_address,30,173,218,brief.visible_message_characters,101);
        text(brief.message_address,28,171,216,brief.visible_message_characters,109);
        text(brief.planet_name_address,30,41,224,brief.visible_planet_characters,97);
        text(brief.planet_name_address,28,39,224,brief.visible_planet_characters,100);
    }
    if(circle_flags&0x10) append(std::move(circle),CalibratedGameLayer::native_ui,circle_blend);
    append(vr::source_shutter_packet(previous.wipe,scene.wipe,alpha),CalibratedGameLayer::screen);
    vr::ScenePacketValidator validator(true,true);
    for(const auto& draw:frame.draws) validator.add(draw.packet);
    state_->capture_fx(frame,game);
    return frame;
}
}
