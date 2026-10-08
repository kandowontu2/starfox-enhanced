#pragma once
#include "starfox/render/sdl_gpu_preparation.h"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_error.h>

/* Private pinned-SDL helper: only the resource API below runs in the callback.
 * Nothing from the command buffer or presenter/blit cache is borrowed. */
typedef struct StarfoxSdlGpuPipelineCreate {
    SDL_GPUDevice *device;
    const SDL_GPUGraphicsPipelineCreateInfo *info;
    SDL_GPUGraphicsPipeline *result;
} StarfoxSdlGpuPipelineCreate;
static bool SDLCALL Starfox_SdlCreatePipeline(void *argument) {
    StarfoxSdlGpuPipelineCreate *request = (StarfoxSdlGpuPipelineCreate *)argument;
    request->result = SDL_CreateGPUGraphicsPipeline(request->device, request->info);
    return request->result != NULL;
}
static SDL_GPUGraphicsPipeline *Starfox_SdlPreparePipeline(SDL_GPUDevice *device,
    const SDL_GPUGraphicsPipelineCreateInfo *info) {
    const StarfoxSdlGpuPreparation *hook = (const StarfoxSdlGpuPreparation *)
        SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device), STARFOX_SDL_GPU_PIPELINE_PREPARATION, NULL);
    if (hook && hook->version == 1 && hook->prepare) {
        StarfoxSdlGpuPipelineCreate request = {device, info, NULL};
        if (!hook->prepare(hook->user, Starfox_SdlCreatePipeline, &request)) {
            if (request.result) SDL_ReleaseGPUGraphicsPipeline(device, request.result);
            return NULL;
        }
        if (!request.result) SDL_SetError("Presenter pipeline callback produced no resource");
        return request.result;
    }
    return SDL_CreateGPUGraphicsPipeline(device, info);
}
