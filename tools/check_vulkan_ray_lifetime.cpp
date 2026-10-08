// Real pinned SDL/Vulkan owner with diagnostic-only native allocation faults.
// This does not prove Linux ABI, presentation, application cleanup or FPS.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/vulkan_hardware_rt.hpp"
#include "native_ray_owner_fixture.hpp"
#include "starfox/render/vulkan_ray_support.hpp"
#include "starfox/render/sdl_vulkan_bridge.h"
#include "native_environment_cube_fixture.hpp"
#include <SDL3/SDL.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

// GNU linker wrapping is limited to this executable. Real submissions can be
// consumed and have their fence hidden, so the tested GPU work is genuinely live.
namespace faults {
bool active{},wait_error{},query_error{};unsigned lose_fences{},waits{},queries{},submits{},cancels{};
std::vector<SDL_GPUFence*> hidden_fences;
}
extern "C" SDL_GPUFence* __real_SDL_SubmitGPUCommandBufferAndAcquireFence(SDL_GPUCommandBuffer*);
extern "C" bool __real_SDL_WaitForGPUFences(SDL_GPUDevice*,bool,SDL_GPUFence* const*,Uint32);
extern "C" bool __real_SDL_WaitForGPUIdle(SDL_GPUDevice*);
extern "C" bool __real_SDL_QueryGPUFence(SDL_GPUDevice*,SDL_GPUFence*);
extern "C" bool __real_SDL_CancelGPUCommandBuffer(SDL_GPUCommandBuffer*);
extern "C" SDL_GPUFence* __wrap_SDL_SubmitGPUCommandBufferAndAcquireFence(SDL_GPUCommandBuffer* command) {
    if(faults::active)++faults::submits;
    auto* fence=__real_SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    if(faults::active && faults::lose_fences){--faults::lose_fences;if(fence)faults::hidden_fences.push_back(fence);
        SDL_SetError("Injected consumed-submit fence loss");return nullptr;}return fence;
}
extern "C" bool __wrap_SDL_WaitForGPUFences(SDL_GPUDevice* device,bool all,SDL_GPUFence* const* fences,Uint32 count) {
    if(faults::active){++faults::waits;if(faults::wait_error)return SDL_SetError("Injected fence wait failure");}
    return __real_SDL_WaitForGPUFences(device,all,fences,count);
}
extern "C" bool __wrap_SDL_WaitForGPUIdle(SDL_GPUDevice* device) {
    if(faults::active){++faults::waits;if(faults::wait_error)return SDL_SetError("Injected idle failure");}
    return __real_SDL_WaitForGPUIdle(device);
}
extern "C" bool __wrap_SDL_QueryGPUFence(SDL_GPUDevice* device,SDL_GPUFence* fence) {
    if(faults::active){++faults::queries;if(faults::query_error)return SDL_SetError("Injected query failure");}
    return __real_SDL_QueryGPUFence(device,fence);
}
extern "C" bool __wrap_SDL_CancelGPUCommandBuffer(SDL_GPUCommandBuffer* command) {
    if(faults::active)++faults::cancels;return __real_SDL_CancelGPUCommandBuffer(command);
}

namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct NativeLedger {
    std::unordered_set<VkBuffer> buffers;
    std::unordered_set<VkDeviceMemory> memory;
    std::unordered_set<VkShaderModule> shaders;
    std::unordered_set<VkPipeline> pipelines;
    std::unordered_set<VkPipelineLayout> pipeline_layouts;
    std::unordered_set<VkDescriptorSetLayout> layouts;
    std::unordered_set<VkDescriptorPool> pools;
    std::unordered_set<VkAccelerationStructureKHR> acceleration;
    unsigned dispatches{};
    unsigned count()const{return unsigned(buffers.size()+memory.size()+shaders.size()+pipelines.size()
        +pipeline_layouts.size()+layouts.size()+pools.size()+acceleration.size());}
} ledger;
PFN_vkGetDeviceProcAddr original_get{};
bool fail_pool{},fail_allocation{},fail_binding{},fail_finish{},fail_pipeline{};
#define PROC(name) PFN_##name real_##name{}
PROC(vkCreateBuffer);PROC(vkDestroyBuffer);PROC(vkAllocateMemory);PROC(vkFreeMemory);
PROC(vkCreateShaderModule);PROC(vkDestroyShaderModule);PROC(vkCreateComputePipelines);PROC(vkDestroyPipeline);
PROC(vkCreatePipelineLayout);PROC(vkDestroyPipelineLayout);PROC(vkCreateDescriptorSetLayout);PROC(vkDestroyDescriptorSetLayout);
PROC(vkCreateDescriptorPool);PROC(vkDestroyDescriptorPool);
PROC(vkBindBufferMemory);PROC(vkCreateAccelerationStructureKHR);PROC(vkDestroyAccelerationStructureKHR);PROC(vkCmdDispatch);
#undef PROC
#define SINGLE_CREATE(name,Info,Handle,collection) \
VKAPI_ATTR VkResult VKAPI_CALL proxy_##name(VkDevice device,const Info* info,const VkAllocationCallbacks* alloc,Handle* out) { \
    const auto result=real_##name(device,info,alloc,out);if(result==VK_SUCCESS)ledger.collection.insert(*out);return result; }
SINGLE_CREATE(vkCreateBuffer,VkBufferCreateInfo,VkBuffer,buffers)
SINGLE_CREATE(vkCreateShaderModule,VkShaderModuleCreateInfo,VkShaderModule,shaders)
SINGLE_CREATE(vkCreatePipelineLayout,VkPipelineLayoutCreateInfo,VkPipelineLayout,pipeline_layouts)
SINGLE_CREATE(vkCreateDescriptorSetLayout,VkDescriptorSetLayoutCreateInfo,VkDescriptorSetLayout,layouts)
SINGLE_CREATE(vkCreateAccelerationStructureKHR,VkAccelerationStructureCreateInfoKHR,VkAccelerationStructureKHR,acceleration)
#undef SINGLE_CREATE
#define SINGLE_DESTROY(name,Handle,collection) \
VKAPI_ATTR void VKAPI_CALL proxy_##name(VkDevice device,Handle handle,const VkAllocationCallbacks* alloc) { \
    require(ledger.collection.erase(handle)==1,"Native owner destroyed an unowned or already freed handle");real_##name(device,handle,alloc); }
SINGLE_DESTROY(vkDestroyBuffer,VkBuffer,buffers)
SINGLE_DESTROY(vkFreeMemory,VkDeviceMemory,memory)
SINGLE_DESTROY(vkDestroyShaderModule,VkShaderModule,shaders)
SINGLE_DESTROY(vkDestroyPipeline,VkPipeline,pipelines)
SINGLE_DESTROY(vkDestroyPipelineLayout,VkPipelineLayout,pipeline_layouts)
SINGLE_DESTROY(vkDestroyDescriptorSetLayout,VkDescriptorSetLayout,layouts)
SINGLE_DESTROY(vkDestroyDescriptorPool,VkDescriptorPool,pools)
SINGLE_DESTROY(vkDestroyAccelerationStructureKHR,VkAccelerationStructureKHR,acceleration)
#undef SINGLE_DESTROY
VKAPI_ATTR VkResult VKAPI_CALL proxy_vkCreateDescriptorPool(VkDevice device,const VkDescriptorPoolCreateInfo* info,
    const VkAllocationCallbacks* alloc,VkDescriptorPool* out) {
    if(fail_pool){fail_pool=false;*out=VK_NULL_HANDLE;return VK_ERROR_OUT_OF_HOST_MEMORY;}
    const auto result=real_vkCreateDescriptorPool(device,info,alloc,out);if(result==VK_SUCCESS)ledger.pools.insert(*out);return result;
}
VKAPI_ATTR VkResult VKAPI_CALL proxy_vkAllocateMemory(VkDevice device,const VkMemoryAllocateInfo* info,
    const VkAllocationCallbacks* alloc,VkDeviceMemory* out) {
    if(fail_allocation){fail_allocation=false;*out=VK_NULL_HANDLE;return VK_ERROR_OUT_OF_DEVICE_MEMORY;}
    const auto result=real_vkAllocateMemory(device,info,alloc,out);if(result==VK_SUCCESS)ledger.memory.insert(*out);return result;
}
VKAPI_ATTR VkResult VKAPI_CALL proxy_vkBindBufferMemory(VkDevice device,VkBuffer buffer,VkDeviceMemory memory,VkDeviceSize offset) {
    if(fail_binding){fail_binding=false;return VK_ERROR_OUT_OF_DEVICE_MEMORY;}return real_vkBindBufferMemory(device,buffer,memory,offset);
}
VKAPI_ATTR void VKAPI_CALL proxy_vkCmdDispatch(VkCommandBuffer command,unsigned x,unsigned y,unsigned z) {
    ++ledger.dispatches;real_vkCmdDispatch(command,x,y,z);
}
bool (*original_finish)(void*,void*){};
bool proxy_finish(void* command,void* buffer) {
    if(fail_finish){fail_finish=false;return SDL_SetError("Injected post-dispatch finish failure");}return original_finish(command,buffer);
}
VKAPI_ATTR VkResult VKAPI_CALL proxy_vkCreateComputePipelines(VkDevice device,VkPipelineCache cache,unsigned count,
    const VkComputePipelineCreateInfo* info,const VkAllocationCallbacks* alloc,VkPipeline* out) {
    const auto result=real_vkCreateComputePipelines(device,cache,count,info,alloc,out);
    for(unsigned i=0;i<count;++i)if(out[i])ledger.pipelines.insert(out[i]);
    if(fail_pipeline && result==VK_SUCCESS){fail_pipeline=false;return VK_ERROR_OUT_OF_DEVICE_MEMORY;}
    return result;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL proxy_get(VkDevice device,const char* name) {
#define WRAP(entry) if(std::strcmp(name,#entry)==0){real_##entry=reinterpret_cast<PFN_##entry>(original_get(device,name));return reinterpret_cast<PFN_vkVoidFunction>(proxy_##entry);}
    WRAP(vkCreateBuffer);WRAP(vkDestroyBuffer);WRAP(vkAllocateMemory);WRAP(vkFreeMemory);
    WRAP(vkCreateShaderModule);WRAP(vkDestroyShaderModule);WRAP(vkCreateComputePipelines);WRAP(vkDestroyPipeline);
    WRAP(vkCreatePipelineLayout);WRAP(vkDestroyPipelineLayout);WRAP(vkCreateDescriptorSetLayout);WRAP(vkDestroyDescriptorSetLayout);
    WRAP(vkCreateDescriptorPool);WRAP(vkDestroyDescriptorPool);
    WRAP(vkBindBufferMemory);WRAP(vkCreateAccelerationStructureKHR);WRAP(vkDestroyAccelerationStructureKHR);WRAP(vkCmdDispatch);
#undef WRAP
    return original_get(device,name);
}
struct Device {
    SDL_GPUDevice* device{};
    ~Device(){if(device)SDL_DestroyGPUDevice(device);SDL_Quit();}
};
struct BridgeLease {
    StarfoxSdlVulkanBridgeV2* original{};const StarfoxSdlVulkanRayBridgeV3* ray{};
    SDL_PropertiesID properties{};StarfoxSdlVulkanRayBridgeV3 ray_copy{};
    BridgeLease(SDL_GPUDevice* device) {
        properties=SDL_GetGPUDeviceProperties(device);
        original=static_cast<StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(properties,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
        ray=static_cast<const StarfoxSdlVulkanRayBridgeV3*>(SDL_GetPointerProperty(properties,STARFOX_SDL_VULKAN_RAY_BRIDGE,nullptr));
        require(original && original->version==2 && ray && ray->version==3,"Missing pinned Vulkan bridge");
        // The heap-owned device bridge has a property cleanup; mutate/restore
        // that table without replacing/freeing it. The ray bridge is read-only
        // static storage with no cleanup, so its property can borrow our copy.
        ray_copy=*ray;original_finish=ray->finish_write;ray_copy.finish_write=proxy_finish;
        require(SDL_SetPointerProperty(properties,STARFOX_SDL_VULKAN_RAY_BRIDGE,&ray_copy),SDL_GetError());
        original_get=original->get_device_proc;original->get_device_proc=proxy_get;
    }
    ~BridgeLease(){original->get_device_proc=original_get;SDL_SetPointerProperty(properties,STARFOX_SDL_VULKAN_RAY_BRIDGE,const_cast<StarfoxSdlVulkanRayBridgeV3*>(ray));}
};
struct PendingGate {
    SDL_GPUDevice* sdl{};VkDevice device{};VkSemaphore semaphore{};bool signalled{};
    PFN_vkSignalSemaphore signal{};PFN_vkDestroySemaphore destroy{};
    PendingGate(SDL_GPUDevice* input,const StarfoxSdlVulkanBridgeV2& bridge):sdl(input),device(bridge.device) {
        const auto create=reinterpret_cast<PFN_vkCreateSemaphore>(original_get(device,"vkCreateSemaphore"));
        signal=reinterpret_cast<PFN_vkSignalSemaphore>(original_get(device,"vkSignalSemaphore"));
        destroy=reinterpret_cast<PFN_vkDestroySemaphore>(original_get(device,"vkDestroySemaphore"));
        require(create && signal && destroy && bridge.wait_timeline,"Missing timeline gate support");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};info.pNext=&type;
        require(create(device,&info,nullptr,&semaphore)==VK_SUCCESS,"Cannot create timeline gate");
        if(!bridge.wait_timeline(sdl,semaphore,1)){destroy(device,semaphore,nullptr);semaphore=VK_NULL_HANDLE;require(false,SDL_GetError());}
    }
    void open() {
        if(signalled)return;VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};info.semaphore=semaphore;info.value=1;
        require(signal(device,&info)==VK_SUCCESS,"Cannot release timeline gate");signalled=true;
    }
    ~PendingGate(){if(semaphore){open();__real_SDL_WaitForGPUIdle(sdl);destroy(device,semaphore,nullptr);}}
};
struct Source {
    SDL_GPUDevice* device{};SDL_GPUBuffer* geometry{};SDL_GPUTransferBuffer *upload{},*download{};
    starfox::render::RayMaterials materials;
    native_reflection_fixture::NativeCubeInput input=native_reflection_fixture::cube_input(16,0,1);
    starfox::render::shadows::RayReflectionHistory history{16,{64,48},{96,85,31.3,22.7},1,700,4};
    explicit Source(SDL_GPUDevice* d):device(d) {
        materials.encoding=starfox::render::RayMaterialEncoding::native_rgba;
        const unsigned prefix=sizeof(input.source.source.geometry.vertices)+unsigned(input.source.source.words.size()*4+input.cube.size()*4);
        history.previous_vertex_offset=(prefix+15)&~15U;history.previous_index_offset=history.previous_vertex_offset+192;
        const unsigned bytes=history.previous_index_offset+16;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes,0};
        geometry=SDL_CreateGPUBuffer(device,&info);
        SDL_GPUTransferBufferCreateInfo up{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0},down{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,64*48*24,0};
        upload=SDL_CreateGPUTransferBuffer(device,&up);download=SDL_CreateGPUTransferBuffer(device,&down);
        require(geometry && upload && download,SDL_GetError());
        auto* data=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));require(data,SDL_GetError());
        const auto vertex_bytes=sizeof(input.source.source.geometry.vertices),material_bytes=input.source.source.words.size()*4;
        std::memset(data,0,bytes);
        std::memcpy(data,input.source.source.geometry.vertices.data(),vertex_bytes);
        std::memcpy(data+vertex_bytes,input.source.source.words.data(),material_bytes);
        std::memcpy(data+vertex_bytes+material_bytes,input.cube.data(),input.cube.size()*4);
        auto old=input.source.source.geometry.vertices;for(auto& p:old)p[3]=1;
        constexpr std::array<unsigned,4> mapping{17,9,6,12};
        std::memcpy(data+history.previous_vertex_offset,old.data(),192);
        std::memcpy(data+history.previous_index_offset,mapping.data(),16);SDL_UnmapGPUTransferBuffer(device,upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{geometry,0,bytes};SDL_UploadToGPUBuffer(copy,&from,&to,false);
        SDL_EndGPUCopyPass(copy);auto* fence=__real_SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,SDL_GetError());
        require(__real_SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
    }
    auto resident()const {
        return starfox::render::GpuScene::RayGeometryOutput{device,geometry,12,true,&materials,
            unsigned(sizeof(input.source.source.geometry.vertices)),unsigned(input.source.source.words.size()*4)};
    }
    auto cube()const {return starfox::render::shadows::ResidentEnvironmentCube{unsigned(input.source.source.words.size()*4),16};}
    bool reflect(starfox::render::shadows::NativeRayOwner& rays,const starfox::render::shadows::Camera& camera,
        const starfox::render::shadows::RayWater* water=nullptr,bool history_enabled=false,unsigned history_lobes=0,bool history_paths=false,bool scene_paths=false,bool separated_planar=false,
        unsigned separated_liquid=0,bool old_liquid=true)const {
        std::array<unsigned,256> palette{};const auto c=cube();
        std::optional<starfox::render::shadows::ReceiverPlane> plane;
        starfox::render::shadows::RayWater planar{1.3f,.7f,2};if(scene_paths || separated_planar)water=&planar;
        starfox::render::shadows::RayWater liquid{2.4f,.85f,separated_liquid==1?0U:3U};
        if(separated_liquid) {water=&liquid;if(separated_liquid==1){liquid.source_colour=std::array{.2f,.3f,.4f};liquid.auxiliary_layers=true;}}
        if(water)plane={{0,200,0},{0,-1,0}};
        auto records=history;records.separated=history_lobes!=0 || separated_planar || separated_liquid;records.model_lobes=history_lobes;records.model_paths=history_paths;records.scene_paths=scene_paths;
        if(separated_planar)records.previous_ground=starfox::render::shadows::RayReflectionGround{{0,210,0},{0,-1,0}};
        if(separated_liquid && old_liquid)records.previous_liquid=starfox::render::shadows::RayReflectionLiquid{
            starfox::render::shadows::RayReflectionGround{{0,210,0},{0,-1,0}},{1,0,0,0,1,0,0,0,1},{},1.3f,liquid.material};
        return rays.render_reflections(device,resident(),camera,palette,0xff346b98,1,history_lobes==8?.35f:0,0,plane,nullptr,water,false,1,{},&c,history_paths,
            history_enabled?&records:nullptr);
    }
    std::vector<unsigned> pixels(const starfox::render::shadows::GpuReflectionOutput& image) {
        auto* command=SDL_AcquireGPUCommandBuffer(device);auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(image.buffer),0,64*48*4};SDL_GPUTransferBufferLocation to{download,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=__real_SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,SDL_GetError());
        require(__real_SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
        const auto* data=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(device,download,false));require(data,SDL_GetError());
        std::vector<unsigned> result(data,data+64*48);SDL_UnmapGPUTransferBuffer(device,download);return result;
    }
    ~Source(){if(geometry)SDL_ReleaseGPUBuffer(device,geometry);if(upload)SDL_ReleaseGPUTransferBuffer(device,upload);if(download)SDL_ReleaseGPUTransferBuffer(device,download);}
};
}
int main(int argc,char** argv) try {
    bool integrated=false;
    for(int i=1;i<argc;++i){const std::string flag=argv[i];if(flag=="--integrated" && !integrated)integrated=true;
        else require(false,"Usage: starfox_vulkan_ray_lifetime_check [--integrated]");}
    Device owner;require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    const auto props=SDL_CreateProperties();require(props,SDL_GetError());
    SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan");
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,integrated);
    require(starfox::render::shadows::request_vulkan_ray_query(props),"Could not request native ray features");
    owner.device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);require(owner.device,SDL_GetError());
    SDL_SetBooleanProperty(SDL_GetGPUDeviceProperties(owner.device),"starfox.vulkan.ray_query.enabled",true);
    const auto support=starfox::render::shadows::query_vulkan_ray_query(owner.device);
    if(!support.available){std::cerr<<support.status<<"; SKIP, not acceptance\n";return 2;}
    BridgeLease lease(owner.device);
    const auto get_properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(lease.original->get_instance_proc(lease.original->instance,"vkGetPhysicalDeviceProperties"));
    VkPhysicalDeviceProperties properties{};get_properties(lease.original->physical_device,&properties);
    std::cout<<"Native Vulkan ray lifetime on "<<properties.deviceName<<'\n'<<std::flush;
    if(integrated && properties.deviceType!=VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)return 2;
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
    // Create a real second device before deliberately blocking the first
    // queue. Never use a fake device pointer to claim device-switch coverage.
    struct OtherDevice {SDL_GPUDevice* value{};~OtherDevice(){if(value)SDL_DestroyGPUDevice(value);}} other;
    const auto other_props=SDL_CreateProperties();require(other_props,SDL_GetError());
    SDL_SetStringProperty(other_props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan");
    SDL_SetBooleanProperty(other_props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(other_props,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,integrated);
    require(starfox::render::shadows::request_vulkan_ray_query(other_props),"Cannot request second-device native features");
    other.value=SDL_CreateGPUDeviceWithProperties(other_props);SDL_DestroyProperties(other_props);require(other.value,SDL_GetError());
    require(SDL_SetBooleanProperty(SDL_GetGPUDeviceProperties(other.value),"starfox.vulkan.ray_query.enabled",true)
        && starfox::render::shadows::query_vulkan_ray_query(other.value).available,"Second native device unavailable");
#endif
    starfox::render::shadows::NativeRayOwner rays;
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
    std::cout<<"Routing through the live calibrated SDL ray adapter (native Vulkan)\n"<<std::flush;
#endif
    starfox::render::shadows::Scene empty;
    const starfox::render::shadows::Camera camera{64,48,96,31.3,22.7,85};
    fail_pool=true;
    require(!rays.render_shadows(owner.device,empty,camera,{0,-1,0},{}),"Injected shadow initialization unexpectedly succeeded");
    require(!rays.shadow_output().buffer,"Initialization failure retained a borrowed image");
    require(ledger.count()==0,"Failed initialization retained partial native pipeline resources");
    require(rays.render_shadows(owner.device,empty,camera,{0,-1,0},{}),rays.status().c_str());
    rays.release_device();require(ledger.count()==0,"Successful cleanup leaked a native resource");
    Source source(owner.device);
    require(rays.render_shadows(owner.device,empty,camera,{0,-1,0},{}),rays.status().c_str());
    require(__real_SDL_WaitForGPUIdle(owner.device),SDL_GetError());
    const auto shadow_resources=ledger.count();fail_pool=true;
    require(!source.reflect(rays,camera) && !rays.reflection_output().buffer,"Reflection pool fault did not decline cleanly");
    require(ledger.count()==shadow_resources,"Reflection initialization changed the live shadow resources");
    require(source.reflect(rays,camera),rays.status().c_str());
    const auto reference=source.pixels(rays.reflection_output());unsigned painted=0;
    for(auto pixel:reference)painted+=pixel>>24==255;require(painted>100,"Lifetime baseline has no visible native models");
    rays.release_device();require(ledger.count()==0,"Reflection initialization retry leaked resources");
    for(unsigned failure=0;failure<2;++failure) {
        require(source.reflect(rays,camera),rays.status().c_str());require(source.pixels(rays.reflection_output())==reference,"Fresh native reflection changed");
        const auto before=ledger.count();const auto buffers=ledger.buffers.size(),allocations=ledger.memory.size();
        if(failure==0)fail_allocation=true;else fail_binding=true;
        require(!source.reflect(rays,camera) && !rays.reflection_output().buffer,"Buffer construction fault retained output");
        require(ledger.count()==before && ledger.buffers.size()==buffers && ledger.memory.size()==allocations,"Failed buffer construction leaked buffer or memory");
        require(source.reflect(rays,camera),rays.status().c_str());require(source.pixels(rays.reflection_output())==reference,"Allocation recovery changed model pixels");
        rays.release_device();require(ledger.count()==0 && !rays.working_image_bytes(),"Allocation recovery cleanup leaked resources/images");
    }
    require(source.reflect(rays,camera),rays.status().c_str());source.pixels(rays.reflection_output());
    faults::active=true;const auto cancelled=faults::cancels;fail_finish=true;
    require(!source.reflect(rays,camera) && !rays.reflection_output().buffer,"Post-dispatch record fault retained output");
    require(faults::cancels==cancelled+1,"Unsubmitted native command was not cancelled exactly once");
    require(rays.native_work_complete() && rays.try_release_device(),"Cancelled recording stayed pending");
    require(ledger.count()==0 && !rays.working_image_bytes(),"Cancelled recording leaked resources/images");faults::active=false;

    require(source.reflect(rays,camera),rays.status().c_str());require(source.pixels(rays.reflection_output())==reference,"Post-record recovery changed model pixels");
    const auto geometry=source.resident();
    require(rays.render_shadows(owner.device,empty,camera,{0,-1,0},{},&geometry),rays.status().c_str());
    starfox::render::shadows::RayWater water;water.time=1.3f;water.reflection_strength=.7f;water.brightness=.8f;
    water.source_colour=std::array{.2f,.3f,.4f};water.auxiliary_layers=true;
    const auto before_variant=ledger.count();fail_pipeline=true;
    require(!source.reflect(rays,camera,&water) && !rays.reflection_output().buffer,"Partial native variant unexpectedly became ready");
    require(ledger.count()==before_variant,"Failed native variant leaked a pipeline or destroyed live resources");
    require(source.reflect(rays,camera,&water),rays.status().c_str());
    require(__real_SDL_WaitForGPUIdle(owner.device),SDL_GetError());
    const auto full=rays.reflection_output().water_layers;
    require(full.storage_bytes==64*48*24,"Unexpected canonical retained water size");
    require(rays.working_image_bytes()==std::uint64_t(full.storage_bytes)+64*48*4,"Working-image count omitted shadow/full-water capacity");
    require(source.reflect(rays,camera),rays.status().c_str());require(source.pixels(rays.reflection_output())==reference,"Full-to-small image changed model pixels");
    require(!rays.reflection_output().water_layers.storage_bytes,"Small reflection retained stale water layout");
    const auto retained_bytes=rays.working_image_bytes();
    require(retained_bytes==std::uint64_t(full.storage_bytes)+64*48*4,"Smaller request hid retained larger image capacity");
    unsigned pending_queries=0;
    {
        PendingGate gate(owner.device,*lease.original);faults::active=true;
        require(rays.render_shadows(owner.device,empty,camera,{0,-1,0},{},&geometry),rays.status().c_str());
        require(source.reflect(rays,camera) && source.reflect(rays,camera),rays.status().c_str());
        const auto live=ledger.count(),waits=faults::waits;
        require(!rays.native_work_complete() && !rays.try_release_device(),"Blocked real GPU work was declared complete");
        require(faults::waits==waits,"Nonblocking completion/cleanup waited on GPU");
        require(ledger.count()==live && rays.working_image_bytes()==retained_bytes,"Pending cleanup destroyed native/image resources");
        require(!rays.shadow_output().buffer && !rays.reflection_output().buffer,"Cleanup request retained borrowed outputs");
        faults::wait_error=true;rays.release_device();
        require(ledger.count()==live && rays.working_image_bytes()==retained_bytes,"Failed blocking wait destroyed pending resources");
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
        require(!rays.adapter().render_resident(other.value,empty,camera,{0,-1,0},{})
            && !rays.adapter().output().buffer && !rays.adapter().reflection_output().buffer,
            "Pending old device was replaced by a different SDL device");
        require(ledger.count()==live && rays.working_image_bytes()==retained_bytes,
            "Failed adapter device switch discarded pending native resources/images");
#endif
        require(!source.reflect(rays,camera),"Slot-reuse wait failure was ignored");
        require(!rays.render_shadows(owner.device,empty,camera,{0,-1,0},{},&geometry),"Shadow slot wait failure was ignored");
        require(ledger.count()==live,"Failed slot reuse freed pending native storage");
        faults::wait_error=false;gate.open();require(__real_SDL_WaitForGPUIdle(owner.device),SDL_GetError());
        faults::query_error=true;const auto completed_live=ledger.count(),completed_waits=faults::waits;
        require(!rays.native_work_complete() && !rays.try_release_device(),"Query error was treated as completion");
        require(ledger.count()==completed_live && faults::waits==completed_waits,"Query-error cleanup destroyed resources or waited");
        faults::query_error=false;require(rays.native_work_complete() && rays.try_release_device(),"Signalled native work could not retire");
        require(ledger.count()==0 && !rays.working_image_bytes(),"Completed cleanup leaked resources/images");
        pending_queries=faults::queries;faults::active=false;
    }
    require(source.reflect(rays,camera),rays.status().c_str());require(source.pixels(rays.reflection_output())==reference,"Pending cleanup retry changed native model pixels");rays.release_device();
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
    require(rays.adapter().render_resident(other.value,empty,camera,{0,-1,0},{}),rays.status().c_str());
    require(rays.adapter().output().device==other.value && !rays.adapter().output().packed_row_bytes
        && !rays.adapter().reflection_output().buffer,"Adapter relabelled a uint32 shadow as an RGBA/packed-byte image");
    require(__real_SDL_WaitForGPUIdle(other.value),SDL_GetError());
    // A drained switch back must use the original submitted source, never a
    // stale resident image or a previous device's native descriptors.
    require(source.reflect(rays,camera),rays.status().c_str());
    require(!rays.adapter().output().buffer && rays.adapter().reflection_output().device==owner.device
        && source.pixels(rays.reflection_output())==reference,"Drained switch back changed original native pixels/ownership");
    rays.release_device();require(!rays.working_image_bytes() && ledger.count()==0,"Adapter device-switch cleanup retained resources");
    std::cout<<"Adapter-only: pending real-device replacement refused; drained two-way replacement, uint32-shadow/RGBA ownership and exact original pixels passed\n";
#endif
    unsigned consumed_failures=0,late_markers=0,post_idle_polls=0;
    for(unsigned producer=0;producer<11;++producer)for(unsigned lost:{1U,2U})for(unsigned cycle=0;cycle<2;++cycle) {
        require(source.reflect(rays,camera),rays.status().c_str());require(source.pixels(rays.reflection_output())==reference,"Consumed-submit baseline changed");
        require(rays.render_shadows(owner.device,empty,camera,{0,-1,0},{},&geometry),rays.status().c_str());
        require(__real_SDL_WaitForGPUIdle(owner.device),SDL_GetError());
        {
            PendingGate gate(owner.device,*lease.original);faults::active=true;faults::lose_fences=lost;
            const auto dispatches=ledger.dispatches,cancels=faults::cancels;
            const unsigned producer_dispatches=producer==7 || producer==8?2:1;
            const bool result=producer?source.reflect(rays,camera,nullptr,producer>=2,producer>=3 && producer<6?8:0,producer>=4 && producer<6,producer==5,producer==6,
                producer>=7?(producer%2?1:2):0,producer<9)
                :rays.render_shadows(owner.device,empty,camera,{0,-1,0},{},&geometry);
            require(!result,"Consumed-submit fence loss unexpectedly succeeded");
            require(producer?!rays.reflection_output().buffer:!rays.shadow_output().buffer,"Consumed-submit failure retained output");
            require(ledger.dispatches==dispatches+producer_dispatches && faults::cancels==cancels,"Consumed native work was replayed/cancelled");
            const auto live=ledger.count(),waits=faults::waits;const auto bytes=rays.working_image_bytes();
            // Completion is nonblocking and may stop at an earlier slot. Force
            // that case so a twice-hidden fence's recovery marker is first
            // submitted AFTER the fixture's idle wait, not assumed complete
            // by one poll. The producer must retain everything until it signals.
            const bool defer_marker=lost==2 && cycle==1;faults::query_error=defer_marker;
            require(!rays.native_work_complete() && !rays.try_release_device(),"Consumed but unfenced GPU work was declared complete");
            require(ledger.count()==live && rays.working_image_bytes()==bytes && faults::waits==waits,"Unfenced cleanup freed resources or waited");
            require(ledger.dispatches==dispatches+producer_dispatches,"Completion marker replayed native dispatch");
            faults::query_error=false;
            gate.open();require(__real_SDL_WaitForGPUIdle(owner.device),SDL_GetError());
            const auto idle_submits=faults::submits,idle_queries=faults::queries;
            const auto deadline=SDL_GetTicksNS()+2'000'000'000ULL;
            bool complete=rays.native_work_complete(),released=false;
            while(!complete && SDL_GetTicksNS()<deadline) {
                // Completion may advance between the query and cleanup poll.
                released=rays.try_release_device();if(released)break;
                require(ledger.count()==live && rays.working_image_bytes()==bytes && faults::waits==waits,
                    "Late recovery marker polling freed resources or waited");
                // Diagnostic-only pacing; no wait is added to the native owner.
                SDL_Delay(1);++post_idle_polls;complete=rays.native_work_complete();
            }
            if(!released && complete)released=rays.try_release_device();
            if(!released)throw std::runtime_error("Ordered recovery marker could not retire consumed work: producer/lost/cycle="
                +std::to_string(producer)+"/"+std::to_string(lost)+"/"+std::to_string(cycle)
                +" complete="+std::to_string(complete)+" post-idle submits/queries="+std::to_string(faults::submits-idle_submits)
                +"/"+std::to_string(faults::queries-idle_queries)+" remaining hidden fences="+std::to_string(faults::lose_fences)
                +" status="+rays.status());
            if(defer_marker) {require(faults::submits==idle_submits+1,"Deferred completion did not enqueue exactly one late marker");++late_markers;}
            require(faults::waits==waits && ledger.dispatches==dispatches+producer_dispatches && faults::cancels==cancels,
                "Late recovery replayed/cancelled native work or waited");
            require(ledger.count()==0 && !rays.working_image_bytes(),"Consumed-submit recovery leaked resources/images");
            for(auto* fence:faults::hidden_fences)SDL_ReleaseGPUFence(owner.device,fence);faults::hidden_fences.clear();faults::active=false;
        }
        require(source.reflect(rays,camera),rays.status().c_str());require(source.pixels(rays.reflection_output())==reference,"Consumed-submit recovery changed native pixels");rays.release_device();++consumed_failures;
    }
    for(unsigned kind=0;kind<7;++kind) {
        require(source.reflect(rays,camera),rays.status().c_str());source.pixels(rays.reflection_output());
        const auto before_history_variant=ledger.count();fail_pipeline=true;
        require(!source.reflect(rays,camera,nullptr,true,kind && kind<4?8:0,kind>=2 && kind<4,kind==3,kind==4,kind>=5?kind-4:0) && !rays.reflection_output().buffer,"Partial history variant unexpectedly became ready");
        require(ledger.count()==before_history_variant,"Failed history variant leaked its temporary shader/pipeline or destroyed live resources");
        require(source.reflect(rays,camera,nullptr,true,kind && kind<4?8:0,kind>=2 && kind<4,kind==3,kind==4,kind>=5?kind-4:0),rays.status().c_str());
        const auto expected=starfox::render::shadows::native_reflection_history(64,48,source.history.extent,kind!=0,rays.reflection_output().water_layers,kind && kind<4?8:0,kind>=2 && kind<4,kind==3);
        require(expected && rays.reflection_output().reflection_history==*expected,"History pipeline retry lost its output contract");
        if(kind==0)require(source.pixels(rays.reflection_output())==reference,"Sharp history variant recovery changed current RGB");
        else source.pixels(rays.reflection_output());
        rays.release_device();require(ledger.count()==0 && !rays.working_image_bytes(),"History variant cleanup leaked native resources/images");
    }
    for(unsigned liquid:{1U,2U}) {
        // Prime only RT. The OLD frame is absent, so inverse-solve pipeline
        // creation must remain lazy and its partial failure independently safe.
        require(source.reflect(rays,camera,nullptr,true,0,false,false,false,liquid,false),rays.status().c_str());source.pixels(rays.reflection_output());
        const auto before=ledger.count();fail_pipeline=true;
        require(!source.reflect(rays,camera,nullptr,true,0,false,false,false,liquid) && !rays.reflection_output().buffer,"Partial inverse-solve pipeline became ready");
        require(ledger.count()==before,"Partial inverse-solve creation leaked resources/destroyed cached RT");
        require(source.reflect(rays,camera,nullptr,true,0,false,false,false,liquid),rays.status().c_str());source.pixels(rays.reflection_output());
        faults::active=true;const auto cancels=faults::cancels,dispatches=ledger.dispatches;fail_finish=true;
        require(!source.reflect(rays,camera,nullptr,true,0,false,false,false,liquid),"Two-pass post-record failure was ignored");
        require(faults::cancels==cancels+1 && ledger.dispatches==dispatches+2,"Two-pass unsubmitted work was replayed or cancelled incorrectly");
        require(rays.native_work_complete() && rays.try_release_device(),"Two-pass cancelled recording stayed pending");
        require(ledger.count()==0 && !rays.working_image_bytes(),"Two-pass cancelled recording leaked native resources/images");faults::active=false;
    }
    require(late_markers==11,"Missing deliberate late-marker coverage");
    require(ledger.count()==0 && rays.native_work_complete() && rays.try_release_device(),"Final native owner did not release cleanly");
    std::cout<<"Native lifetime: shadow/reflection initialization, allocation/bind, partial water/sharp/lobe/path/scene/planar/liquid RT and inverse-solve pipeline, one/two-pass post-dispatch cancellation faults; "
        <<"three real timeline-blocked slots, failed waits/query errors, nonblocking retirement, retained shadow/full-water image bytes="<<retained_bytes
        <<", pending queries="<<pending_queries<<", consumed-submit recovery cases="<<consumed_failures
        <<", deliberate post-idle recovery markers="<<late_markers<<", post-idle nonblocking polls="<<post_idle_polls
        <<" (one/two missing fences, shadow/RGBA/sharp/lobe/path/scene/planar/water/lava producers, old-frame present/absent, repeated cycles), exact restored model pixels="<<painted
        <<"; zero leaked native buffers/memory/AS/pipelines/descriptors, no native dispatch replay, no poll/try-release waits; NOT Linux/application/FPS acceptance\n"<<std::flush;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
