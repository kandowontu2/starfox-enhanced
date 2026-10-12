// Exercise the actual native NDSP owner against explicit libctru test doubles.
// These are lifecycle/PCM/error contracts, not physical DSP/audio acceptance.
#include "../platform/3ds/native_audio.hpp"
#include "../platform/3ds/native_audio_owner.hpp"
extern "C" {
#include <3ds/ndsp/ndsp.h>
#include <3ds/ndsp/channel.h>
#include <3ds/services/dsp.h>
#include <3ds/allocator/linear.h>
}
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace starfox::platform::nintendo_3ds;
unsigned checks{};
void require(bool ok,const char* message) {++checks;if(!ok) throw std::runtime_error(message);}
template<class F> std::string rejects(F callback,const char* message) {
    try {callback();} catch(const std::exception& error) {++checks;return error.what();}
    throw std::runtime_error(message);
}
struct Mock {
    Result init_result{},flush_result{};
    bool allocation_failure{},ready{},paused{};
    unsigned allocations{},inits{},exits{},frees{},flushes{},adds{};
    void* storage{};
    const void* last_flushed{};
    std::array<ndspWaveBuf*,AudioPcm::blocks> waves{};
    std::array<std::vector<std::int16_t>,AudioPcm::blocks> submitted{};
    std::vector<unsigned> order;
    std::vector<std::string> events;
    void reset() {
        require(!ready && !storage,"Previous native NDSP service/storage leaked");
        *this=Mock{};
    }
    unsigned slot(const void* pointer) const {
        const auto base=reinterpret_cast<std::uintptr_t>(storage);
        const auto address=reinterpret_cast<std::uintptr_t>(pointer);
        require(storage && address>=base && address-base<AudioPcm::storage_bytes
            && (address-base)%AudioPcm::bytes==0,"NDSP buffer outside/aligned incorrectly in its owned pool");
        return unsigned((address-base)/AudioPcm::bytes);
    }
    void live_pcm_unchanged() const {
        for(unsigned i=0;i<waves.size();++i) if(waves[i]) {
            const auto* wave=waves[i];
            require(wave->data_pcm16==static_cast<std::int16_t*>(storage)+i*AudioPcm::samples
                && wave->nsamples==AudioPcm::frames && !wave->looping,
                "Active NDSP descriptor was rewritten before worker retirement");
            if(wave->status==NDSP_WBUF_QUEUED || wave->status==NDSP_WBUF_PLAYING)
                require(std::equal(submitted[i].begin(),submitted[i].end(),wave->data_pcm16),
                    "Active NDSP PCM overwritten");
        }
    }
} mock;
std::array<std::int16_t,AudioPcm::samples> samples(unsigned seed) {
    std::array<std::int16_t,AudioPcm::samples> result{};
    for(unsigned i=0;i<result.size();++i) result[i]=std::int16_t((i*17+seed*199)&0x7fff);
    result[0]=-32768;result[1]=32767;
    return result;
}
void queue_and_pause() {
    mock.reset();
    {
        NativeAudio output;
        require(output.available_blocks()==8 && mock.allocations==1 && mock.inits==1,"Incorrect native startup queue");
        rejects([]{NativeAudio second;},"Duplicate NDSP owner acquired service");
        rejects([]{detail::AudioOutputLease other;},"Second audio service lease acquired live output");
        require(mock.allocations==1 && mock.inits==1,"Rejected lease allocated/touched a service");
        for(unsigned i=0;i<AudioPcm::blocks;++i) {
            auto pcm=samples(i);const auto before=pcm;
            require(output.try_submit(pcm),"Available NDSP slot refused");
            require(pcm==before,"NDSP submission changed borrowed source PCM");
            pcm.fill(0);mock.live_pcm_unchanged();
            require(output.available_blocks()==AudioPcm::blocks-i-1,"Queued descriptor counted writable");
        }
        require(mock.order==std::vector<unsigned>({0,1,2,3,4,5,6,7}),"Native submission order changed");
        for(auto* wave:mock.waves) wave->status=NDSP_WBUF_PLAYING;
        const auto full_events=mock.events;
        require(!output.try_submit(samples(9)),"Full native queue overwritten");
        rejects([&]{output.submit(samples(9));},"PCM sink silently discarded an exhausted block");
        require(mock.events==full_events,"Exhausted queue flushed/submitted/reallocated PCM");
        mock.live_pcm_unchanged();
        output.pause(true);require(mock.paused,"Home/suspend did not pause native channel");
        mock.live_pcm_unchanged();
        output.pause(false);require(!mock.paused,"Resume did not unpause native channel");
        mock.live_pcm_unchanged();
        mock.waves[5]->status=NDSP_WBUF_DONE;
        require(output.available_blocks()==1 && output.try_submit(samples(15)) && mock.order.back()==5,
            "Completed native slot not reused across cursor wrap");
        mock.waves[6]->status=255;
        require(output.available_blocks()==0 && !output.try_submit(samples(16)),"Unknown native status reused");
        const auto stable=mock.events;
        rejects([&]{output.submit({});},"Empty PCM accepted by actual adapter");
        const auto pcm=samples(20);
        rejects([&]{output.submit(std::span(pcm).first(AudioPcm::samples-1));},"Truncated PCM accepted by adapter");
        require(mock.events==stable,"Invalid PCM changed native queue/cache state");
    }
    require(mock.exits==1 && mock.frees==1,"Native queue owner not retired once");
    require(mock.events[mock.events.size()-3]=="clear" && mock.events[mock.events.size()-2]=="exit"
        && mock.events.back()=="free","Native storage freed before NDSP worker retirement");
}
void handoff_and_failures() {
    mock.reset();
    {
        NativeAudio output;
        output.submit(samples(1));output.submit(samples(2));output.pause(true);
        const auto* storage=mock.storage;
        output.reset();
        require(mock.storage==storage && mock.allocations==1 && mock.inits==2 && mock.exits==1,
            "State handoff reallocated storage or failed to retire old worker");
        require(output.available_blocks()==8 && !mock.paused,"Reset retained old descriptors/pause state");
        output.submit(samples(3));require(mock.order.back()==0,"Reset did not restart rotating queue");
        mock.init_result=-13;
        const auto message=rejects([&]{output.reset();},"Failed reset initialization accepted");
        require(message.find("FFFFFFF3")!=std::string::npos && message.find("dspfirm.cdc")!=std::string::npos,
            "Initialization error lost result/setup instructions");
        require(mock.inits==3 && mock.exits==2 && !mock.ready,"Failed reset retained old worker");
        const auto stable=mock.events;
        rejects([&]{output.submit(samples(4));},"Failed output allowed PCM submission");
        rejects([&]{output.pause(true);},"Failed output still touched NDSP pause");
        rejects([&]{static_cast<void>(output.available_blocks());},"Failed output reported writable buffers");
        rejects([]{NativeAudio other;},"Failed output released its still-owned storage lease");
        require(mock.events==stable,"Unavailable NDSP owner touched a service");
        mock.init_result=0;output.reset();output.submit(samples(5));
        require(mock.inits==4 && mock.exits==2 && mock.order.back()==0,"Explicit reset could not recover safely");
    }
    require(mock.exits==3 && mock.frees==1,"Reset recovery double-freed/failed to retire owner");
    mock.reset();mock.allocation_failure=true;
    rejects([]{NativeAudio output;},"Failed native linear allocation accepted");
    require(mock.inits==0 && mock.frees==0,"Allocation failure touched service/unowned storage");
    mock.reset();mock.init_result=-27;
    rejects([]{NativeAudio output;},"Failed native initialization accepted");
    require(mock.inits==1 && mock.exits==0 && mock.frees==1,"Failed initialization leaked/freed a live service");
    mock.reset();
    {
        NativeAudio output;output.submit(samples(1));
        mock.flush_result=-11;
        rejects([&]{output.submit(samples(2));},"Failed DSP cache flush accepted");
        require(mock.adds==1 && output.available_blocks()==7,"Failed flush queued/consumed a descriptor");
        mock.live_pcm_unchanged();
        mock.flush_result=0;output.submit(samples(3));
        require(mock.order.back()==1 && mock.adds==2,"Flush retry skipped the unsubmitted slot");
        const auto stable=mock.events;
        rejects([&]{output.submit(std::span(static_cast<std::int16_t*>(mock.storage)+2*AudioPcm::samples,AudioPcm::samples));},
            "Adapter allowed source/destination storage alias");
        require(mock.events==stable,"Aliased source flushed/queued PCM");
    }
    require(mock.exits==1 && mock.frees==1,"Failed cache flush leaked owner");
    mock.reset();{detail::AudioOutputLease csnd;rejects([]{NativeAudio output;},"NDSP acquired live CSND lease");}
    require(mock.allocations==0 && mock.inits==0,"CSND lease conflict touched native output");
    {NativeAudio output;output.submit(samples(9));} // Every prior failure released its lease.
}
void channel(int id) {require(mock.ready && id==0,"Unowned/wrong native NDSP channel");}
} // namespace
extern "C" {
void* linearAlloc(size_t bytes) {
    require(!mock.storage && bytes==AudioPcm::storage_bytes,"Wrong/duplicate NDSP linear allocation");
    ++mock.allocations;mock.events.push_back("allocate");
    if(mock.allocation_failure) return nullptr;
    mock.storage=std::malloc(bytes);return mock.storage;
}
void linearFree(void* pointer) {
    require(!mock.ready && pointer==mock.storage,"NDSP storage freed before service retirement");
    ++mock.frees;mock.events.push_back("free");std::free(pointer);mock.storage=nullptr;
}
Result ndspInit(void) {
    require(!mock.ready && mock.storage,"NDSP initialized without its owned pool/old worker retirement");
    ++mock.inits;mock.events.push_back("init");
    if(mock.init_result<0) return mock.init_result;
    mock.ready=true;mock.paused=false;mock.waves.fill(nullptr);
    for(auto& pcm:mock.submitted) pcm.clear();
    mock.last_flushed=nullptr;
    return mock.init_result;
}
void ndspExit(void) {
    require(mock.ready,"NDSP exited an unowned service");mock.live_pcm_unchanged();
    ++mock.exits;mock.events.push_back("exit");mock.ready=false;
}
void ndspSetOutputMode(ndspOutputMode mode) {channel(0);require(mode==NDSP_OUTPUT_STEREO,"Wrong native stereo mode");}
void ndspChnReset(int id) {channel(id);mock.events.push_back("channel-reset");}
void ndspChnSetFormat(int id,u16 format) {channel(id);require(format==NDSP_FORMAT_STEREO_PCM16,"Wrong native sample format");}
void ndspChnSetRate(int id,float rate) {channel(id);require(rate==32000.F,"Source SPC rate changed");}
void ndspChnSetInterp(int id,ndspInterpType type) {channel(id);require(type==NDSP_INTERP_LINEAR,"Wrong native interpolation");}
void ndspChnSetMix(int id,float mix[12]) {
    channel(id);for(unsigned i=0;i<12;++i) require(mix[i]==(i<2?1.F:0.F),"Wrong native stereo/auxiliary mix");
}
void ndspChnSetPaused(int id,bool paused) {channel(id);mock.live_pcm_unchanged();mock.paused=paused;mock.events.push_back(paused?"pause":"resume");}
void ndspChnWaveBufClear(int id) {
    channel(id);mock.live_pcm_unchanged();mock.events.push_back("clear");
    // Keep descriptors/data reachable until exit to detect premature reuse.
}
void ndspChnWaveBufAdd(int id,ndspWaveBuf* wave) {
    channel(id);mock.live_pcm_unchanged();require(wave!=nullptr,"Null native wave descriptor");
    const auto slot=mock.slot(wave->data_pcm16);
    require(wave->nsamples==1600 && !wave->looping && wave->adpcm_data==nullptr && !wave->offset,
        "Native PCM used sample/byte count or looping/ADPCM metadata");
    require(wave->status==NDSP_WBUF_FREE || wave->status==NDSP_WBUF_DONE,"Active native descriptor resubmitted");
    require(mock.last_flushed==wave->data_pcm16,"NDSP submission preceded exact cache flush");
    mock.waves[slot]=wave;mock.submitted[slot].assign(wave->data_pcm16,wave->data_pcm16+AudioPcm::samples);
    mock.order.push_back(slot);++mock.adds;mock.events.push_back("add");wave->status=NDSP_WBUF_QUEUED;
    mock.last_flushed=nullptr;
}
Result DSP_FlushDataCache(const void* pointer,u32 bytes) {
    channel(0);mock.live_pcm_unchanged();static_cast<void>(mock.slot(pointer));
    require(bytes==AudioPcm::bytes,"NDSP cache flush omitted stereo samples/wrong units");
    ++mock.flushes;mock.events.push_back("flush");
    mock.last_flushed=mock.flush_result<0?nullptr:pointer;return mock.flush_result;
}
}
int main() {
    try {
        queue_and_pause();handoff_and_failures();
        std::cout<<"Actual NDSP adapter queue/lifecycle/error contracts: "<<checks
            <<" checks passed (fake libctru; not physical DSP/audio/FPS acceptance)\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"Native NDSP adapter regression: "<<error.what()<<'\n';return 1;}
}
