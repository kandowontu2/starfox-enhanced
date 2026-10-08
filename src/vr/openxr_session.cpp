#include "starfox/vr/openxr_session.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>
namespace starfox::vr {
namespace {
void check(XrResult result,const char* operation) {
    if(XR_FAILED(result)) throw std::runtime_error(std::string(operation)+": OpenXR result "+std::to_string(result));
}
}
OpenXrSession::~OpenXrSession() {close();}
void OpenXrSession::close() noexcept {
    if(frame_active_) {
        XrFrameEndInfo info{XR_TYPE_FRAME_END_INFO};
        info.displayTime=frame_time_;info.environmentBlendMode=blend_;
        api_.end_frame(session_,&info);
    }
    if(space_!=XR_NULL_HANDLE) api_.destroy_space(space_);
    if(session_!=XR_NULL_HANDLE) api_.destroy_session(session_);
    instance_=XR_NULL_HANDLE;session_=XR_NULL_HANDLE;space_=XR_NULL_HANDLE;
    running_=exit_=frame_active_=renderable_=false;
    state_=XR_SESSION_STATE_UNKNOWN;frame_time_=0;
    origin_changes_.clear();
}
bool OpenXrSession::initialize(XrInstance instance,XrSystemId system,const void* binding) {
    close();
    if(instance==XR_NULL_HANDLE || system==XR_NULL_SYSTEM_ID || !binding) {
        status_="Missing OpenXR instance, system or graphics binding";return false;
    }
    try {
        instance_=instance;
        uint32_t count{};
        check(api_.blend_modes(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"Enumerate blend modes");
        if(!count || count>32) throw std::runtime_error("Invalid OpenXR blend mode count");
        std::vector<XrEnvironmentBlendMode> modes(count);
        check(api_.blend_modes(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,modes.data()),"Read blend modes");
        if(!count || count>modes.size()) throw std::runtime_error("OpenXR blend modes changed during enumeration");
        modes.resize(count);
        blend_=std::find(modes.begin(),modes.end(),XR_ENVIRONMENT_BLEND_MODE_OPAQUE)!=modes.end()
            ?XR_ENVIRONMENT_BLEND_MODE_OPAQUE:modes.front();
        XrSessionCreateInfo create{XR_TYPE_SESSION_CREATE_INFO};create.next=binding;create.systemId=system;
        check(api_.create_session(instance,&create,&session_),"Create graphics session");
        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;space.poseInReferenceSpace.orientation.w=1;
        check(api_.create_space(session_,&space,&space_),"Create local tracking space");
        status_="OpenXR session awaiting READY";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
bool OpenXrSession::poll_events() {
    if(instance_==XR_NULL_HANDLE) {status_="OpenXR session not initialized";return false;}
    if(frame_active_) {status_="Finish the OpenXR frame before polling events";return false;}
    try {
        for(;;) {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            const auto result=api_.poll_event(instance_,&event);
            if(result==XR_EVENT_UNAVAILABLE) return true;
            check(result,"Poll session events");
            if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {exit_=true;running_=false;return true;}
            if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
                const auto& change=*reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(&event);
                if(change.session==session_ && change.referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL)
                    origin_changes_.push_back(change.changeTime);
                continue;
            }
            if(event.type!=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) continue;
            const auto& change=*reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
            if(change.session!=session_) continue;
            state_=change.state;
            if(state_==XR_SESSION_STATE_READY && !running_) {
                XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                check(api_.begin_session(session_,&begin),"Begin stereo session");running_=true;
            } else if(state_==XR_SESSION_STATE_STOPPING && running_) {
                check(api_.end_session(session_),"End stereo session");running_=false;
            } else if(state_==XR_SESSION_STATE_EXITING || state_==XR_SESSION_STATE_LOSS_PENDING) {
                exit_=true;running_=false;
            }
        }
    } catch(const std::exception& e) {status_=e.what();return false;}
}
std::optional<StereoFrame> OpenXrSession::begin_frame(const void* locate_chain,void* view_state_chain) {
    if(!running_ || exit_ || frame_active_) return {};
    try {
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState timing{XR_TYPE_FRAME_STATE};
        check(api_.wait_frame(session_,&wait,&timing),"Wait for XR frame");
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
        check(api_.begin_frame(session_,&begin),"Begin XR frame");
        frame_active_=true;frame_time_=timing.predictedDisplayTime;renderable_=false;
        StereoFrame frame;frame.display_time=frame_time_;frame.display_period=timing.predictedDisplayPeriod;
        // A queued reference-space change is not effective until its time.
        // Signal even invisible frames so the next valid pose can re-anchor.
        std::erase_if(origin_changes_,[&](XrTime time) {
            if(time>frame_time_) return false;
            frame.tracking_origin_changed=true;return true;
        });
        if(timing.shouldRender) {
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
            locate.next=locate_chain;
            locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locate.displayTime=frame_time_;locate.space=space_;
            XrViewState view_state{XR_TYPE_VIEW_STATE};uint32_t count{};
            view_state.next=view_state_chain;
            check(api_.locate_views(session_,&locate,&view_state,2,&count,frame.views.data()),"Locate stereo eyes");
            const auto valid=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
            renderable_=count==2 && (view_state.viewStateFlags&valid)==valid;
        }
        frame.should_render=renderable_;return frame;
    } catch(const std::exception& e) {
        const std::string failure=e.what();
        if(frame_active_) end_frame();
        status_=failure;return {};
    }
}
bool OpenXrSession::end_frame(std::span<const XrCompositionLayerBaseHeader* const> layers) {
    if(!frame_active_) {status_="No active OpenXR frame";return false;}
    if(layers.size()>std::numeric_limits<uint32_t>::max()) {status_="Too many OpenXR layers";return false;}
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frame_time_;end.environmentBlendMode=blend_;
    end.layerCount=renderable_?static_cast<uint32_t>(layers.size()):0;
    end.layers=end.layerCount?layers.data():nullptr;
    const auto result=api_.end_frame(session_,&end);frame_active_=false;renderable_=false;
    if(XR_FAILED(result)) {status_="End XR frame: OpenXR result "+std::to_string(result);return false;}
    return true;
}
}
