#pragma once
#include <functional>
// Diagnostic consumer only: GPU copy of imported memory, then host verification.
inline bool read_imported_dxr(VkDevice device,VkPhysicalDevice physical,
    PFN_vkGetPhysicalDeviceMemoryProperties memory_properties,PFN_vkGetDeviceProcAddr get,
    uint32_t family,VkBuffer source,VkSemaphore ready,uint64_t value,VkDeviceSize bytes,
    std::vector<uint8_t>& output,
    const std::function<bool(VkCommandBuffer,VkBuffer)>& draw={},
    VkPipelineStageFlags read_stage=VK_PIPELINE_STAGE_TRANSFER_BIT,VkAccessFlags access=0,
    const std::function<bool(VkCommandBuffer,bool)>& ownership={}) {
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name))
    LOAD(vkCreateBuffer);LOAD(vkDestroyBuffer);LOAD(vkGetBufferMemoryRequirements);
    LOAD(vkAllocateMemory);LOAD(vkFreeMemory);LOAD(vkBindBufferMemory);LOAD(vkMapMemory);LOAD(vkUnmapMemory);
    LOAD(vkCreateCommandPool);LOAD(vkDestroyCommandPool);LOAD(vkAllocateCommandBuffers);
    LOAD(vkBeginCommandBuffer);LOAD(vkEndCommandBuffer);LOAD(vkCmdPipelineBarrier);LOAD(vkCmdCopyBuffer);
    LOAD(vkGetDeviceQueue);LOAD(vkQueueSubmit);LOAD(vkCreateFence);LOAD(vkDestroyFence);LOAD(vkWaitForFences);LOAD(vkDeviceWaitIdle);
#undef LOAD
    VkBuffer staging{};VkDeviceMemory allocation{};VkCommandPool pool{};VkFence fence{};
    bool submitted=false,completed=false;
    const bool result=[&] {
        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=bytes;buffer.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if(vkCreateBuffer(device,&buffer,nullptr,&staging)!=VK_SUCCESS) return false;
        VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(device,staging,&requirements);
        VkPhysicalDeviceMemoryProperties properties{};memory_properties(physical,&properties);
        uint32_t type=0;
        const auto flags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        while(type<properties.memoryTypeCount && (!(requirements.memoryTypeBits&(1U<<type))
            || (properties.memoryTypes[type].propertyFlags&flags)!=flags)) ++type;
        if(type==properties.memoryTypeCount) return false;
        VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};memory.allocationSize=requirements.size;memory.memoryTypeIndex=type;
        if(vkAllocateMemory(device,&memory,nullptr,&allocation)!=VK_SUCCESS
            || vkBindBufferMemory(device,staging,allocation,0)!=VK_SUCCESS) return false;
        VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool_info.queueFamilyIndex=family;
        if(vkCreateCommandPool(device,&pool_info,nullptr,&pool)!=VK_SUCCESS) return false;
        VkCommandBufferAllocateInfo command_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        command_info.commandPool=pool;command_info.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;command_info.commandBufferCount=1;
        VkCommandBuffer command{};
        if(vkAllocateCommandBuffers(device,&command_info,&command)!=VK_SUCCESS) return false;
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(vkBeginCommandBuffer(command,&begin)!=VK_SUCCESS) return false;
        VkBufferMemoryBarrier acquire{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        acquire.buffer=source;acquire.size=VK_WHOLE_SIZE;acquire.srcQueueFamilyIndex=VK_QUEUE_FAMILY_EXTERNAL;acquire.dstQueueFamilyIndex=family;
        acquire.dstAccessMask=access?access:draw?VK_ACCESS_SHADER_READ_BIT:VK_ACCESS_TRANSFER_READ_BIT;
        if(ownership) {if(!ownership(command,true)) return false;}
        else vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,read_stage,0,0,nullptr,1,&acquire,0,nullptr);
        if(draw) {if(!draw(command,staging)) return false;}
        else {VkBufferCopy copy{0,0,bytes};vkCmdCopyBuffer(command,source,staging,1,&copy);}
        auto release=acquire;release.srcQueueFamilyIndex=family;release.dstQueueFamilyIndex=VK_QUEUE_FAMILY_EXTERNAL;
        release.srcAccessMask=acquire.dstAccessMask;release.dstAccessMask=0;
        if(ownership) {if(!ownership(command,false)) return false;}
        else vkCmdPipelineBarrier(command,read_stage,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,1,&release,0,nullptr);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=staging;host.size=bytes;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        if(vkEndCommandBuffer(command)!=VK_SUCCESS) return false;
        VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if(vkCreateFence(device,&fence_info,nullptr,&fence)!=VK_SUCCESS) return false;
        VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};timeline.waitSemaphoreValueCount=1;timeline.pWaitSemaphoreValues=&value;
        const VkPipelineStageFlags stage=read_stage;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.pNext=&timeline;submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&ready;
        submit.pWaitDstStageMask=&stage;submit.commandBufferCount=1;submit.pCommandBuffers=&command;
        VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
        if(vkQueueSubmit(queue,1,&submit,fence)!=VK_SUCCESS) return false;
        submitted=true;
        if(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL)!=VK_SUCCESS) return false;
        completed=true;
        void* mapped{};
        if(vkMapMemory(device,allocation,0,bytes,0,&mapped)!=VK_SUCCESS) return false;
        output.assign(static_cast<const uint8_t*>(mapped),static_cast<const uint8_t*>(mapped)+bytes);
        vkUnmapMemory(device,allocation);return true;
    }();
    if(submitted && !completed) vkDeviceWaitIdle(device);
    if(fence) vkDestroyFence(device,fence,nullptr);
    if(pool) vkDestroyCommandPool(device,pool,nullptr);
    if(staging) vkDestroyBuffer(device,staging,nullptr);
    if(allocation) vkFreeMemory(device,allocation,nullptr);
    return result;
}
