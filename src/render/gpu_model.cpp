#include "starfox/render/gpu_model.hpp"
#include "starfox/compat/bit_cast.hpp"
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/gpu_scene_counters.hpp"
#include "starfox/render/packed_projection.hpp"
#include "starfox/render/packed_faces.hpp"
#include "starfox/render/gpu_bsp.hpp"
#include "starfox/render/gpu_colour_warp.hpp"
#include "starfox/render/source_shading.hpp"
#include "starfox/render/gpu_clip.hpp"
#include "starfox/render/face_material.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <chrono>
#include <sstream>
#include <cstdio>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "gpu_model_timing_hook.hpp"
#include "model_host_clock.hpp"
#include "shaders/generated/surface_portable.hpp"
#include "shaders/generated/billboard_portable.hpp"
#endif
namespace starfox::render {
#include "gpu_model_source_pool.inc"
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {
struct ModelHostCost {
    std::uint32_t shape{},vertices{},polygons{};
    std::uint64_t pack{},prepare{},upload{},encode{},bytes{};
    std::array<ModelThreadCpuClock,4> cpu{};
    std::uint64_t batch{};unsigned draw_index{};
};
struct ModelHostCostTrace {
    static constexpr std::size_t budget=32768;
    std::vector<ModelHostCost> records;
    std::uint64_t dropped{};
    bool cpu_requested{};
    ModelHostCostTrace():cpu_requested(SDL_getenv("STARFOX_TRACE_MODEL_THREAD_COST")!=nullptr){records.reserve(budget);}
    void add(ModelHostCost value) {
        if(records.size()<budget) records.push_back(value);
        else ++dropped;
    }
    ~ModelHostCostTrace() {
        // Windows stderr flushing per draw substantially changes GPU queue
        // overlap. Dump bounded host-only diagnostics after rendering stops,
        // not inside the workload being measured. This is opt-in and is not
        // GPU timestamp data. Separate threads keep independent collectors.
        try {
            std::ostringstream output;
            output<<"model-host-cost-trace: deferred=1 count="<<records.size()<<" dropped="<<dropped<<'\n';
            if(cpu_requested) {
                const auto invalid=std::count_if(records.begin(),records.end(),[](const auto& value) {
                    return std::any_of(value.cpu.begin(),value.cpu.end(),[](auto sample){return !sample.valid;});
                });
                output<<"model-host-cpu-clock: provider="<<model_thread_cpu_provider()
                    <<" unit=100ns count="<<records.size()<<" invalid="<<invalid<<'\n';
            }
            for(const auto& value:records) {
                output<<"model-host-cost-us shape="<<value.shape<<" vertices="<<value.vertices<<" polygons="<<value.polygons
                    <<" pack="<<value.pack<<" prepare="<<value.prepare<<" upload="<<value.upload<<" encode="<<value.encode
                    <<" bytes="<<value.bytes<<'\n';
                if(cpu_requested) output<<"model-host-cpu-100ns shape="<<value.shape<<" vertices="<<value.vertices
                    <<" polygons="<<value.polygons<<" pack="<<value.cpu[0].ticks<<" prepare="<<value.cpu[1].ticks
                    <<" upload="<<value.cpu[2].ticks<<" encode="<<value.cpu[3].ticks
                    <<" batch="<<value.batch<<" index="<<value.draw_index<<'\n';
            }
            // One CRT-locked write instead of hundreds of thousands of stderr
            // flushes; also keeps separate thread collectors from interleaving.
            const auto text=output.str();std::fwrite(text.data(),1,text.size(),stderr);
        } catch(...) {std::fputs("model-host-cost-output: failed\n",stderr);}
    }
};
ModelHostCostTrace& model_host_cost_trace() {
    static thread_local ModelHostCostTrace trace;
    return trace;
}
}
#endif
struct GpuModel::Impl {
    std::string status{"GPU model rendering unavailable"};
    GpuModelUploadInfo last_upload{};
    // Upload copies consume these immediately; no command retains host pointers.
    // Reuse capacity across models instead of allocating twice per draw.
    std::vector<std::array<float,4>> near_scratch;
    std::vector<std::array<std::int32_t,4>> normal_scratch;
    GpuProjection projection;GpuBsp bsp;GpuClip clip;GpuRaster raster;GpuColourWarp warp,reflection_warp;GpuMsaa msaa;
    GpuProjection axis_ray_projection;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};
    std::array<SDL_GPUBuffer*,18> buffers{};std::array<std::uint32_t,18> capacities{};
    SDL_GPUComputePipeline* surface_pipeline{};SDL_GPUBuffer* surface_materials{};std::uint32_t surface_capacity{};
    SDL_GPUBuffer* geometry_planes{};std::uint32_t geometry_capacity{};
    SDL_GPUTransferBuffer* upload{};std::uint32_t upload_capacity{};
    // Compare actual packed bytes, never a shape pointer/hash or GPU result.
    // Valid only within an explicit ordered command-recording scope. A new
    // recording always uploads again, including after cancellation/address reuse.
    std::vector<Uint8> input_snapshot;
    std::array<std::uint32_t,18> snapshot_sizes{};
    // Only bytes actually uploaded to the model's own buffers are reusable.
    // Borrowed stereo sources have separate storage, even at an equal layout.
    std::array<bool,18> snapshot_owned{};
    bool upload_batch_active{},snapshot_valid{},fused_visibility_reported{},small_model_reported{};
    unsigned reported_wobble_modes{};
    std::array<std::uint64_t,4> wobble_span_draws{};
    // Finite test diagnostics: distinguish real polygon work from billboard-
    // only scenes. A missing span marker must never count as tracer coverage.
    std::array<std::uint64_t,3> model_path_draws{};
    static constexpr std::uint64_t snapshot_budget=1024U*1024;
    SDL_GPUComputePipeline* billboard_pipeline{};
    SDL_GPUBuffer* billboard_spans{};std::uint32_t billboard_rows{};
    std::array<SDL_GPUBuffer*,2> axis_ray_buffers{};
    std::array<std::uint32_t,2> axis_ray_capacities{};
    SDL_GPUTransferBuffer* axis_ray_upload{};std::uint32_t axis_ray_upload_capacity{};
    ~Impl(){release();}
    void release()noexcept {
        if(std::any_of(model_path_draws.begin(),model_path_draws.end(),[](auto count){return count!=0;}))
            std::cerr<<"model-path-counts: spans="<<model_path_draws[0]<<" billboards="<<model_path_draws[1]
                <<" empty="<<model_path_draws[2]<<'\n';
        model_path_draws={};
        if(std::any_of(wobble_span_draws.begin(),wobble_span_draws.end(),[](auto count){return count!=0;}))
            std::cerr<<"model-style-counts: normal="<<wobble_span_draws[0]<<" repeated="<<wobble_span_draws[1]
                <<" sparse="<<wobble_span_draws[2]<<" combined="<<wobble_span_draws[3]<<'\n';
        wobble_span_draws={};
        fused_visibility_reported=false;
        small_model_reported=false;
        reported_wobble_modes=0;
        projection.release_device();bsp.release_device();clip.release_device();raster.release_device();
        warp.release_device();
        reflection_warp.release_device();
        msaa.release_device();
        axis_ray_projection.release_device();
        for(auto* buffer:axis_ray_buffers) if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        if(axis_ray_upload) SDL_ReleaseGPUTransferBuffer(device,axis_ray_upload);
        axis_ray_buffers={};axis_ray_capacities={};axis_ray_upload=nullptr;axis_ray_upload_capacity=0;
        for(auto* buffer:buffers) if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(surface_pipeline) SDL_ReleaseGPUComputePipeline(device,surface_pipeline);
        if(surface_materials) SDL_ReleaseGPUBuffer(device,surface_materials);
        if(geometry_planes) SDL_ReleaseGPUBuffer(device,geometry_planes);
        if(billboard_pipeline) SDL_ReleaseGPUComputePipeline(device,billboard_pipeline);
        if(billboard_spans) SDL_ReleaseGPUBuffer(device,billboard_spans);
        billboard_pipeline=nullptr;billboard_spans=nullptr;billboard_rows=0;
        surface_pipeline=nullptr;surface_materials=nullptr;surface_capacity=0;
        geometry_planes=nullptr;geometry_capacity=0;
        device=nullptr;buffers={};capacities={};upload=nullptr;upload_capacity=0;
        snapshot_valid=false;snapshot_sizes={};std::vector<Uint8>().swap(input_snapshot);
    }
    static void require(bool value){if(!value) throw std::runtime_error(SDL_GetError());}
    void exploding_axis_casters(SDL_GPUCommandBuffer* command,const assets::Shape& shape,
        RenderPose pose,const RenderSettings& settings,GpuModelRaySource& out) {
        // Axis display deliberately suppresses fragment motion. Its shadow
        // still follows the exploding source faces, so use a separate GPU
        // transform stream instead of changing the visible laser's geometry.
        pose.collapse_to_axis_line=false;
        auto vertices=pack_projection(shape,pose,settings);
        const auto graph=pack_bsp(shape,true);
        const bool colour_warp=pose.colour_warp && !pose.force_colour;
        auto faces=colour_warp?pack_warp_faces(shape,graph,pose,settings):pack_faces(shape,graph,pose,settings);
        std::vector<ContinuousTransformPose> continuous_poses;
        std::vector<NativeTransformPose> native_poses;
        if(vertices.continuous) continuous_poses=pack_continuous_fragments(vertices,faces,graph,pose,settings,colour_warp);
        else {
            const auto original=std::move(vertices.native_vertices);
            vertices.native_vertices.clear();
            for(std::size_t f=0;f<graph.faces.size();++f) {
                const auto& face=graph.faces[f];
                native_poses.push_back(native_explosion_pose(vertices.native_pose,face.normal.x,face.normal.y,face.normal.z,pose.explosion_progress));
                const auto first=faces.polygons[f][0];
                for(std::size_t c=0;c<face.vertex_indices.size();++c) {
                    const auto index=face.vertex_indices[c];
                    NativeTransformVertex vertex{};vertex.pose=UINT32_MAX;
                    if(index<original.size()) {vertex=original[index];vertex.pose=std::uint32_t(f);}
                    faces.corners[first+c][0]=std::uint32_t(vertices.native_vertices.size());
                    vertices.native_vertices.push_back(vertex);
                }
            }
        }
        const auto point_count=vertices.continuous?vertices.continuous_vertices.size():vertices.native_vertices.size();
        const auto pose_count=vertices.continuous?continuous_poses.size():native_poses.size();
        if(!point_count || !pose_count || point_count>1'000'000 || pose_count>1'000'000)
            throw std::runtime_error("Exploding axis caster storage limit exceeded");
        const std::array<Uint32,2> sizes{Uint32(point_count*16),Uint32(pose_count*80)};
        const std::array<const void*,2> data{vertices.continuous?static_cast<const void*>(vertices.continuous_vertices.data()):vertices.native_vertices.data(),
            vertices.continuous?static_cast<const void*>(continuous_poses.data()):native_poses.data()};
        for(unsigned i=0;i<2;++i) if(axis_ray_capacities[i]<sizes[i]) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
            auto* buffer=SDL_CreateGPUBuffer(device,&info);require(buffer);
            if(axis_ray_buffers[i]) SDL_ReleaseGPUBuffer(device,axis_ray_buffers[i]);
            axis_ray_buffers[i]=buffer;axis_ray_capacities[i]=sizes[i];
        }
        const auto bytes=sizes[0]+sizes[1];
        if(axis_ray_upload_capacity<bytes) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0};
            auto* upload=SDL_CreateGPUTransferBuffer(device,&info);require(upload);
            if(axis_ray_upload) SDL_ReleaseGPUTransferBuffer(device,axis_ray_upload);
            axis_ray_upload=upload;axis_ray_upload_capacity=bytes;
        }
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,axis_ray_upload,true));require(mapped);
        std::memcpy(mapped,data[0],sizes[0]);std::memcpy(mapped+sizes[0],data[1],sizes[1]);
        SDL_UnmapGPUTransferBuffer(device,axis_ray_upload);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(unsigned i=0;i<2;++i) {
            SDL_GPUTransferBufferLocation from{axis_ray_upload,i?sizes[0]:0};
            SDL_GPUBufferRegion to{axis_ray_buffers[i],0,sizes[i]};scene_counters::upload_buffer(copy,&from,&to,true);
        }
        SDL_EndGPUCopyPass(copy);
        if(vertices.continuous) out.points=axis_ray_projection.enqueue_continuous(device,command,axis_ray_buffers[0],Uint32(point_count),axis_ray_buffers[1],Uint32(pose_count),&out.residuals);
        else {
            auto* projected=axis_ray_projection.enqueue_transformed(device,command,axis_ray_buffers[0],Uint32(point_count),axis_ray_buffers[1],Uint32(pose_count),&out.points);
            if(!projected) throw std::runtime_error(axis_ray_projection.status());
        }
        if(!out.points) throw std::runtime_error(axis_ray_projection.status());
        out.point_count=Uint32(point_count);out.mode=vertices.continuous?(out.residuals?2U:1U):0U;
        for(std::size_t f=0;f<faces.polygons.size();++f) if(faces.primitives[f]==PackedPrimitive::polygon) {
            const auto first=faces.polygons[f][0],count=faces.polygons[f][1];
            for(Uint32 c=1;c+1<count;++c) out.triangles.push_back({faces.corners[first][0],faces.corners[first+c][0],faces.corners[first+c+1][0],Uint32(f)});
        }
    }
    GpuRasterOutput billboard(void* next_device,void* command,const assets::Shape& shape,
        const RenderPose& pose,const RenderSettings& settings,std::uint32_t width,std::uint32_t height,
        bool surfaces,const GpuRasterOutput* background,std::array<std::uint32_t,2> raster_size={},std::array<float,2> jitter={},bool depth=false,std::uint32_t painter_flags=0,bool in_place_background=false,
        const GpuPreparedModelSource* gpu_source=nullptr,bool bounded=false,bool compact_tiles=false) {
        auto* next=static_cast<SDL_GPUDevice*>(next_device);
        if(device!=next){release();device=next;}
        const auto scale=settings.render_scale;
        const auto* texture=texture_for_colour(shape,pose.simple_sprite_colour,pose.colour_frame);
        if(!texture || pose.simple_sprite_world_size<=0 || pose.z<128)
            return raster.enqueue_row_spans(device,command,nullptr,0,width*scale,height*scale,surfaces,nullptr,true,background,false,0,0,0,nullptr,raster_size,jitter,painter_flags,in_place_background,bounded,compact_tiles);
        for(auto value:{pose.x,pose.y,pose.z,pose.vanish_x,pose.vanish_y})
            if(!std::isfinite(value) || std::abs(value)>1000000)
                throw std::runtime_error("GPU billboard coordinate exceeds compensated range");
        if(!std::isfinite(settings.focal_length) || settings.focal_length<0 || settings.focal_length>4096
            || std::abs(std::int64_t(pose.effect_clip_left))>1000000 || std::abs(std::int64_t(pose.effect_clip_right))>1000000)
            throw std::runtime_error("Invalid GPU billboard focal length or clip");
        const auto required=(std::uint64_t(texture->u_mask)+1)*(std::uint64_t(texture->v_mask)+1);
        if(required>texture->texels.size() || texture->texels.size()>256U*1024*1024)
            throw std::runtime_error("Invalid GPU billboard texture size");
        const auto bytes=std::uint32_t((texture->texels.size()+3)&~std::size_t(3));
        const bool shared=gpu_source && gpu_source->device_==device && gpu_source->encoded_ && *gpu_source->encoded_
            && gpu_source->source_.billboard_texture==texture && gpu_source->buffers_[4] && gpu_source->sizes_[4]==bytes;
        auto* texels=shared?static_cast<SDL_GPUBuffer*>(gpu_source->buffers_[4]):nullptr;
        if(!shared && capacities[9]<bytes) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,bytes,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(buffers[9]) SDL_ReleaseGPUBuffer(device,buffers[9]);
            buffers[9]=replacement;capacities[9]=bytes;
        }
        if(!shared) texels=buffers[9];
        const auto texture_upload_bytes=shared?0U:bytes;
        const auto upload_bytes=texture_upload_bytes+(depth?16U:0U);
        if(depth && geometry_capacity<1) {
            // This cache is shared with polygon plane generation, which may
            // reuse the one-plane allocation on a later draw.
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,16,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(geometry_planes) SDL_ReleaseGPUBuffer(device,geometry_planes);
            geometry_planes=replacement;geometry_capacity=1;
        }
        if(upload_capacity<upload_bytes) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,upload_bytes,0};
            auto* replacement=SDL_CreateGPUTransferBuffer(device,&info);require(replacement);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            upload=replacement;upload_capacity=upload_bytes;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        if(upload_bytes) {
            auto* mapped=SDL_MapGPUTransferBuffer(device,upload,true);require(mapped);
            if(!shared) {
                std::memset(mapped,0,bytes);std::memcpy(mapped,texture->texels.data(),texture->texels.size());
            }
            if(depth) {
                const std::array<float,4> plane{0,0,1,float(pose.z)};
                std::memcpy(static_cast<std::uint8_t*>(mapped)+texture_upload_bytes,plane.data(),16);
            }
            SDL_UnmapGPUTransferBuffer(device,upload);
            auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
            if(!shared) {
                SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{buffers[9],0,bytes};
                scene_counters::upload_buffer(copy,&from,&to,true);
                // Independent billboard uploads replace polygon input buffer 9.
                snapshot_valid=false;
            }
            if(depth) {
                SDL_GPUTransferBufferLocation plane_from{upload,texture_upload_bytes};
                SDL_GPUBufferRegion plane_to{geometry_planes,0,16};
                scene_counters::upload_buffer(copy,&plane_from,&plane_to,true);
            }
            SDL_EndGPUCopyPass(copy);
        }
        last_upload={bytes+(depth?16U:0U),upload_bytes,(shared?0U:1U)+(depth?1U:0U),0};
        if(shared) {last_upload.shared_bytes=bytes;last_upload.shared_buffers=1;}
        if(!billboard_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            if(spirv) {
                info.format=SDL_GPU_SHADERFORMAT_SPIRV;
                info.code=billboard_shader::spirv;info.code_size=sizeof(billboard_shader::spirv);
            }
#if defined(_WIN32)
            else if(dxil) {
                info.format=SDL_GPU_SHADERFORMAT_DXIL;
                info.code=billboard_shader::dxil;info.code_size=sizeof(billboard_shader::dxil);
            }
#endif
#if defined(__APPLE__)
            else {
                info.format=SDL_GPU_SHADERFORMAT_MSL;
                info.code=reinterpret_cast<const Uint8*>(billboard_shader::metal);info.code_size=std::strlen(billboard_shader::metal);
            }
#else
            else throw std::runtime_error("No supported billboard shader format");
#endif
            info.entrypoint=(spirv||dxil)?"main":"main0";info.num_uniform_buffers=1;info.num_readwrite_storage_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            billboard_pipeline=create_gpu_compute_pipeline(device,&info);require(billboard_pipeline);
        }
        const auto rows=height*scale;
        if(billboard_rows<rows) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,rows*96,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(billboard_spans) SDL_ReleaseGPUBuffer(device,billboard_spans);
            billboard_spans=replacement;billboard_rows=rows;
        }
        struct Settings {
            std::array<Uint32,4> camera_lo{},camera_hi{},view_lo{},view_hi{};
            std::array<std::int32_t,4> bounds{};
            std::array<Uint32,4> material{};
        } config;
        static_assert(sizeof(Settings)==96);
        const std::array<double,4> camera{pose.x,pose.y,pose.z,settings.focal_length};
        const std::array<double,4> view{pose.vanish_x,pose.vanish_y,double(pose.simple_sprite_world_size),double(scale)};
        for(unsigned i=0;i<4;++i) {
            const auto camera_bits=starfox::bit_cast<std::uint64_t>(camera[i]);
            const auto view_bits=starfox::bit_cast<std::uint64_t>(view[i]);
            config.camera_lo[i]=Uint32(camera_bits);config.camera_hi[i]=Uint32(camera_bits>>32);
            config.view_lo[i]=Uint32(view_bits);config.view_hi[i]=Uint32(view_bits>>32);
        }
        config.bounds={pose.effect_clip_left,pose.effect_clip_right,int(width*scale),int(rows)};
        config.material={texture->u_mask,texture->v_mask,settings.colour_index_base,pose.palette_override?256U+*pose.palette_override:0U};
        SDL_PushGPUComputeUniformData(cmd,0,&config,sizeof(config));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=billboard_spans;binding.cycle=true;
        auto* pass=scene_counters::begin_compute_pass(cmd,nullptr,0,&binding,1);require(pass);
        SDL_BindGPUComputePipeline(pass,billboard_pipeline);SDL_DispatchGPUCompute(pass,(rows+63)/64,1,1);SDL_EndGPUComputePass(pass);
        const GpuGeometryDepthInput plane{geometry_planes,1,float(settings.focal_length*scale),float(settings.focal_length*scale),float(pose.vanish_x*scale),float(pose.vanish_y*scale),true};
        // Sprites remain unlit; their plane identifier is only a temporal guide.
        return raster.enqueue_row_spans(device,command,billboard_spans,1,width*scale,rows,surfaces,texels,true,background,false,0,0,0,depth?&plane:nullptr,raster_size,jitter,painter_flags,in_place_background,bounded,compact_tiles);
    }
    struct SurfaceSettings {
        Uint32 count,points,corners,fractional_camera;
        std::array<std::int32_t,4> row0{},row1{},row2{};
        Uint32 fractional_normal{},want_geometry{},padding[2]{};
    };
    static_assert(sizeof(SurfaceSettings)==80);
    void* emit_surface(SDL_GPUCommandBuffer* command,void* camera,const SurfaceSettings& settings,
        const std::array<SDL_GPUBuffer*,18>& inputs_for_draw) {
        if(!surface_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            if(spirv) {
                info.format=SDL_GPU_SHADERFORMAT_SPIRV;
                info.code=surface_shader::spirv;info.code_size=sizeof(surface_shader::spirv);
            }
#if defined(_WIN32)
            else if(dxil) {
                info.format=SDL_GPU_SHADERFORMAT_DXIL;
                info.code=surface_shader::dxil;info.code_size=sizeof(surface_shader::dxil);
            }
#endif
#if defined(__APPLE__)
            else {
                info.format=SDL_GPU_SHADERFORMAT_MSL;
                info.code=reinterpret_cast<const Uint8*>(surface_shader::metal);info.code_size=std::strlen(surface_shader::metal);
            }
#else
            else throw std::runtime_error("No supported surface shader format");
#endif
            info.entrypoint=(spirv||dxil)?"main":"main0";info.num_uniform_buffers=1;
            info.num_readonly_storage_buffers=5;info.num_readwrite_storage_buffers=2;
            info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
            surface_pipeline=create_gpu_compute_pipeline(device,&info);require(surface_pipeline);
        }
        if(surface_capacity<settings.count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,settings.count*96,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(surface_materials) SDL_ReleaseGPUBuffer(device,surface_materials);
            surface_materials=replacement;surface_capacity=settings.count;
        }
        const auto plane_count=settings.want_geometry?settings.count:1U;
        if(geometry_capacity<plane_count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,plane_count*16,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(geometry_planes) SDL_ReleaseGPUBuffer(device,geometry_planes);
            geometry_planes=replacement;geometry_capacity=plane_count;
        }
        SDL_PushGPUComputeUniformData(command,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding bindings[2]{};
        bindings[0].buffer=surface_materials;bindings[1].buffer=geometry_planes;
        bindings[0].cycle=bindings[1].cycle=true;
        auto* pass=scene_counters::begin_compute_pass(command,nullptr,0,bindings,2);require(pass);
        SDL_BindGPUComputePipeline(pass,surface_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(camera),inputs_for_draw[6],inputs_for_draw[7],inputs_for_draw[8],inputs_for_draw[11]};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,5);SDL_DispatchGPUCompute(pass,(settings.count+31)/32,1,1);SDL_EndGPUComputePass(pass);
        return surface_materials;
    }
#endif
};
GpuModel::GpuModel():impl_(std::make_unique<Impl>()){}
GpuModel::~GpuModel()=default;
const std::string& GpuModel::status()const noexcept{return impl_->status;}
GpuModelUploadInfo GpuModel::upload_info()const noexcept {
    auto result=impl_->last_upload;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    result.snapshot_bytes=impl_->input_snapshot.capacity();
#endif
    return result;
}
void GpuModel::begin_upload_batch()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->upload_batch_active=true;impl_->snapshot_valid=false;
#endif
}
void GpuModel::end_upload_batch()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->upload_batch_active=false;impl_->snapshot_valid=false;
#endif
}
bool GpuModel::output_aliases(const GpuRasterOutput& input) const noexcept {
    return impl_->raster.output_aliases(input);
}
void GpuModel::release_device()noexcept {
    end_upload_batch();impl_->last_upload={};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
GpuRasterOutput GpuModel::enqueue(void* device,void* command,const assets::Shape& shape,const RenderPose& unjittered_pose,
    const RenderSettings& settings,std::uint32_t width,std::uint32_t height,bool surface_metadata,const GpuRasterOutput* background,GpuModelDiagnostics* diagnostics,bool geometry_depth,GpuModelRaySource* ray_source,const RenderPose* previous_pose,std::array<float,2> raster_jitter,std::array<std::uint32_t,2> raster_size,GpuMsaaFaces* msaa_faces,unsigned msaa_samples,
    std::optional<GpuProjection::MotionSurfaceSettings>* deferred_motion,std::uint32_t painter_flags,bool in_place_background,const PreparedBspSource* source_topology,
    const PreparedProjectionSource* source_projection,const GpuPreparedModelSource* gpu_source,const PreparedFacesSource* source_faces,const PreparedRayTopology* source_rays,bool bounded_raster,bool compact_tiles) {
    if(msaa_faces) *msaa_faces={};
    impl_->last_upload={};
    if(deferred_motion) deferred_motion->reset();
    auto pose=unjittered_pose;
    if(diagnostics) *diagnostics={};
    if(ray_source) {const bool requested=ray_source->request_materials,reference=ray_source->reference_materials;
        *ray_source={};ray_source->request_materials=requested;ray_source->reference_materials=reference;}
    geometry_depth=geometry_depth || previous_pose;
    // Temporal depth must not opt unlit native shadows/helpers into lighting.
    // Plane preparation can run without publishing effects surface metadata.
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(SDL_getenv("STARFOX_TRACE_GPU_MODEL_DISPATCH")) std::cerr<<"model-enqueue: "<<shape.name<<'\n';
        if(!device || !command || !width || !height || width>32767 || height>32767 || settings.render_scale<1 || settings.render_scale>max_gpu_render_scale)
            throw std::runtime_error("Invalid GPU model input");
        if(previous_pose && background)
            throw std::runtime_error("Temporal model draws must be merged after motion generation");
        const auto emit_motion=[&](GpuRasterOutput& output,const GpuProjection::MotionSurfaceSettings& motion) {
            if(deferred_motion) {
                if(!GpuProjection::valid_motion_surface_settings(motion))
                    throw std::runtime_error("Invalid per-pixel motion settings");
                *deferred_motion=motion;
            } else {
                output.motion=impl_->projection.enqueue_motion_surface(device,command,output.geometry_depth,motion);
                if(!output.motion) throw std::runtime_error(impl_->projection.status());
            }
        };
        const bool custom_raster=raster_size[0] || raster_size[1];
        if(custom_raster && (!raster_size[0] || !raster_size[1] || raster_size[0]>32767 || raster_size[1]>32767
            || !pose.continuous_geometry || !pose.subpixel_projection || pose.wave_mode))
            throw std::runtime_error("Custom model raster requires continuous geometry without wave effects");
        const auto raster_width=custom_raster?raster_size[0]:width*settings.render_scale;
        const auto raster_height=custom_raster?raster_size[1]:height*settings.render_scale;
        const double scale_x=double(raster_width)/width,scale_y=double(raster_height)/height;
        if(!std::isfinite(raster_jitter[0]) || !std::isfinite(raster_jitter[1]))
            throw std::runtime_error("Invalid model raster jitter");
        if(!pose.simple_scaled_sprite && (raster_jitter[0]!=0 || raster_jitter[1]!=0)) {
            if(!pose.continuous_geometry || !pose.subpixel_projection)
                throw std::runtime_error("Model jitter requires continuous subpixel projection");
            pose.vanish_x+=raster_jitter[0]/scale_x;
            pose.vanish_y+=raster_jitter[1]/scale_y;
        }
        if(pose.simple_scaled_sprite) {
            auto output=impl_->billboard(device,command,shape,pose,settings,width,height,surface_metadata,background,raster_size,raster_jitter,geometry_depth,painter_flags,in_place_background,gpu_source,bounded_raster,compact_tiles);
            if(!output.pixels) throw std::runtime_error(impl_->raster.status());
            if(previous_pose && output.geometry_depth && !background
                && previous_pose->simple_scaled_sprite
                && texture_for_colour(shape,previous_pose->simple_sprite_colour,previous_pose->colour_frame)
                    ==texture_for_colour(shape,pose.simple_sprite_colour,pose.colour_frame)) {
                // Match the source's rounded sprite rectangles, not a rigid
                // model transform: billboards always face the camera.
                const auto rectangle=[&](const RenderPose& p)->std::optional<std::array<float,4>> {
                    for(double v:{p.x,p.y,p.z,p.vanish_x,p.vanish_y})
                        if(!std::isfinite(v) || std::abs(v)>1000000) return {};
                    if(p.z<128 || p.simple_sprite_world_size<=0) return {};
                    const auto size=std::clamp(std::trunc(double(p.simple_sprite_world_size)*settings.focal_length/p.z),0.,240.);
                    if(size<1) return {};
                    const auto left=std::round(p.vanish_x)+std::trunc(p.x*settings.focal_length/p.z)-int(size)/2;
                    const auto top=std::round(p.vanish_y)+std::trunc(p.y*settings.focal_length/p.z)-int(size)/2;
                    return std::array<float,4>{float(size*scale_x),float(size*scale_y),float(left*scale_x),float(top*scale_y)};
                };
                const auto current=rectangle(pose),previous=rectangle(*previous_pose);
                if(current && previous) {
                    GpuProjection::MotionSurfaceSettings motion;
                    motion.width=output.width;motion.height=output.height;
                    std::copy(current->begin(),current->end(),motion.current_projection);
                    std::copy(previous->begin(),previous->end(),motion.previous_projection);
                    // Normalized coordinates within each rectangle correspond
                    // across depth/size changes; retain current Z for validity.
                    motion.jitter_x=raster_jitter[0];motion.jitter_y=raster_jitter[1];
                    emit_motion(output,motion);
                }
            }
            if(SDL_getenv("STARFOX_TEST_MODEL_PATH_RESULT")) ++impl_->model_path_draws[1];
            impl_->status="Whole-object billboard GPU resident";return output;
        }
        // Diagnostic host costs only. Exclude stderr from the measured stages;
        // this does not time GPU execution, submit work or read back an image.
        const bool trace_model_cost=SDL_getenv("STARFOX_TRACE_MODEL_COST")!=nullptr;
        const bool trace_model_cpu=trace_model_cost && SDL_getenv("STARFOX_TRACE_MODEL_THREAD_COST")!=nullptr;
        auto* cost_trace=trace_model_cost?&model_host_cost_trace():nullptr;
        const auto cost_now=[&]{return model_host_clock_now(trace_model_cost,trace_model_cpu);};
        const auto cost_begin=cost_now();
        auto vertices=pack_projection(shape,pose,settings,source_projection);
        const auto source_vertex_count=std::uint32_t(vertices.continuous?vertices.continuous_input().size():vertices.native_input().size());
        // These paths expand/replace source arrays. Ordinary rigid draws keep
        // immutable borrowed views and package only their own transform constants.
        if(pose.explosion_progress || pose.collapse_to_axis_line) vertices.own_source();
        const auto axis_groups=pose.collapse_to_axis_line?pack_axis_groups(shape,pose.animation_frame)
            :std::array<std::vector<std::uint32_t>,2>{};
        const bool axis=pose.collapse_to_axis_line && !axis_groups[0].empty() && !shape.faces.empty();
        const bool colour_warp=pose.colour_warp && !pose.force_colour;
        auto material_pose=pose;material_pose.collapse_to_axis_line=false;
        // Collapsed axes bypass the source BSP entirely, just like the software
        // renderer. An unused source graph must not reject an otherwise valid line.
        PackedBsp local_graph;
        std::vector<std::uint32_t> axis_indices;std::uint32_t axis_ranges[4]{};
        if(axis) {
            for(unsigned group=0;group<2;++group) {
                if(axis_groups[group].size()>65536) throw std::runtime_error("Axis group exceeds reduction budget");
                axis_ranges[group*2]=std::uint32_t(axis_indices.size());axis_ranges[group*2+1]=std::uint32_t(axis_groups[group].size());
                axis_indices.insert(axis_indices.end(),axis_groups[group].begin(),axis_groups[group].end());
            }
            assets::Shape topology;topology.faces={shape.faces.front()};
            topology.faces[0].vertex_indices={0,1};topology.faces[0].sprite=false;topology.faces[0].visibility_index=-1;
            local_graph=pack_bsp(topology);vertices.visibility_faces={{{UINT32_MAX,UINT32_MAX,UINT32_MAX,1}}};
        } else if(source_topology) {
            if(!source_topology->matches(shape,pose.explosion_progress!=0))
                throw std::runtime_error("Prepared model topology belongs to a different source/policy");
        } else local_graph=pack_bsp(shape,pose.explosion_progress!=0);
        const auto& graph=source_topology && !axis?source_topology->graph():local_graph;
        if(source_faces && (!source_topology || !source_faces->matches(shape,*source_topology,pose,settings)))
            throw std::runtime_error("Prepared faces belong to a different source/material state");
        if(ray_source && source_rays && (!source_faces || !source_rays->matches(*source_faces,ray_source->request_materials)))
            throw std::runtime_error("Prepared ray topology belongs to a different source/material policy");
        PackedFaces local_faces;
        if(!source_faces) local_faces=colour_warp?pack_warp_faces(shape,graph,material_pose,settings):pack_faces(shape,graph,material_pose,settings);
        PackedWarpTextures warp_textures;
        std::array<std::uint8_t,2480> warp_diffuse{};
        GpuWarpSettings warp_settings{};
        if(colour_warp) {
            warp_textures=pack_warp_textures(shape);local_faces.texels=warp_textures.texels;
            const auto shading=pack_warp_shading(shape,pose,settings,axis);
            warp_settings=shading.settings;warp_diffuse=shading.diffuse;
        }
        if(axis) {
            local_faces.polygons[0][2]=0;local_faces.polygons[0][3]=2;
            local_faces.materials[0].textured=0;local_faces.materials[0].tag=std::uint32_t(PixelLayer::three_d);
        }
        std::vector<NativeTransformPose> destruction_poses;
        std::vector<ContinuousTransformPose> continuous_destruction_poses;
        if(pose.explosion_progress && vertices.continuous && !axis) {
            continuous_destruction_poses=pack_continuous_fragments(vertices,local_faces,graph,pose,settings,colour_warp);
        }
        if(pose.explosion_progress && !vertices.continuous && !axis) {
            const auto original=std::move(vertices.native_vertices);
            vertices.native_vertices.clear();vertices.native_vertices.reserve(local_faces.corners.size()+graph.faces.size());
            destruction_poses.reserve(graph.faces.size()+1);
            vertices.visibility_faces={{{UINT32_MAX,UINT32_MAX,UINT32_MAX,1}}};
            vertices.visibility_faces.reserve(graph.faces.size()+1);
            for(std::size_t f=0;f<graph.faces.size();++f) {
                const auto& face=graph.faces[f];
                destruction_poses.push_back(native_explosion_pose(vertices.native_pose,
                    face.normal.x,face.normal.y,face.normal.z,pose.explosion_progress));
                auto& polygon=local_faces.polygons[f];polygon[2]=0;
                if(face.sprite && (colour_warp || local_faces.materials[f].textured) && face.vertex_indices.size()==1) {
                    const auto source=face.vertex_indices[0];
                    std::uint32_t centre=UINT32_MAX;
                    if(source<original.size()) {
                        auto vertex=original[source];
                        // The final pose is an unmodified object transform.
                        vertex.pose=std::uint32_t(graph.faces.size());
                        centre=std::uint32_t(vertices.native_vertices.size());
                        vertices.native_vertices.push_back(vertex);
                    }
                    polygon[2]=std::uint32_t(vertices.visibility_faces.size());
                    vertices.visibility_faces.push_back({centre,0,0,3});
                }
                for(std::size_t j=0;j<face.vertex_indices.size();++j) {
                    const auto source=face.vertex_indices[j];
                    auto& corner=local_faces.corners[polygon[0]+j];
                    if(source>=original.size()) {polygon[1]=0;corner[0]=UINT32_MAX;continue;}
                    auto vertex=original[source];vertex.pose=std::uint32_t(f);
                    corner[0]=std::uint32_t(vertices.native_vertices.size());vertices.native_vertices.push_back(vertex);
                }
            }
            destruction_poses.push_back(vertices.native_pose);
        }
        const auto& faces=source_faces?source_faces->faces():local_faces;
        const auto vertex_count=std::uint32_t(vertices.continuous?vertices.continuous_input().size():vertices.native_input().size());
        if(!vertex_count || faces.polygons.empty()) {
            auto output=impl_->raster.enqueue_row_spans(device,command,nullptr,0,raster_width,raster_height,surface_metadata,nullptr,true,background,false,0,0,0,nullptr,{},{},painter_flags,in_place_background,bounded_raster,compact_tiles);
            if(!output.pixels) throw std::runtime_error(impl_->raster.status());
            if(SDL_getenv("STARFOX_TEST_MODEL_PATH_RESULT")) ++impl_->model_path_draws[2];
            impl_->status="Empty GPU model cleared resident";return output;
        }
        const bool sequential_euler=vertices.continuous && !pose.use_rotation_matrix && !pose.explosion_progress;
        if(sequential_euler) {
            continuous_destruction_poses.assign(vertices.continuous_poses.begin(),vertices.continuous_poses.end());
            continuous_destruction_poses.insert(continuous_destruction_poses.end(),vertices.euler_operands.begin(),vertices.euler_operands.end());
            for(unsigned kind=0;kind<2;++kind) continuous_destruction_poses[kind].vanish[3]=2.f;
        }
        const auto polygons=std::uint32_t(faces.polygons.size()),slots=std::max(1U,graph.output_capacity);
        const auto visibility_count=std::uint32_t(vertices.visibility_input().size());
        const std::array<std::uint32_t,4> tree{graph.root,0,slots,graph.work_limit};
        // Keep source-matrix fragment projection residuals through viewport
        // clipping. Euler packets retain their compensated projected result.
        const bool accurate_fragment_clip=axis || (pose.explosion_progress && vertices.continuous && pose.use_rotation_matrix && !pose.subpixel_projection);
        auto& near=impl_->near_scratch;
        near.assign(colour_warp?slots:polygons,{float(pose.vanish_x),float(pose.vanish_y),float(settings.focal_length),accurate_fragment_clip?1.f:(vertices.continuous?2.f:0.f)});
        auto& normal_scratch=impl_->normal_scratch;
        if(!source_topology || axis) {
            normal_scratch.clear();normal_scratch.reserve(graph.faces.size());
            for(const auto& face:graph.faces) normal_scratch.push_back({face.normal.x,face.normal.y,face.normal.z,0});
        }
        const auto& normals=source_topology && !axis?source_topology->normals():normal_scratch;
        std::array<Uint8,4> short_texels{};
        if(faces.texels.size()<short_texels.size())
            std::copy(faces.texels.begin(),faces.texels.end(),short_texels.begin());
        std::array<const void*,18> data{vertices.continuous?static_cast<const void*>(vertices.continuous_input().data()):vertices.native_input().data(),
            vertices.continuous?static_cast<const void*>(continuous_destruction_poses.empty()?vertices.continuous_poses.data():continuous_destruction_poses.data()):destruction_poses.empty()?&vertices.native_pose:destruction_poses.data(),
            vertices.visibility_input().data(),graph.nodes.data(),graph.face_ids.data(),tree.data(),faces.corners.data(),faces.polygons.data(),
            faces.materials.data(),faces.texels.size()<short_texels.size()?static_cast<const void*>(short_texels.data()):faces.texels.data(),near.data(),axis?static_cast<const void*>(axis_indices.data()):normals.data(),
            warp_diffuse.data(),pose.depth_colour_tables.data(),warp_textures.lookup.data(),warp_textures.textures.data(),warp_textures.coordinates.data(),normals.data()};
        const auto native_pose_count=std::uint32_t(std::max(std::size_t(1),destruction_poses.size()));
        const auto continuous_pose_count=std::uint32_t(std::max(std::size_t(4),continuous_destruction_poses.size()));
        const bool small_model=vertices.continuous && !axis && !pose.explosion_progress
            && vertex_count<=128 && visibility_count<=128
            && SDL_getenv("STARFOX_TEST_FUSED_SMALL_MODEL") && !SDL_getenv("STARFOX_TEST_SEPARATE_SMALL_MODEL");
        // Fewer storage-copy commands did not establish a reliable frame-time
        // gain under loaded ABBA tests. Keep this transport experimental.
        const bool inline_pose=SDL_getenv("STARFOX_TEST_INLINE_MODEL_POSES")
            && !small_model && !axis && !pose.explosion_progress
            && !pose.collapse_to_axis_line && !SDL_getenv("STARFOX_TEST_BUFFERED_MODEL_POSES")
            && (vertices.continuous?continuous_pose_count<=6:native_pose_count==1);
        std::array<std::uint32_t,18> sizes{vertex_count*16,vertices.continuous?continuous_pose_count*80U:native_pose_count*80U,visibility_count*16,
            std::uint32_t(graph.nodes.size()*32),std::uint32_t(graph.face_ids.size()*4),16,std::uint32_t(faces.corners.size()*16),polygons*16,
            polygons*96,std::uint32_t(std::max(std::size_t(4),faces.texels.size())),std::uint32_t(near.size()*16),axis?std::uint32_t(axis_indices.size()*4):polygons*16,
            colour_warp?2480U:0U,colour_warp?128U:0U,std::uint32_t(warp_textures.lookup.size()*4),std::uint32_t(warp_textures.textures.size()*16),std::uint32_t(warp_textures.coordinates.size()*8),colour_warp&&axis?polygons*16:0U};
        const auto inline_pose_bytes=inline_pose?sizes[1]:0U;
        if(inline_pose) sizes[1]=0; // The exact same poses live in this command's constants.
        const auto cost_packed=cost_now();
        auto* next=static_cast<SDL_GPUDevice*>(device);
        if(impl_->device!=next){impl_->release();impl_->device=next;}
        std::uint64_t total=0;for(auto size:sizes) total+=size;
        if(total>256U*1024*1024) throw std::runtime_error("GPU model upload exceeds budget");
        auto b=impl_->buffers;
        std::array<bool,18> shared{};
        if(gpu_source && gpu_source->device_==device && gpu_source->encoded_ && *gpu_source->encoded_) {
            const std::array<unsigned,4> indices{0,2,3,4};
            const bool projection=source_projection && vertices.source==source_projection
                && gpu_source->source_.projection==source_projection;
            const bool topology=source_topology && !axis && gpu_source->source_.topology==source_topology;
            for(unsigned slot=0;slot<indices.size();++slot) {
                const auto i=indices[slot];
                if((slot<2?projection:topology) && gpu_source->buffers_[slot]
                    && sizes[i]==gpu_source->sizes_[slot]) {
                    b[i]=static_cast<SDL_GPUBuffer*>(gpu_source->buffers_[slot]);shared[i]=true;
                    impl_->last_upload.shared_bytes+=sizes[i];
                    if(sizes[i]) ++impl_->last_upload.shared_buffers;
                }
            }
            if(source_faces && gpu_source->source_.faces==source_faces) {
                const std::array<unsigned,4> face_indices{6,7,8,9};
                for(unsigned slot=0;slot<face_indices.size();++slot) {
                    const auto i=face_indices[slot],source_slot=slot+5;
                    if(gpu_source->buffers_[source_slot] && sizes[i]==gpu_source->sizes_[source_slot]) {
                        b[i]=static_cast<SDL_GPUBuffer*>(gpu_source->buffers_[source_slot]);shared[i]=true;
                        impl_->last_upload.shared_bytes+=sizes[i];
                        if(sizes[i]) ++impl_->last_upload.shared_buffers;
                    }
                }
            }
        }
        for(unsigned i=0;i<sizes.size();++i) if(!shared[i] && !(inline_pose && i==1) && impl_->capacities[i]<std::max(4U,sizes[i])) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,std::max(4U,sizes[i]),0};
            auto* replacement=SDL_CreateGPUBuffer(next,&info);Impl::require(replacement);
            if(impl_->buffers[i]) SDL_ReleaseGPUBuffer(next,impl_->buffers[i]);
            impl_->buffers[i]=replacement;impl_->capacities[i]=info.size;
            impl_->snapshot_valid=false;
        }
        for(unsigned i=0;i<sizes.size();++i) if(!shared[i]) b[i]=impl_->buffers[i];
        // Borrowed source slots do not seed their owned allocations. They must
        // not disable reuse of the remaining, actually uploaded pose/material/
        // texture buffers either. A later shared -> owned switch uploads every
        // formerly borrowed slot before it can become a cached input.
        const bool memoize=impl_->upload_batch_active && total<=Impl::snapshot_budget;
        const bool compare=memoize && impl_->snapshot_valid && impl_->snapshot_sizes==sizes;
        std::array<bool,18> changed{};std::uint32_t upload_bytes=0;
        std::size_t snapshot_offset=0;
        for(unsigned i=0;i<sizes.size();++i) {
            changed[i]=sizes[i] && !shared[i] && (!compare || !impl_->snapshot_owned[i]
                || std::memcmp(data[i],impl_->input_snapshot.data()+snapshot_offset,sizes[i])!=0);
            if(changed[i]) {upload_bytes+=sizes[i];++impl_->last_upload.uploaded_buffers;}
            else if(sizes[i]) ++impl_->last_upload.reused_buffers;
            snapshot_offset+=sizes[i];
        }
        impl_->last_upload.input_bytes=total+inline_pose_bytes;impl_->last_upload.uploaded_bytes=upload_bytes;
        if(inline_pose) {
            impl_->last_upload.pose_uniform_input_bytes=inline_pose_bytes;
            impl_->last_upload.pose_uniform_bytes=vertices.continuous?496:96;
            impl_->last_upload.pose_uniform_pushes=1;
        }
        if(impl_->upload_capacity<upload_bytes) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,upload_bytes,0};
            auto* replacement=SDL_CreateGPUTransferBuffer(next,&info);Impl::require(replacement);
            if(impl_->upload) SDL_ReleaseGPUTransferBuffer(next,impl_->upload);
            impl_->upload=replacement;impl_->upload_capacity=info.size;
        }
        const auto cost_prepared=cost_now();
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        if(upload_bytes) {
            auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(next,impl_->upload,true));Impl::require(mapped);
            std::uint32_t offset=0;
            for(unsigned i=0;i<sizes.size();++i) if(changed[i]) {std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}
            SDL_UnmapGPUTransferBuffer(next,impl_->upload);
            auto* copy=SDL_BeginGPUCopyPass(cmd);Impl::require(copy);offset=0;
            for(unsigned i=0;i<sizes.size();++i) if(changed[i]) {
                SDL_GPUTransferBufferLocation from{impl_->upload,offset};SDL_GPUBufferRegion to{impl_->buffers[i],0,sizes[i]};
                scene_counters::upload_buffer(copy,&from,&to,true);offset+=sizes[i];
            }
            SDL_EndGPUCopyPass(copy);
        }
        if(memoize) {
            // Layouts change throughout a frame. Retain bounded high-water
            // capacity instead of allocating/freeing a snapshot for each model.
            // Grow exactly (not geometrically beyond the memo budget).
            if(impl_->input_snapshot.capacity()<total)
                std::vector<Uint8>(std::size_t(total)).swap(impl_->input_snapshot);
            else impl_->input_snapshot.resize(std::size_t(total));
            snapshot_offset=0;
            for(unsigned i=0;i<sizes.size();++i) {
                if(changed[i]) std::memcpy(impl_->input_snapshot.data()+snapshot_offset,data[i],sizes[i]);
                impl_->snapshot_owned[i]=!shared[i] && sizes[i]!=0;
                snapshot_offset+=sizes[i];
            }
            impl_->snapshot_sizes=sizes;impl_->snapshot_valid=true;
        } else impl_->snapshot_valid=false;
        if(SDL_getenv("STARFOX_TRACE_MODEL_UPLOADS")) {
            const auto& stats=impl_->last_upload;
            std::cerr<<"model-upload: input="<<stats.input_bytes<<" uploaded="<<stats.uploaded_bytes
                <<" copies="<<stats.uploaded_buffers<<" reused="<<stats.reused_buffers<<'\n';
        }
        const auto cost_uploaded=cost_now();
        void* camera=nullptr;
        void* point_residuals=nullptr;
        // Matrix-interpolated ordinary models also cross half-pixel edges.
        // Dropping their residuals changes coverage for distant small models.
        const bool retain_projection=vertices.continuous;
        const GpuBspSettings bs{1,std::uint32_t(graph.nodes.size()),visibility_count,std::uint32_t(graph.face_ids.size()),slots,{}};
        GpuBspOutput ordered;
        void* projected=nullptr;
        if(small_model) {
            ordered=impl_->bsp.enqueue_continuous_model(device,command,b[0],b[1],continuous_pose_count,
                b[3],b[2],b[4],b[5],bs,vertex_count,settings.backface_culling);
            projected=ordered.points;point_residuals=ordered.residuals;
            if(!projected || !ordered.order) throw std::runtime_error(impl_->bsp.status());
        } else {
            projected=vertices.continuous?impl_->projection.enqueue_continuous(device,command,b[0],vertex_count,inline_pose?nullptr:b[1],continuous_pose_count,
                retain_projection?&point_residuals:nullptr,axis || settings.backface_culling,
                inline_pose?std::span(static_cast<const ContinuousTransformPose*>(data[1]),continuous_pose_count):std::span<const ContinuousTransformPose>{})
                :impl_->projection.enqueue_transformed(device,command,b[0],vertex_count,inline_pose?nullptr:b[1],native_pose_count,&camera,
                    inline_pose?std::span<const NativeTransformPose>(&vertices.native_pose,1):std::span<const NativeTransformPose>{});
            if(!projected) throw std::runtime_error(impl_->projection.status());
        }
        // Continuous visibility is evaluated on the original transformed mesh,
        // even if the displayed laser is subsequently collapsed to two points.
        auto* visibility_points=projected;
        // Shadow casters use the source mesh, not screen-space wave/wobble,
        // material warping or the collapsed laser-axis display substitute.
        // Preserve these records before axis reduction replaces raster inputs.
        auto* ray_points=vertices.continuous?projected:camera;
        auto* ray_residuals=point_residuals;
        if(axis) {
            auto* centres=impl_->projection.enqueue_axis_points(device,command,vertices.continuous?projected:camera,vertex_count,b[11],
                std::uint32_t(axis_indices.size()),axis_ranges,vertices.continuous,float(pose.vanish_x),float(pose.vanish_y),float(settings.focal_length),point_residuals,vertices.continuous?&point_residuals:nullptr,!vertices.continuous);
            if(!centres)throw std::runtime_error(impl_->projection.status());
            if(vertices.continuous)projected=centres;
            else {camera=centres;projected=impl_->projection.enqueue(device,command,camera,2);}
            if(!projected) throw std::runtime_error(impl_->projection.status());
        }
        void* visible=nullptr;
        const bool fused_visibility=visibility_count<=4096 && SDL_getenv("STARFOX_TEST_FUSED_VISIBILITY_BSP")
            && !SDL_getenv("STARFOX_TEST_SEPARATE_VISIBILITY_BSP");
        if(small_model) visible=ordered.visibility;
        else if(fused_visibility) {
            ordered=impl_->bsp.enqueue_projected(device,command,b[3],vertices.continuous?visibility_points:projected,
                b[2],b[4],b[5],bs,vertices.continuous?vertex_count:axis?2U:vertex_count,vertices.continuous);
            visible=ordered.visibility;
        } else {
            visible=vertices.continuous?impl_->projection.enqueue_continuous_visibility(command,b[2],visibility_count)
                :impl_->projection.enqueue_visibility(command,b[2],visibility_count);
            if(!visible) throw std::runtime_error(impl_->projection.status());
            ordered=impl_->bsp.enqueue(device,command,b[3],visible,b[4],b[5],bs);
        }
        if(!ordered.order) throw std::runtime_error(impl_->bsp.status());
        mark_model_gpu_time(command);
        if(small_model && !impl_->small_model_reported && SDL_getenv("STARFOX_TEST_SMALL_MODEL_RESULT")) {
            SDL_Log("model-small-stage: projection visibility painter in one resident pass");impl_->small_model_reported=true;
        }
        if(!small_model && fused_visibility && !impl_->fused_visibility_reported && SDL_getenv("STARFOX_TEST_VISIBILITY_BSP_RESULT")) {
            SDL_Log("model-visibility-bsp: fused resident pass");impl_->fused_visibility_reported=true;
        }
        NativeClipSettings cs{polygons,axis?2U:vertex_count,std::uint32_t(faces.corners.size()),visibility_count,int(width),int(height),{}};
        const GpuSpanOrder order{ordered.order,ordered.results,0,slots,0};
        void* materials=b[8];
        const bool planar_depth=geometry_depth && !axis && !colour_warp && !pose.wave_mode && pose.wobble_mode==0;
        if((surface_metadata || planar_depth) && !axis) {
            Impl::SurfaceSettings ss{polygons,vertex_count,std::uint32_t(faces.corners.size()),vertices.continuous?1U:0U};
            ss.want_geometry=planar_depth?1U:0U;
            ss.fractional_normal=!pose.use_rotation_matrix || pose.subpixel_projection;
            if(ss.fractional_normal) {
                const auto& normal_pose=vertices.continuous_poses[1];
                for(unsigned i=0;i<3;++i) {ss.row0[i]=starfox::bit_cast<std::int32_t>(normal_pose.row0[i]);ss.row1[i]=starfox::bit_cast<std::int32_t>(normal_pose.row1[i]);ss.row2[i]=starfox::bit_cast<std::int32_t>(normal_pose.row2[i]);}
            } else for(unsigned i=0;i<3;++i) {ss.row0[i]=pose.rotation_matrix[i];ss.row1[i]=pose.rotation_matrix[3+i];ss.row2[i]=pose.rotation_matrix[6+i];}
            materials=impl_->emit_surface(cmd,vertices.continuous?projected:camera,ss,b);
        }
        void* clip_corners=b[6];void* clip_polygons=b[7];
        GpuWarpOutput reflection_warp_output{};
        if(colour_warp) {
            warp_settings.capacity=slots;warp_settings.face_count=polygons;warp_settings.visibility_count=visibility_count;
            warp_settings.seed=std::uint16_t(pose.projected_points_address+source_vertex_count*6U)
                |(pose.explosion_progress?0x80000000U:0U);
            warp_settings.corner_count=cs.corner_count;
            warp_settings.texture_count=std::uint32_t(warp_textures.textures.size());
            warp_settings.coordinate_count=std::uint32_t(warp_textures.coordinates.size());
            const GpuWarpInputs inputs{ordered.order,ordered.results,b[7],b[6],visible,materials,axis?b[17]:b[11],b[12],b[13],b[14],b[15],b[16]};
            const auto expanded=impl_->warp.enqueue(device,command,inputs,warp_settings);
            if(!expanded.polygons)throw std::runtime_error(impl_->warp.status());
            clip_corners=expanded.corners;clip_polygons=expanded.polygons;materials=expanded.materials;
            cs.polygon_count=slots;cs.corner_count=slots*32U;
            if(ray_source && ray_source->request_materials && !ray_source->reference_materials && !axis) {
                auto reflection_settings=warp_settings;reflection_settings.reflection_materials=true;
                reflection_warp_output=impl_->reflection_warp.enqueue(device,command,inputs,reflection_settings);
                if(!reflection_warp_output.polygons) throw std::runtime_error(impl_->reflection_warp.status());
            }
        }
        // Only host-packed descriptors have a verified source-corner bound.
        // GPU-expanded colour-warp topology must retain generic clipping.
        std::uint32_t verified_corners=0;
        if(!colour_warp && SDL_getenv("STARFOX_TEST_SMALL_CLIP") && !SDL_getenv("STARFOX_TEST_FULL_CLIP")) for(const auto& polygon:faces.polygons)
            verified_corners=std::max(verified_corners,polygon[1]);
        auto* clipped=impl_->clip.enqueue(device,command,projected,clip_corners,clip_polygons,visible,cs,vertices.continuous,
            vertices.continuous?b[10]:camera,vertices.continuous?cs.polygon_count:vertex_count,point_residuals,point_residuals?cs.point_count:0,verified_corners,settings.render_scale,raster_size);
        if(!clipped) throw std::runtime_error(impl_->clip.status());
        // Diagnostic boundary only: separate exact clipping/material setup
        // from row-span construction without another pass or submission.
        mark_model_gpu_time(command);
        // Consumers retain per-sample color across draws and resolve after
        // painter composition. Colour-warp output is already expanded into
        // ordered occurrence slots: applying the source BSP order again would
        // substitute the wrong randomized material on repeated faces.
        void* masked_texels=nullptr;
        const bool repeated_rows=(pose.wobble_mode&1U)!=0;
        auto* spans=impl_->clip.enqueue_spans(command,materials,custom_raster || settings.render_scale>1,settings.render_scale,colour_warp?nullptr:&order,settings.wireframe_thickness,
            repeated_rows?b[9]:nullptr,repeated_rows?std::uint32_t(faces.texels.size()):0,repeated_rows?&masked_texels:nullptr,raster_size,
            diagnostics==nullptr,repeated_rows && msaa_faces!=nullptr?msaa_samples:0,compact_tiles);
        if(!spans) throw std::runtime_error(impl_->clip.status());
        if(SDL_getenv("STARFOX_TEST_MODEL_PATH_RESULT")) ++impl_->model_path_draws[0];
        if(SDL_getenv("STARFOX_TEST_MODEL_STYLE_RESULT") && pose.wobble_mode<=3) {
            ++impl_->wobble_span_draws[pose.wobble_mode];
            if(!(impl_->reported_wobble_modes&(1U<<pose.wobble_mode))) {
                std::cerr<<"model-style: actual-wobble="<<unsigned(pose.wobble_mode)
                    <<" repeated-rows="<<repeated_rows<<" span-slots="<<slots<<'\n';
                impl_->reported_wobble_modes|=1U<<pose.wobble_mode;
            }
        }
        if(msaa_faces) {
            const std::array<unsigned,4> masks=repeated_rows?std::array<unsigned,4>{std::uint32_t(faces.texels.size()),((raster_width+31)/32)*4,raster_height,msaa_samples}:std::array<unsigned,4>{};
            *msaa_faces=impl_->msaa.pack_faces(device,command,clipped,materials,nullptr,slots,cs.polygon_count,
                vertices.continuous,{float(scale_x),float(scale_y)},colour_warp?nullptr:&order,settings.wireframe_thickness,masks);
            if(!msaa_faces->triangles) throw std::runtime_error(impl_->msaa.status());
            msaa_faces->texels=repeated_rows?masked_texels:b[9];
            msaa_faces->texel_bytes=repeated_rows?impl_->clip.mask_buffer_bytes():std::uint32_t(faces.texels.size());
        }
        const GpuGeometryDepthInput depth_input{impl_->geometry_planes,polygons,
            float(settings.focal_length*scale_x),float(settings.focal_length*scale_y),
            float((vertices.continuous?pose.vanish_x:vertices.native_pose.vanish[0])*scale_x),
            float((vertices.continuous?pose.vanish_y:vertices.native_pose.vanish[1])*scale_y)};
        mark_model_gpu_time(command);
        auto output=impl_->raster.enqueue_row_spans(device,command,spans,slots,raster_width,raster_height,surface_metadata,repeated_rows?masked_texels:b[9],true,background,pose.wave_mode,pose.wave_offset,pose.animation_frame,
            repeated_rows?impl_->clip.mask_buffer_bytes():std::uint32_t(faces.texels.size()),planar_depth?&depth_input:nullptr,{},{},painter_flags,in_place_background,bounded_raster,compact_tiles);
        if(!output.pixels) throw std::runtime_error(impl_->raster.status());
        if(previous_pose && output.geometry_depth && !background && planar_depth
            && !pose.explosion_progress && !previous_pose->explosion_progress
            && !previous_pose->wave_mode && !previous_pose->wobble_mode
            && !previous_pose->simple_scaled_sprite && !previous_pose->collapse_to_axis_line
            && !(previous_pose->colour_warp && !previous_pose->force_colour)
            && (shape.frames.empty() || previous_pose->animation_frame%shape.frames.size()
                ==pose.animation_frame%shape.frames.size())) {
            const auto old=pack_projection(shape,*previous_pose,settings,
                source_projection && source_projection->matches(shape,*previous_pose,settings)?source_projection:nullptr);
            if(auto motion=pack_motion_surface(vertices,old,output.width,output.height,settings.render_scale)) {
                for(unsigned i=0;i<4;++i) {
                    const float ratio=float((i%2?scale_y:scale_x)/settings.render_scale);
                    motion->current_projection[i]*=ratio;motion->previous_projection[i]*=ratio;
                }
                motion->current_projection[2]-=raster_jitter[0];
                motion->current_projection[3]-=raster_jitter[1];
                motion->jitter_x=raster_jitter[0];
                motion->jitter_y=raster_jitter[1];
                emit_motion(output,*motion);
            }
        }
        if(diagnostics) *diagnostics={projected,clipped,cs.point_count,cs.polygon_count,vertices.continuous};
        if(ray_source && axis && pose.explosion_progress)
            impl_->exploding_axis_casters(cmd,shape,pose,settings,*ray_source);
        else if(ray_source) {
            ray_source->points=ray_points;
            ray_source->residuals=ray_residuals;
            ray_source->point_count=vertex_count;
            ray_source->mode=vertices.continuous?(ray_residuals?2U:1U):0U;
            if(axis) {
                for(std::size_t face=0;face<shape.faces.size();++face) {
                    const auto& f=shape.faces[face];
                    if(f.sprite || f.vertex_indices.size()<3) continue;
                    for(std::size_t c=1;c+1<f.vertex_indices.size();++c)
                        ray_source->triangles.push_back({f.vertex_indices[0],f.vertex_indices[c],f.vertex_indices[c+1],std::uint32_t(face)});
                }
            } else {
              std::vector<std::array<std::uint32_t,4>> material_topology;
              if(source_rays) {
                ray_source->borrowed_triangles=source_rays->triangles();
                if(ray_source->request_materials) ray_source->borrowed_material_topology=source_rays->material_topology();
                // Unencoded, foreign-device or mismatched packets are a normal
                // full-quality fallback. Never expose stale source buffers.
                if(gpu_source && gpu_source->device_==device && gpu_source->encoded_ && *gpu_source->encoded_
                    && gpu_source->source_.rays==source_rays && gpu_source->source_.faces==source_faces) {
                    if(gpu_source->buffers_[9] && gpu_source->sizes_[9]==source_rays->triangles().size_bytes())
                        ray_source->triangle_topology=gpu_source->buffers_[9];
                    if(ray_source->request_materials && gpu_source->buffers_[10]
                        && gpu_source->sizes_[10]==source_rays->material_topology().size_bytes())
                        ray_source->material_connectivity=gpu_source->buffers_[10];
                }
              } else for(std::size_t face=0;face<faces.polygons.size();++face) {
                if(faces.primitives[face]!=PackedPrimitive::polygon) continue;
                const auto first=faces.polygons[face][0],count=faces.polygons[face][1];
                for(std::uint32_t corner=1;corner+1<count;++corner) {
                    ray_source->triangles.push_back({faces.corners[first][0],faces.corners[first+corner][0],
                        faces.corners[first+corner+1][0],std::uint32_t(face)});
                    if(ray_source->request_materials)
                        material_topology.push_back({first,first+corner,first+corner+1,std::uint32_t(face)});
                }
              }
              // Warp materials are resolved per occurrence on GPU. Keep all
              // original geometry for shadows; ray material lookup selects the
              // last emitted occurrence without CPU readback or placeholder ink.
              if(ray_source->request_materials && colour_warp && !ray_source->reference_materials) {
                  std::size_t triangle=0;
                  for(std::size_t face=0;face<faces.polygons.size();++face) {
                      if(faces.primitives[face]!=PackedPrimitive::polygon) continue;
                      for(std::uint32_t corner=1;corner+1<faces.polygons[face][1];++corner)
                          material_topology[triangle++]={0,corner,corner+1,std::uint32_t(face)|0x80000000U};
                  }
                  ray_source->materials.texels=faces.texels;
                  ray_source->materials_complete=true;
                  ray_source->material_corners=reflection_warp_output.corners;ray_source->material_polygons=reflection_warp_output.polygons;
                  ray_source->material_commands=reflection_warp_output.materials;
                  ray_source->material_corner_count=(slots+polygons)*32U;ray_source->material_count=slots+polygons;
                  ray_source->material_lookup=reflection_warp_output.face_lookup;ray_source->material_lookup_count=polygons;
                  ray_source->material_topology=std::move(material_topology);
              } else if(ray_source->request_materials && !colour_warp) {
                  if(ray_source->reference_materials)
                      ray_source->materials_complete=pack_ray_materials(faces,source_rays?source_rays->material_topology():std::span<const std::array<std::uint32_t,4>>(material_topology),ray_source->materials);
                  else {
                      ray_source->materials.texels=faces.texels;
                      ray_source->materials_complete=true; // GPU pack validates individual records.
                  }
                  ray_source->material_corners=clip_corners;ray_source->material_polygons=clip_polygons;
                  ray_source->material_commands=materials;
                  ray_source->material_corner_count=std::uint32_t(faces.corners.size());
                  ray_source->material_count=std::uint32_t(faces.polygons.size());
                  ray_source->material_topology=std::move(material_topology);
              }
            }
        }
        if(ray_source && ray_source->request_materials && axis) {
            // Axis effects rasterize lines, not filled polygons. Keep their
            // original caster geometry but explicitly exclude polygon mirrors.
            ray_source->reflection_excluded=true;ray_source->materials_complete=true;
            if(ray_source->reference_materials) {
                ray_source->materials.triangles.resize(ray_source->triangles.size());
                for(auto& material:ray_source->materials.triangles) material.reserved=1;
            }
        }
        if(trace_model_cost) {
            const auto done=cost_now();
            const auto us=[](auto a,auto b){return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(b.wall-a.wall).count());};
            ModelHostCost value{shape.header.address,vertex_count,polygons,us(cost_begin,cost_packed),us(cost_packed,cost_prepared),
                us(cost_prepared,cost_uploaded),us(cost_uploaded,done),impl_->last_upload.uploaded_bytes};
            value.cpu={model_thread_cpu_delta(cost_begin.cpu,cost_packed.cpu),model_thread_cpu_delta(cost_packed.cpu,cost_prepared.cpu),
                model_thread_cpu_delta(cost_prepared.cpu,cost_uploaded.cpu),model_thread_cpu_delta(cost_uploaded.cpu,done.cpu)};
            value.batch=model_gpu_timing_hook.batch;value.draw_index=model_gpu_timing_hook.draw_index;
            cost_trace->add(value);
        }
        impl_->status="Packed model geometry GPU resident";return output;
    }catch(const std::exception& error){if(ray_source) *ray_source={};impl_->snapshot_valid=false;impl_->status=error.what();}
#endif
    return {};
}
}
