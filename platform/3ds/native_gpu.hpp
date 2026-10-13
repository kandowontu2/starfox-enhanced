#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/platform/nintendo_3ds/pica_display_sync.hpp"
#include <memory>

namespace starfox::platform::nintendo_3ds {
class PicaValidatedFrame;
// NativeDisplay owns gfx/HID. This and NativeMono can retain resources in one
// context, but only the activated owner submits. Retire both before CPU swaps.
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
    // Select source_clock only after PresentationClock admitted a ready frame.
    // It skips display pacing, never the blocking GPU queue retirement.
    void present(const PicaFrame&,ImageView dashboard,PicaDisplayPacing=PicaDisplayPacing::lcd);
    // Trusted composition still checks its owner/generation and dashboard;
    // changed texture bytes retain every residency/provenance check.
    void present(const PicaValidatedFrame&,ImageView dashboard,PicaDisplayPacing=PicaDisplayPacing::lcd);
private:
    void present_checked(const PicaFrame&,ImageView dashboard,PicaDisplayPacing);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace starfox::platform::nintendo_3ds
