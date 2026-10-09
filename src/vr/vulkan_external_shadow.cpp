#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "starfox/vr/vulkan_external_shadow.hpp"
#include <cstring>
#include <cmath>

namespace starfox::vr {
void VulkanExternalShadow::close() noexcept {
    pipeline_.close();vertices_.close();pass_={};width_=height_=0;bytes_=0;
    textures_.close();
    if(device_ && get_) {
        if(ready_) reinterpret_cast<PFN_vkDestroySemaphore>(get_(device_,"vkDestroySemaphore"))(device_,ready_,nullptr);
        if(buffer_) reinterpret_cast<PFN_vkDestroyBuffer>(get_(device_,"vkDestroyBuffer"))(device_,buffer_,nullptr);
        if(memory_) reinterpret_cast<PFN_vkFreeMemory>(get_(device_,"vkFreeMemory"))(device_,memory_,nullptr);
    }
    ready_={};buffer_={};memory_={};device_={};get_={};
}
bool VulkanExternalShadow::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkPhysicalDevice physical,
    PFN_vkGetPhysicalDeviceProperties2 properties,const std::array<std::uint8_t,8>& luid,
    void* resource,void* fence,VkDeviceSize bytes) {
    // Reject replacement: caller explicitly retires GPU use and closes first.
    if(device_) return false;
#if defined(_WIN32)
    if(!device || !get || !physical || !properties || !resource || !fence
        || !bytes || bytes%4 || bytes>256ULL*1024*1024) return false;
    VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&id;
    properties(physical,&props);
    if(!id.deviceLUIDValid || std::memcmp(id.deviceLUID,luid.data(),8)!=0) return false;
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name)); if(!name) return false
    LOAD(vkCreateBuffer);LOAD(vkDestroyBuffer);LOAD(vkGetBufferMemoryRequirements);
    LOAD(vkGetMemoryWin32HandlePropertiesKHR);LOAD(vkAllocateMemory);LOAD(vkFreeMemory);
    LOAD(vkBindBufferMemory);LOAD(vkCreateSemaphore);LOAD(vkDestroySemaphore);LOAD(vkImportSemaphoreWin32HandleKHR);
#undef LOAD
    device_=device;get_=get;
    const bool success=[&] {
        VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
        external.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.pNext=&external;
        buffer.size=bytes;buffer.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if(vkCreateBuffer(device,&buffer,nullptr,&buffer_)!=VK_SUCCESS) return false;
        VkMemoryRequirements need{};vkGetBufferMemoryRequirements(device,buffer_,&need);
        VkMemoryWin32HandlePropertiesKHR allowed{VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR};
        if(vkGetMemoryWin32HandlePropertiesKHR(device,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,resource,&allowed)!=VK_SUCCESS) return false;
        const auto bits=need.memoryTypeBits&allowed.memoryTypeBits;
        if(!bits) return false;
        unsigned type=0;while(!(bits&(1U<<type))) ++type;
        VkImportMemoryWin32HandleInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR};
        import.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;import.handle=resource;
        VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
        dedicated.pNext=&import;dedicated.buffer=buffer_;
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.pNext=&dedicated;
        allocate.allocationSize=need.size;allocate.memoryTypeIndex=type;
        if(vkAllocateMemory(device,&allocate,nullptr,&memory_)!=VK_SUCCESS
            || vkBindBufferMemory(device,buffer_,memory_,0)!=VK_SUCCESS) return false;
        VkSemaphoreTypeCreateInfo timeline{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};timeline.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};semaphore.pNext=&timeline;
        if(vkCreateSemaphore(device,&semaphore,nullptr,&ready_)!=VK_SUCCESS) return false;
        VkImportSemaphoreWin32HandleInfoKHR sync{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR};
        sync.semaphore=ready_;sync.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;sync.handle=fence;
        return vkImportSemaphoreWin32HandleKHR(device,&sync)==VK_SUCCESS
            && textures_.initialize_external(device,get,buffer_,bytes);
    }();
    if(!success) close();
    else bytes_=bytes;
    return success;
#else
    (void)device;(void)get;(void)physical;(void)properties;(void)luid;(void)resource;(void)fence;(void)bytes;
    return false;
#endif
}
bool VulkanExternalShadow::prepare_draw(const VkPhysicalDeviceMemoryProperties& properties,VkRenderPass pass,
    uint32_t width,uint32_t height,std::array<float,4> color,SceneBlend blend) {
    if(!device_ || !pass || !width || !height || width>16384 || height>16384
        || ((uint64_t(width)+3)&~uint64_t(3))*height>bytes_) return false;
    for(auto value:color) if(!std::isfinite(value) || value<0 || value>1) return false;
    if(pass_) return pass_==pass && width_==width && height_==height && color_==color && blend_==blend;
    std::array<SceneVertex,6> quad{};
    constexpr unsigned corners[]{0,1,2,0,2,3};
    constexpr float xy[4][2]{{0,0},{1,0},{1,1},{0,1}};
    for(unsigned i=0;i<6;++i) {
        auto& v=quad[i];const auto corner=corners[i];
        v.position[0]=xy[corner][0]*2-1;v.position[1]=xy[corner][1]*2-1;v.position[2]=.5F;
        for(unsigned c=0;c<4;++c) v.color[c]=color[c];
        v.uv[0]=xy[corner][0]*width;v.uv[1]=xy[corner][1]*height;
        v.texture[1]=width-1;v.texture[2]=height-1;v.texture[3]=1073741824U|1U;
    }
    if(!vertices_.initialize(device_,get_,properties,quad)
        || !pipeline_.initialize(device_,get_,pass,false,SceneTopology::triangles,layout(),blend)) {
        pipeline_.close();vertices_.close();return false;
    }
    pass_=pass;width_=width;height_=height;color_=color;blend_=blend;return true;
}
bool VulkanExternalShadow::record_draw(VkCommandBuffer command,VkExtent2D extent) const {
    if(!pass_) return false;
    const Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    return pipeline_.record(command,extent,vertices_.buffer(),vertices_.count(),{identity,identity},descriptor());
}
}
