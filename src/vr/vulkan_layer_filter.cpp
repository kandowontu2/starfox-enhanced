#include "starfox/vr/vulkan_layer_filter.hpp"
#include "starfox/vr/modern_frame.hpp"
#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include "shaders/scene_spirv.hpp"
#include "../modern/shaders/generated/layer_filter.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
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
uint32_t memory_type(const VkPhysicalDeviceMemoryProperties& memory,uint32_t bits,VkMemoryPropertyFlags flags) {
    for(uint32_t i=0;i<memory.memoryTypeCount;++i)
        if((bits&(1U<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags) return i;
    throw std::runtime_error("No compatible memory for the layer filter");
}
// Column-major a x b.
Matrix4 multiply(const Matrix4& a,const Matrix4& b) noexcept {
    Matrix4 result{};
    for(unsigned column=0;column<4;++column) for(unsigned row=0;row<4;++row) {
        float sum=0;
        for(unsigned k=0;k<4;++k) sum+=a[k*4+row]*b[column*4+k];
        result[column*4+row]=sum;
    }
    return result;
}
constexpr VkFormat unorm=VK_FORMAT_R8G8B8A8_UNORM,encoded=VK_FORMAT_R8G8B8A8_SRGB;
// The quad's push constants (hud_layer.hlsl HudLayerConstants).
struct QuadConstants {Matrix4 clip;std::array<float,4> extent,decode;};
static_assert(sizeof(QuadConstants)==96);
}
VulkanLayerFilter::~VulkanLayerFilter() {close();}
void VulkanLayerFilter::close_output() noexcept {
    for(auto release=release_output_.rbegin();release!=release_output_.rend();++release) (*release)();
    release_output_.clear();
    output_={};output_view_={};factor_=0;filtered_=false;
}
void VulkanLayerFilter::close() noexcept {
    close_output();
    sprites_.close();
    for(auto release=release_.rbegin();release!=release_.rend();++release) (*release)();
    release_.clear();
    layer_pass_={};framebuffer_={};source_={};source_target_={};source_read_={};
    for(auto& layout:compute_layouts_) layout={};
    for(auto& set:compute_sets_) set={};
    quad_layout_={};pool_={};quad_set_={};compute_layout_={};quad_pipeline_layout_={};compute_={};quad_={};
    sampler_={};settings_={};empty_={};settings_words_={};has_sprites_=false;device_={};
}
bool VulkanLayerFilter::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory,
    VkRenderPass eye_pass,bool srgb,VulkanPipelineCache* cache) {
    close();
    try {
        if(!device || !get || !eye_pass) throw std::runtime_error("Missing layer filter device");
        device_=device;get_=get;memory_=memory;srgb_=srgb;
#define LOAD(member,type,name) member=entry<type>(get,device,name)
        LOAD(begin_pass_,PFN_vkCmdBeginRenderPass,"vkCmdBeginRenderPass");
        LOAD(end_pass_,PFN_vkCmdEndRenderPass,"vkCmdEndRenderPass");
        LOAD(barrier_,PFN_vkCmdPipelineBarrier,"vkCmdPipelineBarrier");
        LOAD(bind_pipeline_,PFN_vkCmdBindPipeline,"vkCmdBindPipeline");
        LOAD(bind_sets_,PFN_vkCmdBindDescriptorSets,"vkCmdBindDescriptorSets");
        LOAD(dispatch_,PFN_vkCmdDispatch,"vkCmdDispatch");
        LOAD(push_,PFN_vkCmdPushConstants,"vkCmdPushConstants");
        LOAD(viewport_,PFN_vkCmdSetViewport,"vkCmdSetViewport");
        LOAD(scissor_,PFN_vkCmdSetScissor,"vkCmdSetScissor");
        LOAD(draw_,PFN_vkCmdDraw,"vkCmdDraw");
        LOAD(update_sets_,PFN_vkUpdateDescriptorSets,"vkUpdateDescriptorSets");
#undef LOAD
        const auto destroy_image=entry<PFN_vkDestroyImage>(get,device,"vkDestroyImage");
        const auto destroy_view=entry<PFN_vkDestroyImageView>(get,device,"vkDestroyImageView");
        const auto destroy_buffer=entry<PFN_vkDestroyBuffer>(get,device,"vkDestroyBuffer");
        const auto free_memory=entry<PFN_vkFreeMemory>(get,device,"vkFreeMemory");
        const auto pipeline_cache=cache?cache->get():VK_NULL_HANDLE;
        // The source layer: one texel per SNES pixel. On an sRGB eye its
        // pipelines write through an sRGB view, so the bytes are the encoded
        // colours the desktop filters; the filter reads them as UNORM.
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image.flags=srgb?VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT:0;
        image.imageType=VK_IMAGE_TYPE_2D;image.format=unorm;image.extent={canvas_width,canvas_height,1};
        image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;
        image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        check(entry<PFN_vkCreateImage>(get,device,"vkCreateImage")(device,&image,nullptr,&source_),"Create layer image");
        release_.push_back([=,image=source_] {destroy_image(device,image,nullptr);});
        VkMemoryRequirements required{};
        entry<PFN_vkGetImageMemoryRequirements>(get,device,"vkGetImageMemoryRequirements")(device,source_,&required);
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize=required.size;allocate.memoryTypeIndex=memory_type(memory,required.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory source_memory{};
        check(entry<PFN_vkAllocateMemory>(get,device,"vkAllocateMemory")(device,&allocate,nullptr,&source_memory),"Allocate layer image");
        release_.push_back([=] {free_memory(device,source_memory,nullptr);});
        check(entry<PFN_vkBindImageMemory>(get,device,"vkBindImageMemory")(device,source_,source_memory,0),"Bind layer image");
        const auto create_view=entry<PFN_vkCreateImageView>(get,device,"vkCreateImageView");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image=source_;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        view.format=srgb?encoded:unorm;
        check(create_view(device,&view,nullptr,&source_target_),"Create layer target view");
        release_.push_back([=,view=source_target_] {destroy_view(device,view,nullptr);});
        view.format=unorm;
        check(create_view(device,&view,nullptr,&source_read_),"Create layer read view");
        release_.push_back([=,view=source_read_] {destroy_view(device,view,nullptr);});
        // Cleared transparent; left ready for the filter's reads.
        VkAttachmentDescription attachment{};
        attachment.format=srgb?encoded:unorm;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference colour{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount=1;subpass.pColorAttachments=&colour;
        std::array<VkSubpassDependency,2> dependencies{};
        dependencies[0]={VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,0};
        dependencies[1]={0,VK_SUBPASS_EXTERNAL,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,0};
        VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        pass.attachmentCount=1;pass.pAttachments=&attachment;pass.subpassCount=1;pass.pSubpasses=&subpass;
        pass.dependencyCount=uint32_t(dependencies.size());pass.pDependencies=dependencies.data();
        check(entry<PFN_vkCreateRenderPass>(get,device,"vkCreateRenderPass")(device,&pass,nullptr,&layer_pass_),"Create layer pass");
        const auto destroy_pass=entry<PFN_vkDestroyRenderPass>(get,device,"vkDestroyRenderPass");
        release_.push_back([=,pass=layer_pass_] {destroy_pass(device,pass,nullptr);});
        VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer.renderPass=layer_pass_;framebuffer.attachmentCount=1;framebuffer.pAttachments=&source_target_;
        framebuffer.width=canvas_width;framebuffer.height=canvas_height;framebuffer.layers=1;
        check(entry<PFN_vkCreateFramebuffer>(get,device,"vkCreateFramebuffer")(device,&framebuffer,nullptr,&framebuffer_),"Create layer framebuffer");
        const auto destroy_framebuffer=entry<PFN_vkDestroyFramebuffer>(get,device,"vkDestroyFramebuffer");
        release_.push_back([=,target=framebuffer_] {destroy_framebuffer(device,target,nullptr);});
        sprites_.set_pipeline_cache(cache);
        // The filter's settings (layer_filter.hlsl Settings) and its unused
        // ScaleFX input.
        const auto create_buffer=entry<PFN_vkCreateBuffer>(get,device,"vkCreateBuffer");
        const auto buffer_requirements=entry<PFN_vkGetBufferMemoryRequirements>(get,device,"vkGetBufferMemoryRequirements");
        const auto bind_buffer=entry<PFN_vkBindBufferMemory>(get,device,"vkBindBufferMemory");
        const auto make_buffer=[&](VkBufferUsageFlags usage,VkBuffer& buffer,void** mapped) {
            VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=32;info.usage=usage;
            check(create_buffer(device,&info,nullptr,&buffer),"Create layer filter buffer");
            release_.push_back([=] {destroy_buffer(device,buffer,nullptr);});
            VkMemoryRequirements need{};buffer_requirements(device,buffer,&need);
            constexpr VkMemoryPropertyFlags host=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            VkMemoryAllocateInfo info_memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            info_memory.allocationSize=need.size;info_memory.memoryTypeIndex=memory_type(memory,need.memoryTypeBits,host);
            VkDeviceMemory buffer_memory{};
            check(entry<PFN_vkAllocateMemory>(get,device,"vkAllocateMemory")(device,&info_memory,nullptr,&buffer_memory),"Allocate layer filter buffer");
            release_.push_back([=] {free_memory(device,buffer_memory,nullptr);});
            check(bind_buffer(device,buffer,buffer_memory,0),"Bind layer filter buffer");
            if(mapped) check(entry<PFN_vkMapMemory>(get,device,"vkMapMemory")(device,buffer_memory,0,VK_WHOLE_SIZE,0,mapped),"Map layer filter settings");
        };
        void* mapped{};
        make_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,settings_,&mapped);
        settings_words_=static_cast<uint32_t*>(mapped);
        make_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,empty_,nullptr);
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check(entry<PFN_vkCreateSampler>(get,device,"vkCreateSampler")(device,&sampler,nullptr,&sampler_),"Create layer sampler");
        const auto destroy_sampler=entry<PFN_vkDestroySampler>(get,device,"vkDestroySampler");
        release_.push_back([=,sampler=sampler_] {destroy_sampler(device,sampler,nullptr);});
        // Descriptors: the filter's sets 0-2 (layer_filter.hlsl registers:
        // t0/t1 space0, u0 space1, b0 space2) and the quad's set 0.
        const auto create_set_layout=entry<PFN_vkCreateDescriptorSetLayout>(get,device,"vkCreateDescriptorSetLayout");
        const auto destroy_set_layout=entry<PFN_vkDestroyDescriptorSetLayout>(get,device,"vkDestroyDescriptorSetLayout");
        const auto set_layout=[&](std::initializer_list<VkDescriptorSetLayoutBinding> bindings,VkDescriptorSetLayout& layout) {
            const std::vector<VkDescriptorSetLayoutBinding> list(bindings);
            VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            info.bindingCount=uint32_t(list.size());info.pBindings=list.data();
            check(create_set_layout(device,&info,nullptr,&layout),"Create layer filter set layout");
            release_.push_back([=] {destroy_set_layout(device,layout,nullptr);});
        };
        constexpr auto compute=VK_SHADER_STAGE_COMPUTE_BIT;constexpr auto fragment=VK_SHADER_STAGE_FRAGMENT_BIT;
        set_layout({{0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,compute,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,compute,nullptr}},compute_layouts_[0]);
        set_layout({{0,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,compute,nullptr}},compute_layouts_[1]);
        set_layout({{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,compute,nullptr}},compute_layouts_[2]);
        set_layout({{0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,fragment,nullptr},{1,VK_DESCRIPTOR_TYPE_SAMPLER,1,fragment,nullptr}},quad_layout_);
        const std::array<VkDescriptorPoolSize,5> sizes{{{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_SAMPLER,1}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets=4;pool.poolSizeCount=uint32_t(sizes.size());pool.pPoolSizes=sizes.data();
        check(entry<PFN_vkCreateDescriptorPool>(get,device,"vkCreateDescriptorPool")(device,&pool,nullptr,&pool_),"Create layer filter pool");
        const auto destroy_pool=entry<PFN_vkDestroyDescriptorPool>(get,device,"vkDestroyDescriptorPool");
        release_.push_back([=,pool=pool_] {destroy_pool(device,pool,nullptr);});
        const std::array<VkDescriptorSetLayout,4> layouts{compute_layouts_[0],compute_layouts_[1],compute_layouts_[2],quad_layout_};
        std::array<VkDescriptorSet,4> sets{};
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool=pool_;allocation.descriptorSetCount=4;allocation.pSetLayouts=layouts.data();
        check(entry<PFN_vkAllocateDescriptorSets>(get,device,"vkAllocateDescriptorSets")(device,&allocation,sets.data()),"Allocate layer filter sets");
        for(unsigned i=0;i<3;++i) compute_sets_[i]=sets[i];
        quad_set_=sets[3];
        const VkDescriptorImageInfo source_info{VK_NULL_HANDLE,source_read_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkDescriptorBufferInfo empty_info{empty_,0,VK_WHOLE_SIZE},settings_info{settings_,0,VK_WHOLE_SIZE};
        const VkDescriptorImageInfo sampler_info{sampler_,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED};
        std::array<VkWriteDescriptorSet,4> writes{};
        for(auto& write:writes) {write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;write.descriptorCount=1;}
        writes[0].dstSet=compute_sets_[0];writes[0].dstBinding=0;writes[0].descriptorType=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;writes[0].pImageInfo=&source_info;
        writes[1].dstSet=compute_sets_[0];writes[1].dstBinding=1;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[1].pBufferInfo=&empty_info;
        writes[2].dstSet=compute_sets_[2];writes[2].dstBinding=0;writes[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[2].pBufferInfo=&settings_info;
        writes[3].dstSet=quad_set_;writes[3].dstBinding=1;writes[3].descriptorType=VK_DESCRIPTOR_TYPE_SAMPLER;writes[3].pImageInfo=&sampler_info;
        update_sets_(device,uint32_t(writes.size()),writes.data(),0,nullptr);
        // Pipelines: the flat renderer's filter compute (its SPIR-V, copied
        // to aligned words) and the HUD quad.
        const auto create_layout=entry<PFN_vkCreatePipelineLayout>(get,device,"vkCreatePipelineLayout");
        const auto destroy_layout=entry<PFN_vkDestroyPipelineLayout>(get,device,"vkDestroyPipelineLayout");
        const auto destroy_pipeline=entry<PFN_vkDestroyPipeline>(get,device,"vkDestroyPipeline");
        const auto create_shader=entry<PFN_vkCreateShaderModule>(get,device,"vkCreateShaderModule");
        const auto destroy_shader=entry<PFN_vkDestroyShaderModule>(get,device,"vkDestroyShaderModule");
        std::vector<VkShaderModule> modules;
        const auto module=[&](const void* code,std::size_t size) {
            std::vector<uint32_t> words((size+3)/4);std::memcpy(words.data(),code,size);
            VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=size;info.pCode=words.data();
            VkShaderModule result{};check(create_shader(device,&info,nullptr,&result),"Create layer filter shader");
            modules.push_back(result);return result;
        };
        try {
            VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layout.setLayoutCount=3;layout.pSetLayouts=compute_layouts_;
            check(create_layout(device,&layout,nullptr,&compute_layout_),"Create layer filter layout");
            release_.push_back([=,layout=compute_layout_] {destroy_layout(device,layout,nullptr);});
            namespace filter_code=modern::layer_filter_shader;
            VkComputePipelineCreateInfo compute_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            compute_info.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,
                module(filter_code::spirv_compute,sizeof(filter_code::spirv_compute)),"filter_main",nullptr};
            compute_info.layout=compute_layout_;
            check(entry<PFN_vkCreateComputePipelines>(get,device,"vkCreateComputePipelines")(device,pipeline_cache,1,&compute_info,nullptr,&compute_),
                "Create layer filter pipeline");
            release_.push_back([=,pipeline=compute_] {destroy_pipeline(device,pipeline,nullptr);});
            const VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(QuadConstants)};
            layout.setLayoutCount=1;layout.pSetLayouts=&quad_layout_;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;
            check(create_layout(device,&layout,nullptr,&quad_pipeline_layout_),"Create layer quad layout");
            release_.push_back([=,layout=quad_pipeline_layout_] {destroy_layout(device,layout,nullptr);});
            std::array<VkPipelineShaderStageCreateInfo,2> stages{};
            for(auto& stage:stages) stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=module(shader::hud_layer_vertex,sizeof(shader::hud_layer_vertex));
            stages[0].pName="hud_layer_vertex_main";
            stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=module(shader::hud_layer_fragment,sizeof(shader::hud_layer_fragment));
            stages[1].pName="hud_layer_fragment_main";
            VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewport.viewportCount=viewport.scissorCount=1;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
            VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            // A HUD layer: over the world, like the sprite layer it replaces.
            VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=0xf;blend.blendEnable=VK_TRUE;
            blend.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp=VK_BLEND_OP_ADD;
            blend.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.alphaBlendOp=VK_BLEND_OP_ADD;
            VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blending.attachmentCount=1;blending.pAttachments=&blend;
            const VkDynamicState dynamic_states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamic_states;
            VkGraphicsPipelineCreateInfo graphics{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            graphics.stageCount=2;graphics.pStages=stages.data();graphics.pVertexInputState=&input;
            graphics.pInputAssemblyState=&assembly;graphics.pViewportState=&viewport;graphics.pRasterizationState=&raster;
            graphics.pMultisampleState=&samples;graphics.pDepthStencilState=&depth;graphics.pColorBlendState=&blending;
            graphics.pDynamicState=&dynamic;graphics.layout=quad_pipeline_layout_;graphics.renderPass=eye_pass;
            check(entry<PFN_vkCreateGraphicsPipelines>(get,device,"vkCreateGraphicsPipelines")(device,pipeline_cache,1,&graphics,nullptr,&quad_),
                "Create layer quad pipeline");
            release_.push_back([=,pipeline=quad_] {destroy_pipeline(device,pipeline,nullptr);});
        } catch(...) {for(auto shader:modules) destroy_shader(device,shader,nullptr);throw;}
        for(auto shader:modules) destroy_shader(device,shader,nullptr);
        status_="Layer filter ready";return true;
    } catch(const std::exception& error) {status_=error.what();close();return false;}
}
bool VulkanLayerFilter::create_output(unsigned factor) {
    close_output();
    try {
        const auto device=device_;const auto get=get_;
        const auto destroy_image=entry<PFN_vkDestroyImage>(get,device,"vkDestroyImage");
        const auto destroy_view=entry<PFN_vkDestroyImageView>(get,device,"vkDestroyImageView");
        const auto free_memory=entry<PFN_vkFreeMemory>(get,device,"vkFreeMemory");
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image.imageType=VK_IMAGE_TYPE_2D;image.format=unorm;image.extent={canvas_width*factor,canvas_height*factor,1};
        image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;
        image.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        check(entry<PFN_vkCreateImage>(get,device,"vkCreateImage")(device,&image,nullptr,&output_),"Create filtered layer");
        release_output_.push_back([=,image=output_] {destroy_image(device,image,nullptr);});
        VkMemoryRequirements required{};
        entry<PFN_vkGetImageMemoryRequirements>(get,device,"vkGetImageMemoryRequirements")(device,output_,&required);
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize=required.size;allocate.memoryTypeIndex=memory_type(memory_,required.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory output_memory{};
        check(entry<PFN_vkAllocateMemory>(get,device,"vkAllocateMemory")(device,&allocate,nullptr,&output_memory),"Allocate filtered layer");
        release_output_.push_back([=] {free_memory(device,output_memory,nullptr);});
        check(entry<PFN_vkBindImageMemory>(get,device,"vkBindImageMemory")(device,output_,output_memory,0),"Bind filtered layer");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image=output_;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=unorm;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        check(entry<PFN_vkCreateImageView>(get,device,"vkCreateImageView")(device,&view,nullptr,&output_view_),"Create filtered layer view");
        release_output_.push_back([=,view=output_view_] {destroy_view(device,view,nullptr);});
        const VkDescriptorImageInfo storage{VK_NULL_HANDLE,output_view_,VK_IMAGE_LAYOUT_GENERAL};
        const VkDescriptorImageInfo sampled{VK_NULL_HANDLE,output_view_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        std::array<VkWriteDescriptorSet,2> writes{};
        for(auto& write:writes) {write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;write.descriptorCount=1;}
        writes[0].dstSet=compute_sets_[1];writes[0].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;writes[0].pImageInfo=&storage;
        writes[1].dstSet=quad_set_;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;writes[1].pImageInfo=&sampled;
        update_sets_(device,uint32_t(writes.size()),writes.data(),0,nullptr);
        // layer_filter.hlsl Settings: output size, factor, filter (2 = xBRZ), source size.
        const uint32_t settings[8]{canvas_width*factor,canvas_height*factor,factor,2,canvas_width,canvas_height,0,0};
        std::memcpy(settings_words_,settings,sizeof(settings));
        factor_=factor;return true;
    } catch(const std::exception& error) {status_=error.what();close_output();return false;}
}
bool VulkanLayerFilter::upload(std::span<const DrawPacket> sprites,const Matrix4& layer) {
    if(!device_) {status_="Layer filter not initialized";return false;}
    if(!sprites_.initialize(device_,get_,memory_,layer_pass_,sprites,{},false)) {status_=sprites_.status();return false;}
    has_sprites_=std::any_of(sprites.begin(),sprites.end(),[](const DrawPacket& packet) {
        return !packet.geometry.vertex_view().empty() || !packet.geometry.line_view().empty();
    });
    layer_=layer;
    // source_panel_camera at the layer's own vanishing point (the matrix's
    // translation over its scale), so canvas pixels land on layer texels.
    const float vanish_x=-layer[12]/layer[0],vanish_y=layer[13]/layer[0];
    layer_camera_=EyeCamera{identity_matrix,{2,0,0,0,0,-512.F/224,0,0,
        1.F-2.F*vanish_x/256.F,1.F-2.F*vanish_y/224.F,-1,-1,0,0,-.05F,0}};
    layer_camera_=reversed_depth_camera(layer_camera_);
    // One target pixel per source pixel, nearest art, resolved dither (as
    // modern::SdlFrameRenderer::record_filtered_layers draws its layers).
    layer_camera_.effects[3]=2U|(16U<<8);
    return true;
}
bool VulkanLayerFilter::filter(VkCommandBuffer command,unsigned factor) {
    filtered_=false;
    if(!device_ || !command) {status_="Layer filter not initialized";return false;}
    if(!has_sprites_) return true;
    factor=std::clamp(factor,2U,6U);
    if(factor!=factor_ && !create_output(factor)) return false;
    VkClearValue clear{};
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass=layer_pass_;begin.framebuffer=framebuffer_;begin.renderArea.extent={canvas_width,canvas_height};
    begin.clearValueCount=1;begin.pClearValues=&clear;
    begin_pass_(command,&begin,VK_SUBPASS_CONTENTS_INLINE);
    const bool drawn=sprites_.record(command,{canvas_width,canvas_height},layer_camera_);
    end_pass_(command);
    if(!drawn) {status_="Layer recording failed: "+sprites_.status();return false;}
    // The previous frame's eyes completed before this one began.
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask=0;barrier.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=output_;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    barrier_(command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    bind_pipeline_(command,VK_PIPELINE_BIND_POINT_COMPUTE,compute_);
    bind_sets_(command,VK_PIPELINE_BIND_POINT_COMPUTE,compute_layout_,0,3,compute_sets_,0,nullptr);
    dispatch_(command,(canvas_width*factor_+7)/8,(canvas_height*factor_+7)/8,1);
    // Every eye this frame samples it (later submissions on this queue).
    barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout=VK_IMAGE_LAYOUT_GENERAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier_(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    filtered_=true;
    return true;
}
bool VulkanLayerFilter::record(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera) const {
    if(!filtered_) return true;
    if(!command || !extent.width || !extent.height) return false;
    QuadConstants constants{multiply(camera.projection,multiply(camera.view,layer_)),
        {float(canvas_width),float(canvas_height),1,1},{srgb_?1.F:0.F,0,0,0}};
    const VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1};
    const VkRect2D scissor{{0,0},extent};
    bind_pipeline_(command,VK_PIPELINE_BIND_POINT_GRAPHICS,quad_);
    bind_sets_(command,VK_PIPELINE_BIND_POINT_GRAPHICS,quad_pipeline_layout_,0,1,&quad_set_,0,nullptr);
    viewport_(command,0,1,&viewport);scissor_(command,0,1,&scissor);
    push_(command,quad_pipeline_layout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(constants),&constants);
    draw_(command,6,1,0,0);
    return true;
}
}
