#include "starfox/platform/nintendo_3ds/game_remap.hpp"
#include <algorithm>
#include <bit>

namespace starfox::platform::nintendo_3ds {
GameRemap::GameRemap() {
    constexpr std::array<Point3,4> corners{{{0,0,0},{400,0,0},{400,240,0},{0,240,0}}};
    constexpr std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
    unsigned i=0;for(unsigned corner:{0U,1U,2U,0U,2U,3U}) vertices_[i++]={corners[corner],{1,1,1,1},uv[corner]};
    draws_[0]={0,6,0,pica_identity,PicaSpace::screen,false,false,false};
    draws_[0].source_layer=0;
}
void GameRemap::open(const GameBindings& bindings) {
    if(!bindings.valid()) throw std::invalid_argument("Invalid bindings for native controller editor");
    bindings_=bindings;active_=true;capturing_=false;await_release_=true;ambiguous_=false;
    candidate_.reset();selection_=0;previous_=0;redraw();
}
void GameRemap::suspend() {
    capturing_=false;candidate_.reset();ambiguous_=false;await_release_=true;previous_=0;
    if(active_) redraw();
}
bool GameRemap::update(PadSample sample) {
    if(!active_) return false;
    const auto sources=physical_sources(sample,bindings_.deadzone);
    const auto fixed=fixed_menu_buttons(sample);
    if(await_release_) {
        previous_=fixed;
        if(!sources && !fixed) {await_release_=false;previous_=0;}
        return false;
    }
    if(capturing_) {
        // Defer committing until release: B and Start remain individually
        // assignable, but holding them together can cancel without rebinding B.
        if((sample.physical&(input::b|input::start))==(input::b|input::start)) {
            capturing_=false;candidate_.reset();ambiguous_=false;await_release_=true;redraw();return true;
        }
        if(std::popcount(sources)>1 || (candidate_ && sources && sources!=(1U<<*candidate_))) {
            if(!ambiguous_) {ambiguous_=true;candidate_.reset();redraw();return true;}
            return false;
        }
        if(!sources) {
            if(candidate_ && !ambiguous_) {
                bindings_.sources[selection_]=std::uint8_t(*candidate_);
                capturing_=false;candidate_.reset();previous_=fixed;redraw();return true;
            }
            if(ambiguous_) {ambiguous_=false;redraw();return true;}
        } else if(!ambiguous_ && !candidate_) {
            candidate_=unsigned(std::countr_zero(sources));redraw();return true;
        }
        return false;
    }
    const auto pressed=input::ButtonMask(fixed&~previous_);previous_=fixed;
    bool changed=false;
    if(pressed&input::b) {active_=false;return true;}
    if(pressed&input::up) {selection_=(selection_+12)%13;changed=true;}
    else if(pressed&input::down) {selection_=(selection_+1)%13;changed=true;}
    if(pressed&input::y) {bindings_=GameBindings{};changed=true;}
    if(selection_<12) {
        if(pressed&input::x) {bindings_.sources[selection_]=GameBindings::unbound;changed=true;}
        if(pressed&input::a) {capturing_=true;await_release_=true;candidate_.reset();ambiguous_=false;changed=true;}
    } else if(pressed&(input::left|input::right)) {
        const auto step=(pressed&input::left)?-10:10;
        bindings_.deadzone=std::uint8_t(std::clamp(int(bindings_.deadzone)+step,10,100));changed=true;
    }
    if(changed) redraw();
    return changed;
}
PicaFrame GameRemap::frame() const {
    return {plan_frame(0,false,ScreenUse::setup),active_?std::span<const PicaVertex>(vertices_):std::span<const PicaVertex>{},
        active_?std::span<const PicaDraw>(draws_):std::span<const PicaDraw>{},
        active_?std::span<const PicaImage>(images_):std::span<const PicaImage>{},{8,15,28}};
}
void GameRemap::redraw() {
    constexpr Rgb background{8,15,28},edge{228,234,242},shell{159,169,189},inset{103,112,137},button{43,46,75},selected{255,209,70};
    upper_.clear(background);lower_.clear(background);
    upper_.text(77,12,"CONTROLLER / IN-GAME ACTIONS",edge);
    const auto ellipse=[&](int x,int y,int rx,int ry,Rgb colour) {
        for(int row=-ry;row<=ry;++row) for(int col=-rx;col<=rx;++col)
            if(std::int64_t(col)*col*ry*ry+std::int64_t(row)*row*rx*rx<=std::int64_t(rx)*rx*ry*ry)
                upper_.rectangle(x+col,y+row,1,1,colour);
    };
    const auto label=[&](std::string_view value,int x,int y,Rgb colour) {
        upper_.text(x-int(value.size())*3,y,value,colour);
    };
    const auto chosen=highlighted_action();
    const auto ink=[&](input::ButtonMask action) {return chosen==action?selected:button;};
    upper_.rectangle(121,54,44,13,edge);upper_.rectangle(122,55,42,11,ink(input::left_shoulder));
    upper_.rectangle(235,54,44,13,edge);upper_.rectangle(236,55,42,11,ink(input::right_shoulder));
    label("L",143,56,edge);label("R",257,56,edge);
    ellipse(200,102,96,39,edge);ellipse(200,102,94,37,shell);ellipse(200,101,90,33,inset);
    ellipse(146,103,39,31,shell);ellipse(254,103,39,31,shell);
    upper_.rectangle(137,85,18,36,edge);upper_.rectangle(128,94,36,18,edge);
    upper_.rectangle(139,87,14,32,button);upper_.rectangle(130,96,32,14,button);
    upper_.rectangle(139,87,14,11,ink(input::up));upper_.rectangle(139,108,14,11,ink(input::down));
    upper_.rectangle(130,96,11,14,ink(input::left));upper_.rectangle(151,96,11,14,ink(input::right));
    upper_.rectangle(142,99,8,8,inset);
    for(auto system:{std::pair{178,input::ButtonMask(input::select)},std::pair{217,input::ButtonMask(input::start)}}) {
        ellipse(system.first,109,12,7,edge);ellipse(system.first,109,10,5,ink(system.second));
    }
    label("SEL",178,122,edge);label("START",217,122,edge);
    const auto face=[&](int x,int y,std::string_view name,input::ButtonMask action) {
        ellipse(x,y,11,11,edge);ellipse(x,y,9,9,ink(action));label(name,x,y-3,chosen==action?button:edge);
    };
    face(259,84,"X",input::x);face(278,102,"A",input::a);face(259,120,"B",input::b);face(240,102,"Y",input::y);
    const auto action=selection_<12?std::string(game_action_names[selection_]):"CIRCLE DEADZONE";
    const auto bound=selection_<12?std::string(binding_name(bindings_.sources[selection_])):std::to_string(bindings_.deadzone);
    upper_.text(88,163,"ACTION: "+action,selected);upper_.text(88,180,"PHYSICAL: "+bound,edge);
    upper_.text(25,210,"SNES ACTION HIGHLIGHT / FIXED MENU NAVIGATION",shell,1,350,20);
    for(unsigned row=0;row<13;++row) {
        const auto y=10+int(row)*13;const auto colour=row==selection_?selected:edge;
        if(row==selection_) lower_.text(7,y,">",selected);
        lower_.text(22,y,row<12?game_action_names[row]:"DEADZONE",colour);
        lower_.text(137,y,row<12?binding_name(bindings_.sources[row]):std::string_view{},colour);
        if(row==12) lower_.text(137,y,std::to_string(bindings_.deadzone),colour);
    }
    if(capturing_) {
        lower_.text(8,188,ambiguous_?"ONE CONTROL ONLY; RELEASE TO RETRY":candidate_?"RELEASE TO COMMIT THIS CONTROL":"PRESS ONE BUTTON OR MOVE CIRCLE PAD",selected);
        lower_.text(8,204,"B + START: CANCEL CAPTURE",edge);
    } else lower_.text(8,188,"A: ASSIGN  X: CLEAR  Y: DEFAULTS\nB: BACK  D-PAD: CHOOSE / DEADZONE",edge);
    lower_.text(8,223,"MENU BUTTONS STAY FIXED",shell);
    const auto image=upper_.view();images_[0]={image.pixels,top_width,screen_height,top_width*3,3};++redraws_;
}
} // namespace starfox::platform::nintendo_3ds
