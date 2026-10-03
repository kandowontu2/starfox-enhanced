#include "starfox/vr/application.hpp"
#include "starfox/audio/msu1_pack.hpp"
#include "desktop_paths.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>
#include <optional>

namespace {
using starfox::vr::VrControlAction;
using starfox::vr::vr_control_bit;
volatile std::sig_atomic_t interrupted=0;
void interrupt(int) {interrupted=1;}

#if defined(STARFOX_STEAM_FRAME)
std::filesystem::path executable_directory(const char* argv0) {
    const char* base_path = SDL_GetBasePath();
    if (base_path && *base_path) {
        return std::filesystem::absolute(base_path);
    }
    if (!argv0 || !*argv0) {
        throw std::runtime_error("Cannot determine the executable directory");
    }
    return std::filesystem::absolute(argv0).parent_path();
}
#endif

#if defined(STARFOX_STEAM_FRAME)
constexpr bool steam_frame_player=true;
#else
constexpr bool steam_frame_player=false;
#endif
class DesktopGamepad {
public:
    DesktopGamepad() {
        initialized_=SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if(!initialized_) std::cerr<<"Gamepad input unavailable: "<<SDL_GetError()<<'\n';
    }
    ~DesktopGamepad() {
        stop_rumble();
        if(gamepad_) SDL_CloseGamepad(gamepad_);
        if(initialized_) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
    starfox::vr::VrControls sample() {
        starfox::vr::VrControls controls;
        if(!initialized_) return edges_.sample(controls);
        SDL_PumpEvents();
        if(gamepad_ && !SDL_GamepadConnected(gamepad_)) {
            stop_rumble();
            SDL_CloseGamepad(gamepad_);gamepad_=nullptr;
        }
        if(!gamepad_) {
            int count=0;
            auto* ids=SDL_GetGamepads(&count);
            for(int i=0;i<count && !gamepad_;++i) gamepad_=SDL_OpenGamepad(ids[i]);
            SDL_free(ids);
            if(gamepad_) std::cout<<"PCVR gamepad connected: "<<SDL_GetGamepadName(gamepad_)<<'\n';
        }
        if(!gamepad_) return edges_.sample(controls);
        controls.active_actions = vr_control_bit(VrControlAction::steer)
            | vr_control_bit(VrControlAction::fire)
            | vr_control_bit(VrControlAction::bomb)
            | vr_control_bit(VrControlAction::boost)
            | vr_control_bit(VrControlAction::brake)
            | vr_control_bit(VrControlAction::menu)
            | vr_control_bit(VrControlAction::roll_left)
            | vr_control_bit(VrControlAction::roll_right)
            | vr_control_bit(VrControlAction::select)
            | vr_control_bit(VrControlAction::stick_left)
            | vr_control_bit(VrControlAction::stick_right);
        const auto button=[&](SDL_GamepadButton name) {return SDL_GetGamepadButton(gamepad_,name);};
        const auto axis=[&](SDL_GamepadAxis name) {
            return std::clamp(float(SDL_GetGamepadAxis(gamepad_,name))/32767.F,-1.F,1.F);
        };
        float x=axis(SDL_GAMEPAD_AXIS_LEFTX),y=-axis(SDL_GAMEPAD_AXIS_LEFTY);
        if(button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) x=-1.F;
        if(button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) x=1.F;
        if(button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) y=-1.F;
        if(button(SDL_GAMEPAD_BUTTON_DPAD_UP)) y=1.F;
        const float radius=std::hypot(x,y);
        if(radius>.18F) {
            const float scale=(std::min(radius,1.F)-.18F)/(.82F*radius);
            controls.steer={x*scale,y*scale};
        }
#if defined(STARFOX_STEAM_FRAME)
        constexpr bool frame_face_buttons=true;
#else
        constexpr bool frame_face_buttons=false;
#endif
        starfox::vr::desktop_face_buttons(controls,frame_face_buttons,
            button(SDL_GAMEPAD_BUTTON_SOUTH),button(SDL_GAMEPAD_BUTTON_EAST),
            button(SDL_GAMEPAD_BUTTON_WEST),button(SDL_GAMEPAD_BUTTON_NORTH));
        controls.menu=button(SDL_GAMEPAD_BUTTON_START);
        controls.select=button(SDL_GAMEPAD_BUTTON_BACK);
#if defined(STARFOX_STEAM_FRAME)
        controls.roll_left=button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        controls.roll_right=button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
#else
        controls.roll_left=axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>.35F;
        controls.roll_right=axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>.35F;
#endif
        controls.stick_left=button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
        controls.stick_right=button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        return edges_.sample(controls);
    }
    bool rumble(std::uint16_t low,std::uint16_t high,std::uint32_t duration_ms) noexcept {
        if(!gamepad_) return false;
        const bool succeeded=SDL_RumbleGamepad(gamepad_,low,high,duration_ms);
        if(succeeded) rumbling_=low!=0U || high!=0U;
        return succeeded;
    }
    bool rumble_available() const noexcept {return gamepad_!=nullptr;}
    void stop_rumble() noexcept {
        if(gamepad_ && rumbling_)
            static_cast<void>(SDL_RumbleGamepad(gamepad_,0U,0U,0U));
        rumbling_=false;
    }
private:
    bool initialized_{},rumbling_{};
    starfox::vr::DesktopControlEdges edges_{steam_frame_player};
    SDL_Gamepad* gamepad_{};
};
}

int main(int argc,char** argv) try {
#if defined(STARFOX_STEAM_FRAME)
    const auto directory=executable_directory(argc>0?argv[0]:nullptr);
#else
    // PCVR and Quest keep resolving their folder exactly as before.
    const auto directory=std::filesystem::absolute(argv[0]).parent_path();
#endif
    starfox::vr::DesktopPathOverrides path_overrides;
    std::string msu;
    bool enhanced_sky=false;
#if defined(STARFOX_STEAM_FRAME)
    std::optional<std::filesystem::path> profile_csv;
    std::optional<unsigned> profile_frames;
#endif
    for(int i=1;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--help" || option=="-h") {
#if defined(STARFOX_STEAM_FRAME)
            std::cout<<"Star Fox Enhanced Steam Frame (development)\n"
                "Usage: starfox_steamframe [--bundle Starfox-Assets.BIN] [--data-dir DIRECTORY] [--msu PACK] [--enhanced-sky] [--profile-csv FILE [--profile-frames N]]\n"
                "--enhanced-sky: start with Enhanced Sky on (also in 2D Options); unsupported families remain native.\n"
                "--profile-csv FILE: write per-frame CPU and available Vulkan GPU timestamps to FILE.\n"
                "--profile-frames N: stop after N submitted frames in profiling mode (default 120; range 1..1000000).\n"
                "Requires a Vulkan-capable GPU and an active OpenXR headset runtime.\n"
                "Default bundle and saves/settings/shader cache: $XDG_DATA_HOME/StarFoxEnhanced\n"
                "If XDG_DATA_HOME is unset or relative, HOME/.local/share/StarFoxEnhanced is used.\n"
                "Generate your own BIN with starfox_asset_builder; no cartridge is bundled.\n";
#else
            std::cout<<"Star Fox Enhanced PCVR (development)\n"
                "Usage: starfox_pcvr [--bundle Starfox-Assets.BIN] [--data-dir DIRECTORY] [--msu PACK] [--enhanced-sky]\n"
                "--enhanced-sky: start with Enhanced Sky on (also in 2D Options); unsupported families remain native.\n"
                "Requires a Vulkan-capable GPU and an active OpenXR headset runtime.\n"
                "Default bundle: beside this executable. Saves/settings: vr-data beside this executable.\n"
                "Generate your own BIN with starfox_asset_builder; no cartridge is bundled.\n";
#endif
            return 0;
        }
        if(option=="--enhanced-sky") {enhanced_sky=true;continue;}
#if defined(STARFOX_STEAM_FRAME)
        if(option=="--profile-csv" && i+1<argc) {profile_csv=argv[++i];continue;}
        if(option=="--profile-frames" && i+1<argc) {
            const std::string_view value=argv[++i];unsigned frames{};
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),frames);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || frames==0 || frames>1000000) {
                std::cerr<<"--profile-frames must be 1..1000000\n";return 2;
            }
            profile_frames=frames;continue;
        }
#endif
        if(i+1>=argc || (option!="--bundle" && option!="--data-dir" && option!="--msu")) {
            std::cerr<<"Unknown or incomplete option: "<<option<<". Use --help.\n";return 2;
        }
        if(option=="--bundle") path_overrides.bundle=argv[++i];
        else if(option=="--data-dir") path_overrides.data_directory=argv[++i];
        else msu=argv[++i];
    }
#if defined(STARFOX_STEAM_FRAME)
    if(profile_frames && !profile_csv) {
        std::cerr<<"--profile-frames requires --profile-csv\n";return 2;
    }
#endif
#if defined(STARFOX_STEAM_FRAME)
    constexpr bool steam_frame=true;
#else
    constexpr bool steam_frame=false;
#endif
    const auto paths=starfox::vr::resolve_desktop_paths(
        directory,steam_frame,
        std::getenv("XDG_DATA_HOME")?std::getenv("XDG_DATA_HOME"):"",
        std::getenv("HOME")?std::getenv("HOME"):"",
        path_overrides);
    const auto& bundle=paths.bundle;
    const auto& data=paths.data_directory;
    if(!std::filesystem::is_regular_file(bundle)) {
        std::cerr<<"Missing asset bundle: "<<bundle<<"\n"
#if defined(STARFOX_STEAM_FRAME)
            "Use starfox_asset_builder to prepare your own Starfox-Assets.BIN in the default data directory or pass --bundle PATH.\n";
#else
            "Use starfox_asset_builder to prepare your own Starfox-Assets.BIN, then place it beside this executable or pass --bundle PATH.\n";
#endif
        return 2;
    }
    if(msu.empty()) {
        // Asset builders commonly leave the optional pack beside the BIN.
        // Preserve the packaged executable's local pack as first priority,
        // then support --bundle pointing to a separate asset directory.
        for(const auto& base:{directory,bundle.parent_path()}) {
            const auto candidate=base/starfox::audio::msu1_pack_filename;
            if(std::filesystem::is_regular_file(candidate)) {
                msu=candidate.string();break;
            }
        }
    }
    std::cerr<<"PCVR soundtrack: "<<(msu.empty()?"native SPC (MSU pack not found)":msu)<<'\n';
    starfox::vr::ApplicationHost host;
    // The diagnostic host defaults to 120 frames / 30 seconds. A player
    // executable must run until an actual exit, never expire mid-game.
#if defined(STARFOX_STEAM_FRAME)
    host.frame_limit=profile_csv?profile_frames.value_or(120U):0U;
    host.profile_csv_path=profile_csv;
#else
    host.frame_limit=0;
#endif
    host.time_limit=std::chrono::seconds(0);
    host.steam_frame=steam_frame;
    host.cartridge_save_path=data/"starfox-ex.srm";
    std::signal(SIGINT,interrupt);
    std::signal(SIGTERM,interrupt);
    host.stop_requested=[] {return interrupted!=0;};
    DesktopGamepad gamepad;
    host.desktop_controls=[&gamepad] {return gamepad.sample();};
    host.desktop_rumble_available=[&gamepad] {return gamepad.rumble_available();};
    host.desktop_rumble=[&gamepad](std::uint16_t low,std::uint16_t high,std::uint32_t duration) {
        return gamepad.rumble(low,high,duration);
    };
    host.stop_desktop_rumble=[&gamepad] {gamepad.stop_rumble();};
    std::vector<std::string> arguments{argv[0],"--bundle",bundle.string()};
    if(enhanced_sky) arguments.emplace_back("--enhanced-sky");
    if(!msu.empty()) {arguments.emplace_back("--msu");arguments.push_back(msu);}
    std::vector<char*> pointers;
    for(auto& arg:arguments) pointers.push_back(arg.data());
    return starfox::vr::run_application(int(pointers.size()),pointers.data(),host);
} catch(const std::exception& error) {
    std::cerr<<"PCVR could not start: "<<error.what()<<'\n';return 1;
}
