#include "starfox/vr/source_sprites.hpp"
#include "starfox/vr/packed_vram.hpp"
#include "starfox/vr/frame_menu.hpp"
#include "starfox/vr/game_scene.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/render/hud_layout.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace starfox::vr {
std::vector<DrawPacket> layout_a_menu_packets(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    const FrameMenu& menu,bool srgb) {
    std::array<uint16_t,256> palette{};palette[1]=0x7fff;palette[2]=0x03ff;
    std::vector<DrawPacket> rows;
    const unsigned visible_rows=std::min(6U,menu.row_count());
    const int footer=67+18*int(visible_rows);
    rows.push_back(layout_a_surface(true,menu.selection-menu.first_visible_row(),srgb,visible_rows));
    const auto add=[&](std::string_view text,int y,uint8_t ink) {
        auto row=source_ui_text_packet(rom,symbols,text,16,y,240,ink,palette,15,srgb,true);
        row.model=identity_matrix;rows.push_back(std::move(row));
    };
    const auto first=menu.first_visible_row();
    if(menu.language>=1 && menu.language<=4) {
        const auto add_unicode=[&](std::u32string_view text,int y,uint8_t ink) {
            auto row=source_unicode_ui_text_packet(rom,symbols,text,16,y,ink,palette,15,srgb);
            row.model=identity_matrix;rows.push_back(std::move(row));
        };
        const auto labels=menu.localized_labels();add_unicode(menu.translate(menu.title()),35,1);
        for(unsigned i=first;i<labels.size() && i<first+6;++i)
            add_unicode((i==menu.selection?U"> ":U"  ")+labels[i],67+int(i-first)*18,i==menu.selection?2:1);
        const auto help=menu.localized_help();add_unicode(help[0],footer+8,1);add_unicode(help[1],footer+26,1);
    } else {
        add(menu.title(),35,1);const auto labels=menu.labels();
        for(unsigned i=first;i<labels.size() && i<first+6;++i)
            add((i==menu.selection?"> ":"  ")+labels[i],67+int(i-first)*18,i==menu.selection?2:1);
        add("STICK: MOVE",footer+8,1);add("MENU: SELECT",footer+26,1);
    }
    return rows;
}
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
    unsigned brightness,bool srgb) {
    std::vector<DrawPacket> packets;
    if(!dialogue.active) return packets;
    packets.push_back(source_portrait_packet(rom,symbols,dialogue.portrait_frame,
        dialogue.alternate_portraits,cgram,brightness,srgb));
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
DrawPacket source_portrait_packet(const assets::RomImage& rom,const assets::SymbolMap& symbols,
    uint8_t frame,bool alternate,const std::array<uint16_t,256>& cgram,unsigned brightness,bool srgb) {
    const auto address=[&](const char* name) {
        for(auto value:symbols.find(name))
            if((value&0xffffU)>=0x8000U && ((value>>16)&255U)<0x70U) return value;
        return uint32_t{};
    };
    auto base=alternate?address("FACEDATA2"):0U;
    if(!base) base=address("FACEDATA");
    if(!base) throw std::runtime_error("Missing portrait data");
    // Keep the authored planar tiles packed: the existing background shader
    // decodes their four bitplanes and palette, with no CPU pixel rasterization.
    simulation::SnesPpuState ppu{};
    ppu.main_screen=1;ppu.background_mode=1;ppu.bg1_screen_base=512;
    ppu.cgram=cgram;
    for(unsigned i=0;i<640;++i) ppu.vram[i]=rom.read8(base+uint32_t(frame)*640+i);
    for(unsigned x=0;x<4;++x) for(unsigned y=0;y<5;++y) {
        const unsigned offset=1024+(y*32+x)*2;
        const unsigned tile=x*5+y;
        ppu.vram[offset]=uint8_t(tile);ppu.vram[offset+1]=28; // Palette seven.
    }
    BackgroundTileOptions options;options.brightness=brightness;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg1,options,srgb);
    packet.geometry.vertices.clear();
    for(bool ink:{false,true}) for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
        const float u=(corner==1 || corner==2)?32.F:0.F,v=corner>=2?40.F:0.F;
        SceneVertex vertex{};
        // Correct SNES pixel aspect without moving the right edge into text.
        vertex.position[0]=80.F-(32.F-u)*7.F/6.F;vertex.position[1]=152.F+v;
        vertex.uv[0]=u;vertex.uv[1]=v;
        // Tile index zero is transparent in the shared shader. Its native
        // portrait meaning is palette 112, supplied by this backing quad.
        vertex.texture[1]=ink?0U:112U;vertex.texture[3]=(ink?8U:32U)|(srgb?2U:0U);
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
    const render::HudLayout* layout,SourceSpritePass pass) {
    if(brightness>15 || (priority && *priority>3)) throw std::invalid_argument("Invalid source sprite options");
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
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
        const auto offset=layout && element?(*layout)[*element]:render::HudOffset{};
        const int size=int(sizes[selection][(high>>1)&1]);
        const int left=std::max(x,0),right=std::min(x+size,256);
        if(right<=left) continue;
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
                packet.geometry.vertices.push_back(v);
            }
        }
    }
    if(packet.geometry.vertices.empty()) return packet;
    auto& data=packet.geometry.texels;data.resize(272+16384);data[13]=15-brightness;
    const auto palette=render::decode_bgr555_palette(ppu.cgram);
    for(unsigned i=0;i<256;++i) data[16+i]=palette[i].r|(uint32_t(palette[i].g)<<8)|(uint32_t(palette[i].b)<<16)|0xff000000U;
    pack_vram(ppu.vram,std::span<uint32_t,16384>(data.data()+272,16384));
    return packet;
}
}
