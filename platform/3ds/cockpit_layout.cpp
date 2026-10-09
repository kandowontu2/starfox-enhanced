#include "starfox/platform/nintendo_3ds/frontend.hpp"

namespace starfox::platform::nintendo_3ds {
namespace {
constexpr Rgb edge{126,158,177},ink{6,18,28},label{213,237,244};
void meter(Canvas& c,int x,int y,unsigned w,unsigned value,Rgb colour) {
    c.rectangle(x,y,w,9,edge);c.rectangle(x+2,y+2,w-4,5,ink);
    c.rectangle(x+2,y+2,int((w-4)*std::min(100U,value)/100),5,colour);
}
bool draw_widget(Canvas& c,HudWidget widget,const HudState& s) {
    switch(widget) {
    case HudWidget::radio:
        c.rectangle(0,0,304,70,edge);c.rectangle(2,2,300,66,ink);
        if(s.radio_artwork.width) c.image(10,8,s.radio_artwork);
        else c.text(10,8,s.radio_message.empty()?"RADIO / STANDBY":s.radio_message,label,2,284,56);
        break;
    case HudWidget::portrait:
        c.rectangle(0,0,74,74,edge);c.rectangle(2,2,70,70,ink);
        if(s.portrait.width) c.image(5,5,s.portrait);else c.text(9,31,"COMMS",edge,2);
        break;
    case HudWidget::ally_one:case HudWidget::ally_two:case HudWidget::ally_three: {
        const auto id=unsigned(widget)-unsigned(HudWidget::ally_one);
        if(!s.ally_percent[id]) return false;
        meter(c,0,0,id==2?64:62,*s.ally_percent[id],{86,208,168});break;
    }
    case HudWidget::shield:
        if(!s.meters_enabled) return false;
        c.text(0,0,"SHIELD",label,1,96,8);meter(c,0,12,96,s.shield_percent,{239,90,99});break;
    case HudWidget::boost:
        if(!s.meters_enabled || !s.boost_enabled) return false;
        c.text(36,0,"BOOST",label,1,60,8);meter(c,0,12,96,s.boost_percent,{88,160,244});break;
    case HudWidget::boss:
        if(!s.meters_enabled || !s.boss_percent) return false;
        c.text(0,0,"ENEMY",label,1,82,8);meter(c,0,12,82,*s.boss_percent,{246,204,75});break;
    case HudWidget::lives:case HudWidget::bombs:
        if(!s.counters_enabled) return false;
        c.text(widget==HudWidget::bombs && !s.second_player_view?32:0,0,
            std::string(s.second_player_view?"P2 ":"")+(widget==HudWidget::lives?"LIVES ":"BOMBS ")
                +std::to_string(widget==HudWidget::lives?s.lives:s.bombs),label,1,108,8);break;
    case HudWidget::second_shield:
        if(!s.meters_enabled || !s.second_shield_percent) return false;
        c.text(0,0,"P2",label,1,20,8);meter(c,20,2,76,*s.second_shield_percent,{86,208,168});break;
    case HudWidget::second_lives:case HudWidget::second_bombs:
        if(!s.counters_enabled || !s.second_counters) return false;
        c.text(0,0,std::string(widget==HudWidget::second_lives?"P2 LIVES ":"P2 BOMBS ")
            +std::to_string(widget==HudWidget::second_lives?s.second_counters->lives:s.second_counters->bombs),label,1,108,8);break;
    default:throw std::invalid_argument("Unknown native HUD widget");
    }
    return true;
}
}
std::size_t CockpitWidgets::bytes() const noexcept {
    std::size_t bytes=0;for(const auto& panel:panels_) if(panel) bytes+=panel->view().pixels.size()+panel->coverage().size();return bytes;
}
void CockpitWidgets::draw(Canvas& destination,const HudState& s) {
    if(destination.view().width!=bottom_width || destination.view().height!=screen_height || !s.layout.valid()
        || (s.portrait.width && (s.portrait.width>64 || s.portrait.height>64 || !valid_image(s.portrait,s.portrait.width,s.portrait.height)))
        || (s.radio_artwork.width && (s.radio_artwork.width>284 || s.radio_artwork.height>56 || !valid_image(s.radio_artwork,s.radio_artwork.width,s.radio_artwork.height))))
        throw std::invalid_argument("Invalid isolated native HUD inputs");
    // Allocate all potential panels before touching the published lower LCD.
    for(unsigned id=0;id<hud_widget_count;++id) if(s.layout.widgets[id].visible && !panels_[id])
        panels_[id]=std::make_unique<Canvas>(hud_widget_sizes[id][0],hud_widget_sizes[id][1]);
    std::array<bool,hud_widget_count> active{};
    for(unsigned id=0;id<hud_widget_count;++id) if(s.layout.widgets[id].visible) {
        panels_[id]->begin_artwork();active[id]=draw_widget(*panels_[id],static_cast<HudWidget>(id),s);
    }
    destination.clear({15,29,42});
    for(int y=83;y<240;++y) {
        const int slope=std::min(92,(y-83)*2/3);
        destination.rectangle(0,y,112-slope,1,{51,66,82});destination.rectangle(208+slope,y,112-slope,1,{51,66,82});
        destination.line(111-slope,y,111-slope,y,edge);destination.line(208+slope,y,208+slope,y,edge);
    }
    for(unsigned id=0;id<hud_widget_count;++id) {
        const auto& p=s.layout.widgets[id];if(!active[id]) continue;
        const auto& panel=*panels_[id];destination.scaled_artwork(p.x,p.y,panel.view(),panel.coverage(),p.quarters);
    }
}
} // namespace starfox::platform::nintendo_3ds
