#pragma once
#include "starfox/render/fsr1_settings.hpp"
#include <memory>
#include <string>
namespace starfox::render {
// One instance per native eye; the existing presenter owns commands and fences.
// Genuine lower-resolution scene -> EASU/RCAS -> full-resolution native ink.
class GpuCalibratedFsr1 {
public:
    GpuCalibratedFsr1();~GpuCalibratedFsr1();
    bool initialize(void* device,int color_format);
    bool enqueue(void* command,void* scene,Fsr1Extent input,void* native_ink,
        void* native_ownership,void* destination,Fsr1Extent output);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
