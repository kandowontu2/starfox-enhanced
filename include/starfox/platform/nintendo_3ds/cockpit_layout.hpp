#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace starfox::platform::nintendo_3ds {
enum class HudWidget : unsigned {radio,portrait,ally_one,ally_two,ally_three,shield,boost,boss,lives,bombs,second_shield,second_lives,second_bombs,count};
inline constexpr unsigned hud_widget_count=unsigned(HudWidget::count);
inline constexpr std::array<std::string_view,hud_widget_count> hud_widget_names{
    "RADIO","PORTRAIT","FALCO","PEPPY","SLIPPY","SHIELD","BOOST","BOSS","LIVES","BOMBS","P2 SHIELD","P2 LIVES","P2 BOMBS"};
inline constexpr std::array<std::array<unsigned,2>,hud_widget_count> hud_widget_sizes{{
    {304,70},{74,74},{62,9},{62,9},{64,9},{96,21},{96,21},{82,21},{108,8},{108,8},{96,11},{108,8},{108,8}}};
struct HudPlacement {
    std::uint16_t x{},y{};
    std::uint8_t quarters{4}; // 2..8, 50..200%; extent must still fit the lower LCD.
    bool visible{true};
    bool operator==(const HudPlacement&) const=default;
};
struct CockpitLayout {
    std::array<HudPlacement,hud_widget_count> widgets{{
        {8,8},{123,99},{17,100},{241,100},{128,181},{12,200},{212,200},{119,214},
        {12,182},{212,182},{12,224},{12,164},{212,164}}};
    [[nodiscard]] bool valid() const noexcept {
        for(unsigned i=0;i<widgets.size();++i) {
            const auto& p=widgets[i];
            const unsigned w=(hud_widget_sizes[i][0]*p.quarters+3)/4,h=(hud_widget_sizes[i][1]*p.quarters+3)/4;
            if(p.quarters<2 || p.quarters>8 || w>320 || h>240 || p.x>320-w || p.y>240-h) return false;
        }
        return true;
    }
    bool operator==(const CockpitLayout&) const=default;
};
inline std::array<unsigned,2> hud_widget_extent(unsigned id,const HudPlacement& p) {
    return {(hud_widget_sizes.at(id)[0]*p.quarters+3)/4,(hud_widget_sizes.at(id)[1]*p.quarters+3)/4};
}
inline void bound_hud_placement(unsigned id,HudPlacement& p) {
    p.quarters=std::clamp<unsigned>(p.quarters,2,8);
    while(hud_widget_extent(id,p)[0]>320 || hud_widget_extent(id,p)[1]>240) --p.quarters;
    const auto extent=hud_widget_extent(id,p);
    p.x=std::min<unsigned>(p.x,320-extent[0]);p.y=std::min<unsigned>(p.y,240-extent[1]);
}
} // namespace starfox::platform::nintendo_3ds
