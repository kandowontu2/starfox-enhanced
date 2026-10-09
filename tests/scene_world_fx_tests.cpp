#include "starfox/render/scene_enhancements.hpp"
#include <iostream>
#include <set>
#include <limits>
#include <stdexcept>
using namespace starfox::render;
namespace {
unsigned checks{};
void require(bool ok,const char* message) {++checks;if(!ok) throw std::runtime_error(message);}
void equal(const SceneFxFrame& a,const SceneFxFrame& b) {
    require(a.camera==b.camera && a.data==b.data,"Flat projection changed scene data");
    for(unsigned n=0;n<scene_fx_capacity;++n) {
        require(a.motion_points[n].identity==b.motion_points[n].identity
            && a.motion_points[n].projected==b.motion_points[n].projected
            && a.motion_points[n].identified==b.motion_points[n].identified,"Flat projection changed birth/cell identity");
        require(!a.motion_previous[n].valid && !b.motion_previous[n].valid,"Projection invented accepted particle motion");
    }
}
// Independent copy of the pre-refactor flat projection equations. Event
// tracking has no projection callback; verify clipping/radius/focus/identity
// without generating the reference through project_scene_fx_frame.
template<class Project> SceneFxFrame reference(const SceneFxWorldFrame& world,Project project,
    float cx,float cy,float focal,float scale) {
    SceneFxFrame result;result.camera={cx,cy,focal,0};
    float player_depth=world.player?float(project(*world.player)[2]):0;
    for(unsigned n=0;n<world.count;++n) {
        const auto& q=world.points[n];const auto p=project(q.position);
        if(p[2]<32 || p[2]>10000 || (q.type==8 && p[2]<std::max(32.f,player_depth*.6f))) continue;
        const float size=std::clamp(focal*q.radius/float(p[2]),.5f*scale,(q.type==8?30.f:180.f)*scale);
        auto extra=q.extra;
        if(q.type==1) extra={float(p[0]),float(p[1]),float(p[2]),q.radius};
        if(q.type==8) extra={player_depth,std::max(64.f,player_depth*.5f),0,0};
        result.add({cx+focal*float(p[0]/p[2]),cy+focal*float(p[1]/p[2]),size,q.strength},
            {q.type,float(p[2]),q.age,0},extra,q.identity);
    }
    return result;
}
}
int main() try {
    const auto project=[](const auto& p) {return p;};
    SceneFxTracker world_tracker,flat_tracker;
    std::array<SceneFxEmitter,2> emitters{{{1,{0,0,500},false,false,true,255},{2,{40,-10,750},false,true,false,255}}};
    SceneFxWorldFrame saved;
    for(unsigned frame=0;frame<80;++frame) {
        const double time=double(frame)/120;
        emitters[0].position[0]=double(frame)*3;emitters[0].position[2]=500+double(frame)*2;
        emitters[1].explosion=frame>=8 && frame<36;
        if(frame==8) emitters[1].health=240;
        if(frame==24) emitters[1].id+=65536; // Recycled source slot, new generation.
        const unsigned modes=frame<60?255:frame<70?0:255,extras=modes?15:0;
        const auto world=world_tracker.update_world(modes,time,frame<40?1:2,emitters,{0,0,500},1,extras,400);
        const auto flat=flat_tracker.update(modes,time,frame<40?1:2,emitters,{0,0,500},1,project,200,112,256,1,extras,400);
        equal(flat,reference(world,project,200,112,256,1));
        for(unsigned scale:{1U,2U,6U,10U}) {
            const auto eye=[&](const auto& p) {return std::array<double,3>{p[0]-19,p[1]+7,p[2]-16};};
            equal(project_scene_fx_frame(world,eye,180,96,256*float(scale),float(scale)),
                reference(world,eye,180,96,256*float(scale),float(scale)));
        }
        require(world.count<=scene_fx_world_capacity,"Unbounded world point population");
        require(valid_scene_fx_world_frame(world),"Tracker emitted an invalid world point");
        if(frame==12) saved=world;
        if(frame==60) require(!world.active() && !world.player,"OFF retained world particles/focus");
    }
    require(saved.active(),"Retained world frame was empty");
    // Neither per-view adapter mutates the source snapshot.
    const auto retained=saved;
    (void)project_scene_fx_frame(saved,project,100,50,512,2);
    (void)project_scene_fx_frame(saved,[](const auto& p){return std::array<double,3>{p[0]+40,p[1]-10,p[2]+20};},100,50,512,2);
    require(saved==retained,"Per-view projection mutated the retained world frame");
    {
        SceneFxTracker tracker;
        std::array<SceneFxEmitter,6> explosions;
        for(unsigned n=0;n<explosions.size();++n) explosions[n]={n+1,{0,0,500},true,false,false,255};
        const auto all=tracker.update_world(255,0,1,explosions,{0,0,5000},1,3,400);
        require(all.count==61 && all.count>scene_fx_capacity,"World frame culled before independent-eye projection");
        require(project_scene_fx_frame(all,project,200,112,256,1).camera[3]==48,"Visible flat point limit changed");
        const auto alternative_eye=[](const auto& p) {auto q=p;if(p[2]<1000) q[2]=-32;return q;};
        const auto visible=project_scene_fx_frame(all,alternative_eye,200,112,256,1);
        require(visible.camera[3]==27,"Other eye lost weather hidden by centre-eye compaction");
        for(unsigned n=0;n<27;++n) require(visible.data[n*3+1][0]==3,"Other eye retained a clipped particle");
        require(tracker.update_world(255,0,1,explosions,{0,0,5000},1,3,400)==all,"Held world events spawned twice");
        const auto later=tracker.update_world(255,.1,1,explosions,{0,0,5000},1,3,400);
        const auto reset=tracker.update_world(255,.1,2,explosions,{0,0,5000},1,3,400);
        require(later.points[6].identity==all.points[6].identity && later.points[6].age>0,"Live ring lost birth/age");
        require(reset.points[6].identity!=all.points[6].identity && reset.points[6].age==0,"Scene reset borrowed old births");
        const auto rewind=tracker.update_world(255,.05,2,explosions,{0,0,5000},1,3,400);
        require(rewind.points[6].identity!=reset.points[6].identity,"Rewind reused a birth identity");
    }
    {
        SceneFxWorldFrame world;world.player=std::array<double,3>{0,0,500};
        for(unsigned type=0;type<=8;++type) for(double z:{-100.,31.,32.,300.,500.,10000.,10001.})
            world.add({10,-20,z},type==8?1000.f:260.f,.7f,float(type),.1f,{.2f,.5f,1,0},{type,1,0,0});
        equal(project_scene_fx_frame(world,project,200,112,256,1),reference(world,project,200,112,256,1));
        world.count=scene_fx_world_capacity+1;bool rejected=false;
        try {(void)project_scene_fx_frame(world,project,0,0,256,1);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"Malformed world count read past its storage");
    }
    for(unsigned fault=0;fault<12;++fault) {
        SceneFxWorldFrame invalid;invalid.add({0,0,500},5,.7f,3,.1f,{.7f,.85f,1,0},{3,1,0,0});
        switch(fault) {
        case 0: invalid.points[0].position[0]=std::numeric_limits<double>::quiet_NaN();break;
        case 1: invalid.points[0].position[2]=1.e10;break;
        case 2: invalid.points[0].radius=-1;break;
        case 3: invalid.points[0].strength=2;break;
        case 4: invalid.points[0].type=9;break;
        case 5: invalid.points[0].type=1.5f;break;
        case 6: invalid.points[0].age=-.1f;break;
        case 7: invalid.points[0].age=std::numeric_limits<float>::infinity();break;
        case 8: invalid.points[0].extra[1]=std::numeric_limits<float>::quiet_NaN();break;
        case 9: invalid.player=std::array<double,3>{0,std::numeric_limits<double>::infinity(),500};break;
        case 10: invalid.count=scene_fx_world_capacity+1;break;
        case 11: invalid.points[0].type=1;invalid.points[0].radius=0;break;
        }
        require(!valid_scene_fx_world_frame(invalid),"Malformed native world payload accepted");
        bool rejected=false;
        try {(void)project_scene_fx_frame(invalid,project,0,0,256,1);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"Flat adapter consumed malformed world payload");
    }
    std::cout<<"Unprojected scene events, independent-eye clipping, flat projection/identity parity, held/OFF/generation/cut/rewind passed ("<<checks<<" checks).\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
