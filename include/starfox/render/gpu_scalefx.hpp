#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace starfox::render {
struct GpuScaleFxOutput {
    void* buffer{}; // borrowed SDL_GPUBuffer of tightly packed RGBA float4
    std::uint32_t width{}, height{};
};

class GpuScaleFx {
public:
    GpuScaleFx();
    ~GpuScaleFx();
    // Record into the caller's command buffer on its existing SDL GPU device.
    // Input: width*height normalized RGBA float4 values, storage-readable.
    // No submission, upload, readback or CPU/GPU wait is performed here.
    // Call with no active pass. On failure, caller must cancel the command.
    // Consume output before the next enqueue; never feed our output back as input.
    // Release this object before destroying the borrowed device, after command
    // buffers using it have been submitted or cancelled.
    GpuScaleFxOutput enqueue(void* device, void* command, void* input,
        std::uint32_t width, std::uint32_t height);
    // Same contract with a storage-readable RGBA texture instead of float4
    // buffer. Adapter and all filtering stay on the GPU.
    GpuScaleFxOutput enqueue_texture(void* device, void* command, void* texture,
        std::uint32_t width, std::uint32_t height);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
