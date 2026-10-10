#include "starfox/vr/vulkan_device.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cstring>

namespace starfox::vr {
namespace {
void require(bool success,const char* message) {if(!success) throw std::runtime_error(message);}
std::uint32_t vk_version(XrVersion v) {
    require(XR_VERSION_MAJOR(v)<128 && XR_VERSION_MINOR(v)<1024 && XR_VERSION_PATCH(v)<4096,
        "OpenXR returned an unrepresentable Vulkan version");
    return VK_MAKE_VERSION(XR_VERSION_MAJOR(v),XR_VERSION_MINOR(v),XR_VERSION_PATCH(v));
}
template<class T> T xr_proc(XrInstance xr,PFN_xrGetInstanceProcAddr get,const char* name) {
    PFN_xrVoidFunction p{};
    require(XR_SUCCEEDED(get(xr,name,&p)) && p,name);
    return reinterpret_cast<T>(p);
}
template<class T> T vk_proc(VkInstance instance,PFN_vkGetInstanceProcAddr get,const char* name) {
    const auto p=get(instance,name);require(p!=nullptr,name);return reinterpret_cast<T>(p);
}
}
VulkanDevice::~VulkanDevice() {close();}
void VulkanDevice::close() noexcept {
    if(binding_.device) {
        if(wait_idle_) wait_idle_(binding_.device);
        if(destroy_device_) destroy_device_(binding_.device,nullptr);
    }
    if(binding_.instance && destroy_instance_) destroy_instance_(binding_.instance,nullptr);
    binding_={XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};queue_={};version_=0;
    timestamp_valid_bits_=0;timestamp_period_ns_=0.;
    external_shadows_=false;luid_.reset();
    destroy_instance_=nullptr;destroy_device_=nullptr;wait_idle_=nullptr;
}
bool VulkanDevice::initialize(XrInstance xr,XrSystemId system,PFN_vkGetInstanceProcAddr vk_get,
    PFN_xrGetInstanceProcAddr xr_get) {
    close();
    try {
        require(xr && system && vk_get && xr_get,"Missing XR system or Vulkan loader");
        const auto requirements=xr_proc<PFN_xrGetVulkanGraphicsRequirements2KHR>(xr,xr_get,"xrGetVulkanGraphicsRequirements2KHR");
        const auto create_instance=xr_proc<PFN_xrCreateVulkanInstanceKHR>(xr,xr_get,"xrCreateVulkanInstanceKHR");
        const auto get_device=xr_proc<PFN_xrGetVulkanGraphicsDevice2KHR>(xr,xr_get,"xrGetVulkanGraphicsDevice2KHR");
        const auto create_device=xr_proc<PFN_xrCreateVulkanDeviceKHR>(xr,xr_get,"xrCreateVulkanDeviceKHR");
        XrGraphicsRequirementsVulkan2KHR required{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        require(XR_SUCCEEDED(requirements(xr,system,&required)),"Read XR Vulkan requirements failed");
        std::uint32_t available=VK_API_VERSION_1_0;
        if(const auto enumerate=reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vk_get(nullptr,"vkEnumerateInstanceVersion")))
            require(enumerate(&available)==VK_SUCCESS,"Read Vulkan loader version failed");
        const auto minimum=vk_version(required.minApiVersionSupported);
        const auto maximum=std::min(vk_version(required.maxApiVersionSupported),available);
        require(minimum<=maximum,"OpenXR and Vulkan loader have no compatible API version");
        version_=std::max(minimum,std::min(maximum,std::uint32_t(VK_API_VERSION_1_1)));
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.pApplicationName="Star Fox Enhanced VR";application.apiVersion=version_;
        VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance_info.pApplicationInfo=&application;
        XrVulkanInstanceCreateInfoKHR xr_instance{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        xr_instance.systemId=system;xr_instance.pfnGetInstanceProcAddr=vk_get;xr_instance.vulkanCreateInfo=&instance_info;
        VkResult result=VK_ERROR_INITIALIZATION_FAILED;
        const auto instance_result=create_instance(xr,&xr_instance,&binding_.instance,&result);
        if(binding_.instance) destroy_instance_=vk_proc<PFN_vkDestroyInstance>(binding_.instance,vk_get,"vkDestroyInstance");
        require(XR_SUCCEEDED(instance_result) && result==VK_SUCCESS && binding_.instance,"Create XR Vulkan instance failed");
        XrVulkanGraphicsDeviceGetInfoKHR device_info{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        device_info.systemId=system;device_info.vulkanInstance=binding_.instance;
        require(XR_SUCCEEDED(get_device(xr,&device_info,&binding_.physicalDevice)) && binding_.physicalDevice,
            "Select headset Vulkan adapter failed");
        VkPhysicalDeviceProperties properties{};
        vk_proc<PFN_vkGetPhysicalDeviceProperties>(binding_.instance,vk_get,"vkGetPhysicalDeviceProperties")(binding_.physicalDevice,&properties);
        require(properties.apiVersion>=version_,"Headset adapter does not support the requested Vulkan API");
        const auto queues=vk_proc<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(binding_.instance,vk_get,"vkGetPhysicalDeviceQueueFamilyProperties");
        std::uint32_t count{};queues(binding_.physicalDevice,&count,nullptr);
        require(count>0 && count<=1024,"Invalid Vulkan queue family count");
        std::vector<VkQueueFamilyProperties> families(count);queues(binding_.physicalDevice,&count,families.data());
        require(count<=families.size(),"Vulkan queue family count changed");
        families.resize(count);
        const auto family=std::find_if(families.begin(),families.end(),[](const auto& f) {
            return f.queueCount && (f.queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT);
        });
        require(family!=families.end(),"Headset adapter has no combined graphics/compute queue");
        binding_.queueFamilyIndex=std::uint32_t(family-families.begin());binding_.queueIndex=0;
        timestamp_valid_bits_=family->timestampValidBits;
        timestamp_period_ns_=properties.limits.timestampPeriod;
        const float priority=1;
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex=binding_.queueFamilyIndex;queue_info.queueCount=1;queue_info.pQueuePriorities=&priority;
        VkDeviceCreateInfo create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        create_info.queueCreateInfoCount=1;create_info.pQueueCreateInfos=&queue_info;
        // Optional Windows sharing support, never required for headset startup.
        // The runtime-selected adapter remains authoritative.
        VkPhysicalDeviceTimelineSemaphoreFeatures timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
        const char* shared_extensions[]{"VK_KHR_external_memory_win32","VK_KHR_external_semaphore_win32","VK_KHR_timeline_semaphore"};
#if defined(_WIN32)
        if(version_>=VK_API_VERSION_1_1) {
            const auto enumerate=reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(vk_get(binding_.instance,"vkEnumerateDeviceExtensionProperties"));
            const auto features=reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(vk_get(binding_.instance,"vkGetPhysicalDeviceFeatures2"));
            const auto ids=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(vk_get(binding_.instance,"vkGetPhysicalDeviceProperties2"));
            if(enumerate && features && ids) {
                VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
                VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&id;
                ids(binding_.physicalDevice,&props);
                if(id.deviceLUIDValid) {luid_.emplace();std::copy_n(id.deviceLUID,8,luid_->begin());}
                uint32_t extension_count{};
                if(luid_ && enumerate(binding_.physicalDevice,nullptr,&extension_count,nullptr)==VK_SUCCESS && extension_count<=4096) {
                    std::vector<VkExtensionProperties> extensions(extension_count);
                    if(enumerate(binding_.physicalDevice,nullptr,&extension_count,extensions.data())==VK_SUCCESS && extension_count<=extensions.size()) {
                        extensions.resize(extension_count);
                        bool supported=true;
                        for(const auto name:shared_extensions) supported &= std::any_of(extensions.begin(),extensions.end(),[&](const auto& ext){return std::strcmp(name,ext.extensionName)==0;});
                        if(supported) {
                            VkPhysicalDeviceFeatures2 caps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};caps.pNext=&timeline;
                            features(binding_.physicalDevice,&caps);
                            external_shadows_=timeline.timelineSemaphore==VK_TRUE;
                        }
                    }
                }
            }
        }
#endif
        if(external_shadows_) {
            create_info.enabledExtensionCount=3;create_info.ppEnabledExtensionNames=shared_extensions;
            create_info.pNext=&timeline;
        }
        XrVulkanDeviceCreateInfoKHR xr_device{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        xr_device.systemId=system;xr_device.pfnGetInstanceProcAddr=vk_get;
        xr_device.vulkanPhysicalDevice=binding_.physicalDevice;xr_device.vulkanCreateInfo=&create_info;
        // Resolve dispatch before allocating a device, so a broken loader
        // cannot leave a newly created device without a cleanup path.
        const auto device_proc=vk_proc<PFN_vkGetDeviceProcAddr>(binding_.instance,vk_get,"vkGetDeviceProcAddr");
        result=VK_ERROR_INITIALIZATION_FAILED;
        const auto device_result=create_device(xr,&xr_device,&binding_.device,&result);
        if(binding_.device) {
            destroy_device_=reinterpret_cast<PFN_vkDestroyDevice>(device_proc(binding_.device,"vkDestroyDevice"));
            wait_idle_=reinterpret_cast<PFN_vkDeviceWaitIdle>(device_proc(binding_.device,"vkDeviceWaitIdle"));
        }
        require(XR_SUCCEEDED(device_result) && result==VK_SUCCESS && binding_.device && destroy_device_ && wait_idle_,
            "Create XR Vulkan device failed");
        const auto get_queue=reinterpret_cast<PFN_vkGetDeviceQueue>(device_proc(binding_.device,"vkGetDeviceQueue"));
        require(get_queue!=nullptr,"Missing Vulkan queue accessor");
        get_queue(binding_.device,binding_.queueFamilyIndex,0,&queue_);
        require(queue_!=nullptr,"Missing headset Vulkan queue");
        status_=std::string("Headset Vulkan device ready: ")+properties.deviceName;return true;
    } catch(const std::exception& error) {status_=error.what();close();return false;}
}
}
