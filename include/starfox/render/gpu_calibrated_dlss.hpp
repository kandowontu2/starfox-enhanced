#pragma once
#include "starfox/render/calibrated_dlss.hpp"
#include "starfox/render/calibrated_effects.hpp"
#include "starfox/render/fsr1_settings.hpp"
#include <memory>
#include <span>
#include <string>
namespace starfox::render {
namespace shadows {struct GpuReflectionOutput;}
struct CalibratedDlssCoverage {
    void* ownership{}; // Actual raster ownership, not an averaged label.
    unsigned factor{1},samples{1}; // SSAA grid factor OR hardware MSAA sample count.
};
// One independent SDK viewport/history per physical eye (never mono ID 99).
// All guides, evaluation and full-panel ink restoration stay on the SDL GPU.
class GpuCalibratedDlss {
public:
    GpuCalibratedDlss();~GpuCalibratedDlss();
    GpuCalibratedDlss(const GpuCalibratedDlss&)=delete;
    GpuCalibratedDlss& operator=(const GpuCalibratedDlss&)=delete;
    // Call only after all this owner's commands are submitted/cancelled. The
    // host keeps the borrowed SDK/device alive through retire(). D3D12 only.
    // The stereo/presentation owner, NOT each eye, must invoke the borrowed
    // finish_frame once after the complete submitted/cancelled pair drains.
    // This also applies to held-image presentations with no reevaluation.
    bool initialize(void* device,int color_format,CalibratedDlssApi,unsigned viewport,
        unsigned mode,unsigned model,Fsr1Extent output);
    Fsr1Extent input_extent() const noexcept;
    bool enqueue(void* command,void* scene,void* ownership,void* surface,void* motion,
        void* native_ink,void* native_ownership,void* destination,
        StarfoxDlssFrameV1 constants,std::array<float,2> previous_jitter={},
        std::span<const shadows::GpuReflectionOutput> rays={},unsigned rejected_layers=0,
        CalibratedDlssCoverage coverage={},CalibratedPatternGuide patterns={},bool model_reflections=true);
    // Native ray RGB (with optional exact liquid aux layout) and actual matching
    // raster samples supply local current/accepted eligibility. Secondary RGB
    // never uses primary rigid flow or seeds a later same-depth dry pixel.
    // Untracked post effects locally invalidate motion and bias current colour
    // only for their ownership layer (bit 0 WORLD, bit 1 MODEL). Other geometry
    // retains real guides; this is not distortion/reflection correspondence.
    // A pattern descriptor opts into current-AA/accepted-footprint phase checks
    // through the SDK's current-colour bias guide. It neither moves post RGB
    // after the SDK nor asserts knowledge of its private reconstruction filter.
    bool enqueue_accepted(void* command,void* native_ink,void* native_ownership,void* destination);
    // Accept ONLY after the whole stereo/compositor presentation succeeds.
    // SDL command cancellation cannot roll back SDK CPU temporal/resource state.
    // If has_recorded_sdk_work(), SUBMIT/DRAIN the recorded command even on a
    // later failure, but reject presentation and discard these app-owned banks.
    // Never cancel that command: reset/private viewport retirement cannot repair
    // the SDK's retained native resource-state tracker. Held reuse evaluates
    // nothing and never overwrites either accepted eye or its history.
    void commit() noexcept;
    void discard() noexcept;
    bool has_recorded_sdk_work() const noexcept;
    bool retire() noexcept;
    // Pattern phases are private guide witnesses, never substituted for any
    // SDK input format. They exist only after a pattern has been selected.
    struct Inputs {void *color{},*depth{},*motion{},*exposure{},*current_color_bias{},
        *pattern_phase{},*accepted_pattern_phase{};};
    Inputs resident_inputs() const noexcept; // borrowed; diagnostic/interop, no mapping
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
