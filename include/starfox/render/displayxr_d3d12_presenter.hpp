#pragma once
#include "starfox/render/displayxr_session.hpp"
#include <cstdint>
#include <memory>
struct SDL_GPUDevice;
struct SDL_GPUTexture;
struct SDL_GPUFence;

namespace starfox::render {
class DisplayXrVulkanBinding;
enum class DisplayXrSubmission {waiting,submitted,failed};
struct DisplayXrD3D12Frame {
    DisplayXrFrame calibrated;
    // SDL upward-NDC Y (both D3D12 and its negative-viewport Vulkan backend),
    // adapted once from the shared native Vulkan/VR convention.
    // Runtime composition poses/FOV above are never altered.
    std::array<vr::EyeCamera,2> cameras;
};
// Native runtime-owned eye images, not packed SBS or a CPU-readback weaver.
// Caller renders the complete scene using begin_frame()'s calibrated cameras
// into two same-format/extent SDL textures. Runtime, SDL device and panel HWND
// outlive this object and its cleanup. The historical class name is retained
// for compatibility; Vulkan uses the same frame/lifetime owner, with a prior
// runtime-created Vulkan2 binding rather than a borrowed-device shortcut.
class DisplayXrD3D12Presenter {
public:
    DisplayXrD3D12Presenter();
    ~DisplayXrD3D12Presenter();
    DisplayXrD3D12Presenter(const DisplayXrD3D12Presenter&)=delete;
    DisplayXrD3D12Presenter& operator=(const DisplayXrD3D12Presenter&)=delete;
    bool initialize(const DisplayXrRuntime&,SDL_GPUDevice*,void* panel_window,
                    DisplayXrVulkanBinding* vulkan_creation=nullptr);
    // On an unconfirmed GPU-completion failure, retain resources and return
    // false; never destroy images underneath in-flight work or overwrite them
    // in initialize(). Caller retains dependencies and retries cleanup.
    bool close() noexcept;
    // Live recovery never waits for GPU idle. false retains every dependency
    // until a later poll confirms completion (including the scene producer).
    bool try_close() noexcept;
    // Takes ownership of the producer's SDL fence. A null fence records an
    // uncertain failed submission, not permission to destroy its resources.
    // A later ordered fence supersedes image completion, but earlier fence
    // handles remain retained until individually complete (no early recycling).
    void retain_scene_submission(SDL_GPUFence*) noexcept;
    bool poll_events();
    std::optional<DisplayXrD3D12Frame> begin_frame(const DisplayXrRigSettings&);
    // Retries an XR image wait without reacquiring it. After submission, the
    // sources cannot be replaced until finish_frame() returns submitted/failed.
    // Timeout is bounded to 100ms; 0 is the normal nonblocking path.
    DisplayXrSubmission submit_frame(SDL_GPUTexture* left,SDL_GPUTexture* right,
                                    XrDuration image_wait_timeout=0);
    DisplayXrSubmission finish_frame(); // nonblocking SDL fence query
    // Retry either a runtime-image wait or an already submitted GPU copy,
    // retaining the same two sources. Never reacquire an already held image.
    DisplayXrSubmission continue_frame(SDL_GPUTexture* left,SDL_GPUTexture* right);
    DisplayXrSubmission cancel_frame(XrDuration image_wait_timeout=0);
    bool running() const noexcept;
    bool exit_requested() const noexcept;
    bool frame_pending() const noexcept;
    std::uint64_t presented_frames() const noexcept; // complete visible projections only
    std::array<std::uint32_t,2> eye_extent(unsigned eye) const noexcept;
    int color_format() const noexcept; // SDL_GPUTextureFormat, or 0 uninitialized
    const std::string& status() const noexcept {return status_;}
private:
    struct State;
    std::unique_ptr<State> state_;
    std::string status_{"Leia D3D12 presentation not initialized"};
};
}
