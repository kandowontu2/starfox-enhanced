#include "starfox/app/runtime_input.hpp"
#include <charconv>
#include <iostream>
#include <string_view>

int main(int argc,char** argv) {
    unsigned seconds=0;
    if(argc==3 && std::string_view(argv[1])=="--seconds") {
        const std::string_view value=argv[2];
        const auto parsed=std::from_chars(value.data(),value.data()+value.size(),seconds);
        if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || seconds<1 || seconds>30) return 1;
    } else if(argc!=2 || std::string_view(argv[1])!="--snapshot") {
        std::cerr<<"Usage: starfox_controller_check --snapshot | --seconds 1..30\n";return 1;
    }
    starfox::app::configure_native_gamepad_support();
    if(!SDL_Init(SDL_INIT_GAMEPAD|(seconds?SDL_INIT_VIDEO:0))) {std::cerr<<SDL_GetError()<<'\n';return 2;}
    struct Lifetime {~Lifetime(){SDL_Quit();}} lifetime;
    std::cout<<"SDL "<<SDL_GetVersion()<<"; platform "<<SDL_GetPlatform()<<'\n';
    // No environment dump, device paths, serials, account IDs or typed text.
    for(const char* hint:{SDL_HINT_JOYSTICK_HIDAPI,SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK,SDL_HINT_XINPUT_ENABLED}) {
        const auto* value=SDL_GetHint(hint);
        std::cout<<hint<<"="<<(value?(std::string_view(value)=="0"?"disabled":std::string_view(value)=="1"?"enabled":"custom"):"inherited")<<'\n';
    }
    std::cout<<"Steam launch context: "<<(SDL_getenv("SteamAppId") || SDL_getenv("SteamGameId")?"present":"absent")<<'\n';
    int count=0;auto* ids=SDL_GetJoysticks(&count);
    std::cout<<"Joystick devices: "<<count<<'\n';
    for(int i=0;i<count;++i) {
        const auto id=ids[i];
        std::cout<<"device "<<id<<" vendor="<<SDL_GetJoystickVendorForID(id)<<" product="<<SDL_GetJoystickProductForID(id)
            <<" mapped="<<SDL_IsGamepad(id)<<" virtual="<<SDL_IsJoystickVirtual(id)
            <<" player="<<SDL_GetGamepadPlayerIndexForID(id)<<'\n';
    }
    SDL_free(ids);
    if(!seconds) return 0;
    auto* window=SDL_CreateWindow("Controller diagnostic: press controls; Escape exits (30s max)",640,128,0);
    if(!window) {std::cerr<<SDL_GetError()<<'\n';return 2;}
    auto* gamepad=starfox::app::open_preferred_gamepad();
    std::cout<<"Selected controller: "<<starfox::app::gamepad_device_label(gamepad)<<'\n';
    std::cout<<"Event capture only while this window is focused; no text is recorded.\n";
    unsigned buttons=0,axes=0,keys=0;bool done=false;
    const auto end=SDL_GetTicks()+seconds*1000;
    while(!done && SDL_GetTicks()<end) {
        SDL_Event event;
        if(!SDL_WaitEventTimeout(&event,50)) continue;
        if(event.type==SDL_EVENT_QUIT) break;
        if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)) continue;
        if(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            ++keys;
            const auto key=event.key.scancode;
            const char* kind=key==SDL_SCANCODE_RETURN?"Enter":key==SDL_SCANCODE_ESCAPE?"Escape":key==SDL_SCANCODE_TAB?"Tab":"other (redacted)";
            std::cout<<"keyboard "<<kind<<'\n';done=key==SDL_SCANCODE_ESCAPE;
        } else if(event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            ++buttons;std::cout<<"gamepad "<<event.gbutton.which<<" button="<<unsigned(event.gbutton.button)<<'\n';
        } else if(event.type==SDL_EVENT_GAMEPAD_AXIS_MOTION && (event.gaxis.value>16000 || event.gaxis.value<-16000)) ++axes;
    }
    std::cout<<"Summary: keyboard presses="<<keys<<" gamepad presses="<<buttons<<" active-axis events="<<axes<<'\n';
    if(keys && !buttons && !axes) std::cout<<"Only keyboard events observed; check Steam Input layout and launch mode. This is not proof of hardware failure.\n";
    if(gamepad) SDL_CloseGamepad(gamepad);
    SDL_DestroyWindow(window);
    return 0;
}
