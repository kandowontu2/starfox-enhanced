#pragma once
#include "starfox/render/calibrated_game_motion.hpp"

namespace starfox::render {
// Actual accepted presentations, shared by BOTH eyes. Encoding, submitting a
// GPU command, or retrying an XR image wait does not accept a new source/time.
class CalibratedMotionHistory {
public:
    bool prepare(std::shared_ptr<const CalibratedGameFrame> next,
        const std::array<vr::EyeCamera,2>& cameras,std::array<float,2> jitter={}) {
        if(!next || !next->current || !std::isfinite(next->presentation_seconds)
            || next->presentation_seconds<0) return false;
        for(float value:jitter) if(!std::isfinite(value) || std::abs(value)>1) return false;
        pending_=std::move(next);pending_cameras_=cameras;pending_jitter_=jitter;interval_=0;
        if(!accepted_ || pending_->settings.exposure_paused) return true;
        auto now=pending_->settings,old=accepted_->settings;
        now.effect_seconds=old.effect_seconds=0;now.exposure_paused=old.exposure_paused=false;
        now.camera_response_pose=old.camera_response_pose={};
        const auto& a=*pending_->current;const auto& b=*accepted_->current;
        const double elapsed=pending_->presentation_seconds-accepted_->presentation_seconds;
        if(now!=old || a.scene_epoch!=b.scene_epoch || a.flow!=b.flow
            || timing::camera_transform_is_discontinuous(b.camera,a.camera)
            || !(elapsed>0 && elapsed<=.25)) return true;
        bool held=same_calibrated_presentation(*pending_,*accepted_);
        for(unsigned eye=0;eye<2;++eye) held=held && cameras[eye].view==accepted_cameras_[eye].view
            && cameras[eye].projection==accepted_cameras_[eye].projection
            && cameras[eye].effects==accepted_cameras_[eye].effects;
        if(!held) interval_=elapsed;
        return true;
    }
    void settle(bool presented) noexcept {
        if(presented && pending_ && !pending_->settings.exposure_paused) {
            accepted_=std::move(pending_);accepted_cameras_=pending_cameras_;accepted_jitter_=pending_jitter_;
        } else {pending_.reset();accepted_.reset();}
        interval_=0;
    }
    const std::shared_ptr<const CalibratedGameFrame>& accepted() const noexcept {return accepted_;}
    const std::array<vr::EyeCamera,2>& cameras() const noexcept {return accepted_cameras_;}
    const std::array<float,2>& jitter() const noexcept {return accepted_jitter_;}
    double interval() const noexcept {return interval_;}
private:
    std::shared_ptr<const CalibratedGameFrame> accepted_,pending_;
    std::array<vr::EyeCamera,2> accepted_cameras_{},pending_cameras_{};
    std::array<float,2> accepted_jitter_{},pending_jitter_{};
    double interval_{};
};
}
