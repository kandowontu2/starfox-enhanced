#include "starfox/vr/vulkan_source_model.hpp"
#include <cmath>
#include <algorithm>
namespace starfox::vr {
void VulkanSourceModel::close() noexcept {
    graphics_pipelines_.reset();line_vertices_.close();has_lines_=false;
    graphics_vertices_.close();graphics_textures_.close();
    device_=VK_NULL_HANDLE;get_=nullptr;memory_={};
    inputs_.clear();input_scratch_.clear();upload_views_.clear();
    updated_input_bytes_=0;
    compute_dirty_=true;compute_recorded_=false;
    warp_bindings_.close();warp_layout_={};warp_=false;
    axis_bindings_.close();axis_layout_={};axis_=false;
    bindings_.close();storage_.close();layout_={};pipelines_={};counts_={};barrier_=nullptr;
}
bool VulkanSourceModel::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& memory,const VkPhysicalDeviceLimits& limits,
    const SourceSpanModel& model,const std::array<const VulkanSpanPipeline*,5>& pipelines,
    const SourceWarpInputs* warp,const std::array<const VulkanSpanPipeline*,3>* warp_pipelines,
    const SourceAxisInputs* axis,const VulkanSpanPipeline* axis_pipeline) {
    close();
    const auto fail=[&](const std::string& reason){status_=reason;close();return false;};
    if(!device || !get) return fail("Invalid source model device");
    if(bool(axis)!=bool(axis_pipeline) || (axis && axis->settings.point_count!=model.projection_settings[0]))
        return fail("Axis model requires matching projection and pipeline");
    if(model.warp_expanded!=bool(warp) || bool(warp)!=bool(warp_pipelines))
        return fail("Expanded warp model requires matching resident warp inputs/pipelines");
    counts_={model.spans.count,model.spans.polygon_count,model.projection_settings[0],model.visibility_settings[0],1};
    if(counts_[2]!=model.projection.continuous_vertices.size() || counts_[3]!=model.projection.visibility_faces.size()
        || model.projection_settings[1]!=model.poses.size()) return fail("Inconsistent source model dispatch counts");
    for(size_t i=0;i<counts_.size();++i) {
        const uint32_t lanes=i==2 || i==3?64:32;
        const uint64_t groups=(uint64_t(counts_[i])+lanes-1)/lanes;
        if(!counts_[i] || groups>limits.maxComputeWorkGroupCount[0] || groups>65535)
            return fail("Source model exceeds dispatch limits");
    }
    if(!layout_source_span_model(model,limits.minStorageBufferOffsetAlignment,limits.minUniformBufferOffsetAlignment,
        limits.maxStorageBufferRange,limits.maxUniformBufferRange,layout_,status_)) {close();return false;}
    std::vector<std::byte> image;
    if(!source_span_upload_image(model,layout_,image,status_)) {close();return false;}
    uint64_t arena_bytes=layout_.bytes;
    if(warp) {
        if(!layout_source_warp_inputs(*warp,layout_.bytes,limits.minStorageBufferOffsetAlignment,
            limits.minUniformBufferOffsetAlignment,limits.maxStorageBufferRange,limits.maxUniformBufferRange,
            warp_layout_,status_)) {close();return false;}
        SourceSpanInputWrite graphics;
        if(!source_warp_graphics_write(model,layout_,*warp,warp_layout_,graphics,status_)) {close();return false;}
        arena_bytes=warp_layout_.bytes;image.resize(arena_bytes);
        std::vector<SourceSpanInputWrite> writes;
        if(!source_warp_input_writes(*warp,warp_layout_,writes,status_)) {close();return false;}
        writes.push_back(std::move(graphics));
        for(const auto& write:writes) std::copy(write.bytes.begin(),write.bytes.end(),image.begin()+write.offset);
    }
    if(axis) {
        if(!layout_source_axis_inputs(*axis,arena_bytes,limits.minStorageBufferOffsetAlignment,
            limits.minUniformBufferOffsetAlignment,limits.maxStorageBufferRange,limits.maxUniformBufferRange,
            axis_layout_,status_)) {close();return false;}
        arena_bytes=axis_layout_.bytes;image.resize(arena_bytes);
        std::vector<SourceSpanInputWrite> writes;
        if(!source_axis_input_writes(*axis,axis_layout_,writes,status_)) {close();return false;}
        for(const auto& write:writes) std::copy(write.bytes.begin(),write.bytes.end(),image.begin()+write.offset);
    }
    if(!storage_.initialize(device,get,memory,arena_bytes)) return fail(storage_.status());
    if(!storage_.upload(0,image)) return fail(storage_.status());
    if(!bindings_.initialize(device,get,storage_.buffer(),arena_bytes,layout_,pipelines,model.graphics_unclipped,
        warp?&warp_layout_:nullptr)) return fail(bindings_.status());
    if(warp && !warp_bindings_.initialize(device,get,storage_.buffer(),arena_bytes,layout_,warp_layout_,*warp,*warp_pipelines))
        return fail(warp_bindings_.status());
    warp_=bool(warp);
    if(axis && !axis_bindings_.initialize(device,get,storage_.buffer(),arena_bytes,layout_,axis_layout_,*axis,*axis_pipeline))
        return fail(axis_bindings_.status());
    axis_=bool(axis);
    barrier_=reinterpret_cast<PFN_vkCmdPipelineBarrier>(get(device,"vkCmdPipelineBarrier"));
    if(!barrier_) return fail("Missing source model pipeline barrier");
    device_=device;get_=get;memory_=memory;unclipped_=model.graphics_unclipped;
    has_lines_=source_span_has_lines(model);
    mixed_=warp_ || (!unclipped_ && source_span_has_ordinary_faces(model));
    corner_capacity_=warp_?32:source_span_corner_capacity(model);
    if(unclipped_ && corner_capacity_>32) return fail("Source polygon template exceeds corner limit");
    pipelines_=pipelines;status_="Source model ready";return true;
}
bool VulkanSourceModel::prepare_graphics(VkRenderPass pass,float units,const SceneVertex& material,
    std::shared_ptr<SourceGraphicsPipelines> shared) {
    std::vector<SceneVertex> vertices;
    if(!pass || !cover_geometry(units,material,vertices)) return false;
    if(shared && ((shared->device && shared->device!=device_) || (shared->pass && shared->pass!=pass))) {
        status_="Incompatible shared source graphics pipelines";return false;
    }
    graphics_pipelines_=shared?std::move(shared):std::make_shared<SourceGraphicsPipelines>();
    auto& pipelines=*graphics_pipelines_;pipelines.device=device_;pipelines.pass=pass;
    line_vertices_.close();graphics_vertices_.close();graphics_textures_.close();
    if(!graphics_textures_.initialize_external(device_,get_,buffer(),storage_.size())) {
        status_=graphics_textures_.status();return false;
    }
    if(axis_) {
        if(!line_vertices_.initialize(device_,get_,memory_,vertices)) {status_=line_vertices_.status();return false;}
        if(!pipelines.lines_ready && !pipelines.lines.initialize(device_,get_,pass,true,SceneTopology::lines,graphics_textures_.layout(),SceneBlend::opaque,pipelines.cache)) {
            status_=pipelines.lines.status();return false;
        }
        pipelines.lines_ready=true;status_="Resident axis endpoint graphics ready";return true;
    }
    if(!graphics_vertices_.initialize(device_,get_,memory_,vertices)) {
        status_=graphics_vertices_.status();return false;
    }
    if(!pipelines.triangles_ready && !pipelines.triangles.initialize(device_,get_,pass,true,SceneTopology::triangles,graphics_textures_.layout(),SceneBlend::opaque,pipelines.cache)) {
        status_=pipelines.triangles.status();return false;
    }
    pipelines.triangles_ready=true;
    if(has_lines_) {
        std::vector<SceneVertex> lines;lines.reserve(size_t(counts_[0])*2);
        for(uint32_t slot=0;slot<counts_[0];++slot) for(uint32_t corner=0;corner<2;++corner) {
            auto vertex=vertices.front();vertex.texture[1]=slot;vertex.texture[2]=corner;
            vertex.texture[3]|=16777216U;lines.push_back(vertex);
        }
        if(!line_vertices_.initialize(device_,get_,memory_,lines)) {status_=line_vertices_.status();return false;}
        if(!pipelines.lines_ready && !pipelines.lines.initialize(device_,get_,pass,true,SceneTopology::lines,graphics_textures_.layout(),SceneBlend::opaque,pipelines.cache)) {
            status_=pipelines.lines.status();return false;
        }
        pipelines.lines_ready=true;
    }
    status_="Resident source model graphics ready";return true;
}
bool VulkanSourceModel::record_graphics(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera) const {
    if(!graphics_pipelines_) return false;
    if(axis_) return buffer() && graphics_pipelines_->lines.record(command,extent,line_vertices_.buffer(),
        line_vertices_.count(),camera,graphics_textures_.descriptor());
    if(has_lines_) {
        if(!buffer()) return false;
        const uint32_t per_slot=(corner_capacity_-2)*3;
        for(uint32_t slot=0;slot<counts_[0];++slot) {
            if(!graphics_pipelines_->triangles.record_range(command,extent,graphics_vertices_.buffer(),graphics_vertices_.count(),
                slot*per_slot,per_slot,camera,graphics_textures_.descriptor())) return false;
            if(!graphics_pipelines_->lines.record_range(command,extent,line_vertices_.buffer(),line_vertices_.count(),
                slot*2,2,camera,graphics_textures_.descriptor())) return false;
        }
        return true;
    }
    return buffer() && graphics_pipelines_->triangles.record(command,extent,graphics_vertices_.buffer(),
        graphics_vertices_.count(),camera,graphics_textures_.descriptor());
}
bool VulkanSourceModel::update(const SourceSpanModel& model,const SourceWarpInputs* warp,const SourceAxisInputs* axis) {
    const std::array<uint32_t,5> counts{model.spans.count,model.spans.polygon_count,
        model.projection_settings[0],model.visibility_settings[0],1};
    if(!buffer() || axis_!=bool(axis) || (axis && axis->settings.point_count!=counts_[2])
        || warp_!=bool(warp) || model.warp_expanded!=warp_ || model.graphics_unclipped!=unclipped_
        || source_span_has_lines(model)!=has_lines_
        || (warp_ || (!model.graphics_unclipped && source_span_has_ordinary_faces(model)))!=mixed_
        || (warp_?32:source_span_corner_capacity(model))!=corner_capacity_
        || counts!=counts_ || counts[2]!=model.projection.continuous_vertices.size()
        || counts[3]!=model.projection.visibility_faces.size() || model.projection_settings[1]!=model.poses.size()) {
        status_="Source model update requires compatible dispatch counts";return false;
    }
    std::vector<SourceSpanInputWrite> warp_writes;
    SourceSpanInputWrite warp_graphics;
    std::vector<SourceSpanInputWrite> axis_writes;
    if(axis && !source_axis_input_writes(*axis,axis_layout_,axis_writes,status_)) return false;
    if(warp && (!source_warp_graphics_write(model,layout_,*warp,warp_layout_,warp_graphics,status_)
        || !source_warp_input_writes(*warp,warp_layout_,warp_writes,status_))) return false;
    if(!source_span_input_writes(model,layout_,inputs_,status_,&input_scratch_)) return false;
    if(warp) {
        for(auto& write:inputs_) if(write.offset==warp_graphics.offset) {
            write=std::move(warp_graphics);break;
        }
        for(auto& write:warp_writes) inputs_.push_back(std::move(write));
    }
    for(auto& write:axis_writes) inputs_.push_back(std::move(write));
    upload_views_.clear();upload_views_.reserve(inputs_.size());
    for(size_t i=0;i<inputs_.size();++i) {
        const auto& input=inputs_[i];
        // input_scratch_ owns the last successful input snapshot after the
        // transactional swap. Static topology/textures need no GPU rewrite.
        if(i<input_scratch_.size() && input.offset==input_scratch_[i].offset
            && input.bytes==input_scratch_[i].bytes) continue;
        upload_views_.push_back({input.offset,input.bytes});
    }
    if(!upload_views_.empty()) {compute_dirty_=true;compute_recorded_=false;}
    if(!storage_.upload_many(upload_views_)) {
        status_=storage_.status();inputs_.clear();input_scratch_.clear();return false;
    }
    updated_input_bytes_=0;
    for(const auto& write:upload_views_) updated_input_bytes_+=write.bytes.size();
    status_="Source model updated without reallocation";return true;
}
bool VulkanSourceModel::record(VkCommandBuffer command) const {
    if(!command || !buffer() || !barrier_) return false;
    if(!compute_dirty_) return true;
    for(size_t i=unclipped_?2:0;i<pipelines_.size();++i)
        if(!pipelines_[i] || !pipelines_[i]->descriptor_layout(0) || static_cast<size_t>(pipelines_[i]->stage())!=i) return false;
    constexpr std::array<unsigned,5> order{2,3,4,1,0};
    for(unsigned index:order) {
        // Ordinary 3D polygons consume transformed points, visibility and BSP
        // order directly. They do not read clipped polygons, spans or masks.
        if(unclipped_ && (index==1 || index==0)) continue;
        if(!pipelines_[index]->record(command,bindings_.sets(static_cast<SourceComputeStage>(index)),counts_[index])) return false;
        const bool final=index==0 || (unclipped_ && index==4);
        VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
        dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|(final?VK_ACCESS_HOST_READ_BIT:0);
        const VkPipelineStageFlags destination=final?
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_HOST_BIT
                |(warp_?VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:0):
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        barrier_(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,destination,0,1,&dependency,0,nullptr,0,nullptr);
        if(warp_ && index==4 && !warp_bindings_.record(command)) return false;
        if(axis_ && index==2 && !axis_bindings_.record(command)) return false;
    }
    compute_recorded_=true;
    return true;
}
bool VulkanSourceModel::ray_source_ranges(std::array<VkDescriptorBufferInfo,3>& output,bool include_warp_candidates) const noexcept {
    if(!buffer() || (warp_ && !include_warp_candidates) || axis_) return false;
    std::array<VkDescriptorBufferInfo,3> pending{};
    const SourceSpanRegion regions[]{SourceSpanRegion::points,SourceSpanRegion::residuals,SourceSpanRegion::corners};
    for(unsigned i=0;i<3;++i) {
        const auto& range=layout_[regions[i]];
        if(!range.size || range.offset>storage_.size() || range.size>storage_.size()-range.offset) return false;
        pending[i]={buffer(),range.offset,range.size};
    }
    output=pending;return true;
}
bool VulkanSourceModel::readback_axis(SourceAxisRegion region,std::span<std::byte> output) {
    if(!axis_ || size_t(region)>=axis_layout_.regions.size()) return false;
    const auto& range=axis_layout_[region];
    return output.size()<=range.size && storage_.readback(range.offset,output);
}
bool VulkanSourceModel::readback_warp(SourceWarpRegion region,std::span<std::byte> output) {
    if(!warp_ || size_t(region)>=warp_layout_.regions.size()) return false;
    const auto& range=warp_layout_[region];
    return output.size()<=range.size && storage_.readback(range.offset,output);
}
bool VulkanSourceModel::update_palette(std::span<const uint32_t,256> palette) {
    // Palette indices stay in the generated commands; graphics resolves them
    // from this buffer. A palette write does not invalidate geometry output.
    if(!buffer() || layout_[SourceSpanRegion::palette].size!=palette.size_bytes()) {
        status_="Source palette update requires an initialized model";return false;
    }
    if(!storage_.upload(layout_[SourceSpanRegion::palette].offset,std::as_bytes(palette))) {
        inputs_.clear();input_scratch_.clear();
        status_=storage_.status();return false;
    }
    // A direct palette write bypasses the input snapshot; invalidate it so a
    // later full update can restore even a previously identical palette.
    inputs_.clear();input_scratch_.clear();
    status_="Source palette updated without geometry upload or recompute";return true;
}
bool VulkanSourceModel::readback(SourceSpanRegion region,std::span<std::byte> output) {
    if(static_cast<size_t>(region)>=layout_.regions.size() || output.size()>layout_[region].size) {
        status_="Source model readback outside region";return false;
    }
    if(!storage_.readback(layout_[region].offset,output)) {status_=storage_.status();return false;}
    return true;
}
bool VulkanSourceModel::cover_geometry(float units,const SceneVertex& material,std::vector<SceneVertex>& output) {
    if(!buffer() || !std::isfinite(units) || units<=0 || !counts_[0] || counts_[0]>4'000'000/6) {
        status_="Invalid source cover geometry size/units";return false;
    }
    for(unsigned i=0;i<4;++i) if(!std::isfinite(material.color[i]) || !std::isfinite(material.odd_color[i])) {
        status_="Invalid source cover material";return false;
    }
    if(axis_) {
        std::vector<SceneVertex> pending(2);
        for(unsigned endpoint=0;endpoint<2;++endpoint) {
            auto& vertex=pending[endpoint];
            std::copy_n(material.color,4,vertex.color);std::copy_n(material.odd_color,4,vertex.odd_color);
            vertex.dither_scale=material.dither_scale;
            vertex.texture[0]=uint32_t(axis_layout_[SourceAxisRegion::endpoints].offset/4);
            vertex.texture[1]=endpoint;vertex.texture[3]=67108864U;vertex.billboard[1]=units;
            vertex.texture[2]=uint32_t(layout_[SourceSpanRegion::graphics_lookup].offset/4);
        }
        output=std::move(pending);status_="Axis endpoint templates ready";return true;
    }
    const uint32_t triangles=(unclipped_ || mixed_)?corner_capacity_-2:2U;
    if(counts_[0]>4'000'000/(triangles*3)) {status_="Source template budget exceeded";return false;}
    std::vector<SceneVertex> pending;pending.reserve(size_t(counts_[0])*triangles*3);
    for(uint32_t slot=0;slot<counts_[0];++slot) for(uint32_t triangle=0;triangle<triangles;++triangle)
        for(uint32_t corner:{0U,triangle+1,triangle+2}) {
        SceneVertex vertex{};
        std::copy_n(material.color,4,vertex.color);std::copy_n(material.odd_color,4,vertex.odd_color);
        vertex.dither_scale=material.dither_scale;
        vertex.texture[0]=static_cast<uint32_t>(layout_[SourceSpanRegion::graphics_lookup].offset/4);
        vertex.texture[1]=slot;vertex.texture[2]=corner;
        vertex.texture[3]=131072U|(unclipped_?(262144U|4194304U):524288U);
        if(mixed_) vertex.texture[3]|=33554432U;
        vertex.billboard[1]=units;pending.push_back(vertex);
    }
    output=std::move(pending);status_="Source cover templates ready";return true;
}
}
