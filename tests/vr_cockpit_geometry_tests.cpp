#include "starfox/vr/cockpit.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <stdexcept>
using namespace starfox;using namespace starfox::vr;
namespace {
void require(bool value,const std::source_location& where=std::source_location::current()) {
    if(!value)throw std::runtime_error("Cockpit geometry regression at "+std::to_string(where.line()));
}
std::array<float,3> point(const Matrix4& m,const float* p) {
    std::array<float,3> result{};
    for(unsigned r=0;r<3;++r) {result[r]=m[12+r];for(unsigned c=0;c<3;++c)result[r]+=m[c*4+r]*p[c];}
    return result;
}
void near(float a,float b,const std::source_location& where=std::source_location::current()) {require(std::abs(a-b)<.001F,where);}
void verify_rig() {
    auto rear=cockpit_rear_packet();require(rear.geometry.vertices.size()==13*12*3); // walls, floor, rear bulkhead, roll hoop
    // Rear modules stay clear of the head and never reach the forward view.
    for(const auto& v:rear.geometry.vertices)require(std::hypot(v.position[0],v.position[1],v.position[2])>.5F
        && v.position[2]>-1.2F && !v.visibility_enabled && !v.texture[3]);
    // The roll hoop joins the bulkhead rim to the window-frame strut tips (flared to about +/-2.04, 0.86, 0.32).
    for(float side:{-1.F,1.F}) {
        bool tip=false,shoulder=false;
        for(const auto& v:rear.geometry.vertices) {
            tip|=std::hypot(v.position[0]-side*2.04F,v.position[1]-.86F,v.position[2]-.32F)<.08F;
            shoulder|=std::hypot(v.position[0]-side*1.122F,v.position[1]+.29F,v.position[2]-.63F)<.08F;
        }
        require(tip && shoulder);
    }
    auto black=cockpit_rear_packet(false,0);for(const auto& v:black.geometry.vertices)require(v.color[0]==0 && v.color[3]==1);
    // Source units; +Z is forward. The first triangle lies wholly on the nose,
    // ahead of the cabin. The second reaches back into the tub and is cut.
    DrawPacket player;
    for(auto p:std::array<std::array<float,3>,6>{{{0,0,100},{10,0,100},{0,10,100},{0,0,100},{10,0,100},{0,0,10}}}) {
        SceneVertex v{};std::copy(p.begin(),p.end(),v.position);v.color[0]=p[2]/100;v.color[3]=1;
        v.visibility_enabled=v.group_enabled=1;player.geometry.vertices.push_back(v);
    }
    const auto hull=cockpit_ship_packet(player);require(hull.geometry.vertices.size()==9);
    for(size_t i=0;i<3;++i) {
        const auto& v=hull.geometry.vertices[i];const auto& source=player.geometry.vertices[i];
        near(v.position[0],source.position[0]/256);near(v.position[1],-source.position[1]/256);
        near(v.position[2],-source.position[2]/256);near(v.color[0],source.color[0]);
    }
    bool cut=false;
    for(const auto& v:hull.geometry.vertices) {
        const auto pilot=point(hull.model,v.position);
        require(pilot[2]<=cockpit_hull_cutouts[0].low[2]+.0001F && !v.visibility_enabled && !v.group_enabled);
        if(std::abs(pilot[2]-cockpit_hull_cutouts[0].low[2])<.0001F) {
            cut=true;near(v.color[0],(-(cockpit_hull_cutouts[0].low[2]+cockpit_seat_m[2])/cockpit_ship_scale*256)/100); // colour interpolates along the cut edge
        }
    }
    require(cut);near(hull.model[0],cockpit_ship_scale);near(hull.model[13],-cockpit_seat_m[1]);near(hull.model[14],-cockpit_seat_m[2]);
    for(auto& v:player.geometry.vertices)v.position[2]=10;
    require(cockpit_ship_packet(player).geometry.vertices.empty());
    std::vector<DrawPacket> hud(1);hud[0].model=overlay_panel_matrix();mount_cockpit_instruments(hud);
    const float anchor[]{76,175,0};const auto mounted=point(hud[0].model,anchor);
    near(mounted[0],-.015F);near(mounted[1],-.852F);near(mounted[2],-1.243F);
    GameSceneSnapshot scene;scene.flow=simulation::GameFlowState::gameplay;scene.pilot_tracking=true;
    scene.view_matrix={32767,0,0,0,32767,0,0,0,32767};scene.pilot_reference.emplace();
    scene.pilot_reference->rotation_matrix={0,32767,0,-32767,0,0,0,0,32767};
    PresentationPreferences prefs;prefs.cockpit=true;
    for(bool follow:{false,true}) {
        prefs.follow_ship_rotation=follow;
        const auto rig=presentation_instrument_matrix(scene,scene,1,prefs);
        const float cabin_reference[]{.76F,1.75F,0};
        const auto centre=point(rig,cabin_reference); // Rotation changes ship-local points only when not following.
        if(follow) {near(centre[0],.76F);near(centre[1],1.75F);}else {near(centre[0],1.75F);near(centre[1],-.76F);}
        auto calibrated=prefs;calibrated.origin_x=16;calibrated.origin_y=4;calibrated.origin_z=-10;
        const auto offset=presentation_instrument_matrix(scene,scene,1,calibrated);
        const float origin[]{.16F,.04F,-.1F};const auto transformed=point(rig,origin);
        for(unsigned i=0;i<3;++i)near(offset[12+i],-transformed[i]);
        for(unsigned scale:{0U,5U}) {
            calibrated.world_scale=scale;require(presentation_instrument_matrix(scene,scene,1,calibrated)==offset);
        }
        XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;view.fov={-.7F,.7F,.7F,-.7F};
        const auto seated=eye_camera(view,1,.05F).value();view.pose.position={.16F,.04F,0};
        const auto leaned=eye_camera(view,1,.05F).value();
        const float cabin_point[]{.3F,-.4F,-1.5F};
        const auto a=point(multiply_matrix(seated.view,rig),cabin_point);
        const auto b=point(multiply_matrix(leaned.view,rig),cabin_point);
        near(b[0]-a[0],-.16F);near(b[1]-a[1],-.04F);
    }
    assets::Shape invalid;bool rejected=false;
    try {(void)cockpit_front_packet(invalid);}catch(const std::runtime_error&) {rejected=true;}
    require(rejected);
}
void dump(const std::string& file,std::span<const DrawPacket> cabin,std::span<const DrawPacket> hud,
    const Matrix4& rig,const Matrix4& world,std::span<const DrawPacket> source) {
    std::ofstream out(file);out<<"{\"rig\":[";
    for(unsigned i=0;i<16;++i)out<<(i?",":"")<<rig[i];out<<"],\"world\":[";
    for(unsigned i=0;i<16;++i)out<<(i?",":"")<<world[i];out<<"],\"packets\":[";bool first=true;
    const auto packets=[&](auto items,const char* group) {for(const auto& p:items) {
        if(!first)out<<',';first=false;out<<"{\"group\":\""<<group<<"\",\"model\":[";
        for(unsigned i=0;i<16;++i)out<<(i?",":"")<<p.model[i];out<<"],\"vertices\":[";bool vertex_first=true;
        for(const auto& v:p.geometry.vertex_view()) {
            if(!vertex_first)out<<',';vertex_first=false;out<<"{\"p\":["<<v.position[0]<<','<<v.position[1]<<','<<v.position[2]<<"],\"c\":[";
            for(unsigned i=0;i<4;++i)out<<(i?",":"")<<v.color[i];out<<"],\"uv\":["<<v.uv[0]<<','<<v.uv[1]<<"],\"t\":[";
            for(unsigned i=0;i<4;++i)out<<(i?",":"")<<v.texture[i];out<<"]}";
        }
        out<<"],\"lines\":[";bool lfirst=true;
        for(const auto& v:p.geometry.line_view()) {out<<(lfirst?"":",")<<'[';lfirst=false;
            for(unsigned c=0;c<3;++c)out<<(c?",":"")<<v.position[c];
            for(float c:v.color)out<<','<<c;out<<']';}
        out<<"],\"texels\":[";bool tfirst=true;for(auto t:p.geometry.texel_view()){out<<(tfirst?"":",")<<t;tfirst=false;}out<<"]}";
    }};
    packets(cabin,"cabin");packets(hud,"hud");packets(source,"world");out<<"]}\n";
}
void cartridge(const assets::RomImage& rom,const assets::SymbolMap& symbols,const std::string& evidence) {
    simulation::GameSimulation game(rom,symbols,"LEVEL1_1",{},true);audio::Spc700Audio audio;
    game.set_timing_mode(simulation::TimingMode::unlocked_20_fps);
    GameSceneHistory history(game,rom,symbols);
    PresentationPreferences prefs;prefs.cockpit=true;
    for(unsigned tick=0;tick<2400 && !pilot_view_active(*history.current(),prefs);++tick) {
        auto result=game.tick({});(void)audio.render_logic_tick(result.audio_port_writes);
        game.synchronize_apu_output_ports(audio.output_ports());history.capture();
    }
    const auto scene=*history.current();require(pilot_view_active(scene,prefs));
    SourceModels models(rom,symbols,true,true);CockpitGeometry cockpit(rom,symbols);
    const auto defaults=models.assemble_world_interpolated(scene,scene,1,false,true);
    auto world=models.assemble_world_interpolated(scene,scene,1,false,true,true);require(world.pending.empty());
    require(defaults.handles==world.handles);
    for(size_t i=0;i<world.handles.size();++i)if(world.handles[i]!=scene.player)
        require(same_draw_geometry(std::span(&world.packets[i],1),std::span(&defaults.packets[i],1)));
    for(const auto& model:world.compute_models)
        require(model.packet_index<world.handles.size() && world.handles[model.packet_index]==model.key && model.key!=scene.player);
    const auto before=world;const auto saved=game.save_state();
    auto off=prefs;off.cockpit=false;require(cockpit.assemble(world,scene,off).empty());
    require(same_draw_geometry(world.packets,before.packets));
    const auto cabin=cockpit.assemble(world,scene,prefs);require(cabin.size()==3);
    // The cockpit encloses the 48-face cutscene Arwing (MY_DEMOS, 56 triangles);
    // the chase view keeps the in-flight ship on the GPU source path.
    {
        const auto slot=[&](const SourceModelPackets& packets) {
            const auto found=std::find(packets.handles.begin(),packets.handles.end(),scene.player);
            require(found!=packets.handles.end());return size_t(found-packets.handles.begin());
        };
        require(std::any_of(defaults.compute_models.begin(),defaults.compute_models.end(),
            [&](const auto& model){return model.packet_index==slot(defaults);})); // in-flight ship on the GPU source path
        require(before.packets[slot(before)].geometry.vertex_view().size()==56*3);
        // A lost wing (MYSHIP_L is narrower on +X) trims the cutscene hull to match.
        auto damaged=game.restored_state(saved);
        damaged->objects().at(damaged->player()).shape=uint16_t(symbols.find("MYSHIP_L").at(0));
        GameSceneHistory damaged_history(*damaged,rom,symbols);const auto& damaged_scene=*damaged_history.current();
        require(pilot_view_active(damaged_scene,prefs));
        auto damaged_world=models.assemble_world_interpolated(damaged_scene,damaged_scene,1,false,true,true);
        const auto damaged_cabin=cockpit.assemble(damaged_world,damaged_scene,prefs);require(damaged_cabin.size()==3);
        float low=1e9F,high=-1e9F;
        for(const auto& v:damaged_cabin[2].geometry.vertices) {const auto q=point(damaged_cabin[2].model,v.position);low=std::min(low,q[0]);high=std::max(high,q[0]);}
        const float edge=25.F/256*cockpit_ship_scale;
        require(std::abs(high-edge)<.01F && low<-3.F);
    }
    require(cabin[0].geometry.vertices.size()==366 && cabin[1].geometry.vertices.size()==468 && !cabin[2].geometry.vertices.empty());
    // The live hull surrounds the canopy seat: nose ahead, wings beside and behind.
    const auto player_slot=std::find(before.handles.begin(),before.handles.end(),scene.player);require(player_slot!=before.handles.end());
    const auto& source_player=before.packets[size_t(player_slot-before.handles.begin())];
    {const auto expected_hull=cockpit_ship_packet(source_player);
        require(same_draw_geometry(std::span(&cabin[2],1),std::span(&expected_hull,1)) && cabin[2].model==expected_hull.model);}
    std::array<float,3> lowest{1e9F,1e9F,1e9F},highest{-1e9F,-1e9F,-1e9F};
    std::vector<std::array<std::array<float,3>,3>> hull;
    for(size_t i=0;i<cabin[2].geometry.vertices.size();i+=3) {
        auto& triangle=hull.emplace_back();
        for(unsigned k=0;k<3;++k) {
            triangle[k]=point(cabin[2].model,cabin[2].geometry.vertices[i+k].position);
            for(unsigned a=0;a<3;++a) {lowest[a]=std::min(lowest[a],triangle[k][a]);highest[a]=std::max(highest[a],triangle[k][a]);}
        }
    }
    require(lowest[2]<-3.5F && highest[2]>1.5F && lowest[0]<-1.5F && highest[0]>1.5F);
    const auto sub=[](auto a,auto b){return std::array<float,3>{a[0]-b[0],a[1]-b[1],a[2]-b[2]};};
    const auto cross=[](auto a,auto b){return std::array<float,3>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
    const auto dot=[](auto a,auto b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    // Seated head clearance: sampled triangle surfaces stay outside 0.35 m.
    for(const auto& t:hull)for(unsigned u=0;u<=20;++u)for(unsigned v=0;u+v<=20;++v) {
        const float a=u/20.F,b=v/20.F;std::array<float,3> q{};
        for(unsigned c=0;c<3;++c)q[c]=t[0][c]+(t[1][c]-t[0][c])*a+(t[2][c]-t[0][c])*b;
        require(dot(q,q)>.35F*.35F);
    }
    // The hull never covers the mounted instrument face seen from the seat.
    for(float x=-.229F;x<=.203F;x+=.012F)for(float y=-.964F;y<=-.744F;y+=.011F) {
        const std::array<float,3> target{x,y,-1.243F};
        for(const auto& t:hull) {
            const auto e1=sub(t[1],t[0]),e2=sub(t[2],t[0]),h=cross(target,e2);const float det=dot(e1,h);
            if(std::abs(det)<1e-9F)continue;
            const auto s0=sub(std::array<float,3>{0,0,0},t[0]);const float u=dot(s0,h)/det;
            const auto q=cross(s0,e1);const float v=dot(target,q)/det,distance=dot(e2,q)/det;
            require(!(u>=0 && v>=0 && u+v<=1 && distance>0 && distance<1));
        }
    }
    for(size_t i=0;i<world.handles.size();++i) {
        if(world.handles[i]==scene.player)require(world.packets[i].geometry.vertex_view().empty() && world.packets[i].model==identity_matrix);
        else require(same_draw_geometry(std::span(&world.packets[i],1),std::span(&before.packets[i],1)) && world.packets[i].model==before.packets[i].model);
    }
    require(game.save_state()==saved);
    for(const auto& model:world.compute_models)
        require(model.packet_index<world.handles.size() && world.handles[model.packet_index]==model.key);
    for(const auto& p:cabin)for(const auto& v:p.geometry.vertex_view())require(!v.visibility_enabled && !v.group_enabled);
    // Source-derived window lip moves from +/-0.27,-0.39 to +/-0.459,-0.55m;
    // its instrument face is pinned, not stretched together with the canopy.
    const auto has_front_point=[&](float x,float y,float z) {
        return std::any_of(cabin[0].geometry.vertices.begin(),cabin[0].geometry.vertices.end(),[&](const auto& v) {
            return std::abs(v.position[0]-x)<.001F && std::abs(v.position[1]-y)<.001F && std::abs(v.position[2]-z)<.001F;
        });
    };
    require(has_front_point(-.459F,-.55F,-2.03F) && has_front_point(.459F,-.55F,-2.03F));
    require(has_front_point(.21F,-.72F,-1.25F) && has_front_point(-.24F,-.99F,-1.25F));
    for(const auto& packet:std::span(cabin).first(2))for(size_t i=0;i<packet.geometry.vertices.size();i+=3) {
        const auto& a=packet.geometry.vertices[i];const auto& b=packet.geometry.vertices[i+1];const auto& c=packet.geometry.vertices[i+2];
        const float ux=b.position[0]-a.position[0],uy=b.position[1]-a.position[1],uz=b.position[2]-a.position[2];
        const float vx=c.position[0]-a.position[0],vy=c.position[1]-a.position[1],vz=c.position[2]-a.position[2];
        const float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
        require(nx*nx+ny*ny+nz*nz>1e-12F);
    }
    render::ScaledTextRenderer text(rom,symbols);auto hud=layout_a_instrument_packets(rom,symbols,scene,text);
    const auto original_hud=hud;mount_cockpit_instruments(hud,scene.meters.extended);require(same_draw_geometry(hud,original_hud));
    const auto fits_face=[&](const auto& packets) {for(const auto& packet:packets)for(const auto& vertex:packet.geometry.vertex_view()) {
        const auto position=point(packet.model,vertex.position);
        require(position[0]>=-.229F && position[0]<=.203F && position[1]>=-.964F && position[1]<=-.744F);
    }};
    fits_face(hud);
    for(unsigned bombs:{0U,scene.meters.extended?5U:3U}) {
        auto variation=game.restored_state(saved);variation->map().write_native_word(symbols.find(scene.meters.extended?"SPECWEPCNTONE":"SPECWEPCNT").at(0),uint16_t(bombs));
        (void)variation->tick({});(void)variation->tick({});GameSceneHistory varied_history(*variation,rom,symbols);
        for(bool full:{false,true})for(unsigned comms=0;comms<3;++comms) {
            auto varied=*varied_history.current();varied.meters.damage=full?varied.meters.player_health_max:0;
            varied.dialogue.active=comms!=0;varied.dialogue.text_visible=true;varied.dialogue.three_lines=comms==2;
            varied.dialogue.text_address=symbols.find("MSG_1").at(0);
            auto packets=layout_a_instrument_packets(rom,symbols,varied,text);
            mount_cockpit_instruments(packets,varied.meters.extended);fits_face(packets);
            require(packets[0].model==hud[0].model && packets[1].model==hud[1].model);
        }
    }
    // Boss meters retain the existing top-row placement and packet ordering.
    // They are intentionally not cropped into the single-player band.
    auto boss_scene=scene;boss_scene.meters.boss_health=100;boss_scene.meters.boss_max_health=180;
    auto boss_hud=layout_a_instrument_packets(rom,symbols,boss_scene,text);
    const auto boss_source=boss_hud;
    require(boss_hud.size()==original_hud.size());
    require(boss_hud[1].geometry.vertex_view().size()>original_hud[1].geometry.vertex_view().size());
    mount_cockpit_instruments(boss_hud,boss_scene.meters.extended);
    require(same_draw_geometry(boss_hud,boss_source));
    bool outside_band=false;
    for(const auto& vertex:boss_hud[1].geometry.vertex_view())
        outside_band|=point(boss_hud[1].model,vertex.position)[1]>-.744F;
    require(outside_band);
    assets::ShapeDecoder decoder(rom,symbols);auto front=decoder.decode_by_name(symbols,"COCKPIT");
    front.faces[0].vertex_indices.pop_back();bool rejected=false;
    try {(void)cockpit_front_packet(front);}catch(const std::runtime_error&) {rejected=true;}require(rejected);
    for(unsigned inactive=0;inactive<3;++inactive) {
        auto other=scene;if(inactive==0)other.pilot_tracking=false;if(inactive==1)other.pilot_reference.reset();if(inactive==2)other.flow=simulation::GameFlowState::title;
        require(cockpit.assemble(world,other,prefs).empty());
    }
    if(!evidence.empty())for(bool follow:{false,true}) {
        prefs.follow_ship_rotation=follow;
        dump(evidence+(follow?"-follow.json":"-existing-rotation.json"),cabin,hud,
            presentation_instrument_matrix(scene,scene,1,prefs),presentation_scene_matrix(scene,scene,1,prefs),world.packets);
    }
    // Seed the same native FLASHPLAYER initializer as the existing desktop
    // capture fixture. Native ticks own its shape, material, blink and lifetime.
    const auto flash=game.objects().allocate_after();require(flash!=0);
    auto& effect=game.objects().at(flash);const auto ship=game.objects().at(game.player());
    effect.world_x=ship.world_x;effect.world_y=ship.world_y;effect.world_z=ship.world_z;
    effect.shape=ship.shape;effect.colour_table=ship.colour_table;
    effect.rotation_x=ship.rotation_x;effect.rotation_y=ship.rotation_y;effect.rotation_z=ship.rotation_z;
    effect.strategy_address=symbols.find("FLASHPLAYER_ISTRAT").at(0);
    unsigned visible=0,hidden=0;bool captured=false;
    for(unsigned tick=0;tick<80 && game.objects().is_active(flash);++tick) {
        auto result=game.tick({});(void)audio.render_logic_tick(result.audio_port_writes);
        game.synchronize_apu_output_ports(audio.output_ports());history.capture();
        const auto now=*history.current();
        if(!pilot_view_active(now,prefs))continue;
        auto native=models.assemble_world_interpolated(*history.previous(),now,.5,false,true,true);
        require(native.pending.empty());
        const auto slot=std::find(native.handles.begin(),native.handles.end(),flash);
        if(slot==native.handles.end()) {++hidden;continue;}
        const auto index=size_t(slot-native.handles.begin());const auto source=native.packets[index];
        if(source.geometry.vertex_view().empty() && source.geometry.line_view().empty()) {++hidden;continue;}
        ++visible;const auto before_effect=game.save_state();
        const auto attached=cockpit.assemble(native,now,prefs);require(game.save_state()==before_effect);
        require(native.packets[index].geometry.vertex_view().empty() && native.packets[index].geometry.line_view().empty());
        const auto expected=cockpit_ship_packet(source);
        const auto actual=std::find_if(attached.begin()+2,attached.end(),[&](const auto& packet){
            return same_draw_geometry(std::span(&packet,1),std::span(&expected,1)) && packet.model==expected.model;
        });require(actual!=attached.end());
        require(!actual->geometry.vertex_view().empty() || !actual->geometry.line_view().empty());
        // Kept geometry lies outside the cabin cut-outs; uncut vertices keep their source colours.
        const auto check_kept=[&](auto shown,auto from) {for(const auto& v:shown) {
            const auto pilot=point(actual->model,v.position);
            for(const auto& box:cockpit_hull_cutouts) {
                bool inside=true;
                for(unsigned a=0;a<3;++a)inside&=pilot[a]>box.low[a]+.001F && pilot[a]<box.high[a]-.001F;
                require(!inside);
            }
            // Flat faces share corners, so a kept corner matches one of the source faces meeting there.
            bool at_source=false,matched=false;
            for(const auto& f:from)if(std::abs(f.position[0]/256-v.position[0])<1e-6F && std::abs(-f.position[1]/256-v.position[1])<1e-6F
                && std::abs(-f.position[2]/256-v.position[2])<1e-6F) {
                at_source=true;bool same=true;for(unsigned c=0;c<4;++c)same&=std::abs(f.color[c]-v.color[c])<.001F;matched|=same;
            }
            require(!at_source || matched);
        }};
        check_kept(actual->geometry.vertex_view(),source.geometry.vertex_view());
        check_kept(actual->geometry.line_view(),source.geometry.line_view());
        for(bool follow:{false,true})for(unsigned scale:{0U,5U}) {
            auto calibrated=prefs;calibrated.follow_ship_rotation=follow;calibrated.world_scale=scale;
            calibrated.origin_x=16;calibrated.origin_y=4;calibrated.origin_z=-10;
            const auto rig=presentation_instrument_matrix(*history.previous(),now,.5,calibrated);
            const auto camera=multiply_matrix(rig,actual->model);
            const auto native_camera=multiply_matrix(presentation_scene_matrix(*history.previous(),now,.5,calibrated),source.model);
            const auto check_stream=[&](auto from) {for(size_t i=0;i<from.size();++i) {
                const float pilot[]{from[i].position[0]/256,-from[i].position[1]/256,-from[i].position[2]/256};
                const auto registered=point(camera,pilot);
                const float world=cockpit_world_scale(calibrated);
                const float unscaled[]{pilot[0]*world-cockpit_seat_m[0],
                    pilot[1]*world-cockpit_seat_m[1],pilot[2]*world-cockpit_seat_m[2]};
                const auto authored=point(native_camera,from[i].position),attached_reference=point(rig,unscaled);
                for(unsigned axis=0;axis<3;++axis) {
                    require(std::abs(authored[axis]-attached_reference[axis])<.005F);
                    // At default world scale the native ship lands exactly on the cabin's ship.
                    if(scale==0)require(std::abs(authored[axis]-registered[axis])<.005F);
                }
            }};
            check_stream(source.geometry.vertex_view());
            check_stream(source.geometry.line_view());
            require(actual->model==cabin[2].model); // Same calibrated ship/seat rig as the live hull.
        }
        if(!captured && !evidence.empty()) {
            for(bool follow:{false,true}) {
                auto view=prefs;view.follow_ship_rotation=follow;
                dump(evidence+(follow?"-repair-follow.json":"-repair-existing-rotation.json"),attached,hud,
                    presentation_instrument_matrix(*history.previous(),now,.5,view),
                    presentation_scene_matrix(*history.previous(),now,.5,view),native.packets);
            }
            captured=true;
        }
    }
    require(visible>0 && hidden>0);
    std::cout<<(scene.meters.extended?"EX":"Original")<<" seeded native repair flash: "<<visible
        <<" visible and "<<hidden<<" hidden phases, complete source geometry/materials and common ship-rig registration passed\n";
    std::cout<<(scene.meters.extended?"EX":"Original")<<" bundle cockpit: 122 front + 156 rear + "
        <<cabin[2].geometry.vertices.size()/3<<" player hull triangles; source state/other objects/HUD art unchanged\n";
}
}
int main(int argc,char** argv) try {
    verify_rig();
    if(argc>=3 && std::string_view(argv[1])=="--bundle") {
        std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});require(bytes.size()>12);
        uint32_t manifest=0;for(unsigned i=0;i<4;++i)manifest|=uint32_t(bytes[8+i])<<(8*i);
        // Positive structure/CRC integration check; application validates its embedded manifest separately.
        const auto bundle=assets::decode_runtime_bundle(bytes,manifest);
        for(bool ex:{false,true})cartridge(assets::RomImage(ex?bundle.starfox_ex_rom:bundle.original_rom),
            assets::SymbolMap::parse(ex?bundle.starfox_ex_symbols:bundle.original_symbols),argc==4?std::string(argv[3])+(ex?"-ex":"-original"):"");
    } else if(argc==3)cartridge(assets::RomImage::load(argv[1]),assets::SymbolMap::load(argv[2]),"");
    else require(argc==1);
    std::cout<<"Cockpit geometry/clip/material/mount/calibration/follow/head-tracking checks passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
