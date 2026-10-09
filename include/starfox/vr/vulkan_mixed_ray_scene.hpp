#pragma once
#include "starfox/vr/vulkan_compute_ray_scene.hpp"
#include "starfox/vr/vulkan_legacy_ray_geometry.hpp"
#include <map>

namespace starfox::vr {
// Ordinary compute + legacy triangle candidates and texture-alpha coverage.
// Procedural geometry and warped materials still require separate support.
// All borrowed scene/pipeline/output resources must outlive GPU consumption.
class VulkanMixedRayScene {
public:
    struct LegacyPart {uint32_t key,count;uint64_t offset;bool operator==(const LegacyPart&) const=default;};
    struct Plan {VulkanComputeRayScene::Plan compute;std::vector<LegacyPart> legacy;uint64_t bytes{};};
    static bool coverage(const SourceModelPackets& packets,const Plan& plan,SourceRayCoverage& output) {
        if(plan.bytes%48 || plan.bytes/48>4'000'000 || plan.compute.bytes>plan.bytes
            || packets.handles.size()!=packets.packets.size()) return false;
        SourceRayCoverage next;
        if(!VulkanComputeRayScene::coverage(packets.compute_models,plan.compute,next)) return false;
        next.triangles.resize(plan.bytes/48);
        uint64_t previous=plan.compute.bytes;
        for(const auto& part:plan.legacy) {
            if(part.count%3 || part.offset%48 || part.offset<previous || part.offset>plan.bytes
                || uint64_t(part.count)*16>plan.bytes-part.offset) return false;
            const ShapeBatch* batch=nullptr;
            for(size_t i=0;i<packets.handles.size();++i) if(packets.handles[i]==part.key) {
                if(batch) return false;batch=&packets.packets[i].geometry;
            }
            if(!batch || batch->vertex_view().size()!=part.count
                || batch->texels.size()+next.texels.size()>16'000'000) return false;
            const auto vertices=batch->vertex_view();
            const uint32_t texture_base=uint32_t(next.texels.size());
            next.texels.insert(next.texels.end(),batch->texels.begin(),batch->texels.end());
            // Both triangles of a sprite (and repeated faces) share one decode.
            // Keep this batch-local: equal offsets in different batches differ.
            std::map<std::array<uint32_t,3>,uint32_t> decoded;
            for(size_t triangle=0;triangle<part.count/3;++triangle) {
                const auto& first=vertices[triangle*3];
                render::shadows::DxrShadows::TriangleCoverage material;
                if(first.texture[3]&~0x28000007U) return false;
                material.flags=first.texture[3]&1;
                if(material.flags) {
                    const auto u=first.texture[1],v=first.texture[2];
                    if(u>4095 || v>4095 || (u&(u+1)) || (v&(v+1))) return false;
                    const uint64_t count=uint64_t(u+1)*(v+1),offset=first.texture[0];
                    const bool indexed=(first.texture[3]&536870912U)!=0;
                    if(offset+(indexed?256+(count+3)/4:count)>batch->texels.size()) return false;
                    material.offset=texture_base+first.texture[0];
                    if(indexed) {
                        const std::array<uint32_t,3> key{first.texture[0],u,v};
                        const auto found=decoded.find(key);
                        if(found!=decoded.end()) material.offset=found->second;
                        else {
                            if(next.texels.size()+count>16'000'000) return false;
                            material.offset=uint32_t(next.texels.size());
                            for(uint64_t pixel=0;pixel<count;++pixel) {
                                const auto index=(batch->texels[offset+256+pixel/4]>>((pixel%4)*8))&255;
                                next.texels.push_back(batch->texels[offset+index]);
                            }
                            decoded.emplace(key,material.offset);
                        }
                    }
                    material.u_mask=u;material.v_mask=v;
                }
                for(unsigned corner=0;corner<3;++corner) {
                    const auto& vertex=vertices[triangle*3+corner];
                    if(vertex.visibility_enabled==2 || !std::equal(std::begin(first.texture),std::end(first.texture),std::begin(vertex.texture))) return false;
                    if(material.flags) for(unsigned axis=0;axis<2;++axis) {
                        const auto uv=vertex.uv[axis];
                        if(!std::isfinite(uv) || std::abs(uv)>1e8f) return false;
                        material.uv[corner*2+axis]=uv;
                    }
                }
                next.triangles[part.offset/48+triangle]=material;
            }
            previous=part.offset+uint64_t(part.count)*16;
        }
        output=std::move(next);return true;
    }
    static bool plan(const SourceModelPackets& packets,const VulkanSourceScene& scene,uint64_t alignment,Plan& output) {
        Plan next;
        if(packets.handles.size()!=packets.packets.size() || !packets.pending.empty()
            || !VulkanComputeRayScene::plan(packets.compute_models,alignment,next.compute)) return false;
        next.bytes=next.compute.bytes;
        const auto step=std::lcm(std::max(uint64_t(1),alignment),uint64_t(48));
        for(size_t i=0;i<packets.packets.size();++i) {
            if(is_source_shadow_pass(packets.handles[i])) continue;
            if(packets.packets[i].geometry.vertex_view().empty()) continue;
            VulkanSourceScene::LegacyRaySource source;
            if(!scene.legacy_ray_source(packets.handles[i],source)) return false;
            const auto offset=((next.bytes+step-1)/step)*step;
            next.bytes=offset+uint64_t(source.geometry.count)*16;
            if(next.bytes>256ULL*1024*1024) return false;
            next.legacy.push_back({packets.handles[i],source.geometry.count,offset});
        }
        output=std::move(next);return true;
    }
    bool initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory,
        const VkPhysicalDeviceLimits& limits,const VulkanSpanPipeline& pipeline,const VulkanSourceScene& scene,
        VkDescriptorBufferInfo output,const Plan& plan,const EyeCamera& eye) {
        if(scene_ || !get || !plan.bytes || plan.bytes>256ULL*1024*1024 || plan.bytes%48
            || !output.buffer || output.offset%48 || output.range<plan.bytes || output.offset>UINT64_MAX-plan.bytes) return false;
        fill_=reinterpret_cast<PFN_vkCmdFillBuffer>(get(device,"vkCmdFillBuffer"));
        barrier_=reinterpret_cast<PFN_vkCmdPipelineBarrier>(get(device,"vkCmdPipelineBarrier"));
        if(!fill_ || !barrier_) return false;
        if(plan.compute.bytes && !compute_.initialize(device,get,memory,limits,pipeline,scene,
            {output.buffer,output.offset,plan.compute.bytes},plan.compute,eye)) return false;
        uint64_t previous=plan.compute.bytes;
        for(const auto& part:plan.legacy) {
            VulkanSourceScene::LegacyRaySource source;
            const uint64_t bytes=uint64_t(part.count)*16;
            if(!part.count || part.count%3 || part.offset%48 || part.offset<previous || part.offset>plan.bytes
                || bytes>plan.bytes-part.offset || !scene.legacy_ray_source(part.key,source) || source.geometry.count!=part.count) {close();return false;}
            auto geometry=std::make_unique<VulkanLegacyRayGeometry>();
            if(!geometry->initialize(device,get,memory,limits,pipeline,source.geometry,
                {output.buffer,output.offset+part.offset,bytes},eye)) {close();return false;}
            legacy_.push_back({part,source.generation,source.geometry.vertices,std::move(geometry)});previous=part.offset+bytes;
        }
        scene_=&scene;output_=output;bytes_=plan.bytes;has_compute_=plan.compute.bytes!=0;valid_=true;return true;
    }
    // Compatible frames update uniforms/generations without new descriptors.
    // Any failure disables recording until a successful refresh/reinitialize.
    bool refresh(const Plan& plan,const EyeCamera& eye) {
        valid_=false;
        if(!scene_ || plan.bytes!=bytes_ || (plan.compute.bytes!=0)!=has_compute_ || plan.legacy.size()!=legacy_.size()) return false;
        for(size_t i=0;i<legacy_.size();++i) {
            const auto& item=legacy_[i];VulkanSourceScene::LegacyRaySource source;
            if(plan.legacy[i]!=item.part || !scene_->legacy_ray_source(item.part.key,source)
                || source.geometry.count!=item.part.count || source.geometry.vertices.buffer!=item.vertices.buffer
                || source.geometry.vertices.offset!=item.vertices.offset || source.geometry.vertices.range!=item.vertices.range) return false;
        }
        if(has_compute_ && !compute_.refresh(plan.compute,eye)) return false;
        for(auto& item:legacy_) {
            VulkanSourceScene::LegacyRaySource source;
            if(!scene_->legacy_ray_source(item.part.key,source) || !item.geometry->update(source.geometry,eye)) return false;
            item.generation=source.generation;
        }
        valid_=true;return true;
    }
    // Outside render pass, after scene.record_compute. Output requires
    // TRANSFER_DST usage. Caller performs AS-consumer barriers/ownership handoff.
    // clear_output=false requires the producer to have cleared all padding.
    bool record(VkCommandBuffer command,bool clear_output=true) const {
        if(!scene_ || !valid_ || !command) return false;
        for(const auto& item:legacy_) {
            VulkanSourceScene::LegacyRaySource source;
            if(!scene_->legacy_ray_source(item.part.key,source) || source.generation!=item.generation) return false;
        }
        if(clear_output) fill_(command,output_.buffer,output_.offset,bytes_,0);
        VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        dependency.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        barrier_(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
        if(has_compute_ && !compute_.record(command,false)) return false;
        for(const auto& item:legacy_) if(!item.geometry->record(command)) return false;
        return true;
    }
    uint32_t vertex_count() const {return uint32_t(bytes_/16);}
    void close() {legacy_.clear();compute_.close();scene_=nullptr;output_={};bytes_=0;has_compute_=false;valid_=false;}
private:
    struct Item {LegacyPart part;uint64_t generation;VkDescriptorBufferInfo vertices;std::unique_ptr<VulkanLegacyRayGeometry> geometry;};
    VulkanComputeRayScene compute_;std::vector<Item> legacy_;
    const VulkanSourceScene* scene_{};VkDescriptorBufferInfo output_{};uint64_t bytes_{};bool has_compute_{};
    bool valid_{};
    PFN_vkCmdFillBuffer fill_{};PFN_vkCmdPipelineBarrier barrier_{};
};
// Source-packet lifetime cache shared by stereo eyes. The owner invalidates
// before replacing inputs and only after consumers finish borrowed materials.
class StereoRayPlan {
public:
    void invalidate() {valid_.reset();}
    bool prepare(const SourceModelPackets& packets,const VulkanSourceScene& scene,uint64_t alignment) {
        if(!valid_) {
            plan_={};materials_={};
            valid_=VulkanMixedRayScene::plan(packets,scene,alignment,plan_) && plan_.bytes
                && VulkanMixedRayScene::coverage(packets,plan_,materials_);
        }
        return *valid_;
    }
    const VulkanMixedRayScene::Plan& plan() const {return plan_;}
    const SourceRayCoverage& materials() const {return materials_;}
private:
    std::optional<bool> valid_;
    VulkanMixedRayScene::Plan plan_;
    SourceRayCoverage materials_;
};
}
