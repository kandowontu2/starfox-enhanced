#include "starfox/platform/nintendo_3ds/pica_raster.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"
#include <bit>
#include <cstring>
#include <type_traits>

namespace starfox::platform::nintendo_3ds {
static_assert(std::is_same_v<std::uint8_t,unsigned char>,"RGBA byte views require the standard unsigned-char alias");
namespace {
constexpr unsigned native_height=224;
unsigned darkest(const std::array<std::uint16_t,256>& palette) {
    unsigned selected=0,luma=~0U;
    for(unsigned i=0;i<palette.size();++i) {
        const auto word=palette[i];const auto value=77U*(word&31)+150U*((word>>5)&31)+29U*((word>>10)&31);
        if(value<luma) {selected=i;luma=value;}
    }
    return selected;
}
bool same_vram_range(const simulation::SnesPpuState& a,const simulation::SnesPpuState& b,
    unsigned first,unsigned count) noexcept {
    // Word-addressed maps/characters wrap independently at 64 KiB. Compare
    // actual bytes, not a hash or an assumed non-wrapping allocation.
    first&=65535U;
    const auto head=std::min(count,65536U-first);
    return std::memcmp(a.vram.data()+first,b.vram.data()+first,head)==0
        && std::memcmp(a.vram.data(),b.vram.data(),count-head)==0;
}
bool same_background_vram(const simulation::SnesPpuState& a,const simulation::SnesPpuState& b,
    unsigned characters,unsigned map,unsigned size,unsigned depth) noexcept {
    const unsigned pages=((size&1U)?2U:1U)*((size&2U)?2U:1U);
    // All 1024 characters, including 16x16 sub-character wrap, and every
    // configured map page remain dependencies. Mode-3 BG1 covers all VRAM.
    return same_vram_range(a,b,characters*2U,1024U*depth*8U)
        && same_vram_range(a,b,map*2U,pages*2048U);
}
bool same_source(const simulation::SnesPpuState& a,const simulation::SnesPpuState& b,const PpuPass& pass) {
    // Palette-only fades recolour cached indices. OAM activity must not force
    // another background tile traversal, nor scrolling a sky another OBJ pass.
    const unsigned enabled=pass.layer==PpuLayer::bg1?1U:pass.layer==PpuLayer::bg2?2U:
        pass.layer==PpuLayer::bg3?4U:16U;
    if((a.main_screen&enabled)!=(b.main_screen&enabled)) return false;
    if(!(a.main_screen&enabled)) return true;
    if(pass.transparent_black && a.cgram!=b.cgram) return false;
    if(pass.layer==PpuLayer::objects) {
        if(a.oam!=b.oam || a.object_select!=b.object_select) return false;
        const unsigned base=(a.object_select&7U)*0x4000U;
        const unsigned gap=(((a.object_select>>3U)&3U)+1U)*0x2000U;
        // Shared OBJ sampling adds up to seven rows/columns to tile 255 for
        // a 64x64 sprite. Retain that carry, plus both name banks and wrap.
        return same_vram_range(a,b,base,384U*32U)
            && same_vram_range(a,b,base+gap,384U*32U);
    }
    if(a.background_mode!=b.background_mode || a.mosaic!=b.mosaic) return false;
    switch(pass.layer) {
    case PpuLayer::bg1:
        return (a.main_screen&1)==(b.main_screen&1) && a.bg1_character_base==b.bg1_character_base
            && a.bg1_screen_base==b.bg1_screen_base && a.bg1_screen_size==b.bg1_screen_size
            && a.bg1_tile_size_16==b.bg1_tile_size_16 && a.bg1_scroll_x==b.bg1_scroll_x && a.bg1_scroll_y==b.bg1_scroll_y
            && a.tunnel_scene==b.tunnel_scene
            && same_background_vram(a,b,a.bg1_character_base,a.bg1_screen_base,a.bg1_screen_size,
                a.background_mode==3?8U:4U);
    case PpuLayer::bg2:
        return (a.main_screen&2)==(b.main_screen&2) && a.bg2_character_base==b.bg2_character_base
            && a.bg2_screen_base==b.bg2_screen_base && a.bg2_screen_size==b.bg2_screen_size
            && a.bg2_tile_size_16==b.bg2_tile_size_16 && a.bg2_scroll_x==b.bg2_scroll_x && a.bg2_scroll_y==b.bg2_scroll_y
            && a.bg2_horizontal_offsets_enabled==b.bg2_horizontal_offsets_enabled && a.bg2_horizontal_offsets==b.bg2_horizontal_offsets
            && a.bg2_vertical_offsets_enabled==b.bg2_vertical_offsets_enabled && a.bg2_scanline_scroll_enabled==b.bg2_scanline_scroll_enabled
            && a.bg2_scanline_scroll_y==b.bg2_scanline_scroll_y && a.tunnel_scene==b.tunnel_scene
            && (a.cgram==b.cgram || darkest(a.cgram)==darkest(b.cgram))
            && same_background_vram(a,b,a.bg2_character_base,a.bg2_screen_base,a.bg2_screen_size,4U)
            && (a.background_mode!=2 || !a.bg2_vertical_offsets_enabled
                || same_vram_range(a,b,0x5f40U,64U));
    case PpuLayer::bg3:
        return (a.main_screen&4)==(b.main_screen&4) && a.bg3_character_base==b.bg3_character_base
            && a.bg3_screen_base==b.bg3_screen_base && a.bg3_screen_size==b.bg3_screen_size
            && a.bg3_tile_size_16==b.bg3_tile_size_16 && a.bg3_scroll_x==b.bg3_scroll_x && a.bg3_scroll_y==b.bg3_scroll_y
            && same_background_vram(a,b,a.bg3_character_base,a.bg3_screen_base,a.bg3_screen_size,2U);
    default: return false;
    }
}
std::array<std::uint8_t,3> colour(std::uint16_t word,unsigned brightness,unsigned subtract) {
    std::array<std::uint8_t,3> result{};
    for(unsigned channel=0;channel<3;++channel) {
        const auto five=std::max(0,int((word>>(channel*5))&31)-int(subtract));
        result[channel]=std::uint8_t(((five<<3)|(five>>2))*brightness/15);
    }
    return result;
}
void validate(const simulation::SnesPpuState& ppu,const PpuBatch& batch,const FramePlan& plan,unsigned brightness,unsigned subtract) {
    if(ppu.background_mode<1 || ppu.background_mode>3 || brightness>15 || subtract>31
        || batch.passes.size()>16 || batch.first_row>=batch.last_row || batch.last_row>native_height
        || (batch.water_receiver && (ppu.background_mode!=1 || batch.space!=PicaSpace::scenery
            || batch.passes.empty() || std::any_of(batch.passes.begin(),batch.passes.end(),
                [](const auto& pass){return pass.layer!=PpuLayer::bg2;})))
        || (batch.corridor_open_left && (!batch.corridor_receiver || ppu.background_mode!=1))
        || (batch.corridor_receiver && (batch.water_receiver || ppu.background_mode>2
            || (!ppu.tunnel_scene && !batch.corridor_open_left)
            || batch.space!=PicaSpace::scenery || batch.passes.empty()
            || std::any_of(batch.passes.begin(),batch.passes.end(),[](const auto& pass){return pass.layer!=PpuLayer::bg2;})))
        || (batch.visible_scenery_only && (batch.space!=PicaSpace::scenery || !batch.expand_horizontal
            || batch.water_receiver || batch.corridor_receiver || batch.landscape_receiver))
        || (batch.landscape_receiver && (ppu.background_mode!=2 || ppu.tunnel_scene
            || batch.water_receiver || batch.corridor_receiver
            || batch.space!=PicaSpace::scenery || !batch.expand_horizontal || batch.passes.empty()
            || std::any_of(batch.passes.begin(),batch.passes.end(),[](const auto& pass){return pass.layer!=PpuLayer::bg2;})))
        || (batch.space!=PicaSpace::screen && batch.space!=PicaSpace::scenery)
        || (batch.space==PicaSpace::scenery && !batch.expand_horizontal))
        throw std::invalid_argument("Unsupported/incomplete 3DS PPU painter group");
    if(plan.eye_count!=(plan.stereo?2U:1U)) throw std::invalid_argument("Invalid 3DS PPU eye plan");
    for(unsigned eye=0;eye<plan.eye_count;++eye) {
        static_cast<void>(PicaProjection(plan,eye));
    }
    for(const auto& pass:batch.passes) {
        if((pass.layer!=PpuLayer::bg1 && pass.layer!=PpuLayer::bg2 && pass.layer!=PpuLayer::bg3 && pass.layer!=PpuLayer::objects)
            || pass.priority< -1 || pass.priority>(pass.layer==PpuLayer::objects?3:1)
            || pass.guard_inset>128 || pass.single_occurrence_top_rows>512
            || (pass.single_occurrence_sky_half && (pass.layer!=PpuLayer::bg2 || ppu.background_mode!=2
                || !batch.expand_horizontal || !pass.extend_horizontal || !pass.single_occurrence_sky_half->rows
                || pass.single_occurrence_sky_half->rows>512))
            || (batch.space==PicaSpace::scenery && pass.layer==PpuLayer::objects)
            || (pass.sprites!=render::SpriteSelection::all && pass.sprites!=render::SpriteSelection::world_only
                && pass.sprites!=render::SpriteSelection::configurable_hud_only)
            || (pass.scroll && pass.layer!=PpuLayer::bg2))
            throw std::invalid_argument("Invalid 3DS PPU source pass");
    }
}
}
PicaFrame PicaRaster::prepare(std::shared_ptr<const simulation::SnesPpuState> source,const PpuBatch& batch,
    const FramePlan& plan,unsigned brightness,unsigned subtract,unsigned receiver_guard,bool trim_transparent) {
    if(!source) throw std::invalid_argument("Missing immutable 3DS PPU snapshot");
    validate(*source,batch,plan,brightness,subtract);
    const auto guard=batch.space==PicaSpace::scenery?std::max(pica_scenery_guard(plan),receiver_guard):pica_raster_base_guard;
    if(guard>(pica_raster_max_width-top_width)/2)
        throw std::invalid_argument("3DS source raster exceeds horizontal storage");
    auto width=batch.expand_horizontal?top_width+guard*2:256U;
    bool decode=!indexed_ || batch!=batch_;
    if(!decode && source_!=source)
        for(const auto& pass:batch.passes) if(!same_source(*source_,*source,pass)) {decode=true;break;}
    // Reuse already sufficient coverage during slider-only presentations.
    // A genuinely changed source pass can retire excess decoded storage.
    if(!decode) width=std::max(width,unsigned(indexed_->width()));
    decode=decode || indexed_->width()!=width;
    std::array<unsigned,pica_raster_max_strips+1> boundaries{};unsigned pages=0;
    while(boundaries[pages]<width) {
        if(pages==pica_raster_max_strips) throw std::length_error("3DS source strip count exceeded");
        const auto remaining=width-boundaries[pages];
        // Water receivers may just cross a power-of-two padding boundary.
        // Borrow another native-width strip instead of doubling its allocation;
        // the last of at most four strips can keep a non-power-of-two width.
        const auto span=(batch.water_receiver || batch.corridor_receiver || batch.compact_strips) && remaining<=pica_raster_strip_width && pages+1<pica_raster_max_strips
            ?std::bit_floor(remaining):std::min(pica_raster_strip_width,remaining);
        boundaries[pages+1]=boundaries[pages]+span;++pages;
    }
    const int origin=int((width-256)/2);
    auto next=std::unique_ptr<render::Framebuffer>{};
    if(decode) {
        STARFOX_3DS_FRAME_PHASE(bg_decode);
        next=std::make_unique<render::Framebuffer>(width,native_height);
        next->enable_layer_tags(true);next->begin_write_coverage();
        const render::BackgroundRenderer backgrounds;const render::SpriteRenderer sprites;
        for(const auto& pass:batch.passes) {
            const bool extend=batch.expand_horizontal && pass.extend_horizontal;
            const auto priority=pass.priority<0?render::TilePriorityPass::all:
                pass.priority?render::TilePriorityPass::high:render::TilePriorityPass::low;
            // Internal tags deliberately differ from shared PixelLayer::two_d
            // (1), which SpriteRenderer applies inside its own scoped pass.
            // Do not modify the shared renderer or infer OBJ from palette ink.
            const auto bg_bit=pass.layer==PpuLayer::bg1?1:pass.layer==PpuLayer::bg2?2:4;
            const render::ScopedLayer tag(*next,static_cast<render::PixelLayer>(64|bg_bit));
            switch(pass.layer) {
            case PpuLayer::bg1:
                backgrounds.draw_bg1(*source,*next,priority,origin,extend,
                    pass.guard_inset,pass.transparent_black,pass.mosaic_inset);break;
            case PpuLayer::bg2: {
                const auto scroll=pass.scroll.value_or(std::array{source->bg2_scroll_x,source->bg2_scroll_y});
                render::BackgroundUniqueRegion region{};
                std::span<const render::BackgroundUniqueRegion> unique;
                if(pass.single_occurrence_sky_half) {
                    const int half=int(((source->bg2_screen_size&1)?64U:32U)*(source->bg2_tile_size_16?16U:8U)/2);
                    region={pass.single_occurrence_sky_half->right?half:0,0,
                        pass.single_occurrence_sky_half->right?half*2:half,int(pass.single_occurrence_sky_half->rows),
                        0,255,0,half};
                    unique={&region,1};
                }
                backgrounds.draw_bg2(*source,scroll[0],scroll[1],*next,priority,origin,extend,
                    pass.wrap_horizontal,pass.transparent_black,pass.single_occurrence_top_rows,unique);break;
            }
            case PpuLayer::bg3: backgrounds.draw_bg3(*source,*next,priority,origin,extend);break;
            case PpuLayer::objects:
                sprites.draw_objects(*source,*next,pass.priority<0?std::nullopt:std::optional<std::uint8_t>(pass.priority),
                    origin,extend,false,nullptr,false,nullptr,pass.sprites);break;
            }
        }
        next->end_write_coverage();
    }
    const auto& bitmap=decode?*next:*indexed_;
    const bool recolour=decode || palette_!=source->cgram || brightness_!=brightness || subtract_!=subtract;
    auto pixels=std::vector<std::uint32_t>{};auto layers=std::vector<std::uint8_t>{};bool visible=visible_;
    auto occupied=occupied_;
    if(decode) for(auto& bounds:occupied) bounds={pica_raster_strip_width,screen_height,0,0};
    if(decode) layers.assign(std::size_t(width)*screen_height,0);
    if(recolour) {
        STARFOX_3DS_FRAME_PHASE(bg_colour);
        pixels.assign(std::size_t(width)*screen_height,0);visible=false;
        std::array<std::uint32_t,256> normal{},background{};
        const auto packed=[](std::array<std::uint8_t,3> rgb) {
            // bit_cast preserves RGBA byte order on either endian host; the
            // native ARM store is aligned by vector<uint32_t>'s actual type.
            return std::bit_cast<std::uint32_t>(std::array<std::uint8_t,4>{rgb[0],rgb[1],rgb[2],255});
        };
        for(unsigned ink=0;ink<normal.size();++ink) {
            normal[ink]=packed(colour(source->cgram[ink],brightness,0));
            background[ink]=packed(colour(source->cgram[ink],brightness,subtract));
        }
        const auto convert=[&]<bool Decode>() {
            const auto indices=bitmap.pixels().data();
            const auto coverage=bitmap.write_coverage().data();
            const auto tags=bitmap.layer_tags().data();
            for(unsigned y=0;y<screen_height;++y) {
                const int logical_y=int(y)-8;
                if(batch.space==PicaSpace::screen && (logical_y<0 || logical_y>=int(native_height))) continue;
                const unsigned sy=unsigned(std::clamp(logical_y,0,int(native_height-1)));
                if(sy<batch.first_row || sy>=batch.last_row) continue;
                const auto in=std::size_t(sy)*width,out=std::size_t(y)*width;
                for(unsigned page=0;page<pages;++page) {
                    const auto first=boundaries[page],end=boundaries[page+1];
                    unsigned occupied_first=end,occupied_end=first;
                    for(unsigned x=first;x<end;++x) {
                        unsigned layer;
                        if constexpr(Decode) {
                            if(!coverage[in+x]) continue;
                            const auto tag=tags[in+x];
                            layer=tag==unsigned(render::PixelLayer::two_d)?16U:unsigned(tag&63);
                            if(!pica_source_layer(layer) || !layer) throw std::logic_error("Unclassified 3DS PPU source pixel");
                            layers[out+x]=std::uint8_t(layer);
                            if(occupied_first==end) occupied_first=x;
                            occupied_end=x+1;
                        } else {
                            // A palette-only update retains the successful
                            // decode's already validated coverage/ownership.
                            layer=layers_[out+x];if(!layer) continue;
                        }
                        const auto ink=indices[in+x];
                        pixels[out+x]=(layer==2?background:normal)[ink];visible=true;
                    }
                    if constexpr(Decode) if(occupied_first!=end) {
                        auto& bounds=occupied[page];
                        bounds[0]=std::min(bounds[0],occupied_first-first);bounds[1]=std::min(bounds[1],y);
                        bounds[2]=std::max(bounds[2],occupied_end-first);bounds[3]=std::max(bounds[3],y+1);
                    }
                }
            }
        };
        if(decode) convert.template operator()<true>();
        else convert.template operator()<false>();
    }
    // All allocating work precedes publication. Shader/native upload errors
    // are the presenter's responsibility; no failed source decode is published.
    auto next_batch=batch;
    if(decode) {indexed_=std::move(next);layers_=std::move(layers);occupied_=occupied;++work_.decodes;}
    if(recolour) {rgba_=std::move(pixels);++work_.colour_updates;}
    batch_=std::move(next_batch);source_=std::move(source);palette_=source_->cgram;
    brightness_=brightness;subtract_=subtract;visible_=visible;
    const float left=(float(top_width)-width)*.5F;
    constexpr std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
    unsigned strips=0;
    const auto emit=[&](unsigned start,std::array<unsigned,4> bounds) {
        if(bounds[0]>=bounds[2] || bounds[1]>=bounds[3]) return;
        if(strips==pica_raster_max_strips) throw std::length_error("3DS visible source strip count exceeded");
        const unsigned size=bounds[2]-bounds[0],height=bounds[3]-bounds[1];
        unsigned vertex=strips*6;
        for(unsigned corner:{0U,1U,2U,0U,2U,3U})
            vertices_[vertex++]={{left+start+bounds[0]+uv[corner][0]*size,bounds[1]+uv[corner][1]*height,0},{1,1,1,1},uv[corner]};
        draws_[strips]={strips*6,6,strips,pica_identity,batch.space,false,false,true};
        const auto offset=std::size_t(bounds[1])*width+start+bounds[0];
        // Reading an object's representation through unsigned-byte storage is
        // legal; its backing allocation remains the correctly typed word vector.
        images_[strips]={std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(rgba_.data()),rgba_.size()*4).subspan(offset*4),size,height,width*4,4,false,
            std::span<const std::uint8_t>(layers_).subspan(offset),width};
        ++strips;
    };
    if(batch.visible_scenery_only && width>top_width+pica_raster_base_guard*2) {
        // Infinity projection translates each eye by its exact off-axis
        // offset. The gap between disjoint frusta contains no visible pixel.
        // Keep the entire canonical mono LCD too, including opaque black,
        // for mono presentations and independent source-pixel comparisons.
        std::array<std::array<unsigned,2>,3> intervals{};unsigned count=1;
        const auto interval=[&](double offset) {
            const auto first=std::clamp(std::floor(-double(left)-offset),0.,double(width));
            const auto last=std::clamp(std::ceil(top_width-double(left)-offset),0.,double(width));
            return std::array<unsigned,2>{unsigned(first),unsigned(last)};
        };
        intervals[0]=interval(0);
        for(unsigned eye=0;eye<plan.eye_count;++eye) intervals[count++]=interval(background_offset(plan,eye));
        // At most three intervals; an explicit bounded insertion sort also
        // avoids GCC's generic 16-element insertion-sort bounds warning.
        for(unsigned i=1;i<count;++i) for(unsigned j=i;j>0 && intervals[j]<intervals[j-1];--j)
            std::swap(intervals[j],intervals[j-1]);
        unsigned merged=0;
        for(unsigned i=0;i<count;++i) {
            if(merged && intervals[i][0]<=intervals[merged-1][1])
                intervals[merged-1][1]=std::max(intervals[merged-1][1],intervals[i][1]);
            else intervals[merged++]=intervals[i];
        }
        for(unsigned i=0;i<merged;++i) for(unsigned start=intervals[i][0];start<intervals[i][1];) {
            const unsigned end=std::min(start+pica_raster_strip_width,intervals[i][1]);
            auto bounds=std::array<unsigned,4>{end-start,screen_height,0,0};
            for(unsigned y=0;y<screen_height;++y) for(unsigned x=start;x<end;++x)
                if(layers_[std::size_t(y)*width+x]) {
                    bounds[0]=std::min(bounds[0],x-start);bounds[1]=std::min(bounds[1],y);
                    bounds[2]=std::max(bounds[2],x-start+1);bounds[3]=std::max(bounds[3],y+1);
                }
            emit(start,bounds);start=end;
        }
    } else for(unsigned page=0;page<pages;++page) {
        const auto start=boundaries[page];
        auto bounds=std::array<unsigned,4>{0,0,boundaries[page+1]-start,screen_height};
        // A split screen-space OBJ group can contain only one tiny sprite.
        // Borrow its occupied rectangle too; retaining a whole guarded LCD
        // page per priority needlessly consumes the water compositor budget.
        // Geometry retains the exact source origin, including opaque black.
        if(trim_transparent) bounds=occupied_[page];
        emit(start,bounds);
    }
    // Isolated artwork retains its actual one-hot ownership when its A8
    // descriptor is omitted. In water scenes the distant sky is BG3, not BG2.
    if(!batch.passes.empty() && batch.passes.front().layer!=PpuLayer::objects
        && std::all_of(batch.passes.begin(),batch.passes.end(),[&](const auto& pass) {
            return pass.layer==batch.passes.front().layer;
        })) {
        const auto layer=batch.passes.front().layer;
        for(unsigned i=0;i<strips;++i) draws_[i].source_layer=layer==PpuLayer::bg1?1:layer==PpuLayer::bg2?2:4;
    }
    return {plan,visible_?std::span<const PicaVertex>(vertices_).first(strips*6):std::span<const PicaVertex>{},
        visible_?std::span<const PicaDraw>(draws_).first(strips):std::span<const PicaDraw>{},
        visible_?std::span<const PicaImage>(images_).first(strips):std::span<const PicaImage>{}};
}
} // namespace starfox::platform::nintendo_3ds
