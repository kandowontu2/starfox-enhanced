#include "starfox/platform/nintendo_3ds/pica_raster.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include "starfox/platform/nintendo_3ds/pica_window.hpp"
#include "starfox/platform/nintendo_3ds/pica_colour.hpp"
#include "starfox/platform/nintendo_3ds/game_effects.hpp"
#include <iostream>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
void require(bool value,const char* why) {++checks;if(!value) throw std::runtime_error(why);}
template<class Action> void rejected(Action action,const char* why) {
    bool failed=false;try {action();} catch(const std::exception&) {failed=true;} require(failed,why);
}
void tile(simulation::SnesPpuState& ppu,unsigned character_base,unsigned number,unsigned ink,unsigned depth=4) {
    for(unsigned y=0;y<8;++y) for(unsigned bit=0;bit<depth;++bit)
        ppu.vram[(character_base*2+number*depth*8+(bit/2)*16+y*2+(bit&1))&65535]=(ink&(1U<<bit))?255:0;
}
void object(simulation::SnesPpuState& ppu,unsigned i,unsigned x,unsigned y,unsigned number) {
    ppu.oam[i*4]=x;ppu.oam[i*4+1]=y;ppu.oam[i*4+2]=number;ppu.oam[i*4+3]=0x20;
    tile(ppu,0,number,1);
}
std::shared_ptr<simulation::SnesPpuState> source() {
    auto ppu=std::make_shared<simulation::SnesPpuState>();
    ppu->main_screen=23;ppu->object_select=0;
    ppu->bg1_screen_base=0x6400;ppu->bg1_character_base=0x4400;ppu->bg1_screen_size=0;
    ppu->bg2_screen_base=0x6000;ppu->bg2_character_base=0x4000;ppu->bg2_screen_size=0;
    ppu->bg3_screen_base=0x6800;ppu->bg3_character_base=0x4800;ppu->bg3_screen_size=0;
    for(unsigned i=0;i<1024;++i) {
        ppu->vram[0xc000+i*2+1]=4; // palette 1, tile 0, low priority.
        ppu->vram[0xc800+i*2]=1;ppu->vram[0xc800+i*2+1]=32; // high BG1.
    }
    tile(*ppu,0x4000,0,1);tile(*ppu,0x4400,1,2);tile(*ppu,0x4800,0,3,2);
    ppu->cgram[17]=0; // Opaque black, not transparent index zero.
    ppu->cgram[2]=31<<5;ppu->cgram[3]=31<<10;ppu->cgram[129]=31;
    for(unsigned i=0;i<128;++i) ppu->oam[i*4+1]=240;
    object(*ppu,0,10,10,189);object(*ppu,1,100,180,0x61);object(*ppu,2,190,85,7);
    return ppu;
}
std::array<unsigned,4> pixel(const PicaImage& image,unsigned x,unsigned y) {
    const auto offset=std::size_t(y)*image.pitch+x*4;
    return {image.pixels[offset],image.pixels[offset+1],image.pixels[offset+2],image.pixels[offset+3]};
}
std::array<unsigned,5> scenery_pixel(const PicaFrame& frame,int x,unsigned y) {
    std::array<unsigned,5> result{};
    for(const auto& draw:frame.draws) {
        const auto& image=frame.textures[draw.texture];
        const auto& origin=frame.vertices[draw.first].position;
        const int ix=x-int(origin[0]),iy=int(y)-int(origin[1]);
        if(ix<0 || iy<0 || ix>=int(image.width) || iy>=int(image.height)) continue;
        const auto rgb=pixel(image,unsigned(ix),unsigned(iy));
        if(!rgb[3]) continue;
        result={rgb[0],rgb[1],rgb[2],rgb[3],image.source_layers[std::size_t(iy)*image.layer_pitch+unsigned(ix)]};
    }
    return result;
}
void disjoint_scenery_coverage() {
    auto ppu=source();ppu->main_screen=2;ppu->background_mode=2;
    tile(*ppu,0x4000,1,2);tile(*ppu,0x4000,2,3);
    ppu->cgram[18]=31<<5;ppu->cgram[19]=31<<10;
    for(unsigned y=0;y<32;++y) for(unsigned x=0;x<32;++x) {
        ppu->vram[0xc000+(y*32+x)*2]=std::uint8_t((x+2*y)%3);
        ppu->vram[0xc001+(y*32+x)*2]=std::uint8_t(4+(((x+y)%2)?32:0));
    }
    ppu->bg2_scroll_x=13;ppu->bg2_scroll_y=-7;
    const auto unchanged=*ppu;
    for(int priority:{-1,0,1}) for(float convergence:{16.F,32.F,1024.F}) {
        PpuBatch ordinary;ordinary.space=PicaSpace::scenery;ordinary.expand_horizontal=true;
        ordinary.passes.push_back({PpuLayer::bg2,priority});
        auto compact=ordinary;compact.visible_scenery_only=true;
        PicaRaster oracle,packed;StereoSettings settings;settings.strength=2;settings.separation=64;settings.convergence=convergence;
        for(float slider:{1.F,.5F,0.F,.123F,1.F}) {
            const auto plan=plan_frame(slider,true,ScreenUse::world,settings);
            const auto full=oracle.prepare(ppu,ordinary,plan);
            const auto frame=packed.prepare(ppu,compact,plan);
            unsigned bytes=512*256*4;
            for(auto image:frame.textures) {
                image.source_layers={};image.layer_pitch=0;
                bytes+=pica_resident_texture_bytes(image);
                require(image.width<=1024,"Disjoint scenery exceeded native sampler dimensions");
            }
            require(bytes<=2U*1024U*1024U && frame.textures.size()<=3,
                "Disjoint infinity frusta retained a resident unseen gap or reduced optical coverage");
            for(unsigned y=0;y<240;++y) for(unsigned x=0;x<400;++x) {
                require(scenery_pixel(frame,int(x),y)==scenery_pixel(full,int(x),y),
                    "Disjoint scenery changed canonical source pixels, priority, ownership or opaque black");
                for(unsigned eye=0;eye<plan.eye_count;++eye) {
                    // Independent inverse of the off-axis infinity projection:
                    // use pixel centres, including non-integer eye offsets.
                    const double offset=double(plan.focal_x)*plan.eyes[eye].x/plan.convergence;
                    const int source_x=int(std::floor(x+.5-offset));
                    require(scenery_pixel(frame,source_x,y)==scenery_pixel(full,source_x,y),
                        "A disjoint eye frustum lost source pixels or left an LCD edge/strip seam");
                }
            }
            const auto cached=packed.work();packed.prepare(ppu,compact,plan);
            require(packed.work().decodes==cached.decodes && packed.work().colour_updates==cached.colour_updates,
                "Visible infinity descriptors defeated source raster/cache reuse");
        }
        auto invalid=compact;invalid.space=PicaSpace::screen;
        rejected([&]{packed.prepare(ppu,invalid,plan_frame(1,true,ScreenUse::world,settings));},
            "Screen artwork accepted infinity-only cropping");
        invalid=compact;invalid.water_receiver=true;ppu->background_mode=1;
        rejected([&]{packed.prepare(ppu,invalid,plan_frame(1,true,ScreenUse::world,settings));},
            "Finite water accepted infinity-only cropping");
        ppu->background_mode=2;
    }
    require(ppu->vram==unchanged.vram && ppu->oam==unchanged.oam && ppu->cgram==unchanged.cgram,
        "Disjoint scenery preparation changed cartridge source storage");
}
void unique_sky_halves() {
    for(bool right:{false,true}) for(int scroll:{0,255,-1}) {
        auto ppu=std::make_shared<simulation::SnesPpuState>();
        ppu->background_mode=2;ppu->main_screen=2;ppu->bg2_screen_size=3;
        ppu->bg2_screen_base=0x2000;ppu->bg2_character_base=0;
        for(unsigned ink=1;ink<=4;++ink) tile(*ppu,0,ink,ink);
        ppu->cgram[1]=0; // The unique artwork includes opaque black.
        ppu->cgram[2]=31<<5;ppu->cgram[3]=31<<10;ppu->cgram[4]=31;
        for(unsigned y=0;y<64;++y) for(unsigned x=0;x<64;++x) {
            const auto entry=((x/32)+(y/32)*2)*1024+(y%32)*32+x%32;
            const bool selected=(x>=32)==right;
            ppu->vram[0x4000+entry*2]=std::uint8_t(y<44?(selected?1:2):(x<32?3:4));
        }
        ppu->bg2_scroll_x=std::int16_t(scroll);ppu->bg2_scroll_y=240;
        const auto unchanged=*ppu;
        auto settings=StereoSettings{};settings.separation=64;settings.convergence=16;
        const auto plan=plan_frame(1,true,ScreenUse::world,settings);
        PpuBatch batch;batch.space=PicaSpace::scenery;batch.expand_horizontal=true;
        batch.passes.push_back({PpuLayer::bg2});
        batch.passes[0].scroll=std::array<std::int16_t,2>{std::int16_t(scroll),240};
        batch.passes[0].single_occurrence_sky_half=PpuUniqueSkyHalf{right,352};
        PicaRaster renderer;const auto frame=renderer.prepare(ppu,batch,plan);
        const auto guard=pica_scenery_guard(plan);
        unsigned removed{},native_black{},ground{};
        for(unsigned strip=0;strip<frame.textures.size();++strip) {
            const auto& image=frame.textures[strip];
            const int lcd_x=int(frame.vertices[strip*6].position[0]);
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<image.width;++x) {
                const int logical=lcd_x+int(x)-72;
                int sx=(logical+scroll)%512;if(sx<0) sx+=512;
                const unsigned sy=(240+y)%512;
                const bool selected=(sx>=256)==right;
                unsigned ink=sy<352?(selected?1:2):(sx<256?3:4);
                // Independent source-coordinate oracle: retain the native
                // window and one complete authored occurrence. Only wrapped
                // copies of its unique sky half use the other, repeatable half.
                if((logical<0 || logical>=256) && (logical+scroll<0 || logical+scroll>=512)
                    && sy<352 && selected) {ink=2;++removed;}
                if(logical>=0 && logical<256 && ink==1) ++native_black;
                if(sy>=352) ++ground;
                const auto word=ppu->cgram[ink];
                const auto expand=[](unsigned five){return (five<<3)|(five>>2);};
                require(pixel(image,x,y+8)==std::array<unsigned,4>{expand(word&31),expand((word>>5)&31),expand((word>>10)&31),255},
                    "Wide native landscape duplicated unique sky artwork or changed canonical/ground pixels");
                require(image.source_layers[std::size_t(y+8)*image.layer_pitch+x]==2,
                    "Unique-sky replacement changed source painter ownership");
            }
        }
        require(removed>0 && ground>0 && guard>=512,"Unique-half fixture missed the repeated sky or finite ground band");
        if((scroll==0 && !right) || (scroll==255 && right)) require(native_black>0,"Opaque unique native ink was not exercised");
        const auto work=renderer.work();
        for(float slider:{0.F,.5F,1.F}) {
            renderer.prepare(ppu,batch,plan_frame(slider,true,ScreenUse::world,settings));
            require(renderer.work().decodes==work.decodes && renderer.work().colour_updates==work.colour_updates,
                "Slider reran single-occurrence sky decoding");
        }
        auto bad=batch;bad.passes[0].single_occurrence_sky_half->rows=513;
        rejected([&]{renderer.prepare(ppu,bad,plan);},"Out-of-atlas unique-half rows accepted");
        bad=batch;bad.passes[0].layer=PpuLayer::bg1;
        rejected([&]{renderer.prepare(ppu,bad,plan);},"Unique BG2 policy incorrectly applied to another layer");
        auto faded=std::make_shared<simulation::SnesPpuState>(*ppu);faded->cgram[2]=31<<10;
        const auto fade=renderer.prepare(faded,batch,plan,7,3);
        require(renderer.work().decodes==work.decodes && renderer.work().colour_updates==work.colour_updates+1,
            "Unique-sky palette fade redecoded source artwork");
        require(pixel(fade.textures.front(),0,8)==std::array<unsigned,4>{0,0,107,255},
            "Unique-sky replacement failed to follow native palette/subtract/brightness changes");
        auto changed=batch;changed.passes[0].single_occurrence_sky_half->right=!right;
        renderer.prepare(faded,changed,plan,7,3);
        require(renderer.work().decodes==work.decodes+1,"Unique-half policy change reused stale cached source indices");
        require(*ppu==unchanged,"Unique-half source presentation modified cartridge PPU state");
    }
}
void raster() {
    const auto plan=plan_frame(1,true,ScreenUse::world);
    auto ppu=source();const auto original=*ppu;
    PicaRaster renderer;PpuBatch batch;batch.space=PicaSpace::scenery;batch.expand_horizontal=true;
    batch.passes.push_back({PpuLayer::bg2});
    auto frame=renderer.prepare(ppu,batch,plan);
    require(frame.vertices.size()==6 && frame.draws.size()==1 && frame.textures.size()==1,"Native sky quad missing");
    require(frame.draws[0].space==PicaSpace::scenery && !frame.draws[0].depth_test && frame.draws[0].alpha_blend,"Sky placed at model/HUD depth");
    const auto image=frame.textures[0];
    const auto* ownership=image.source_layers.data();
    require(image.width==464 && image.height==240 && frame.vertices[0].position[0]==-32,"Infinite sky guard/layout mismatch");
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<464;++x)
        require(pixel(image,x,y)==std::array<unsigned,4>{0,0,0,255},"Opaque black source ink became transparent or viewport guard was uncovered");
    for(float slider:{0.F,.25F,.5F,1.F}) {
        const auto other=renderer.prepare(ppu,batch,plan_frame(slider,true,ScreenUse::world));
        require(other.textures[0].pixels.data()==image.pixels.data(),"Eye/slider read replaced cached sky artwork");
    }
    require(renderer.work().decodes==1 && renderer.work().colour_updates==1,"Slider rerasterized/recoloured the 2D source");
    auto moved=std::make_shared<simulation::SnesPpuState>(*ppu);moved->oam[0]=20;
    renderer.prepare(moved,batch,plan);
    require(renderer.work().decodes==1,"Unrelated OAM change traversed background tiles");
    auto palette=std::make_shared<simulation::SnesPpuState>(*moved);palette->cgram[17]=31;
    frame=renderer.prepare(palette,batch,plan,7);
    require(renderer.work().decodes==1 && renderer.work().colour_updates==2,"Palette-only fade did not reuse source indices");
    require(pixel(frame.textures[0],200,100)==std::array<unsigned,4>{119,0,0,255},"Native integer brightness/fade mismatch");
    require(frame.textures[0].source_layers.data()==ownership && frame.textures[0].source_layers[100*464+200]==2,
        "Palette-only fade re-decoded/reallocated source-layer coverage");
    frame=renderer.prepare(palette,batch,plan,15,10);
    require(pixel(frame.textures[0],200,100)==std::array<unsigned,4>{173,0,0,255},"BG2 native colour subtraction mismatch");
    auto rolled=std::make_shared<simulation::SnesPpuState>(*palette);
    rolled->bg2_horizontal_offsets_enabled=true;rolled->bg2_vertical_offsets_enabled=true;
    rolled->bg2_horizontal_offsets.fill(-37);
    for(unsigned i=0;i<32;++i) {rolled->vram[0x5f40+i*2]=7;rolled->vram[0x5f40+i*2+1]=64;}
    renderer.prepare(rolled,batch,plan);
    require(renderer.work().decodes==2,"HDMA/offset updates did not invalidate source raster");
    const auto work=renderer.work();const auto retained=renderer.prepare(rolled,batch,plan);
    const std::vector<std::uint8_t> saved(retained.textures[0].pixels.begin(),retained.textures[0].pixels.end());
    auto bad=batch;bad.passes[0].priority=3;
    rejected([&]{renderer.prepare(rolled,bad,plan);},"Invalid priority was silently substituted");
    require(std::equal(saved.begin(),saved.end(),retained.textures[0].pixels.begin()) && renderer.work().decodes==work.decodes,"Failed source pass invalidated previous layer");
    auto impossible=plan;impossible.eyes[0].projection_offset=-2000;
    rejected([&]{renderer.prepare(rolled,batch,impossible);},"Scenery outside allocated eye coverage accepted");
    require(*ppu==original,"Layer preparation mutated cartridge data");
    PicaRaster overlay;PpuBatch ui;ui.passes.push_back({PpuLayer::objects});ui.passes[0].sprites=render::SpriteSelection::world_only;
    const auto hud=overlay.prepare(ppu,ui,plan);
    require(hud.draws[0].space==PicaSpace::screen && pixel(hud.textures[0],10,18)[3]==0,"Moved lower-screen HUD retained on upper LCD");
    require(pixel(hud.textures[0],100,188)==std::array<unsigned,4>{255,0,0,255},"Top-world reticle lost while moving HUD");
    require(pixel(hud.textures[0],190,93)==std::array<unsigned,4>{255,0,0,255},"Top-world explosion OBJ lost");
    require(pixel(hud.textures[0],0,0)[3]==0 && pixel(hud.textures[0],100,239)[3]==0,"Screen artwork grew opaque vertical guards");
    PpuBatch order;order.passes={{PpuLayer::bg2},{PpuLayer::bg1,1}};
    const auto green=overlay.prepare(ppu,order,plan);
    require(pixel(green.textures[0],100,100)==std::array<unsigned,4>{0,255,0,255},"Painter order/priority did not retain opaque foreground");
    order.passes[1].priority=0;
    const auto black=overlay.prepare(ppu,order,plan);
    require(pixel(black.textures[0],100,100)==std::array<unsigned,4>{0,0,0,255},"Low-priority source pass painted high tile");
    auto mode3=std::make_shared<simulation::SnesPpuState>(*ppu);mode3->background_mode=3;
    tile(*mode3,0x4400,1,2,8);
    const auto map=overlay.prepare(mode3,PpuBatch{{{PpuLayer::bg1,1}}},plan);
    require(pixel(map.textures[0],100,100)==std::array<unsigned,4>{0,255,0,255},"Mode-3 8bpp map artwork lost");
    auto mode1=std::make_shared<simulation::SnesPpuState>(*ppu);mode1->background_mode=1;
    const auto bg3=overlay.prepare(mode1,PpuBatch{{{PpuLayer::bg3}}},plan);
    require(pixel(bg3.textures[0],100,100)==std::array<unsigned,4>{0,0,255,255},"Mode-1 2bpp BG3 artwork lost");
    require(bg3.textures[0].source_layers[100*256+100]==4,"BG3 lost its distinct colour-math source layer");
    auto mixed=std::make_shared<simulation::SnesPpuState>(*ppu);
    for(unsigned y=0;y<32;++y) for(unsigned x=0;x<16;++x) mixed->vram[0xc800+(y*32+x)*2]=0;
    const auto group=overlay.prepare(mixed,PpuBatch{{{PpuLayer::bg2},{PpuLayer::bg1},{PpuLayer::objects}}},plan);
    const auto& texture=group.textures[0];
    require(texture.source_layers[100*256+5]==2 && texture.source_layers[100*256+200]==1
        && texture.source_layers[93*256+191]==16,"Mixed painter group lost winning BG2/BG1/OBJ provenance");
    require(pixel(texture,5,100)==std::array<unsigned,4>{0,0,0,255} && texture.source_layers[5]==0,
        "Opaque black and uncovered vertical guard ownership confused");
    require(validate_pica_layers(texture)==19,"Mixed PPU group did not retain precisely its occupied source classes");
}
void same_raster(const PicaFrame& cached,const PicaFrame& fresh) {
    require(cached.vertices.size()==fresh.vertices.size()
        && std::equal(cached.vertices.begin(),cached.vertices.end(),fresh.vertices.begin()),
        "Cached source update changed its geometry against a fresh decode");
    require(cached.draws.size()==fresh.draws.size() && cached.textures.size()==fresh.textures.size(),
        "Cached source update changed its painter/texture count");
    for(unsigned i=0;i<cached.draws.size();++i) {
        const auto& a=cached.draws[i];const auto& b=fresh.draws[i];
        require(a.first==b.first && a.count==b.count && a.texture==b.texture && a.model==b.model
            && a.space==b.space && a.depth_test==b.depth_test && a.depth_write==b.depth_write
            && a.alpha_blend==b.alpha_blend && a.source_layer==b.source_layer,
            "Cached source update changed painter depth/order/ownership");
    }
    for(unsigned i=0;i<cached.textures.size();++i) {
        const auto& a=cached.textures[i];const auto& b=fresh.textures[i];
        require(a.width==b.width && a.height==b.height && a.pitch==b.pitch
            && a.channels==b.channels && a.repeat==b.repeat && a.layer_pitch==b.layer_pitch,
            "Cached source update changed texture descriptors");
        require(a.pixels.size()==b.pixels.size()
            && std::equal(a.pixels.begin(),a.pixels.end(),b.pixels.begin()),
            "Cache retained stale RGBA against a fresh source decode");
        require(a.source_layers.size()==b.source_layers.size()
            && std::equal(a.source_layers.begin(),a.source_layers.end(),b.source_layers.begin()),
            "Cache retained stale source coverage/ownership against a fresh decode");
    }
}
void pass_memory_dependencies() {
    const auto plan=plan_frame(1,true,ScreenUse::world);
    const auto make_source=[] {
        auto ppu=std::make_shared<simulation::SnesPpuState>();
        std::uint32_t random=0x385ad7;
        for(auto& byte:ppu->vram) {random=random*1664525U+1013904223U;byte=std::uint8_t(random>>24);}
        for(unsigned i=0;i<256;++i) ppu->cgram[i]=std::uint16_t((i*131U)&32767U);
        return ppu;
    };
    const auto check=[&](PicaRaster& owner,const auto& ppu,const PpuBatch& batch,
        unsigned brightness=15,unsigned subtract=0) {
        const auto unchanged=*ppu;
        const auto cached=owner.prepare(ppu,batch,plan,brightness,subtract);
        PicaRaster oracle;
        same_raster(cached,oracle.prepare(ppu,batch,plan,brightness,subtract));
        require(*ppu==unchanged,"Pass-dependent cache modified its cartridge snapshot");
    };
    for(const auto layer:{PpuLayer::bg1,PpuLayer::bg2,PpuLayer::bg3})
        for(unsigned mode=1;mode<=3;++mode) for(unsigned size=0;size<4;++size)
        for(bool big:{false,true}) for(bool wrapped:{false,true}) {
        auto ppu=make_source();ppu->background_mode=std::uint8_t(mode);
        const unsigned characters=wrapped?0x7ff0U:0x4000U,map=wrapped?0x7ffcU:0x2000U;
        const unsigned bit=layer==PpuLayer::bg1?1U:layer==PpuLayer::bg2?2U:4U;
        ppu->main_screen=std::uint8_t(bit);
        if(layer==PpuLayer::bg1) {
            ppu->bg1_character_base=std::uint16_t(characters);ppu->bg1_screen_base=std::uint16_t(map);
            ppu->bg1_screen_size=std::uint8_t(size);ppu->bg1_tile_size_16=big;
            ppu->bg1_scroll_x=-17;ppu->bg1_scroll_y=31;
        } else if(layer==PpuLayer::bg2) {
            ppu->bg2_character_base=std::uint16_t(characters);ppu->bg2_screen_base=std::uint16_t(map);
            ppu->bg2_screen_size=std::uint8_t(size);ppu->bg2_tile_size_16=big;
            ppu->bg2_scroll_x=-17;ppu->bg2_scroll_y=31;
        } else {
            ppu->bg3_character_base=std::uint16_t(characters);ppu->bg3_screen_base=std::uint16_t(map);
            ppu->bg3_screen_size=std::uint8_t(size);ppu->bg3_tile_size_16=big;
            ppu->bg3_scroll_x=-17;ppu->bg3_scroll_y=31;
        }
        const unsigned depth=layer==PpuLayer::bg3?2U:layer==PpuLayer::bg1 && mode==3?8U:4U;
        const unsigned character_bytes=1024U*depth*8U;
        const unsigned map_bytes=2048U*((size&1U)?2U:1U)*((size&2U)?2U:1U);
        const auto contains=[](unsigned address,unsigned first,unsigned count) {
            return ((address-first)&65535U)<count;
        };
        PpuBatch batch{{{layer}},PicaSpace::screen,true};PicaRaster owner;
        check(owner,ppu,batch);
        const auto work=owner.work();
        unsigned unrelated=0;
        while(unrelated<65536 && (contains(unrelated,characters*2,character_bytes)
            || contains(unrelated,map*2,map_bytes))) ++unrelated;
        if(unrelated<65536) {
            auto changed=std::make_shared<simulation::SnesPpuState>(*ppu);changed->vram[unrelated]^=255;
            check(owner,changed,batch);
            require(owner.work().decodes==work.decodes && owner.work().colour_updates==work.colour_updates,
                "Unrelated VRAM still decoded/recoloured an unchanged background");
            ppu=changed;
        }
        // Test both ends of character and map storage, including ranges that
        // cross address 65535 and 16x16 character carry/wrap.
        for(unsigned address:{(characters*2)&65535U,(characters*2+character_bytes-1)&65535U,
            (map*2)&65535U,(map*2+map_bytes-1)&65535U}) {
            const auto before=owner.work();
            auto changed=std::make_shared<simulation::SnesPpuState>(*ppu);changed->vram[address]^=0x5a;
            check(owner,changed,batch);
            require(owner.work().decodes==before.decodes+1,"Relevant character/map bytes failed to invalidate cache");
            ppu=changed;
        }
        auto faded=std::make_shared<simulation::SnesPpuState>(*ppu);faded->cgram[33]^=0x20;
        check(owner,faded,batch,7,3);
        auto disabled=std::make_shared<simulation::SnesPpuState>(*faded);disabled->main_screen=0;
        check(owner,disabled,batch);const auto blank=owner.work();
        auto unrelated_disabled=std::make_shared<simulation::SnesPpuState>(*disabled);
        unrelated_disabled->vram[0]^=255;unrelated_disabled->mosaic^=0xff;
        check(owner,unrelated_disabled,batch);
        require(owner.work().decodes==blank.decodes,"Disabled background traversed unused source memory");
        auto enabled=std::make_shared<simulation::SnesPpuState>(*unrelated_disabled);enabled->main_screen=std::uint8_t(bit);
        check(owner,enabled,batch);
        require(owner.work().decodes==blank.decodes+1,"Re-enabled background reused a blank cache");
    }
    // OBJ uses two name banks; its shared 64x64 sampler carries past tile 255.
    for(unsigned base=0;base<8;++base) for(unsigned gap=0;gap<4;++gap) for(unsigned bank=0;bank<2;++bank) {
        auto ppu=make_source();ppu->main_screen=16;
        ppu->object_select=std::uint8_t((5U<<5)|base|(gap<<3));
        std::fill(ppu->oam.begin()+512,ppu->oam.end(),0x55); // All unused sprites outside the native LCD.
        ppu->oam[0]=24;ppu->oam[1]=20;ppu->oam[2]=255;ppu->oam[3]=std::uint8_t(0x20|bank);
        ppu->oam[512]=0x56; // First sprite at positive X, large 64x64 selection.
        const unsigned first=base*0x4000U,second=first+(gap+1U)*0x2000U;
        PpuBatch batch{{{PpuLayer::objects}}};PicaRaster owner;check(owner,ppu,batch);
        const auto work=owner.work();unsigned unrelated=0;
        while(unrelated<65536 && (((unrelated-first)&65535U)<384U*32U
            || ((unrelated-second)&65535U)<384U*32U)) ++unrelated;
        require(unrelated<65536,"OBJ dependency fixture has no unrelated byte");
        auto changed=std::make_shared<simulation::SnesPpuState>(*ppu);changed->vram[unrelated]^=255;
        check(owner,changed,batch);
        require(owner.work().decodes==work.decodes,"Unrelated BG memory traversed sprite characters");
        const auto before=owner.work();
        changed=std::make_shared<simulation::SnesPpuState>(*changed);
        changed->vram[((bank?second:first)+374U*32U+31U)&65535U]^=255;
        check(owner,changed,batch);
        require(owner.work().decodes==before.decodes+1,"Large OBJ tile carry escaped name-bank dependencies");
        changed=std::make_shared<simulation::SnesPpuState>(*changed);changed->oam[0]+=1;check(owner,changed,batch);
        changed=std::make_shared<simulation::SnesPpuState>(*changed);changed->object_select^=8;check(owner,changed,batch);
    }
    auto ppu=source();ppu->background_mode=2;ppu->main_screen=2;
    ppu->bg2_vertical_offsets_enabled=true;
    PpuBatch batch{{{PpuLayer::bg2}},PicaSpace::scenery,true};PicaRaster owner;check(owner,ppu,batch);
    for(unsigned address:{0x5f40U,0x5f7fU}) {
        const auto before=owner.work();auto changed=std::make_shared<simulation::SnesPpuState>(*ppu);
        changed->vram[address]^=255;check(owner,changed,batch);
        require(owner.work().decodes==before.decodes+1,"Mode-2 vertical offset VRAM left stale ground/sky");ppu=changed;
    }
    auto changed=std::make_shared<simulation::SnesPpuState>(*ppu);
    changed->bg2_horizontal_offsets_enabled=true;changed->bg2_horizontal_offsets.fill(-13);check(owner,changed,batch);
    changed=std::make_shared<simulation::SnesPpuState>(*changed);
    changed->bg2_scanline_scroll_enabled=true;changed->bg2_scanline_scroll_y.fill(256);check(owner,changed,batch);
    changed=std::make_shared<simulation::SnesPpuState>(*changed);changed->tunnel_scene=true;check(owner,changed,batch);
    changed=std::make_shared<simulation::SnesPpuState>(*changed);changed->mosaic=0x32;check(owner,changed,batch);
    changed=std::make_shared<simulation::SnesPpuState>(*changed);changed->bg2_scroll_x=-511;check(owner,changed,batch);
}
void optical_coverage() {
    auto ppu=source();const auto untouched=*ppu;Canvas lower;PicaRaster renderer;
    PpuBatch batch{{{PpuLayer::bg2}},PicaSpace::scenery,true};
    // Patterned source columns make both eye borders and strip joins observable.
    ppu->cgram[17]=31;ppu->cgram[18]=31<<5;
    tile(*ppu,0x4000,1,2);
    for(unsigned row=0;row<32;++row) for(unsigned col=0;col<32;++col)
        ppu->vram[0xc000+(row*32+col)*2]=std::uint8_t(col&1);
    const auto patterned=*ppu;
    for(float separation:{0.F,12.F,32.F,64.F}) for(float strength:{.5F,1.F,2.F})
        for(float convergence:{16.F,32.F,64.F,1024.F}) for(float slider:{0.F,.5F,1.F}) {
            StereoSettings settings;settings.separation=separation;settings.strength=strength;settings.convergence=convergence;
            const auto plan=plan_frame(slider,true,ScreenUse::world,settings);
            const auto frame=renderer.prepare(ppu,batch,plan);validate_pica_frame(frame,lower.view());
            const unsigned width=frame.textures.front().pitch/4;
            const int left=(int(top_width)-int(width))/2;
            require(frame.textures.size()==(width+1023)/1024 && width>=464,
                "Eye coverage was clamped or failed to split an oversized native texture");
            require(plan.separation==slider*strength*separation,"Rendering changed requested optics");
            for(unsigned i=0;i<frame.textures.size();++i) {
                const auto& image=frame.textures[i];
                require(image.width<=1024 && image.pitch==width*4
                    && image.pixels.data()==frame.textures.front().pixels.data()+i*1024*4
                    && frame.vertices[i*6].position[0]==left+int(i*1024),
                    "Native strips copied source pixels or left a horizontal gap");
            }
            for(unsigned eye=0;eye<plan.eye_count;++eye) for(unsigned x=0;x<400;++x) {
                const double source_x=double(x)+.5-background_offset(plan,eye);
                require(source_x>=left && source_x<left+width,"LCD eye border escaped decoded source coverage");
                const unsigned column=unsigned(std::floor(source_x-left)),strip=column/1024;
                const auto rgb=pixel(frame.textures[strip],column%1024,120);
                const int native_x=int(std::floor(source_x))-72;
                const bool green=((native_x&255)/8)&1;
                require(rgb==std::array<unsigned,4>{green?0U:255U,green?255U:0U,0,255},
                    "Eye border/strip join did not sample the exact authored BG tile");
            }
        }
    require(*ppu==patterned && untouched.oam==ppu->oam,"Optical coverage changed source data");
}
void transparent_priority_crop() {
    auto ppu=source();ppu->vram[0xc001+(10*32+5)*2]|=32; // One high-priority opaque-black tile.
    const auto unchanged=*ppu;PpuBatch batch{{{PpuLayer::bg2,1}},PicaSpace::scenery,true};
    StereoSettings settings;settings.strength=2;settings.separation=64;settings.convergence=16;
    const auto plan=plan_frame(1,true,ScreenUse::world,settings);PicaRaster renderer;Canvas lower;
    const auto full=renderer.prepare(ppu,batch,plan);validate_pica_frame(full,lower.view());
    const auto width=full.textures[0].pitch/4;
    const std::vector<std::uint8_t> pixels(full.textures[0].pixels.begin(),full.textures[0].pixels.end());
    unsigned bytes=0;for(auto image:full.textures) bytes+=pica_resident_texture_bytes(image);
    const auto cropped=renderer.prepare(ppu,batch,plan,15,0,32,true);validate_pica_frame(cropped,lower.view());
    unsigned crop_bytes=0;for(auto image:cropped.textures) crop_bytes+=pica_resident_texture_bytes(image);
    require(crop_bytes<bytes/8 && renderer.work().decodes==1 && renderer.work().colour_updates==1,
        "Empty priority rows still consumed full texture pages or cropping decoded the source again");
    unsigned black=0;
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<width;++x) {
        std::array<unsigned,4> actual{};
        for(auto draw:cropped.draws) {
            const auto& origin=cropped.vertices[draw.first].position;const auto image=cropped.textures[draw.texture];
            const int ix=int(x)+int((400.-width)/2)-int(origin[0]),iy=int(y)-int(origin[1]);
            if(ix>=0 && iy>=0 && ix<int(image.width) && iy<int(image.height)) actual=pixel(image,unsigned(ix),unsigned(iy));
        }
        const auto at=(std::size_t(y)*width+x)*4;
        require(actual==std::array<unsigned,4>{pixels[at],pixels[at+1],pixels[at+2],pixels[at+3]},
            "Trimming an empty priority region changed source pixels/black opacity or strip joins");
        black+=actual[3]==255;
    }
    require(black>0 && *ppu==unchanged,"Priority crop erased black ink or mutated source");
    const auto faded=renderer.prepare(ppu,batch,plan,0,0,32,true);
    require(renderer.work().decodes==1 && faded.draws.size()==cropped.draws.size(),
        "Palette-only fade changed occupied source priority bounds");
}
void composition() {
    const auto plan=plan_frame(1,true,ScreenUse::world);
    Canvas dashboard;const auto ppu=source();PicaRaster sky,ui;
    PpuBatch sky_batch{{{PpuLayer::bg2}},PicaSpace::scenery,true};
    PpuBatch ui_batch{{{PpuLayer::objects}}};
    const auto backdrop=sky.prepare(ppu,sky_batch,plan),overlay=ui.prepare(ppu,ui_batch,plan);
    const std::array<PicaVertex,3> vertices{{{{-1,-1,512}},{{1,-1,512}},{{0,1,512}}}};
    const std::array<PicaDraw,1> draws{{{0,3}}};
    const PicaFrame models{plan,vertices,draws,{}};
    const std::array groups{backdrop,models,overlay};PicaComposite compositor;
    const auto frame=compositor.prepare(plan,groups,dashboard.view());
    validate_pica_frame(frame,dashboard.view());
    require(frame.draws.size()==3 && frame.draws[0].space==PicaSpace::scenery && frame.draws[1].space==PicaSpace::world
        && frame.draws[2].space==PicaSpace::screen,"Compositor flattened/reordered depth and foreground groups");
    require(frame.draws[0].texture==0 && frame.draws[1].texture==pica_no_texture && frame.draws[2].texture==1
        && frame.draws[1].first==6 && frame.draws[2].first==9,"Composed source texture/geometry offsets wrong");
    require(frame.textures[0].pixels.data()==backdrop.textures[0].pixels.data(),"Compositor copied/repainted source artwork");
    require(frame.textures[0].source_layers.data()==backdrop.textures[0].source_layers.data(),"Compositor copied/repainted cached source ownership");
    const std::vector<PicaVertex> saved(frame.vertices.begin(),frame.vertices.end());
    auto wrong=models;wrong.plan.slider=0;
    rejected([&]{compositor.prepare(plan,std::array{backdrop,wrong,overlay},dashboard.view());},"Mismatched eye plans combined");
    require(std::equal(saved.begin(),saved.end(),frame.vertices.begin()),"Failed composition discarded previous native geometry");
    std::array<PicaFrame,8> oversized;oversized.fill(backdrop);
    rejected([&]{compositor.prepare(plan,oversized,dashboard.view());},"Combined padded texture budget not checked");
    const auto layers=compositor.prepare_layers(plan,groups);
    validate_pica_frame(layers,dashboard.view());
    require(layers.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),layers.vertices.begin())
        && layers.textures[0].pixels.data()==backdrop.textures[0].pixels.data(),"Dashboard-independent composition copied or flattened painter groups");
    rejected([&]{compositor.prepare_layers(plan,oversized);},"Artwork-only composition forgot the reserved lower LCD texture");
    rejected([&]{validate_pica_group(models,pica_texture_budget+1);},"Group validator accepted overflowing reserved residency");
}
void corridor_batch_contract() {
    auto ppu=source();ppu->background_mode=1;ppu->tunnel_scene=true;
    PpuBatch batch{{{PpuLayer::bg2}},PicaSpace::scenery,true};batch.corridor_receiver=true;
    PicaRaster owner;const auto plan=plan_frame(0,true,ScreenUse::world);
    const auto prepared=owner.prepare(ppu,batch,plan);
    const auto work=owner.work();const std::vector<PicaVertex> saved(prepared.vertices.begin(),prepared.vertices.end());
    for(unsigned failure=0;failure<8;++failure) {
        auto bad=batch;auto invalid=std::make_shared<simulation::SnesPpuState>(*ppu);
        if(failure==0) bad.water_receiver=true;
        if(failure==1) invalid->tunnel_scene=false;
        if(failure==2) invalid->background_mode=3;
        if(failure==3) bad.passes[0].layer=PpuLayer::bg3;
        if(failure==4) bad.space=PicaSpace::screen;
        if(failure==5) bad.passes.clear();
        if(failure==6) {bad.corridor_open_left=true;bad.corridor_receiver=false;}
        if(failure==7) {bad.corridor_open_left=true;invalid->background_mode=2;}
        rejected([&]{owner.prepare(invalid,bad,plan);},"Invalid corridor painter batch was accepted");
        require(owner.work().decodes==work.decodes && owner.work().colour_updates==work.colour_updates
            && std::equal(saved.begin(),saved.end(),prepared.vertices.begin()),"Invalid corridor batch published partial cache/geometry state");
    }
    auto colony=std::make_shared<simulation::SnesPpuState>(*ppu);colony->tunnel_scene=false;
    auto open=batch;open.corridor_open_left=true;
    const auto unchanged=*colony;const auto artwork=owner.prepare(colony,open,plan);
    require(!artwork.draws.empty() && *colony==unchanged,
        "Explicit open colony failed to decode without changing source WATER metadata");
}
void compact_strip_contract() {
    auto ppu=source();ppu->background_mode=1;
    ppu->cgram[17]=0;ppu->cgram[18]=31<<5;tile(*ppu,0x4000,1,2);
    for(unsigned row=0;row<32;++row) for(unsigned col=0;col<32;++col)
        ppu->vram[0xc000+(row*32+col)*2]=std::uint8_t(col&1);
    const auto unchanged=*ppu;const auto plan=plan_frame(0,true,ScreenUse::world);
    for(unsigned guard:{32U,80U,512U,1024U,1848U}) {
        PpuBatch ordinary{{{PpuLayer::bg2}},PicaSpace::scenery,true};
        auto compact=ordinary;compact.compact_strips=true;
        PicaRaster baseline,packed;
        const auto full=baseline.prepare(ppu,ordinary,plan,15,0,guard);
        const auto frame=packed.prepare(ppu,compact,plan,15,0,guard);
        const unsigned width=400+guard*2;const float left=-float(guard);
        unsigned start=0,bytes=0,baseline_bytes=0;
        require(frame.textures.size()<=4 && frame.draws.size()==frame.textures.size(),
            "Compact strips exceeded the native draw/storage contract");
        const auto base=frame.textures.front().pixels.data();const auto mask=frame.textures.front().source_layers.data();
        for(unsigned page=0;page<frame.textures.size();++page) {
            const auto& image=frame.textures[page];const auto& draw=frame.draws[page];
            require(image.width<=1024 && image.pitch==width*4 && image.layer_pitch==width
                && image.pixels.data()==base+start*4 && image.source_layers.data()==mask+start
                && frame.vertices[draw.first].position==Point3{left+start,0,0}
                && frame.vertices[draw.first+2].position==Point3{left+start+image.width,240,0},
                "Compact strips copied/reduced pixels or lost source UV/origin registration");
            require(draw.source_layer==2,"Compact isolated BG2 lost its one-hot source ownership");
            for(unsigned y=0;y<240;y+=7) for(unsigned x=0;x<image.width;++x) {
                const auto& original=full.textures[(start+x)/1024];const unsigned ox=(start+x)%1024;
                require(pixel(image,x,y)==pixel(original,ox,y)
                    && image.source_layers[std::size_t(y)*width+x]==original.source_layers[std::size_t(y)*width+ox],
                    "Compact strip boundaries changed source color/coverage/opaque black");
            }
            bytes+=pica_resident_texture_bytes(image);start+=image.width;
        }
        for(const auto& image:full.textures) baseline_bytes+=pica_resident_texture_bytes(image);
        require(start==width && bytes<=baseline_bytes,"Compact partition dropped coverage or wasted additional padded texture storage");
        if(guard==512) require(bytes<baseline_bytes,"Water-sized strip split did not avoid power-of-two padding waste");
        const auto cached=packed.work();
        const auto repeat=packed.prepare(ppu,compact,plan,15,0,guard);
        require(repeat.textures.front().pixels.data()==base && packed.work().decodes==cached.decodes
            && packed.work().colour_updates==cached.colour_updates,"Compact descriptors defeated source reuse");
        auto bad=compact;bad.water_receiver=true;bad.passes[0].layer=PpuLayer::bg3;
        rejected([&]{packed.prepare(ppu,bad,plan);},"BG3 was accepted as a finite BG2 water receiver");
        bad.passes[0].layer=PpuLayer::bg2;bad.space=PicaSpace::screen;
        rejected([&]{packed.prepare(ppu,bad,plan);},"Screen-space batch was accepted as finite water");
        bad.space=PicaSpace::scenery;auto mode2=std::make_shared<simulation::SnesPpuState>(*ppu);mode2->background_mode=2;
        rejected([&]{packed.prepare(mode2,bad,plan);},"Mode-2 source was mistaken for a Mode-1 water receiver");
        require(packed.work().decodes==cached.decodes && packed.work().colour_updates==cached.colour_updates
            && repeat.textures.front().pixels.data()==base,"Rejected water batch replaced a valid cached source");
    }
    auto bg3=PpuBatch{{{PpuLayer::bg3}},PicaSpace::scenery,true};bg3.compact_strips=true;PicaRaster sky;
    const auto image=sky.prepare(ppu,bg3,plan);
    require(!image.draws.empty() && image.draws.front().source_layer==4,"Isolated distant BG3 was mislabeled BG2");
    require(*ppu==unchanged,"Compact texture preparation modified cartridge state");
}
void screen_sprite_crop() {
    auto ppu=source();for(unsigned i=0;i<128;++i) ppu->oam[i*4+1]=240;
    object(*ppu,0,190,85,7);ppu->cgram[129]=0;const auto unchanged=*ppu;
    PpuBatch batch{{{PpuLayer::objects,2}},PicaSpace::screen,true};PicaRaster raster;
    const auto plan=plan_frame(1,true,ScreenUse::world);const auto full=raster.prepare(ppu,batch,plan);
    const auto pixels=full.textures[0].pixels.data(),layers=full.textures[0].source_layers.data();
    const auto crop=raster.prepare(ppu,batch,plan,15,0,32,true);
    require(crop.textures.size()==1 && crop.vertices.front().position==Point3{262,93,0},
        "Screen sprite crop shifted the authored LCD position");
    const auto& image=crop.textures[0];
    require(image.width==8 && image.height==8 && image.pitch==464*4 && image.layer_pitch==464
        && image.pixels.data()==pixels+(93*464+294)*4 && image.source_layers.data()==layers+93*464+294,
        "Screen sprite crop copied or resized source pixels instead of borrowing its rectangle");
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x)
        require(pixel(image,x,y)==std::array<unsigned,4>{0,0,0,255} && image.source_layers[y*464+x]==16,
            "Black screen-space sprite became transparent or lost OBJ ownership");
    require(raster.work().decodes==1 && raster.work().colour_updates==1 && pica_resident_texture_bytes(image)==320,
        "Screen cropping redecoded source sprites or retained unnecessary padded storage");
    raster.prepare(ppu,batch,plan,0,0,32,true);
    require(raster.work().decodes==1 && *ppu==unchanged,"Screen crop fade redecoded or changed source data");
}
void game_effect_flow() {
    auto scene=std::make_shared<vr::GameSceneSnapshot>();
    auto raster=std::make_shared<GameRasterSnapshot>();raster->brightness=15;
    raster->circle={true,128,112,65535,31,0,0,63};
    GamePresentation source;source.current=scene;source.raster=raster;
    source.plan=plan_frame(1,true,ScreenUse::front_end);
    GameEffects effects;
    for(const auto flow:{simulation::GameFlowState::controls_type,simulation::GameFlowState::controls_choice}) {
        scene->flow=flow;
        const auto frame=effects.prepare(source).colour;
        require(frame.draws.size()==1 && frame.draws.front().clip==PicaClip{96,32,208,120},
            "Native Controls demonstration circle spills outside its source flight panel");
        require(frame.vertices.front().position==Point3{96,32,0}
            && frame.vertices[2].position==Point3{208,120,0},"Controls circle geometry exceeds the demonstration panel");
    }
    constexpr std::array authored{simulation::GameFlowState::pregame_menu,simulation::GameFlowState::title,
        simulation::GameFlowState::controls_type,simulation::GameFlowState::controls_choice,
        simulation::GameFlowState::planet_select,simulation::GameFlowState::planet_travel,
        simulation::GameFlowState::game_over,simulation::GameFlowState::continue_choice,simulation::GameFlowState::finished};
    raster->wipe.active=true;raster->wipe.logic=1;raster->wipe.left.fill(0);raster->wipe.right.fill(0);
    for(unsigned flow=0;flow<=unsigned(simulation::GameFlowState::finished);++flow)
        for(bool boss:{false,true}) for(bool score:{false,true}) {
            scene->flow=simulation::GameFlowState(flow);raster->boss_roll=boss;raster->final_score=score;
            const bool expanded=score || (std::find(authored.begin(),authored.end(),scene->flow)==authored.end()
                && !(boss && scene->flow==simulation::GameFlowState::credits));
            const auto policy=game_effect_plan(source);
            require(policy.window==(expanded?WindowCoverage::full_scene:boss?WindowCoverage::vertical_scene:WindowCoverage::authored),
                "Native flow window policy changed source X/Y extension rules");
            const bool controls=scene->flow==simulation::GameFlowState::controls_type || scene->flow==simulation::GameFlowState::controls_choice;
            require(policy.circle_clip.has_value()==controls,"Non-Controls bomb/death circle inherited the demonstration clip");
            for(float slider:{0.F,.5F,1.F}) {
                source.plan=plan_frame(slider,true,ScreenUse::world);
                const auto prepared=effects.prepare(source);
                Canvas dashboard;validate_pica_frame(prepared.colour,dashboard.view());validate_pica_frame(prepared.window,dashboard.view());
                require(prepared.window.draws.size()==1 && prepared.window.draws.front().source_layer==0,
                    "Closed scene wipe lost its opaque, colour-protected post-world ownership");
                if(expanded) require(prepared.window.vertices.size()==6
                    && prepared.window.vertices[0].position==Point3{0,0,0}
                    && prepared.window.vertices[2].position==Point3{400,240,0},"Expanded intro/results/EX/final-score wipe leaves LCD edges visible");
                else if(boss) require(prepared.window.vertices[0].position==Point3{88,0,0}
                    && prepared.window.vertices[2].position==Point3{313,240,0},"Boss dossier wipe leaves the upper/lower source guard rows visible");
            }
        }
    scene->flow=simulation::GameFlowState::gameplay;raster->boss_roll=raster->final_score=false;
    raster->wipe.active=false;
    const auto world=effects.prepare(source);
    require(world.window.vertices.empty() && world.colour.draws[0].clip==std::nullopt,
        "Returning to gameplay retained a closed wipe or Controls-only circle clip");
}
void colour_effects() {
    const auto plan=plan_frame(1,true,ScreenUse::world);Canvas dashboard;PicaColourEffects effects;
    simulation::CircleEffectState circle;simulation::ColourMathEffectState math;
    require(effects.prepare(circle,math,15,plan).draws.empty(),"Inactive source effects generated a colour pass");
    circle.active=true;circle.red=31;circle.green=15;circle.blue=7;circle.affected_layers=3;
    const auto check=[&](int cx,int cy,unsigned radius,std::optional<PicaClip> clip) {
        circle.centre_x=std::int16_t(cx);circle.centre_y=std::int16_t(cy);circle.radius=std::uint16_t(radius);
        const auto frame=effects.prepare(circle,math,15,plan,clip);validate_pica_frame(frame,dashboard.view());
        require(frame.textures.empty() && frame.draws.size()<=1,"Circle flattened the world or allocated a mask bitmap/per-row draw");
        if(!frame.draws.empty()) require(frame.draws[0].colour_op==PicaColourOp{false,false,3}
            && frame.draws[0].source_layer==0 && frame.draws[0].clip==clip,"Circle lost source CGADSUB/clip contract");
        std::vector<unsigned char> coverage(top_width*screen_height,0);
        for(unsigned i=0;i<frame.vertices.size();i+=6) {
            const auto& a=frame.vertices[i];const auto& b=frame.vertices[i+2];
            const int left=int(a.position[0]),top=int(a.position[1]),right=int(b.position[0]),bottom=int(b.position[1]);
            require(left>=0 && top>=0 && left<right && top<bottom && right<=400 && bottom<=240,"Colour rectangle outside LCD");
            for(int y=top;y<bottom;++y) for(int x=left;x<right;++x) ++coverage[y*400+x];
        }
        const auto bounds=clip.value_or(PicaClip{});
        for(int y=0;y<240;++y) for(int x=0;x<400;++x) {
            const std::int64_t dx=x-cx-72,dy=y-cy-8;
            const bool inside=radius && dx*dx+dy*dy<=std::int64_t(radius)*radius
                && x>=bounds.left && x<bounds.right && y>=bounds.top && y<bounds.bottom;
            require(coverage[y*400+x]==unsigned(inside),"Native disk differs from independent source pixel circle (overlap/edge/clip)");
        }
        const auto builds=effects.builds();const auto* vertices=frame.vertices.data();
        for(float slider:{0.F,.5F,1.F}) {
            const auto other=effects.prepare(circle,math,15,plan_frame(slider,true,ScreenUse::world),clip);
            require(other.vertices.data()==vertices && effects.builds()==builds,"Slider/second eye rebuilt colour coverage");
        }
    };
    for(unsigned radius:{0U,1U,2U,31U,96U,240U,65535U}) check(128,112,radius,{});
    check(-32768,-32768,65535,{});check(32767,32767,1,{});check(-73,-8,30,{});
    check(128,112,80,PicaClip{140,80,260,180});
    circle.centre_x=128;circle.centre_y=112;circle.radius=40;
    math.active=true;math.subtract=true;math.half=true;math.affected_layers=0x2f;math.red=31;math.green=5;math.blue=0;
    for(unsigned brightness=0;brightness<16;++brightness) {
        const auto frame=effects.prepare(circle,math,brightness,plan);validate_pica_frame(frame,dashboard.view());
        require(frame.draws.size()==2 && frame.draws[0].colour_op==PicaColourOp{false,false,3}
            && frame.draws[1].colour_op==PicaColourOp{true,true,0x2f},"Source circle/global colour math order or add/sub/half semantics lost");
        for(unsigned channel=0;channel<3;++channel) {
            const unsigned five=std::array<unsigned,3>{31,15,7}[channel];
            const auto expanded=((five<<3)|(five>>2))*brightness/15;
            const auto fixed=expanded>>3;const auto expected=(fixed<<3)|(fixed>>2);
            require(std::abs(frame.vertices[0].colour[channel]*255-expected)<.001F,"Circle fixed-colour brightness/5-bit conversion mismatch");
        }
        require(frame.vertices[frame.draws[1].first].colour[0]==1,"Global fixed-colour fade incorrectly scaled by source brightness");
        const auto last=frame.draws[1].first;
        require(frame.vertices[last].position==Point3{0,0,0} && frame.vertices[last+2].position==Point3{400,240,0},
            "Full-screen source flash/fade left uncovered LCD margins");
    }
    for(unsigned flags:{0U,64U,128U,192U}) {
        circle.affected_layers=std::uint8_t(flags|17);
        const auto frame=effects.prepare(circle,math,15,plan);
        require(frame.draws[0].colour_op==PicaColourOp{bool(flags&128),bool(flags&64),17},"Circle CGADSUB high bits not decoded independently");
    }
    const auto retained=effects.prepare(circle,math,15,plan);const auto builds=effects.builds();
    const std::vector<PicaVertex> saved(retained.vertices.begin(),retained.vertices.end());
    rejected([&]{effects.prepare(circle,math,16,plan);},"Unsupported brightness accepted");
    rejected([&]{effects.prepare(circle,math,15,plan,PicaClip{0,0,0,240});},"Empty source circle clip accepted");
    require(effects.builds()==builds && std::equal(saved.begin(),saved.end(),retained.vertices.begin()),"Failed colour preparation discarded last complete native pass");
    auto bad_draw=retained.draws.front();bad_draw.colour_op->layers=0;
    PicaFrame bad{plan,std::span(retained.vertices).first(bad_draw.count),std::span(&bad_draw,1),{}};
    rejected([&]{validate_pica_frame(bad,dashboard.view());},"Colour operation without selected source layers accepted");
    bad_draw=retained.draws.front();bad_draw.source_layer=1;
    rejected([&]{validate_pica_frame(bad,dashboard.view());},"Colour operation may not replace source ownership");
    bad_draw=retained.draws.front();bad_draw.alpha_blend=true;
    rejected([&]{validate_pica_frame(bad,dashboard.view());},"Ordinary opacity may not silently override colour-math blend");
    circle.active=false;math.active=false;
    require(effects.prepare(circle,math,15,plan).vertices.empty(),"Retired source colour math left stale damage/death effects");
    circle.active=true;circle.affected_layers=128;circle.radius=40;
    require(effects.prepare(circle,math,15,plan).draws.empty(),"Circle with no affected source layers generated a blend pass");
    // Exhaust each source-mask selector from a real prepared operation. This
    // proves the native contract; physical PICA stencil/colour pixels remain
    // a separate acceptance test, not inferred from this portable oracle.
    for(unsigned selected=1;selected<64;++selected) {
        circle.affected_layers=std::uint8_t(selected|128);math.active=false;
        const auto selected_frame=effects.prepare(circle,math,15,plan);
        const auto op=*selected_frame.draws[0].colour_op;
        for(unsigned bit=0;bit<6;++bit) require(bool(op.layers&(1U<<bit))==bool((selected>>bit)&1),
            "Prepared colour operation included/excluded wrong source layer");
        require(op.subtract && !op.half && selected_frame.draws[0].source_layer==0,
            "Prepared subtract operation unexpectedly halves or overwrites source ownership");
    }
}
bool expected_mask(const simulation::WindowWipeState& wipe,WindowCoverage coverage,unsigned x,unsigned y) {
    if(!wipe.active) return false;
    const bool wide=coverage==WindowCoverage::full_scene;
    const bool vertical=coverage!=WindowCoverage::authored;
    const int sx=wide?16+int(x*223/399):int(x)-72;
    if(wipe.horizontal_opening) {
        const double sy=vertical?(double(y)+.5)*.8:double(y)+.5-24;
        if(sy<0 || sy>=192) return false;
        if(sy<wipe.opening_top || sy>=wipe.opening_bottom) return true;
        return wide?x<2:(!(sx>=15 && sx<=16))!=(sx>=16 && sx<=240);
    }
    const int sy=vertical?int(y*191/239):int(y)-24;
    if(sy<0 || sy>=192) return false;
    const int left=std::uint8_t(wipe.left[sy]),right=std::uint8_t(wipe.right[sy]);
    const bool dynamic=left<=right?(sx>=left && sx<=right):(sx>=left || sx<=right);
    const bool first=!dynamic,second=sx>=16 && sx<=240;
    if((wipe.logic&3)==0) return first || second;
    if((wipe.logic&3)==1) return first && second;
    if((wipe.logic&3)==2) return first!=second;
    return first==second;
}
void window_masks() {
    const auto plan=plan_frame(1,true,ScreenUse::world);Canvas dashboard;PicaWindow window;
    simulation::WindowWipeState wipe;wipe.active=true;
    const auto check=[&](WindowCoverage coverage) {
        const auto before=wipe;
        const auto frame=window.prepare(wipe,plan,coverage);validate_pica_frame(frame,dashboard.view());
        require(frame.textures.empty() && frame.draws.size()<=1,"Source wipe allocated a full world/mask bitmap or one draw per row");
        if(!frame.draws.empty()) require(frame.draws[0].space==PicaSpace::screen && !frame.draws[0].depth_test
            && !frame.draws[0].depth_write && !frame.draws[0].alpha_blend,"Black source wipe lost post-composition screen role");
        std::vector<std::uint8_t> actual(top_width*screen_height,0);
        for(unsigned i=0;i<frame.vertices.size();i+=6) {
            const auto& a=frame.vertices[i];const auto& b=frame.vertices[i+2];
            const unsigned left=unsigned(a.position[0]),top=unsigned(a.position[1]);
            const unsigned right=unsigned(b.position[0]),bottom=unsigned(b.position[1]);
            require(left<right && top<bottom && right<=400 && bottom<=240,"Native wipe emits out-of-bounds/empty rectangle");
            for(unsigned j=i;j<i+6;++j) require(frame.vertices[j].colour==std::array<float,4>{0,0,0,1},"Wipe rectangle not opaque black");
            for(unsigned y=top;y<bottom;++y) for(unsigned x=left;x<right;++x) ++actual[y*400+x];
        }
        for(unsigned y=0;y<240;++y) for(unsigned x=0;x<400;++x)
            require(actual[y*400+x]==unsigned(expected_mask(wipe,coverage,x,y)),
                "Native source-window geometry disagrees with independent per-pixel logic or leaves an edge seam");
        const auto builds=window.builds();const auto* data=frame.vertices.data();
        for(float slider:{0.F,.5F,1.F}) {
            const auto other=window.prepare(wipe,plan_frame(slider,true,ScreenUse::world),coverage);
            require(window.builds()==builds && other.vertices.data()==data,"Slider/second eye rebuilt source window mask");
        }
        require(wipe.left==before.left && wipe.right==before.right && wipe.opening_top==before.opening_top
            && wipe.opening_bottom==before.opening_bottom,"Window preparation mutated native raster state");
    };
    for(unsigned logic=0;logic<4;++logic) for(unsigned phase=0;phase<5;++phase) {
        wipe.logic=logic;
        for(unsigned y=0;y<192;++y) {
            wipe.left[y]=std::uint16_t((y*13+phase*59)&511);
            wipe.right[y]=std::uint16_t((y*7+255-phase*39)&511);
        }
        check(WindowCoverage::authored);check(WindowCoverage::full_scene);check(WindowCoverage::vertical_scene);
    }
    wipe.logic=1;wipe.left.fill(0);wipe.right.fill(0);check(WindowCoverage::full_scene);
    const auto closed=window.prepare(wipe,plan,WindowCoverage::full_scene);
    require(closed.vertices.size()==6 && closed.vertices[0].position==Point3{0,0,0}
        && closed.vertices[2].position==Point3{400,240,0},"Closed Training wipe must cover every added LCD column/row");
    wipe.horizontal_opening=true;
    for(double edge:{0.,32.125,63.5,96.}) {
        wipe.opening_top=edge;wipe.opening_bottom=192-edge;
        check(WindowCoverage::authored);check(WindowCoverage::full_scene);check(WindowCoverage::vertical_scene);
    }
    const auto retained=window.prepare(wipe,plan,WindowCoverage::full_scene);
    const std::vector<PicaVertex> saved(retained.vertices.begin(),retained.vertices.end());const auto builds=window.builds();
    auto bad=wipe;bad.opening_top=std::numeric_limits<double>::quiet_NaN();
    rejected([&]{window.prepare(bad,plan,WindowCoverage::full_scene);},"Non-finite shutter accepted");
    require(window.builds()==builds && std::equal(saved.begin(),saved.end(),retained.vertices.begin()),"Failed window transaction lost last complete mask");
    const std::array<PicaVertex,3> geometry{{{{-1,-1,512}},{{1,-1,512}},{{0,1,512}}}};
    const std::array<PicaDraw,1> draw{{{0,3}}};const PicaFrame models{plan,geometry,draw,{}};
    PicaComposite composite;const auto composed=composite.prepare(plan,std::array{models,retained},dashboard.view());
    require(composed.draws.front().space==PicaSpace::world && composed.draws.back().space==PicaSpace::screen
        && std::equal(geometry.begin(),geometry.end(),composed.vertices.begin()),"Window flattened or reordered stereo model geometry");
    wipe.active=false;
    require(window.prepare(wipe,plan,WindowCoverage::authored).vertices.empty(),"Retired source wipe left a stale black shutter");
    require(pica_screen_scissor({10,20,30,40})==std::array<unsigned,4>{200,370,220,390},"LCD-to-rotated-target scissor mapping wrong");
    rejected([&]{pica_screen_scissor({0,0,400,241});},"Off-LCD effect scissor accepted");
    rejected([&]{pica_screen_scissor({0,0,0,240});},"Empty effect scissor accepted");
}
}
int main() try {game_effect_flow();raster();pass_memory_dependencies();optical_coverage();disjoint_scenery_coverage();transparent_priority_crop();unique_sky_halves();composition();corridor_batch_contract();compact_strip_contract();screen_sprite_crop();window_masks();colour_effects();std::cout<<checks<<" 3DS native PPU/cache/composition checks passed; NOT full game/hardware acceptance\n";}
catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
