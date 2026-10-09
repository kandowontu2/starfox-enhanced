#pragma once
#include "starfox/vr/eye_camera.hpp"
#include <memory>
#include <string>
namespace starfox::render {
// AO / depth of field on the actual calibrated eye, using its raster-owned
// normals and forward depth in cartridge units. Encodes only; the frame owner
// retains all borrowed images until its GPU and compositor work completes.
class GpuCalibratedDepth {
public:
    GpuCalibratedDepth();~GpuCalibratedDepth();
    GpuCalibratedDepth(const GpuCalibratedDepth&)=delete;
    GpuCalibratedDepth& operator=(const GpuCalibratedDepth&)=delete;
    bool initialize(void* device,int color_format);
    bool enqueue(void* command,void* source,void* ownership,void* surfaces,void* destination,
        unsigned width,unsigned height,unsigned scale,unsigned modes,const vr::EyeCamera&,
        float focus_source_units=500);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
