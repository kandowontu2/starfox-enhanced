#include "starfox/vr/vulkan_eye_commands.hpp"
#include "starfox/vr/vulkan_loader.hpp"
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
template<class T> T instance_proc(PFN_vkGetInstanceProcAddr get,VkInstance instance,const char* name) {
    const auto result=reinterpret_cast<T>(get(instance,name));
    if(!result) throw std::runtime_error(std::string("Missing Vulkan instance function: ")+name);
    return result;
}
struct InstanceOwner {
    VkInstance handle{};
    PFN_vkDestroyInstance destroy{};
    ~InstanceOwner() {if(handle && destroy) destroy(handle,nullptr);}
};
struct DeviceOwner {
    VkDevice handle{};
    PFN_vkDestroyDevice destroy{};
    ~DeviceOwner() {if(handle && destroy) destroy(handle,nullptr);}
};
void check_timestamp_math() {
    const auto ordinary=starfox::vr::VulkanEyeCommands::timestamp_duration_ms(17,37,64,1000.);
    require(ordinary && std::abs(*ordinary-.02)<1e-12,"64-bit timestamp period conversion is wrong");
    const auto wrapped=starfox::vr::VulkanEyeCommands::timestamp_duration_ms(250,5,8,1'000'000.);
    require(wrapped && std::abs(*wrapped-11.)<1e-12,"valid-bit wraparound conversion is wrong");
    require(!starfox::vr::VulkanEyeCommands::timestamp_duration_ms(1,2,0,1.),
        "queue without timestamp bits produced a GPU duration");
    require(!starfox::vr::VulkanEyeCommands::timestamp_duration_ms(1,2,65,1.),
        "invalid timestamp bit width produced a GPU duration");
    require(!starfox::vr::VulkanEyeCommands::timestamp_duration_ms(1,2,64,0.),
        "invalid timestamp period produced a GPU duration");
}
}

int main(int argc,char** argv) try {
    check_timestamp_math();
    if(argc==2 && std::string(argv[1])=="--math-only") {
        std::cout<<"Vulkan timestamp period conversion and valid-bit wraparound passed\n";
        return 0;
    }
    const bool require_native=argc==2 && std::string(argv[1])=="--require-native";
    require(argc==1 || require_native,"Unexpected Vulkan timestamp test argument");
    const auto unavailable=[&](const std::string& reason) {
        if(require_native) throw std::runtime_error("Native Vulkan timestamp query required: "+reason);
        std::cout<<"SKIP: "<<reason<<'\n';
        return 77;
    };
    starfox::vr::VulkanLoader loader;
    if(!loader.initialize()) return unavailable(loader.status());
    const auto get=loader.get_instance_proc_addr();
    const auto create_instance=instance_proc<PFN_vkCreateInstance>(get,VK_NULL_HANDLE,"vkCreateInstance");
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName="Star Fox Vulkan timestamp check";
    application.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo=&application;
    const char* portability_extension=VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    const auto enumerate_instance_extensions=instance_proc<PFN_vkEnumerateInstanceExtensionProperties>(
        get,VK_NULL_HANDLE,"vkEnumerateInstanceExtensionProperties");
    uint32_t extension_count{};
    if(enumerate_instance_extensions(nullptr,&extension_count,nullptr)==VK_SUCCESS && extension_count<=4096) {
        std::vector<VkExtensionProperties> extensions(extension_count);
        if(enumerate_instance_extensions(nullptr,&extension_count,extensions.data())==VK_SUCCESS) {
            for(const auto& extension:extensions)
                if(std::strcmp(extension.extensionName,VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)==0) {
                    instance_info.enabledExtensionCount=1;instance_info.ppEnabledExtensionNames=&portability_extension;
                    instance_info.flags=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                    break;
                }
        }
    }
    InstanceOwner instance;
    const auto create_result=create_instance(&instance_info,nullptr,&instance.handle);
    if(create_result!=VK_SUCCESS)
        return unavailable("vkCreateInstance failed with VkResult "+std::to_string(create_result));
    instance.destroy=instance_proc<PFN_vkDestroyInstance>(get,instance.handle,"vkDestroyInstance");
    const auto enumerate=instance_proc<PFN_vkEnumeratePhysicalDevices>(get,instance.handle,"vkEnumeratePhysicalDevices");
    uint32_t physical_count{};
    require(enumerate(instance.handle,&physical_count,nullptr)==VK_SUCCESS,"Cannot enumerate Vulkan physical devices");
    if(physical_count==0) return unavailable("Vulkan loader has no physical device");
    std::vector<VkPhysicalDevice> physical_devices(physical_count);
    require(enumerate(instance.handle,&physical_count,physical_devices.data())==VK_SUCCESS,"Cannot read Vulkan physical devices");
    const auto get_properties=instance_proc<PFN_vkGetPhysicalDeviceProperties>(get,instance.handle,"vkGetPhysicalDeviceProperties");
    const auto get_queues=instance_proc<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(get,instance.handle,"vkGetPhysicalDeviceQueueFamilyProperties");
    VkPhysicalDevice physical{};VkQueueFamilyProperties selected_queue{};uint32_t selected_family{};
    VkPhysicalDeviceProperties selected_properties{};
    for(const auto candidate:physical_devices) {
        VkPhysicalDeviceProperties properties{};get_properties(candidate,&properties);
        uint32_t queue_count{};get_queues(candidate,&queue_count,nullptr);
        std::vector<VkQueueFamilyProperties> queues(queue_count);
        if(queue_count) get_queues(candidate,&queue_count,queues.data());
        for(uint32_t index=0;index<queue_count;++index) if(queues[index].queueCount && queues[index].timestampValidBits) {
            physical=candidate;selected_queue=queues[index];selected_family=index;selected_properties=properties;break;
        }
        if(physical) break;
    }
    if(!physical) return unavailable("no physical queue reports timestampValidBits");
    if(require_native) {
        std::string device_name=selected_properties.deviceName;
        for(auto& character:device_name) character=static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        const bool is_lavapipe=selected_properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU
            && (device_name.find("llvmpipe")!=std::string::npos
                || device_name.find("lavapipe")!=std::string::npos);
        if(!is_lavapipe)
            throw std::runtime_error("Required Vulkan ICD is not Mesa Lavapipe/llvmpipe: "+device_name);
    }
    const auto get_device_proc=instance_proc<PFN_vkGetDeviceProcAddr>(get,instance.handle,"vkGetDeviceProcAddr");
    const float priority=1.F;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex=selected_family;queue_info.queueCount=1;queue_info.pQueuePriorities=&priority;
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount=1;device_info.pQueueCreateInfos=&queue_info;
    const auto create_device=instance_proc<PFN_vkCreateDevice>(get,instance.handle,"vkCreateDevice");
    DeviceOwner device;
    require(create_device(physical,&device_info,nullptr,&device.handle)==VK_SUCCESS,"Cannot create Vulkan test device");
    device.destroy=reinterpret_cast<PFN_vkDestroyDevice>(get_device_proc(device.handle,"vkDestroyDevice"));
    require(device.destroy!=nullptr,"Missing Vulkan device destroy function");
    const auto get_queue=reinterpret_cast<PFN_vkGetDeviceQueue>(get_device_proc(device.handle,"vkGetDeviceQueue"));
    require(get_queue!=nullptr,"Missing Vulkan device queue accessor");
    VkQueue queue{};get_queue(device.handle,selected_family,0,&queue);
    require(queue!=VK_NULL_HANDLE,"Vulkan test queue is null");
    starfox::vr::VulkanEyeCommands commands;
    require(commands.initialize(device.handle,queue,selected_family,get_device_proc,
        starfox::vr::VulkanEyeCommands::TimestampConfig{
            selected_queue.timestampValidBits,selected_properties.limits.timestampPeriod}),
        "Vulkan timestamp command owner failed to initialize");
    if(!commands.gpu_timestamps_available()) {
        return unavailable(commands.timestamp_status());
    }
    for(unsigned sample=0;sample<2;++sample) {
        require(commands.submit_work({1,1},[](VkCommandBuffer,VkExtent2D){}),
            "Vulkan timestamp command submission failed");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        auto completion=starfox::vr::VulkanEyeCommands::Completion::pending;
        while(completion==starfox::vr::VulkanEyeCommands::Completion::pending
            && std::chrono::steady_clock::now()<deadline) {
            completion=commands.poll(1'000'000);
            if(completion==starfox::vr::VulkanEyeCommands::Completion::pending)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        require(completion==starfox::vr::VulkanEyeCommands::Completion::complete,
            "Vulkan timestamp query submission did not complete within five seconds");
        const auto elapsed=commands.take_gpu_duration_ms();
        require(elapsed && std::isfinite(*elapsed) && *elapsed>=0.,
            "Fence-complete Vulkan timestamp query did not return a valid GPU duration");
    }
    std::cout<<"Vulkan timestamp query/readback passed on "<<selected_properties.deviceName
        <<" (queue family "<<selected_family<<", valid bits "<<selected_queue.timestampValidBits
        <<", period "<<selected_properties.limits.timestampPeriod<<" ns)\n";
    return 0;
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
