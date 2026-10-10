#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace starfox::render {
// A source frame owns one starting endpoint, shared by every interpolated
// presentation/eye. Only its first presentation may advance the carried state.
class GridLineHistory {
public:
    struct State {
        bool initialized{},committed{};
        std::uint64_t number{};
        std::array<std::int16_t,2> previous{},start{};
        bool operator==(const State&) const=default;
    };
    [[nodiscard]] State state() const noexcept {return {initialized_,committed_,number_,previous_,start_};}
    void restore(const State& state) {
        if((!state.initialized && (state.committed || state.number || state.previous!=std::array<std::int16_t,2>{}
            || state.start!=std::array<std::int16_t,2>{}))
            || (state.initialized && !state.committed && state.previous!=state.start))
            throw std::invalid_argument("Invalid carried source grid state");
        initialized_=state.initialized;committed_=state.committed;number_=state.number;
        previous_=state.previous;start_=state.start;
    }
    struct Frame {
        std::uint64_t number{};
        std::array<std::int16_t,2> start{};
        bool advances{};
    };
    Frame begin(std::uint64_t number) noexcept {
        const bool fresh=!initialized_ || number_!=number;
        if(fresh) {
            initialized_=true;number_=number;start_=previous_;committed_=false;
        }
        return {number,start_,fresh};
    }
    void finish(const Frame& frame,std::array<std::int16_t,2> endpoint) noexcept {
        if(initialized_ && frame.number==number_ && frame.advances && !committed_) {
            previous_=endpoint;committed_=true;
        }
    }
    void reset() noexcept { *this=GridLineHistory{}; }
private:
    bool initialized_{},committed_{};
    std::uint64_t number_{};
    std::array<std::int16_t,2> previous_{},start_{};
};
}
