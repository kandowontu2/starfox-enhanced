// SDL3 video backend for the PS5: Vulkan WSI only, through the statically
// linked RADV driver and its VK_KHR_display presentation on VideoOut. There is
// deliberately no CPU framebuffer path; SDL_Renderer runs on SDL GPU/Vulkan.
#include "SDL_internal.h"
#include "video/SDL_sysvideo.h"
#include "video/SDL_vulkan_internal.h"
#include "events/SDL_keyboard_c.h"
#include "events/SDL_windowevents_c.h"
#include "native.h"
#include "display_mode.h"

// RADV links into the title without a loader; its ICD entry point resolves
// every global, instance and device command (PS5_Vulkan radv/radv_smoke.c).
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char *name);

// VideoOut drives one 3840x2160 display. RADV offers a 59.94 Hz mode and,
// when the title metadata enables it, a 119.88 Hz mode listed first.
#define PS_DISPLAY_WIDTH 3840
#define PS_DISPLAY_HEIGHT 2160
#define PS_TARGET_REFRESH_MILLIHERTZ 60000u

static bool PS_VideoInit(SDL_VideoDevice *device)
{
    (void)device;
    SDL_DisplayMode mode;
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = PS_DISPLAY_WIDTH;
    mode.h = PS_DISPLAY_HEIGHT;
    mode.refresh_rate_numerator = 60000;
    mode.refresh_rate_denominator = 1001;
    mode.pixel_density = 1.0f;
    return SDL_AddBasicVideoDisplay(&mode) != 0;
}

static void PS_VideoQuit(SDL_VideoDevice *device)
{
    (void)device;
    StarfoxPS5_CloseGamepads();
}

static bool PS_CreateWindow(SDL_VideoDevice *device, SDL_Window *window, SDL_PropertiesID props)
{
    (void)device;
    (void)props;
    // The window is the whole display plane.
    window->x = window->floating.x = window->windowed.x = 0;
    window->y = window->floating.y = window->windowed.y = 0;
    window->w = window->floating.w = window->windowed.w = PS_DISPLAY_WIDTH;
    window->h = window->floating.h = window->windowed.h = PS_DISPLAY_HEIGHT;
    SDL_SetKeyboardFocus(window);
    return true;
}

static void PS_SetWindowSize(SDL_VideoDevice *device, SDL_Window *window)
{
    (void)device;
    // VideoOut has one fixed resolution; keep reporting it.
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, PS_DISPLAY_WIDTH, PS_DISPLAY_HEIGHT);
}

static bool PS_LoadVulkan(SDL_VideoDevice *device, const char *path)
{
    if (path) {
        return SDL_SetError("PS5 Vulkan is statically linked; no library path is loaded");
    }
    device->vulkan_config.vkGetInstanceProcAddr = (SDL_FunctionPointer)vk_icdGetInstanceProcAddr;
    device->vulkan_config.vkEnumerateInstanceExtensionProperties =
        (SDL_FunctionPointer)vk_icdGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!device->vulkan_config.vkEnumerateInstanceExtensionProperties) {
        return SDL_SetError("RADV exposes no vkEnumerateInstanceExtensionProperties");
    }
    SDL_strlcpy(device->vulkan_config.loader_path, "radv-static", SDL_arraysize(device->vulkan_config.loader_path));
    return true;
}

static void PS_UnloadVulkan(SDL_VideoDevice *device)
{
    device->vulkan_config.vkGetInstanceProcAddr = NULL;
    device->vulkan_config.vkEnumerateInstanceExtensionProperties = NULL;
}

static char const *const *PS_GetInstanceExtensions(SDL_VideoDevice *device, Uint32 *count)
{
    (void)device;
    static const char *const names[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_DISPLAY_EXTENSION_NAME,
    };
    if (count) {
        *count = SDL_arraysize(names);
    }
    return names;
}

static bool PS_CreateSurface(SDL_VideoDevice *device, SDL_Window *window, VkInstance instance,
                             const struct VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
    (void)device;
    (void)window;
    PFN_vkEnumeratePhysicalDevices enumerate_devices =
        (PFN_vkEnumeratePhysicalDevices)vk_icdGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceDisplayPropertiesKHR get_displays = (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)
        vk_icdGetInstanceProcAddr(instance, "vkGetPhysicalDeviceDisplayPropertiesKHR");
    PFN_vkGetDisplayModePropertiesKHR get_modes =
        (PFN_vkGetDisplayModePropertiesKHR)vk_icdGetInstanceProcAddr(instance, "vkGetDisplayModePropertiesKHR");
    PFN_vkCreateDisplayPlaneSurfaceKHR create_surface =
        (PFN_vkCreateDisplayPlaneSurfaceKHR)vk_icdGetInstanceProcAddr(instance, "vkCreateDisplayPlaneSurfaceKHR");
    if (!enumerate_devices || !get_displays || !get_modes || !create_surface) {
        return SDL_SetError("The PS5 Vulkan driver lacks VK_KHR_display");
    }

    // RADV exposes the one GPU; asking for one is complete or VK_INCOMPLETE.
    uint32_t count = 1;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkResult result = enumerate_devices(instance, &count, &physical);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || count == 0) {
        return SDL_SetError("vkEnumeratePhysicalDevices: %s", SDL_Vulkan_GetResultString(result));
    }
    VkDisplayPropertiesKHR display;
    count = 1;
    result = get_displays(physical, &count, &display);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || count == 0) {
        return SDL_SetError("No VideoOut display: %s", SDL_Vulkan_GetResultString(result));
    }

    count = 0;
    result = get_modes(physical, display.display, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        return SDL_SetError("No VideoOut display mode: %s", SDL_Vulkan_GetResultString(result));
    }
    VkDisplayModePropertiesKHR *modes = SDL_calloc(count, sizeof(*modes));
    if (!modes) {
        return false;
    }
    result = get_modes(physical, display.display, &count, modes);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || count == 0) {
        SDL_free(modes);
        return SDL_SetError("vkGetDisplayModePropertiesKHR: %s", SDL_Vulkan_GetResultString(result));
    }
    uint32_t *refresh = SDL_calloc(count, sizeof(*refresh));
    if (!refresh) {
        SDL_free(modes);
        return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        refresh[i] = modes[i].parameters.refreshRate;
    }
    const uint32_t picked = StarfoxPS5_PickRefresh(refresh, count, PS_TARGET_REFRESH_MILLIHERTZ);
    SDL_free(refresh);
    VkDisplayModePropertiesKHR const *mode = &modes[picked];

    VkDisplaySurfaceCreateInfoKHR info;
    SDL_zero(info);
    info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    info.displayMode = mode->displayMode;
    info.planeIndex = 0;
    info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.globalAlpha = 1.0f;
    info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    info.imageExtent = mode->parameters.visibleRegion;
    SDL_free(modes);
    result = create_surface(instance, &info, allocator, surface);
    if (result != VK_SUCCESS) {
        return SDL_SetError("vkCreateDisplayPlaneSurfaceKHR: %s", SDL_Vulkan_GetResultString(result));
    }
    return true;
}

static void PS_DestroySurface(SDL_VideoDevice *device, VkInstance instance, VkSurfaceKHR surface,
                              const struct VkAllocationCallbacks *allocator)
{
    (void)device;
    PFN_vkDestroySurfaceKHR destroy =
        (PFN_vkDestroySurfaceKHR)vk_icdGetInstanceProcAddr(instance, "vkDestroySurfaceKHR");
    if (destroy) {
        destroy(instance, surface, allocator);
    }
}

static bool PS_GetPresentationSupport(SDL_VideoDevice *device, VkInstance instance,
                                      VkPhysicalDevice physical, Uint32 family)
{
    (void)device;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties query = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)
        vk_icdGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    if (!query) {
        return false;
    }
    uint32_t count = 0;
    query(physical, &count, NULL);
    if (family >= count) {
        return false;
    }
    VkQueueFamilyProperties *properties = SDL_calloc(count, sizeof(*properties));
    if (!properties) {
        return false;
    }
    query(physical, &count, properties);
    // VideoOut flips are submitted from the graphics queue.
    const bool supported = family < count && (properties[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
    SDL_free(properties);
    return supported;
}

static void PS_PumpEvents(SDL_VideoDevice *device)
{
    (void)device;
    if (SDL_WasInit(SDL_INIT_JOYSTICK)) {
        StarfoxPS5_PollGamepads();
    }
}

static void PS_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *PS_CreateDevice(void)
{
    SDL_VideoDevice *device = SDL_calloc(1, sizeof(*device));
    if (!device) {
        return NULL;
    }
    device->VideoInit = PS_VideoInit;
    device->VideoQuit = PS_VideoQuit;
    device->CreateSDLWindow = PS_CreateWindow;
    device->SetWindowSize = PS_SetWindowSize;
    device->PumpEvents = PS_PumpEvents;
    device->Vulkan_LoadLibrary = PS_LoadVulkan;
    device->Vulkan_UnloadLibrary = PS_UnloadVulkan;
    device->Vulkan_GetInstanceExtensions = PS_GetInstanceExtensions;
    device->Vulkan_CreateSurface = PS_CreateSurface;
    device->Vulkan_DestroySurface = PS_DestroySurface;
    device->Vulkan_GetPresentationSupport = PS_GetPresentationSupport;
    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY;
    device->free = PS_DeleteDevice;
    return device;
}

// Not "preferred": SDL skips preferred drivers when SDL_HINT_VIDEO_DRIVER names
// one, and the game names this driver.
VideoBootStrap STARFOXPS5_bootstrap = {
    "ps5", "PS5 VideoOut through native Vulkan", PS_CreateDevice, NULL, false
};
