#pragma once
#include "starfox/render/temporal_jitter.hpp"
#include <algorithm>
namespace starfox::render {
// Accumulate one complete sampling cycle through the selected SDK mode for
// the frozen setup image, then retain that actual neural output. Holding its
// last raster phase keeps post-DLSS ownership/lighting masks aligned too.
// Gameplay never uses this reuse policy.
class DlssPreviewHistory {
    std::uint64_t epoch_{};
    unsigned samples_{};
    std::array<float,2> jitter_{};
public:
    static constexpr unsigned sample_count=32;
    void reset() noexcept {epoch_=0;samples_=0;jitter_={};}
    bool reusable(bool frozen,bool reset_history,std::uint64_t epoch) const noexcept {
        return frozen && !reset_history && samples_>=sample_count && epoch_==epoch;
    }
    std::array<float,2> jitter(bool frozen,std::uint64_t serial,std::uint64_t epoch) const noexcept {
        return reusable(frozen,false,epoch)?jitter_:temporal_jitter(serial);
    }
    void evaluated(bool frozen,bool reset_history,std::uint64_t epoch,std::array<float,2> jitter) noexcept {
        if(!frozen) {reset();return;}
        if(reset_history || epoch_!=epoch) samples_=0;
        epoch_=epoch;jitter_=jitter;samples_=std::min(samples_+1,sample_count);
    }
    unsigned samples() const noexcept {return samples_;}
};
}
