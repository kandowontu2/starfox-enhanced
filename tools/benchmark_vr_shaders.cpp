#include "starfox/vr/vulkan_loader.hpp"
#include "starfox/vr/vulkan_span_pipeline.hpp"
#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string_view>

template<class T> T proc(PFN_vkVoidFunction value) {
    if(!value) throw std::runtime_error("Missing Vulkan function");
    return reinterpret_cast<T>(value);
}
void check(VkResult value) {
    if(value!=VK_SUCCESS) throw std::runtime_error("Vulkan result "+std::to_string(value));
}
int main(int argc,char** argv) {
    const bool fast_compile=argc==2 && std::string_view(argv[1])=="--fast-compile";
    const bool cached=argc==2 && std::string_view(argv[1])=="--cache";
    const bool persistent=argc==3 && std::string_view(argv[1])=="--cache-dir";
    if(argc>1 && !fast_compile && !cached && !persistent) {std::cerr<<"Usage: starfox_vr_shader_bench [--fast-compile | --cache | --cache-dir PATH]\n";return 2;}
    starfox::vr::VulkanLoader loader;
    VkInstance instance{};VkDevice device{};
    VkPipelineCache cache{};PFN_vkDestroyPipelineCache destroy_cache{};
    PFN_vkDestroyInstance destroy_instance{};PFN_vkDestroyDevice destroy_device{};
    int result=0;
    try {
        if(!loader.initialize()) throw std::runtime_error(loader.status());
        const auto get=loader.get_instance_proc_addr();
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName="Star Fox shader timing diagnostic";app.apiVersion=VK_API_VERSION_1_0;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pApplicationInfo=&app;
        check(proc<PFN_vkCreateInstance>(get(nullptr,"vkCreateInstance"))(&info,nullptr,&instance));
        destroy_instance=proc<PFN_vkDestroyInstance>(get(instance,"vkDestroyInstance"));
        const auto enumerate=proc<PFN_vkEnumeratePhysicalDevices>(get(instance,"vkEnumeratePhysicalDevices"));
        uint32_t count=0;check(enumerate(instance,&count,nullptr));
        if(!count) throw std::runtime_error("No Vulkan device");
        std::vector<VkPhysicalDevice> physical(count);check(enumerate(instance,&count,physical.data()));
        VkPhysicalDeviceProperties properties{};
        proc<PFN_vkGetPhysicalDeviceProperties>(get(instance,"vkGetPhysicalDeviceProperties"))(physical[0],&properties);
        std::cout<<"GPU: "<<properties.deviceName<<"; driver="<<properties.driverVersion<<std::endl;
        std::cout<<"Fast compilation: "<<fast_compile<<std::endl;
        const auto queues=proc<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(get(instance,"vkGetPhysicalDeviceQueueFamilyProperties"));
        queues(physical[0],&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);queues(physical[0],&count,families.data());
        uint32_t family=0;while(family<count && !(families[family].queueFlags&VK_QUEUE_COMPUTE_BIT)) ++family;
        if(family==count) throw std::runtime_error("No compute queue");
        const float priority=1;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex=family;queue.queueCount=1;queue.pQueuePriorities=&priority;
        VkDeviceCreateInfo create{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};create.queueCreateInfoCount=1;create.pQueueCreateInfos=&queue;
        check(proc<PFN_vkCreateDevice>(get(instance,"vkCreateDevice"))(physical[0],&create,nullptr,&device));
        const auto get_device=proc<PFN_vkGetDeviceProcAddr>(get(instance,"vkGetDeviceProcAddr"));
        destroy_device=proc<PFN_vkDestroyDevice>(get_device(device,"vkDestroyDevice"));
        starfox::vr::VulkanPipelineCache disk_cache;
        if(persistent) {
            if(!disk_cache.initialize(device,get_device,properties,argv[2])) throw std::runtime_error("Cache initialization failed");
            cache=disk_cache.get();
        }
        if(cached) {
            destroy_cache=proc<PFN_vkDestroyPipelineCache>(get_device(device,"vkDestroyPipelineCache"));
            VkPipelineCacheCreateInfo cache_info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
            check(proc<PFN_vkCreatePipelineCache>(get_device(device,"vkCreatePipelineCache"))(device,&cache_info,nullptr,&cache));
        }
        for(unsigned run=0;run<2;++run) for(unsigned stage=0;stage<8;++stage) {
            std::cout<<"Compiling run="<<run<<" stage="<<stage<<std::endl;
            const auto start=std::chrono::steady_clock::now();
            starfox::vr::VulkanSpanPipeline pipeline;
            if(!pipeline.initialize(device,get_device,static_cast<starfox::vr::SourceComputeStage>(stage),fast_compile,cache))
                throw std::runtime_error(pipeline.status());
            std::cout<<"Compile ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<std::endl;
            if(persistent && run==0 && !disk_cache.checkpoint()) throw std::runtime_error("Cache checkpoint failed");
        }
        std::cout<<"Shader creation only; not gameplay FPS or headset presentation proof."<<std::endl;
    } catch(const std::exception& error) {std::cerr<<error.what()<<std::endl;result=1;}
    if(cache && destroy_cache) destroy_cache(device,cache,nullptr);
    if(device && destroy_device) destroy_device(device,nullptr);
    if(instance && destroy_instance) destroy_instance(instance,nullptr);
    return result;
}
