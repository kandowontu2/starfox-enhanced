#include "starfox/render/vulkan_hardware_rt.hpp"
#include "starfox/render/environment_effects.hpp"

#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#include "shaders/generated/vulkan_shadow_rayquery.hpp"
#include "shaders/generated/vulkan_reflection_rayquery.hpp"
#include "shaders/generated/vulkan_reflection_history_rayquery.hpp"
#include "shaders/generated/vulkan_reflection_ground_history_rayquery.hpp"
#include "shaders/generated/vulkan_reflection_liquid_history_rayquery.hpp"
#include "shaders/generated/vulkan_reflection_liquid_motion_rayquery.hpp"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace starfox::render::shadows {
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
namespace {
struct alignas(16) Float4 {float x{},y{},z{},w{};};
struct Parameters {
    Float4 extent_focal{},center_ground{},ground_point{},ground_normal{};
    std::array<Float4,16> lights{};
    std::array<std::uint32_t,4> coverage{};
    Float4 primary_range{};
};
static_assert(sizeof(Parameters)==352);
struct alignas(16) ReflectionParameters {
    std::array<std::uint32_t,4> dimensions{};
    Float4 camera{},settings{},ground_point{},ground_normal{};
    std::array<std::uint32_t,4> environment{};
    Float4 water_settings{},water_row0{},water_row1{},water_row2{};
    std::array<std::uint32_t,4> enhanced_size{},enhanced_modes{};
    Float4 enhanced_motion{},enhanced_plane{},enhanced_projection{},enhanced_palette{};
    Float4 enhanced_keep0{},enhanced_keep1{};
    std::array<std::uint32_t,4> material_info{};
    Float4 source_colour{};
    std::array<std::uint32_t,4> liquid_layers{};
    Float4 primary_range{};
    std::array<std::uint32_t,4> cube_info{};
    Float4 cube_row0{},cube_row1{},cube_row2{};
};
static_assert(sizeof(ReflectionParameters)==416);
struct alignas(16) ReflectionHistoryParameters {
    ReflectionParameters current{};
    std::array<std::uint32_t,4> history_info{}; // Relative previous words/index words, accepted extent.
    Float4 history_projection{},history_clip{};
};
static_assert(sizeof(ReflectionHistoryParameters)==464);
struct alignas(16) ReflectionGroundHistoryParameters {
    ReflectionHistoryParameters history{};
    Float4 previous_ground_point{},previous_ground_normal{};
};
static_assert(sizeof(ReflectionGroundHistoryParameters)==496);
struct alignas(16) ReflectionLiquidHistoryParameters {
    ReflectionHistoryParameters history{};
    std::array<float,32> previous_liquid{};
};
static_assert(sizeof(ReflectionLiquidHistoryParameters)==592);
bool valid_primary_range(const std::optional<PrimaryRayRange>& range) {
    // Distinct double planes can collapse on the GPU. Never submit an invalid
    // tMin/tMax pair after float conversion, even when the source range is valid.
    return !range || (range->valid() && float(range->near_depth)<float(range->far_depth));
}
Float4 primary_parameters(const std::optional<PrimaryRayRange>& range) {
    const auto depth=range.value_or(PrimaryRayRange{});
    return {float(depth.near_depth),float(depth.far_depth),range?1.f:0.f,0};
}
Float4 vector4(Vec3 v) {return {float(v.x),float(v.y),float(v.z),0};}
Float4 vector4(const std::array<float,4>& v) {return {v[0],v[1],v[2],v[3]};}
bool empty_native_geometry(const GpuScene::RayGeometryOutput& geometry) {
    // Same padded empty-world contract as the calibrated GPU producer. Zero
    // triangle counts are not permission to accept an incomplete caster batch.
    return geometry.vertex_count==0 && geometry.materials
        && geometry.materials->encoding==RayMaterialEncoding::native_rgba
        && geometry.material_offset>=16 && geometry.material_offset%16==0
        && geometry.material_bytes>=16 && geometry.material_bytes%4==0;
}
std::array<Float4,16> light_samples(Vec3 light,unsigned samples,double angular_radius) {
    light=light*(1.0/std::sqrt(dot(light,light)));
    const auto reference=std::abs(light.y)<.9?Vec3{0,1,0}:Vec3{1,0,0};
    auto tangent=cross(light,reference);
    tangent=tangent*(1.0/std::sqrt(dot(tangent,tangent)));
    const auto bitangent=cross(light,tangent);
    std::array<Float4,16> result{};
    for(unsigned i=0;i<samples;++i) {
        const auto radius=angular_radius*std::sqrt((i+.5)/samples);
        const auto angle=i*2.399963229728653;
        auto direction=light+tangent*(radius*std::cos(angle))
            +bitangent*(radius*std::sin(angle));
        direction=direction*(1.0/std::sqrt(dot(direction,direction)));
        result[i]=vector4(direction);
    }
    result[0].w=float(samples);return result;
}
struct Buffer {VkBuffer handle{};VkDeviceMemory memory{};VkDeviceSize size{};};
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": Vulkan error "+std::to_string(result));
}
}
#endif

struct VulkanHardwareRt::Impl {
    std::string status{"Vulkan hardware rays unavailable"};
    GpuShadowOutput output{};
    GpuReflectionOutput reflection{};
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    SDL_GPUDevice* sdl{};
    const StarfoxSdlVulkanBridgeV2* bridge{};
    const StarfoxSdlVulkanRayBridgeV3* ray_bridge{};
    VkDevice device{};
    VkPhysicalDeviceMemoryProperties memory{};
    VkDeviceSize scratch_alignment{1};
    SDL_GPUBuffer* sdl_output{};
    std::uint32_t output_capacity{};
    SDL_GPUBuffer* sdl_reflection{};
    std::uint32_t reflection_capacity{};
    GpuBackground reflection_backdrop{};
    VkDescriptorSetLayout set_layout{};
    VkPipelineLayout pipeline_layout{};
    VkPipeline pipeline{};
    VkDescriptorPool descriptor_pool{};
    VkShaderModule shader{};
    VkDescriptorSetLayout reflection_set_layout{};
    VkPipelineLayout reflection_pipeline_layout{};
    VkPipeline reflection_pipeline{};
    VkPipeline native_water_pipeline{};
    VkPipeline native_surface_pipeline{};
    VkPipeline native_model_pipeline{};
    VkPipeline native_history_pipeline{};
    VkPipeline native_lobe_history_pipeline{},native_path_history_pipeline{},native_scene_history_pipeline{},native_ground_history_pipeline{};
    VkPipeline native_water_history_pipeline{},native_lava_history_pipeline{},native_liquid_motion_pipeline{};
    VkDescriptorPool reflection_descriptor_pool{};
    VkShaderModule reflection_shader{};
    struct Slot {
        SDL_GPUFence* fence{};
        bool unfenced{};
        Buffer vertices{},instances{},scratch{},blas_buffer{},tlas_buffer{},parameters{};
        Buffer reflection_parameters{},materials{},palette{},texels{},backdrop{},enhanced_backdrop{};
        BackdropUploadCache enhanced_upload{};
        VkAccelerationStructureKHR blas{},tlas{};
        VkDescriptorSet descriptors{};
        VkDescriptorSet reflection_descriptors{};
    };
    std::array<Slot,3> slots{};
    unsigned serial{};
    bool initialized{},reflection_initialized{};

#define VK_FN(name) PFN_##name name{}
    VK_FN(vkCreateBuffer);VK_FN(vkDestroyBuffer);VK_FN(vkGetBufferMemoryRequirements);
    VK_FN(vkAllocateMemory);VK_FN(vkFreeMemory);VK_FN(vkBindBufferMemory);
    VK_FN(vkMapMemory);VK_FN(vkUnmapMemory);VK_FN(vkGetBufferDeviceAddress);
    VK_FN(vkCreateAccelerationStructureKHR);VK_FN(vkDestroyAccelerationStructureKHR);
    VK_FN(vkGetAccelerationStructureBuildSizesKHR);VK_FN(vkGetAccelerationStructureDeviceAddressKHR);
    VK_FN(vkCmdBuildAccelerationStructuresKHR);VK_FN(vkCmdPipelineBarrier);
    VK_FN(vkCreateDescriptorSetLayout);VK_FN(vkDestroyDescriptorSetLayout);
    VK_FN(vkCreatePipelineLayout);VK_FN(vkDestroyPipelineLayout);
    VK_FN(vkCreateComputePipelines);VK_FN(vkDestroyPipeline);
    VK_FN(vkCreateDescriptorPool);VK_FN(vkDestroyDescriptorPool);
    VK_FN(vkAllocateDescriptorSets);VK_FN(vkUpdateDescriptorSets);
    VK_FN(vkCreateShaderModule);VK_FN(vkDestroyShaderModule);
    VK_FN(vkCmdBindPipeline);VK_FN(vkCmdBindDescriptorSets);VK_FN(vkCmdDispatch);
#undef VK_FN

    void load_functions() {
#define LOAD(name) name=reinterpret_cast<PFN_##name>(bridge->get_device_proc(device,#name)); \
        if(!name) throw std::runtime_error("Missing Vulkan entry " #name)
        LOAD(vkCreateBuffer);LOAD(vkDestroyBuffer);LOAD(vkGetBufferMemoryRequirements);
        LOAD(vkAllocateMemory);LOAD(vkFreeMemory);LOAD(vkBindBufferMemory);
        LOAD(vkMapMemory);LOAD(vkUnmapMemory);LOAD(vkGetBufferDeviceAddress);
        LOAD(vkCreateAccelerationStructureKHR);LOAD(vkDestroyAccelerationStructureKHR);
        LOAD(vkGetAccelerationStructureBuildSizesKHR);LOAD(vkGetAccelerationStructureDeviceAddressKHR);
        LOAD(vkCmdBuildAccelerationStructuresKHR);LOAD(vkCmdPipelineBarrier);
        LOAD(vkCreateDescriptorSetLayout);LOAD(vkDestroyDescriptorSetLayout);
        LOAD(vkCreatePipelineLayout);LOAD(vkDestroyPipelineLayout);
        LOAD(vkCreateComputePipelines);LOAD(vkDestroyPipeline);
        LOAD(vkCreateDescriptorPool);LOAD(vkDestroyDescriptorPool);
        LOAD(vkAllocateDescriptorSets);LOAD(vkUpdateDescriptorSets);
        LOAD(vkCreateShaderModule);LOAD(vkDestroyShaderModule);
        LOAD(vkCmdBindPipeline);LOAD(vkCmdBindDescriptorSets);LOAD(vkCmdDispatch);
#undef LOAD
        auto get_memory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
            bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceMemoryProperties"));
        if(!get_memory) throw std::runtime_error("Missing Vulkan memory properties");
        get_memory(bridge->physical_device,&memory);
        const auto get_properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceProperties2"));
        if(!get_properties)throw std::runtime_error("Missing Vulkan acceleration-structure properties");
        VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_properties{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};properties.pNext=&acceleration_properties;
        get_properties(bridge->physical_device,&properties);
        scratch_alignment=acceleration_properties.minAccelerationStructureScratchOffsetAlignment;
        if(!scratch_alignment || (scratch_alignment&(scratch_alignment-1)))
            throw std::runtime_error("Invalid Vulkan acceleration-structure scratch alignment");
    }
    Buffer create_buffer(VkDeviceSize size,VkBufferUsageFlags usage,bool host,bool address) {
        Buffer out{};out.size=size;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=size;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device,&info,nullptr,&out.handle),"create ray buffer");
        try {
        VkMemoryRequirements requirement{};
        vkGetBufferMemoryRequirements(device,out.handle,&requirement);
        const auto wanted=host
            ?VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            :VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        unsigned index=memory.memoryTypeCount;
        for(unsigned i=0;i<memory.memoryTypeCount;++i)
            if((requirement.memoryTypeBits&(1u<<i))
                && (memory.memoryTypes[i].propertyFlags&wanted)==wanted) {index=i;break;}
        if(index==memory.memoryTypeCount)
            throw std::runtime_error("No suitable Vulkan ray buffer memory");
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
        flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize=requirement.size;allocate.memoryTypeIndex=index;
        if(address) allocate.pNext=&flags;
        check(vkAllocateMemory(device,&allocate,nullptr,&out.memory),"allocate ray buffer");
        check(vkBindBufferMemory(device,out.handle,out.memory,0),"bind ray buffer");
        return out;
        } catch(...) {
            vkDestroyBuffer(device,out.handle,nullptr);
            if(out.memory)vkFreeMemory(device,out.memory,nullptr);
            throw;
        }
    }
    void upload(Buffer& buffer,const void* data,std::size_t bytes) {
        void* destination{};
        check(vkMapMemory(device,buffer.memory,0,bytes,0,&destination),"map ray input");
        std::memcpy(destination,data,bytes);
        vkUnmapMemory(device,buffer.memory);
    }
    VkDeviceAddress address(Buffer buffer) const {
        VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        info.buffer=buffer.handle;return vkGetBufferDeviceAddress(device,&info);
    }
    void destroy(Buffer& buffer) {
        if(buffer.handle) vkDestroyBuffer(device,buffer.handle,nullptr);
        if(buffer.memory) vkFreeMemory(device,buffer.memory,nullptr);
        buffer={};
    }
    bool recover_fence(Slot& slot) noexcept {
        if(!slot.unfenced)return true;
        auto* marker=SDL_AcquireGPUCommandBuffer(sdl);
        if(!marker)return false;
        // Even a failed submit has consumed the command. Do not cancel/replay
        // it: an empty later marker certifies all earlier same-queue work.
        slot.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(marker);
        if(slot.fence)slot.unfenced=false;
        return !slot.unfenced;
    }
    bool complete() noexcept {
        if(!sdl)return true;
        for(auto& slot:slots)if(!recover_fence(slot)
            || (slot.fence && !SDL_QueryGPUFence(sdl,slot.fence)))return false;
        return true;
    }
    void await(Slot& slot) {
        if(!recover_fence(slot)) {
            if(!SDL_WaitForGPUIdle(sdl))throw std::runtime_error(SDL_GetError());
            slot.unfenced=false;
        }
        if(slot.fence && !SDL_WaitForGPUFences(sdl,true,&slot.fence,1))
            throw std::runtime_error(SDL_GetError());
    }
    void clear_completed(Slot& slot,bool release_cached=false) {
        if(slot.fence){SDL_ReleaseGPUFence(sdl,slot.fence);slot.fence=nullptr;}
        if(slot.blas) vkDestroyAccelerationStructureKHR(device,slot.blas,nullptr);
        if(slot.tlas) vkDestroyAccelerationStructureKHR(device,slot.tlas,nullptr);
        slot.blas=slot.tlas=VK_NULL_HANDLE;
        for(auto* buffer:{&slot.vertices,&slot.instances,&slot.scratch,&slot.blas_buffer,
                          &slot.tlas_buffer,&slot.parameters,&slot.reflection_parameters,
                          &slot.materials,&slot.palette,&slot.texels,&slot.backdrop}) destroy(*buffer);
        if(release_cached) {destroy(slot.enhanced_backdrop);slot.enhanced_upload={};}
    }
    void clear(Slot& slot) {await(slot);clear_completed(slot);}
    void submit(Slot& slot,SDL_GPUCommandBuffer* command) {
        slot.unfenced=true;
        slot.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
        if(slot.fence){slot.unfenced=false;return;}
        const std::string error=SDL_GetError();
        (void)recover_fence(slot);
        throw std::runtime_error(error.empty()?"Vulkan ray submission returned no completion fence":error);
    }
    void release_reflection_pipeline() {
        if(reflection_descriptor_pool)vkDestroyDescriptorPool(device,reflection_descriptor_pool,nullptr);
        for(auto* handle:{&reflection_pipeline,&native_water_pipeline,&native_surface_pipeline,&native_model_pipeline,&native_history_pipeline,
            &native_lobe_history_pipeline,&native_path_history_pipeline,&native_scene_history_pipeline,&native_ground_history_pipeline,
            &native_water_history_pipeline,&native_lava_history_pipeline,&native_liquid_motion_pipeline})
            if(*handle){vkDestroyPipeline(device,*handle,nullptr);*handle=VK_NULL_HANDLE;}
        if(reflection_pipeline_layout)vkDestroyPipelineLayout(device,reflection_pipeline_layout,nullptr);
        if(reflection_set_layout)vkDestroyDescriptorSetLayout(device,reflection_set_layout,nullptr);
        if(reflection_shader)vkDestroyShaderModule(device,reflection_shader,nullptr);
        reflection_descriptor_pool=VK_NULL_HANDLE;reflection_pipeline_layout=VK_NULL_HANDLE;
        reflection_set_layout=VK_NULL_HANDLE;reflection_shader=VK_NULL_HANDLE;reflection_initialized=false;
        for(auto& slot:slots)slot.reflection_descriptors=VK_NULL_HANDLE;
    }
    bool release(bool wait) noexcept {
        output={};reflection={};
        if(!sdl)return true;
        try {
            // Certify EVERY slot before freeing ANY resource. A later failed
            // wait must not partially tear down a live owner.
            if(wait){for(auto& slot:slots)await(slot);}
            else if(!complete()){status="Vulkan ray cleanup pending";return false;}
        } catch(const std::exception& error){status=std::string("Vulkan ray cleanup pending: ")+error.what();return false;}
        for(auto& slot:slots)clear_completed(slot,true);
        if(sdl_output) SDL_ReleaseGPUBuffer(sdl,sdl_output);
        if(sdl_reflection) SDL_ReleaseGPUBuffer(sdl,sdl_reflection);
        reflection_backdrop.release_device();
        release_reflection_pipeline();
        if(descriptor_pool) vkDestroyDescriptorPool(device,descriptor_pool,nullptr);
        if(pipeline) vkDestroyPipeline(device,pipeline,nullptr);
        if(pipeline_layout) vkDestroyPipelineLayout(device,pipeline_layout,nullptr);
        if(set_layout) vkDestroyDescriptorSetLayout(device,set_layout,nullptr);
        if(shader) vkDestroyShaderModule(device,shader,nullptr);
        // release_device may be followed by a new request on the same SDL
        // device. Destroyed handles must not satisfy the lazy-ready checks.
        descriptor_pool=reflection_descriptor_pool=VK_NULL_HANDLE;
        pipeline=reflection_pipeline=native_water_pipeline=native_surface_pipeline=native_model_pipeline=native_history_pipeline=VK_NULL_HANDLE;
        native_lobe_history_pipeline=native_path_history_pipeline=native_scene_history_pipeline=native_ground_history_pipeline=VK_NULL_HANDLE;
        native_water_history_pipeline=native_lava_history_pipeline=native_liquid_motion_pipeline=VK_NULL_HANDLE;
        pipeline_layout=reflection_pipeline_layout=VK_NULL_HANDLE;
        set_layout=reflection_set_layout=VK_NULL_HANDLE;
        shader=reflection_shader=VK_NULL_HANDLE;
        for(auto& slot:slots)slot.descriptors=slot.reflection_descriptors=VK_NULL_HANDLE;
        serial=0;device=VK_NULL_HANDLE;bridge=nullptr;ray_bridge=nullptr;
        sdl_output=sdl_reflection=nullptr;output_capacity=reflection_capacity=0;
        output={};reflection={};sdl=nullptr;
        initialized=false;return true;
    }
    void initialize(SDL_GPUDevice* source) {
        if(sdl==source && initialized)return;
        if(!release(true))throw std::runtime_error(status);
        const auto properties=SDL_GetGPUDeviceProperties(source);
        bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(
            SDL_GetPointerProperty(properties,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
        ray_bridge=static_cast<const StarfoxSdlVulkanRayBridgeV3*>(
            SDL_GetPointerProperty(properties,STARFOX_SDL_VULKAN_RAY_BRIDGE,nullptr));
        if(!bridge || bridge->version!=2 || !bridge->device || !bridge->physical_device
            || !bridge->get_device_proc || !bridge->get_instance_proc || !ray_bridge || ray_bridge->version!=3
            || !ray_bridge->command || !ray_bridge->prepare_write || !ray_bridge->finish_write
            || !ray_bridge->copy_ray_range)
            throw std::runtime_error("Vulkan ray command bridge unavailable");
        sdl=source;device=bridge->device;
        try {
        load_functions();
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=vulkan_shadow_rayquery_spirv.size()*4;
        module.pCode=vulkan_shadow_rayquery_spirv.data();
        check(vkCreateShaderModule(device,&module,nullptr,&shader),"create ray shader");
        const std::array<VkDescriptorSetLayoutBinding,5> bindings{{
            {0,VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount=unsigned(bindings.size());layout.pBindings=bindings.data();
        check(vkCreateDescriptorSetLayout(device,&layout,nullptr,&set_layout),"create ray descriptors");
        VkPipelineLayoutCreateInfo pipeline_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_info.setLayoutCount=1;pipeline_info.pSetLayouts=&set_layout;
        check(vkCreatePipelineLayout(device,&pipeline_info,nullptr,&pipeline_layout),"create ray layout");
        VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=shader;
        compute.stage.pName="main";compute.layout=pipeline_layout;
        check(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&compute,nullptr,&pipeline),"create ray pipeline");
        const std::array<VkDescriptorPoolSize,3> pools{{
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,9},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets=3;pool.poolSizeCount=unsigned(pools.size());pool.pPoolSizes=pools.data();
        check(vkCreateDescriptorPool(device,&pool,nullptr,&descriptor_pool),"create ray descriptor pool");
        const std::array<VkDescriptorSetLayout,3> layouts{set_layout,set_layout,set_layout};
        std::array<VkDescriptorSet,3> sets{};
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate.descriptorPool=descriptor_pool;allocate.descriptorSetCount=3;
        allocate.pSetLayouts=layouts.data();
        check(vkAllocateDescriptorSets(device,&allocate,sets.data()),"allocate ray descriptors");
        for(unsigned i=0;i<3;++i) slots[i].descriptors=sets[i];
        initialized=true;
        } catch(...) {(void)release(false);throw;}
    }
    void ensure_output(std::uint32_t pixels) {
        if(sdl_output && output_capacity>=pixels) return;
        SDL_GPUBufferCreateInfo info{};
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        info.size=pixels*4U;
        auto* next=SDL_CreateGPUBuffer(sdl,&info);
        if(!next) throw std::runtime_error(SDL_GetError());
        if(sdl_output) SDL_ReleaseGPUBuffer(sdl,sdl_output);
        sdl_output=next;output_capacity=pixels;
    }
    void ensure_reflection_output(std::uint32_t bytes) {
        if(sdl_reflection && reflection_capacity>=bytes) return;
        SDL_GPUBufferCreateInfo info{};
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        info.size=bytes;
        auto* next=SDL_CreateGPUBuffer(sdl,&info);
        if(!next) throw std::runtime_error(SDL_GetError());
        if(sdl_reflection) SDL_ReleaseGPUBuffer(sdl,sdl_reflection);
        sdl_reflection=next;reflection_capacity=bytes;
    }
    void ensure_reflection_pipeline() {
        if(reflection_initialized)return;
        try {
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=vulkan_reflection_rayquery_spirv.size()*4;
        module.pCode=vulkan_reflection_rayquery_spirv.data();
        check(vkCreateShaderModule(device,&module,nullptr,&reflection_shader),"create reflection shader");
        const std::array<VkDescriptorSetLayoutBinding,9> bindings{{
            {0,VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {5,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {6,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {7,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {8,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount=unsigned(bindings.size());layout.pBindings=bindings.data();
        check(vkCreateDescriptorSetLayout(device,&layout,nullptr,&reflection_set_layout),
            "create reflection descriptors");
        VkPipelineLayoutCreateInfo pipeline_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_info.setLayoutCount=1;pipeline_info.pSetLayouts=&reflection_set_layout;
        check(vkCreatePipelineLayout(device,&pipeline_info,nullptr,&reflection_pipeline_layout),
            "create reflection layout");
        VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=reflection_shader;
        compute.stage.pName="main";compute.layout=reflection_pipeline_layout;
        check(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&compute,nullptr,&reflection_pipeline),
            "create reflection pipeline");
        const std::array<VkDescriptorPoolSize,3> pools{{
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,21},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets=3;pool.poolSizeCount=unsigned(pools.size());pool.pPoolSizes=pools.data();
        check(vkCreateDescriptorPool(device,&pool,nullptr,&reflection_descriptor_pool),
            "create reflection descriptor pool");
        const std::array<VkDescriptorSetLayout,3> layouts{
            reflection_set_layout,reflection_set_layout,reflection_set_layout};
        std::array<VkDescriptorSet,3> sets{};
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate.descriptorPool=reflection_descriptor_pool;allocate.descriptorSetCount=3;
        allocate.pSetLayouts=layouts.data();
        check(vkAllocateDescriptorSets(device,&allocate,sets.data()),
            "allocate reflection descriptors");
        for(unsigned i=0;i<3;++i) slots[i].reflection_descriptors=sets[i];
        reflection_initialized=true;
        } catch(...) {release_reflection_pipeline();throw;}
    }
    void ensure_native_water_pipeline() {
        if(native_water_pipeline)return;
        const VkBool32 enabled=VK_TRUE;
        const std::array<VkSpecializationMapEntry,2> entries{{{0,0,sizeof(enabled)},{2,0,sizeof(enabled)}}};
        const VkSpecializationInfo specialization{unsigned(entries.size()),entries.data(),sizeof(enabled),&enabled};
        VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=reflection_shader;
        compute.stage.pName="main";compute.stage.pSpecializationInfo=&specialization;
        compute.layout=reflection_pipeline_layout;
        create_variant_pipeline(compute,native_water_pipeline,"create calibrated water pipeline");
    }
    void ensure_native_surface_pipeline() {
        if(native_surface_pipeline)return;
        const VkBool32 enabled=VK_TRUE;
        const std::array<VkSpecializationMapEntry,2> entries{{{1,0,sizeof(enabled)},{2,0,sizeof(enabled)}}};
        const VkSpecializationInfo specialization{unsigned(entries.size()),entries.data(),sizeof(enabled),&enabled};
        VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=reflection_shader;
        compute.stage.pName="main";compute.stage.pSpecializationInfo=&specialization;
        compute.layout=reflection_pipeline_layout;
        create_variant_pipeline(compute,native_surface_pipeline,"create native planar/lava reflection pipeline");
    }
    void ensure_native_model_pipeline() {
        if(native_model_pipeline)return;
        const VkBool32 enabled=VK_TRUE;
        const VkSpecializationMapEntry entry{2,0,sizeof(enabled)};
        const VkSpecializationInfo specialization{1,&entry,sizeof(enabled),&enabled};
        VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=reflection_shader;
        compute.stage.pName="main";compute.stage.pSpecializationInfo=&specialization;
        compute.layout=reflection_pipeline_layout;
        create_variant_pipeline(compute,native_model_pipeline,"create native calibrated model reflection pipeline");
    }
    void create_variant_pipeline(const VkComputePipelineCreateInfo& compute,VkPipeline& destination,const char* label) {
        // Creation may return an error with a non-null partial pipeline. Do
        // not let that handle satisfy the next call's cached-ready check.
        VkPipeline next=VK_NULL_HANDLE;
        const auto result=vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&compute,nullptr,&next);
        if(result!=VK_SUCCESS) {
            if(next)vkDestroyPipeline(device,next,nullptr);
            check(result,label);
        }
        destination=next;
    }
    void ensure_native_history_pipeline(unsigned model_records=0) {
        auto& destination=model_records==6?native_lava_history_pipeline:model_records==5?native_water_history_pipeline:
            model_records==4?native_ground_history_pipeline:model_records==3?native_scene_history_pipeline:model_records==2?native_path_history_pipeline
            :model_records==1?native_lobe_history_pipeline:native_history_pipeline;
        if(destination)return;
        VkShaderModule next=VK_NULL_HANDLE;
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        const std::span<const std::uint32_t> shader=model_records>=5
            ?std::span<const std::uint32_t>(vulkan_reflection_liquid_history_rayquery_spirv):model_records==4
            ?std::span<const std::uint32_t>(vulkan_reflection_ground_history_rayquery_spirv)
            :std::span<const std::uint32_t>(vulkan_reflection_history_rayquery_spirv);
        module.codeSize=shader.size_bytes();
        module.pCode=shader.data();
        check(vkCreateShaderModule(device,&module,nullptr,&next),"create reflection history shader");
        try {
            VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=next;compute.stage.pName="main";
            const std::array<std::uint32_t,4> values{model_records==5?1U:0U,
                model_records==3 || model_records==4 || model_records==6?1U:0U,model_records?1U:0U,model_records};
            const std::array<VkSpecializationMapEntry,4> entries{{{0,0,4},{1,4,4},{2,8,4},{3,12,4}}};
            const VkSpecializationInfo specialization{unsigned(entries.size()),entries.data(),sizeof(values),values.data()};
            compute.stage.pSpecializationInfo=&specialization;
            compute.layout=reflection_pipeline_layout;
            create_variant_pipeline(compute,destination,"create native reflection history pipeline");
        } catch(...) {vkDestroyShaderModule(device,next,nullptr);throw;}
        vkDestroyShaderModule(device,next,nullptr);
    }
    void ensure_native_liquid_motion_pipeline() {
        if(native_liquid_motion_pipeline)return;
        VkShaderModule next=VK_NULL_HANDLE;
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=sizeof(vulkan_reflection_liquid_motion_rayquery_spirv);
        module.pCode=vulkan_reflection_liquid_motion_rayquery_spirv.data();
        check(vkCreateShaderModule(device,&module,nullptr,&next),"create liquid motion shader");
        try {
            VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=next;compute.stage.pName="main";
            compute.layout=reflection_pipeline_layout;
            create_variant_pipeline(compute,native_liquid_motion_pipeline,"create liquid motion pipeline");
        } catch(...) {vkDestroyShaderModule(device,next,nullptr);throw;}
        vkDestroyShaderModule(device,next,nullptr);
    }
    VkAccelerationStructureKHR acceleration(Buffer& storage,VkAccelerationStructureTypeKHR type,
        VkDeviceSize bytes) {
        storage=create_buffer(bytes,VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
            |VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,false,true);
        VkAccelerationStructureCreateInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        info.buffer=storage.handle;info.size=bytes;info.type=type;
        VkAccelerationStructureKHR result{};
        check(vkCreateAccelerationStructureKHR(device,&info,nullptr,&result),"create acceleration structure");
        return result;
    }
    VkDeviceAddress acceleration_address(VkAccelerationStructureKHR structure) const {
        VkAccelerationStructureDeviceAddressInfoKHR info{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
        info.accelerationStructure=structure;
        return vkGetAccelerationStructureDeviceAddressKHR(device,&info);
    }
    Buffer scratch_buffer(VkDeviceSize bytes) {
        return create_buffer(std::max<VkDeviceSize>(1,bytes)+scratch_alignment-1,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,false,true);
    }
    VkDeviceAddress scratch_address(Buffer buffer) const {
        return (address(buffer)+scratch_alignment-1)&~VkDeviceAddress(scratch_alignment-1);
    }
    void record_empty_build(VkCommandBuffer command,Slot& slot) {
        // A real zero-instance TLAS, no BLAS or manufactured triangle. Vulkan
        // still requires a valid, aligned instances data address for count0;
        // reuse the reserved storage/input buffer without uploading source data.
        VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType=VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
        geometry.geometry.instances.data.deviceAddress=(address(slot.vertices)+15)&~VkDeviceAddress(15);
        VkAccelerationStructureBuildGeometryInfoKHR build{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        build.type=VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.geometryCount=1;build.pGeometries=&geometry;
        const std::uint32_t count=0;
        VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(device,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,&build,&count,&sizes);
        slot.tlas=acceleration(slot.tlas_buffer,VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,sizes.accelerationStructureSize);
        slot.scratch=scratch_buffer(sizes.buildScratchSize);
        build.dstAccelerationStructure=slot.tlas;build.scratchData.deviceAddress=scratch_address(slot.scratch);
        VkAccelerationStructureBuildRangeInfoKHR range{};const auto* ranges=&range;
        vkCmdBuildAccelerationStructuresKHR(command,1,&build,&ranges);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    }
    void record_build(VkCommandBuffer command,Slot& slot,std::uint32_t vertices) {
        if(!vertices){record_empty_build(command,slot);return;}
        VkAccelerationStructureGeometryTrianglesDataKHR triangles{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
        triangles.vertexFormat=VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress=address(slot.vertices);
        triangles.vertexStride=sizeof(Float4);triangles.maxVertex=vertices-1;
        triangles.indexType=VK_INDEX_TYPE_NONE_KHR;
        VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType=VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags=VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles=triangles;
        VkAccelerationStructureBuildGeometryInfoKHR build{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        build.type=VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        build.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.geometryCount=1;build.pGeometries=&geometry;
        const std::uint32_t count=vertices/3;
        VkAccelerationStructureBuildSizesInfoKHR sizes{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(device,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &build,&count,&sizes);
        slot.blas=acceleration(slot.blas_buffer,VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            sizes.accelerationStructureSize);
        VkAccelerationStructureInstanceKHR instance{};
        instance.transform.matrix[0][0]=instance.transform.matrix[1][1]=
            instance.transform.matrix[2][2]=1;
        instance.mask=0xff;
        instance.flags=VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference=acceleration_address(slot.blas);
        slot.instances=create_buffer(sizeof(instance),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            |VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,true,true);
        upload(slot.instances,&instance,sizeof(instance));
        VkAccelerationStructureGeometryInstancesDataKHR instances{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
        instances.data.deviceAddress=address(slot.instances);
        VkAccelerationStructureGeometryKHR top_geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        top_geometry.geometryType=VK_GEOMETRY_TYPE_INSTANCES_KHR;
        top_geometry.geometry.instances=instances;
        VkAccelerationStructureBuildGeometryInfoKHR top{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        top.type=VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        top.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        top.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        top.geometryCount=1;top.pGeometries=&top_geometry;
        const std::uint32_t one=1;
        VkAccelerationStructureBuildSizesInfoKHR top_sizes{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(device,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &top,&one,&top_sizes);
        slot.tlas=acceleration(slot.tlas_buffer,VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            top_sizes.accelerationStructureSize);
        slot.scratch=scratch_buffer(std::max(sizes.buildScratchSize,top_sizes.buildScratchSize));
        build.dstAccelerationStructure=slot.blas;build.scratchData.deviceAddress=scratch_address(slot.scratch);
        VkAccelerationStructureBuildRangeInfoKHR range{};range.primitiveCount=count;
        const VkAccelerationStructureBuildRangeInfoKHR* ranges=&range;
        vkCmdBuildAccelerationStructuresKHR(command,1,&build,&ranges);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,0,1,&barrier,0,nullptr,0,nullptr);
        top.dstAccelerationStructure=slot.tlas;top.scratchData.deviceAddress=scratch_address(slot.scratch);
        VkAccelerationStructureBuildRangeInfoKHR top_range{};top_range.primitiveCount=1;
        const VkAccelerationStructureBuildRangeInfoKHR* top_ranges=&top_range;
        vkCmdBuildAccelerationStructuresKHR(command,1,&top,&top_ranges);
        barrier.dstAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    }
    void record_dispatch(VkCommandBuffer command,Slot& slot,VkBuffer target,
        std::uint32_t pixels) {
        VkWriteDescriptorSetAccelerationStructureKHR structure{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        structure.accelerationStructureCount=1;structure.pAccelerationStructures=&slot.tlas;
        // Solid/geometry-only scenes never read material descriptors. Reuse
        // their existing storage-capable vertices instead of two dummy
        // allocations/uploads on every shadow frame.
        const auto& records=slot.materials.handle?slot.materials:slot.vertices;
        const auto& texels=slot.texels.handle?slot.texels:slot.vertices;
        const std::array<VkDescriptorBufferInfo,4> info{{
            {target,0,VkDeviceSize(pixels)*4},
            {slot.parameters.handle,0,sizeof(Parameters)},
            {records.handle,0,records.size},
            {texels.handle,0,texels.size}}};
        std::array<VkWriteDescriptorSet,5> writes{};
        for(unsigned i=0;i<writes.size();++i) {
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet=slot.descriptors;writes[i].dstBinding=i;
            writes[i].descriptorCount=1;
            writes[i].descriptorType=i==0?VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
                :i==2?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            if(i) writes[i].pBufferInfo=&info[i-1];
        }
        writes[0].pNext=&structure;
        vkUpdateDescriptorSets(device,unsigned(writes.size()),writes.data(),0,nullptr);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,
            0,1,&slot.descriptors,0,nullptr);
        vkCmdDispatch(command,(pixels+63)/64,1,1);
    }
    void record_reflection_dispatch(VkCommandBuffer command,Slot& slot,VkBuffer target,
        std::uint32_t pixels,std::uint32_t output_bytes,bool native_water,bool native_surface,bool native_model,bool history=false,
        unsigned history_model_records=0,bool liquid_motion=false) {
        VkWriteDescriptorSetAccelerationStructureKHR structure{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        structure.accelerationStructureCount=1;structure.pAccelerationStructures=&slot.tlas;
        const std::array<VkDescriptorBufferInfo,8> info{{
            {target,0,output_bytes},
            {slot.reflection_parameters.handle,0,slot.reflection_parameters.size},
            {slot.vertices.handle,0,slot.vertices.size},
            {slot.materials.handle,0,slot.materials.size},
            {slot.palette.handle?slot.palette.handle:slot.vertices.handle,0,
                slot.palette.handle?slot.palette.size:slot.vertices.size},
            {slot.texels.handle?slot.texels.handle:slot.vertices.handle,0,
                slot.texels.handle?slot.texels.size:slot.vertices.size},
            {slot.backdrop.handle?slot.backdrop.handle:slot.vertices.handle,0,
                slot.backdrop.handle?slot.backdrop.size:slot.vertices.size},
            {slot.enhanced_backdrop.handle?slot.enhanced_backdrop.handle:slot.vertices.handle,0,
                slot.enhanced_backdrop.handle?slot.enhanced_backdrop.size:slot.vertices.size}}};
        std::array<VkWriteDescriptorSet,9> writes{};
        for(unsigned i=0;i<writes.size();++i) {
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet=slot.reflection_descriptors;
            writes[i].dstBinding=i;
            writes[i].descriptorCount=1;
            writes[i].descriptorType=i==0?VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
                :i==2?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            if(i) writes[i].pBufferInfo=&info[i-1];
        }
        writes[0].pNext=&structure;
        vkUpdateDescriptorSets(device,unsigned(writes.size()),writes.data(),0,nullptr);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,history?(history_model_records==6?native_lava_history_pipeline:history_model_records==5?native_water_history_pipeline:
            history_model_records==4?native_ground_history_pipeline:history_model_records==3?native_scene_history_pipeline:history_model_records==2?native_path_history_pipeline
            :history_model_records==1?native_lobe_history_pipeline:native_history_pipeline):native_water?native_water_pipeline
            :native_surface?native_surface_pipeline:native_model?native_model_pipeline:reflection_pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,reflection_pipeline_layout,
            0,1,&slot.reflection_descriptors,0,nullptr);
        vkCmdDispatch(command,(pixels+63)/64,1,1);
        if(liquid_motion) {
            // Same resident output, descriptor set, command and completion
            // fence. Retire ray-query state before the bounded inverse solve.
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,1,&barrier,0,nullptr,0,nullptr);
            vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,native_liquid_motion_pipeline);
            vkCmdDispatch(command,(pixels+63)/64,1,1);
        }
    }
#endif
};

VulkanHardwareRt::VulkanHardwareRt():impl_(std::make_unique<Impl>()) {}
VulkanHardwareRt::~VulkanHardwareRt(){
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    // As with the live calibrated owner, failed completion is not permission
    // to destroy GPU-visible storage. Callers retain the device and retry.
    if(!impl_->release(true))(void)impl_.release();
#endif
}
bool VulkanHardwareRt::available(void* raw) const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    auto* device=static_cast<SDL_GPUDevice*>(raw);
    return device && SDL_GetBooleanProperty(SDL_GetGPUDeviceProperties(device),
        "starfox.vulkan.ray_query.enabled",false)
        && SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device),
            STARFOX_SDL_VULKAN_RAY_BRIDGE,nullptr);
#else
    (void)raw;return false;
#endif
}
bool VulkanHardwareRt::render_shadows(void* raw,const Scene& scene,Camera camera,
    Vec3 light,std::optional<ReceiverPlane> ground,const GpuScene::RayGeometryOutput* geometry,bool ground_only,
    std::optional<PrimaryRayRange> primary_range) {
    impl_->output={};
    if(ground_only && !ground) return false;
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    try {
        if(!valid_primary_range(primary_range)) {
            impl_->status="Invalid shadow primary depth range";return false;
        }
        const std::uint64_t pixels=std::uint64_t(camera.width)*camera.height;
        const auto length=std::sqrt(dot(light,light));
        const bool resident=geometry && geometry->complete && geometry->device==raw
            && geometry->buffer && ((geometry->vertex_count && geometry->vertex_count%3==0)
                || empty_native_geometry(*geometry));
        const auto vertex_count=resident?geometry->vertex_count:scene.triangle_count()*3;
        const auto* materials=resident?geometry->materials:nullptr;
        if(geometry && geometry->materials
            && geometry->materials->encoding!=RayMaterialEncoding::indexed
            && geometry->materials->encoding!=RayMaterialEncoding::native_rgba) {
            impl_->status="Unknown shadow material encoding";return false;
        }
        const bool native_rgba=geometry && geometry->materials
            && geometry->materials->encoding==RayMaterialEncoding::native_rgba;
        const auto record_bytes=std::uint64_t(vertex_count/3U)*sizeof(RayMaterial);
        if(native_rgba && (!resident || !geometry->material_offset || geometry->material_offset%4
            || std::uint64_t(geometry->material_offset)<std::uint64_t(vertex_count)*sizeof(Float4)
            || !materials->triangles.empty() || !materials->texels.empty() || vertex_count/3U>4'000'000
            || geometry->material_bytes<record_bytes || geometry->material_bytes>record_bytes+16'000'000
            || geometry->material_bytes%4 || (!vertex_count && !empty_native_geometry(*geometry)))) {
            impl_->status="Invalid resident native RGBA shadow material payload";return false;
        }
        // Indexed solid ink, including zero, is opaque. Only an actual source
        // texture atlas can contain cutouts, so opaque scenes keep their fast
        // traversal with no additional material copy or per-candidate lookup.
        const bool indexed=materials && !native_rgba && !materials->texels.empty();
        const bool cutouts=indexed || native_rgba;
        const auto material_bytes=native_rgba?std::uint64_t(geometry->material_bytes):record_bytes;
        if(indexed && ((!geometry->material_offset && materials->triangles.size()!=vertex_count/3U)
            || materials->texels.size()>16'000'000 || material_bytes>std::numeric_limits<std::uint32_t>::max())) {
            impl_->status="Invalid indexed shadow material topology/atlas";return false;
        }
        if(!available(raw) || !pixels || pixels>std::numeric_limits<std::uint32_t>::max()/4
            || !std::isfinite(length) || length<1.e-10 || camera.focal_length<=0
            || !std::isfinite(camera.focal_length) || !std::isfinite(camera.vertical_focal_length())
            || !std::isfinite(camera.center_x) || !std::isfinite(camera.center_y)
            || camera.vertical_focal_length()<=0
            || vertex_count>std::numeric_limits<std::uint32_t>::max()/sizeof(Float4))
            return false;
        if(ground && (!std::isfinite(dot(ground->point,ground->point))
            || !std::isfinite(dot(ground->normal,ground->normal)) || dot(ground->normal,ground->normal)<1.e-20)) {
            impl_->status="Invalid shadow ground plane";return false;
        }
        impl_->initialize(static_cast<SDL_GPUDevice*>(raw));
        auto& slot=impl_->slots[impl_->serial++%impl_->slots.size()];
        impl_->clear(slot);
        std::vector<Float4> vertices;
        if(!resident) {
            vertices.reserve(vertex_count);
            for(const auto& triangle:scene.triangles()) {
                vertices.push_back(vector4(triangle.a));
                vertices.push_back(vector4(triangle.b));
                vertices.push_back(vector4(triangle.c));
            }
        }
        slot.vertices=impl_->create_buffer(vertex_count?vertex_count*sizeof(Float4):32,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                |VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                |VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                |(resident?VK_BUFFER_USAGE_TRANSFER_DST_BIT:0),!resident,true);
        if(!resident && vertex_count) impl_->upload(slot.vertices,vertices.data(),vertices.size()*sizeof(Float4));
        if(cutouts) {
            slot.materials=impl_->create_buffer(material_bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                |(geometry->material_offset?VK_BUFFER_USAGE_TRANSFER_DST_BIT:0),!geometry->material_offset,false);
            if(!geometry->material_offset)impl_->upload(slot.materials,materials->triangles.data(),material_bytes);
        }
        if(indexed) {
            std::vector<std::uint32_t> texels((materials->texels.size()+3)/4);
            std::memcpy(texels.data(),materials->texels.data(),materials->texels.size());
            slot.texels=impl_->create_buffer(texels.size()*sizeof(std::uint32_t),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,false);
            impl_->upload(slot.texels,texels.data(),slot.texels.size);
        }
        Parameters params{};
        params.primary_range=primary_parameters(primary_range);
        params.extent_focal={float(camera.width),float(camera.height),
            float(camera.focal_length),float(camera.vertical_focal_length())};
        params.center_ground={float(camera.center_x),float(camera.center_y),ground?1.f:0.f,ground_only?1.f:0.f};
        if(ground) {params.ground_point=vector4(ground->point);params.ground_normal=vector4(ground->normal);}
        params.lights=light_samples(light,camera.shadow_samples(),camera.shadow_angular_radius());
        if(indexed)params.coverage={unsigned(vertex_count/3U),unsigned(materials->texels.size()),1,0};
        if(native_rgba)params.coverage={unsigned(vertex_count/3U),0,2,unsigned(material_bytes)};
        slot.parameters=impl_->create_buffer(sizeof(params),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true,false);
        impl_->upload(slot.parameters,&params,sizeof(params));
        impl_->ensure_output(std::uint32_t(pixels));
        auto* command=SDL_AcquireGPUCommandBuffer(impl_->sdl);
        if(!command) throw std::runtime_error(SDL_GetError());
        bool submitted=false;
        try {
            const auto native=impl_->ray_bridge->command(command);
            if(!native) throw std::runtime_error("Missing native Vulkan ray command");
            if(resident && vertex_count && !impl_->ray_bridge->copy_ray_range(command,geometry->buffer,0,
                slot.vertices.handle,slot.vertices.size,unsigned(vertex_count*sizeof(Float4))))
                throw std::runtime_error(SDL_GetError());
            if(cutouts && geometry->material_offset && !impl_->ray_bridge->copy_ray_range(command,geometry->buffer,
                geometry->material_offset,slot.materials.handle,slot.materials.size,unsigned(material_bytes)))
                throw std::runtime_error(SDL_GetError());
            impl_->record_build(native,slot,unsigned(vertex_count));
            const auto target=impl_->ray_bridge->prepare_write(command,impl_->sdl_output);
            if(!target) throw std::runtime_error(SDL_GetError());
            impl_->record_dispatch(native,slot,target,std::uint32_t(pixels));
            if(!impl_->ray_bridge->finish_write(command,impl_->sdl_output))
                throw std::runtime_error(SDL_GetError());
            submitted=true;
            impl_->submit(slot,command);
        } catch(...) {
            if(!submitted) SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        impl_->output={raw,impl_->sdl_output,camera.width,camera.height,0};
        impl_->status=!vertex_count?"Vulkan hardware rays with an empty caster scene"
            :resident?"Vulkan hardware rays from resident GPU casters"
            :"Vulkan hardware ray queries from CPU casters";
        return true;
    } catch(const std::exception& error) {impl_->status=error.what();return false;}
#else
    (void)raw;(void)scene;(void)camera;(void)light;(void)ground;(void)geometry;(void)ground_only;(void)primary_range;return false;
#endif
}
bool VulkanHardwareRt::render_reflections(void* raw,
    const GpuScene::RayGeometryOutput& geometry,Camera camera,
    std::span<const std::uint32_t,256> palette,std::uint32_t environment,
    std::uint8_t quality,float roughness,std::uint32_t metallic,
    std::optional<ReceiverPlane> ground,const GpuBackgroundDraw* background,
    const RayWater* water,bool ground_only,unsigned colour_encoding,std::optional<PrimaryRayRange> primary_range,
    const ResidentEnvironmentCube* resident_environment,bool specular_models,const RayReflectionHistory* history) {
    impl_->reflection={};
    if(geometry.materials && geometry.materials->encoding!=RayMaterialEncoding::indexed
        && geometry.materials->encoding!=RayMaterialEncoding::native_rgba) {
        impl_->status="Unknown reflection material encoding";return false;
    }
    if(ground_only && (!ground || !water)) return false;
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    try {
        const std::uint64_t pixels=std::uint64_t(camera.width)*camera.height;
        const auto reject=[&](const char* reason){impl_->status=reason;return false;};
        if(!valid_primary_range(primary_range)) return reject("Invalid reflection primary depth range");
        if(!available(raw)) return reject("Vulkan ray-query device unavailable");
        if(!quality || quality>3 || !pixels
            || pixels>std::numeric_limits<std::uint32_t>::max()/4U)
            return reject("Invalid reflection quality or frame dimensions");
        if(!std::isfinite(camera.focal_length) || !std::isfinite(camera.vertical_focal_length())
            || !std::isfinite(camera.center_x) || !std::isfinite(camera.center_y)
            || camera.focal_length<=0 || camera.vertical_focal_length()<=0)
            return reject("Invalid reflection camera focal length");
        if(!std::isfinite(roughness) || roughness<0 || roughness>1 || metallic>3 || colour_encoding>2)
            return reject("Invalid reflection roughness");
        if(water && ((!ground && (!water->mirror_models || water->material!=0 || water->caustics!=0))
            || water->material>3 || water->caustics>3
            || !std::isfinite(water->time) || !std::isfinite(water->reflection_strength)
            || !std::isfinite(water->brightness) || water->brightness<0 || water->brightness>1
            || water->reflection_strength<0 || water->reflection_strength>1))
            return reject("Invalid ray-water settings");
        if(!geometry.complete || geometry.device!=raw || !geometry.buffer)
            return reject("Resident reflection geometry unavailable");
        if((!geometry.vertex_count && !empty_native_geometry(geometry)) || geometry.vertex_count%3
            || geometry.vertex_count>std::numeric_limits<std::uint32_t>::max()/sizeof(Float4))
            return reject("Invalid reflection triangle vertex count");
        if(!geometry.materials) return reject("Reflection triangle materials missing");
        const bool native_rgba=geometry.materials->encoding==RayMaterialEncoding::native_rgba;
        if(specular_models && (!native_rgba || !colour_encoding))
            return reject("Calibrated model transport requires native materials and explicit linear/sRGB encoding");
        if(history && (!native_rgba || !colour_encoding || !geometry.vertex_count || ground_only
            || !reflection_history_receiver_valid(*history,water,ground.has_value())
            || !reflection_history_transport_valid(*history,roughness,metallic,specular_models)))
            return reject("Invalid native reflection history contract");
        if(history && history->curved_paths)
            return reject("Ordered curved native producer ABI is not implemented by this Vulkan pipeline");
        // Curved liquids have their actual old optical frame and unchanged
        // water prefix. Never alias flat-plane motion or MODEL path records.
        const bool liquid_history=history && history->separated && !history->model_lobes && water
            && (water->material==0 || water->material==3);
        const unsigned history_model_records=history && history->model_lobes?
            (history->scene_paths?3U:history->model_paths?2U:1U):liquid_history?(water->material==0?5U:6U):history && history->separated?4U:0U;
        const bool native_water=water && water->source_colour.has_value();
        const bool native_surface=native_rgba && colour_encoding && water && water->material!=0;
        const bool native_model=native_rgba && colour_encoding;
        std::optional<NativeWaterLayers> layers;
        if(water) {
            for(float value:water->world_to_view) if(!std::isfinite(value)) return reject("Non-finite ray-water transform");
            for(float value:water->camera_position) if(!std::isfinite(value)) return reject("Non-finite ray-water position");
            if(native_water) {
                if(!native_rgba || !ground || water->material!=0 || !colour_encoding)
                    return reject("Calibrated water requires native materials, a plane and explicit linear/sRGB encoding");
                for(float value:*water->source_colour)
                    if(!std::isfinite(value) || value<0 || value>1) return reject("Invalid calibrated water source colour");
            }
            if(native_water || (native_rgba && colour_encoding && ground && water->material!=0)) {
                const auto& r=water->world_to_view;
                const double determinant=double(r[0])*(double(r[4])*r[8]-double(r[5])*r[7])
                    -double(r[1])*(double(r[3])*r[8]-double(r[5])*r[6])
                    +double(r[2])*(double(r[3])*r[7]-double(r[4])*r[6]);
                double volume=1;
                for(unsigned row=0;row<3;++row) {
                    double squared=0;for(unsigned c=0;c<3;++c)squared+=double(r[row*3+c])*r[row*3+c];
                    volume*=std::sqrt(squared);
                }
                // A small but independent basis is not singular. Native planar
                // and lava geometry must not depend on the chosen world units.
                // Preserve the existing calibrated-water conditioning gate.
                if(!std::isfinite(determinant) || (native_water?std::abs(determinant)<1.e-8
                    :!std::isfinite(volume) || volume==0 || std::abs(determinant)<=volume*1.e-8))
                    return reject("Singular calibrated liquid/planar transform");
            }
            if(native_water) {
                if(water->auxiliary_layers || water->surface_layers) {
                    layers=native_water_layers(camera.width,camera.height,water->auxiliary_layers);
                    if(!layers) return reject("Calibrated water layer dimensions overflow");
                }
            } else if(water->auxiliary_layers || water->surface_layers)
                return reject("Water layers require calibrated source colour");
        }
        if(ground && (!std::isfinite(dot(ground->point,ground->point))
            || !std::isfinite(dot(ground->normal,ground->normal)) || dot(ground->normal,ground->normal)<1.e-20))
            return reject("Invalid reflection ground plane");
        const auto record_bytes=std::uint64_t(geometry.vertex_count/3U)*sizeof(RayMaterial);
        if(native_rgba && (!geometry.material_offset || geometry.material_offset%4
            || std::uint64_t(geometry.material_offset)<std::uint64_t(geometry.vertex_count)*sizeof(Float4)
            || !geometry.materials->triangles.empty() || !geometry.materials->texels.empty()
            || geometry.vertex_count/3U>4'000'000 || geometry.material_bytes<record_bytes
            || geometry.material_bytes>record_bytes+16'000'000 || geometry.material_bytes%4
            || (!geometry.vertex_count && !empty_native_geometry(geometry))))
            return reject("Invalid resident native RGBA reflection material payload");
        if(!native_rgba && !geometry.material_offset
            && geometry.materials->triangles.size()!=geometry.vertex_count/3U)
            return reject("Reflection material/vertex topology mismatch");
        if(geometry.materials->texels.size()>16'000'000)
            return reject("Reflection texture atlas too large");
        std::uint64_t cube_bytes=0;
        if(resident_environment) {
            const auto& cube=*resident_environment;
            if(!native_rgba || !colour_encoding || background || cube.relative_offset!=geometry.material_bytes
                || !cube.relative_offset || cube.relative_offset%4 || cube.face_size<8 || cube.face_size>512
                || (cube.face_size&(cube.face_size-1))) return reject("Invalid resident reflection environment metadata");
            for(unsigned row=0;row<3;++row)for(unsigned other=0;other<3;++other) {
                double product=0;
                for(unsigned c=0;c<3;++c)product+=double(cube.rotation[row*3+c])*cube.rotation[other*3+c];
                if(!std::isfinite(product) || std::abs(product-double(row==other))>.01)
                    return reject("Invalid resident environment rotation");
            }
            cube_bytes=std::uint64_t(cube.face_size)*cube.face_size*6*4;
            if(std::uint64_t(geometry.material_offset)+cube.relative_offset+cube_bytes>std::numeric_limits<std::uint32_t>::max())
                return reject("Resident environment byte range exceeds SDL buffer limits");
        }
        std::uint64_t material_copy_bytes=geometry.material_bytes+cube_bytes;
        std::optional<NativeReflectionHistory> history_layers;
        if(history) {
            const auto prefix_end=std::uint64_t(geometry.material_offset)+material_copy_bytes;
            const auto old_end=std::uint64_t(history->previous_vertex_offset)+std::uint64_t(geometry.vertex_count)*16;
            if(history->previous_vertex_offset<prefix_end || (history->previous_index_offset && history->previous_index_offset<old_end))
                return reject("Previous reflection correspondence overlaps current source data");
            const auto end=history->previous_index_offset?std::uint64_t(history->previous_index_offset)
                +std::uint64_t(geometry.vertex_count/3)*4:old_end;
            if(end>std::numeric_limits<std::uint32_t>::max())return reject("Previous reflection correspondence exceeds SDL buffer limits");
            if(!std::isfinite(float(history->near_plane)) || !std::isfinite(float(history->far_plane))
                || float(history->near_plane)<=0 || float(history->near_plane)>=float(history->far_plane))
                return reject("Previous reflection clip planes collapse on the GPU");
            for(unsigned i=0;i<4;++i)if(!std::isfinite(float(history->projection[i]))
                || (i<2 && float(history->projection[i])<=0))return reject("Previous reflection projection collapses on the GPU");
            if(history->previous_ground) {
                float squared=0;
                for(double value:history->previous_ground->normal) squared+=float(value)*float(value);
                if(!std::isfinite(squared) || squared<=1.e-20f)
                    return reject("Previous reflection ground plane collapses on the GPU");
            }
            if(history->previous_liquid) {
                const auto frame=reflection_liquid_frame_words(*history);
                float squared=0;for(unsigned i=4;i<7;++i)squared+=frame[i]*frame[i];
                const float determinant=frame[8]*(frame[13]*frame[18]-frame[14]*frame[17])
                    +frame[9]*(frame[14]*frame[16]-frame[12]*frame[18])+frame[10]*(frame[12]*frame[17]-frame[13]*frame[16]);
                if(!std::isfinite(squared) || squared<=1.e-20f || !std::isfinite(determinant) || std::abs(determinant)<=1.e-12f)
                    return reject("Previous liquid optical frame collapses on the GPU");
            }
            material_copy_bytes=end-geometry.material_offset;
            history_layers=native_reflection_history(camera.width,camera.height,history->extent,history->separated,layers.value_or(NativeWaterLayers{}),
                history->model_lobes,history->model_paths,history->scene_paths);
            if(!history_layers)return reject("Native reflection history dimensions overflow");
        }
        impl_->initialize(static_cast<SDL_GPUDevice*>(raw));
        impl_->ensure_reflection_pipeline();
        if(native_water && !history)impl_->ensure_native_water_pipeline();
        if(native_surface && !history)impl_->ensure_native_surface_pipeline();
        if(history)impl_->ensure_native_history_pipeline(history_model_records);
        else if(native_model && !native_water && !native_surface)impl_->ensure_native_model_pipeline();
        const bool liquid_motion=liquid_history && history->previous_liquid.has_value();
        if(liquid_motion)impl_->ensure_native_liquid_motion_pipeline();
        auto& slot=impl_->slots[impl_->serial++%impl_->slots.size()];
        impl_->clear(slot);
        slot.vertices=impl_->create_buffer(geometry.vertex_count?std::size_t(geometry.vertex_count)*sizeof(Float4):32,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                |VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                |VK_BUFFER_USAGE_TRANSFER_DST_BIT,false,true);
        const auto& materials=*geometry.materials;
        const auto material_bytes=native_rgba?std::size_t(geometry.material_bytes):std::size_t(record_bytes);
        slot.materials=impl_->create_buffer(history?material_copy_bytes:material_bytes+cube_bytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                |(geometry.material_offset?VK_BUFFER_USAGE_TRANSFER_DST_BIT:0),
            !geometry.material_offset,false);
        if(!geometry.material_offset)
            impl_->upload(slot.materials,materials.triangles.data(),material_bytes);
        // Native materials carry their own resident colours, including native
        // palette images. Never allocate/upload an unused indexed palette for
        // that path. Cubes also replace unused panorama/dummy images below;
        // unused descriptors reuse storage-capable vertices.
        if(!native_rgba) {
            slot.palette=impl_->create_buffer(palette.size_bytes(),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,false);
            impl_->upload(slot.palette,palette.data(),palette.size_bytes());
        }
        if(!materials.texels.empty()) {
            std::vector<std::uint32_t> texels((materials.texels.size()+3)/4);
            std::memcpy(texels.data(),materials.texels.data(),materials.texels.size());
            slot.texels=impl_->create_buffer(texels.size()*sizeof(std::uint32_t),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,false);
            impl_->upload(slot.texels,texels.data(),slot.texels.size);
        }
        // atan2 spans +/-pi, or about 804 source pixels at 256 px/radian.
        // Leave a small guard on both sides without rasterizing empty columns.
        constexpr std::uint32_t backdrop_width=1664,backdrop_height=224;
        constexpr std::uint32_t backdrop_origin=(backdrop_width-256)/2;
        const bool backdrop_requested=background && background->ppu
            && background->settings.layer==2 && (background->ppu->main_screen&2);
        if(!resident_environment)slot.backdrop=impl_->create_buffer(backdrop_requested
                ?std::size_t(backdrop_width)*backdrop_height*4:4,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                |(backdrop_requested?VK_BUFFER_USAGE_TRANSFER_DST_BIT:0),
            !backdrop_requested,false);
        if(!backdrop_requested && !resident_environment) {
            constexpr std::uint32_t empty=0;
            impl_->upload(slot.backdrop,&empty,sizeof(empty));
        }
        const auto* enhanced_environment=backdrop_requested
            ?background->settings.reflection_environment:nullptr;
        const auto* enhanced_image=enhanced_environment
            && enhanced_environment->modes[2]
            && enhanced_environment->backdrop_projection[3]==0
            ?enhanced_environment->backdrop:nullptr;
        if(enhanced_image) {
            if(!enhanced_image->width || !enhanced_image->height
                || enhanced_image->width>8192 || enhanced_image->height>8192
                || enhanced_image->pixels.size()!=std::size_t(enhanced_image->width)*enhanced_image->height)
                throw std::runtime_error("Invalid enhanced reflection sky");
            const auto bytes=enhanced_image->pixels.size()*sizeof(std::uint32_t);
            if(slot.enhanced_backdrop.size!=bytes) {
                impl_->destroy(slot.enhanced_backdrop);
                slot.enhanced_backdrop=impl_->create_buffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,false);
                slot.enhanced_upload={};
            }
            if(!slot.enhanced_upload.matches(*enhanced_image)) {
                impl_->upload(slot.enhanced_backdrop,enhanced_image->pixels.data(),bytes);
                slot.enhanced_upload.remember(*enhanced_image);
            }
        } else if(!slot.enhanced_backdrop.handle && !resident_environment) {
            slot.enhanced_backdrop=impl_->create_buffer(4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,false);
            constexpr std::uint32_t empty=0;
            impl_->upload(slot.enhanced_backdrop,&empty,sizeof(empty));
        }
        ReflectionParameters params{};
        params.primary_range=primary_parameters(primary_range);
        params.dimensions={camera.width,camera.height,quality,metallic};
        params.camera={float(camera.focal_length),float(camera.vertical_focal_length()),
            float(camera.center_x),float(camera.center_y)};
        params.settings={roughness,ground?1.f:0.f,float(materials.texels.size()),ground_only?1.f:0.f};
        if(native_rgba)params.material_info={2,unsigned(material_bytes),0,0};
        params.material_info[2]=colour_encoding;
        params.material_info[3]=specular_models?1U:0U;
        if(resident_environment) {
            const auto& cube=*resident_environment;
            params.cube_info={cube.relative_offset/4,cube.face_size,colour_encoding,1};
            params.cube_row0={cube.rotation[0],cube.rotation[1],cube.rotation[2],0};
            params.cube_row1={cube.rotation[3],cube.rotation[4],cube.rotation[5],0};
            params.cube_row2={cube.rotation[6],cube.rotation[7],cube.rotation[8],0};
        }
        if(ground) {
            params.ground_point=vector4(ground->point);
            params.ground_normal=vector4(ground->normal);
        }
        params.environment[0]=environment;
        if(enhanced_image) {
            params.enhanced_size={enhanced_image->width,enhanced_image->height,1,0};
            params.enhanced_modes=enhanced_environment->modes;
            params.enhanced_motion=vector4(enhanced_environment->motion);
            params.enhanced_plane=vector4(enhanced_environment->plane);
            params.enhanced_projection=vector4(enhanced_environment->backdrop_projection);
            params.enhanced_palette=vector4(enhanced_environment->backdrop_palette[0]);
            params.enhanced_keep0=vector4(enhanced_environment->backdrop_keep[0]);
            params.enhanced_keep1=vector4(enhanced_environment->backdrop_keep[1]);
        }
        if(water) {
            params.water_settings={water->time,water->reflection_strength,water->brightness,float(water->material+(water->mirror_models?16:0)+(water->caustics<<5))};
            params.water_row0={water->world_to_view[0],water->world_to_view[1],water->world_to_view[2],water->camera_position[0]};
            params.water_row1={water->world_to_view[3],water->world_to_view[4],water->world_to_view[5],water->camera_position[1]};
            params.water_row2={water->world_to_view[6],water->world_to_view[7],water->world_to_view[8],water->camera_position[2]};
            if(native_water) params.source_colour={(*water->source_colour)[0],(*water->source_colour)[1],(*water->source_colour)[2],1};
            if(layers) params.liquid_layers={layers->world_offset/4,layers->surface_offset/4,
                water->auxiliary_layers?3U:2U,layers->storage_bytes/4};
        }
        ReflectionHistoryParameters history_params{};
        ReflectionGroundHistoryParameters ground_history_params{};
        ReflectionLiquidHistoryParameters liquid_history_params{};
        if(history) {
            history_params.current=params;
            history_params.history_info={(history->previous_vertex_offset-geometry.material_offset)/4,
                history->previous_index_offset?(history->previous_index_offset-geometry.material_offset)/4:UINT32_MAX,
                history->extent[0],history->extent[1]};
            history_params.history_projection={float(history->projection[0]),float(history->projection[1]),
                float(history->projection[2]),float(history->projection[3])};
            history_params.history_clip={float(history->near_plane),float(history->far_plane),float(history->model_lobes),
                history->previous_ground || history->previous_liquid?1.f:0.f};
            if(history->previous_ground) {
                const auto& old=*history->previous_ground;
                ground_history_params.previous_ground_point={float(old.point[0]),float(old.point[1]),float(old.point[2]),0};
                ground_history_params.previous_ground_normal={float(old.normal[0]),float(old.normal[1]),float(old.normal[2]),0};
            }
            if(liquid_history)liquid_history_params.previous_liquid=reflection_liquid_frame_words(*history);
        }
        const auto uniform_bytes=liquid_history?sizeof(liquid_history_params):history_model_records==4?sizeof(ground_history_params):history?sizeof(history_params):sizeof(params);
        slot.reflection_parameters=impl_->create_buffer(uniform_bytes,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true,false);
        const auto output_bytes=history_layers?history_layers->storage_bytes:layers?layers->storage_bytes:std::uint32_t(pixels)*4U;
        impl_->ensure_reflection_output(output_bytes);
        auto* command=SDL_AcquireGPUCommandBuffer(impl_->sdl);
        if(!command) throw std::runtime_error(SDL_GetError());
        bool submitted=false;
        try {
            if(backdrop_requested) {
                auto settings=background->settings;
                settings.priority=TilePriorityPass::all;
                settings.horizontal_origin=int(backdrop_origin);
                settings.extend_horizontal=true;
                settings.logical_viewport={backdrop_width,backdrop_height};
                settings.raster_jitter={};
                const auto panorama=impl_->reflection_backdrop.enqueue(raw,command,
                    *background->ppu,backdrop_width,backdrop_height,1,settings);
                if(panorama.pixels) {
                    if(!impl_->ray_bridge->copy_ray_range(command,panorama.pixels,0,
                        slot.backdrop.handle,slot.backdrop.size,
                        backdrop_width*backdrop_height*4))
                        throw std::runtime_error(SDL_GetError());
                    params.environment[1]=backdrop_width;
                    params.environment[2]=backdrop_height;
                    params.environment[3]=backdrop_origin;
                }
            }
            if(liquid_history) {
                history_params.current=params;liquid_history_params.history=history_params;
                impl_->upload(slot.reflection_parameters,&liquid_history_params,sizeof(liquid_history_params));
            } else if(history_model_records==4) {
                history_params.current=params;ground_history_params.history=history_params;
                impl_->upload(slot.reflection_parameters,&ground_history_params,sizeof(ground_history_params));
            } else if(history) {history_params.current=params;impl_->upload(slot.reflection_parameters,&history_params,sizeof(history_params));}
            else impl_->upload(slot.reflection_parameters,&params,sizeof(params));
            const auto native=impl_->ray_bridge->command(command);
            if(!native) throw std::runtime_error("Missing native Vulkan ray command");
            if(geometry.vertex_count && !impl_->ray_bridge->copy_ray_range(command,geometry.buffer,0,
                slot.vertices.handle,slot.vertices.size,
                unsigned(std::size_t(geometry.vertex_count)*sizeof(Float4))))
                throw std::runtime_error(SDL_GetError());
            if(geometry.material_offset
                && !impl_->ray_bridge->copy_ray_range(command,geometry.buffer,
                    geometry.material_offset,slot.materials.handle,slot.materials.size,
                    unsigned(history?material_copy_bytes:material_bytes+cube_bytes)))
                throw std::runtime_error(SDL_GetError());
            impl_->record_build(native,slot,geometry.vertex_count);
            const auto target=impl_->ray_bridge->prepare_write(command,impl_->sdl_reflection);
            if(!target) throw std::runtime_error(SDL_GetError());
            impl_->record_reflection_dispatch(native,slot,target,std::uint32_t(pixels),output_bytes,native_water,native_surface,native_model,
                history!=nullptr,history_model_records,liquid_motion);
            if(!impl_->ray_bridge->finish_write(command,impl_->sdl_reflection))
                throw std::runtime_error(SDL_GetError());
            submitted=true;
            impl_->submit(slot,command);
        } catch(...) {
            if(!submitted) SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        impl_->reflection={raw,impl_->sdl_reflection,camera.width,camera.height,camera.width*4U};
        if(layers) impl_->reflection.water_layers=*layers;
        if(history_layers)impl_->reflection.reflection_history=*history_layers;
        impl_->status=history?(liquid_history?"Vulkan native separated curved liquid reflected-hit correspondence":history_model_records==4?"Vulkan native separated planar reflected-hit correspondence":history_model_records==3?"Vulkan native ordered MODEL/planar reflection paths":history_model_records==2?"Vulkan native ordered MODEL reflection paths":history_model_records==1
            ?"Vulkan native compact MODEL reflection lobes":"Vulkan native sharp reflected-hit correspondence")
            :native_model?(roughness>0?"Vulkan native calibrated reflections (8 fixed lobes)":"Vulkan native calibrated reflections (sharp)")
            :quality==1?"Vulkan hardware reflections LOW (1 ray)"
            :quality==2?"Vulkan hardware reflections MEDIUM (2 rays)"
            :"Vulkan hardware reflections HIGH (4 rays)";
        return true;
    } catch(const std::exception& error) {impl_->status=error.what();return false;}
#else
    (void)raw;(void)geometry;(void)camera;(void)palette;(void)environment;
    (void)quality;(void)roughness;(void)metallic;(void)ground;(void)background;(void)water;(void)colour_encoding;(void)primary_range;(void)resident_environment;(void)specular_models;(void)history;return false;
#endif
}
GpuShadowOutput VulkanHardwareRt::shadow_output() const noexcept{return impl_->output;}
GpuReflectionOutput VulkanHardwareRt::reflection_output() const noexcept{return impl_->reflection;}
const std::string& VulkanHardwareRt::status() const noexcept{return impl_->status;}
bool VulkanHardwareRt::native_work_complete() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    return impl_->complete();
#else
    return true;
#endif
}
std::uint64_t VulkanHardwareRt::working_image_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    return (impl_->sdl_output?std::uint64_t(impl_->output_capacity)*4:0)
        +(impl_->sdl_reflection?std::uint64_t(impl_->reflection_capacity):0);
#else
    return 0;
#endif
}
bool VulkanHardwareRt::try_release_device() noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    return impl_->release(false);
#else
    return true;
#endif
}
void VulkanHardwareRt::release_device() noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS) && (defined(__linux__) || defined(STARFOX_NATIVE_VULKAN_OWNER_PROBE))
    (void)impl_->release(true);
#endif
}
}
