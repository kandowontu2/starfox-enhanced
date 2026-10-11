#include "starfox/render/gpu_calibrated_ray_composite.hpp"
#include "starfox/render/sdl_metal_composite_payload.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>

using namespace starfox::render::metal_composite;
namespace {
void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "GRAPHICS_LINK_FAILURE %s\n", message); std::exit(1); }
}
auto fields(const SDL_GPUShaderCreateInfo& p) {
    return std::tie(p.code_size, p.code, p.entrypoint, p.format, p.stage,
        p.num_samplers, p.num_storage_textures, p.num_storage_buffers, p.num_uniform_buffers, p.props);
}
SDL_GPUShaderCreateInfo descriptor(const Program& p) {
    SDL_GPUShaderCreateInfo result{};
    result.code_size = 4;
    result.code = reinterpret_cast<const unsigned char*>("DXIL");
    result.entrypoint = p.entry;
    result.format = SDL_GPU_SHADERFORMAT_DXIL;
    result.stage = p.stage;
    result.num_samplers = p.samplers;
    result.num_storage_buffers = p.storage_buffers;
    result.num_uniform_buffers = p.uniforms;
    result.props = 39;
    return result;
}
void refuse(SDL_GPUShaderCreateInfo p, const char* name) {
    const auto original = p;
    require(!select(p, SDL_GPU_SHADERFORMAT_METALLIB, name), "incorrect graphics descriptor accepted");
    require(fields(p) == fields(original), "graphics refusal changed caller descriptor");
}
}
void check_graphics_link() {
    starfox::render::GpuCalibratedRayComposite owner;
    require(!owner.initialize(nullptr, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM), "null graphics device accepted");
    require(owner.status() == "Calibrated ray compositor requires a GPU", "null graphics owner contract changed");
    owner.release_device();
    const auto programs = programs_for_sdk();
    require(programs.size() == 5, "complete five-stage SDK family absent");
    SDL_GPUShaderCreateInfo reused{};
    for (const auto& p : programs) {
        const auto original = descriptor(p);
        auto selected = original, expected = original;
        expected.code = p.bytes; expected.code_size = p.size; expected.format = SDL_GPU_SHADERFORMAT_METALLIB;
        require(select(selected, SDL_GPU_SHADERFORMAT_METALLIB, p.name), "native graphics payload absent");
        require(fields(selected) == fields(expected), "selection changed graphics resources/stage/extension");
        require(reinterpret_cast<std::uintptr_t>(selected.code) % 16 == 0, "graphics native payload misaligned");
        for (const auto format : {SDL_GPU_SHADERFORMAT_DXIL, SDL_GPU_SHADERFORMAT_SPIRV}) {
            auto native = original; native.format = format; const auto saved = native;
            require(select(native, format, p.name) && fields(native) == fields(saved), "non-Metal shader changed");
        }
        auto bad = original; bad.entrypoint = nullptr; refuse(bad, p.name);
        bad = original; bad.entrypoint = "missing_graphics_entry"; refuse(bad, p.name);
        refuse(original, "missing_graphics_variant");
        for (const auto member : {&SDL_GPUShaderCreateInfo::num_samplers,
            &SDL_GPUShaderCreateInfo::num_storage_textures, &SDL_GPUShaderCreateInfo::num_storage_buffers,
            &SDL_GPUShaderCreateInfo::num_uniform_buffers}) {
            bad = original; ++(bad.*member); refuse(bad, p.name);
        }
        bad = original; bad.stage = p.stage == SDL_GPU_SHADERSTAGE_VERTEX ? SDL_GPU_SHADERSTAGE_FRAGMENT : SDL_GPU_SHADERSTAGE_VERTEX;
        refuse(bad, p.name);
        // Production initialization reuses one descriptor across both stages.
        // Deliberately retain the previous Metal format while changing recipe.
        reused = original; reused.format = SDL_GPU_SHADERFORMAT_METALLIB;
        reused.code = reinterpret_cast<const unsigned char*>("SPRV");
        require(select(reused, SDL_GPU_SHADERFORMAT_METALLIB, p.name) && fields(reused) == fields(expected),
            "reused descriptor retained previous or foreign shader bytes");
        std::printf("GRAPHICS_PAYLOAD_PASS %s\n", p.name);
    }
    std::printf("ALL5_GRAPHICS_FACTORY_LINK_PASS; no GPU or gameplay execution\n");
}
