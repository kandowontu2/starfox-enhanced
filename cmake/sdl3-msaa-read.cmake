# Private opt-in Texture2DMS support for our exact pinned SDL GPU backends.
# No ordinary texture, pipeline, descriptor layout or validation rule changes.
set(common "${SOURCE_DIR}/src/gpu/SDL_gpu.c")
file(READ "${common}" code)
string(FIND "${code}" "/* Star Fox multisample shader read v1 */" installed)
if(installed LESS 0)
    set(anchor [=[        if (createinfo->sample_count > SDL_GPU_SAMPLECOUNT_1 &&
            (createinfo->usage & (SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                  SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ |
                                  SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ |
                                  SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE))) {]=])
    set(replacement [=[        /* Star Fox multisample shader read v1 */
        const bool starfox_multisample_read =
            SDL_GetBooleanProperty(createinfo->props, "starfox.gpu.multisample-read.v1", false) &&
            SDL_GetBooleanProperty(SDL_GetGPUDeviceProperties(device), "starfox.gpu.multisample-read.v1", false) &&
            createinfo->type == SDL_GPU_TEXTURETYPE_2D && createinfo->num_levels == 1 &&
            createinfo->layer_count_or_depth == 1 && (createinfo->usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        if (createinfo->sample_count > SDL_GPU_SAMPLECOUNT_1 &&
            ((createinfo->usage & (SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ |
                                   SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ |
                                   SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE)) ||
             ((createinfo->usage & SDL_GPU_TEXTUREUSAGE_SAMPLER) && !starfox_multisample_read))) {]=])
    string(FIND "${code}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Pinned SDL multisample validation anchor missing")
    endif()
    string(REPLACE "${anchor}" "${replacement}" code "${code}")
    file(WRITE "${common}" "${code}")
endif()
set(d3d "${SOURCE_DIR}/src/gpu/d3d12/SDL_gpu_d3d12.c")
file(READ "${d3d}" code)
# Preserve the existing interop patch's contiguous publication anchor on every
# reconfigure. Upgrade the first development insertion without repatching SDL.
set(device_property "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_DEVICE, renderer->device);")
set(bridge_property "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_BRIDGE, (void *)&starfox_d3d12_bridge);")
set(read_property "    SDL_SetBooleanProperty(renderer->props, \"starfox.gpu.multisample-read.v1\", true);")
string(FIND "${code}" "${device_property}\n${read_property}\n${bridge_property}" early_capability)
if(early_capability GREATER_EQUAL 0)
    string(REPLACE "${device_property}\n${read_property}\n${bridge_property}"
        "${device_property}\n${bridge_property}\n${read_property}" code "${code}")
    file(WRITE "${d3d}" "${code}")
endif()
string(FIND "${code}" "/* Star Fox multisample SRV v1 */" installed)
if(installed LESS 0)
    set(anchor "        } else {\n            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;")
    string(FIND "${code}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Pinned SDL multisample D3D12 SRV anchor missing")
    endif()
    string(REPLACE "${anchor}" "        } else if (isMultisample) {\n            /* Star Fox multisample SRV v1 */\n            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;\n        } else {\n            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;" code "${code}")
    set(anchor "${bridge_property}")
    string(FIND "${code}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Pinned SDL multisample D3D12 capability anchor missing")
    endif()
    string(REPLACE "${anchor}" "${anchor}\n    SDL_SetBooleanProperty(renderer->props, \"starfox.gpu.multisample-read.v1\", true);" code "${code}")
    file(WRITE "${d3d}" "${code}")
endif()
set(vulkan "${SOURCE_DIR}/src/gpu/vulkan/SDL_gpu_vulkan.c")
file(READ "${vulkan}" code)
string(FIND "${code}" "/* Star Fox multisample Vulkan capability v1 */" installed)
if(installed LESS 0)
    set(anchor "    Starfox_Vulkan_PublishBridge(renderer);")
    string(FIND "${code}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Pinned SDL multisample Vulkan capability anchor missing")
    endif()
    set(replacement [=[    Starfox_Vulkan_PublishBridge(renderer);
    /* Star Fox multisample Vulkan capability v1 */
    {
        VkPhysicalDeviceProperties starfox_sample_properties;
        renderer->vkGetPhysicalDeviceProperties(renderer->physicalDevice, &starfox_sample_properties);
        SDL_SetBooleanProperty(renderer->props, "starfox.gpu.multisample-read.v1",
            starfox_sample_properties.limits.standardSampleLocations == VK_TRUE);
    }]=])
    string(REPLACE "${anchor}" "${replacement}" code "${code}")
    file(WRITE "${vulkan}" "${code}")
endif()
