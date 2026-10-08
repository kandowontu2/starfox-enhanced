/* Creation/lifetime half of the optional OpenXR Vulkan2 handshake. Included
 * before backend destruction/creation functions, after VulkanRenderer. */
static bool Starfox_Vulkan_XrPrepare(VulkanRenderer *renderer, SDL_PropertiesID props)
{
    const StarfoxSdlVulkanXrCreateV1 *api = SDL_GetPointerProperty(props, STARFOX_SDL_VULKAN_XR_CREATE, NULL);
    if (!api) {
        if (SDL_HasProperty(props, STARFOX_SDL_VULKAN_XR_CREATE))
            return SDL_SetError("Null OpenXR Vulkan creation interface");
        return true;
    }
    if (api->version != 1 || !api->default_api_version || !api->user || !api->retain || !api->release ||
        !api->create_instance || !api->physical_device || !api->create_device ||
        !api->instance_destroyed || !api->device_destroyed)
        return SDL_SetError("Invalid OpenXR Vulkan creation interface");
    if (!api->retain(api->user)) return SDL_SetError("OpenXR Vulkan creation was revoked");
    renderer->starfoxXrCreate = *api;
    return true;
}
static void Starfox_Vulkan_XrDestroyHandles(VulkanRenderer *renderer)
{
    if (renderer->logicalDevice) {
        PFN_vkDestroyDevice destroy = renderer->vkDestroyDevice;
        if (!destroy) destroy = (PFN_vkDestroyDevice)vkGetInstanceProcAddr(renderer->instance, "vkDestroyDevice");
        destroy(renderer->logicalDevice, NULL);
        if (renderer->starfoxXrCreate.user)
            renderer->starfoxXrCreate.device_destroyed(renderer->starfoxXrCreate.user, renderer->logicalDevice);
        renderer->logicalDevice = VK_NULL_HANDLE;
    }
    if (renderer->instance) {
        PFN_vkDestroyInstance destroy = renderer->vkDestroyInstance;
        if (!destroy) destroy = (PFN_vkDestroyInstance)vkGetInstanceProcAddr(renderer->instance, "vkDestroyInstance");
        destroy(renderer->instance, NULL);
        if (renderer->starfoxXrCreate.user)
            renderer->starfoxXrCreate.instance_destroyed(renderer->starfoxXrCreate.user, renderer->instance);
        renderer->instance = VK_NULL_HANDLE;
    }
    if (renderer->starfoxXrCreate.user) {
        renderer->starfoxXrCreate.release(renderer->starfoxXrCreate.user);
        SDL_zero(renderer->starfoxXrCreate);
    }
}
