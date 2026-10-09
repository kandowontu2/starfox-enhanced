# Guard every insertion against the exact pinned SDL source. Device extension
# options are validated upstream but were omitted from VkDeviceCreateInfo.
set(backend "${SOURCE_DIR}/src/gpu/vulkan/SDL_gpu_vulkan.c")
file(COPY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../src/render/sdl_vulkan_submit_timing.inc"
    "${SOURCE_DIR}/src/gpu/vulkan/sdl_vulkan_submit_timing.inc"
    ONLY_IF_DIFFERENT)
file(COPY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../src/render/sdl_vulkan_bridge.inc"
    "${SOURCE_DIR}/src/gpu/vulkan/sdl_vulkan_bridge.inc"
    ONLY_IF_DIFFERENT)
file(COPY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../src/render/sdl_vulkan_xr_create.inc"
    "${SOURCE_DIR}/src/gpu/vulkan/sdl_vulkan_xr_create.inc"
    ONLY_IF_DIFFERENT)
file(READ "${backend}" code)
set(marker "/* Star Fox Vulkan interop v1 */")
set(ray_marker "/* Star Fox Vulkan ray options v1 */")
function(replace_exact old new)
    string(FIND "${code}" "${old}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Unexpected pinned SDL Vulkan source: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" code "${code}")
    set(code "${code}" PARENT_SCOPE)
endfunction()
set(xr_marker "/* Star Fox Vulkan OpenXR creation v1 */")
string(FIND "${code}" "${xr_marker}" xr_applied)
if(xr_applied LESS 0)
    replace_exact("#include <SDL3/SDL_vulkan.h>"
        "#include <SDL3/SDL_vulkan.h>\n#include \"starfox/render/sdl_vulkan_bridge.h\"")
    replace_exact("struct VulkanRenderer\n{\n    VkInstance instance;"
        "struct VulkanRenderer\n{\n    ${xr_marker}\n    StarfoxSdlVulkanXrCreateV1 starfoxXrCreate;\n    VkInstance instance;")
    replace_exact("static void VULKAN_DestroyDevice("
        "#include \"sdl_vulkan_xr_create.inc\"\n\nstatic void VULKAN_DestroyDevice(")
    replace_exact("    renderer->vkDestroyDevice(renderer->logicalDevice, NULL);\n    renderer->vkDestroyInstance(renderer->instance, NULL);"
        "    Starfox_Vulkan_XrDestroyHandles(renderer);")
    replace_exact("                             : VK_MAKE_VERSION(1, 0, 0);"
        "                             : (renderer->starfoxXrCreate.user ? renderer->starfoxXrCreate.default_api_version : VK_MAKE_VERSION(1, 0, 0));")
    replace_exact("    vulkanResult = vkCreateInstance(&createInfo, NULL, &renderer->instance);"
        "    vulkanResult = renderer->starfoxXrCreate.user\n        ? renderer->starfoxXrCreate.create_instance(renderer->starfoxXrCreate.user, vkGetInstanceProcAddr, &createInfo, NULL, &renderer->instance)\n        : vkCreateInstance(&createInfo, NULL, &renderer->instance);")
    replace_exact("    // Any suitable device will do, but we'd like the best"
        "    VkPhysicalDevice requiredPhysicalDevice = VK_NULL_HANDLE;\n    if (renderer->starfoxXrCreate.user &&\n        (renderer->starfoxXrCreate.physical_device(renderer->starfoxXrCreate.user, renderer->instance, &requiredPhysicalDevice) != VK_SUCCESS || !requiredPhysicalDevice)) {\n        SDL_stack_free(physicalDevices);\n        SDL_stack_free(physicalDeviceExtensions);\n        return 0;\n    }\n\n    // Any suitable device will do, but we'd like the best")
    replace_exact("        Uint64 deviceRank;\n\n        if (!VULKAN_INTERNAL_IsDeviceSuitable("
        "        Uint64 deviceRank;\n\n        if (requiredPhysicalDevice && physicalDevices[i] != requiredPhysicalDevice) continue;\n\n        if (!VULKAN_INTERNAL_IsDeviceSuitable(")
    replace_exact("    vulkanResult = renderer->vkCreateDevice(\n        renderer->physicalDevice,\n        &deviceCreateInfo,\n        NULL,\n        &renderer->logicalDevice);"
        "    vulkanResult = renderer->starfoxXrCreate.user\n        ? renderer->starfoxXrCreate.create_device(renderer->starfoxXrCreate.user, vkGetInstanceProcAddr, renderer->instance, renderer->physicalDevice, &deviceCreateInfo, NULL, &renderer->logicalDevice)\n        : renderer->vkCreateDevice(renderer->physicalDevice, &deviceCreateInfo, NULL, &renderer->logicalDevice);")
    replace_exact("    SDL_zerop(features);\n\n    // Opt out device features"
        "    SDL_zerop(features);\n    if (!Starfox_Vulkan_XrPrepare(renderer, props)) return false;\n\n    // Opt out device features")
    replace_exact("        if (result) {\n            renderer->vkDestroyInstance(renderer->instance, NULL);\n        }"
        "        Starfox_Vulkan_XrDestroyHandles(renderer);")
    replace_exact("        SET_STRING_ERROR(\"Failed to initialize Vulkan!\");\n        SDL_free(renderer);"
        "        SET_STRING_ERROR(\"Failed to initialize Vulkan!\");\n        Starfox_Vulkan_XrDestroyHandles(renderer);\n        SDL_free(renderer);")
    replace_exact("        SET_STRING_ERROR(\"Failed to create logical device!\");\n        SDL_free(renderer);"
        "        SET_STRING_ERROR(\"Failed to create logical device!\");\n        Starfox_Vulkan_XrDestroyHandles(renderer);\n        SDL_DestroyProperties(renderer->props);\n        SDL_free(renderer);")
    replace_exact("    renderer->props = SDL_CreateProperties();\n    if (verboseLogs) {"
        "    renderer->props = SDL_CreateProperties();\n    if (renderer->starfoxXrCreate.user) SDL_SetPointerProperty(renderer->props, STARFOX_SDL_VULKAN_XR_CREATION_CONTEXT, renderer->starfoxXrCreate.user);\n    if (verboseLogs) {")
endif()
set(cleanup_marker "/* Star Fox Vulkan failed creation cleanup v1 */")
set(submit_timing_marker "/* Star Fox Vulkan submit timing diagnostics v1 */")
string(FIND "${code}" "${submit_timing_marker}" submit_timing_applied)
if(submit_timing_applied LESS 0)
    replace_exact("    StarfoxSdlVulkanXrCreateV1 starfoxXrCreate;"
        "    StarfoxSdlVulkanXrCreateV1 starfoxXrCreate;\n    ${submit_timing_marker}\n    void *starfoxSubmitTimings;")
    replace_exact("#include \"sdl_vulkan_xr_create.inc\"\n\nstatic void VULKAN_DestroyDevice("
        "#include \"sdl_vulkan_xr_create.inc\"\n#include \"sdl_vulkan_submit_timing.inc\"\n\nstatic void VULKAN_DestroyDevice(")
    replace_exact("    SDL_free(renderer->submittedCommandBuffers);"
        "    Starfox_Vulkan_DestroySubmitTimings(renderer);\n    SDL_free(renderer->submittedCommandBuffers);")
    replace_exact("    bool performCleanups =\n        (renderer->claimedWindowCount > 0 && vulkanCommandBuffer->swapchainRequested) ||\n        renderer->claimedWindowCount == 0;\n\n    SDL_LockMutex(renderer->submitLock);"
        "    const bool starfoxTiming = renderer->starfoxSubmitTimings != NULL;\n    Uint64 starfoxMarks[7] = {0};\n    bool performCleanups =\n        (renderer->claimedWindowCount > 0 && vulkanCommandBuffer->swapchainRequested) ||\n        renderer->claimedWindowCount == 0;\n\n    if (starfoxTiming) starfoxMarks[0] = SDL_GetTicksNS();\n    SDL_LockMutex(renderer->submitLock);\n    if (starfoxTiming) starfoxMarks[1] = SDL_GetTicksNS();")
    replace_exact("    if (!VULKAN_INTERNAL_EndCommandBuffer(renderer, vulkanCommandBuffer)) {"
        "    if (starfoxTiming) starfoxMarks[2] = SDL_GetTicksNS();\n    if (!VULKAN_INTERNAL_EndCommandBuffer(renderer, vulkanCommandBuffer)) {")
    replace_exact("    vulkanCommandBuffer->inFlightFence = VULKAN_INTERNAL_AcquireFenceFromPool(renderer);"
        "    if (starfoxTiming) starfoxMarks[3] = SDL_GetTicksNS();\n    vulkanCommandBuffer->inFlightFence = VULKAN_INTERNAL_AcquireFenceFromPool(renderer);")
    replace_exact("    vulkanResult = renderer->vkQueueSubmit(\n        renderer->unifiedQueue,\n        1,\n        &submitInfo,\n        vulkanCommandBuffer->inFlightFence->fence);"
        "    if (starfoxTiming) starfoxMarks[4] = SDL_GetTicksNS();\n    vulkanResult = renderer->vkQueueSubmit(\n        renderer->unifiedQueue,\n        1,\n        &submitInfo,\n        vulkanCommandBuffer->inFlightFence->fence);\n    if (starfoxTiming) starfoxMarks[5] = SDL_GetTicksNS();")
    replace_exact("    // Mark command buffer as submitted\n    VULKAN_INTERNAL_ReleaseCommandBuffer(vulkanCommandBuffer);\n\n    SDL_UnlockMutex(renderer->submitLock);"
        "    // Mark command buffer as submitted\n    VULKAN_INTERNAL_ReleaseCommandBuffer(vulkanCommandBuffer);\n    if (starfoxTiming) {\n        starfoxMarks[6] = SDL_GetTicksNS();\n        Starfox_Vulkan_RecordSubmitTimings(renderer, starfoxMarks, vulkanCommandBuffer->swapchainRequested);\n    }\n\n    SDL_UnlockMutex(renderer->submitLock);")
endif()
string(FIND "${code}" "${cleanup_marker}" cleanup_applied)
if(cleanup_applied LESS 0)
    replace_exact("    if (vulkanResult != VK_SUCCESS) {\n        CHECK_VULKAN_ERROR_AND_RETURN(vulkanResult, vkCreateInstance, 0);"
        "    if (vulkanResult != VK_SUCCESS) {\n        ${cleanup_marker}\n        renderer->instance = VK_NULL_HANDLE;\n        CHECK_VULKAN_ERROR_AND_RETURN(vulkanResult, vkCreateInstance, 0);")
    replace_exact("    SDL_stack_free((void *)deviceExtensions);\n    CHECK_VULKAN_ERROR_AND_RETURN(vulkanResult, vkCreateDevice, 0);"
        "    SDL_stack_free((void *)deviceExtensions);\n    if (vulkanResult != VK_SUCCESS) renderer->logicalDevice = VK_NULL_HANDLE;\n    CHECK_VULKAN_ERROR_AND_RETURN(vulkanResult, vkCreateDevice, 0);")
endif()
set(sampler_marker "/* Star Fox Vulkan compute sampler barriers v1 */")
string(FIND "${code}" "${sampler_marker}" sampler_applied)
if(sampler_applied LESS 0)
    # SDL's sampler default is used by both graphics and compute passes. An
    # exposure/environment compute read must finish before a recycled colour
    # target is written, and must see the preceding colour attachment writes.
    replace_exact("        srcStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;\n        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;\n        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;"
        "        ${sampler_marker}\n        srcStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;\n        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;\n        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;")
    replace_exact("        dstStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;\n        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;\n        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;"
        "        dstStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;\n        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;\n        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;")
endif()
set(pin_marker "/* Star Fox Vulkan recorded allocation pinning v1 */")
string(FIND "${code}" "${pin_marker}" pin_applied)
if(pin_applied LESS 0)
    # Lazy resource preparation can submit a texture-initialization command
    # while another command is still being recorded. Moving that command's
    # attachment/sampler allocation would copy its old contents before its
    # recorded writes run. Pin all referenced allocations until retirement,
    # not only explicit transfer operands. Existing transfer lists deduplicate
    # the references and release them on both submission and cancellation.
    replace_exact("static void VULKAN_INTERNAL_TrackBuffer(\n    VulkanCommandBuffer *commandBuffer,\n    VulkanBuffer *buffer)\n{\n    TRACK_RESOURCE("
        "${pin_marker}\nstatic void VULKAN_INTERNAL_TrackBufferTransfer(VulkanCommandBuffer *commandBuffer, VulkanBuffer *buffer);\nstatic void VULKAN_INTERNAL_TrackTextureTransfer(VulkanCommandBuffer *commandBuffer, VulkanTexture *texture);\n\nstatic void VULKAN_INTERNAL_TrackBuffer(\n    VulkanCommandBuffer *commandBuffer,\n    VulkanBuffer *buffer)\n{\n    if (buffer->usedRegion && buffer->type != VULKAN_BUFFER_TYPE_TRANSFER) VULKAN_INTERNAL_TrackBufferTransfer(commandBuffer, buffer);\n    TRACK_RESOURCE(")
    replace_exact("static void VULKAN_INTERNAL_TrackTexture(\n    VulkanCommandBuffer *commandBuffer,\n    VulkanTexture *texture)\n{\n    TRACK_RESOURCE("
        "static void VULKAN_INTERNAL_TrackTexture(\n    VulkanCommandBuffer *commandBuffer,\n    VulkanTexture *texture)\n{\n    VULKAN_INTERNAL_TrackTextureTransfer(commandBuffer, texture);\n    TRACK_RESOURCE(")
endif()
string(FIND "${code}" "${ray_marker}" ray_applied)
if(ray_applied GREATER_EQUAL 0)
    if(xr_applied LESS 0 OR cleanup_applied LESS 0 OR sampler_applied LESS 0 OR pin_applied LESS 0 OR submit_timing_applied LESS 0)
        file(WRITE "${backend}" "${code}")
    endif()
    return()
endif()
string(FIND "${code}" "${marker}" applied)
if(applied LESS 0)
    replace_exact("        &renderer->supports);\n    deviceExtensions = SDL_stack_alloc("
        "        &renderer->supports) + features->additionalDeviceExtensionCount;\n    deviceExtensions = SDL_stack_alloc(")
    replace_exact("    CreateDeviceExtensionArray(&renderer->supports, deviceExtensions);"
        "    CreateDeviceExtensionArray(&renderer->supports, deviceExtensions);\n    for (Uint32 extra = 0; extra < features->additionalDeviceExtensionCount; ++extra) {\n        deviceExtensions[GetDeviceExtensionCount(&renderer->supports) + extra] = features->additionalDeviceExtensionNames[extra];\n    }")
    replace_exact("static SDL_GPUDevice *VULKAN_CreateDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)"
        "${marker}\n#include \"sdl_vulkan_bridge.inc\"\n\nstatic SDL_GPUDevice *VULKAN_CreateDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)")
    replace_exact("    result->driverData = (SDL_GPURenderer *)renderer;"
        "    result->driverData = (SDL_GPURenderer *)renderer;\n    Starfox_Vulkan_PublishBridge(renderer);")
endif()
replace_exact("    VkPhysicalDeviceVulkan13Features desiredVulkan13DeviceFeatures;"
    "    VkPhysicalDeviceVulkan13Features desiredVulkan13DeviceFeatures;\n    VkPhysicalDeviceAccelerationStructureFeaturesKHR desiredAccelerationStructure;\n    VkPhysicalDeviceRayQueryFeaturesKHR desiredRayQuery;")
replace_exact("            features->desiredVulkan13DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;"
    "            features->desiredVulkan13DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;\n            SDL_zero(features->desiredAccelerationStructure);\n            SDL_zero(features->desiredRayQuery);\n            features->desiredAccelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;\n            features->desiredRayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;")
replace_exact("                        VULKAN_INTERNAL_TryAddDeviceFeatures_Vulkan_12_Or_Later(vk10Features,\n                                                                                vk11Features,\n                                                                                vk12Features,\n                                                                                vk13Features,\n                                                                                features->desiredApiVersion,\n                                                                                nextStructure);\n                        nextStructure = nextStructure->pNext;"
    "                        VULKAN_INTERNAL_TryAddDeviceFeatures_Vulkan_12_Or_Later(vk10Features,\n                                                                                vk11Features,\n                                                                                vk12Features,\n                                                                                vk13Features,\n                                                                                features->desiredApiVersion,\n                                                                                nextStructure);\n                        if (nextStructure->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR) {\n                            features->desiredAccelerationStructure.accelerationStructure = ((VkPhysicalDeviceAccelerationStructureFeaturesKHR *)nextStructure)->accelerationStructure;\n                        } else if (nextStructure->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR) {\n                            features->desiredRayQuery.rayQuery = ((VkPhysicalDeviceRayQueryFeaturesKHR *)nextStructure)->rayQuery;\n                        }\n                        nextStructure = nextStructure->pNext;")
replace_exact("        deviceCreateInfo.pEnabledFeatures = NULL;\n        deviceCreateInfo.pNext = &featureList;"
    "        deviceCreateInfo.pEnabledFeatures = NULL;\n        deviceCreateInfo.pNext = &featureList;\n        ${ray_marker}\n        if (features->desiredAccelerationStructure.accelerationStructure && features->desiredRayQuery.rayQuery) {\n            VkBaseOutStructure *tail = (VkBaseOutStructure *)&featureList;\n            while (tail->pNext) tail = tail->pNext;\n            tail->pNext = (VkBaseOutStructure *)&features->desiredAccelerationStructure;\n            features->desiredAccelerationStructure.pNext = &features->desiredRayQuery;\n            features->desiredRayQuery.pNext = NULL;\n        }")
file(WRITE "${backend}" "${code}")
