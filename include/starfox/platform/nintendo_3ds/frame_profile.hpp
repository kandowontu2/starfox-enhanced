#pragma once
#include <array>
#include <cstdint>
#include <ostream>
#include <string_view>

namespace starfox::platform::nintendo_3ds {
// Optional owner-thread diagnostics. Raw native clock ticks, not guessed FPS
// or desktop wall time. Parent phases deliberately include their children.
enum class FramePhase : unsigned {
    frame, advance, video, logic, capture, audio, raster, checkpoint, menu,
    models, layers, dots, composite, present,
    cpu, strategies, view, cull, audio_irq, music, effects, spc_emulate,
    spc_filter, bg_decode, bg_colour, count
};
inline constexpr std::array<std::string_view, unsigned(FramePhase::count)> frame_phase_names{
    "frame", "advance", "video", "logic", "capture", "audio", "raster",
    "checkpoint", "menu", "models", "layers", "dots", "composite", "present",
    "cpu", "strategies", "view", "cull", "audio_irq", "music", "effects",
    "spc_emulate", "spc_filter", "bg_decode", "bg_colour"
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
    // At most 512 one-second windows and FramePhase::count bounded-width rows
    // per window. New phases are appended so existing phase indices stay stable.
    // A failed/full stream is disabled, never allowed to terminate the game.
    bool write_window(std::ostream& stream, std::uint64_t time, int flow, unsigned background,
                      unsigned video_phases, unsigned logic_ticks, unsigned audio_blocks) noexcept {
        if(!clock_ || !frequency_ || stopped_ || time - previous_ < frequency_) return false;
        try {
            if(!windows_) stream << "tick,clock_hz,flow,bg,video_phases,logic_ticks,audio_blocks,phase,calls,total_ticks,max_ticks\n";
            for(unsigned phase = 0; phase < totals_.size(); ++phase) {
                const auto& value = totals_[phase];
                if(!value.calls) continue;
                stream << time << ',' << frequency_ << ',' << flow << ',' << background << ','
                    << video_phases << ',' << logic_ticks << ',' << audio_blocks << ','
                    << frame_phase_names[phase] << ',' << value.calls << ',' << value.ticks << ','
                    << value.maximum << '\n';
            }
            stream.flush();
            if(!stream) { stopped_ = true; return false; }
            totals_ = {}; previous_ = time;
            if(++windows_ == maximum_windows) stopped_ = true;
            return true;
        } catch(...) { stopped_ = true; return false; }
    }
private:
    Clock clock_{};
    std::uint64_t frequency_{}, previous_{};
    std::array<FramePhaseTotals, unsigned(FramePhase::count)> totals_{};
    unsigned windows_{};
    bool stopped_{};
};
inline FrameProfile* active_frame_profile{};
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
