#pragma once
#include <array>
#include <cstdint>

namespace starfox::platform::nintendo_3ds {
enum class PicaDisplayPacing { lcd, source_clock };
// C3D_FrameSync waits for BOTH LCD counters to advance from the instant it is
// called. Waiting unconditionally after slow frame preparation adds another
// refresh interval even though the previous pacing interval already elapsed.
// Remember the last synchronization instead. This does not retire the GPU
// queue: the ordinary blocking C3D_FrameBegin(0) must still follow this call.
// Gameplay already consumes completed source phases through PresentationClock.
// An LCD wait there adds a second, unrelated deadline. Editors/probes without
// a source clock retain LCD pacing; changing ownership discards its old lease.
class PicaDisplaySync {
public:
    template<class Read,class Wait> void synchronize(Read read,Wait wait,
            PicaDisplayPacing pacing=PicaDisplayPacing::lcd) {
        if(pacing==PicaDisplayPacing::source_clock) {armed_=false;return;}
        const auto current=read();
        if(!armed_ || current[0]==previous_[0] || current[1]==previous_[1]) wait();
        previous_=read();
        armed_=true;
    }
private:
    std::array<std::uint32_t,2> previous_{};
    bool armed_{};
};
} // namespace starfox::platform::nintendo_3ds
