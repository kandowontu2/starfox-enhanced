// Starwing 3DS tile/carry strategy, adapted for exact expanded source roll,
// painter priorities and bounded uniform rectangles. See UPSTREAM-STARWING.md.
#include "starfox/platform/nintendo_3ds/bg2_tile_plan.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace starfox::platform::nintendo_3ds {
namespace {
std::uint16_t word(const simulation::SnesPpuState& ppu,unsigned address) {
    const unsigned at=(address&32767)*2;
    return std::uint16_t(ppu.vram[at]|(unsigned(ppu.vram[at+1])<<8));
}
struct SolidRow {unsigned x{},width{};std::uint8_t ink{};};
using Rows=std::array<std::vector<SolidRow>,224>;
bool append(Rows& rows,unsigned x,unsigned y,unsigned width,std::uint8_t ink,unsigned& count,unsigned capacity) {
    if(!ink) return true;
    auto& row=rows[y];
    if(!row.empty() && row.back().ink==ink && row.back().x+row.back().width==x) row.back().width+=width;
    else {if(count==std::max(2048U,capacity*8)) return false;row.push_back({x,width,ink});++count;}
    return true;
}
bool emit_rows(const Rows& rows,std::vector<Bg2TileRect>& output,unsigned capacity) {
    struct Link {SolidRow row;unsigned index;};
    std::vector<Link> previous,current;
    for(unsigned y=0;y<224;++y) {
        current.clear();current.reserve(rows[y].size());unsigned old=0;
        for(const auto& row:rows[y]) {
            while(old<previous.size() && previous[old].row.x<row.x) ++old;
            unsigned index=unsigned(output.size());
            if(old<previous.size() && previous[old].row.x==row.x && previous[old].row.width==row.width
                && previous[old].row.ink==row.ink) {index=previous[old].index;++output[index].height;}
            else {
                if(output.size()==capacity) return false;
                Bg2TileRect rect;rect.x=row.x;rect.y=y;rect.width=row.width;rect.height=1;rect.solid_index=row.ink;
                output.push_back(rect);
            }
            current.push_back({row,index});
        }
        previous.swap(current);
    }
    return true;
}
}
bool plan_rolled_bg2_tiles(const simulation::SnesPpuState& ppu,int scroll_x,int scroll_y,
    unsigned width,int origin,int priority,std::vector<Bg2TileRect>& output,unsigned capacity) {
    output.clear();
    if((ppu.background_mode!=1 && ppu.background_mode!=2) || (ppu.mosaic&2) || ppu.tunnel_scene || !width || width>4096 || origin<0 || origin>4096
        || priority< -1 || priority>1 || scroll_x< -32768 || scroll_x>32767 || scroll_y< -32768 || scroll_y>32767
        || capacity>16384) return false;
    if(!(ppu.main_screen&2)) return true;
    const unsigned columns=(ppu.bg2_screen_size&1)?64:32,rows=(ppu.bg2_screen_size&2)?64:32;
    const unsigned edge=ppu.bg2_tile_size_16?16:8,shift=ppu.bg2_tile_size_16?4:3;
    const unsigned x_mask=columns*edge-1,y_mask=rows*edge-1,pages_wide=columns/32;
    constexpr int missing=std::numeric_limits<int>::min();
    const bool vertical=ppu.background_mode==2 && ppu.bg2_vertical_offsets_enabled;
    const bool expanded=width>256 && vertical;
    const bool water_margin=ppu.background_mode==1 && ppu.bg2_scanline_scroll_enabled && width>256;
    std::array<std::uint16_t,32> offsets{};
    if(vertical) for(unsigned i=0;i<32;++i) offsets[i]=word(ppu,0x2fa0+i);
    const auto difference=[](int a,int b){int delta=(a-b)&8191;return delta>4095?delta-8192:delta;};
    unsigned first=32,last=32,count=0;double sx=0,sy=0,sxx=0,sxy=0;int raw_previous=0,unwrapped=0;
    for(unsigned i=0;i<32;++i) if(offsets[i]&0x4000) {
        if(first==32) first=i;
        last=i;
        const int raw=offsets[i]&8191;unwrapped=count?unwrapped+difference(raw,raw_previous):raw;raw_previous=raw;
        const double x=i+1;++count;sx+=x;sy+=unwrapped;sxx+=x*x;sxy+=x*unwrapped;
    }
    const double denominator=count*sxx-sx*sx;
    const double slope=count>1 && denominator!=0?(count*sxy-sx*sy)/denominator:0;
    const double intercept=count?(sy-slope*sx)/count:0;
    const int delta=first<32 && last!=first?difference(offsets[last]&8191,offsets[first]&8191):0;
    const int span=first<32 && last!=first?int(last-first):1;
    const auto wrap=[](int value){value%=8192;return value<0?value+8192:value;};
    std::vector<int> column_y(width,missing);
    for(unsigned x=0;x<width && vertical;++x) {
        const int coordinate=int(x)-origin+(scroll_x&7);
        if(expanded && count) column_y[x]=wrap(int(std::lround(intercept+slope*double(coordinate)/8.)));
        else {
            const int column=coordinate>=0?coordinate/8:-((-coordinate+7)/8);
            if(column>=1 && column<=32) {if(offsets[unsigned(column-1)]&0x4000) column_y[x]=offsets[unsigned(column-1)]&8191;}
            else if(column<=0 && (offsets[0]&0x4000)) column_y[x]=wrap((offsets[0]&8191)+delta*(expanded?column-1:std::min(column+1,0))/span);
            else if(column>32 && (offsets[31]&0x4000)) column_y[x]=wrap((offsets[31]&8191)+delta*(column-32)/span);
        }
    }
    const auto row_x=[&](unsigned y){return ppu.bg2_horizontal_offsets_enabled?int(ppu.bg2_horizontal_offsets[y]):scroll_x;};
    const auto row_y=[&](unsigned y){return ppu.bg2_scanline_scroll_enabled?int(ppu.bg2_scanline_scroll_y[y]):scroll_y;};
    const auto source_y=[&](unsigned x,unsigned y){return unsigned(int(y)+(column_y[x]!=missing?column_y[x]:row_y(y)))&y_mask;};
    std::vector<unsigned> wrapped_at(width,224);
    if(expanded) for(unsigned x=0;x<width;++x) {
        if(column_y[x]!=missing || !ppu.bg2_scanline_scroll_enabled) {
            const unsigned wrap_y=y_mask+1-source_y(x,0);
            if(wrap_y>=144 && wrap_y<224) wrapped_at[x]=wrap_y;
        } else {
            auto previous=source_y(x,143);
            for(unsigned y=144;y<224;++y) {const auto next=source_y(x,y);if(next<previous && previous-next>(y_mask+1)/2) {wrapped_at[x]=y;break;}previous=next;}
        }
    }
    std::array<std::int8_t,1024> uniform;uniform.fill(-2);
    const auto uniform_ink=[&](unsigned character) {
        auto& result=uniform[character];if(result!=-2) return int(result);
        const unsigned base=ppu.bg2_character_base*2+character*32;
        int ink=-2;
        for(unsigned y=0;y<8;++y) {
            int row=0;
            for(unsigned plane=0;plane<4;++plane) {
                const auto byte=ppu.vram[(base+y*2+(plane&1)+(plane>=2?16:0))&65535];
                if(byte!=0 && byte!=255) {result=-1;return -1;}
                if(byte) row|=1<<plane;
            }
            if(ink==-2) ink=row;else if(ink!=row) {result=-1;return -1;}
        }
        result=std::int8_t(ink);return ink;
    };
    const auto tile_sample=[&](unsigned sx_value,unsigned sy_value) {
        const unsigned tx=sx_value>>shift,ty=sy_value>>shift;
        const auto tile=word(ppu,unsigned(ppu.bg2_screen_base)+((tx>>5)+(ty>>5)*pages_wide)*1024+(ty&31)*32+(tx&31));
        unsigned px=sx_value&(edge-1),py=sy_value&(edge-1),character=tile&1023;
        const bool fx=(tile&0x4000)!=0,fy=(tile&0x8000)!=0;
        if(fx) px=edge-1-px;
        if(fy) py=edge-1-py;
        if(ppu.bg2_tile_size_16) character=(character+(px>=8?1:0)+(py>=8?16:0))&1023;
        return std::array<unsigned,6>{character,unsigned((tile>>10)&7),px&7,py&7,unsigned(fx),unsigned(fy)};
    };
    const auto selected=[&](unsigned sx_value,unsigned sy_value) {
        if(priority<0) return true;
        const unsigned tx=sx_value>>shift,ty=sy_value>>shift;
        const auto tile=word(ppu,unsigned(ppu.bg2_screen_base)+((tx>>5)+(ty>>5)*pages_wide)*1024+(ty&31)*32+(tx&31));
        return bool(tile&0x2000)==bool(priority);
    };
    const auto colour_at=[&](unsigned x,unsigned y) {
        const unsigned sx_value=unsigned(int(x)-origin+row_x(y))&x_mask,sy_value=source_y(x,y);
        if(!selected(sx_value,sy_value)) return std::uint8_t{};
        const auto sample=tile_sample(sx_value,sy_value);int ink=uniform_ink(sample[0]);
        if(ink<0) {
            const unsigned base=ppu.bg2_character_base*2+sample[0]*32+sample[3]*2,mask=128>>sample[2];ink=0;
            for(unsigned plane=0;plane<4;++plane) if(ppu.vram[(base+(plane&1)+(plane>=2?16:0))&65535]&mask) ink|=1<<plane;
        }
        return ink?std::uint8_t(sample[1]*16+unsigned(ink)):std::uint8_t{};
    };
    std::vector<Bg2TileRect> next;next.reserve(std::min(capacity,2048U));Rows solids,carry;unsigned solid_count=0,carry_count=0;
    for(unsigned band_top=0;band_top<224;) {
        const int band_x=row_x(band_top),band_y=row_y(band_top);unsigned band_end=band_top+1;
        while(band_end<224 && row_x(band_end)==band_x && row_y(band_end)==band_y) ++band_end;
        for(unsigned x=0;x<width;) {
            const int logical=int(x)-origin;
            const bool margin=water_margin && (logical<0 || logical>=256);
            const int unwrapped=int(unsigned(128+band_x)&x_mask)+logical-128;
            const unsigned sx_value=margin?unsigned(std::clamp(unwrapped,0,int(x_mask))):unsigned(logical+band_x)&x_mask;
            const bool constant_x=margin && (unwrapped<0 || unwrapped>=int(x_mask));
            unsigned width_here=std::min(width-x,8-(sx_value&7));
            if(margin) {
                const unsigned boundary=logical<0?std::min(width,unsigned(origin)):width;
                if(constant_x) width_here=std::min(boundary-x,unwrapped<0?unsigned(1-unwrapped):boundary-x);
                else width_here=std::min(width_here,boundary-x);
            } else if(water_margin) width_here=std::min(width_here,unsigned(origin+256)-x);
            for(unsigned dx=1;dx<width_here;++dx) if(column_y[x+dx]!=column_y[x] || wrapped_at[x+dx]!=wrapped_at[x]) {width_here=dx;break;}
            for(unsigned y=band_top;y<band_end && y<wrapped_at[x];) {
                const unsigned sy_value=source_y(x,y),height=std::min({band_end-y,8-(sy_value&7),wrapped_at[x]-y});
                if(selected(sx_value,sy_value)) {
                    const auto sample=tile_sample(sx_value,sy_value);const int ink=uniform_ink(sample[0]);
                    if(ink>0) {
                        for(unsigned row=y;row<y+height;++row) if(!append(solids,x,row,width_here,std::uint8_t(sample[1]*16+unsigned(ink)),solid_count,capacity)) return false;
                    } else if(ink<0) {
                        if(next.size()==capacity) return false;
                        next.push_back({x,y,width_here,height,std::uint16_t(sample[0]),std::uint8_t(sample[1]),
                            std::uint8_t(sample[2]),std::uint8_t(sample[3]),bool(sample[4]),bool(sample[5]),0,constant_x});
                    }
                }
                y+=height;
            }
            x+=width_here;
        }
        band_top=band_end;
    }
    if(expanded) for(unsigned x=0;x<width;++x) {
        std::uint8_t last_opaque{};bool prior_resolved=false;
        const auto recover_prior=[&](unsigned y) {
            if(prior_resolved) return;
            prior_resolved=true;
            while(y) {const auto ink=colour_at(x,--y);if(ink) {last_opaque=ink;break;}}
        };
        for(unsigned y=144;y<224;++y) {
            if(y>=wrapped_at[x]) {recover_prior(y);if(!append(carry,x,y,1,last_opaque,carry_count,capacity)) return false;continue;}
            const auto ink=colour_at(x,y);
            if(ink) {last_opaque=ink;prior_resolved=true;}
            else {recover_prior(y);if(!append(carry,x,y,1,last_opaque,carry_count,capacity)) return false;}
        }
    }
    if(!emit_rows(solids,next,capacity) || !emit_rows(carry,next,capacity)) return false;
    output.swap(next);return true;
}
} // namespace starfox::platform::nintendo_3ds
