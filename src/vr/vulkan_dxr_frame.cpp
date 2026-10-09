#include "starfox/vr/vulkan_dxr_frame.hpp"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace starfox::vr {
namespace {
void close_shared(void* handle) {
#if defined(_WIN32)
    if(handle) CloseHandle(handle);
#else
    (void)handle;
#endif
}
}
void VulkanDxrFrame::close() {
    producer_.close();output_.close();dxr_.reset();geometry_={};
    imported_resource_=nullptr;ready_value_=0;width_=height_=0;device_={};state_=State::idle;coverage_=nullptr;
}
bool VulkanDxrFrame::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkPhysicalDevice physical,
    PFN_vkGetPhysicalDeviceProperties2 properties,const std::array<uint8_t,8>& luid,
    VkQueue queue,uint32_t family,uint32_t vertices) {
    if(dxr_ || !device || !get || !physical || !properties || !queue) return false;
    dxr_=std::make_unique<render::shadows::DxrShadows>(luid);
    geometry_=dxr_->prepare_shared_geometry(vertices);
    auto resource=dxr_->export_geometry_handle();auto fence=dxr_->export_ready_fence_handle();
    const bool valid=geometry_.resource && producer_.initialize(device,get,physical,properties,luid,
        resource,fence,uint64_t(vertices)*16,queue,family);
    close_shared(resource);close_shared(fence);
    if(!valid) {close();return false;}
    device_=device;get_=get;physical_=physical;properties_=properties;ready_value_=geometry_.ready_value;
    barrier_=reinterpret_cast<PFN_vkCmdPipelineBarrier>(get(device,"vkCmdPipelineBarrier"));family_=family;queue_=queue;
    return true;
}
bool VulkanDxrFrame::resize_geometry(uint32_t vertices) {
    if(!dxr_ || state_!=State::idle || !vertices || vertices%3 || vertices>12'000'000) return false;
    if(vertices==geometry_.vertex_count) return true;
    const auto luid=geometry_.adapter_luid;
    producer_.close();
    geometry_=dxr_->prepare_shared_geometry(vertices);
    auto resource=dxr_->export_geometry_handle();auto fence=dxr_->export_ready_fence_handle();
    const bool valid=geometry_.resource && producer_.initialize(device_,get_,physical_,properties_,luid,
        resource,fence,uint64_t(vertices)*16,queue_,family_);
    close_shared(resource);close_shared(fence);
    if(!valid) {state_=State::error;return false;}
    ready_value_=geometry_.ready_value;
    return true;
}
bool VulkanDxrFrame::begin(render::shadows::Camera camera,render::shadows::Vec3 light,
    std::optional<render::shadows::ReceiverPlane> ground,const VulkanEyeCommands::Record& record,
    const render::shadows::DxrShadows::Coverage* coverage) {
    if(!dxr_ || state_!=State::idle || !camera.width || !camera.height || camera.width>16384 || camera.height>16384) return false;
    if(!producer_.submit(ready_value_,record)) return false;
    camera_=camera;light_=light;ground_=ground;coverage_=coverage;state_=State::producing;return true;
}
VulkanDxrFrame::State VulkanDxrFrame::poll() {
    if(state_!=State::producing) return state_;
    const auto completion=producer_.poll();
    if(completion==VulkanEyeCommands::Completion::pending) return state_;
    if(completion==VulkanEyeCommands::Completion::error
        || !dxr_->render_resident({},camera_,light_,ground_,&geometry_,coverage_,true,true)) return state_=State::error;
    coverage_=nullptr;
    auto fence=dxr_->export_ready_fence_handle();
    const auto result=dxr_->resident_output();
    if(!fence) return state_=State::error;
    bool valid=true;
    if(imported_resource_!=result.resource || width_!=result.width || height_!=result.height) {
        output_.close();auto resource=dxr_->export_resident_handle();
        valid=output_.initialize(device_,get_,physical_,properties_,result.adapter_luid,
            resource,fence,uint64_t(result.row_bytes)*result.height);
        close_shared(resource);
        if(valid) {imported_resource_=result.resource;width_=result.width;height_=result.height;}
    }
    close_shared(fence);
    if(!valid) return state_=State::error;
    ready_value_=result.ready_value;return state_=State::ready;
}
bool VulkanDxrFrame::record_acquire(VkCommandBuffer command) const {
    if(state_!=State::ready || !command || !barrier_) return false;
    VkBufferMemoryBarrier acquire{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    acquire.buffer=output_.buffer();acquire.size=VK_WHOLE_SIZE;
    acquire.srcQueueFamilyIndex=VK_QUEUE_FAMILY_EXTERNAL;acquire.dstQueueFamilyIndex=family_;
    acquire.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    barrier_(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,1,&acquire,0,nullptr);
    return true;
}
bool VulkanDxrFrame::record_release(VkCommandBuffer command) const {
    if(state_!=State::ready || !command || !barrier_) return false;
    VkBufferMemoryBarrier release{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    release.buffer=output_.buffer();release.size=VK_WHOLE_SIZE;
    release.srcQueueFamilyIndex=family_;release.dstQueueFamilyIndex=VK_QUEUE_FAMILY_EXTERNAL;
    release.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
    barrier_(command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,1,&release,0,nullptr);
    return true;
}
}
