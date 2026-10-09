#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace starfox::render {
using CalibratedPostExtents=std::array<std::array<std::uint32_t,2>,2>;
// Application-owned working images, NOT free VRAM or opaque SDK/driver/AS
// storage. Both source and full-panel work must fit the SAME four-GiB bound.
inline constexpr bool calibrated_reconstructed_post_extent_supported(
    const CalibratedPostExtents& source,const CalibratedPostExtents& panel,
    std::uint64_t selected_source_bytes=0,std::uint64_t source_transition_bytes=0) noexcept {
    constexpr std::uint64_t limit=4ULL*1024*1024*1024;
    std::uint64_t source_pixels=0,panel_pixels=0;
    for(unsigned eye=0;eye<2;++eye) {
        for(const auto& extent:{source[eye],panel[eye]})
            if(!extent[0] || !extent[1] || extent[0]>16384 || extent[1]>16384
                || std::uint64_t(extent[0])*extent[1]>64ULL*1024*1024) return false;
        source_pixels+=std::uint64_t(source[eye][0])*source[eye][1];
        panel_pixels+=std::uint64_t(panel[eye][0])*panel[eye][1];
    }
    // Panel: output/centre ink/class/depth 16; transactional SDK outputs 16;
    // composite scratch 12; old+new raw/two-scratch/float-surface sets 56;
    // all three retained double-float histories 96; four bloom float images
    // (even 1x1 reduction) 64; volume integral 16; exposure tiles <=1 B/pixel
    // apart from tiny-tile rounding covered by the shared 64-MiB allowance.
    // Panel liquid ownership/physical surfaces add 20, doubled during a
    // transactional replacement. 317 rounds UP to 320. Source keeps its
    // established 576-B allowance, or
    // the larger actual selected MSAA/shutter/liquid/fog bound. Replacing a
    // sample layout also charges the old attachments/native outputs until
    // the transactional replacement has succeeded and its owner is retired.
    constexpr std::uint64_t panel_bytes_per_pixel=320;
    static_assert(panel_bytes_per_pixel>=16+16+12+56+96+64+16+1+40);
    const auto sources=std::max(source_pixels*576,selected_source_bytes);
    const auto panels=panel_pixels*panel_bytes_per_pixel+64ULL*1024*1024;
    return sources<=limit && source_transition_bytes<=limit-sources
        && panels<=limit-sources-source_transition_bytes;
}
}
