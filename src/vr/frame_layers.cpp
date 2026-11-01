// Transcribed from the PCVR frame loop (src/vr/application.cpp, sprite and
// background assembly plus background motion). Keep the two in step until
// the VR applications call these functions themselves.
#include "starfox/vr/frame_layers.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/enhanced_landscape.hpp"
#include "starfox/vr/game_model_pose.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/hud_layout.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/timing/fixed_step.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
namespace starfox::vr {
FrameLayers assemble_frame_layers(const GameSceneSnapshot& scene,const FrameLayerInputs& in) {
    if(!scene.ppu) throw std::runtime_error("Frame layers need a PPU snapshot");
    const auto* snapshot=&scene;
    const bool srgb=in.srgb;
    const bool enhanced_sky=in.enhanced_landscape && in.prepare_photograph;
    FrameLayers out;
    const bool fixed_menu=snapshot->flow==simulation::GameFlowState::ex_pregame_menu;
    // The PPU canvas placement shared by sprites and flat background layers.
    const auto ppu_layer_model=source_ui_layer_matrix(
        float(snapshot->source_vanishing_point[0]),
        float(snapshot->source_vanishing_point[1]),fixed_menu).value();
    DrawPacket packet,meters;
    if(!in.backgrounds_only) {
        // Edge HUD: the desktop's band classification, expressed as layout
        // offsets (lives/shield left, bombs/boost/boss right; comms stays).
        std::optional<render::HudLayout> edge_layout;
        if(in.hud_edge_shift && !fixed_menu) {
            edge_layout.emplace();
            const auto shift=int16_t(in.hud_edge_shift);
            (*edge_layout)[render::HudElement::lives].x=int16_t(-shift);
            (*edge_layout)[render::HudElement::shield].x=int16_t(-shift);
            (*edge_layout)[render::HudElement::bombs_boost].x=shift;
            (*edge_layout)[render::HudElement::boss_health].x=shift;
        }
        const bool vector_reticle=in.vector_reticle && !source_vector_reticle_packet(*snapshot->ppu,*snapshot->ppu,1.,
            snapshot->display_brightness,srgb).geometry.vertex_view().empty();
        packet=source_sprite_packet(*snapshot->ppu,snapshot->display_brightness,{},srgb,&snapshot->meters,
            edge_layout?&*edge_layout:nullptr,vector_reticle?SourceSpritePass::all_but_reticle:SourceSpritePass::all,
            in.sprite_overrides);
        meters=source_meter_packet(snapshot->meters,snapshot->ppu->cgram,snapshot->display_brightness,srgb);
        if(edge_layout) {
            // Meter rectangles (six vertices each, inner 224-wide coordinates)
            // follow the same sides; shifting after the builder keeps its
            // 0..256 clip from cutting the moved bars.
            auto& vertices=meters.geometry.vertices;
            for(std::size_t first=0;first+6<=vertices.size();first+=6) {
                float left=vertices[first].position[0],right=left;
                for(std::size_t i=first;i<first+6;++i) {
                    left=std::min(left,vertices[i].position[0]);right=std::max(right,vertices[i].position[0]);
                }
                const float shift=(left+right)*.5F<112.F?-float(in.hud_edge_shift):float(in.hud_edge_shift);
                for(std::size_t i=first;i<first+6;++i) vertices[i].position[0]+=shift;
            }
        }
        // Meters use the inner 224x192 Super FX coordinates; OAM uses the
        // surrounding 256x224 PPU coordinates.
        meters.model=source_ui_layer_matrix(
            float(snapshot->source_vanishing_point[0]),
            float(snapshot->source_vanishing_point[1]),fixed_menu,true).value();
    }
    packet.model=ppu_layer_model;
    BackgroundTileOptions background_options;
    background_options.colour_subtract=snapshot->background_colour_subtract;
    background_options.brightness=snapshot->display_brightness;
    background_options.scroll_override=snapshot->background_scroll_override;
    background_options.single_occurrence_top_rows=snapshot->background_unique_top_rows;
    background_options.ex_twin_planets=snapshot->background_ex_twin_planets;
    background_options.ex_face_planets=snapshot->background_ex_face_planets;
    background_options.ex_ocean_island=snapshot->background_ex_ocean_island;
    background_options.ex_volcanic_horizon=snapshot->background_ex_volcanic_horizon;
    background_options.ex_city_planets=snapshot->background_ex_city_planets;
    // Space black is the transparent sky between authored planet pixels.
    background_options.transparent_black=snapshot->background_unique_top_rows!=0;
    // Mode-2 terrain is a world layer: extend logical coordinates.
    if(snapshot->ppu->background_mode==2 || (in.flat_tunnel_extension && snapshot->ppu->tunnel_scene)) {
        background_options.expanded_horizontal=true;
        background_options.horizontal_bounds={-384,640};
        background_options.clamp_tunnel_edges=in.flat_tunnel_extension;
    }
    auto& next_backgrounds=out.backgrounds;
    std::optional<DrawPacket> corridor;
    if(in.tunnel_corridor && snapshot->ppu->tunnel_scene && snapshot->ppu->background_mode>=1 && snapshot->ppu->background_mode<=2) {
        // The authored box when the scene knows it (MTUNNEL: x +-90).
        const auto& bounds=snapshot->background_corridor;
        const float half_width=bounds && bounds->right>bounds->left?float(int(bounds->right)-int(bounds->left))*.5F:90.F;
        // An enhancement: on any failure keep the cartridge's own layer
        // rather than failing (and dropping out of) the whole frame.
        try {
            corridor=tunnel_corridor_packet(*snapshot->ppu,snapshot->display_brightness,srgb,half_width);
            if(corridor->geometry.vertex_view().empty()) corridor.reset();
        } catch(const std::exception& error) {
            static bool reported=false;
            if(!reported) {std::fprintf(stderr,"modern: 3D corridor skipped: %s\n",error.what());reported=true;}
            corridor.reset();
        }
    }
    if(snapshot->ppu->tunnel_scene && !corridor && !in.flat_tunnel_extension) {
        const auto border=[&](unsigned y,unsigned x) {
            const auto ink=render::tunnel_border_index(*snapshot->ppu,y,x);
            return source_backdrop_colour(snapshot->ppu->cgram[ink],snapshot->display_brightness,srgb);
        };
        auto surround=tunnel_surround_packet(border(112,0),border(4,128),border(219,128));
        surround.model=source_layer_matrix(
            float(snapshot->source_vanishing_point[0])+16.F,
            float(snapshot->source_vanishing_point[1])+16.F).value();
        out.tunnel_surround.push_back(std::move(surround));
    }
    const bool intro_sky=!snapshot->ppu->tunnel_scene
        && snapshot->flow==simulation::GameFlowState::intro
        && !snapshot->meters.extended && snapshot->background_unique_top_rows==224;
    if(intro_sky)
        next_backgrounds.push_back(intro_star_sphere_packet(*snapshot->ppu,snapshot->display_brightness,srgb));
    if(!snapshot->ppu->tunnel_scene && snapshot->background_unique_space)
        next_backgrounds.push_back(intro_star_sphere_packet(*snapshot->ppu,snapshot->display_brightness,srgb,false,true));
    if(snapshot->ppu->background_mode>=1 && snapshot->ppu->background_mode<=2) {
        auto plain_options=background_options;
        plain_options.scroll_fraction=in.bg2_scroll_fraction;
        // (A 3D corridor replaces the native tunnel layer below.)
        if(snapshot->ppu->background_mode==2) plain_options.mode2_horizon=in.mode2_horizon;
        auto space_options=plain_options;
        if(in.space_roll_geometry && space_background_roll(*snapshot) && space_options.mode2_horizon) {
            const auto [intercept,slope]=*space_options.mode2_horizon;
            space_options.mode2_horizon=std::array<float,2>{intercept+slope*float(128+(snapshot->ppu->bg2_scroll_x&7))/8.F,0.F};
        }
        auto bg2=background_tile_packet(*snapshot->ppu,BackgroundLayer::bg2,plain_options,srgb);
        if(intro_sky)
            bg2=intro_planet_packet(*snapshot->ppu,snapshot->display_brightness,srgb);
        else if(!snapshot->ppu->tunnel_scene && snapshot->background_unique_space)
            bg2=unique_planet_packet(*snapshot->ppu,background_options,snapshot->background_planet_rect,srgb);
        else if(!snapshot->ppu->tunnel_scene && snapshot->background_star_sphere) {
            bg2=intro_star_sphere_packet(*snapshot->ppu,snapshot->display_brightness,srgb,true,false,snapshot->background_retain_sky_scroll,
                space_options.scroll_fraction,space_options.mode2_horizon);
            if(snapshot->background_ex_face_planets && !bg2.geometry.texels.empty()) bg2.geometry.texels[7]|=256U;
        }
        else if(!snapshot->ppu->tunnel_scene && snapshot->background_space_horizon && in.flat_space_horizon) {
            // Rolled as geometry: reach past the view so no corner shows the
            // backdrop (the airspace's planet surface below its limb).
            auto horizon_options=space_options;
            if(in.space_roll_geometry) horizon_options.overscan=512;
            bg2=background_tile_packet(*snapshot->ppu,BackgroundLayer::bg2,horizon_options,srgb);
            bg2.model=packet.model;
        }
        else if(!snapshot->ppu->tunnel_scene && snapshot->background_space_horizon)
            bg2=snapshot->background_orbital_planet
                ?orbital_planet_sphere_packet(*snapshot->ppu,background_options,srgb,snapshot->background_orbital_thin,snapshot->background_orbital_entry,
                    snapshot->flow==simulation::GameFlowState::gameplay && in.ex_experience)
                :space_horizon_sphere_packet(*snapshot->ppu,space_options,srgb);
        else if(!snapshot->ppu->tunnel_scene && snapshot->background_landscape && in.landscape_world) {
            // The cartridge's dots are a field about the camera, gone from
            // view once a stage-clear camera climbs away; the world-fixed
            // lattice would crowd into a dense grid. Fade it out between 200
            // and 800 units above the ground (flight stays near 50-150).
            std::optional<uint32_t> dots;
            if(in.landscape_ground_dots && snapshot->dots_mode>0 && !snapshot->grid_lines) {
                const double height=double(snapshot->shadow_height)-double(snapshot->camera.y);
                const double fade=std::clamp((800.-height)/600.,0.,1.);
                if(fade>0) dots=(landscape_dot_colour(*snapshot)&0xffffffU)|(uint32_t(std::lround(fade*255.))<<24);
            }
            bg2=landscape_world_packet(*snapshot->ppu,plain_options,float(snapshot->source_vanishing_point[1])+16.F,srgb,
                in.classic_ground_scroll,in.landscape_horizon,dots);
        }
        else if(!snapshot->ppu->tunnel_scene && snapshot->background_landscape && !in.flat_landscape) {
            bg2=landscape_sphere_packet(*snapshot->ppu,background_options,
                112.F,srgb,snapshot->background_landscape_unique_half,snapshot->landscape_atlas_origin,
                snapshot->background_landscape_unique_right_half);
            if(snapshot->landscape_grid_height<0)
                place_landscape_ground(bg2,std::clamp(float(snapshot->landscape_grid_height)/256.F,-8.F,-.001F),true);
        }
        else if(snapshot->background_water_surround)
            bg2=water_surface_packet(*snapshot->ppu,background_options,
                std::clamp(std::abs(float(snapshot->camera.y-snapshot->shadow_height))/256.F,.01F,8.F),srgb);
        else bg2.model=packet.model;
        if(corridor) bg2=std::move(*corridor);
        // Space Armada's boss room as a room around the reactor: its ceiling
        // and floor (drawn first) and the drum wall over their edges.
        std::optional<DrawPacket> nucleus_floor;
        if(in.tunnel_corridor && snapshot->background_nucleus && snapshot->ppu->background_mode==2) {
            const auto& room=*snapshot->background_nucleus;
            const float offset=float(int16_t(uint16_t(room.centre_z-snapshot->camera.z)));
            try {
                auto packet=nucleus_room_packet(*snapshot->ppu,snapshot->display_brightness,srgb,
                    float(room.radius),float(room.top),float(room.bottom),float(snapshot->camera.y),offset);
                if(!packet.geometry.vertex_view().empty()) {
                    // About the reactor (behind the ring's centre), turned with
                    // its spinning cover (BOSS_8_1C); model Z is minus world Z.
                    float reactor_x=0,reactor_z=0;double turn=0;
                    for(const auto& item:snapshot->objects) if(item.object.shape==room.reactor_shape) {
                        reactor_x=float(item.object.world_x);
                        reactor_z=-float(int16_t(uint16_t(item.object.world_z-room.centre_z)));
                        turn=double(item.object.rotation_y)*(2*3.14159265358979323846/256.);
                        break;
                    }
                    auto floor=nucleus_floor_packet(*snapshot->ppu,snapshot->display_brightness,srgb,
                        float(room.radius),float(room.top),float(room.bottom),float(snapshot->camera.y),offset,
                        reactor_x,reactor_z,turn,16);
                    if(!floor.geometry.vertex_view().empty()) {
                        bg2=std::move(packet);
                        nucleus_floor=std::move(floor);
                    }
                }
            } catch(const std::exception& error) {
                static bool reported=false;
                if(!reported) {std::fprintf(stderr,"modern: 3D boss room skipped: %s\n",error.what());reported=true;}
            }
        }
        auto photograph=enhanced_sky?in.prepare_photograph():std::optional<DrawPacket>{};
        if(photograph) in.enhanced_landscape->retain_native_ground(bg2,*snapshot->ppu);
        if(!photograph && snapshot->flow==simulation::GameFlowState::game_over)
            next_backgrounds.push_back(game_over_star_sphere_packet(*snapshot->ppu,
                snapshot->display_brightness,snapshot->background_colour_subtract,srgb));
        if(photograph && snapshot->flow==simulation::GameFlowState::game_over) {
            next_backgrounds.push_back(std::move(*photograph));photograph.reset();
        }
        auto ground=landscape_ground_packet(bg2);
        if(nucleus_floor) next_backgrounds.push_back(std::move(*nucleus_floor));
        const auto bg2_index=next_backgrounds.size();
        next_backgrounds.push_back(std::move(bg2));
        if(!ground.geometry.vertex_view().empty()) next_backgrounds.push_back(std::move(ground));
        if(photograph) {
            next_backgrounds.push_back(std::move(*photograph));
            const auto& bodies=in.enhanced_landscape->bodies();
            next_backgrounds.insert(next_backgrounds.end(),bodies.begin(),bodies.end());
        }
        if(!photograph && !snapshot->ppu->tunnel_scene && snapshot->background_ex_city_planets) {
            auto planet_options=background_options;
            planet_options.scroll_override=std::array<int16_t,2>{0,248};
            next_backgrounds.push_back(unique_planet_packet(*snapshot->ppu,planet_options,{384,208,56,48},srgb));
        }
        if(!photograph && !snapshot->ppu->tunnel_scene && snapshot->background_orbital_entry) {
            auto planet_options=background_options;
            planet_options.scroll_override=std::array<int16_t,2>{0,312};
            next_backgrounds.push_back(unique_planet_packet(*snapshot->ppu,planet_options,{336,320,56,64},srgb));
        }
        if(snapshot->ppu->background_mode==1) {
            background_options.scroll_override.reset();
            auto plain_options=background_options;plain_options.scroll_fraction=in.bg3_scroll_fraction;
            auto bg3=background_tile_packet(*snapshot->ppu,BackgroundLayer::bg3,plain_options,srgb);
            if(snapshot->background_water_surround)
                bg3=water_surround_packet(*snapshot->ppu,background_options,srgb);
            else bg3.model=packet.model;
            if(snapshot->background_water_surround)
                next_backgrounds.insert(next_backgrounds.end()-1,std::move(bg3));
            else if(in.bg3_behind_bg2 && !snapshot->ppu->bg3_high_priority && bg2_index<next_backgrounds.size())
                // SNES Mode 1: BG3 shows only through BG2's transparent pixels
                // unless its priority bit is set (EX 4-4's tunnel wall art).
                next_backgrounds.insert(next_backgrounds.begin()+std::ptrdiff_t(bg2_index),std::move(bg3));
            else next_backgrounds.push_back(std::move(bg3));
        }
    }
    if(in.backgrounds_only) return out;
    DrawPacket bitmap;
    const bool replace_dialogue=replace_native_dialogue(*snapshot);
    if(snapshot->native_ex_bitmap) {
        BackgroundTileOptions options;
        options.brightness=snapshot->display_brightness;
        options.guard_inset=16;options.transparent_black=true;
        bitmap=background_tile_packet(*snapshot->ppu,BackgroundLayer::bg1,options,srgb);
        bitmap.model=packet.model;
        // Match desktop: replace active EX communications, retaining the
        // native pause and results bitmap.
        if(replace_dialogue) bitmap.geometry={};
    }
    auto& next_sprites=out.sprites;
    if(snapshot->flow==simulation::GameFlowState::title) {
        next_sprites=title_foreground_packets(*snapshot->ppu,
            snapshot->display_brightness,!snapshot->ex_title_logo_screen,
            snapshot->background_scroll_override,srgb);
        for(auto& foreground:next_sprites) foreground.model=packet.model;
    }
    const auto ppu_transform=packet.model;
    next_sprites.push_back(std::move(bitmap));
    next_sprites.push_back(std::move(packet));
    next_sprites.push_back(std::move(meters));
    if(snapshot->ppu->background_mode==3) {
        next_sprites=source_mode3_packets(*snapshot->ppu,
            snapshot->display_brightness,srgb,snapshot->background_scroll_override);
        for(auto& layer:next_sprites) layer.model=ppu_transform;
    }
    // ppu_transform moved by whole source pixels (column-major model).
    const auto ui_transform=[&](std::array<int,2> offset) {
        auto model=ppu_transform;
        for(unsigned row=0;row<4;++row) model[12+row]+=model[row]*float(offset[0])+model[4+row]*float(offset[1]);
        return model;
    };
    if(replace_dialogue) {
        if(!in.dialogue_layout || !in.rom || !in.symbols) throw std::runtime_error("Dialogue layers need ROM, symbols and layout");
        in.dialogue_layout->set_language(in.language);
        auto dialogue_packets=source_dialogue_packets(*in.rom,*in.symbols,
            snapshot->dialogue,*in.dialogue_layout,snapshot->ppu->cgram,snapshot->display_brightness,srgb,
            in.portrait_overrides);
        const auto comms_transform=ui_transform(in.comms_offset);
        for(auto& layer:dialogue_packets) {
            layer.model=comms_transform;next_sprites.push_back(std::move(layer));
        }
    }
    if(in.pause_text && snapshot->paused && in.rom && in.symbols) {
        // Desktop: draw_game_text(PAUSETXT, 90, 90) on the Super FX UI layer
        // (224 wide, right clip 224), ink 7*16 + the record's colour byte.
        for(const auto address:in.symbols->find("PAUSETXT")) {
            if((address&0xffffU)<0x8000U || ((address>>16U)&0xffU)>=0x70U) continue;
            auto text=source_game_text_packet(*in.rom,*in.symbols,address,90,90,224,256,
                uint8_t(112+(in.rom->read8(address)&15U)),snapshot->ppu->cgram,snapshot->display_brightness,srgb);
            text.model=ui_transform(in.comms_offset);next_sprites.push_back(std::move(text));
            break;
        }
    }
    if(in.stage_tally && in.stage_tally->ink && in.rom && in.symbols) {
        // Through the 2D layer path (filtering, replacement portraits), not
        // a host overlay.
        const auto& tally=*in.stage_tally;
        const auto tally_transform=ui_transform(in.comms_offset);
        auto ink=source_indexed_ink_packet(*tally.ink,snapshot->ppu->cgram,snapshot->display_brightness,srgb);
        ink.model=tally_transform;next_sprites.push_back(std::move(ink));
        for(unsigned i=0;i<3;++i) {
            auto face=source_portrait_packet(*in.rom,*in.symbols,tally.faces[i],false,
                snapshot->ppu->cgram,snapshot->display_brightness,srgb,in.portrait_overrides,tally.face_at[i]);
            face.model=tally_transform;next_sprites.push_back(std::move(face));
        }
    }
    if(snapshot->briefing.active) {
        if(!in.rom || !in.symbols) throw std::runtime_error("Briefing layers need ROM and symbols");
        const auto append_text=[&](uint32_t address,int x,int y,int right,size_t count,uint8_t ink) {
            auto text=source_game_text_packet(*in.rom,*in.symbols,
                address,x,y,right,count,ink,snapshot->ppu->cgram,snapshot->display_brightness,srgb);
            text.model=ppu_transform;
            next_sprites.push_back(std::move(text));
        };
        const auto& brief=snapshot->briefing;
        append_text(brief.message_address,30,173,218,brief.visible_message_characters,101);
        append_text(brief.message_address,28,171,216,brief.visible_message_characters,109);
        append_text(brief.planet_name_address,30,41,224,brief.visible_planet_characters,97);
        append_text(brief.planet_name_address,28,39,224,brief.visible_planet_characters,100);
    }
    return out;
}
std::array<float,4> frame_clear_colour(const GameSceneSnapshot& scene,bool srgb) {
    const auto* snapshot=&scene;
    if(!snapshot->ppu) return {0,0,0,1};
    const bool controls=snapshot->flow==simulation::GameFlowState::controls_type
        || snapshot->flow==simulation::GameFlowState::controls_choice;
    return (controls || snapshot->flow==simulation::GameFlowState::continue_choice)
        ?source_menu_background_colour(*snapshot->ppu,snapshot->display_brightness,srgb)
        :snapshot->flow==simulation::GameFlowState::game_over || snapshot->ppu->tunnel_scene || snapshot->background_unique_top_rows!=0
            || snapshot->background_star_sphere || snapshot->background_space_horizon
        ?source_background_border_colour(snapshot->ppu->cgram,snapshot->display_brightness,srgb)
        :source_backdrop_colour(snapshot->ppu->cgram[0],snapshot->display_brightness,srgb);
}
uint32_t landscape_dot_colour(const GameSceneSnapshot& scene) noexcept {
    // The native grid material: model palette colour 14, with the display fade.
    std::array<uint16_t,256> words{};words[0]=scene.model_palette[14];
    const auto c=render::apply_snes_brightness(render::decode_bgr555_palette(words),scene.display_brightness)[0];
    return uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)|0xff000000U;
}
bool space_background_roll(const GameSceneSnapshot& scene) noexcept {
    return scene.ppu && !scene.ppu->tunnel_scene
        && (scene.background_star_sphere || scene.background_space_horizon || scene.background_unique_space);
}
std::vector<Matrix4> frame_background_models(std::span<const DrawPacket> uploaded_backgrounds,
    const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha,const FrameLayerInputs& in) {
    const auto* snapshot=&current;
    const auto* before=&previous;
    const bool enhanced_sky=in.enhanced_landscape && in.prepare_photograph;
    std::vector<Matrix4> model_updates;
    for(const auto& background:uploaded_backgrounds)
        if(!background.geometry.vertex_view().empty() || !background.geometry.line_view().empty())
            model_updates.push_back(background.model);
    if(!snapshot->ppu) return model_updates;
    // Each block below recomputes the full list from base models, so a later
    // matching block replaces an earlier one, exactly as the PCVR loop does.
    if(snapshot->background_orbital_planet && !snapshot->ppu->tunnel_scene && before->ppu && !in.flat_space_horizon) {
        const bool continuous=before->background_orbital_planet && before->flow==snapshot->flow
            && before->background_id==snapshot->background_id
            && !timing::camera_transform_is_discontinuous(before->camera,snapshot->camera);
        const auto scroll=[](const auto& state) {
            return state.background_scroll_override?float((*state.background_scroll_override)[1]):float(state.ppu->bg2_scroll_y);
        };
        const auto motion=orbital_horizon_motion(scroll(continuous?*before:*snapshot),scroll(*snapshot),
            continuous?alpha:1.,snapshot->background_orbital_thin,snapshot->background_orbital_entry,
            snapshot->flow==simulation::GameFlowState::gameplay && in.ex_experience);
        const auto photographic_motion=orbital_horizon_motion(scroll(continuous?*before:*snapshot),scroll(*snapshot),
            continuous?alpha:1.,snapshot->background_orbital_thin,snapshot->background_orbital_entry,false);
        model_updates.clear();
        for(std::size_t i=0;i<uploaded_backgrounds.size();++i) {
            const auto& background=uploaded_backgrounds[i];
            if(!background.geometry.vertex_view().empty() || !background.geometry.line_view().empty())
                model_updates.push_back(enhanced_sky && in.enhanced_landscape->orbital_body(background)
                    ?photographic_motion:i==0?motion:background.model);
        }
    }
    if(snapshot->background_water_surround) {
        const bool continuous=before->background_water_surround && before->flow==snapshot->flow
            && !timing::camera_transform_is_discontinuous(before->camera,snapshot->camera);
        const auto height=[](const auto& state) {
            return std::clamp(std::abs(float(state.camera.y-state.shadow_height))/256.F,.01F,8.F);
        };
        const auto motion=water_height_motion(height(continuous?*before:*snapshot),height(*snapshot),continuous?alpha:1.);
        model_updates.clear();
        for(const auto& background:uploaded_backgrounds) {
            if(background.geometry.vertex_view().empty() && background.geometry.line_view().empty()) continue;
            const auto payload=background.geometry.texel_view();
            const bool receiver=payload.size()>15 && (payload[15]&32U)!=0;
            model_updates.push_back(receiver?motion:background.model);
        }
    }
    if(snapshot->background_landscape && !in.flat_landscape) {
        const bool continuous=same_landscape_mapping(*before,*snapshot)
            && !timing::camera_transform_is_discontinuous(before->camera,snapshot->camera);
        const auto motion=landscape_camera_motion(*before,*snapshot,continuous?alpha:1.);
        model_updates.clear();
        for(std::size_t i=0;i<uploaded_backgrounds.size();++i) {
            const auto& background=uploaded_backgrounds[i];
            if(!background.geometry.vertex_view().empty() || !background.geometry.line_view().empty())
                model_updates.push_back((i==0 || snapshot->background_ex_city_planets
                    || (enhanced_sky && in.enhanced_landscape->landscape_body(background))
                    || (background.geometry.shared_texels && !background.geometry.vertex_view().empty()
                        && (background.geometry.vertex_view().front().texture[3]&backdrop_texture_flag)==backdrop_texture_flag))
                    ?motion:background.model);
        }
    }
    if(enhanced_sky && in.enhanced_landscape->scrolling_pattern()) {
        const auto motion=in.enhanced_landscape->pattern_motion(*before,*snapshot,alpha);
        model_updates.clear();
        for(const auto& background:uploaded_backgrounds) {
            if(background.geometry.vertex_view().empty() && background.geometry.line_view().empty()) continue;
            model_updates.push_back(in.enhanced_landscape->pattern_panorama(background)?motion:background.model);
        }
    }
    if(enhanced_sky && in.enhanced_landscape->moving_body()) {
        model_updates.clear();
        for(const auto& background:uploaded_backgrounds) {
            const auto vertices=background.geometry.vertex_view();
            if(vertices.empty() && background.geometry.line_view().empty()) continue;
            const auto body=in.enhanced_landscape->tracked_body(background);
            model_updates.push_back(body?in.enhanced_landscape->body_motion(*before,*snapshot,alpha,*body):background.model);
        }
    }
    if(snapshot->flow==simulation::GameFlowState::intro && !snapshot->meters.extended
        && snapshot->background_unique_top_rows==224) {
        const auto scroll=[](const auto& state) {
            return state.background_scroll_override?(*state.background_scroll_override)[1]:state.background_vertical_scroll;
        };
        const bool continuous=before->flow==snapshot->flow && before->background_unique_top_rows==224;
        const auto motion=intro_planet_motion(scroll(continuous?*before:*snapshot),scroll(*snapshot),
            continuous?alpha:1.,float(snapshot->source_vanishing_point[1])+16.F);
        model_updates.clear();
        for(std::size_t i=0;i<uploaded_backgrounds.size();++i) {
            const auto& background=uploaded_backgrounds[i];
            if(!background.geometry.vertex_view().empty() || !background.geometry.line_view().empty())
                model_updates.push_back(i==1?motion:background.model);
        }
    }
    return model_updates;
}
}
