#pragma once
#include <memory>
#include <string>
namespace starfox::render {
// The existing separate 2D/3D bloom qualities on actual calibrated eye images.
// Half-native-resolution float halos stay on GPU. Caller owns/submits the
// command and retains its inputs/destination through native presentation.
class GpuCalibratedBloom {
public:
    GpuCalibratedBloom();~GpuCalibratedBloom();
    GpuCalibratedBloom(const GpuCalibratedBloom&)=delete;
    GpuCalibratedBloom& operator=(const GpuCalibratedBloom&)=delete;
    bool initialize(void* device,int color_format);
    bool enqueue(void* command,void* source,void* ownership,void* destination,
        unsigned width,unsigned height,unsigned scale,unsigned model,unsigned world);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
