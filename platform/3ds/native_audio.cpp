#include "native_audio.hpp"
#include "native_audio_owner.hpp"
#include <cstdio>
#include <string>
// Use the actual libctru headers, with its C linkage, without pulling in
// unrelated graphics/thread services through the umbrella header.
extern "C" {
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/allocator/linear.h>
#include <3ds/services/dsp.h>
#include <3ds/ndsp/ndsp.h>
#include <3ds/ndsp/channel.h>
}

namespace starfox::platform::nintendo_3ds {
namespace {
constexpr int channel=0;
static_assert(NDSP_WBUF_FREE==int(AudioBufferState::free)
    && NDSP_WBUF_QUEUED==int(AudioBufferState::queued)
    && NDSP_WBUF_PLAYING==int(AudioBufferState::playing)
    && NDSP_WBUF_DONE==int(AudioBufferState::done));
struct LinearDeleter {
    void operator()(std::int16_t* data) const noexcept {if(data) linearFree(data);}
};
std::runtime_error dsp_error(const char* operation,Result result,bool startup=false) {
    char code[16]{};std::snprintf(code,sizeof(code),"%08lX",static_cast<unsigned long>(static_cast<u32>(result)));
    return std::runtime_error(std::string(operation)+" ("+code+")."+(startup
        ?" Check the homebrew DSP setup or /3ds/dspfirm.cdc from your own console. No DSP firmware is bundled.":""));
}
} // namespace
struct NativeAudio::Impl {
    detail::AudioOutputLease lease;
    std::unique_ptr<std::int16_t,LinearDeleter> pcm{
        static_cast<std::int16_t*>(linearAlloc(AudioPcm::storage_bytes))};
    std::array<ndspWaveBuf,AudioPcm::blocks> waves{};
    unsigned cursor{};
    bool initialized{};
    Impl() {
        if(!pcm) throw std::runtime_error("3DS audio linear-memory allocation failed (51200 bytes)");
        initialize();
    }
    ~Impl() {shutdown();} // PCM/descriptor destruction follows DSP shutdown.
    void initialize() {
        const auto result=ndspInit();
        if(R_FAILED(result)) throw dsp_error("3DS DSP initialization failed",result,true);
        initialized=true;
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspChnReset(channel);
        ndspChnSetFormat(channel,NDSP_FORMAT_STEREO_PCM16);
        ndspChnSetRate(channel,AudioPcm::rate);
        ndspChnSetInterp(channel,NDSP_INTERP_LINEAR);
        float mix[12]{};mix[0]=1;mix[1]=1;ndspChnSetMix(channel,mix);
        cursor=0;
        for(unsigned i=0;i<waves.size();++i) {
            waves[i]={};waves[i].data_pcm16=pcm.get()+i*AudioPcm::samples;
            waves[i].nsamples=AudioPcm::frames; // NOT 3200 samples / 6400 bytes.
        }
    }
    void shutdown() noexcept {
        if(!initialized) return;
        ndspChnWaveBufClear(channel);
        // libctru joins its audio worker and finalizes the DSP here. Do not
        // free/rewrite DMA storage immediately after WaveBufClear instead.
        ndspExit();initialized=false;
    }
    void require_ready() const {
        if(!initialized) throw std::runtime_error("3DS DSP is not ready; recreate the audio output");
    }
    auto states() const {
        std::array<AudioBufferState,AudioPcm::blocks> result{};
        for(unsigned i=0;i<waves.size();++i) {
            // libctru publishes status from its worker. Follow its wave-buffer
            // polling interface; prevent caching this byte across host calls.
            const volatile ndspWaveBuf& wave=waves[i];
            result[i]=static_cast<AudioBufferState>(wave.status);
        }
        return result;
    }
};
NativeAudio::NativeAudio():impl_(std::make_unique<Impl>()) {}
NativeAudio::~NativeAudio()=default;
bool NativeAudio::try_submit(std::span<const std::int16_t> source) {
    impl_->require_ready();
    if(source.size()!=AudioPcm::samples)
        throw std::invalid_argument("3DS audio requires 1600 interleaved stereo frames");
    const auto slot=next_audio_buffer(impl_->states(),impl_->cursor);
    if(!slot) return false;
    auto& wave=impl_->waves[*slot];
    copy_audio_pcm(source,{wave.data_pcm16,AudioPcm::samples});
    const auto flushed=DSP_FlushDataCache(wave.data_pcm16,AudioPcm::bytes);
    if(R_FAILED(flushed)) throw dsp_error("3DS audio cache flush failed",flushed);
    ndspChnWaveBufAdd(channel,&wave);
    impl_->cursor=(*slot+1)%AudioPcm::blocks;
    return true;
}
void NativeAudio::submit(std::span<const std::int16_t> source) {
    if(!try_submit(source)) throw std::runtime_error("3DS audio queue exhausted; no active PCM was overwritten");
}
unsigned NativeAudio::available_blocks() const {
    impl_->require_ready();
    const auto states=impl_->states();
    return static_cast<unsigned>(std::count_if(states.begin(),states.end(),audio_buffer_writable));
}
void NativeAudio::pause(bool paused) {impl_->require_ready();ndspChnSetPaused(channel,paused);}
void NativeAudio::reset() {impl_->shutdown();impl_->initialize();}
} // namespace starfox::platform::nintendo_3ds
