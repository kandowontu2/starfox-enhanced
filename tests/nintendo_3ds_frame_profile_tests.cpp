#include "starfox/platform/nintendo_3ds/frame_profile.hpp"
#include <limits>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace {
std::uint64_t tick{};
unsigned clock_calls{};
std::uint64_t clock_tick() noexcept { ++clock_calls; return tick; }
void require(bool condition, const char* message) {
    if(!condition) throw std::runtime_error(message);
}
struct CountingStreamBuffer : std::stringbuf {
    unsigned flushes{};
    int sync() override { ++flushes; return std::stringbuf::sync(); }
};
}
int main() try {
    using namespace starfox::platform::nintendo_3ds;
    FrameProfile profile(clock_tick, 1000);
    {
        ScopedFrameProfileActivation active(profile);
        { ScopedFramePhase parent(active_frame_profile, FramePhase::advance);
          tick = 20;
          { ScopedFramePhase child(active_frame_profile, FramePhase::audio); tick = 70; }
          tick = 90; }
        require(profile.totals()[unsigned(FramePhase::advance)] == FramePhaseTotals{1,90,90}, "Parent phase changed");
        require(profile.totals()[unsigned(FramePhase::audio)] == FramePhaseTotals{1,50,50}, "Child phase changed");
        FrameProfile nested(clock_tick, 1000);
        { ScopedFrameProfileActivation activation(nested); require(active_frame_profile == &nested, "Nested activation failed"); }
        require(active_frame_profile == &profile, "Nested activation lost previous owner");
        try { ScopedFramePhase failed(&profile, FramePhase::models); tick = 120; throw std::runtime_error("fixture"); }
        catch(const std::runtime_error&) {}
        require(profile.totals()[unsigned(FramePhase::models)] == FramePhaseTotals{1,30,30}, "Unwinding lost timing");
    }
    require(!active_frame_profile, "Profiler outlived owner");
    static_assert(unsigned(FramePhase::present) == 13);
    static_assert(unsigned(FramePhase::bg_colour) == 24);
    static_assert(unsigned(FramePhase::gpu_sync) == 25);
    static_assert(unsigned(FramePhase::terrain_build) == 30);
    static_assert(unsigned(FramePhase::mono_canvas) == 40);
    static_assert(unsigned(FramePhase::object_export) == 41);
    static_assert(unsigned(FramePhase::object_import) == 42);
    static_assert(frame_phase_names.size() == unsigned(FramePhase::count));
    for(unsigned i = 0; i < frame_phase_names.size(); ++i) {
        require(!frame_phase_names[i].empty(), "Empty phase name");
        for(unsigned j = 0; j < i; ++j)
            require(frame_phase_names[i] != frame_phase_names[j], "Duplicate phase name");
    }
    profile.record(FramePhase::audio, 200, 210);
    std::ostringstream stream;
    require(!profile.write_window(stream, 999, 9, 4, 30, 10, 10) && stream.str().empty(), "Sub-second diagnostic wrote data");
    require(profile.write_window(stream, 1000, 9, 4, 30, 10, 10, 17), "Due diagnostic failed");
    require(stream.str() ==
        "tick,clock_hz,flow,bg,video_phases,logic_ticks,audio_blocks,phase,calls,total_ticks,max_ticks,presentations\n"
        "1000,1000,9,4,30,10,10,advance,1,90,90,17\n"
        "1000,1000,9,4,30,10,10,audio,2,60,50,17\n"
        "1000,1000,9,4,30,10,10,models,1,30,30,17\n", "CSV values, units or counts changed");
    require(profile.totals() == std::array<FramePhaseTotals,unsigned(FramePhase::count)>{}, "Accepted window did not reset");
    profile.record(FramePhase::dots, std::numeric_limits<std::uint64_t>::max()-5, 4);
    require(profile.totals()[unsigned(FramePhase::dots)] == FramePhaseTotals{1,10,10}, "Clock rollover lost duration");
    std::ostringstream broken; broken.setstate(std::ios::badbit);
    require(!profile.write_window(broken, 2000, 9, 4, 60, 20, 20) && profile.stopped(), "Bad stream not disabled");
    const auto stopped = profile.totals(); profile.record(FramePhase::logic, 0, 42);
    require(profile.totals() == stopped && !profile.write_window(stream, 3000, 9, 4, 60, 20, 20), "Failed diagnostic kept growing");
    tick = 0; FrameProfile capped(clock_tick, 1); std::ostringstream bounded;
    for(unsigned i = 1; i <= FrameProfile::maximum_windows; ++i) {
        capped.record(FramePhase::frame, 0, 1);
        require(capped.write_window(bounded, i, 0, 0, i, i, i), "Bounded window failed early");
    }
    const auto size = bounded.str().size();
    require(capped.stopped() && !capped.write_window(bounded, 999, 0, 0, 0, 0, 0) && bounded.str().size() == size,
        "Diagnostic window cap lost");
    FrameProfile no_clock(nullptr, 1), no_frequency(clock_tick, 0);
    for(auto* disabled : {&no_clock, &no_frequency}) {
        disabled->record(FramePhase::frame, 0, 100);
        require(!disabled->write_window(stream, 1000, 0, 0, 0, 0, 0)
            && disabled->totals() == std::array<FramePhaseTotals,unsigned(FramePhase::count)>{}, "Disabled clock emitted data");
    }
    FrameProfile macro_profile(clock_tick, 1);
    {
        ScopedFrameProfileActivation activation(macro_profile);
        const auto before = clock_calls;
        { STARFOX_3DS_FRAME_PHASE(video); tick += 7; }
#if defined(STARFOX_3DS_PROFILE_FRAMES)
        require(clock_calls == before+2 && macro_profile.totals()[unsigned(FramePhase::video)] == FramePhaseTotals{1,7,7},
            "Enabled native macro lost timing");
#else
        require(clock_calls == before && macro_profile.totals() == std::array<FramePhaseTotals,unsigned(FramePhase::count)>{},
            "Disabled native macro read the clock or changed stats");
#endif
    }
    FrameProfile throwing(clock_tick, 1); std::ostringstream failed_stream;
    failed_stream.setstate(std::ios::badbit);
    try { failed_stream.exceptions(std::ios::badbit); } catch(const std::ios_base::failure&) {}
    require(!throwing.write_window(failed_stream, tick+1, 0, 0, 0, 0, 0) && throwing.stopped(),
        "Throwing stream escaped noexcept diagnostic");
    tick = 0;
    FrameProfile details(clock_tick, 1000);
    {
        ScopedFrameProfileActivation activation(details);
        const auto before = clock_calls;
        { STARFOX_3DS_FRAME_PHASE(cpu); tick += 1; }
        { STARFOX_3DS_FRAME_PHASE(strategies); tick += 2; }
        { STARFOX_3DS_FRAME_PHASE(view); tick += 3; }
        { STARFOX_3DS_FRAME_PHASE(cull); tick += 4; }
        { STARFOX_3DS_FRAME_PHASE(audio_irq); tick += 5; }
        { STARFOX_3DS_FRAME_PHASE(music); tick += 6; }
        { STARFOX_3DS_FRAME_PHASE(effects); tick += 7; }
        { STARFOX_3DS_FRAME_PHASE(spc_emulate); tick += 8; }
        { STARFOX_3DS_FRAME_PHASE(spc_filter); tick += 9; }
        { STARFOX_3DS_FRAME_PHASE(bg_decode); tick += 10; }
        { STARFOX_3DS_FRAME_PHASE(bg_colour); tick += 11; }
        { STARFOX_3DS_FRAME_PHASE(gpu_sync); tick += 12; }
        { STARFOX_3DS_FRAME_PHASE(gpu_wait); tick += 13; }
        { STARFOX_3DS_FRAME_PHASE(gpu_upload); tick += 14; }
        { STARFOX_3DS_FRAME_PHASE(gpu_record); tick += 15; }
        { STARFOX_3DS_FRAME_PHASE(gpu_submit); tick += 16; }
        { STARFOX_3DS_FRAME_PHASE(terrain_build); tick += 17; }
        { STARFOX_3DS_FRAME_PHASE(terrain_reuse); tick += 18; }
        { STARFOX_3DS_FRAME_PHASE(mono_background); tick += 19; }
        { STARFOX_3DS_FRAME_PHASE(mono_prepare); tick += 20; }
        { STARFOX_3DS_FRAME_PHASE(mono_raster); tick += 21; }
        { STARFOX_3DS_FRAME_PHASE(mono_finish); tick += 22; }
        { STARFOX_3DS_FRAME_PHASE(lcd_copy); tick += 23; }
        { STARFOX_3DS_FRAME_PHASE(mono_retry); tick += 24; }
        { STARFOX_3DS_FRAME_PHASE(mono_fallback); tick += 25; }
        { STARFOX_3DS_FRAME_PHASE(mono_overlay); tick += 26; }
        { STARFOX_3DS_FRAME_PHASE(mono_canvas); tick += 27; }
        { STARFOX_3DS_FRAME_PHASE(object_export); tick += 28; }
        { STARFOX_3DS_FRAME_PHASE(object_import); tick += 29; }
#if defined(STARFOX_3DS_PROFILE_FRAMES)
        require(clock_calls == before + 58, "Detailed macros lost clock boundaries");
        for(unsigned i = unsigned(FramePhase::cpu); i < unsigned(FramePhase::count); ++i) {
            const std::uint64_t duration = i - unsigned(FramePhase::cpu) + 1U;
            require(details.totals()[i] == FramePhaseTotals{1, duration, duration},
                "Detailed phase timing/count mismatch");
        }
#else
        require(clock_calls == before && details.totals()
            == std::array<FramePhaseTotals,unsigned(FramePhase::count)>{},
            "Disabled detailed markers changed runtime work");
#endif
    }
    // Exercise every row, not only the original fourteen. New details must
    // obey the same stream failure/reset/window limits as their parents.
    for(unsigned i = 0; i < unsigned(FramePhase::count); ++i)
        details.record(FramePhase(i), 100, 101);
    std::ostringstream detailed_csv;
    require(details.write_window(detailed_csv, 1000, 9, 3, 3, 1, 1),
        "Detailed CSV failed");
    const auto detailed_text = detailed_csv.str();
    for(auto name : frame_phase_names)
        require(detailed_text.find("," + std::string(name) + ",") != std::string::npos,
            "Detailed CSV omitted a phase");
    require(details.totals() == std::array<FramePhaseTotals,unsigned(FramePhase::count)>{},
        "Detailed window retained counters");
    // The native player captures only bounded numeric snapshots during play.
    // Compare every phase/window byte-for-byte to the original CSV serializer,
    // including unsigned clock rollover, metadata, nested totals and the cap.
    tick = std::numeric_limits<std::uint64_t>::max() - 500;
    FrameProfile reference(clock_tick, 1000), buffered_profile(clock_tick, 1000);
    FrameProfileBuffer buffer;
    CountingStreamBuffer destination;
    std::ostream buffered_csv(&destination);
    std::ostringstream reference_csv;
    const auto storage = buffer.storage_bytes();
    require(storage <= FrameProfileBuffer::maximum_storage_bytes && !buffer.size()
        && FrameProfileBuffer::maximum_windows <= FrameProfile::maximum_windows
        && FrameProfileBuffer::maximum_windows >= 480,
        "Diagnostic phases exceeded memory budget or unexpectedly truncated capture");
    const auto before_capture = clock_calls;
    for(unsigned window = 1; window <= FrameProfileBuffer::maximum_windows; ++window) {
        for(unsigned phase = 0; phase < unsigned(FramePhase::count); ++phase) {
            reference.record(FramePhase(phase), 100, 101 + phase);
            buffered_profile.record(FramePhase(phase), 100, 101 + phase);
        }
        const auto time = tick + std::uint64_t(window) * 1000;
        if(window == 1) require(!buffer.capture(buffered_profile,time-1,9,27,60,20,20),
            "Buffered sub-second window consumed totals");
        require(reference.write_window(reference_csv,time,int(window%10),window,window*3,window,window,window*2),
            "Reference window serialization failed");
        require(buffer.capture(buffered_profile,time,int(window%10),window,window*3,window,window,window*2),
            "Buffered window stopped early");
        require(destination.str().empty() && !destination.flushes,
            "Capturing a gameplay window performed stream I/O");
        require(buffer.size() == window && buffer.storage_bytes() == storage
            && buffered_profile.totals() == reference.totals(),
            "Buffered windows grew storage or changed timing/retirement");
    }
    require(clock_calls == before_capture && buffered_profile.stopped()
        && !buffer.capture(buffered_profile,tick+999999,9,27,0,0,0),
        "Buffered capture read extra clocks or lost its bounded stop");
    require(buffer.flush(buffered_csv) && destination.flushes == 1
        && destination.str() == reference_csv.str(),
        "Shutdown CSV differs from all original diagnostic windows");
    require(buffer.flush(buffered_csv) && destination.flushes == 1
        && !buffer.capture(buffered_profile,tick+999999,9,27,0,0,0),
        "Repeated shutdown duplicated diagnostic output");
    tick = 0;
    FrameProfile bad_buffer_profile(clock_tick,1); FrameProfileBuffer bad_buffer;
    bad_buffer_profile.record(FramePhase::frame,0,1);
    require(bad_buffer.capture(bad_buffer_profile,1,9,27,0,0,0), "Failed-output fixture lost capture");
    std::ostringstream bad_buffer_stream; bad_buffer_stream.setstate(std::ios::badbit);
    try {bad_buffer_stream.exceptions(std::ios::badbit);} catch(const std::ios_base::failure&) {}
    require(!bad_buffer.flush(bad_buffer_stream) && !bad_buffer.flush(buffered_csv)
        && destination.flushes == 1, "Failed shutdown logging escaped or retried a partial CSV");
    FrameProfileBuffer empty_buffer;
    require(empty_buffer.flush(buffered_csv) && destination.flushes == 1,
        "Empty diagnostic owner performed shutdown I/O");
    std::cout << "3DS native phase totals, nested owners, rollover, unwinding, CSV cadence, bounded in-memory capture and shutdown-only logging pass\n";
} catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
