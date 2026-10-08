#include "starfox/platform/nintendo_3ds/game_hud_editor.hpp"

namespace starfox::platform::nintendo_3ds {
GameHudEditor::GameHudEditor() {
    constexpr std::array<Point3,4> corners{{{0,0,0},{400,0,0},{400,240,0},{0,240,0}}};
    constexpr std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
    unsigned i=0;for(unsigned corner:{0U,1U,2U,0U,2U,3U}) vertices_[i++]={corners[corner],{1,1,1,1},uv[corner]};
    draws_[0]={0,6,0,pica_identity,PicaSpace::screen,false,false,false};draws_[0].source_layer=0;
}
void GameHudEditor::open(const CockpitLayout& layout) {
    if(!layout.valid()) throw std::invalid_argument("Invalid native HUD editor layout");
    layout_=layout;selection_=0;active_=true;applied_=false;await_release_=true;dragging_=false;touch_blocked_=false;previous_=0;redraw();
}
bool GameHudEditor::update(PadSample pad,HudTouch touch) {
    if(!active_) return false;
    const auto fixed=fixed_menu_buttons(pad);
    if(await_release_) {if(!fixed && !touch.held) {await_release_=false;previous_=0;}return false;}
    const auto pressed=input::ButtonMask(fixed&~previous_);previous_=fixed;
    if(pressed&input::b) {active_=false;applied_=false;dragging_=false;return true;}
    if(pressed&input::start) {active_=false;applied_=true;dragging_=false;return true;}
    const auto before=layout_;const auto selected=selection_;
    if(pressed&input::left_shoulder) selection_=(selection_+hud_widget_count-1)%hud_widget_count;
    else if(pressed&input::right_shoulder) selection_=(selection_+1)%hud_widget_count;
    if(pressed&input::y) layout_=CockpitLayout{};
    auto& p=layout_.widgets[selection_];
    if(pressed&input::x) p.visible=!p.visible;
    if(fixed&input::a) {
        if(pressed&input::up) p.quarters=std::min<unsigned>(8,p.quarters+1);
        else if(pressed&input::down) p.quarters=std::max(2,int(p.quarters)-1);
        bound_hud_placement(selection_,p);
    } else {
        const int step=(fixed&input::select)?1:4;
        const auto extent=hud_widget_extent(selection_,p);
        p.x=std::clamp(int(p.x)+((pressed&input::right)?step:0)-((pressed&input::left)?step:0),0,int(320-extent[0]));
        p.y=std::clamp(int(p.y)+((pressed&input::down)?step:0)-((pressed&input::up)?step:0),0,int(240-extent[1]));
    }
    // A shoulder/resize/hide/reset action during a drag must not transfer the
    // old touch anchor to a different or resized widget. Begin a new gesture
    // only after the finger/stylus has been released.
    if(touch.held && dragging_ && (before!=layout_ || selected!=selection_)) {
        dragging_=false;touch_blocked_=true;
    }
    if(!touch.held) touch_blocked_=false;
    if(!touch.held || touch_blocked_ || touch.x<0 || touch.x>=320 || touch.y<0 || touch.y>=240) dragging_=false;
    else {
        if(!dragging_) {
            // Selected widget wins overlapping hit tests; otherwise last
            // painted widget wins. Hidden widgets remain selectable with L/R.
            const auto hit=[&](unsigned id) {
                const auto& item=layout_.widgets[id];const auto extent=hud_widget_extent(id,item);
                return item.visible && touch.x>=item.x && touch.y>=item.y
                    && unsigned(touch.x)<item.x+extent[0] && unsigned(touch.y)<item.y+extent[1];
            };
            std::optional<unsigned> candidate;
            if(hit(selection_)) candidate=selection_;
            else for(unsigned i=hud_widget_count;i>0;--i) if(hit(i-1)) {candidate=i-1;break;}
            if(candidate) {
                selection_=*candidate;const auto& item=layout_.widgets[selection_];
                drag_x_=touch.x-item.x;drag_y_=touch.y-item.y;dragging_=true;
            }
        }
        if(dragging_) {
            auto& item=layout_.widgets[selection_];const auto extent=hud_widget_extent(selection_,item);
            item.x=std::clamp(touch.x-drag_x_,0,int(320-extent[0]));item.y=std::clamp(touch.y-drag_y_,0,int(240-extent[1]));
        }
    }
    if(before==layout_ && selected==selection_) return false;
    redraw();return true;
}
PicaFrame GameHudEditor::frame() const {
    return {plan_frame(0,false,ScreenUse::setup),active_?std::span<const PicaVertex>(vertices_):std::span<const PicaVertex>{},
        active_?std::span<const PicaDraw>(draws_):std::span<const PicaDraw>{},active_?std::span<const PicaImage>(images_):std::span<const PicaImage>{}};
}
void GameHudEditor::redraw() {
    HudState sample;sample.layout=layout_;sample.shield_percent=76;sample.boost_percent=92;sample.lives=2;sample.bombs=3;
    sample.boss_percent=67;sample.ally_percent={84,58,95};sample.second_shield_percent=61;sample.second_counters=HudCounters{6,2};
    sample.radio_message="SAMPLE RADIO\nHUD LAYOUT PREVIEW";
    preview_.update(sample);lower_.image(0,0,preview_.view());
    const auto& p=layout_.widgets[selection_];const auto extent=hud_widget_extent(selection_,p);
    constexpr Rgb selected{255,209,70},text{213,237,244};
    lower_.line(p.x,p.y,p.x+extent[0]-1,p.y,selected);lower_.line(p.x,p.y,p.x,p.y+extent[1]-1,selected);
    lower_.line(p.x+extent[0]-1,p.y,p.x+extent[0]-1,p.y+extent[1]-1,selected);
    lower_.line(p.x,p.y+extent[1]-1,p.x+extent[0]-1,p.y+extent[1]-1,selected);
    upper_.clear({8,15,28});upper_.text(34,14,"CUSTOMIZE LOWER-SCREEN HUD",text,2,340,30);
    upper_.text(30,53,std::string(hud_widget_names[selection_])+"  "+std::to_string(selection_+1)+"/"+std::to_string(hud_widget_count),selected,2,340,26);
    upper_.text(30,90,"X "+std::to_string(p.x)+"  Y "+std::to_string(p.y)+"  SIZE "+std::to_string(p.quarters*25)+"%  "+(p.visible?"VISIBLE":"HIDDEN"),text);
    upper_.text(30,113,"TOUCH: DRAG   L/R: CHOOSE\nD-PAD: MOVE   SELECT: FINE MOVE\nHOLD A + UP/DOWN: RESIZE\nX: HIDE/SHOW   Y: RESET ALL\nSTART: APPLY   B: CANCEL",text,1,340,60);
    upper_.text(30,189,"ILLUSTRATIVE HUD / GAME AND AUDIO PAUSED\nSAVED SEPARATELY FROM DESKTOP LAYOUT\nDEFAULT COCKPIT REMAINS UNCHANGED",{159,169,189},1,340,38);
    images_[0]={upper_.view().pixels,top_width,screen_height,top_width*3,3};++redraws_;
}
} // namespace starfox::platform::nintendo_3ds
