# Private joined resource compilation for pinned SDL presenter/blit pipelines.
# Device/window/cache/command ownership stays on the calling thread.
set(source "${SOURCE_DIR}/src/render/gpu/SDL_render_gpu.c")
file(READ "${source}" code)
set(marker "/* Star Fox responsive presenter pipelines v1 */")
string(FIND "${code}" "${marker}" installed)
if(installed LESS 0)
    set(anchor "    if (!GPU_InitPipelineCache(&data->pipeline_cache, data->device)) {")
    string(FIND "${code}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Pinned SDL presenter pipeline-cache anchor missing")
    endif()
    set(replacement [=[    /* Star Fox responsive presenter pipelines v1 */
    const StarfoxSdlGpuPreparation *starfox_pipeline_preparation =
        (const StarfoxSdlGpuPreparation *)SDL_GetPointerProperty(create_props, STARFOX_SDL_GPU_PIPELINE_PREPARATION, NULL);
    if (starfox_pipeline_preparation && starfox_pipeline_preparation->version == 1 && starfox_pipeline_preparation->prepare) {
        SDL_SetPointerProperty(SDL_GetGPUDeviceProperties(data->device), STARFOX_SDL_GPU_PIPELINE_PREPARATION, (void *)starfox_pipeline_preparation);
        SDL_SetBooleanProperty(SDL_GetRendererProperties(renderer), STARFOX_SDL_GPU_PIPELINE_PREPARATION, true);
    }

    if (!GPU_InitPipelineCache(&data->pipeline_cache, data->device)) {]=])
    string(REPLACE "${anchor}" "${replacement}" code "${code}")
    file(WRITE "${source}" "${code}")
endif()
foreach(relative "src/render/gpu/SDL_pipeline_gpu.c" "src/gpu/SDL_gpu.c")
    set(source "${SOURCE_DIR}/${relative}")
    file(READ "${source}" code)
    string(FIND "${code}" "${marker}" installed)
    if(installed LESS 0)
        if(relative STREQUAL "src/render/gpu/SDL_pipeline_gpu.c")
            set(anchor "    return SDL_CreateGPUGraphicsPipeline(device, &pci);")
            set(replacement "    return Starfox_SdlPreparePipeline(device, &pci);")
        else()
            set(anchor "    pipeline = SDL_CreateGPUGraphicsPipeline(\n        device,\n        &blit_pipeline_create_info);")
            set(replacement "    pipeline = Starfox_SdlPreparePipeline(\n        device,\n        &blit_pipeline_create_info);")
        endif()
        string(FIND "${code}" "${anchor}" found)
        if(found LESS 0)
            message(FATAL_ERROR "Pinned SDL graphics resource anchor missing: ${relative}")
        endif()
        string(REPLACE "${anchor}" "${replacement}" code "${code}")
        string(REPLACE "#include \"SDL_internal.h\"" "#include \"SDL_internal.h\"\n${marker}\n#include \"starfox/render/sdl_gpu_pipeline_preparation.h\"" code "${code}")
        file(WRITE "${source}" "${code}")
    endif()
endforeach()
