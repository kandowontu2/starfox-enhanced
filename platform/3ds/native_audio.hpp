#pragma once
#include "starfox/platform/nintendo_3ds/audio_pcm.hpp"
#include <memory>

namespace starfox::platform::nintendo_3ds {
// Sole process libctru/NDSP owner. Do not mix with other ndspInit/Exit clients.
// Main-thread methods only; the DSP worker owns
// QUEUED/PLAYING blocks. Construct before GameSession and outlive its PCM sink.
class NativeAudio {
public:
    NativeAudio();
    ~NativeAudio();
    NativeAudio(const NativeAudio&)=delete;
    NativeAudio& operator=(const NativeAudio&)=delete;
    NativeAudio(NativeAudio&&)=delete;
    NativeAudio& operator=(NativeAudio&&)=delete;

    // Copies synchronously into DSP-visible linear memory. No sleep, allocation
    // or overwrite of active buffers. Invalid blocks throw before queue changes.
    [[nodiscard]] bool try_submit(std::span<const std::int16_t>);
    // GameSession sink: exhaustion is an explicit error, never silent PCM loss.
    void submit(std::span<const std::int16_t>);
    [[nodiscard]] unsigned available_blocks() const;
    void pause(bool); // Home/suspend: preserve queued PCM and source audio phase.
    // Cartridge/state handoff only. Retire the old DSP worker before reusing
    // storage; clearing a channel alone does not synchronously end hardware DMA.
    void reset();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace starfox::platform::nintendo_3ds
