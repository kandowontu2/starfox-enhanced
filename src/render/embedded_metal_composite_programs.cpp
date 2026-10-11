#if !defined(STARFOX_CALIBRATED_COMPOSITE_METAL)
#error "The complete native compositor payload graph is required"
#endif
#include "embedding_programs.hpp"
namespace starfox::render::metal_composite {
std::span<const Program> programs_for_sdk() noexcept {
    return graphics_embedding_programs;
}
}
