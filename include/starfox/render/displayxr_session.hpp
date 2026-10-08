#pragma once
#include "starfox/render/displayxr_runtime.hpp"
#include "starfox/render/displayxr_abi.hpp"
#include "starfox/vr/eye_camera.hpp"
#include "starfox/vr/openxr_session.hpp"
#include <memory>

namespace starfox::render {
struct DisplayXrRigSettings {
    // Camera-centric rig. The runtime returns poses already in world units:
    // never multiply them by this factor a second time or re-anchor each frame.
    XrPosef pose{{0,0,0,1},{0,0,0}};
    float ipd_factor{1},parallax_factor{1},convergence{1024};
    float vertical_fov{.824821F},meters_to_virtual{256};
    float near_plane{1};
};
struct DisplayXrFrame {
    vr::StereoFrame xr;
    // Existing VR/Vulkan downward-NDC convention; D3D12 presentation must
    // adapt projection Y at its graphics boundary. SDL also uses upward NDC
    // on Vulkan (negative viewport), unlike direct headset Vulkan rendering.
    // The original OpenXR views above remain unchanged.
    std::array<vr::EyeCamera,2> cameras{};
    displayxr::RawViews physical;
};
// Native session foundation, not a completed renderer/presentation integration.
// Runtime and native graphics device/queue must outlive this object. Caller
// first negotiates OpenXR graphics requirements on that same device, then
// supplies its binding and an HWND located on the confirmed physical panel.
// No implicit loader, global runtime switch, fixed SBS rig, or CPU readback.
class DisplayXrSession {
public:
    DisplayXrSession()=default;
    ~DisplayXrSession();
    DisplayXrSession(const DisplayXrSession&)=delete;
    DisplayXrSession& operator=(const DisplayXrSession&)=delete;
    bool initialize(const DisplayXrRuntime&,const void* graphics_binding,void* window);
    void close() noexcept;
    bool poll_events();
    std::optional<DisplayXrFrame> begin_frame(const DisplayXrRigSettings&);
    bool end_frame(std::span<const XrCompositionLayerBaseHeader* const> layers={});
    XrSession handle() const noexcept {return session_?session_->handle():XR_NULL_HANDLE;}
    XrSpace space() const noexcept {return session_?session_->space():XR_NULL_HANDLE;}
    bool running() const noexcept {return session_ && session_->running();}
    bool exit_requested() const noexcept {return session_ && session_->exit_requested();}
    const std::string& status() const noexcept {return status_;}
private:
    std::array<displayxr::RenderingMode,64> modes_{};
    std::uint32_t mode_index_{};
    displayxr::EnumerateRenderingModes enumerate_modes_{};
    std::unique_ptr<vr::OpenXrSession> session_;
    bool renderable_{};
    std::string status_{"Leia SR graphics session not initialized"};
    std::uint32_t read_modes();
    bool native_mode_active();
};
}
