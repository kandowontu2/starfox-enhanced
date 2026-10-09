#include "starfox/platform/nintendo_3ds/pica_raster.hpp"
#include <iostream>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
std::uint64_t checks{},bytes{},digest=14695981039346656037ULL;
void require(bool value,const char* why) {++checks;if(!value) throw std::runtime_error(why);}
void observe(std::span<const std::uint8_t> data) {
    for(auto value:data) {digest^=value;digest*=1099511628211ULL;} bytes+=data.size();
}
void character(simulation::SnesPpuState& ppu,unsigned base,unsigned number,unsigned depth) {
    for(unsigned y=0;y<8;++y) for(unsigned plane=0;plane<depth;++plane) {
        unsigned bits=0;
        for(unsigned x=0;x<8;++x) {
            const unsigned ink=(number+x+3*y)% (1U<<depth);
            if(ink&(1U<<plane)) bits|=128U>>x;
        }
        ppu.vram[(base*2+number*depth*8+(plane/2)*16+y*2+(plane&1))&65535]=std::uint8_t(bits);
    }
}
std::shared_ptr<simulation::SnesPpuState> source(unsigned mode) {
    auto ppu=std::make_shared<simulation::SnesPpuState>();
    ppu->background_mode=std::uint8_t(mode);ppu->main_screen=23;
    ppu->bg1_character_base=0;ppu->bg2_character_base=0x2000;ppu->bg3_character_base=0x2800;
    ppu->bg1_screen_base=0x5000;ppu->bg2_screen_base=0x5400;ppu->bg3_screen_base=0x5800;
    ppu->bg1_scroll_x=-13;ppu->bg1_scroll_y=19;ppu->bg2_scroll_x=31;ppu->bg2_scroll_y=-9;
    ppu->bg3_scroll_x=-5;ppu->bg3_scroll_y=7;ppu->object_select=3;
    for(unsigned i=0;i<256;++i) character(*ppu,0,i,mode==3?8:4);
    for(unsigned i=0;i<16;++i) {character(*ppu,0x2000,i,4);character(*ppu,0x2800,i,2);}
    for(unsigned i=0;i<1024;++i) for(unsigned base:{0x5000U,0x5400U,0x5800U}) {
        const unsigned tile=base==0x5000?i%256:i%16;
        const unsigned attributes=((i/17)%8)<<10 | ((i/31)%2)<<13 | ((i/43)%4)<<14;
        const unsigned word=tile|attributes;
        ppu->vram[base*2+i*2]=std::uint8_t(word);ppu->vram[base*2+i*2+1]=std::uint8_t(word>>8);
    }
    for(unsigned i=0;i<128;++i) ppu->oam[i*4+1]=240;
    for(unsigned i=0;i<12;++i) {
        character(*ppu,0x6000,i,4);
        ppu->oam[i*4]=std::uint8_t(9+19*i);ppu->oam[i*4+1]=std::uint8_t(11+17*i);
        ppu->oam[i*4+2]=std::uint8_t(i);ppu->oam[i*4+3]=std::uint8_t(((i%4)<<4)|((i%8)<<1));
    }
    return ppu;
}
void palette(simulation::SnesPpuState& ppu,unsigned seed) {
    for(unsigned ink=0;ink<256;++ink) {
        const unsigned r=(ink+seed)&31,g=(ink*13+seed*7)&31,b=(ink*23+seed*11)&31;
        ppu.cgram[ink]=std::uint16_t(r | (g<<5) | (b<<10) | ((ink&1)<<15));
    }
    ppu.cgram[17]=0; // Covered pure black must not become a hole.
}
// Independent scalar oracle: unchanged shared indexed painters, followed by
// explicit per-channel floor/clamp and byte writes, never packed-word helpers.
void compare(const simulation::SnesPpuState& ppu,const PpuBatch& batch,const PicaFrame& frame,bool trim=false) {
    require(!frame.textures.empty() && frame.draws.size()==frame.textures.size(),"Fixture lost nonempty source artwork");
    const unsigned width=frame.textures.front().pitch/4;
    const int origin=int((width-256)/2);
    render::Framebuffer indexed(width,224);indexed.enable_layer_tags(true);indexed.begin_write_coverage();
    const render::BackgroundRenderer backgrounds;const render::SpriteRenderer sprites;
    for(const auto& pass:batch.passes) {
        const bool wide=batch.expand_horizontal&&pass.extend_horizontal;
        const auto priority=pass.priority<0?render::TilePriorityPass::all:pass.priority?render::TilePriorityPass::high:render::TilePriorityPass::low;
        const auto bit=pass.layer==PpuLayer::bg1?1:pass.layer==PpuLayer::bg2?2:4;
        const render::ScopedLayer tag(indexed,static_cast<render::PixelLayer>(64|bit));
        if(pass.layer==PpuLayer::bg1) backgrounds.draw_bg1(ppu,indexed,priority,origin,wide,pass.guard_inset,pass.transparent_black,pass.mosaic_inset);
        else if(pass.layer==PpuLayer::bg2) {
            const auto scroll=pass.scroll.value_or(std::array{ppu.bg2_scroll_x,ppu.bg2_scroll_y});
            backgrounds.draw_bg2(ppu,scroll[0],scroll[1],indexed,priority,origin,wide,pass.wrap_horizontal,pass.transparent_black,pass.single_occurrence_top_rows);
        } else if(pass.layer==PpuLayer::bg3) backgrounds.draw_bg3(ppu,indexed,priority,origin,wide);
        else sprites.draw_objects(ppu,indexed,pass.priority<0?std::nullopt:std::optional<std::uint8_t>(pass.priority),origin,wide,false,nullptr,false,nullptr,pass.sprites);
    }
    indexed.end_write_coverage();
    std::vector<std::uint8_t> rgba(std::size_t(width)*240*4),owner(std::size_t(width)*240);
    // Brightness and subtract are supplied by this fixture's current sweep.
    extern unsigned current_brightness,current_subtract;
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<width;++x) {
        const int logical=int(y)-8;
        if(batch.space==PicaSpace::screen && (logical<0 || logical>=224)) continue;
        const unsigned sy=unsigned(std::clamp(logical,0,223));const auto at=std::size_t(sy)*width+x;
        if(sy<batch.first_row || sy>=batch.last_row || !indexed.write_coverage()[at]) continue;
        const auto tag=indexed.layer_tags()[at];const unsigned layer=tag==1?16:tag&63;
        require(pica_source_layer(layer)&&layer,"Oracle encountered an invalid source tag");
        const auto word=ppu.cgram[indexed.pixels()[at]];
        for(unsigned c=0;c<3;++c) {
            const int value=int((word>>(c*5))&31)-int(layer==2?current_subtract:0);
            const unsigned five=unsigned(value<0?0:value);
            rgba[(std::size_t(y)*width+x)*4+c]=std::uint8_t(((five*8)+(five/4))*current_brightness/15);
        }
        rgba[(std::size_t(y)*width+x)*4+3]=255;owner[std::size_t(y)*width+x]=std::uint8_t(layer);
    }
    std::vector<std::array<unsigned,4>> expected;
    unsigned pages=0;
    for(unsigned start=0;start<width;) {
        const unsigned remaining=width-start;
        const unsigned span=(batch.water_receiver || batch.corridor_receiver || batch.compact_strips)
            && remaining<=pica_raster_strip_width && pages+1<pica_raster_max_strips
            ?std::bit_floor(remaining):std::min(pica_raster_strip_width,remaining);
        auto bounds=std::array<unsigned,4>{start,0,start+span,240};
        if(trim) {
            bounds={start+span,240,start,0};
            // Deliberately scalar per-pixel oracle, unlike row-batched bounds.
            for(unsigned y=0;y<240;++y) for(unsigned x=start;x<start+span;++x)
                if(owner[std::size_t(y)*width+x]) {
                    bounds[0]=std::min(bounds[0],x);bounds[1]=std::min(bounds[1],y);
                    bounds[2]=std::max(bounds[2],x+1);bounds[3]=std::max(bounds[3],y+1);
                }
        }
        if(bounds[0]<bounds[2] && bounds[1]<bounds[3]) expected.push_back(bounds);
        start+=span;++pages;
    }
    require(expected.size()==frame.draws.size(),"Source crop omitted/added an occupied strip");
    unsigned strip=0;
    for(const auto& draw:frame.draws) {
        const auto& image=frame.textures[draw.texture];const auto position=frame.vertices[draw.first].position;
        const int x=int(position[0]+(float(width)-400)*.5F),y=int(position[1]);
        const auto bounds=expected[strip++];
        require(unsigned(x)==bounds[0] && unsigned(y)==bounds[1] && image.width==bounds[2]-bounds[0]
            && image.height==bounds[3]-bounds[1],"Exact source occupied rectangle changed");
        require(x>=0 && y>=0 && unsigned(x)+image.width<=width && unsigned(y)+image.height<=240,"Crop descriptor left its source buffer");
        const auto at=std::size_t(y)*width+unsigned(x);
        require(image.channels==4 && image.layer_pitch==width && !image.repeat,"RGBA/ownership descriptor changed");
        require(image.pixels.size()==rgba.size()-at*4 && std::equal(image.pixels.begin(),image.pixels.end(),rgba.begin()+at*4),"Exact RGBA bytes differ from scalar source oracle");
        require(image.source_layers.size()==owner.size()-at && std::equal(image.source_layers.begin(),image.source_layers.end(),owner.begin()+at),"Exact source ownership differs from scalar oracle");
        observe(image.pixels);observe(image.source_layers);
    }
}
unsigned current_brightness=15,current_subtract=0;
void sweep() {
    const auto plan=plan_frame(1,true,ScreenUse::world);
    for(unsigned mode:{1U,2U,3U}) for(bool wide:{false,true}) {
        auto ppu=source(mode);PpuBatch batch;batch.expand_horizontal=wide;batch.space=wide?PicaSpace::scenery:PicaSpace::screen;
        batch.passes={{PpuLayer::bg2,0},{PpuLayer::bg3,-1},{PpuLayer::bg1,0},{PpuLayer::objects,0},{PpuLayer::bg2,1},{PpuLayer::objects,3}};
        if(mode!=1) batch.passes.erase(batch.passes.begin()+1); // BG3 belongs to Mode 1 only.
        // Scenery batches cannot include OBJ; screen mixed passes test its
        // unsubtracted ownership separately, including overlapping priorities.
        if(wide) std::erase_if(batch.passes,[](const auto& pass){return pass.layer==PpuLayer::objects;});
        PicaRaster raster;
        for(unsigned phase=0;phase<32;++phase) {
            auto next=std::make_shared<simulation::SnesPpuState>(*ppu);palette(*next,phase);
            current_brightness=phase%16;current_subtract=phase;
            const auto frame=raster.prepare(next,batch,plan,current_brightness,current_subtract,700,true);
            compare(*next,batch,frame,true);
            const auto work=raster.work();
            raster.prepare(next,batch,plan,current_brightness,current_subtract,700,true);
            require(raster.work().decodes==work.decodes && raster.work().colour_updates==work.colour_updates,"Unchanged source lost palette cache reuse");
            ppu=next;
        }
    }
    auto ppu=source(2);ppu->main_screen=2;PpuBatch batch;batch.passes={{PpuLayer::bg2}};
    PicaRaster raster;
    for(unsigned brightness=0;brightness<16;++brightness) for(unsigned subtract=0;subtract<32;++subtract) {
        auto next=std::make_shared<simulation::SnesPpuState>(*ppu);palette(*next,brightness+subtract);
        current_brightness=brightness;current_subtract=subtract;
        compare(*next,batch,raster.prepare(next,batch,plan,brightness,subtract));ppu=next;
    }
    // Cropped/compact receiver strips retain duplicated scenery edge rows and
    // empty rows outside authored coverage; no palette or ownership shortcut.
    ppu=source(1);ppu->main_screen=2;palette(*ppu,9);batch.passes={{PpuLayer::bg2}};
    batch.expand_horizontal=true;batch.space=PicaSpace::scenery;batch.water_receiver=true;batch.compact_strips=true;
    batch.first_row=39;batch.last_row=197;current_brightness=7;current_subtract=19;
    compare(*ppu,batch,raster.prepare(ppu,batch,plan,7,19,700,true),true);
}
}
int main() try {
    sweep();require(bytes==606949280 && digest==0xf84f538e5c7521b5ULL,"Frozen pre-change complete view stream changed");
    std::cout<<"PASS scalar RGBA/ownership/crop/cache checks="<<checks<<" bytes="<<bytes<<" digest="<<std::hex<<digest
        <<"; not whole-game/native/physical performance acceptance\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
