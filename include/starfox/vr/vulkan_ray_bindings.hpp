#pragma once
#include "starfox/vr/vulkan_span_pipeline.hpp"

namespace starfox::vr {
// Borrows all six buffers and the pipeline. Finish GPU use before destruction.
// Ranges: points, residuals, corners, triangle topology, output, settings.
class VulkanRayBindings {
public:
    ~VulkanRayBindings() {close();}
    VulkanRayBindings()=default;
    VulkanRayBindings(const VulkanRayBindings&)=delete;
    VulkanRayBindings& operator=(const VulkanRayBindings&)=delete;
    bool initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceLimits& limits,
        const VulkanSpanPipeline& pipeline,const std::array<VkDescriptorBufferInfo,6>& ranges,
        uint32_t triangles,uint32_t points,uint32_t corners) {
        if(device_ || !device || !get || pipeline.stage()!=SourceComputeStage::ray_expand
            || !triangles || triangles>65535U*64 || !points || !corners) return false;
        const std::array<uint64_t,6> minimum{uint64_t(points)*32,uint64_t(points)*32,
            uint64_t(corners)*16,uint64_t(triangles)*16,uint64_t(triangles)*48,64};
        for(unsigned i=0;i<6;++i) {
            const auto alignment=i==5?limits.minUniformBufferOffsetAlignment:limits.minStorageBufferOffsetAlignment;
            const auto maximum=i==5?limits.maxUniformBufferRange:limits.maxStorageBufferRange;
            if(!ranges[i].buffer || ranges[i].range<minimum[i] || ranges[i].range>maximum
                || (alignment && ranges[i].offset%alignment)) return false;
        }
        const auto create=reinterpret_cast<PFN_vkCreateDescriptorPool>(get(device,"vkCreateDescriptorPool"));
        const auto allocate=reinterpret_cast<PFN_vkAllocateDescriptorSets>(get(device,"vkAllocateDescriptorSets"));
        const auto update=reinterpret_cast<PFN_vkUpdateDescriptorSets>(get(device,"vkUpdateDescriptorSets"));
        destroy_=reinterpret_cast<PFN_vkDestroyDescriptorPool>(get(device,"vkDestroyDescriptorPool"));
        if(!create || !allocate || !update || !destroy_) return false;
        std::array<VkDescriptorSetLayout,3> layouts{pipeline.descriptor_layout(0),pipeline.descriptor_layout(1),pipeline.descriptor_layout(2)};
        for(auto layout:layouts) if(!layout) return false;
        device_=device;
        const std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,5},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1}}};
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};info.maxSets=3;info.poolSizeCount=2;info.pPoolSizes=sizes.data();
        if(create(device,&info,nullptr,&pool_)!=VK_SUCCESS) {close();return false;}
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocation.descriptorPool=pool_;
        allocation.descriptorSetCount=3;allocation.pSetLayouts=layouts.data();
        if(allocate(device,&allocation,sets_.data())!=VK_SUCCESS) {close();return false;}
        for(unsigned i=0;i<6;++i) {
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=sets_[i<4?0:i-3];
            write.dstBinding=i<4?i:0;write.descriptorCount=1;
            write.descriptorType=i==5?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo=&ranges[i];update(device,1,&write,0,nullptr);
        }
        pipeline_=&pipeline;count_=triangles;return true;
    }
    bool record(VkCommandBuffer command) const {return pipeline_ && pipeline_->record(command,sets_,count_);}
    void close() noexcept {
        if(pool_) destroy_(device_,pool_,nullptr);
        pool_={};device_={};sets_={};pipeline_=nullptr;count_=0;
    }
private:
    VkDevice device_{};VkDescriptorPool pool_{};std::array<VkDescriptorSet,3> sets_{};
    PFN_vkDestroyDescriptorPool destroy_{};
    const VulkanSpanPipeline* pipeline_{};uint32_t count_{};
};
}
