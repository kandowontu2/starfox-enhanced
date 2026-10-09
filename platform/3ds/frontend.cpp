#include "starfox/platform/nintendo_3ds/frontend.hpp"
#include "starfox/localization/bitmap_font.hpp"
#include <algorithm>
#include <fstream>
#include <string>

namespace starfox::platform::nintendo_3ds {
void Canvas::pixel(int x,int y,Rgb c) {
    if(x<0 || y<0 || x>=int(width_) || y>=int(height_)) return;
    const auto at=(std::size_t(y)*width_+unsigned(x))*3;
    pixels_[at]=c.r;pixels_[at+1]=c.g;pixels_[at+2]=c.b;
    if(!coverage_.empty()) coverage_[std::size_t(y)*width_+unsigned(x)]=1;
}
void Canvas::clear(Rgb c) {rectangle(0,0,int(width_),height_,c);}
void Canvas::rectangle(int x,int y,int w,int h,Rgb c) {
    if(w<=0 || h<=0) return;
    const auto end_y=std::min<std::int64_t>(height_,std::int64_t(y)+h);
    const auto end_x=std::min<std::int64_t>(width_,std::int64_t(x)+w);
    for(int row=std::max(0,y);row<end_y;++row)
        for(int col=std::max(0,x);col<end_x;++col) pixel(col,row,c);
}
void Canvas::line(int x0,int y0,int x1,int y1,Rgb c) {
    // Bounded panel/diagnostic primitives; reject coordinates that could make
    // an invalid scene drive an unbounded off-screen raster loop.
    if(std::abs(double(x0))>4096 || std::abs(double(y0))>4096
        || std::abs(double(x1))>4096 || std::abs(double(y1))>4096) return;
    const int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1;
    int error=dx+dy;
    for(;;) {
        pixel(x0,y0,c);if(x0==x1 && y0==y1) break;
        const int twice=error*2;
        if(twice>=dy) {error+=dy;x0+=sx;}
        if(twice<=dx) {error+=dx;y0+=sy;}
    }
}
void Canvas::text(int x,int y,std::string_view value,Rgb c,unsigned scale,
    unsigned box_width,unsigned box_height) {
    if(!scale || scale>3) throw std::invalid_argument("Invalid 3DS font scale");
    if(x<0 || x>=int(width_) || y<0 || y>=int(height_)) return;
    const auto available=width_-unsigned(x);
    const int right=x+int(std::min(box_width?box_width:(available>8?available-8:available),available));
    const int bottom=y+int(std::min(box_height?box_height:height_-unsigned(y),height_-unsigned(y)));
    const int initial=x;
    for(unsigned char code:value) {
        if(y+int(8*scale)>bottom) break;
        if(code=='\n') {x=initial;y+=9*int(scale);continue;}
        const auto* glyph=localization::glyph(code);
        if(!glyph) glyph=localization::glyph('?');
        if(!glyph) continue;
        if(x+int(glyph->advance*scale)>right) {x=initial;y+=9*int(scale);}
        if(y+int(8*scale)>bottom) break;
        for(unsigned row=0;row<8;++row) for(unsigned col=0;col<8;++col)
            if(x+int((col+1)*scale)<=right && (glyph->rows[row]&(0x80U>>col)))
                rectangle(x+int(col*scale),y+int(row*scale),scale,scale,c);
        x+=int(glyph->advance*scale);
    }
}
void Canvas::image(int x,int y,ImageView source) {
    if(!source.width || source.width>width_ || !source.height || source.height>screen_height
        || !valid_image(source,source.width,source.height)) throw std::invalid_argument("Invalid 3DS canvas image");
    if(x>=int(width_) || y>=int(height_) || std::int64_t(x)+source.width<=0
        || std::int64_t(y)+source.height<=0) return;
    for(unsigned row=0;row<source.height;++row) for(unsigned col=0;col<source.width;++col) {
        const auto at=std::size_t(row)*source.pitch+col*3;
        pixel(x+int(col),y+int(row),{source.pixels[at],source.pixels[at+1],source.pixels[at+2]});
    }
}
void Canvas::scaled_artwork(int x,int y,ImageView source,std::span<const std::uint8_t> mask,unsigned quarters) {
    if(!valid_image(source,source.width,source.height) || !source.pixels.data() || quarters<2 || quarters>8
        || mask.size()!=std::size_t(source.width)*source.height)
        throw std::invalid_argument("Invalid scaled 3DS HUD artwork");
    const unsigned w=(source.width*quarters+3)/4,h=(source.height*quarters+3)/4;
    for(unsigned row=0;row<h;++row) for(unsigned col=0;col<w;++col) {
        const auto sx=std::min(source.width-1,col*4/quarters),sy=std::min(source.height-1,row*4/quarters);
        if(!mask[std::size_t(sy)*source.width+sx]) continue;
        const auto at=std::size_t(sy)*source.pitch+sx*3;
        pixel(x+int(col),y+int(row),{source.pixels[at],source.pixels[at+1],source.pixels[at+2]});
    }
}
void Canvas::write_bmp(std::string_view path) const {
    std::ofstream out(std::string(path),std::ios::binary);
    if(!out) throw std::runtime_error("Cannot create 3DS diagnostic BMP");
    const unsigned pitch=(width_*3+3)&~3U,bytes=pitch*height_;
    const auto word=[&](unsigned v,unsigned count) {while(count--) {out.put(char(v&255));v>>=8;}};
    out.write("BM",2);word(bytes+54,4);word(0,4);word(54,4);word(40,4);word(width_,4);word(height_,4);
    word(1,2);word(24,2);word(0,4);word(bytes,4);word(0,4);word(0,4);word(0,4);word(0,4);
    for(int y=int(height_)-1;y>=0;--y) {
        for(unsigned x=0;x<width_;++x) {
            const auto at=(std::size_t(y)*width_+x)*3;
            out.put(char(pixels_[at+2]));out.put(char(pixels_[at+1]));out.put(char(pixels_[at]));
        }
        for(unsigned pad=width_*3;pad<pitch;++pad) out.put(0);
    }
    if(!out) throw std::runtime_error("3DS diagnostic BMP write failed");
}
void draw_cockpit(Canvas& c,const HudState& s) {
    if(!s.layout.valid()) throw std::invalid_argument("Invalid native cockpit layout");
    if(s.layout!=CockpitLayout{}) {CockpitWidgets widgets;widgets.draw(c,s);return;}
    if(c.view().width!=bottom_width) throw std::invalid_argument("Cockpit belongs on the lower LCD");
    if(s.portrait.width && (s.portrait.width>64 || s.portrait.height>64
        || !valid_image(s.portrait,s.portrait.width,s.portrait.height)))
        throw std::invalid_argument("Invalid 3DS radio portrait");
    constexpr Rgb metal{51,66,82},edge{126,158,177},ink{6,18,28},label{213,237,244};
    if(s.radio_artwork.width && (s.radio_artwork.width>284 || s.radio_artwork.height>56
        || !valid_image(s.radio_artwork,s.radio_artwork.width,s.radio_artwork.height)))
        throw std::invalid_argument("Invalid 3DS radio text artwork");
    c.clear({15,29,42});
    for(int y=83;y<240;++y) {
        const int slope=std::min(92,(y-83)*2/3);
        c.rectangle(0,y,112-slope,1,metal);c.rectangle(208+slope,y,112-slope,1,metal);
        c.line(111-slope,y,111-slope,y,edge);c.line(208+slope,y,208+slope,y,edge);
    }
    c.rectangle(8,8,304,70,edge);c.rectangle(10,10,300,66,ink);
    if(s.radio_artwork.width) c.image(18,16,s.radio_artwork);
    else c.text(18,16,s.radio_message.empty()?"RADIO / STANDBY":s.radio_message,label,2,284,56);
    c.rectangle(123,99,74,74,edge);c.rectangle(125,101,70,70,ink);
    if(s.portrait.width) {
        c.image(128,104,s.portrait);
    } else c.text(132,130,"COMMS",edge,2);
    const auto meter=[&](int x,int y,int width,unsigned percent,Rgb colour) {
        c.rectangle(x,y,width,9,edge);c.rectangle(x+2,y+2,width-4,5,ink);
        c.rectangle(x+2,y+2,int((width-4)*std::min(100U,percent)/100),5,colour);
    };
    for(unsigned ally=0;ally<3;++ally) if(s.ally_percent[ally]) {
        const int x=ally==0?17:ally==1?241:128,y=ally==2?181:100;
        meter(x,y,ally==2?64:62,*s.ally_percent[ally],{86,208,168});
    }
    if(s.meters_enabled) {
        c.text(12,200,"SHIELD",label);meter(12,212,96,s.shield_percent,{239,90,99});
        if(s.second_shield_percent) {
            c.text(12,224,"P2",label);meter(32,226,76,*s.second_shield_percent,{86,208,168});
        }
        if(s.boost_enabled) {c.text(248,200,"BOOST",label);meter(212,212,96,s.boost_percent,{88,160,244});}
        if(s.boss_percent) {c.text(119,214,"ENEMY",label);meter(119,226,82,*s.boss_percent,{246,204,75});}
    }
    if(s.counters_enabled) {
        c.text(12,182,std::string(s.second_player_view?"P2 LIVES ":"LIVES ")+std::to_string(s.lives),label);
        c.text(s.second_player_view?212:244,182,std::string(s.second_player_view?"P2 BOMBS ":"BOMBS ")+std::to_string(s.bombs),label);
        if(s.second_counters) {
            c.text(12,164,"P2 LIVES "+std::to_string(s.second_counters->lives),label);
            c.text(212,164,"P2 BOMBS "+std::to_string(s.second_counters->bombs),label);
        }
    }
}
bool CockpitDashboard::update(const HudState& s) {
    if(!s.layout.valid()) throw std::invalid_argument("Invalid native cockpit layout");
    if(s.portrait.width && (s.portrait.width>64 || s.portrait.height>64
        || !valid_image(s.portrait,s.portrait.width,s.portrait.height)))
        throw std::invalid_argument("Invalid 3DS radio portrait");
    if(s.radio_artwork.width && (s.radio_artwork.width>284 || s.radio_artwork.height>56
        || !valid_image(s.radio_artwork,s.radio_artwork.width,s.radio_artwork.height)))
        throw std::invalid_argument("Invalid 3DS radio text artwork");
    const auto same_image=[](ImageView source,ImageView previous,const std::vector<std::uint8_t>& owned) {
        if(!source.width) return !previous.width;
        if(source.width!=previous.width || source.height!=previous.height)
            return false;
        const auto row_bytes=std::size_t(source.width)*3;
        for(unsigned row=0;row<source.height;++row)
            if(!std::equal(source.pixels.begin()+std::size_t(row)*source.pitch,
                source.pixels.begin()+std::size_t(row)*source.pitch+row_bytes,
                owned.begin()+std::size_t(row)*row_bytes)) return false;
        return true;
    };
    if(previous_ && s.layout==previous_->layout && s.shield_percent==previous_->shield_percent
        && s.boost_percent==previous_->boost_percent && s.lives==previous_->lives
        && s.bombs==previous_->bombs && s.boss_percent==previous_->boss_percent
        && s.ally_percent==previous_->ally_percent && s.radio_message==message_
        && s.meters_enabled==previous_->meters_enabled && s.boost_enabled==previous_->boost_enabled
        && s.counters_enabled==previous_->counters_enabled && s.second_shield_percent==previous_->second_shield_percent
        && s.second_player_view==previous_->second_player_view && s.second_counters==previous_->second_counters
        && same_image(s.portrait,previous_->portrait,portrait_)
        && same_image(s.radio_artwork,previous_->radio_artwork,radio_))
        return false;
    // Prepare ownership before touching the published canvas. Ignore padding
    // in source images; only the visible pixels are lower-screen content.
    std::string message(s.radio_message);
    std::vector<std::uint8_t> portrait(std::size_t(s.portrait.width)*s.portrait.height*3);
    if(s.portrait.width) for(unsigned row=0;row<s.portrait.height;++row)
        std::copy_n(s.portrait.pixels.begin()+std::size_t(row)*s.portrait.pitch,
            std::size_t(s.portrait.width)*3,portrait.begin()+std::size_t(row)*s.portrait.width*3);
    std::vector<std::uint8_t> radio(std::size_t(s.radio_artwork.width)*s.radio_artwork.height*3);
    if(s.radio_artwork.width) for(unsigned row=0;row<s.radio_artwork.height;++row)
        std::copy_n(s.radio_artwork.pixels.begin()+std::size_t(row)*s.radio_artwork.pitch,
            std::size_t(s.radio_artwork.width)*3,radio.begin()+std::size_t(row)*s.radio_artwork.width*3);
    if(s.layout==CockpitLayout{}) draw_cockpit(canvas_,s);
    else widgets_.draw(canvas_,s);
    message_=std::move(message);portrait_=std::move(portrait);radio_=std::move(radio);previous_=s;
    previous_->radio_message=message_;
    previous_->portrait=s.portrait.width?ImageView{portrait_,s.portrait.width,s.portrait.height,s.portrait.width*3}:ImageView{};
    previous_->radio_artwork=s.radio_artwork.width?ImageView{radio_,s.radio_artwork.width,s.radio_artwork.height,s.radio_artwork.width*3}:ImageView{};
    return true;
}
} // namespace starfox::platform::nintendo_3ds
