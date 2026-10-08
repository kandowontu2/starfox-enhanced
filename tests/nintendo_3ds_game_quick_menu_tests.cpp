#include "starfox/platform/nintendo_3ds/game_quick_menu.hpp"
#include <iostream>

using namespace starfox;
using namespace platform::nintendo_3ds;
namespace {
unsigned checks{};
void require(bool value,const char* text) {++checks;if(!value) throw std::runtime_error(text);}
void tap(GameQuickMenu& menu,input::ButtonMask key) {menu.update({key});menu.update({});}
}
int main() try {
    GameQuickMenu menu;menu.open(0,true,true);
    const auto first=menu.redraws();
    for(unsigned i=0;i<120;++i) menu.update({input::ButtonMask(input::select|input::y)});
    require(menu.redraws()==first && !menu.take_action(),"Opening chord acted on a quick menu row");
    menu.update({});
    for(unsigned i=0;i<240;++i) require(!menu.update({}),"Idle quick menu redrew");
    const auto frame=menu.frame();
    require(!frame.plan.stereo && frame.vertices.size()==6 && frame.draws.size()==1 && frame.textures.size()==1,
        "Quick menu fabricated source/world stereo");
    require(frame.draws[0].source_layer==0 && frame.draws[0].space==PicaSpace::screen
        && !frame.draws[0].depth_write,"Quick menu text joined source depth/wipes");
    require(valid_image(menu.upper_view(),top_width,screen_height) && valid_image(menu.lower_view(),bottom_width,screen_height),"Quick menu LCD images invalid");
    tap(menu,input::down);tap(menu,input::a);require(menu.take_action()==QuickAction::options,"Actual game options inaccessible");
    tap(menu,input::down);tap(menu,input::left);require(menu.slot()==9,"State slot did not wrap independently");
    tap(menu,input::right);require(menu.slot()==0,"State slot forward wrap incorrect");
    menu.set_info({true,true,7,{}});tap(menu,input::down);tap(menu,input::a);
    require(menu.confirming() && !menu.take_action(),"Occupied state overwritten without confirmation");
    tap(menu,input::b);require(!menu.confirming() && !menu.take_action(),"Cancel confirmation resumed/discarded game");
    tap(menu,input::a);tap(menu,input::a);require(menu.take_action()==QuickAction::save,"Explicit confirmed save did not request host write");
    menu.message("SAVED");menu.update({});tap(menu,input::down);tap(menu,input::a);
    require(menu.confirming() && !menu.take_action(),"Load discarded current run without confirmation");
    menu.suspend();tap(menu,input::a);require(!menu.take_action(),"Home/wake injected held confirm");
    menu.update({});tap(menu,input::a);tap(menu,input::a);require(menu.take_action()==QuickAction::load,"Fresh confirmed load inaccessible");
    menu.open(3,false,false);menu.update({});tap(menu,input::down);tap(menu,input::a);
    require(!menu.take_action(),"Unavailable options acted");tap(menu,input::down);tap(menu,input::down);
    tap(menu,input::a);require(!menu.take_action(),"Unavailable state action acted");
    tap(menu,input::down);tap(menu,input::a);require(!menu.take_action(),"Empty/unavailable state loaded");
    tap(menu,input::b);require(menu.take_action()==QuickAction::resume,"Physical B did not resume");
    menu.open(0,true,true);menu.update({});menu.set_info({false,false,0,"CORRUPT"});
    for(unsigned i=0;i<3;++i) tap(menu,input::down);
    tap(menu,input::a);
    require(!menu.take_action(),"All-corrupt state files were offered overwrite");
    const auto cached=menu.redraws();for(unsigned i=0;i<120;++i) menu.update({});
    require(menu.redraws()==cached,"Paused panel has idle rendering allocations");
    bool rejected=false;try {menu.open(10,true,true);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Invalid quick state slot accepted");
    std::cout<<"3DS native quick menu: "<<checks<<" checks passed; physical navigation/cache contracts, not console pixels\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
