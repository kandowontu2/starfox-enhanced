#pragma once
#include <array>
#include <cstdint>
#include <ostream>
#include <string_view>
#include <vector>

namespace starfox::platform::nintendo_3ds {
// Optional owner-thread diagnostics. Raw native clock ticks, not guessed FPS
// or desktop wall time. Parent phases deliberately include their children.
enum class FramePhase : unsigned {
    frame, advance, video, logic, capture, audio, raster, checkpoint, menu,
    models, layers, dots, composite, present,
    cpu, strategies, view, cull, audio_irq, music, effects, spc_emulate,
    spc_filter, bg_decode, bg_colour,
    gpu_sync, gpu_wait, gpu_upload, gpu_record, gpu_submit,
    terrain_build, terrain_reuse,
    mono_background, mono_prepare, mono_raster, mono_finish, lcd_copy,
    mono_retry, mono_fallback, mono_overlay, mono_canvas,
    object_export, object_import, count
};
inline constexpr std::array<std::string_view, unsigned(FramePhase::count)> frame_phase_names{
    "frame", "advance", "video", "logic", "capture", "audio", "raster",
    "checkpoint", "menu", "models", "layers", "dots", "composite", "present",
    "cpu", "strategies", "view", "cull", "audio_irq", "music", "effects",
    "spc_emulate", "spc_filter", "bg_decode", "bg_colour",
    "gpu_sync", "gpu_wait", "gpu_upload", "gpu_record", "gpu_submit",
    "terrain_build", "terrain_reuse",
    "mono_background", "mono_prepare", "mono_raster", "mono_finish", "lcd_copy",
    "mono_retry", "mono_fallback", "mono_overlay", "mono_canvas",
    "object_export", "object_import"
};
struct FramePhaseTotals {
    std::uint64_t calls{}, ticks{}, maximum{};
    bool operator==(const FramePhaseTotals&) const = default;
};
class FrameProfile {
public:
    using Clock = std::uint64_t (*)() noexcept;
    static constexpr unsigned maximum_windows = 512;
    explicit FrameProfile(Clock clock, std::uint64_t frequency) noexcept
        :clock_(clock), frequency_(frequency), previous_(now()) {}
    [[nodiscard]] std::uint64_t now() const noexcept { return clock_ ? clock_() : 0; }
    void record(FramePhase phase, std::uint64_t begin, std::uint64_t end) noexcept {
        if(!clock_ || !frequency_ || stopped_ || unsigned(phase) >= totals_.size()) return;
        const auto elapsed = end - begin; // unsigned clock rollover is intentional
        auto& value = totals_[unsigned(phase)];
        ++value.calls; value.ticks += elapsed;
        if(elapsed > value.maximum) value.maximum = elapsed;
    }
    [[nodiscard]] const auto& totals() const noexcept { return totals_; }
    [[nodiscard]] bool stopped() const noexcept { return stopped_; }
    // Each joined worker records into its own counters. Merge only after task
    // completion; a worker never writes the owner's active CSV/window state.
    [[nodiscard]] FrameProfile fork() const noexcept { return FrameProfile(clock_, frequency_); }
    void merge_completed(const FrameProfile& child) noexcept {
        if(!clock_ || !frequency_ || stopped_ || child.clock_ != clock_
            || child.frequency_ != frequency_) return;
        for(unsigned phase = 0; phase < totals_.size(); ++phase) {
            const auto& source = child.totals_[phase];
            auto& destination = totals_[phase];
            destination.calls += source.calls;
            destination.ticks += source.ticks;
            if(source.maximum > destination.maximum) destination.maximum = source.maximum;
        }
    }
    // At most 512 one-second windows and FramePhase::count bounded-width rows
    // per window. New phases are appended so existing phase indices stay stable.
    // A failed/full stream is disabled, never allowed to terminate the game.
    bool write_window(std::ostream& stream, std::uint64_t time, int flow, unsigned background,
                      unsigned video_phases, unsigned logic_ticks, unsigned audio_blocks,
                      unsigned presentations = 0) noexcept {
        if(!clock_ || !frequency_ || stopped_ || time - previous_ < frequency_) return false;
        try {
            if(!windows_) stream << "tick,clock_hz,flow,bg,video_phases,logic_ticks,audio_blocks,phase,calls,total_ticks,max_ticks,presentations\n";
            for(unsigned phase = 0; phase < totals_.size(); ++phase) {
                const auto& value = totals_[phase];
                if(!value.calls) continue;
                stream << time << ',' << frequency_ << ',' << flow << ',' << background << ','
                    << video_phases << ',' << logic_ticks << ',' << audio_blocks << ','
                    << frame_phase_names[phase] << ',' << value.calls << ',' << value.ticks << ','
                    << value.maximum << ',' << presentations << '\n';
            }
            stream.flush();
            if(!stream) { stopped_ = true; return false; }
            totals_ = {}; previous_ = time;
            if(++windows_ == maximum_windows) stopped_ = true;
            return true;
        } catch(...) { stopped_ = true; return false; }
    }
private:
    friend class FrameProfileBuffer;
    Clock clock_{};
    std::uint64_t frequency_{}, previous_{};
    std::array<FramePhaseTotals, unsigned(FramePhase::count)> totals_{};
    unsigned windows_{};
    bool stopped_{};
};
// SD writes/flushes can block for seconds on a real console. Keep the same
// bounded timing windows in memory during play, and serialize them only after
// the game/audio/GPU owners have retired. In particular, do not hide an SD
// flush outside the measured frame and then let that delay enter catch-up.
// The separate short startup trace still provides receipts for a hard freeze.
class FrameProfileBuffer {
    struct Window {
        std::uint64_t time{}, frequency{};
        int flow{};
        // Successful whole-frame presentation returns, NOT source phases,
        // loops, eyes or GPU retirement. The counter uses alignment padding.
        unsigned background{}, video_phases{}, logic_ticks{}, audio_blocks{}, presentations{};
        std::array<FramePhaseTotals, unsigned(FramePhase::count)> totals{};
    };
public:
    static constexpr std::size_t maximum_storage_bytes = 512U * 1024U;
    // New diagnostic phases must not grow the console's allocation. The
    // bounded capture duration shrinks instead (489 windows with 43 phases).
    static constexpr unsigned maximum_windows =
        maximum_storage_bytes / sizeof(Window) < FrameProfile::maximum_windows
        ? unsigned(maximum_storage_bytes / sizeof(Window)) : FrameProfile::maximum_windows;
    static_assert(maximum_windows > 0);
    FrameProfileBuffer() : windows_(maximum_windows) {}
    bool capture(FrameProfile& profile, std::uint64_t time, int flow, unsigned background,
                 unsigned video_phases, unsigned logic_ticks, unsigned audio_blocks,
                 unsigned presentations = 0) noexcept {
        if(flush_attempted_ || !profile.clock_ || !profile.frequency_ || profile.stopped_
            || time - profile.previous_ < profile.frequency_
            || count_ == windows_.size()) return false;
        auto& window = windows_[count_++];
        window = {time, profile.frequency_, flow, background, video_phases, logic_ticks,
            audio_blocks, presentations, profile.totals_};
        profile.totals_ = {}; profile.previous_ = time;
        if(++profile.windows_ == maximum_windows) profile.stopped_ = true;
        return true;
    }
    // One explicit, idempotent shutdown write; neither capture nor destruction
    // touches a stream. A failed write is not retried into a partial CSV.
    bool flush(std::ostream& stream) noexcept {
        if(flush_attempted_) return flush_succeeded_;
        flush_attempted_ = true;
        if(!count_) { flush_succeeded_ = true; return true; }
        try {
            stream << "tick,clock_hz,flow,bg,video_phases,logic_ticks,audio_blocks,phase,calls,total_ticks,max_ticks,presentations\n";
            for(unsigned i = 0; i < count_; ++i) {
                const auto& window = windows_[i];
                for(unsigned phase = 0; phase < window.totals.size(); ++phase) {
                    const auto& value = window.totals[phase];
                    if(!value.calls) continue;
                    stream << window.time << ',' << window.frequency << ',' << window.flow << ','
                        << window.background << ',' << window.video_phases << ',' << window.logic_ticks
                        << ',' << window.audio_blocks << ',' << frame_phase_names[phase] << ','
                        << value.calls << ',' << value.ticks << ',' << value.maximum << ','
                        << window.presentations << '\n';
                }
            }
            stream.flush(); flush_succeeded_ = bool(stream);
        } catch(...) { flush_succeeded_ = false; }
        return flush_succeeded_;
    }
    [[nodiscard]] unsigned size() const noexcept { return count_; }
    [[nodiscard]] std::size_t storage_bytes() const noexcept { return windows_.size() * sizeof(Window); }
private:
    static_assert(sizeof(Window) * maximum_windows <= maximum_storage_bytes);
    std::vector<Window> windows_;
    unsigned count_{};
    bool flush_attempted_{}, flush_succeeded_{};
};
inline thread_local FrameProfile* active_frame_profile{};
class ScopedFrameProfileActivation {
public:
    explicit ScopedFrameProfileActivation(FrameProfile& profile) noexcept
        :previous_(active_frame_profile) { active_frame_profile = &profile; }
    ~ScopedFrameProfileActivation() { active_frame_profile = previous_; }
    ScopedFrameProfileActivation(const ScopedFrameProfileActivation&) = delete;
    ScopedFrameProfileActivation& operator=(const ScopedFrameProfileActivation&) = delete;
private:
    FrameProfile* previous_{};
};
class ScopedFramePhase {
public:
    ScopedFramePhase(FrameProfile* profile, FramePhase phase) noexcept
        :profile_(profile), phase_(phase), begin_(profile ? profile->now() : 0) {}
    ~ScopedFramePhase() { if(profile_) profile_->record(phase_, begin_, profile_->now()); }
    ScopedFramePhase(const ScopedFramePhase&) = delete;
    ScopedFramePhase& operator=(const ScopedFramePhase&) = delete;
private:
    FrameProfile* profile_{};
    FramePhase phase_{};
    std::uint64_t begin_{};
};
}
#if defined(STARFOX_3DS_PROFILE_FRAMES)
#define STARFOX_3DS_FRAME_PHASE(name) \
    ::starfox::platform::nintendo_3ds::ScopedFramePhase sfe_profile_##name( \
        ::starfox::platform::nintendo_3ds::active_frame_profile, \
        ::starfox::platform::nintendo_3ds::FramePhase::name)
#else
#define STARFOX_3DS_FRAME_PHASE(name) ((void)0)
#endif
