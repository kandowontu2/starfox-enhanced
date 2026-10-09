// Link the actual native adapter to explicit fake libctru headers/functions.
// This proves owner/error/DMA rules, NOT firmware, IPC or physical output.
#include "../platform/3ds/native_csnd_check.hpp"
#include "../platform/3ds/native_audio_owner.hpp"
#include "starfox/platform/nintendo_3ds/csnd_check_pcm.hpp"
extern "C" {
#include <3ds/services/csnd.h>
#include <3ds/os.h>
#include <3ds/svc.h>
#include <3ds/allocator/linear.h>
}
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace starfox::platform::nintendo_3ds;
unsigned checks{};
void require(bool ok,const char* message) {++checks;if(!ok) throw std::runtime_error(message);}
template<class F> void rejects(F callback,const char* message) {
    bool failed=false;try {callback();} catch(const std::exception&) {failed=true;}
    require(failed,message);
}
struct Command {u32 flags,first,second,bytes,volume,capture;};
struct Mock {
    Result init_result{},flush_result{},exec_result{};
    u32 grant=BIT(8)|BIT(9);
    bool allocation_failure{},never_ack{},null_info{},ready{},pending{};
    unsigned inits{},exits{},frees{},submissions{},sleeps{},ack_delay{};
    u64 ticks{};
    void* storage{};
    std::vector<std::string> events;
    std::vector<Command> starts;
    std::vector<unsigned> stops;
    std::array<CSND_ChnInfo,32> info{};
    alignas(u32) std::array<u8,32> first_command{};
    void complete() {
        for(const auto& command:starts) info[command.flags&31].active=1;
        for(const auto channel:stops) info[channel].active=0;
        first_command[4]=1;pending=false;
    }
    void reset() {
        require(!ready && !storage,"Previous mock service/DMA owner leaked");
        *this=Mock{};
    }
} mock;
void channels_and_pcm() {
    for(unsigned a=0;a<32;++a) {
        require(!csnd_stereo_channels(std::uint32_t{1}<<a),"Single channel accepted for stereo");
        for(unsigned b=a+1;b<32;++b) {
            const auto pair=csnd_stereo_channels((std::uint32_t{1}<<a)|(std::uint32_t{1}<<b));
            require(pair && (*pair)[0]==a && (*pair)[1]==b,"Granted channel pair changed/undefined shift");
        }
    }
    require(!csnd_stereo_channels(0),"Zero granted channels accepted");
    std::uint32_t bits=0x38756291;
    for(unsigned i=0;i<65536;++i) {
        bits=bits*1664525+1013904223;
        const auto pair=csnd_stereo_channels(bits);
        require(pair && (*pair)[0]<(*pair)[1] && (bits&(std::uint32_t{1}<<(*pair)[0]))
            && (bits&(std::uint32_t{1}<<(*pair)[1])),"Selected ungranted/duplicate stereo channel");
    }
    std::vector<std::int16_t> pcm(CsndCheckPcm::frames*2,123);
    const auto left=std::span(pcm).first(CsndCheckPcm::frames);
    const auto right=std::span(pcm).last(CsndCheckPcm::frames);
    rejects([&]{fill_csnd_check_pcm(left,left);},"Overlapping mono planes accepted");
    rejects([&]{fill_csnd_check_pcm(left,right.first(31999));},"Truncated plane accepted");
    require(std::all_of(pcm.begin(),pcm.end(),[](auto value){return value==123;}),"Invalid PCM mutated destination");
    fill_csnd_check_pcm(left,right);
    require(CsndCheckPcm::storage_bytes==128000,"CSND test allocation units changed");
    for(unsigned frame=0;frame<CsndCheckPcm::frames;++frame) {
        const bool first=frame<16000;
        const auto sample=static_cast<std::int16_t>(4096*std::sin(6.283185307179586*(first?220:440)*frame/32000));
        require(left[frame]==(first?sample:0) && right[frame]==(first?0:sample),"Left/right/quiet test signal changed");
    }
}
void native_success() {
    mock.reset();mock.grant=BIT(11)|BIT(17)|BIT(30);
    {
        NativeCsndCheck output;
        require(output.finished(),"Unstarted check reported active DMA");
        rejects([]{NativeCsndCheck second;},"Second service owner acquired active output");
        rejects([]{detail::AudioOutputLease ndsp;},"NDSP lease allowed while CSND owns process output");
        require(mock.inits==1,"Duplicate lease touched native service");
        output.start();
        require(mock.starts.size()==2 && mock.submissions==1,"Stereo start not one atomic command batch");
        const auto& a=mock.starts[0];const auto& b=mock.starts[1];
        require((a.flags&31)==11 && (b.flags&31)==17,"Native start ignored granted channels");
        require(a.bytes==64000 && b.bytes==64000 && a.second==0 && b.second==0,"Native start wrong size/looping buffer");
        require(a.volume==0x8000 && b.volume==0x80000000U && a.capture==a.volume && b.capture==b.volume,
            "Planar stereo panning changed");
        const auto flags=SOUND_ENABLE|SOUND_ONE_SHOT|SOUND_FORMAT_16BIT|SOUND_LINEAR_INTERP|(2094U<<16);
        require((a.flags&~31U)==flags && (b.flags&~31U)==flags,"PCM16/one-shot/timer flags wrong");
        require(mock.events[1]=="allocate" && mock.events[2]=="flush" && mock.events[3]=="commands",
            "Native PCM was submitted before allocation/cache flush");
        const auto* data=static_cast<const std::int16_t*>(mock.storage);
        const std::vector<std::int16_t> frozen(data,data+64000);
        require(!output.finished(),"Playing native channel reported done");
        rejects([&]{output.start();},"Playing one-shot DMA overwritten by restart");
        require(std::equal(frozen.begin(),frozen.end(),data),"Active DMA data changed on rejected restart");
        mock.info[11].active=0;
        require(!output.finished(),"One side still active but stereo pair marked reusable");
        mock.info[17].active=0;
        require(output.finished(),"Inactive stereo pair never completed");
        output.start();
        require(mock.info[11].active && mock.info[17].active,"Completed one-shot could not be repeated safely");
    }
    require(mock.exits==1 && mock.frees==1,"Service/storage not retired exactly once");
    require(mock.stops==std::vector<unsigned>{11,17},"Teardown did not stop both acquired channels");
    require(mock.events[mock.events.size()-2]=="exit" && mock.events.back()=="free","DMA freed before service exit");
    {detail::AudioOutputLease ndsp;rejects([]{NativeCsndCheck output;},"CSND allowed while NDSP owns service lease");}
    mock.reset();mock.ack_delay=3;
    {NativeCsndCheck output;output.start();require(mock.sleeps==3,"Delayed acknowledgment did not yield/bound correctly");}
    mock.reset();mock.ack_delay=3;mock.ticks=std::numeric_limits<u64>::max()-SYSCLOCK_ARM11/1000;
    {NativeCsndCheck output;output.start();require(mock.sleeps==3,"Wrapping system clock broke bounded acknowledgment");}
}
void native_faults() {
    for(Result failure:{Result(-13),Result(1)}) {
        mock.reset();mock.init_result=failure;
        rejects([]{NativeCsndCheck output;},"Nonzero service initialization accepted");
        require(mock.exits==0 && mock.frees==0,"Failed init released unowned service/storage");
        mock.reset();mock.flush_result=failure;
        {NativeCsndCheck output;rejects([&]{output.start();},"Nonzero cache flush accepted");
            require(mock.starts.empty(),"Failed flush began DMA");}
        require(mock.exits==1 && mock.frees==1,"Flush failure leaked owner");
        mock.reset();mock.exec_result=failure;
        {NativeCsndCheck output;rejects([&]{output.start();},"Nonzero command submission accepted");
            const auto submissions=mock.submissions;
            rejects([&]{output.start();},"Failed command list reused without service retirement");
            require(mock.exits==1 && mock.submissions==submissions,"Poisoned owner submitted another command list");}
        require(mock.exits==1 && mock.frees==1,"Partial submit freed storage without retiring service");
    }
    for(u32 mask:{0U,BIT(9)}) {
        mock.reset();mock.grant=mask;
        rejects([]{NativeCsndCheck output;},"Insufficient granted channels accepted");
        require(mock.exits==1 && !mock.storage,"Missing channel pair leaked initialized service");
    }
    mock.reset();mock.allocation_failure=true;
    rejects([]{NativeCsndCheck output;},"Failed linear allocation accepted");
    require(mock.exits==1 && mock.frees==0,"Allocation failure leaked service/freed nonexistent memory");
    mock.reset();mock.never_ack=true;
    {NativeCsndCheck output;rejects([&]{output.start();},"Missing acknowledgment hung/accepted");
        require(mock.sleeps>=100 && mock.sleeps<=102,"Command wait not bounded to 100ms with yielding");
        const auto submissions=mock.submissions;
        rejects([&]{static_cast<void>(output.finished());},"Timed-out owner still usable");
        require(mock.exits==1 && mock.submissions==submissions,"Timed-out list overwritten before service exit");}
    require(mock.exits==1 && mock.frees==1,"Timeout did not retire service before freeing DMA");
    mock.reset();
    {NativeCsndCheck output;output.start();mock.null_info=true;
        rejects([&]{static_cast<void>(output.finished());},"Missing native channel status accepted");}
    require(mock.exits==1 && mock.frees==1,"Channel status failure leaked output owner");
    mock.reset(); // Prove lease is also released after every failure path.
    {NativeCsndCheck output;output.start();}
}
} // namespace
extern "C" {
u32 csndChannels{};
Result csndInit() {++mock.inits;mock.events.push_back("init");if(mock.init_result) return mock.init_result;
    mock.ready=true;csndChannels=mock.grant;return 0;}
void csndExit() {++mock.exits;mock.events.push_back("exit");mock.ready=false;mock.pending=false;
    for(auto& channel:mock.info) channel.active=0;}
void* linearAlloc(size_t bytes) {require(bytes==128000,"Wrong native linear allocation size");
    mock.events.push_back("allocate");if(mock.allocation_failure) return nullptr;
    mock.storage=std::malloc(bytes);return mock.storage;}
void linearFree(void* pointer) {require(!mock.ready && pointer==mock.storage,"Linear storage freed with live service");
    ++mock.frees;mock.events.push_back("free");std::free(pointer);mock.storage=nullptr;}
u32 osConvertVirtToPhys(const void* pointer) {return static_cast<u32>(reinterpret_cast<std::uintptr_t>(pointer));}
u64 svcGetSystemTick() {return mock.ticks;}
Result svcSleepThread(int64_t ns) {require(ns==1'000'000,"CSND wait stopped yielding at 1ms");
    ++mock.sleeps;mock.ticks+=SYSCLOCK_ARM11/1000;
    if(mock.pending && !mock.never_ack && mock.sleeps>=mock.ack_delay) mock.complete();
    return 0;}
u32* csndAddCmd(int id) {require(id==0x300 && mock.ready,"Wrong native acknowledgment marker/uninitialized service");
    mock.first_command.fill(0);mock.starts.clear();mock.stops.clear();mock.events.push_back("commands");
    return reinterpret_cast<u32*>(mock.first_command.data()+8);}
Result csndExecCmds(bool waitDone) {require(!waitDone,"Native adapter used unbounded libctru busy-wait");++mock.submissions;
    if(mock.exec_result) return mock.exec_result;
    mock.pending=true;if(!mock.never_ack && !mock.ack_delay) mock.complete();return 0;}
void CSND_SetPlayStateR(u32 channel,u32 value) {require(value==0 && (mock.grant&BIT(channel)),"Stopped ungranted channel");
    mock.stops.push_back(channel);}
void CSND_SetChnRegs(u32 flags,u32 first,u32 second,u32 bytes,u32 volume,u32 capture) {
    require((mock.grant&BIT(flags&31))!=0,"Started ungranted channel");
    mock.starts.push_back({flags,first,second,bytes,volume,capture});}
Result CSND_FlushDataCache(const void* pointer,u32 bytes) {
    require(pointer==mock.storage && bytes==128000,"Incomplete/incorrect native DMA cache flush");
    mock.events.push_back("flush");return mock.flush_result;}
CSND_ChnInfo* csndGetChnInfo(u32 channel) {return mock.null_info?nullptr:&mock.info[channel];}
}
int main() {
    try {
        channels_and_pcm();native_success();native_faults();
        std::cout<<"CSND check PCM/service/ownership/fault contracts: "<<checks
            <<" passed (fake libctru, not physical audio/streaming acceptance)\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"CSND check regression: "<<error.what()<<'\n';return 1;}
}
