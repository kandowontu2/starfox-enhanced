#include "reflection_source_proof_receipts.hpp"
#include <array>
#include <iostream>
#include <limits>

namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F&& action,const char* message) {
    bool rejected=false;try {action();}catch(const std::runtime_error&){rejected=true;}require(rejected,message);
}
std::vector<unsigned char> transfer;
unsigned created{},copies{},mapped{},released{},begun{},ended{};bool completed{};
std::array<unsigned char,64*48> queries;
std::vector<unsigned char> proofs(64*24576);
SDL_GPUTransferBuffer* SDLCALL create(SDL_GPUDevice*,const SDL_GPUTransferBufferCreateInfo* info) {
    require(info->usage==SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD && info->size<=16*1024*1024,
        "Diagnostic requested an upload or exceeded its bound");
    ++created;transfer.assign(info->size,0);return reinterpret_cast<SDL_GPUTransferBuffer*>(1);
}
SDL_GPUCopyPass* SDLCALL begin(SDL_GPUCommandBuffer*) {++begun;return reinterpret_cast<SDL_GPUCopyPass*>(1);}
void SDLCALL download(SDL_GPUCopyPass*,const SDL_GPUBufferRegion* source,const SDL_GPUTransferBufferLocation* dest) {
    const auto& input=source->buffer==reinterpret_cast<SDL_GPUBuffer*>(2)?queries.data():proofs.data();
    const auto limit=source->buffer==reinterpret_cast<SDL_GPUBuffer*>(2)?queries.size():proofs.size();
    require(source->offset<=limit && source->size<=limit-source->offset
        && dest->offset<=transfer.size() && source->size<=transfer.size()-dest->offset,"Receipt copy out of bounds");
    std::memcpy(transfer.data()+dest->offset,input+source->offset,source->size);++copies;
}
void SDLCALL end(SDL_GPUCopyPass*) {++ended;}
void* SDLCALL map(SDL_GPUDevice*,SDL_GPUTransferBuffer*,bool cycle) {
    require(completed && !cycle,"Read before exact completed fence or cycled pending download");++mapped;return transfer.data();
}
void SDLCALL unmap(SDL_GPUDevice*,SDL_GPUTransferBuffer*) {}
void SDLCALL release(SDL_GPUDevice*,SDL_GPUTransferBuffer*) {++released;}
}

int main() try {
    using namespace starfox::render;
    for(unsigned total=0;total<30000;++total) {
        const auto layout=ReflectionSourceProofLayout::make(total,48,24576,576);
        require(bool(layout)==(total && std::uint64_t(total)*624<=16*1024*1024),"Receipt limit/off-by-one mismatch");
        if(layout)require(layout->queries_bytes==total*48 && layout->bytes==total*624,"Receipt planes overlap");
    }
    require(!ReflectionSourceProofLayout::make(std::numeric_limits<std::uint64_t>::max(),48,24576,576),"Huge stream admitted");
    require(!ReflectionSourceProofLayout::make(64,47,24576,576)
        && !ReflectionSourceProofLayout::make(64,48,24575,576)
        && !ReflectionSourceProofLayout::make(64,48,24576,575),"Foreign packet ABI admitted");
    ReflectionSourceProofReceipts::Api api{create,begin,download,end,map,unmap,release};
    ReflectionSourceProofReceipts receipt(api);
    ReflectionSourceStageEvent event{reinterpret_cast<void*>(1),reinterpret_cast<void*>(1),0,153,64,0,0,
        ReflectionSourceStage::composition,reinterpret_cast<void*>(2),reinterpret_cast<void*>(3),48,24576,576};
    receipt.observe(event);require(!created && !copies && !mapped,"Disabled receipt performed GPU work");
    receipt.selected=true;
    auto wrong=event;wrong.eye=1;receipt.observe(wrong);wrong=event;wrong.sample=1;receipt.observe(wrong);
    wrong=event;wrong.stage=ReflectionSourceStage::colour;receipt.observe(wrong);
    require(!created,"Foreign eye/sample/stage allocated a receipt");
    wrong=event;wrong.first=1;rejects([&]{receipt.observe(wrong);},"Missing first batch accepted");
    for(unsigned first=0;first<153;first+=64) {
        event.first=first;event.count=std::min(64U,153-first);
        for(unsigned i=0;i<event.count;++i) {
            std::fill_n(queries.data()+i*48,48,(first+i)%251);
            std::fill_n(proofs.data()+i*24576,576,(first+i+31)%251);
        }
        receipt.observe(event);
        rejects([&]{receipt.retired(false);},"Receipt read on observation timeout");
        if(first==0)rejects([&]{receipt.retired(true);},"Partial stream read as complete");
    }
    require(created==1 && copies==156 && begun==3 && ended==3 && !mapped && !released,"Stream coverage/lifetime wrong");
    rejects([&]{receipt.observe(event);},"Repeated last batch overwrote a receipt");
    completed=true;const auto words=receipt.retired(true);
    const auto* bytes=reinterpret_cast<const unsigned char*>(words.data());
    for(unsigned r=0;r<153;++r) {
        for(unsigned b=0;b<48;++b)require(bytes[r*48+b]==r%251,"Query omitted or scratch overwritten");
        for(unsigned b=0;b<576;++b)require(bytes[153*48+r*576+b]==(r+31)%251,"Proof omitted or scratch overwritten");
    }
    require(mapped==1 && released==1,"Exact retirement did not release once");
    event.first=0;event.count=64;completed=false;receipt.observe(event);receipt.release();
    require(mapped==1 && released==2,"Aborted receipt read unfinished work or leaked");
    std::cout<<"PASS: complete bounded source-proof receipts; all scratch prefixes copied, no upload/read before exact retirement; disabled, partial, repeated, ABI and abort guards. Mock transport, not native optics.\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
