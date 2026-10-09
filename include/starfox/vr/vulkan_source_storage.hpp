#pragma once
#include <vulkan/vulkan.h>
#include <span>
#include <string>
#include <cstddef>
namespace starfox::vr {
// Storage/uniform arena shared by source compute and scene graphics. Caller
// must finish GPU use before upload, readback, close or reinitialization.
// readback is for diagnostics only; graphics can bind buffer() directly.
class VulkanSourceStorage {
public:
    ~VulkanSourceStorage();
    VulkanSourceStorage()=default;
    VulkanSourceStorage(const VulkanSourceStorage&)=delete;
    VulkanSourceStorage& operator=(const VulkanSourceStorage&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,VkDeviceSize,
        VkBufferUsageFlags extra_usage=0);
    bool upload(VkDeviceSize,std::span<const std::byte>);
    struct Write {VkDeviceSize offset{};std::span<const std::byte> bytes;};
    bool upload_many(std::span<const Write>);
    bool readback(VkDeviceSize,std::span<std::byte>);
    void close() noexcept;
    VkBuffer buffer() const noexcept {return buffer_;}
    VkDeviceSize size() const noexcept {return size_;}
    const std::string& status() const noexcept {return status_;}
private:
    bool transfer(VkDeviceSize,void*,size_t,bool);
    VkDevice device_{};VkBuffer buffer_{};VkDeviceMemory memory_{};
    VkDeviceSize size_{};bool coherent_{},mapped_{};
    void* mapped_address_{};
    PFN_vkDestroyBuffer destroy_{};PFN_vkFreeMemory free_{};
    PFN_vkMapMemory map_{};PFN_vkUnmapMemory unmap_{};
    PFN_vkFlushMappedMemoryRanges flush_{};PFN_vkInvalidateMappedMemoryRanges invalidate_{};
    std::string status_{"Source storage not initialized"};
};
}
