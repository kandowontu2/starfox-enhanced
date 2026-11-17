#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include "shaders/scene_spirv.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <chrono>
#include <iostream>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto fn=reinterpret_cast<T>(get(device,name));
    if(!fn) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return fn;
}
void check(VkResult value,const char* operation) {
    if(value!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": "+std::to_string(value));
}
}
namespace {bool reversed_depth{};const VulkanSceneFog* registered_fog{};const SceneRayBinding* registered_rays{};}
void set_scene_reversed_depth(bool enabled) noexcept {reversed_depth=enabled;}
bool scene_reversed_depth() noexcept {return reversed_depth;}
void set_scene_fog(const VulkanSceneFog* fog) noexcept {registered_fog=fog;}
const VulkanSceneFog* scene_fog() noexcept {return registered_fog;}
void set_scene_rays(const SceneRayBinding* rays) noexcept {registered_rays=rays;}
const SceneRayBinding* scene_rays() noexcept {return registered_rays;}
VulkanSceneFog::~VulkanSceneFog() {close();}
void VulkanSceneFog::close() noexcept {
    if(buffer_) destroy_buffer_(device_,buffer_,nullptr);
    if(memory_) free_(device_,memory_,nullptr); // Unmaps.
    if(pool_) destroy_pool_(device_,pool_,nullptr);
    if(layout_) destroy_layout_(device_,layout_,nullptr);
    if(empty_) destroy_layout_(device_,empty_,nullptr);
    buffer_={};memory_={};mapped_={};pool_={};set_={};layout_={};empty_={};device_={};
}
bool VulkanSceneFog::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory) {
    close();
    try {
        if(!device || !get) throw std::runtime_error("Missing scene fog device");
        device_=device;
        destroy_layout_=entry<PFN_vkDestroyDescriptorSetLayout>(get,device,"vkDestroyDescriptorSetLayout");
        destroy_pool_=entry<PFN_vkDestroyDescriptorPool>(get,device,"vkDestroyDescriptorPool");
        destroy_buffer_=entry<PFN_vkDestroyBuffer>(get,device,"vkDestroyBuffer");
        free_=entry<PFN_vkFreeMemory>(get,device,"vkFreeMemory");
        const auto create_set_layout=entry<PFN_vkCreateDescriptorSetLayout>(get,device,"vkCreateDescriptorSetLayout");
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount=1;layout.pBindings=&binding;
        check(create_set_layout(device,&layout,nullptr,&layout_),"Create scene fog layout");
        layout.bindingCount=0;layout.pBindings=nullptr;
        check(create_set_layout(device,&layout,nullptr,&empty_),"Create empty scene layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
        check(entry<PFN_vkCreateDescriptorPool>(get,device,"vkCreateDescriptorPool")(device,&pool,nullptr,&pool_),"Create scene fog pool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool=pool_;allocation.descriptorSetCount=1;allocation.pSetLayouts=&layout_;
        check(entry<PFN_vkAllocateDescriptorSets>(get,device,"vkAllocateDescriptorSets")(device,&allocation,&set_),"Allocate scene fog set");
        // colour, then start, end, near x 256, unused (std140: two float4).
        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer.size=8*sizeof(float);buffer.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        check(entry<PFN_vkCreateBuffer>(get,device,"vkCreateBuffer")(device,&buffer,nullptr,&buffer_),"Create scene fog buffer");
        VkMemoryRequirements required{};
        entry<PFN_vkGetBufferMemoryRequirements>(get,device,"vkGetBufferMemoryRequirements")(device,buffer_,&required);
        constexpr VkMemoryPropertyFlags host=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uint32_t type=memory.memoryTypeCount;
        for(uint32_t i=0;i<memory.memoryTypeCount;++i)
            if((required.memoryTypeBits&(1U<<i)) && (memory.memoryTypes[i].propertyFlags&host)==host) {type=i;break;}
        if(type==memory.memoryTypeCount) throw std::runtime_error("No host-visible memory for scene fog");
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.allocationSize=required.size;allocate.memoryTypeIndex=type;
        check(entry<PFN_vkAllocateMemory>(get,device,"vkAllocateMemory")(device,&allocate,nullptr,&memory_),"Allocate scene fog memory");
        check(entry<PFN_vkBindBufferMemory>(get,device,"vkBindBufferMemory")(device,buffer_,memory_,0),"Bind scene fog memory");
        void* mapped{};
        check(entry<PFN_vkMapMemory>(get,device,"vkMapMemory")(device,memory_,0,VK_WHOLE_SIZE,0,&mapped),"Map scene fog memory");
        mapped_=static_cast<float*>(mapped);
        VkDescriptorBufferInfo info{buffer_,0,VK_WHOLE_SIZE};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet=set_;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&info;
        entry<PFN_vkUpdateDescriptorSets>(get,device,"vkUpdateDescriptorSets")(device,1,&write,0,nullptr);
        clear();
        status_="Scene fog ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
void VulkanSceneFog::set(const std::array<float,4>& colour,float start,float end,float near_metres) noexcept {
    if(!mapped_) return;
    const float values[8]{colour[0],colour[1],colour[2],colour[3],start,end,near_metres*256.F,0};
    std::copy(std::begin(values),std::end(values),mapped_);
}
VulkanScenePipeline::~VulkanScenePipeline() {close();}
void VulkanScenePipeline::close() noexcept {
    if(pipeline_) destroy_pipeline_(device_,pipeline_,nullptr);
    if(layout_) destroy_layout_(device_,layout_,nullptr);
    pipeline_={};layout_={};device_={};fog_={};rays_={};
}
bool VulkanScenePipeline::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkRenderPass pass,bool depth_test,SceneTopology topology,VkDescriptorSetLayout textures,SceneBlend mode,VulkanPipelineCache* cache,bool depth_write,bool painter) {
    painter=painter && depth_test;
    close();
    std::array<VkShaderModule,2> modules{};
    PFN_vkDestroyShaderModule destroy_shader{};
    const auto clean_modules=[&] {for(auto module:modules) if(module) destroy_shader(device,module,nullptr);};
    try {
        if(!device || !get || !pass) throw std::runtime_error("Missing scene device/render pass");
        if(topology!=SceneTopology::triangles && topology!=SceneTopology::lines)
            throw std::runtime_error("Invalid scene topology");
        topology_=topology;
        textured_=textures!=VK_NULL_HANDLE;
        // MODERN on a headset: the fragment variants with the registered fog.
        const auto* fog=scene_fog();
        fog_=fog?fog->set():VK_NULL_HANDLE;
        if(fog && !fog_) throw std::runtime_error("Registered scene fog is not initialized");
        // Ray tracing (textured fragments only) builds on the fog variant.
        const auto* rays=fog_ && textured_?scene_rays():nullptr;
        rays_=rays?rays->set:VK_NULL_HANDLE;
        if(rays && (!rays_ || !rays->layout)) throw std::runtime_error("Registered scene rays are not initialized");
        if(textured_ || fog_) bind_descriptors_=entry<PFN_vkCmdBindDescriptorSets>(get,device,"vkCmdBindDescriptorSets");
        device_=device;
#define LOAD(member,type,name) member=entry<type>(get,device,name)
        LOAD(destroy_pipeline_,PFN_vkDestroyPipeline,"vkDestroyPipeline");
        LOAD(destroy_layout_,PFN_vkDestroyPipelineLayout,"vkDestroyPipelineLayout");
        LOAD(bind_pipeline_,PFN_vkCmdBindPipeline,"vkCmdBindPipeline");
        LOAD(bind_vertices_,PFN_vkCmdBindVertexBuffers,"vkCmdBindVertexBuffers");
        LOAD(push_,PFN_vkCmdPushConstants,"vkCmdPushConstants");
        LOAD(viewport_,PFN_vkCmdSetViewport,"vkCmdSetViewport");
        LOAD(scissor_,PFN_vkCmdSetScissor,"vkCmdSetScissor");
        LOAD(draw_,PFN_vkCmdDraw,"vkCmdDraw");
#undef LOAD
        destroy_shader=entry<PFN_vkDestroyShaderModule>(get,device,"vkDestroyShaderModule");
        auto create_shader=entry<PFN_vkCreateShaderModule>(get,device,"vkCreateShaderModule");
        auto create_layout=entry<PFN_vkCreatePipelineLayout>(get,device,"vkCreatePipelineLayout");
        auto create_pipeline=entry<PFN_vkCreateGraphicsPipelines>(get,device,"vkCreateGraphicsPipelines");
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=textured_?sizeof(shader::vertex_textured):sizeof(shader::vertex);
        module.pCode=textured_?shader::vertex_textured:shader::vertex;
        check(create_shader(device,&module,nullptr,&modules[0]),"Create vertex shader");
        module.codeSize=rays_?sizeof(shader::fragment_textured_rt)
            :fog_?(textured_?sizeof(shader::fragment_textured_fog):sizeof(shader::fragment_fog))
            :(textured_?sizeof(shader::fragment_textured):sizeof(shader::fragment));
        module.pCode=rays_?shader::fragment_textured_rt
            :fog_?(textured_?shader::fragment_textured_fog:shader::fragment_fog)
            :(textured_?shader::fragment_textured:shader::fragment);
        check(create_shader(device,&module,nullptr,&modules[1]),"Create fragment shader");
        VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(SceneConstants)};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;
        const std::array<VkDescriptorSetLayout,3> sets{textured_?textures:(fog?fog->empty_layout():VK_NULL_HANDLE),
            fog?fog->layout():VK_NULL_HANDLE,rays?rays->layout:VK_NULL_HANDLE};
        if(textured_ || fog_) {layout.setLayoutCount=rays_?3:fog_?2:1;layout.pSetLayouts=sets.data();}
        check(create_layout(device,&layout,nullptr,&layout_),"Create scene layout");
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(auto& stage:stages) stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=modules[0];stages[0].pName=textured_?"vertex_textured_main":"vertex_main";
        stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=modules[1];stages[1].pName=textured_?"fragment_textured_main":"fragment_main";
        VkVertexInputBindingDescription binding{0,sizeof(SceneVertex),VK_VERTEX_INPUT_RATE_VERTEX};
        std::array<VkVertexInputAttributeDescription,15> attributes{{
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,position)},
            {1,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(SceneVertex,color)},
            {2,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(SceneVertex,odd_color)},
            {3,0,VK_FORMAT_R32_UINT,offsetof(SceneVertex,dither_scale)},
            {4,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,visibility_a)},
            {5,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,visibility_b)},
            {6,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,visibility_c)},
            {7,0,VK_FORMAT_R32_UINT,offsetof(SceneVertex,visibility_enabled)},
            {8,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,group_a)},
            {9,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,group_b)},
            {10,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,group_c)},
            {11,0,VK_FORMAT_R32_UINT,offsetof(SceneVertex,group_enabled)},
            {12,0,VK_FORMAT_R32G32_SFLOAT,offsetof(SceneVertex,uv)},
            {13,0,VK_FORMAT_R32G32B32A32_UINT,offsetof(SceneVertex,texture)},
            {14,0,VK_FORMAT_R32G32_SFLOAT,offsetof(SceneVertex,billboard)}}};
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&binding;
        input.vertexAttributeDescriptionCount=static_cast<uint32_t>(attributes.size());input.pVertexAttributeDescriptions=attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology=topology==SceneTopology::lines?VK_PRIMITIVE_TOPOLOGY_LINE_LIST:VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo view{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        view.viewportCount=view.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable=depth_test;depth.depthWriteEnable=depth_test && depth_write;
        depth.depthCompareOp=painter?VK_COMPARE_OP_ALWAYS:scene_reversed_depth()?VK_COMPARE_OP_GREATER:VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{};attachment.colorWriteMask=0xf;
        if(mode!=SceneBlend::opaque) {
            attachment.blendEnable=VK_TRUE;
            attachment.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
            attachment.dstColorBlendFactor=(mode==SceneBlend::half_add || mode==SceneBlend::half_subtract)
                ?VK_BLEND_FACTOR_SRC_ALPHA:VK_BLEND_FACTOR_ONE;
            attachment.colorBlendOp=(mode==SceneBlend::subtract || mode==SceneBlend::half_subtract)
                ?VK_BLEND_OP_REVERSE_SUBTRACT:VK_BLEND_OP_ADD;
            attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
            attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;attachment.alphaBlendOp=VK_BLEND_OP_ADD;
            if(mode==SceneBlend::alpha) {
                attachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
                attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
            if(mode==SceneBlend::shadow) {
                // Coverage darkens the existing scene proportionally, retaining
                // destination alpha. Source RGB is deliberately irrelevant.
                attachment.srcColorBlendFactor=VK_BLEND_FACTOR_ZERO;
                attachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
        }
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount=1;blend.pAttachments=&attachment;
        const VkDynamicState dynamic_states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamic_states;
        VkGraphicsPipelineCreateInfo create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        create.stageCount=2;create.pStages=stages.data();create.pVertexInputState=&input;
        create.pInputAssemblyState=&assembly;create.pViewportState=&view;create.pRasterizationState=&raster;
        create.pMultisampleState=&samples;create.pColorBlendState=&blend;create.pDynamicState=&dynamic;
        create.pDepthStencilState=&depth;
        create.layout=layout_;create.renderPass=pass;
        const auto compile_start=std::chrono::steady_clock::now();
        check(create_pipeline(device,cache?cache->get():VK_NULL_HANDLE,1,&create,nullptr,&pipeline_),"Create scene pipeline");
        const auto compile_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-compile_start).count();
        if(compile_ms>50) std::cout<<"VR slow graphics pipeline: topology="<<int(topology)
            <<" depth="<<depth_test<<" blend="<<int(mode)<<" ms="<<compile_ms<<'\n';
        if(cache) cache->checkpoint();
        clean_modules();status_="Native Vulkan scene pipeline ready";return true;
    } catch(const std::exception& e) {clean_modules();status_=e.what();close();return false;}
}
bool VulkanScenePipeline::record(VkCommandBuffer command,VkExtent2D extent,VkBuffer vertices,
    uint32_t count,const EyeCamera& camera,VkDescriptorSet textures) const {
    return record_range(command,extent,vertices,count,0,count,camera,textures);
}
bool VulkanScenePipeline::record_range(VkCommandBuffer command,VkExtent2D extent,VkBuffer vertices,
    uint32_t total,uint32_t first,uint32_t count,const EyeCamera& camera,VkDescriptorSet textures) const {
    const uint32_t primitive=topology_==SceneTopology::lines?2:3;
    if(!pipeline_ || !command || !vertices || !count || first>total || count>total-first
        || first%primitive || count%primitive || !extent.width || !extent.height) return false;
    if(textured_ && !textures) return false;
    const VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1};
    const VkRect2D scissor{{0,0},extent};const VkDeviceSize offset=0;
    bind_pipeline_(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_);
    if(textured_) bind_descriptors_(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,0,1,&textures,0,nullptr);
    if(fog_) bind_descriptors_(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,1,1,&fog_,0,nullptr);
    if(rays_) bind_descriptors_(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,2,1,&rays_,0,nullptr);
    viewport_(command,0,1,&viewport);scissor_(command,0,1,&scissor);
    bind_vertices_(command,0,1,&vertices,&offset);
    const auto constants=scene_constants(camera);
    push_(command,layout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(constants),&constants);
    draw_(command,count,topology_==SceneTopology::lines?scene_line_instances(camera):1,first,0);return true;
}
bool VulkanScenePipeline::record_model(VkCommandBuffer command,VkExtent2D extent,VkBuffer vertices,
    uint32_t count,const EyeCamera& camera,const Matrix4& model,VkDescriptorSet textures) const {
    const auto combined=model_eye_camera(camera,model);
    return combined && record(command,extent,vertices,count,*combined,textures);
}
}
