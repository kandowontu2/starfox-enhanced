// Moved from src/modern/texture_dump.cpp and sdl_frame.cpp so the PCVR app
// loads the same replacement artwork as the flat renderer.
#include "starfox/render/texture_override_files.hpp"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
namespace starfox::render {
std::optional<TextureOverrideImage> load_rgba_image(const std::string& path,unsigned max_size) {
    SDL_Surface* loaded=SDL_LoadSurface(path.c_str());
    if(!loaded) return {};
    SDL_Surface* rgba=SDL_ConvertSurface(loaded,SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(loaded);
    if(!rgba) return {};
    TextureOverrideImage image{unsigned(rgba->w),unsigned(rgba->h),{}};
    if(image.width==0 || image.height==0 || image.width>max_size || image.height>max_size) {SDL_DestroySurface(rgba);return {};}
    image.rgba.resize(std::size_t(image.width)*image.height);
    for(unsigned y=0;y<image.height;++y)
        std::memcpy(image.rgba.data()+std::size_t(y)*image.width,
            static_cast<const uint8_t*>(rgba->pixels)+std::size_t(y)*rgba->pitch,image.width*4U);
    SDL_DestroySurface(rgba);
    return image;
}
namespace {
// Every file in `directory` whose name `key` accepts (at most 4096 a side).
template<class Map,class Key> Map load_overrides(const std::string& directory,Key key) {
    Map overrides;
    std::error_code error;
    for(const auto& entry:std::filesystem::directory_iterator(directory,error)) {
        const auto id=key(entry.path().filename().string());
        if(!id) continue;
        if(auto image=load_rgba_image(entry.path().string(),4096)) {
            overrides[*id]=std::move(*image);
            std::fprintf(stderr,"MODERN textures: %s loaded\n",entry.path().string().c_str());
        }
    }
    return overrides;
}
}
TextureOverrides load_texture_overrides(const std::string& directory) {
    return load_overrides<TextureOverrides>(directory,[](const std::string& name)->std::optional<uint32_t> {
        unsigned address=0;char plane=0;
        if(name.size()!=16 || std::sscanf(name.c_str(),"tex_%6x_%c.png",&address,&plane)!=2 || (plane!='l' && plane!='h')) return std::nullopt;
        return address|(plane=='h'?0x1000000U:0U);
    });
}
TextureOverrides load_portrait_overrides(const std::string& directory) {
    return load_overrides<TextureOverrides>(directory,[](const std::string& name)->std::optional<uint32_t> {
        unsigned address=0;
        if(name.size()!=15 || std::sscanf(name.c_str(),"face_%6x.png",&address)!=1) return std::nullopt;
        return address;
    });
}
SpriteOverrides load_sprite_overrides(const std::string& directory) {
    return load_overrides<SpriteOverrides>(directory,[](const std::string& name)->std::optional<uint64_t> {
        unsigned long long key=0;
        if(name.size()!=24 || std::sscanf(name.c_str(),"obj_%16llx.png",&key)!=1) return std::nullopt;
        return uint64_t(key);
    });
}
}
