#pragma once
#include "starfox/render/motion_blur.hpp"
#include <array>
#include <memory>
#include <string>
#include "starfox/render/sdl_dxr_shadows.hpp"

namespace starfox::render {
// Bridge the actual calibrated colour/ownership/surface/motion MRTs to the
// shared centred-shutter foreground reconstruction. This is NOT a persistence
// effect or a surface-local gather: vacated/swept silhouettes reveal the caller's
// same-presentation, model-free, fully styled world underlay.
//
// Both input colours and destination use the initialized RGBA/BGRA UNORM/sRGB
// format. Ownership is RGBA8_UNORM; surfaces and motion are RGBA32_FLOAT.
// Native motion.xy is previous-minus-current pixels, motion.z is PREVIOUS
// forward depth, and motion.w is explicit correspondence. Current depth comes
// from surfaces.w. sample_delta is current-minus-previous raster jitter (pixels),
// removes the raster-offset difference, NEVER creating actual surface velocity.
// Optional native water layers replace visible liquid depth and discard the
// submerged rigid-model velocity. Liquid is current-only until a physical
// liquid correspondence exists; dry/protected pixels retain their guides.
//
// Caller owns and submits/cancels the command and all inputs. No submission,
// readback, history acceptance, CPU geometry transformation or fence wait here.
// Consume each destination before reusing this encoder's scratch on the same
// ordered queue. Different eye destinations retain their independent results.
// Identity WITHOUT resident particles returns source with ZERO allocations/
// shader preparation/dispatches; destination is untouched and other guides/
// underlay need not exist. A particle payload still renders current-only colour
// on identity; its validated resident projection is supplied by SceneFx.
class GpuCalibratedMotionBlur {
public:
    GpuCalibratedMotionBlur();
    ~GpuCalibratedMotionBlur();
    GpuCalibratedMotionBlur(const GpuCalibratedMotionBlur&)=delete;
    GpuCalibratedMotionBlur& operator=(const GpuCalibratedMotionBlur&)=delete;
    bool initialize(void* device,int color_format);
    bool enqueue(void* command,void* source,void* underlay,void* ownership,
        void* surfaces,void* motion,void* destination,unsigned width,unsigned height,
        const MotionBlurSettings&,bool history_valid,void*& output,
        std::array<float,2> sample_delta={},std::array<float,2> guide_jitter={},
        void* resident_particles=nullptr,float particle_scale=1,
        const shadows::GpuReflectionOutput* water=nullptr);
    void release_device() noexcept;
    unsigned dispatch_count() const noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
