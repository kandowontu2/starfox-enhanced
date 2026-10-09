#pragma once
#include "starfox/vr/vulkan_external_shadow.hpp"
#include "starfox/vr/vulkan_eye_commands.hpp"

namespace starfox::vr {
// Shared D3D12 triangle-buffer producer. No staging allocation or readback.
// The D3D12 owner must finish work and return the buffer to COMMON before
// submit. Poll complete before tracing/reusing it. Device outlives this owner.
class VulkanRayProducer {
public:
    ~VulkanRayProducer() {close();}
    bool initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkPhysicalDevice physical,
        PFN_vkGetPhysicalDeviceProperties2 properties,const std::array<uint8_t,8>& luid,
        void* resource,void* fence,uint64_t bytes,VkQueue queue,uint32_t family) {
        if(bytes_ || !get || !bytes || bytes%48 || bytes>256ULL*1024*1024) return false;
        barrier_=reinterpret_cast<PFN_vkCmdPipelineBarrier>(get(device,"vkCmdPipelineBarrier"));
        fill_=reinterpret_cast<PFN_vkCmdFillBuffer>(get(device,"vkCmdFillBuffer"));
        if(!barrier_ || !fill_ || !import_.initialize(device,get,physical,properties,luid,resource,fence,bytes)) return false;
        if(!commands_.initialize(device,queue,family,get)) {import_.close();return false;}
        bytes_=bytes;family_=family;return true;
    }
    VkDescriptorBufferInfo output() const {return {import_.buffer(),0,bytes_};}
    bool submit(uint64_t ready_value,const VulkanEyeCommands::Record& record) {
        if(!bytes_ || !record || !ready_value) return false;
        VulkanEyeCommands::TimelineWait wait{import_.ready(),ready_value,
            VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
        return commands_.submit_work({},[&](VkCommandBuffer command,VkExtent2D extent) {
            VkBufferMemoryBarrier acquire{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            acquire.srcQueueFamilyIndex=VK_QUEUE_FAMILY_EXTERNAL;acquire.dstQueueFamilyIndex=family_;
            acquire.buffer=import_.buffer();acquire.size=bytes_;
            acquire.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT;
            barrier_(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,wait.stage,0,0,nullptr,1,&acquire,0,nullptr);
            fill_(command,import_.buffer(),0,bytes_,0); // Alignment gaps are degenerate triangles.
            VkMemoryBarrier cleared{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            cleared.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;cleared.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT;
            barrier_(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&cleared,0,nullptr,0,nullptr);
            record(command,extent);
            VkBufferMemoryBarrier release{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            release.srcQueueFamilyIndex=family_;release.dstQueueFamilyIndex=VK_QUEUE_FAMILY_EXTERNAL;
            release.buffer=import_.buffer();release.size=bytes_;
            release.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT;
            barrier_(command,wait.stage,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,1,&release,0,nullptr);
        },&wait);
    }
    VulkanEyeCommands::Completion poll() {return commands_.poll();}
    void close() {commands_.close();import_.close();bytes_=0;}
private:
    VulkanExternalShadow import_;
    VulkanEyeCommands commands_;
    PFN_vkCmdPipelineBarrier barrier_{};
    PFN_vkCmdFillBuffer fill_{};
    uint64_t bytes_{};
    uint32_t family_{};
};
}
