#pragma once
#include "starfox/render/displayxr_game_renderer.hpp"
#include <SDL3/SDL.h>
#include <chrono>

namespace starfox::render {
// Optional ordinary-desktop owner. Discovery is explicit, never part of a
// normal 2D launch. Runtime, calibrated renderer and panel placement are kept
// together through reconnect, device replacement and pending-frame teardown.
class DisplayXrDesktop {
public:
    using Clock=std::chrono::steady_clock;
    DisplayXrDesktop();~DisplayXrDesktop();
    DisplayXrDesktop(const DisplayXrDesktop&)=delete;
    DisplayXrDesktop& operator=(const DisplayXrDesktop&)=delete;
    bool prepare(const std::filesystem::path& runtime_directory={},
                 DisplayXrBackend backend=DisplayXrBackend::direct3d12);
    // Borrowed dispatch for embedding/deterministic integration fixtures. It
    // remains callable through successful close(), exactly as Runtime's API.
    bool prepare_with_api(PFN_xrGetInstanceProcAddr,DisplayXrBackend backend=DisplayXrBackend::direct3d12);
    bool configure_gpu_properties(SDL_PropertiesID) const;
    bool attach(SDL_Window*,SDL_GPUDevice*,CalibratedDlssApi dlss={});
    bool close() noexcept;
    bool try_close() noexcept;
    bool close_pending() const noexcept;
    std::uint64_t generation() const noexcept {return generation_;}
    std::uint64_t presented_frames() const noexcept;
    bool active() const noexcept;
    bool frame_pending() const noexcept;
    bool srgb_target() const noexcept;
    // true means native presentation owns this iteration (including a wait or
    // an invisible tracking frame). false permits the ordinary 2D fallback.
    bool present(std::shared_ptr<const CalibratedGameFrame>,Clock::time_point now=Clock::now());
    const std::string& status() const noexcept {return status_;}
private:
    bool prepare_impl(const std::function<bool(DisplayXrRuntime&)>&);
    struct State;std::unique_ptr<State> state_;
    std::uint64_t generation_{};
    std::string status_{"Leia SR not checked"};
};
}
