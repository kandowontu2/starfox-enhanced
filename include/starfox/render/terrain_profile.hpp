#pragma once
#include "starfox/simulation/snes_ppu.hpp"
#include <array>
#include <cstdint>
namespace starfox::render {
inline std::uint32_t terrain_tilemap_hash(const simulation::SnesPpuState& ppu,std::uint32_t tile_offset=0) {
    std::uint32_t hash=2166136261u;
    const auto start=std::uint32_t(ppu.bg2_screen_base)*2;
    for(unsigned i=0;i<8192;i+=2) {
        const std::uint32_t word=ppu.vram[(start+i)&65535u]|(std::uint32_t(ppu.vram[(start+i+1)&65535u])<<8);
        const auto normalized=(word&0xfc00u)|((word-tile_offset)&1023u);
        hash=(hash^(normalized&255u))*16777619u;
        hash=(hash^(normalized>>8))*16777619u;
    }
    return hash;
}
inline std::array<std::uint32_t,2> authored_terrain_rows(const simulation::SnesPpuState& ppu) {
    // ST-P.SCR, 1-4.SCR and F-1.SCR are 8192-byte source tilemaps.
    // Rows 0..44 contain backdrop art; rows 45..63 are their authored
    // ground gradients. Verify the complete tilemap:
    // arbitrary similarly coloured or differently arranged art is not terrain.
    if(ppu.tunnel_scene || ppu.background_mode!=2 || ppu.bg2_tile_size_16 || ppu.bg2_screen_size!=3)
        return {};
    // The source loader relocates character indices (observed +192 in both
    // Original and EX). Normalize only that uniform tile offset, preserving
    // every palette/priority/flip bit and all 4096 tile relationships.
    const auto start=std::uint32_t(ppu.bg2_screen_base)*2;
    const auto first=ppu.vram[start&65535u]|(std::uint32_t(ppu.vram[(start+1)&65535u])<<8);
    struct Profile {std::uint32_t first_word,hash;};
    constexpr std::array<Profile,3> profiles{{
        {0x146a,0x536ac185u}, // ST-P: Corneria/Training
        {0x440f,0x80d53f12u}, // 1-4
        {0x1809,0x33b82640u}, // F-1: surface approaches/exits
    }};
    for(const auto& profile:profiles) {
        if((first&0xfc00u)!=(profile.first_word&0xfc00u)) continue;
        const auto tile_offset=(first-profile.first_word)&1023u;
        if(terrain_tilemap_hash(ppu,tile_offset)==profile.hash) return {360,512};
    }
    return {};
}
}
