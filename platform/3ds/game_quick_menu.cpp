#include "starfox/platform/nintendo_3ds/game_quick_menu.hpp"

namespace starfox::platform::nintendo_3ds {
GameQuickMenu::GameQuickMenu() {
    constexpr std::array<Point3,4> corners{{{0,0,0},{400,0,0},{400,240,0},{0,240,0}}};
    constexpr std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
    unsigned i=0;for(unsigned corner:{0U,1U,2U,0U,2U,3U}) vertices_[i++]={corners[corner],{1,1,1,1},uv[corner]};
    draws_[0]={0,6,0,pica_identity,PicaSpace::screen,false,false,false};draws_[0].source_layer=0;
}
void GameQuickMenu::open(unsigned slot,bool states,bool options) {
    if(slot>=GameStateStorage::slots) throw std::invalid_argument("Invalid quick menu state slot");
    slot_=slot;selection_=0;states_=states;options_=options;await_release_=true;confirming_=false;
    previous_=0;action_.reset();info_={};message_.clear();redraw();
}
void GameQuickMenu::set_info(GameStateInfo info) {info_=std::move(info);message_.clear();confirming_=false;redraw();}
void GameQuickMenu::message(std::string text) {message_=std::move(text);confirming_=false;await_release_=true;redraw();}
void GameQuickMenu::suspend() {previous_=0;await_release_=true;confirming_=false;action_.reset();redraw();}
bool GameQuickMenu::update(PadSample sample) {
    const auto fixed=fixed_menu_buttons(sample);
    if(await_release_) {if(!fixed && !physical_sources(sample,40)) {await_release_=false;previous_=0;}return false;}
    if(action_) return false;
    const auto pressed=input::ButtonMask(fixed&~previous_);previous_=fixed;
    if(!pressed) return false;
    if(pressed&input::b) {
        if(confirming_) {confirming_=false;redraw();}
        else action_=QuickAction::resume;
        return true;
    }
    if(pressed&(input::up|input::down)) {
        selection_=(selection_+((pressed&input::up)?4U:1U))%5;confirming_=false;message_.clear();redraw();return true;
    }
    if(selection_==2 && (pressed&(input::left|input::right))) {
        slot_=(slot_+((pressed&input::left)?GameStateStorage::slots-1:1))%GameStateStorage::slots;
        info_={};confirming_=false;message_.clear();redraw();return true;
    }
    if(!(pressed&input::a)) return false;
    if(selection_==0) action_=QuickAction::resume;
    else if(selection_==1 && options_) action_=QuickAction::options;
    else if(selection_==3 && states_ && info_.writable) {
        if(info_.found && !confirming_) {confirming_=true;redraw();}
        else {action_=QuickAction::save;confirming_=false;}
    } else if(selection_==4 && states_ && info_.found) {
        if(!confirming_) {confirming_=true;redraw();}
        else {action_=QuickAction::load;confirming_=false;}
    } else return false;
    return true;
}
PicaFrame GameQuickMenu::frame() const {
    return {plan_frame(0,false,ScreenUse::setup),vertices_,draws_,images_,{8,15,28}};
}
void GameQuickMenu::redraw() {
    constexpr Rgb bg{8,15,28},ink{228,234,242},selected{255,209,70},muted{119,133,153};
    upper_.clear(bg);lower_.clear(bg);
    upper_.text(90,16,"STAR FOX ENHANCED / PAUSED",ink);
    const std::array<std::string,5> rows{"RESUME","GAME OPTIONS","STATE SLOT  "+std::to_string(slot_),"SAVE STATE","LOAD STATE"};
    for(unsigned i=0;i<rows.size();++i) {
        const bool disabled=(i==1 && !options_) || (i==3 && (!states_ || !info_.writable)) || (i==4 && (!states_ || !info_.found));
        const auto colour=disabled?muted:i==selection_?selected:ink;
        if(i==selection_) upper_.text(73,59+int(i)*25,">",selected);
        upper_.text(90,59+int(i)*25,rows[i],colour,2);
    }
    upper_.text(33,210,"SELECT + Y: QUICK MENU / GAME OPTIONS STAY ORIGINAL",muted,1,340,20);
    lower_.text(12,12,"FULL GAME + AUDIO TIMELINE",ink);
    lower_.text(12,38,info_.found?"SLOT "+std::to_string(slot_)+" / GENERATION "+std::to_string(info_.generation):"SLOT "+std::to_string(slot_)+" / EMPTY",selected);
    lower_.text(12,65,"SETTINGS AND EX GAME PROGRESS ARE SEPARATE.\nSAVE WRITES ONLY THIS CARTRIDGE'S SLOT.\nLOAD REPLACES YOUR CURRENT RUNNING GAME.",ink,1,296,50);
    if(confirming_) lower_.text(12,127,selection_==3?"A AGAIN: SAVE OVER THIS SLOT\nB: CANCEL":"A AGAIN: RESTORE SAVED GAME\nB: CANCEL",selected);
    else lower_.text(12,127,"A: CHOOSE  B: RESUME\nD-PAD: MOVE  LEFT/RIGHT: CHANGE SLOT",ink);
    const auto status=message_.empty()?info_.warning:message_;
    if(!status.empty()) lower_.text(12,174,status,selected,1,296,62);
    images_[0]={upper_.view().pixels,top_width,screen_height,top_width*3,3};++redraws_;
}
} // namespace starfox::platform::nintendo_3ds
