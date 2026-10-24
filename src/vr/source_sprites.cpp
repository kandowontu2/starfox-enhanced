#include "starfox/vr/source_sprites.hpp"
#include "starfox/vr/packed_vram.hpp"
#include "starfox/vr/game_scene.hpp"
#include "starfox/vr/presentation.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/render/hud_layout.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/math.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
namespace starfox::vr {
std::vector<DrawPacket> layout_a_instrument_packets(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    const GameSceneSnapshot& scene,render::ScaledTextRenderer& text,bool srgb) {
    const auto layout=layout_a_hud(scene.meters.extended);
    std::vector<DrawPacket> packets;
    // Source portraits, glyph shadows and meters provide their own framing.
    // Leave unoccupied gameplay HUD pixels open to the world.
    auto oam=source_sprite_packet(*scene.ppu,scene.display_brightness,{},srgb,&scene.meters,&layout,SourceSpritePass::hud);
    oam.model=overlay_panel_matrix();packets.push_back(std::move(oam));
    auto meters=source_meter_packet(scene.meters,scene.ppu->cgram,scene.display_brightness,srgb,256,false,&layout);
    meters.model=overlay_panel_matrix();
    // Inner FX meter origin is (16,16) in the full PPU canvas. Offsets above
    // move each group identically after that authored composition.
    meters.model[12]+=16*meters.model[0];meters.model[13]+=16*meters.model[5];
    packets.push_back(std::move(meters));
    if(replace_native_dialogue(scene)) {
        auto dialogue=source_dialogue_packets(rom,symbols,scene.dialogue,text,scene.ppu->cgram,scene.display_brightness,srgb);
        for(auto& packet:dialogue) {
            packet.model=overlay_panel_matrix();
            packet.model[12]+=layout[render::HudElement::comms].x*packet.model[0];
            packet.model[13]+=layout[render::HudElement::comms].y*packet.model[5];
            packets.push_back(std::move(packet));
        }
    }
    return packets;
}
render::HudLayout layout_a_hud(bool extended) {
    render::HudLayout layout;
    layout[render::HudElement::lives]={int16_t(extended?-21:-10),int16_t(extended?15:173)};
    layout[render::HudElement::shield]={int16_t(extended?22:15),int16_t(extended?4:7)};
    layout[render::HudElement::bombs_boost]={int16_t(extended?-37:-104),8};
    layout[render::HudElement::comms]={-35,-12};
    return layout;
}
DrawPacket layout_a_surface(bool menu,unsigned selection,bool srgb,unsigned menu_rows) {
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const auto rect=[&](float x,float y,float w,float h,uint32_t rgb,float alpha=1.F) {
        std::array<float,4> colour{float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255,alpha};
        if(srgb) for(unsigned i=0;i<3;++i) colour[i]=colour[i]<=.04045F?colour[i]/12.92F:std::pow((colour[i]+.055F)/1.055F,2.4F);
        for(unsigned c:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex v{};v.position[0]=x+((c==1 || c==2)?w:0);v.position[1]=y+(c>=2?h:0);
            std::copy(colour.begin(),colour.end(),v.color);packet.geometry.vertices.push_back(v);
        }
    };
    menu_rows=std::clamp(menu_rows,1U,6U);
    const float footer=67.F+18.F*menu_rows;
    const float top=menu?(menu_rows<=2?24.F:1.F):136.F,bottom=menu?footer+48.F:214.F;
    rect(1,top,254,bottom-top,0x071017,.95F);
    rect(1,top,254,1,0x8b9ca9);rect(1,bottom-1,254,1,0x8b9ca9);
    rect(1,top,1,bottom-top,0x8b9ca9);rect(254,top,1,bottom-top,0x8b9ca9);
    if(menu) {
        rect(12,55,232,1,0x3d505f);rect(12,footer,232,1,0x3d505f);
        const float y=65.F+18.F*std::min(selection,menu_rows-1);
        rect(10,y,236,16,0x283326);rect(10,y,2,16,0xffff69);
    }
    return packet;
}

std::vector<DrawPacket> source_dialogue_packets(const assets::RomImage& rom,
    const assets::SymbolMap& symbols,const simulation::DialogueState& dialogue,
    render::ScaledTextRenderer& layout,const std::array<uint16_t,256>& cgram,
    unsigned brightness,bool srgb,const render::TextureOverrides* portraits) {
    std::vector<DrawPacket> packets;
    if(!dialogue.active) return packets;
    packets.push_back(source_portrait_packet(rom,symbols,dialogue.portrait_frame,
        dialogue.alternate_portraits,cgram,brightness,srgb,portraits));
    if(dialogue.text_visible && (dialogue.text_address&0xffffU)>=0x8000U) {
        const auto translated=layout.translated_game_text_lines(dialogue.text_address,92);
        int y=dialogue.three_lines?153:169;
        if(!translated.empty()) y=std::min(y,183-10*(int(translated.size())-1));
        for(unsigned pass=0;pass<2;++pass) {
            const auto ink=pass?uint8_t(112+(rom.read8(dialogue.text_address)&15U)):uint8_t(121);
            if(!translated.empty()) {
                int line_y=y+(pass?0:1);
                for(const auto line:translated) {
                    packets.push_back(source_unicode_ui_text_packet(rom,symbols,line,
                        pass?82:83,line_y,ink,cgram,brightness,srgb));
                    line_y+=10;
                }
            } else {
                packets.push_back(source_game_text_packet(rom,symbols,dialogue.text_address,
                    pass?82:83,y+(pass?0:1),174+(pass?0:1),256,ink,cgram,brightness,srgb));
            }
        }
    }
    packets.push_back(source_dialogue_meter_packet(dialogue,cgram,brightness,srgb));
    return packets;
}
DrawPacket source_dialogue_meter_packet(const simulation::DialogueState& dialogue,
    const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb) {
    if(brightness>15) throw std::invalid_argument("Invalid dialogue meter brightness");
    DrawPacket packet;
    if(!dialogue.active || !dialogue.meter_visible) return packet;
    const auto rectangle=[&](int left,int top,int right,int bottom,unsigned index) {
        if(right<=left) return;
        const auto colour=source_backdrop_colour(cgram[index],brightness,srgb);
        const int corners[4][2]{{left,top},{right,top},{right,bottom},{left,bottom}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex v{};v.position[0]=float(corners[corner][0]);v.position[1]=float(corners[corner][1]);
            std::copy(colour.begin(),colour.end(),v.color);packet.geometry.vertices.push_back(v);
        }
    };
    rectangle(82,177,126,178,126);rectangle(82,188,126,189,126);
    rectangle(82,178,83,188,126);rectangle(125,178,126,188,126);
    // The source frame wins over the fill at x=125, even for corrupt health.
    rectangle(84,179,84+std::min<unsigned>(dialogue.meter_health,41),187,114);
    return packet;
}
uint32_t source_portrait_address(const assets::SymbolMap& symbols,uint8_t frame,bool alternate) {
    const auto address=[&](const char* name) {
        for(auto value:symbols.find(name))
            if((value&0xffffU)>=0x8000U && ((value>>16)&255U)<0x70U) return value;
        return uint32_t{};
    };
    auto base=alternate?address("FACEDATA2"):0U;
    if(!base) base=address("FACEDATA");
    if(!base) throw std::runtime_error("Missing portrait data");
    return base+uint32_t(frame)*640;
}
DrawPacket source_portrait_packet(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    uint8_t frame,bool alternate,const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb,
    const render::TextureOverrides* portraits,std::optional<std::array<int,2>> at) {
    const auto address=source_portrait_address(symbols,frame,alternate);
    // The same 32x40 placement as the native portrait below, corrected for
    // SNES pixel aspect (or square at `at`, as the tally draws them).
    const auto corner_vertex=[at](unsigned corner) {
        const float u=(corner==1 || corner==2)?32.F:0.F,v=corner>=2?40.F:0.F;
        SceneVertex vertex{};
        if(at) {vertex.position[0]=float((*at)[0])+u;vertex.position[1]=float((*at)[1])+v;}
        else {vertex.position[0]=80.F-(32.F-u)*7.F/6.F;vertex.position[1]=152.F+v;}
        vertex.uv[0]=u;vertex.uv[1]=v;
        return vertex;
    };
    if(portraits) {
        const auto found=portraits->find(address);
        if(found!=portraits->end() && found->second.width && found->second.height
            && found->second.rgba.size()==std::size_t(found->second.width)*found->second.height) {
            // Replacement art through the shader's resident-texture
            // replacement path: a material header (masks at 9/10, image
            // offset+1 at 12, packed size at 13) followed by RGBA words.
            // The screen fade is baked in, as the native palette is.
            const auto& image=found->second;
            constexpr uint32_t header=14;
            DrawPacket packet;
            auto& texels=packet.geometry.texels;
            texels.assign(header,0U);
            texels[9]=31;texels[10]=39;texels[12]=header+1;texels[13]=image.width|(image.height<<16);
            texels.reserve(header+image.rgba.size());
            const auto level=std::min(brightness,15U);
            for(const auto pixel:image.rgba) {
                uint32_t faded=pixel&0xff000000U;
                for(unsigned shift:{0U,8U,16U}) faded|=((((pixel>>shift)&255U)*level)/15U)<<shift;
                texels.push_back(faded);
            }
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                auto vertex=corner_vertex(corner);
                vertex.texture[3]=8388608U|4U|(srgb?2U:0U);
                packet.geometry.vertices.push_back(vertex);
            }
            return packet;
        }
    }
    // Keep the authored planar tiles packed: the existing background shader
    // decodes their four bitplanes and palette, with no CPU pixel rasterization.
    simulation::SnesPpuState ppu{};
    ppu.main_screen=1;ppu.background_mode=1;ppu.bg1_screen_base=512;
    ppu.cgram=cgram;
    for(unsigned i=0;i<640;++i) ppu.vram[i]=rom.read8(address+i);
    for(unsigned x=0;x<4;++x) for(unsigned y=0;y<5;++y) {
        const unsigned offset=1024+(y*32+x)*2;
        const unsigned tile=x*5+y;
        ppu.vram[offset]=uint8_t(tile);ppu.vram[offset+1]=28; // Palette seven.
    }
    BackgroundTileOptions options;options.brightness=brightness;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg1,options,srgb);
    packet.geometry.vertices.clear();
    for(bool ink:{false,true}) for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
        // Correct SNES pixel aspect without moving the right edge into text.
        auto vertex=corner_vertex(corner);
        // Tile index zero is transparent in the shared shader. Its native
        // portrait meaning is palette 112, supplied by this backing quad.
        vertex.texture[1]=ink?0U:112U;vertex.texture[3]=(ink?8U:32U)|(srgb?2U:0U);
        packet.geometry.vertices.push_back(vertex);
    }
    return packet;
}
DrawPacket source_indexed_ink_packet(const render::Framebuffer& ink,
    const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb) {
    if(brightness>15) throw std::invalid_argument("Invalid ink brightness");
    DrawPacket packet;
    const auto palette=render::decode_bgr555_palette(cgram);
    // One 16x16 one-bit mask per colour per block, as the UI text quads.
    for(unsigned top=0;top<ink.height();top+=16) for(unsigned left=0;left<ink.width();left+=16) {
        std::array<bool,256> seen{};
        for(unsigned row=0;row<16 && top+row<ink.height();++row) for(unsigned column=0;column<16 && left+column<ink.width();++column)
            seen[ink.get(left+column,top+row)]=true;
        for(unsigned index=1;index<256;++index) {
            if(!seen[index]) continue;
            std::array<uint32_t,8> rows{};
            for(unsigned row=0;row<16 && top+row<ink.height();++row) for(unsigned column=0;column<16 && left+column<ink.width();++column)
                if(ink.get(left+column,top+row)==index) rows[row/2]|=(0x8000U>>column)<<((row&1)*16);
            const auto& colour=palette[index];
            const uint32_t rgba=uint32_t(colour.r*brightness/15)|(uint32_t(colour.g*brightness/15)<<8)
                |(uint32_t(colour.b*brightness/15)<<16)|0xff000000U;
            const auto offset=uint32_t(packet.geometry.texels.size());packet.geometry.texels.push_back(rgba);
            packet.geometry.texels.insert(packet.geometry.texels.end(),rows.begin(),rows.end());
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex v{};const float dx=(corner==1 || corner==2)?16.f:0.f,dy=corner>=2?16.f:0.f;
                v.position[0]=float(left)+dx;v.position[1]=float(top)+dy;v.uv[0]=dx;v.uv[1]=dy;
                v.texture[0]=offset;v.texture[1]=v.texture[2]=15;v.texture[3]=1024U|(srgb?2U:0U);
                packet.geometry.vertices.push_back(v);
            }
        }
    }
    return packet;
}
DrawPacket source_colour_math_packet(const simulation::ColourMathEffectState& effect,bool srgb) {
    DrawPacket packet;
    if(!effect.active || effect.affected_layers==0U) return packet;
    const auto channel=[srgb](std::uint8_t value) {
        const double v=double(((value&31U)<<3U)|((value&31U)>>2U))/255.;
        return float(srgb?(v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4)):v);
    };
    const float colour[4]{channel(effect.red),channel(effect.green),channel(effect.blue),1};
    const float corners[4][2]{{-1,-1},{1,-1},{1,1},{-1,1}};
    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
        SceneVertex vertex{};
        vertex.position[0]=corners[corner][0];vertex.position[1]=corners[corner][1];
        for(unsigned c=0;c<4;++c) vertex.color[c]=vertex.odd_color[c]=colour[c];
        packet.geometry.vertices.push_back(vertex);
    }
    return packet;
}
DrawPacket source_shutter_packet(const simulation::WindowWipeState& previous,
    const simulation::WindowWipeState& current,double alpha) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid shutter interpolation");
    DrawPacket packet;
    const auto wipe=simulation::interpolate_window_wipe(previous.active?previous:current,current,alpha);
    if(!wipe.active || !wipe.horizontal_opening) return packet;
    const auto rectangle=[&](float top,float bottom) {
        if(bottom<=top) return;
        const float corners[4][2]{{-1,top},{1,top},{1,bottom},{-1,bottom}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex vertex{};
            vertex.position[0]=corners[corner][0];vertex.position[1]=corners[corner][1];
            vertex.color[3]=1;vertex.odd_color[3]=1;
            packet.geometry.vertices.push_back(vertex);
        }
    };
    rectangle(-1.F,float(-1.+2.*std::clamp(wipe.opening_top,0.,192.)/192.));
    rectangle(float(-1.+2.*std::clamp(wipe.opening_bottom,0.,192.)/192.),1.F);
    return packet;
}
DrawPacket source_circle_packet(const simulation::CircleEffectState& previous,
    const simulation::CircleEffectState& current,double alpha,unsigned brightness,bool srgb) {
    if(!std::isfinite(alpha) || brightness>15) throw std::invalid_argument("Invalid circle presentation");
    DrawPacket packet;
    if(!current.active || !(current.affected_layers&0x3f)) return packet;
    alpha=std::clamp(alpha,0.,1.);
    const auto& from=previous.active?previous:current;
    const float radius=float(std::lerp(previous.active?double(previous.radius):0.,double(current.radius),alpha));
    if(radius<=0) return packet;
    const auto center=[alpha](int16_t a,int16_t b) {
        int delta=int(b)-int(a);if(delta>32767) delta-=65536;else if(delta< -32768) delta+=65536;
        return float(a+delta*alpha);
    };
    const float x=center(from.centre_x,current.centre_x),y=center(from.centre_y,current.centre_y);
    const auto channel=[alpha](uint8_t a,uint8_t b) {return uint16_t(std::lround(std::lerp(double(a&31),double(b&31),alpha)));};
    auto color=source_backdrop_colour(channel(from.red,current.red)|(channel(from.green,current.green)<<5)
        |(channel(from.blue,current.blue)<<10),brightness,srgb);
    color[3]=(current.affected_layers&0x40)? .5F:1.F;
    constexpr float corners[4][2]{{-1,-1},{1,-1},{1,1},{-1,1}};
    for(unsigned i:{0U,1U,2U,0U,2U,3U}) {
        SceneVertex v{};v.position[0]=x+corners[i][0]*radius;v.position[1]=y+corners[i][1]*radius;
        v.uv[0]=corners[i][0];v.uv[1]=corners[i][1];v.texture[3]=4096;
        std::copy(color.begin(),color.end(),v.color);packet.geometry.vertices.push_back(v);
    }
    return packet;
}
std::vector<DrawPacket> source_mode3_packets(const simulation::SnesPpuState& ppu,
    unsigned brightness,bool srgb,std::optional<std::array<int16_t,2>> bg2_scroll) {
    if(ppu.background_mode!=3) throw std::invalid_argument("Mode-3 compositor requires Mode 3");
    std::vector<DrawPacket> packets;
    for(unsigned priority:{1U,2U}) {
        BackgroundTileOptions options;options.brightness=brightness;options.priority=priority;
        options.scroll_override=bg2_scroll;
        packets.push_back(background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb));
        packets.push_back(source_sprite_packet(ppu,brightness,(priority-1)*2,srgb));
        options.scroll_override.reset();
        packets.push_back(background_tile_packet(ppu,BackgroundLayer::bg1,options,srgb));
        packets.push_back(source_sprite_packet(ppu,brightness,(priority-1)*2+1,srgb));
    }
    return packets;
}
DrawPacket source_game_text_packet(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    uint32_t address,int x,int y,int right_clip,size_t characters,uint8_t ink,
    const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb) {
    if(brightness>15) throw std::invalid_argument("Invalid text brightness");
    if((address&0xffffU)<0x8000U || !characters) return {};
    std::string text;
    for(size_t i=0;i<std::min<size_t>(characters,256);++i) {
        auto value=rom.read8(address+1+uint32_t(i));if(!value) break;text.push_back(char(value));
    }
    return source_ui_text_packet(rom,symbols,text,x,y,right_clip,ink,cgram,brightness,srgb);
}
DrawPacket source_unicode_ui_text_packet(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    std::u32string_view text,int x,int y,uint8_t ink,const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb) {
    if(brightness>15 || ink==0) throw std::invalid_argument("Invalid Unicode menu ink/brightness");
    render::Framebuffer bitmap(256,224);
    render::ScaledTextRenderer font(rom,symbols);
    font.draw_unicode(text.substr(0,256),x,y,bitmap,ink,0);
    DrawPacket packet;
    const auto colour=render::decode_bgr555_palette(cgram)[ink];
    const uint32_t rgba=uint32_t(colour.r*brightness/15)|(uint32_t(colour.g*brightness/15)<<8)
        |(uint32_t(colour.b*brightness/15)<<16)|0xff000000U;
    for(unsigned top=0;top<224;top+=16) for(unsigned left=0;left<256;left+=16) {
        std::array<uint32_t,8> rows{};bool visible=false;
        for(unsigned row=0;row<16;++row) for(unsigned column=0;column<16;++column)
            if(bitmap.pixels()[(top+row)*256+left+column]!=0) {
                rows[row/2]|=(0x8000U>>column)<<((row&1)*16);visible=true;
            }
        if(!visible) continue;
        const auto offset=uint32_t(packet.geometry.texels.size());packet.geometry.texels.push_back(rgba);
        packet.geometry.texels.insert(packet.geometry.texels.end(),rows.begin(),rows.end());
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex v{};const float dx=(corner==1 || corner==2)?16.f:0.f,dy=corner>=2?16.f:0.f;
            v.position[0]=float(left)+dx;v.position[1]=float(top)+dy;v.uv[0]=dx;v.uv[1]=dy;
            v.texture[0]=offset;v.texture[1]=v.texture[2]=15;v.texture[3]=1024U|(srgb?2U:0U);
            packet.geometry.vertices.push_back(v);
        }
    }
    return packet;
}
DrawPacket source_ui_text_packet(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    std::string_view text,int x,int y,int right_clip,uint8_t ink,
    const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb,bool frame_glyphs) {
    DrawPacket packet;
    if(brightness>15) throw std::invalid_argument("Invalid text brightness");
    text=text.substr(0,256);
    const auto symbol=[&](const char* name) {
        for(auto value:symbols.find(name))
            if((value&0xffffU)>=0x8000U && ((value>>16)&255)<0x70) return value;
        throw std::runtime_error(std::string("Missing text font: ")+name);
    };
    const auto widths=symbol("FONT0WID"),font=symbol("FONT0FON"),translation=symbol("FONT0TRN");
    const auto width=[&](uint8_t c)->unsigned {
        if(c==':' || c=='/' || c=='>' || (frame_glyphs && (c=='(' || c==')' || c=='+'))) return 5;
        return c==32?5:c<32?0:rom.read8(widths+rom.read8(translation+c-32));
    };
    const auto colour=render::decode_bgr555_palette(cgram)[ink];
    const uint32_t rgba=uint32_t(colour.r*brightness/15)
        |(uint32_t(colour.g*brightness/15)<<8)|(uint32_t(colour.b*brightness/15)<<16)|0xff000000U;
    size_t start=0;
    while(start<text.size() && y<224) {
        auto end=text.size(),next=text.size(),space=text.size();int total=0;
        for(size_t i=start;i<text.size();++i) {
            const int w=int(width(text[i]));if(text[i]==32) space=i;
            if(x+total+w>right_clip) {
                if(space!=text.size() && space>=start) {end=space;next=space+1;}
                else {end=i;next=i;}break;
            }
            total+=w;
        }
        int cursor=x;
        for(size_t i=start;i<end;++i) {
            const auto c=text[i];const unsigned w=width(c);
            if(c>32 && w) {
                if(w>16) throw std::runtime_error("Invalid source glyph width");
                const auto glyph=font+uint32_t(rom.read8(translation+c-32))*24;
                const auto offset=uint32_t(packet.geometry.texels.size());
                packet.geometry.texels.push_back(rgba);
                // The cartridge table aliases host punctuation to unrelated
                // glyphs. Supply actual menu punctuation without changing the
                // authored dialogue/font translation tables.
                const auto glyph_row=[&](unsigned row)->uint32_t {
                    if(row>=12) return 0;
                    if(c==':') return row==3 || row==4 || row==8 || row==9?0x6000U:0U;
                    if(c=='/') return 0x8000U>>(3-row*4/12);
                    if(frame_glyphs && c=='(') return row==1 || row==10?0x2000U:row==2 || row==9?0x4000U:row>=3 && row<=8?0x8000U:0U;
                    if(frame_glyphs && c==')') return row==1 || row==10?0x8000U:row==2 || row==9?0x4000U:row>=3 && row<=8?0x2000U:0U;
                    if(frame_glyphs && c=='+') return row==5?0xf800U:row>=2 && row<=9?0x2000U:0U;
                    if(c=='>') return row>=2 && row<=9?0x8000U>>(row<=5?row-2:9-row):0U;
                    return rom.read16(glyph+row*2);
                };
                for(unsigned row=0;row<16;row+=2)
                    packet.geometry.texels.push_back(glyph_row(row)|(glyph_row(row+1)<<16));
                for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                    SceneVertex v{};const float dx=(corner==1 || corner==2)?float(w):0;
                    const float dy=corner>=2?12.F:0;
                    v.position[0]=float(cursor)+dx;v.position[1]=float(y)+dy;
                    v.uv[0]=dx;v.uv[1]=dy;v.texture[0]=offset;
                    v.texture[1]=v.texture[2]=15;v.texture[3]=1024U|(srgb?2U:0U);
                    packet.geometry.vertices.push_back(v);
                }
            }
            cursor+=int(w);
        }
        if(next==text.size()) break;
        start=next<=start?start+1:next;y+=13;
    }
    return packet;
}
namespace {
struct ReticleObject {int x{},y{};uint8_t attributes{};uint16_t tile{};};
// The four reticle objects (x, y, attributes) when they use the retail art.
std::optional<std::array<ReticleObject,4>> reticle_objects(const simulation::SnesPpuState& ppu) {
    if(!(ppu.main_screen&16)) return {};
    std::array<ReticleObject,4> found{};std::size_t count=0;
    for(unsigned object=0;object<128;++object) {
        const auto low=object*4;
        const uint16_t tile=uint16_t(ppu.oam[low+2])|uint16_t((ppu.oam[low+3]&1U)<<8);
        if((tile&0x7fU)!=0x61U) continue;
        if(ppu.oam[low]==248 && ppu.oam[low+1]==248) continue;
        if(count==4) return {};
        const auto high=ppu.oam[512+object/4]>>((object%4)*2);
        if(high&2) return {}; // Large objects are not the retail 8x8 quarters.
        int x=ppu.oam[low]+((high&1)<<8);if(x>=256) x-=512;
        found[count++]={x,int(ppu.oam[low+1]),ppu.oam[low+3],tile};
        // Retail quarter art, rows top to bottom (bit 7 = left pixel).
        constexpr std::array<uint8_t,8> retail{0x3f,0x3f,0xff,0xf0,0xe8,0xe4,0xe2,0xe1};
        const unsigned select=ppu.object_select;
        unsigned table=(select&7U)*0x2000U;
        if(tile&256U) table+=(((select>>3)&3U)+1U)*0x1000U;
        std::array<uint8_t,8> stored{};
        for(unsigned row=0;row<8;++row) {
            const auto address=(table*2U+(tile&255U)*32U+row*2U)&0xffffU;
            stored[row]=ppu.vram[address]|ppu.vram[(address+1)&0xffffU]
                |ppu.vram[(address+16)&0xffffU]|ppu.vram[(address+17)&0xffffU];
        }
        const auto reverse=[](uint8_t v) {uint8_t r=0;for(unsigned b=0;b<8;++b) if(v&(1U<<b)) r|=uint8_t(128U>>b);return r;};
        bool matched=false;
        for(unsigned flips=0;flips<4 && !matched;++flips) {
            matched=true;
            for(unsigned row=0;row<8 && matched;++row) {
                auto value=stored[(flips&2)?7-row:row];
                if(flips&1) value=reverse(value);
                matched=value==retail[row];
            }
            // Fold the stored art's own orientation into the object flips.
            if(matched) found[count-1].attributes^=uint8_t(((flips&1)?64U:0U)|((flips&2)?128U:0U));
        }
        if(!matched) return {};
    }
    if(count!=4) return {};
    return found;
}
}
DrawPacket source_vector_reticle_packet(const simulation::SnesPpuState& previous,
    const simulation::SnesPpuState& current,double alpha,unsigned brightness,bool srgb) {
    if(brightness>15) throw std::invalid_argument("Invalid reticle brightness");
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const auto now=reticle_objects(current);
    if(!now) return packet;
    // Rigid group motion between ticks (the cartridge rebuilds it at 20 Hz).
    double dx=0,dy=0;
    if(const auto before=reticle_objects(previous); before && alpha>=0 && alpha<1) {
        const auto origin=[](const std::array<ReticleObject,4>& objects) {
            int x=objects[0].x,y=objects[0].y;
            for(const auto& object:objects) {x=std::min(x,object.x);y=std::min(y,object.y);}
            return std::array<int,2>{x,y};
        };
        const auto a=origin(*before),b=origin(*now);
        if(std::abs(b[0]-a[0])<64 && std::abs(b[1]-a[1])<64) {
            dx=(a[0]-b[0])*(1.-alpha);dy=(a[1]-b[1])*(1.-alpha);
        }
    }
    for(const auto& object:*now) {
        // Ink: the art is single-colour; use its first opaque pixel's index.
        const uint32_t ink=128U+((object.attributes>>1)&7U)*16U+[&] {
            const unsigned select=current.object_select;
            unsigned table=(select&7U)*0x2000U;
            if(object.tile&256U) table+=(((select>>3)&3U)+1U)*0x1000U;
            const unsigned address=(table*2U+(object.tile&255U)*32U+2U*2U)&0xffffU;
            for(unsigned bit=0;bit<8;++bit) {
                unsigned index=0;
                for(unsigned pair=0;pair<2;++pair) {
                    if(current.vram[(address+pair*16)&0xffffU]&(128U>>bit)) index|=1U<<(pair*2);
                    if(current.vram[(address+pair*16+1)&0xffffU]&(128U>>bit)) index|=2U<<(pair*2);
                }
                if(index) return index;
            }
            return 1U;
        }();
        const bool flip_x=(object.attributes&64)!=0,flip_y=(object.attributes&128)!=0;
        const auto emit=[&](double x,double y) {
            SceneVertex v{};
            v.position[0]=float(object.x+dx+(flip_x?8.-x:x));
            v.position[1]=float(object.y+dy+(flip_y?8.-y:y));
            v.texture[1]=ink;v.texture[3]=32|(srgb?2:0);packet.geometry.vertices.push_back(v);
        };
        const auto triangle=[&](std::array<double,2> a,std::array<double,2> b,std::array<double,2> c) {
            emit(a[0],a[1]);emit(b[0],b[1]);emit(c[0],c[1]);
        };
        const auto rectangle=[&](double x0,double y0,double x1,double y1) {
            triangle({x0,y0},{x1,y0},{x1,y1});triangle({x0,y0},{x1,y1},{x0,y1});
        };
        // Bracket (tile-local pixel edges): top arm, side arm, rounded outer
        // corner (radius 2 about 2,2) and the inner corner's fillet.
        rectangle(2,0,8,3);rectangle(0,2,3,8);
        constexpr double pi=3.14159265358979323846;
        for(unsigned step=0;step<8;++step) {
            const double a0=pi+step*(pi/2)/8,a1=pi+(step+1)*(pi/2)/8;
            triangle({2,2},{2+2*std::cos(a0),2+2*std::sin(a0)},{2+2*std::cos(a1),2+2*std::sin(a1)});
        }
        triangle({3,3},{4,3},{3,4});
        // One-pixel diagonal from pixel (4,4) to pixel (7,7).
        // Band |y-x| <= 0.707 (one pixel wide across the line) between the
        // first pixel's corner (x+y=8) and the last pixel's corner (x+y=16).
        const double h=.3535533905932738;
        triangle({4-h,4+h},{4+h,4-h},{8+h,8-h});
        triangle({4-h,4+h},{8+h,8-h},{8-h,8+h});
    }
    auto& data=packet.geometry.texels;data.resize(272);data[13]=15-brightness;
    const auto palette=render::decode_bgr555_palette(current.cgram);
    for(unsigned i=0;i<256;++i) data[16+i]=palette[i].r|(uint32_t(palette[i].g)<<8)|(uint32_t(palette[i].b)<<16)|0xff000000U;
    return packet;
}
DrawPacket source_cockpit_hud_packet(const simulation::TrigTables& trigonometry,double rotation,
    uint8_t colour,uint8_t damage_flags,const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb) {
    if(brightness>15 || !std::isfinite(rotation)) throw std::invalid_argument("Invalid cockpit HUD options");
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    rotation=std::fmod(rotation,256.);if(rotation<0) rotation+=256.;
    const auto angle=uint8_t(rotation);const double fraction=rotation-angle;
    const double sine=std::lerp(double(trigonometry.sin8(angle)),double(trigonometry.sin8(uint8_t(angle+1U))),fraction);
    const double cosine=std::lerp(double(trigonometry.cos8(angle)),double(trigonometry.cos8(uint8_t(angle+1U))),fraction);
    // MROTPNTY: (value * trig) / 512 about the fixed 112,96 viewport centre.
    const auto rotate=[&](double x,double y) {
        return std::array<double,2>{112.+(x*cosine+y*sine)/512.,96.+(y*cosine-x*sine)/512.};
    };
    const uint8_t normal=uint8_t(112U+(colour&15U)),damaged=114U;
    const auto paint=[&](std::array<double,2> a,std::array<double,2> b,uint8_t ink) {
        // The desktop plots each Bresenham point as a full source pixel:
        // sweep a unit square from a to b (convex hull of both squares).
        std::array<std::array<double,2>,8> points{};
        for(unsigned i=0;i<4;++i) {
            const double dx=(i==1 || i==2)?1:0,dy=i>=2?1:0;
            points[i]={a[0]+dx,a[1]+dy};points[i+4]={b[0]+dx,b[1]+dy};
        }
        std::sort(points.begin(),points.end());
        const auto cross=[](const auto& o,const auto& p,const auto& q) {
            return (p[0]-o[0])*(q[1]-o[1])-(p[1]-o[1])*(q[0]-o[0]);
        };
        std::vector<std::array<double,2>> hull;
        for(int pass=0;pass<2;++pass) {
            const auto start=hull.size();
            for(std::size_t n=0;n<points.size();++n) {
                const auto& point=points[pass?points.size()-1-n:n];
                while(hull.size()>=start+2 && cross(hull[hull.size()-2],hull.back(),point)<=0) hull.pop_back();
                hull.push_back(point);
            }
            hull.pop_back();
        }
        for(std::size_t i=1;i+1<hull.size();++i) for(const auto& corner:{hull[0],hull[i],hull[i+1]}) {
            SceneVertex v{};v.position[0]=float(corner[0]);v.position[1]=float(corner[1]);
            v.texture[1]=ink;v.texture[3]=32|(srgb?2:0);packet.geometry.vertices.push_back(v);
        }
    };
    const auto positive=(damage_flags&2U)?damaged:normal,negative=(damage_flags&1U)?damaged:normal;
    // Exact MHUD.MC geometry: the vertical pair plus mirrored wing triangles.
    for(const auto& [x1,y1,x2,y2]:std::array<std::array<double,4>,4>{{{0,200,0,265},{250,0,315,-32},{315,-32,315,32},{250,0,315,32}}}) {
        paint(rotate(x1,y1),rotate(x2,y2),positive);
        paint(rotate(-x1,-y1),rotate(-x2,-y2),negative);
    }
    auto& data=packet.geometry.texels;data.resize(272);data[13]=15-brightness;
    const auto palette=render::decode_bgr555_palette(cgram);
    for(unsigned i=0;i<256;++i) data[16+i]=palette[i].r|(uint32_t(palette[i].g)<<8)|(uint32_t(palette[i].b)<<16)|0xff000000U;
    return packet;
}
DrawPacket source_meter_packet(const simulation::MeterState& meters,
    const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb,
    unsigned viewport_width,bool anchor_to_edges,const render::HudLayout* layout) {
    if(brightness>15 || viewport_width==0 || viewport_width>8192)
        throw std::invalid_argument("Invalid GPU meter options");
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const auto rectangles=render::meter_rectangles(meters,viewport_width,anchor_to_edges,layout);
    for(size_t i=0;i<rectangles.count;++i) {
        const auto& r=rectangles.rectangles[i];
        const int left=std::max(r.x,0),right=std::min(r.x+r.width,int(viewport_width));
        const int top=std::max(r.y,0),bottom=std::min(r.y+r.height,224);
        if(right<=left || bottom<=top) continue;
        const int corners[4][2]{{left,top},{right,top},{right,bottom},{left,bottom}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex v{};v.position[0]=float(corners[corner][0]);v.position[1]=float(corners[corner][1]);
            v.texture[1]=r.colour;v.texture[3]=32|(srgb?2:0);packet.geometry.vertices.push_back(v);
        }
    }
    if(packet.geometry.vertices.empty()) return packet;
    auto& data=packet.geometry.texels;data.resize(272);data[13]=15-brightness;
    const auto palette=render::decode_bgr555_palette(cgram);
    for(unsigned i=0;i<256;++i) data[16+i]=palette[i].r|(uint32_t(palette[i].g)<<8)|(uint32_t(palette[i].b)<<16)|0xff000000U;
    return packet;
}
DrawPacket source_sprite_packet(const simulation::SnesPpuState& ppu,unsigned brightness,
    std::optional<unsigned> priority,bool srgb,const simulation::MeterState* meters,
    const render::HudLayout* layout,SourceSpritePass pass,const render::SpriteOverrides* overrides) {
    if(brightness>15 || (priority && *priority>3)) throw std::invalid_argument("Invalid source sprite options");
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    // Vertices drawn from replacement art; their material headers follow the
    // packed VRAM once its size is fixed.
    struct Replaced {std::size_t vertex;const render::TextureOverrideImage* image;uint32_t size;};
    std::vector<Replaced> replaced;
    if(!(ppu.main_screen&16)) return packet;
    constexpr unsigned sizes[6][2]{{8,16},{8,32},{8,64},{16,32},{16,64},{32,64}};
    const auto selection=std::min(unsigned(ppu.object_select>>5),5U);
    for(unsigned object=128;object-->0;) {
        const auto low=object*4;const auto attr=ppu.oam[low+3];
        if(!(ppu.oam[low]|ppu.oam[low+1]|ppu.oam[low+2]|attr)) continue;
        if(ppu.oam[low]==248 && ppu.oam[low+1]==248) continue;
        if(priority && ((attr>>4)&3)!=*priority) continue;
        const auto high=ppu.oam[512+object/4]>>((object%4)*2);
        int x=ppu.oam[low]+((high&1)<<8);if(x>=256) x-=512;
        const unsigned glyph=ppu.oam[low+2]&0x7f;
        const bool boss_label=ppu.oam[low+1]<32 && glyph>=0x71 && glyph<=0x74;
        if(boss_label && meters) {
            if(!meters->enabled || !meters->boss_max_health) continue;
            const unsigned maximum=meters->boss_max_health;
            const unsigned span=(maximum&0x80)?maximum>>1:maximum;
            x=256-18-int(span+4)-33+int(glyph-0x71)*8;
        }
        const auto tile=uint16_t(ppu.oam[low+2])|uint16_t((attr&1U)<<8);
        const auto y_byte=ppu.oam[low+1];
        const bool reticle=(tile&0x7fU)==0x61U;
        const bool warning=(tile&0x7fU)==0x3eU && (x==119 || x==127)
            && (y_byte==24 || y_byte==33 || y_byte==184 || y_byte==193);
        const bool lives=tile==189 || tile==226 || tile==229 || tile==230;
        std::optional<render::HudElement> element;
        if(!reticle && !warning) {
            if(boss_label) element=render::HudElement::boss_health;
            else if(lives || (y_byte<32 && x<128)) element=render::HudElement::lives;
            else if(y_byte>=168) element=x<128?render::HudElement::shield:render::HudElement::bombs_boost;
            else if(y_byte>=128 && x<128) element=render::HudElement::comms;
        }
        if((pass==SourceSpritePass::hud && !element) || (pass==SourceSpritePass::world && element)) continue;
        if(pass==SourceSpritePass::all_but_reticle && reticle) continue;
        const auto offset=layout && element?(*layout)[*element]:render::HudOffset{};
        const int size=int(sizes[selection][(high>>1)&1]);
        const int left=std::max(x,0),right=std::min(x+size,256);
        if(right<=left) continue;
        const render::TextureOverrideImage* image=nullptr;
        if(overrides && !overrides->empty()) {
            const auto found=overrides->find(render::sprite_override_key(
                render::object_inks(ppu,tile,unsigned(size)),unsigned(size),(attr>>1)&7U));
            if(found!=overrides->end() && found->second.width && found->second.height
                && found->second.rgba.size()==std::size_t(found->second.width)*found->second.height)
                image=&found->second;
        }
        for(int wrap=0;wrap<2;++wrap) {
            const int y=int(ppu.oam[low+1])+(boss_label?1:0)-wrap*256;
            const int top=std::max(y,0),bottom=std::min(y+size,224);
            if(bottom<=top) continue;
            const int corners[4][2]{{left,top},{right,top},{right,bottom},{left,bottom}};
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex v{};v.position[0]=float(corners[corner][0]+offset.x);v.position[1]=float(corners[corner][1]+offset.y);
                v.uv[0]=float(corners[corner][0]-x);v.uv[1]=float(corners[corner][1]-y);
                v.texture[1]=ppu.object_select|((uint32_t(ppu.oam[low+2])|((attr&1U)<<8))<<8);
                v.texture[2]=attr|(uint32_t(size)<<8);v.texture[3]=16|(srgb?2:0);
                if(image) {
                    if(attr&0x40) v.uv[0]=float(size)-v.uv[0];
                    if(attr&0x80) v.uv[1]=float(size)-v.uv[1];
                    v.texture[1]=v.texture[2]=0;v.texture[3]=8388608U|4U|(srgb?2U:0U);
                    replaced.push_back({packet.geometry.vertices.size(),image,uint32_t(size)});
                }
                packet.geometry.vertices.push_back(v);
            }
        }
    }
    if(packet.geometry.vertices.empty()) return packet;
    auto& data=packet.geometry.texels;data.resize(272+16384);data[13]=15-brightness;
    const auto palette=render::decode_bgr555_palette(ppu.cgram);
    for(unsigned i=0;i<256;++i) data[16+i]=palette[i].r|(uint32_t(palette[i].g)<<8)|(uint32_t(palette[i].b)<<16)|0xff000000U;
    pack_vram(ppu.vram,std::span<uint32_t,16384>(data.data()+272,16384));
    // Replacement art through the shader's resident-texture replacement path
    // (as portraits): a material header (masks at 9/10, image offset+1 at 12,
    // packed size at 13) then RGBA words with the screen fade baked in. The
    // key includes the OBJ size, so each image has one size.
    std::unordered_map<const render::TextureOverrideImage*,uint32_t> headers;
    for(const auto& item:replaced) {
        auto [entry,added]=headers.try_emplace(item.image,uint32_t(data.size()));
        if(added) {
            const auto header=entry->second;
            const auto& image=*item.image;
            data.resize(header+14,0U);
            data[header+9]=data[header+10]=item.size-1;
            data[header+12]=header+15;data[header+13]=image.width|(image.height<<16);
            for(const auto pixel:image.rgba) {
                uint32_t faded=pixel&0xff000000U;
                for(unsigned shift:{0U,8U,16U}) faded|=((((pixel>>shift)&255U)*brightness)/15U)<<shift;
                data.push_back(faded);
            }
        }
        packet.geometry.vertices[item.vertex].texture[0]=entry->second;
    }
    return packet;
}
}
