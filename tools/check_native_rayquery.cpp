// Execute the exact checked-in Linux GLSL reflection shader on a native Vulkan
// ray-query device. No DXR, SDL bridge, precomputed GPU hits or colour oracle.
// Windows runs verify this SPIR-V, NOT the Linux SDL owner/lifetime integration.
#define VK_NO_PROTOTYPES
#include "starfox/vr/vulkan_loader.hpp"
#include "native_reflection_fixture.hpp"
#include "native_indexed_cutout_fixture.hpp"
#include "native_shadow_cutout_fixture.hpp"
#include "native_rgba_shadow_fixture.hpp"
#include "native_rgba_reflection_fixture.hpp"
#include "native_water_fixture.hpp"
#include "native_primary_range_fixture.hpp"
#include "native_environment_cube_fixture.hpp"
#include "native_ground_fixture.hpp"
#include "native_model_fixture.hpp"
#include "native_empty_fixture.hpp"
#include "../src/render/shaders/generated/vulkan_reflection_rayquery.hpp"
#include "../src/render/shaders/generated/vulkan_shadow_rayquery.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok,const char* text) {if(!ok) throw std::runtime_error(text);}
void check(VkResult result,const char* text) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(text)+": "+std::to_string(result));
}
struct Buffer {VkBuffer handle{};VkDeviceMemory memory{};VkDeviceSize size{};};
struct Vulkan {
    starfox::vr::VulkanLoader loader;
    VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};
    PFN_vkGetDeviceProcAddr get_device{};
    VkPhysicalDeviceMemoryProperties memory{};
    VkCommandPool command_pool{};VkCommandBuffer command{};
    std::vector<Buffer> buffers;
    std::vector<VkAccelerationStructureKHR> structures;
    VkDescriptorSetLayout layout{};VkPipelineLayout pipeline_layout{};
    VkDescriptorPool descriptor_pool{};VkShaderModule shader{};VkPipeline pipeline{},native_water_pipeline{},native_surface_pipeline{},native_model_pipeline{};
    template<class T> T instance_proc(const char* name) {
        auto fn=loader.get_instance_proc_addr()(instance,name);require(fn!=nullptr,name);return reinterpret_cast<T>(fn);
    }
    template<class T> T proc(const char* name) {
        auto fn=get_device(device,name);require(fn!=nullptr,name);return reinterpret_cast<T>(fn);
    }
#define VCALL(name) proc<PFN_##name>(#name)
#define ICALL(name) instance_proc<PFN_##name>(#name)
    ~Vulkan() {
        if(device) {
            VCALL(vkDeviceWaitIdle)(device);
            if(pipeline) VCALL(vkDestroyPipeline)(device,pipeline,nullptr);
            if(native_water_pipeline) VCALL(vkDestroyPipeline)(device,native_water_pipeline,nullptr);
            if(native_surface_pipeline) VCALL(vkDestroyPipeline)(device,native_surface_pipeline,nullptr);
            if(native_model_pipeline) VCALL(vkDestroyPipeline)(device,native_model_pipeline,nullptr);
            if(shader) VCALL(vkDestroyShaderModule)(device,shader,nullptr);
            if(descriptor_pool) VCALL(vkDestroyDescriptorPool)(device,descriptor_pool,nullptr);
            if(pipeline_layout) VCALL(vkDestroyPipelineLayout)(device,pipeline_layout,nullptr);
            if(layout) VCALL(vkDestroyDescriptorSetLayout)(device,layout,nullptr);
            for(auto as:structures) VCALL(vkDestroyAccelerationStructureKHR)(device,as,nullptr);
            for(auto b:buffers) {VCALL(vkDestroyBuffer)(device,b.handle,nullptr);VCALL(vkFreeMemory)(device,b.memory,nullptr);}
            if(command_pool) VCALL(vkDestroyCommandPool)(device,command_pool,nullptr);
            VCALL(vkDestroyDevice)(device,nullptr);
        }
        if(instance) ICALL(vkDestroyInstance)(instance,nullptr);
    }
    void initialize(bool integrated) {
        require(loader.initialize(),loader.status().c_str());
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_2;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
        check(ICALL(vkCreateInstance)(&ci,nullptr,&instance),"Create native ray-query instance");
        std::uint32_t count=0;check(ICALL(vkEnumeratePhysicalDevices)(instance,&count,nullptr),"Count devices");
        std::vector<VkPhysicalDevice> devices(count);check(ICALL(vkEnumeratePhysicalDevices)(instance,&count,devices.data()),"Enumerate devices");
        const auto preferred=integrated?VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        const auto type=[&](VkPhysicalDevice d){VkPhysicalDeviceProperties p{};ICALL(vkGetPhysicalDeviceProperties)(d,&p);return p.deviceType;};
        std::stable_sort(devices.begin(),devices.end(),[&](auto a,auto b){return (type(a)==preferred)>(type(b)==preferred);});
        std::uint32_t family=0;
        for(auto candidate:devices) {
            VkPhysicalDeviceRayQueryFeaturesKHR query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
            VkPhysicalDeviceAccelerationStructureFeaturesKHR as{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};as.pNext=&query;
            VkPhysicalDeviceVulkan12Features core{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};core.pNext=&as;
            VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&core;
            ICALL(vkGetPhysicalDeviceFeatures2)(candidate,&features);
            if(!query.rayQuery || !as.accelerationStructure || !core.bufferDeviceAddress) continue;
            std::uint32_t ec=0;check(ICALL(vkEnumerateDeviceExtensionProperties)(candidate,nullptr,&ec,nullptr),"Count extensions");
            std::vector<VkExtensionProperties> extensions(ec);check(ICALL(vkEnumerateDeviceExtensionProperties)(candidate,nullptr,&ec,extensions.data()),"Read extensions");
            bool ready=true;
            for(const char* name:{"VK_KHR_acceleration_structure","VK_KHR_ray_query","VK_KHR_deferred_host_operations"})
                ready&=std::any_of(extensions.begin(),extensions.end(),[&](auto e){return std::strcmp(e.extensionName,name)==0;});
            if(!ready) continue;
            std::uint32_t qc=0;ICALL(vkGetPhysicalDeviceQueueFamilyProperties)(candidate,&qc,nullptr);
            std::vector<VkQueueFamilyProperties> queues(qc);ICALL(vkGetPhysicalDeviceQueueFamilyProperties)(candidate,&qc,queues.data());
            for(family=0;family<qc;++family) if(queues[family].queueCount && (queues[family].queueFlags&VK_QUEUE_COMPUTE_BIT)) break;
            if(family==qc) continue;
            physical=candidate;break;
        }
        require(physical!=VK_NULL_HANDLE,"No native Vulkan ray-query device");
        VkPhysicalDeviceProperties props{};ICALL(vkGetPhysicalDeviceProperties)(physical,&props);
        std::cout<<"Native Vulkan ray-query shader on "<<props.deviceName<<std::endl;
        const float priority=1;
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
        VkPhysicalDeviceRayQueryFeaturesKHR query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};query.rayQuery=VK_TRUE;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR as{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};as.pNext=&query;as.accelerationStructure=VK_TRUE;
        VkPhysicalDeviceVulkan12Features core{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};core.pNext=&as;core.bufferDeviceAddress=VK_TRUE;
        const char* extensions[]{"VK_KHR_acceleration_structure","VK_KHR_ray_query","VK_KHR_deferred_host_operations"};
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.pNext=&core;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
        dci.enabledExtensionCount=3;dci.ppEnabledExtensionNames=extensions;
        check(ICALL(vkCreateDevice)(physical,&dci,nullptr,&device),"Create native ray-query device");
        get_device=ICALL(vkGetDeviceProcAddr);VCALL(vkGetDeviceQueue)(device,family,0,&queue);
        ICALL(vkGetPhysicalDeviceMemoryProperties)(physical,&memory);
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=family;cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        check(VCALL(vkCreateCommandPool)(device,&cp,nullptr,&command_pool),"Create command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=command_pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        check(VCALL(vkAllocateCommandBuffers)(device,&ca,&command),"Allocate commands");
    }
    Buffer buffer(VkDeviceSize size,VkBufferUsageFlags usage,bool host) {
        Buffer b{};b.size=size;
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=size;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(VCALL(vkCreateBuffer)(device,&ci,nullptr,&b.handle),"Create buffer");
        VkMemoryRequirements mr{};VCALL(vkGetBufferMemoryRequirements)(device,b.handle,&mr);
        const auto wanted=host?VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        unsigned type=0;for(;type<memory.memoryTypeCount;++type) if((mr.memoryTypeBits&(1u<<type)) && (memory.memoryTypes[type].propertyFlags&wanted)==unsigned(wanted)) break;
        require(type<memory.memoryTypeCount,"No memory type");
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=type;
        if(usage&VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ai.pNext=&flags;
        check(VCALL(vkAllocateMemory)(device,&ai,nullptr,&b.memory),"Allocate buffer");
        check(VCALL(vkBindBufferMemory)(device,b.handle,b.memory,0),"Bind buffer");buffers.push_back(b);return b;
    }
    void copy(Buffer b,void* data,bool upload) {
        void* at=nullptr;check(VCALL(vkMapMemory)(device,b.memory,0,b.size,0,&at),"Map buffer");
        if(upload) std::memcpy(at,data,std::size_t(b.size));else std::memcpy(data,at,std::size_t(b.size));
        VCALL(vkUnmapMemory)(device,b.memory);
    }
    void upload(Buffer b,const void* data) {
        void* at=nullptr;check(VCALL(vkMapMemory)(device,b.memory,0,b.size,0,&at),"Map upload buffer");
        std::memcpy(at,data,std::size_t(b.size));VCALL(vkUnmapMemory)(device,b.memory);
    }
    VkDeviceAddress address(Buffer b) {
        VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};info.buffer=b.handle;
        return VCALL(vkGetBufferDeviceAddress)(device,&info);
    }
    void begin() {
        check(VCALL(vkResetCommandBuffer)(command,0),"Reset command");
        VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(VCALL(vkBeginCommandBuffer)(command,&info),"Begin command");
    }
    void submit() {
        check(VCALL(vkEndCommandBuffer)(command),"End command");
        VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&command;
        check(VCALL(vkQueueSubmit)(queue,1,&info,VK_NULL_HANDLE),"Submit command");check(VCALL(vkQueueWaitIdle)(queue),"Wait for ray-query result");
    }
    VkAccelerationStructureKHR build(VkAccelerationStructureTypeKHR type,VkAccelerationStructureGeometryKHR geometry,unsigned count) {
        VkAccelerationStructureBuildGeometryInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        info.type=type;info.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;info.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        info.geometryCount=1;info.pGeometries=&geometry;
        VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        VCALL(vkGetAccelerationStructureBuildSizesKHR)(device,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,&info,&count,&sizes);
        auto storage=buffer(sizes.accelerationStructureSize,VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,false);
        VkAccelerationStructureCreateInfoKHR ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};ci.buffer=storage.handle;ci.size=storage.size;ci.type=type;
        VkAccelerationStructureKHR structure{};check(VCALL(vkCreateAccelerationStructureKHR)(device,&ci,nullptr,&structure),"Create AS");structures.push_back(structure);
        VkPhysicalDeviceAccelerationStructurePropertiesKHR asprops{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&asprops;ICALL(vkGetPhysicalDeviceProperties2)(physical,&props);
        const auto alignment=asprops.minAccelerationStructureScratchOffsetAlignment;
        auto scratch=buffer(sizes.buildScratchSize+alignment,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,false);
        info.scratchData.deviceAddress=(address(scratch)+alignment-1)&~VkDeviceAddress(alignment-1);info.dstAccelerationStructure=structure;
        VkAccelerationStructureBuildRangeInfoKHR range{count,0,0,0};const auto* ranges=&range;
        begin();
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        VCALL(vkCmdPipelineBarrier)(command,VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,0,1,&barrier,0,nullptr,0,nullptr);
        VCALL(vkCmdBuildAccelerationStructuresKHR)(command,1,&info,&ranges);submit();return structure;
    }
    VkDescriptorSet prepare(VkAccelerationStructureKHR scene,std::array<Buffer,8> inputs,bool shadow=false) {
        std::array<VkDescriptorSetLayoutBinding,9> bindings{};
        const unsigned binding_count=shadow?5:9;
        for(unsigned i=0;i<binding_count;++i) bindings[i]={i,i==0?VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:i==2?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=binding_count;li.pBindings=bindings.data();
        check(VCALL(vkCreateDescriptorSetLayout)(device,&li,nullptr,&layout),"Create descriptors");
        VkPipelineLayoutCreateInfo pi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pi.setLayoutCount=1;pi.pSetLayouts=&layout;
        check(VCALL(vkCreatePipelineLayout)(device,&pi,nullptr,&pipeline_layout),"Create pipeline layout");
        const auto spirv=shadow?std::span<const std::uint32_t>(starfox::render::shadows::vulkan_shadow_rayquery_spirv)
            :std::span<const std::uint32_t>(starfox::render::shadows::vulkan_reflection_rayquery_spirv);
        VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};si.codeSize=spirv.size()*4;si.pCode=spirv.data();
        check(VCALL(vkCreateShaderModule)(device,&si,nullptr,&shader),"Create exact GLSL shader");
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.layout=pipeline_layout;
        ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,shader,"main",nullptr};
        check(VCALL(vkCreateComputePipelines)(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline),"Create native ray-query pipeline");
        std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,binding_count-2}}};
        VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=1;dpi.poolSizeCount=3;dpi.pPoolSizes=sizes.data();
        check(VCALL(vkCreateDescriptorPool)(device,&dpi,nullptr,&descriptor_pool),"Create descriptor pool");
        VkDescriptorSetAllocateInfo di{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};di.descriptorPool=descriptor_pool;di.descriptorSetCount=1;di.pSetLayouts=&layout;
        VkDescriptorSet set{};check(VCALL(vkAllocateDescriptorSets)(device,&di,&set),"Allocate descriptor set");
        VkWriteDescriptorSetAccelerationStructureKHR write_as{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};write_as.accelerationStructureCount=1;write_as.pAccelerationStructures=&scene;
        std::array<VkWriteDescriptorSet,9> writes{};std::array<VkDescriptorBufferInfo,8> bi{};
        for(unsigned i=0;i<binding_count;++i) {
            writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,i,0,1,bindings[i].descriptorType,nullptr,nullptr,nullptr};
            if(i==0) writes[i].pNext=&write_as;
            else {bi[i-1]={inputs[i-1].handle,0,inputs[i-1].size};writes[i].pBufferInfo=&bi[i-1];}
        }
        VCALL(vkUpdateDescriptorSets)(device,binding_count,writes.data(),0,nullptr);return set;
    }
    void dispatch(VkDescriptorSet set,unsigned pixels,bool water=false,bool surface=false,bool model=false) {
        if(water && !native_water_pipeline) {
            const VkBool32 enabled=VK_TRUE;
            const std::array<VkSpecializationMapEntry,2> entries{{{0,0,sizeof(enabled)},{2,0,sizeof(enabled)}}};
            const VkSpecializationInfo specialization{unsigned(entries.size()),entries.data(),sizeof(enabled),&enabled};
            VkComputePipelineCreateInfo create{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};create.layout=pipeline_layout;
            create.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,shader,"main",&specialization};
            check(VCALL(vkCreateComputePipelines)(device,VK_NULL_HANDLE,1,&create,nullptr,&native_water_pipeline),"Create specialized native water pipeline");
        }
        if(surface && !native_surface_pipeline) {
            const VkBool32 enabled=VK_TRUE;
            const std::array<VkSpecializationMapEntry,2> entries{{{1,0,sizeof(enabled)},{2,0,sizeof(enabled)}}};
            const VkSpecializationInfo specialization{unsigned(entries.size()),entries.data(),sizeof(enabled),&enabled};
            VkComputePipelineCreateInfo create{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};create.layout=pipeline_layout;
            create.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,shader,"main",&specialization};
            check(VCALL(vkCreateComputePipelines)(device,VK_NULL_HANDLE,1,&create,nullptr,&native_surface_pipeline),"Create specialized native planar/lava pipeline");
        }
        if(model && !water && !surface && !native_model_pipeline) {
            const VkBool32 enabled=VK_TRUE;
            const VkSpecializationMapEntry entry{2,0,sizeof(enabled)};
            const VkSpecializationInfo specialization{1,&entry,sizeof(enabled),&enabled};
            VkComputePipelineCreateInfo create{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};create.layout=pipeline_layout;
            create.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,shader,"main",&specialization};
            check(VCALL(vkCreateComputePipelines)(device,VK_NULL_HANDLE,1,&create,nullptr,&native_model_pipeline),"Create specialized native model pipeline");
        }
        begin();
        VkMemoryBarrier input{VK_STRUCTURE_TYPE_MEMORY_BARRIER};input.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        input.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        VCALL(vkCmdPipelineBarrier)(command,VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&input,0,nullptr,0,nullptr);
        VCALL(vkCmdBindPipeline)(command,VK_PIPELINE_BIND_POINT_COMPUTE,water?native_water_pipeline:surface?native_surface_pipeline:model?native_model_pipeline:pipeline);
        VCALL(vkCmdBindDescriptorSets)(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&set,0,nullptr);
        VCALL(vkCmdDispatch)(command,(pixels+63)/64,1,1);
        VkMemoryBarrier output{VK_STRUCTURE_TYPE_MEMORY_BARRIER};output.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;output.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        VCALL(vkCmdPipelineBarrier)(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&output,0,nullptr,0,nullptr);submit();
    }
};
using native_reflection_fixture::Parameters;
using native_reflection_fixture::Material;
}

int main(int argc,char** argv) try {
    bool integrated=false,shadow=false,water_only=false,ranges_only=false,environment_only=false,ground_only=false,model_only=false,empty_only=false;
    for(int i=1;i<argc;++i) {
        const std::string flag=argv[i];
        if(flag=="--integrated" && !integrated)integrated=true;
        else if(flag=="--shadow-cutouts" && !shadow)shadow=true;
        else if(flag=="--native-water-only" && !water_only)water_only=true;
        else if(flag=="--primary-ranges-only" && !ranges_only)ranges_only=true;
        else if(flag=="--native-environment-only" && !environment_only)environment_only=true;
        else if(flag=="--native-ground-only" && !ground_only)ground_only=true;
        else if(flag=="--native-model-only" && !model_only)model_only=true;
        else if(flag=="--native-empty-only" && !empty_only)empty_only=true;
        else throw std::runtime_error("Usage: starfox_native_rayquery_check [--integrated] [--shadow-cutouts] [--native-water-only | --primary-ranges-only | --native-environment-only | --native-ground-only | --native-model-only | --native-empty-only]");
    }
    require(!shadow || !water_only,"Choose shadow or native water fixture");
    require(!water_only || !ranges_only,"Choose water or primary range fixture");
    require(!environment_only || (!shadow && !water_only && !ranges_only),"Choose native environment fixture alone");
    require(!ground_only || (!shadow && !water_only && !ranges_only && !environment_only),"Choose native ground fixture alone");
    require(!model_only || (!shadow && !water_only && !ranges_only && !environment_only && !ground_only),"Choose native model fixture alone");
    require(!empty_only || (!water_only && !ranges_only && !environment_only && !ground_only && !model_only),"Choose native empty fixture alone");
    Vulkan vk;vk.initialize(integrated);
    auto fixture=native_reflection_fixture::make_fixture();
    auto& vertices=fixture.vertices;
    constexpr auto width=native_reflection_fixture::width,height=native_reflection_fixture::height;
    auto geometry=vk.buffer(sizeof(vertices),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,true);
    vk.copy(geometry,vertices.data(),true);
    VkAccelerationStructureGeometryKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};triangles.geometryType=VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    triangles.geometry.triangles={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,nullptr,VK_FORMAT_R32G32B32_SFLOAT,{vk.address(geometry)},16,11,VK_INDEX_TYPE_NONE_KHR,{0},{0}};
    auto blas=vk.build(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,triangles,4);
    VkAccelerationStructureDeviceAddressInfoKHR ai{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};ai.accelerationStructure=blas;
    VkAccelerationStructureInstanceKHR instance{};instance.transform.matrix[0][0]=instance.transform.matrix[1][1]=instance.transform.matrix[2][2]=1;
    instance.mask=255;instance.flags=VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;instance.accelerationStructureReference=vk.VCALL(vkGetAccelerationStructureDeviceAddressKHR)(vk.device,&ai);
    auto instances=vk.buffer(sizeof(instance),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,true);vk.copy(instances,&instance,true);
    VkAccelerationStructureGeometryKHR top{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};top.geometryType=VK_GEOMETRY_TYPE_INSTANCES_KHR;
    top.geometry.instances={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,nullptr,VK_FALSE,{vk.address(instances)}};
    const auto scene=vk.build(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,top,1);
    auto output=vk.buffer(width*height*(shadow?4:24),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    auto parameters=vk.buffer(shadow?sizeof(native_reflection_fixture::ShadowParameters):sizeof(Parameters),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
    auto& materials=fixture.materials;
    auto records=vk.buffer(native_reflection_fixture::native_shadow_word_capacity*4+((environment_only || ground_only || model_only || empty_only)?512U*512*6*4:0U),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    const auto upload_records=[&](const void* data,std::size_t bytes) {
        std::vector<unsigned> storage(records.size/4);
        require(bytes<=records.size,"Ray material fixture storage bound");
        std::memcpy(storage.data(),data,bytes);vk.copy(records,storage.data(),true);
    };
    upload_records(materials.data(),sizeof(materials));
    auto& palette=fixture.palette;
    auto colours=vk.buffer(sizeof(palette),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);vk.copy(colours,palette.data(),true);
    auto empty=vk.buffer(4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);unsigned zero=0;vk.copy(empty,&zero,true);
    auto texels=vk.buffer(16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::array<unsigned char,16> blank{};vk.copy(texels,blank.data(),true);
    const auto set=vk.prepare(scene,shadow?std::array<Buffer,8>{output,parameters,records,texels,empty,empty,empty,empty}
        :std::array<Buffer,8>{output,parameters,geometry,records,colours,texels,empty,empty},shadow);
    const auto replace_geometry=[&](const auto& source,bool empty_scene=false) {
        if(!empty_scene) {
            vk.upload(geometry,source.vertices.data());
            const auto replacement_blas=vk.build(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,triangles,4);
            ai.accelerationStructure=replacement_blas;
            instance.accelerationStructureReference=vk.VCALL(vkGetAccelerationStructureDeviceAddressKHR)(vk.device,&ai);
            vk.copy(instances,&instance,true);
        }
        const auto replacement_scene=vk.build(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,top,empty_scene?0:1);
        VkWriteDescriptorSetAccelerationStructureKHR as{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        as.accelerationStructureCount=1;as.pAccelerationStructures=&replacement_scene;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.pNext=&as;write.dstSet=set;
        write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        vk.VCALL(vkUpdateDescriptorSets)(vk.device,1,&write,0,nullptr);
    };
    if(shadow) {
        if(empty_only) {
            replace_geometry(fixture,true);
            native_reflection_fixture::run_native_empty_shadows([&](auto p,const auto& input) {
                upload_records(input.source.source.words.data(),input.source.source.words.size()*4);vk.copy(parameters,&p,true);
                vk.dispatch(set,width*height);std::vector<unsigned> result(width*height);vk.copy(output,result.data(),false);return result;
            });return 0;
        }
        const auto shadow_dispatch=[&](auto p,const auto& cutout,const auto& ink) {
            if(ranges_only)replace_geometry(cutout);
            upload_records(cutout.materials.data(),sizeof(cutout.materials));vk.upload(texels,ink.data());vk.copy(parameters,&p,true);
            vk.dispatch(set,width*height);std::vector<unsigned> result(width*height);vk.copy(output,result.data(),false);return result;
        };
        if(ranges_only){native_reflection_fixture::run_primary_ranges<true>(shadow_dispatch);return 0;}
        native_reflection_fixture::run_shadow_cutouts(fixture,shadow_dispatch);
        native_reflection_fixture::run_native_rgba_shadows([&](auto p,const auto& source) {
            upload_records(source.words.data(),source.words.size()*4);vk.copy(parameters,&p,true);
            vk.dispatch(set,width*height);std::vector<unsigned> result(width*height);vk.copy(output,result.data(),false);return result;
        });
        return 0;
    }
    const auto dispatch=[&](Parameters p) {
        vk.copy(parameters,&p,true);vk.dispatch(set,width*height,p.source_colour[3]!=0,
            p.material_info[0]==2 && p.material_info[2]!=0 && (unsigned(p.water[3])&15)!=0,
            p.material_info[0]==2 && p.material_info[2]!=0);
        std::vector<unsigned> result(width*height*6);vk.copy(output,result.data(),false);
        result.resize(p.liquid_layers[3]?p.liquid_layers[3]:width*height);return result;
    };
    const auto water_dispatch=[&](Parameters p,const auto& source) {
        upload_records(source.source.words.data(),source.source.words.size()*4);return dispatch(p);
    };
    if(environment_only || ground_only || model_only || empty_only) {
        auto previous_vertices=fixture.vertices;
        bool previous_empty=false;
        const auto cube_dispatch=[&](Parameters p,const auto& input) {
            if((model_only && previous_vertices!=input.source.source.geometry.vertices)
                || (empty_only && previous_empty!=input.source.empty_geometry)) {
                replace_geometry(input.source.source.geometry,input.source.empty_geometry);
                previous_vertices=input.source.source.geometry.vertices;previous_empty=input.source.empty_geometry;
            }
            auto words=input.source.source.words;words.insert(words.end(),input.cube.begin(),input.cube.end());
            upload_records(words.data(),words.size()*4);return dispatch(p);
        };
        if(ground_only){native_reflection_fixture::run_native_ground(cube_dispatch);return 0;}
        if(model_only){native_reflection_fixture::run_native_models(cube_dispatch);return 0;}
        if(empty_only){native_reflection_fixture::run_native_empty(cube_dispatch);native_reflection_fixture::run_native_empty_transitions(cube_dispatch);return 0;}
        native_reflection_fixture::run_native_environment(cube_dispatch);
        native_reflection_fixture::run_native_water_environment(cube_dispatch);return 0;
    }
    if(ranges_only) {
        native_reflection_fixture::run_primary_ranges<false>([&](Parameters p,const auto& input,const auto& ink) {
            replace_geometry(input);upload_records(input.materials.data(),sizeof(input.materials));
            vk.upload(colours,input.palette.data());vk.upload(texels,ink.data());return dispatch(p);
        });
        replace_geometry(fixture);
        native_reflection_fixture::run_native_water_ranges(water_dispatch);return 0;
    }
    if(water_only){native_reflection_fixture::run_native_water(water_dispatch);return 0;}
    native_reflection_fixture::run(fixture,dispatch);
    native_reflection_fixture::run_primary(fixture,dispatch);
    native_reflection_fixture::run_indexed_cutouts(fixture,[&](Parameters p,auto& cutout,auto& ink) {
        upload_records(cutout.materials.data(),sizeof(cutout.materials));vk.copy(colours,cutout.palette.data(),true);vk.copy(texels,ink.data(),true);
        return dispatch(p);
    });
    native_reflection_fixture::run_native_rgba_reflections([&](Parameters p,const auto& source) {
        upload_records(source.source.words.data(),source.source.words.size()*4);return dispatch(p);
    });
    native_reflection_fixture::run_native_water(water_dispatch);
    native_reflection_fixture::run_near_cutouts([&](Parameters p,auto& cutout,auto& ink) {
        vk.copy(geometry,cutout.vertices.data(),true);
        auto close_blas=vk.build(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,triangles,4);
        ai.accelerationStructure=close_blas;
        instance.accelerationStructureReference=vk.VCALL(vkGetAccelerationStructureDeviceAddressKHR)(vk.device,&ai);
        vk.copy(instances,&instance,true);
        const auto close_scene=vk.build(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,top,1);
        VkWriteDescriptorSetAccelerationStructureKHR as{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        as.accelerationStructureCount=1;as.pAccelerationStructures=&close_scene;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.pNext=&as;write.dstSet=set;
        write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        vk.VCALL(vkUpdateDescriptorSets)(vk.device,1,&write,0,nullptr);
        upload_records(cutout.materials.data(),sizeof(cutout.materials));vk.copy(colours,cutout.palette.data(),true);vk.copy(texels,ink.data(),true);
        return dispatch(p);
    });
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
