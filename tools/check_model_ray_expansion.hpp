#pragma once
#include "starfox/vr/vulkan_ray_bindings.hpp"
#include "starfox/vr/vulkan_ray_geometry.hpp"
#include "starfox/vr/vulkan_source_model.hpp"
#include "starfox/vr/source_ray_topology.hpp"
#include <bit>
#include <limits>
inline bool check_model_ray_expansion(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& memory,const VkPhysicalDeviceLimits& limits,
    VkCommandBuffer command,VkQueue queue,starfox::vr::VulkanSourceModel& producer,
    const starfox::vr::SourceSpanModel& model,bool include_warp_candidates=false) {
    using namespace starfox::vr;
    std::array<VkDescriptorBufferInfo,3> source;
    if(!producer.ray_source_ranges(source,include_warp_candidates)) return false;
    std::vector<std::array<uint32_t,4>> topology;std::string error;
    if(!source_ray_topology(model.faces,model.projection_settings[0],topology,error) || topology.empty()) return false;
    VulkanSpanPipeline pipeline;
    if(!pipeline.initialize(device,get,SourceComputeStage::ray_expand)) return false;
    VulkanSourceStorage output;
    struct Settings {uint32_t count,points,corners,mode;std::array<float,4> rows[3];};
    const Settings settings{uint32_t(topology.size()),model.projection_settings[0],uint32_t(model.faces.corners.size()),
        model.projection_settings[2],{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    if(!output.initialize(device,get,memory,topology.size()*48)) return false;
    VulkanRayGeometry geometry;
    const VkDescriptorBufferInfo destination{output.buffer(),0,output.size()};
    if(!geometry.initialize(device,get,memory,limits,pipeline,source,destination,topology,
        settings.points,settings.corners,settings.mode)) return false;
    if(geometry.vertex_count()!=topology.size()*3 || geometry.initialize(device,get,memory,limits,pipeline,
        source,destination,topology,settings.points,settings.corners,settings.mode)) return false;
    auto invalid=VulkanRayGeometry::identity;invalid[0][0]=std::numeric_limits<float>::infinity();
    if(geometry.update_transform(invalid) || !geometry.update_transform(VulkanRayGeometry::identity)) return false;
    auto shifted=VulkanRayGeometry::identity;shifted[0][3]=17;shifted[1][3]=-3;shifted[2][3]=10;
    if(!geometry.update_transform(shifted) || !geometry.update_transform(shifted)
        || geometry.update_transform(invalid)) return false;
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name))
    LOAD(vkBeginCommandBuffer);LOAD(vkEndCommandBuffer);LOAD(vkCmdPipelineBarrier);LOAD(vkQueueSubmit);LOAD(vkQueueWaitIdle);
#undef LOAD
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if(vkBeginCommandBuffer(command,&begin)!=VK_SUCCESS) return false;
    VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
    if(!geometry.record(command)) return false;
    dependency.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&dependency,0,nullptr,0,nullptr);
    if(vkEndCommandBuffer(command)!=VK_SUCCESS) return false;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
    if(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE)!=VK_SUCCESS || vkQueueWaitIdle(queue)!=VK_SUCCESS) return false;
    // Diagnostic comparison only: these downloads are never expansion inputs.
    struct Point {std::array<float,4> camera,screen;};
    std::vector<Point> points(settings.points),residuals(settings.points);
    std::vector<std::array<float,4>> result(topology.size()*3);
    if(!producer.readback(SourceSpanRegion::points,std::as_writable_bytes(std::span(points)))
        || !producer.readback(SourceSpanRegion::residuals,std::as_writable_bytes(std::span(residuals)))
        || !output.readback(0,std::as_writable_bytes(std::span(result)))) return false;
    for(size_t t=0;t<topology.size();++t) for(unsigned c=0;c<3;++c) {
        const auto index=model.faces.corners[topology[t][c]][0];
        for(unsigned axis=0;axis<3;++axis) {
            float expected=points[index].camera[axis];
            if(settings.mode) {
                const auto& tail=residuals[index];
                if(tail.camera[3]==3) expected=float(std::bit_cast<double>(uint64_t(std::bit_cast<uint32_t>(tail.camera[axis]))
                    | (uint64_t(std::bit_cast<uint32_t>(tail.screen[axis]))<<32)));
                else expected+=tail.camera[axis];
            }
            if(result[t*3+c][axis]!=expected+shifted[axis][3]) return false;
        }
        if(result[t*3+c][3]!=1) return false;
    }
    return true;
}
