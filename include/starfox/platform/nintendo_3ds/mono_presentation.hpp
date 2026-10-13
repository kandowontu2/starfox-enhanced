#pragma once

namespace starfox::platform::nintendo_3ds {

enum class MonoTileResult { presented, busy, rejected };

// A busy queue is not a rejected tile scene. The retry callback must acquire
// the GPU in blocking mode before rewriting textures. Retry the same immutable
// foreground once; actual rejection retains the complete software fallback.
// The fallback's acquire also retires any partial submission before writes.
template<class Tiles, class BeforeComplete, class Complete>
bool present_mono_tiles_or_complete(Tiles&& tiles, BeforeComplete&& before_complete, Complete&& complete) {
    const auto first = tiles(false);
    if (first == MonoTileResult::presented) return true;
    if (first == MonoTileResult::busy) {
        const auto retried = tiles(true);
        if (retried == MonoTileResult::presented) return true;
    }
    before_complete();
    return complete();
}

} // namespace starfox::platform::nintendo_3ds
