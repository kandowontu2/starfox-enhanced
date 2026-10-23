#pragma once
#include <cstdint>
#include <ostream>
#include <string_view>

namespace starfox::platform::nintendo_3ds {
// A phase-entry receipt survives a stall, unlike completed-frame CSV totals.
// Only the owner thread writes. Source replacement, ordinary Start and first
// GPU initialization rearm a bounded window, including after menu idle time.
// A warm presenter handoff must not rearm SD writes during ordinary gameplay.
// Repeated audio entries have their own budget: preroll cannot consume the
// phase receipts needed to distinguish VBlank from GPU queue completion.
// SD failures disable diagnostics rather than interrupting the game.
class StartupTrace {
public:
    using Clock = std::uint64_t (*)() noexcept;
    static constexpr unsigned maximum_frames = 16, maximum_rows = 512;
    static constexpr unsigned maximum_windows = 8, maximum_detail_rows = 96;
    StartupTrace(std::ostream& stream, Clock clock) noexcept : stream_(stream), clock_(clock) {}
    void begin_frame() noexcept { if(frame_ < maximum_frames + 1) ++frame_; }
    void restart_window() noexcept {
        if(stopped_ || window_ + 1 >= maximum_windows) return;
        ++window_; frame_ = rows_ = detail_rows_ = 0;
    }
    void restart_source_window() noexcept {
        gpu_window_started_ = false;
        restart_window();
    }
    void first_gpu_window() noexcept {
        if(gpu_window_started_) return;
        gpu_window_started_ = true;
        restart_window();
        write("first_gpu_frame");
    }
    void write_detail(std::string_view phase) noexcept {
        if(detail_rows_ >= maximum_detail_rows) return;
        ++detail_rows_;write(phase);
    }
    void write(std::string_view phase) noexcept {
        if(stopped_ || !clock_ || frame_ > maximum_frames || rows_ >= maximum_rows) return;
        try {
            if(!header_written_) {stream_ << "frame,tick,phase\n";header_written_ = true;}
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
    unsigned frame_{}, rows_{}, detail_rows_{}, window_{};
    bool stopped_{}, header_written_{}, gpu_window_started_{};
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
#define STARFOX_3DS_STARTUP_FIRST_GPU() do { \
    if(::starfox::platform::nintendo_3ds::active_startup_trace) \
        ::starfox::platform::nintendo_3ds::active_startup_trace->first_gpu_window(); \
} while(false)
#define STARFOX_3DS_STARTUP_RESTART(name) do { \
    if(::starfox::platform::nintendo_3ds::active_startup_trace) { \
        ::starfox::platform::nintendo_3ds::active_startup_trace->restart_window(); \
        ::starfox::platform::nintendo_3ds::active_startup_trace->write(#name); \
    } \
} while(false)
#define STARFOX_3DS_STARTUP_MARK(name) do { \
    if(::starfox::platform::nintendo_3ds::active_startup_trace) \
        ::starfox::platform::nintendo_3ds::active_startup_trace->write(#name); \
} while(false)
#define STARFOX_3DS_STARTUP_DETAIL_MARK(name) do { \
    if(::starfox::platform::nintendo_3ds::active_startup_trace) \
        ::starfox::platform::nintendo_3ds::active_startup_trace->write_detail(#name); \
} while(false)
#else
#define STARFOX_3DS_STARTUP_FIRST_GPU() ((void)0)
#define STARFOX_3DS_STARTUP_RESTART(name) ((void)0)
#define STARFOX_3DS_STARTUP_MARK(name) ((void)0)
#define STARFOX_3DS_STARTUP_DETAIL_MARK(name) ((void)0)
#endif
