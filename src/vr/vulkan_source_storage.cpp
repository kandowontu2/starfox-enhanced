#include "starfox/vr/vulkan_source_storage.hpp"
#include <cstring>
#include <stdexcept>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto fn=reinterpret_cast<T>(get(device,name));
    if(!fn) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return fn;
}
void check(VkResult r,const char* action) {
    if(r!=VK_SUCCESS) throw std::runtime_error(std::string(action)+": "+std::to_string(r));
}
}
VulkanSourceStorage::~VulkanSourceStorage(){close();}
void VulkanSourceStorage::close() noexcept {
    if(mapped_) unmap_(device_,memory_);
    if(buffer_) destroy_(device_,buffer_,nullptr);
    if(memory_) free_(device_,memory_,nullptr);
    device_={};buffer_={};memory_={};size_=0;mapped_=false;mapped_address_=nullptr;
}
bool VulkanSourceStorage::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& properties,VkDeviceSize size,VkBufferUsageFlags extra_usage) {
    close();
    try {
        if(!device || !get || !size || size>256ULL*1024*1024 || (size&3)
            || properties.memoryTypeCount>VK_MAX_MEMORY_TYPES
            || (extra_usage&~(VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT)))
            throw std::runtime_error("Invalid source storage allocation");
        device_=device;
        destroy_=entry<PFN_vkDestroyBuffer>(get,device,"vkDestroyBuffer");
        free_=entry<PFN_vkFreeMemory>(get,device,"vkFreeMemory");
        map_=entry<PFN_vkMapMemory>(get,device,"vkMapMemory");
        unmap_=entry<PFN_vkUnmapMemory>(get,device,"vkUnmapMemory");
        flush_=entry<PFN_vkFlushMappedMemoryRanges>(get,device,"vkFlushMappedMemoryRanges");
        invalidate_=entry<PFN_vkInvalidateMappedMemoryRanges>(get,device,"vkInvalidateMappedMemoryRanges");
        const auto create=entry<PFN_vkCreateBuffer>(get,device,"vkCreateBuffer");
        const auto requirements=entry<PFN_vkGetBufferMemoryRequirements>(get,device,"vkGetBufferMemoryRequirements");
        const auto allocate=entry<PFN_vkAllocateMemory>(get,device,"vkAllocateMemory");
        const auto bind=entry<PFN_vkBindBufferMemory>(get,device,"vkBindBufferMemory");
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=size;info.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|extra_usage;
        check(create(device,&info,nullptr,&buffer_),"Create source storage");
        VkMemoryRequirements required{};requirements(device,buffer_,&required);
        if(required.size<size) throw std::runtime_error("Invalid source allocation requirements");
        uint32_t selected=VK_MAX_MEMORY_TYPES;
        for(uint32_t i=0;i<properties.memoryTypeCount;++i) {
            const auto flags=properties.memoryTypes[i].propertyFlags;
            if((required.memoryTypeBits&(1U<<i)) && (flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                selected=i;if(flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) break;
            }
        }
        if(selected==VK_MAX_MEMORY_TYPES) throw std::runtime_error("No host-visible source storage memory");
        coherent_=(properties.memoryTypes[selected].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize=required.size;allocation.memoryTypeIndex=selected;
        check(allocate(device,&allocation,nullptr,&memory_),"Allocate source storage");
        check(bind(device,buffer_,memory_,0),"Bind source storage");
        // Keep the host-visible arena mapped across frames. Callers still
        // fence GPU access; mapping lifetime does not replace synchronization.
        check(map_(device_,memory_,0,VK_WHOLE_SIZE,0,&mapped_address_),"Map source storage");
        mapped_=true;
        if(!mapped_address_) throw std::runtime_error("Null source storage mapping");
        size_=size;status_="Source storage ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
bool VulkanSourceStorage::transfer(VkDeviceSize offset,void* data,size_t bytes,bool reading) {
    try {
        if(!buffer_ || !mapped_address_ || offset>size_ || bytes>size_-offset || (!data && bytes))
            throw std::runtime_error("Source storage transfer outside allocation");
        if(!bytes) return true;
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=memory_;range.size=VK_WHOLE_SIZE;
        if(reading && !coherent_) check(invalidate_(device_,1,&range),"Invalidate source storage");
        auto* address=static_cast<std::byte*>(mapped_address_)+offset;
        if(reading) std::memcpy(data,address,bytes);else std::memcpy(address,data,bytes);
        if(!reading && !coherent_) check(flush_(device_,1,&range),"Flush source storage");
        return true;
    } catch(const std::exception& e) {
        status_=e.what();return false;
    }
}
bool VulkanSourceStorage::upload(VkDeviceSize offset,std::span<const std::byte> data) {
    return transfer(offset,const_cast<std::byte*>(data.data()),data.size(),false);
}
bool VulkanSourceStorage::upload_many(std::span<const Write> writes) {
    try {
        if(!buffer_ || !memory_ || !mapped_address_) throw std::runtime_error("Source storage not initialized");
        for(const auto& write:writes)
            if(write.offset>size_ || write.bytes.size()>size_-write.offset)
                throw std::runtime_error("Source input write outside storage");
        if(writes.empty()) return true;
        for(const auto& write:writes) if(!write.bytes.empty())
            std::memcpy(static_cast<std::byte*>(mapped_address_)+write.offset,write.bytes.data(),write.bytes.size());
        if(!coherent_) {
            VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=memory_;range.size=VK_WHOLE_SIZE;
            check(flush_(device_,1,&range),"Flush source input writes");
        }
        return true;
    } catch(const std::exception& e) {
        status_=e.what();return false;
    }
}
bool VulkanSourceStorage::readback(VkDeviceSize offset,std::span<std::byte> data) {
    return transfer(offset,data.data(),data.size(),true);
}
}
