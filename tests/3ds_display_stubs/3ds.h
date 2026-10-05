#pragma once
// Narrow host UNIT-TEST doubles. Native builds never include this directory;
// libctru ABI/service behavior must still pass the real SDK/device checks.
#include <cstdint>
using u8=std::uint8_t;
using u32=std::uint32_t;
using Result=int;
#define R_SUCCEEDED(result) ((result)>=0)
enum {CFG_MODEL_3DS=0,CFG_MODEL_3DSXL=1,CFG_MODEL_N3DS=2,
    CFG_MODEL_2DS=3,CFG_MODEL_N3DSXL=4,CFG_MODEL_N2DSXL=5};
enum APT_HookType {APTHOOK_ONSUSPEND,APTHOOK_ONSLEEP,APTHOOK_ONRESTORE,APTHOOK_ONWAKEUP,APTHOOK_ONEXIT};
struct aptHookCookie {void (*callback)(APT_HookType,void*){};void* parameter{};};
struct circlePosition {std::int16_t dx{},dy{};};
struct touchPosition {std::uint16_t px{},py{};};
inline constexpr u32 KEY_TOUCH=1U<<20;
enum gfxScreen_t {GFX_TOP,GFX_BOTTOM};
enum gfx3dSide_t {GFX_LEFT,GFX_RIGHT};
Result cfguInit();
Result CFGU_GetSystemModel(u8*);
void cfguExit();
void osSetSpeedupEnable(bool);
float osGet3DSliderState();
void aptHook(aptHookCookie*,void (*)(APT_HookType,void*),void*);
void aptUnhook(aptHookCookie*);
bool aptMainLoop();
void gfxInitDefault();
void gfxSet3D(bool);
void gfxExit();
void hidScanInput();
void hidCircleRead(circlePosition*);
u32 hidKeysHeld();
void hidTouchRead(touchPosition*);
u8* gfxGetFramebuffer(gfxScreen_t,gfx3dSide_t,void*,void*);
void gfxFlushBuffers();
void gfxSwapBuffers();
void gspWaitForVBlank();
