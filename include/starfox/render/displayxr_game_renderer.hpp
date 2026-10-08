#pragma once
#include "starfox/render/calibrated_game_scene.hpp"
#include "starfox/render/displayxr_d3d12_presenter.hpp"
#include "starfox/render/calibrated_dlss.hpp"
#include "starfox/render/reflection_source_diagnostics.hpp"

namespace starfox::render {
namespace shadows {struct GpuReflectionOutput;}
struct GpuReflectionSourceAdmission;
struct ReflectionLiquidDiagnosticInputs {
    void* device{};void* vertices{};
    std::uint32_t previous_vertex_offset{},vertex_count{};
    std::array<float,32> frame{};
    // Actual pre-liquid-guide ownership image, borrowed for AA-OFF ordered
    // diagnostics only. No allocation/copy/wait; same finished-frame lifetime.
    // MSAA extraction reuses the target, so sampled inputs leave this empty.
    void* ownership{};
    // Read-only copies of the actual encoded history calibration. A separate
    // diagnostic trace needs both views/current liquid transform after camera
    // motion; assuming a fixed view can give a misleading refusal/guide.
    std::array<float,9> current_cube{},previous_cube{};
    std::array<float,12> current_liquid_rotation{};
    std::array<float,4> current_projection{};
};
// Owns the two GPU game images through runtime-image waits and native queue
// completion. Runtime/device/panel HWND must outlive successful close().
// Native calibrated game packets, not an SBS image or CPU-readback converter.
class DisplayXrGameRenderer {
public:
    DisplayXrGameRenderer();~DisplayXrGameRenderer();
    DisplayXrGameRenderer(const DisplayXrGameRenderer&)=delete;
    DisplayXrGameRenderer& operator=(const DisplayXrGameRenderer&)=delete;
    bool initialize(const DisplayXrRuntime&,SDL_GPUDevice*,void* panel_window,
                    DisplayXrVulkanBinding* vulkan_creation=nullptr,CalibratedDlssApi dlss={});
    bool close() noexcept;
    bool try_close() noexcept; // nonblocking; retains source/eyes on false
    bool poll_events(); // only with no pending frame
    bool running() const noexcept;
    bool exit_requested() const noexcept;
    bool frame_pending() const noexcept;
    std::uint64_t presented_frames() const noexcept;
    bool srgb_target() const noexcept;
    // Diagnostic raster dimensions, distinct from the runtime panel size.
    // No GPU readback or synchronization; last prepared source layout only.
    std::array<std::uint32_t,2> scene_extent(unsigned eye) const noexcept;
    // Borrowed accepted secondary sample for explicit diagnostics only.
    // No readback/wait. Empty while a frame is pending or no bank exists.
    // The caller must finish any use before the next submit/close/device teardown.
    shadows::GpuReflectionOutput accepted_reflection_sample(unsigned eye,unsigned sample=0) const noexcept;
    // Test-only discrete admitted-lobe mask, paired with the accepted sample.
    // Empty without explicit complete-source validation or while pending.
    GpuReflectionSourceAdmission accepted_reflection_source_admission(unsigned eye,unsigned sample=0) const noexcept;
    // Explicit diagnostics only: all resident secondary history buffers,
    // including inactive sample slots and pending replacement allocations.
    // No readback, wait or allocation; zero without diagnostic opt-in.
    std::uint64_t reflection_history_resident_bytes() const noexcept;
    // Disabled by default: retain bounded CPU input words and borrowed GPU
    // vertex handles only when a diagnostic explicitly requests them. Does
    // not copy GPU data, allocate GPU storage or synchronize the game queue.
    bool enable_reflection_diagnostics(bool enabled);
    // Explicit whole-owner staging for the ordered liquid ABI. Requires the
    // diagnostic opt-in and D3D12; never a menu/config option or enabled by
    // ordinary reflection diagnostics. Switching cuts accepted color banks.
    // Normal callers retain the guarded full-quality CURRENT liquid path.
    bool enable_curved_reflection_validation(bool enabled) noexcept;
    // Independent opt-in real-producer validation of the entire resident
    // source/proof/colour/full-frame publisher. Requires reflection diagnostics;
    // never selected by the menu/config or the existing curved diagnostic.
    // Both native eyes and all hardware/rough samples retain full resolution.
    // Explicit diagnostics may be slow: no normal-player/cost qualification.
    bool enable_source_reflection_validation(bool enabled) noexcept;
    // Explicit diagnostics only, rejected while pending or without reflection
    // diagnostics. Borrowed callback/context must outlive submit and be removed
    // before destruction. Same-command markers only; no proof mutation/waits.
    bool set_source_stage_observer(ReflectionSourceStageObserver,void* context=nullptr) noexcept;
    // Explicit diagnostic source-pixel offset, not a user effect or an XR
    // pose/FOV change. Disabled unless diagnostics are enabled; no GPU copy or
    // wait. Applied after SDK jitter, retaining physical panel/native ink.
    // Changing it invalidates diagnostic SDK reuse. Rejects pending frames,
    // nonfinite values and offsets outside one source pixel in either axis.
    bool set_source_sample_diagnostic_offset(std::array<float,2> pixels) noexcept;
    // Last finished source sample BEFORE SDK packing/reconstruction. Borrowed
    // diagnostic handle only, same lifetime/busy restrictions as ray records.
    // scene_extent() supplies its dimensions; no allocation/readback/wait.
    void* source_sample_diagnostic_image(unsigned eye,unsigned sample=0) const noexcept;
    // Encoded primary raster camera (before hardware sample positions), not
    // reconstructed XR angles. CPU-only, disabled/busy restrictions as above.
    std::optional<vr::EyeCamera> source_raster_diagnostic_camera(unsigned eye) const noexcept;
    // Unfiltered native incident records for the last encoded sample. Enabled
    // diagnostics only; no readback/wait/allocation. Not an accepted-history
    // promise after rejection; the handle expires on the next submit/close.
    shadows::GpuReflectionOutput current_reflection_sample(unsigned eye,unsigned sample=0) const noexcept;
    // Last encoded inputs, not a promise of accepted history. Empty while
    // pending/disabled; borrowed handles expire on the next submit or close.
    ReflectionLiquidDiagnosticInputs reflection_liquid_inputs(unsigned eye,unsigned sample=0) const noexcept;
    // Accepted source state remains immutable across both eyes and retries.
    // Submit only when running() and !frame_pending(); poll() completes it.
    DisplayXrSubmission submit(std::shared_ptr<const CalibratedGameFrame>);
    DisplayXrSubmission poll();
    std::shared_ptr<const CalibratedGameFrame> retained_frame() const noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    struct State;std::unique_ptr<State> state_;
    std::string status_{"Leia game renderer not initialized"};
};
}
