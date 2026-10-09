#include "starfox/render/displayxr_session.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace starfox::render {
namespace {
void require(XrResult result,const char* name) {
    if(XR_FAILED(result)) throw std::runtime_error(std::string(name)+": OpenXR result "+std::to_string(result));
}
template<class Function>
Function function(const DisplayXrRuntime& runtime,const char* name) {
    PFN_xrVoidFunction pointer{};
    require(runtime.get_instance_proc()(runtime.instance(),name,&pointer),name);
    if(!pointer) throw std::runtime_error(std::string("Missing runtime entry point: ")+name);
    return reinterpret_cast<Function>(pointer);
}
bool native(const displayxr::RenderingMode& mode) {
    return mode.hardware_3d==XR_TRUE && mode.eye_count==2 && mode.columns==2 && mode.rows==1
        && mode.view_width>0 && mode.view_height>0 && mode.view_width<=32768 && mode.view_height<=32768
        && std::isfinite(mode.scale_x) && mode.scale_x>0 && mode.scale_x<=8
        && std::isfinite(mode.scale_y) && mode.scale_y>0 && mode.scale_y<=8
        && mode.name[0] && std::find(std::begin(mode.name),std::end(mode.name),'\0')!=std::end(mode.name);
}
bool valid(const DisplayXrRigSettings& settings) {
    for(float value:{settings.ipd_factor,settings.parallax_factor,settings.convergence,settings.vertical_fov,
        settings.meters_to_virtual,settings.near_plane,settings.pose.position.x,settings.pose.position.y,
        settings.pose.position.z,settings.pose.orientation.x,settings.pose.orientation.y,
        settings.pose.orientation.z,settings.pose.orientation.w}) if(!std::isfinite(value)) return false;
    const auto& q=settings.pose.orientation;
    const double norm=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
    return std::abs(norm-1)<1e-4 && settings.ipd_factor>=0 && settings.parallax_factor>=0
        && settings.convergence>settings.near_plane && settings.near_plane>0
        && settings.vertical_fov>0 && settings.vertical_fov<std::numbers::pi_v<float>
        && settings.meters_to_virtual>0;
}
bool valid(const displayxr::RawViews& raw) {
    if(raw.eye_count!=2 || raw.canvas_rect.extent.width<=0 || raw.canvas_rect.extent.height<=0
        || !std::isfinite(raw.canvas_size.width) || !std::isfinite(raw.canvas_size.height)
        || raw.canvas_size.width<=0 || raw.canvas_size.height<=0) return false;
    for(unsigned eye=0;eye<2;++eye) {
        const auto& p=raw.eyes[eye];
        if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || p.z<=0) return false;
    }
    const auto& p=raw.display_plane.position;const auto& q=raw.display_plane.orientation;
    for(float value:{p.x,p.y,p.z,q.x,q.y,q.z,q.w}) if(!std::isfinite(value)) return false;
    const double norm=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
    return std::abs(norm-1)<1e-4;
}
}
DisplayXrSession::~DisplayXrSession() {close();}
void DisplayXrSession::close() noexcept {
    session_.reset();enumerate_modes_=nullptr;modes_={};mode_index_=0;renderable_=false;
}
std::uint32_t DisplayXrSession::read_modes() {
    modes_.fill({});
    std::uint32_t count{};
    require(enumerate_modes_(handle(),modes_.size(),&count,modes_.data()),"Read native display modes");
    if(!count || count>modes_.size()) throw std::runtime_error("Invalid display mode count");
    return count;
}
bool DisplayXrSession::native_mode_active() {
    const auto count=read_modes();
    for(std::uint32_t i=0;i<count;++i)
        if(modes_[i].index==mode_index_ && native(modes_[i]) && modes_[i].active==XR_TRUE) return true;
    return false;
}
bool DisplayXrSession::initialize(const DisplayXrRuntime& runtime,const void* graphics,void* window) {
    close();
    try {
        if(!runtime.detected() || !graphics || !window)
            throw std::runtime_error("Missing confirmed Leia panel, graphics binding or panel window");
        const auto* binding=static_cast<const XrBaseInStructure*>(graphics);
        const auto expected=runtime.backend()==DisplayXrBackend::direct3d12
            ?XR_TYPE_GRAPHICS_BINDING_D3D12_KHR:XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
        if(binding->type!=expected)
            throw std::runtime_error("Leia graphics binding does not match the negotiated runtime backend");
        // Resolve every required entry before creating owned graphics objects.
        vr::SessionApi api{
            function<PFN_xrCreateSession>(runtime,"xrCreateSession"),
            function<PFN_xrDestroySession>(runtime,"xrDestroySession"),
            function<PFN_xrCreateReferenceSpace>(runtime,"xrCreateReferenceSpace"),
            function<PFN_xrDestroySpace>(runtime,"xrDestroySpace"),
            function<PFN_xrEnumerateEnvironmentBlendModes>(runtime,"xrEnumerateEnvironmentBlendModes"),
            function<PFN_xrPollEvent>(runtime,"xrPollEvent"),
            function<PFN_xrBeginSession>(runtime,"xrBeginSession"),
            function<PFN_xrEndSession>(runtime,"xrEndSession"),
            function<PFN_xrWaitFrame>(runtime,"xrWaitFrame"),
            function<PFN_xrBeginFrame>(runtime,"xrBeginFrame"),
            function<PFN_xrEndFrame>(runtime,"xrEndFrame"),
            function<PFN_xrLocateViews>(runtime,"xrLocateViews"),
            function<PFN_xrRequestExitSession>(runtime,"xrRequestExitSession")};
        enumerate_modes_=function<displayxr::EnumerateRenderingModes>(runtime,"xrEnumerateDisplayRenderingModesDXR");
        const auto request=function<displayxr::RequestRenderingMode>(runtime,"xrRequestDisplayRenderingModeDXR");
        displayxr::WindowBinding window_binding;window_binding.window=window;window_binding.next=graphics;
        session_=std::make_unique<vr::OpenXrSession>(api);
        if(!session_->initialize(runtime.instance(),runtime.system(),&window_binding))
            throw std::runtime_error(session_->status());
        const auto count=read_modes();
        const auto mode=std::find_if(modes_.begin(),modes_.begin()+count,[](const auto& mode) {
            return native(mode) && (mode.active==XR_TRUE || mode.requestable==XR_TRUE);
        });
        if(mode==modes_.begin()+count) throw std::runtime_error("Native hardware-weaved stereo mode is unavailable or locked");
        mode_index_=mode->index;
        if(mode->active!=XR_TRUE) require(request(handle(),mode_index_),"Request native Leia mode");
        // A request can be denied by the panel lease without an API error.
        // The active metadata must confirm it; success alone is not sufficient.
        if(!native_mode_active()) throw std::runtime_error("Native Leia display-mode request was not applied");
        status_="Leia SR graphics session awaiting READY; presentation not yet integrated";
        return true;
    } catch(const std::exception& error) {const std::string failure=error.what();close();status_=failure;return false;}
}
bool DisplayXrSession::poll_events() {
    if(!session_) {status_="Leia SR graphics session not initialized";return false;}
    if(!session_->poll_events()) {status_=session_->status();return false;}
    if(session_->exit_requested()) status_="Leia SR display/session disconnected";
    return true;
}
std::optional<DisplayXrFrame> DisplayXrSession::begin_frame(const DisplayXrRigSettings& settings) {
    if(!session_) {status_="Leia SR graphics session not initialized";return {};}
    if(!valid(settings)) {status_="Invalid Leia SR camera rig";return {};}
    try {
        if(!session_->running() || session_->exit_requested()) return {};
        // Recheck native mode so a hardware-mode change never silently presents
        // fixed SBS as if it were calibrated native Leia output. No allocation.
        if(!native_mode_active()) throw std::runtime_error("Native Leia display mode is no longer active");
        displayxr::CameraRig rig;
        rig.pose=settings.pose;rig.ipd_factor=settings.ipd_factor;rig.parallax_factor=settings.parallax_factor;
        rig.convergence_diopters=1/settings.convergence;rig.vertical_fov=settings.vertical_fov;
        rig.meters_to_virtual=settings.meters_to_virtual;
        DisplayXrFrame result;
        const auto frame=session_->begin_frame(&rig,&result.physical);
        if(!frame) {status_=session_->status();return {};}
        result.xr=*frame;renderable_=frame->should_render;
        if(renderable_) {
            for(unsigned eye=0;eye<2;++eye) {
                // XrCameraRigDXR v3 already applies metersToVirtual to poses.
                // Keep calibrated pose/FOV and the physical panel origin intact.
                const auto camera=vr::eye_camera(frame->views[eye],1,settings.near_plane);
                if(!camera) {renderable_=false;break;}
                result.cameras[eye]=*camera;
            }
            if(!valid(result.physical)) renderable_=false;
        }
        result.xr.should_render=renderable_;
        status_=renderable_?"Leia SR calibrated eye views ready"
            :"Leia SR frame suppressed: invisible, untracked or invalid views";
        return result;
    } catch(const std::exception& error) {status_=error.what();return {};}
}
bool DisplayXrSession::end_frame(std::span<const XrCompositionLayerBaseHeader* const> layers) {
    if(!session_) {status_="Leia SR graphics session not initialized";return false;}
    std::string mode_failure;
    if(renderable_ && !layers.empty()) {
        try {
            if(!native_mode_active()) {renderable_=false;mode_failure="Native Leia display mode lost before submission";}
        } catch(const std::exception& error) {renderable_=false;mode_failure=error.what();}
    }
    const bool result=session_->end_frame(renderable_?layers:std::span<const XrCompositionLayerBaseHeader* const>{});
    renderable_=false;
    if(!result) status_=session_->status();
    else if(!mode_failure.empty()) status_=mode_failure;
    return result && mode_failure.empty();
}
}
