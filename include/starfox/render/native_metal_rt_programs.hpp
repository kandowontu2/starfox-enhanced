#pragma once
#include <cstdint>
#include <span>

namespace starfox::render::shadows {
enum class NativeMetalRtProgram { shadow, reflection, empty_shadow, empty_reflection };
// Full build-generated native library, borrowed from the executable's read-only
// section. No source compiler, file search or shorter runtime shader fallback.
std::span<const std::uint8_t> native_metal_rt_program(NativeMetalRtProgram) noexcept;
}
