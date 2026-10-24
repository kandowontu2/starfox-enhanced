#pragma once
#include "starfox/platform/nintendo_3ds/bg2_tile_plan.hpp"
#include <algorithm>
#include <tuple>

namespace starfox::platform::nintendo_3ds {
// Nonoverlapping textured rectangles can be joined in decoded-map space.
// Keep the original solid/carry suffix in painter order: its zero-ink fills
// must not move underneath the tile whose transparent texels they replace.
inline std::vector<Bg2TileRect> coalesce_bg2_map_rects(const std::vector<Bg2TileRect>& source) {
    std::vector<Bg2TileRect> work;work.reserve(source.size());
    for(const auto& rect:source) if(!rect.solid_index) work.push_back(rect);
    for(unsigned iteration=0;iteration<4;++iteration) {
        const auto before=work.size();
        for(bool vertical:{false,true}) {
            std::sort(work.begin(),work.end(),[&](const auto& a,const auto& b) {
                if(vertical) return std::tuple(a.x,a.width,a.map_x,a.y)<std::tuple(b.x,b.width,b.map_x,b.y);
                return std::tuple(a.y,a.height,a.map_y,a.x)<std::tuple(b.y,b.height,b.map_y,b.x);
            });
            unsigned count=0;
            for(const auto& rect:work) {
                if(count) {
                    auto& old=work[count-1];
                    const bool join=vertical
                        ?old.x==rect.x && old.width==rect.width && old.y+old.height==rect.y
                            && old.map_x==rect.map_x && unsigned(old.map_y)+old.height==rect.map_y
                        :old.y==rect.y && old.height==rect.height && old.x+old.width==rect.x
                            && old.map_y==rect.map_y && unsigned(old.map_x)+old.width==rect.map_x;
                    if(join) {if(vertical) old.height+=rect.height;else old.width+=rect.width;continue;}
                }
                work[count++]=rect;
            }
            work.resize(count);
        }
        if(work.size()==before) break;
    }
    for(const auto& rect:source) if(rect.solid_index) work.push_back(rect);
    return work;
}
}
