#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/audio/stem_mixer.hpp"
#include "starfox/platform/nintendo_3ds/audio_pcm.hpp"
#include "starfox/platform/nintendo_3ds/game_menu.hpp"
#include "starfox/platform/nintendo_3ds/game_layers.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"
#include "starfox/compat/bit_cast.hpp"
#include <cctype>

namespace starfox::platform::nintendo_3ds {
static_assert(AudioPcm::rate==audio::Spc700Audio::sample_rate
    && AudioPcm::frames==audio::Spc700Audio::stereo_frames_per_logic_tick);
namespace {
input::ButtonMask gameplay_buttons(input::ButtonMask held,bool swap) noexcept {
    if(!swap) return held;
    const auto pair=[&](input::ButtonMask a,input::ButtonMask b) {
        const auto before=held;
        held=static_cast<input::ButtonMask>(held&~(a|b));
        if(before&a) held|=b;
        if(before&b) held|=a;
    };
    pair(input::a,input::b);pair(input::x,input::y);return held;
}
}
GameSession::GameSession(assets::RomImage rom,assets::SymbolMap symbols,PcmSink sink,
    std::string initial_map,std::span<const std::uint8_t> cartridge_ram,const GameSessionOptions& options)
    :rom_(std::move(rom)),symbols_(std::move(symbols)),
     native_model_trig_(simulation::TrigTables::load(rom_,symbols_)),
     cartridge_experience_(symbols_.find("SPECWEPCNTONE").empty()
         ?simulation::Experience::original:simulation::Experience::starfox_ex),
     decoder_(rom_,symbols_),game_(rom_,symbols_,initial_map,cartridge_ram,true),
     sink_(std::move(sink)),hud_(rom_,symbols_),history_(game_,rom_,symbols_,vr::SceneCameraPolicy::source) {
    if(!sink_) throw std::invalid_argument("3DS game requires a PCM consumer");
    constexpr std::array native_names{"M_BIGZ","M_DEPTHSTAB","M_DEPTHTABLE",
        "M_WIREMODE","M_WOBBLEMODE","M_WABBLEMODE","M_CELMODE",
        "M_SINEOFFSET","M_COLORWARP","M_PROJPNTS"};
    for(unsigned i=0;i<native_names.size();++i) {
        for(auto address:symbols_.find(native_names[i])) if((address>>16)==0x70) {
            native_model_addresses_[i]=address;break;
        }
        if(i<3 && !native_model_addresses_[i])
            throw std::runtime_error(std::string("Missing 3DS viewer symbol: ")+native_names[i]);
    }
    const auto& depth_tables=symbols_.find("DEPTHTABLES");
    if(depth_tables.empty()) throw std::runtime_error("Missing 3DS viewer depth tables");
    native_model_depth_tables_=depth_tables.front();
    game_.set_experience(cartridge_experience_);
    game_.set_timing_mode(simulation::TimingMode::original_speed);
    game_.set_shape_face_counts(&face_counts_);
    // Native cartridge audio remains available; MSU support must not be
    // advertised without a decoder/streaming adapter connected to this owner.
    game_.set_msu1_available(false);
    const auto boot_writes=game_.map().take_apu_port_writes();
    if(!boot_writes.empty()) {
        static_cast<void>(audio_.prime_upload_sequence(boot_writes));
        for(auto& c:initial_map) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        // Match the ordinary runtime's base-driver initialization before a
        // direct stage-bank overlay. Do not queue inaudible startup preroll.
        if(initial_map!="BOOT") for(unsigned tick=0;tick<30;++tick)
            audio_.render_stems_logic_tick({});
        game_.synchronize_apu_output_ports(audio_.output_ports());
    }
    if(options.preferences) {
        const auto& prefs=*options.preferences;
        if(!prefs.hud_layout.valid()) throw std::invalid_argument("Invalid 3DS HUD preferences");
        hud_layout_=prefs.hud_layout;hud_.set_layout(hud_layout_);
        if(prefs.render_fps!=30 && prefs.render_fps!=60)
            throw std::invalid_argument("3DS render FPS must be 30 or 60");
        game_.set_presentation_fps(prefs.render_fps);game_.set_show_fps(prefs.show_fps);
        game_.set_timing_mode(prefs.timing);game_.set_music_volume(prefs.music);game_.set_sfx_volume(prefs.sfx);
        game_.set_language(prefs.language);game_.set_swap_face_buttons(prefs.swap);
        game_.set_god_mode(prefs.god);game_.set_infinite_bombs(prefs.bombs);game_.set_infinite_boost(prefs.boost);
        game_.set_infinite_lives(prefs.lives);game_.set_planet_select_cheat(prefs.planet_cheat);
        game_.set_default_laser(prefs.laser);game_.set_selected_level(prefs.level);
        game_.set_stereo_separation(std::min<std::uint16_t>(64,prefs.separation));
        game_.set_stereo_convergence(prefs.convergence);
    }
    apply_native_capabilities();
    if(options.preview) {
        if(initial_map=="BOOT" || options.start_after_preview)
            throw std::invalid_argument("3DS preview requires a real stage owner, not BOOT/Start");
        // The same real Corneria/chatter reference as the desktop preview,
        // not an empty stage initializer or a prerecorded mono screenshot.
        const bool god=game_.god_mode();game_.set_god_mode(true);
        std::optional<unsigned> first_meter;
        std::uint32_t previous_dialogue{};unsigned dialogues{};
        bool stable=false;
        for(unsigned tick=0;tick<2'400;++tick) {
            if(tick%16==0 && options.preview_progress && !options.preview_progress(tick))
                throw std::runtime_error("3DS preview loading cancelled");
            const auto advance=game_.tick({});
            audio_.render_stems_logic_tick(advance.audio_port_writes);
            game_.synchronize_apu_output_ports(audio_.output_ports());
            static_cast<void>(game_.map().take_msu_register_writes());
            const auto meters=game_.peek_meter_state();const auto dialogue=game_.dialogue_state();
            if(meters.enabled && !first_meter) first_meter=tick;
            if(meters.enabled && dialogue.active && dialogue.text_visible && dialogue.text_address!=previous_dialogue) {
                previous_dialogue=dialogue.text_address;
                if(++dialogues>=4) {
                    for(unsigned settle=0;settle<12;++settle) {
                        const auto next=game_.tick({});
                        audio_.render_stems_logic_tick(next.audio_port_writes);
                        game_.synchronize_apu_output_ports(audio_.output_ports());
                        static_cast<void>(game_.map().take_msu_register_writes());
                    }
                    stable=true;break;
                }
            }
            if(first_meter && tick-*first_meter>=720) {stable=true;break;}
        }
        game_.set_god_mode(god);
        if(!stable) throw std::runtime_error("Cartridge did not produce a stable 3DS preview frame");
        game_.enable_menu_preview();
    }
    start_after_preview_=options.start_after_preview;
    history_.capture();history_.reset_interpolation();publish_raster();
}
void GameSession::apply_native_capabilities() noexcept {
    // A GAME archive carries desktop host preferences as well as the source
    // timeline. PICA uses native LCD geometry and the cartridge's own effects;
    // ignored desktop settings must not survive a successful native restore.
    // These setters touch host preferences only, never cartridge RAM/SPC state.
    game_.set_display_mode(simulation::DisplayMode::standard_4_3);
    game_.set_renderer_mode(simulation::RendererMode::gpu);
    game_.set_render_scale(simulation::RenderScale::scale_1x);
    game_.set_anti_aliasing_mode(simulation::AntiAliasingMode::off);
    game_.set_aa_type(0);game_.set_integer_scaling(false);
    game_.set_enhanced_graphics(false);game_.set_smooth_polys(false);
    game_.set_rtx_lighting(false);game_.set_two_d_filter(simulation::TwoDFilterMode::off);
    game_.set_effect(0);game_.set_world_effect(0);game_.set_bloom(0);game_.set_bloom_2d(0);
    game_.set_model_smoothing(0);game_.set_enhanced_shadows(false);game_.set_ray_tracing(false);
    game_.set_reflective_surfaces(0);game_.set_chromatic_aberration(0);game_.set_hdr_effect(0);
    game_.set_material(0);game_.set_manipulation(0);game_.set_extra_effects({});game_.set_environment({});
    game_.set_global_enhancements(0);game_.set_scene_enhancements(0);game_.set_depth_enhancements(0);
    game_.set_particle_enhancements(0);game_.set_phosphor_persistence(0);game_.set_adaptive_exposure(0);
    game_.set_water_caustics(0);game_.set_camera_response(0);game_.set_volumetric_fog(0);game_.set_motion_blur(0);
    game_.set_dlss_mode(0);game_.set_dlss45_mode(0);game_.set_fsr1_mode(0);game_.set_fsr1_menu(false);
    game_.set_vsync(false);game_.set_msu1_available(false);
}
GamePreferences GameSession::preferences() const noexcept {
    return {game_.timing_mode(),game_.music_volume(),game_.sfx_volume(),game_.language(),
        game_.default_laser(),game_.selected_level(),game_.swap_face_buttons(),game_.god_mode(),
        game_.infinite_bombs(),game_.infinite_boost(),game_.infinite_lives(),game_.planet_select_cheat(),
        game_.stereo_separation(),game_.stereo_convergence(),
        static_cast<std::uint8_t>(game_.presentation_fps()),game_.show_fps(),hud_layout_};
}
void GameSession::prepare_pace_shapes() {
    if(game_.timing_mode()!=simulation::TimingMode::original_speed) return;
    // Same source draw list/shape counts as desktop, before the pace decision;
    // never estimate original timing from the current eye's culling or GPU work.
    for(auto handle:game_.draw_order()) {
        if(!game_.objects().is_active(handle)) continue;
        const auto shape=game_.objects().at(handle).shape;
        if(face_counts_.contains(shape) || invalid_shapes_.contains(shape)) continue;
        try {face_counts_.emplace(shape,static_cast<std::uint32_t>(decoder_.decode(shape).faces.size()));}
        catch(const std::exception&) {invalid_shapes_.insert(shape);}
    }
}
void GameSession::publish_raster() {
    STARFOX_3DS_FRAME_PHASE(raster);
    auto next=std::make_shared<GameRasterSnapshot>();
    const auto& ppu=game_.map().ppu_state();
    // At most one immutable PPU copy per host advance, never per eye. Reuse
    // unchanged video storage and completed-model snapshots where possible.
    if(ppu.tunnel_scene && history_.is_final_vortex_sky(game_.map().background(),ppu.background_mode)) {
        // Retain this video phase's VRAM/OAM/HDMA/palette, not the older scene
        // PPU. The final room's stale INATUNNEL bit must not pin its sky to
        // screen depth or clamp the surround to corridor edge texels.
        auto presented=std::make_shared<simulation::SnesPpuState>(ppu);
        presented->tunnel_scene=false;
        if(raster_ && *raster_->ppu==*presented) next->ppu=raster_->ppu;
        else next->ppu=std::move(presented);
    }
    else if(raster_ && *raster_->ppu==ppu) next->ppu=raster_->ppu;
    else if(history_.current()->ppu && *history_.current()->ppu==ppu) next->ppu=history_.current()->ppu;
    else next->ppu=std::make_shared<const simulation::SnesPpuState>(ppu);
    next->brightness=game_.map().display_brightness();
    next->circle=game_.circle_effect_state();next->wipe=game_.window_wipe_state();
    next->colour_math=game_.colour_math_effect_state();
    next->boss_roll=game_.boss_roll_active();
    next->stage_hud=game_.stage_results_state().visible;
    next->final_score=game_.final_score_active();
    const auto& native=game_.map().native_model_draw();
    const auto flow=game_.flow_state();
    if(native.active && native.shape && (flow==simulation::GameFlowState::continue_choice
        || flow==simulation::GameFlowState::ex_pregame_menu)) {
        const auto word=[&](unsigned i) {
            return native_model_addresses_[i]?game_.map().peek_ram_word(native_model_addresses_[i]).value():std::uint16_t{};
        };
        const auto byte=[&](unsigned i) {
            return native_model_addresses_[i]?game_.map().peek_ram_byte(native_model_addresses_[i]).value():std::uint8_t{};
        };
        auto& viewer=next->native_model.emplace();viewer.shape=native.shape;viewer.colour_table=native.colour_table;
        auto& pose=viewer.pose;pose.x=native.x;pose.y=native.y;
        // The source shoulder handler updates M_BIGZ after the FX launch.
        // Peek, never read_native_word: even a bus read changes VM latch state.
        pose.z=starfox::bit_cast<std::int16_t>(word(0));
        pose.pitch=(native.rotation_x&255U)<<8;pose.yaw=(native.rotation_y&255U)<<8;
        pose.roll=(native.rotation_z&255U)<<8;
        pose.rotation_matrix=simulation::rotation_matrix_q15(native_model_trig_,
            starfox::bit_cast<std::int16_t>(std::uint16_t(pose.pitch)),
            starfox::bit_cast<std::int16_t>(std::uint16_t(pose.yaw)),
            starfox::bit_cast<std::int16_t>(std::uint16_t(pose.roll)));
        pose.use_rotation_matrix=true;pose.scale=1; // MSHOWOBJ3 bypasses Huge Models/normal LODs.
        pose.vanish_x=native.vanish_x;pose.vanish_y=native.vanish_y;
        pose.animation_frame=native.animation_frame;pose.colour_frame=native.colour_frame;
        pose.wireframe_mode=byte(3);pose.wobble_mode=byte(4);pose.wave_mode=byte(5)!=0;
        pose.cel_mode=byte(6)!=0;pose.wave_offset=starfox::bit_cast<std::int16_t>(word(7));
        pose.colour_warp=word(8)!=0;
        if(native_model_addresses_[9]) pose.projected_points_address=std::uint16_t(native_model_addresses_[9]);
        render::apply_source_depth_tables(rom_,native_model_depth_tables_,word(1),word(2),0,pose);
    }
    hud_frame_=hud_.capture(game_);hud_.update(hud_frame_);
    raster_=std::move(next);
}
void GameSession::finish_controller_remap() {
    if(!requested_controller_remap_) throw std::logic_error("No native controller editor to close");
    requested_controller_remap_=false;input_.reset();clock_.reset();previous_time_.reset();fraction_=0;
    suppress_held_=true;reset_hold_.cancel();history_.reset_interpolation();
}
void GameSession::finish_hud_customization(std::optional<CockpitLayout> applied) {
    if(!requested_hud_customization_) throw std::logic_error("No native HUD editor to close");
    if(applied && !applied->valid()) throw std::invalid_argument("Invalid native HUD edit");
    if(applied) {
        const auto old=hud_layout_;hud_layout_=*applied;hud_.set_layout(hud_layout_);
        try {publish_raster();}
        catch(...) {hud_layout_=old;hud_.set_layout(old);throw;}
    }
    requested_hud_customization_=false;input_.reset();clock_.reset();previous_time_.reset();fraction_=0;
    suppress_held_=true;reset_hold_.cancel();history_.reset_interpolation();
}
GameAdvance GameSession::advance(std::int64_t time,input::ButtonMask held,bool focused,
    std::optional<input::ButtonMask> mapped_gameplay) {
    STARFOX_3DS_FRAME_PHASE(advance);
    if(failed_) throw std::runtime_error("Reconstruct 3DS game after a failed source/audio tick");
    if(time<0) throw std::invalid_argument("Invalid 3DS monotonic frame time");
    GameAdvance result;result.requested_experience=requested_experience_;
    result.requested_preview=requested_preview_;result.start_after_preview=start_after_preview_;
    result.requested_settings_reset=requested_settings_reset_;
    result.requested_controller_remap=requested_controller_remap_;
    result.requested_hud_customization=requested_hud_customization_;
    if(requested_experience_ || requested_preview_ || requested_settings_reset_ || requested_controller_remap_ || requested_hud_customization_) return result;
    if(!focused) {
        reset_hold_.cancel();
        previous_time_.reset();clock_.reset();input_.reset();fraction_=0;
        suppress_held_=true;history_.reset_interpolation();return result;
    }
    // Home/sleep/resume must not act as a fresh held Start/A press. Wait for
    // release; pending pre-suspend APU events/partial 20 Hz cadence survive.
    auto gameplay=mapped_gameplay.value_or(held);
    const bool any_held=(held|gameplay)!=0;
    if(suppress_held_) {
        if(!(held|gameplay)) suppress_held_=false;
        else held=gameplay=0;
    }
    // Check continuously at the host input clock, including duplicate raster
    // times. A rewind, release, Home/sleep or leaving setup cancels the hold.
    const bool reset_eligible=game_.in_setup_menu() && (!previous_time_ || time>=*previous_time_);
    const bool reset_was_active=reset_hold_.active();
    if(reset_hold_.update(reset_eligible,gameplay,time)) {
        requested_settings_reset_=result.requested_settings_reset=true;
        clock_.reset();fraction_=result.raster_fraction=0;input_.reset();
        history_.reset_interpolation();return result;
    }
    if(reset_was_active && !reset_hold_.active()) {
        suppress_held_=true;held=gameplay=0;input_.reset();
    }
    // Remapped shoulders can be physical A/B/directions. While their reset
    // chord is held, do not also open a submenu/change experience/navigate.
    if(game_.in_setup_menu() && reset_hold_.active()) {held=0;input_.reset();}
    if(!game_.in_setup_menu()) held=gameplay_buttons(gameplay,game_.swap_face_buttons());
    input_.sample(held); // Also retain quick input on a duplicate display time.
    if(previous_time_ && time==*previous_time_) {
        result.duplicate=true;result.raster_fraction=fraction_;return result;
    }
    if(!previous_time_ || time<*previous_time_) {
        const bool rewound=previous_time_ && time<*previous_time_;
        previous_time_=time;clock_.reset();fraction_=0;history_.reset_interpolation();
        if(rewound) {input_.reset();suppress_held_=any_held;}
        return result;
    }
    const auto batch=clock_.advance(std::chrono::nanoseconds(time-*previous_time_));previous_time_=time;
    result.time_clamped=batch.time_was_clamped;
    fraction_=result.raster_fraction=batch.interpolation_alpha;
    try {
        for(unsigned phase=0;phase<batch.simulation_steps;++phase) {
            {
                STARFOX_3DS_FRAME_PHASE(video);
                prepare_pace_shapes();game_.present_frame();++result.video_phases;
            }
            if(game_.logic_tick_ready()) {
                const bool runtime=game_.runtime_options_open(),paused=game_.paused();
                auto controls=GameMenu::filter(game_,input_.consume());
                const auto previous_output_rate=game_.presentation_fps();
                const bool native_fps_menu=game_.in_setup_menu()
                    && game_.pregame_page()==simulation::PregamePage::main;
                const auto editor=game_.in_setup_menu()
                    ?GameMenu::editor(game_.pregame_page(),game_.pregame_selection(),controls)
                    :GameMenuEditor::none;
                if(editor!=GameMenuEditor::none) {
                    if(editor==GameMenuEditor::hud)
                        requested_hud_customization_=result.requested_hud_customization=true;
                    else requested_controller_remap_=result.requested_controller_remap=true;
                    // Consume the host action, but send the same tick's Up/Down
                    // to the source. Otherwise the editor opens on the row we
                    // left or returns to a cursor that never reached its row.
                    // Finish the ordinary VM/SPC phase; partial audio survives.
                    constexpr auto navigation=input::ButtonMask(input::up|input::down);
                    controls.held&=navigation;controls.pressed&=navigation;controls.released&=navigation;
                    input_.reset();reset_hold_.cancel();
                }
                if(start_after_preview_ && game_.in_setup_menu() && !game_.menu_preview()) {
                    // A single source Start action, not a jump into gameplay or
                    // a held Start that could pause the newly launched stage.
                    controls.held|=input::start;controls.pressed|=input::start;start_after_preview_=false;
                }
                const auto tick=[&] {
                    STARFOX_3DS_FRAME_PHASE(logic);
                    return game_.tick(controls);
                }();++result.logic_ticks;
                // Let the source own navigation, Start and confirmation/axis
                // release guards. Replace only an output-rate change it really
                // applied, not an action guessed from the pre-tick cursor or
                // raw pressed bits. The native LCD's list is just 30/60.
                // A combined Start/action may also leave runtime options in
                // this tick, after the source has already changed its FPS row.
                if(native_fps_menu && game_.presentation_fps()!=previous_output_rate)
                    game_.set_presentation_fps(previous_output_rate==60?30:60);
                // State/import bounds remain independent of menu activation.
                const auto output_rate=game_.presentation_fps();
                if(output_rate!=30 && output_rate!=60) game_.set_presentation_fps(output_rate<=30?30:60);
                // The native eye projector has a bounded 64-world-unit range,
                // unlike desktop stereo displays' larger separation overrides.
                if(game_.stereo_separation()>64) game_.set_stereo_separation(64);
                if(!runtime) pending_audio_.insert(pending_audio_.end(),
                    tick.audio_port_writes.begin(),tick.audio_port_writes.end());
                // No MSU consumer is enabled. Retire the unused register stream
                // rather than allowing an unbounded vector on original hardware.
                static_cast<void>(game_.map().take_msu_register_writes());
                if(runtime && !game_.runtime_options_open()) input_.reset();
                { STARFOX_3DS_FRAME_PHASE(capture);history_.capture(); }
                if(paused || game_.paused()) history_.reset_interpolation();
                if(game_.experience()!=cartridge_experience_) {
                    requested_experience_=game_.experience();
                    result.requested_experience=requested_experience_;
                    clock_.reset();fraction_=result.raster_fraction=0;
                    history_.reset_interpolation();break;
                }
                if(!runtime && !game_.runtime_options_open() && game_.in_setup_menu()
                    && (game_.preview_requested()!=game_.menu_preview() || game_.preview_start_requested())) {
                    start_after_preview_=game_.preview_start_requested();
                    requested_preview_=game_.preview_requested() && !start_after_preview_;
                    result.requested_preview=requested_preview_;result.start_after_preview=start_after_preview_;
                    clock_.reset();fraction_=result.raster_fraction=0;history_.reset_interpolation();break;
                }
            }
            if(!game_.runtime_options_open() && ++audio_phase_==3) {
                STARFOX_3DS_FRAME_PHASE(audio);
                audio_.render_stems_logic_tick(pending_audio_);
                audio::mix_stems(audio_.last_music_samples(),audio_.last_effect_samples(),
                    game_.music_volume(),game_.sfx_volume(),mixed_);
                sink_(mixed_);game_.synchronize_apu_output_ports(audio_.output_ports());
                pending_audio_.clear();audio_phase_=0;++result.audio_blocks;
            }
            if(requested_controller_remap_ || requested_hud_customization_) {
                clock_.reset();fraction_=result.raster_fraction=0;
                history_.reset_interpolation();break;
            }
        }
        if(result.video_phases) {
            if(history_.current()->scene_epoch!=game_.scene_revision()
                || history_.current()->flow!=game_.flow_state()) {
                history_.capture();history_.reset_interpolation();
            }
            publish_raster();
        }
    } catch(...) {failed_=true;throw;}
    return result;
}
GamePresentation GameSession::presentation(float slider,bool hardware,const StereoSettings& settings) const {
    if(failed_) throw std::runtime_error("Cannot present a failed 3DS game session");
    GamePresentation result{plan_frame(slider,hardware,hud_frame_.routing.screen,settings),
        history_.previous(),history_.current(),raster_,game_.logic_interpolation_alpha(fraction_),
        GameHud::top_selection(hud_frame_),hud_.view()};
    // The source menu/HUD routing remains intact. A verified EX surround may
    // use the physical slider, while its separate screen-space text stays at
    // zero disparity. Use the same source policy as the actual painter owner.
    if(result.current->flow==simulation::GameFlowState::ex_pregame_menu && native_panorama_scene(result))
        result.plan=plan_frame(slider,hardware,ScreenUse::menu_preview,settings);
    return result;
}
} // namespace starfox::platform::nintendo_3ds
