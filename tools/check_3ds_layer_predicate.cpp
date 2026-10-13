// Asset-free ARM code-generation probe, not a console FPS benchmark.
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"

extern "C" __attribute__((used,noinline)) bool starfox_layer_predicate(unsigned layer) {
#if defined(STARFOX_LAYER_BASELINE)
    return layer==0 || (layer<=32 && std::has_single_bit(layer));
#else
    return starfox::platform::nintendo_3ds::pica_source_layer(layer);
#endif
}
