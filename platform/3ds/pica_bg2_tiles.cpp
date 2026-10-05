// Tile atlas/HDMA quad integration inspired by Esteban PDN's starwing-3ds.
// Adapted for our exact RGB8, source-layer stencil and shared-eye PICA owner.
#include "starfox/platform/nintendo_3ds/pica_bg2_tiles.hpp"
#include <cstring>

namespace starfox::platform::nintendo_3ds {
namespace {
bool same_range(const simulation::SnesPpuState& a,const simulation::SnesPpuState& b,unsigned first,unsigned count) {
    first&=65535;const unsigned head=std::min(count,65536-first);
    return std::memcmp(a.vram.data()+first,b.vram.data()+first,head)==0
        && std::memcmp(a.vram.data(),b.vram.data(),count-head)==0;
}
bool same_geometry(const simulation::SnesPpuState& a,const simulation::SnesPpuState& b) {
    const unsigned pages=((a.bg2_screen_size&1)?2:1)*((a.bg2_screen_size&2)?2:1);
    return a.background_mode==b.background_mode && (a.main_screen&2)==(b.main_screen&2) && a.bg2_screen_base==b.bg2_screen_base
        && a.bg2_screen_size==b.bg2_screen_size && a.bg2_tile_size_16==b.bg2_tile_size_16
        && a.bg2_scroll_x==b.bg2_scroll_x && a.bg2_scroll_y==b.bg2_scroll_y
        && a.bg2_horizontal_offsets_enabled==b.bg2_horizontal_offsets_enabled
        && a.bg2_scanline_scroll_enabled==b.bg2_scanline_scroll_enabled
        && a.bg2_vertical_offsets_enabled==b.bg2_vertical_offsets_enabled
        && (!a.bg2_horizontal_offsets_enabled || a.bg2_horizontal_offsets==b.bg2_horizontal_offsets)
        && (!a.bg2_scanline_scroll_enabled || a.bg2_scanline_scroll_y==b.bg2_scanline_scroll_y)
        && (!a.bg2_vertical_offsets_enabled || (same_range(a,b,0x5f40,64)
            && a.bg2_character_base==b.bg2_character_base
            && same_range(a,b,a.bg2_character_base*2,32768)))
        && same_range(a,b,a.bg2_screen_base*2,pages*2048);
}
}
std::optional<PicaFrame> PicaBg2Tiles::prepare(std::shared_ptr<const simulation::SnesPpuState> source,
    const PpuBatch& batch,const FramePlan& plan,unsigned brightness,unsigned subtract,unsigned vertex_budget,
    unsigned source_guard,bool complete_roll) {
    if(!source || brightness>15 || subtract>31) throw std::invalid_argument("Invalid 3DS GPU tile source");
    vertex_budget=std::min(vertex_budget,pica_vertex_limit);
    const bool water=batch.water_receiver && batch.space==PicaSpace::scenery && source->background_mode==1
        && batch.expand_horizontal && complete_roll;
    const bool mode1_scenery=source->background_mode==1 && batch.space==PicaSpace::scenery;
    const bool complete_plan=complete_roll || mode1_scenery;
    if(batch.passes.size()!=1 || (batch.space!=PicaSpace::screen && batch.space!=PicaSpace::scenery)
        || (batch.water_receiver && !water) || batch.corridor_receiver || (batch.compact_strips && !water)
        || batch.first_row!=0 || batch.last_row!=224) return {};
    const auto& pass=batch.passes.front();
    if(pass.layer!=PpuLayer::bg2 || !pass.wrap_horizontal || pass.transparent_black
        || pass.mosaic_inset || pass.guard_inset || pass.single_occurrence_top_rows || pass.single_occurrence_sky_half
        || (source->background_mode!=2 && !mode1_scenery) || (source->mosaic&2) || (water && !pass.extend_horizontal)
        || source->tunnel_scene || pass.priority< -1 || pass.priority>1) return {};
    if(plan.eye_count!=(plan.stereo?2U:1U)) throw std::invalid_argument("Invalid 3DS GPU tile eye plan");
    for(unsigned eye=0;eye<plan.eye_count;++eye) static_cast<void>(PicaProjection(plan,eye));
    if(source_guard>(pica_raster_max_width-top_width)/2) throw std::invalid_argument("3DS tile guard exceeds source storage");
    const unsigned guard=std::max(source_guard,batch.space==PicaSpace::scenery?pica_scenery_guard(plan):pica_raster_base_guard);
    unsigned width=batch.expand_horizontal && pass.extend_horizontal?top_width+2*guard:256;
    const auto scroll=pass.scroll.value_or(std::array{source->bg2_scroll_x,source->bg2_scroll_y});
    bool decode=!source_ || batch!=batch_ || complete_roll_!=complete_plan
        || !same_geometry(*source_,*source) || (complete_plan && (source_->bg2_character_base!=source->bg2_character_base
            || !same_range(*source_,*source,source->bg2_character_base*2,32768)));
    // Slider-only presentations may need less guard than an already decoded
    // frame. Retain sufficient coverage, as PicaRaster does, rather than
    // alternating source traversals while the game's PPU remains unchanged.
    if(!decode) width=std::max(width,width_);
    decode=decode || width_!=width;
    std::vector<Bg2TileRect> rectangles;
    // Mode 1 panorama HDMA uses the source's clamped bridge margins too,
    // but keeps the existing infinity projection; it is not a water plane.
    const auto planner=complete_plan?plan_rolled_bg2_tiles:plan_bg2_tiles;
    if(decode && !planner(*source,scroll[0],scroll[1],width,int((width-256)/2),pass.priority,
        rectangles,std::min(4096U,vertex_budget/6))) return {};
    const auto& rects=decode?rectangles:rectangles_;
    const unsigned edge_quads=batch.space==PicaSpace::scenery?unsigned(std::count_if(rects.begin(),rects.end(),
        [](const auto& rect){return rect.y==0 || rect.y+rect.height==224;})):0;
    if((rects.size()+edge_quads)*6>vertex_budget) return {};
    std::vector<std::uint16_t> keys;
    if(decode) {
        keys.reserve(rects.size());
        for(const auto& rect:rects) keys.push_back(std::uint16_t(rect.solid_index?8192+rect.solid_index:rect.character*8+rect.bank));
        std::sort(keys.begin(),keys.end());keys.erase(std::unique(keys.begin(),keys.end()),keys.end());
    }
    const auto& active_keys=decode?keys:keys_;
    // A bounded compact atlas avoids replacing a half-MiB LCD layer with a
    // full 2-MiB all-characters/all-palettes allocation on Original 3DS.
    if(active_keys.size()>1024) return {};
    const unsigned atlas_width=256,atlas_height=std::max(8U,unsigned((active_keys.size()+31)/32)*8);
    const bool rekey=decode && keys!=keys_;
    const bool recolour=!source_ || rekey || brightness_!=brightness || subtract_!=subtract
        || source_->cgram!=source->cgram || source_->bg2_character_base!=source->bg2_character_base
        || !same_range(*source_,*source,source->bg2_character_base*2,32768);
    std::vector<std::uint8_t> pixels;
    if(recolour) {
        pixels.assign(std::size_t(atlas_width)*atlas_height*4,0);
        for(unsigned slot=0;slot<active_keys.size();++slot) {
            if(active_keys[slot]>=8192) {
                const unsigned word=source->cgram[active_keys[slot]-8192];
                for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
                    const auto at=(std::size_t(slot/32*8+y)*atlas_width+slot%32*8+x)*4;
                    for(unsigned channel=0;channel<3;++channel) {
                        const unsigned five=unsigned(std::max(0,int((word>>(channel*5))&31)-int(subtract)));
                        pixels[at+channel]=std::uint8_t(((five<<3)|(five>>2))*brightness/15);
                    }
                    pixels[at+3]=255;
                }
                continue;
            }
            const unsigned character=active_keys[slot]/8,bank=active_keys[slot]%8;
            const unsigned base=source->bg2_character_base*2+character*32;
            for(unsigned y=0;y<8;++y) {
                const unsigned a=source->vram[(base+y*2)&65535],b=source->vram[(base+y*2+1)&65535];
                const unsigned c=source->vram[(base+y*2+16)&65535],d=source->vram[(base+y*2+17)&65535];
                for(unsigned x=0;x<8;++x) {
                    const unsigned mask=128>>x,ink=unsigned(bool(a&mask))|(unsigned(bool(b&mask))<<1)
                        |(unsigned(bool(c&mask))<<2)|(unsigned(bool(d&mask))<<3);
                    if(!ink) continue; // Only tile colour zero is transparent; black RGB is opaque.
                    const unsigned word=source->cgram[bank*16+ink];
                    const auto at=(std::size_t(slot/32*8+y)*atlas_width+slot%32*8+x)*4;
                    for(unsigned channel=0;channel<3;++channel) {
                        const unsigned five=unsigned(std::max(0,int((word>>(channel*5))&31)-int(subtract)));
                        pixels[at+channel]=std::uint8_t(((five<<3)|(five>>2))*brightness/15);
                    }
                    pixels[at+3]=255;
                }
            }
        }
    }
    std::vector<PicaVertex> vertices;
    // Atlas growth/reordering changes normalized UVs even if scrolling did not.
    if(decode) {
        vertices.reserve((rects.size()+edge_quads)*6);
        const float left=(float(top_width)-width)*.5F;
        for(const auto& rect:rects) {
            const auto key=std::uint16_t(rect.solid_index?8192+rect.solid_index:rect.character*8+rect.bank);
            const unsigned slot=unsigned(std::lower_bound(active_keys.begin(),active_keys.end(),key)-active_keys.begin());
            const float tx=rect.solid_index?float(slot%32*8)+.5F:rect.constant_x?float(slot%32*8+rect.source_x)+.5F
                :float(slot%32*8+rect.source_x+(rect.reverse_x?1:0));
            const float ty=rect.solid_index?float(slot/32*8)+.5F:float(slot/32*8+rect.source_y+(rect.reverse_y?1:0));
            const float dx=rect.solid_index || rect.constant_x?0:rect.reverse_x?-float(rect.width):float(rect.width);
            const float dy=rect.solid_index?0:rect.reverse_y?-float(rect.height):float(rect.height);
            const auto emit=[&](float y,float height,float first_y,float last_y) {
                for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                    const bool right=corner==1 || corner==2,bottom=corner>=2;
                    vertices.push_back({{left+rect.x+(right?float(rect.width):0),y+(bottom?height:0),0},
                        {1,1,1,1},{(tx+(right?dx:0))/atlas_width,(bottom?last_y:first_y)/atlas_height}});
                }
            };
            emit(8.F+rect.y,float(rect.height),ty,ty+dy);
            if(batch.space==PicaSpace::scenery) {
                // Same source scanline clamping as PicaRaster, not a stretched
                // tile at the LCD's extra eight top/bottom rows.
                const float step=rect.solid_index?0:rect.reverse_y?-1.F:1.F;
                if(rect.y==0) emit(0,8,ty,ty+step);
                if(rect.y+rect.height==224) emit(232,8,ty+dy-step,ty+dy);
            }
        }
    }
    std::optional<PpuBatch> next_batch;
    if(decode) next_batch=batch; // Complete all allocating work before publication.
    if(decode) {rectangles_.swap(rectangles);++work_.decodes;}
    if(decode) {vertices_.swap(vertices);keys_.swap(keys);batch_=std::move(*next_batch);}
    if(recolour) {pixels_.swap(pixels);++work_.colour_updates;}
    source_=std::move(source);width_=width;
    complete_roll_=complete_plan;
    brightness_=brightness;subtract_=subtract;
    image_={pixels_,atlas_width,atlas_height,atlas_width*4,4};
    draw_={0,unsigned(vertices_.size()),0,pica_identity,batch.space,false,false,false};draw_.source_layer=2;
    if(vertices_.empty()) return PicaFrame{plan,{},{},{}};
    return PicaFrame{plan,vertices_,std::span(&draw_,1),std::span(&image_,1)};
}
} // namespace starfox::platform::nintendo_3ds
