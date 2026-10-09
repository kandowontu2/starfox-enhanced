# Joined opt-in preparation of SDL's own presenter shaders. No window/device,
# command acquisition/submission or swapchain operation moves off its owner.
set(source "${SOURCE_DIR}/src/render/gpu/SDL_render_gpu.c")
file(READ "${source}" code)
set(marker "/* Star Fox responsive presenter shaders v1 */")
string(FIND "${code}" "${marker}" installed)
if(installed GREATER_EQUAL 0)
    return()
endif()
set(anchor "static bool GPU_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, SDL_PropertiesID create_props)")
set(thunk [=[/* Star Fox responsive presenter shaders v1 */
#include "starfox/render/sdl_gpu_preparation.h"
static bool SDLCALL Starfox_GPU_InitPresenterShaders(void *argument)
{
    GPU_RenderData *data = (GPU_RenderData *)argument;
    return GPU_InitShaders(&data->shaders, data->device);
}

]=])
string(FIND "${code}" "${anchor}" found)
if(found LESS 0)
    message(FATAL_ERROR "Pinned SDL GPU renderer initialization anchor missing")
endif()
string(REPLACE "${anchor}" "${thunk}${anchor}" code "${code}")
set(anchor "    if (!GPU_InitShaders(&data->shaders, data->device)) {")
set(replacement [=[    const StarfoxSdlGpuPreparation *starfox_preparation =
        (const StarfoxSdlGpuPreparation *)SDL_GetPointerProperty(create_props, STARFOX_SDL_GPU_PREPARATION, NULL);
    const bool starfox_joined = starfox_preparation && starfox_preparation->version == 1 && starfox_preparation->prepare;
    const bool starfox_shaders_ready = starfox_joined
        ? starfox_preparation->prepare(starfox_preparation->user, Starfox_GPU_InitPresenterShaders, data)
        : GPU_InitShaders(&data->shaders, data->device);
    SDL_SetBooleanProperty(SDL_GetRendererProperties(renderer), STARFOX_SDL_GPU_PREPARATION, starfox_joined);
    if (!starfox_shaders_ready) {]=])
string(FIND "${code}" "${anchor}" found)
if(found LESS 0)
    message(FATAL_ERROR "Pinned SDL GPU presenter shader anchor missing")
endif()
string(REPLACE "${anchor}" "${replacement}" code "${code}")
file(WRITE "${source}" "${code}")
