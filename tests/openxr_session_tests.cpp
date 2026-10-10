#include "starfox/vr/openxr_session.hpp"
#include "starfox/vr/stereo_renderer.hpp"
#include <cstring>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace starfox::vr;
namespace {
void require(bool v,const char* message) {if(!v) throw std::runtime_error(message);}
template<typename T> T handle(uintptr_t n) {return reinterpret_cast<T>(n);}
struct Fake {
    std::deque<XrSessionState> events;
    std::deque<XrEventDataReferenceSpaceChangePending> origin_changes;
    std::vector<int> calls;
    bool render=true,tracked=true,fail_space=false,fail_locate=false,fail_exit=false;
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
XrResult XRAPI_PTR request_exit(XrSession) {fake.calls.push_back(11);return fake.fail_exit?XR_ERROR_SESSION_NOT_RUNNING:XR_SUCCESS;}
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
    require(!info->next && !state->next,"ordinary headset path received a display-specific rig");
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
SessionApi api() {return {create,destroy,space,destroy_space,modes,poll,begin,stop,wait,begin_frame,end_frame,views,request_exit};}
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
        OpenXrSession s(api());start(s);
        XrEventDataReferenceSpaceChangePending change{XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING};
        change.session=s.handle();change.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
        change.changeTime=123457;fake.origin_changes.push_back(change);
        require(s.poll_events(),"origin event failed");
        auto frame=s.begin_frame();require(frame && !frame->tracking_origin_changed,"future origin applied early");
        require(s.end_frame(),"end future frame");
        change.changeTime=123456;fake.origin_changes.push_back(change);
        require(s.poll_events(),"due origin event failed");
        frame=s.begin_frame();require(frame && frame->tracking_origin_changed,"due origin not signaled");
        require(s.end_frame(),"end due frame");
        frame=s.begin_frame();require(frame && !frame->tracking_origin_changed,"origin signaled twice");
        require(s.end_frame(),"end repeated frame");
        fake.predicted_time=123457;fake.render=false;
        frame=s.begin_frame();require(frame && frame->tracking_origin_changed && !frame->should_render,
            "queued origin lost on invisible frame");
        require(s.end_frame(),"end invisible origin frame");
    }
    fake=Fake{};
    {
        OpenXrSession s(api());start(s);
        auto frame=s.begin_frame();require(frame && frame->should_render,"tracked frame not renderable");
        require(frame->display_period==11111 && frame->views[0].pose.position.x<0 && frame->views[1].pose.position.x>0,"stereo timing/eyes lost");
        require(!s.begin_frame() && !s.poll_events(),"double begin/event polling during frame accepted");
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        const XrCompositionLayerBaseHeader* layers[]{reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer)};
        require(s.end_frame(layers) && fake.submitted==1 && fake.submitted_time==123456,"projection not submitted at predicted time");
        require(!s.end_frame(),"double frame end accepted");
        fake.render=false;frame=s.begin_frame();require(frame && !frame->should_render,"invisible frame rendered");
        require(s.end_frame(layers) && fake.submitted==0,"invisible frame submitted layers");
        fake.render=true;fake.tracked=false;frame=s.begin_frame();require(frame && !frame->should_render,"invalid tracking rendered");
        require(s.end_frame(layers) && fake.submitted==0,"untracked frame submitted layers");
        fake.tracked=true;fake.fail_locate=true;
        require(!s.begin_frame() && fake.submitted==0 && fake.calls.back()==7,"locate failure leaked a begun frame");
        fake.fail_locate=false;
        fake.events.push_back(XR_SESSION_STATE_STOPPING);
        require(s.poll_events() && !s.running() && fake.calls.back()==8,"STOPPING did not end session");
        fake.events.push_back(XR_SESSION_STATE_READY);require(s.poll_events() && s.running(),"session could not restart");
        fake.events.push_back(XR_SESSION_STATE_LOSS_PENDING);
        require(s.poll_events() && s.exit_requested() && !s.begin_frame(),"loss did not stop frame production");
    }
    require(fake.calls[fake.calls.size()-2]==9 && fake.calls.back()==10,"space/session destruction order wrong");
    // Quit to Steam: xrRequestExitSession, then the runtime's STOPPING and
    // EXITING events end the session cleanly (xrEndSession in between).
    fake=Fake{};
    {
        OpenXrSession s(api());int binding=1;
        require(!s.request_exit(),"exit request accepted without a session");
        require(s.initialize(handle<XrInstance>(1),7,&binding),"exit fixture init failed");
        fake.events.push_back(XR_SESSION_STATE_READY);fake.events.push_back(XR_SESSION_STATE_FOCUSED);
        require(s.poll_events() && s.running() && !s.exit_requested() && !s.exit_in_progress(),"exit fixture not running");
        require(s.request_exit() && s.exit_in_progress() && !s.exit_requested()
            && fake.calls.back()==11 && s.running(),"running session did not request exit from the runtime");
        const auto calls=fake.calls.size();
        require(s.request_exit() && fake.calls.size()==calls,"exit request repeated");
        fake.events.push_back(XR_SESSION_STATE_STOPPING);
        require(s.poll_events() && !s.running() && !s.exit_requested() && fake.calls.back()==8,
            "STOPPING after exit request did not end the session");
        fake.events.push_back(XR_SESSION_STATE_EXITING);
        require(s.poll_events() && s.exit_requested() && !s.begin_frame(),"EXITING did not finish the exit");
    }
    fake=Fake{};
    {
        OpenXrSession s(api());int binding=1;
        require(s.initialize(handle<XrInstance>(1),7,&binding),"idle exit fixture init failed");
        require(s.request_exit() && s.exit_requested() && fake.calls.back()!=11,
            "a session that never began must exit without asking the runtime");
    }
    fake=Fake{};fake.fail_exit=true;
    {
        OpenXrSession s(api());int binding=1;
        require(s.initialize(handle<XrInstance>(1),7,&binding),"refused exit fixture init failed");
        fake.events.push_back(XR_SESSION_STATE_READY);require(s.poll_events() && s.running(),"refused exit fixture not running");
        require(!s.request_exit() && s.exit_requested() && !s.running()
            && s.status().find("Request OpenXR session exit")!=std::string::npos,"refused exit request not reported");
    }
    fake=Fake{};fake.fail_space=true;
    {
        OpenXrSession s(api());int binding=1;
        require(!s.initialize(handle<XrInstance>(1),7,&binding) && s.handle()==XR_NULL_HANDLE,"failed initialization retained session");
        require(fake.calls==std::vector<int>({1,2,10}),"failed space creation did not roll back session");
    }
    fake=Fake{};
    {
        OpenXrSession s(api());start(s);require(s.begin_frame().has_value(),"cleanup fixture failed");
        s.close();s.close();
        require(fake.calls==std::vector<int>({1,2,3,4,5,6,7,9,10}),"active-frame cleanup order or idempotence failed");
    }
    fake=Fake{};
    {
        OpenXrSession session(api());start(session);
        OpenXrSwapchains chains({formats,swap_create,swap_destroy,images,acquire,image_wait,release});
        std::array<XrViewConfigurationView,2> config{};
        for(auto& eye:config) {
            eye.recommendedImageRectWidth=eye.recommendedImageRectHeight=100;
            eye.maxImageRectWidth=eye.maxImageRectHeight=100;
            eye.maxSwapchainSampleCount=1;
        }
        const std::array<int64_t,1> preferred{43};
        require(chains.initialize(session.handle(),config,preferred),"renderer swapchains failed");
        StereoRenderer renderer(session,chains);
        using Result=StereoRenderer::Result;
        unsigned draws=0;
        const auto draw=[&](unsigned eye,uint32_t index,const EyeCamera& camera,XrTime time) {
            require(eye==draws && index==1 && time==123456,"eye render arguments changed");
            require(eye==0?camera.view[12]>0:camera.view[12]<0,"stereo camera translation lost");
            ++draws;return true;
        };
        image_pending=true;
        require(renderer.step(draw,1,.1F)==Result::waiting && draws==0,"pending image rendered");
        require(renderer.step(draw,1,.1F)==Result::waiting && acquisitions==1,"pending frame reacquired");
        image_pending=false;
        require(renderer.step(draw,1,.1F)==Result::submitted,"stereo frame not submitted");
        require(draws==2 && released==2 && fake.submitted==1,"incomplete stereo submission");
        require(renderer.step([](auto,auto,const auto&,auto){return false;},1,.1F)==Result::error,
            "renderer failure accepted");
        require(fake.submitted==0 && released==3,"failed frame leaked image or submitted stale layer");
        draws=0;require(renderer.step(draw,1,.1F)==Result::submitted,"failed frame prevented recovery");
        fake.tracked=false;
        require(renderer.step(draw,1,.1F)==Result::skipped && draws==2 && fake.submitted==0,
            "untracked frame rendered or submitted stale eyes");
        fake.tracked=true;
        unsigned async_calls=0;
        const auto releases_before=released;
        const auto async_draw=[&](unsigned eye,uint32_t,const EyeCamera&,XrTime) {
            ++async_calls;
            if(async_calls<3) {require(eye==0,"pending eye advanced");return StereoRenderer::EyeResult::pending;}
            return StereoRenderer::EyeResult::complete;
        };
        require(renderer.step_async(async_draw,1,.1F)==Result::waiting && released==releases_before,
            "GPU pending eye released early");
        require(renderer.step_async(async_draw,1,.1F)==Result::waiting && released==releases_before,
            "GPU pending retry released early");
        require(renderer.step_async(async_draw,1,.1F)==Result::submitted && released==releases_before+2,
            "completed asynchronous stereo frame not submitted");
        const auto fatal_draw=[](auto,auto,const auto&,auto){return StereoRenderer::EyeResult::fatal;};
        require(renderer.step_async(fatal_draw,1,.1F)==Result::error && renderer.teardown_required(),
            "uncertain GPU failure did not require teardown");
        require(released==releases_before+2,"uncertain GPU image released");
        require(renderer.step_async(async_draw,1,.1F)==Result::error && released==releases_before+2,
            "fatal frame was reused");
    }
    std::cout<<"OpenXR injected session lifecycle tests passed (not headset validation).\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
