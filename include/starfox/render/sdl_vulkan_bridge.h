#pragma once
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
/* Private ABI for our pinned SDL build. Include Vulkan declarations first.
 * All handles are borrowed and expire with the SDL GPU device. This exposes
 * no queue: external submissions must use a separately synchronized bridge. */
#define STARFOX_SDL_VULKAN_BRIDGE "starfox.gpu.vulkan.bridge.v2"
typedef struct StarfoxSdlVulkanBridgeV2 {
    uint32_t version;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    PFN_vkGetInstanceProcAddr get_instance_proc;
    PFN_vkGetDeviceProcAddr get_device_proc;
    uint32_t queue_family;
    /* Semaphore/source belong to this device and must outlive the submitted
     * copy. Source is externally owned; copy acquires and releases ownership.
     * Call outside active SDL passes, with a transfer-source-capable buffer. */
    bool (*wait_timeline)(void *device, VkSemaphore semaphore, uint64_t value);
    bool (*copy_external)(void *command, VkBuffer source, uint64_t source_bytes,
                          void *destination, uint32_t bytes);
} StarfoxSdlVulkanBridgeV2;

#define STARFOX_SDL_VULKAN_GEOMETRY_BRIDGE "starfox.gpu.vulkan.geometry.v1"
typedef struct StarfoxSdlVulkanGeometryBridgeV1 {
    uint32_t version;
    /* Destination is externally owned, transfer-destination-capable. Acquire
     * and release ownership around the copy; caller waits before tracing. */
    bool (*copy_to_external)(void *command, void *source, VkBuffer destination,
                             uint64_t destination_bytes, uint32_t bytes);
    bool (*signal_timeline)(void *device, VkSemaphore semaphore, uint64_t value);
} StarfoxSdlVulkanGeometryBridgeV1;

#define STARFOX_SDL_VULKAN_RAY_BRIDGE "starfox.gpu.vulkan.ray.bridge.v3"
typedef struct StarfoxSdlVulkanRayBridgeV3 {
    uint32_t version;
    /* Record native Vulkan work inside one SDL command buffer. The caller
     * must be outside SDL passes and finish every prepared output. */
    VkCommandBuffer (*command)(void *sdl_command);
    VkBuffer (*prepare_write)(void *sdl_command, void *sdl_buffer);
    bool (*finish_write)(void *sdl_command, void *sdl_buffer);
    /* Copy submitted SDL geometry into a native AS input in the same command
     * buffer, with transfer-to-AS visibility and no queue ownership transfer. */
    bool (*copy_ray_range)(void *sdl_command, void *sdl_source,
                           uint64_t source_offset, VkBuffer destination,
                           uint64_t capacity, uint32_t bytes);
} StarfoxSdlVulkanRayBridgeV3;

/* Separate optional OpenXR color-image ABI. Runtime images belong to this
 * device and queue family; enter/leave COLOR_ATTACHMENT_OPTIMAL, with one
 * mip/layer/sample and TRANSFER_SRC/DST usage. Exact format/extent, no blit,
 * CPU readback, ownership transfer, or VK_QUEUE_FAMILY_EXTERNAL. The supplied
 * metadata must come from the validated XR swapchain, not guessed values.
 * All native queue access must use with_queue. Its non-throwing callback may
 * not call SDL submission/wait/interop functions or re-enter with_queue. */
#define STARFOX_SDL_VULKAN_XR_BRIDGE "starfox.gpu.vulkan.xr.v1"
typedef bool (*StarfoxVulkanQueueCallback)(void *user, VkQueue queue);
typedef struct StarfoxSdlVulkanXrBridgeV1 {
    uint32_t version;
    VkQueue (*queue)(void *device);
    bool (*with_queue)(void *device, StarfoxVulkanQueueCallback callback, void *user);
    bool (*copy_to_color)(void *command, void *sdl_texture, VkImage image,
                          VkFormat format, uint32_t width, uint32_t height);
    bool (*copy_from_color)(void *command, VkImage image, VkFormat format,
                            uint32_t width, uint32_t height, void *sdl_texture);
} StarfoxSdlVulkanXrBridgeV1;

/* Optional creation handshake, supplied BEFORE SDL_CreateGPUDevice. Vulkan2
 * OpenXR bindings require xrCreateVulkanInstanceKHR/xrCreateVulkanDeviceKHR,
 * not a borrowed device created directly by SDL. SDL copies this table and
 * retains user for EACH probe/device until native handles are destroyed.
 * Properties need their own lease. All callbacks must be non-throwing; close
 * may revoke new creation, but destruction notifications must remain safe.
 * SDL still owns/destroys the returned handles through Vulkan, not OpenXR.
 * The exact SDL extensions/features/allocators are passed through unchanged.
 */
#define STARFOX_SDL_VULKAN_XR_CREATE "starfox.gpu.vulkan.xr.create.v1"
#define STARFOX_SDL_VULKAN_XR_CREATION_CONTEXT "starfox.gpu.vulkan.xr.creation.context"
typedef struct StarfoxSdlVulkanXrCreateV1 {
    uint32_t version;
    uint32_t default_api_version; /* Used only without custom SDL Vulkan options. */
    void *user;
    bool (*retain)(void *user);
    void (*release)(void *user);
    VkResult (*create_instance)(void *user, PFN_vkGetInstanceProcAddr get,
        const VkInstanceCreateInfo *info, const VkAllocationCallbacks *allocator, VkInstance *instance);
    VkResult (*physical_device)(void *user, VkInstance instance, VkPhysicalDevice *physical);
    VkResult (*create_device)(void *user, PFN_vkGetInstanceProcAddr get, VkInstance instance, VkPhysicalDevice physical,
        const VkDeviceCreateInfo *info, const VkAllocationCallbacks *allocator, VkDevice *device);
    void (*instance_destroyed)(void *user, VkInstance instance);
    void (*device_destroyed)(void *user, VkDevice device);
} StarfoxSdlVulkanXrCreateV1;
