#pragma once
#include "starfox/render/embedded_metal_reflection_program.hpp"
#include <span>
namespace starfox::render::embedded_metal {
// Implemented from the exact all25 SDK-specific read-only native bundle.
std::span<const Program> programs_for_sdk() noexcept;
}
