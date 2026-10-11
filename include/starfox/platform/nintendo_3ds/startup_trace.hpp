#pragma once
#include <cstdint>
#include <ostream>
#include <string_view>

namespace starfox::platform::nintendo_3ds {
// A phase-entry receipt survives a stall, unlike completed-frame CSV totals.
// Only the owner thread writes. The initial 16 frames / 512 rows are bounded;
// SD failures disable diagnostics rather than interrupting the game.
class StartupTrace {
public:
    using Clock = std::uint64_t (*)() noexcept;
    static constexpr unsigned maximum_frames = 16, maximum_rows = 512;
    StartupTrace(std::ostream& stream, Clock clock) noexcept : stream_(stream), clock_(clock) {}
    void begin_frame() noexcept { if(frame_ < maximum_frames + 1) ++frame_; }
    void write(std::string_view phase) noexcept {
        if(stopped_ || !clock_ || frame_ > maximum_frames || rows_ >= maximum_rows) return;
        try {
            if(!rows_) stream_ << "frame,tick,phase\n";
            stream_ << frame_ << ',' << clock_() << ',' << phase << '\n';
            stream_.flush();
            ++rows_;
            if(!stream_) stopped_ = true;
        } catch(...) { stopped_ = true; }
    }
    [[nodiscard]] bool stopped() const noexcept { return stopped_; }
private:
    std::ostream& stream_;
    Clock clock_{};
    unsigned frame_{}, rows_{};
    bool stopped_{};
};
inline thread_local StartupTrace* active_startup_trace{};
class ScopedStartupTraceActivation {
public:
    explicit ScopedStartupTraceActivation(StartupTrace& trace) noexcept
        : previous_(active_startup_trace) { active_startup_trace = &trace; }
    ~ScopedStartupTraceActivation() { active_startup_trace = previous_; }
    ScopedStartupTraceActivation(const ScopedStartupTraceActivation&) = delete;
    ScopedStartupTraceActivation& operator=(const ScopedStartupTraceActivation&) = delete;
private:
    StartupTrace* previous_{};
};
}
#if defined(STARFOX_3DS_PROFILE_FRAMES)
#define STARFOX_3DS_STARTUP_MARK(name) do { \
    if(::starfox::platform::nintendo_3ds::active_startup_trace) \
        ::starfox::platform::nintendo_3ds::active_startup_trace->write(#name); \
} while(false)
#else
#define STARFOX_3DS_STARTUP_MARK(name) ((void)0)
#endif
