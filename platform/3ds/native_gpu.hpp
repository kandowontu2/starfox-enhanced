#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include <memory>

namespace starfox::platform::nintendo_3ds {
// NativeDisplay owns gfx/HID; this owns the sole Citro3D presenter. Do not call
// NativeDisplay::present/gfxSwapBuffers while this renderer is alive.
class NativeGpu {
public:
    explicit NativeGpu(std::span<const std::uint8_t> shader);
    ~NativeGpu();
    NativeGpu(const NativeGpu&)=delete;
    NativeGpu& operator=(const NativeGpu&)=delete;
    NativeGpu(NativeGpu&&)=delete;
    NativeGpu& operator=(NativeGpu&&)=delete;
    // Validates then waits for the previous GPU queue BEFORE overwriting any
    // VBO/texture. Geometry uploads once, projection/draw recording per eye.
    void present(const PicaFrame&,ImageView dashboard);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace starfox::platform::nintendo_3ds
