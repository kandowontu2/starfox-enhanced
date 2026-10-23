#include "../platform/3ds/reference/bg2_plan.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace starfox;
using platform_3ds::Bg2Rect;
std::uint64_t checks{};
void require(bool condition,const char* message) {
    ++checks;if(!condition) throw std::runtime_error(message);
}
void word(simulation::SnesPpuState& ppu,unsigned address,unsigned value) {
    const auto byte=(address&32767)*2;
    ppu.vram[byte]=std::uint8_t(value);ppu.vram[byte+1]=std::uint8_t(value>>8);
}
std::unique_ptr<simulation::SnesPpuState> fixture(unsigned number) {
    auto ppu=std::make_unique<simulation::SnesPpuState>();
    ppu->main_screen=2;ppu->bg2_screen_base=0x6000;
    ppu->bg2_character_base=(number&32)?0x7ff8:0x1000;ppu->bg2_screen_size=std::uint8_t(number%4);
    ppu->bg2_tile_size_16=(number&8)!=0;
    for(unsigned character=0;character<64;++character) {
        for(unsigned row=0;row<8;++row) for(unsigned plane=0;plane<4;++plane) {
            const unsigned offset=(plane/2)*16+(plane%2)+row*2;
            auto value=std::uint8_t((character&(1<<plane))?255:0);
            if(character>=32 && plane==0) value=std::uint8_t(0x81^(row*11));
            ppu->vram[(ppu->bg2_character_base*2+character*32+offset)&65535]=value;
        }
    }
    for(unsigned entry=0;entry<4096;++entry) {
        const unsigned character=number==0?1:number==1?0:
            ((entry*13+entry/32)%((number&4)?64:16));
        const unsigned tile=character|(((entry/32)%8)<<10)
            |((entry&1)?0x4000:0)|((entry&2)?0x8000:0);
        word(*ppu,ppu->bg2_screen_base+entry,tile);
    }
    ppu->bg2_horizontal_offsets_enabled=true;
    ppu->bg2_scanline_scroll_enabled=true;
    for(unsigned y=0;y<224;++y) {
        ppu->bg2_horizontal_offsets[y]=std::int16_t(y%2?3:-5);
        ppu->bg2_scanline_scroll_y[y]=std::int16_t((int(y)*7)%512-128);
    }
    if(number&16) {
        ppu->bg2_vertical_offsets_enabled=true;
        for(unsigned column=0;column<32;++column)
            word(*ppu,0x2fa0+column,0x4000|((340+int(column)*int(number%7+1))&8191));
    }
    return ppu;
}
// Independent texture sampling: neither uniform detection nor span merging is
// shared with the planner. Index and coverage must both survive compaction.
using Pixel=std::array<std::uint8_t,2>;
std::vector<Pixel> raster(const simulation::SnesPpuState& ppu,
    const std::vector<Bg2Rect>& plan,unsigned width,unsigned height) {
    std::vector<Pixel> pixels(std::size_t(width)*height);
    for(const auto& rect:plan) {
        require(rect.width && rect.height && rect.x+rect.width<=width
            && rect.y+rect.height<=height,"Compacted rectangle escaped target");
        for(unsigned y=0;y<rect.height;++y) for(unsigned x=0;x<rect.width;++x) {
            auto& pixel=pixels[std::size_t(rect.y+y)*width+rect.x+x];
            if(rect.solid) {pixel={rect.solid_colour,1};continue;}
            const int sx=int(rect.source_x)+(rect.reverse_x?-int(x):int(x));
            const int sy=int(rect.source_y)+(rect.reverse_y?-int(y):int(y));
            require(sx>=0 && sx<8 && sy>=0 && sy<8,"Textured fragment left its character");
            const unsigned base=ppu.bg2_character_base*2+rect.character*32+unsigned(sy)*2;
            unsigned ink{};
            for(unsigned plane=0;plane<4;++plane) {
                const unsigned offset=(plane/2)*16+(plane%2);
                if(ppu.vram[(base+offset)&65535]&(0x80>>sx)) ink|=1<<plane;
            }
            if(ink) pixel={std::uint8_t(rect.palette_bank*16+ink),1};
        }
    }
    return pixels;
}
void parity(unsigned number,unsigned width,unsigned height,int origin_x,int origin_y) {
    const auto ppu=fixture(number);const auto before=*ppu;
    std::vector<Bg2Rect> raw,compact,uncached;
    const int sx=int(number)*19-137,sy=int(number)*29-211;
    require(platform_3ds::plan_bg2_rects(*ppu,sx,sy,raw,192000,width,height,origin_x,origin_y,true,false),
        "Unmerged oracle unexpectedly rejected synthetic scene");
    require(platform_3ds::plan_bg2_rects(*ppu,sx,sy,compact,192000,width,height,origin_x,origin_y,true,true),
        "Compacted planner rejected synthetic scene");
    require(platform_3ds::plan_bg2_rects(*ppu,sx,sy,uncached,192000,width,height,origin_x,origin_y,false,true),
        "Uncached carry rejected synthetic scene");
    const auto expected=raster(*ppu,raw,width,height);
    const auto actual=raster(*ppu,compact,width,height);
    const auto without_cache=raster(*ppu,uncached,width,height);
    for(std::size_t i=0;i<expected.size();++i) {
        require(actual[i]==expected[i],"Compaction changed source ink/alpha/roll/priority");
        require(without_cache[i]==expected[i],"Uniform cache changed rolled carry pixels");
    }
    require(*ppu==before,"Planner modified borrowed PPU snapshot");
    const auto exact_capacity=compact.size();
    require(platform_3ds::plan_bg2_rects(*ppu,sx,sy,compact,exact_capacity,width,height,origin_x,origin_y),
        "Exact compacted capacity was incorrectly rejected");
    require(!platform_3ds::plan_bg2_rects(*ppu,sx,sy,uncached,exact_capacity-1,width,height,origin_x,origin_y)
        && uncached.empty(),"Undersized capacity retained partial geometry");
    if(number==1) require(std::none_of(compact.begin(),compact.end(),[](const auto& r) {return r.solid;}),
        "Transparent characters became opaque solids");
    if(number==0 && width==400) {
        require(raw.size()>16383,"Budget regression fixture no longer exceeds raw limit");
        require(compact.size()<2000,"Flat scanline strips were not merged before capacity");
        require(platform_3ds::plan_bg2_rects(*ppu,sx,sy,compact,16383,width,height,origin_x,origin_y),
            "Flat scene still forced a complete CPU fallback");
        require(!platform_3ds::plan_bg2_rects(*ppu,sx,sy,raw,16383,width,height,origin_x,origin_y,true,false)
            && raw.empty(),"Rejected unmerged scene leaked partial output");
    }
    compact.push_back({});
    require(!platform_3ds::plan_bg2_rects(*ppu,sx,sy,compact,0,width,height,origin_x,origin_y)
        && compact.empty(),"Zero capacity leaked a partial compacted scene");
}
}
int main() {
    try {
        for(unsigned number=0;number<64;++number) {
            parity(number,400,240,72,8);
            parity(number,256,224,0,0);
        }
        auto ppu=fixture(0);std::vector<Bg2Rect> output(1);
        ppu->background_mode=1;
        require(!platform_3ds::plan_bg2_rects(*ppu,0,0,output) && output.empty(),"Mode rejection retained partial output");
        ppu->background_mode=2;ppu->mosaic=2;
        require(!platform_3ds::plan_bg2_rects(*ppu,0,0,output) && output.empty(),"Mosaic rejection retained partial output");
        std::cout<<"Reference BG2 compaction: "<<checks<<" pixel/coverage/budget checks passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
