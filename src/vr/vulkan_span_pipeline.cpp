#include "starfox/vr/vulkan_span_pipeline.hpp"
#include "../render/shaders/generated/spans_portable.hpp"
#include "../render/shaders/generated/clip_continuous_portable.hpp"
#include "../render/shaders/generated/continuous_portable.hpp"
#include "../render/shaders/generated/continuous_visibility_portable.hpp"
#include "../render/shaders/generated/bsp_portable.hpp"
#include "../render/shaders/generated/colour_warp_portable.hpp"
#include "../render/shaders/generated/warp_material_portable.hpp"
#include "../render/shaders/generated/warp_expand_portable.hpp"
#include "../render/shaders/generated/axis_portable.hpp"
#include "shaders/ray_expand_spirv.hpp"
#include <cstring>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <iostream>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto fn=reinterpret_cast<T>(get(device,name));
    if(!fn) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return fn;
}
void check(VkResult result,const char* action) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(action)+": "+std::to_string(result));
}
}
VulkanSpanPipeline::~VulkanSpanPipeline(){close();}
void VulkanSpanPipeline::close() noexcept {
    if(pipeline_) destroy_pipeline_(device_,pipeline_,nullptr);
    if(layout_) destroy_layout_(device_,layout_,nullptr);
    for(auto set:sets_) if(set) destroy_set_(device_,set,nullptr);
    pipeline_={};layout_={};sets_={};device_={};
}
bool VulkanSpanPipeline::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,SourceComputeStage stage,bool fast_compile,VkPipelineCache cache) {
    close();
    VkShaderModule module{};PFN_vkDestroyShaderModule destroy_module{};
    try {
        if(!device || !get) throw std::runtime_error("Invalid span pipeline device");
        if(stage!=SourceComputeStage::spans && stage!=SourceComputeStage::continuous_clip && stage!=SourceComputeStage::continuous_projection && stage!=SourceComputeStage::continuous_visibility && stage!=SourceComputeStage::bsp && stage!=SourceComputeStage::colour_warp && stage!=SourceComputeStage::warp_material && stage!=SourceComputeStage::warp_expand && stage!=SourceComputeStage::axis && stage!=SourceComputeStage::ray_expand)
            throw std::runtime_error("Invalid source compute stage");
        group_width_=stage==SourceComputeStage::continuous_projection || stage==SourceComputeStage::continuous_visibility?64:32;
        if(stage==SourceComputeStage::colour_warp) group_width_=1;
        if(stage==SourceComputeStage::warp_material) group_width_=64;
        if(stage==SourceComputeStage::axis) group_width_=2;
        if(stage==SourceComputeStage::ray_expand) group_width_=64;
        device_=device;
        destroy_pipeline_=entry<PFN_vkDestroyPipeline>(get,device,"vkDestroyPipeline");
        destroy_layout_=entry<PFN_vkDestroyPipelineLayout>(get,device,"vkDestroyPipelineLayout");
        destroy_set_=entry<PFN_vkDestroyDescriptorSetLayout>(get,device,"vkDestroyDescriptorSetLayout");
        destroy_module=entry<PFN_vkDestroyShaderModule>(get,device,"vkDestroyShaderModule");
        bind_=entry<PFN_vkCmdBindPipeline>(get,device,"vkCmdBindPipeline");
        bind_sets_=entry<PFN_vkCmdBindDescriptorSets>(get,device,"vkCmdBindDescriptorSets");
        dispatch_=entry<PFN_vkCmdDispatch>(get,device,"vkCmdDispatch");
        const auto create_set=entry<PFN_vkCreateDescriptorSetLayout>(get,device,"vkCreateDescriptorSetLayout");
        const auto create_layout=entry<PFN_vkCreatePipelineLayout>(get,device,"vkCreatePipelineLayout");
        const auto create_module=entry<PFN_vkCreateShaderModule>(get,device,"vkCreateShaderModule");
        const auto create_pipeline=entry<PFN_vkCreateComputePipelines>(get,device,"vkCreateComputePipelines");
        for(unsigned set=0;set<3;++set) {
            std::array<VkDescriptorSetLayoutBinding,8> bindings{};
            const unsigned count=stage==SourceComputeStage::ray_expand?(set==0?4:1):stage==SourceComputeStage::axis?(set==0?3:set==1?2:1):stage==SourceComputeStage::warp_expand?(set==0?8:set==1?3:1):(stage==SourceComputeStage::spans || stage==SourceComputeStage::bsp || stage==SourceComputeStage::colour_warp)?(set==0?4:set==1?2:1):
                (stage==SourceComputeStage::continuous_clip || stage==SourceComputeStage::warp_material)?(set==0?6:1):stage==SourceComputeStage::continuous_projection?(set<2?2:1):(set==0?2:1);
            for(unsigned i=0;i<count;++i) {
                bindings[i].binding=i;bindings[i].descriptorCount=1;
                bindings[i].descriptorType=set==2?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
            }
            VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            info.bindingCount=count;info.pBindings=bindings.data();
            check(create_set(device,&info,nullptr,&sets_[set]),"Create span descriptor layout");
        }
        VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout_info.setLayoutCount=3;layout_info.pSetLayouts=sets_.data();
        check(create_layout(device,&layout_info,nullptr,&layout_),"Create span pipeline layout");
        // Generated shader bytes need an explicitly word-aligned copy for Vulkan.
        static_assert(sizeof(render::spans_shader::spirv)%4==0);
        static_assert(sizeof(render::clip_continuous_shader::spirv)%4==0);
        static_assert(sizeof(render::continuous_shader::spirv)%4==0);
        static_assert(sizeof(render::continuous_visibility_shader::spirv)%4==0);
        static_assert(sizeof(render::bsp_shader::spirv)%4==0);
        const auto* bytes=stage==SourceComputeStage::warp_expand?render::warp_expand_shader::spirv:stage==SourceComputeStage::warp_material?render::warp_material_shader::spirv:stage==SourceComputeStage::colour_warp?render::colour_warp_shader::spirv:stage==SourceComputeStage::spans?render::spans_shader::spirv:
            stage==SourceComputeStage::continuous_clip?render::clip_continuous_shader::spirv:
            stage==SourceComputeStage::continuous_projection?render::continuous_shader::spirv:
            stage==SourceComputeStage::continuous_visibility?render::continuous_visibility_shader::spirv:render::bsp_shader::spirv;
        const auto size=stage==SourceComputeStage::warp_expand?sizeof(render::warp_expand_shader::spirv):stage==SourceComputeStage::warp_material?sizeof(render::warp_material_shader::spirv):stage==SourceComputeStage::colour_warp?sizeof(render::colour_warp_shader::spirv):stage==SourceComputeStage::spans?sizeof(render::spans_shader::spirv):
            stage==SourceComputeStage::continuous_clip?sizeof(render::clip_continuous_shader::spirv):
            stage==SourceComputeStage::continuous_projection?sizeof(render::continuous_shader::spirv):
            stage==SourceComputeStage::continuous_visibility?sizeof(render::continuous_visibility_shader::spirv):sizeof(render::bsp_shader::spirv);
        const auto shader_size=stage==SourceComputeStage::ray_expand?sizeof(ray_shader::spirv):stage==SourceComputeStage::axis?sizeof(render::axis_shader::spirv):size;
        const auto* shader_bytes=stage==SourceComputeStage::ray_expand?reinterpret_cast<const unsigned char*>(ray_shader::spirv):stage==SourceComputeStage::axis?render::axis_shader::spirv:bytes;
        static_assert(sizeof(render::axis_shader::spirv)%4==0);
        std::vector<uint32_t> words(shader_size/4);
        std::memcpy(words.data(),shader_bytes,shader_size);
        VkShaderModuleCreateInfo shader_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        shader_info.codeSize=words.size()*4;shader_info.pCode=words.data();
        check(create_module(device,&shader_info,nullptr,&module),"Create span compute shader");
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.flags=fast_compile?VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT:0;
        info.layout=layout_;info.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;info.stage.module=module;info.stage.pName="main";
        const auto compile_start=std::chrono::steady_clock::now();
        check(create_pipeline(device,cache,1,&info,nullptr,&pipeline_),"Create span compute pipeline");
        const auto compile_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-compile_start).count();
        if(compile_ms>50) std::cout<<"VR slow compute pipeline: stage="<<int(stage)<<" ms="<<compile_ms<<'\n';
        destroy_module(device,module,nullptr);module={};
        stage_=stage;status_="Native source span pipeline ready";return true;
    } catch(const std::exception& e) {
        if(module) destroy_module(device,module,nullptr);
        status_=e.what();close();return false;
    }
}
bool VulkanSpanPipeline::record(VkCommandBuffer command,const std::array<VkDescriptorSet,3>& sets,uint32_t count) const {
    // Stay within Vulkan's guaranteed 65535 X workgroups.
    if(!pipeline_ || !command || !count || count>65535U*group_width_) return false;
    if(stage_==SourceComputeStage::colour_warp && count!=1) return false;
    if(stage_==SourceComputeStage::axis && count!=2) return false;
    for(auto set:sets) if(!set) return false;
    bind_(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);
    bind_sets_(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,3,sets.data(),0,nullptr);
    dispatch_(command,(count+group_width_-1U)/group_width_,1,1);return true;
}
}
