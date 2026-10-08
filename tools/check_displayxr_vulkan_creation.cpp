// Mock OpenXR negotiation delegates to the REAL Vulkan driver. SDL performs
// both its normal probe and real device creation; no panel/runtime is claimed.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "starfox/render/displayxr_vulkan_binding.hpp"
#include "starfox/render/sdl_vulkan_bridge.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
unsigned assertions{};
void require(bool value,const char* message) {++assertions;if(!value) throw std::runtime_error(message);}
const auto xr_instance=reinterpret_cast<XrInstance>(std::uintptr_t(0x200));
constexpr XrSystemId system=77;
enum class Fault {none,instance_xr,instance_vk,instance_null,physical_xr,physical_null,physical_foreign,
    device_xr,device_vk,device_null,instance_allocated_xr,device_allocated_xr,instance_failed_garbage,device_failed_garbage};
struct Mock {
    XrVersion minimum{XR_MAKE_VERSION(1,1,0)},maximum{XR_MAKE_VERSION(1,3,0)};
    std::string_view missing,null_proc;bool requirements_fail{},integrated{},custom{};
    Fault fault{};unsigned requirements{},instances{},physical{},devices{},custom_seen{};
    VkPhysicalDevice selected{};VkPhysicalDeviceType selected_type{};
    PFN_vkGetInstanceProcAddr last_get{};VkInstance last_instance{};
} mock;
XrResult XRAPI_CALL requirements(XrInstance xr,XrSystemId id,XrGraphicsRequirementsVulkan2KHR* out) {
    ++mock.requirements;
    require(xr==xr_instance && id==system && out && out->type==XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR && !out->next,"Bad requirements request");
    out->minApiVersionSupported=mock.minimum;out->maxApiVersionSupported=mock.maximum;
    return mock.requirements_fail?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
XrResult XRAPI_CALL instance_create(XrInstance xr,const XrVulkanInstanceCreateInfoKHR* info,VkInstance* out,VkResult* result) {
    ++mock.instances;*out=VK_NULL_HANDLE;*result=VK_ERROR_INITIALIZATION_FAILED;
    require(xr==xr_instance && mock.requirements==1 && info && info->type==XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR
        && !info->next && !info->createFlags && info->systemId==system && info->pfnGetInstanceProcAddr
        && info->vulkanCreateInfo && !info->vulkanAllocator,"Bad XR Vulkan instance handshake");
    require(info->vulkanCreateInfo->enabledExtensionCount && info->vulkanCreateInfo->ppEnabledExtensionNames,"SDL instance extensions lost");
    if(mock.fault==Fault::instance_xr) return XR_ERROR_RUNTIME_FAILURE;
    if(mock.fault==Fault::instance_vk) {*result=VK_ERROR_OUT_OF_HOST_MEMORY;return XR_SUCCESS;}
    if(mock.fault==Fault::instance_failed_garbage) {*out=reinterpret_cast<VkInstance>(std::uintptr_t(0xdead));return XR_SUCCESS;}
    if(mock.fault==Fault::instance_null) {*result=VK_SUCCESS;return XR_SUCCESS;}
    const auto create=reinterpret_cast<PFN_vkCreateInstance>(info->pfnGetInstanceProcAddr(nullptr,"vkCreateInstance"));
    require(create,"Real Vulkan instance entry unavailable");
    *result=create(info->vulkanCreateInfo,info->vulkanAllocator,out);
    mock.last_get=info->pfnGetInstanceProcAddr;mock.last_instance=*out;
    if(mock.fault==Fault::instance_allocated_xr) return XR_ERROR_RUNTIME_FAILURE;
    return XR_SUCCESS;
}
XrResult XRAPI_CALL physical_get(XrInstance xr,const XrVulkanGraphicsDeviceGetInfoKHR* info,VkPhysicalDevice* out) {
    ++mock.physical;*out=VK_NULL_HANDLE;
    require(xr==xr_instance && info && info->type==XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR && !info->next
        && info->systemId==system && info->vulkanInstance==mock.last_instance,"Bad XR physical-device request");
    if(mock.fault==Fault::physical_xr) return XR_ERROR_RUNTIME_FAILURE;
    if(mock.fault==Fault::physical_null) return XR_SUCCESS;
    if(mock.fault==Fault::physical_foreign) {*out=reinterpret_cast<VkPhysicalDevice>(std::uintptr_t(0xdead));return XR_SUCCESS;}
    const auto enumerate=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(mock.last_get(info->vulkanInstance,"vkEnumeratePhysicalDevices"));
    const auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(mock.last_get(info->vulkanInstance,"vkGetPhysicalDeviceProperties"));
    require(enumerate && properties,"Real Vulkan adapter entries unavailable");
    std::uint32_t count{};require(enumerate(info->vulkanInstance,&count,nullptr)==VK_SUCCESS && count && count<1024,"No real Vulkan GPU");
    std::vector<VkPhysicalDevice> devices(count);require(enumerate(info->vulkanInstance,&count,devices.data())==VK_SUCCESS,"Real adapter enumeration failed");
    *out=devices[0];
    for(const auto device:devices) {
        VkPhysicalDeviceProperties prop{};properties(device,&prop);
        const auto wanted=mock.integrated?VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        if(prop.deviceType==wanted) {*out=device;break;}
    }
    VkPhysicalDeviceProperties prop{};properties(*out,&prop);
    mock.selected=*out;mock.selected_type=prop.deviceType;return XR_SUCCESS;
}
XrResult XRAPI_CALL device_create(XrInstance xr,const XrVulkanDeviceCreateInfoKHR* info,VkDevice* out,VkResult* result) {
    ++mock.devices;*out=VK_NULL_HANDLE;*result=VK_ERROR_INITIALIZATION_FAILED;
    require(xr==xr_instance && info && info->type==XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR && !info->next && !info->createFlags
        && info->systemId==system && info->pfnGetInstanceProcAddr && info->vulkanPhysicalDevice==mock.selected
        && info->vulkanCreateInfo && !info->vulkanAllocator,"Bad XR Vulkan device handshake");
    const auto& vk=*info->vulkanCreateInfo;
    require(vk.queueCreateInfoCount==1 && vk.pQueueCreateInfos && vk.pQueueCreateInfos[0].queueCount==1
        && vk.enabledExtensionCount && vk.ppEnabledExtensionNames,"SDL queue/extensions lost");
    if(mock.custom) {
        require(std::any_of(vk.ppEnabledExtensionNames,vk.ppEnabledExtensionNames+vk.enabledExtensionCount,
            [](auto name) {return std::string_view(name)=="VK_KHR_timeline_semaphore";}),"Custom device extension lost");
        bool timeline{};
        for(auto* next=static_cast<const VkBaseInStructure*>(vk.pNext);next;next=next->pNext)
            if(next->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)
                timeline=reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(next)->timelineSemaphore==VK_TRUE;
        require(timeline,"Custom SDL device feature lost");++mock.custom_seen;
    }
    if(mock.fault==Fault::device_xr) return XR_ERROR_RUNTIME_FAILURE;
    if(mock.fault==Fault::device_vk) {*result=VK_ERROR_OUT_OF_DEVICE_MEMORY;return XR_SUCCESS;}
    if(mock.fault==Fault::device_failed_garbage) {*out=reinterpret_cast<VkDevice>(std::uintptr_t(0xdead));return XR_SUCCESS;}
    if(mock.fault==Fault::device_null) {*result=VK_SUCCESS;return XR_SUCCESS;}
    const auto create=reinterpret_cast<PFN_vkCreateDevice>(info->pfnGetInstanceProcAddr(mock.last_instance,"vkCreateDevice"));
    require(create,"Real Vulkan device entry unavailable");
    *result=create(info->vulkanPhysicalDevice,&vk,info->vulkanAllocator,out);
    if(mock.fault==Fault::device_allocated_xr) return XR_ERROR_RUNTIME_FAILURE;
    return XR_SUCCESS;
}
XrResult XRAPI_CALL get(XrInstance xr,const char* name,PFN_xrVoidFunction* out) {
    *out=nullptr;if(xr!=xr_instance || name==mock.missing) return XR_ERROR_FUNCTION_UNSUPPORTED;
    if(name==mock.null_proc) return XR_SUCCESS;
#define PROC(n,f) if(std::string_view(name)==n) {*out=reinterpret_cast<PFN_xrVoidFunction>(f);return XR_SUCCESS;}
    PROC("xrGetVulkanGraphicsRequirements2KHR",requirements)
    PROC("xrCreateVulkanInstanceKHR",instance_create)
    PROC("xrGetVulkanGraphicsDevice2KHR",physical_get)
    PROC("xrCreateVulkanDeviceKHR",device_create)
#undef PROC
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
struct Properties {
    SDL_PropertiesID id{SDL_CreateProperties()};
    ~Properties() {if(id) SDL_DestroyProperties(id);}
};
struct Device {
    SDL_GPUDevice* gpu{};
    ~Device() {if(gpu) SDL_DestroyGPUDevice(gpu);}
};
struct Observer {
    StarfoxSdlVulkanXrCreateV1 original,api;
    unsigned retains{},releases{},instances{},instance_destroys{},devices{},device_destroys{};
} observer;
bool retain(void* user) {if(!observer.original.retain(user)) return false;++observer.retains;return true;}
void release(void* user) {++observer.releases;observer.original.release(user);}
VkResult create_instance(void* user,PFN_vkGetInstanceProcAddr get,const VkInstanceCreateInfo* info,const VkAllocationCallbacks* allocator,VkInstance* out) {
    const auto result=observer.original.create_instance(user,get,info,allocator,out);
    if(result==VK_SUCCESS && *out) ++observer.instances;return result;
}
VkResult create_device(void* user,PFN_vkGetInstanceProcAddr get,VkInstance instance,VkPhysicalDevice physical,
    const VkDeviceCreateInfo* info,const VkAllocationCallbacks* allocator,VkDevice* out) {
    const auto result=observer.original.create_device(user,get,instance,physical,info,allocator,out);
    if(result==VK_SUCCESS && *out) ++observer.devices;return result;
}
void instance_destroyed(void* user,VkInstance instance) {++observer.instance_destroys;observer.original.instance_destroyed(user,instance);}
void device_destroyed(void* user,VkDevice device) {++observer.device_destroys;observer.original.device_destroyed(user,device);}
void SDLCALL cleanup(void* user,void*) {release(user);}
void instrument(Properties& props) {
    auto* api=static_cast<const StarfoxSdlVulkanXrCreateV1*>(SDL_GetPointerProperty(props.id,STARFOX_SDL_VULKAN_XR_CREATE,nullptr));
    require(api,"Missing creation table");observer={};observer.original=*api;observer.api=*api;
    observer.api.retain=retain;observer.api.release=release;observer.api.create_instance=create_instance;
    observer.api.create_device=create_device;observer.api.instance_destroyed=instance_destroyed;observer.api.device_destroyed=device_destroyed;
    require(retain(api->user),"Property lease failed");
    require(SDL_SetPointerPropertyWithCleanup(props.id,STARFOX_SDL_VULKAN_XR_CREATE,&observer.api,cleanup,api->user),"Instrumentation property failed");
}
void base(Properties& props) {
    require(props.id && SDL_SetStringProperty(props.id,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan")
        && SDL_SetBooleanProperty(props.id,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true)
        && SDL_SetBooleanProperty(props.id,SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN,false),"SDL properties failed");
}
void exact_upload(SDL_GPUDevice* device) {
    const unsigned bytes=37*23*4;
    SDL_GPUTransferBufferCreateInfo info{};info.size=bytes;info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    auto* upload=SDL_CreateGPUTransferBuffer(device,&info);info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    auto* download=SDL_CreateGPUTransferBuffer(device,&info);
    SDL_GPUTextureCreateInfo texture{};texture.type=SDL_GPU_TEXTURETYPE_2D;texture.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;texture.width=37;texture.height=23;texture.layer_count_or_depth=texture.num_levels=1;
    auto* image=SDL_CreateGPUTexture(device,&texture);SDL_GPUCommandBuffer* command{};SDL_GPUFence* fence{};
    struct Cleanup {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer *upload,*download;SDL_GPUTexture* image;
        SDL_GPUCommandBuffer*& command;SDL_GPUFence*& fence;
        ~Cleanup() {
            if(command) SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);
            if(fence) SDL_ReleaseGPUFence(device,fence);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);if(download) SDL_ReleaseGPUTransferBuffer(device,download);
            if(image) SDL_ReleaseGPUTexture(device,image);
        }
    } cleanup{device,upload,download,image,command,fence};
    require(upload && download && image,"GPU allocation failed");
    std::vector<unsigned char> expected(bytes);for(unsigned i=0;i<bytes;++i) expected[i]=static_cast<unsigned char>((i*71+i/37)%251);
    auto* data=SDL_MapGPUTransferBuffer(device,upload,false);require(data,"GPU upload map failed");
    std::memcpy(data,expected.data(),bytes);SDL_UnmapGPUTransferBuffer(device,upload);
    command=SDL_AcquireGPUCommandBuffer(device);require(command,"GPU command failed");
    auto* pass=SDL_BeginGPUCopyPass(command);require(pass,"GPU copy pass failed");
    SDL_GPUTextureTransferInfo transfer{};transfer.transfer_buffer=upload;transfer.pixels_per_row=37;transfer.rows_per_layer=23;
    SDL_GPUTextureRegion region{};region.texture=image;region.w=37;region.h=23;region.d=1;
    SDL_UploadToGPUTexture(pass,&transfer,&region,false);transfer.transfer_buffer=download;
    SDL_DownloadFromGPUTexture(pass,&region,&transfer);SDL_EndGPUCopyPass(pass);
    fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence && SDL_WaitForGPUFences(device,true,&fence,1),"GPU submission failed");
    data=SDL_MapGPUTransferBuffer(device,download,false);require(data,"GPU download map failed");
    const bool same=std::memcmp(data,expected.data(),bytes)==0;SDL_UnmapGPUTransferBuffer(device,download);
    require(same,"XR-created SDL device changed the pixels");
}
void initialization_tests() {
    using starfox::render::DisplayXrVulkanBinding;
    DisplayXrVulkanBinding binding;
    require(!binding.initialize_with_api(nullptr,system,get) && !binding.binding(),"Missing XR instance accepted");
    require(!binding.initialize_with_api(xr_instance,0,get) && !binding.initialize_with_api(xr_instance,system,nullptr),"Missing XR system/dispatch accepted");
    const std::array names{"xrGetVulkanGraphicsRequirements2KHR","xrCreateVulkanInstanceKHR","xrGetVulkanGraphicsDevice2KHR","xrCreateVulkanDeviceKHR"};
    for(const auto name:names) {
        mock={};mock.missing=name;require(!binding.initialize_with_api(xr_instance,system,get),"Missing XR procedure accepted");
        mock={};mock.null_proc=name;require(!binding.initialize_with_api(xr_instance,system,get),"Null XR procedure accepted");
    }
    mock={};mock.requirements_fail=true;require(!binding.initialize_with_api(xr_instance,system,get),"Failed requirements accepted");
    for(unsigned i=0;i<4;++i) {
        mock={};if(i==0) mock.minimum=0;if(i==1) mock.minimum=XR_MAKE_VERSION(2,0,0);
        if(i==2) mock.maximum=XR_MAKE_VERSION(1,0,0);if(i==3) mock.maximum=XR_MAKE_VERSION(1,1024,0);
        require(!binding.initialize_with_api(xr_instance,system,get),"Malformed API range accepted");
    }
    mock={};require(binding.initialize_with_api(xr_instance,system,get),"Valid XR requirements rejected");
    require(!binding.configure_gpu_properties(0) && !binding.attach(nullptr) && !binding.binding(),"Invalid SDL input accepted");
    binding.close();
}
VkQueue wrong_queue(void*) {return reinterpret_cast<VkQueue>(std::uintptr_t(0xdead));}
void reject_tampered_binding(starfox::render::DisplayXrVulkanBinding& binding,SDL_GPUDevice* device) {
    const auto props=SDL_GetGPUDeviceProperties(device);
    auto* original=static_cast<StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
    const auto* queue=static_cast<const StarfoxSdlVulkanXrBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_XR_BRIDGE,nullptr));
    const auto original_value=*original;const auto queue_value=*queue;
    const auto context=SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_XR_CREATION_CONTEXT,nullptr);
    for(unsigned failure=0;failure<10;++failure) {
        auto changed=original_value;auto changed_queue=queue_value;
        if(failure==0) changed.version=1;
        if(failure==1) changed.instance=reinterpret_cast<VkInstance>(std::uintptr_t(0xdead));
        if(failure==2) changed.physical_device=reinterpret_cast<VkPhysicalDevice>(std::uintptr_t(0xdead));
        if(failure==3) changed.device=reinterpret_cast<VkDevice>(std::uintptr_t(0xdead));
        if(failure==4) ++changed.queue_family;
        if(failure==5) changed.get_device_proc=nullptr;
        if(failure==6) changed_queue.version=2;
        if(failure==7) changed_queue.queue=nullptr;
        if(failure==8) changed_queue.queue=wrong_queue;
        if(failure==9) SDL_SetPointerProperty(props,STARFOX_SDL_VULKAN_XR_CREATION_CONTEXT,nullptr);
        // The device table is property-owned heap storage, whereas the queue
        // table is static const storage without a property cleanup callback.
        *original=changed;SDL_SetPointerProperty(props,STARFOX_SDL_VULKAN_XR_BRIDGE,&changed_queue);
        const bool rejected=!binding.attach(device) && !binding.binding();
        // Restore every borrowed property before asserting/returning.
        *original=original_value;SDL_SetPointerProperty(props,STARFOX_SDL_VULKAN_XR_BRIDGE,const_cast<StarfoxSdlVulkanXrBridgeV1*>(queue));
        SDL_SetPointerProperty(props,STARFOX_SDL_VULKAN_XR_CREATION_CONTEXT,context);
        require(rejected,"Tampered native binding was accepted");
        require(binding.attach(device),"Original binding failed after invalid input");
    }
}
void real_creation(bool integrated,bool custom,bool revoke) {
    using starfox::render::DisplayXrVulkanBinding;
    mock={};mock.integrated=integrated;mock.custom=custom;
    DisplayXrVulkanBinding binding;require(binding.initialize_with_api(xr_instance,system,get),binding.status().c_str());
    VkPhysicalDeviceVulkan12Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};features.timelineSemaphore=VK_TRUE;
    const char* extensions[]{"VK_KHR_timeline_semaphore"};SDL_GPUVulkanOptions options{};
    options.vulkan_api_version=VK_API_VERSION_1_2;options.feature_list=&features;options.device_extension_count=1;options.device_extension_names=extensions;
    Properties props;base(props);require(binding.configure_gpu_properties(props.id),"Configure failed");instrument(props);
    require(SDL_SetBooleanProperty(props.id,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,!integrated),"Power preference failed");
    if(custom) require(SDL_SetPointerProperty(props.id,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER,&options),"Custom options failed");
    Device device;device.gpu=SDL_CreateGPUDeviceWithProperties(props.id);require(device.gpu,SDL_GetError());
    require(mock.instances==2 && mock.physical==2 && mock.devices==1 && observer.instances==2 && observer.instance_destroys==1,
        "SDL probe/actual creation sequence incorrect");
    require(!custom || mock.custom_seen==1,"Custom options not forwarded");
    require(binding.attach(device.gpu) && binding.binding(),binding.status().c_str());
    const auto* xr=static_cast<const XrGraphicsBindingVulkan2KHR*>(binding.binding());
    const auto* api=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device.gpu),STARFOX_SDL_VULKAN_BRIDGE,nullptr));
    require(xr->type==XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR && !xr->next && xr->instance==api->instance
        && xr->physicalDevice==api->physical_device && xr->device==api->device && xr->queueFamilyIndex==api->queue_family && !xr->queueIndex,
        "Binding is not the exact SDL device/queue");
    require(!binding.attach(nullptr) && !binding.binding(),"Rejected attachment retained stale binding");
    require(binding.attach(device.gpu),"Binding could not recover");exact_upload(device.gpu);
    reject_tampered_binding(binding,device.gpu);
    if(revoke) {
        const auto calls=mock.instances;binding.close();require(!binding.binding(),"Revocation retained binding");
        Device rejected;rejected.gpu=SDL_CreateGPUDeviceWithProperties(props.id);
        require(!rejected.gpu && mock.instances==calls,"Revoked properties invoked unloaded XR dispatch");
        // Existing SDL graphics remain valid even after the XR owner is gone.
        exact_upload(device.gpu);
    }
    SDL_DestroyGPUDevice(device.gpu);device.gpu=nullptr;
    require(!binding.binding() && observer.instances==observer.instance_destroys && observer.devices==observer.device_destroys,
        "SDL teardown retained stale binding/native handles");
    SDL_DestroyProperties(props.id);props.id=0;binding.close();
    require(observer.retains==observer.releases,"Creation lease leaked/double-released");
    std::cout<<"Real SDL Vulkan2 creation: "<<(integrated?"integrated":"discrete")<<" selection (actual GPU type="<<mock.selected_type<<"), "<<(custom?"custom":"default")
        <<" options, "<<(revoke?"revoked-live-device":"device teardown")<<"; exact pixels and balanced leases passed\n";
}
void invalid_abi_tests() {
    using starfox::render::DisplayXrVulkanBinding;
    for(unsigned failure=0;failure<10;++failure) {
        mock={};DisplayXrVulkanBinding binding;require(binding.initialize_with_api(xr_instance,system,get),"ABI setup rejected");
        Properties props;base(props);require(binding.configure_gpu_properties(props.id),"ABI configure rejected");instrument(props);
        if(failure==0) observer.api.version=2;if(failure==1) observer.api.default_api_version=0;
        if(failure==2) observer.api.user=nullptr;if(failure==3) observer.api.retain=nullptr;
        if(failure==4) observer.api.release=nullptr;if(failure==5) observer.api.create_instance=nullptr;
        if(failure==6) observer.api.physical_device=nullptr;if(failure==7) observer.api.create_device=nullptr;
        if(failure==8) observer.api.instance_destroyed=nullptr;if(failure==9) observer.api.device_destroyed=nullptr;
        Device device;device.gpu=SDL_CreateGPUDeviceWithProperties(props.id);
        require(!device.gpu && !mock.instances && observer.retains==1,"Malformed SDL creation ABI reached Vulkan");
        SDL_DestroyProperties(props.id);props.id=0;binding.close();require(observer.retains==observer.releases,"Malformed ABI leaked property lease");
    }
}
void renderer_creation_test() {
    mock={};starfox::render::DisplayXrVulkanBinding binding;
    require(binding.initialize_with_api(xr_instance,system,get),"Renderer initialization failed");
    Properties props;base(props);require(binding.configure_gpu_properties(props.id),"Renderer configure failed");instrument(props);
    auto* window=SDL_CreateWindow("Vulkan2 creation fixture",320,240,SDL_WINDOW_HIDDEN);
    require(window,"Fixture window failed");
    struct Owner {SDL_Window* window;SDL_Renderer* renderer{};~Owner() {if(renderer) SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);}} owner{window};
    require(SDL_SetStringProperty(props.id,SDL_PROP_RENDERER_CREATE_NAME_STRING,"gpu")
        && SDL_SetPointerProperty(props.id,SDL_PROP_RENDERER_CREATE_WINDOW_POINTER,window),"Renderer properties failed");
    owner.renderer=SDL_CreateRendererWithProperties(props.id);require(owner.renderer,SDL_GetError());
    auto* device=static_cast<SDL_GPUDevice*>(SDL_GetPointerProperty(SDL_GetRendererProperties(owner.renderer),SDL_PROP_RENDERER_GPU_DEVICE_POINTER,nullptr));
    require(device && binding.attach(device) && binding.binding(),"SDL renderer dropped the Vulkan2 creation property");
    require(SDL_SetRenderDrawColor(owner.renderer,19,37,73,255) && SDL_RenderClear(owner.renderer) && SDL_RenderPresent(owner.renderer),"XR-created ordinary SDL renderer failed");
    binding.close();
    require(SDL_SetRenderDrawColor(owner.renderer,73,37,19,255) && SDL_RenderClear(owner.renderer) && SDL_RenderPresent(owner.renderer),"Renderer failed after creation owner was revoked");
    SDL_DestroyRenderer(owner.renderer);owner.renderer=nullptr;
    require(observer.instances==observer.instance_destroys && observer.devices==observer.device_destroys,"Renderer teardown leaked native handles");
    SDL_DestroyProperties(props.id);props.id=0;require(observer.retains==observer.releases,"Renderer teardown leaked lease");
}
void multiple_devices_test() {
    mock={};starfox::render::DisplayXrVulkanBinding binding;
    require(binding.initialize_with_api(xr_instance,system,get),"Multiple device setup failed");
    Properties props;base(props);require(binding.configure_gpu_properties(props.id),"Multiple device configure failed");
    Device first,second;first.gpu=SDL_CreateGPUDeviceWithProperties(props.id);require(first.gpu,SDL_GetError());
    second.gpu=SDL_CreateGPUDeviceWithProperties(props.id);require(second.gpu,SDL_GetError());
    require(binding.attach(first.gpu) && binding.attach(second.gpu),"Concurrent SDL devices lost creation records");
    const auto second_handle=static_cast<const XrGraphicsBindingVulkan2KHR*>(binding.binding())->device;
    SDL_DestroyGPUDevice(first.gpu);first.gpu=nullptr;
    require(binding.binding() && static_cast<const XrGraphicsBindingVulkan2KHR*>(binding.binding())->device==second_handle,
        "Unrelated device teardown revoked the current binding");
    exact_upload(second.gpu);
    // Reinitialization revokes old properties/devices but does not destroy or
    // relabel them as if they belonged to the new XR instance generation.
    require(binding.initialize_with_api(xr_instance,system,get) && !binding.attach(second.gpu) && !binding.binding(),
        "New XR owner accepted a previous generation's device");
    Device rejected;rejected.gpu=SDL_CreateGPUDeviceWithProperties(props.id);require(!rejected.gpu,"Old properties remained live after reinitialization");
    exact_upload(second.gpu);binding.close();
}
void failure_tests() {
    using starfox::render::DisplayXrVulkanBinding;
    for(const auto fault:{Fault::instance_xr,Fault::instance_vk,Fault::instance_null,Fault::physical_xr,Fault::physical_null,
        Fault::physical_foreign,Fault::device_xr,Fault::device_vk,Fault::device_null,Fault::instance_allocated_xr,Fault::device_allocated_xr,
        Fault::instance_failed_garbage,Fault::device_failed_garbage}) {
        mock={};mock.fault=fault;
        DisplayXrVulkanBinding binding;require(binding.initialize_with_api(xr_instance,system,get),"Failure setup rejected");
        Properties props;base(props);require(binding.configure_gpu_properties(props.id),"Failure configure rejected");instrument(props);
        Device device;device.gpu=SDL_CreateGPUDeviceWithProperties(props.id);
        require(!device.gpu && !binding.binding(),"Failed XR creation fell back to foreign GPU");
        require(observer.instances==observer.instance_destroys && observer.devices==observer.device_destroys,"Failed SDL setup leaked its successful native handles");
        SDL_DestroyProperties(props.id);props.id=0;binding.close();require(observer.retains==observer.releases,"Failed setup leaked a creation lease");
    }
    // Caller-supplied version remains authoritative, but must match XR.
    mock={};mock.maximum=XR_MAKE_VERSION(1,1,0);
    DisplayXrVulkanBinding binding;require(binding.initialize_with_api(xr_instance,system,get),"API mismatch setup failed");
    Properties props;base(props);require(binding.configure_gpu_properties(props.id),"API mismatch configure failed");instrument(props);
    SDL_GPUVulkanOptions options{};options.vulkan_api_version=VK_API_VERSION_1_2;
    require(SDL_SetPointerProperty(props.id,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER,&options),"API property failed");
    Device rejected;rejected.gpu=SDL_CreateGPUDeviceWithProperties(props.id);
    require(!rejected.gpu && !mock.instances,"Out-of-range SDL API reached XR creation");
    SDL_DestroyProperties(props.id);props.id=0;binding.close();require(observer.retains==observer.releases,"API rejection leaked lease");
}
void foreign_device_test() {
    mock={};starfox::render::DisplayXrVulkanBinding binding;
    require(binding.initialize_with_api(xr_instance,system,get),"Foreign-device test initialization failed");
    Properties props;base(props);Device device;device.gpu=SDL_CreateGPUDeviceWithProperties(props.id);require(device.gpu,SDL_GetError());
    require(!binding.attach(device.gpu) && !binding.binding() && !mock.instances && !mock.devices,"Ordinary borrowed Vulkan device accepted as Vulkan2");
    exact_upload(device.gpu);binding.close();
}
}
int main() {
    try {
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());struct Quit {~Quit() {SDL_Quit();}} quit;
        initialization_tests();foreign_device_test();failure_tests();invalid_abi_tests();renderer_creation_test();multiple_devices_test();
        for(bool integrated:{false,true}) for(bool custom:{false,true}) for(bool revoke:{false,true}) real_creation(integrated,custom,revoke);
        std::cout<<assertions<<" Vulkan2 creation/binding assertions passed on real SDL devices; OpenXR dispatch mocked, no physical panel validation\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"Vulkan2 creation check: "<<error.what()<<'\n';return 1;}
}
