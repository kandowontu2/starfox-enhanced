#include "starfox/vr/source_span_model.hpp"
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <cstring>
#include "starfox/vr/decal_surface.hpp"
#include <cmath>
namespace starfox::vr {
bool layout_source_axis_inputs(const SourceAxisInputs& input,uint64_t prefix_bytes,
    uint64_t storage_alignment,uint64_t uniform_alignment,uint64_t max_storage_range,
    uint64_t max_uniform_range,SourceAxisArenaLayout& output,std::string& error) {
    try {
        constexpr uint64_t budget=256ULL*1024*1024;
        const auto aligned=[](uint64_t n){return n && !(n&(n-1));};
        if(!aligned(storage_alignment) || !aligned(uniform_alignment)
            || storage_alignment>budget || uniform_alignment>budget || prefix_bytes>budget)
            throw std::runtime_error("Invalid axis arena alignment/prefix");
        const auto& s=input.settings;
        if(!s.point_count || s.point_count>4U*1024*1024 || s.index_count!=input.indices.size()
            || s.fractional!=1 || s.has_residuals>1)
            throw std::runtime_error("Invalid axis input counts/mode");
        for(unsigned group=0;group<2;++group) {
            const auto first=s.ranges[group*2],count=s.ranges[group*2+1];
            if(!count || count>65536 || first>s.index_count || count>s.index_count-first)
                throw std::runtime_error("Axis group exceeds index storage");
        }
        for(auto index:input.indices) if(index>=s.point_count)
            throw std::runtime_error("Axis index exceeds projection storage");
        for(auto value:s.projection) if(!std::isfinite(value))
            throw std::runtime_error("Invalid axis projection parameters");
        SourceAxisArenaLayout pending;pending.bytes=prefix_bytes;
        const auto add=[&](SourceAxisRegion region,uint64_t size,bool uniform=false) {
            const auto alignment=std::max(uint64_t(4),uniform?uniform_alignment:storage_alignment);
            const auto offset=(pending.bytes+alignment-1)&~(alignment-1);
            if(size>(uniform?max_uniform_range:max_storage_range) || offset>budget || size>budget-offset)
                throw std::runtime_error("Axis descriptor exceeds device or arena limit");
            pending.regions[static_cast<size_t>(region)]={offset,size};pending.bytes=offset+size;
        };
        add(SourceAxisRegion::indices,uint64_t(s.index_count)*4);
        add(SourceAxisRegion::endpoints,64);add(SourceAxisRegion::residuals,64);
        add(SourceAxisRegion::settings,sizeof(s),true);
        output=pending;error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool prepare_source_axis_inputs(const assets::Shape& shape,const render::RenderPose& pose,
    const render::RenderSettings& settings,uint32_t projected_vertices,bool residuals,
    SourceAxisInputs& output,std::string& error) {
    try {
        if(!pose.collapse_to_axis_line || shape.faces.empty())
            throw std::runtime_error("Axis reduction requires an authored face and collapse pose");
        const auto& vertices=shape.frames.empty()?shape.vertices
            :shape.frames[pose.animation_frame%shape.frames.size()].vertices;
        if(vertices.empty() || vertices.size()!=projected_vertices)
            throw std::runtime_error("Axis reduction vertex count differs from projection");
        const auto groups=render::pack_axis_groups(shape,pose.animation_frame);
        SourceAxisInputs pending;
        for(unsigned group=0;group<2;++group) {
            if(groups[group].empty() || groups[group].size()>65536)
                throw std::runtime_error("Axis group exceeds reduction budget");
            pending.settings.ranges[group*2]=uint32_t(pending.indices.size());
            pending.settings.ranges[group*2+1]=uint32_t(groups[group].size());
            pending.indices.insert(pending.indices.end(),groups[group].begin(),groups[group].end());
        }
        pending.settings.point_count=projected_vertices;
        pending.settings.index_count=uint32_t(pending.indices.size());
        pending.settings.has_residuals=residuals?1U:0U;
        pending.settings.projection={float(pose.vanish_x),float(pose.vanish_y),float(settings.focal_length),0};
        for(auto value:pending.settings.projection) if(!std::isfinite(value))
            throw std::runtime_error("Invalid axis projection parameters");
        output=std::move(pending);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool prepare_source_axis_model(const assets::Shape& shape,const render::RenderPose& pose,
    const render::RenderSettings& settings,uint32_t width,uint32_t height,
    SourceSpanModel& output,SourceAxisInputs& axis_output,std::string& error) {
    try {
        if(!pose.collapse_to_axis_line || shape.faces.empty())
            throw std::runtime_error("Axis model requires an authored face");
        // Keep every authored point for extrema reduction, but the source axis
        // bypasses BSP and shades one line using the first authored face.
        auto topology=shape;
        topology.faces.resize(1);topology.faces[0].vertex_indices={0,1};
        topology.faces[0].sprite=false;topology.faces[0].visibility_index=-1;
        topology.visibilities.clear();topology.face_batches.clear();
        topology.bsp_nodes.clear();topology.bsp_leaves.clear();topology.bsp_root_address=0;
        auto projection_pose=pose;projection_pose.collapse_to_axis_line=false;
        projection_pose.colour_warp=false; // Axis material is generated separately.
        projection_pose.explosion_progress=0;projection_pose.continuous_geometry=true;
        SourceSpanModel pending;SourceAxisInputs axis;
        if(!prepare_source_span_model(topology,projection_pose,settings,width,height,pending,error,true)) return false;
        if(!prepare_source_axis_inputs(shape,pose,settings,pending.projection_settings[0],
            pending.projection_settings[2]!=0,axis,error)) return false;
        output=std::move(pending);axis_output=std::move(axis);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool validate_source_axis_upload(const SourceAxisInputs& input,const SourceAxisArenaLayout& layout,std::string& error) {
    try {
        SourceAxisArenaLayout minimum;
        if(!layout_source_axis_inputs(input,0,4,4,UINT64_MAX,UINT64_MAX,minimum,error)) return false;
        if(!layout.bytes || layout.bytes>256ULL*1024*1024)
            throw std::runtime_error("Invalid axis upload size");
        uint64_t previous=0;
        for(size_t i=0;i<layout.regions.size();++i) {
            const auto& range=layout.regions[i];
            if((range.offset&3U) || range.offset<previous || range.size!=minimum.regions[i].size
                || range.offset>layout.bytes || range.size>layout.bytes-range.offset)
                throw std::runtime_error("Invalid axis upload region");
            previous=range.offset+range.size;
        }
        error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool source_axis_input_writes(const SourceAxisInputs& input,const SourceAxisArenaLayout& layout,
    std::vector<SourceSpanInputWrite>& output,std::string& error) {
    try {
        if(!validate_source_axis_upload(input,layout,error)) return false;
        std::vector<SourceSpanInputWrite> pending;
        const auto indices=std::as_bytes(std::span(input.indices));
        const auto settings=std::as_bytes(std::span(&input.settings,1));
        pending.push_back({layout[SourceAxisRegion::indices].offset,{indices.begin(),indices.end()}});
        pending.push_back({layout[SourceAxisRegion::settings].offset,{settings.begin(),settings.end()}});
        output=std::move(pending);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool prepare_source_warp_inputs(const assets::Shape& shape,const render::RenderPose& pose,
    const render::RenderSettings& settings,const render::PackedProjection& projection,
    const render::PackedBsp& bsp,SourceWarpInputs& output,std::string& error,
    const render::PackedFaces* fragments,uint32_t source_vertex_count) {
    try {
        if(pose.collapse_to_axis_line) {
            if(shape.faces.empty() || fragments
                || (source_vertex_count && source_vertex_count!=projection.continuous_vertices.size())
                || bsp.faces.size()!=1 || bsp.output_capacity!=1)
                throw std::runtime_error("Axis warp requires one authored-face occurrence");
            // Axis collapse bypasses face traversal and destruction. Preserve
            // all source points for the PRNG seed, but shade only face zero.
            auto topology=shape;topology.faces.resize(1);
            topology.faces[0].vertex_indices={0,1};topology.faces[0].sprite=false;
            topology.faces[0].visibility_index=-1;
            topology.visibilities.clear();topology.face_batches.clear();
            topology.bsp_nodes.clear();topology.bsp_leaves.clear();topology.bsp_root_address=0;
            auto material_pose=pose;material_pose.collapse_to_axis_line=false;
            material_pose.explosion_progress=0;
            SourceWarpInputs pending;
            if(!prepare_source_warp_inputs(topology,material_pose,settings,projection,bsp,pending,error)) return false;
            // Even a random textured descriptor supplies a flat line colour.
            pending.shading.settings.flags|=16U;
            output=std::move(pending);error.clear();return true;
        }
        if(!pose.colour_warp || pose.force_colour || (pose.explosion_progress && (!fragments || !source_vertex_count))
            || pose.collapse_to_axis_line || !projection.continuous
            || projection.continuous_vertices.empty() || !bsp.output_capacity)
            throw std::runtime_error("Unsupported resident warp input mode");
        SourceWarpInputs pending;
        pending.templates=fragments?*fragments:render::pack_warp_faces(shape,bsp,pose,settings);
        pending.textures=render::pack_warp_textures(shape);
        pending.templates.texels=pending.textures.texels;
        pending.shading=render::pack_warp_shading(shape,pose,settings);
        pending.depth_colours=pose.depth_colour_tables;
        for(const auto& face:bsp.faces)
            pending.normals.push_back({face.normal.x,face.normal.y,face.normal.z,0});
        auto& uniforms=pending.shading.settings;
        uniforms.capacity=bsp.output_capacity;uniforms.face_count=static_cast<uint32_t>(bsp.faces.size());
        uniforms.visibility_count=static_cast<uint32_t>(projection.visibility_faces.size());
        uniforms.seed=uint16_t(pose.projected_points_address+
            (pose.explosion_progress?source_vertex_count:projection.continuous_vertices.size())*6U)
            |(pose.explosion_progress?0x80000000U:0U);
        uniforms.corner_count=static_cast<uint32_t>(pending.templates.corners.size());
        uniforms.texture_count=static_cast<uint32_t>(pending.textures.textures.size());
        uniforms.coordinate_count=static_cast<uint32_t>(pending.textures.coordinates.size());
        output=std::move(pending);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool layout_source_warp_inputs(const SourceWarpInputs& input,uint64_t prefix_bytes,
    uint64_t storage_alignment,uint64_t uniform_alignment,uint64_t max_storage_range,
    uint64_t max_uniform_range,SourceWarpArenaLayout& output,std::string& error) {
    try {
        constexpr uint64_t budget=256ULL*1024*1024;
        const auto aligned=[](uint64_t n){return n && !(n&(n-1));};
        const auto& s=input.shading.settings;
        if(!aligned(storage_alignment) || !aligned(uniform_alignment)
            || storage_alignment>budget || uniform_alignment>budget || prefix_bytes>budget)
            throw std::runtime_error("Invalid warp arena alignment/prefix");
        if(!s.capacity || !s.face_count || s.face_count!=input.templates.polygons.size()
            || s.face_count!=input.templates.materials.size() || s.face_count!=input.normals.size()
            || s.corner_count!=input.templates.corners.size() || s.texture_count!=input.textures.textures.size()
            || s.coordinate_count!=input.textures.coordinates.size() || input.textures.lookup.size()!=65536)
            throw std::runtime_error("Inconsistent warp input counts");
        for(const auto& polygon:input.templates.polygons)
            if(polygon[1]>32 || polygon[0]>s.corner_count || polygon[1]>s.corner_count-polygon[0])
                throw std::runtime_error("Warp source corner range exceeds storage");
        for(const auto& material:input.templates.materials)
            if(material.textured) throw std::runtime_error("Warp template must be untextured");
        for(auto count:s.shade_counts)
            if(count>62) throw std::runtime_error("Warp diffuse count exceeds storage");
        if(s.flags&~31U) throw std::runtime_error("Unknown warp shading flags");
        SourceWarpArenaLayout pending;pending.bytes=prefix_bytes;
        const auto add=[&](SourceWarpRegion region,uint64_t size,bool uniform=false) {
            size=std::max(uint64_t(4),size);
            const auto alignment=std::max(uint64_t(4),uniform?uniform_alignment:storage_alignment);
            const auto offset=(pending.bytes+alignment-1)&~(alignment-1);
            if(size>(uniform?max_uniform_range:max_storage_range) || offset>budget || size>budget-offset)
                throw std::runtime_error("Warp descriptor exceeds device or arena limit");
            pending.regions[static_cast<size_t>(region)]={offset,size};pending.bytes=offset+size;
        };
        using R=SourceWarpRegion;
        add(R::descriptors,uint64_t(s.capacity)*4);add(R::result,8);
        add(R::normals,uint64_t(s.face_count)*16);add(R::diffuse,sizeof(input.shading.diffuse));
        add(R::depth_colours,sizeof(input.depth_colours));add(R::texture_lookup,65536ULL*4);
        add(R::textures,uint64_t(s.texture_count)*16);add(R::coordinates,uint64_t(s.coordinate_count)*8);
        add(R::decoded,uint64_t(s.capacity)*16);add(R::polygons,uint64_t(s.capacity)*16);
        add(R::corners,uint64_t(s.capacity)*32*16);add(R::materials,uint64_t(s.capacity)*96);
        add(R::sequence_settings,16,true);add(R::material_settings,64,true);add(R::expand_settings,32,true);
        output=pending;error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool validate_source_warp_upload(const SourceWarpInputs& input,const SourceWarpArenaLayout& layout,std::string& error) {
    try {
        SourceWarpArenaLayout minimum;
        if(!layout_source_warp_inputs(input,0,4,4,UINT64_MAX,UINT64_MAX,minimum,error)) return false;
        if(!layout.bytes || layout.bytes>256ULL*1024*1024) throw std::runtime_error("Invalid warp upload size");
        auto sorted=layout.regions;
        std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.offset<b.offset;});
        uint64_t previous=0;
        for(const auto& range:sorted) {
            if((range.offset&3U) || range.offset<previous || range.offset>layout.bytes || range.size>layout.bytes-range.offset)
                throw std::runtime_error("Overlapping or out-of-range warp upload");
            previous=range.offset+range.size;
        }
        for(size_t i=0;i<layout.regions.size();++i)
            if(layout.regions[i].size!=minimum.regions[i].size)
                throw std::runtime_error("Incorrect warp upload region size");
        error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool source_warp_input_writes(const SourceWarpInputs& input,const SourceWarpArenaLayout& layout,
    std::vector<SourceSpanInputWrite>& output,std::string& error) {
    try {
        if(!validate_source_warp_upload(input,layout,error)) return false;
        std::vector<SourceSpanInputWrite> pending;
        const auto copy=[&](SourceWarpRegion region,const auto& values) {
            const auto bytes=std::as_bytes(std::span(values));const auto& range=layout[region];
            if(bytes.size()>range.size) throw std::runtime_error("Warp input exceeds region");
            if(!bytes.empty()) pending.push_back({range.offset,{bytes.begin(),bytes.end()}});
        };
        using R=SourceWarpRegion;
        copy(R::normals,input.normals);copy(R::diffuse,input.shading.diffuse);
        copy(R::depth_colours,input.depth_colours);copy(R::texture_lookup,input.textures.lookup);
        copy(R::textures,input.textures.textures);copy(R::coordinates,input.textures.coordinates);
        const auto& s=input.shading.settings;
        const std::array<uint32_t,4> sequence{s.capacity,s.face_count,s.visibility_count,s.seed};
        const std::array<uint32_t,16> material{s.capacity,s.face_count,s.depth_band,s.flags,
            uint32_t(s.light[0]),uint32_t(s.light[1]),uint32_t(s.light[2]),uint32_t(s.light[3]),
            s.shade_counts[0],s.shade_counts[1],s.shade_counts[2],s.shade_counts[3],
            s.colour_base,s.override_colour,s.forced_colour,0};
        const std::array<uint32_t,8> expand{s.capacity,s.face_count,s.corner_count,s.texture_count,
            s.coordinate_count,s.colour_base,uint32_t(s.scroll_x),uint32_t(s.scroll_y)};
        copy(R::sequence_settings,sequence);copy(R::material_settings,material);copy(R::expand_settings,expand);
        // descriptors/result/decoded and expanded geometry/materials are GPU
        // outputs. Updating settings must never upload stale output bytes.
        output=std::move(pending);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool layout_source_span_model(const SourceSpanModel& model,uint64_t storage_alignment,
    uint64_t uniform_alignment,uint64_t max_storage_range,uint64_t max_uniform_range,
    SourceSpanArenaLayout& output,std::string& error) {
    try {
        constexpr uint64_t budget=256ULL*1024*1024;
        const auto aligned=[](uint64_t n){return n && !(n&(n-1));};
        if(!aligned(storage_alignment) || !aligned(uniform_alignment)
            || storage_alignment>budget || uniform_alignment>budget)
            throw std::runtime_error("Invalid source descriptor alignment");
        const auto span_sizes=source_span_buffer_sizes(model.spans);
        const uint64_t vertices=model.projection.continuous_vertices.size();
        const uint64_t faces=model.bsp.faces.size();
        if(!span_sizes || !model.projection.continuous || !vertices
            || (model.warp_expanded?model.spans.count:faces)!=model.spans.polygon_count || model.faces.polygons.size()!=faces
            || model.faces.materials.size()!=faces || model.faces.primitives.size()!=faces
            || model.bsp.output_capacity!=model.spans.count || model.poses.size()<4
            || (model.fragmented?model.poses.size()!=faces*6+4:model.poses.size()>6)
            || model.projection_parameters.size()!=faces)
            throw std::runtime_error("Inconsistent source span model arrays");
        if(model.warp_expanded && (model.graphics_unclipped || model.spans.ordered_mode!=2
            || !(model.graphics_palette_flags&4U)))
            throw std::runtime_error("Expanded warp requires clipped occurrence consumers");
        const auto clip_faces=model.warp_expanded?model.spans.count:static_cast<uint32_t>(faces);
        const auto clip_corners=model.warp_expanded?model.spans.count*32U:static_cast<uint32_t>(model.faces.corners.size());
        // Byte textures are fetched through aligned uint32 storage loads.
        if((model.faces.texels.size()&3U) || model.faces.texels.size()>16U*1024*1024)
            throw std::runtime_error("Invalid source texture storage");
        for(const auto& material:model.faces.materials) if(material.textured) {
            const uint64_t size=(uint64_t(material.u_mask)+1)*(uint64_t(material.v_mask)+1);
            if(material.u_mask>65535 || material.v_mask>65535
                || material.texture_offset>model.faces.texels.size()
                || size>model.faces.texels.size()-material.texture_offset)
                throw std::runtime_error("Source texture exceeds storage");
        }
        const auto visibility=model.projection.visibility_faces.size();
        if(model.projection_settings[0]!=vertices || model.projection_settings[1]!=model.poses.size()
            || model.projection_settings[2]>2 || model.projection_settings[3]
            || model.visibility_settings!=std::array<uint32_t,4>{static_cast<uint32_t>(visibility),static_cast<uint32_t>(vertices),0,0}
            || model.bsp_settings!=std::array<uint32_t,8>{1,static_cast<uint32_t>(model.bsp.nodes.size()),static_cast<uint32_t>(visibility),
                static_cast<uint32_t>(model.bsp.face_ids.size()),model.spans.count,0,0,0}
            || model.clip_settings!=std::array<uint32_t,8>{clip_faces,static_cast<uint32_t>(vertices),
                clip_corners,static_cast<uint32_t>(visibility),
                static_cast<uint32_t>(model.spans.width),static_cast<uint32_t>(model.spans.height),static_cast<uint32_t>(faces),
                model.projection_settings[2]?static_cast<uint32_t>(vertices):0U}
            || model.tree!=std::array<uint32_t,4>{model.bsp.root,0,model.spans.count,model.bsp.work_limit}
            || (model.graphics_wave_phase&0x7ff00000U) || (model.graphics_palette_flags&~7U)
            || ((model.graphics_palette_flags&4U) && model.spans.ordered_mode!=2))
            throw std::runtime_error("Inconsistent source shader settings");
        SourceSpanArenaLayout pending;
        const auto add=[&](SourceSpanRegion region,uint64_t size,bool uniform=false) {
            size=std::max(size,uint64_t(4));
            const auto alignment=std::max(uint64_t(4),uniform?uniform_alignment:storage_alignment);
            const auto offset=(pending.bytes+alignment-1)&~(alignment-1);
            if(size>(uniform?max_uniform_range:max_storage_range) || offset>budget || size>budget-offset)
                throw std::runtime_error("Source descriptor exceeds device or arena limit");
            pending.regions[static_cast<size_t>(region)]={offset,size};pending.bytes=offset+size;
        };
        using R=SourceSpanRegion;
        add(R::vertices,vertices*sizeof(render::ContinuousTransformVertex));
        add(R::poses,model.poses.size()*sizeof(render::ContinuousTransformPose));
        add(R::points,vertices*32);add(R::residuals,vertices*32);
        add(R::triples,model.projection.visibility_faces.size()*16ULL);
        add(R::visibility,model.projection.visibility_faces.size()*4ULL);
        add(R::nodes,model.bsp.nodes.size()*32ULL);add(R::face_ids,model.bsp.face_ids.size()*4ULL);
        // Mode 2's span consumer only needs a dummy order binding, but this
        // arena also owns the BSP producer's full output (and warp's input).
        add(R::trees,16);add(R::order,std::max((*span_sizes)[2],uint64_t(model.bsp.output_capacity)*4));add(R::results,(*span_sizes)[3]);
        add(R::corners,model.faces.corners.size()*16ULL);add(R::polygons,faces*16);
        add(R::projection_parameters,faces*16);add(R::clipped,model.graphics_unclipped?4:(*span_sizes)[0]);
        add(R::materials,faces*96);add(R::commands,model.graphics_unclipped?4:(*span_sizes)[4]);
        add(R::masks,model.graphics_unclipped?4:(*span_sizes)[5]);
        add(R::projection_settings,16,true);add(R::visibility_settings,16,true);
        add(R::bsp_settings,32,true);add(R::clip_settings,32,true);add(R::span_settings,64,true);
        add(R::graphics_headers,uint64_t(model.spans.count)*16);
        add(R::graphics_lookup,80);
        add(R::palette,sizeof(model.graphics_palette));
        add(R::texture_bytes,model.faces.texels.size());
        add(R::graphics_polygons,faces*16);
        output=pending;error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool validate_source_span_upload(const SourceSpanModel& model,const SourceSpanArenaLayout& layout,std::string& error) {
    try {
        SourceSpanArenaLayout minimum;
        if(!layout_source_span_model(model,4,4,UINT64_MAX,UINT64_MAX,minimum,error)) return false;
        if(!layout.bytes || layout.bytes>256ULL*1024*1024) throw std::runtime_error("Invalid source upload size");
        auto sorted=layout.regions;
        std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.offset<b.offset;});
        uint64_t previous=0;
        for(const auto& range:sorted) {
            if((range.offset&3U) || range.offset<previous || range.offset>layout.bytes || range.size>layout.bytes-range.offset)
                throw std::runtime_error("Overlapping or out-of-range source upload");
            previous=range.offset+range.size;
        }
        for(size_t i=0;i<layout.regions.size();++i)
            if(layout.regions[i].size!=minimum.regions[i].size)
                throw std::runtime_error("Incorrect source upload region size");
        error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool source_span_input_writes(const SourceSpanModel& model,const SourceSpanArenaLayout& layout,
    std::vector<SourceSpanInputWrite>& output,std::string& error,std::vector<SourceSpanInputWrite>* recycle) {
    try {
        if(!validate_source_span_upload(model,layout,error)) return false;
        if(recycle==&output) throw std::runtime_error("Source input recycle aliases output");
        std::vector<SourceSpanInputWrite> pending=recycle?std::move(*recycle):std::vector<SourceSpanInputWrite>{};
        pending.reserve(21);size_t written=0;
        const auto copy=[&](SourceSpanRegion region,const auto& values) {
            const auto bytes=std::as_bytes(std::span(values));const auto& range=layout[region];
            if(bytes.size()>range.size) throw std::runtime_error("Source input exceeds region");
            if(!bytes.empty()) {
                if(written==pending.size()) pending.emplace_back();
                auto& write=pending[written++];write.offset=range.offset;
                write.bytes.assign(bytes.begin(),bytes.end());
            }
        };
        using R=SourceSpanRegion;
        copy(R::vertices,model.projection.continuous_vertices);copy(R::poses,model.poses);
        copy(R::triples,model.projection.visibility_faces);copy(R::nodes,model.bsp.nodes);
        copy(R::face_ids,model.bsp.face_ids);copy(R::trees,model.tree);
        copy(R::corners,model.faces.corners);copy(R::polygons,model.faces.polygons);
        // Clipping deliberately suppresses non-solid faces. Graphics needs
        // their intact topology in the same BSP face-ID space, so keep a
        // separate descriptor view rather than deleting those primitives.
        auto graphics_polygons=model.faces.polygons;
        for(size_t i=0;i<graphics_polygons.size();++i) {
            const auto count=model.bsp.faces[i].vertex_indices.size();
            const auto primitive=model.faces.primitives[i];
            graphics_polygons[i][1]=(primitive==render::PackedPrimitive::polygon && count>=3)
                || (primitive==render::PackedPrimitive::line && count==2)
                || (primitive==render::PackedPrimitive::sprite && count==1)?static_cast<uint32_t>(count):0U;
        }
        copy(R::graphics_polygons,graphics_polygons);
        copy(R::projection_parameters,model.projection_parameters);copy(R::materials,model.faces.materials);
        copy(R::projection_settings,model.projection_settings);copy(R::visibility_settings,model.visibility_settings);
        copy(R::bsp_settings,model.bsp_settings);copy(R::clip_settings,model.clip_settings);
        const std::array<SourceSpanSettings,1> settings{model.spans};copy(R::span_settings,settings);
        // Graphics uses word offsets into the very same resident arena. Each
        // ordered occurrence has its own span block, including repeated faces.
        std::vector<std::array<uint32_t,4>> headers(model.spans.count);
        for(uint32_t slot=0;!model.graphics_unclipped && slot<model.spans.count;++slot)
            headers[slot]={static_cast<uint32_t>(layout[R::commands].offset/4+uint64_t(slot)*model.spans.height*24),
                static_cast<uint32_t>(layout[R::masks].offset/4),static_cast<uint32_t>(model.spans.height),model.graphics_wave_phase};
        copy(R::graphics_headers,headers);
        const auto word=[&](R region){return static_cast<uint32_t>(layout[region].offset/4);};
        // Immutable lookup metadata; order/results/points themselves remain
        // GPU-owned. Counts bound every indirection performed by graphics.
        const std::array<uint32_t,20> lookup{word(R::order),word(R::results),word(R::points),word(R::graphics_headers),
            model.spans.count,model.spans.polygon_count,model.projection_settings[0],static_cast<uint32_t>(model.spans.width),
            word(R::graphics_polygons),word(R::corners),static_cast<uint32_t>(model.faces.corners.size()),word(R::projection_parameters),
            word(R::clipped),static_cast<uint32_t>(model.spans.height),word(R::palette),model.graphics_palette_flags,
            word(R::materials),word(R::visibility),model.visibility_settings[0],word(R::texture_bytes)};
        copy(R::graphics_lookup,lookup);
        copy(R::palette,model.graphics_palette);
        copy(R::texture_bytes,model.faces.texels);
        pending.resize(written);
        output.swap(pending);if(recycle) *recycle=std::move(pending);
        error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool source_span_upload_image(const SourceSpanModel& model,const SourceSpanArenaLayout& layout,
    std::vector<std::byte>& output,std::string& error) {
    try {
        std::vector<SourceSpanInputWrite> writes;
        if(!source_span_input_writes(model,layout,writes,error)) return false;
        std::vector<std::byte> pending(static_cast<size_t>(layout.bytes));
        for(const auto& write:writes) std::memcpy(pending.data()+write.offset,write.bytes.data(),write.bytes.size());
        output=std::move(pending);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool source_warp_graphics_write(const SourceSpanModel& model,const SourceSpanArenaLayout& source,
    const SourceWarpInputs& input,const SourceWarpArenaLayout& warp,SourceSpanInputWrite& output,std::string& error) {
    try {
        if(!model.warp_expanded || input.shading.settings.capacity!=model.spans.count
            || input.shading.settings.face_count!=model.bsp.faces.size())
            throw std::runtime_error("Warp graphics requires matching expanded source counts");
        for(const auto& range:warp.regions)
            if(range.offset<source.bytes) throw std::runtime_error("Warp graphics overlaps source prefix");
        if(!validate_source_warp_upload(input,warp,error) || !validate_source_span_upload(model,source,error)) return false;
        const auto s=[&](SourceSpanRegion region){return uint32_t(source[region].offset/4);};
        const auto w=[&](SourceWarpRegion region){return uint32_t(warp[region].offset/4);};
        using S=SourceSpanRegion;using W=SourceWarpRegion;
        // Only metadata is needed: never copy vertices or the 256 KiB warp
        // texture lookup just to replace these twenty graphics words.
        const std::array<uint32_t,20> lookup{
            s(S::order),w(W::result),s(S::points),s(S::graphics_headers),
            model.spans.count,model.spans.polygon_count,model.projection_settings[0],uint32_t(model.spans.width),
            w(W::polygons),w(W::corners),model.spans.count*32U,s(S::projection_parameters),
            s(S::clipped),uint32_t(model.spans.height),s(S::palette),model.graphics_palette_flags,
            w(W::materials),s(S::visibility),model.visibility_settings[0],s(S::texture_bytes)};
        const auto bytes=std::as_bytes(std::span(lookup));
        SourceSpanInputWrite pending{source[S::graphics_lookup].offset,{bytes.begin(),bytes.end()}};
        output=std::move(pending);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool prepare_source_span_model(const assets::Shape& shape,const render::RenderPose& pose,
    const render::RenderSettings& settings,uint32_t width,uint32_t height,
    SourceSpanModel& output,std::string& error,bool unclipped) {
    try {
        if(!width || !height || width>4096 || height>4096)
            throw std::runtime_error("Invalid source span dimensions");
        SourceSpanModel pending;
        pending.fragmented=pose.explosion_progress!=0;
        pending.projection=render::pack_projection(shape,pose,settings);
        pending.source_vertex_count=uint32_t(pending.projection.continuous_vertices.size());
        if(!pending.projection.continuous)
            throw std::runtime_error("Source span model requires continuous projection");
        pending.bsp=render::pack_bsp(shape,pending.fragmented);
        const bool colour_warp=pose.colour_warp && !pose.force_colour;
        pending.faces=colour_warp?render::pack_warp_faces(shape,pending.bsp,pose,settings):render::pack_faces(shape,pending.bsp,pose,settings);
        const bool effect_requested=pose.wireframe_mode || pose.wobble_mode || pose.wave_mode || pose.cel_mode;
        bool affected_solid=false;
        for(size_t i=0;effect_requested && !affected_solid && i<pending.faces.primitives.size();++i)
            affected_solid=pending.faces.primitives[i]==render::PackedPrimitive::polygon
                && !pending.faces.materials[i].textured;
        if(unclipped && effect_requested && affected_solid)
            throw std::runtime_error("Unclipped source graphics cannot bypass span effects");
        // Native alternate span routines never handle lines, sprites or
        // textured polygons. Effect flags alone must not force those through
        // a solid-span emitter that would omit them.
        if(effect_requested && !affected_solid) unclipped=true;
        pending.graphics_unclipped=unclipped;
        auto& s=pending.spans;
        s.count=pending.bsp.output_capacity;
        s.polygon_count=static_cast<uint32_t>(pending.bsp.faces.size());
        s.width=static_cast<int32_t>(width);s.height=static_cast<int32_t>(height);
        s.render_scale=settings.render_scale;s.fractional=1;s.ordered_mode=1;
        s.line_thickness=settings.wireframe_thickness;
        s.mask_enabled=(pose.wobble_mode&1U)!=0;
        s.mask_stride=((width+31U)/32U)*4U;
        if(pose.wave_mode && !pose.cel_mode && !pose.wireframe_mode && !(pose.wobble_mode&2U))
            pending.graphics_wave_phase=0x80000000U|uint16_t(pose.wave_offset)|((pose.animation_frame&15U)<<16);
        if(!source_span_buffer_sizes(s)) throw std::runtime_error("Source span model exceeds buffer bounds");
        pending.tree={pending.bsp.root,0,pending.bsp.output_capacity,pending.bsp.work_limit};
        pending.poses.assign(pending.projection.continuous_poses.begin(),pending.projection.continuous_poses.end());
        const bool euler=!pose.use_rotation_matrix;
        if(pending.fragmented) {
            pending.poses=render::pack_continuous_fragments(pending.projection,pending.faces,pending.bsp,pose,settings,colour_warp);
        } else if(euler) {
            pending.poses.insert(pending.poses.end(),pending.projection.euler_operands.begin(),pending.projection.euler_operands.end());
            pending.poses[0].vanish[3]=pending.poses[1].vanish[3]=2.f;
        }
        const uint32_t vertices=static_cast<uint32_t>(pending.projection.continuous_vertices.size());
        const uint32_t visibility=static_cast<uint32_t>(pending.projection.visibility_faces.size());
        const bool residuals=(euler && !pending.fragmented) || settings.backface_culling
            || (pending.fragmented && (!pose.use_rotation_matrix || pose.subpixel_projection));
        pending.projection_parameters.assign(s.polygon_count,{float(pose.vanish_x),float(pose.vanish_y),float(settings.focal_length),2.f});
        pending.projection_settings={vertices,static_cast<uint32_t>(pending.poses.size()),residuals?(settings.backface_culling?2U:1U):0U,0};
        pending.visibility_settings={visibility,vertices,0,0};
        pending.bsp_settings={1,static_cast<uint32_t>(pending.bsp.nodes.size()),visibility,
            static_cast<uint32_t>(pending.bsp.face_ids.size()),s.count,0,0,0};
        pending.clip_settings={s.polygon_count,vertices,static_cast<uint32_t>(pending.faces.corners.size()),visibility,
            width,height,s.polygon_count,residuals?vertices:0};
        // Keep non-solid faces in the common index space for BSP output, but
        // clip zero corners: their native texture/line/sprite paths own them.
        // Removing entries here would remap every later ordered face ID.
        // Mark authored solid/texture overlays, including inset signs and
        // duplicated vertex records. Bit 4 belongs to graphics; compute ignores it.
        const auto surface=[&](size_t face) {
            DecalSurface key;
            key.reserve(pending.bsp.faces[face].vertex_indices.size());
            for(auto index:pending.bsp.faces[face].vertex_indices) {
                if(index>=pending.projection.continuous_vertices.size())
                    throw std::runtime_error("Source decal vertex is out of range");
                const auto& v=pending.projection.continuous_vertices[index];
                key.push_back({{v.x,v.y,v.z},v.pose});
            }
            return key;
        };
        std::vector<DecalSurface> solids;
        // Solid-only models cannot contain decals. Avoid building
        // surfaces on their per-frame input preparation path.
        const bool has_textures=std::any_of(pending.faces.materials.begin(),pending.faces.materials.end(),
            [](const auto& material){return material.textured!=0;});
        if(has_textures && !pending.fragmented) {
            for(size_t i=0;i<pending.faces.polygons.size();++i)
                if(pending.faces.primitives[i]==render::PackedPrimitive::polygon && !pending.faces.materials[i].textured)
                    solids.push_back(surface(i));
            for(size_t i=0;i<pending.faces.polygons.size();++i)
                if(pending.faces.primitives[i]==render::PackedPrimitive::polygon && pending.faces.materials[i].textured) {
                    const auto overlay=surface(i);
                    if(std::any_of(solids.begin(),solids.end(),[&](const auto& solid) {
                        return decal_surface_contains(solid,overlay);
                    })) pending.faces.polygons[i][3]|=16U;
                }
        }
        for(std::size_t i=0;!colour_warp && i<pending.faces.polygons.size();++i)
            if((pending.faces.primitives[i]!=render::PackedPrimitive::polygon
                && !(unclipped && pending.faces.primitives[i]==render::PackedPrimitive::line)
                && !(unclipped && pending.faces.primitives[i]==render::PackedPrimitive::sprite))
                || (!unclipped && pending.faces.materials[i].textured))
                pending.faces.polygons[i][1]=0;
        output=std::move(pending);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
}
