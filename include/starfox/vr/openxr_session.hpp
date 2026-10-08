#pragma once
#include <openxr/openxr.h>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace starfox::vr {
// Injectable dispatch permits lifecycle tests without pretending a headset
// or graphics driver is present. Normal callers use the OpenXR loader.
struct SessionApi {
    PFN_xrCreateSession create_session{xrCreateSession};
    PFN_xrDestroySession destroy_session{xrDestroySession};
    PFN_xrCreateReferenceSpace create_space{xrCreateReferenceSpace};
    PFN_xrDestroySpace destroy_space{xrDestroySpace};
    PFN_xrEnumerateEnvironmentBlendModes blend_modes{xrEnumerateEnvironmentBlendModes};
    PFN_xrPollEvent poll_event{xrPollEvent};
    PFN_xrBeginSession begin_session{xrBeginSession};
    PFN_xrEndSession end_session{xrEndSession};
    PFN_xrWaitFrame wait_frame{xrWaitFrame};
    PFN_xrBeginFrame begin_frame{xrBeginFrame};
    PFN_xrEndFrame end_frame{xrEndFrame};
    PFN_xrLocateViews locate_views{xrLocateViews};
};
struct StereoFrame {
    XrTime display_time{};
    XrDuration display_period{};
    bool should_render{};
    bool tracking_origin_changed{};
    std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
};
class OpenXrSession {
public:
    explicit OpenXrSession(SessionApi api={}):api_(api) {}
    ~OpenXrSession();
    OpenXrSession(const OpenXrSession&)=delete;
    OpenXrSession& operator=(const OpenXrSession&)=delete;
    // Caller owns instance/device, performs graphics-requirements negotiation,
    // and supplies that runtime-compatible graphics binding. Both outlive us.
    bool initialize(XrInstance,XrSystemId,const void* graphics_binding);
    void close() noexcept;
    bool poll_events(); // transitions READY/STOPPING, records exit/loss
    // Optional vendor rig/result chains; defaults preserve headset behavior.
    // Caller owns the chains for the duration of the locate call.
    std::optional<StereoFrame> begin_frame(const void* locate_chain=nullptr,void* view_state_chain=nullptr);
    // Submit only completed/released swapchain layers. Invisible/untracked
    // frames always submit zero layers, while retaining frame pacing.
    bool end_frame(std::span<const XrCompositionLayerBaseHeader* const> layers={});
    XrSession handle() const noexcept {return session_;}
    XrSpace space() const noexcept {return space_;}
    bool running() const noexcept {return running_;}
    bool exit_requested() const noexcept {return exit_;}
    XrSessionState state() const noexcept {return state_;}
    const std::string& status() const noexcept {return status_;}
private:
    SessionApi api_;
    XrInstance instance_{XR_NULL_HANDLE};
    XrSession session_{XR_NULL_HANDLE};
    XrSpace space_{XR_NULL_HANDLE};
    XrSessionState state_{XR_SESSION_STATE_UNKNOWN};
    XrEnvironmentBlendMode blend_{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
    bool running_{},exit_{},frame_active_{},renderable_{};
    XrTime frame_time_{};
    std::vector<XrTime> origin_changes_;
    std::string status_{"OpenXR session not initialized"};
};
}
