#pragma once
#include "starfox/vr/vulkan_eye_targets.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include "starfox/vr/vulkan_dxr_frame.hpp"
inline bool draw_imported_dxr(VkDevice device,VkPhysicalDevice physical,
    PFN_vkGetPhysicalDeviceMemoryProperties memory_properties,PFN_vkGetDeviceProcAddr get,
    uint32_t family,starfox::vr::VulkanExternalShadow& source,uint64_t value,
    uint32_t width,uint32_t height,std::vector<uint8_t>& rgba,bool composite=false,starfox::vr::VulkanDxrFrame* frame=nullptr) {
    using namespace starfox::vr;
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name))
    LOAD(vkCreateImage);LOAD(vkDestroyImage);LOAD(vkGetImageMemoryRequirements);
    LOAD(vkAllocateMemory);LOAD(vkFreeMemory);LOAD(vkBindImageMemory);
    LOAD(vkCmdBeginRenderPass);LOAD(vkCmdEndRenderPass);LOAD(vkCmdPipelineBarrier);LOAD(vkCmdCopyImageToBuffer);
#undef LOAD
    VkImage image{};VkDeviceMemory allocation{};
    VulkanEyeTargets targets;
    const bool result=[&] {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;
        info.format=VK_FORMAT_R8G8B8A8_UNORM;info.extent={width,height,1};info.mipLevels=info.arrayLayers=1;
        info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;
        info.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if(vkCreateImage(device,&info,nullptr,&image)!=VK_SUCCESS) return false;
        VkMemoryRequirements need{};vkGetImageMemoryRequirements(device,image,&need);
        VkPhysicalDeviceMemoryProperties properties{};memory_properties(physical,&properties);
        uint32_t type=0;while(type<properties.memoryTypeCount && !(need.memoryTypeBits&(1U<<type))) ++type;
        if(type==properties.memoryTypeCount) return false;
        VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};memory.allocationSize=need.size;memory.memoryTypeIndex=type;
        if(vkAllocateMemory(device,&memory,nullptr,&allocation)!=VK_SUCCESS || vkBindImageMemory(device,image,allocation,0)!=VK_SUCCESS) return false;
        const std::span<const VkImage> images(&image,1);
        const VkExtent2D extent{width,height};
        if(!targets.initialize(device,get,info.format,{images,images},{extent,extent})) return false;
        const auto blend=composite?SceneBlend::shadow:SceneBlend::opaque;
        const std::array<float,4> color{0,1,0,composite?.5F:1.F};
        if(source.prepare_draw(properties,targets.render_pass(),width,height+1,color,blend)) return false;
        for(unsigned repeat=0;repeat<2;++repeat)
            if(!source.prepare_draw(properties,targets.render_pass(),width,height,color,blend)) return false;
        if(source.prepare_draw(properties,targets.render_pass(),width,height,{1,0,0,1},SceneBlend::opaque)) return false;
        if(frame && (!composite || !frame->prepare_draw(properties,targets.render_pass(),color)
            || frame->draw_wait().semaphore!=source.ready() || frame->draw_wait().value!=value)) return false;
        return read_imported_dxr(device,physical,memory_properties,get,family,source.buffer(),source.ready(),value,
            uint64_t(width)*height*4,rgba,[&](VkCommandBuffer command,VkBuffer staging) {
                VkClearValue clear{};
                if(composite) {clear.color.float32[0]=64.F/255;clear.color.float32[1]=128.F/255;
                    clear.color.float32[2]=192.F/255;clear.color.float32[3]=1;}
                VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=targets.render_pass();
                begin.framebuffer=targets.framebuffer(0,0);begin.renderArea.extent=extent;begin.clearValueCount=1;begin.pClearValues=&clear;
                vkCmdBeginRenderPass(command,&begin,VK_SUBPASS_CONTENTS_INLINE);
                const bool drawn=frame?frame->record_draw(command,extent):source.record_draw(command,extent);
                vkCmdEndRenderPass(command);
                if(!drawn) return false;
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=image;
                barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};
                vkCmdCopyImageToBuffer(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,staging,1,&copy);
                return true;
            },VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,frame?std::function<bool(VkCommandBuffer,bool)>([&](VkCommandBuffer command,bool acquire) {
                return acquire?frame->record_acquire(command):frame->record_release(command);
            }):std::function<bool(VkCommandBuffer,bool)>{});
    }();
    targets.close();
    if(image) vkDestroyImage(device,image,nullptr);
    if(allocation) vkFreeMemory(device,allocation,nullptr);
    return result;
}
