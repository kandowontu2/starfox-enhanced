#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/platform/nintendo_3ds/game_state.hpp"
#include "starfox/assets/bps.hpp"

namespace starfox::platform::nintendo_3ds {
bool GameSession::state_available() const noexcept {
    return !failed_ && !game_.runtime_options_open() && !requested_experience_ && !requested_preview_
        && !requested_settings_reset_ && !requested_controller_remap_ && !requested_hud_customization_ && !start_after_preview_;
}
std::vector<std::uint8_t> GameSession::save_state() const {
    if(!state_available()) throw std::runtime_error("Close runtime options or finish cartridge handoff before saving a state");
    GameStateData data;
    data.game=game_.save_state();data.audio=audio_.save_state();data.pending_audio=pending_audio_;
    data.audio_phase=static_cast<std::uint8_t>(audio_phase_);data.grid=history_.grid_history_state();
    data.scene_revision=history_.current()->revision;data.grid_start=history_.current()->grid_line_start;
    return encode_game_state(data,assets::crc32(rom_.bytes()));
}
std::unique_ptr<GameSession> GameSession::restored_state(std::span<const std::uint8_t> bytes) const {
    if(!state_available()) throw std::runtime_error("Close runtime options or finish cartridge handoff before loading a state");
    auto data=decode_game_state(bytes,assets::crc32(rom_.bytes()));
    GameSessionOptions options;options.preferences=preferences();options.stem_executor=stem_executor_;
    auto next=std::make_unique<GameSession>(rom_,symbols_,sink_,"BOOT",std::span<const std::uint8_t>{},options);
    // Restore from the candidate, NOT this->game_: GameSimulation retains ROM
    // and symbol pointers from the caller. Those must survive retiring *this.
    auto game=next->game_.restored_state(data.game);
    if(game->experience()!=cartridge_experience_ || game->runtime_options_open())
        throw std::runtime_error("Native state belongs to a different cartridge or transient menu");
    next->audio_.load_state(data.audio);
    next->game_.swap_state(*game);
    next->apply_native_capabilities();
    // A valid cross-platform GAME archive can contain a desktop-only target.
    // Keep the native output bounded without changing VM/SPC cadence.
    const auto rate=next->game_.presentation_fps();
    if(rate!=30 && rate!=60) next->game_.set_presentation_fps(rate<=30?30:60);
    next->pending_audio_=std::move(data.pending_audio);next->audio_phase_=data.audio_phase;
    next->history_.capture();
    next->history_.restore_grid_history(data.grid,data.scene_revision,data.grid_start);
    next->publish_raster();next->suppress_held_=true;
    return next;
}
bool GameSession::toggle_runtime_options() {
    if(failed_ || requested_experience_ || requested_preview_ || requested_settings_reset_ || requested_controller_remap_ || requested_hud_customization_)
        throw std::runtime_error("Cannot open native options during a cartridge handoff");
    if(!game_.toggle_runtime_options()) return false;
    input_.reset();clock_.reset();previous_time_.reset();fraction_=0;suppress_held_=true;reset_hold_.cancel();
    try {history_.capture();history_.reset_interpolation();publish_raster();}
    catch(...) {failed_=true;throw;}
    return true;
}
} // namespace starfox::platform::nintendo_3ds
