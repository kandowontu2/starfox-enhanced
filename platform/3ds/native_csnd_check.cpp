#include "native_csnd_check.hpp"
#include "native_audio_owner.hpp"
#include "starfox/platform/nintendo_3ds/csnd_check_pcm.hpp"
#include <cstdio>
#include <string>
extern "C" {
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/allocator/linear.h>
#include <3ds/os.h>
#include <3ds/svc.h>
#include <3ds/services/csnd.h>
}

namespace starfox::platform::nintendo_3ds {
namespace {
std::runtime_error csnd_error(const char* operation,Result result) {
    char code[16]{};std::snprintf(code,sizeof(code),"%08lX",static_cast<unsigned long>(static_cast<u32>(result)));
    return std::runtime_error(std::string(operation)+" ("+code+"). CSND check only; gameplay still uses NDSP.");
}
struct LinearDeleter {
    void operator()(std::int16_t* data) const noexcept {if(data) linearFree(data);}
};
// libctru csndExecCmds(true) busy-waits on the first command's completion
// byte without a deadline. Use its same acknowledgment, but bound this check
// to 100ms and yield. The sole service lease guarantees an empty command list.
volatile u8* begin_commands() {
    auto* parameters=csndAddCmd(0x300); // Documented UpdateInfo command.
    return reinterpret_cast<volatile u8*>(parameters)-4;
}
void finish_commands(volatile u8* done) {
    const auto result=csndExecCmds(false);
    if(result!=0) throw csnd_error("CSND command submission failed",result);
    const auto began=svcGetSystemTick();
    while(*done==0) {
        if(svcGetSystemTick()-began>=SYSCLOCK_ARM11/10)
            throw std::runtime_error("CSND command acknowledgment timed out (100ms). Gameplay output was not changed.");
        svcSleepThread(1'000'000);
    }
}
} // namespace
struct NativeCsndCheck::Impl {
    detail::AudioOutputLease lease;
    std::unique_ptr<std::int16_t,LinearDeleter> pcm;
    std::array<unsigned,2> channels{};
    bool initialized{},started{};
    Impl() {
        const auto result=csndInit();
        if(result!=0) throw csnd_error("CSND service initialization failed",result);
        initialized=true;
        try {
            const auto pair=csnd_stereo_channels(csndChannels);
            if(!pair) throw std::runtime_error("CSND did not grant two stereo-test channels. Gameplay still uses NDSP.");
            channels=*pair;
            pcm.reset(static_cast<std::int16_t*>(linearAlloc(CsndCheckPcm::storage_bytes)));
            if(!pcm) throw std::runtime_error("CSND stereo-check linear-memory allocation failed (128000 bytes)");
        } catch(...) {shutdown();throw;}
    }
    ~Impl() {shutdown();} // Service retired before linear DMA memory is freed.
    void execute(volatile u8* done) {
        try {finish_commands(done);}
        catch(...) {
            // Never append another list after an unacknowledged/failed list.
            // Retire the service before this owner can rewrite/free its PCM.
            csndExit();initialized=false;started=false;throw;
        }
    }
    void shutdown() noexcept {
        if(!initialized) return;
        try {
            if(started) {
                auto* done=begin_commands();
                for(auto channel:channels) CSND_SetPlayStateR(channel,0);
                finish_commands(done);
            }
        } catch(...) { /* csndExit releases channels and shuts down the service. */ }
        csndExit();initialized=false;started=false;
    }
    bool finished() {
        if(!initialized) throw std::runtime_error("CSND check owner is no longer ready");
        if(!started) return true;
        auto* done=begin_commands();execute(done);
        // This is actual service state, not an elapsed-time DMA estimate.
        const auto* left=csndGetChnInfo(channels[0]);
        const auto* right=csndGetChnInfo(channels[1]);
        if(!left || !right) throw std::runtime_error("CSND channel information unavailable");
        return left->active==0 && right->active==0;
    }
};
NativeCsndCheck::NativeCsndCheck():impl_(std::make_unique<Impl>()) {}
NativeCsndCheck::~NativeCsndCheck()=default;
void NativeCsndCheck::start() {
    if(!impl_->finished()) throw std::logic_error("CSND check still playing; active DMA was not rewritten");
    auto* left=impl_->pcm.get();auto* right=left+CsndCheckPcm::frames;
    fill_csnd_check_pcm({left,CsndCheckPcm::frames},{right,CsndCheckPcm::frames});
    const auto flushed=CSND_FlushDataCache(left,CsndCheckPcm::storage_bytes);
    if(flushed!=0) throw csnd_error("CSND check cache flush failed",flushed);
    static_assert(CSND_TIMER(AudioPcm::rate)==2094);
    const u32 flags=SOUND_ENABLE|SOUND_ONE_SHOT|SOUND_FORMAT_16BIT|SOUND_LINEAR_INTERP
        |(CSND_TIMER(AudioPcm::rate)<<16);
    auto* done=begin_commands();
    // Record attempted ownership before submitting: even a partial failure
    // must stop BOTH channels and retire the service before freeing storage.
    impl_->started=true;
    for(unsigned side=0;side<2;++side) {
        auto* plane=side?right:left;const auto physical=osConvertVirtToPhys(plane);
        const auto volume=CSND_VOL(1.F,side?1.F:-1.F);
        CSND_SetChnRegs(flags|SOUND_CHANNEL(impl_->channels[side]),physical,0,
            CsndCheckPcm::plane_bytes,volume,volume);
    }
    impl_->execute(done); // Both stereo starts in the same command batch.
}
bool NativeCsndCheck::finished() {return impl_->finished();}
} // namespace starfox::platform::nintendo_3ds
