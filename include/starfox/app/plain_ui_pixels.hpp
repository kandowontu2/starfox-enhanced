#pragma once

#include "starfox/render/palette.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace starfox::app {
// A host menu has no source-clock animation or scene history. Retain its exact
// indexed pixels rather than expanding and uploading an unchanged image every
// presentation. Compare the complete key: this is not a hash-only cache.
class PlainUiPixels {
public:
    bool update(std::uint32_t width,std::uint32_t height,
        std::span<const std::uint8_t> pixels,
        std::span<const starfox::render::Rgba8> palette) {
        const auto count=std::uint64_t(width)*height;
        if(!width || !height || count!=pixels.size()
            || count>std::numeric_limits<std::size_t>::max()/4)
            throw std::invalid_argument("Plain UI pixel extent mismatch");
        // Host UI is always opaque. Canonicalize palette alpha before comparing
        // it, including the black fallback for an out-of-range palette index.
        std::array<std::uint32_t,256> colours{};
        colours.fill(0xff000000U);
        for(std::size_t i=0;i<std::min(palette.size(),colours.size());++i)
            colours[i]=std::uint32_t(palette[i].r) | (std::uint32_t(palette[i].g)<<8)
                | (std::uint32_t(palette[i].b)<<16) | 0xff000000U;
        if(valid_ && width==width_ && height==height_ && colours==colours_
            && std::equal(pixels.begin(),pixels.end(),pixels_.begin())) return false;
        valid_=false;
        pixels_.assign(pixels.begin(),pixels.end());
        rgba_.resize(std::size_t(count)*4);
        for(std::size_t i=0;i<pixels.size();++i) {
            const auto colour=colours[pixels[i]];
            rgba_[i*4]=std::uint8_t(colour);rgba_[i*4+1]=std::uint8_t(colour>>8);
            rgba_[i*4+2]=std::uint8_t(colour>>16);rgba_[i*4+3]=255;
        }
        width_=width;height_=height;colours_=colours;valid_=true;
        return true;
    }
    [[nodiscard]] std::span<const std::uint8_t> rgba() const noexcept {return rgba_;}
    void invalidate() noexcept {valid_=false;}
private:
    bool valid_{};
    std::uint32_t width_{},height_{};
    std::array<std::uint32_t,256> colours_{};
    std::vector<std::uint8_t> pixels_,rgba_;
};
}
