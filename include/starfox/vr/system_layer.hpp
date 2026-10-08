#pragma once
#include <chrono>
#include "sfvr/sfvr_view.h"

namespace starfox::vr {
// Cross-port system layer (steam-frame-vr-port standard, section 1): L View and
// R Menu timing. The View state machine is the shared sfvr library's
// sfvr_view (vendored in third_party/sfvr); this class is a thin adapter that
// adds only what is specific to this port: the L View + R Menu chord and the
// rule that a View press belonging to the chord never also taps or recentres.
//
//   L View short press (< 1 s)   -> view_tap (the game's Select / menu back).
//                                   Reported on release, because a press is
//                                   not a tap until it is known not to be a hold.
//   L View held 1 s              -> recentre (yaw + horizontal position).
//   L View held 3 s              -> recentre_height (recentre and recalibrate
//                                   standing height); a further step after the
//                                   1 s one, not instead of it.
//   L View + R Menu held 0.5 s   -> open_menu (this port's runtime menu).
//
// A hold that has already recentred is never also a tap. A View press that
// was already down at construction or through a reset() (focus loss) is
// ignored until released (sfvr_view's rule), and the chord additionally needs
// Menu to have been seen released, so nothing fires on resume.
struct SystemLayerEvents {
    bool view_tap{},recentre{},recentre_height{},open_menu{};
};

class SystemLayer {
public:
    static constexpr double menu_chord_hold_seconds=0.5;

    // `now` is monotonic seconds; only differences are used.
    [[nodiscard]] SystemLayerEvents update(bool view_down,bool menu_down,double now) noexcept {
        SystemLayerEvents events;
        const unsigned view=sfvr_view_update(&view_,nullptr,view_down?1:0,now);
        if(!menu_down) menu_armed_=true;
        if(!view_down) view_armed_=true;
        if(view_down && !view_prev_) {recentred_=false;swallowed_=menu_down;}
        // Menu overlapping a View press claims it, until that press has recentred.
        if(view_down && menu_down && !recentred_) swallowed_=true;
        if(!swallowed_) {
            if(view&SFVR_VIEW_SHORT_PRESS) events.view_tap=true;
            if(view&SFVR_VIEW_RECENTRE) {events.recentre=true;recentred_=true;}
            if(view&SFVR_VIEW_RECALIBRATE_HEIGHT) events.recentre_height=true;
        }
        if(view_down && menu_down && menu_armed_ && view_armed_ && !recentred_) {
            if(!chord_active_) {chord_active_=true;chord_since_=now;chord_done_=false;}
            if(!chord_done_ && now-chord_since_>=menu_chord_hold_seconds) {
                chord_done_=true;events.open_menu=true;
            }
        } else chord_active_=false;
        view_prev_=view_down;
        return events;
    }
    // Focus loss, session change or failure: drop all in-flight presses.
    void reset() noexcept {*this=SystemLayer{};}

    [[nodiscard]] static double steady_seconds() noexcept {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
private:
    sfvr_view_button view_{};
    bool menu_armed_{},view_armed_{},view_prev_{},swallowed_{},recentred_{};
    bool chord_active_{},chord_done_{};
    double chord_since_{};
};
}
