#include "starfox/vr/vulkan_source_bindings.hpp"
#include <stdexcept>
#include <algorithm>
#include <utility>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto fn=reinterpret_cast<T>(get(device,name));
    if(!fn) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return fn;
}
void check(VkResult r,const char* action) {
    if(r!=VK_SUCCESS) throw std::runtime_error(std::string(action)+": "+std::to_string(r));
}
}
VulkanSourceBindings::~VulkanSourceBindings(){close();}
VulkanAxisBindings::~VulkanAxisBindings(){close();}
void VulkanAxisBindings::close() noexcept {
    if(pool_) destroy_(device_,pool_,nullptr);
    pool_={};device_={};pipeline_=nullptr;sets_={};barrier_=nullptr;
}
bool VulkanAxisBindings::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkBuffer buffer,
    uint64_t bytes,const SourceSpanArenaLayout& source,const SourceAxisArenaLayout& axis,
    const SourceAxisInputs& input,const VulkanSpanPipeline& pipeline) {
    close();
    try {
        if(!device || !get || !buffer || !source.bytes || source.bytes>axis.bytes
            || axis.bytes>bytes || bytes>256ULL*1024*1024
            || pipeline.stage()!=SourceComputeStage::axis || !pipeline.descriptor_layout(0))
            throw std::runtime_error("Invalid axis arena or pipeline");
        SourceAxisArenaLayout minimum;
        if(!layout_source_axis_inputs(input,0,4,4,UINT64_MAX,UINT64_MAX,minimum,status_))
            throw std::runtime_error(status_);
        uint64_t previous=source.bytes;
        for(size_t i=0;i<axis.regions.size();++i) {
            const auto& r=axis.regions[i];
            if((r.offset&3U) || r.offset<previous || r.size<minimum.regions[i].size
                || r.offset>axis.bytes || r.size>axis.bytes-r.offset)
                throw std::runtime_error("Invalid axis descriptor range");
            previous=r.offset+r.size;
        }
        using S=SourceSpanRegion;using A=SourceAxisRegion;
        for(auto region:{S::points,S::residuals}) {
            const auto& r=source[region];
            if((r.offset&3U) || r.size<uint64_t(input.settings.point_count)*32
                || r.offset>source.bytes || r.size>source.bytes-r.offset)
                throw std::runtime_error("Axis projection descriptor is truncated");
        }
        device_=device;
        destroy_=entry<PFN_vkDestroyDescriptorPool>(get,device,"vkDestroyDescriptorPool");
        barrier_=entry<PFN_vkCmdPipelineBarrier>(get,device,"vkCmdPipelineBarrier");
        const auto create=entry<PFN_vkCreateDescriptorPool>(get,device,"vkCreateDescriptorPool");
        const auto allocate=entry<PFN_vkAllocateDescriptorSets>(get,device,"vkAllocateDescriptorSets");
        const auto update=entry<PFN_vkUpdateDescriptorSets>(get,device,"vkUpdateDescriptorSets");
        const std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,5},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1}}};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pi.maxSets=3;pi.poolSizeCount=2;pi.pPoolSizes=sizes.data();
        check(create(device,&pi,nullptr,&pool_),"Create axis descriptors");
        const std::array<VkDescriptorSetLayout,3> layouts{pipeline.descriptor_layout(0),pipeline.descriptor_layout(1),pipeline.descriptor_layout(2)};
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool=pool_;ai.descriptorSetCount=3;ai.pSetLayouts=layouts.data();
        check(allocate(device,&ai,sets_.data()),"Allocate axis descriptors");
        const std::array<SourceSpanRegionRange,6> ranges{source[S::points],axis[A::indices],source[S::residuals],axis[A::endpoints],axis[A::residuals],axis[A::settings]};
        std::array<VkDescriptorBufferInfo,6> buffers{};std::array<VkWriteDescriptorSet,6> writes{};
        for(unsigned i=0;i<6;++i) {
            buffers[i]={buffer,ranges[i].offset,ranges[i].size};
            auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet=sets_[i<3?0:i<5?1:2];w.dstBinding=i<3?i:i<5?i-3:0;
            w.descriptorCount=1;w.descriptorType=i==5?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.pBufferInfo=&buffers[i];
        }
        update(device,6,writes.data(),0,nullptr);pipeline_=&pipeline;
        status_="Axis descriptors ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
bool VulkanAxisBindings::record(VkCommandBuffer command) const {
    if(!pool_ || !pipeline_ || !barrier_ || !pipeline_->record(command,sets_,2)) return false;
    VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
    dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_HOST_READ_BIT;
    barrier_(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_HOST_BIT,
        0,1,&dependency,0,nullptr,0,nullptr);
    return true;
}
VulkanWarpBindings::~VulkanWarpBindings(){close();}
void VulkanWarpBindings::close() noexcept {
    if(pool_) destroy_(device_,pool_,nullptr);
    pool_={};device_={};capacity_=0;sets_={};pipelines_={};barrier_=nullptr;
}
bool VulkanWarpBindings::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkBuffer buffer,
    uint64_t bytes,const SourceSpanArenaLayout& source,const SourceWarpArenaLayout& warp,
    const SourceWarpInputs& input,const std::array<const VulkanSpanPipeline*,3>& pipelines) {
    close();
    try {
        if(!device || !get || !buffer || !source.bytes || warp.bytes>bytes || bytes>256ULL*1024*1024)
            throw std::runtime_error("Invalid combined warp arena");
        for(const auto& range:source.regions)
            if(!range.size || (range.offset&3) || range.offset>source.bytes || range.size>source.bytes-range.offset)
                throw std::runtime_error("Source descriptor outside warp prefix");
        for(const auto& range:warp.regions)
            if(range.offset<source.bytes) throw std::runtime_error("Warp descriptor overlaps source prefix");
        std::vector<SourceSpanInputWrite> checked;
        if(!source_warp_input_writes(input,warp,checked,status_)) throw std::runtime_error(status_);
        // In-bounds ranges alone do not prove the producer prefix can supply
        // the counts declared by the warp uniforms. Reject truncated inputs
        // before any descriptor allocation or GPU dispatch.
        const auto& settings=input.shading.settings;
        const std::array<std::pair<SourceSpanRegion,uint64_t>,6> required{{
            {SourceSpanRegion::order,uint64_t(settings.capacity)*4},
            {SourceSpanRegion::results,8},
            {SourceSpanRegion::polygons,uint64_t(settings.face_count)*16},
            {SourceSpanRegion::visibility,uint64_t(settings.visibility_count)*4},
            {SourceSpanRegion::corners,uint64_t(settings.corner_count)*16},
            {SourceSpanRegion::materials,uint64_t(settings.face_count)*96}}};
        for(const auto& [region,size]:required)
            if(source[region].size<std::max(uint64_t(4),size))
                throw std::runtime_error("Warp producer descriptor is smaller than its declared input count");
        for(size_t i=0;i<3;++i)
            if(!pipelines[i] || size_t(pipelines[i]->stage())!=size_t(SourceComputeStage::colour_warp)+i
                || !pipelines[i]->descriptor_layout(0)) throw std::runtime_error("Incorrect warp pipeline");
        device_=device;
        destroy_=entry<PFN_vkDestroyDescriptorPool>(get,device,"vkDestroyDescriptorPool");
        barrier_=entry<PFN_vkCmdPipelineBarrier>(get,device,"vkCmdPipelineBarrier");
        const auto create=entry<PFN_vkCreateDescriptorPool>(get,device,"vkCreateDescriptorPool");
        const auto allocate=entry<PFN_vkAllocateDescriptorSets>(get,device,"vkAllocateDescriptorSets");
        const auto update=entry<PFN_vkUpdateDescriptorSets>(get,device,"vkUpdateDescriptorSets");
        const std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,24},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3}}};
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.maxSets=9;info.poolSizeCount=2;info.pPoolSizes=sizes.data();
        check(create(device,&info,nullptr,&pool_),"Create warp descriptors");
        using S=SourceSpanRegion;using W=SourceWarpRegion;
        const std::array<std::array<SourceSpanRegionRange,12>,3> ranges{{
            {source[S::order],source[S::results],source[S::polygons],source[S::visibility],
                warp[W::descriptors],warp[W::result],warp[W::sequence_settings]},
            {warp[W::descriptors],source[S::order],warp[W::normals],warp[W::diffuse],warp[W::depth_colours],
                warp[W::texture_lookup],warp[W::decoded],warp[W::material_settings]},
            {source[S::order],warp[W::result],source[S::polygons],source[S::corners],source[S::materials],
                warp[W::decoded],warp[W::textures],warp[W::coordinates],warp[W::polygons],warp[W::corners],
                warp[W::materials],warp[W::expand_settings]}}};
        constexpr std::array<unsigned,3> counts{7,8,12},inputs{4,6,8};
        for(unsigned stage=0;stage<3;++stage) {
            std::array<VkDescriptorSetLayout,3> layouts{};
            for(unsigned set=0;set<3;++set) layouts[set]=pipelines[stage]->descriptor_layout(set);
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool=pool_;allocation.descriptorSetCount=3;allocation.pSetLayouts=layouts.data();
            check(allocate(device,&allocation,sets_[stage].data()),"Allocate warp descriptors");
            std::array<VkDescriptorBufferInfo,12> buffers{};std::array<VkWriteDescriptorSet,12> writes{};
            for(unsigned i=0;i<counts[stage];++i) {
                const auto& range=ranges[stage][i];const bool uniform=i+1==counts[stage];
                const unsigned set=uniform?2:i<inputs[stage]?0:1;
                buffers[i]={buffer,range.offset,range.size};
                auto& write=writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet=sets_[stage][set];write.dstBinding=uniform?0:set==0?i:i-inputs[stage];
                write.descriptorCount=1;write.descriptorType=uniform?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.pBufferInfo=&buffers[i];
            }
            update(device,counts[stage],writes.data(),0,nullptr);
        }
        pipelines_=pipelines;capacity_=input.shading.settings.capacity;
        status_="Warp descriptors ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
bool VulkanWarpBindings::record(VkCommandBuffer command) const {
    if(!command || !pool_ || !barrier_) return false;
    for(unsigned stage=0;stage<3;++stage) {
        if(!pipelines_[stage]->record(command,sets_[stage],stage==0?1:capacity_)) return false;
        VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
        dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_HOST_READ_BIT;
        barrier_(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                |VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_HOST_BIT,
            0,1,&dependency,0,nullptr,0,nullptr);
    }
    return true;
}
void VulkanSourceBindings::close() noexcept {
    if(pool_) destroy_(device_,pool_,nullptr);
    pool_={};device_={};sets_={};
}
bool VulkanSourceBindings::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkBuffer buffer,
    uint64_t bytes,const SourceSpanArenaLayout& arena,const std::array<const VulkanSpanPipeline*,5>& pipelines,bool unclipped,
    const SourceWarpArenaLayout* warp) {
    close();
    try {
        if(!device || !get || !buffer || !arena.bytes || arena.bytes>bytes || bytes>256ULL*1024*1024)
            throw std::runtime_error("Invalid source descriptor arena");
        for(const auto& range:arena.regions)
            if(!range.size || (range.offset&3) || (range.size&3) || range.offset>arena.bytes || range.size>arena.bytes-range.offset)
                throw std::runtime_error("Source descriptor outside arena");
        if(warp) {
            if(unclipped || warp->bytes>bytes) throw std::runtime_error("Invalid warp consumer arena");
            for(const auto& range:warp->regions)
                if(!range.size || (range.offset&3) || (range.size&3) || range.offset<arena.bytes
                    || range.offset>warp->bytes || range.size>warp->bytes-range.offset)
                    throw std::runtime_error("Warp consumer descriptor outside appended arena");
        }
        for(size_t i=unclipped?2:0;i<pipelines.size();++i)
            if(!pipelines[i] || static_cast<size_t>(pipelines[i]->stage())!=i || !pipelines[i]->descriptor_layout(0))
                throw std::runtime_error("Incorrect source descriptor pipeline");
        device_=device;
        destroy_=entry<PFN_vkDestroyDescriptorPool>(get,device,"vkDestroyDescriptorPool");
        const auto create=entry<PFN_vkCreateDescriptorPool>(get,device,"vkCreateDescriptorPool");
        const auto allocate=entry<PFN_vkAllocateDescriptorSets>(get,device,"vkAllocateDescriptorSets");
        const auto update=entry<PFN_vkUpdateDescriptorSets>(get,device,"vkUpdateDescriptorSets");
        const std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,26},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,5}}};
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.maxSets=15;info.poolSizeCount=2;info.pPoolSizes=sizes.data();
        check(create(device,&info,nullptr,&pool_),"Create source descriptors");
        using R=SourceSpanRegion;
        const std::array<std::array<R,8>,5> regions{{
            {R::clipped,R::materials,R::order,R::results,R::commands,R::masks,R::span_settings},
            {R::points,R::corners,R::polygons,R::visibility,R::projection_parameters,R::residuals,R::clipped,R::clip_settings},
            {R::vertices,R::poses,R::points,R::residuals,R::projection_settings},
            {R::points,R::triples,R::visibility,R::visibility_settings},
            {R::nodes,R::visibility,R::face_ids,R::trees,R::order,R::results,R::bsp_settings}}};
        constexpr std::array<unsigned,5> counts{7,8,5,4,7},inputs{4,6,2,2,4};
        for(unsigned stage=unclipped?2:0;stage<5;++stage) {
            std::array<VkDescriptorSetLayout,3> layouts{};
            for(unsigned set=0;set<3;++set) layouts[set]=pipelines[stage]->descriptor_layout(set);
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool=pool_;allocation.descriptorSetCount=3;allocation.pSetLayouts=layouts.data();
            check(allocate(device,&allocation,sets_[stage].data()),"Allocate source descriptor sets");
            std::array<VkDescriptorBufferInfo,8> buffers{};
            std::array<VkWriteDescriptorSet,8> writes{};
            for(unsigned i=0;i<counts[stage];++i) {
                auto range=arena[regions[stage][i]];
                if(warp) {
                    // Projection/visibility/BSP still address source faces.
                    // Only consumers after warp expansion address occurrences.
                    using W=SourceWarpRegion;
                    if(stage==0 && i==1) range=(*warp)[W::materials];
                    if(stage==0 && i==3) range=(*warp)[W::result];
                    if(stage==1 && i==1) range=(*warp)[W::corners];
                    if(stage==1 && i==2) range=(*warp)[W::polygons];
                }
                const bool uniform=i+1==counts[stage];
                const unsigned set=uniform?2:i<inputs[stage]?0:1;
                buffers[i]={buffer,range.offset,range.size};
                auto& write=writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet=sets_[stage][set];write.dstBinding=uniform?0:set==0?i:i-inputs[stage];
                write.descriptorCount=1;write.descriptorType=uniform?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.pBufferInfo=&buffers[i];
            }
            update(device,counts[stage],writes.data(),0,nullptr);
        }
        status_="Source descriptors ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
}
