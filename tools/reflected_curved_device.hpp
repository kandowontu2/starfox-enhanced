#pragma once
// Diagnostic-only device contract for the binary32 compensated optics. The
// strict SPIR-V requests SignedZeroInfNanPreserve 32; do not run it on an
// unqueried device or silently fall back to a relaxed shader.
#include <SDL3/SDL.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "../include/starfox/render/sdl_vulkan_bridge.h"
#include <iostream>
#include <stdexcept>
#include <string_view>

inline SDL_GPUDevice* create_reflected_curved_device(const char* backend,bool prefer_low_power=false) {
    const auto props=SDL_CreateProperties();if(!props)return nullptr;
    SDL_GPUVulkanOptions options{};options.vulkan_api_version=VK_API_VERSION_1_2;
    const bool ok=SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,backend)
        && SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true)
        && SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true)
        && SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true)
        && (!prefer_low_power || SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,true))
        && (std::string_view(backend)!="vulkan" || SDL_SetPointerProperty(props,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER,&options));
    auto* device=ok?SDL_CreateGPUDeviceWithProperties(props):nullptr;
    SDL_DestroyProperties(props);return device;
}
inline void require_reflected_curved_precision(SDL_GPUDevice* device,const char* backend) {
    if(std::string_view(backend)!="vulkan")return;
    const auto* bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(
        SDL_GetGPUDeviceProperties(device),STARFOX_SDL_VULKAN_BRIDGE,nullptr));
    if(!bridge || bridge->version!=2 || !bridge->get_instance_proc)
        throw std::runtime_error("Curved diagnostic requires the pinned Vulkan property-query bridge");
    const auto get=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceProperties2"));
    if(!get)throw std::runtime_error("Curved diagnostic requires Vulkan 1.2 property queries");
    VkPhysicalDeviceFloatControlsProperties controls{};controls.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
    VkPhysicalDeviceProperties2 properties{};properties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;properties.pNext=&controls;
    get(bridge->physical_device,&properties);
    if(properties.properties.apiVersion<VK_API_VERSION_1_2 || !controls.shaderSignedZeroInfNanPreserveFloat32)
        throw std::runtime_error("Curved diagnostic requires Vulkan 1.2 and SignedZeroInfNanPreserve for binary32; relaxed fallback is not accepted");
    std::cout<<"Curved Vulkan float contract: API >=1.2, SignedZeroInfNanPreserve32="
        <<controls.shaderSignedZeroInfNanPreserveFloat32<<std::endl;
}
