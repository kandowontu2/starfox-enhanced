#pragma once
#include <memory>
#include <string>
namespace starfox::render {
class GpuSmaa {
public:
    GpuSmaa();~GpuSmaa();
    // Records all three SMAA 1x passes. Inputs and command share one SDL device.
    // Output is borrowed until next enqueue/resize/release; caller submits.
    void* enqueue(void* device,void* command,void* color,void* packed_pixels,
        unsigned width,unsigned height,unsigned quality,bool packed_tags=true);
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
