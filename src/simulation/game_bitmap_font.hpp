#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace starfox::simulation::detail {

// The source font is a 16-bit, MSB-first row. Merge its occupied pixels into
// each touched 8-pixel tile row once, preserving transparent glyph holes and
// the column-major, 4-bpp BITMAP1 layout (including 16-bit address wrapping).
inline void write_game_font_row(std::span<std::uint8_t,65536> ram,
    std::uint16_t bitmap,int x,int y,std::uint16_t bits,unsigned width,
    std::uint8_t colour) noexcept {
    if(!bits || !width || !colour || y<0 || y>=192 || x<=-16 || x>=224) return;
    width=std::min(width,16U);
    unsigned first=unsigned(std::max(0,-x));
    const unsigned last=std::min(width,unsigned(224-x));
    while(first<last) {
        const unsigned destination=unsigned(x+int(first));
        const unsigned count=std::min(last-first,8U-(destination&7U));
        const auto mask=std::uint8_t(((unsigned(bits)>>(16U-first-count))
            & ((1U<<count)-1U)) << (8U-(destination&7U)-count));
        if(mask) {
            const auto base=std::uint16_t(bitmap+((destination>>3U)*24U+unsigned(y>>3))*32U
                +unsigned(y&7)*2U);
            for(unsigned plane=0;plane<4;++plane) {
                auto& byte=ram[std::uint16_t(base+(plane>>1U)*16U+(plane&1U))];
                byte=std::uint8_t((byte&~mask)|((colour&(1U<<plane))?mask:0U));
            }
        }
        first+=count;
    }
}

} // namespace starfox::simulation::detail
