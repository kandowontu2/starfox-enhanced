#include "../src/simulation/game_bitmap_font.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using Ram=std::array<std::uint8_t,65536>;
void point_row(Ram& ram,std::uint16_t bitmap,int x,int y,std::uint16_t bits,
    unsigned width,std::uint8_t colour) {
    for(unsigned column=0;column<width && column<16;++column) {
        if(!(bits&(0x8000U>>column))) continue;
        const int px=x+int(column);
        if(px<0 || px>=224 || y<0 || y>=192 || !colour) continue;
        const auto base=std::uint16_t(bitmap+((px>>3)*24+(y>>3))*32+(y&7)*2);
        const auto mask=std::uint8_t(0x80U>>(px&7));
        for(unsigned plane=0;plane<4;++plane) {
            auto& byte=ram[std::uint16_t(base+(plane>>1)*16+(plane&1))];
            if(colour&(1U<<plane)) byte|=mask;
            else byte&=std::uint8_t(~mask);
        }
    }
}
}
int main() {
    try {
        auto expected=std::make_unique<Ram>(),actual=std::make_unique<Ram>();
        std::uint32_t random=0x65c816U;
        for(auto& byte:*expected) {random^=random<<13;random^=random>>17;random^=random<<5;byte=std::uint8_t(random);}
        *actual=*expected;
        std::uint64_t cases=0;
        const auto compare=[&](std::uint16_t bitmap,int x,int y,std::uint16_t bits,
            unsigned width,std::uint8_t colour) {
            point_row(*expected,bitmap,x,y,bits,width,colour);
            starfox::simulation::detail::write_game_font_row(*actual,bitmap,x,y,bits,width,colour);
            if(std::memcmp(actual->data(),expected->data(),actual->size()))
                throw std::runtime_error("Font row changed source pixels, holes, planes or address wrapping");
            ++cases;
        };
        // Every possible glyph row at every sub-tile alignment. Existing RAM
        // is nonzero and retained, exposing incorrect OR-only/clear-only writes.
        for(unsigned alignment=0;alignment<8;++alignment)
            for(unsigned bits=0;bits<=65535;++bits)
                compare(0x8000,80+int(alignment),23,std::uint16_t(bits),16,std::uint8_t(bits%32));
        for(const auto bitmap:{0U,0x7fffU,0x8000U,0xfff0U,0xffffU})
            for(const int x:{-32768,-17,-16,-15,-8,-1,0,1,7,8,15,16,207,208,215,216,223,224,32767})
                for(const int y:{-32768,-1,0,7,8,15,183,184,191,192,32767})
                    for(unsigned width=0;width<=16;++width)
                        for(const auto bits:{0U,1U,0x8000U,0x5555U,0xaaaaU,0xffffU})
                            compare(std::uint16_t(bitmap),x,y,std::uint16_t(bits),width,
                                std::uint8_t((width+bits)%32));
        // Each ink's full low nibble, including nonzero high-only inks, at
        // the last tile and a bitmap origin whose plane addresses wrap.
        for(unsigned colour=0;colour<256;++colour)
            for(unsigned width=0;width<=16;++width)
                compare(0xfff7,217,191,0xa55a,width,std::uint8_t(colour));
        std::cout<<cases<<" exact font-row / planar-memory comparisons passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
