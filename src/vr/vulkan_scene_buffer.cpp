#include "starfox/vr/vulkan_scene_buffer.hpp"
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    const auto fn=reinterpret_cast<T>(get(device,name));
    if(!fn) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return fn;
}
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": "+std::to_string(result));
}
}
VulkanSceneBuffer::~VulkanSceneBuffer() {close();}
void VulkanSceneBuffer::close() noexcept {
    if(mapped_) unmap_(device_,memory_);
    if(buffer_) destroy_(device_,buffer_,nullptr);
    if(memory_) free_(device_,memory_,nullptr);
    device_={};buffer_={};memory_={};count_=0;mapped_=false;
}
bool VulkanSceneBuffer::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& properties,std::span<const SceneVertex> vertices) {
    close();
    try {
        if(!device || !get || vertices.empty() || vertices.size()>4'000'000
            || properties.memoryTypeCount>VK_MAX_MEMORY_TYPES)
            throw std::runtime_error("Invalid scene vertex upload");
        for(const auto& vertex:vertices) {
            if(vertex.visibility_enabled>2 || (vertex.visibility_enabled==2
                && (vertex.group_c[0]<1 || vertex.group_c[0]>255 || vertex.group_c[1]<=0
                    || (vertex.group_c[2]!=0 && vertex.group_c[2]!=1) || vertex.group_enabled)))
                throw std::runtime_error("Invalid explosion/visibility payload");
            for(float component:vertex.billboard) if(!std::isfinite(component)) throw std::runtime_error("Nonfinite billboard offset");
            for(float component:vertex.uv) if(!std::isfinite(component)) throw std::runtime_error("Nonfinite texture coordinate");
            for(float component:vertex.position) if(!std::isfinite(component)) throw std::runtime_error("Nonfinite scene position");
            for(float component:vertex.color) if(!std::isfinite(component)) throw std::runtime_error("Nonfinite scene color");
            for(float component:vertex.odd_color) if(!std::isfinite(component)) throw std::runtime_error("Nonfinite alternate scene color");
            for(const auto* triple:{vertex.visibility_a,vertex.visibility_b,vertex.visibility_c,
                                   vertex.group_a,vertex.group_b,vertex.group_c})
                for(unsigned i=0;i<3;++i) if(!std::isfinite(triple[i])) throw std::runtime_error("Nonfinite visibility vertex");
        }
        device_=device;
        destroy_=entry<PFN_vkDestroyBuffer>(get,device,"vkDestroyBuffer");
        free_=entry<PFN_vkFreeMemory>(get,device,"vkFreeMemory");
        unmap_=entry<PFN_vkUnmapMemory>(get,device,"vkUnmapMemory");
        const auto create=entry<PFN_vkCreateBuffer>(get,device,"vkCreateBuffer");
        const auto requirements=entry<PFN_vkGetBufferMemoryRequirements>(get,device,"vkGetBufferMemoryRequirements");
        const auto allocate=entry<PFN_vkAllocateMemory>(get,device,"vkAllocateMemory");
        const auto bind=entry<PFN_vkBindBufferMemory>(get,device,"vkBindBufferMemory");
        const auto map=entry<PFN_vkMapMemory>(get,device,"vkMapMemory");
        const auto flush=entry<PFN_vkFlushMappedMemoryRanges>(get,device,"vkFlushMappedMemoryRanges");
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=vertices.size_bytes();info.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(create(device,&info,nullptr,&buffer_),"Create vertex buffer");
        VkMemoryRequirements required{};requirements(device,buffer_,&required);
        if(required.size<info.size) throw std::runtime_error("Invalid vertex allocation size");
        uint32_t selected=VK_MAX_MEMORY_TYPES;
        for(uint32_t i=0;i<properties.memoryTypeCount;++i) {
            const auto flags=properties.memoryTypes[i].propertyFlags;
            if((required.memoryTypeBits&(uint32_t(1)<<i)) && (flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                selected=i;
                if(flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) break;
            }
        }
        if(selected==VK_MAX_MEMORY_TYPES) throw std::runtime_error("No host-visible vertex memory type");
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize=required.size;allocation.memoryTypeIndex=selected;
        check(allocate(device,&allocation,nullptr,&memory_),"Allocate vertex memory");
        check(bind(device,buffer_,memory_,0),"Bind vertex memory");
        void* destination{};
        check(map(device,memory_,0,VK_WHOLE_SIZE,0,&destination),"Map vertex memory");mapped_=true;
        if(!destination) throw std::runtime_error("Null mapped vertex memory");
        std::memcpy(destination,vertices.data(),vertices.size_bytes());
        if(!(properties.memoryTypes[selected].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
            range.memory=memory_;range.size=VK_WHOLE_SIZE;
            check(flush(device,1,&range),"Flush vertex memory");
        }
        unmap_(device,memory_);mapped_=false;
        count_=static_cast<uint32_t>(vertices.size());status_="Scene vertices uploaded";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
}
