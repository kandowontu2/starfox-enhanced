// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#pragma once

namespace starfox::platform_3ds {

struct HudRect { int x{},y{},w{},h{}; };
struct StackedHudLayout { HudRect upper,bar; };

// The source label/icons and meter are independent layers. Anchor the meter
// to the corner, then put the label/icons above it with a visible gap.
constexpr StackedHudLayout stack_hud(int upper_w,int upper_h,
    int bar_w,int bar_h,bool right,int screen_w=400,int screen_h=240) {
    constexpr int margin=8,gap=4;
    const int bar_x=right?screen_w-margin-bar_w:margin;
    const int bar_y=screen_h-margin-bar_h;
    const int upper_x=right?screen_w-margin-upper_w:margin;
    const int upper_y=bar_y-gap-upper_h;
    return {{upper_x,upper_y,upper_w,upper_h},
        {bar_x,bar_y,bar_w,bar_h}};
}

} // namespace starfox::platform_3ds
