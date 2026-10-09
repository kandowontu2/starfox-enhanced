#include "starfox/vr/pcm_output.hpp"
#include "starfox/app/audio_queue.hpp"
#include "starfox/audio/stem_mixer.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void require(bool v) {if(!v) throw std::runtime_error("PCM playback assertion failed");}}
int main() try {
    std::vector<int16_t> mixed;
    const std::array<int16_t,8> music{32767,-32768,301,-301,100,-100,123,-123};
    const std::array<int16_t,8> effects{32767,-32768,-101,101,-100,100,0,0};
    starfox::audio::mix_stems(music,effects,100,100,mixed);
    require(mixed==std::vector<int16_t>({32767,-32768,200,-200,0,0,123,-123}));
    starfox::audio::mix_stems(music,effects,50,25,mixed);
    require(mixed==std::vector<int16_t>({24574,-24576,125,-125,25,-25,61,-61}));
    starfox::audio::mix_stems(music,effects,0,0,mixed);
    require(mixed==std::vector<int16_t>(8,0));
    starfox::audio::mix_stems(music,effects,255,255,mixed);
    require(mixed.front()==32767 && mixed[1]==-32768 && mixed[2]==200);
    starfox::audio::mix_stems(music,std::span<const int16_t>(effects.data(),3),100,100,mixed);
    require(mixed.size()==3);
    starfox::audio::mix_stems({},effects,100,100,mixed);require(mixed.empty());
    require(SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy"));
    starfox::vr::PcmOutput output;
    std::array<int16_t,3200> samples{};
    require(!output.push(samples));require(output.open());
    require(output.push(samples) && output.queued_bytes()==0);
    require(output.set_active(true));
    for(unsigned i=0;i<100;++i) {
        require(output.push(samples));
        require(output.queued_bytes()>=0 && output.queued_bytes()<=32000*4*starfox::app::realtime_audio_limit_ms/1000);
    }
    require(!output.push(std::span<const int16_t>(samples.data(),3)));
    const std::array<int16_t,10000> oversized{};require(!output.push(oversized));
    require(output.set_active(false) && output.queued_bytes()==0);
    require(output.set_active(false) && output.push(samples) && output.queued_bytes()==0);
    require(output.set_active(true) && output.push(samples));
    require(output.open() && output.queued_bytes()==0);
    output.close();output.close();require(!output.set_active(true));
    std::cout<<"SDL dummy PCM: bounded catch-up, inactive discard, focus clear/resume and reopen passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
