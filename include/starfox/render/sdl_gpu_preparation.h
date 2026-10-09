#pragma once
#include <SDL3/SDL_stdinc.h>

/* Private opt-in extension of the pinned SDL GPU renderer. Only its built-in
 * shader compilation is borrowed by this joined callback. Renderer/window,
 * device, command-buffer and swapchain ownership remain on the calling thread.
 * The hook and every borrowed argument must remain alive until prepare returns.
 */
#define STARFOX_SDL_GPU_PREPARATION "starfox.sdl.gpu.preparation.v1"
/* Separate retained hook for presenter/blit pipeline resource creation. Its
 * table/user must outlive the GPU device; borrowed create-info lives until
 * prepare returns. Cache mutations and commands remain on their owner. */
#define STARFOX_SDL_GPU_PIPELINE_PREPARATION "starfox.sdl.gpu.pipeline.preparation.v1"
typedef struct StarfoxSdlGpuPreparation {
    Uint32 version;
    void *user;
    bool (SDLCALL *prepare)(void *user, bool (SDLCALL *compile)(void *), void *argument);
} StarfoxSdlGpuPreparation;
