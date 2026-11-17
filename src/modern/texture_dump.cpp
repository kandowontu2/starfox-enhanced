#include "starfox/modern/texture_dump.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>

namespace starfox::modern {
namespace {
constexpr const char* readme=
"Star Fox MODERN texture dump\n"
"============================\n"
"\n"
"Each file is written the first time the game draws it. To replace one, edit\n"
"it (any resolution) and copy it with the SAME name into the textures folder\n"
"next to starfox_pc.exe, then restart with STARFOX_MODERN=1.\n"
"index.txt lists every file with its native size and where it was first seen.\n"
"To get every model texture and portrait without playing, run\n"
"  starfox_texture_dump Starfox-Assets.BIN <folder>\n"
"which writes the same tex_*.png and face_*.png names for Original and EX.\n"
"\n"
"obj_HHHHHHHHHHHHHHHH.png - a 2D sprite (HUD labels, icons, digits)\n"
"  Named by a hash of the sprite's own pixels, size and palette, so the same\n"
"  picture always gets the same name. Big words like SHIELD are several\n"
"  sprites side by side: replace each piece. Written the first time a sprite\n"
"  is drawn, so play until a digit or icon appears to get it. Keep the\n"
"  square shape (8x8 -> 32x32 or 64x64); alpha below 50% is transparent.\n"
"\n"
"face_AAAAAA.png - a communications portrait frame (32x40)\n"
"  AAAAAA is the frame's SNES ROM address. Each mouth and eye position is its\n"
"  own frame, so replace every frame of a character for smooth animation.\n"
"  The image covers the whole portrait box; keep the 4:5 aspect (128x160,\n"
"  256x320...). Pixels with alpha below 50% show the scene behind.\n"
"\n"
"tex_AAAAAA_l.png / tex_AAAAAA_h.png - Super FX model textures\n"
"  AAAAAA is the texture's SNES ROM address; l/h is the low/high nibble plane\n"
"  (two different images stored in the same bytes). Colours are the level's\n"
"  model palette when first seen. The image is stretched over the texture's\n"
"  native area, so keep the aspect ratio and use power-of-two sizes (32x32 ->\n"
"  128x128 or 256x256, 64x16 -> 256x64); whole-object sprites such as shots\n"
"  and explosions ignore other sizes. Pixels with alpha below 50% are\n"
"  transparent (the source's colour 0). Some files are small atlases: the\n"
"  model uses only part of them, so replace the whole image in place.\n"
"\n"
"bg_XXXX_sky.png - planet sky (XXXX = background id, hex; Corneria 001b)\n"
"  One full turn of the BG2 art, 112 native rows tall. The bottom edge sits on\n"
"  the horizon and the image wraps left to right. Any size works; the\n"
"  mountains at the bottom keep their position if you scale uniformly.\n"
"\n"
"bg_XXXX_ground.png - planet ground template\n"
"  The native ground is a distance haze, not a picture, so this is a starting\n"
"  point: the near haze colour with the classic scroll stripes. The image\n"
"  tiles every 2048 x 2048 world units, fixed to the world (top = ahead).\n"
"  The ground dots are drawn separately on a 256-unit grid (8 x 8 per tile);\n"
"  STARFOX_MODERN_GROUND_DOTS=0 hides them.\n"
"\n"
"bg_XXXX_atlas.png - the whole BG2 art for that level, for reference only\n"
"  (not loaded).\n";
std::string hex(unsigned value,int digits) {
    char text[16];std::snprintf(text,sizeof text,"%0*x",digits,value);return text;
}
uint32_t rgba(const render::Rgba8& c) noexcept {
    return uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)|0xff000000U;
}
}
TextureDump::TextureDump(std::string directory):directory_(std::move(directory)) {
    std::error_code error;
    std::filesystem::create_directories(directory_,error);
    std::ofstream(directory_+"README.txt",std::ios::binary)<<readme;
}
bool TextureDump::write(const std::string& name,unsigned width,unsigned height,std::span<const uint32_t> pixels,const std::string& note) {
    // Dumps accumulate across sessions: an existing file (maybe already
    // being edited) is never overwritten.
    std::error_code exists_error;
    if(std::filesystem::exists(directory_+name,exists_error)) return false;
    std::vector<uint32_t> copy(pixels.begin(),pixels.end());
    auto* surface=SDL_CreateSurfaceFrom(int(width),int(height),SDL_PIXELFORMAT_ABGR8888,copy.data(),int(width*4U));
    if(!surface) return false;
    const auto path=directory_+name;
    const bool saved=SDL_SavePNG(surface,path.c_str());
    SDL_DestroySurface(surface);
    if(!saved) {std::fprintf(stderr,"MODERN texture dump: %s failed: %s\n",path.c_str(),SDL_GetError());return false;}
    std::ofstream(directory_+"index.txt",std::ios::binary|std::ios::app)<<name<<"  "<<width<<'x'<<height<<"  "<<note<<'\n';
    ++written_;
    return true;
}
void TextureDump::model_texture(const assets::TextureImage& texture,const std::array<uint16_t,16>& palette) {
    const auto key=render::texture_override_key(texture);
    if(!models_.insert(key).second) return;
    const unsigned width=texture.u_mask+1U,height=texture.v_mask+1U;
    if(texture.texels.size()<std::size_t(width)*height) return;
    std::array<uint16_t,256> words{};
    std::copy(palette.begin(),palette.end(),words.begin());
    const auto colours=render::decode_bgr555_palette(words);
    std::vector<uint32_t> image(std::size_t(width)*height);
    for(std::size_t i=0;i<image.size();++i) {
        const auto index=texture.texels[i]&15U;
        image[i]=index?rgba(colours[index]):0U;
    }
    write(render::texture_override_name(key)+".png",width,height,image,
        "model texture, descriptor 0x"+hex(texture.descriptor,4));
}
void TextureDump::sprites(const simulation::SnesPpuState& ppu) {
    if(!(ppu.main_screen&16)) return;
    constexpr unsigned sizes[6][2]{{8,16},{8,32},{8,64},{16,32},{16,64},{32,64}};
    const auto selection=std::min(unsigned(ppu.object_select>>5),5U);
    std::optional<render::Palette256> colours;
    for(unsigned object=0;object<128;++object) {
        const auto low=object*4;const auto attr=ppu.oam[low+3];
        if(!(ppu.oam[low]|ppu.oam[low+1]|ppu.oam[low+2]|attr)) continue;
        if(ppu.oam[low]==248 && ppu.oam[low+1]==248) continue;
        const auto high=ppu.oam[512+object/4]>>((object%4)*2);
        const unsigned size=sizes[selection][(high>>1)&1];
        const auto tile=uint16_t(ppu.oam[low+2]|((attr&1U)<<8));
        const unsigned palette=(attr>>1)&7U;
        const auto inks=render::object_inks(ppu,tile,size);
        if(std::all_of(inks.begin(),inks.end(),[](auto ink){return ink==0;})) continue;
        const auto key=render::sprite_override_key(inks,size,palette);
        if(!sprites_.insert(key).second) continue;
        if(!colours) colours=render::decode_bgr555_palette(ppu.cgram);
        std::vector<uint32_t> image(inks.size());
        for(std::size_t i=0;i<inks.size();++i)
            image[i]=inks[i]?rgba((*colours)[128+palette*16+inks[i]]):0U;
        int x=ppu.oam[low]+((high&1)<<8);if(x>=256) x-=512;
        write(render::sprite_override_name(key)+".png",size,size,image,
            "sprite, tile 0x"+hex(tile,3)+" palette "+std::to_string(palette)
            +" at "+std::to_string(x)+","+std::to_string(ppu.oam[low+1]));
    }
}
void TextureDump::landscape(uint16_t background,const simulation::SnesPpuState& ppu,float horizon,std::span<const uint32_t> haze) {
    if(!backgrounds_.insert(background).second) return;
    const auto stem="bg_"+hex(background,4);
    const auto [width,height]=vr::bg2_art_size(ppu);
    // Sky: art rows [horizon-112, horizon), atlas column 0 at the left edge;
    // matches landscape_texture_sample's mapping.
    const int top=int(std::lround(horizon))-112;
    auto sky=vr::bg2_art_image(ppu,0,top,width,112);
    // Index 0 shows the backdrop colour (CGRAM 0) on the cartridge.
    const auto backdrop=rgba(render::decode_bgr555_palette(ppu.cgram)[0]);
    for(auto& pixel:sky) if(!pixel) pixel=backdrop;
    write(stem+"_sky.png",width,112,sky,"planet sky, bottom edge = horizon (art row "+std::to_string(int(std::lround(horizon)))+")");
    // Ground template: 512 px per 2048 units, near haze colour, 128-unit
    // stripes (32 px) alternating with an 80% linear-light shade.
    constexpr unsigned size=512;
    const uint32_t base=haze.size()>100?haze[100]:0xff406040U;
    const auto shade=[](uint32_t c) {
        uint32_t out=0xff000000U;
        for(unsigned channel=0;channel<3;++channel) {
            const double v=double((c>>(channel*8))&255U)/255.;
            const double linear=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);
            const double dark=linear*.8;
            const double encoded=dark<=.0031308?dark*12.92:1.055*std::pow(dark,1/2.4)-.055;
            out|=uint32_t(std::clamp(std::lround(encoded*255.),0L,255L))<<(channel*8);
        }
        return out;
    };
    std::vector<uint32_t> ground(std::size_t(size)*size);
    for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x)
        ground[std::size_t(y)*size+x]=((y/32U)&1U)?shade(base):base;
    write(stem+"_ground.png",size,size,ground,"planet ground template, tiles every 2048 world units");
    write(stem+"_atlas.png",width,height,vr::bg2_art_image(ppu,0,0,width,height),"BG2 art reference (not loaded)");
}
}
