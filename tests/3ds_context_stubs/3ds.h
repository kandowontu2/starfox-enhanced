#pragma once
enum gfxScreen_t { GFX_TOP, GFX_BOTTOM };
enum GSPGPU_FramebufferFormat { GSP_BGR8_OES = 1 };
extern "C" {
void gfxSet3D(bool);
void gfxSetScreenFormat(gfxScreen_t, GSPGPU_FramebufferFormat);
}
