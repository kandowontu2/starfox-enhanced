#pragma once
#include "starfox/render/gpu_preparation.hpp"

namespace starfox::render {

// Teardown only: retain the device, fence array and resources until the real
// backend wait has joined. SDL may retire completed backend command buffers
// inside this wait; no app command acquisition/recording/submission, cache
// mutation, resource release or presentation moves to the worker. Normal
// per-frame waits remain unchanged (no polling/thread overhead on that path).
inline bool wait_gpu_retirement(SDL_GPUDevice* device,bool wait_all,
    SDL_GPUFence* const* fences,Uint32 count) noexcept {
    try {
        return prepare_gpu_resource([&] {
            return SDL_WaitForGPUFences(device,wait_all,fences,count);
        },"retirement-fences");
    } catch(...) {
        // An unavailable worker must not permit releasing in-flight resources.
        // Preserve SDL's ordinary blocking retirement as the safe fallback.
        return SDL_WaitForGPUFences(device,wait_all,fences,count);
    }
}

} // namespace starfox::render
