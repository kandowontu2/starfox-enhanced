#include "starfox/vr/openxr_session.hpp"
#include "starfox/vr/stereo_renderer.hpp"
#include "starfox/vr/frame_menu.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/game_model_pose.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/render/hud_layout.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include <cmath>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>
using namespace starfox::vr;
namespace {
void require(bool v,const char* message) {if(!v) throw std::runtime_error(message);}
void close_matrix(const Matrix4& a,const Matrix4& b,float tolerance=1e-4F) {
    for(unsigned i=0;i<16;++i) require(std::abs(a[i]-b[i])<tolerance,"Presentation matrix mismatch");
}
std::array<float,4> point(const Matrix4& m,std::array<float,4> p) {
    std::array<float,4> out{};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) out[r]+=m[c*4+r]*p[c];
    return out;
}
// Cockpit display smoothing through three ticks, measured through the real
// content path (interpolate_scene_poses + game_model_matrix) on static points.
void verify_cockpit_smoothing() {
    using namespace starfox;
    const auto rotation=[](double yaw,double roll) {
        const double cy=std::cos(yaw),sy=std::sin(yaw),cr=std::cos(roll),sr=std::sin(roll);
        const double m[9]{cy*cr,sr,-sy*cr, -cy*sr,cr,sy*sr, sy,0,cy}; // column-major
        simulation::MatrixQ15 out{};for(unsigned i=0;i<9;++i)out[i]=int16_t(std::lround(m[i]*32767));
        return out;
    };
    const auto wrap=[](double v) {auto w=std::fmod(v+32768.,65536.);if(w<0)w+=65536.;return int32_t(std::lround(w-32768.));};
    // Tick k: camera and pilot move by `velocity(k)`; view yaws and the ship rolls by `turn(k)`.
    const auto tick=[&](double k,auto position,auto turn,bool cut=false) {
        GameSceneSnapshot s;s.flow=simulation::GameFlowState::gameplay;s.pilot_tracking=true;s.player=7;
        const auto at=position(k);const auto angle=turn(k);
        s.camera={wrap(at[0]+(cut?9000:0)),wrap(at[1]),wrap(at[2]),0,0,0};
        s.view_matrix=rotation(angle,0);
        render::ObjectPresentationSnapshot pilot;pilot.generation=2;pilot.strategy_address=1;
        pilot.transform={wrap(at[0]+40+(cut?9000:0)),wrap(at[1]-30),wrap(at[2]+600),0,0,0};
        pilot.rotation_matrix=rotation(angle*.5,angle*2);s.pilot_reference=pilot;
        return s;
    };
    const auto eye=[&](const GameSceneSnapshot* older,const GameSceneSnapshot& previous,const GameSceneSnapshot& current,
        double alpha,const PresentationPreferences& prefs,std::array<int32_t,3> world) {
        auto before=previous,now=current;
        render::ObjectPresentationSnapshot object;object.generation=1;object.rotation_matrix=rotation(0,0);
        object.transform={world[0],world[1],world[2],0,0,0};
        now.objects.emplace_back();now.objects.back().handle=900;now.objects.back().presentation=object;
        before.transforms[900]=object;now.transforms[900]=object;
        const auto poses=interpolate_scene_poses(before,now,alpha,{});
        const auto model=*game_model_matrix(poses.back(),256);
        const auto pres=older?presentation_scene_matrix(*older,previous,current,alpha,prefs):presentation_scene_matrix(previous,current,alpha,prefs);
        const auto p=point(pres,{model[12],model[13],model[14],1});
        return std::array<double,3>{p[0],p[1],p[2]};
    };
    const auto distance=[](auto a,auto b) {return std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]);};
    const std::array<std::array<int32_t,3>,2> statics{{{32740,-200,9000},{-32000,400,3000}}};
    for(bool follow:{false,true}) {
        PresentationPreferences prefs;prefs.cockpit=true;prefs.follow_ship_rotation=follow;
        // Uniform translation across the 16-bit wrap: the B-spline is the linear path half a tick later.
        const auto steady=[](double k) {return std::array<double,3>{32700+60*k,-10*k,250*k};};
        const auto still=[](double) {return .3;};
        const auto s0=tick(0,steady,still),s1=tick(1,steady,still),s2=tick(2,steady,still);
        for(double alpha:{0.,.25,.5})for(const auto& x:statics) {
            require(distance(eye(&s0,s1,s2,alpha,prefs,x),eye(nullptr,s0,s1,alpha+.5,prefs,x))<1e-3,
                "Cockpit smoothing is not the linear path half a tick later");
            require(presentation_instrument_matrix(s0,s1,s2,alpha,prefs)==presentation_instrument_matrix(s0,s1,alpha+.5,prefs),
                "Smoothed cabin left the smoothed pilot");
        }
        // Accelerating, turning flight: position and velocity stay continuous across a
        // tick, where the linear path changes velocity abruptly.
        const auto speeding=[](double k) {return std::array<double,3>{40*k*k,-15*k*k,250*k+30*k*k};};
        const auto turning=[](double k) {return .02*k*k;};
        const auto t0=tick(0,speeding,turning),t1=tick(1,speeding,turning),t2=tick(2,speeding,turning),t3=tick(3,speeding,turning);
        constexpr double h=1e-2; // larger than Q15 rotation rounding, smaller than the curvature
        for(const auto& x:statics) {
            const auto end=eye(&t0,t1,t2,1,prefs,x),start=eye(&t1,t2,t3,0,prefs,x);
            require(distance(end,start)<1e-3,"Cockpit smoothing jumps at a tick");
            const auto before=eye(&t0,t1,t2,1-h,prefs,x),after=eye(&t1,t2,t3,h,prefs,x);
            std::array<double,3> v0{},v1{};for(unsigned i=0;i<3;++i) {v0[i]=(end[i]-before[i])/h;v1[i]=(after[i]-start[i])/h;}
            const auto lin_end=eye(nullptr,t1,t2,1,prefs,x),lin_before=eye(nullptr,t1,t2,1-h,prefs,x);
            const auto lin_start=eye(nullptr,t2,t3,0,prefs,x),lin_after=eye(nullptr,t2,t3,h,prefs,x);
            std::array<double,3> l0{},l1{};for(unsigned i=0;i<3;++i) {l0[i]=(lin_end[i]-lin_before[i])/h;l1[i]=(lin_after[i]-lin_start[i])/h;}
            require(distance(l0,l1)>10*distance(v0,v1) && distance(v0,v1)<.05*std::hypot(v0[0],v0[1],v0[2]),
                "Cockpit smoothing does not keep velocity continuous across ticks");
        }
        // A cut before previous restarts there; a cut before current jumps like the linear path.
        const auto c0=tick(0,steady,still,true);
        for(const auto& x:statics) {
            require(distance(eye(&c0,s1,s2,0,prefs,x),eye(nullptr,s1,s2,0,prefs,x))<1e-3,"Smoothing replayed a cut");
            require(distance(eye(&s0,c0,s2,.3,prefs,x),eye(nullptr,c0,s2,.3,prefs,x))<1e-3,"Smoothing crossed a cut");
        }
        auto off=prefs;off.cockpit=false;
        require(presentation_scene_matrix(t0,t1,t2,.4,off)==presentation_scene_matrix(t1,t2,.4,off)
            && presentation_instrument_matrix(t0,t1,t2,.4,off)==identity_matrix,"Smoothing changed a non-cockpit view");
    }
}
void verify_follow_ease() {
    using namespace starfox;
    const auto rotation=[](double yaw,double roll) {
        const double cy=std::cos(yaw),sy=std::sin(yaw),cr=std::cos(roll),sr=std::sin(roll);
        const double m[9]{cy*cr,sr,-sy*cr, -cy*sr,cr,sy*sr, sy,0,cy};
        simulation::MatrixQ15 out{};for(unsigned i=0;i<9;++i)out[i]=int16_t(std::lround(m[i]*32767));
        return out;
    };
    const auto roll_of=[](const simulation::MatrixQ15& m) {return std::atan2(double(m[1]),double(m[0]));};
    const auto same=[](const simulation::MatrixQ15& a,const simulation::MatrixQ15& b) {
        for(unsigned i=0;i<9;++i) if(std::abs(int(a[i])-int(b[i]))>3) return false;
        return true;
    };
    CockpitFollowEase ease;
    const auto start=rotation(.4,.3);
    require(same(ease.update(start,true,10),start),"Follow ease did not start at the source attitude");
    require(same(ease.update(start,true,10+1/90.),start),"Follow ease drifted from a still attitude");
    // A 40 degree bank eases in: one frame moves by 1-exp(-dt/tau), and it settles within about 0.5 s.
    const double bank=40*std::numbers::pi/180;
    const auto banked=rotation(0,bank);ease.reset();(void)ease.update(rotation(0,0),true,0);
    const double first=roll_of(ease.update(banked,true,1/90.));
    require(std::abs(first-bank*(1-std::exp(-1/90./CockpitFollowEase::time_constant_seconds)))<.002,"Follow ease rate wrong");
    double t=1/90.;simulation::MatrixQ15 shown{};
    for(;t<.6;t+=1/90.) shown=ease.update(banked,true,t);
    require(std::abs(roll_of(shown)-bank)<.01,"Follow ease did not settle");
    // A cut or a long gap snaps; a large jump never trails by more than 90 degrees.
    require(same(ease.update(rotation(0,-bank),false,t+=1/90.),rotation(0,-bank)),"Follow ease crossed a cut");
    require(same(ease.update(rotation(0,bank),true,t+1),rotation(0,bank)),"Follow ease resumed after a pause");
    ease.reset();(void)ease.update(rotation(0,0),true,0);
    const double far=170*std::numbers::pi/180;
    require(std::abs(roll_of(ease.update(rotation(0,far),true,1/90.)))>(far-std::numbers::pi/2)-.01,"Follow ease trailed by more than 90 degrees");
    // Passing the source attitude reproduces the unfollowed-ease scene; Follow OFF ignores it.
    GameSceneSnapshot a,b,c;
    for(auto* s:{&a,&b,&c}) {
        s->flow=simulation::GameFlowState::gameplay;s->pilot_tracking=true;s->player=7;s->view_matrix=rotation(.1,0);
        render::ObjectPresentationSnapshot pilot;pilot.generation=2;pilot.strategy_address=1;s->pilot_reference=pilot;
    }
    a.pilot_reference->rotation_matrix=rotation(0,.1);b.pilot_reference->rotation_matrix=rotation(0,.3);c.pilot_reference->rotation_matrix=rotation(0,.6);
    PresentationPreferences prefs;prefs.cockpit=prefs.follow_ship_rotation=true;
    const auto attitude=cockpit_follow_attitude(a,b,c,.4,prefs);require(attitude && attitude->continuous,"Follow attitude missing");
    close_matrix(presentation_scene_matrix(a,b,c,.4,prefs,&attitude->rotation),presentation_scene_matrix(a,b,c,.4,prefs));
    require(presentation_scene_matrix(a,b,c,.4,prefs,&start)!=presentation_scene_matrix(a,b,c,.4,prefs),"Follow attitude ignored");
    auto off=prefs;off.follow_ship_rotation=false;
    require(!cockpit_follow_attitude(a,b,c,.4,off),"Follow attitude outside Follow ship rotation");
    require(presentation_scene_matrix(a,b,c,.4,off,&start)==presentation_scene_matrix(a,b,c,.4,off),"Follow attitude changed Follow OFF");
}
template<typename T> T handle(uintptr_t n) {return reinterpret_cast<T>(n);}
struct Fake {
    std::deque<XrSessionState> events;
    std::deque<XrEventDataReferenceSpaceChangePending> origin_changes;
    std::vector<int> calls;
    bool render=true,tracked=true,fail_space=false,fail_locate=false;
    uint32_t submitted=99;
    XrTime submitted_time{};
    XrTime predicted_time{123456};
} fake;
XrResult XRAPI_PTR create(XrInstance,const XrSessionCreateInfo* info,XrSession* out) {
    require(info->next!=nullptr && info->systemId==7,"graphics binding/system not forwarded");
    fake.calls.push_back(1);*out=handle<XrSession>(2);return XR_SUCCESS;
}
XrResult XRAPI_PTR destroy(XrSession) {fake.calls.push_back(10);return XR_SUCCESS;}
XrResult XRAPI_PTR space(XrSession,const XrReferenceSpaceCreateInfo* info,XrSpace* out) {
    require(info->referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL && info->poseInReferenceSpace.orientation.w==1,
        "tracking space is not identity LOCAL");
    fake.calls.push_back(2);if(fake.fail_space) return XR_ERROR_RUNTIME_FAILURE;
    *out=handle<XrSpace>(3);return XR_SUCCESS;
}
XrResult XRAPI_PTR destroy_space(XrSpace) {fake.calls.push_back(9);return XR_SUCCESS;}
XrResult XRAPI_PTR modes(XrInstance,XrSystemId,XrViewConfigurationType type,uint32_t capacity,uint32_t* count,XrEnvironmentBlendMode* out) {
    require(type==XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,"not stereo blend query");
    *count=2;if(capacity) {out[0]=XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;out[1]=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;}
    return XR_SUCCESS;
}
XrResult XRAPI_PTR poll(XrInstance,XrEventDataBuffer* out) {
    require(out->type==XR_TYPE_EVENT_DATA_BUFFER,"event buffer type not reset");
    if(!fake.origin_changes.empty()) {
        const auto e=fake.origin_changes.front();fake.origin_changes.pop_front();
        std::memcpy(out,&e,sizeof(e));return XR_SUCCESS;
    }
    if(fake.events.empty()) return XR_EVENT_UNAVAILABLE;
    XrEventDataSessionStateChanged e{XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
    e.session=handle<XrSession>(2);e.state=fake.events.front();fake.events.pop_front();
    std::memcpy(out,&e,sizeof(e));return XR_SUCCESS;
}
XrResult XRAPI_PTR begin(XrSession,const XrSessionBeginInfo* info) {
    require(info->primaryViewConfigurationType==XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,"session not stereo");
    fake.calls.push_back(3);return XR_SUCCESS;
}
XrResult XRAPI_PTR stop(XrSession) {fake.calls.push_back(8);return XR_SUCCESS;}
XrResult XRAPI_PTR wait(XrSession,const XrFrameWaitInfo*,XrFrameState* out) {
    fake.calls.push_back(4);out->predictedDisplayTime=fake.predicted_time;out->predictedDisplayPeriod=11111;
    out->shouldRender=fake.render;return XR_SUCCESS;
}
XrResult XRAPI_PTR begin_frame(XrSession,const XrFrameBeginInfo*) {fake.calls.push_back(5);return XR_SUCCESS;}
XrResult XRAPI_PTR end_frame(XrSession,const XrFrameEndInfo* info) {
    fake.calls.push_back(7);fake.submitted=info->layerCount;fake.submitted_time=info->displayTime;
    require(info->environmentBlendMode==XR_ENVIRONMENT_BLEND_MODE_OPAQUE,"preferred opaque blend not selected");
    return XR_SUCCESS;
}
XrResult XRAPI_PTR views(XrSession,const XrViewLocateInfo* info,XrViewState* state,uint32_t capacity,uint32_t* count,XrView* out) {
    require(info->displayTime==fake.predicted_time && info->space==handle<XrSpace>(3) && capacity==2,"view location ignored predicted time/local space");
    fake.calls.push_back(6);if(fake.fail_locate) return XR_ERROR_RUNTIME_FAILURE;
    *count=2;state->viewStateFlags=fake.tracked?XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT:0;
    for(unsigned i=0;i<2;++i) {
        require(out[i].type==XR_TYPE_VIEW,"view not initialized");
        out[i].pose.orientation.w=1;out[i].pose.position.x=i?.032F:-.032F;
        out[i].fov={-.7F,.7F,.7F,-.7F};
    }
    return XR_SUCCESS;
}
SessionApi api() {return {create,destroy,space,destroy_space,modes,poll,begin,stop,wait,begin_frame,end_frame,views};}
void start(OpenXrSession& session) {
    int graphics_binding=1;
    require(session.initialize(handle<XrInstance>(1),7,&graphics_binding),"session initialization failed");
    require(!session.running() && !session.begin_frame(),"session ran before READY");
    fake.events.push_back(XR_SESSION_STATE_READY);
    require(session.poll_events() && session.running(),"READY did not start session");
}
unsigned acquisitions{},released{};
bool image_pending{};
XrResult XRAPI_PTR formats(XrSession,uint32_t capacity,uint32_t* count,int64_t* out) {
    *count=1;if(capacity) *out=43;return XR_SUCCESS;
}
XrResult XRAPI_PTR swap_create(XrSession,const XrSwapchainCreateInfo*,XrSwapchain* out) {
    *out=handle<XrSwapchain>(4);return XR_SUCCESS;
}
XrResult XRAPI_PTR swap_destroy(XrSwapchain) {return XR_SUCCESS;}
XrResult XRAPI_PTR images(XrSwapchain,uint32_t,uint32_t* count,XrSwapchainImageBaseHeader*) {
    *count=2;return XR_SUCCESS;
}
XrResult XRAPI_PTR acquire(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index) {
    ++acquisitions;*index=1;return XR_SUCCESS;
}
XrResult XRAPI_PTR image_wait(XrSwapchain,const XrSwapchainImageWaitInfo*) {
    return image_pending?XR_TIMEOUT_EXPIRED:XR_SUCCESS;
}
XrResult XRAPI_PTR release(XrSwapchain,const XrSwapchainImageReleaseInfo*) {
    ++released;return XR_SUCCESS;
}
}
int main() try {
    {
        using namespace starfox;
        const simulation::MatrixQ15 identity{32767,0,0,0,32767,0,0,0,32767};
        // Independent source-space 30-degree pitch, yaw, and bank fixtures.
        const std::array<simulation::MatrixQ15,3> rotations{{
            {32767,0,0,0,28378,16384,0,-16384,28378},
            {28378,0,-16384,0,32767,0,16384,0,28378},
            {28378,16384,0,-16384,28378,0,0,0,32767}}};
        GameSceneSnapshot now;now.flow=simulation::GameFlowState::gameplay;
        now.pilot_tracking=true;now.player=7;now.view_matrix=identity;
        render::ObjectPresentationSnapshot pilot;pilot.generation=2;pilot.strategy_address=1;
        PresentationPreferences off;off.cockpit=true;
        auto on=off;on.follow_ship_rotation=true;
        for(unsigned axis=0;axis<3;++axis) {
            pilot.rotation_matrix=rotations[axis];now.pilot_reference=pilot;
            auto off_rotation=presentation_scene_matrix(now,now,.5,off);
            off_rotation[12]=off_rotation[13]=off_rotation[14]=0;
            auto enlarged=identity_matrix;enlarged[0]=enlarged[5]=enlarged[10]=cockpit_world_scale(off);
            close_matrix(off_rotation,enlarged);
            const auto following=presentation_scene_matrix(now,now,.5,on);
            const auto moved=point(following,axis==2?std::array<float,4>{1,0,-4,0}:std::array<float,4>{0,0,-4,0});
            require(std::abs(moved[axis==0?1:axis==1?0:1]-(axis==2?.5F:-2.F)*cockpit_world_scale(on))<.01F,
                "Ship pitch/yaw/bank turned world in wrong direction");
            close_matrix(presentation_instrument_matrix(now,now,.5,on),identity_matrix);
            require(presentation_instrument_matrix(now,now,.5,off)!=identity_matrix,"Legacy rotating HUD changed");
        }
        // Exercise the real object interpolation + model matrix, including a
        // rotated source camera, instead of only cancelling hand-built matrices.
        now.view_matrix=rotations[1];pilot.rotation_matrix=rotations[2];
        pilot.transform={256,128,1024,0,0,0};now.pilot_reference=pilot;
        now.objects.emplace_back();now.objects[0].handle=7;now.objects[0].presentation=pilot;
        const auto poses=interpolate_scene_poses(now,now,.5,{});
        const auto model=*game_model_matrix(poses[0],256);
        for(unsigned world_scale:{0U,5U}) {
            on.world_scale=world_scale;
            const auto uncalibrated=presentation_scene_matrix(now,now,.5,on);
            auto local=multiply_matrix(uncalibrated,model);
            auto expected=identity_matrix;
            expected[0]=cockpit_world_scale(on)/256;expected[5]=expected[10]=-cockpit_world_scale(on)/256;
            for(unsigned i=0;i<3;++i)expected[12+i]=-cockpit_seat_m[i];
            close_matrix(local,expected,.001F);
            on.origin_x=25;on.origin_y=-15;on.origin_z=35;
            const auto scene=presentation_scene_matrix(now,now,.5,on);
            const auto instruments=presentation_instrument_matrix(now,now,.5,on);
            auto stable=identity_matrix;stable[12]=-.25F;stable[13]=.15F;stable[14]=-.35F;
            close_matrix(instruments,stable);
            // Calibration uses physical metres in the rotating ship frame.
            for(unsigned i=0;i<3;++i)
                require(std::abs(scene[12+i]-uncalibrated[12+i]-stable[12+i])<.001F,"Rotating pivot calibration scaled or changed axis");
            const float world=cockpit_world_scale(on);
            const auto pivot=point(model,{.25F*256/world,
                (.15F-cockpit_seat_m[1])*256/world,(-.35F-cockpit_seat_m[2])*256/world,1});
            const auto centered=point(scene,pivot);
            for(unsigned i=0;i<3;++i) require(std::abs(centered[i])<.001F,"Calibrated pilot pivot moved under rotation");
            // Application composes tracking on the left for every world pass.
            // Verify independent physical translation, head yaw, and stereo IPD.
            for(unsigned eye=0;eye<2;++eye) {
                XrView view{XR_TYPE_VIEW};view.pose.orientation={0,.258819F,0,.965926F};
                view.pose.position={.2F+(eye?.032F:-.032F),.1F,.05F};view.fov={-.7F,.7F,.7F,-.7F};
                const auto raw=view;
                const auto tracking=*eye_camera(view,1,.05F);
                const auto composed=multiply_matrix(tracking.view,scene);
                const auto expected_head=point(tracking.view,{0,0,0,1});
                const auto actual_head=point(composed,pivot);
                for(unsigned i=0;i<3;++i) require(std::abs(actual_head[i]-expected_head[i])<.001F,"Ship rotation altered local head pose/IPD");
                require(std::memcmp(&view,&raw,sizeof(view))==0,"Runtime eye pose was mutated");
            }
            on.origin_x=on.origin_y=on.origin_z=0;
        }
        on.world_scale=0;
        auto before=now;before.pilot_reference->rotation_matrix=identity;
        const auto endpoint=presentation_scene_matrix(now,now,1,on);
        require(presentation_scene_matrix(before,now,.25,on)!=endpoint,"Pilot rotation did not interpolate");
        for(unsigned reset=0;reset<7;++reset) {
            auto discontinuous=before;
            if(reset==0) discontinuous.pilot_reference.reset();
            if(reset==1) discontinuous.pilot_tracking=false;
            if(reset==2) discontinuous.player=8;
            if(reset==3) discontinuous.pilot_reference->generation++;
            if(reset==4) discontinuous.pilot_reference->strategy_address++;
            if(reset==5) discontinuous.flow=simulation::GameFlowState::title;
            if(reset==6) discontinuous.camera.x=20000;
            close_matrix(presentation_scene_matrix(discontinuous,now,.25,on),endpoint);
            close_matrix(presentation_instrument_matrix(discontinuous,now,.25,on),identity_matrix);
        }
        close_matrix(presentation_scene_matrix(before,now,std::numeric_limits<double>::quiet_NaN(),on),endpoint);
        for(unsigned inactive=0;inactive<4;++inactive) {
            auto scene=now;auto preferences=on;
            if(inactive==0) preferences.cockpit=false;
            if(inactive==1) scene.pilot_tracking=false;
            if(inactive==2) scene.pilot_reference.reset();
            if(inactive==3) scene.flow=simulation::GameFlowState::title;
            close_matrix(presentation_scene_matrix(scene,scene,.5,preferences),identity_matrix);
            close_matrix(presentation_instrument_matrix(scene,scene,.5,preferences),identity_matrix);
        }
        now.flow=simulation::GameFlowState::training;
        close_matrix(presentation_scene_matrix(now,now,.5,on),endpoint);
    }
    {
        using namespace starfox;
        GameSceneSnapshot before,now;before.flow=now.flow=simulation::GameFlowState::gameplay;
        before.view_matrix=now.view_matrix={32767,0,0,0,32767,0,0,0,32767};
        before.pilot_tracking=now.pilot_tracking=true;
        render::ObjectPresentationSnapshot pilot;
        pilot.transform={256,128,1024,0,0,0};pilot.rotation_matrix=now.view_matrix;
        pilot.generation=2;pilot.strategy_address=1;
        before.pilot_reference=now.pilot_reference=pilot;
        PresentationPreferences preferences;
        require(presentation_scene_matrix(before,now,.5,preferences)==identity_matrix,"Default camera changed");
        preferences.cockpit=true;
        const auto cockpit=presentation_scene_matrix(before,now,.5,preferences);
        const float ship=cockpit_ship_scale;
        // Q15 rounding scales with the enlarged world.
        require(std::abs(cockpit[12]+ship)<.001F*ship && std::abs(cockpit[13]-(.5F*ship-cockpit_seat_m[1]))<.001F*ship
            && std::abs(cockpit[14]-(4*ship-cockpit_seat_m[2]))<.001F*ship,"Pilot reference not in ship-scaled source view space");
        require(cockpit[0]==ship && cockpit[5]==ship && cockpit[10]==ship,"Cockpit world not enlarged to the cabin's ship");
        preferences.origin_x=25;
        const auto offset=presentation_scene_matrix(before,now,.5,preferences);
        require(std::abs(offset[12]-cockpit[12]+.25F)<.001F,"Calibrated origin changed wrong axis");
        const auto instruments=presentation_instrument_matrix(before,now,.5,preferences);
        require(std::abs(instruments[12]+.25F)<.001F,"Instruments did not stay attached to authored reference");
        preferences.world_scale=5;
        const auto large_offset=presentation_scene_matrix(before,now,.5,preferences);
        preferences.origin_x=0;
        const auto large_origin=presentation_scene_matrix(before,now,.5,preferences);
        require(std::abs(large_offset[12]-large_origin[12]+.25F)<.001F,
            "Physical cockpit calibration scaled with the world");
        preferences.world_scale=0;preferences.origin_x=25;
        now.pilot_tracking=false;
        require(presentation_scene_matrix(before,now,.5,preferences)==identity_matrix,"Script camera overridden");
        now.pilot_tracking=true;now.pilot_reference.reset();
        require(presentation_scene_matrix(before,now,.5,preferences)==identity_matrix,"Missing pilot reference invented");
        preferences.world_scale=5;
        const auto scaled=presentation_scene_matrix(before,now,.5,preferences);
        require(scaled[0]==2 && scaled[5]==2 && scaled[10]==2,"World scale missing outside the cockpit");
        for(bool ex:{false,true}) {
            const auto layout=layout_a_hud(ex);
            const int label_y=(ex?186:183)+layout[render::HudElement::shield].y;
            const int bar_y=(ex?182:178)+16+layout[render::HudElement::shield].y;
            require(label_y==190 && bar_y>=201,"Shield source composition lost");
            require(192+layout[render::HudElement::comms].y<label_y,"Portrait still overlaps shield");
        }
        simulation::SnesPpuState ppu;ppu.main_screen=16;ppu.oam.fill(0);
        ppu.oam[0]=100;ppu.oam[1]=180;ppu.oam[2]=0x61; // reticle in HUD band
        ppu.oam[4]=24;ppu.oam[5]=183;ppu.oam[6]=0x77; // shield label
        const auto layout=layout_a_hud(false);
        const auto world=source_sprite_packet(ppu,15,{},false,nullptr,nullptr,SourceSpritePass::world);
        const auto hud=source_sprite_packet(ppu,15,{},false,nullptr,&layout,SourceSpritePass::hud);
        require(world.geometry.vertices.size()==6 && world.geometry.vertices[0].position[0]==100
            && world.geometry.vertices[0].position[1]==180,"Aim sprite moved with HUD");
        require(hud.geometry.vertices.size()==6 && hud.geometry.vertices[0].position[0]==39
            && hud.geometry.vertices[0].position[1]==190,"HUD label group offset wrong");
    }
    {
        // No cartridge pixels are active: gameplay must not acquire an opaque
        // decorative panel. This exercises the actual production HUD builder.
        const starfox::assets::RomImage rom(std::vector<uint8_t>(0x8000));
        const auto symbols=starfox::assets::SymbolMap::parse(
            "MSCALECHARS $008000\nMARIOMSGS $008000\nFONT0WID $008000\n"
            "FONT0FON $008000\nFONT0TRN $008000\nFACEDATA $008000\n");
        starfox::render::ScaledTextRenderer text(rom,symbols);
        GameSceneSnapshot scene;scene.ppu=std::make_shared<starfox::simulation::SnesPpuState>();
        const auto packets=layout_a_instrument_packets(rom,symbols,scene,text);
        for(const auto& packet:packets)
            require(packet.geometry.vertex_view().empty() && packet.geometry.line_view().empty(),
                "Inactive source HUD added geometry that obscures the world");
        require(!layout_a_surface(true).geometry.vertex_view().empty(),
            "Separate menu panel was removed with gameplay backing");
    }
    {
        using Flow=starfox::simulation::GameFlowState;
        GameSceneSnapshot scene;
        for(auto flow:{Flow::title,Flow::controls_type,Flow::controls_choice,Flow::planet_select,
            Flow::planet_travel,Flow::ex_pregame_menu}) {
            scene.flow=flow;require(world_panel_scene(scene),"Authored interface escaped whole-scene quad");
        }
        for(auto flow:{Flow::gameplay,Flow::training,Flow::intro}) {
            scene.flow=flow;require(!world_panel_scene(scene),"Live scene was flattened with menus");
        }
        scene.paused=true;require(world_panel_scene(scene),"Pause lost interface quad");
        scene.paused=false;scene.briefing.active=true;
        require(world_panel_scene(scene),"Briefing lost interface quad");
        const auto far=panel_matrix(),near=overlay_panel_matrix();
        require(std::abs(near[14]+.75F)<1e-6F && std::abs(far[14]+1.75F)<1e-6F,"Interface depths changed");
        for(unsigned i:{0U,5U,12U,13U})
            require(std::abs(near[i]/near[14]-far[i]/far[14])<1e-6F,"Near overlay changed angular layout");
        require(source_ui_layer_matrix(112,96,false)->at(14)==-2.F,"Source aiming plane moved");
        std::array<XrView,2> views{};for(auto& view:views)view.pose.orientation.w=1;
        views[0].pose.position.x=-.032F;views[1].pose.position.x=.032F;
        WorldPanelAnchor anchor;
        require(anchor.pose(views).position.z==-1.75F,"Initial menu depth wrong");
        for(auto& view:views)view.pose.position.z=1;
        require(anchor.pose(views).position.z==-1.75F,"Stable menu followed head translation");
        require(anchor.pose(views,.75F).position.z==.25F,"Near family retained far anchor");
        for(auto& view:views)view.pose.position.z=2;
        require(anchor.pose(views,.75F).position.z==.25F,"Stable overlay followed head translation");
        for(auto& view:views)view.pose.position.z=3;
        require(anchor.pose(views,1.75F).position.z==1.25F,"Far family retained near anchor");
    }
    OpenXrSession session(api());start(session);
    OpenXrSwapchains chains({formats,swap_create,swap_destroy,images,acquire,image_wait,release});
    std::array<XrViewConfigurationView,2> config{};
    for(auto& eye:config) {eye.recommendedImageRectWidth=eye.recommendedImageRectHeight=100;
        eye.maxImageRectWidth=eye.maxImageRectHeight=100;eye.maxSwapchainSampleCount=1;}
    const std::array<int64_t,1> preferred{43};
    require(chains.initialize(session.handle(),config,preferred),"Projection initialization failed");
    OpenXrQuad quad({formats,swap_create,swap_destroy,images,acquire,image_wait,release});
    require(quad.initialize(session.handle(),43),"Quad initialization failed");
    StereoRenderer renderer(session,chains,true);
    unsigned draws=0,compositions=0;
    const auto draw=[&](unsigned,uint32_t,const EyeCamera&,XrTime) {++draws;return StereoRenderer::EyeResult::complete;};
    StereoRenderer::Composition composition{
        [&](const StereoFrame& frame,std::vector<const XrCompositionLayerBaseHeader*>& layers) {
            ++compositions;
            require(frame.views[0].pose.position.x==-.032F && frame.views[1].pose.position.x==.032F
                && frame.views[0].fov.angleLeft==-.7F,"Compositor raw pose/FOV changed");
            const auto state=quad.acquire();
            if(state==ImageWait::waiting) return StereoRenderer::EyeResult::pending;
            require(state==ImageWait::ready,"Quad acquire failed");
            if(compositions==1) {renderer.request_recenter();return StereoRenderer::EyeResult::pending;}
            require(quad.release(),"Quad release failed");
            XrPosef pose{};pose.orientation.w=1;pose.position.z=-1.75F;
            const auto* layer=quad.layer(session.space(),pose);
            require(layer && layer->size.width==1.15F,"Physical panel width changed");
            layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(layer));
            return StereoRenderer::EyeResult::complete;
        },[&]{return quad.cancel();}};
    const auto first=renderer.step_async(draw,1,.05F,std::nullopt,composition);
    require(first==StereoRenderer::Result::waiting && draws==2 && released==2,"Quad pending released early");
    require(renderer.step_async(draw,1,.05F,std::nullopt,composition)==StereoRenderer::Result::submitted
        && draws==2 && released==3 && fake.submitted==2,"Quad retry rerendered eyes or omitted layer");
    require(renderer.step_async(draw,1,.05F,std::nullopt,composition)==StereoRenderer::Result::submitted
        && draws==4 && fake.submitted==2,"Recenter prevented next stereo frame");
    image_pending=true;
    require(quad.acquire()==ImageWait::waiting && !quad.image_index(),"Quad timeout exposed image");
    require(quad.cancel()==ImageWait::waiting,"Quad cancellation released unwaited image");
    image_pending=false;require(quad.cancel()==ImageWait::ready && !quad.layer(session.space(),{}),"Cancelled quad submitted");
    verify_cockpit_smoothing();
    verify_follow_ease();
    std::cout<<"Presentation camera, source HUD grouping and fenced projection+quad tests passed (no headset).\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
