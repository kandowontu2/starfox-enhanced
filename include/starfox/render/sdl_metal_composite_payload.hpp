#pragma once

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_error.h>
#include <cstddef>
#include <cstring>
#include <span>

namespace starfox::render::metal_composite {
struct Program {
    const char* name;
    const char* entry;
    const unsigned char* bytes;
    std::size_t size;
    SDL_GPUShaderStage stage;
    Uint32 samplers, storage_buffers, uniforms;
};

#if defined(STARFOX_CALIBRATED_COMPOSITE_METAL)
// Defined by the complete five-program, SDK-specific, read-only native bundle.
std::span<const Program> programs_for_sdk() noexcept;
#else
inline std::span<const Program> programs_for_sdk() noexcept { return {}; }
#endif

// Preserve the selected SPIR-V/DXIL program on those backends. Metal receives
// only the complete precompiled shader, never an unrelated backend's bytes or
// a runtime compiler that silently chooses different floating-point settings.
inline bool select(SDL_GPUShaderCreateInfo& shader, SDL_GPUShaderFormat formats,
    const char* name) {
    // One create-info is reused by the vertex and fragment factories. Recheck
    // Metal's recipe even when the prior factory already selected METALLIB.
    if (shader.format != SDL_GPU_SHADERFORMAT_METALLIB && (formats & shader.format) != 0) return true;
    if ((formats & SDL_GPU_SHADERFORMAT_METALLIB) == 0 || !name || !shader.entrypoint)
        return SDL_SetError("No supported complete compositor shader payload");
    const Program* selected = nullptr;
    for (const auto& program : programs_for_sdk()) {
        if (!program.name || std::strcmp(program.name, name) != 0) continue;
        if (selected) return SDL_SetError("Duplicate native compositor shader payload");
        selected = &program;
    }
    if (!selected || !selected->entry || !selected->bytes || selected->size <= 92 ||
        selected->size > 1024ULL * 1024 * 1024 ||
        std::memcmp(selected->bytes, "MTLB", 4) != 0 ||
        std::strcmp(selected->entry, shader.entrypoint) != 0 ||
        selected->stage != shader.stage || selected->samplers != shader.num_samplers ||
        selected->storage_buffers != shader.num_storage_buffers ||
        selected->uniforms != shader.num_uniform_buffers || shader.num_storage_textures != 0)
        return SDL_SetError("Missing or incompatible complete native compositor shader payload");
    shader.code = selected->bytes;
    shader.code_size = selected->size;
    shader.format = SDL_GPU_SHADERFORMAT_METALLIB;
    return true;
}
} // namespace starfox::render::metal_composite
