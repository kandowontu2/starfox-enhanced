#pragma once

#include "starfox/render/responsive_preparation.hpp"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_error.h>
#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>

namespace starfox::render {

// SDL errors are thread-local. Return the compiler's actual failure to the
// owner rather than reporting an unrelated stale error from its event pump.
template<class Create> auto prepare_gpu_resource(Create&& create,const char* kind=nullptr) {
    using Resource=std::invoke_result_t<Create>;
    struct Result {Resource resource{};std::array<char,1024> error{};};
    const bool trace=kind && std::getenv("STARFOX_TRACE_GPU");
    const auto started=std::chrono::steady_clock::now();
    if(trace) std::cerr<<"gpu-prepare-resource: begin "<<kind<<'\n';
    const auto result=responsive_prepare([&] {
        Result value;value.resource=create();
        if(!value.resource) {
            const auto* error=SDL_GetError();
            std::strncpy(value.error.data(),error?error:"GPU preparation failed",value.error.size()-1);
        }
        return value;
    });
    if(trace) std::cerr<<"gpu-prepare-resource: end "<<kind<<" ms="
        <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()<<'\n';
    if(!result.resource) SDL_SetError("%s",result.error.data());
    return result.resource;
}
inline SDL_GPUComputePipeline* create_gpu_compute_pipeline(SDL_GPUDevice* device,
    const SDL_GPUComputePipelineCreateInfo* info) {
    return prepare_gpu_resource([&] {return SDL_CreateGPUComputePipeline(device,info);},"compute");
}
inline SDL_GPUGraphicsPipeline* create_gpu_graphics_pipeline(SDL_GPUDevice* device,
    const SDL_GPUGraphicsPipelineCreateInfo* info) {
    return prepare_gpu_resource([&] {return SDL_CreateGPUGraphicsPipeline(device,info);},"graphics");
}
inline SDL_GPUShader* create_gpu_shader(SDL_GPUDevice* device,const SDL_GPUShaderCreateInfo* info) {
    return prepare_gpu_resource([&] {return SDL_CreateGPUShader(device,info);},"shader");
}

} // namespace starfox::render
