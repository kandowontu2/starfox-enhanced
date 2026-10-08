#include "starfox/vr/application.hpp"
#include "starfox/vr/diagnostic_log.hpp"
#include "starfox/audio/msu1_pack.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
volatile std::sig_atomic_t interrupted=0;
void interrupt(int) {interrupted=1;}
class DesktopGamepad {
public:
    DesktopGamepad() {
        initialized_=SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if(!initialized_) std::cerr<<"Gamepad input unavailable: "<<SDL_GetError()<<'\n';
    }
    ~DesktopGamepad() {
        if(gamepad_) SDL_CloseGamepad(gamepad_);
        if(initialized_) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
    starfox::vr::VrControls sample() {
        starfox::vr::VrControls controls;
        if(!initialized_) return controls;
        SDL_PumpEvents();
        if(gamepad_ && !SDL_GamepadConnected(gamepad_)) {
            SDL_CloseGamepad(gamepad_);gamepad_=nullptr;
        }
        if(!gamepad_) {
            int count=0;
            auto* ids=SDL_GetGamepads(&count);
            for(int i=0;i<count && !gamepad_;++i) gamepad_=SDL_OpenGamepad(ids[i]);
            SDL_free(ids);
            if(gamepad_) std::cout<<"PCVR gamepad connected: "<<SDL_GetGamepadName(gamepad_)<<'\n';
        }
        if(!gamepad_) {last_menu_=last_select_=last_reset_=false;return controls;}
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
        controls.fire=button(SDL_GAMEPAD_BUTTON_SOUTH);
        controls.bomb=button(SDL_GAMEPAD_BUTTON_EAST);
        controls.boost=button(SDL_GAMEPAD_BUTTON_WEST);
        controls.brake=button(SDL_GAMEPAD_BUTTON_NORTH);
        controls.menu=button(SDL_GAMEPAD_BUTTON_START);
        controls.select=button(SDL_GAMEPAD_BUTTON_BACK);
        controls.roll_left=axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>.35F;
        controls.roll_right=axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>.35F;
        controls.stick_left=button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
        controls.stick_right=button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        controls.menu_pressed=controls.menu && !last_menu_;
        controls.select_pressed=controls.select && !last_select_;
        const bool reset=controls.roll_left && controls.roll_right
            && controls.stick_left && controls.stick_right;
        controls.reset_pressed=reset && !last_reset_;
        last_menu_=controls.menu;last_select_=controls.select;last_reset_=reset;
        return controls;
    }
private:
    bool initialized_{},last_menu_{},last_select_{},last_reset_{};
    SDL_Gamepad* gamepad_{};
};
}

int main(int argc,char** argv) try {
    const auto directory=std::filesystem::absolute(argv[0]).parent_path();
    auto bundle=directory/"Starfox-Assets.BIN";
    auto data=directory/"vr-data";
    std::string msu;
    bool enhanced_sky=false;
    for(int i=1;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--help" || option=="-h") {
            std::cout<<"Star Fox Enhanced PCVR (development)\n"
                "Usage: starfox_pcvr [--bundle Starfox-Assets.BIN] [--data-dir DIRECTORY] [--msu PACK] [--enhanced-sky]\n"
                "--enhanced-sky: start with Enhanced Sky on (also in 2D Options); unsupported families remain native.\n"
                "Requires a Vulkan-capable GPU and an active OpenXR headset runtime.\n"
                "Default bundle: beside this executable. Saves/settings: vr-data beside this executable.\n"
                "Diagnostics: vr-data/vr-session.log and vr-session.previous.log (8 MiB each).\n"
                "Generate your own BIN with starfox_asset_builder; no cartridge is bundled.\n";
            return 0;
        }
        if(option=="--enhanced-sky") {enhanced_sky=true;continue;}
        if(i+1>=argc || (option!="--bundle" && option!="--data-dir" && option!="--msu")) {
            std::cerr<<"Unknown or incomplete option: "<<option<<". Use --help.\n";return 2;
        }
        if(option=="--bundle") bundle=std::filesystem::absolute(argv[++i]);
        else if(option=="--data-dir") data=std::filesystem::absolute(argv[++i]);
        else msu=argv[++i];
    }
    // Only app-owned diagnostics are rotated; assets, preferences and saves are
    // never replaced here. An unavailable destination leaves console output on.
    std::ofstream diagnostic_file;
    std::error_code log_error;
    std::filesystem::create_directories(data,log_error);
    if(!log_error) {
        const auto path=data/"vr-session.log";
        bool may_replace=true;
        if(std::filesystem::exists(path,log_error) && std::filesystem::is_regular_file(path,log_error)) {
#if defined(_WIN32)
            // MinGW copy_file can reject an existing backup even with the
            // overwrite flag. Use the OS's explicit overwrite semantics.
            if(!CopyFileW(path.c_str(),(data/"vr-session.previous.log").c_str(),FALSE))
                log_error.assign(static_cast<int>(GetLastError()),std::system_category());
#else
            std::filesystem::copy_file(path,data/"vr-session.previous.log",
                std::filesystem::copy_options::overwrite_existing,log_error);
#endif
            if(log_error) {
                may_replace=false;
                std::cerr<<"Could not retain previous PCVR diagnostic: "<<log_error.message()<<'\n';
            }
        }
        if(may_replace && !log_error) diagnostic_file.open(path,std::ios::out|std::ios::trunc);
    }
    if(!diagnostic_file.is_open()) std::cerr<<"Persistent PCVR diagnostics unavailable; console logging remains active.\n";
    starfox::vr::BoundedDiagnosticLog diagnostic_log(diagnostic_file);
    starfox::vr::ScopedDiagnosticTee output(std::cout,diagnostic_log),errors(std::cerr,diagnostic_log);
    diagnostic_log.line("Star Fox Enhanced PCVR development session");
    std::cout<<"PCVR session start_unix_ms="
        <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        <<" data_directory="<<data<<'\n';
    try {
    if(!std::filesystem::is_regular_file(bundle)) {
        std::cerr<<"Missing asset bundle: "<<bundle<<"\n"
            "Use starfox_asset_builder to prepare your own Starfox-Assets.BIN, then place it beside this executable or pass --bundle PATH.\n";
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
    host.frame_limit=0;host.time_limit=std::chrono::seconds(0);
    host.cartridge_save_path=data/"starfox-ex.srm";
    std::signal(SIGINT,interrupt);
    std::signal(SIGTERM,interrupt);
    host.stop_requested=[] {return interrupted!=0;};
    DesktopGamepad gamepad;
    host.desktop_controls=[&gamepad] {return gamepad.sample();};
    std::vector<std::string> arguments{argv[0],"--bundle",bundle.string()};
    if(enhanced_sky) arguments.emplace_back("--enhanced-sky");
    if(!msu.empty()) {arguments.emplace_back("--msu");arguments.push_back(msu);}
    std::vector<char*> pointers;
    for(auto& arg:arguments) pointers.push_back(arg.data());
    const auto result=starfox::vr::run_application(int(pointers.size()),pointers.data(),host);
    std::cout<<"PCVR native return_code="<<result<<'\n';
    return result;
    } catch(const std::exception& error) {
        std::cerr<<"PCVR could not start/run: "<<error.what()<<'\n';return 1;
    } catch(...) {
        std::cerr<<"PCVR failed with a non-standard native exception\n";return 1;
    }
} catch(const std::exception& error) {
    std::cerr<<"PCVR could not start: "<<error.what()<<'\n';return 1;
}
