// MODERN's ray-traced lighting, API-free parts (moved from
// src/modern/modern_runtime.cpp and src/modern/sdl_ray_shadows.cpp).
#include "starfox/vr/ray_lighting.hpp"
#include "starfox/vr/scene_camera.hpp"
#include "starfox/vr/source_shadow_environment.hpp"
#include "starfox/simulation/math.hpp"
#include "starfox/timing/fixed_step.hpp"
#include <algorithm>
#include <cmath>
#include <initializer_list>
namespace starfox::vr {
using render::shadows::Vec3;
RayEmitters::RayEmitters(const assets::SymbolMap& symbols) {
    const auto shapes=[&](std::initializer_list<const char*> names,std::unordered_set<uint16_t>& into) {
        for(const auto* name:names) for(const auto address:symbols.find(name))
            if(address>=0x8000U && address<=0xffffU) into.insert(uint16_t(address));
    };
    // Shots by strategy: the Arwings fire ELASER2 shapes too. Arwing shots
    // (laser, twin laser, plasma) and enemy bombs are known by their
    // strategies; ring shots and plasma balls by shape. Lasers are the
    // existing renderer's emissive beam shapes (starfox_pc.cpp emissive_beam_shapes).
    shapes({"ELASER2","ELASER2_S2","ELASER2A","OVALBEAM","LASERLINE","LASER_0"},beam_shapes_);
    shapes({"PLASMA","LARGEPLASMA","RINGLASER","PLAYERBEAM"},plasma_shapes_);
    const auto strategies=[&](std::initializer_list<const char*> names,std::unordered_set<uint32_t>& into) {
        for(const auto* name:names) for(const auto address:symbols.find(name)) into.insert(address);
    };
    strategies({"PELASER_STRAT","PBEAM_STRAT"},arwing_shot_strategies_);
    strategies({"MISSILE1_STRAT","MISSILE2A_STRAT","HMISSILE1_STRAT","HMISSILE2_STRAT","HMISSILE3_STRAT",
        "QHMISSILE1_STRAT","STBHMISSILE1_STRAT","CHICKHMISSILE1_STRAT","NUKE_STRAT"},bomb_strategies_);
    shapes({"MYSHIP_4","MYSHIP_L","MYSHIP_R","MYSHIP_B","IMYSHIP_4","FRIENDSHIP_4"},ship_shapes_);
    shapes({"NUKE"},bomb_shapes_);
    shapes({"BOOSTSHAPE"},boost_shapes_);
    // Indicators draw normally but are not physical occluders
    // (vr::SourceRayPolicy): muzzle flashes and the reticle.
    if(const auto flashes=symbols.find("FLASH_STRAT");!flashes.empty()) flash_strategy_=flashes.front();
    if(const auto reticles=symbols.find("XHAIR2");!reticles.empty()) reticle_shape_=uint16_t(reticles.front());
    const auto lists=symbols.find("BGLISTS"),intro=symbols.find("BG_1_1I");
    if(!lists.empty() && !intro.empty() && intro.front()>lists.front() && intro.front()-lists.front()<=0xffffU)
        intro_background_=uint16_t(intro.front()-lists.front());
}
unsigned RayEmitters::kind(const GameSceneSnapshot& scene,uint32_t key) const {
    for(const auto& item:scene.objects) if(item.handle==(key&65535)) {
        const auto shape=item.object.shape;
        const auto strategy=item.object.strategy_address;
        if(arwing_shot_strategies_.contains(strategy)) return EmitterArwingLaser;
        if(bomb_shapes_.contains(shape) || bomb_strategies_.contains(strategy)) return EmitterBomb;
        if(plasma_shapes_.contains(shape)) return EmitterPlasma;
        if(beam_shapes_.contains(shape)) return EmitterLaser;
        if(boost_shapes_.contains(shape)) return EmitterBoost; // the cartridge's boost flame
        if(item.handle==scene.player || ship_shapes_.contains(shape)) return EmitterEngine;
        return (item.object.flags&1U)!=0 && item.object.count>0?EmitterExplosion:EmitterNone;
    }
    return EmitterNone;
}
bool RayEmitters::caster(const GameSceneSnapshot& scene,uint32_t key) const {
    if(key==0x20000 || key==0x30000) return false;
    // Shots, plasma, bombs and the boost flame are light, not matter: they
    // never shade anything, not even the light of the shot beside them (a
    // twin laser, the next shot in a stream).
    switch(kind(scene,key)) {
    case EmitterLaser: case EmitterArwingLaser: case EmitterPlasma:
    case EmitterBomb: case EmitterBoost: return false;
    default: break;
    }
    for(const auto& item:scene.objects) if(item.handle==(key&65535))
        return !((flash_strategy_ && item.object.strategy_address==flash_strategy_)
            || (reticle_shape_ && item.presentation.shape==reticle_shape_));
    return true;
}
bool RayEmitters::shadowless(const GameSceneSnapshot& scene) const noexcept {
    return intro_background_ && scene.background_id==*intro_background_;
}
std::optional<Vec3> ray_sun(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha,double elevation) {
    double fraction=alpha;
    if(previous.flow!=current.flow || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) fraction=1;
    const auto view=simulation::interpolate_rotation_matrix_q15(previous.view_matrix,current.view_matrix,fraction);
    if(!source_shadow_environment(view,double(current.camera.y),current.shadow_height,true)) return {};
    // The environment's world basis (x, y, z rows of the view, as
    // source_shadow_environment builds them).
    const Vec3 x{view[0]/32768.,-view[1]/32768.,-view[2]/32768.};
    const Vec3 y{view[3]/32768.,-view[4]/32768.,-view[5]/32768.};
    const Vec3 z{view[6]/32768.,-view[7]/32768.,-view[8]/32768.};
    // Corridors, tunnels and the boss room: straight overhead, as the
    // cartridge drops shadows directly under the ship.
    const bool enclosed=(current.ppu && current.ppu->tunnel_scene) || current.background_corridor || current.background_nucleus;
    const double angle=(enclosed?90.:elevation)*3.14159265358979/180.;
    const double across=std::cos(angle)/std::sqrt(2.);
    return x*-across+y*-std::sin(angle)+z*-across;
}
RaySoftness ray_softness(int level) noexcept {
    // GPU FAST's sun disc radii per SHADOW SOFTNESS level, with 8/12/16
    // fixed rays for the soft levels.
    constexpr float radii[]{0.F,.0075F,.015F,.03F};
    constexpr unsigned samples[]{1,8,12,16};
    level=std::clamp(level,0,3);
    return {radii[level],samples[level]};
}
// vr::ShadowView / vr::shadow_model_transform (shadow_camera.hpp, which pulls
// in OpenXR): native eye axes are +Y down, +Z forward, in source units.
bool ray_rigid(const EyeCamera& eye) noexcept {
    const auto& m=eye.view;
    for(auto value:m) if(!std::isfinite(value)) return false;
    if(m[3]!=0 || m[7]!=0 || m[11]!=0 || m[15]!=1) return false;
    for(unsigned a=0;a<3;++a) for(unsigned b=0;b<3;++b) {
        double product=0;for(unsigned k=0;k<3;++k) product+=double(m[k*4+a])*m[k*4+b];
        if(std::abs(product-(a==b?1.:0.))>1e-4) return false;
    }
    return true;
}
Vec3 ray_eye_direction(const EyeCamera& eye,Vec3 p) noexcept {
    const auto& m=eye.view;
    return {m[0]*p.x+m[4]*p.y+m[8]*p.z,-(m[1]*p.x+m[5]*p.y+m[9]*p.z),-(m[2]*p.x+m[6]*p.y+m[10]*p.z)};
}
std::optional<std::array<std::array<float,4>,3>> ray_model_rows(const EyeCamera& eye,const Matrix4& placement,double units) {
    if(!std::isfinite(units) || units<=0) return {};
    const auto composed=model_eye_camera(eye,placement);
    if(!composed) return {};
    const auto& m=composed->view;
    constexpr double sign[]{1,-1,-1};
    std::array<std::array<float,4>,3> rows{};
    for(unsigned row=0;row<3;++row) {
        for(unsigned column=0;column<3;++column)
            rows[row][column]=float(sign[row]*m[column*4+row]*sign[column]*256./units);
        rows[row][3]=float(sign[row]*m[12+row]*256.);
        for(float value:rows[row]) if(!std::isfinite(value)) return {};
    }
    return rows;
}
std::optional<RayBombFlash> ray_bomb_flash(const GameSceneSnapshot& scene) noexcept {
    const auto& circle=scene.circle;
    if(!circle.active || circle.radius<=0) return {};
    // The smart bomb's detonation circle: its colour-math colour (0-31 per
    // channel), or white when it has none.
    std::array<float,3> colour{circle.red/31.F,circle.green/31.F,circle.blue/31.F};
    if(std::max({colour[0],colour[1],colour[2]})<.05F) colour={1,1,1};
    return RayBombFlash{float(circle.centre_x),float(circle.centre_y),float(circle.radius),colour};
}
std::vector<RayPointLight> ray_point_lights(const EyeCamera& eye,const Vec3& sun,
    const std::unordered_map<uint32_t,uint32_t>& objects,std::span<const SourceComputeModel> models,
    std::span<const DrawPacket> packets,std::span<const uint32_t> handles,const RayEmitterKind& emitter,
    std::optional<RayBombFlash> bomb,std::size_t limit,float radius_scale,uint32_t tick) {
    struct Candidate {RayPointLight light;double priority{};};
    std::vector<Candidate> found;
    const auto length=[](const std::array<float,3>& v){return std::sqrt(double(v[0])*v[0]+double(v[1])*v[1]+double(v[2])*v[2]);};
    const auto is_compute=[&](uint32_t key) {
        return std::any_of(models.begin(),models.end(),[&](const auto& model){return model.key==key;});
    };
    // `light` holds the look: colour, reach, hardness and so on.
    const auto place=[&](std::size_t packet,uint32_t key,RayPointLight light,float strength,float back=0) {
        if(packet>=packets.size()) return;
        // The object's origin and forward axis in native eye space. A
        // compute-route model keeps its placement in its projection pose
        // (camera-space translation and rotation rows, model units), not in
        // the packet matrix: map the pose through the packet like its points.
        const SourceComputeModel* compute=nullptr;
        for(const auto& model:models) if(model.key==key) {compute=&model;break;}
        auto rows=ray_model_rows(eye,packets[packet].model,compute?double(compute->units):256.);
        if(!rows) return;
        if(compute) {
            const auto& pose=compute->model.projection.continuous_poses[0];
            const auto apply=[&](const std::array<double,3>& p,bool point) {
                std::array<float,3> out{};
                for(unsigned row=0;row<3;++row)
                    out[row]=float((*rows)[row][0]*p[0]+(*rows)[row][1]*p[1]+(*rows)[row][2]*p[2]+(point?(*rows)[row][3]:0.F));
                return out;
            };
            const auto origin=apply({pose.translation[0],pose.translation[1],pose.translation[2]},true);
            const auto axis=apply({pose.row0[2],pose.row1[2],pose.row2[2]},false);
            for(unsigned row=0;row<3;++row) {(*rows)[row][3]=origin[row];(*rows)[row][2]=axis[row];}
        }
        std::array<float,3> forward{(*rows)[0][2],(*rows)[1][2],(*rows)[2][2]};
        const double forward_length=length(forward);
        if(forward_length>1e-9) for(auto& value:forward) value=float(value/forward_length);
        // `back` moves the light toward the object's tail (an engine).
        light.position={float((*rows)[0][3]-forward[0]*back),float((*rows)[1][3]-forward[1]*back),float((*rows)[2][3]-forward[2]*back)};
        // CPU-built packets (beam lines, sprites) may carry their geometry
        // already placed: light from the centre of their own vertices.
        const auto& geometry=packets[packet].geometry;
        if(!is_compute(key) && (!geometry.vertex_view().empty() || !geometry.line_view().empty())) {
            double centre[3]{};std::size_t count=0;
            for(const auto& list:{geometry.vertex_view(),geometry.line_view()}) for(const auto& vertex:list) {
                for(unsigned c=0;c<3;++c) centre[c]+=vertex.position[c];
                ++count;
            }
            if(const auto placed=model_eye_camera(eye,packets[packet].model);placed && count) {
                const auto& m=placed->view;
                for(auto& value:centre) value/=double(count);
                // Scene metres (+Y up, -Z forward) to native eye units.
                constexpr double sign[]{1,-1,-1};
                for(unsigned row=0;row<3;++row)
                    light.position[row]=float(sign[row]*(m[row]*centre[0]+m[4+row]*centre[1]+m[8+row]*centre[2]+m[12+row])*256.);
            }
        }
        if(light.projected) {
            // A pool on the ground stretched along the flight direction:
            // the forward axis flattened across the sun's rays.
            const double along=forward[0]*sun.x+forward[1]*sun.y+forward[2]*sun.z;
            std::array<float,3> axis{float(forward[0]-sun.x*along),float(forward[1]-sun.y*along),float(forward[2]-sun.z*along)};
            const double axis_length=length(axis);
            if(axis_length>1e-6) {for(auto& value:axis) value=float(value/axis_length);light.axis=axis;}
            else light.aspect=1;
        }
        if(light.projected && light.lift>0) {
            // The pool's centre (along the sun) stays put; the light is
            // traced at the lifted point (the shader measures the height
            // from the real light, `lift` below it).
            light.position={float(light.position[0]+sun.x*light.lift),float(light.position[1]+sun.y*light.lift),
                float(light.position[2]+sun.z*light.lift)};
        }
        light.radius*=radius_scale;
        for(auto& value:light.colour) value*=strength;
        if(const auto own=objects.find(key);own!=objects.end() && !light.self_shadowed) light.object=own->second;
        const double distance=length(light.position);
        if(!std::isfinite(distance)) return;
        found.push_back({light,double(strength)*light.radius/(distance+light.radius)});
    };
    // A ship's half wingspan in native units (compute models: projected
    // vertex X through the pose's X column), for its engine glow's size.
    const auto half_span=[&](uint32_t key)->float {
        for(const auto& model:models) if(model.key==key) {
            const auto& projection=model.model.projection;
            double widest=0;
            for(const auto& vertex:projection.continuous_vertices) {
                const auto& pose=projection.continuous_poses[std::min<std::size_t>(vertex.pose,projection.continuous_poses.size()-1)];
                const double column=std::sqrt(double(pose.row0[0])*pose.row0[0]+double(pose.row1[0])*pose.row1[0]
                    +double(pose.row2[0])*pose.row2[0]);
                widest=std::max(widest,std::abs(double(vertex.x))*column);
            }
            return model.units>0?float(widest*256./model.units):0.F;
        }
        return 0.F;
    };
    const auto coloured=[](std::array<float,3> colour,float radius,float hardness) {
        RayPointLight light;light.colour=colour;light.radius=radius;light.hardness=hardness;return light;
    };
    // Authored coloured lights by emitter class: each kind has its own
    // colour, reach and edge, chosen for the look rather than taken from the art.
    for(std::size_t index=0;index<packets.size() && index<handles.size();++index) {
        const uint32_t key=handles[index];
        if(is_source_shadow_pass(key) || key>=0x20000U) continue;
        const unsigned kind=emitter?emitter(key):EmitterNone;
        switch(kind) {
        case EmitterLaser: // enemy lasers: yellow, on the ground and anything they pass
            place(index,key,coloured({1.F,.82F,.3F},1000.F,0.F),1.F);break;
        case EmitterArwingLaser: // every Arwing shot (laser, twin laser, plasma): blue
            place(index,key,coloured({.3F,.5F,1.F},1000.F,0.F),1.F);break;
        case EmitterExplosion: // a large dark orange/red light over everything near
            place(index,key,coloured({.85F,.22F,.04F},3200.F,.2F),1.8F);break;
        case EmitterPlasma: // enemy ring shots and plasma balls: blue
            place(index,key,coloured({.3F,.5F,1.F},1000.F,0.F),1.F);break;
        case EmitterBoost: { // the boost flame: blue on the floor and walls (tunnels), never on ships
            auto flame=coloured({.3F,.55F,1.F},700.F,.2F);flame.background_only=true;
            place(index,key,flame,1.4F);
            break;
        }
        case EmitterEngine: {
            // The engine's glow on the ground at the tail of the ship's
            // shadow: round, soft from its centre, about the ship's width
            // across, fading as the ship climbs (red/yellow flicker). Traced
            // to the engine with the ship's own hull as an occluder, so the
            // side toward the ship is cut off along the hull, a little
            // differently from the sun's shadow (the light is much nearer).
            // Without ray tracing, renderers draw vr::ModernFrame::engine_glow.
            // The light sits just behind the tail (0.3 half spans behind the
            // origin) and is traced 2 half spans up the sun's rays from there:
            // the cut follows the fuselage's shadow, the wings' stays lit.
            constexpr float engine_back=.3F,engine_lift=2.F;
            const bool red=(tick/2U)%2U==0U;
            const float span=half_span(key);
            auto light=coloured(red?std::array<float,3>{1.F,.35F,.1F}:std::array<float,3>{1.F,.75F,.2F},
                span>0?span*2.F:40.F,0.F);
            // Its present size near the ground (low flight is 30-55 units
            // up; a half span is 35), three times as wide and gone by about
            // two wingspans up (4.5 half spans).
            light.projected=true;light.fade_height=span>0?span*4.5F:160.F;light.spread=3.F;
            light.self_shadowed=true;light.lift=span>0?span*engine_lift:40.F;
            place(index,key,light,1.6F,span>0?span*engine_back:20.F);
            break;
        }
        case EmitterBomb: // the smart bomb and enemy bombs: blue-white
            place(index,key,coloured({.6F,.75F,1.F},2000.F,.5F),.8F);break;
        default: break;
        }
    }
    if(bomb && bomb->radius>0) {
        // The detonation has no world position on the cartridge (a screen
        // circle): a large hard-edged light along the view ray through its centre.
        constexpr double depth=2400;
        RayPointLight light;
        light.position={float((bomb->centre_x-128.)/256.*depth),float((bomb->centre_y-112.)/256.*depth),float(depth)};
        light.radius=std::max(8000.F,bomb->radius*48.F)*radius_scale;light.hardness=.6F;light.colour=bomb->colour;
        for(auto& value:light.colour) value*=1.F;
        found.push_back({light,1e9});
    }
    std::sort(found.begin(),found.end(),[](const auto& a,const auto& b){return a.priority>b.priority;});
    std::vector<RayPointLight> result;
    for(std::size_t i=0;i<found.size() && i<limit;++i) result.push_back(found[i].light);
    return result;
}
}
