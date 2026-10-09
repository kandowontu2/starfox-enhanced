#pragma once
#include "starfox/render/gpu_composite.hpp"
#include "starfox/render/scene_enhancements.hpp"
#include <memory>
namespace starfox::render {
// Particle-only resident exposure pass. Input must not already contain these
// particles. Cancel the caller's command on failure. Output is borrowed until
// resize/release; enqueue it for consumption before reusing this object.
class GpuSceneShutter {
public:
    GpuSceneShutter();
    ~GpuSceneShutter();
    bool enqueue(void* command,const GpuCompositeOutput&,const SceneFxFrame&,
        const MotionBlurSettings&,float draw_scale,void*& output);
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
