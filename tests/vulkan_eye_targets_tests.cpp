#include "starfox/vr/vulkan_eye_targets.hpp"
#include "starfox/vr/vulkan_eye_commands.hpp"
#include "starfox/vr/vulkan_stereo_draw.hpp"
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include "starfox/vr/scene_material.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <source_location>
using namespace starfox::vr;
namespace {
void require(bool value,std::source_location location=std::source_location::current()) {
    if(!value) throw std::runtime_error("Eye target assertion failed at line "+std::to_string(location.line()));
}
template<class T> T handle(uintptr_t value) {return reinterpret_cast<T>(value);}
unsigned passes{},views{},frames{},destroyed_passes{},destroyed_views{},destroyed_frames{};
bool fail_frame{};
bool pending_fence=true;
bool inside_pass=false;
unsigned submissions{},idles{},pool_destroys{},fence_destroys{};
unsigned shader_destroys{},draws{};
VkPrimitiveTopology expected_topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
unsigned expected_vertex_count=3;
bool expected_depth_test=false,expected_depth_write=false;
std::array<unsigned char,1024> uploaded{};
unsigned flushes{},unmaps{},buffers_destroyed{},memory_freed{};
bool fail_flush{};
VKAPI_ATTR VkResult VKAPI_CALL create_buffer(VkDevice,const VkBufferCreateInfo* info,const VkAllocationCallbacks*,VkBuffer* out) {
    require(info->size==3*sizeof(SceneVertex)
        && info->usage==(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    *out=handle<VkBuffer>(1);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_buffer(VkDevice,VkBuffer,const VkAllocationCallbacks*) {++buffers_destroyed;}
VKAPI_ATTR void VKAPI_CALL requirements(VkDevice,VkBuffer,VkMemoryRequirements* out) {*out={1024,16,1};}
VKAPI_ATTR VkResult VKAPI_CALL allocate_memory(VkDevice,const VkMemoryAllocateInfo* info,const VkAllocationCallbacks*,VkDeviceMemory* out) {
    require(info->memoryTypeIndex==0 && info->allocationSize==1024);*out=handle<VkDeviceMemory>(1);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL free_memory(VkDevice,VkDeviceMemory,const VkAllocationCallbacks*) {++memory_freed;}
VKAPI_ATTR VkResult VKAPI_CALL bind_memory(VkDevice,VkBuffer,VkDeviceMemory,VkDeviceSize) {return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL map_memory(VkDevice,VkDeviceMemory,VkDeviceSize,VkDeviceSize size,VkMemoryMapFlags,void** out) {
    require(size==VK_WHOLE_SIZE);*out=uploaded.data();return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL unmap_memory(VkDevice,VkDeviceMemory) {++unmaps;}
VKAPI_ATTR VkResult VKAPI_CALL flush_memory(VkDevice,uint32_t count,const VkMappedMemoryRange* range) {
    require(count==1 && range->size==VK_WHOLE_SIZE && range->offset==0);++flushes;
    return fail_flush?VK_ERROR_MEMORY_MAP_FAILED:VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL create_shader(VkDevice,const VkShaderModuleCreateInfo* info,const VkAllocationCallbacks*,VkShaderModule* out) {
    require(info->codeSize>20 && info->pCode[0]==0x07230203);*out=handle<VkShaderModule>(1);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_shader(VkDevice,VkShaderModule,const VkAllocationCallbacks*) {++shader_destroys;}
VKAPI_ATTR VkResult VKAPI_CALL create_layout(VkDevice,const VkPipelineLayoutCreateInfo* info,const VkAllocationCallbacks*,VkPipelineLayout* out) {
    require(info->pushConstantRangeCount==1 && info->pPushConstantRanges[0].size==128);
    *out=handle<VkPipelineLayout>(1);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_layout(VkDevice,VkPipelineLayout,const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL create_pipeline(VkDevice,VkPipelineCache,uint32_t count,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks*,VkPipeline* out) {
    require(count==1 && info->stageCount==2 && info->pVertexInputState->vertexAttributeDescriptionCount==15);
    require(info->pDynamicState->dynamicStateCount==2 && info->pInputAssemblyState->topology==expected_topology);
    require(bool(info->pDepthStencilState->depthTestEnable)==expected_depth_test
        && bool(info->pDepthStencilState->depthWriteEnable)==expected_depth_write);
    *out=handle<VkPipeline>(1);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_pipeline(VkDevice,VkPipeline,const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL bind_pipeline(VkCommandBuffer,VkPipelineBindPoint,VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL bind_vertices(VkCommandBuffer,uint32_t,uint32_t,const VkBuffer*,const VkDeviceSize*) {}
VKAPI_ATTR void VKAPI_CALL push(VkCommandBuffer,VkPipelineLayout,VkShaderStageFlags,uint32_t,uint32_t size,const void*) {require(size==128);}
VKAPI_ATTR void VKAPI_CALL viewport(VkCommandBuffer,uint32_t,uint32_t,const VkViewport* view) {require(view->width==100 && view->height==200);}
VKAPI_ATTR void VKAPI_CALL scissor(VkCommandBuffer,uint32_t,uint32_t,const VkRect2D*) {}
VKAPI_ATTR void VKAPI_CALL draw_scene(VkCommandBuffer,uint32_t count,uint32_t instances,uint32_t,uint32_t) {require(count==expected_vertex_count && instances==1);++draws;}
VKAPI_ATTR VkResult VKAPI_CALL create_pool(VkDevice,const VkCommandPoolCreateInfo* info,const VkAllocationCallbacks*,VkCommandPool* out) {
    require(info->queueFamilyIndex==2);*out=handle<VkCommandPool>(1);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_pool(VkDevice,VkCommandPool,const VkAllocationCallbacks*) {++pool_destroys;}
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkCommandBufferAllocateInfo* info,VkCommandBuffer* out) {
    require(info->commandBufferCount==1);*out=handle<VkCommandBuffer>(1);return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL create_fence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out) {
    *out=handle<VkFence>(1);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_fence(VkDevice,VkFence,const VkAllocationCallbacks*) {++fence_destroys;}
VKAPI_ATTR VkResult VKAPI_CALL idle(VkQueue) {++idles;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL reset_pool(VkDevice,VkCommandPool,VkCommandPoolResetFlags) {inside_pass=false;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL reset_fences(VkDevice,uint32_t,const VkFence*) {return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL begin(VkCommandBuffer,const VkCommandBufferBeginInfo*) {return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL end(VkCommandBuffer) {return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL begin_pass(VkCommandBuffer,const VkRenderPassBeginInfo* info,VkSubpassContents) {
    require(!inside_pass);inside_pass=true;
    require(info->renderArea.extent.width==100 && info->clearValueCount==1);
}
VKAPI_ATTR void VKAPI_CALL end_pass(VkCommandBuffer) {require(inside_pass);inside_pass=false;}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue,uint32_t count,const VkSubmitInfo* info,VkFence fence) {
    if(info->waitSemaphoreCount) {
        require(info->waitSemaphoreCount==1 && *info->pWaitSemaphores==handle<VkSemaphore>(77)
            && *info->pWaitDstStageMask==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT && info->pNext);
        const auto* timeline=static_cast<const VkTimelineSemaphoreSubmitInfo*>(info->pNext);
        require(timeline->sType==VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO
            && timeline->waitSemaphoreValueCount==1 && *timeline->pWaitSemaphoreValues==42);
    } else require(!info->pNext);
    require(count==1 && info->commandBufferCount==1 && fence);++submissions;return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL fence_status(VkDevice,VkFence) {return pending_fence?VK_NOT_READY:VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL wait_fences(VkDevice,uint32_t,const VkFence*,VkBool32,uint64_t timeout) {
    require(timeout<=1000000);return pending_fence?VK_TIMEOUT:VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL pass(VkDevice,const VkRenderPassCreateInfo* info,const VkAllocationCallbacks*,VkRenderPass* out) {
    require(info->attachmentCount==1 && info->subpassCount==1);
    require(info->pAttachments[0].finalLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    require(info->pAttachments[0].loadOp==VK_ATTACHMENT_LOAD_OP_CLEAR);
    *out=handle<VkRenderPass>(++passes);return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL view(VkDevice,const VkImageViewCreateInfo* info,const VkAllocationCallbacks*,VkImageView* out) {
    require(info->viewType==VK_IMAGE_VIEW_TYPE_2D && info->subresourceRange.layerCount==1);
    *out=handle<VkImageView>(++views);return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL frame(VkDevice,const VkFramebufferCreateInfo* info,const VkAllocationCallbacks*,VkFramebuffer* out) {
    require(info->width==100 && info->height==200 && info->layers==1);
    if(fail_frame) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *out=handle<VkFramebuffer>(++frames);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_pass(VkDevice,VkRenderPass,const VkAllocationCallbacks*) {
    require(destroyed_views==views && destroyed_frames==frames);++destroyed_passes;
}
VKAPI_ATTR void VKAPI_CALL destroy_view(VkDevice,VkImageView,const VkAllocationCallbacks*) {
    require(destroyed_frames==frames);++destroyed_views;
}
VKAPI_ATTR void VKAPI_CALL destroy_frame(VkDevice,VkFramebuffer,const VkAllocationCallbacks*) {++destroyed_frames;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get(VkDevice,const char* name) {
#define ENTRY(n,f) if(std::strcmp(name,n)==0) return reinterpret_cast<PFN_vkVoidFunction>(f)
    ENTRY("vkCreateRenderPass",pass);ENTRY("vkCreateImageView",view);ENTRY("vkCreateFramebuffer",frame);
    ENTRY("vkDestroyRenderPass",destroy_pass);ENTRY("vkDestroyImageView",destroy_view);ENTRY("vkDestroyFramebuffer",destroy_frame);
    ENTRY("vkCreateCommandPool",create_pool);ENTRY("vkDestroyCommandPool",destroy_pool);
    ENTRY("vkAllocateCommandBuffers",allocate);ENTRY("vkCreateFence",create_fence);ENTRY("vkDestroyFence",destroy_fence);
    ENTRY("vkQueueWaitIdle",idle);ENTRY("vkResetCommandPool",reset_pool);ENTRY("vkResetFences",reset_fences);
    ENTRY("vkBeginCommandBuffer",begin);ENTRY("vkEndCommandBuffer",end);
    ENTRY("vkCmdBeginRenderPass",begin_pass);ENTRY("vkCmdEndRenderPass",end_pass);
    ENTRY("vkQueueSubmit",submit);ENTRY("vkGetFenceStatus",fence_status);
    ENTRY("vkWaitForFences",wait_fences);
    ENTRY("vkCreateShaderModule",create_shader);ENTRY("vkDestroyShaderModule",destroy_shader);
    ENTRY("vkCreatePipelineLayout",create_layout);ENTRY("vkDestroyPipelineLayout",destroy_layout);
    ENTRY("vkCreateGraphicsPipelines",create_pipeline);ENTRY("vkDestroyPipeline",destroy_pipeline);
    ENTRY("vkCmdBindPipeline",bind_pipeline);ENTRY("vkCmdBindVertexBuffers",bind_vertices);
    ENTRY("vkCmdPushConstants",push);ENTRY("vkCmdSetViewport",viewport);ENTRY("vkCmdSetScissor",scissor);ENTRY("vkCmdDraw",draw_scene);
    ENTRY("vkCreateBuffer",create_buffer);ENTRY("vkDestroyBuffer",destroy_buffer);ENTRY("vkGetBufferMemoryRequirements",requirements);
    ENTRY("vkAllocateMemory",allocate_memory);ENTRY("vkFreeMemory",free_memory);ENTRY("vkBindBufferMemory",bind_memory);
    ENTRY("vkMapMemory",map_memory);ENTRY("vkUnmapMemory",unmap_memory);ENTRY("vkFlushMappedMemoryRanges",flush_memory);
#undef ENTRY
    return nullptr;
}
}
int main(int argc,char** argv) try {
    if(argc==2 && std::string_view(argv[1])=="--cockpit-depth-only") {
        VulkanScenePipeline pipeline;
        require(pipeline.initialize(handle<VkDevice>(1),get,handle<VkRenderPass>(1)));
        expected_depth_test=true;expected_depth_write=false;
        require(pipeline.initialize(handle<VkDevice>(1),get,handle<VkRenderPass>(1),true,
            SceneTopology::triangles,VK_NULL_HANDLE,SceneBlend::opaque,nullptr,false));
        expected_depth_write=true;
        require(pipeline.initialize(handle<VkDevice>(1),get,handle<VkRenderPass>(1),true));
        std::cout<<"Cockpit HUD depth-read/no-write and unchanged default/world pipeline policy passed (mock Vulkan)\n";
        return 0;
    }
    require(argc==1);
    {
        SceneVertex vertex{};
        starfox::render::Palette256 palette{};palette[3]={128,64,255,128};palette[4]={255,0,0,255};
        starfox::render::FaceMaterial material{{3,4,true},nullptr};
        require(apply_scene_material(vertex,material,palette,0,2,false));
        require(vertex.color[0]==128/255.F && vertex.odd_color[0]==1 && vertex.dither_scale==2);
        require(apply_scene_material(vertex,material,palette,0,2,true));
        require(vertex.color[0]>.21F && vertex.color[0]<.22F && vertex.color[3]==128/255.F);
        require(!apply_scene_material(vertex,material,palette,0,0,false));
        starfox::assets::TextureImage texture;material.texture=&texture;
        require(!apply_scene_material(vertex,material,palette,0,1,false));
    }
    {
        VulkanSceneBuffer buffer;
        VkPhysicalDeviceMemoryProperties properties{};properties.memoryTypeCount=1;
        properties.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        std::array<SceneVertex,3> vertices{};vertices[0].position[0]=42;
        require(buffer.initialize(handle<VkDevice>(1),get,properties,vertices));
        require(buffer.count()==3 && flushes==1 && unmaps==1);
        require(std::memcmp(vertices.data(),uploaded.data(),sizeof(vertices))==0);
        buffer.close();buffer.close();require(buffers_destroyed==1 && memory_freed==1);
        properties.memoryTypes[0].propertyFlags|=VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        require(buffer.initialize(handle<VkDevice>(1),get,properties,vertices));
        require(flushes==1);
        properties.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;fail_flush=true;
        require(!buffer.initialize(handle<VkDevice>(1),get,properties,vertices));
        require(!buffer.buffer() && buffer.count()==0 && unmaps==3 && memory_freed==3);
        properties.memoryTypes[0].propertyFlags=0;
        require(!buffer.initialize(handle<VkDevice>(1),get,properties,vertices));
        require(buffers_destroyed==4 && memory_freed==3);
    }
    VulkanEyeTargets targets;
    const std::array<VkImage,2> images{handle<VkImage>(1),handle<VkImage>(2)};
    const std::array<std::span<const VkImage>,2> eyes{images,images};
    const std::array<VkExtent2D,2> sizes{{{100,200},{100,200}}};
    require(targets.initialize(handle<VkDevice>(1),get,VK_FORMAT_R8G8B8A8_SRGB,eyes,sizes));
    require(views==4 && frames==4 && passes==1 && targets.framebuffer(1,1));
    require(!targets.framebuffer(2,0) && !targets.framebuffer(0,2));
    {
        VulkanScenePipeline pipeline;
        require(pipeline.initialize(handle<VkDevice>(1),get,targets.render_pass()));
        require(shader_destroys==2);
        EyeCamera camera{};
        require(!pipeline.record(handle<VkCommandBuffer>(1),sizes[0],handle<VkBuffer>(1),2,camera));
        require(pipeline.record(handle<VkCommandBuffer>(1),sizes[0],handle<VkBuffer>(1),3,camera));
        require(draws==1);
        expected_topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;expected_vertex_count=2;
        expected_depth_test=true;expected_depth_write=false;
        require(pipeline.initialize(handle<VkDevice>(1),get,targets.render_pass(),true,
            SceneTopology::lines,VK_NULL_HANDLE,SceneBlend::opaque,nullptr,false));
        expected_depth_write=true;
        require(pipeline.initialize(handle<VkDevice>(1),get,targets.render_pass(),true,SceneTopology::lines));
        require(!pipeline.record(handle<VkCommandBuffer>(1),sizes[0],handle<VkBuffer>(1),3,camera));
        require(pipeline.record(handle<VkCommandBuffer>(1),sizes[0],handle<VkBuffer>(1),2,camera));
        expected_topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;expected_vertex_count=3;
    }
    {
        VulkanEyeCommands commands;
        require(commands.initialize(handle<VkDevice>(1),handle<VkQueue>(1),2,get));
        VkClearColorValue clear{};
        unsigned recorded=0,prepared=0;
        require(commands.submit(targets,0,1,clear,[&](VkCommandBuffer,VkExtent2D){require(prepared==1 && inside_pass);++recorded;},
            [&](VkCommandBuffer command,VkExtent2D extent){require(!inside_pass && recorded==0 && command && extent.width==100);++prepared;}));
        require(recorded==1 && submissions==1);
        require(commands.poll()==VulkanEyeCommands::Completion::pending);
        require(!commands.submit(targets,1,0,clear));
        pending_fence=false;require(commands.poll()==VulkanEyeCommands::Completion::complete);
        require(!commands.submit(targets,0,0,clear,[&](auto,auto){++recorded;},
            [](auto,auto){throw std::runtime_error("compute failed");}));
        require(recorded==1 && submissions==1);
        require(!commands.submit(targets,0,0,clear,[](auto,auto){throw std::runtime_error("draw failed");}));
        require(submissions==1);
        const VulkanEyeCommands::TimelineWait invalid{};
        require(!commands.submit(targets,1,0,clear,{},{},{},&invalid));
        const VulkanEyeCommands::TimelineWait wait{handle<VkSemaphore>(77),42};
        unsigned released=0;
        require(commands.submit(targets,1,0,clear,{},{},[&](auto,auto){require(!inside_pass);++released;},&wait));
        require(released==1);
        commands.close();commands.close();
        require(idles==1 && pool_destroys==1 && fence_destroys==1);
        require(commands.initialize(handle<VkDevice>(1),handle<VkQueue>(1),2,get));
        VulkanStereoDraw draw(commands,targets);
        EyeCamera camera{};
        pending_fence=true;
        const auto prior=submissions;
        unsigned stereo_prepared=0;
        unsigned stereo_released=0;
        const VulkanStereoDraw::Record prepare=[&](auto,auto,const auto&,XrTime time){require(time==123);++stereo_prepared;};
        const VulkanStereoDraw::Record release=[&](auto,auto,const auto&,XrTime time){require(time==123 && !inside_pass);++stereo_released;};
        // The live DXR path supplies an external timeline wait. Polling a pending
        // eye must neither resubmit it nor record another ownership release.
        require(draw.draw(0,0,camera,123,clear,{},prepare,release,&wait)==StereoRenderer::EyeResult::pending);
        require(draw.draw(0,0,camera,123,clear,{},prepare,release,&wait)==StereoRenderer::EyeResult::pending);
        require(stereo_prepared==1 && stereo_released==1);
        require(submissions==prior+1);
        pending_fence=false;
        require(draw.take_completion_timing().eyes==0);
        require(draw.draw(0,0,camera,123,clear,{},prepare,release,&wait)==StereoRenderer::EyeResult::complete);
        require(stereo_prepared==1 && stereo_released==1);
        require(submissions==prior+1);
        require(draw.draw(1,1,camera,123,clear)==StereoRenderer::EyeResult::complete);
        require(submissions==prior+2);
        const auto timing=draw.take_completion_timing();
        require(timing.eyes==2 && timing.total_ms>=timing.maximum_ms && timing.maximum_ms>=0);
        require(draw.take_completion_timing().eyes==0);
    }
    {
        VulkanEyeCommands unsupported;
        require(unsupported.initialize(handle<VkDevice>(1),handle<VkQueue>(1),2,get,
            VulkanEyeCommands::TimestampConfig{0,1000.}));
        require(!unsupported.gpu_timestamps_available()
            && unsupported.timestamp_status().find("timestampValidBits is zero")!=std::string::npos);
    }
    targets.close();targets.close();
    require(destroyed_passes==1 && destroyed_views==4 && destroyed_frames==4);
    fail_frame=true;
    require(!targets.initialize(handle<VkDevice>(1),get,VK_FORMAT_R8G8B8A8_SRGB,eyes,sizes));
    require(destroyed_passes==2 && destroyed_views==5 && destroyed_frames==4);
    require(!targets.render_pass() && !targets.framebuffer(0,0));
    require(!targets.initialize(handle<VkDevice>(1),nullptr,VK_FORMAT_R8G8B8A8_SRGB,eyes,sizes));
    std::cout<<"Vulkan eye target resource tests passed (injected driver).\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
