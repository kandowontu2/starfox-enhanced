#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>

namespace starfox::platform::nintendo_3ds {
// One source SPC tick, not one LCD/eye frame. NDSP's stereo nsamples is the
// number of frames; DMA/cache-flush sizes are bytes, including both channels.
struct AudioPcm {
    static constexpr unsigned rate=32'000,channels=2,frames=1'600;
    static constexpr unsigned samples=frames*channels,bytes=samples*sizeof(std::int16_t);
    // Five blocks can arrive from the bounded 250ms source catch-up. Leave
    // room for ordinary playback without allocating during a source tick.
    static constexpr unsigned blocks=8,storage_bytes=blocks*bytes;
};
enum class AudioBufferState : std::uint8_t {free,queued,playing,done};
constexpr bool audio_buffer_writable(AudioBufferState state) noexcept {
    return state==AudioBufferState::free || state==AudioBufferState::done;
}
inline std::optional<unsigned> next_audio_buffer(
    const std::array<AudioBufferState,AudioPcm::blocks>& states,unsigned cursor) {
    if(cursor>=AudioPcm::blocks) throw std::invalid_argument("Invalid 3DS audio cursor");
    for(unsigned i=0;i<AudioPcm::blocks;++i) {
        const auto slot=(cursor+i)%AudioPcm::blocks;
        if(audio_buffer_writable(states[slot])) return slot;
    }
    return std::nullopt; // Never overwrite QUEUED/PLAYING (or unknown) storage.
}
inline void copy_audio_pcm(std::span<const std::int16_t> source,std::span<std::int16_t> destination) {
    if(source.size()!=AudioPcm::samples || destination.size()!=AudioPcm::samples)
        throw std::invalid_argument("3DS audio requires 1600 interleaved stereo frames");
    const auto src=reinterpret_cast<std::uintptr_t>(source.data());
    const auto dst=reinterpret_cast<std::uintptr_t>(destination.data());
    if((dst>=src && dst-src<AudioPcm::bytes) || (src>=dst && src-dst<AudioPcm::bytes))
        throw std::invalid_argument("3DS audio requires independent DSP storage");
    std::copy(source.begin(),source.end(),destination.begin());
}
} // namespace starfox::platform::nintendo_3ds
