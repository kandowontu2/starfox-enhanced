#include "starfox/platform/nintendo_3ds/audio_pcm.hpp"
#include <iostream>
#include <limits>

namespace {
using namespace starfox::platform::nintendo_3ds;
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F f,const char* message) {
    bool rejected=false;try {f();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,message);
}
void buffer_selection() {
    std::array<AudioBufferState,AudioPcm::blocks> states{};
    for(unsigned cursor=0;cursor<AudioPcm::blocks;++cursor) {
        states.fill(AudioBufferState::queued);
        require(!next_audio_buffer(states,cursor),"Full queue must not overwrite active DMA");
        states.fill(AudioBufferState::playing);
        require(!next_audio_buffer(states,cursor),"Playing queue must not be reused");
        states.fill(static_cast<AudioBufferState>(255));
        require(!next_audio_buffer(states,cursor),"Unknown hardware status must not be reused");
        for(auto writable:{AudioBufferState::free,AudioBufferState::done}) {
            for(unsigned slot=0;slot<AudioPcm::blocks;++slot) {
                states.fill(AudioBufferState::queued);states[slot]=writable;
                require(next_audio_buffer(states,cursor)==slot,"Available buffer must be found across wraparound");
            }
        }
        states.fill(AudioBufferState::free);
        require(next_audio_buffer(states,cursor)==cursor,"Empty queue must preserve rotating slot order");
        for(unsigned i=0;i<AudioPcm::blocks;++i) {
            const auto slot=next_audio_buffer(states,cursor);
            require(slot && *slot==(cursor+i)%AudioPcm::blocks,"Queued blocks changed slot order");
            states[*slot]=AudioBufferState::queued;
        }
        require(!next_audio_buffer(states,cursor),"Eight-block limit must be exact");
        // A done block can be reused without changing any other active slot.
        const auto before=states;const auto done=(cursor+5)%AudioPcm::blocks;
        states[done]=AudioBufferState::done;
        require(next_audio_buffer(states,cursor)==done,"Completed block was not reused");
        for(unsigned i=0;i<AudioPcm::blocks;++i) if(i!=done)
            require(states[i]==before[i],"Selection changed another DSP descriptor");
    }
    rejects([&]{static_cast<void>(next_audio_buffer(states,AudioPcm::blocks));},"Out-of-range audio cursor accepted");
}
void pcm_copy() {
    require(AudioPcm::rate==32000 && AudioPcm::frames==1600 && AudioPcm::channels==2,
        "Native source audio units changed");
    require(AudioPcm::samples==3200 && AudioPcm::bytes==6400 && AudioPcm::storage_bytes==51200,
        "DSP memory or stereo frame count wrong");
    std::array<std::int16_t,AudioPcm::samples> source{},destination{};
    for(unsigned frame=0;frame<AudioPcm::frames;++frame) {
        source[frame*2]=static_cast<std::int16_t>(frame+1);
        source[frame*2+1]=static_cast<std::int16_t>(-int(frame)-1);
    }
    source[0]=std::numeric_limits<std::int16_t>::min();source[1]=std::numeric_limits<std::int16_t>::max();
    const auto original=source;copy_audio_pcm(source,destination);
    require(source==original,"PCM output mutated the borrowed SPC block");
    for(unsigned i=0;i<source.size();++i) require(destination[i]==source[i],"PCM copy changed channel order/value");
    source.fill(0);require(destination==original,"DSP storage borrowed reused source samples");
    const auto unchanged=destination;
    rejects([&]{copy_audio_pcm({},destination);},"Empty PCM accepted");
    rejects([&]{copy_audio_pcm(std::span(source).first(3199),destination);},"Truncated/odd stereo block accepted");
    rejects([&]{copy_audio_pcm(source,std::span(destination).first(1600));},"Frame count used as halfword count");
    rejects([&]{copy_audio_pcm(source,source);},"DMA/source alias accepted");
    std::array<std::int16_t,AudioPcm::samples+1> overlap{};
    rejects([&]{copy_audio_pcm(std::span(overlap).first(AudioPcm::samples),std::span(overlap).last(AudioPcm::samples));},
        "Forward overlapping DMA storage accepted");
    rejects([&]{copy_audio_pcm(std::span(overlap).last(AudioPcm::samples),std::span(overlap).first(AudioPcm::samples));},
        "Backward overlapping DMA storage accepted");
    require(destination==unchanged,"Invalid PCM changed destination storage");
    // Adjacent regions, as used by the contiguous native linear pool, are safe.
    std::array<std::int16_t,AudioPcm::samples*2> adjacent{};
    std::copy(original.begin(),original.end(),adjacent.begin());
    copy_audio_pcm(std::span(adjacent).first(AudioPcm::samples),std::span(adjacent).last(AudioPcm::samples));
    require(std::equal(original.begin(),original.end(),adjacent.begin()+AudioPcm::samples),"Adjacent buffers rejected/changed");
}
} // namespace
int main() {
    try {
        buffer_selection();pcm_copy();
        std::cout<<"3DS audio PCM/ownership contract: "<<checks<<" checks passed (not NDSP hardware acceptance)\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"3DS audio regression: "<<error.what()<<'\n';return 1;}
}
