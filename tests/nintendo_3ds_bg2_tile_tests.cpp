#include "starfox/platform/nintendo_3ds/pica_bg2_tiles.hpp"
#include "starfox/platform/nintendo_3ds/game_layers.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
std::uint64_t checks{};
std::atomic<std::size_t> allocations{};
bool count_allocations{};
void require(bool value,const char* why) {++checks;if(!value) throw std::runtime_error(why);}
unsigned seed=0x8d37264f;
unsigned random_word() {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
using Pixel=std::array<std::uint8_t,5>; // RGBA + source ownership.
std::vector<Pixel> sample(const PicaFrame& frame,unsigned width,int left) {
    std::vector<Pixel> result(std::size_t(width)*240);
    for(const auto& draw:frame.draws) {
        require((draw.space==PicaSpace::screen || draw.space==PicaSpace::scenery) && !draw.depth_test && !draw.depth_write
            && draw.source_layer==2 && draw.texture!=pica_no_texture,"Tile draw changed BG2 painter ownership/depth");
        const auto& image=frame.textures[draw.texture];
        for(unsigned at=draw.first;at<draw.first+draw.count;at+=6) {
            const auto& a=frame.vertices[at];const auto& c=frame.vertices[at+2];
            const int x0=int(a.position[0]),y0=int(a.position[1]),x1=int(c.position[0]),y1=int(c.position[1]);
            require(x0<x1 && y0<y1,"Tile quad reversed its source rectangle");
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                if(x<left || x>=left+int(width) || y<0 || y>=240) continue;
                // Independent inverse nearest sampling at pixel centres.
                const double u=a.uv[0]+(c.uv[0]-double(a.uv[0]))*(x+.5-x0)/(x1-x0);
                const double v=a.uv[1]+(c.uv[1]-double(a.uv[1]))*(y+.5-y0)/(y1-y0);
                const unsigned sx=unsigned(std::clamp(u*image.width,0.,double(image.width-1)));
                const unsigned sy=unsigned(std::clamp(v*image.height,0.,double(image.height-1)));
                const auto offset=std::size_t(sy)*image.pitch+sx*4;
                if(!image.pixels[offset+3]) continue;
                result[std::size_t(y)*width+unsigned(x-left)]={image.pixels[offset],image.pixels[offset+1],
                    image.pixels[offset+2],image.pixels[offset+3],2};
            }
        }
    }
    return result;
}
void parity(std::shared_ptr<const simulation::SnesPpuState> ppu,const PpuBatch& batch,
    const FramePlan& plan,unsigned brightness,unsigned subtract,PicaBg2Tiles& owner,bool complete_roll=false,unsigned guard=32) {
    const auto prepared=owner.prepare(ppu,batch,plan,brightness,subtract,pica_vertex_limit,guard,complete_roll);
    require(bool(prepared),"Supported ordinary tile fixture fell back");
    const unsigned width=batch.expand_horizontal?400+2*std::max(guard,batch.space==PicaSpace::scenery?pica_scenery_guard(plan):32):256;
    const int left=(400-int(width))/2,origin=(int(width)-256)/2;
    const auto actual=sample(*prepared,width,left);
    render::Framebuffer reference(width,224);reference.enable_layer_tags(true);reference.begin_write_coverage();
    const auto& pass=batch.passes[0];
    const auto scroll=pass.scroll.value_or(std::array{ppu->bg2_scroll_x,ppu->bg2_scroll_y});
    const auto priority=pass.priority<0?render::TilePriorityPass::all:
        pass.priority?render::TilePriorityPass::high:render::TilePriorityPass::low;
    render::BackgroundRenderer{}.draw_bg2(*ppu,scroll[0],scroll[1],reference,priority,origin,batch.expand_horizontal);
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<width;++x) {
        Pixel expected{};
        const unsigned row=unsigned(std::clamp(int(y)-8,0,223));
        if((batch.space==PicaSpace::scenery || (y>=8 && y<232)) && reference.write_coverage()[std::size_t(row)*width+x]) {
            const unsigned word=ppu->cgram[reference.get(x,row)];
            for(unsigned channel=0;channel<3;++channel) {
                const unsigned five=unsigned(std::max(0,int((word>>(channel*5))&31)-int(subtract)));
                expected[channel]=std::uint8_t(((five<<3)|(five>>2))*brightness/15);
            }
            expected[3]=255;expected[4]=2;
        }
        if(actual[std::size_t(y)*width+x]!=expected) {
            std::cerr<<"Mismatch x="<<x<<" y="<<y<<" width="<<width<<" vertical="<<ppu->bg2_vertical_offsets_enabled
                <<" brightness="<<brightness<<" expected="<<unsigned(expected[0])<<','<<unsigned(expected[3])
                <<" actual="<<unsigned(actual[std::size_t(y)*width+x][0])<<','<<unsigned(actual[std::size_t(y)*width+x][3])<<'\n';
        }
        require(actual[std::size_t(y)*width+x]==expected,"GPU atlas disagrees with original indexed pixels/coverage/palette/ownership");
    }
    unsigned bytes=512*256*4;
    for(const auto image:prepared->textures) bytes+=pica_resident_texture_bytes(image);
    require(bytes<=768*1024,"Compact tile atlas exceeded its fixed native budget");
}
std::shared_ptr<simulation::SnesPpuState> fixture(unsigned number) {
    auto ppu=std::make_shared<simulation::SnesPpuState>();
    ppu->background_mode=2;ppu->main_screen=2;ppu->bg2_screen_base=0x6000;
    ppu->bg2_character_base=(number&8)?0x7800:0x1000;ppu->bg2_screen_size=std::uint8_t(number%4);
    ppu->bg2_tile_size_16=(number&4)!=0;
    ppu->bg2_scroll_x=std::int16_t(int(random_word()%1024)-512);
    ppu->bg2_scroll_y=std::int16_t(int(random_word()%1024)-512);
    for(auto& byte:ppu->vram) byte=std::uint8_t(random_word());
    for(unsigned entry=0;entry<4096;++entry) {
        const unsigned value=random_word(),tile=(value&31)|((value&7)<<10)|((value&7)<<13);
        const unsigned at=(ppu->bg2_screen_base*2+entry*2)&65535;
        ppu->vram[at]=std::uint8_t(tile);ppu->vram[at+1]=std::uint8_t(tile>>8);
    }
    for(auto& word:ppu->cgram) word=std::uint16_t(random_word()&32767);
    for(unsigned bank=0;bank<8;++bank) ppu->cgram[bank*16+1]=0; // Opaque black.
    ppu->bg2_horizontal_offsets_enabled=(number&16)!=0;ppu->bg2_scanline_scroll_enabled=(number&32)!=0;
    for(unsigned band=0;band<14;++band) {
        const auto x=std::int16_t(int(random_word()%1024)-512),y=std::int16_t(int(random_word()%1024)-512);
        for(unsigned row=band*16;row<band*16+16;++row) {ppu->bg2_horizontal_offsets[row]=x;ppu->bg2_scanline_scroll_y[row]=y;}
    }
    return ppu;
}
void rejection_and_reuse() {
    auto ppu=fixture(0);PpuBatch batch;batch.expand_horizontal=true;batch.passes.push_back({PpuLayer::bg2});
    PicaBg2Tiles owner;const auto plan=plan_frame(1,true,ScreenUse::world);
    auto frame=owner.prepare(ppu,batch,plan,15,0,pica_vertex_limit);require(bool(frame),"Initial cache fixture failed");
    const auto* vertices=frame->vertices.data();const auto* pixels=frame->textures[0].pixels.data();
    allocations=0;count_allocations=true;
    bool stable=true;
    for(unsigned i=0;i<2048;++i) {
        const auto warm=owner.prepare(ppu,batch,plan,15,0,pica_vertex_limit);
        stable&=warm && warm->vertices.data()==vertices && warm->textures[0].pixels.data()==pixels;
    }
    count_allocations=false;
    require(stable && allocations==0,"Warmed tile preparation allocated or replaced its complete geometry/atlas");
    const auto old_pixels=sample(*frame,464,-32);
    const auto before=owner.work();
    auto changed=std::make_shared<simulation::SnesPpuState>(*ppu);changed->oam[0]^=7;
    parity(changed,batch,plan,15,0,owner);
    require(owner.work().decodes==before.decodes && owner.work().colour_updates==before.colour_updates,"OAM defeated isolated BG2 caching");
    changed=std::make_shared<simulation::SnesPpuState>(*changed);changed->cgram[17]^=31;
    parity(changed,batch,plan,7,3,owner);
    require(owner.work().decodes==before.decodes && owner.work().colour_updates==before.colour_updates+1,"Palette fade reran tile geometry or failed to recolour atlas");
    const auto saved=owner.prepare(changed,batch,plan,7,3,pica_vertex_limit);
    const auto saved_pixels=sample(*saved,464,-32);
    require(!owner.prepare(changed,batch,plan,7,3,5),"Capacity overflow published an incomplete BG2 scene");
    require(sample(*saved,464,-32)==saved_pixels,"Rejected capacity invalidated previous borrowed frame");
    for(unsigned rejected_case=0;rejected_case<8;++rejected_case) {
        auto bad=std::make_shared<simulation::SnesPpuState>(*changed);auto policy=batch;
        switch(rejected_case) {
        case 0:bad->background_mode=1;break;
        case 1:bad->mosaic=0x12;break;
        case 2:bad->bg2_vertical_offsets_enabled=true;break;
        case 3:bad->tunnel_scene=true;break;
        case 4:policy.passes[0].single_occurrence_top_rows=112;break;
        case 5:policy.passes[0].wrap_horizontal=false;break;
        case 6:policy.passes.push_back({PpuLayer::objects});break;
        case 7:policy.space=PicaSpace::world;break;
        }
        require(!owner.prepare(bad,policy,plan,7,3,pica_vertex_limit),"Complex source policy incorrectly bypassed exact raster fallback");
        require(sample(*saved,464,-32)==saved_pixels,"Rejected source policy replaced complete cached BG2");
    }
    auto inactive=std::make_shared<simulation::SnesPpuState>(*changed);inactive->main_screen=0;
    const auto empty=owner.prepare(inactive,batch,plan,7,3,pica_vertex_limit);
    require(empty && empty->vertices.empty() && empty->draws.empty() && empty->textures.empty(),"Disabled BG2 left stale native geometry");
    require(old_pixels!=saved_pixels,"Fade fixture never changed actual BG2 colours");
}
void painter_integration() {
    auto ppu=fixture(0);auto scene=std::make_shared<vr::GameSceneSnapshot>();scene->flow=simulation::GameFlowState::training;
    auto raster=std::make_shared<GameRasterSnapshot>();raster->ppu=ppu;raster->brightness=15;
    GamePresentation source;source.current=source.previous=scene;source.raster=raster;source.plan=plan_frame(1,true,ScreenUse::world);
    GameLayers native,reference;
    const auto gpu=native.prepare(source,pica_vertex_limit),cpu=reference.prepare(source);
    require(gpu.before_models.textures.size()==1 && gpu.before_models.textures[0].width==256,"Native painter did not use direct source tile atlas");
    require(gpu.after_models.vertices.size()==cpu.after_models.vertices.size() && gpu.clear==cpu.clear,"GPU BG2 changed foreground/backdrop policy");
    // A tiny whole-scene budget must select the reference path, not clip tiles.
    const auto fallback=native.prepare(source,5);
    require(fallback.before_models.vertices.size()==cpu.before_models.vertices.size()
        && fallback.before_models.textures[0].width==cpu.before_models.textures[0].width,"Whole-scene vertex overflow did not restore the complete raster");
    // A level can have finite ground before its roll-offset enable bit is set.
    // The atlas uses its own finite-quad contract, never the raster-strip one.
    scene=std::make_shared<vr::GameSceneSnapshot>(*scene);scene->background_landscape=true;scene->landscape_grid_height=-256;
    source.current=source.previous=scene;
    const auto terrain=native.prepare(source,pica_vertex_limit);
    require(native_landscape_scene(source) && !terrain.before_models.textures.empty()
        && terrain.before_models.textures[0].width==256
        && std::any_of(terrain.before_models.draws.begin(),terrain.before_models.draws.end(),[](const auto& draw) {
            return draw.space==PicaSpace::world && draw.depth_test && draw.depth_write && draw.projected_uv;
        }),"Unrolled tile terrain lost its finite source plane");
}
void constant_vertical_offsets() {
    for(unsigned number=0;number<8;++number) {
        auto ppu=fixture(number);ppu->bg2_character_base=0x1000;
        for(unsigned character=0;character<64;++character) for(unsigned y=0;y<8;++y) {
            const unsigned at=0x2000+character*32+y*2;
            ppu->vram[at]=255;ppu->vram[at+1]=ppu->vram[at+16]=ppu->vram[at+17]=0;
        }
        ppu->bg2_vertical_offsets_enabled=true;
        for(unsigned i=0;i<32;++i) {ppu->vram[0x5f40+i*2]=16;ppu->vram[0x5f41+i*2]=0x40;}
        // A valid offset overrides both a pass's VOFS and scanline HDMA.
        ppu->bg2_scanline_scroll_enabled=true;ppu->bg2_scanline_scroll_y.fill(-511);
        PpuBatch batch;batch.expand_horizontal=(number&1)!=0;batch.space=PicaSpace::scenery;
        batch.passes.push_back({PpuLayer::bg2,(number&1)?-1:int(number%3)-1});batch.passes[0].scroll=std::array<std::int16_t,2>{-7,257};
        PicaBg2Tiles owner;const auto plan=plan_frame(1,true,ScreenUse::world);
        parity(ppu,batch,plan,15,0,owner);
        const auto before=owner.work();auto fade=std::make_shared<simulation::SnesPpuState>(*ppu);fade->cgram[1]^=31;
        parity(fade,batch,plan,7,3,owner);
        require(owner.work().decodes==before.decodes,"Constant offset palette fade rebuilt its geometry");
        if(!(number&1)) continue;
        const auto saved=owner.prepare(fade,batch,plan,7,3,pica_vertex_limit);
        const auto saved_pixels=sample(*saved,464,-32);
        auto priority_policy=batch;priority_policy.passes[0].priority=0;
        require(!owner.prepare(fade,priority_policy,plan,7,3,pica_vertex_limit),"Lower priority holes lost reference carry");
        for(unsigned rejection=0;rejection<4;++rejection) {
            auto bad=std::make_shared<simulation::SnesPpuState>(*fade);
            if(rejection==0) bad->vram[0x5f42]=17; // A real slope, not flat space.
            if(rejection==1) bad->vram[0x5f43]=0; // A register-fallback gap.
            if(rejection==2) for(unsigned i=0;i<32;++i) {bad->vram[0x5f40+i*2]=255;bad->vram[0x5f41+i*2]=0x43;}
            if(rejection==3) for(unsigned character=0;character<64;++character) for(unsigned y=0;y<8;++y)
                bad->vram[0x2000+character*32+y*2]=0; // New zero texels need reference carry.
            require(!owner.prepare(bad,batch,plan,7,3,pica_vertex_limit),"Offset/carry policy incorrectly bypassed raster fallback");
            require(sample(*saved,464,-32)==saved_pixels,"Offset/carry rejection invalidated the previous complete atlas");
        }
        auto shifted=std::make_shared<simulation::SnesPpuState>(*fade);
        for(unsigned i=0;i<32;++i) shifted->vram[0x5f40+i*2]=24;
        parity(shifted,batch,plan,7,3,owner);
        require(owner.work().decodes==before.decodes+1,"Updated offset table did not invalidate native geometry");
    }
}
void rolled_offsets_and_carry() {
    for(unsigned number=0;number<24;++number) {
        auto ppu=fixture(number);ppu->bg2_character_base=0x1000;
        // Include uniform palette bands, zero texels, opaque RGB black and
        // ordinary multicolour source tiles, rather than an invented floor.
        for(unsigned character=0;character<64;++character) if(character%4!=3 || number>=16) {
            const unsigned ink=character%16;
            for(unsigned y=0;y<8;++y) for(unsigned plane=0;plane<4;++plane)
                ppu->vram[0x2000+character*32+y*2+(plane&1)+(plane>=2?16:0)]=(ink&(1U<<plane))?255:0;
        }
        ppu->bg2_vertical_offsets_enabled=true;
        const double slope=number%2?.25:-.25;
        const int start=number%3==0?8190:number%3==1?1008:8;
        for(unsigned i=0;i<32;++i) {
            unsigned offset=unsigned(start+int(std::lround(slope*(i+1))))&8191;
            if(number%4!=0 || i%5!=0) offset|=0x4000;
            ppu->vram[0x5f40+i*2]=std::uint8_t(offset);ppu->vram[0x5f41+i*2]=std::uint8_t(offset>>8);
        }
        // Exercise register/HDMA fallback when the entire offset table is
        // disabled, and sticky lower wrapping when a valid fit is present.
        if(number>=20) for(unsigned i=0;i<32;++i) ppu->vram[0x5f41+i*2]&=0x3f;
        ppu->bg2_scanline_scroll_enabled=true;ppu->bg2_horizontal_offsets_enabled=true;
        for(unsigned y=0;y<224;++y) {
            ppu->bg2_horizontal_offsets[y]=std::int16_t((y/32)*7-29);
            ppu->bg2_scanline_scroll_y[y]=std::int16_t(y>=176?257:60);
        }
        PpuBatch batch;batch.expand_horizontal=number%4!=0;batch.space=PicaSpace::scenery;
        batch.passes.push_back({PpuLayer::bg2,int(number%3)-1});
        PicaBg2Tiles owner;const auto plan=plan_frame(1,true,ScreenUse::world);
        const auto unchanged=*ppu;
        parity(ppu,batch,plan,number%16,number%5,owner,true,40);
        const auto before=owner.work();auto fade=std::make_shared<simulation::SnesPpuState>(*ppu);fade->cgram[17]^=31;
        parity(fade,batch,plan,7,3,owner,true,40);
        require(owner.work().decodes==before.decodes,"Rolled source palette fade rebuilt tile geometry");
        require(ppu->vram==unchanged.vram && ppu->cgram==unchanged.cgram,"Rolled tile planning changed source memory");
    }
}
}
// Only count native owner preparation, not fixture creation or pixel oracles.
void* operator new(std::size_t count) {
    if(count_allocations) ++allocations;
    if(auto* memory=std::malloc(std::max(count,std::size_t{1}))) return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t count) {return ::operator new(count);}
void operator delete(void* memory) noexcept {std::free(memory);}
void operator delete[](void* memory) noexcept {std::free(memory);}
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}
int main() try {
    for(unsigned number=0;number<64;++number) {
        const auto ppu=fixture(number);const auto unchanged=*ppu;
        PpuBatch batch;batch.expand_horizontal=(number&1)!=0;batch.passes.push_back({PpuLayer::bg2,int(number%3)-1});
        if(number&2) batch.passes[0].scroll=std::array<std::int16_t,2>{-511,257};
        PicaBg2Tiles owner;parity(ppu,batch,plan_frame((number&1)?1.F:0.F,true,ScreenUse::world),number%16,number%5,owner);
        if(number%4==0) {
            batch.space=PicaSpace::scenery;batch.expand_horizontal=true;batch.visible_scenery_only=true;
            PicaBg2Tiles infinity;
            parity(ppu,batch,plan_frame(1,true,ScreenUse::world),number%16,number%5,infinity);
        }
        require(ppu->vram==unchanged.vram && ppu->cgram==unchanged.cgram && ppu->oam==unchanged.oam,"Native tile planning mutated cartridge memory");
    }
    rejection_and_reuse();painter_integration();constant_vertical_offsets();rolled_offsets_and_carry();
    std::cout<<"Native BG2 tile planner/atlas: "<<checks<<" exact pixel, palette, ownership, HDMA, flips, budget and fallback checks PASS\n";
    return 0;
} catch(const std::exception& error) {count_allocations=false;std::cerr<<error.what()<<'\n';return 1;}
