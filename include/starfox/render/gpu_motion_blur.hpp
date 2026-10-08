#pragma once
#include "starfox/render/gpu_composite.hpp"
#include <array>
#include <memory>
#include <string>
namespace starfox::render {
struct SceneFxFrame;
// Resident counterpart of reconstruct_motion_blur. The underlay must exclude
// moving foreground and HUD. Caller submits the command and owns all inputs.
// On failure cancel the command; on success the borrowed output remains valid
// until resize/release and must not be fed back as either input. No downloads
// or CPU fence waits occur here. Queue ordering protects scratch-buffer reuse.
// Identity frames without particles return the caller's input texture directly
// with zero dispatches. Optional particles are composed at each reconstruction
// sample against its foreground coverage/depth, not after model averaging.
class GpuMotionBlur {
public:
    GpuMotionBlur();
    ~GpuMotionBlur();
    bool enqueue(void* command,const GpuCompositeOutput& input,
        const GpuCompositeOutput& underlay,const MotionBlurSettings&,bool history_valid,void*& texture,
        std::array<float,2> guide_jitter={},const SceneFxFrame* particles=nullptr,float particle_scale=1,
        void* resident_particles=nullptr);
    // resident_particles is the GPU-only calibrated projection ABI. Mutually
    // exclusive with CPU SceneFxFrame. Caller validates source payloads and
    // consumes/cycles the borrowed buffer on the same command. No readback.
    void release_device() noexcept;
    unsigned dispatch_count() const noexcept;
    const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
