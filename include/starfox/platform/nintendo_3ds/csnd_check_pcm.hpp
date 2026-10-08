#pragma once
#include "audio_pcm.hpp"
#include <cmath>

namespace starfox::platform::nintendo_3ds {
// An immutable one-shot compatibility test, NOT a streaming game-audio ring.
struct CsndCheckPcm {
    static constexpr unsigned frames=AudioPcm::rate;
    static constexpr unsigned plane_bytes=frames*sizeof(std::int16_t);
    static constexpr unsigned storage_bytes=2*plane_bytes;
};
inline std::optional<std::array<unsigned,2>> csnd_stereo_channels(std::uint32_t available) noexcept {
    std::array<unsigned,2> pair{};unsigned count=0;
    for(unsigned channel=0;channel<32;++channel)
        if(available&(std::uint32_t{1}<<channel)) {
            pair[count++]=channel;if(count==2) return pair;
        }
    return std::nullopt;
}
inline void fill_csnd_check_pcm(std::span<std::int16_t> left,std::span<std::int16_t> right) {
    if(left.size()!=CsndCheckPcm::frames || right.size()!=CsndCheckPcm::frames)
        throw std::invalid_argument("CSND test requires two independent 32000-frame mono planes");
    const auto a=reinterpret_cast<std::uintptr_t>(left.data());
    const auto b=reinterpret_cast<std::uintptr_t>(right.data());
    if((a>=b && a-b<CsndCheckPcm::plane_bytes) || (b>=a && b-a<CsndCheckPcm::plane_bytes))
        throw std::invalid_argument("CSND stereo test planes overlap");
    // Same quiet left-220Hz/right-440Hz test as the NDSP probe. No SPC,
    // borrowed source memory, elapsed-time cursor or live DMA rewrite.
    for(unsigned frame=0;frame<CsndCheckPcm::frames;++frame) {
        const bool first=frame<CsndCheckPcm::frames/2;
        const auto sample=static_cast<std::int16_t>(4096*std::sin(
            6.283185307179586*(first?220:440)*frame/AudioPcm::rate));
        left[frame]=first?sample:0;right[frame]=first?0:sample;
    }
}
} // namespace starfox::platform::nintendo_3ds
