#pragma once
#include "starfox/audio/msu1_audio.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include <algorithm>
#include <limits>

namespace starfox::audio {
inline void mix_stems(std::span<const int16_t> music,std::span<const int16_t> effects,
    uint8_t music_volume,uint8_t effects_volume,std::vector<int16_t>& output) {
    output.resize(std::min(music.size(),effects.size()));
    for(std::size_t i=0;i<output.size();++i) {
        const auto mixed=int32_t(music[i])*int32_t(std::min<unsigned>(music_volume,100))/100
            + int32_t(effects[i])*int32_t(std::min<unsigned>(effects_volume,100))/100;
        output[i]=static_cast<int16_t>(std::clamp(mixed,int32_t(std::numeric_limits<int16_t>::min()),int32_t(std::numeric_limits<int16_t>::max())));
    }
}
// Advance SPC even while MSU replaces its music stem: source handshakes and
// effects must retain their native timing. Select music before render(), as
// the desktop mixer does, so staff-roll completion changes on the next tick.
inline void render_mixed_tick(Spc700Audio& spc,Msu1Audio& msu,
    std::span<const simulation::ApuPortWrite> apu,std::span<const simulation::MsuRegisterWrite> writes,
    uint8_t music_volume,uint8_t effects_volume,std::vector<int16_t>& output) {
    spc.render_stems_logic_tick(apu);
    msu.process_register_writes(writes);
    const auto music=msu.enabled() && !msu.resume_native_music()
        ? msu.render(Spc700Audio::stereo_frames_per_logic_tick,Spc700Audio::sample_rate)
        : spc.last_music_samples();
    mix_stems(music,spc.last_effect_samples(),music_volume,effects_volume,output);
}
}
