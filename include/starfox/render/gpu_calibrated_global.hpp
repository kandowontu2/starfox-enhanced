#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
// The existing 13 global menu enhancements and SDR appearance controls,
// evaluated on each actual native
// eye. Protected ownership and alpha stay exact, including neighbourhood taps.
// Encodes only; the caller retains all borrowed images through presentation.
class GpuCalibratedGlobal {
public:
    GpuCalibratedGlobal();~GpuCalibratedGlobal();
    GpuCalibratedGlobal(const GpuCalibratedGlobal&)=delete;
    GpuCalibratedGlobal& operator=(const GpuCalibratedGlobal&)=delete;
    bool initialize(void* device,int color_format);
    bool enqueue(void* command,void* source,void* ownership,void* destination,
        unsigned width,unsigned height,unsigned scale,std::uint32_t selections,float seconds);
    // One resident pass: byte-rounded SDR contrast followed by model-only
    // channel separation. World/UI taps cannot bleed into model channels.
    bool enqueue_appearance(void* command,void* source,void* ownership,void* destination,
        unsigned width,unsigned height,unsigned scale,unsigned contrast,unsigned chromatic);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    bool enqueue_impl(void* command,void* source,void* ownership,void* destination,
        unsigned width,unsigned height,unsigned scale,std::uint32_t selections,float seconds,
        unsigned contrast,unsigned chromatic,bool appearance);
    struct State;std::unique_ptr<State> state_;
};
}
