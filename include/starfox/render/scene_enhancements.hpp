#pragma once
#include "starfox/render/global_enhancements.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/scene_motion_history.hpp"
#include <bit>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <span>
#include <optional>
#include <stdexcept>

namespace starfox::render {
inline constexpr unsigned scene_fx_capacity=48;
inline constexpr std::array<std::string_view,4> scene_enhancement_names{
    "EXPLOSION SHOCKWAVES","WEAPON LIGHTING","EXHAUST TRAILS","STAGE WEATHER"};
struct SceneFxFrame {
    std::array<float,4> camera{}; // Stored cx,cy,focal,count.
    std::array<std::array<float,4>,scene_fx_capacity*3> data{};
    std::array<SceneMotionPoint,scene_fx_capacity> motion_points{};
    std::array<SceneMotionSample,scene_fx_capacity> motion_previous{};
    bool active() const {return camera[3]>0;}
    // Surface lighting changes radiance only. Other scene effects add or
    // displace visible coverage and need their own velocity correspondence.
    bool surface_lighting_only() const {
        const float count=camera[3];
        if(!std::isfinite(count) || count<0 || count>scene_fx_capacity || std::floor(count)!=count) return false;
        for(unsigned n=0;n<unsigned(count);++n) if(data[n*3+1][0]!=1) return false;
        return true;
    }
    void add(std::array<float,4> screen,std::array<float,4> kind,std::array<float,4> extra,
        std::array<std::int64_t,4> identity={}) {
        const unsigned n=unsigned(camera[3]);if(n>=scene_fx_capacity) return;
        data[n*3]=screen;data[n*3+1]=kind;data[n*3+2]=extra;camera[3]+=1;
        motion_points[n]={identity,{screen[0],screen[1],kind[1]},identity!=std::array<std::int64_t,4>{}};
        motion_previous[n]={};
    }
    void eye(float x,float convergence) {
        camera[0]+=camera[2]*x/convergence;
        for(unsigned n=0;n<unsigned(camera[3]);++n) {
            auto current=motion_points[n];
            if(!scene_motion_eye(current,motion_previous[n],x,camera[2],convergence)) motion_previous[n]={};
            const float z=data[n*3+1][1];
            data[n*3][0]+=camera[2]*x*(1.f/convergence-1.f/z);
            motion_points[n].projected={data[n*3][0],data[n*3][1],z};
            if(data[n*3+1][0]==1) data[n*3+2][0]-=x;
        }
    }
};
struct SceneFxEmitter {
    std::uint64_t id{};
    std::array<double,3> position{};
    bool explosion{},weapon{},player{};
    unsigned health{256}; // Unknown for synthetic/non-damage emitters.
};
// Transient event state BEFORE any screen/eye projection. The maximum live
// population is 6 lights + 4 rings + 24 debris/sparks + 24 exhaust/heat points
// + 27 weather cells. Keep all of them here: clipping must happen independently
// in each eye before selecting its 48 visible points, not at the stereo centre.
inline constexpr unsigned scene_fx_world_capacity=96;
static_assert(scene_fx_world_capacity>=6+4+24+24+27);
struct SceneFxWorldPoint {
    std::array<double,3> position{};
    float radius{},strength{},type{},age{};
    std::array<float,4> extra{};
    std::array<std::int64_t,4> identity{};
    bool operator==(const SceneFxWorldPoint&) const = default;
};
struct SceneFxWorldFrame {
    std::array<SceneFxWorldPoint,scene_fx_world_capacity> points{};
    unsigned count{};
    std::optional<std::array<double,3>> player;
    bool active() const noexcept {return count>0;}
    void add(std::array<double,3> position,float radius,float strength,float type,float age,
        std::array<float,4> extra,std::array<std::int64_t,4> identity) {
        if(count<points.size()) points[count++]={position,radius,strength,type,age,extra,identity};
    }
    bool operator==(const SceneFxWorldFrame&) const = default;
};
// Bounded, finite payload contract shared by native GPU consumers and
// the ordinary projection adapter. Validation never mutates a retained frame.
inline bool valid_scene_fx_world_frame(const SceneFxWorldFrame& frame) noexcept {
    if(frame.count>frame.points.size()) return false;
    const auto valid_position=[](const std::array<double,3>& p) {
        return std::all_of(p.begin(),p.end(),[](double value){return std::isfinite(value) && std::abs(value)<=1.e9;});
    };
    if(frame.player && !valid_position(*frame.player)) return false;
    for(unsigned n=0;n<frame.count;++n) {
        const auto& point=frame.points[n];
        if(!valid_position(point.position) || !std::isfinite(point.radius) || point.radius<0 || point.radius>1.e8F
            || !std::isfinite(point.strength) || point.strength<0 || point.strength>1
            || !std::isfinite(point.type) || point.type<0 || point.type>8 || std::floor(point.type)!=point.type
            || (point.type==1 && point.radius==0)
            || !std::isfinite(point.age) || point.age<0 || point.age>1.e4F) return false;
        for(float value:point.extra) if(!std::isfinite(value) || std::abs(value)>1.e8F) return false;
    }
    return true;
}
// The ordinary flat renderer's existing projection policy. Native calibrated
// consumers use the unprojected frame above; they must not call this helper to
// invent a shared centre-eye image or ignore asymmetric tracked-eye matrices.
template<class Project> SceneFxFrame project_scene_fx_frame(const SceneFxWorldFrame& world,
    Project project,float cx,float cy,float focal,float scale) {
    SceneFxFrame out;out.camera={cx,cy,focal,0};
    if(!valid_scene_fx_world_frame(world)) throw std::invalid_argument("Invalid world-space effect payload");
    const float player_depth=world.player?float(project(*world.player)[2]):0;
    for(unsigned n=0;n<world.count;++n) {
        const auto& point=world.points[n];const auto p=project(point.position);
        if(p[2]<32 || p[2]>10000) continue;
        // Old exhaust can approach the camera rapidly in boosted flight;
        // discard it before perspective turns a local plume into a screen wash.
        if(point.type==8 && p[2]<std::max(32.f,player_depth*.6f)) continue;
        const float size=std::clamp(focal*point.radius/float(p[2]),.5f*scale,(point.type==8?30.f:180.f)*scale);
        auto extra=point.extra;
        if(point.type==1) extra={float(p[0]),float(p[1]),float(p[2]),point.radius};
        if(point.type==8) extra={player_depth,std::max(64.f,player_depth*.5f),0,0};
        out.add({cx+focal*float(p[0]/p[2]),cy+focal*float(p[1]/p[2]),size,point.strength},
            {point.type,float(p[2]),point.age,0},extra,point.identity);
    }
    return out;
}
// Transient host visuals, deliberately outside game state/RNG. A rewind,
// scene change or toggle clears histories, rather than replaying stale events.
class SceneFxTracker {
    struct Node {std::array<double,3> position;double born;std::array<double,3> velocity{};std::uint64_t identity{};};
    std::uint64_t sequence_{};
    std::deque<Node> rings_,trail_;
    struct Particle {Node motion;bool debris;};
    std::deque<Particle> particles_;
    std::unordered_map<std::uint64_t,unsigned> health_;
    std::unordered_set<std::uint64_t> exploding_;
    double last_time_{-1},trail_time_{-1};std::uint64_t epoch_{};unsigned settings_{},extras_{};
    std::array<double,3> previous_player_{};
    double player_time_{-1};
public:
    SceneFxWorldFrame update_world(unsigned settings,double time,std::uint64_t epoch,
        std::span<const SceneFxEmitter> emitters,const std::array<double,3>& camera,
        unsigned weather,unsigned extras=0,float gravity=0) {
        if(!(settings|extras) || settings!=settings_ || extras!=extras_ || epoch!=epoch_ || time<last_time_ || time-last_time_>1.) {
            rings_.clear();trail_.clear();exploding_.clear();trail_time_=-1;player_time_=-1;
            particles_.clear();health_.clear();
        }
        settings_=settings;extras_=extras;epoch_=epoch;last_time_=time;
        SceneFxWorldFrame out;if(!(settings|extras)) return out;
        std::unordered_set<std::uint64_t> now;
        std::unordered_map<std::uint64_t,unsigned> health_now;
        const auto hash=[](std::uint32_t n) {n^=n>>16;n*=0x7feb352dU;n^=n>>15;n*=0x846ca68bU;return n^(n>>16);};
        unsigned lights=0;
        const auto emitted=[&](const auto& position,float radius,float strength,float type,float age,std::array<float,4> extra,
            std::array<std::int64_t,4> identity) {
            out.add(position,radius,strength,type,age,extra,identity);
        };
        for(const auto& e:emitters) {
            if(e.player) out.player=e.position;
            const auto previous=health_.find(e.id);
            const bool damaged=e.health<=255 && previous!=health_.end() && e.health<previous->second;
            if(e.health<=255) health_now[e.id]=e.health;
            if((extras&3) && (!e.weapon || e.explosion) && (damaged || (e.explosion && !exploding_.contains(e.id)))) {
                const unsigned count=(extras&3)*4;
                for(unsigned i=0;i<count;++i) {
                    const auto seed=hash(std::uint32_t(e.id)^std::uint32_t(e.id>>32)^e.health*733U^i*1973U);
                    const bool debris=i%4==0;
                    std::array<double,3> direction{double(int(seed&1023)-512),double(int((seed>>10)&1023)-512),double(int((seed>>20)&1023)-512)};
                    const double length=std::max(1.,std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]));
                    for(auto& v:direction) v*=double(debris?130:360)/length;
                    particles_.push_back({{e.position,time,direction,++sequence_},debris});
                    if(particles_.size()>24) particles_.pop_front();
                }
            }
            if(e.explosion) {
                now.insert(e.id);
                if((settings&3) && !exploding_.contains(e.id)) {
                    rings_.push_back({e.position,time,{},++sequence_});if(rings_.size()>4) rings_.pop_front();
                }
            }
            if(((settings>>2)&3) && (e.weapon || e.explosion) && lights<6) {
                emitted(e.position,e.explosion?600.f:260.f,float((settings>>2)&3)/3.f,1,e.explosion?1.f:0.f,{},
                    {1,std::bit_cast<std::int64_t>(e.id),0,0});++lights;
            }
            if((((settings>>4)&3) || ((extras>>2)&3)) && e.player && !e.explosion && time>player_time_) {
                std::array<double,3> velocity{};
                if(player_time_>=0 && time-player_time_<.2) for(unsigned axis=0;axis<3;++axis)
                    velocity[axis]=std::clamp(std::remainder(e.position[axis]-previous_player_[axis],65536.)/(time-player_time_)*.85,-10000.,10000.);
                if(trail_time_<0 || time-trail_time_>=1./30.) {
                    trail_.push_back({e.position,time,velocity,++sequence_});trail_time_=time;
                }
                previous_player_=e.position;player_time_=time;
            }
        }
        exploding_=std::move(now);
        health_=std::move(health_now);
        while(!particles_.empty() && time-particles_.front().motion.born>.7) particles_.pop_front();
        while(!rings_.empty() && time-rings_.front().born>.8) rings_.pop_front();
        while(!trail_.empty() && (time-trail_.front().born>.4 || trail_.size()>12)) trail_.pop_front();
        for(const auto& n:rings_) {
            const float age=float(time-n.born);
            emitted(n.position,40+age*900,(1-age/.8f)*float(settings&3)/3.f,0,age,{0,0,0,0},
                {0,std::bit_cast<std::int64_t>(n.identity),0,0});
        }
        for(const auto& particle:particles_) {
            const auto& n=particle.motion;const float age=float(time-n.born);
            auto position=n.position;
            for(unsigned axis=0;axis<3;++axis) position[axis]+=n.velocity[axis]*age;
            position[1]+=.5*gravity*age*age;
            emitted(position,particle.debris?4.f:2.5f,1-age/.7f,particle.debris?7.f:6.f,age,
                particle.debris?std::array<float,4>{.24f,.18f,.13f,0}:std::array<float,4>{1.f,.65f,.18f,0},
                {particle.debris?7:6,std::bit_cast<std::int64_t>(n.identity),0,0});
        }
        for(const auto& n:trail_) {
            const float age=float(time-n.born);
            auto position=n.position;for(unsigned axis=0;axis<3;++axis) position[axis]+=n.velocity[axis]*age;
            if((settings>>4)&3) emitted(position,6+age*12,(1-age/.4f)*float((settings>>4)&3)/9.f,2,age,{.15f,.55f,1.f,0},
                {2,std::bit_cast<std::int64_t>(n.identity),0,0});
            if(((extras>>2)&3) && age>.035f)
                emitted(position,10+age*25,(1-age/.4f)*float((extras>>2)&3)/3.f,8,age,{},
                    {8,std::bit_cast<std::int64_t>(n.identity),0,0});
        }
        const unsigned quality=(settings>>6)&3;
        if(weather && quality) {
            constexpr double cell=480.;
            const double fall=time*(weather==1?170.:weather==2?750.:75.);
            for(int z=-1;z<=1;++z) for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
                const auto gx=int(std::floor(camera[0]/cell))+x,gy=int(std::floor((camera[1]-fall)/cell))+y,gz=int(std::floor(camera[2]/cell))+z;
                const auto seed=hash(std::uint32_t(gx)*1973U+std::uint32_t(gy)*9277U+std::uint32_t(gz)*26699U);
                if(seed%3>=quality) continue;
                std::array<double,3> position{(gx+double(seed&1023)/1024.)*cell,
                    (gy+double((seed>>10)&1023)/1024.)*cell+fall,
                    (gz+double((seed>>20)&1023)/1024.)*cell};
                if(weather!=2) position[0]+=std::sin(time+double(seed&255)) * 30.;
                emitted(position,weather==2?3.f:5.f,.65f,weather==1?3.f:weather==2?4.f:5.f,0,
                    weather==3?std::array<float,4>{1.f,.4f,.12f,0}:std::array<float,4>{.7f,.85f,1.f,0},
                    {weather==1?3:weather==2?4:5,gx,gy,gz});
            }
        }
        return out;
    }
    template<class Project> SceneFxFrame update(unsigned settings,double time,std::uint64_t epoch,
        std::span<const SceneFxEmitter> emitters,const std::array<double,3>& camera,
        unsigned weather,Project project,float cx,float cy,float focal,float scale,unsigned extras=0,float gravity=0) {
        return project_scene_fx_frame(update_world(settings,time,epoch,emitters,camera,weather,extras,gravity),
            project,cx,cy,focal,scale);
    }
};
template<class Sampler> inline float scene_channel(Sampler sample,const SceneFxFrame& fx,float x,float y,
    unsigned channel,float scale,float depth,float normal_x,float normal_y,float normal_z) {
    using std::sqrt;using std::abs;using std::sin;using std::cos;
#define S_MIN std::min
#define S_MAX std::max
#define S_SAMPLE(a,b) sample(a,b)
#define S_HEAT_SAMPLE(a,b,f,r,z) sample(a,b,f,r,z)
#define S_DATA(i,j) fx.data[i][j]
#define S_CAMERA(i) fx.camera[i]
#include "scene_enhancements.inc"
#undef S_CAMERA
#undef S_DATA
#undef S_SAMPLE
#undef S_HEAT_SAMPLE
#undef S_MIN
#undef S_MAX
}
inline void apply_scene_enhancements(const SceneFxFrame& fx,const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba,std::vector<std::uint8_t>& scratch,
    const SurfaceBuffer* surfaces,int ox,int oy) {
    if(!fx.active() || !frame.layer_tags_enabled() || rgba.size()!=frame.pixels().size()*4) return;
    scratch=rgba;const auto w=frame.stored_width(),h=frame.stored_height();
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const auto i=std::size_t(y)*w+x;if(frame.layer_tags()[i]==1) continue;
        SurfaceSample s{};
        if(surfaces && int(x)>=ox && int(y)>=oy && int(x)-ox<int(surfaces->width()) && int(y)-oy<int(surfaces->height())) {
            s=surfaces->get(int(x)-ox,int(y)-oy);
            if(!s.valid || s.palette_index!=frame.pixels()[i]) s={};
        }
        for(unsigned c=0;c<3;++c) {
            const auto sample=[&](float sx,float sy,float focus=0,float range=0,float plume=0) {
                sx=std::clamp(sx,0.f,float(w-1));sy=std::clamp(sy,0.f,float(h-1));
                const unsigned ax=unsigned(sx),ay=unsigned(sy);const float tx=sx-ax,ty=sy-ay;float value=0;
                for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                    auto n=std::size_t(std::min(ay+dy,h-1))*w+std::min(ax+dx,w-1);
                    const int xx=int(n%w),yy=int(n/w);
                    if(range>0 && surfaces && xx>=ox && yy>=oy && xx-ox<int(surfaces->width()) && yy-oy<int(surfaces->height())) {
                        const auto& tap=surfaces->get(xx-ox,yy-oy);
                        if(tap.valid && tap.palette_index==frame.pixels()[n] &&
                            (tap.depth+8<plume || std::abs(tap.depth-focus)<range)) n=i;
                    }
                    if(frame.layer_tags()[n]==1) n=i;
                    value+=scratch[n*4+c]*(dx?tx:1-tx)*(dy?ty:1-ty);
                }
                return value/255.f;
            };
            rgba[i*4+c]=std::uint8_t(scene_channel(sample,fx,float(x),float(y),c,float(frame.draw_scale()),
                s.valid?s.depth:0,s.normal_x,s.normal_y,s.normal_z)*255.f+.5f);
        }
    }
}
}
