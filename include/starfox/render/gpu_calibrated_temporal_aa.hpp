#pragma once
#include "starfox/render/calibrated_effects.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
namespace shadows {struct GpuReflectionOutput;}
struct CalibratedTemporalAaSettings {
    std::uint64_t epoch{}; // Change on source discontinuity or incompatible settings.
    float weight{.85F};
    unsigned rejected_layers{}; // Bit 0 WORLD, bit 1 MODEL; exclude from stored history too.
    CalibratedPatternGuide patterns{}; // Optional exact per-pixel phase witness, not a motion substitute.
    CalibratedEdgeGuide edges{}; // Actual post-pass branches, not reconstructed from styled RGB.
    bool model_reflections{true}; // Match the compositor's nonzero model reflection strength.
};
// Native-eye temporal resolve, not persistence or a spatial blur. Inputs stay
// resident: colour, exact AA ownership, surface forward depth, and geometric
// previous-minus-current pixel motion / previous depth / correspondence validity.
// One instance per eye. Jitter is already included in the supplied motion.
class GpuCalibratedTemporalAa {
public:
    GpuCalibratedTemporalAa();~GpuCalibratedTemporalAa();
    GpuCalibratedTemporalAa(const GpuCalibratedTemporalAa&)=delete;
    GpuCalibratedTemporalAa& operator=(const GpuCalibratedTemporalAa&)=delete;
    bool initialize(void* device,int color_format);
    bool enqueue(void* command,void* source,void* ownership,void* surface,
        void* motion,void* destination,unsigned width,unsigned height,
        const CalibratedTemporalAaSettings&,const shadows::GpuReflectionOutput* rays=nullptr,
        void* edge_witness=nullptr);
    // Optional resident native ray radiance rejects actual liquid/ground/model
    // reflection consumers. Current colour cannot seed a later dry history;
    // unaffected geometry keeps real correspondence. No new pass/readback.
    // Separate presentation reconstruction removes the current sample jitter
    // WITHOUT feeding another spatial filter back into history. Centre-raster
    // ink/ownership keeps protected art exact. Does not commit/discard history.
    bool enqueue_presentation(void* command,void* resolved,void* jittered_ownership,
        void* jittered_surface,void* native_ink,void* native_ownership,
        void* destination,unsigned width,unsigned height,std::array<float,2> jitter,
        const CalibratedPatternGuide& patterns={},const CalibratedEdgeGuide& edges={},
        void* edge_witness=nullptr,void* centre_edge_witness=nullptr);
    // Caller submits/cancels commands on one ordered queue. Commit ONLY after
    // the whole eye presentation is accepted, not after encoding/submission.
    // Rejected/cancelled work never replaces accepted history, even on resize.
    void commit() noexcept;
    void discard() noexcept;
    void reset() noexcept;
    // Borrowed resident colour for an identical held presentation. Never exposes
    // an encoded/rejected candidate; invalid epoch/extent/pending state returns
    // null. Lets the owner avoid accumulating rounding drift while paused.
    void* accepted_colour(unsigned width,unsigned height,std::uint64_t epoch) const noexcept;
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
