#pragma once
#include "starfox/vr/openxr_input.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/vr/menu_stick.hpp"
#include <cmath>
#include <limits>
namespace starfox::vr {
// Column-major projection of native pad directions into cockpit pad axes.
// It contains no physical head pose.
using SteeringMatrix=std::array<float,4>;
// Produces the same SNES-button TickInput consumed by GameSimulation. Native
// control type/inversion remains the game's responsibility. Default type A:
// Y fire, A bomb, X boost, B brake, L/R roll. Retains quick taps between ticks.
class VrGameInput {
public:
    void sample(const VrControls& controls,bool menu_navigation=false) noexcept {
        input::ButtonMask buttons{};
        const auto add=[&](bool held,input::Button button) {if(held) buttons|=button;};
        add(controls.fire,input::y);add(controls.bomb,input::a);
        add(controls.boost,input::x);add(controls.brake,input::b);
        add(controls.roll_left,input::left_shoulder);add(controls.roll_right,input::right_shoulder);
        // Do not turn a held menu at startup/focus regain into a new Start.
        if(!controls.menu) menu_held_=false;
        if(controls.menu_pressed) menu_held_=true;
        add(menu_held_ && controls.menu,input::start);
        if(!controls.select) select_held_=false;
        if(controls.select_pressed) select_held_=true;
        add(select_held_ && controls.select,input::select);
        if(menu_navigation) buttons|=menu_stick_.sample(controls.steer.x,controls.steer.y);
        else {
            menu_stick_.reset();
            if(std::isfinite(controls.steer.x)) {
                add(controls.steer.x<-.35F,input::left);add(controls.steer.x>.35F,input::right);
            }
            if(std::isfinite(controls.steer.y)) {
                add(controls.steer.y<-.35F,input::down);add(controls.steer.y>.35F,input::up);
            }
        }
        latch_.sample(buttons);
    }
    input::TickInput consume(std::optional<SteeringMatrix> steering=std::nullopt) noexcept {
        auto tick=latch_.consume();
        constexpr input::ButtonMask directions=input::left|input::right|input::up|input::down;
        if(steering) {
            // Threshold/latched taps were resolved before rotation. Rotating
            // analog .4 into (.283,.283) must not erase low-sensitivity input.
            const auto rotate=[&](float x,float y) {
                if(x==0 && y==0) return input::ButtonMask{};
                // Choose the source's eight-way direction whose rendered
                // direction is closest. This also handles foreshortened axes
                // under pitch/yaw without applying a second analog deadzone.
                float best=-std::numeric_limits<float>::infinity();input::ButtonMask selected{};
                for(const auto& direction:std::array<std::array<int,2>,8>{{
                    {1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}}}) {
                    const auto dx=direction[0],dy=direction[1];
                    const float a=(*steering)[0]*dx+(*steering)[2]*dy;
                    const float b=(*steering)[1]*dx+(*steering)[3]*dy;
                    const float length=std::hypot(a,b);
                    if(!(length>0)) continue;
                    const float score=(a*x+b*y)/length;
                    if(score<=best) continue;
                    best=score;selected=0;
                    if(dx) selected|=dx>0?input::right:input::left;
                    if(dy) selected|=dy>0?input::up:input::down;
                }
                return selected;
            };
            const auto map=[&](input::ButtonMask buttons) {
                const bool left=buttons&input::left,right=buttons&input::right;
                const bool up=buttons&input::up,down=buttons&input::down;
                auto result=input::ButtonMask(buttons&~directions);
                if((left && right) || (up && down)) {
                    // Opposite quick taps may coexist in a latched edge mask.
                    if(left) result|=rotate(-1,0);if(right) result|=rotate(1,0);
                    if(up) result|=rotate(0,1);if(down) result|=rotate(0,-1);
                } else result|=rotate(float(right)-left,float(up)-down);
                return result;
            };
            tick.held=map(tick.held);tick.pressed=map(tick.pressed);tick.released=map(tick.released);
            // A held stick can cross a source direction as the ship banks.
            // Emit the corresponding edges once, at source consumption.
            tick.pressed|=(tick.held&directions)&~previous_directions_;
            tick.released|=previous_directions_&~(tick.held&directions);
        }
        previous_directions_=tick.held&directions;
        return tick;
    }
    void reset() noexcept {latch_.reset();menu_stick_.reset();menu_held_=select_held_=false;previous_directions_=0;}
private:
    input::InputLatch latch_;
    MenuStick menu_stick_;
    bool menu_held_{},select_held_{};
    input::ButtonMask previous_directions_{};
};
}
