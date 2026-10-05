#include "native_display.hpp"
#include <3ds.h>

extern "C" {
// libctru's 32 KiB default is smaller than the shared SPC bank snapshot alone.
// The actual ARM player overflowed below 0x08000000 during load_driver, before
// reaching its menu. Reserve a bounded native main stack, within the existing
// process heap allocation; this does not request a New-only memory layout.
unsigned int __stacksize__ = 256U * 1024U;
}

namespace starfox::platform::nintendo_3ds {
static_assert(CFG_MODEL_3DS==0 && CFG_MODEL_3DSXL==1 && CFG_MODEL_N3DS==2
    && CFG_MODEL_2DS==3 && CFG_MODEL_N3DSXL==4 && CFG_MODEL_N2DSXL==5);
struct NativeDisplay::Runtime {
    explicit Runtime(HardwareProfile profile):profile(profile) {
        osSetSpeedupEnable(profile.cpu_speedup);
        aptHook(&cookie,callback,this);
    }
    ~Runtime() {aptUnhook(&cookie);osSetSpeedupEnable(false);}
    static void callback(APT_HookType event,void* pointer) noexcept {
        if(event==APTHOOK_ONRESTORE || event==APTHOOK_ONWAKEUP)
            osSetSpeedupEnable(static_cast<Runtime*>(pointer)->profile.cpu_speedup);
    }
    HardwareProfile profile;
    aptHookCookie cookie{};
};
NativeDisplay::NativeDisplay() {
    std::optional<std::uint8_t> detected_model;
    if(R_SUCCEEDED(cfguInit())) {
        u8 model{};
        if(R_SUCCEEDED(CFGU_GetSystemModel(&model)))
            detected_model=model;
        cfguExit();
    }
    profile_=hardware_profile(detected_model);
    // Use the official New-model CPU/cache speedup, not faster cartridge ticks.
    // The lease reapplies it after Home/sleep, and retires before SDK teardown.
    runtime_=std::make_unique<Runtime>(profile_);
    gfxInitDefault();gfxSet3D(false);
}
NativeDisplay::~NativeDisplay() {runtime_.reset();gfxExit();}
NativeInput NativeDisplay::poll() {
    const bool running=aptMainLoop();
    if(!running) return {};
    hidScanInput();circlePosition circle{};hidCircleRead(&circle);
    const auto physical=hidKeysHeld();
    touchPosition touch{};hidTouchRead(&touch);
    const auto slider=profile_.stereo?osGet3DSliderState():0.F;
    return {buttons(physical,circle.dx,circle.dy),slider,true,profile_.stereo,
        buttons(physical),circle.dx,circle.dy,(physical&KEY_TOUCH)!=0,touch.px,touch.py};
}
void NativeDisplay::present(const FramePlan& frame,ImageView left,ImageView right,ImageView lower) {
    if(!valid_image(left,top_width,screen_height) || !valid_image(lower,bottom_width,screen_height)
        || (frame.stereo && (!profile_.stereo || !valid_image(right,top_width,screen_height)
            || right.pixels.data()==left.pixels.data())) || frame.eye_count!=(frame.stereo?2U:1U))
        throw std::invalid_argument("Incomplete independently rendered 3DS frame");
    gfxSet3D(frame.stereo);
    auto* top_left=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,nullptr,nullptr);
    auto* bottom=gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,nullptr,nullptr);
    auto* top_right=frame.stereo?gfxGetFramebuffer(GFX_TOP,GFX_RIGHT,nullptr,nullptr):nullptr;
    if(!top_left || !bottom || (frame.stereo && !top_right)) throw std::runtime_error("3DS LCD allocation failed");
    copy_lcd(left,{top_left,top_width*screen_height*3});
    if(frame.stereo) copy_lcd(right,{top_right,top_width*screen_height*3});
    copy_lcd(lower,{bottom,bottom_width*screen_height*3});
    gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
}
} // namespace starfox::platform::nintendo_3ds
