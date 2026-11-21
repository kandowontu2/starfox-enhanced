#include "starfox/vr/background_tiles.hpp"
#include "starfox/compat/bit_cast.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/packed_vram.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/background_renderer.hpp"
#include <algorithm>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <numbers>
#include <map>
#include <optional>
#include <span>
#include <mutex>
#include <bit>

namespace starfox::vr {
Matrix4 photographic_scroll_correction(float dx,float dy,double alpha,float horizontal_scale,float vertical_scale) {
    if(!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(alpha)
        || !std::isfinite(horizontal_scale) || horizontal_scale<=0
        || !std::isfinite(vertical_scale) || vertical_scale<=0)
        throw std::invalid_argument("Invalid photographic scroll correction");
    const float weight=float(1-std::clamp(alpha,0.,1.));
    const float yaw=std::remainder(dx,512.F)*weight/(512*horizontal_scale);
    const float pitch=std::atan(-std::remainder(dy,512.F)*weight/(224*vertical_scale));
    const float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    return {cy,0,sy,0, -sy*sp,cp,cy*sp,0, -sy*cp,-sp,cy*cp,0, 0,0,0,1};
}
Matrix4 photographic_body_motion(std::array<float,2> center) {
    for(float value:center) if(!std::isfinite(value) || std::abs(value)>4096)
        throw std::invalid_argument("Invalid photographic body motion");
    const float yaw=std::atan((center[0]-128)/512),pitch=std::atan((112-center[1])/512);
    const float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    return {cy,0,sy,0, -sy*sp,cp,cy*sp,0, -sy*cp,-sp,cy*cp,0, 0,0,0,1};
}
DrawPacket photographic_body_packet(std::shared_ptr<const std::vector<uint32_t>> texture,
    const PhotographicBody& options,bool srgb) {
    if(!texture || texture->size()<7 || !backdrop_texture_valid(*texture,(*texture)[5],(*texture)[6])
        || (*texture)[2]!=0 || options.bright>32767 || options.dark>32767)
        throw std::invalid_argument("Invalid photographic body texture/palette");
    for(float value:options.center) if(!std::isfinite(value) || std::abs(value)>4096)
        throw std::invalid_argument("Invalid photographic body center");
    for(float value:options.diameter) if(!std::isfinite(value) || value<=0 || value>1024)
        throw std::invalid_argument("Invalid photographic body size");
    for(float value:options.response) if(!std::isfinite(value) || value<0 || value>4096)
        throw std::invalid_argument("Invalid photographic body response");
    for(float value:options.palette_shift) if(!std::isfinite(value) || std::abs(value)>4096)
        throw std::invalid_argument("Invalid photographic body palette shift");
    if((options.palette[0]!=0 && (options.palette[0]<4 || options.palette[0]>8)) || (options.palette[0] && options.two_tone)
        || std::any_of(options.palette.begin()+1,options.palette.end(),[](auto c){return c>32767;}))
        throw std::invalid_argument("Invalid photographic body shade ramp");
    DrawPacket packet;packet.geometry.shared_texels=std::move(texture);
    auto vertices=std::make_shared<std::vector<SceneVertex>>();vertices->reserve(6);
    const float yaw=std::atan((options.center[0]-128)/512),pitch=std::atan((112-options.center[1])/512);
    const float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    for(const auto& corner:std::array<std::array<float,2>,6>{{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}}}) {
        const float u=(corner[0]-.5F)*options.diameter[0]*.125F;
        const float v=(.5F-corner[1])*options.diameter[1]*.125F;
        SceneVertex vertex{};
        vertex.position[0]=64*sy*cp+u*cy-v*sy*sp;
        vertex.position[1]=64*sp+v*cp;
        vertex.position[2]=-64*cy*cp+u*sy+v*cy*sp;
        std::copy(corner.begin(),corner.end(),vertex.uv);
        std::copy(options.response.begin(),options.response.end(),vertex.color);
        std::copy(options.palette_shift.begin(),options.palette_shift.end(),vertex.odd_color);
        vertex.texture[1]=(*packet.geometry.shared_texels)[5]-1;
        vertex.texture[2]=(*packet.geometry.shared_texels)[6]-1;
        vertex.texture[3]=backdrop_texture_flag|(srgb?2U:0U);
        if(options.two_tone) {
            vertex.visibility_a[0]=3;vertex.visibility_a[1]=options.bright;vertex.visibility_a[2]=options.dark;
        }
        if(options.palette[0]) {
            float* ramp[]{vertex.visibility_a,vertex.visibility_b,vertex.visibility_c,vertex.group_a,vertex.group_b,vertex.group_c};
            for(unsigned i=0;i<16;++i) ramp[i/3][i%3]=options.palette[i];
        }
        vertices->push_back(vertex);
    }
    packet.geometry.shared_vertices=std::move(vertices);return packet;
}
DrawPacket photographic_landscape_packet(std::shared_ptr<const std::vector<uint32_t>> texture,
    const PhotographicLandscape& options,bool srgb) {
    if(!texture || texture->size()<7 || !backdrop_texture_valid(*texture,(*texture)[5],(*texture)[6])
        || !std::isfinite(options.horizon_v) || options.horizon_v<(options.full_sphere?-4:0)
        || options.horizon_v>(options.full_sphere?4:1)
        || !std::isfinite(options.vertical_scale) || options.vertical_scale<=0 || options.vertical_scale>16
        || !std::isfinite(options.horizontal_offset) || std::abs(options.horizontal_offset)>16
        || !std::isfinite(options.horizontal_scale) || options.horizontal_scale<=0 || options.horizontal_scale>4
        || options.repeats<1 || options.repeats>16 || (options.latitude_uv && !options.full_sphere)
        || (options.orbital_surface && (!options.full_sphere || options.latitude_uv)))
        throw std::invalid_argument("Invalid photographic landscape projection");
    for(float value:options.response) if(!std::isfinite(value) || value<0 || value>4096)
        throw std::invalid_argument("Invalid photographic landscape response");
    for(float value:options.palette_shift) if(!std::isfinite(value) || std::abs(value)>4096)
        throw std::invalid_argument("Invalid photographic landscape palette shift");
    if(options.cloud_palette[0]>2 || std::any_of(options.cloud_palette.begin()+1,options.cloud_palette.end(),
        [](auto colour){return colour>32767;})) throw std::invalid_argument("Invalid photographic cloud palette");
    DrawPacket packet;packet.geometry.shared_texels=std::move(texture);
    constexpr unsigned columns=64;
    const unsigned rows=options.full_sphere?32:16;
    constexpr float pi=std::numbers::pi_v<float>;
    auto vertices=std::make_shared<std::vector<SceneVertex>>();vertices->reserve(columns*rows*6);
    for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<columns;++x) {
        constexpr unsigned corners[6][2]{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
        for(const auto& corner:corners) {
            const float longitude=(float(x+corner[0])/columns*2-1)*pi;
            const float latitude=(1.F-float(y+corner[1])/rows*(options.full_sphere?2.F:1.F))*pi*.5F;
            const float radial=64*std::max(0.F,std::cos(latitude));
            SceneVertex vertex{};
            vertex.position[0]=radial*std::sin(longitude);vertex.position[1]=64*std::sin(latitude);
            vertex.position[2]=-radial*std::cos(longitude);
            const float rear=longitude/pi;
            // Keep the forward angular scale unchanged. Spread the small
            // periodic-closure correction toward the rear of the panorama.
            vertex.uv[0]=options.horizontal_offset+longitude*options.horizontal_scale
                +(.5F*options.repeats-pi*options.horizontal_scale)*rear*rear*rear;
            vertex.uv[1]=options.latitude_uv?.5F-latitude/pi
                :std::clamp(options.horizon_v-vertex.position[1]/std::max(radial,.0001F)*options.vertical_scale,0.F,1.F);
            vertex.texture[1]=(*packet.geometry.shared_texels)[5]-1;
            vertex.texture[2]=(*packet.geometry.shared_texels)[6]-1;
            vertex.texture[3]=backdrop_texture_flag|(srgb?2U:0U);
            std::copy(options.response.begin(),options.response.end(),vertex.color);
            std::copy(options.palette_shift.begin(),options.palette_shift.end(),vertex.odd_color);
            vertex.odd_color[3]=options.orbital_surface?4:options.full_sphere?3:1;
            float* ramp[]{vertex.visibility_a,vertex.visibility_b,vertex.visibility_c,
                vertex.group_a,vertex.group_b,vertex.group_c};
            for(unsigned i=0;i<16;++i) ramp[i/3][i%3]=float(options.cloud_palette[i]);
            vertices->push_back(vertex);
        }
    }
    packet.geometry.shared_vertices=std::move(vertices);
    return packet;
}
namespace {
constexpr uint32_t corridor_marker=2048U;
constexpr float corridor_ceiling_height=60.F,corridor_floor_height=60.F;
constexpr float corridor_ceiling_vp=112.5F,corridor_floor_vp=111.5F;
// Page-24 bands in world Z mod 256, as the cartridge shows them against the
// objects: its CHEQUERED tables are indexed by OLDVIEWPOSZ, one tick (63
// units) behind the camera the objects are drawn with, so on screen every
// band sits 63 units further out than the table's own phase (2 / 184).
constexpr uint32_t corridor_ring_start=2+63,corridor_ring_length=64,corridor_floor_start=184+63,corridor_floor_length=128;
constexpr float corridor_far=8388608.F; // Far enough that the planes meet the horizon.
constexpr std::size_t corridor_tables=272,corridor_zones=272+2*448,corridor_zone_count=3,corridor_texels=272+2*448+4*8;
}
DrawPacket tunnel_corridor_packet(const simulation::SnesPpuState& source,unsigned brightness,bool srgb,float half_width) {
    if(brightness>15) throw std::invalid_argument("Invalid corridor brightness");
    if(!(half_width>0)) throw std::invalid_argument("Invalid corridor half-width");
    DrawPacket packet;packet.model=tunnel_corridor_model(0,0,-60,0);
    if(!source.tunnel_scene || !source.bg2_scanline_scroll_enabled || !(source.main_screen&2)) return packet;
    // Art probe: BG2 at native screen (x,row) on page 24 or 280. Each row,
    // from the centre outward: a graded ceiling/floor colour, then one or two
    // strip colours and the side-wall colour. The art draws those boundaries
    // as 8-pixel tile staircases approximating lines through the vanishing
    // point; fit each as a straight line (|column offset| / |row offset|).
    // Per-row horizontal offsets follow the camera's X with perspective
    // (128 = centred); probe the centred art and let the model carry X.
    auto ppu=source;
    ppu.bg2_horizontal_offsets.fill(128);
    const auto art=[&](unsigned page,int x,int row) {
        ppu.bg2_scanline_scroll_y.fill(int16_t(page?280:24));
        return render::tunnel_border_index(ppu,unsigned(std::clamp(row,0,223)),unsigned(std::clamp(x,0,255)));
    };
    // The corridor mirrors its zones about the centre column. Asymmetric
    // tunnel art (a decorated wall on one side only: EX 4-4/4-5, Original
    // 2-6) is not a corridor; keep the native layer for it.
    {
        unsigned samples=0,different=0;
        for(unsigned page=0;page<2;++page) for(int row=0;row<224;row+=4) for(int k=4;k<124;k+=8) {
            ++samples;
            if(art(page,127-k,row)!=art(page,128+k,row)) ++different;
        }
        if(different*8>samples) return packet;
    }
    std::vector<uint32_t> data(corridor_texels);
    unsigned fitted=0;
    for(unsigned page=0;page<2;++page) for(const bool ceiling:{true,false}) {
        const float vp=ceiling?corridor_ceiling_vp:corridor_floor_vp;
        struct Transition {uint32_t colour;int column;};
        std::vector<std::vector<Transition>> rows(224);
        std::size_t canonical=0;
        for(int row=ceiling?0:112;row<(ceiling?112:224);++row) {
            data[corridor_tables+page*448+unsigned(row)*2]=art(page,128,row);
            auto previous=art(page,128,row);
            for(int column=127;column>=0;--column) {
                const auto colour=art(page,column,row);
                if(colour!=previous) rows[size_t(row)].push_back({colour,column+1});
                previous=colour;
            }
            if(rows[size_t(row)].size()>rows[canonical].size()) canonical=size_t(row);
        }
        const auto& zones=rows[canonical];
        const auto base=corridor_zones+(page*2+(ceiling?0U:1U))*8;
        for(std::size_t zone=0;zone<std::min<std::size_t>(zones.size(),corridor_zone_count);++zone) {
            double sum_xy=0,sum_xx=0;unsigned samples=0;
            for(int row=ceiling?0:112;row<(ceiling?112:224);++row)
                for(const auto& transition:rows[size_t(row)]) if(transition.colour==zones[zone].colour) {
                    const double x=std::abs(double(row)+.5-vp),y=128.-transition.column;
                    if(x>4) {sum_xy+=x*y;sum_xx+=x*x;++samples;}
                    break;
                }
            if(samples<3) break;
            data[base+zone*2]=starfox::bit_cast<uint32_t>(float(sum_xy/sum_xx));
            data[base+zone*2+1]=zones[zone].colour;
            data[base+6+0]=uint32_t(zone+1);
        }
        fitted+=data[base+6]!=0;
    }
    if(!fitted) return packet;
    // The art grades the ceiling's and floor's centre by screen row: a
    // stepped haze fixed to the screen that does not move with the corridor,
    // so in 3D it reads as static stripes over the moving bands. Keep each
    // page's dominant centre colour on every row instead.
    for(unsigned page=0;page<2;++page) for(const bool ceiling:{true,false}) {
        std::array<unsigned,256> counts{};
        const int first=ceiling?0:112,last=ceiling?112:224;
        for(int row=first;row<last;++row) ++counts[data[corridor_tables+page*448+unsigned(row)*2]&255U];
        const auto dominant=uint32_t(std::max_element(counts.begin(),counts.end())-counts.begin());
        for(int row=first;row<last;++row) data[corridor_tables+page*448+unsigned(row)*2]=dominant;
    }
    {
        // The art's side-wall line: the outermost boundary of the table with
        // the most zones (Armada: 1.470, i.e. 88 units out at height 60).
        // Scale every boundary so it lands on the box's real walls; the
        // zones past it (page 24's light rib chunk, the wall colours) are
        // the art's painting of those walls and go on the wall planes.
        float line=0;unsigned most=0;
        for(unsigned table=0;table<4;++table) {
            const auto base=corridor_zones+table*8;
            if(data[base+6]>most) {most=data[base+6];line=starfox::bit_cast<float>(data[base+(most-1)*2]);}
        }
        if(line>0) {
            const float factor=half_width/corridor_ceiling_height/line;
            for(unsigned table=0;table<4;++table) {
                const auto base=corridor_zones+table*8;
                for(unsigned zone=0;zone<data[base+6];++zone)
                    data[base+zone*2]=starfox::bit_cast<uint32_t>(starfox::bit_cast<float>(data[base+zone*2])*factor);
            }
        }
    }
    data[8]=starfox::bit_cast<uint32_t>(half_width);
    data[1]=starfox::bit_cast<uint32_t>(corridor_ceiling_height);
    data[2]=starfox::bit_cast<uint32_t>(corridor_floor_height);
    data[3]=corridor_ring_start;data[4]=corridor_ring_length;
    data[5]=corridor_floor_start;data[6]=corridor_floor_length;
    data[7]=corridor_marker;
    data[10]=starfox::bit_cast<uint32_t>(corridor_ceiling_vp);
    data[11]=starfox::bit_cast<uint32_t>(corridor_floor_vp);
    data[13]=15-brightness;
    const auto palette=render::decode_bgr555_palette(source.cgram);
    for(unsigned i=0;i<256;++i) data[16+i]=uint32_t(palette[i].r)
        |(uint32_t(palette[i].g)<<8)|(uint32_t(palette[i].b)<<16)|0xff000000U;
    packet.geometry.texels=std::move(data);
    // A closed box in model space (origin on the corridor's centre line, +Y
    // up, forward -Z), from one band period behind the eye to far away:
    // ceiling and floor (uv.x = 0, 1) as wide as the art's strips, and the
    // side walls (uv.x = 2) the art paints beyond its wall lines.
    const float w=half_width,top=corridor_ceiling_height,bottom=-corridor_floor_height,near=256.F,far=-corridor_far;
    const auto quad=[&](std::array<std::array<float,3>,4> corners,float face) {
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex vertex{};
            for(unsigned axis=0;axis<3;++axis) vertex.position[axis]=corners[corner][axis];
            vertex.uv[0]=face;vertex.texture[3]=8|(srgb?2:0);
            packet.geometry.vertices.push_back(vertex);
        }
    };
    quad({{{-w,top,near},{w,top,near},{w,top,far},{-w,top,far}}},0);
    quad({{{-w,bottom,near},{w,bottom,near},{w,bottom,far},{-w,bottom,far}}},1);
    quad({{{-w,bottom,near},{-w,top,near},{-w,top,far},{-w,bottom,far}}},2);
    quad({{{w,bottom,near},{w,top,near},{w,top,far},{w,bottom,far}}},2);
    return packet;
}
std::array<unsigned,2> bg2_art_size(const simulation::SnesPpuState& ppu) noexcept {
    const unsigned edge=ppu.bg2_tile_size_16?16U:8U;
    return {((ppu.bg2_screen_size&1U)?64U:32U)*edge,((ppu.bg2_screen_size&2U)?64U:32U)*edge};
}
unsigned bg2_art_index(const simulation::SnesPpuState& ppu,int x,int y) noexcept {
    const unsigned edge=ppu.bg2_tile_size_16?16U:8U;
    const auto [width,height]=bg2_art_size(ppu);
    const auto byte=[&](unsigned address) {return unsigned(ppu.vram[address&0xffffU]);};
    const unsigned px=unsigned(x)&(width-1),py=unsigned(y)&(height-1);
    const unsigned cx=px/edge,cy=py/edge;
    const unsigned page=(cx>=32?1024U:0U)+(cy>=32?((ppu.bg2_screen_size&1U)?2048U:1024U):0U);
    const unsigned address=(ppu.bg2_screen_base+page+(cy&31U)*32U+(cx&31U))*2U;
    const unsigned tile=byte(address)|(byte(address+1)<<8);
    unsigned tx=px&(edge-1),ty=py&(edge-1);
    if(tile&16384U) tx=edge-1-tx;
    if(tile&32768U) ty=edge-1-ty;
    const unsigned number=((tile&1023U)+(tx>>3)+(ty>>3)*16U)&1023U;
    const unsigned base=ppu.bg2_character_base*2U+number*32U+(ty&7U)*2U;
    const unsigned mask=128U>>(tx&7U);
    unsigned index=0;
    for(unsigned pair=0;pair<2;++pair) {
        if(byte(base+pair*16)&mask) index|=1U<<(pair*2);
        if(byte(base+pair*16+1)&mask) index|=2U<<(pair*2);
    }
    return index?((tile>>10)&7U)*16U+index:0U;
}
std::vector<uint32_t> bg2_art_image(const simulation::SnesPpuState& ppu,int x,int y,unsigned width,unsigned height) {
    const auto palette=render::decode_bgr555_palette(ppu.cgram);
    std::vector<uint32_t> image(std::size_t(width)*height);
    for(unsigned row=0;row<height;++row) for(unsigned column=0;column<width;++column) {
        const unsigned index=bg2_art_index(ppu,x+int(column),y+int(row));
        const auto c=palette[index];
        image[std::size_t(row)*width+column]=index?uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)|0xff000000U:0U;
    }
    return image;
}
DrawPacket landscape_world_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,float vanishing_y,bool srgb,bool classic_ground_scroll,
    std::optional<float> level_horizon,std::optional<uint32_t> ground_dot_colour) {
    auto plain=options;plain.scroll_fraction.reset();plain.mode2_horizon.reset();
    plain.expanded_horizontal=false;plain.scroll_override.reset();
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,plain,srgb);
    if(packet.geometry.vertex_view().empty()) return packet;
    auto& data=packet.geometry.texels;
    data.resize(272+16384);
    // Native mapping at the screen centre: art row = screen row + offset.
    double offset=double(int16_t(ppu.bg2_scroll_y));
    if(ppu.background_mode==2 && ppu.bg2_vertical_offsets_enabled) {
        double count=0,sum_x=0,sum_y=0,sum_xx=0,sum_xy=0;int previous_raw=0,previous_unwrapped=0;
        for(int i=0;i<32;++i) {
            const auto address=std::size_t((0x2fa0U+unsigned(i))*2U)&0xffffU;
            const unsigned value=ppu.vram[address]|(unsigned(ppu.vram[(address+1)&0xffffU])<<8U);
            if(!(value&0x4000U)) continue;
            const int raw=int(value&8191U);
            int delta=(raw-previous_raw)&8191;if(delta>4095) delta-=8192;
            const int unwrapped=count==0?raw:previous_unwrapped+delta;
            previous_raw=raw;previous_unwrapped=unwrapped;
            count+=1;sum_x+=i+1;sum_y+=unwrapped;sum_xx+=double(i+1)*(i+1);sum_xy+=double(i+1)*unwrapped;
        }
        if(count>0) {
            const double denominator=count*sum_xx-sum_x*sum_x;
            const double slope=count>1 && denominator!=0?(count*sum_xy-sum_x*sum_y)/denominator:0;
            offset=(sum_y-slope*sum_x)/count+slope*(128.+(ppu.bg2_scroll_x&7))/8.;
        }
    }
    // Display-rate presentation: the interpolated offset line and scroll fraction.
    if(options.mode2_horizon && ppu.background_mode==2 && ppu.bg2_vertical_offsets_enabled)
        offset=double((*options.mode2_horizon)[0])+double((*options.mode2_horizon)[1])*(128.+(ppu.bg2_scroll_x&7))/8.;
    // The cartridge shifts the art by -tan(pitch) x 256 rows to fake camera
    // pitch; the caller applies the real pitch as geometry and supplies the
    // level (unpitched) horizon row.
    const float horizon=level_horizon?*level_horizon:float(double(vanishing_y)+offset);
    const float centre=float(128+int16_t(ppu.bg2_scroll_x))+(options.scroll_fraction?(*options.scroll_fraction)[0]:0.F);
    data[3]=data[4]=data[9]=data[10]=data[11]=data[12]=data[14]=data[15]=0;
    data[7]=4096U;
    data.push_back(starfox::bit_cast<uint32_t>(horizon));
    data.push_back(starfox::bit_cast<uint32_t>(centre));
    // The native screen shows rows [horizon-vanishing_y, +224); hold its top
    // and bottom rows beyond them.
    data.push_back(starfox::bit_cast<uint32_t>(horizon-vanishing_y));
    data.push_back(starfox::bit_cast<uint32_t>(horizon-vanishing_y+223.F));
    data.push_back(classic_ground_scroll?1U:0U);
    data.push_back(starfox::bit_cast<uint32_t>(256.F)); // Dot grid cell (source units).
    // Smooth ground haze for classic scroll: the art's centre-column colour
    // for the 112 rows below the horizon, with each stepped run replaced by a
    // linear blend between run centres, so nothing on the ground is fixed to
    // the screen except a soft fade (RGBA, red low byte).
    {
        const auto art=[&](int x,int y) {return bg2_art_index(ppu,x,y);};
        const auto palette=render::decode_bgr555_palette(ppu.cgram);
        std::array<std::array<float,3>,112> colours{};
        for(int row=0;row<112;++row) {
            const auto colour=palette[art(int(std::lround(centre)),int(std::floor(horizon))+row)];
            colours[size_t(row)]={float(colour.r),float(colour.g),float(colour.b)};
        }
        // Run centres as keys, then linear interpolation between them.
        std::vector<std::pair<float,std::array<float,3>>> keys;
        for(int row=0;row<112;) {
            int end=row;
            while(end<112 && colours[size_t(end)]==colours[size_t(row)]) ++end;
            keys.push_back({(row+end-1)*.5F,colours[size_t(row)]});
            row=end;
        }
        for(int row=0;row<112;++row) {
            std::array<float,3> value=keys.front().second;
            if(row>=keys.back().first) value=keys.back().second;
            else for(std::size_t k=0;k+1<keys.size();++k) if(row>=keys[k].first && row<=keys[k+1].first) {
                const float t=(row-keys[k].first)/std::max(1e-3F,keys[k+1].first-keys[k].first);
                for(unsigned c=0;c<3;++c) value[c]=keys[k].second[c]+(keys[k+1].second[c]-keys[k].second[c])*t;
                break;
            }
            data.push_back(uint32_t(std::lround(value[0]))|(uint32_t(std::lround(value[1]))<<8)
                |(uint32_t(std::lround(value[2]))<<16)|0xff000000U);
        }
    }
    // Ground dots drawn on the 3D ground plane itself (RGBA, alpha 0 = none):
    // the cartridge's 256-unit lattice, anchored to the world.
    data.push_back(ground_dot_colour.value_or(0U));
    // Ground dot spacing (source units): every fourth dot of the cartridge's
    // 256-unit lattice, which crowded wide views (512 still did at 32:9).
    data.push_back(starfox::bit_cast<uint32_t>(1024.F));
    // Geometry: a dome (64 x 32) then the ground plane at Y = -1; drawn in
    // that order without depth so the ground covers the dome's lower half.
    auto& vertices=packet.geometry.vertices;vertices.clear();
    constexpr float radius=200000.F,ground=100000.F;
    constexpr unsigned columns=64,rows=32;
    constexpr float pi=3.14159265358979F;
    const auto emit=[&](float x,float y,float z) {
        SceneVertex v{};v.position[0]=x;v.position[1]=y;v.position[2]=z;v.texture[3]=8|(srgb?2:0);vertices.push_back(v);
    };
    for(unsigned row=0;row<rows;++row) for(unsigned column=0;column<columns;++column)
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            const float u=float(column+((corner==1 || corner==2)?1:0))/columns*2*pi;
            const float v=(float(row+(corner>=2?1:0))/rows-.5F)*pi;
            emit(radius*std::cos(v)*std::sin(u),radius*std::sin(v),-radius*std::cos(v)*std::cos(u));
        }
    for(unsigned corner:{0U,1U,2U,0U,2U,3U})
        emit((corner==1 || corner==2)?ground:-ground,-1,corner>=2?ground:-ground);
    return packet;
}
float landscape_art_horizon(const simulation::SnesPpuState& ppu,float vanishing_y) {
    double offset=double(int16_t(ppu.bg2_scroll_y));
    if(ppu.background_mode==2 && ppu.bg2_vertical_offsets_enabled) {
        double count=0,sum_x=0,sum_y=0,sum_xx=0,sum_xy=0;int previous_raw=0,previous_unwrapped=0;
        for(int i=0;i<32;++i) {
            const auto address=std::size_t((0x2fa0U+unsigned(i))*2U)&0xffffU;
            const unsigned value=ppu.vram[address]|(unsigned(ppu.vram[(address+1)&0xffffU])<<8U);
            if(!(value&0x4000U)) continue;
            const int raw=int(value&8191U);
            int delta=(raw-previous_raw)&8191;if(delta>4095) delta-=8192;
            const int unwrapped=count==0?raw:previous_unwrapped+delta;
            previous_raw=raw;previous_unwrapped=unwrapped;
            count+=1;sum_x+=i+1;sum_y+=unwrapped;sum_xx+=double(i+1)*(i+1);sum_xy+=double(i+1)*unwrapped;
        }
        if(count>0) {
            const double denominator=count*sum_xx-sum_x*sum_x;
            const double slope=count>1 && denominator!=0?(count*sum_xy-sum_x*sum_y)/denominator:0;
            offset=(sum_y-slope*sum_x)/count+slope*(128.+(ppu.bg2_scroll_x&7))/8.;
        }
    }
    return float(double(vanishing_y)+offset);
}
// Tile payload, horizon/centre/rows/classic/cell, 112 haze rows, dot colour,
// dot spacing.
constexpr std::size_t landscape_world_texels=272+16384+6+112+2;
bool is_landscape_world(const DrawPacket& packet) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()==landscape_world_texels && texels[7]==4096U && !packet.geometry.vertex_view().empty();
}
DrawPacket landscape_ground_packet(const DrawPacket& world) {
    DrawPacket packet;
    if(!is_landscape_world(world)) return packet;
    packet.model=world.model;
    packet.geometry.texels.assign(world.geometry.texel_view().begin(),world.geometry.texel_view().end());
    packet.geometry.texels[7]|=16384U;
    const uint32_t flags=world.geometry.vertex_view().front().texture[3];
    constexpr float extent=400000.F;
    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
        SceneVertex v{};
        v.position[0]=(corner==1 || corner==2)?extent:-extent;v.position[2]=corner>=2?extent:-extent;
        v.texture[3]=flags;packet.geometry.vertices.push_back(v);
    }
    return packet;
}
bool is_landscape_ground(const DrawPacket& packet) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()==landscape_world_texels && texels[7]==(4096U|16384U) && !packet.geometry.vertex_view().empty();
}
namespace {
// Rz(roll) * Rx(pitch) * uniform scale * translation, column-major.
Matrix4 landscape_orientation(double roll,double pitch,float scale,std::array<float,3> translation) {
    const float cr=float(std::cos(roll)),sr=float(std::sin(roll)),cp=float(std::cos(pitch)),sp=float(std::sin(pitch));
    // Columns of Rz*Rx.
    const std::array<std::array<float,3>,3> r{{{cr,-sr,0},{sr*cp,cr*cp,sp},{-sr*sp,-cr*sp,cp}}};
    Matrix4 m{};
    for(unsigned c=0;c<3;++c) for(unsigned row=0;row<3;++row) m[c*4+row]=r[c][row]*scale;
    for(unsigned row=0;row<3;++row)
        m[12+row]=(r[0][row]*translation[0]+r[1][row]*translation[1]+r[2][row]*translation[2])*scale;
    m[15]=1;
    return m;
}
}
Matrix4 landscape_ground_model(double roll,double pitch,double height,double camera_x,double camera_z) noexcept {
    // Model space is world-fixed within an 8192-unit period (a multiple of the
    // 256-unit grid and of any power-of-two texture tile up to 8192): X runs
    // with world X, Z against world Z (forward is -Z), ground at Y = 0.
    const auto wrap=[](double value) {value=std::fmod(value,8192.);return value<0?value+8192.:value;};
    return landscape_orientation(roll,pitch,1.F/256.F,
        {float(-wrap(camera_x)),float(-std::max(height,1.)),float(wrap(camera_z))});
}
Matrix4 landscape_ground_view_model(const Matrix4& rotation,double height,double camera_x,double camera_z) noexcept {
    const auto wrap=[](double value) {value=std::fmod(value,8192.);return value<0?value+8192.:value;};
    const std::array<float,3> t{float(-wrap(camera_x)),float(-std::max(height,1.)),float(wrap(camera_z))};
    constexpr float scale=1.F/256.F;
    Matrix4 m{};
    for(unsigned column=0;column<3;++column) for(unsigned row=0;row<3;++row) m[column*4+row]=rotation[column*4+row]*scale;
    for(unsigned row=0;row<3;++row) m[12+row]=(rotation[row]*t[0]+rotation[4+row]*t[1]+rotation[8+row]*t[2])*scale;
    m[15]=1;
    return m;
}
Matrix4 landscape_dome_view_model(const Matrix4& rotation,double height) noexcept {
    const auto normalize=[](std::array<float,3> v) {
        const float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
        return length>1e-6F?std::array<float,3>{v[0]/length,v[1]/length,v[2]/length}:std::array<float,3>{0,0,0};
    };
    const auto up=normalize({rotation[4],rotation[5],rotation[6]});
    // Camera forward (-Z) flattened onto the ground: the dome's heading.
    auto forward=normalize({up[0]*up[2],up[1]*up[2],-1.F+up[2]*up[2]});
    if(forward==std::array<float,3>{0,0,0}) forward=normalize({-rotation[8],-rotation[9],-rotation[10]});
    const std::array<float,3> back{-forward[0],-forward[1],-forward[2]};
    const std::array<float,3> right{up[1]*back[2]-up[2]*back[1],up[2]*back[0]-up[0]*back[2],up[0]*back[1]-up[1]*back[0]};
    const float scale=float(std::max(height,1.))/256.F;
    Matrix4 m{};
    for(unsigned row=0;row<3;++row) {m[row]=right[row]*scale;m[4+row]=up[row]*scale;m[8+row]=back[row]*scale;}
    m[15]=1;
    return m;
}
DrawPacket landscape_texture_packet(LandscapeTexture kind,std::span<const uint32_t> rgba,
    unsigned width,unsigned height,float art_width,float rows_above,float tile,bool srgb) {
    if(!width || !height || rgba.size()!=std::size_t(width)*height || width>8192 || height>8192)
        throw std::invalid_argument("Invalid landscape texture");
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    auto& data=packet.geometry.texels;
    data.resize(16+rgba.size());
    data[0]=width;data[1]=height;data[2]=kind==LandscapeTexture::ground?1U:0U;
    data[3]=starfox::bit_cast<uint32_t>(art_width);data[4]=starfox::bit_cast<uint32_t>(rows_above);
    data[5]=starfox::bit_cast<uint32_t>(tile);data[7]=4096U|32768U;
    std::copy(rgba.begin(),rgba.end(),data.begin()+16);
    const auto emit=[&](float x,float y,float z) {
        SceneVertex v{};v.position[0]=x;v.position[1]=y;v.position[2]=z;v.texture[3]=8|(srgb?2:0);
        packet.geometry.vertices.push_back(v);
    };
    if(kind==LandscapeTexture::ground) {
        constexpr float extent=400000.F;
        for(unsigned corner:{0U,1U,2U,0U,2U,3U})
            emit((corner==1 || corner==2)?extent:-extent,0,corner>=2?extent:-extent);
    } else {
        constexpr unsigned columns=64,rows=16;constexpr float pi=3.14159265358979F,radius=200000.F;
        for(unsigned row=0;row<rows;++row) for(unsigned column=0;column<columns;++column)
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                const float u=float(column+((corner==1 || corner==2)?1:0))/columns*2*pi;
                const float v=float(row+(corner>=2?1:0))/rows*.5F*pi; // Upper hemisphere.
                emit(radius*std::cos(v)*std::sin(u),radius*std::sin(v),-radius*std::cos(v)*std::cos(u));
            }
    }
    return packet;
}
bool is_landscape_texture(const DrawPacket& packet,LandscapeTexture kind) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()>16 && texels[7]==(4096U|32768U) && texels[2]==(kind==LandscapeTexture::ground?1U:0U);
}
void set_headset_mapping(DrawPacket& packet) noexcept {
    // Header words the flat mappings leave unused: 3 (world/ground; zeroed
    // by landscape_world_packet), 6 (replacement art), 12 (corridor).
    auto& data=packet.geometry.texels;
    if(is_landscape_world(packet) || is_landscape_ground(packet)) data[3]=1;
    else if(is_landscape_texture(packet,LandscapeTexture::sky) || is_landscape_texture(packet,LandscapeTexture::ground)) data[6]=1;
    else if(is_tunnel_corridor(packet)) {
        // Rows by distance along the corridor (either way), and the planes
        // continue behind the seat instead of ending one band back.
        data[12]=1;
        for(auto& vertex:packet.geometry.vertices) if(vertex.position[2]>0) vertex.position[2]=corridor_far;
    }
}
Matrix4 landscape_world_model(double roll,double pitch,double height) noexcept {
    return landscape_orientation(roll,pitch,float(std::max(height,1.))/256.F,{0,0,0});
}
bool is_tunnel_corridor(const DrawPacket& packet) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()==corridor_texels && texels[7]==corridor_marker && !packet.geometry.vertex_view().empty()
        && (packet.geometry.vertex_view().front().texture[3]&8U)!=0;
}
namespace {
constexpr uint32_t nucleus_marker=4096U,nucleus_floor_marker=4097U;
constexpr std::size_t nucleus_texels=16;
// The painting's ceiling and floor bands placed in the room (model Z ranges
// about the ring's centre, nearest first) and its wall colour.
struct NucleusBand {double z0,z1;std::array<float,4> rgba;};
struct NucleusSurfaces {std::array<std::vector<NucleusBand>,2> bands;std::array<float,2> y{};std::array<float,4> wall{};};
std::optional<NucleusSurfaces> nucleus_surfaces(const simulation::SnesPpuState& source,unsigned brightness,bool srgb,
    float radius,float top,float bottom,float eye_y,float camera_offset) {
    if(brightness>15) throw std::invalid_argument("Invalid nucleus room brightness");
    if(!(radius>0) || !(top<bottom) || !std::isfinite(eye_y) || !std::isfinite(camera_offset))
        throw std::invalid_argument("Invalid nucleus room dimensions");
    if(!(source.main_screen&2) || (source.background_mode!=1 && source.background_mode!=2)) return {}; // Sector Z: Mode 1
    // The entry (BG_1_3DA) twists the camera through heights outside the
    // room; the eye only places the painting's bands, so keep it inside.
    eye_y=std::clamp(eye_y,top+1.F,bottom-1.F);
    // The picture as the screen shows it (the layer sits 16 rows up from its
    // scroll), centre column, with the per-row offsets centred.
    auto ppu=source;
    ppu.bg2_scroll_y=int16_t(ppu.bg2_scroll_y+16);
    ppu.bg2_horizontal_offsets.fill(128);ppu.bg2_horizontal_offsets_enabled=true;
    ppu.bg2_scanline_scroll_enabled=false;
    std::array<uint8_t,224> ink{};
    for(unsigned row=0;row<224;++row) ink[row]=render::tunnel_border_index(ppu,row,128);
    // The far wall: the band through the centre row. Its edges are where the
    // painting's ceiling and floor meet it; with the panels' real heights
    // above and below the eye they give the art's vanishing row.
    constexpr unsigned middle=118;
    unsigned first=middle,last=middle;
    while(first>0 && ink[first-1]==ink[middle]) --first;
    while(last<223 && ink[last+1]==ink[middle]) ++last;
    if(first==0 || last==223) return {};
    const double above=eye_y-top,below=bottom-eye_y;
    const double vanishing=first+double(last+1-first)*above/(above+below);
    const auto colour=[&](uint8_t index){return source_backdrop_colour(source.cgram[index],brightness,srgb);};
    const double depth_z=2.*radius;
    NucleusSurfaces result;
    result.wall=colour(ink[middle]);
    result.y={-top,-bottom};
    // One band per run of equal rows, from the distance its nearest row
    // paints to its farthest; the nearest extends to the room's back and the
    // farthest to the wall. The art's wall edge is nearer than the ring
    // straight ahead; stretch its bands so that edge lands on the real wall.
    const double painted_wall=(above+below)*256./double(last+1-first);
    const double stretch=std::max(1.,(camera_offset+depth_z)/painted_wall);
    for(const bool ceiling:{true,false}) {
        const double height=ceiling?above:below;
        struct Run {double near,far;uint8_t index;};
        std::vector<Run> runs;
        const auto distance=[&](double row) {
            const double rows=ceiling?vanishing-row:row-vanishing;
            return rows>0?stretch*height*256./rows:1e9;
        };
        if(ceiling) for(int row=0;row<int(first);) {
            int end=row;while(end+1<int(first) && ink[size_t(end+1)]==ink[size_t(row)]) ++end;
            runs.push_back({distance(row),distance(end+1),ink[size_t(row)]});row=end+1;
        } else for(int row=223;row>int(last);) {
            int end=row;while(end-1>int(last) && ink[size_t(end-1)]==ink[size_t(row)]) --end;
            runs.push_back({distance(row+1),distance(end),ink[size_t(row)]});row=end-1;
        }
        if(runs.empty()) continue;
        runs.front().near=-1e9;runs.back().far=1e9;
        for(const auto& run:runs) {
            // Model Z is camera_offset - distance ahead of the eye.
            const double z0=std::clamp(camera_offset-run.far,-depth_z,depth_z),z1=std::clamp(camera_offset-run.near,-depth_z,depth_z);
            if(z1>z0) result.bands[ceiling?0:1].push_back({z0,z1,colour(run.index)});
        }
    }
    return result;
}
void push_flat(DrawPacket& packet,std::span<const std::array<float,3>> polygon,const std::array<float,4>& rgba) {
    for(std::size_t i=1;i+1<polygon.size();++i) for(const auto* corner:{&polygon[0],&polygon[i],&polygon[i+1]}) {
        SceneVertex vertex{};
        std::copy(corner->begin(),corner->end(),vertex.position);
        std::copy(rgba.begin(),rgba.end(),vertex.color);vertex.color[3]=1;
        packet.geometry.vertices.push_back(vertex);
    }
}
}
DrawPacket nucleus_room_packet(const simulation::SnesPpuState& source,unsigned brightness,bool srgb,
    float radius,float top,float bottom,float eye_y,float camera_offset) {
    DrawPacket packet;
    const auto surfaces=nucleus_surfaces(source,brightness,srgb,radius,top,bottom,eye_y,camera_offset);
    if(!surfaces) return packet;
    packet.geometry.texels.assign(nucleus_texels,0);
    packet.geometry.texels[7]=nucleus_marker;
    packet.model=nucleus_room_model(0,eye_y,-camera_offset,0);
    // Layers rebuild every tick while the entry scrolls: reuse the geometry
    // (shared, so the GPU upload is skipped too) until its inputs change.
    std::vector<float> key{radius};
    for(unsigned plane=0;plane<2;++plane) {
        key.push_back(surfaces->y[plane]);
        for(const auto& band:surfaces->bands[plane]) {
            key.push_back(float(band.z0));key.push_back(float(band.z1));
            key.insert(key.end(),band.rgba.begin(),band.rgba.end());
        }
    }
    key.insert(key.end(),surfaces->wall.begin(),surfaces->wall.end());
    thread_local std::vector<float> cached_key;
    thread_local std::shared_ptr<const std::vector<SceneVertex>> cached;
    if(cached && key==cached_key) {packet.geometry.shared_vertices=cached;return packet;}
    const double depth_x=radius,depth_z=2.*radius;
    constexpr unsigned segments=128;
    // The wall, all round the ring.
    for(unsigned i=0;i<segments;++i) {
        const double a0=2*std::numbers::pi*i/segments,a1=2*std::numbers::pi*(i+1)/segments;
        const float x0=float(depth_x*std::sin(a0)),z0=float(depth_z*std::cos(a0));
        const float x1=float(depth_x*std::sin(a1)),z1=float(depth_z*std::cos(a1));
        const std::array<std::array<float,3>,4> quad{{{x0,-bottom,z0},{x1,-bottom,z1},{x1,-top,z1},{x0,-top,z0}}};
        push_flat(packet,quad,surfaces->wall);
    }
    cached=std::make_shared<const std::vector<SceneVertex>>(std::move(packet.geometry.vertices));
    cached_key=std::move(key);
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=cached;
    return packet;
}
DrawPacket nucleus_floor_packet(const simulation::SnesPpuState& source,unsigned brightness,bool srgb,
    float radius,float top,float bottom,float eye_y,float camera_offset,float reactor_x,float reactor_z,double turn,unsigned sectors) {
    DrawPacket packet;
    if(sectors<2 || sectors%2 || !std::isfinite(reactor_x) || !std::isfinite(reactor_z) || !std::isfinite(turn))
        throw std::invalid_argument("Invalid nucleus floor");
    const auto surfaces=nucleus_surfaces(source,brightness,srgb,radius,top,bottom,eye_y,camera_offset);
    if(!surfaces || surfaces->bands[0].empty() || surfaces->bands[1].empty()) return packet;
    // The painting's nearest (lightest) band and the same shade a touch
    // darker and greyer, alternating round the reactor like the Super FX
    // ground stripes: a subtle flat-shaded pinwheel the drum turns.
    constexpr unsigned arc=8;
    for(unsigned plane=0;plane<2;++plane) {
        const auto& bands=surfaces->bands[plane];
        const auto& light=bands.front().rgba;
        const float grey=(light[0]+light[1]+light[2])/3.F;
        std::array<float,4> dark=light;
        for(unsigned c=0;c<3;++c) dark[c]=std::clamp((light[c]+(grey-light[c])*.35F)*.92F,0.F,1.F);
        const std::array<std::array<float,4>,2> shades{light,dark};
        const float y=surfaces->y[plane];
        // Wedges from the reactor (turned by its spin), clipped to the
        // drum's ellipse on the unit circle (u = x / radius, v = z / 2 radius)
        // so nothing shows outside the room (the entry looks in from outside).
        using Point=std::array<double,2>;
        constexpr unsigned rim=96;
        const double reach=8.,c=std::cos(turn),s=std::sin(turn);
        const Point centre{reactor_x/radius,reactor_z/(2.*radius)};
        for(unsigned sector=0;sector<sectors;++sector) for(unsigned i=0;i<arc;++i) {
            std::vector<Point> polygon{centre};
            for(const unsigned k:{sector*arc+i,sector*arc+i+1}) {
                const double a=2*std::numbers::pi*k/(sectors*arc);
                const double dx=std::sin(a),dz=std::cos(a);
                const double x=c*dx+s*dz,z=-s*dx+c*dz;
                polygon.push_back({centre[0]+reach*x,centre[1]+reach*z/2.});
            }
            for(unsigned edge=0;edge<rim && polygon.size()>=3;++edge) {
                // Keep the side of this rim edge towards the circle's centre.
                const double a0=2*std::numbers::pi*edge/rim,a1=2*std::numbers::pi*(edge+1)/rim;
                const Point p{std::sin(a0),std::cos(a0)},q{std::sin(a1),std::cos(a1)};
                const auto side=[&](const Point& t){return (q[0]-p[0])*(t[1]-p[1])-(q[1]-p[1])*(t[0]-p[0]);};
                const double inside=side({0,0});
                std::vector<Point> out;
                for(std::size_t j=0;j<polygon.size();++j) {
                    const auto& a=polygon[j];const auto& b=polygon[(j+1)%polygon.size()];
                    const double sa=side(a)*inside,sb=side(b)*inside;
                    if(sa>=0) out.push_back(a);
                    if((sa>=0)!=(sb>=0)) {const double t=sa/(sa-sb);out.push_back({a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t});}
                }
                polygon=std::move(out);
            }
            if(polygon.size()<3) continue;
            std::vector<std::array<float,3>> points;
            for(const auto& point:polygon) points.push_back({float(point[0]*radius),y,float(point[1]*2.*radius)});
            push_flat(packet,points,shades[sector%2]);
        }
    }
    packet.model=nucleus_room_model(0,eye_y,-camera_offset,0);
    packet.geometry.texels.assign(nucleus_texels,0);
    packet.geometry.texels[7]=nucleus_floor_marker;
    return packet;
}
bool is_nucleus_floor(const DrawPacket& packet) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()==nucleus_texels && texels[7]==nucleus_floor_marker && !packet.geometry.vertex_view().empty();
}
bool is_nucleus_room(const DrawPacket& packet) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()==nucleus_texels && texels[7]==nucleus_marker && !packet.geometry.vertex_view().empty();
}
Matrix4 nucleus_room_model(double camera_x,double camera_y,double camera_z,double centre_z) noexcept {
    // Source units to scene metres: 1/256. Source Y is down and scene Y up;
    // model Z is minus world Z about the ring's centre (16-bit world Z wraps).
    constexpr float scale=1.F/256.F;
    const double offset=camera_z-centre_z,whole=std::round(offset);
    const double z=double(int16_t(uint16_t(int32_t(whole)&0xffff)))+(offset-whole);
    return {scale,0,0,0,0,scale,0,0,0,0,scale,0,float(-camera_x)*scale,float(camera_y)*scale,float(z)*scale,1};
}
Matrix4 tunnel_corridor_model(double camera_z,double camera_x,double camera_y,double centre_y,double centre_x) noexcept {
    // Model Z is minus world Z within one 256-unit band period; the eye sits
    // at model Z = -(camera Z mod 256). Source units to scene metres: 1/256.
    double phase=std::fmod(camera_z,256.);if(phase<0) phase+=256.;
    constexpr float scale=1.F/256.F;
    // The model's origin is the box's centre line. Source Y is down and
    // scene Y up, so a camera resting 2 units below centre (Y -58) sees the
    // box 2 units higher. Cameras far from the box (scripted views) keep the
    // art's eye-centred placement.
    const double y=std::abs(camera_y-centre_y)<32.?camera_y-centre_y:0.;
    return {scale,0,0,0,0,scale,0,0,0,0,scale,0,float(centre_x-camera_x)*scale,float(y)*scale,float(phase)*scale,1};
}
namespace {constexpr uint32_t screen_surround_marker=4200U;}
DrawPacket boss_roll_room_packet(const simulation::SnesPpuState& ppu,
    unsigned brightness,bool srgb,std::array<float,2> centre,double depth) {
    DrawPacket packet;
    if(!(depth>0) || !std::isfinite(depth) || !std::isfinite(centre[0]) || !std::isfinite(centre[1])) return packet;
    // The window: BG3 open and BG2 drawn.
    int left=256,right=-1,top=224,bottom=-1;
    for(unsigned y=0;y<224;y+=2) for(unsigned x=0;x<256;x+=2) {
        if(render::mode1_layer_index(ppu,3,x,y) || !render::mode1_layer_index(ppu,2,x,y)) continue;
        left=std::min(left,int(x));right=std::max(right,int(x)+2);
        top=std::min(top,int(y));bottom=std::max(bottom,int(y)+2);
    }
    if(right-left<16 || bottom-top<16) return packet;
    // The gradient down the window's middle column.
    const unsigned column=unsigned(left+right)/2U;
    std::array<std::array<float,4>,224> rows{};
    for(int y=top;y<bottom;++y) {
        const auto index=render::mode1_layer_index(ppu,2,column,unsigned(y));
        rows[size_t(y)]=source_backdrop_colour(ppu.cgram[index],brightness,srgb);
    }
    const auto shade=[&](double fraction,float light) {
        const int y=std::clamp(int(std::lround(top+fraction*(bottom-top-1))),top,bottom-1);
        auto c=rows[size_t(y)];for(int k=0;k<3;++k) c[size_t(k)]*=light;c[3]=1;return c;
    };
    // Camera space (x right, y down, z ahead), a little wider than the
    // window at the boss's depth, from well in front of it to past it.
    constexpr double margin=1.15;
    const double half_w=(right-left)*.5*margin,half_h=(bottom-top)*.5*margin;
    const double mid_x=(left+right)*.5-centre[0],mid_y=(top+bottom)*.5-centre[1];
    const double x0=(mid_x-half_w)*depth/256.,x1=(mid_x+half_w)*depth/256.;
    const double y0=(mid_y-half_h)*depth/256.,y1=(mid_y+half_h)*depth/256.;
    const double z0=depth*.45,z1=depth*2.2;
    const auto vertex=[&](double x,double y,double z,const std::array<float,4>& colour) {
        SceneVertex v{};v.position[0]=float(x);v.position[1]=float(-y);v.position[2]=float(-z);
        std::copy(colour.begin(),colour.end(),v.color);packet.geometry.vertices.push_back(v);
    };
    const auto quad=[&](std::array<std::array<double,3>,4> p,std::array<std::array<float,4>,4> c) {
        for(unsigned i:{0U,1U,2U,0U,2U,3U}) vertex(p[i][0],p[i][1],p[i][2],c[i]);
    };
    constexpr int strips=24;
    for(int i=0;i<strips;++i) {
        const double f0=double(i)/strips,f1=double(i+1)/strips;
        const double ya=y0+(y1-y0)*f0,yb=y0+(y1-y0)*f1;
        // Back wall, full light; side walls a little darker.
        quad({{{x0,ya,z1},{x1,ya,z1},{x1,yb,z1},{x0,yb,z1}}},{shade(f0,1),shade(f0,1),shade(f1,1),shade(f1,1)});
        quad({{{x0,ya,z0},{x0,ya,z1},{x0,yb,z1},{x0,yb,z0}}},{shade(f0,.78F),shade(f0,.9F),shade(f1,.9F),shade(f1,.78F)});
        quad({{{x1,ya,z1},{x1,ya,z0},{x1,yb,z0},{x1,yb,z1}}},{shade(f0,.9F),shade(f0,.78F),shade(f1,.78F),shade(f1,.9F)});
    }
    // Ceiling and floor take the gradient's ends, dimmer towards the front.
    quad({{{x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1}}},{shade(0,.7F),shade(0,.7F),shade(0,.9F),shade(0,.9F)});
    quad({{{x0,y1,z1},{x1,y1,z1},{x1,y1,z0},{x0,y1,z0}}},{shade(1,.9F),shade(1,.9F),shade(1,.7F),shade(1,.7F)});
    constexpr float scale=1.F/256.F;
    packet.model={scale,0,0,0,0,scale,0,0,0,0,scale,0,0,0,0,1};
    return packet;
}
DrawPacket screen_surround_packet(const std::array<float,4>& colour) {
    for(auto value:colour) if(!std::isfinite(value) || value<0 || value>1)
        throw std::invalid_argument("Invalid screen surround colour");
    DrawPacket packet;
    const auto quad=[&](float l,float t,float r,float b) {
        const std::array<float,2> corners[4]{{l,t},{r,t},{r,b},{l,b}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex vertex{};vertex.position[0]=corners[corner][0];vertex.position[1]=corners[corner][1];
            std::copy(colour.begin(),colour.end(),vertex.color);vertex.color[3]=1;
            packet.geometry.vertices.push_back(vertex);
        }
    };
    constexpr float far=8192;
    quad(-far,-far,0,224+far);quad(256,-far,256+far,224+far);
    quad(0,-far,256,0);quad(0,224,256,224+far);
    packet.geometry.texels.assign(16,0);packet.geometry.texels[7]=screen_surround_marker;
    return packet;
}
bool is_screen_surround(const DrawPacket& packet) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()==16 && texels[7]==screen_surround_marker;
}
DrawPacket tunnel_surround_packet(const std::array<float,4>& colour) {
    return tunnel_surround_packet(colour,colour,colour);
}
DrawPacket tunnel_surround_packet(const std::array<float,4>& wall,
    const std::array<float,4>& ceiling,const std::array<float,4>& floor) {
    for(const auto& colour:{wall,ceiling,floor}) for(auto value:colour)
        if(!std::isfinite(value) || value<0 || value>1)
        throw std::invalid_argument("Invalid tunnel surround colour");
    DrawPacket packet;
    const auto quad=[&](std::array<float,3> a,std::array<float,3> b,
        std::array<float,3> c,std::array<float,3> d,
        const std::array<float,4>& colour) {
        for(const auto& point:{a,b,c,a,c,d}) {
            SceneVertex vertex{};std::copy(point.begin(),point.end(),vertex.position);
            std::copy(colour.begin(),colour.end(),vertex.color);
            vertex.color[3]=1;packet.geometry.vertices.push_back(vertex);
        }
    };
    constexpr float l=-1024,r=1280,t=-1024,b=1248,z=4;
    quad({l,t,0},{0,t,0},{0,b,0},{l,b,0},wall);
    quad({256,t,0},{r,t,0},{r,b,0},{256,b,0},wall);
    quad({l,t,0},{r,t,0},{r,0,0},{l,0,0},ceiling);
    quad({l,224,0},{r,224,0},{r,b,0},{l,b,0},floor);
    quad({l,t,z},{r,t,z},{r,b,z},{l,b,z},wall);
    quad({l,t,0},{l,t,z},{l,b,z},{l,b,0},wall);
    quad({r,t,0},{r,t,z},{r,b,z},{r,b,0},wall);
    quad({l,t,0},{r,t,0},{r,t,z},{l,t,z},ceiling);
    quad({l,b,0},{r,b,0},{r,b,z},{l,b,z},floor);
    packet.model=source_layer_matrix(128,112,2).value();
    return packet;
}
namespace {
std::shared_ptr<const std::vector<SceneVertex>> make_sky_vertices(bool srgb,bool landscape,float horizon=112.F) {
    auto vertices=std::make_shared<std::vector<SceneVertex>>();
    constexpr unsigned columns=64,rows=32;
    vertices->reserve(columns*rows*6);
    constexpr float pi=std::numbers::pi_v<float>;
    for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<columns;++x) {
        constexpr unsigned corners[6][2]{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
        for(const auto& corner:corners) {
            const float longitude=(float(x+corner[0])/columns*2-1)*pi;
            const float latitude=(.5F-float(y+corner[1])/rows)*pi;
            SceneVertex v{};
            v.position[0]=64*std::cos(latitude)*std::sin(longitude);
            v.position[1]=64*std::sin(latitude);
            v.position[2]=-64*std::cos(latitude)*std::cos(longitude);
            v.uv[0]=128+longitude*512;v.uv[1]=112-latitude*512;
            if(landscape) {
                const float radial=std::max(.0001F,std::hypot(v.position[0],v.position[2]));
                v.uv[1]=std::clamp(horizon-v.position[1]*512/radial,0.F,223.F);
            }
            v.texture[3]=8|(srgb?2:0);vertices->push_back(v);
        }
    }
    return vertices;
}
std::shared_ptr<const std::vector<SceneVertex>> sky_vertices(bool srgb,bool landscape,float horizon=112.F) {
    if(landscape && horizon!=112.F) return make_sky_vertices(srgb,true,horizon);
    // Immutable geometry is shared across ticks, eyes and cartridge changes.
    // Only the palette/VRAM payload and model uniforms remain per-scene data.
    if(landscape) {
        if(srgb) {static const auto vertices=make_sky_vertices(true,true);return vertices;}
        static const auto vertices=make_sky_vertices(false,true);return vertices;
    }
    if(srgb) {static const auto vertices=make_sky_vertices(true,false);return vertices;}
    static const auto vertices=make_sky_vertices(false,false);return vertices;
}
}
std::array<float,4> source_menu_background_colour(const simulation::SnesPpuState& ppu,
    unsigned brightness,bool srgb) {
    // One palette-index sample at the authored menu's outer corner; no image
    // is rasterized/uploaded for the surrounding clear. Respect source scroll.
    render::Framebuffer corner(1,1);
    render::BackgroundRenderer decoder;
    decoder.draw_bg2(ppu,ppu.bg2_scroll_x,ppu.bg2_scroll_y,corner);
    return source_backdrop_colour(ppu.cgram[corner.get(0,0)],brightness,srgb);
}
DrawPacket intro_star_sphere_packet(const simulation::SnesPpuState& ppu,unsigned brightness,bool srgb,bool full_atlas,bool upper_left_only,bool retain_source_scroll,
    std::optional<std::array<float,2>> scroll_fraction,std::optional<std::array<float,2>> mode2_horizon) {
    BackgroundTileOptions options;options.brightness=brightness;options.transparent_black=true;
    // Flat presentation: display-rate scroll and Mode 2 tilt (roll).
    if(retain_source_scroll) {options.scroll_fraction=scroll_fraction;options.mode2_horizon=mode2_horizon;}
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=sky_vertices(srgb,false);
    if(!retain_source_scroll) {packet.geometry.texels[10]=0;packet.geometry.texels[12]=0;}
    packet.geometry.texels[15]=full_atlas?1:9; // EX's CRED atlas is entirely stars/nebulae; DEMO has a unique planet below.
    if(upper_left_only) packet.geometry.texels[15]=137; // Star-only upper-left 256x256 quadrant.
    // Full-atlas panoramas retain their authored placement even when native
    // scanline deformation is disabled. Resetting both scroll registers here
    // misplaced meteor/nebula bands in every caller sharing this path.
    if(!retain_source_scroll) packet.geometry.texels.resize(272+16384);
    return packet;
}
DrawPacket game_over_star_sphere_packet(const simulation::SnesPpuState& ppu,
    unsigned brightness,unsigned colour_subtract,bool srgb) {
    BackgroundTileOptions options;options.brightness=brightness;options.colour_subtract=colour_subtract;
    options.transparent_black=true;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();packet.geometry.shared_vertices=sky_vertices(srgb,false);
    auto& data=packet.geometry.texels;
    data[7]|=512U; // Stable star-only patches; keep Andross in the native front layer.
    data[10]=data[12]=0;data[15]=1;data.resize(272+16384);
    return packet;
}
Matrix4 intro_planet_motion(int16_t previous_scroll,int16_t current_scroll,double alpha,float horizon_y) {
    if(!std::isfinite(alpha) || !std::isfinite(horizon_y)) throw std::invalid_argument("Invalid planet motion");
    const int current=uint16_t(current_scroll)&511;
    int delta=current-(uint16_t(previous_scroll)&511);
    if(delta>255) delta-=512;else if(delta< -256) delta+=512;
    const double scroll=current-delta*(1-std::clamp(alpha,0.,1.));
    // Atlas planet center is y=364. Follow the native vertical reveal, but
    // rotate a distant patch rather than moving a close screen-space panel.
    const float angle=float(std::atan((horizon_y-364+scroll)/512.));
    const float c=std::cos(angle),s=std::sin(angle);
    return {1,0,0,0,0,c,s,0,0,-s,c,0,0,0,0,1};
}
DrawPacket intro_planet_packet(const simulation::SnesPpuState& ppu,unsigned brightness,bool srgb) {
    BackgroundTileOptions options;options.brightness=brightness;options.transparent_black=true;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    // DEMO's single planet occupies this atlas rectangle. Use atlas coordinates,
    // not the scrolling foreground window, and place it on the distant sky.
    packet.geometry.texels[3]=packet.geometry.texels[4]=0;
    packet.geometry.texels[10]=packet.geometry.texels[12]=0;
    packet.geometry.texels.resize(272+16384);
    for(auto& vertex:packet.geometry.vertices) {
        const float x=vertex.uv[0]/256.F,y=vertex.uv[1]/224.F;
        vertex.uv[0]=88+x*72;vertex.uv[1]=328+y*72;
        vertex.position[0]=(x-.5F)*9;vertex.position[1]=(.5F-y)*9;
        vertex.position[2]=-64;
    }
    return packet;
}
DrawPacket unique_planet_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,const std::array<unsigned,4>& rectangle,bool srgb) {
    const auto [x,y,w,h]=rectangle;
    if(!w || !h || x>=512 || y>=512 || w>512-x || h>512-y)
        throw std::invalid_argument("Invalid unique planet rectangle");
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    // Use the same effective signed scroll as the ordinary background path,
    // including presentation overrides, before clearing atlas sampling scroll.
    const float cx=float(x)+float(w)*.5F-float(static_cast<int32_t>(packet.geometry.texels[3]));
    const float cy=float(y)+float(h)*.5F-float(static_cast<int32_t>(packet.geometry.texels[4]));
    const float yaw=std::atan((cx-128.F)/512.F),pitch=std::atan((112.F-cy)/512.F);
    const float sy=std::sin(yaw),cyaw=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    packet.geometry.vertices.clear();
    for(const auto& corner:std::array<std::array<float,2>,6>{{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}}}) {
        const float u=(corner[0]-.5F)*float(w)*.125F,v=(.5F-corner[1])*float(h)*.125F;
        SceneVertex vertex{};
        vertex.position[0]=64*sy*cp+u*cyaw-v*sy*sp;
        vertex.position[1]=64*sp+v*cp;
        vertex.position[2]=-64*cyaw*cp+u*sy+v*cyaw*sp;
        vertex.uv[0]=float(x)+corner[0]*float(w);vertex.uv[1]=float(y)+corner[1]*float(h);
        vertex.texture[3]=8|(srgb?2:0);packet.geometry.vertices.push_back(vertex);
    }
    auto& data=packet.geometry.texels;
    data[3]=data[4]=data[9]=data[10]=data[11]=data[12]=data[14]=0;data[15]=1;
    data.resize(272+16384);
    return packet;
}
DrawPacket landscape_landmark_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,const std::array<unsigned,4>& rectangle,
    const std::array<bool,256>& keep,bool srgb) {
    auto packet=unique_planet_packet(ppu,options,rectangle,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    const auto corners=packet.geometry.vertices;packet.geometry.vertices.clear();
    const auto [rx,ry,width,height]=rectangle;
    const auto word=[&](unsigned a){return unsigned(ppu.vram[a&65535])|(unsigned(ppu.vram[(a+1)&65535])<<8);};
    const auto selected=[&](unsigned x,unsigned y) {
        const unsigned edge=ppu.bg2_tile_size_16?16:8;
        x=(x+rx)&((ppu.bg2_screen_size&1)?64*edge-1:32*edge-1);
        y=(y+ry)&((ppu.bg2_screen_size&2)?64*edge-1:32*edge-1);
        const unsigned mx=x/edge,my=y/edge,pages=(ppu.bg2_screen_size&1)?2:1;
        const unsigned entry=(mx/32+my/32*pages)*1024+(my%32)*32+mx%32;
        const unsigned tile=word(ppu.bg2_screen_base*2+entry*2);
        unsigned px=x%edge,py=y%edge;
        if(tile&0x4000) px=edge-1-px;if(tile&0x8000) py=edge-1-py;
        const unsigned character=((tile&1023)+(px/8)+(py/8)*16)&1023;
        const unsigned base=ppu.bg2_character_base*2+character*32+(py%8)*2;
        unsigned ink=0;
        for(unsigned bit=0;bit<4;++bit) ink|=((ppu.vram[(base+bit%2+bit/2*16)&65535]>>(7-px%8))&1)<<bit;
        return ink!=0 && keep[((tile>>10)&7)*16+ink];
    };
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;) {
        if(!selected(x,y)) {++x;continue;}
        const unsigned left=x;while(x<width && selected(x,y)) ++x;
        const std::array<std::array<float,2>,4> run{{{float(left)/width,float(y)/height},
            {float(x)/width,float(y)/height},{float(x)/width,float(y+1)/height},
            {float(left)/width,float(y+1)/height}}};
        for(unsigned i:{0U,1U,2U,0U,2U,3U}) {
            auto vertex=corners[0];const auto [u,v]=run[i];
            for(unsigned c=0;c<3;++c) vertex.position[c]+=u*(corners[1].position[c]-corners[0].position[c])
                +v*(corners[5].position[c]-corners[0].position[c]);
            vertex.uv[0]=float(rx)+u*width;vertex.uv[1]=float(ry)+v*height;
            packet.geometry.vertices.push_back(vertex);
        }
    }
    return packet;
}
DrawPacket landscape_sphere_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,float horizon_y,bool srgb,bool unique_half,uint16_t atlas_origin,bool unique_right_half) {
    if(!std::isfinite(horizon_y)) throw std::invalid_argument("Invalid landscape horizon");
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=sky_vertices(srgb,true,horizon_y);
    // Each atlas has a source-specific origin. Its native rotate/offset
    // tables fake camera motion on a flat screen and must not deform a sphere.
    packet.geometry.texels[3]=0;packet.geometry.texels[4]=atlas_origin&511U;
    // Landscapes surround the viewer horizontally, but their terminal ground
    // row must not wrap vertically into sky when the source origin is low.
    packet.geometry.texels[15]|=0x20000000U;
    if(atlas_origin>=288) packet.geometry.texels[15]|=0x40000000U;
    bool wide_unique_left=false;
    if(unique_half || unique_right_half) {
        const auto atlas_width=((packet.geometry.texels[2]&1U)?64U:32U)
            *(packet.geometry.texels[8]?16U:8U);
        if(atlas_width!=512U && atlas_width!=1024U)
            throw std::invalid_argument("Unsupported unique landscape atlas width");
        wide_unique_left=unique_half && atlas_width==1024U;
        packet.geometry.texels[15]|=64U;
        if(unique_right_half) packet.geometry.texels[15]|=128U;
    }
    if(unique_right_half || options.ex_ocean_island || options.ex_volcanic_horizon || wide_unique_left) {
        // These EX atlases put the moon on the first visible sky row.
        // Clamping the upper hemisphere to that row turns it into a stripe;
        // extend into the verified empty sky band immediately above it.
        const auto build_vertices=[&]() -> std::shared_ptr<const std::vector<SceneVertex>> {
          auto vertices=std::make_shared<std::vector<SceneVertex>>(*packet.geometry.shared_vertices);
          for(auto& vertex:*vertices) {
            const auto radial=std::max(.0001F,std::hypot(vertex.position[0],vertex.position[2]));
            vertex.uv[1]=std::clamp(horizon_y-vertex.position[1]*512/radial,
                options.ex_city_planets?-208.F:options.ex_volcanic_horizon?-176.F:(options.ex_ocean_island || wide_unique_left)?-64.F:-32.F,223.F);
            // Repeated stars must continue varying with latitude all the way
            // to the zenith; a clamped source row creates a large empty cap.
            if(options.ex_city_planets && vertex.position[1]>0)
                vertex.uv[1]=horizon_y-std::atan2(vertex.position[1],radial)*512.F;
          }
          return vertices;
        };
        if(horizon_y==112.F) {
            struct CachedSky {
                std::once_flag initialized;
                std::shared_ptr<const std::vector<SceneVertex>> vertices;
            };
            static std::array<CachedSky,8> cache;
            const unsigned variant=options.ex_city_planets?3U:options.ex_volcanic_horizon?2U:
                (options.ex_ocean_island || wide_unique_left)?1U:0U;
            auto& cached=cache[variant*2U+unsigned(srgb)];
            std::call_once(cached.initialized,[&]{cached.vertices=build_vertices();});
            packet.geometry.shared_vertices=cached.vertices;
        } else packet.geometry.shared_vertices=build_vertices();
    }
    packet.geometry.texels[9]=packet.geometry.texels[10]=packet.geometry.texels[11]=0;
    packet.geometry.texels[12]=packet.geometry.texels[14]=0;
    packet.geometry.texels.resize(272+16384);
    return packet;
}
void place_landscape_ground(DrawPacket& packet,float ground_y,bool gpu) {
    if(!std::isfinite(ground_y) || ground_y>=0 || ground_y< -8.F)
        throw std::invalid_argument("Invalid landscape receiver height");
    const auto source=packet.geometry.vertex_view();
    if(source.empty()) return;
    // Reconstruct the lower hemisphere's source projection per fragment;
    // screen-linear UV interpolation across flattened triangles stretches rows.
    packet.geometry.texels[15]|=0x10000000U;
    if(gpu) {
        if(packet.geometry.texels.size()!=272+16384)
            throw std::invalid_argument("GPU landscape receiver requires a plain landscape tile payload");
        static std::mutex cache_mutex;
        static std::shared_ptr<const std::vector<SceneVertex>> cached_source,cached_vertices;
        std::lock_guard lock(cache_mutex);
        if(!packet.geometry.shared_vertices || cached_source!=packet.geometry.shared_vertices) {
            auto prepared=std::make_shared<std::vector<SceneVertex>>(source.begin(),source.end());
            for(size_t triangle=0;triangle+2<prepared->size();triangle+=3) {
                if((*prepared)[triangle].position[1]>0 || (*prepared)[triangle+1].position[1]>0
                    || (*prepared)[triangle+2].position[1]>0) continue;
                for(size_t corner=0;corner<3;++corner) (*prepared)[triangle+corner].texture[3]|=268435456U;
            }
            cached_source=packet.geometry.shared_vertices;cached_vertices=std::move(prepared);
        }
        packet.geometry.vertices.clear();packet.geometry.shared_vertices=cached_vertices;
        packet.geometry.texels.push_back(starfox::bit_cast<uint32_t>(ground_y));
        return;
    }
    auto vertices=std::make_shared<std::vector<SceneVertex>>(source.begin(),source.end());
    for(size_t triangle=0;triangle+2<vertices->size();triangle+=3) {
        // Horizon vertices are duplicated per triangle. Keep the sky's copy
        // on its distant sphere; pulling it out to the floor's far edge
        // stretches the adjoining cloud triangles into visible wedges.
        if((*vertices)[triangle].position[1]>0 || (*vertices)[triangle+1].position[1]>0
            || (*vertices)[triangle+2].position[1]>0) continue;
        for(size_t corner=0;corner<3;++corner) {
        auto& vertex=(*vertices)[triangle+corner];
        const float radial=std::hypot(vertex.position[0],vertex.position[2]);
        if(radial>.0001F) {
            const float distance=vertex.position[1]<-.0001F
                ?std::min(4096.F,radial*ground_y/vertex.position[1]):4096.F;
            vertex.position[0]*=distance/radial;vertex.position[2]*=distance/radial;
        } else vertex.position[0]=vertex.position[2]=0;
        vertex.position[1]=ground_y;
        }
    }
    packet.geometry.vertices.clear();packet.geometry.shared_vertices=std::move(vertices);
}
DrawPacket water_surround_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,bool srgb) {
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg3,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=sky_vertices(srgb,true);
    // Distant Mode-1 backdrop retains its own atlas origin. The BG2
    // perspective water/bridge layer must not repeat over the sky sphere.
    return packet;
}
DrawPacket water_surface_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,float height,bool srgb) {
    if(!std::isfinite(height) || height<.01F || height>8.F)
        throw std::invalid_argument("Invalid water receiver height");
    auto expanded=options;expanded.expanded_horizontal=true;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,expanded,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.texels[14]=0; // Water is open, not an enclosed tunnel.
    packet.geometry.texels[11]=1; // Clamp side water sampling; never repeat bridge.
    packet.geometry.texels[15]|=32U; // Fragment-local inverse projection.
    constexpr unsigned columns=1,rows=1;
    packet.geometry.vertices.reserve(columns*rows*6*2);
    // One floor and one authored overhead surface. Geometry, unlike the
    // source screen image, remains planar under independent eye/head poses.
    for(int side:{-1,1}) for(unsigned row=0;row<rows;++row) for(unsigned col=0;col<columns;++col) {
        constexpr unsigned corners[6][2]{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
        for(const auto& corner:corners) {
            const float x=(float(col+corner[0])/columns*2-1)*64;
            const float z=(float(row+corner[1])/rows*2-1)*64;
            const float depth=std::max(std::abs(z),.01F);
            SceneVertex vertex{};
            vertex.position[0]=x;vertex.position[1]=side*height;vertex.position[2]=z;
            // Behind the viewer sample water-only outer columns, not a
            // mirrored second bridge. Source rows remain within its band.
            vertex.uv[0]=z<0?128+x*256/depth:-256.F;
            vertex.uv[1]=std::clamp(112-side*height*256/depth,0.F,223.F);
            vertex.texture[3]=8|(srgb?2:0);
            packet.geometry.vertices.push_back(vertex);
        }
    }
    return packet;
}
namespace {
constexpr uint32_t water_bridge_marker=4098U;
}
DrawPacket water_bridge_packet(const simulation::SnesPpuState& source,unsigned brightness,bool srgb,
    float top,float bottom,float view,int32_t camera_x,int32_t camera_y,int32_t camera_z,float vanishing_row) {
    DrawPacket packet;
    if(brightness>15) throw std::invalid_argument("Invalid bridge brightness");
    if(!(top<view && view<bottom)) throw std::invalid_argument("Invalid bridge heights");
    if(!(source.main_screen&2) || source.background_mode!=1) return packet;
    // The art itself (camera independent): the 512-column tile map, centre
    // column 256. The horizon row is where open water starts at the far edge.
    const auto ink=[&](int x,int y) {return bg2_art_index(source,x,y);};
    int horizon=-1;
    for(int row=1;row<512 && horizon<0;++row) if(!ink(0,row-1) && ink(0,row) && ink(511,row)) horizon=row;
    if(horizon<0) return packet;
    const auto half_run=[&](int row) {
        const auto centre=ink(256,row);
        if(!centre) return 0;
        int left=256;while(left>0 && ink(left-1,row)==centre) --left;
        int right=256;while(right<511 && ink(right+1,row)==centre) ++right;
        return (right-left+1)/2;
    };
    // The span: its main colour is the commonest ink in the rows above the
    // horizon (its row-by-row shading is fixed to the screen, so drop it);
    // its edge sits one column further out per row.
    std::array<unsigned,256> votes{};
    double span_wk=0,span_kk=0;int ridge_row=-1;
    // The apex's last row or two can be blank.
    int start=horizon-1;while(start>horizon-4 && start>0 && !ink(256,start)) --start;
    for(int row=start;row>=std::max(0,horizon-200);--row) {
        if(!ink(256,row)) break;
        const double k=horizon-row-.5;
        int left=256;while(left>0 && ink(left-1,row)) --left;
        const double w=256-left;
        span_wk+=w*k;span_kk+=k*k;
        for(int x=left;x<=256;++x) ++votes[ink(x,row)];
        ridge_row=row;
    }
    const double deck_height=bottom-view,span_height=view-top;
    double deck_wk=0,deck_kk=0;
    for(int row=horizon;row<std::min(512,horizon+200);++row) {
        if(!ink(0,row)) break;
        const double k=row-horizon+.5;
        deck_wk+=half_run(row)*k;deck_kk+=k*k;
    }
    if(span_kk<=0 || deck_kk<=0 || ridge_row<0) return packet;
    const unsigned span_ink=unsigned(std::max_element(votes.begin()+1,votes.end())-votes.begin());
    const double deck_half=deck_wk/deck_kk*deck_height,span_half=span_wk/span_kk*span_height;
    if(!(deck_half>1) || !(span_half>1)) return packet;
    // The ridge: the band of another ink about the centre on the span's
    // nearest row, as a width at that row's distance.
    unsigned ridge_ink=ink(256,ridge_row);
    double ridge_half=0;
    if(ridge_ink!=span_ink) {
        int left=256;while(left>0 && ink(left-1,ridge_row)==ridge_ink) --left;
        ridge_half=(256-left)/(horizon-ridge_row-.5)*span_height;
    }
    const auto colour=[&](unsigned index){return source_backdrop_colour(source.cgram[index&255U],brightness,srgb);};
    constexpr float far=-8388608.F,behind=8192.F,side=8388608.F;
    const float deck_y=-bottom,span_y=-top;
    // Water bands: one per run of equal rows at the far edge, from the
    // distance its nearest row paints to its farthest; the last runs on
    // under the eye and behind it.
    struct Band {double near,far;unsigned index;};
    std::vector<Band> bands;
    const auto distance=[&](double rows){return rows>0?deck_height*256./rows:1e9;};
    for(int row=horizon;row<std::min(512,horizon+200);) {
        const auto index=ink(0,row);
        if(!index) break;
        int end=row;while(end+1<std::min(512,horizon+200) && ink(0,end+1)==index) ++end;
        bands.push_back({distance(end+1-horizon),distance(row-horizon),index});
        row=end+1;
    }
    if(bands.empty()) return packet;
    bands.back().near=-behind;
    // Stripes fixed in the world, 128 units light and 128 at 80% shade (in
    // linear light), as on the planet grounds; past `reach` they would be
    // finer than a pixel, so the water there takes their average.
    constexpr double stripe=128.,reach=16384.;
    const double eye=double(uint16_t(camera_z));
    const double light_shade=1.,dark_shade=srgb?.8:std::pow(.8,1./2.2),mean_shade=srgb?.9:std::pow(.9,1./2.2);
    const auto shaded=[&](unsigned index,double shade) {
        auto rgba=colour(index);for(unsigned c=0;c<3;++c) rgba[c]=float(rgba[c]*shade);return rgba;
    };
    for(const auto& band:bands) {
        // Model Z runs from the far edge (z0) to the near one (z1).
        const double z0=std::max(double(far),-band.far),z1=-band.near;
        if(!(z1>z0)) continue;
        const auto water=[&](double a,double b,double shade) {
            push_flat(packet,std::array<std::array<float,3>,4>{{{-side,deck_y,float(b)},{side,deck_y,float(b)},
                {side,deck_y,float(a)},{-side,deck_y,float(a)}}},shaded(band.index,shade));
        };
        const double split=std::max(z0,-reach);
        if(split>z0) water(z0,split,mean_shade);
        // World Z is the eye's world Z minus model Z.
        double z=split;
        while(z<z1) {
            // The stripe holding the world just beyond z, and where it ends.
            const double cell=std::floor((eye-z-1e-6)/stripe);
            const double end=std::min(z1,eye-cell*stripe);
            water(z,end,(int64_t(cell)&1)?dark_shade:light_shade);
            z=end;
        }
    }
    // The deck (the span's shadow on the water), bent by the water wave:
    // each screen row's offset less the camera's sideways term
    // (128 + camera X x rows below the horizon / eye height), at that row's
    // distance. A positive offset scrolls the picture left.
    const double eye_height=std::max(1.,double(bottom)-double(camera_y));
    const auto deck_colour=colour(ink(256,horizon+8));
    struct Bend {double z,x;};
    std::vector<Bend> bends;
    const bool wave=source.bg2_horizontal_offsets_enabled;
    for(int row=223;row>=0;--row) {
        const double k=row+.5-double(vanishing_row);
        if(k<1.) break;
        double shift=0;
        if(wave) shift=double(source.bg2_horizontal_offsets[size_t(row)])-128.-double(camera_x)*k/eye_height;
        // Rows a few pixels off are the wave; larger ones are something else.
        if(std::abs(shift)>12.) shift=0;
        bends.push_back({-deck_height*256./k,shift});
    }
    // Turn pixels into units at each row's distance (a pixel spans
    // distance / 256 units).
    for(auto& bend:bends) bend.x=-bend.x*(-bend.z)/256.;
    // The cartridge's wave (MHOFS.MC mwibbletunnel) is a sine table stepped
    // along the rows' depth, drawn in whole pixels, so the rows step and the
    // steps shift as the wave moves: jagged at headset resolution. Draw the
    // sine itself instead: the least-squares fit of x = a sin(wz) + b cos(wz)
    // + c over a range of wavelengths, through every row.
    if(wave && bends.size()>=16) {
        struct Fit {double error=1e300,w=0,a=0,b=0,c=0;} best;
        // Solves the 3x3 normal equations in place (Gauss-Jordan); false if singular.
        const auto solve=[](double (&m)[3][4]) {
            for(int pivot=0;pivot<3;++pivot) {
                int row=pivot;
                for(int r=pivot+1;r<3;++r) if(std::abs(m[r][pivot])>std::abs(m[row][pivot])) row=r;
                std::swap(m[row],m[pivot]);
                if(std::abs(m[pivot][pivot])<1e-9) return false;
                for(int r=0;r<3;++r) if(r!=pivot) {
                    const double factor=m[r][pivot]/m[pivot][pivot];
                    for(int c=pivot;c<4;++c) m[r][c]-=factor*m[pivot][c];
                }
            }
            return true;
        };
        for(double wavelength=192.;wavelength<=12288.;wavelength*=1.04) {
            const double w=2*3.14159265358979/wavelength;
            double m[3][4]{};
            for(const auto& bend:bends) {
                const double f[3]{std::sin(w*bend.z),std::cos(w*bend.z),1.};
                for(int r=0;r<3;++r) {for(int c=0;c<3;++c) m[r][c]+=f[r]*f[c];m[r][3]+=f[r]*bend.x;}
            }
            if(!solve(m)) continue;
            const double a=m[0][3]/m[0][0],b=m[1][3]/m[1][1],c=m[2][3]/m[2][2];
            double error=0;
            for(const auto& bend:bends) {
                const double d=a*std::sin(w*bend.z)+b*std::cos(w*bend.z)+c-bend.x;error+=d*d;
            }
            if(error<best.error) best={error,w,a,b,c};
        }
        if(best.w>0)
            for(auto& bend:bends) bend.x=best.a*std::sin(best.w*bend.z)+best.b*std::cos(best.w*bend.z)+best.c;
    }
    if(bends.empty()) bends.push_back({0.,0.});
    // Nearer than the bottom row and beyond the farthest: held straight.
    bends.insert(bends.begin(),{double(behind),bends.front().x});
    bends.push_back({double(far),bends.back().x});
    for(std::size_t i=0;i+1<bends.size();++i) {
        const auto& a=bends[i];const auto& b=bends[i+1];
        const float half=float(deck_half);
        push_flat(packet,std::array<std::array<float,3>,4>{{{float(a.x)-half,deck_y,float(a.z)},{float(a.x)+half,deck_y,float(a.z)},
            {float(b.x)+half,deck_y,float(b.z)},{float(b.x)-half,deck_y,float(b.z)}}},deck_colour);
    }
    const auto strip=[&](float half,float y,unsigned index) {
        push_flat(packet,std::array<std::array<float,3>,4>{{{-half,y,behind},{half,y,behind},{half,y,far},{-half,y,far}}},colour(index));
    };
    strip(float(span_half),span_y,span_ink);
    if(ridge_half>.25) strip(float(ridge_half),span_y,ridge_ink);
    packet.geometry.texels.assign(16,0);
    packet.geometry.texels[7]=water_bridge_marker;
    packet.model=nucleus_room_model(0,view,0,0);
    return packet;
}
namespace {
constexpr uint32_t colony_marker=4099U;
// A least-squares line t = a + b row through (row, t) samples.
struct LineFit {
    double n{},sx{},sy{},sxx{},sxy{};
    void add(double x,double y) {n+=1;sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;}
    [[nodiscard]] std::optional<std::array<double,2>> line() const {
        const double d=n*sxx-sx*sx;
        if(n<8 || std::abs(d)<1e-9) return std::nullopt;
        const double b=(n*sxy-sx*sy)/d;return std::array<double,2>{(sy-b*sx)/n,b};
    }
};
}
DrawPacket colony_corridor_packet(const simulation::SnesPpuState& source,unsigned brightness,bool srgb,
    float top,float bottom,int32_t camera_z) {
    DrawPacket packet;
    if(brightness>15) throw std::invalid_argument("Invalid colony brightness");
    if(!(top<bottom)) throw std::invalid_argument("Invalid colony heights");
    if(!source.tunnel_scene || !(source.main_screen&2) || source.background_mode!=1) return packet;
    const double height=(bottom-top)*.5;
    // The art as the screen shows it with the per-row offsets centred; each
    // row's page is its scanline scroll (the cartridge's tunnel table).
    auto ppu=source;
    ppu.bg2_horizontal_offsets.fill(128);ppu.bg2_horizontal_offsets_enabled=true;
    const auto pages=source.bg2_scanline_scroll_y;
    const bool per_row=source.bg2_scanline_scroll_enabled;
    const auto art=[&](int16_t page,int row,int column) {
        ppu.bg2_scanline_scroll_y.fill(page);ppu.bg2_scanline_scroll_enabled=true;
        return render::tunnel_border_index(ppu,unsigned(std::clamp(row,0,223)),unsigned(std::clamp(column,0,255)));
    };
    const auto page_of=[&](int row){return per_row?pages[size_t(row)]:int16_t(source.bg2_scroll_y);};
    // The ceiling's right edge (the same on both pages) gives the vanishing
    // row and the ceiling's width.
    LineFit ceiling_right;
    for(int row=0;row<104;++row) {
        const auto page=page_of(row);
        const auto centre=art(page,row,128);
        if(!centre) continue;
        int right=128;while(right<255 && art(page,row,right+1)==centre) ++right;
        if(right<255) ceiling_right.add(row+.5,right+1.-128.);
    }
    const auto c=ceiling_right.line();
    if(!c || !((*c)[1]<0)) return packet;
    const double vp=-(*c)[0]/(*c)[1];
    if(vp<60 || vp>170) return packet;
    const double ceiling_half=-(*c)[1]*height;
    // Each page's own floor: its extent either side (each edge a line
    // through the vanishing row), and its centre line.
    // The art's haze near the vanishing row is fixed to the screen; colours
    // come from rows far from it.
    struct Page {
        unsigned ceiling{},floor{},line{},wall{};
        double floor_left{},floor_right{},line_half{};bool valid{};
    };
    std::map<int16_t,Page> looks;
    const int deep=221;
    const auto look=[&](int16_t page) -> const Page& {
        auto found=looks.find(page);
        if(found!=looks.end()) return found->second;
        Page result;
        result.ceiling=art(page,2,128);
        result.floor=art(page,deep,160);
        result.line=art(page,deep,128);
        result.wall=art(page,int(vp),255);
        if(result.line!=result.floor) {
            int left=128,right=128;
            while(left>0 && art(page,deep,left-1)==result.line) --left;
            while(right<255 && art(page,deep,right+1)==result.line) ++right;
            result.line_half=(right-left+1)*.5/(deep+.5-vp)*height;
        } else result.line=0;
        LineFit right_edge,left_edge;
        for(int row=int(std::ceil(vp))+4;row<224;++row) {
            // This row's own floor shade (the haze grades it by row).
            const auto shade=art(page,row,160);
            if(!shade || shade==result.wall) continue;
            int right=160;while(right<255 && art(page,row,right+1)==shade) ++right;
            int left=160;
            while(left>0) {
                const auto next=art(page,row,left-1);
                if(next!=shade && !(result.line && next==result.line)) break;
                --left;
            }
            if(right<255) right_edge.add(row+.5-vp,right+1.-128.);
            if(left>0) left_edge.add(row+.5-vp,128.-left);
        }
        // Edges through the vanishing row: width = slope x rows below it.
        const auto slope=[](const LineFit& fit) -> std::optional<double> {
            if(fit.n<8 || fit.sxx<=0) return std::nullopt;
            return fit.sxy/fit.sxx;
        };
        const auto right_slope=slope(right_edge),left_slope=slope(left_edge);
        result.floor_right=right_slope?*right_slope*height:ceiling_half;
        // No left edge on screen: the floor runs on past the view.
        result.floor_left=left_slope?-*left_slope*height:-ceiling_half*4.;
        result.valid=right_slope.has_value() && result.floor!=0;
        return looks.emplace(page,result).first->second;
    };
    const double ceiling_vp=vp,floor_vp=vp;
    for(int row=0;row<224;++row) if(!look(page_of(row)).valid && row>vp+4) return packet;
    const auto colour=[&](unsigned index){return source_backdrop_colour(source.cgram[index&255U],brightness,srgb);};
    constexpr float far=-8388608.F,behind=8192.F;
    const float floor_y=-bottom,ceiling_y=-top;
    // Bands: runs of rows on one page, from the distance the nearest row of
    // the run paints to its farthest. The nearest run continues behind the
    // eye and the farthest to the horizon.
    struct Band {float z0,z1;int16_t page;};
    const auto bands=[&](bool ceiling) {
        std::vector<Band> result;
        const double vp=ceiling?ceiling_vp:floor_vp;
        const int first=ceiling?0:int(std::ceil(vp));
        const int last=ceiling?int(std::floor(vp))-1:223;
        const auto distance=[&](double row){const double k=std::abs(row-vp);return k>.25?height*256./k:1e7;};
        // Walk from the far rows (by the horizon) to the near ones.
        const int step=ceiling?-1:1;
        int row=ceiling?last:first;
        while(ceiling?row>=first:row<=last) {
            const auto page=page_of(row);
            int end=row;while((ceiling?end-1>=first:end+1<=last) && page_of(end+step)==page) end+=step;
            const double row_far=ceiling?row+1.:double(row),row_near=ceiling?double(end):end+1.;
            result.push_back({float(-distance(row_far)),float(-distance(row_near)),page});
            row=end+step;
        }
        if(!result.empty()) {result.front().z0=far;result.back().z1=behind;}
        return result;
    };
    auto floor_bands=bands(false),ceiling_bands=bands(true);
    if(floor_bands.empty() || ceiling_bands.empty()) return packet;
    // The cartridge's table swaps the two pages every 128 units of world Z
    // (one 256-unit cycle), locked to the camera's Z. Bands cut from this
    // tick's rows jump a row's worth each tick and stop where the rows run
    // out near the horizon; lay them in the world instead. The phase comes
    // from the near band edges, where a row is finest.
    {
        constexpr double half_period=128.;
        const double eye=double(uint16_t(camera_z));
        double sum_cos=0,sum_sin=0;
        for(std::size_t i=0;i+1<floor_bands.size();++i) {
            const double edge=-double(floor_bands[i].z1); // Distance of the far band's near edge.
            if(edge>900. || edge<100.) continue;
            const double angle=2*std::numbers::pi*std::fmod(eye+edge,half_period)/half_period;
            const double weight=1./edge;
            sum_cos+=std::cos(angle)*weight;sum_sin+=std::sin(angle)*weight;
        }
        std::vector<int16_t> seen;
        for(const auto& band:floor_bands) if(std::find(seen.begin(),seen.end(),band.page)==seen.end()) seen.push_back(band.page);
        if(std::hypot(sum_cos,sum_sin)>0 && seen.size()==2) {
            double phase=std::atan2(sum_sin,sum_cos)/(2*std::numbers::pi)*half_period;
            if(phase<0) phase+=half_period;
            // Which page holds which parity: the nearest full run on screen.
            const auto& nearest=floor_bands.size()>1?floor_bands[floor_bands.size()-2]:floor_bands.back();
            const double middle=eye-.5*(double(nearest.z0)+double(nearest.z1));
            const auto parity=[&](double world){return int64_t(std::floor((world-phase)/half_period))&1;};
            const int16_t near_page=nearest.page,other_page=seen[0]==near_page?seen[1]:seen[0];
            const int64_t near_parity=parity(middle);
            // The wider floor carries on to the horizon past the last band.
            const int16_t far_page=look(seen[0]).floor_left<look(seen[1]).floor_left?seen[0]:seen[1];
            constexpr double reach=24576.;
            std::vector<Band> world;
            double edge=std::floor((eye-behind-phase)/half_period)*half_period+phase;
            while(edge<eye+reach) {
                const double next=edge+half_period;
                const auto page=parity(edge+1.)==near_parity?near_page:other_page;
                // Model Z is the eye's world Z minus the world Z.
                world.push_back({float(-(next-eye)),float(-(edge-eye)),page});
                edge=next;
            }
            world.push_back({far,float(-(edge-eye)),far_page});
            floor_bands=world;ceiling_bands=std::move(world);
        }
    }
    // The art fades the road into a dark haze at its vanishing point, which
    // hides the set pieces the cartridge spawns about 3,800 units ahead (the
    // exit gate at the end of Venom 2's road). Real geometry runs on to the
    // horizon, so the haze returns as distance: bands darken into the art's
    // own colour at the vanishing point, and ModernFrame fogs the models with
    // it over the same distances (colony_corridor_haze).
    const auto haze=[&] {
        // The darkest of the centre column within 24 rows of the vanishing
        // point, where the art's haze is deepest.
        std::array<float,4> darkest{};float least=1e9F;
        for(int offset=1;offset<=24;++offset) for(const int row:{int(std::floor(vp))-offset,int(std::ceil(vp))+offset}) {
            if(row<0 || row>223) continue;
            const auto c=colour(art(page_of(row),row,128));
            const float luma=.2126F*c[0]+.7152F*c[1]+.0722F*c[2];
            if(luma<least) {least=luma;darkest=c;}
        }
        return darkest;
    }();
    const auto hazed=[&](unsigned index,float z0,float z1) {
        // By the band's near edge; the band to the horizon is all haze.
        const double distance=z0<=far?1e9:-double(z1);
        const double t=std::clamp((distance-colony_haze_start)/(colony_haze_end-colony_haze_start),0.,1.);
        const float amount=float(t*t*(3.-2.*t));
        auto c=colour(index);
        for(int k=0;k<3;++k) c[size_t(k)]+=(haze[size_t(k)]-c[size_t(k)])*amount;
        return c;
    };
    const auto quad=[&](float x0,float x1,float y,float z0,float z1,unsigned index) {
        push_flat(packet,std::array<std::array<float,3>,4>{{{x0,y,z1},{x1,y,z1},{x1,y,z0},{x0,y,z0}}},hazed(index,z0,z1));
    };
    constexpr float side=8388608.F;
    for(const auto& band:ceiling_bands) {
        const auto& page=look(band.page);
        quad(float(-ceiling_half),float(ceiling_half),ceiling_y,band.z0,band.z1,page.ceiling);
    }
    for(const auto& band:floor_bands) {
        const auto& page=look(band.page);
        quad(float(page.floor_left),float(page.floor_right),floor_y,band.z0,band.z1,page.floor);
        if(page.line && page.line_half>.25) quad(float(-page.line_half),float(page.line_half),floor_y,band.z0,band.z1,page.line);
        // The right wall over this band, from the floor's edge to the ceiling's.
        if(page.wall) push_flat(packet,std::array<std::array<float,3>,4>{{{float(page.floor_right),floor_y,band.z1},
            {float(ceiling_half),ceiling_y,band.z1},{float(ceiling_half),ceiling_y,band.z0},{float(page.floor_right),floor_y,band.z0}}},
            hazed(page.wall,band.z0,band.z1));
    }
    packet.geometry.texels.assign(16,0);
    packet.geometry.texels[7]=colony_marker;
    // Words 8-11: the haze colour (target encoding) and a set flag.
    for(int k=0;k<3;++k) packet.geometry.texels[size_t(8+k)]=starfox::bit_cast<uint32_t>(haze[size_t(k)]);
    packet.geometry.texels[11]=1U;
    packet.model=nucleus_room_model(0,(top+bottom)*.5,0,0);
    return packet;
}
std::optional<std::array<float,3>> colony_corridor_haze(const DrawPacket& packet) noexcept {
    if(!is_colony_corridor(packet)) return std::nullopt;
    const auto texels=packet.geometry.texel_view();
    if(texels[11]!=1U) return std::nullopt;
    return std::array<float,3>{starfox::bit_cast<float>(texels[8]),starfox::bit_cast<float>(texels[9]),
        starfox::bit_cast<float>(texels[10])};
}
bool is_colony_corridor(const DrawPacket& packet) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()==16 && texels[7]==colony_marker && !packet.geometry.vertex_view().empty();
}
bool is_water_bridge(const DrawPacket& packet) noexcept {
    const auto texels=packet.geometry.texel_view();
    return texels.size()==16 && texels[7]==water_bridge_marker && !packet.geometry.vertex_view().empty();
}
Matrix4 water_height_motion(float previous_height,float current_height,double alpha) {
    if(!std::isfinite(previous_height) || !std::isfinite(current_height)
        || previous_height<.01F || current_height<.01F || previous_height>8.F || current_height>8.F || !std::isfinite(alpha))
        throw std::invalid_argument("Invalid water height motion");
    const auto height=previous_height+(current_height-previous_height)*std::clamp(alpha,0.,1.);
    Matrix4 motion{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    motion[5]=float(height/current_height);
    return motion;
}
DrawPacket space_horizon_sphere_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,bool srgb) {
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=sky_vertices(srgb,false);
    // Keep source scroll/offset tables and unique-planet policy. Only map the
    // native vertical band onto distant geometry, with space beyond its ends.
    packet.geometry.texels[15]|=options.single_occurrence_top_rows==512?2U:16U;
    return packet;
}
Matrix4 orbital_horizon_motion(float previous_scroll,float current_scroll,double alpha,bool thin,bool entry,bool gameplay) {
    if(!std::isfinite(previous_scroll) || !std::isfinite(current_scroll) || !std::isfinite(alpha))
        throw std::invalid_argument("Invalid orbital horizon motion");
    const double scroll=std::lerp(double(previous_scroll),double(current_scroll),std::clamp(alpha,0.,1.));
    const double horizon=entry?424.:thin?400.:384.;
    const float pitch=float(-std::atan((horizon-scroll-112.)/512.));
    const float c=std::cos(pitch),s=std::sin(pitch);
    // EX's entry/boss horizon needs a clockwise quarter-turn; keep this
    // correction on the planet layer, never on the eye camera or UI.
    if(gameplay && (entry || thin)) return {0,-1,0,0,c,0,s,0,-s,0,c,0,0,0,0,1};
    return {1,0,0,0,0,c,s,0,0,-s,c,0,0,0,0,1};
}
void flat_orbital_surface(DrawPacket& bg2,const simulation::SnesPpuState& source,bool thin_horizon,bool entry_horizon) {
    auto& data=bg2.geometry.texels;
    if(data.size()<272+16384 || bg2.geometry.vertex_view().empty()) return;
    const unsigned horizon=entry_horizon?424U:thin_horizon?400U:384U;
    const unsigned depth=entry_horizon?40U:thin_horizon?15U:80U;
    // The surface rows' inks, probed where the headset sphere probes them
    // (scene.hlsl's vertex stage): atlas rows below the limb, unscrolled.
    auto ppu=source;
    ppu.bg2_horizontal_offsets_enabled=false;ppu.bg2_scanline_scroll_enabled=false;
    ppu.bg2_scroll_x=0;ppu.bg2_scroll_y=int16_t(horizon-100U);
    std::array<uint32_t,2> inks{};
    for(unsigned i=0;i<8;++i) {
        const unsigned row=100U+unsigned(float(depth)*(.3F+.25F*float(i%3)));
        const auto index=render::tunnel_border_index(ppu,row,29U+i*61U);
        // Ink 0 (transparent) falls back to the darkest colour; skip it.
        if(index%16U) inks[i/4]|=uint32_t(index)<<((i%4)*8);
    }
    data[7]|=65536U;
    data.push_back(starfox::bit_cast<uint32_t>(float(horizon)));
    data.push_back(starfox::bit_cast<uint32_t>(float(depth)));
    data.push_back(inks[0]);data.push_back(inks[1]);
}
DrawPacket orbital_planet_sphere_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,bool srgb,bool thin_horizon,bool entry_horizon,bool gameplay) {
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    const auto make=[](bool linear,bool thin,bool entry=false) {
        const float horizon=entry?424.F:thin?400.F:384.F;
        const float depth=entry?40.F:thin?15.F:80.F;
        auto result=std::make_shared<std::vector<SceneVertex>>(*sky_vertices(linear,false));
        for(auto& vertex:*result) {
            const float latitude=(112.F-vertex.uv[1])/512.F;
            // LSB uses rows 384..464; EX's late BG_5_1E uses only
            // rows 400..415. Below either surface, the atlas returns to space.
            // Match the surround's 512 texels/radian at the horizon. A
            // linear fit of this short strip to a hemisphere enlarged its
            // details by 5x (LSB) to 27x (thin EX horizon). Approach the last
            // surface row smoothly farther below; never sample space again.
            vertex.uv[1]=latitude>=0?std::max(0.F,horizon-latitude*512.F)
                :horizon-depth*std::expm1(latitude*512.F/depth);
            // Longitude is undefined at the nadir. Transition to a Cartesian
            // cap before reaching it, keeping the source horizon unchanged.
            const float cap=std::clamp((-latitude-.35F)/.55F,0.F,1.F);
            const float blend=cap*cap*(3.F-2.F*cap);
            // Cartesian cap uses equal texel density on X/Z. The previous
            // fixed X scale stretched surface details by 6.4x..34x along Z.
            vertex.uv[0]=std::lerp(vertex.uv[0],128.F+vertex.position[0]*(depth/128.F),blend);
            vertex.uv[1]=std::lerp(vertex.uv[1],horizon+depth*(.5F+vertex.position[2]/128.F),blend);
        }
        return result;
    };
    static const auto normal=make(false,false),linear=make(true,false);
    static const auto thin_normal=make(false,true),thin_linear=make(true,true);
    static const auto entry_normal=make(false,false,true),entry_linear=make(true,false,true);
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=entry_horizon?(srgb?entry_linear:entry_normal):thin_horizon?(srgb?thin_linear:thin_normal):(srgb?linear:normal);
    // Preserve the source horizon's angular height while extending the surface.
    // The atlas mapping places its boundary at latitude zero before this pitch.
    const float scroll=options.scroll_override?float((*options.scroll_override)[1]):float(ppu.bg2_scroll_y);
    packet.model=orbital_horizon_motion(scroll,scroll,1.,thin_horizon,entry_horizon,gameplay);
    auto& data=packet.geometry.texels;
    data[3]=data[4]=data[9]=data[10]=data[11]=data[12]=data[14]=data[15]=0;
    data[15]=0x80000000U|(entry_horizon?0x8000000U:thin_horizon?0x40000000U:0U);
    data.resize(272+16384);
    return packet;
}
Matrix4 landscape_pitch_motion(uint16_t previous_pitch,uint16_t current_pitch,double alpha) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid landscape pitch");
    int delta=int(current_pitch)-int(previous_pitch);
    if(delta>32767) delta-=65536;else if(delta< -32768) delta+=65536;
    const double pitch=double(previous_pitch)+delta*std::clamp(alpha,0.,1.);
    const float angle=float(-pitch*2*std::numbers::pi/65536.);
    const float c=std::cos(angle),s=std::sin(angle);
    return {1,0,0,0,0,c,s,0,0,-s,c,0,0,0,0,1};
}
Matrix4 landscape_scroll_motion(int16_t previous_scroll,int16_t current_scroll,double alpha,
    uint16_t previous_bank,uint16_t current_bank,uint16_t atlas_origin) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid landscape scroll");
    // Native vertical scroll simulates looking up/down on a flat screen.
    // It must not pitch the surrounding VR world when the player steers.
    (void)previous_scroll;(void)current_scroll;(void)atlas_origin;
    constexpr float c=1.F,s=0.F;
    int delta=int(current_bank)-int(previous_bank);
    if(delta>32767) delta-=65536;else if(delta< -32768) delta+=65536;
    const double bank=(previous_bank+delta*std::clamp(alpha,0.,1.))*2*std::numbers::pi/65536.;
    const float bc=float(std::cos(bank)),bs=float(std::sin(bank));
    return {bc,bs,0,0,-bs*c,bc*c,s,0,bs*s,-bc*s,c,0,0,0,0,1};
}
std::array<float,4> source_background_border_colour(const std::array<uint16_t,256>& palette,
    unsigned brightness,bool srgb) {
    // Match the GPU unique-background border selection, not CGRAM zero:
    // that slot can be a model colour (pink in the source intro).
    uint16_t selected=palette.front();unsigned darkest=~0U;
    for(auto colour:palette) {
        const unsigned luma=77U*(colour&31)+150U*((colour>>5)&31)+29U*((colour>>10)&31);
        if(luma<darkest) {darkest=luma;selected=colour;}
        if(luma==0) break;
    }
    return source_backdrop_colour(selected,brightness,srgb);
}
std::array<float,4> source_backdrop_colour(uint16_t bgr555,unsigned brightness,bool srgb) {
    if(brightness>15) throw std::invalid_argument("Invalid backdrop brightness");
    std::array<float,4> result{0,0,0,1};
    for(unsigned channel=0;channel<3;++channel) {
        const unsigned component=(bgr555>>(channel*5))&31;
        const unsigned expanded=(component<<3)|(component>>2);
        const float faded=float(expanded*brightness/15)/255.F;
        result[channel]=!srgb?faded:faded<=.04045F?faded/12.92F:
            std::pow((faded+.055F)/1.055F,2.4F);
    }
    return result;
}
std::vector<SceneVertex> game_over_foreground_vertices(const simulation::SnesPpuState& ppu,bool srgb) {
    constexpr unsigned width=256,height=224;
    render::Framebuffer image(width,height);
    render::BackgroundRenderer{}.draw_bg2(ppu,ppu.bg2_scroll_x,ppu.bg2_scroll_y,image);
    std::vector<uint8_t> exterior(width*height);
    std::vector<unsigned> queue;queue.reserve(width*height);
    const auto visit=[&](unsigned i) {
        if(!exterior[i] && (ppu.cgram[image.pixels()[i]]&32767U)==0) {
            exterior[i]=1;queue.push_back(i);
        }
    };
    for(unsigned x=0;x<width;++x) {visit(x);visit((height-1)*width+x);}
    for(unsigned y=0;y<height;++y) {visit(y*width);visit(y*width+width-1);}
    for(size_t cursor=0;cursor<queue.size();++cursor) {
        const auto i=queue[cursor],x=i%width,y=i/width;
        if(x) visit(i-1);if(x+1<width) visit(i+1);
        if(y) visit(i-width);if(y+1<height) visit(i+width);
    }
    // Horizontal runs avoid a quad per source pixel. All colour/fading still
    // comes from the original indexed GPU layer, not a baked RGBA replacement.
    std::vector<SceneVertex> vertices;
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;) {
        if(exterior[y*width+x]) {++x;continue;}
        const unsigned left=x;
        while(x<width && !exterior[y*width+x]) ++x;
        const float corners[4][2]{{float(left),float(y)},{float(x),float(y)},
            {float(x),float(y+1)},{float(left),float(y+1)}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex vertex{};
            vertex.position[0]=vertex.uv[0]=corners[corner][0];
            vertex.position[1]=vertex.uv[1]=corners[corner][1];
            vertex.texture[3]=8|(srgb?2:0);vertices.push_back(vertex);
        }
    }
    return vertices;
}
DrawPacket background_tile_packet(const simulation::SnesPpuState& ppu,
    BackgroundLayer layer,const BackgroundTileOptions& options,bool srgb) {
    unsigned enable=0;
    switch(layer) {
    case BackgroundLayer::bg1:enable=1;break;
    case BackgroundLayer::bg2:enable=2;break;
    case BackgroundLayer::bg3:enable=4;break;
    default:throw std::invalid_argument("Invalid background layer");
    }
    if(options.priority>2 || options.brightness>15 || options.colour_subtract>31 || options.guard_inset>128
        || (options.single_occurrence_top_rows>224 && options.single_occurrence_top_rows!=512))
        throw std::invalid_argument("Invalid background packet options");
    const auto bounds=options.horizontal_bounds;
    if(bounds[0]<-4096 || bounds[1]>4096 || bounds[1]<bounds[0])
        throw std::invalid_argument("Invalid background horizontal bounds");
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float left=float(options.guard_inset?std::max(bounds[0],int32_t(options.guard_inset)):bounds[0]);
    const float right=float(options.guard_inset?std::min(bounds[1],256-int32_t(options.guard_inset)):bounds[1]);
    if(!(ppu.main_screen&enable) || right<=left) return packet;
    packet.geometry.texels=background_tile_payload(ppu,layer,options);
    const float over=float(std::min(options.overscan,1024U));
    const float corners[4][2]{{left-over,-over},{right+over,-over},{right+over,224+over},{left-over,224+over}};
    if(options.overscan) packet.geometry.texels[7]|=8192U; // Clamp rows (scene.hlsl tile_background).
    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
        SceneVertex vertex{};
        vertex.position[0]=vertex.uv[0]=corners[corner][0];
        vertex.position[1]=vertex.uv[1]=corners[corner][1];
        vertex.texture[3]=8|(srgb?2:0);
        packet.geometry.vertices.push_back(vertex);
    }
    return packet;
}
std::vector<DrawPacket> title_foreground_packets(const simulation::SnesPpuState& ppu,
    unsigned brightness,bool include_bg1,
    std::optional<std::array<int16_t,2>> bg2_scroll,bool srgb) {
    std::vector<DrawPacket> packets;
    if(ppu.background_mode!=1) return packets;
    BackgroundTileOptions options;
    options.brightness=brightness;options.priority=2;options.scroll_override=bg2_scroll;
    packets.push_back(background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb));
    options.scroll_override.reset();
    if(include_bg1) {
        options.priority=0;
        packets.push_back(background_tile_packet(ppu,BackgroundLayer::bg1,options,srgb));
    }
    options.priority=2;
    packets.push_back(background_tile_packet(ppu,BackgroundLayer::bg3,options,srgb));
    return packets;
}
std::vector<uint32_t> background_tile_payload(const simulation::SnesPpuState& ppu,
    BackgroundLayer layer,const BackgroundTileOptions& options) {
    if(options.priority>2) throw std::invalid_argument("Invalid background priority");
    if(options.brightness>15) throw std::invalid_argument("Invalid background brightness");
    if(options.colour_subtract>31) throw std::invalid_argument("Invalid background palette subtraction");
    if(options.guard_inset>128) throw std::invalid_argument("Invalid background guard inset");
    if(options.single_occurrence_top_rows>224 && options.single_occurrence_top_rows!=512)
        throw std::invalid_argument("Invalid unique background row count");
    if(ppu.background_mode<1 || ppu.background_mode>3
        || (layer==BackgroundLayer::bg3 && ppu.background_mode!=1))
        throw std::invalid_argument("Unsupported GPU background mode/layer");
    const bool bg2=layer==BackgroundLayer::bg2;
    unsigned scanlines=bg2?unsigned(ppu.bg2_horizontal_offsets_enabled)
        +2U*unsigned(ppu.bg2_scanline_scroll_enabled):0;
    const bool presentation=options.scroll_fraction || options.mode2_horizon;
    const std::size_t tail=272+16384+(scanlines?448:0);
    std::vector<uint32_t> data(tail+(presentation?5:0));
    if(presentation) {
        const auto fraction=options.scroll_fraction.value_or(std::array<float,2>{});
        data[tail]=starfox::bit_cast<uint32_t>(fraction[0]);data[tail+1]=starfox::bit_cast<uint32_t>(fraction[1]);
        if(options.mode2_horizon) {
            data[tail+2]=starfox::bit_cast<uint32_t>((*options.mode2_horizon)[0]);
            data[tail+3]=starfox::bit_cast<uint32_t>((*options.mode2_horizon)[1]);
            data[tail+4]=1;
        }
    }
    unsigned mosaic_bit=0;
    switch(layer) {
    case BackgroundLayer::bg1:
        data[0]=ppu.bg1_character_base;data[1]=ppu.bg1_screen_base;data[2]=ppu.bg1_screen_size;
        data[3]=uint32_t(int32_t(ppu.bg1_scroll_x));data[4]=uint32_t(int32_t(ppu.bg1_scroll_y));
        data[5]=ppu.background_mode==3?8:4;data[8]=ppu.bg1_tile_size_16;mosaic_bit=1;break;
    case BackgroundLayer::bg2:
        data[0]=ppu.bg2_character_base;data[1]=ppu.bg2_screen_base;data[2]=ppu.bg2_screen_size;
        data[3]=uint32_t(int32_t(ppu.bg2_scroll_x));data[4]=uint32_t(int32_t(ppu.bg2_scroll_y));
        data[5]=4;data[8]=ppu.bg2_tile_size_16;mosaic_bit=2;break;
    case BackgroundLayer::bg3:
        data[0]=ppu.bg3_character_base;data[1]=ppu.bg3_screen_base;data[2]=ppu.bg3_screen_size;
        data[3]=uint32_t(int32_t(ppu.bg3_scroll_x));data[4]=uint32_t(int32_t(ppu.bg3_scroll_y));
        data[5]=2;data[8]=ppu.bg3_tile_size_16;mosaic_bit=4;break;
    default: throw std::invalid_argument("Invalid background layer");
    }
    if(options.scroll_override) {
        data[3]=uint32_t(int32_t((*options.scroll_override)[0]));
        data[4]=uint32_t(int32_t((*options.scroll_override)[1]));
    }
    data[7]=options.priority|(bg2 && options.expanded_horizontal && options.ex_face_planets?256U:0U)
        |(presentation?1024U:0U);
    data[9]=(ppu.mosaic&mosaic_bit)?(ppu.mosaic>>4)+1:0;
    data[13]=15-options.brightness;
    data[15]=unsigned(options.transparent_black)|(options.wrap_horizontal?0U:2U)
        |(bg2 && options.expanded_horizontal && options.ex_twin_planets?4U:0U)
        |(bg2 && options.expanded_horizontal && options.ex_ocean_island?0x1000000U:0U)
        |(bg2 && options.expanded_horizontal && options.ex_volcanic_horizon?0x2000000U:0U)
        |(bg2 && options.expanded_horizontal && options.ex_city_planets?0x4000000U:0U)
        |(bg2?options.single_occurrence_top_rows<<8:0U);
    data[14]=bg2 && ppu.tunnel_scene && options.expanded_horizontal
        ?(unsigned(render::tunnel_wall_index(ppu))+1U)|(options.clamp_tunnel_edges?0x80000000U:0U):0U;
    data[10]=scanlines;
    data[11]=bg2 && ppu.background_mode==1 && ppu.bg2_scanline_scroll_enabled
        && !ppu.tunnel_scene && options.expanded_horizontal;
    data[12]=bg2 && ppu.background_mode==2 && ppu.bg2_vertical_offsets_enabled
        ?(options.expanded_horizontal?2:1):0;
    if(scanlines) for(unsigned row=0;row<224;++row) {
        data[272+16384+row]=uint32_t(int32_t(ppu.bg2_horizontal_offsets[row]));
        data[272+16384+224+row]=uint32_t(int32_t(ppu.bg2_scanline_scroll_y[row]));
    }
    auto colours=ppu.cgram;
    if(bg2 && options.colour_subtract) for(auto& colour:colours) {
        uint16_t dark=0;
        for(unsigned c=0;c<3;++c) dark|=uint16_t(std::max(0,int((colour>>(c*5))&31)-int(options.colour_subtract))<<(c*5));
        colour=dark;
    }
    const auto palette=render::decode_bgr555_palette(colours);
    for(unsigned i=0;i<256;++i) data[16+i]=uint32_t(palette[i].r)
        |(uint32_t(palette[i].g)<<8)|(uint32_t(palette[i].b)<<16)|0xff000000U;
    pack_vram(ppu.vram,std::span<uint32_t,16384>(data.data()+272,16384));
    return data;
}
}
