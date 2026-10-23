#pragma once
#include "starfox/simulation/snes_ppu.hpp"
#include <algorithm>
#include <cstring>

namespace starfox::platform::nintendo_3ds {
// SNES word-addressed allocations wrap at 64 KiB. All callers supply a range
// no larger than VRAM; compare bytes, never hashes or approximate revisions.
[[nodiscard]] inline bool same_ppu_vram_range(const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b,unsigned first,unsigned count) noexcept {
    first&=65535U;
    const auto head=std::min(count,65536U-first);
    return std::memcmp(a.vram.data()+first,b.vram.data()+first,head)==0
        && std::memcmp(a.vram.data(),b.vram.data(),count-head)==0;
}
[[nodiscard]] inline bool same_background_tiles(const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b,unsigned characters,unsigned map,
    unsigned size,unsigned depth,bool large) noexcept {
    const unsigned pages=((size&1U)?2U:1U)*((size&2U)?2U:1U);
    if(!same_ppu_vram_range(a,b,map*2U,pages*2048U)) return false;
    const auto stride=depth*8U;
    // The ordinary unchanged-bank case retains its single native memcmp.
    if(same_ppu_vram_range(a,b,characters*2U,1024U*stride)) return true;
    // Changes to unrelated model/HUD characters need not decode this layer.
    // Keep ALL configured map entries, even offscreen/hidden-priority ones:
    // future guard coverage, HDMA, edge carry and uniform-rectangle merging
    // must not depend on only the currently visible atlas keys.
    std::array<std::uint8_t,1024> referenced{};
    for(unsigned entry=0;entry<pages*1024U;++entry) {
        const auto at=(map*2U+entry*2U)&65535U;
        const unsigned character=(a.vram[at]|(unsigned(a.vram[(at+1U)&65535U])<<8U))&1023U;
        referenced[character]=1;
        if(large) {
            referenced[(character+1U)&1023U]=1;
            referenced[(character+16U)&1023U]=1;
            referenced[(character+17U)&1023U]=1;
        }
    }
    for(unsigned character=0;character<referenced.size();++character)
        if(referenced[character] && !same_ppu_vram_range(a,b,characters*2U+character*stride,stride)) return false;
    return true;
}
} // namespace starfox::platform::nintendo_3ds
