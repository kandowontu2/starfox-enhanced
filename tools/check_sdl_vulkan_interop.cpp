#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include "starfox/render/dxr_shadows.hpp"
#include <windows.h>
#include <vulkan/vulkan.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
#include <cstring>

static void require(bool value,const char* text) {if(!value) throw std::runtime_error(text);}
#include "check_sdl_vulkan_shadow.hpp"
int main() {
    SDL_GPUDevice* device{};
    SDL_PropertiesID props{};
    int result=0;
    try {
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        props=SDL_CreateProperties();require(props,SDL_GetError());
        VkPhysicalDeviceVulkan12Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        features.timelineSemaphore=VK_TRUE;
        const char* extensions[]={VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME};
        SDL_GPUVulkanOptions options{};
        options.vulkan_api_version=VK_API_VERSION_1_2;options.feature_list=&features;
        options.device_extension_count=2;options.device_extension_names=extensions;
        SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan");
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
        SDL_SetPointerProperty(props,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER,&options);
        for(unsigned iteration=0;iteration<3;++iteration) {
            device=SDL_CreateGPUDeviceWithProperties(props);require(device,SDL_GetError());
            auto* bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(
                SDL_GetGPUDeviceProperties(device),STARFOX_SDL_VULKAN_BRIDGE,nullptr));
            require(bridge && bridge->version==2 && bridge->device && bridge->physical_device
                && bridge->wait_timeline && bridge->copy_external,"Missing native Vulkan bridge");
            auto get=bridge->get_device_proc;
            require(get(bridge->device,"vkGetMemoryWin32HandlePropertiesKHR")
                && get(bridge->device,"vkImportSemaphoreWin32HandleKHR"),"Requested external extensions were not enabled");
            auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceProperties2"));
            require(properties,"Missing physical-device query");
            VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
            VkPhysicalDeviceProperties2 physical{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};physical.pNext=&id;
            properties(bridge->physical_device,&physical);require(id.deviceLUIDValid,"No adapter LUID for DXR matching");
            auto create=reinterpret_cast<PFN_vkCreateSemaphore>(get(bridge->device,"vkCreateSemaphore"));
            auto destroy=reinterpret_cast<PFN_vkDestroySemaphore>(get(bridge->device,"vkDestroySemaphore"));
            auto counter=reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(get(bridge->device,"vkGetSemaphoreCounterValue"));
            require(create && destroy && counter,"Missing timeline semaphore functions");
            VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
            type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;type.initialValue=17;
            VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};info.pNext=&type;
            VkSemaphore semaphore{};require(create(bridge->device,&info,nullptr,&semaphore)==VK_SUCCESS,"Timeline semaphore creation failed");
            uint64_t value{};const auto queried=counter(bridge->device,semaphore,&value);
            require(!bridge->wait_timeline(device,VK_NULL_HANDLE,17),"Null semaphore was accepted");
            require(bridge->wait_timeline(device,semaphore,17),SDL_GetError());
            require(SDL_WaitForGPUIdle(device),SDL_GetError());
            destroy(bridge->device,semaphore,nullptr);
            require(queried==VK_SUCCESS && value==17,"Timeline feature was not enabled");
            check_shadow_copy(device,*bridge,id.deviceLUID);
            std::cout<<"SDL Vulkan external-memory/semaphore extensions and timeline enabled: "<<physical.properties.deviceName<<'\n';
            SDL_DestroyGPUDevice(device);device=nullptr;
        }
        // Unsupported options must reject without silently weakening requirements.
        const char* invalid="VK_STARFOX_nonexistent_extension";
        options.device_extension_count=1;options.device_extension_names=&invalid;
        device=SDL_CreateGPUDeviceWithProperties(props);
        require(!device,"Unsupported extension request was accepted");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';result=1;}
    if(device) SDL_DestroyGPUDevice(device);
    if(props) SDL_DestroyProperties(props);
    SDL_Quit();return result;
}
