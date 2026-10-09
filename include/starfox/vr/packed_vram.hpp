#pragma once
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>

namespace starfox::vr {
// Shader storage words always hold four little-endian source bytes.
inline void pack_vram(std::span<const std::uint8_t,65536> source,
    std::span<std::uint32_t,16384> destination) {
    if constexpr(std::endian::native==std::endian::little) {
        std::memcpy(destination.data(),source.data(),source.size_bytes());
    } else {
        for(std::size_t word=0;word<destination.size();++word) {
            const auto i=word*4;
            destination[word]=std::uint32_t(source[i])|(std::uint32_t(source[i+1])<<8)
                |(std::uint32_t(source[i+2])<<16)|(std::uint32_t(source[i+3])<<24);
        }
    }
}
}
