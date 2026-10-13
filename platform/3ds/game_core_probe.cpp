// Actual VM/SPC/HUD link and SD bring-up, NOT the completed game renderer.
#include "native_display.hpp"
#include "native_audio.hpp"
#include "native_stem_executor.hpp"
#include "companion_manifest.hpp"
#include "starfox/platform/nintendo_3ds/game_assets.hpp"
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/platform/nintendo_3ds/game_models.hpp"
#include "starfox/platform/nintendo_3ds/game_menu.hpp"
#include "starfox/platform/nintendo_3ds/game_storage.hpp"
#include "starfox/platform/nintendo_3ds/game_remap.hpp"
#include "starfox/platform/nintendo_3ds/game_hud_editor.hpp"
#include "starfox/platform/nintendo_3ds/game_quick_menu.hpp"
#include "starfox/platform/nintendo_3ds/presentation_clock.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"
#include "starfox/platform/nintendo_3ds/startup_trace.hpp"
#include "starfox/assets/bps.hpp"
#if defined(STARFOX_3DS_CORE_PICA)
#include "native_gpu.hpp"
#include "native_mono.hpp"
#include "pica_scene_shader.hpp"
#include "starfox/platform/nintendo_3ds/game_layers.hpp"
#include "starfox/platform/nintendo_3ds/game_dots.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include "starfox/platform/nintendo_3ds/pica_colour.hpp"
#include "starfox/platform/nintendo_3ds/game_effects.hpp"
#include "starfox/platform/nintendo_3ds/pica_window.hpp"
#endif
#include <3ds.h>
#include <fstream>

namespace {
namespace ctr=starfox::platform::nintendo_3ds;
constexpr auto companion_path="sdmc:/3ds/starfox-enhanced/Starfox-Assets.BIN";
#if defined(STARFOX_3DS_INDEXED_MONO)
constexpr bool indexed_mono_upload=true;
#else
constexpr bool indexed_mono_upload=false;
#endif
std::int64_t monotonic_time() {
    const auto ticks=svcGetSystemTick();
    // Split before multiplication: uptime must not overflow nanosecond math.
    return static_cast<std::int64_t>((ticks/SYSCLOCK_ARM11)*1'000'000'000ULL
        +((ticks%SYSCLOCK_ARM11)*1'000'000'000ULL)/SYSCLOCK_ARM11);
}
// libctru invokes APT hooks synchronously from aptMainLoop, on the owner thread.
// Preserve source/audio cadence but rebase input/time around Home and sleep.
class Suspension {
public:
    Suspension(ctr::GameSession& session,ctr::NativeAudio& audio,std::function<void()> checkpoint,std::function<bool()> paused)
        :session_(session),audio_(audio),checkpoint_(std::move(checkpoint)),paused_(std::move(paused)) {
        aptHook(&cookie_,callback,this);
    }
    ~Suspension() {aptUnhook(&cookie_);}
    const std::string& error() const {return error_;}
private:
    static void callback(APT_HookType event,void* pointer) noexcept {
        auto& self=*static_cast<Suspension*>(pointer);
        try {
            if(event==APTHOOK_ONSUSPEND || event==APTHOOK_ONSLEEP || event==APTHOOK_ONEXIT) {
                self.audio_.pause(true);
                self.session_.advance(monotonic_time(),0,false);
                self.checkpoint_();
            } else if(event==APTHOOK_ONRESTORE || event==APTHOOK_ONWAKEUP)
                self.audio_.pause(self.paused_());
        } catch(const std::exception& error) {self.error_=error.what();}
    }
    ctr::GameSession& session_;ctr::NativeAudio& audio_;std::function<void()> checkpoint_;
    std::function<bool()> paused_;
    aptHookCookie cookie_{};std::string error_;
};
#if defined(STARFOX_3DS_PROFILE_FRAMES)
class NativeProfileFrame {
public:
    NativeProfileFrame(ctr::FrameProfile& profile,ctr::FrameProfileBuffer& buffer,
        const std::unique_ptr<ctr::GameSession>& session,const unsigned& background,
        const unsigned& rasters,const unsigned& logic,const unsigned& blocks) noexcept
        :profile_(profile),buffer_(buffer),session_(session),background_(background),
         rasters_(rasters),logic_(logic),blocks_(blocks),begin_(profile.now()) {}
    ~NativeProfileFrame() noexcept {
        const auto end=profile_.now();
        profile_.record(ctr::FramePhase::frame,begin_,end);
        buffer_.capture(profile_,end,session_?int(session_->game().flow_state()):-1,
            background_,rasters_,logic_,blocks_);
    }
private:
    ctr::FrameProfile& profile_;ctr::FrameProfileBuffer& buffer_;
    const std::unique_ptr<ctr::GameSession>& session_;
    const unsigned& background_;const unsigned& rasters_;const unsigned& logic_;const unsigned& blocks_;
    std::uint64_t begin_{};
};
#endif
}

int main() {
    using namespace starfox;
    ctr::NativeDisplay display;
    // APT is initialized by the display. The joined worker outlives every
    // source owner, including preview/experience/reset/state replacements.
    ctr::NativeStemExecutor stem_executor;
    ctr::NativeStemExecutor effect_stem_executor(ctr::NativeStemExecutor::Role::budgeted_effects);
    ctr::Canvas top(ctr::top_width),lower;
    std::unique_ptr<ctr::NativeAudio> audio;
    std::unique_ptr<ctr::GameSession> session;
    std::unique_ptr<ctr::GameModels> models;
    std::unique_ptr<ctr::GameMenu> menu;
    std::unique_ptr<Suspension> suspension;
#if defined(STARFOX_3DS_CORE_PICA)
    std::unique_ptr<ctr::NativeMono> mono;
    std::unique_ptr<ctr::NativeGpu> gpu;
    std::unique_ptr<ctr::GameLayers> layers;
    std::unique_ptr<ctr::GameDots> dots;
    ctr::PicaComposite composite;ctr::GameEffects effects;
#if !defined(STARFOX_3DS_TEST_PLAYER)
    // Explicitly label this experimental source-scene test. This small host
    // strip is not a replacement pre-game menu or part of source colour math.
    ctr::Canvas label_canvas(ctr::top_width);
    label_canvas.clear({8,15,28});label_canvas.text(4,4,"NATIVE CHECK / SELECT+Y MENU / FLOW PENDING",{240,181,86});
    constexpr std::array<ctr::Point3,4> label_corners{{{0,0,0},{400,0,0},{400,16,0},{0,16,0}}};
    constexpr std::array<std::array<float,2>,4> label_uv{{{0,0},{1,0},{1,1},{0,1}}};
    std::array<ctr::PicaVertex,6> label_vertices{};unsigned label_index=0;
    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) label_vertices[label_index++]={label_corners[corner],{1,1,1,1},label_uv[corner]};
    ctr::PicaDraw label_draw{0,6,0,ctr::pica_identity,ctr::PicaSpace::screen,false,false,false};label_draw.source_layer=0;
    const ctr::PicaImage label_image{label_canvas.view().pixels.first(400*16*3),400,16,400*3,3};
#endif
#endif
    std::string error;
    auto experience=simulation::Experience::original;
    bool running=false;input::ButtonMask previous{};
    unsigned rasters{},logic{},blocks{};
#if defined(STARFOX_3DS_PROFILE_FRAMES)
    // Separate opt-in diagnostic file: never replace settings, saves or assets.
    // Window count is bounded; unavailable/full SD output disables logging.
    ctr::FrameProfile frame_profile([]() noexcept -> std::uint64_t {return svcGetSystemTick();},SYSCLOCK_ARM11);
    ctr::FrameProfileBuffer profile_buffer;
    ctr::ScopedFrameProfileActivation profile_activation(frame_profile);
    std::ofstream profile_stream(std::string("sdmc:/3ds/starfox-enhanced/native-frame-profile-")
        +std::to_string(svcGetSystemTick())+".csv");
    std::ofstream startup_stream(std::string("sdmc:/3ds/starfox-enhanced/native-startup-")
        +std::to_string(svcGetSystemTick())+".csv");
    ctr::StartupTrace startup_trace(startup_stream,[]() noexcept -> std::uint64_t {return svcGetSystemTick();});
    ctr::ScopedStartupTraceActivation startup_activation(startup_trace);
    if(stem_executor.parallel_available()) STARFOX_3DS_STARTUP_MARK(audio_worker_parallel);
    else STARFOX_3DS_STARTUP_MARK(audio_worker_serial);
    if(effect_stem_executor.parallel_available()) STARFOX_3DS_STARTUP_MARK(audio_effects_worker_parallel);
    else STARFOX_3DS_STARTUP_MARK(audio_effects_worker_serial);
    unsigned profile_background{};
#endif
    ctr::PresentationClock render_clock;
    ctr::PresentationRate rendered_rate;
    bool first_load=true;
    std::array<std::vector<std::uint8_t>,2> cartridge_ram;
    std::array<std::uint32_t,2> bank_crc{};
    ctr::GameStorage storage("sdmc:/3ds/starfox-enhanced",ctr::companion_manifest);
    ctr::GameSaveData saved;
    ctr::GameBindings bindings;
    std::unique_ptr<ctr::GameRemap> remap;
    std::unique_ptr<ctr::GameHudEditor> hud_editor;
    std::unique_ptr<ctr::GameQuickMenu> quick;
    std::unique_ptr<ctr::GameStateStorage> states;
    unsigned state_slot{};
    std::string save_warning;
    bool save_enabled=false;
    std::uint32_t cartridge_crc{};
    std::int64_t last_save_check{};
    try {
        const auto& loaded=storage.load();saved=loaded.data;experience=saved.experience;
        bindings=saved.bindings;
        save_enabled=loaded.writable;save_warning=loaded.warning;
        cartridge_ram[unsigned(simulation::Experience::starfox_ex)]=saved.ex_sram;
        bank_crc[unsigned(simulation::Experience::starfox_ex)]=saved.ex_rom_crc;
    } catch(const std::exception& failure) {save_warning=failure.what();}
    const auto checkpoint=[&](bool force) {
        if(!session || !save_enabled) return;
        const auto now=monotonic_time();
        // No disk activity per eye/frame. Unchanged data does not write, and
        // ordinary checks are bounded to once per second. APT/exit/handoffs
        // checkpoint immediately before the source/SD owners are retired.
        if(!force && now-last_save_check<1'000'000'000LL) return;
        last_save_check=now;
        STARFOX_3DS_FRAME_PHASE(checkpoint);
        try {
            auto next=saved;next.experience=session->game().experience();
            next.preferences=session->preferences();
            next.bindings=bindings;
            // Preview is session-only on 3DS. Persisting ON before its first
            // native frame completed trapped subsequent launches in loading.
            // Retain the compatible field, but never automatically preroll at
            // boot or discard any of the player's other settings / EX SRAM.
            next.preview=false;
            if(session->cartridge_experience()==simulation::Experience::starfox_ex) {
                const auto ram=session->cartridge_ram();
                next.ex_sram.assign(ram.begin(),ram.end());next.ex_rom_crc=cartridge_crc;
            }
            storage.save(next);saved=std::move(next);save_warning=storage.current().warning;
        } catch(const std::exception& failure) {
            save_warning=std::string(failure.what())+"\nSD saving disabled until restart. Existing valid slot preserved.";
            save_enabled=false;
        }
    };
    const auto release_owners=[&] {
        render_clock.reset();rendered_rate.reset();
        remap.reset();hud_editor.reset();quick.reset();states.reset();
        if(audio) audio->pause(true);
#if defined(STARFOX_3DS_CORE_PICA)
        mono.reset();gpu.reset();layers.reset();dots.reset();
#endif
        // All GPU views/asset references and the APT hook retire first.
        menu.reset();suspension.reset();models.reset();session.reset();audio.reset();
    };
    const auto load=[&](const char* map,ctr::GameSessionOptions options={}) {
#if defined(STARFOX_3DS_PROFILE_FRAMES)
        startup_trace.restart_source_window();
#endif
        STARFOX_3DS_STARTUP_MARK(load_begin);
        STARFOX_3DS_STARTUP_MARK(owner_retirement);
        release_owners();
        STARFOX_3DS_STARTUP_MARK(owner_retirement_complete);
        top.clear({8,15,28});lower.clear({8,15,28});
        top.text(24,100,"RENDERING",{240,181,86},2);
        lower.text(12,16,options.preview?"PREPARING REAL CARTRIDGE PREVIEW":"LOADING CARTRIDGE AND SETTINGS",{183,224,240});
        display.present(ctr::plan_frame(0,false,ctr::ScreenUse::setup),top.view(),{},lower.view());
        if(running) rendered_rate.completed(monotonic_time());
        std::ifstream file(companion_path,std::ios::binary);
        STARFOX_3DS_STARTUP_MARK(companion_read);
        auto cartridge=ctr::read_game_cartridge(file,ctr::companion_manifest,experience);
        cartridge_crc=assets::crc32(cartridge.rom.bytes());
        auto& bank=cartridge_ram[unsigned(experience)];
        if(experience==simulation::Experience::starfox_ex && !bank.empty() && bank_crc[unsigned(experience)]!=cartridge_crc) {
            // Do not feed SRAM to a different cartridge or overwrite its save
            // with that cartridge's first-boot defaults.
            bank.clear();save_enabled=false;
            save_warning="EX save belongs to a different cartridge.\nSD saving disabled; back up journal files before recovery.";
        }
        STARFOX_3DS_STARTUP_MARK(audio_initialize);
        audio=std::make_unique<ctr::NativeAudio>();
        options.preview_progress=[&](unsigned) {return display.poll().running;};
        options.stem_executor=&stem_executor;
        options.frame_audio_stem_executor=&effect_stem_executor;
        STARFOX_3DS_STARTUP_MARK(session_initialize);
        session=std::make_unique<ctr::GameSession>(std::move(cartridge.rom),std::move(cartridge.symbols),
            [&](auto pcm){audio->submit(pcm);},map,cartridge_ram[unsigned(experience)],options);
        models=std::make_unique<ctr::GameModels>(session->rom(),session->symbols());
        menu=std::make_unique<ctr::GameMenu>(session->rom(),session->symbols());
        suspension=std::make_unique<Suspension>(*session,*audio,[&]{
            render_clock.reset();rendered_rate.reset();
            if(remap) remap->suspend();
            if(hud_editor) hud_editor->suspend();
            if(quick) quick->suspend();
            checkpoint(true);
        },[&]{return remap || hud_editor || quick || session->game().runtime_options_open();});
        states=std::make_unique<ctr::GameStateStorage>("sdmc:/3ds/starfox-enhanced",ctr::companion_manifest,cartridge_crc);
#if defined(STARFOX_3DS_CORE_PICA)
        layers=std::make_unique<ctr::GameLayers>(indexed_mono_upload);
        dots=std::make_unique<ctr::GameDots>(session->rom(),session->symbols());
        // Preview OFF is a host menu, not a PICA world. Initialize the native
        // GPU only when a real preview/game/editor needs it.
#endif
        running=true;rasters=logic=blocks=0;error.clear();
#if defined(STARFOX_3DS_PROFILE_FRAMES)
        profile_background=0;
#endif
        // The initiating physical A/Start belongs to loading, not the new menu.
        session->advance(monotonic_time(),0,false);
        STARFOX_3DS_STARTUP_MARK(load_complete);
    };
#if defined(STARFOX_3DS_CORE_PICA)
    const auto ensure_gpu=[&] {
        mono.reset();
        if(!gpu) {
            // Damage/wipe effects switch GPU owners repeatedly. Preserve the
            // first-init freeze receipt without rearming SD flushes each time.
            STARFOX_3DS_STARTUP_FIRST_GPU();
            STARFOX_3DS_STARTUP_MARK(gpu_initialize);
            gpu=std::make_unique<ctr::NativeGpu>(ctr::pica_scene_shader);
            STARFOX_3DS_STARTUP_MARK(gpu_initialize_complete);
        }
    };
#endif
    const auto present_quick=[&] {
#if defined(STARFOX_3DS_CORE_PICA)
        ensure_gpu();
        gpu->present(quick->frame(),quick->lower_view());
#else
        display.present(ctr::plan_frame(0,false,ctr::ScreenUse::setup),quick->upper_view(),{},quick->lower_view());
#endif
    };
    while(true) {
#if defined(STARFOX_3DS_PROFILE_FRAMES)
        startup_trace.begin_frame();
        NativeProfileFrame profile_frame(frame_profile,profile_buffer,session,profile_background,rasters,logic,blocks);
#endif
        STARFOX_3DS_STARTUP_MARK(input_poll);
        const auto controls=display.poll();if(!controls.running) break;
        STARFOX_3DS_STARTUP_MARK(input_poll_complete);
        if(!hud_editor && (controls.held&(input::select|input::start))==(input::select|input::start)) break;
        const auto pressed=static_cast<input::ButtonMask>(controls.held&~previous);previous=controls.held;
        try {
            if(!running && (pressed&input::x)) experience=experience==simulation::Experience::original
                ?simulation::Experience::starfox_ex:simulation::Experience::original;
            if(first_load || (!running && (pressed&(input::a|input::y)))) {
                first_load=false;
                ctr::GameSessionOptions options;options.preferences=saved.preferences;
                // Legacy saves with Preview ON also recover through the menu.
                // Y remains the explicit direct-stage diagnostic shortcut.
                options.preview=false;
                const auto initial_map=(pressed&input::y)?"LEVEL1_1":"BOOT";
                load(initial_map,options);
            }
            if(running) {
                if(!suspension->error().empty()) throw std::runtime_error(suspension->error());
                const ctr::PadSample pad{controls.physical,controls.circle_x,controls.circle_y};
                if(hud_editor) {
                    hud_editor->update(pad,{controls.touching,controls.touch_x,controls.touch_y});
                    if(!hud_editor->active()) {
                        session->finish_hud_customization(hud_editor->applied()?std::optional(hud_editor->layout()):std::nullopt);
                        hud_editor.reset();render_clock.reset();rendered_rate.reset();
                        audio->pause(session->game().runtime_options_open());checkpoint(true);continue;
                    }
#if defined(STARFOX_3DS_CORE_PICA)
                    ensure_gpu();
                    gpu->present(hud_editor->frame(),hud_editor->lower_view());
#else
                    display.present(ctr::plan_frame(0,false,ctr::ScreenUse::setup),hud_editor->upper_view(),{},hud_editor->lower_view());
#endif
                    continue; // No cartridge or SPC service until editor acknowledgment.
                }
                if(remap) {
                    remap->update(pad);bindings=remap->bindings();
                    if(!remap->active()) {
                        session->finish_controller_remap();
                        // Returning to runtime Options must keep queued NDSP
                        // playback paused, like the HUD editor return. BOOT's
                        // ordinary pre-game menu still resumes its own audio.
                        audio->pause(session->game().runtime_options_open());
                        checkpoint(true);remap.reset();continue;
                    }
                    checkpoint(false);
#if defined(STARFOX_3DS_CORE_PICA)
                    ensure_gpu();
                    gpu->present(remap->frame(),remap->lower_view());
#else
                    display.present(ctr::plan_frame(0,false,ctr::ScreenUse::setup),remap->upper_view(),{},remap->lower_view());
#endif
                    continue; // No source ticking or world preparation in the editor.
                }
                constexpr auto quick_chord=input::ButtonMask(input::select|input::y);
                if(!quick && (controls.physical&quick_chord)==quick_chord && (pressed&quick_chord)) {
                    render_clock.reset();rendered_rate.reset();
                    session->advance(monotonic_time(),0,false);audio->pause(true);
                    quick=std::make_unique<ctr::GameQuickMenu>();
                    quick->open(state_slot,session->state_available(),
                        !session->game().in_setup_menu() || session->game().runtime_options_open());
                    quick->set_info(states->load(state_slot).info);
                }
                if(quick) {
                    const auto before=quick->slot();quick->update(pad);
                    state_slot=quick->slot();
                    if(before!=state_slot) quick->set_info(states->load(state_slot).info);
                    const auto action=quick->take_action();
                    if(action==ctr::QuickAction::resume || action==ctr::QuickAction::options) {
                        if(action==ctr::QuickAction::options && !session->game().runtime_options_open()) session->toggle_runtime_options();
                        session->advance(monotonic_time(),0,false);quick.reset();
                        audio->pause(session->game().runtime_options_open());continue;
                    }
                    if(action==ctr::QuickAction::save) {
                        quick->message("SAVING STATE...");present_quick();
                        try {
                            const auto bytes=session->save_state();
                            states->save(state_slot,bytes);quick->set_info(states->current(state_slot));
                            quick->message("STATE SAVED / PREVIOUS GENERATION KEPT");
                        } catch(const std::exception& failure) {quick->message(failure.what());}
                    }
                    if(action==ctr::QuickAction::load) {
                        quick->message("READING / VALIDATING STATE...");present_quick();
                        std::unique_ptr<ctr::GameSession> next;
                        std::unique_ptr<ctr::GameModels> next_models;
                        std::unique_ptr<ctr::GameMenu> next_menu;
                        std::unique_ptr<Suspension> next_hook;
#if defined(STARFOX_3DS_CORE_PICA)
                        std::unique_ptr<ctr::GameLayers> next_layers;
                        std::unique_ptr<ctr::GameDots> next_dots;
#endif
                        try {
                            auto saved_state=states->load(state_slot);
                            quick->set_info(saved_state.info);
                            if(!saved_state.info.found) throw std::runtime_error("No compatible saved state in this slot");
                            next=session->restored_state(saved_state.bytes);
                            next_models=std::make_unique<ctr::GameModels>(next->rom(),next->symbols());
                            next_menu=std::make_unique<ctr::GameMenu>(next->rom(),next->symbols());
                            next_menu->update(ctr::GameMenu::capture(next->game()));
                            next_hook=std::make_unique<Suspension>(*next,*audio,[&]{
                                render_clock.reset();rendered_rate.reset();
                                if(remap) remap->suspend();
                                if(hud_editor) hud_editor->suspend();
                                if(quick) quick->suspend();
                                checkpoint(true);
                            },[&]{return remap || hud_editor || quick || session->game().runtime_options_open();});
#if defined(STARFOX_3DS_CORE_PICA)
                            next_layers=std::make_unique<ctr::GameLayers>(indexed_mono_upload);
                            next_dots=std::make_unique<ctr::GameDots>(next->rom(),next->symbols());
#endif
                        } catch(const std::exception& failure) {
#if defined(STARFOX_3DS_CORE_PICA)
                            next_dots.reset();next_layers.reset();
#endif
                            next_hook.reset();next_menu.reset();next_models.reset();next.reset();quick->message(failure.what());
                        }
                        if(next) {
                            // Validation/allocation completed with the old owner
                            // intact. Reset NDSP synchronously before reusing its
                            // buffers; reset failure is terminal, not a half-load.
                            audio->reset();audio->pause(true);
#if defined(STARFOX_3DS_CORE_PICA)
                            // Join the plan worker and retire GPU borrows first.
                            mono.reset();gpu.reset();
#endif
                            suspension.swap(next_hook);menu.swap(next_menu);models.swap(next_models);
#if defined(STARFOX_3DS_CORE_PICA)
                            layers.swap(next_layers);dots.swap(next_dots);
#endif
                            session.swap(next);rasters=logic=blocks=0;quick.reset();
                            render_clock.reset();rendered_rate.reset();
                            checkpoint(true);audio->pause(false);continue;
                        }
                    }
                    // No cartridge ticking, clock catch-up or world preparation
                    // while paused here. APT preserves the same partial cadence.
                    session->advance(monotonic_time(),0,false);
                    present_quick();
                    continue;
                }
                const bool runtime_before=session->game().runtime_options_open();
                STARFOX_3DS_STARTUP_MARK(source_advance);
                const auto source_time=monotonic_time();
                const auto advanced=session->advance(source_time,controls.held,true,ctr::mapped_buttons(bindings,pad),true);
                STARFOX_3DS_STARTUP_MARK(source_advance_complete);
                if(runtime_before!=session->game().runtime_options_open()) audio->pause(session->game().runtime_options_open());
                rasters+=advanced.video_phases;logic+=advanced.logic_ticks;blocks+=advanced.audio_blocks;
                if(advanced.requested_controller_remap) {
                    render_clock.reset();rendered_rate.reset();
                    checkpoint(true);audio->pause(true);
                    remap=std::make_unique<ctr::GameRemap>();remap->open(bindings);continue;
                }
                if(advanced.requested_hud_customization) {
                    render_clock.reset();rendered_rate.reset();checkpoint(true);audio->pause(true);
                    hud_editor=std::make_unique<ctr::GameHudEditor>();hud_editor->open(session->preferences().hud_layout);continue;
                }
                if(advanced.requested_settings_reset) {
                    checkpoint(true);
                    // Preserve the current real EX bank even if SD writing is
                    // disabled. Reset all settings by replacing, not partially
                    // mutating, the source owner. No game-save erasure.
                    const auto ram=session->cartridge_ram();
                    cartridge_ram[unsigned(session->cartridge_experience())].assign(ram.begin(),ram.end());
                    bank_crc[unsigned(session->cartridge_experience())]=cartridge_crc;
                    if(session->cartridge_experience()==simulation::Experience::starfox_ex) {
                        saved.ex_sram.assign(ram.begin(),ram.end());saved.ex_rom_crc=cartridge_crc;
                    }
                    saved=ctr::default_game_settings(std::move(saved));
                    bindings=saved.bindings;
                    if(save_enabled) try {storage.save(saved);save_warning=storage.current().warning;}
                    catch(const std::exception& failure) {
                        save_warning=std::string(failure.what())+"\nDefaults applied in memory; SD saving disabled until restart.";
                        save_enabled=false;
                    }
                    experience=simulation::Experience::original;
                    ctr::GameSessionOptions options;options.preferences=saved.preferences;
                    load("BOOT",options);continue;
                }
                if(advanced.requested_experience || advanced.requested_preview) {
                    checkpoint(true);
                    ctr::GameSessionOptions options;
                    options.preferences=session->preferences();
                    options.preview=advanced.requested_preview.value_or(session->game().menu_preview());
                    options.start_after_preview=advanced.start_after_preview;
                    const auto ram=session->cartridge_ram();
                    cartridge_ram[unsigned(session->cartridge_experience())].assign(ram.begin(),ram.end());
                    bank_crc[unsigned(session->cartridge_experience())]=cartridge_crc;
                    if(advanced.requested_experience) experience=*advanced.requested_experience;
                    load(options.preview?"LEVEL1_1":"BOOT",options);
                    continue;
                }
                bool frame_handled=false;
#if defined(STARFOX_3DS_CORE_PICA)
                if(!session->game().in_setup_menu() && controls.slider<=0 && !session->mono_prepared()) {
                    session->run_frame_work([] {}); // Complete the deferred source block.
                    mono.reset();gpu.reset();audio->pause(true);
                    top.clear({8,15,28});top.text(24,100,"PREPARING MODELS",{240,181,86},2);
                    const bool ready=session->prepare_mono([&](unsigned done,unsigned total) {
                        const auto keys=display.poll();
                        if(!keys.running || (keys.held&(input::select|input::start))==(input::select|input::start)) return false;
                        if(done%16U==0) {
                            lower.clear({8,15,28});
                            lower.text(12,16,"CACHING MODELS AND LODS\n"+std::to_string(done)+" / "+std::to_string(total),{183,224,240});
                            display.present(ctr::plan_frame(0,false,ctr::ScreenUse::setup),top.view(),{},lower.view());
                        }
                        return true;
                    });
                    if(!ready) break;
                    session->advance(monotonic_time(),0,false);
                    render_clock.reset();rendered_rate.reset();audio->pause(false);continue;
                }
#endif
                session->run_frame_work([&] {
                checkpoint(false);
                if(advanced.logic_ticks || menu->state().visible!=session->game().in_setup_menu()) {
                    STARFOX_3DS_FRAME_PHASE(menu);
                    menu->update(ctr::GameMenu::capture(session->game()));
                }
                // Keep input, source raster and NDSP service at 60 Hz. Only
                // skip scene preparation/submission; never spin on a 30 Hz gap.
                if(!render_clock.due(monotonic_time(),session->game().presentation_fps(),advanced.video_phases)) {
                    if(ctr::source_phase_wait_needed(advanced.raster_fraction,monotonic_time()-source_time))
                        gspWaitForVBlank();
                    frame_handled=true;return;
                }
                const auto source=session->presentation(controls.slider,controls.stereoscopic_hardware,session->stereo_settings());
#if defined(STARFOX_3DS_PROFILE_FRAMES)
                profile_background=source.current->background_id;
#endif
                auto dashboard=source.dashboard;
                if(!save_warning.empty() && session->game().in_setup_menu()) {
                    lower.clear({0,0,0});lower.image(0,0,dashboard);
                    lower.text(8,8,"SD SAVE WARNING\n"+save_warning,{239,90,99},1,304,216);
                    dashboard=lower.view();
                }
                if(session->settings_reset_hold().active()) {
                    if(dashboard.pixels.data()!=lower.view().pixels.data()) {
                        lower.clear({0,0,0});lower.image(0,0,dashboard);
                    }
                    const auto seconds=session->settings_reset_hold().elapsed()/1'000'000'000LL;
                    lower.rectangle(0,207,320,33,{8,15,28});
                    lower.text(8,210,"HOLD L+R: RESET SETTINGS "+std::to_string(seconds)+"/5\nRELEASE TO CANCEL / GAME SAVE KEPT",{240,181,86},1,304,28);
                    dashboard=lower.view();
                }
                if(session->game().show_fps()) {
                    if(dashboard.pixels.data()!=lower.view().pixels.data()) {
                        lower.clear({0,0,0});lower.image(0,0,dashboard);
                    }
                    // The eight-pixel header lies above the radio border and
                    // does not overwrite the requested split-HUD artwork.
                    lower.rectangle(244,0,76,8,{15,29,42});
                    lower.text(250,0,"FPS "+(rendered_rate.fps()?std::to_string(rendered_rate.fps()):"--"),{213,237,244},1,70,8);
                    dashboard=lower.view();
                }
                const bool plain=menu->state().visible && !menu->state().preview;
#if defined(STARFOX_3DS_CORE_PICA)
                if(plain) {
                    // Retire the Citro3D queue before using gfxSwapBuffers.
                    // The menu's exact CPU canvas needs neither scene textures
                    // nor a GPU submission (including the initial boot frame).
                    mono.reset();gpu.reset();
                    STARFOX_3DS_STARTUP_MARK(menu_lcd_present);
                    { STARFOX_3DS_FRAME_PHASE(present);display.present(source.plan,menu->plain_view(),{},dashboard); }
                    STARFOX_3DS_STARTUP_MARK(menu_lcd_present_complete);
                    rendered_rate.completed(monotonic_time());frame_handled=true;return;
                }
                // Reference mono does not yet own cartridge window/color
                // effects. Retain the exact native path for those frames.
                const bool source_effect=source.raster && (source.raster->circle.active
                    || source.raster->wipe.active || source.raster->colour_math.active);
                if(!source.plan.stereo && !source_effect && !menu->state().visible && !session->game().runtime_options_open()) {
                    gpu.reset();
                    if(!mono) mono=std::make_unique<ctr::NativeMono>();
                    if(mono->present(*session,source,dashboard)) rendered_rate.completed(monotonic_time());
                    frame_handled=true;return;
                }
                ensure_gpu();
                const auto model_frame=[&] { STARFOX_3DS_FRAME_PHASE(models);return models->prepare(source); }();
                const auto dot_frame=[&] { STARFOX_3DS_FRAME_PHASE(dots);return dots->prepare(source); }();
                const auto effect_frames=effects.prepare(source);
                const auto math=effect_frames.colour,mask=effect_frames.window;
                // Scene/raster/model clocks are shared by the eyes. Only PICA
                // eye matrices differ. The lower cockpit never joins a wipe.
#if defined(STARFOX_3DS_TEST_PLAYER)
                const ctr::PicaFrame label{source.plan,{},{},{}};
#else
                const ctr::PicaFrame label{source.plan,label_vertices,std::span(&label_draw,1),std::span(&label_image,1)};
#endif
                const auto menu_frame=menu->frame(source.plan);
                const std::size_t occupied=model_frame.vertices.size()+dot_frame.vertices.size()+math.vertices.size()
                    +mask.vertices.size()+label.vertices.size()+menu_frame.vertices.size();
                const unsigned tile_budget=occupied<ctr::pica_vertex_limit?ctr::pica_vertex_limit-unsigned(occupied):0;
                const auto artwork=[&] { STARFOX_3DS_FRAME_PHASE(layers);return layers->prepare(source,tile_budget); }();
                const auto frame=[&] {
                    STARFOX_3DS_FRAME_PHASE(composite);
                    return composite.prepare_validated(source.plan,
                        std::array{artwork.before_models,dot_frame,model_frame,artwork.after_models,math,mask,label,menu_frame},dashboard,artwork.clear);
                }();
                { STARFOX_3DS_FRAME_PHASE(present);gpu->present(frame,dashboard); }
                rendered_rate.completed(monotonic_time());
                frame_handled=true;return; // Sole GPU owner: never also swap through NativeDisplay.
#else
                if(plain) {
                    display.present(source.plan,menu->plain_view(),{},dashboard);
                    rendered_rate.completed(monotonic_time());frame_handled=true;return;
                }
                static_cast<void>(models->prepare(source));
#endif
                if(dashboard.pixels.data()!=lower.view().pixels.data()) {
                    lower.clear({0,0,0});lower.image(0,0,dashboard);
                }
                },false); // Keep independent SPC work running during LCD idle.
                if(frame_handled) continue;
            }
        } catch(const std::exception& failure) {
            error=failure.what();running=false;
            release_owners();
        }
        // Explicit diagnostic panel, not a fabricated/replacement pre-game
        // menu, nor a mono image pretending to be native stereoscopic gameplay.
        top.clear({8,15,28});
        top.text(12,12,"STAR FOX ENHANCED / ACTUAL SOURCE CORE",{183,224,240});
#if defined(STARFOX_3DS_TEST_PLAYER)
        top.text(12,38,"EXPERIMENTAL / NEW 3DS FAMILY TEST BUILD",{240,181,86});
#else
        top.text(12,38,"BRING-UP ONLY / FULL FLOW PENDING",{240,181,86});
#endif
        top.text(12,64,experience==simulation::Experience::original?"CARTRIDGE: ORIGINAL":"CARTRIDGE: EX",{227,235,242},2);
        if(running) {
            const auto coverage=models->coverage();
            top.text(12,100,"REAL BOOT VM + SPC + LOWER HUD RUNNING\nSOURCE RASTERS: "+std::to_string(rasters)
                +"\nLOGIC TICKS: "+std::to_string(logic)+"\nPCM BLOCKS: "+std::to_string(blocks)
                +"\nNATIVE MODELS: "+std::to_string(coverage.models),{227,235,242});
        } else {
#if defined(STARFOX_3DS_TEST_PLAYER)
            lower.clear({8,15,28});lower.text(12,12,"TEST BUILD / CHECK ASSET FILE AND SD CARD",{183,224,240});
#else
            lower.clear({8,15,28});lower.text(12,12,"SOURCE CORE CHECK / NOT THE GAME",{183,224,240});
#endif
            top.text(12,100,"A: LOAD STANDARD COMPANION AND RUN BOOT\nY: DIRECT LEVEL1_1 SOURCE SCENE CHECK\nX: SELECT ORIGINAL / EX\nSELECT + START: EXIT\n\nSD CARD: /3ds/starfox-enhanced/\nStarfox-Assets.BIN",{227,235,242});
            if(!error.empty() || !save_warning.empty()) lower.text(12,40,"LOAD / CORE / SD ERROR\n"+error+"\n"+save_warning,{239,90,99},1,296,186);
        }
        display.present(ctr::plan_frame(0,false,ctr::ScreenUse::setup),top.view(),{},lower.view());
    }
    // Hook, model and session references retire before DSP storage / LCDs.
    checkpoint(true);
    release_owners();
#if defined(STARFOX_3DS_PROFILE_FRAMES)
    // No source advancement or live NDSP/PICA owner can be stalled by SD
    // diagnostics here. Normal exit retains the same CSV format/window cap.
    profile_buffer.flush(profile_stream);
#endif
}
