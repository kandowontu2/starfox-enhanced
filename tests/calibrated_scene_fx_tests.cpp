#include "starfox/render/calibrated_game_scene_fx.hpp"
#include "starfox/render/camera_response.hpp"
#include "starfox/render/calibrated_ground_receiver.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace starfox;
namespace {
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
}
int main() try {
    {
        simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
        ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
        for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
        for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
        auto plain=vr::landscape_sphere_packet(ppu,{},112,false,false,232);vr::place_landscape_ground(plain,-.75F,true);
        const vr::EyeCamera camera{identity,identity};
        require(!render::calibrated_ground_ray_receiver(plain,camera,1),"Original ground unexpectedly became a metal receiver");
        for(unsigned material:{5U,6U,7U}) for(float bank:{0.F,.31F}) for(float eye:{-.04F,.035F}) {
            auto packet=plain;render::CalibratedGroundGradient surface;surface.material=material;
            surface.origin={123,-415};surface.seconds=1.75F;surface.brightness=.65F;
            require(render::apply_calibrated_ground(packet,surface),"Metal payload rejected");
            const float c=std::cos(bank),s=std::sin(bank);
            packet.model={c,s,0,0,-s,c,0,0,0,0,1,0,.08F,0,-.12F,1};
            auto view=camera;view.view[12]=-eye;view.view[13]=-.027F;view.view[14]=-.04F;
            const auto receiver=render::calibrated_ground_ray_receiver(packet,view,.75F,true,nullptr,3);
            require(bool(receiver),"Native metal lost its analytic receiver");
            const auto& point=receiver->plane.point;const auto& normal=receiver->plane.normal;
            require(std::abs(point.x-256*(s*.75F+.08F-eye))<.001
                && std::abs(point.y-256*(c*.75F+.027F))<.001
                && std::abs(point.z-256*.16F)<.001,"Retained bank/eye/translation lost from reflection plane");
            require(std::abs(normal.x+s)<1.e-6 && std::abs(normal.y+c)<1.e-6 && std::abs(normal.z)<1.e-6,
                "Native plane normal has wrong handedness/bank");
            require(receiver->transport.material==material-5 && receiver->transport.time==1.75F
                && receiver->transport.brightness==.65F && receiver->transport.reflection_strength==.75F
                && receiver->transport.mirror_models,"Native metal clock/material/quality changed between eyes");
            require(receiver->transport.caustics==(material==5?3U:0U)
                && receiver->transport.source_colour.has_value()==(material==5),"Liquid/metal transport metadata crossed materials");
            if(material==5) {
                const auto combined=vr::model_eye_camera(view,packet.model).value();
                const float local[]{.37F,-.75F,-2.1F},sign[]{1,-1,-1};
                float hit[3]{};
                for(unsigned row=0;row<3;++row) {
                    hit[row]=combined.view[12+row];
                    for(unsigned col=0;col<3;++col) hit[row]+=combined.view[col*4+row]*local[col];
                    hit[row]*=256*sign[row];
                }
                const float expected[]{local[0]*256+123,-local[1]*256,-local[2]*256-415};
                for(unsigned col=0;col<3;++col) {
                    float world=receiver->transport.camera_position[col];
                    for(unsigned row=0;row<3;++row) world+=hit[row]*receiver->transport.world_to_view[row*3+col];
                    require(std::abs(world-expected[col])<.002,"Water coordinates drifted between tracked/banked eyes");
                }
                auto quantized=packet;for(unsigned col=0;col<3;++col) for(unsigned row=0;row<3;++row)
                    quantized.model[col*4+row]*=.9997F;
                const auto scaled=render::calibrated_ground_ray_receiver(quantized,view,1);
                const auto scaled_view=vr::model_eye_camera(view,quantized.model).value();
                for(unsigned row=0;row<3;++row) {
                    hit[row]=scaled_view.view[12+row];
                    for(unsigned col=0;col<3;++col) hit[row]+=scaled_view.view[col*4+row]*local[col];
                    hit[row]*=256*sign[row];
                }
                for(unsigned col=0;col<3;++col) {
                    float world=scaled->transport.camera_position[col];
                    for(unsigned row=0;row<3;++row) world+=hit[row]*scaled->transport.world_to_view[row*3+col];
                    require(std::abs(world-expected[col])<.002,"Source Q15 scaling drifted the world-anchored water coordinates");
                }
            }
            auto override=identity;override[13]=-.2F;
            const auto moved=render::calibrated_ground_ray_receiver(packet,view,1,false,&override);
            require(std::abs(moved->plane.point.y-256*(.95F+.027F))<.001,"Camera-response model override was dropped");
            render::CalibratedGameFrame metal;metal.settings.ray_tracing=metal.settings.reflections=3;
            metal.draws.push_back({packet,render::CalibratedGameLayer::world,vr::SceneBlend::opaque,false});
            auto selected=metal.packets();require(selected[0].ground_receiver && !selected[0].ray_caster
                && !selected[0].reflective_material,"Ground receiver was selected as a model caster/material");
            metal.settings.reflections=0;require(metal.packets()[0].ground_receiver==(material==5),
                "Water transmission or metal reflection has the wrong OFF gate");
            metal.settings.ray_tracing=0;require(!metal.packets()[0].ground_receiver,"Ground ray receiver survived RT OFF");
            bool rejected=false;try {(void)render::calibrated_ground_ray_receiver(packet,view,NAN);}
            catch(const std::invalid_argument&) {rejected=true;}require(rejected,"Invalid ground ray strength accepted");
        }
    }
    auto old=std::make_shared<vr::GameSceneSnapshot>(),now=std::make_shared<vr::GameSceneSnapshot>();
    old->flow=now->flow=simulation::GameFlowState::gameplay;
    old->view_matrix=now->view_matrix={32767,0,0,0,32767,0,0,0,32767};
    old->camera={100,200,300};now->camera={101,203,305};now->player=7;
    render::CalibratedGameFrame frame;frame.previous=old;frame.current=now;frame.alpha=.25;
    frame.settings.scene_enhancements=255;frame.settings.particle_enhancements=15;
    const auto add=[&](unsigned id,bool protected_ink=false,bool after=false,render::CalibratedGameLayer layer=render::CalibratedGameLayer::model) {
        vr::GameSceneObject item;item.handle=simulation::ObjectHandle(id);item.object.shape=10;
        item.object.strategy_address=22;item.object.health=200;
        item.presentation.generation=3;item.presentation.shape=10;item.presentation.strategy_address=22;
        item.presentation.transform={200,300,700};auto before=item.presentation;before.transform={199,297,695};
        old->transforms[item.handle]=before;now->transforms[item.handle]=item.presentation;now->objects.push_back(item);
        render::CalibratedGameDraw draw;draw.source_key=id;draw.layer=layer;draw.after_rays=after;draw.packet.preserve_native_colour=protected_ink;
        frame.draws.push_back(draw);
    };
    add(7);add(8);add(9,true);add(10,true);add(11,false,true);add(12,false,false,render::CalibratedGameLayer::native_ui);
    add(13,false,false,render::CalibratedGameLayer::world);add(14);frame.draws.pop_back();
    now->objects[3].object.shape=99; // Positively identified emissive projectile.
    vr::SceneInterpolationRules rules;
    const auto capture=[&](double scale=1) {return render::calibrated_scene_fx_inputs(frame,rules,[](unsigned shape){return shape==99;},[&](unsigned){return scale;});};
    auto inputs=capture();
    require(inputs.emitters.size()==3,"Emitter ownership leaked into portraits, ordered overlays, shadows or undrawn models");
    require(inputs.origin==std::array<double,3>{100.25,200.75,301.25},"Source camera interpolation lost fractional coordinates");
    require(inputs.emitters[0].position==std::array<double,3>{199.25,297.75,696.25},"Model/event fractional centres differ");
    require(inputs.emitters[0].player && inputs.emitters[2].weapon && inputs.emitters[2].id==((3ULL<<16)|10),"Source roles or generation identity changed");
    constexpr float unit=32767.F/(32768.F*256);
    require(inputs.source_to_rig==vr::Matrix4{unit,0,0,0,0,-unit,0,0,0,0,-unit,0,0,0,0,1},"Source Q15 view/units/sign mapping changed");
    frame.draws.push_back(frame.draws[2]);frame.draws.back().packet.preserve_native_colour=false;
    require(capture().emitters.size()==4,"Duplicate unprotected model draw did not establish source ownership");
    rules.trail=22;require(capture().emitters[0].position==std::array<double,3>{200,300,700},"Discrete source trail was independently interpolated");rules.trail=0;
    old->transforms[7].generation=2;
    require(capture().emitters[0].position==std::array<double,3>{200,300,700},"Recycled slot borrowed its old transform");old->transforms[7].generation=3;
    now->objects[0].object.flags=1;now->objects[0].object.count=2;
    require(capture().emitters[0].explosion,"Source explosion was not captured");
    const auto scaled=capture(.375);
    require(scaled.emitters[0].position[0]==100.25+99*.375,"Presentation centre scale was not shared with source geometry");
    {
        auto attached=now->objects[1];attached.object.strategy_address=attached.presentation.strategy_address=123;
        attached.presentation.transform={-500,-600,1000};auto special=rules;special.flash_player=123;
        const auto sample=vr::interpolate_scene_object(*old,*now,attached,.25,special);
        require(sample.transform.x==199.25 && sample.transform.y==297.75 && sample.transform.z==696.25,
            "Attached overlay took an independent path instead of the player's fractional centre");
        attached.object.strategy_address=attached.presentation.strategy_address=321;attached.presentation.transform={250,300,1000};
        special.flash_player=0;special.crosshair=321;old->view_float_y=10;now->view_float_y=14;
        const auto reticle=vr::interpolate_scene_object(*old,*now,attached,.25,special);
        require(reticle.transform.x==249.25 && reticle.transform.y==308.75 && reticle.transform.z==996.25,
            "New sight station lost relative-birth attachment or floating HUD offset");
        auto station=attached.presentation;station.transform={249,297,995};old->transforms[99]=station;
        const auto matched=vr::interpolate_scene_object(*old,*now,attached,.25,special);
        require(matched.transform.x==reticle.transform.x && matched.transform.y==reticle.transform.y && matched.transform.z==reticle.transform.z,
            "Recycled sight station did not retain its source depth station");
        old->transforms.erase(99);old->view_float_y=now->view_float_y=0;
    }
    frame.settings.scene_enhancements=frame.settings.particle_enhancements=0;
    require(capture().emitters.empty(),"OFF scanned/captured active emitters");
    frame.settings.scene_enhancements=255;frame.settings.particle_enhancements=15;
    old->flow=simulation::GameFlowState::intro;
    require(capture().emitters[0].position==std::array<double,3>{200,300,700} && capture().origin==std::array<double,3>{101,203,305},"Flow cut blended old world positions");
    old->flow=now->flow;old->camera.x=0;now->camera.x=20000;
    require(capture().origin[0]==20000,"Camera cut blended source effect origin");
    old->camera=now->camera;
    for(auto flow:{simulation::GameFlowState::pregame_menu,simulation::GameFlowState::ex_pregame_menu,simulation::GameFlowState::controls_type}) {
        now->flow=flow;require(capture().emitters.empty(),"Nonworld menu emitted physical scene effects");
    }
    now->flow=simulation::GameFlowState::gameplay;
    for(double scale:{0.,-1.,5.,std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected=false;try {(void)capture(scale);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"Invalid source-centre scale accepted");
    }
    for(unsigned type=0;type<9;++type) {
        frame.scene_fx={};frame.scene_fx.add({0,0,500},100,.8F,float(type),.1F,{},{});
        require(render::valid_calibrated_scene_fx(frame),"Valid per-type native frame rejected");
        const auto settings=frame.settings;
        if(type==0) frame.settings.scene_enhancements&=~3U;
        else if(type==1) frame.settings.scene_enhancements&=~12U;
        else if(type==2) frame.settings.scene_enhancements&=~48U;
        else if(type<=5) frame.settings.scene_enhancements&=~192U;
        else if(type<=7) frame.settings.particle_enhancements&=~3U;
        else frame.settings.particle_enhancements&=~12U;
        require(!render::valid_calibrated_scene_fx(frame),"Disabled effect type encoded in a native frame");frame.settings=settings;
    }
    now->flow=simulation::GameFlowState::controls_type;require(!render::valid_calibrated_scene_fx(frame),"Menu accepted active physical effects");now->flow=simulation::GameFlowState::gameplay;
    frame.scene_fx_source_to_rig[15]=0;require(!render::valid_calibrated_scene_fx(frame),"Nonaffine source effect transform accepted");frame.scene_fx_source_to_rig[15]=1;
    frame.scene_fx_origin[0]=std::numeric_limits<double>::infinity();require(!render::valid_calibrated_scene_fx(frame),"Invalid effect origin accepted");frame.scene_fx_origin={};
    frame.scene_fx_source_to_rig=identity;frame.settings.camera_response_pose={.02,-.01,.03};
    const auto response=render::camera_response_world_transform({.02,-.01,.03});
    require(response && frame.scene_fx_rig()==*response,"Effects did not compose the same camera-response transform as models");
    std::cout<<"Native source effect capture: fractional model/camera centres, generation/trail/cut, ownership, scaling, OFF/menu gating and typed payload validation passed ("<<checks<<" checks).\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
