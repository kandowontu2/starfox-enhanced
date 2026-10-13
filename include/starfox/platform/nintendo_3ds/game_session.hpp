#pragma once
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/platform/nintendo_3ds/game_hud.hpp"
#include "starfox/platform/nintendo_3ds/game_presentation.hpp"
#include "starfox/platform/nintendo_3ds/settings_reset.hpp"
#include "starfox/vr/game_scene.hpp"
#include <functional>
#include <type_traits>
#include <unordered_set>

namespace starfox::platform_3ds {class Frame3ds;}
namespace starfox::platform::nintendo_3ds {
struct GameAdvance {
    unsigned video_phases{},logic_ticks{},audio_blocks{};
    double raster_fraction{};
    bool duplicate{},time_clamped{};
    // The host must replace the cartridge owner, not run EX against Original
    // data (or vice versa). No further ticks are accepted while this is pending.
    std::optional<simulation::Experience> requested_experience;
    // Preview uses a real cartridge LEVEL1_1 owner, like desktop. A host
    // rebuild must finish before either the old scene or its SPC can tick again.
    std::optional<bool> requested_preview;
    bool start_after_preview{};
    // Five uninterrupted seconds of mapped in-game L+R in setup/options.
    // Retire this owner and rebuild real Original BOOT with default settings;
    // the host preserves battery SRAM. Never tick the old owner afterwards.
    bool requested_settings_reset{};
    bool requested_controller_remap{};
    bool requested_hud_customization{};
};
struct GamePreferences {
    simulation::TimingMode timing{simulation::TimingMode::original_speed};
    std::uint8_t music{100},sfx{100},language{},laser{},level{};
    bool swap{},god{},bombs{},boost{},lives{},planet_cheat{};
    // Native 400-pixel LCD defaults: 8 pixels of infinity disparity at full
    // slider, with the same parallel cameras for finite terrain/models. Saved
    // convergence remains authoritative; this is not a background-only shift.
    std::uint16_t separation{16},convergence{512};
    std::uint8_t render_fps{60};
    bool show_fps{};
    CockpitLayout hud_layout{};
    bool operator==(const GamePreferences&) const=default;
};
struct GameSessionOptions {
    std::optional<GamePreferences> preferences;
    bool preview{},start_after_preview{};
    // Pump platform events during the bounded, silent source preview preroll.
    // False cancels loading; no half-initialized owner may be presented.
    std::function<bool(unsigned)> preview_progress;
    // Borrowed synchronous executor: complete both stems before source port
    // acknowledgement/state handoff. Platform owner must outlive the session.
    audio::StemExecutor* stem_executor{};
    // Distinct borrowed owner for splitting the audio block already running
    // on the frame worker. Never recursively dispatch to stem_executor.
    audio::StemExecutor* frame_audio_stem_executor{};
};
// Graphics/SDK-independent native game owner. This is the actual simulation,
// SPC driver and cartridge HUD, not the asset-free frontend diagnostic. Native
// PICA/NDSP adapters consume its output; neither eye is allowed to tick it.
class GameSession {
public:
    // Consume/copy PCM synchronously: the span is reused after the callback.
    // The NDSP adapter must copy into a free DSP-owned linear-memory block.
    using PcmSink=std::function<void(std::span<const std::int16_t>)>;
    GameSession(assets::RomImage,assets::SymbolMap,PcmSink,
        std::string initial_map="BOOT",std::span<const std::uint8_t> cartridge_ram={},
        const GameSessionOptions& options={});
    ~GameSession();
    // Reference mono raster; lazily owned by the stable cartridge session.
    // Its interpolation snapshots are captured at actual source ticks, not
    // once per eye or by a separate simulation/audio timeline.
    platform_3ds::Frame3ds& mono_renderer();
    [[nodiscard]] bool mono_prepared() const noexcept {return mono_prepared_;}
    // Bounded reference shape/LOD warmup while the host displays progress and
    // services APT. Does not tick cartridge logic/audio or change preferences.
    bool prepare_mono(const std::function<bool(unsigned,unsigned)>& progress);
    GameSession(const GameSession&)=delete;
    GameSession& operator=(const GameSession&)=delete;
    GameSession(GameSession&&)=delete; // Internal source references must stay stable.
    GameSession& operator=(GameSession&&)=delete;

    // Monotonic nanoseconds from the platform clock, sampled ONCE per host
    // frame. Source raster is 60 Hz; SPC is 20 Hz even at original FX pacing.
    GameAdvance advance(std::int64_t nanoseconds,input::ButtonMask held,bool focused=true,
        std::optional<input::ButtonMask> mapped_gameplay={},bool overlap_frame_audio=false);
    // Optional presentation-only work can run on the owner thread while the
    // last source phase's complete SPC block runs on the joined worker. Work
    // must not advance/replace the session or inspect the mutable SPC owner.
    // All PCM/port acknowledgements finish before this method returns; no task
    // escapes into a later source phase, save, suspension or owner destruction.
    void run_frame_work(audio::StemTask work);
    template<class Work> requires std::is_invocable_v<Work&>
    void run_frame_work(Work&& work) {
        run_frame_work(audio::StemTask{const_cast<void*>(static_cast<const void*>(std::addressof(work))),[](void* context) {
            (*static_cast<std::remove_reference_t<Work>*>(context))();
        }});
    }
    // Explicitly acknowledge the host editor; source cursor/state and partial
    // SPC cadence survive. Suppress held input before returning to Options.
    void finish_controller_remap();
    void finish_hud_customization(std::optional<CockpitLayout> applied={});
    // Full VM/SPC/partial-raster timeline. Preparing a replacement owns its own
    // ROM/symbol references; validation never mutates this running session.
    [[nodiscard]] std::vector<std::uint8_t> save_state() const;
    [[nodiscard]] std::unique_ptr<GameSession> restored_state(std::span<const std::uint8_t>) const;
    [[nodiscard]] bool state_available() const noexcept;
    // Host quick menu entry to the actual shared runtime options, not a second
    // copy of source settings. Rebases input/time without losing partial audio.
    bool toggle_runtime_options();
    [[nodiscard]] bool controller_remap_pending() const noexcept {return requested_controller_remap_;}
    [[nodiscard]] bool hud_customization_pending() const noexcept {return requested_hud_customization_;}
    [[nodiscard]] GamePresentation presentation(float slider,bool stereoscopic_hardware,
        const StereoSettings& settings={}) const;
    [[nodiscard]] const simulation::GameSimulation& game() const noexcept {return game_;}
    [[nodiscard]] const audio::Spc700Audio& audio() const noexcept {return audio_;}
    // Immutable assets for native renderer owners; neither exposes VM mutation.
    [[nodiscard]] const assets::RomImage& rom() const noexcept {return rom_;}
    [[nodiscard]] const assets::SymbolMap& symbols() const noexcept {return symbols_;}
    [[nodiscard]] simulation::Experience cartridge_experience() const noexcept {return cartridge_experience_;}
    [[nodiscard]] std::span<const std::uint8_t> cartridge_ram() const noexcept {
        // Retail has no battery-backed cartridge save bank. The VM's generic
        // mapped RAM allocation must not be passed as EX SRAM to retail BOOT.
        return cartridge_experience_==simulation::Experience::starfox_ex?game_.ex_save_ram():std::span<const std::uint8_t>{};
    }
    [[nodiscard]] GamePreferences preferences() const noexcept;
    [[nodiscard]] const SettingsResetHold& settings_reset_hold() const noexcept {return reset_hold_;}
    [[nodiscard]] StereoSettings stereo_settings() const noexcept {
        StereoSettings result;
        result.separation=float(std::min<std::uint16_t>(64,game_.stereo_separation()));
        result.convergence=float(game_.stereo_convergence());return result;
    }
private:
    void apply_native_capabilities() noexcept;
    void prepare_pace_shapes();
    void publish_raster();
    assets::RomImage rom_;
    assets::SymbolMap symbols_;
    simulation::TrigTables native_model_trig_;
    std::array<std::uint32_t,10> native_model_addresses_{};
    std::uint32_t native_model_depth_tables_{};
    simulation::Experience cartridge_experience_;
    // The counts outlive the simulation's non-owning pace-table pointer.
    std::unordered_map<std::uint32_t,std::uint32_t> face_counts_;
    std::unordered_set<std::uint32_t> invalid_shapes_;
    assets::ShapeDecoder decoder_;
    simulation::GameSimulation game_;
    audio::Spc700Audio audio_;
    PcmSink sink_;
    GameHud hud_;
    GameHudFrame hud_frame_;
    vr::GameSceneHistory history_;
    std::shared_ptr<const GameRasterSnapshot> raster_;
    timing::FixedStepClock clock_{60};
    input::InputLatch input_;
    std::optional<std::int64_t> previous_time_;
    std::vector<simulation::ApuPortWrite> pending_audio_;
    std::vector<std::int16_t> mixed_;
    unsigned audio_phase_{};
    double fraction_{};
    bool failed_{},suppress_held_{},deferred_audio_{},frame_work_active_{};
    std::optional<simulation::Experience> requested_experience_;
    std::optional<bool> requested_preview_;
    bool start_after_preview_{};
    SettingsResetHold reset_hold_;
    bool requested_settings_reset_{};
    bool requested_controller_remap_{};
    bool requested_hud_customization_{};
    CockpitLayout hud_layout_;
    audio::StemExecutor* stem_executor_{};
    audio::StemExecutor* frame_audio_stem_executor_{};
    // Destroy the borrowed-ROM renderer before game_, symbols_ and rom_.
    std::unique_ptr<platform_3ds::Frame3ds> mono_renderer_;
    bool mono_prepared_{};
};
} // namespace starfox::platform::nintendo_3ds
