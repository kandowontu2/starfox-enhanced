#include "starfox/platform/nintendo_3ds/pica_bg2_map.hpp"
#include "starfox/platform/nintendo_3ds/ppu_tile_dependencies.hpp"
#include <bit>

namespace starfox::platform::nintendo_3ds {
namespace {
bool same_map(const simulation::SnesPpuState& a,const simulation::SnesPpuState& b) {
    return a.bg2_screen_base==b.bg2_screen_base && a.bg2_screen_size==b.bg2_screen_size
        && a.bg2_character_base==b.bg2_character_base && a.bg2_tile_size_16==b.bg2_tile_size_16
        && same_background_tiles(a,b,a.bg2_character_base,a.bg2_screen_base,a.bg2_screen_size,4,a.bg2_tile_size_16);
}
bool same_geometry(const simulation::SnesPpuState& a,const simulation::SnesPpuState& b) {
    return a.background_mode==b.background_mode && (a.main_screen&2)==(b.main_screen&2)
        && a.mosaic==b.mosaic && a.tunnel_scene==b.tunnel_scene
        && a.bg2_scroll_x==b.bg2_scroll_x && a.bg2_scroll_y==b.bg2_scroll_y
        && a.bg2_horizontal_offsets_enabled==b.bg2_horizontal_offsets_enabled
        && (!a.bg2_horizontal_offsets_enabled || a.bg2_horizontal_offsets==b.bg2_horizontal_offsets)
        && a.bg2_scanline_scroll_enabled==b.bg2_scanline_scroll_enabled
        && (!a.bg2_scanline_scroll_enabled || a.bg2_scanline_scroll_y==b.bg2_scanline_scroll_y)
        && a.bg2_vertical_offsets_enabled==b.bg2_vertical_offsets_enabled
        && (!a.bg2_vertical_offsets_enabled || same_ppu_vram_range(a,b,0x5f40,64))
        && same_map(a,b);
}
std::uint16_t word(const simulation::SnesPpuState& ppu,unsigned address) {
    const unsigned at=(address&32767)*2;
    return std::uint16_t(ppu.vram[at]|(unsigned(ppu.vram[at+1])<<8));
}
unsigned darkest(const simulation::SnesPpuState& ppu) {
    unsigned selected=0,luma=~0U;
    for(unsigned i=0;i<256;++i) {
        const auto ink=ppu.cgram[i];const unsigned value=77*(ink&31)+150*((ink>>5)&31)+29*((ink>>10)&31);
        if(value<luma) {selected=i;luma=value;}
    }
    return selected;
}
// Unique panoramas retain the cartridge's wrapped centre, but their added
// margins expose only one authored atlas occurrence. Split in screen-row
// bands before map coalescing, including independently scrolling HDMA rows.
void unique_top(std::vector<Bg2TileRect>& rectangles,std::vector<Bg2TileRect>& guards,
    const simulation::SnesPpuState& ppu,unsigned rows,int scroll,unsigned width,unsigned origin,unsigned map_width) {
    std::vector<Bg2TileRect> clipped;
    const auto row_x=[&](unsigned y){return ppu.bg2_horizontal_offsets_enabled?int(ppu.bg2_horizontal_offsets[y]):scroll;};
    for(unsigned y=0;y<std::min(rows,224U);) {
        unsigned end=y+1;while(end<std::min(rows,224U) && row_x(end)==row_x(y)) ++end;
        const int map_first=int(origin)-row_x(y),map_last=map_first+int(map_width);
        std::array<int,6> cuts{0,int(width),int(origin),int(origin+256),map_first,map_last};
        for(auto& cut:cuts) cut=std::clamp(cut,0,int(width));
        std::sort(cuts.begin(),cuts.end());
        for(unsigned i=1;i<cuts.size();++i) {
            const int first=cuts[i-1],last=cuts[i];if(first==last) continue;
            if((first>=int(origin) && first<int(origin+256)) || (first>=map_first && first<map_last)) continue;
            Bg2TileRect rect;rect.x=unsigned(first);rect.y=y;rect.width=unsigned(last-first);rect.height=end-y;guards.push_back(rect);
        }
        y=end;
    }
    for(const auto& rect:rectangles) for(unsigned y=rect.y;y<rect.y+rect.height;) {
        unsigned end=rect.y+rect.height;
        if(y<rows) {end=std::min(end,rows);for(unsigned at=y+1;at<end;++at) if(row_x(at)!=row_x(y)) {end=at;break;}}
        const int map_first=int(origin)-row_x(y),map_last=map_first+int(map_width);
        std::array<int,6> cuts{int(rect.x),int(rect.x+rect.width),int(origin),int(origin+256),map_first,map_last};
        for(auto& cut:cuts) cut=std::clamp(cut,int(rect.x),int(rect.x+rect.width));
        std::sort(cuts.begin(),cuts.end());
        for(unsigned i=1;i<cuts.size();++i) {
            const int first=cuts[i-1],last=cuts[i];if(first==last) continue;
            if(y<rows && !(first>=int(origin) && first<int(origin+256)) && !(first>=map_first && first<map_last)) continue;
            auto part=rect;part.x=unsigned(first);part.y=y;part.width=unsigned(last-first);part.height=end-y;
            if(!part.solid_index) {part.map_x+=unsigned(first)-rect.x;part.map_y+=y-rect.y;}
            clipped.push_back(part);
        }
        y=end;
    }
    rectangles.swap(clipped);
}
}
std::optional<PicaFrame> PicaBg2Map::prepare(std::shared_ptr<const simulation::SnesPpuState> source,
    const PpuBatch& batch,const FramePlan& plan,unsigned brightness,unsigned subtract,
    unsigned vertex_budget,unsigned texture_budget,unsigned guard) {
    if(!source || brightness>15 || subtract>31) throw std::invalid_argument("Invalid decoded BG2 map");
    const auto& ppu=*source;
    if(ppu.background_mode!=2 || ppu.tunnel_scene || (ppu.mosaic&2) || batch.passes.size()!=1
        || batch.space!=PicaSpace::scenery || !batch.expand_horizontal || batch.first_row || batch.last_row!=224
        || batch.water_receiver || batch.corridor_receiver) return {};
    const auto& pass=batch.passes.front();
    if(pass.layer!=PpuLayer::bg2 || !pass.extend_horizontal || !pass.wrap_horizontal
        || pass.transparent_black || pass.guard_inset || pass.mosaic_inset
        || pass.single_occurrence_top_rows>512 || pass.single_occurrence_sky_half || pass.priority< -1 || pass.priority>1) return {};
    if(plan.eye_count!=(plan.stereo?2U:1U)) throw std::invalid_argument("Invalid decoded BG2 eye plan");
    for(unsigned eye=0;eye<plan.eye_count;++eye) static_cast<void>(PicaProjection(plan,eye));
    if(guard>(pica_raster_max_width-top_width)/2) return {};
    const unsigned edge=ppu.bg2_tile_size_16?16:8;
    const unsigned width=((ppu.bg2_screen_size&1)?64:32)*edge;
    const unsigned height=((ppu.bg2_screen_size&2)?64:32)*edge;
    if(std::uint64_t(width)*height>256U*1024U || std::uint64_t(width)*height*4>texture_budget) return {};
    const bool geometry_same=source_ && guard_==guard && batch_==batch && same_geometry(*source_,ppu);
    if(!geometry_same && rejected_ && rejected_guard_==guard && rejected_batch_==batch && same_geometry(*rejected_,ppu)) return {};
    std::vector<Bg2TileRect> rectangles,guards;
    if(!geometry_same) {
        const auto scroll=pass.scroll.value_or(std::array{ppu.bg2_scroll_x,ppu.bg2_scroll_y});
        if(!plan_bg2_tiles(ppu,scroll[0],scroll[1],400+2*guard,int(72+guard),pass.priority,rectangles,16384,batch.continue_ground)
            && !plan_rolled_bg2_tiles(ppu,scroll[0],scroll[1],400+2*guard,int(72+guard),pass.priority,rectangles,16384,batch.continue_ground)) return {};
        const auto old_size=rectangles.size();
        if(pass.single_occurrence_top_rows) unique_top(rectangles,guards,ppu,pass.single_occurrence_top_rows,scroll[0],400+2*guard,72+guard,width);
        rectangles=coalesce_bg2_map_rects(rectangles);
        // Keep small/poorly coalescing scenes on the compact character atlas.
        // A decline never changes the previous published image or geometry.
        if(!pass.single_occurrence_top_rows && (old_size<256 || rectangles.size()*2>=old_size)) {
            auto rejected_batch=batch;rejected_=source;rejected_batch_=std::move(rejected_batch);rejected_guard_=guard;return {};
        }
        unsigned quads=unsigned(rectangles.size()+guards.size());
        for(const auto* set:{&rectangles,&guards}) for(const auto& r:*set) {quads+=r.y==0;quads+=r.y+r.height==224;}
        if(std::uint64_t(quads)*6>vertex_budget) return {};
    } else if(vertices_.size()>vertex_budget) return {};
    const bool map_same=source_ && width_==width && height_==height && same_map(*source_,ppu);
    std::vector<std::uint8_t> indices;
    if(!map_same) {
        indices.resize(std::size_t(width)*height);
        const unsigned columns=width/edge,pages_wide=columns/32;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const unsigned tx=x/edge,ty=y/edge;
            const auto tile=word(ppu,ppu.bg2_screen_base+((tx/32)+(ty/32)*pages_wide)*1024+(ty&31)*32+(tx&31));
            unsigned px=x&(edge-1),py=y&(edge-1),character=tile&1023;
            if(tile&0x4000) px=edge-1-px;
            if(tile&0x8000) py=edge-1-py;
            if(edge==16) character=(character+(px>=8?1:0)+(py>=8?16:0))&1023;
            const unsigned base=ppu.bg2_character_base*2+character*32+(py&7)*2,mask=128>>(px&7);
            unsigned ink=0;
            for(unsigned plane=0;plane<4;++plane) if(ppu.vram[(base+(plane&1)+(plane>=2?16:0))&65535]&mask) ink|=1<<plane;
            indices[y*width+x]=ink?std::uint8_t(((tile>>10)&7)*16+ink):0;
        }
    }
    const auto& map=map_same?indices_:indices;
    const bool recolour=!map_same || brightness_!=brightness || subtract_!=subtract || source_->cgram!=ppu.cgram;
    std::vector<std::uint32_t> pixels;
    if(recolour) {
        std::array<std::uint32_t,256> palette{};
        for(unsigned ink=1;ink<256;++ink) {
            std::array<std::uint8_t,4> rgba{0,0,0,255};
            for(unsigned c=0;c<3;++c) {
                const int value=std::max(0,int((ppu.cgram[ink]>>(c*5))&31)-int(subtract));
                rgba[c]=std::uint8_t(((value<<3)|(value>>2))*brightness/15);
            }
            palette[ink]=std::bit_cast<std::uint32_t>(rgba);
        }
        pixels.resize(map.size());
        for(unsigned i=0;i<map.size();++i) pixels[i]=palette[map[i]];
    }
    std::vector<PicaVertex> vertices;unsigned textured_vertices=textured_vertices_;
    if(!geometry_same) {
        // Solid/carry ink necessarily occurs in this same source map. Sample
        // one interior texel for it; no extra palette texture or new colors.
        std::array<unsigned,256> solid;solid.fill(unsigned(map.size()));
        for(unsigned i=0;i<map.size();++i) if(map[i] && solid[map[i]]==map.size()) solid[map[i]]=i;
        for(const auto& r:rectangles) {
            if(r.solid_index && solid[r.solid_index]==map.size()) throw std::logic_error("Carry ink absent from source map");
            const float u=r.solid_index?float(solid[r.solid_index]%width)+.5F:float(r.map_x);
            const float v=r.solid_index?float(solid[r.solid_index]/width)+.5F:float(r.map_y);
            const float dx=r.solid_index?0:float(r.width),dy=r.solid_index?0:float(r.height);
            const auto emit=[&](float y,float h,float v0,float v1) {
                for(unsigned c:{0U,1U,2U,0U,2U,3U}) {
                    const bool right=c==1 || c==2,bottom=c>=2;
                    vertices.push_back({{float(r.x)-float(guard)+(right?float(r.width):0),y+(bottom?h:0),0},
                        {1,1,1,1},{(u+(right?dx:0))/width,(bottom?v1:v0)/height}});
                }
            };
            emit(float(r.y)+8,float(r.height),v,v+dy);
            const float step=r.solid_index?0:1;
            if(r.y==0) emit(0,8,v,v+step);
            if(r.y+r.height==224) emit(232,8,v+dy-step,v+dy);
        }
        textured_vertices=unsigned(vertices.size());
        for(const auto& r:guards) {
            const auto emit=[&](float y,float height) {
                for(unsigned c:{0U,1U,2U,0U,2U,3U}) vertices.push_back({
                    {float(r.x)-float(guard)+(c==1 || c==2?float(r.width):0),y+(c>=2?height:0),0},{1,1,1,1},{0,0}});
            };
            emit(float(r.y)+8,float(r.height));if(!r.y) emit(0,8);if(r.y+r.height==224) emit(232,8);
        }
    }
    std::optional<PpuBatch> next_batch;
    if(!geometry_same) next_batch=batch; // Allocate only for a changed plan.
    if(!map_same) indices_.swap(indices);
    if(recolour) {pixels_.swap(pixels);++work_.colour_updates;}
    if(!geometry_same) {vertices_.swap(vertices);textured_vertices_=textured_vertices;guard_vertices_=unsigned(vertices_.size())-textured_vertices_;++work_.decodes;}
    if(guard_vertices_) {
        std::array<float,4> colour{0,0,0,1};const auto ink=ppu.cgram[darkest(ppu)];
        for(unsigned c=0;c<3;++c) {
            const int value=std::max(0,int((ink>>(c*5))&31)-int(subtract));
            colour[c]=float(((value<<3)|(value>>2))*brightness/15)/255.F;
        }
        for(unsigned i=textured_vertices_;i<vertices_.size();++i) vertices_[i].colour=colour;
    }
    source_=std::move(source);if(next_batch) batch_=std::move(*next_batch);guard_=guard;width_=width;height_=height;
    brightness_=brightness;subtract_=subtract;
    image_={{reinterpret_cast<const std::uint8_t*>(pixels_.data()),pixels_.size()*4},width,height,width*4,4};
    unsigned draw_count=0;
    if(textured_vertices_) draws_[draw_count++]={0,textured_vertices_,0,pica_identity,PicaSpace::scenery,false,false,false};
    if(guard_vertices_) draws_[draw_count++]={textured_vertices_,guard_vertices_,pica_no_texture,pica_identity,PicaSpace::scenery,false,false,false};
    for(unsigned i=0;i<draw_count;++i) draws_[i].source_layer=2;
    content_.publish(std::span(&image_,1),recolour);
    return PicaFrame{plan,vertices_,std::span(draws_).first(draw_count),std::span(&image_,1)};
}
}
