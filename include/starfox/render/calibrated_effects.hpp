#pragma once
#include "starfox/render/effect_types.hpp"
#include <algorithm>
#include <array>
#include <cmath>
namespace starfox::render {
// These require actual per-eye neighbours/coordinates, not a per-vertex tint.
inline constexpr bool calibrated_post_effect(unsigned value) noexcept {
    switch(static_cast<Effect>(value)) {
    case Effect::cel_drawn: case Effect::ink: case Effect::neon:
    case Effect::dithered: case Effect::blueprint: case Effect::bloom:
    case Effect::night_vision: case Effect::comic: case Effect::vaporwave:
    case Effect::scanlines: case Effect::watercolour: case Effect::chalk:
    case Effect::emboss: case Effect::stained_glass: case Effect::hologram:
    case Effect::mosaic: case Effect::pencil: case Effect::oil_paint:
    case Effect::woodcut: case Effect::xray: case Effect::pop_art:
    case Effect::crt_phosphor: return value<effect_count;
    default:return false;
    }
}
inline constexpr bool calibrated_palette_effect(unsigned value) noexcept {
    switch(static_cast<Effect>(value)) {
    case Effect::monochrome: case Effect::sepia: case Effect::thermal: case Effect::pastel:
    case Effect::posterized: case Effect::ice: case Effect::film: case Effect::negative:
    case Effect::solarized: case Effect::amber: case Effect::emerald: case Effect::cyanotype:
    case Effect::copper: case Effect::lavender: case Effect::cga: case Effect::teal_orange:
    case Effect::handheld: case Effect::bleach_bypass: case Effect::risograph:
    case Effect::duotone: case Effect::tritone: case Effect::iridescent: case Effect::noir:
    case Effect::uv_glow: case Effect::topographic:return value<effect_count;
    default:return false;
    }
}
inline constexpr bool calibrated_composite_effect(unsigned value) noexcept {
    return calibrated_post_effect(value) || calibrated_palette_effect(value)
        || (value<effect_count && (spatial_manipulation(static_cast<Effect>(value))
            || (value>=unsigned(Effect::energy_shield) && value<=unsigned(Effect::gravitational_lens))));
}
struct CalibratedPostEffects {
    unsigned world{},model{},world_intensity{100},model_intensity{100};
    float seconds{};
    bool active() const noexcept {
        return (world && world_intensity) || (model && model_intensity);
    }
};
// Only the explicit pointwise, time-independent colour equations in
// calibrated_colour.hlsli keep the geometry's samples. Do not infer this from
// an effect's name: neighbouring taps, pixel patterns and distortions still
// need their own correspondence, even when their parameters are held.
// Bit 0 is WORLD (ownership class 1), bit 1 MODEL (class 2). Untracked
// image effects reject their own samples, not the other layer's valid motion.
inline constexpr bool calibrated_pattern_effect(unsigned value) noexcept {
    return value==unsigned(Effect::dithered) || value==unsigned(Effect::night_vision)
        || value==unsigned(Effect::scanlines) || value==unsigned(Effect::crt_phosphor);
}
// These become pointwise colour equations once their actual local edge branch
// (and Blueprint/Hologram grid branch) is witnessed. Neighbour averages, warps
// and animation are deliberately NOT inferred to have this correspondence.
inline constexpr bool calibrated_edge_effect(unsigned value) noexcept {
    switch(static_cast<Effect>(value)) {
    case Effect::cel_drawn: case Effect::ink: case Effect::neon: case Effect::blueprint:
    case Effect::comic: case Effect::vaporwave: case Effect::chalk: case Effect::stained_glass:
    case Effect::hologram: case Effect::pencil: case Effect::woodcut: case Effect::xray:
    case Effect::pop_art:return true;
    default:return false;
    }
}
// This opt-in is only for a resolver that stores and checks every positive
// history tap's screen-pattern phase. Native SDK opts in only with its resident
// AA/accepted-phase rejection guide; secondary transport stays guarded.
inline constexpr unsigned calibrated_post_motion_rejection_layers(const CalibratedPostEffects& pass,
    bool pattern_witness=false,bool edge_witness=false) noexcept {
    const auto tracked=[&](unsigned effect) {return calibrated_palette_effect(effect)
        || (pattern_witness && calibrated_pattern_effect(effect))
        || (edge_witness && calibrated_edge_effect(effect));};
    return (pass.world && pass.world_intensity && !tracked(pass.world)?1U:0U)
        | (pass.model && pass.model_intensity && !tracked(pass.model)?2U:0U);
}
inline constexpr bool calibrated_post_requires_motion_rejection(const CalibratedPostEffects& pass) noexcept {
    return calibrated_post_motion_rejection_layers(pass)!=0;
}
// Multi-source neighbourhoods and coordinate/animated transformations do not
// have the primary fragment's motion. Reconstruct geometric radiance FIRST,
// then apply the complete ordered style chain on the unjittered centre grid.
// Moving the whole chain preserves dependencies between its three slots. The
// witnessed pointwise/pattern/edge path remains available for other chains.
// This is an ordering policy, NOT permission to use raw guides for styled RGB.
inline constexpr bool calibrated_post_after_reconstruction(
    const std::array<CalibratedPostEffects,3>& passes) noexcept {
    for(const auto& pass:passes)
        if(calibrated_post_motion_rejection_layers(pass,true,true)) return true;
    return false;
}
// The opaque SDK has a resident pattern-phase guide, but NOT the native TAA
// resolver's edge-branch witnesses. Drawn styles therefore consume the actual
// reconstructed centre image, together with their whole ordered chain. Do not
// pretend the primary geometric motion describes a changing outline branch.
inline constexpr bool calibrated_sdk_post_after_reconstruction(
    const std::array<CalibratedPostEffects,3>& passes) noexcept {
    for(const auto& pass:passes)
        if(calibrated_post_motion_rejection_layers(pass,true,false)) return true;
    return false;
}
struct CalibratedPatternPass {
    unsigned world{},model{},world_intensity{},model_intensity{};
    bool operator==(const CalibratedPatternPass&) const =default;
};
struct CalibratedPatternGuide {
    unsigned scale{1};
    std::array<CalibratedPatternPass,3> passes{};
    bool operator==(const CalibratedPatternGuide&) const =default;
    constexpr bool valid() const noexcept {
        if(!scale || scale>16384) return false;
        for(const auto& p:passes) {
            for(const auto pair:{std::array{p.world,p.world_intensity},std::array{p.model,p.model_intensity}})
                if(pair[0]?(!calibrated_pattern_effect(pair[0]) || !pair[1] || pair[1]>100):pair[1]!=0) return false;
        }
        return true;
    }
};
// Preserve pass order, receiver class, intensity and material grid in the
// accepted-history key. A rejected candidate must not change this descriptor.
inline CalibratedPatternGuide calibrated_pattern_guide(const std::array<CalibratedPostEffects,3>& passes,
    unsigned scale) noexcept {
    CalibratedPatternGuide result;result.scale=scale;
    for(unsigned i=0;i<passes.size();++i) {
        const auto& p=passes[i];auto& q=result.passes[i];
        if(p.world_intensity && calibrated_pattern_effect(p.world)) {q.world=p.world;q.world_intensity=p.world_intensity;}
        if(p.model_intensity && calibrated_pattern_effect(p.model)) {q.model=p.model;q.model_intensity=p.model_intensity;}
    }
    return result;
}
struct CalibratedEdgeGuide {
    unsigned scale{1};std::array<CalibratedPatternPass,3> passes{};
    bool operator==(const CalibratedEdgeGuide&) const =default;
    constexpr bool valid() const noexcept {
        if(!scale || scale>16384) return false;
        for(const auto& p:passes) for(const auto pair:{std::array{p.world,p.world_intensity},std::array{p.model,p.model_intensity}})
            if(pair[0]?(!calibrated_edge_effect(pair[0]) || !pair[1] || pair[1]>100):pair[1]!=0) return false;
        return true;
    }
    constexpr std::array<unsigned,2> masks() const noexcept {
        std::array<unsigned,2> result{};
        for(unsigned i=0;i<passes.size();++i) {
            if(passes[i].world) result[0]|=1U<<(i*3);
            if(passes[i].model) result[1]|=1U<<(i*3);
        }
        return result;
    }
    constexpr bool active() const noexcept {const auto m=masks();return m[0] || m[1];}
};
inline CalibratedEdgeGuide calibrated_edge_guide(const std::array<CalibratedPostEffects,3>& passes,unsigned scale) noexcept {
    CalibratedEdgeGuide result;result.scale=scale;
    for(unsigned i=0;i<passes.size();++i) {
        const auto& p=passes[i];auto& q=result.passes[i];
        if(p.world_intensity && calibrated_edge_effect(p.world)) {q.world=p.world;q.world_intensity=p.world_intensity;}
        if(p.model_intensity && calibrated_edge_effect(p.model)) {q.model=p.model;q.model_intensity=p.model_intensity;}
    }
    // A branch observed before/after an untracked average, warp or animated
    // pass is not correspondence for that layer's final styled radiance.
    for(unsigned layer=0;layer<2;++layer) {
        const bool untracked=std::any_of(passes.begin(),passes.end(),[&](const auto& p) {
            const unsigned effect=layer?p.model:p.world,intensity=layer?p.model_intensity:p.world_intensity;
            return effect && intensity && !calibrated_palette_effect(effect)
                && !calibrated_pattern_effect(effect) && !calibrated_edge_effect(effect);
        });
        if(untracked) for(auto& p:result.passes) {
            if(layer) p.model=p.model_intensity=0;else p.world=p.world_intensity=0;
        }
    }
    return result;
}
// Presentation-time FX continue on a frozen preview, but not while paused.
// Call on every host loop, including compositor waits and frame-debug freezes.
class CalibratedEffectClock {
public:
    float sample(double timestamp,bool paused) noexcept {
        if(!std::isfinite(timestamp) || timestamp<0) return float(seconds_);
        if(initialized_ && timestamp>=previous_ && !paused && !paused_) seconds_+=timestamp-previous_;
        previous_=timestamp;paused_=paused;initialized_=true;return float(seconds_);
    }
private:
    double previous_{},seconds_{};bool initialized_{},paused_{};
};
}
