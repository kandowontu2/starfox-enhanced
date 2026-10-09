#include "starfox/vr/pcm_output.hpp"
#include "starfox/app/audio_queue.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include <SDL3/SDL.h>
#include <array>
namespace starfox::vr {
PcmOutput::~PcmOutput() {close();}
void PcmOutput::close() noexcept {
    if(stream_) SDL_DestroyAudioStream(stream_);
    stream_=nullptr;active_=false;
    if(initialized_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized_=false;
}
bool PcmOutput::open() {
    close();
    if(!SDL_InitSubSystem(SDL_INIT_AUDIO)) {status_=SDL_GetError();return false;}
    initialized_=true;
    const SDL_AudioSpec spec{SDL_AUDIO_S16,2,int(audio::Spc700Audio::sample_rate)};
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
    if(!stream_) {status_=SDL_GetError();close();return false;}
    status_="Native PCM playback ready";return true;
}
bool PcmOutput::set_active(bool active) {
    if(!stream_) {status_="PCM playback is not open";return false;}
    if(active==active_) return true;
    if(!active) {
        if(!SDL_PauseAudioStreamDevice(stream_) || !SDL_ClearAudioStream(stream_)) {status_=SDL_GetError();return false;}
        active_=false;return true;
    }
    constexpr auto frames=audio::Spc700Audio::sample_rate*app::realtime_audio_startup_ms/1000U;
    const std::array<int16_t,frames*2> silence{};
    if(!SDL_ClearAudioStream(stream_) || !SDL_PutAudioStreamData(stream_,silence.data(),int(sizeof(silence)))
        || !SDL_ResumeAudioStreamDevice(stream_)) {status_=SDL_GetError();return false;}
    active_=true;return true;
}
bool PcmOutput::push(std::span<const int16_t> samples) {
    constexpr auto frames=audio::Spc700Audio::sample_rate*app::realtime_audio_limit_ms/1000U;
    if(!stream_ || samples.size()%2 || samples.size()>frames*2U) {status_="Invalid PCM playback packet";return false;}
    if(!active_) return true;
    if(!app::queue_realtime_audio(stream_,samples,frames)) {status_=SDL_GetError();return false;}
    return true;
}
int PcmOutput::queued_bytes() const noexcept {return stream_?SDL_GetAudioStreamQueued(stream_):0;}
}
