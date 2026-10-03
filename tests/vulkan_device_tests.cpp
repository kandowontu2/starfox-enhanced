#include "starfox/vr/vulkan_device.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class T> T handle(uintptr_t n) {return reinterpret_cast<T>(n);}
struct Fake {
    bool incompatible{},no_queue{},fail_device{},missing_dispatch{},missing_queue{};
    bool sharing{},no_timeline{},missing_extension{},requested_sharing{};
    unsigned created{};
    std::vector<int> cleanup;
} fake;
VkResult VKAPI_PTR extensions(VkPhysicalDevice,const char*,uint32_t* count,VkExtensionProperties* out) {
    *count=fake.missing_extension?2:3;
    if(out) {
        const char* names[]{"VK_KHR_external_memory_win32","VK_KHR_external_semaphore_win32","VK_KHR_timeline_semaphore"};
        for(unsigned i=0;i<*count;++i) std::strcpy(out[i].extensionName,names[i]);
    }
    return VK_SUCCESS;
}
void VKAPI_PTR features2(VkPhysicalDevice,VkPhysicalDeviceFeatures2* out) {
    static_cast<VkPhysicalDeviceTimelineSemaphoreFeatures*>(out->pNext)->timelineSemaphore=!fake.no_timeline;
}
void VKAPI_PTR properties2(VkPhysicalDevice,VkPhysicalDeviceProperties2* out) {
    auto* id=static_cast<VkPhysicalDeviceIDProperties*>(out->pNext);id->deviceLUIDValid=VK_TRUE;id->deviceLUID[0]=42;
}
VkResult VKAPI_PTR version(uint32_t* out) {*out=VK_API_VERSION_1_2;return VK_SUCCESS;}
void VKAPI_PTR destroy_instance(VkInstance,const VkAllocationCallbacks*) {fake.cleanup.push_back(3);}
void VKAPI_PTR destroy_device(VkDevice,const VkAllocationCallbacks*) {fake.cleanup.push_back(2);}
VkResult VKAPI_PTR idle(VkDevice) {fake.cleanup.push_back(1);return VK_SUCCESS;}
void VKAPI_PTR properties(VkPhysicalDevice device,VkPhysicalDeviceProperties* out) {
    require(device==handle<VkPhysicalDevice>(42),"runtime-selected adapter lost");
    out->apiVersion=VK_API_VERSION_1_2;out->limits.timestampPeriod=2.F;
    std::strcpy(out->deviceName,"Injected headset GPU");
}
void VKAPI_PTR families(VkPhysicalDevice,uint32_t* count,VkQueueFamilyProperties* out) {
    *count=2;
    if(out) {out[0].queueCount=1;out[0].queueFlags=VK_QUEUE_TRANSFER_BIT;
        out[0].timestampValidBits=64;
        out[1].queueCount=1;out[1].queueFlags=fake.no_queue?VK_QUEUE_TRANSFER_BIT:
            VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT;out[1].timestampValidBits=32;}
}
void VKAPI_PTR queue(VkDevice,uint32_t family,uint32_t index,VkQueue* out) {
    require(family==1 && index==0,"wrong queue selected");*out=handle<VkQueue>(5);
}
PFN_vkVoidFunction VKAPI_PTR device_proc(VkDevice,const char* name) {
    if(!std::strcmp(name,"vkDestroyDevice")) return reinterpret_cast<PFN_vkVoidFunction>(destroy_device);
    if(!std::strcmp(name,"vkDeviceWaitIdle")) return reinterpret_cast<PFN_vkVoidFunction>(idle);
    if(!std::strcmp(name,"vkGetDeviceQueue") && !fake.missing_queue) return reinterpret_cast<PFN_vkVoidFunction>(queue);
    return nullptr;
}
PFN_vkVoidFunction VKAPI_PTR instance_proc(VkInstance,const char* name) {
#define VK_ENTRY(n,f) if(!std::strcmp(name,n)) return reinterpret_cast<PFN_vkVoidFunction>(f)
    VK_ENTRY("vkEnumerateInstanceVersion",version);
    VK_ENTRY("vkDestroyInstance",destroy_instance);
    VK_ENTRY("vkGetPhysicalDeviceProperties",properties);
    VK_ENTRY("vkGetPhysicalDeviceQueueFamilyProperties",families);
    if(fake.sharing) {
        VK_ENTRY("vkEnumerateDeviceExtensionProperties",extensions);
        VK_ENTRY("vkGetPhysicalDeviceFeatures2",features2);
        VK_ENTRY("vkGetPhysicalDeviceProperties2",properties2);
    }
    if(!fake.missing_dispatch) {VK_ENTRY("vkGetDeviceProcAddr",device_proc);}
#undef VK_ENTRY
    return nullptr;
}
XrResult XRAPI_PTR requirements(XrInstance,XrSystemId,XrGraphicsRequirementsVulkan2KHR* out) {
    out->minApiVersionSupported=XR_MAKE_VERSION(1,fake.incompatible?3:0,0);
    out->maxApiVersionSupported=XR_MAKE_VERSION(1,3,0);return XR_SUCCESS;
}
XrResult XRAPI_PTR create_instance(XrInstance,const XrVulkanInstanceCreateInfoKHR* info,VkInstance* out,VkResult* result) {
    require(info->systemId==7 && info->pfnGetInstanceProcAddr==instance_proc,"XR instance requirements not forwarded");
    require(info->vulkanCreateInfo->pApplicationInfo->apiVersion==VK_API_VERSION_1_1,"wrong negotiated API");
    *out=handle<VkInstance>(2);*result=VK_SUCCESS;return XR_SUCCESS;
}
XrResult XRAPI_PTR select_device(XrInstance,const XrVulkanGraphicsDeviceGetInfoKHR* info,VkPhysicalDevice* out) {
    require(info->systemId==7 && info->vulkanInstance==handle<VkInstance>(2),"adapter query uses wrong XR system");
    *out=handle<VkPhysicalDevice>(42);return XR_SUCCESS;
}
XrResult XRAPI_PTR create_device(XrInstance,const XrVulkanDeviceCreateInfoKHR* info,VkDevice* out,VkResult* result) {
    ++fake.created;
    require(info->vulkanPhysicalDevice==handle<VkPhysicalDevice>(42),"created device on wrong adapter");
    require(info->vulkanCreateInfo->pQueueCreateInfos[0].queueFamilyIndex==1,"wrong requested queue");
    fake.requested_sharing=info->vulkanCreateInfo->enabledExtensionCount==3;
    if(fake.requested_sharing) require(info->vulkanCreateInfo->pNext
        && static_cast<const VkPhysicalDeviceTimelineSemaphoreFeatures*>(info->vulkanCreateInfo->pNext)->timelineSemaphore,"timeline feature missing");
    if(fake.fail_device) {*result=VK_ERROR_INITIALIZATION_FAILED;return XR_SUCCESS;}
    *out=handle<VkDevice>(4);*result=VK_SUCCESS;return XR_SUCCESS;
}
XrResult XRAPI_PTR xr_proc(XrInstance,const char* name,PFN_xrVoidFunction* out) {
#define XR_ENTRY(n,f) if(!std::strcmp(name,n)) {*out=reinterpret_cast<PFN_xrVoidFunction>(f);return XR_SUCCESS;}
    XR_ENTRY("xrGetVulkanGraphicsRequirements2KHR",requirements)
    XR_ENTRY("xrCreateVulkanInstanceKHR",create_instance)
    XR_ENTRY("xrGetVulkanGraphicsDevice2KHR",select_device)
    XR_ENTRY("xrCreateVulkanDeviceKHR",create_device)
#undef XR_ENTRY
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
}
int main() {
    try {
        starfox::vr::VulkanDevice device;
        const auto init=[&] {return device.initialize(handle<XrInstance>(1),7,instance_proc,xr_proc);};
        require(init(),"bootstrap failed");
        require(device.binding().physicalDevice==handle<VkPhysicalDevice>(42) && device.queue()==handle<VkQueue>(5),"binding incomplete");
        require(device.api_version()==VK_API_VERSION_1_1,"API not retained");
        require(device.timestamp_valid_bits()==32 && device.timestamp_period_ns()==2.,
            "Selected Vulkan queue timestamp properties not retained");
        require(init(),"reinitialization failed");
        require(fake.cleanup==std::vector<int>({1,2,3}),"reinitialization cleanup order wrong");
        device.close();device.close();
        require(fake.cleanup==std::vector<int>({1,2,3,1,2,3}),"close not idempotent");
        require(!device.binding().device && !device.queue() && device.api_version()==0,"closed device remains exposed");
        require(device.timestamp_valid_bits()==0 && device.timestamp_period_ns()==0.,
            "Closed Vulkan device retained timestamp properties");
        fake={};fake.incompatible=true;require(!init() && fake.cleanup.empty(),"incompatible API accepted");
        fake={};fake.no_queue=true;require(!init() && fake.cleanup==std::vector<int>({3}),"missing queue cleanup failed");
        fake={};fake.fail_device=true;require(!init() && fake.cleanup==std::vector<int>({3}),"device failure cleanup failed");
        fake={};fake.missing_dispatch=true;require(!init() && fake.created==0 && fake.cleanup==std::vector<int>({3}),"allocated device without dispatch");
        fake={};fake.missing_queue=true;require(!init() && fake.cleanup==std::vector<int>({1,2,3}),"late failure leaked device");
        require(!device.initialize({},0,nullptr,xr_proc),"invalid inputs accepted");
#if defined(_WIN32)
        fake={};fake.sharing=true;require(init() && fake.requested_sharing && device.external_shadows_enabled()
            && device.adapter_luid() && (*device.adapter_luid())[0]==42,"sharing capability not enabled");
        device.close();require(!device.external_shadows_enabled() && !device.adapter_luid(),"sharing capability not reset");
        fake={};fake.sharing=true;fake.no_timeline=true;require(init() && !fake.requested_sharing && !device.external_shadows_enabled(),"missing timeline not handled");
        device.close();
        fake={};fake.sharing=true;fake.missing_extension=true;require(init() && !fake.requested_sharing && !device.external_shadows_enabled(),"missing extension not handled");
        device.close();
#endif
        std::cout<<"VR Vulkan bootstrap: adapter, version, queue, reinit and rollback checks passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
