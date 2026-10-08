#pragma once
#include "starfox/render/calibrated_game_motion.hpp"

namespace starfox::render {
struct CalibratedReflectionLiquidClock {unsigned material;float seconds;};
inline std::optional<CalibratedReflectionLiquidClock> calibrated_reflection_liquid_clock(const CalibratedGameFrame& frame) noexcept {
    std::optional<CalibratedReflectionLiquidClock> result;bool seen=false;
    for(const auto& draw:frame.draws) if(draw.source_key==calibrated_ground_source_key) {
        if(seen) return {};
        seen=true;
        const auto material=calibrated_ground_material(draw.packet);
        if((material!=5 && material!=9) || draw.layer!=CalibratedGameLayer::world || draw.ray_caster
            || draw.after_rays || draw.packet.preserve_native_colour || draw.blend!=vr::SceneBlend::opaque) return {};
        const auto words=draw.packet.geometry.texel_view();
        const auto at=std::size_t(draw.packet.geometry.vertex_view().front().texture[0])+calibrated_ground_offset;
        const float seconds=std::bit_cast<float>(words[at+11]);
        if(!std::isfinite(seconds) || seconds<0) return {};
        result=CalibratedReflectionLiquidClock{material,seconds};
    }
    return result;
}
// Secondary RGB has its own accepted presentation, even without primary TAA.
// An encoded command, image wait or rejected presentation must not advance it.
class CalibratedReflectionTimeline {
public:
    using Extents=std::array<std::array<std::uint32_t,2>,2>;
    bool prepare(std::shared_ptr<const CalibratedGameFrame> next,
        const std::array<vr::EyeCamera,2>& cameras,const Extents& extents,
        std::array<float,2> jitter={},unsigned samples=1) {
        if(pending_ || !next || !next->current || !std::isfinite(next->presentation_seconds)
            || next->presentation_seconds<0 || (samples!=1 && samples!=2 && samples!=4 && samples!=8)) return false;
        for(float value:jitter) if(!std::isfinite(value) || std::abs(value)>1) return false;
        for(unsigned eye=0;eye<2;++eye) {
            if(!extents[eye][0] || !extents[eye][1] || extents[eye][0]>16384 || extents[eye][1]>16384
                || !calibrated_motion_mapping(cameras[eye],cameras[eye],vr::Matrix4{
                    1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},vr::Matrix4{
                    1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1})) return false;
        }
        reuse_=bool(accepted_) && extents==accepted_extents_ && samples==accepted_samples_;
        if(reuse_) {
            auto now=next->settings,old=accepted_->settings;
            now.effect_seconds=old.effect_seconds=0;now.exposure_paused=old.exposure_paused=false;
            now.camera_response_pose=old.camera_response_pose={};
            const auto& a=*next->current;const auto& b=*accepted_->current;
            reuse_=now==old && a.scene_epoch==b.scene_epoch && a.flow==b.flow
                && next->presentation_seconds>=accepted_->presentation_seconds
                && (!calibrated_game_uses_effect_clock(next->settings)
                    || next->settings.effect_seconds>=accepted_->settings.effect_seconds)
                && !timing::camera_transform_is_discontinuous(b.camera,a.camera);
            const auto liquid_now=calibrated_reflection_liquid_clock(*next),liquid_old=calibrated_reflection_liquid_clock(*accepted_);
            reuse_=reuse_ && liquid_now.has_value()==liquid_old.has_value()
                && (!liquid_now || (liquid_now->material==liquid_old->material && liquid_now->seconds>=liquid_old->seconds));
        }
        held_=reuse_ && same_calibrated_presentation(*next,*accepted_) && jitter==accepted_jitter_;
        for(unsigned eye=0;eye<2;++eye) held_=held_
            && cameras[eye].view==accepted_cameras_[eye].view
            && cameras[eye].projection==accepted_cameras_[eye].projection
            && cameras[eye].effects==accepted_cameras_[eye].effects;
        pending_epoch_=accepted_epoch_+unsigned(!reuse_);
        pending_=std::move(next);pending_cameras_=cameras;pending_extents_=extents;pending_jitter_=jitter;
        pending_samples_=samples;
        return true;
    }
    void settle(bool presented) noexcept {
        if(presented && pending_) {
            accepted_=std::move(pending_);accepted_cameras_=pending_cameras_;accepted_extents_=pending_extents_;
            accepted_jitter_=pending_jitter_;accepted_epoch_=pending_epoch_;
            accepted_samples_=pending_samples_;
        } else pending_.reset();
        reuse_=held_=false;
    }
    const CalibratedGameFrame* previous() const noexcept {return reuse_?accepted_.get():nullptr;}
    const std::array<vr::EyeCamera,2>& cameras() const noexcept {return accepted_cameras_;}
    const Extents& extents() const noexcept {return accepted_extents_;}
    const std::array<float,2>& jitter() const noexcept {return accepted_jitter_;}
    unsigned samples() const noexcept {return accepted_samples_;}
    std::uint64_t epoch() const noexcept {return pending_epoch_;}
    bool held() const noexcept {return held_;}
private:
    std::shared_ptr<const CalibratedGameFrame> accepted_,pending_;
    std::array<vr::EyeCamera,2> accepted_cameras_{},pending_cameras_{};
    Extents accepted_extents_{},pending_extents_{};
    std::array<float,2> accepted_jitter_{},pending_jitter_{};
    std::uint64_t accepted_epoch_{},pending_epoch_{};
    unsigned accepted_samples_{1},pending_samples_{1};
    bool reuse_{},held_{};
};
}
