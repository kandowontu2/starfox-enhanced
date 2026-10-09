#pragma once
#include "starfox/vr/vulkan_ray_geometry.hpp"
#include "starfox/vr/vulkan_source_scene.hpp"
#include "starfox/vr/source_ray_topology.hpp"
#include "starfox/vr/source_ray_coverage.hpp"
#include "starfox/vr/shadow_camera.hpp"
#include <numeric>

namespace starfox::vr {
// Assembles COMPUTE models only. Legacy geometry/material coverage must be
// supplied by the complete ray scene separately. Borrows scene, pipeline and
// output; finish GPU use before initialize/close or modifying those objects.
class VulkanComputeRayScene {
public:
    struct Part {
        uint32_t key{},points{},corners{},mode{};
        bool warp_candidates{};
        uint64_t offset{};
        std::vector<std::array<uint32_t,4>> topology;
    };
    struct Plan {std::vector<Part> parts;uint64_t bytes{};};
    static bool coverage(std::span<const SourceComputeModel> models,const Plan& plan,SourceRayCoverage& output) {
        if(plan.bytes%48 || plan.bytes/48>4'000'000) return false;
        SourceRayCoverage next;next.triangles.resize(plan.bytes/48);
        uint64_t previous=0;
        for(const auto& part:plan.parts) {
            if(part.offset%48 || part.offset<previous || part.offset>plan.bytes
                || part.topology.size()>(plan.bytes-part.offset)/48) return false;
            const SourceComputeModel* model=nullptr;
            for(const auto& candidate:models) if(candidate.key==part.key) {
                if(model) return false;model=&candidate;
            }
            if(!model || model->axis || (model->warp && !model->warp->textures.texels.empty())) return false;
            SourceRayCoverage local;
            if(!source_ray_coverage(model->model.faces,part.topology,local)
                || next.texels.size()+local.texels.size()>16'000'000) return false;
            for(size_t i=0;i<local.triangles.size();++i) {
                auto material=local.triangles[i];
                if(material.flags) material.offset+=uint32_t(next.texels.size());
                next.triangles[part.offset/48+i]=material;
            }
            next.texels.insert(next.texels.end(),local.texels.begin(),local.texels.end());
            previous=part.offset+part.topology.size()*48;
        }
        output=std::move(next);return true;
    }
    // Padding is whole degenerate triangles so DXR can consume one contiguous
    // triangle list. Vulkan descriptor offsets also obey device alignment.
    static bool plan(std::span<const SourceComputeModel> models,uint64_t alignment,Plan& output) {
        if(alignment>256ULL*1024*1024 || models.size()>4096) return false;
        const uint64_t step=std::lcm(std::max(uint64_t(1),alignment),uint64_t(48));
        Plan next;
        for(const auto& model:models) {
            if(is_source_shadow_pass(model.key)) continue; // flattened visual, not a physical caster
            if(model.axis || (model.warp && !model.warp->textures.texels.empty())) return false;
            for(const auto& previous:next.parts) if(previous.key==model.key) return false;
            Part part;part.key=model.key;part.points=model.model.projection_settings[0];
            part.warp_candidates=bool(model.warp);
            part.corners=uint32_t(model.model.faces.corners.size());part.mode=model.model.projection_settings[2];
            std::string error;
            if(!source_ray_topology(model.model.faces,part.points,part.topology,error)) return false;
            if(part.topology.empty()) continue;
            part.offset=((next.bytes+step-1)/step)*step;
            next.bytes=part.offset+part.topology.size()*48;
            if(next.bytes>256ULL*1024*1024) return false;
            next.parts.push_back(std::move(part));
        }
        output=std::move(next);return true;
    }
    bool initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory,
        const VkPhysicalDeviceLimits& limits,const VulkanSpanPipeline& pipeline,const VulkanSourceScene& source,
        VkDescriptorBufferInfo output,const Plan& plan,const EyeCamera& eye) {
        if(source_ || !device || !get || !output.buffer || !plan.bytes || plan.bytes>256ULL*1024*1024
            || plan.parts.empty() || plan.parts.size()>4096 || output.range<plan.bytes
            || output.offset>UINT64_MAX-plan.bytes
            || output.offset%48 || plan.bytes%48) return false;
        fill_=reinterpret_cast<PFN_vkCmdFillBuffer>(get(device,"vkCmdFillBuffer"));
        barrier_=reinterpret_cast<PFN_vkCmdPipelineBarrier>(get(device,"vkCmdPipelineBarrier"));
        if(!fill_ || !barrier_) return false;
        uint64_t previous_end=0;
        for(const auto& part:plan.parts) {
            VulkanSourceScene::RaySource input;
            if(!source.ray_source(part.key,input,part.warp_candidates)) {close();return false;}
            const auto transform=shadow_model_transform(eye,input.placement,input.units);
            const uint64_t bytes=part.topology.size()*48;
            if(!transform || part.offset<previous_end || part.offset>plan.bytes || bytes>plan.bytes-part.offset
                || part.offset%48 || part.topology.empty()) {close();return false;}
            auto geometry=std::make_unique<VulkanRayGeometry>();
            if(!geometry->initialize(device,get,memory,limits,pipeline,input.ranges,
                {output.buffer,output.offset+part.offset,bytes},part.topology,part.points,part.corners,part.mode,*transform)) {
                close();return false;
            }
            items_.push_back({part.key,input.generation,input.ranges,part,std::move(geometry)});
            previous_end=part.offset+bytes;
        }
        source_=&source;output_=output;bytes_=plan.bytes;valid_=true;return true;
    }
    // No allocation/descriptor rebuild on a compatible frame. Exact topology
    // comparison avoids treating a recycled object's same-sized mesh as equal.
    // A failed refresh disables recording until refresh or initialize succeeds.
    bool refresh(const Plan& plan,const EyeCamera& eye) {
        valid_=false;
        if(!source_ || plan.bytes!=bytes_ || plan.parts.size()!=items_.size()) return false;
        for(size_t i=0;i<items_.size();++i) {
            const auto& item=items_[i];const auto& part=plan.parts[i];const auto& old=item.part;
            if(part.key!=old.key || part.points!=old.points || part.corners!=old.corners || part.mode!=old.mode
                || part.offset!=old.offset || part.topology!=old.topology || part.warp_candidates!=old.warp_candidates) return false;
            VulkanSourceScene::RaySource input;
            if(!source_->ray_source(item.key,input,part.warp_candidates) || !shadow_model_transform(eye,input.placement,input.units)) return false;
            for(unsigned r=0;r<3;++r) if(input.ranges[r].buffer!=item.ranges[r].buffer
                || input.ranges[r].offset!=item.ranges[r].offset || input.ranges[r].range!=item.ranges[r].range) return false;
        }
        for(auto& item:items_) {
            VulkanSourceScene::RaySource input;
            if(!source_->ray_source(item.key,input,item.part.warp_candidates)) return false;
            const auto transform=shadow_model_transform(eye,input.placement,input.units);
            if(!transform || !item.geometry->update_transform(*transform)) return false;
            item.generation=input.generation;
        }
        valid_=true;return true;
    }
    // Must follow source.record_compute, outside a render pass. Output must
    // have TRANSFER_DST usage and external ownership already acquired. Caller
    // adds the barrier/ownership release for the eventual AS consumer.
    bool record(VkCommandBuffer command,bool clear_output=true) const {
        if(!source_ || !valid_ || !command) return false;
        for(const auto& item:items_) {
            VulkanSourceScene::RaySource current;
            if(!source_->ray_source(item.key,current,item.part.warp_candidates) || current.generation!=item.generation) return false;
        }
        if(clear_output) fill_(command,output_.buffer,output_.offset,bytes_,0);
        VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        dependency.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        barrier_(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
        for(const auto& item:items_) if(!item.geometry->record(command)) return false;
        return true;
    }
    uint32_t vertex_count() const {return uint32_t(bytes_/16);}
    void close() {items_.clear();source_=nullptr;output_={};bytes_=0;valid_=false;}
private:
    struct Item {uint32_t key;uint64_t generation;std::array<VkDescriptorBufferInfo,3> ranges;
        Part part;std::unique_ptr<VulkanRayGeometry> geometry;};
    std::vector<Item> items_;
    const VulkanSourceScene* source_{};VkDescriptorBufferInfo output_{};uint64_t bytes_{};
    bool valid_{};
    PFN_vkCmdFillBuffer fill_{};PFN_vkCmdPipelineBarrier barrier_{};
};
}
