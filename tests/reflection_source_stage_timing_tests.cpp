#include "reflection_source_stage_timings.hpp"
#include <memory>
#include <sstream>

namespace {
using namespace starfox::render;
bool fence_retired{};unsigned created{},read_count{},destroyed{},written{},resolved{};
struct FakeContext {std::vector<std::uint64_t> ticks;std::vector<bool> writes,resolves;};
std::vector<std::unique_ptr<FakeContext>> retained;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void* create(void*,std::uint32_t count,std::uint64_t* frequency) {
    require(count && count<=4096,"Query heap exceeded the real bridge bound");++created;
    auto context=std::make_unique<FakeContext>();context->ticks.resize(count);context->writes.resize(count);context->resolves.resize(count);
    *frequency=1'000'000;auto* pointer=context.get();retained.push_back(std::move(context));return pointer;
}
bool write(void*,void* pointer,std::uint32_t index) {
    auto& context=*static_cast<FakeContext*>(pointer);
    require(index<context.ticks.size() && !context.writes[index],"Timestamp slot reused while pending");
    context.writes[index]=true;context.ticks[index]=100+index*100; ++written;return true;
}
bool resolve(void*,void* pointer,std::uint32_t first,std::uint32_t count) {
    auto& context=*static_cast<FakeContext*>(pointer);
    require(count && first+count<=context.ticks.size(),"Timestamp resolve exceeded its chunk");
    for(unsigned i=first;i<first+count;++i) {
        require(context.writes[i] && !context.resolves[i],"Unwritten/repeated timestamp resolve");context.resolves[i]=true;
    }
    ++resolved;return true;
}
bool read(void* pointer,std::uint32_t first,std::uint32_t count,std::uint64_t* ticks) {
    require(fence_retired,"Timestamp read before exact submission retirement");++read_count;
    const auto& context=*static_cast<FakeContext*>(pointer);
    for(unsigned i=0;i<count;++i) {require(context.resolves[first+i],"Reading unresolved query slot");ticks[i]=context.ticks[first+i];}
    return true;
}
void destroy(void*) {require(fence_retired,"Destroyed query heap while pending");++destroyed;}
const StarfoxSdlD3D12TimestampsV1 bridge{1,create,write,resolve,read,destroy};
const StarfoxSdlD3D12TimestampsV1* lookup(void*) {return &bridge;}
template<class F> void rejects(F&& action,const char* message) {
    bool rejected=false;try {action();}catch(const std::exception&){rejected=true;}require(rejected,message);
}
}
int main() try {
    int device{},command{};
    ReflectionSourceStageTimings disabled(false,lookup);ReflectionSourceStageTimings::observe(&disabled,{});
    require(!created && !written && !read_count && !destroyed,"Disabled diagnostics allocated/encoded/read GPU work");
    ReflectionSourceStageTimings timing(true,lookup);
    ReflectionSourceStageEvent event{&device,&command,0,128*72*8,64,0,0,ReflectionSourceStage::begin};
    auto bad=event;bad.stage=ReflectionSourceStage::queries;
    rejects([&]{ReflectionSourceStageTimings::observe(&timing,bad);},"Missing initial batch marker accepted");
    require(!created,"Malformed initial stream allocated timestamps");
    for(unsigned eye=0;eye<2;++eye)for(std::uint64_t first=0;first<event.total;first+=64) {
        event.eye=eye;event.first=first;
        for(unsigned stage=0;stage<10;++stage) {
            event.stage=static_cast<ReflectionSourceStage>(stage);
            ReflectionSourceStageTimings::observe(&timing,event);
        }
    }
    require(created==6 && written==23040 && resolved==2304 && !read_count && !destroyed,
        "Complete native two-eye/eight-lobe stream was sampled, read or retired prematurely");
    rejects([&]{ReflectionSourceStageTimings::observe(&timing,event);},"Pending final timestamp slot reused");
    fence_retired=true;std::ostringstream output;timing.retired(output,"mock-exact-fence");
    require(!timing.active() && read_count==6 && destroyed==6,"Completed chunks did not retire exactly once");
    require(output.str().find("records=73728 batches=1152/1152")!=std::string::npos
        && output.str().find("roots-gpu-ms=115.2")!=std::string::npos,"Exclusive all-batch stage totals or coverage wrong");
    // Reuse only AFTER retirement; an unfinished/device-lost command must not
    // read unwritten/unavailable GPU timing data even when cleanup is safe.
    fence_retired=false;event.eye=0;event.first=0;event.total=153;event.count=64;event.stage=ReflectionSourceStage::begin;
    ReflectionSourceStageTimings::observe(&timing,event);
    require(created==7 && destroyed==6,"Fresh frame did not receive a distinct query context");
    fence_retired=true;timing.retired(output,"mock-device-lost",false);
    require(read_count==6 && destroyed==7 && !timing.active(),"Device loss read unfinished GPU queries or leaked retired storage");
    require(output.str().find("timing-unavailable-or-incomplete")!=std::string::npos,"Unavailable timings reported as valid work");
    retained.clear();
    std::cout<<"PASS: opt-in all-stage/all-batch two-eye eight-lobe timestamp chunks; no reads/reuse/destruction before retirement; disabled/incomplete/device-loss guards. Mock bridge, not native GPU timing acceptance.\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
