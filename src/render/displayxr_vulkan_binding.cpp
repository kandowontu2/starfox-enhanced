#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>
#include "starfox/render/displayxr_vulkan_binding.hpp"
#include "starfox/render/sdl_vulkan_bridge.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <vector>

namespace starfox::render {
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
std::uint32_t vk_version(XrVersion v) {
    require(XR_VERSION_MAJOR(v)==1 && XR_VERSION_MINOR(v)<1024 && XR_VERSION_PATCH(v)<4096,
        "Invalid OpenXR Vulkan API range");
    return VK_MAKE_API_VERSION(0,XR_VERSION_MAJOR(v),XR_VERSION_MINOR(v),XR_VERSION_PATCH(v));
}
template<class T> T xr_proc(XrInstance xr,PFN_xrGetInstanceProcAddr get,const char* name) {
    PFN_xrVoidFunction p{};require(XR_SUCCEEDED(get(xr,name,&p)) && p,name);
    return reinterpret_cast<T>(p);
}
template<class T> T vk_proc(VkInstance instance,PFN_vkGetInstanceProcAddr get,const char* name) {
    const auto p=get(instance,name);require(p,name);return reinterpret_cast<T>(p);
}
}
struct DisplayXrVulkanBinding::State {
    std::atomic<unsigned> references{1};std::mutex mutex;bool active{true};
    XrInstance xr{};XrSystemId system{};std::uint32_t minimum{},maximum{};
    PFN_xrCreateVulkanInstanceKHR create_instance{};
    PFN_xrGetVulkanGraphicsDevice2KHR physical_device{};
    PFN_xrCreateVulkanDeviceKHR create_device{};
    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    struct Instance {VkInstance handle{};PFN_vkGetInstanceProcAddr get{};std::uint32_t version{};VkPhysicalDevice physical{};};
    struct Device {VkDevice handle{};VkInstance instance{};VkPhysicalDevice physical{};std::uint32_t family{};};
    std::vector<Instance> instances;std::vector<Device> devices;
    StarfoxSdlVulkanXrCreateV1 api{1,0,this,retain,release,instance_callback,physical_callback,
        device_callback,instance_destroyed,device_destroyed};
    static bool retain(void* user) noexcept {
        auto& s=*static_cast<State*>(user);std::lock_guard lock(s.mutex);
        if(!s.active) return false;++s.references;return true;
    }
    static void release(void* user) noexcept {
        auto* s=static_cast<State*>(user);if(s->references.fetch_sub(1)==1) delete s;
    }
    static void SDLCALL property_cleanup(void* user,void*) {release(user);}
    static VkResult instance_callback(void* user,PFN_vkGetInstanceProcAddr get,
        const VkInstanceCreateInfo* info,const VkAllocationCallbacks* allocator,VkInstance* out) noexcept {
        if(out) *out=VK_NULL_HANDLE;
        if(!user || !get || !info || !out || info->sType!=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
            || !info->pApplicationInfo || info->pApplicationInfo->sType!=VK_STRUCTURE_TYPE_APPLICATION_INFO)
            return VK_ERROR_INITIALIZATION_FAILED;
        auto& s=*static_cast<State*>(user);std::lock_guard lock(s.mutex);
        VkInstance handle{};VkResult result=VK_ERROR_INITIALIZATION_FAILED;
        try {
            require(s.active,"OpenXR Vulkan creation was revoked");
            const auto version=info->pApplicationInfo->apiVersion;
            require(VK_API_VERSION_VARIANT(version)==0 && version>=s.minimum && version<=s.maximum,
                "SDL Vulkan API is outside the OpenXR requirements");
            require(s.instances.size()<64,"Too many live OpenXR Vulkan instances");
            // Reserve before allocating native resources, so allocation failure
            // cannot lose the only handle cleanup/notification record.
            s.instances.reserve(s.instances.size()+1);
            XrVulkanInstanceCreateInfoKHR xr_info{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
            xr_info.systemId=s.system;xr_info.pfnGetInstanceProcAddr=get;
            xr_info.vulkanCreateInfo=info;xr_info.vulkanAllocator=allocator;
            const auto xr_result=s.create_instance(s.xr,&xr_info,&handle,&result);
            require(XR_SUCCEEDED(xr_result) && result==VK_SUCCESS && handle,"xrCreateVulkanInstanceKHR failed");
            // Resolve the cleanup entry while the owner is still intact.
            vk_proc<PFN_vkDestroyInstance>(handle,get,"vkDestroyInstance");
            s.instances.push_back({handle,get,version,{}});*out=handle;return VK_SUCCESS;
        } catch(const std::exception& error) {
            // Vulkan failure outputs are undefined, even if a broken runtime
            // writes a non-null value. Only a successful Vulkan allocation
            // (possibly paired with a failed XR result) is ours to destroy.
            if(result==VK_SUCCESS && handle) if(auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(get(handle,"vkDestroyInstance")))
                destroy(handle,allocator);
            SDL_SetError("%s",error.what());return VK_ERROR_INITIALIZATION_FAILED;
        } catch(...) {
            if(result==VK_SUCCESS && handle) if(auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(get(handle,"vkDestroyInstance"))) destroy(handle,allocator);
            SDL_SetError("OpenXR Vulkan instance callback failed");return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    static VkResult physical_callback(void* user,VkInstance handle,VkPhysicalDevice* out) noexcept {
        if(out) *out=VK_NULL_HANDLE;if(!user || !handle || !out) return VK_ERROR_INITIALIZATION_FAILED;
        auto& s=*static_cast<State*>(user);std::lock_guard lock(s.mutex);
        try {
            require(s.active,"OpenXR Vulkan creation was revoked");
            auto found=std::find_if(s.instances.begin(),s.instances.end(),[&](const auto& i) {return i.handle==handle;});
            require(found!=s.instances.end(),"Vulkan instance was not created through this OpenXR runtime");
            XrVulkanGraphicsDeviceGetInfoKHR info{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
            info.systemId=s.system;info.vulkanInstance=handle;
            VkPhysicalDevice physical{};
            require(XR_SUCCEEDED(s.physical_device(s.xr,&info,&physical)) && physical,"xrGetVulkanGraphicsDevice2KHR failed");
            // Verify membership before dereferencing any runtime-returned handle.
            const auto enumerate=vk_proc<PFN_vkEnumeratePhysicalDevices>(handle,found->get,"vkEnumeratePhysicalDevices");
            std::uint32_t count{};require(enumerate(handle,&count,nullptr)==VK_SUCCESS && count && count<=1024,
                "Invalid Vulkan physical-device enumeration");
            std::vector<VkPhysicalDevice> devices(count);
            require(enumerate(handle,&count,devices.data())==VK_SUCCESS && count<=devices.size(),"Vulkan devices changed during selection");
            require(std::find(devices.begin(),devices.begin()+count,physical)!=devices.begin()+count,
                "OpenXR returned a foreign Vulkan physical device");
            VkPhysicalDeviceProperties properties{};
            vk_proc<PFN_vkGetPhysicalDeviceProperties>(handle,found->get,"vkGetPhysicalDeviceProperties")(physical,&properties);
            require(properties.apiVersion>=found->version,"OpenXR adapter does not support SDL's Vulkan API");
            found->physical=physical;*out=physical;return VK_SUCCESS;
        } catch(const std::exception& error) {SDL_SetError("%s",error.what());return VK_ERROR_INITIALIZATION_FAILED;}
        catch(...) {SDL_SetError("OpenXR Vulkan adapter callback failed");return VK_ERROR_INITIALIZATION_FAILED;}
    }
    static VkResult device_callback(void* user,PFN_vkGetInstanceProcAddr get,VkInstance instance,VkPhysicalDevice physical,
        const VkDeviceCreateInfo* info,const VkAllocationCallbacks* allocator,VkDevice* out) noexcept {
        if(out) *out=VK_NULL_HANDLE;
        if(!user || !get || !physical || !info || !out || info->sType!=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO)
            return VK_ERROR_INITIALIZATION_FAILED;
        auto& s=*static_cast<State*>(user);std::lock_guard lock(s.mutex);
        VkDevice handle{};PFN_vkDestroyDevice destroy{};VkResult result=VK_ERROR_INITIALIZATION_FAILED;
        try {
            require(s.active,"OpenXR Vulkan creation was revoked");
            auto found=std::find_if(s.instances.begin(),s.instances.end(),[&](const auto& i) {return i.handle==instance && i.physical==physical && i.get==get;});
            require(found!=s.instances.end(),"SDL selected a different adapter from OpenXR");
            require(info->queueCreateInfoCount==1 && info->pQueueCreateInfos
                && info->pQueueCreateInfos[0].sType==VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
                && info->pQueueCreateInfos[0].queueCount && !info->pQueueCreateInfos[0].flags,
                "OpenXR requires SDL's unprotected unified queue");
            const auto family=info->pQueueCreateInfos[0].queueFamilyIndex;
            const auto queues=vk_proc<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(found->handle,get,"vkGetPhysicalDeviceQueueFamilyProperties");
            std::uint32_t count{};queues(physical,&count,nullptr);require(count && count<=1024,"Invalid Vulkan queue-family count");
            std::vector<VkQueueFamilyProperties> families(count);queues(physical,&count,families.data());
            require(count<=families.size() && family<count && families[family].queueCount>=info->pQueueCreateInfos[0].queueCount
                && (families[family].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT),
                "SDL queue does not support Leia graphics/compute");
            require(s.devices.size()<64,"Too many live OpenXR Vulkan devices");s.devices.reserve(s.devices.size()+1);
            destroy=vk_proc<PFN_vkDestroyDevice>(found->handle,get,"vkDestroyDevice");
            XrVulkanDeviceCreateInfoKHR xr_info{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
            xr_info.systemId=s.system;xr_info.pfnGetInstanceProcAddr=get;xr_info.vulkanPhysicalDevice=physical;
            xr_info.vulkanCreateInfo=info;xr_info.vulkanAllocator=allocator;
            const auto xr_result=s.create_device(s.xr,&xr_info,&handle,&result);
            require(XR_SUCCEEDED(xr_result) && result==VK_SUCCESS && handle,"xrCreateVulkanDeviceKHR failed");
            s.devices.push_back({handle,found->handle,physical,family});*out=handle;return VK_SUCCESS;
        } catch(const std::exception& error) {
            if(result==VK_SUCCESS && handle && destroy) destroy(handle,allocator);
            SDL_SetError("%s",error.what());return VK_ERROR_INITIALIZATION_FAILED;
        } catch(...) {
            if(result==VK_SUCCESS && handle && destroy) destroy(handle,allocator);
            SDL_SetError("OpenXR Vulkan device callback failed");return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    static void device_destroyed(void* user,VkDevice handle) noexcept {
        auto& s=*static_cast<State*>(user);std::lock_guard lock(s.mutex);
        std::erase_if(s.devices,[&](const auto& d) {return d.handle==handle;});
        if(s.binding.device==handle) s.binding={XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    }
    static void instance_destroyed(void* user,VkInstance handle) noexcept {
        auto& s=*static_cast<State*>(user);std::lock_guard lock(s.mutex);
        std::erase_if(s.instances,[&](const auto& i) {return i.handle==handle;});
        if(s.binding.instance==handle) s.binding={XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    }
};
DisplayXrVulkanBinding::~DisplayXrVulkanBinding() {close();}
void DisplayXrVulkanBinding::close() noexcept {
    auto* s=state_;state_=nullptr;if(!s) return;
    {std::lock_guard lock(s->mutex);s->active=false;s->binding={XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        s->create_instance=nullptr;s->physical_device=nullptr;s->create_device=nullptr;s->xr=XR_NULL_HANDLE;}
    State::release(s);
}
const void* DisplayXrVulkanBinding::binding() const noexcept {
    if(!state_) return nullptr;std::lock_guard lock(state_->mutex);
    return state_->active && state_->binding.device?&state_->binding:nullptr;
}
bool DisplayXrVulkanBinding::initialize(const DisplayXrRuntime& runtime) {
    if(!runtime.detected() || runtime.backend()!=DisplayXrBackend::vulkan) {
        close();status_="Confirmed Vulkan Leia runtime required";return false;
    }
    return initialize_with_api(runtime.instance(),runtime.system(),runtime.get_instance_proc());
}
bool DisplayXrVulkanBinding::initialize_with_api(XrInstance xr,XrSystemId system,PFN_xrGetInstanceProcAddr get) {
    close();
    try {
        require(xr && system && get,"Missing Vulkan OpenXR system/dispatch");
        const auto requirements=xr_proc<PFN_xrGetVulkanGraphicsRequirements2KHR>(xr,get,"xrGetVulkanGraphicsRequirements2KHR");
        XrGraphicsRequirementsVulkan2KHR need{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        require(XR_SUCCEEDED(requirements(xr,system,&need)),"OpenXR Vulkan requirements failed");
        const auto minimum=vk_version(need.minApiVersionSupported),maximum=vk_version(need.maxApiVersionSupported);
        require(minimum<=maximum,"Inverted OpenXR Vulkan API range");
        auto next=std::make_unique<State>();next->xr=xr;next->system=system;next->minimum=minimum;next->maximum=maximum;
        next->api.default_api_version=std::clamp(std::uint32_t(VK_API_VERSION_1_1),minimum,maximum);
        next->create_instance=xr_proc<PFN_xrCreateVulkanInstanceKHR>(xr,get,"xrCreateVulkanInstanceKHR");
        next->physical_device=xr_proc<PFN_xrGetVulkanGraphicsDevice2KHR>(xr,get,"xrGetVulkanGraphicsDevice2KHR");
        next->create_device=xr_proc<PFN_xrCreateVulkanDeviceKHR>(xr,get,"xrCreateVulkanDeviceKHR");
        state_=next.release();status_="Leia Vulkan2 creation handshake ready";return true;
    } catch(const std::exception& error) {status_=error.what();return false;}
}
bool DisplayXrVulkanBinding::configure_gpu_properties(SDL_PropertiesID props) const {
    if(!props || !state_ || !State::retain(state_)) return SDL_SetError("Leia Vulkan creation is not ready");
    return SDL_SetPointerPropertyWithCleanup(props,STARFOX_SDL_VULKAN_XR_CREATE,&state_->api,State::property_cleanup,state_);
}
bool DisplayXrVulkanBinding::attach(SDL_GPUDevice* device) {
    try {
        require(state_,"Leia Vulkan creation is not ready");
        std::lock_guard lock(state_->mutex);auto& s=*state_;
        s.binding={XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        require(device && s.active && std::string_view(SDL_GetGPUDeviceDriver(device))=="vulkan","Leia requires the negotiated SDL Vulkan device");
        const auto props=SDL_GetGPUDeviceProperties(device);
        require(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_XR_CREATION_CONTEXT,nullptr)==state_,
            "SDL device was not created through this OpenXR runtime");
        const auto* api=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
        const auto* queue=static_cast<const StarfoxSdlVulkanXrBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_XR_BRIDGE,nullptr));
        require(api && api->version==2 && api->get_instance_proc && api->get_device_proc && queue && queue->version==1 && queue->queue,
            "SDL Vulkan native bridges unavailable");
        const auto found=std::find_if(s.devices.begin(),s.devices.end(),[&](const auto& d) {
            return d.handle==api->device && d.instance==api->instance && d.physical==api->physical_device && d.family==api->queue_family;
        });
        require(found!=s.devices.end(),"SDL device/adapter/queue differs from OpenXR creation");
        const auto get_queue=reinterpret_cast<PFN_vkGetDeviceQueue>(api->get_device_proc(api->device,"vkGetDeviceQueue"));
        require(get_queue,"SDL Vulkan queue accessor unavailable");VkQueue expected{};get_queue(api->device,api->queue_family,0,&expected);
        require(expected && expected==queue->queue(device),"Leia must use SDL's exact Vulkan queue");
        s.binding.instance=api->instance;s.binding.physicalDevice=api->physical_device;s.binding.device=api->device;
        s.binding.queueFamilyIndex=api->queue_family;s.binding.queueIndex=0;
        status_="Leia Vulkan2 binding matched to the runtime-created SDL device and queue";return true;
    } catch(const std::exception& error) {status_=error.what();return false;}
}
}
