#include "starfox/platform/nintendo_3ds/startup_trace.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
std::uint64_t tick{};
std::uint64_t clock_tick() noexcept { return ++tick; }
void require(bool ok, const char* message) {
    if(!ok) throw std::runtime_error(message);
}
}
int main() try {
    using namespace starfox::platform::nintendo_3ds;
    std::ostringstream output;
    StartupTrace trace(output,clock_tick);
    {
        ScopedStartupTraceActivation activation(trace);
        STARFOX_3DS_STARTUP_MARK(boot);
        require(active_startup_trace == &trace,"Trace owner missing");
    }
    require(!active_startup_trace,"Trace activation leaked");
#if defined(STARFOX_3DS_PROFILE_FRAMES)
    require(output.str() == "frame,tick,phase\n0,1,boot\n","Boot marker not immediately available");
#else
    require(output.str().empty() && tick == 0,"Disabled marker performed I/O or clock work");
#endif
    std::ostringstream nested;
    StartupTrace second(nested,clock_tick);
    {
        ScopedStartupTraceActivation outer(trace);
        { ScopedStartupTraceActivation inner(second); require(active_startup_trace == &second,"Inner owner missing"); }
        require(active_startup_trace == &trace,"Previous trace owner not restored");
    }
    std::ostringstream rows;
    StartupTrace bounded(rows,clock_tick);
    for(unsigned i=0;i<StartupTrace::maximum_rows+10;++i) bounded.write("step");
    const auto text=rows.str();
    require(std::count(text.begin(),text.end(),'\n') == StartupTrace::maximum_rows+1,"Row bound lost");
    std::ostringstream frames;
    StartupTrace frame_bound(frames,clock_tick);
    for(unsigned i=0;i<StartupTrace::maximum_frames;++i) {frame_bound.begin_frame();frame_bound.write("poll");}
    const auto before=frames.str();const auto clock_before=tick;
    for(unsigned i=0;i<1000;++i) {frame_bound.begin_frame();frame_bound.write("later");}
    require(frames.str() == before && tick == clock_before,"Steady gameplay retained diagnostic I/O");
    std::ostringstream failed;
    failed.setstate(std::ios::badbit);
    try {failed.exceptions(std::ios::badbit);} catch(const std::ios_base::failure&) {}
    StartupTrace unavailable(failed,clock_tick);
    unavailable.write("boot");require(unavailable.stopped(),"Failed SD stream remained active");
    const auto stopped_clock=tick;unavailable.write("retry");
    require(tick == stopped_clock,"Failed SD stream retried I/O");
    std::ostringstream no_clock;
    StartupTrace disabled(no_clock,nullptr);disabled.write("boot");
    require(no_clock.str().empty(),"Disabled clock emitted data");
    std::cout << "Bounded startup trace: entries, nesting, failure and zero-work disabled path passed\n";
    return 0;
} catch(const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
