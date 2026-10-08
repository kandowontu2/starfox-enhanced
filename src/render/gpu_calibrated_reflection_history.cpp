#include "starfox/render/gpu_calibrated_reflection_history.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include "shaders/generated/calibrated_reflection_lobes.hpp"
#include "shaders/generated/calibrated_reflection_paths.hpp"
#include "shaders/generated/calibrated_reflection_curved_paths.hpp"
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
#include "native_reflection_index_tiles_dxil.hpp"
#include "native_reflection_index_tiles_spirv.hpp"
#include "native_reflection_index_reduce_dxil.hpp"
#include "native_reflection_index_reduce_spirv.hpp"
#include "native_reflection_index_query_dxil.hpp"
#include "native_reflection_index_query_spirv.hpp"
#include "native_reflection_index_mapping_dxil.hpp"
#include "native_reflection_index_mapping_spirv.hpp"
#include "native_reflection_index_domains_dxil.hpp"
#include "native_reflection_index_domains_spirv.hpp"
#include "native_reflection_index_frames_dxil.hpp"
#include "native_reflection_index_frames_spirv.hpp"
#include "native_reflection_index_optical_dxil.hpp"
#include "native_reflection_index_optical_spirv.hpp"
#include "native_reflection_index_optical_stream_dxil.hpp"
#include "native_reflection_index_optical_stream_spirv.hpp"
#include "native_reflection_index_optical_schedule_dxil.hpp"
#include "native_reflection_index_optical_schedule_spirv.hpp"
#include "native_reflection_index_jets_dxil.hpp"
#include "native_reflection_index_jets_spirv.hpp"
#include "native_reflection_index_jets_stream_dxil.hpp"
#include "native_reflection_index_jets_stream_spirv.hpp"
#include "native_reflection_index_roots_clear_dxil.hpp"
#include "native_reflection_index_roots_clear_spirv.hpp"
#include "native_reflection_index_witness_dxil.hpp"
#include "native_reflection_index_witness_spirv.hpp"
#include "native_reflection_index_folds_dxil.hpp"
#include "native_reflection_index_folds_spirv.hpp"
#include "native_reflection_index_guide_dxil.hpp"
#include "native_reflection_index_guide_spirv.hpp"
#include "native_reflection_index_colour_dxil.hpp"
#include "native_reflection_index_colour_spirv.hpp"
#include "native_reflection_index_compose_dxil.hpp"
#include "native_reflection_index_compose_spirv.hpp"
#include "native_reflection_index_publish_dxil.hpp"
#include "native_reflection_index_publish_spirv.hpp"
#include "native_reflection_index_publish_diagnostics_dxil.hpp"
#include "native_reflection_index_publish_diagnostics_spirv.hpp"
#include "native_reflection_index_admission_clear_dxil.hpp"
#include "native_reflection_index_admission_clear_spirv.hpp"
#include "native_reflection_capture_dxil.hpp"
#include "native_reflection_capture_spirv.hpp"
#endif
#include <SDL3/SDL.h>
#if __has_include(<vulkan/vulkan.h>)
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#define STARFOX_CURVED_VULKAN_QUERY 1
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Uniform {unsigned width,height,previous_width,previous_height,flags;float weight;unsigned prefix_stride,padding;};
static_assert(sizeof(Uniform)==32);
struct LobeUniform {
    unsigned width,height,previous_width,previous_height,lobes,flags,previous_vertices,previous_mapping;
    unsigned triangles,geometry_bytes;float weight,roughness;
    std::array<float,4> projection,clip;
};
static_assert(sizeof(LobeUniform)==80);
struct PathUniform {
    LobeUniform lobe;
    unsigned old_triangles,scene_paths{},pad1{},pad2{};
    std::array<std::array<float,4>,3> current_cube{},previous_cube{};
    std::array<float,4> current_point{},current_normal{},previous_point{},previous_normal{};
};
static_assert(sizeof(PathUniform)==256);
struct CurvedPathUniform {
    PathUniform path;
    std::array<float,32> previous_liquid;
    std::array<std::array<float,4>,3> current_liquid_rotation;
    std::array<float,4> current_projection;
};
static_assert(sizeof(CurvedPathUniform)==448);
struct SourceIndexUniform {
    unsigned width,height,lobes,primary_prefix,record_prefix,path_stride,total_nodes,level_count;
    unsigned work_level{},query_count{},leaf_budget{},node_budget{};
    std::array<std::array<unsigned,4>,12> levels;
};
static_assert(sizeof(SourceIndexUniform)==240);
struct SourceMappingUniform {
    unsigned width,height,primary_prefix,record_prefix,path_stride,triangles,old_triangles,previous_mapping;
    unsigned geometry_bytes,lobes,first,count,flags,pad0{},pad1{},pad2{};
    std::array<std::array<float,4>,3> current_cube{},previous_cube{};
};
static_assert(sizeof(SourceMappingUniform)==160);
struct SourceFrameUniform {
    unsigned old_triangles,previous_vertices,previous_mapping,geometry_bytes;
    unsigned width,height,record_prefix,path_stride,lobes,first,count,flags;
    float roughness;unsigned pad0{},pad1{},pad2{};
    std::array<float,4> projection{},extent_clip{},point{},normal{};
    std::array<float,32> liquid{};
};
static_assert(sizeof(SourceFrameUniform)==256);
struct SourceTargetUniform {std::array<std::array<float,4>,3> current{},previous{};};
static_assert(sizeof(SourceTargetUniform)==96);
struct SourceColourUniform {
    unsigned width,height,primary_prefix,record_prefix,triangles,first,result_stride,record_offset;
    unsigned record_bytes,flags,count,reserved{};float weight;
    unsigned lobes,path_stride,pad{};
};
static_assert(sizeof(SourceColourUniform)==64);
bool curved_vulkan_precision(SDL_GPUDevice* device) {
#if defined(STARFOX_CURVED_VULKAN_QUERY)
    const auto* bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(
        SDL_GetGPUDeviceProperties(device),STARFOX_SDL_VULKAN_BRIDGE,nullptr));
    if(!bridge || bridge->version!=2 || !bridge->get_instance_proc) return false;
    const auto query=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceProperties2"));
    if(!query) return false;
    VkPhysicalDeviceFloatControlsProperties controls{};controls.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
    VkPhysicalDeviceProperties2 properties{};properties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;properties.pNext=&controls;
    query(bridge->physical_device,&properties);
    return properties.properties.apiVersion>=VK_API_VERSION_1_2 && controls.shaderSignedZeroInfNanPreserveFloat32;
#else
    (void)device;return false;
#endif
}
bool wide_optical_supported(SDL_GPUDevice* device) {
    // D3D12 compute supports this group size. Vulkan only guarantees 128
    // invocations: retain the exact existing 64-lane generic writer if the
    // physical device cannot run the wider compact stream. No cell is dropped.
    if(SDL_strcmp(SDL_GetGPUDeviceDriver(device),"direct3d12")==0)return true;
#if defined(STARFOX_CURVED_VULKAN_QUERY)
    const auto* bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(
        SDL_GetGPUDeviceProperties(device),STARFOX_SDL_VULKAN_BRIDGE,nullptr));
    if(!bridge || bridge->version!=2 || !bridge->get_instance_proc)return false;
    const auto query=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceProperties2"));
    if(!query)return false;
    VkPhysicalDeviceProperties2 properties{};properties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    query(bridge->physical_device,&properties);
    return GpuReflectionSourceOptical::stream_lanes_supported(
        properties.properties.limits.maxComputeWorkGroupInvocations,
        properties.properties.limits.maxComputeWorkGroupSize[0]);
#else
    return false;
#endif
}
}
struct GpuCalibratedReflectionHistory::State {
    SDL_GPUDevice* device{};SDL_GPUComputePipeline *pipeline{},*lobe_pipeline{},*path_pipeline{},*curved_pipeline{};SDL_GPUSampler* sampler{};
    SDL_GPUComputePipeline *source_tiles{},*source_reduce{},*capture_pipeline{};
    std::array<SDL_GPUComputePipeline*,3> query_pipelines{};
    std::array<SDL_GPUComputePipeline*,2> optical_pipelines{};
    SDL_GPUComputePipeline *optical_stream_pipeline{},*optical_schedule_pipeline{};
    SDL_GPUComputePipeline *local_root_pipeline{},*witness_pipeline{},*folds_pipeline{},*guide_pipeline{},*colour_pipeline{},*compose_pipeline{},*publish_pipeline{};
    SDL_GPUComputePipeline *stream_root_pipeline{},*root_clear_pipeline{};
    SDL_GPUComputePipeline *publish_diagnostic_pipeline{},*admission_clear_pipeline{};
    bool source_stream_encoding{};
    std::optional<bool> wide_optical_support;
    bool can_stream_optical() {
        if(!wide_optical_support)wide_optical_support=wide_optical_supported(device);
        return *wide_optical_support;
    }
    struct QueryWork {
        SDL_GPUDevice* device{};std::array<SDL_GPUBuffer*,3> buffers{};
        ~QueryWork(){for(auto* b:buffers)if(b)SDL_ReleaseGPUBuffer(device,b);}
    };
    std::unique_ptr<QueryWork> query_work;
    std::unique_ptr<QueryWork> optical_work;
    std::unique_ptr<QueryWork> optical_stream_work;
    std::unique_ptr<QueryWork> local_root_work;
    GpuReflectionSourceQueries queries{};
    GpuReflectionSourceOptical optical{};
    GpuReflectionSourceLocalRoots local_roots{};
    GpuReflectionSourceWitness witness{};
    GpuReflectionSourceFolds folds{};
    GpuReflectionSourceGuide source_guide{};
    GpuReflectionSourceColour source_colour{};
    GpuReflectionSourceComposition source_composition{};
    bool root_hull_encoded{};
    void* pending_command{};bool pending_sources_compatible{};
    CalibratedReflectionLobeGeometry pending_guide{};
    std::uint64_t publication_next{};bool publication_started{},publication_complete{};
    struct Images {
        SDL_GPUDevice* device;unsigned width,height,storage_bytes,prefix_stride,model_lobes;bool separated;std::array<SDL_GPUBuffer*,2> buffers{};
        std::array<SDL_GPUBuffer*,2> source_indices{};
        SDL_GPUBuffer* publication_buffer{};
        std::array<SDL_GPUBuffer*,2> admission_masks{};
        SDL_GPUBuffer* publication_mask{};
        bool admission_diagnostics{};
        std::optional<ReflectionSourceIndexLayout> source_layout;
        ~Images() {for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
            for(auto* b:source_indices) if(b) SDL_ReleaseGPUBuffer(device,b);
            if(publication_buffer)SDL_ReleaseGPUBuffer(device,publication_buffer);
            for(auto* b:admission_masks)if(b)SDL_ReleaseGPUBuffer(device,b);
            if(publication_mask)SDL_ReleaseGPUBuffer(device,publication_mask);}
        std::uint64_t bytes() const noexcept {return (std::uint64_t(storage_bytes)
            +(source_layout?source_layout->storage_bytes:0))*2+(publication_buffer?storage_bytes:0)
            +(admission_diagnostics?std::uint64_t(width)*height*8:0)+(publication_mask?std::uint64_t(width)*height*4:0);}
    };
    std::unique_ptr<Images> accepted_images,pending_images;
    CalibratedReflectionHistorySettings accepted{},pending_settings{};
    shadows::GpuReflectionOutput candidate{},accepted_output{};
    unsigned index{},pending_index{};bool valid{},pending{},pending_reset{};
    unsigned accepted_triangles{},pending_triangles{};
    float accepted_roughness{},pending_roughness{};
    std::array<float,9> accepted_cube{},pending_cube{};
    std::optional<shadows::RayReflectionGround> accepted_ground,pending_ground;
    std::optional<shadows::RayReflectionLiquid> accepted_liquid,pending_liquid;
    std::array<double,4> accepted_liquid_projection{},pending_liquid_projection{};
    std::array<double,2> accepted_liquid_clip{},pending_liquid_clip{};
    std::string status{"Native reflection history not initialized"};
    ~State() {
        pending_images.reset();accepted_images.reset();
        query_work.reset();
        optical_work.reset();
        optical_stream_work.reset();
        local_root_work.reset();
        for(auto* p:query_pipelines)if(p)SDL_ReleaseGPUComputePipeline(device,p);
        for(auto* p:optical_pipelines)if(p)SDL_ReleaseGPUComputePipeline(device,p);
        if(optical_stream_pipeline)SDL_ReleaseGPUComputePipeline(device,optical_stream_pipeline);
        if(optical_schedule_pipeline)SDL_ReleaseGPUComputePipeline(device,optical_schedule_pipeline);
        if(local_root_pipeline)SDL_ReleaseGPUComputePipeline(device,local_root_pipeline);
        if(stream_root_pipeline)SDL_ReleaseGPUComputePipeline(device,stream_root_pipeline);
        if(root_clear_pipeline)SDL_ReleaseGPUComputePipeline(device,root_clear_pipeline);
        if(witness_pipeline)SDL_ReleaseGPUComputePipeline(device,witness_pipeline);
        if(folds_pipeline)SDL_ReleaseGPUComputePipeline(device,folds_pipeline);
        if(guide_pipeline)SDL_ReleaseGPUComputePipeline(device,guide_pipeline);
        if(colour_pipeline)SDL_ReleaseGPUComputePipeline(device,colour_pipeline);
        if(compose_pipeline)SDL_ReleaseGPUComputePipeline(device,compose_pipeline);
        if(publish_pipeline)SDL_ReleaseGPUComputePipeline(device,publish_pipeline);
        if(publish_diagnostic_pipeline)SDL_ReleaseGPUComputePipeline(device,publish_diagnostic_pipeline);
        if(admission_clear_pipeline)SDL_ReleaseGPUComputePipeline(device,admission_clear_pipeline);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(lobe_pipeline) SDL_ReleaseGPUComputePipeline(device,lobe_pipeline);
        if(path_pipeline) SDL_ReleaseGPUComputePipeline(device,path_pipeline);
        if(curved_pipeline) SDL_ReleaseGPUComputePipeline(device,curved_pipeline);
        if(source_tiles) SDL_ReleaseGPUComputePipeline(device,source_tiles);
        if(source_reduce) SDL_ReleaseGPUComputePipeline(device,source_reduce);
        if(capture_pipeline) SDL_ReleaseGPUComputePipeline(device,capture_pipeline);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
    std::unique_ptr<Images> make_images(unsigned w,unsigned h,unsigned bytes,bool separated,unsigned prefix_stride,unsigned model_lobes,
        const std::optional<ReflectionSourceIndexLayout>& source_layout,bool admission_diagnostics) {
        auto next=std::make_unique<Images>();next->device=device;next->width=w;next->height=h;
        next->storage_bytes=bytes;next->separated=separated;next->prefix_stride=prefix_stride;next->model_lobes=model_lobes;
        const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
            |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,bytes,0};
        for(auto& b:next->buffers) {b=SDL_CreateGPUBuffer(device,&info);require(b,SDL_GetError());}
        next->source_layout=source_layout;
        if(source_layout) {
            SDL_GPUBufferCreateInfo index_info=info;index_info.size=source_layout->storage_bytes;
            for(auto& b:next->source_indices) {b=SDL_CreateGPUBuffer(device,&index_info);require(b,SDL_GetError());}
        }
        next->admission_diagnostics=admission_diagnostics;
        if(admission_diagnostics) {
            SDL_GPUBufferCreateInfo mask_info=info;mask_info.size=w*h*4;
            for(auto& b:next->admission_masks) {b=SDL_CreateGPUBuffer(device,&mask_info);require(b,SDL_GetError());}
        }
        return next;
    }
    void prepare_source_index() {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(!spirv || curved_vulkan_precision(device),"Resident reflection index requires Vulkan1.2 SignedZeroInfNanPreserve32");
        for(unsigned stage=0;stage<2;++stage) {
            auto*& selected=stage?source_reduce:source_tiles;if(selected) continue;
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint=stage?"feature_reduce_main":"feature_tiles_main";
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;info.num_readonly_storage_buffers=stage?0:1;
            info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
            if(stage) {info.code=spirv?native_reflection_index_reduce_spirv:native_reflection_index_reduce_dxil;
                info.code_size=spirv?sizeof(native_reflection_index_reduce_spirv):sizeof(native_reflection_index_reduce_dxil);}
            else {info.code=spirv?native_reflection_index_tiles_spirv:native_reflection_index_tiles_dxil;
                info.code_size=spirv?sizeof(native_reflection_index_tiles_spirv):sizeof(native_reflection_index_tiles_dxil);}
            selected=create_gpu_compute_pipeline(device,&info);require(selected,SDL_GetError());
        }
#else
        require(false,"Resident reflection source index not compiled; retaining current full-quality radiance");
#endif
    }
    void prepare_capture() {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        if(capture_pipeline)return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(!spirv || curved_vulkan_precision(device),"Current ordered-path capture requires Vulkan1.2 SignedZeroInfNanPreserve32");
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="reflection_path_capture_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.num_samplers=info.num_readonly_storage_buffers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
        info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        info.code=spirv?native_reflection_capture_spirv:native_reflection_capture_dxil;
        info.code_size=spirv?sizeof(native_reflection_capture_spirv):sizeof(native_reflection_capture_dxil);
        capture_pipeline=create_gpu_compute_pipeline(device,&info);require(capture_pipeline,SDL_GetError());
#endif
    }
    void prepare_source_queries() {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(!spirv || curved_vulkan_precision(device),"Resident source queries require Vulkan1.2 SignedZeroInfNanPreserve32");
        const unsigned char* code[]{spirv?native_reflection_index_mapping_spirv:native_reflection_index_mapping_dxil,
            spirv?native_reflection_index_query_spirv:native_reflection_index_query_dxil,
            spirv?native_reflection_index_domains_spirv:native_reflection_index_domains_dxil};
        const std::size_t sizes[]{spirv?sizeof(native_reflection_index_mapping_spirv):sizeof(native_reflection_index_mapping_dxil),
            spirv?sizeof(native_reflection_index_query_spirv):sizeof(native_reflection_index_query_dxil),
            spirv?sizeof(native_reflection_index_domains_spirv):sizeof(native_reflection_index_domains_dxil)};
        const char* names[]{"feature_mapping_main","feature_query_main","feature_domains_main"};
        for(unsigned stage=0;stage<3;++stage)if(!query_pipelines[stage]) {
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint=names[stage];info.code=code[stage];info.code_size=sizes[stage];
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.num_readwrite_storage_buffers=1;info.num_readonly_storage_buffers=stage==2?3:2;
            info.num_uniform_buffers=stage==2?2:1;info.threadcount_x=stage==2?8:64;info.threadcount_y=stage==2?8:1;info.threadcount_z=1;
            query_pipelines[stage]=create_gpu_compute_pipeline(device,&info);require(query_pipelines[stage],SDL_GetError());
        }
#else
        require(false,"Resident reflection query stages not compiled");
#endif
    }
    void make_query_work() {
        if(query_work)return;
        auto next=std::make_unique<QueryWork>();next->device=device;
        const unsigned sizes[]{GpuReflectionSourceQueries::capacity*GpuReflectionSourceQueries::query_stride,
            GpuReflectionSourceQueries::capacity*GpuReflectionSourceQueries::leaf_stride,
            GpuReflectionSourceQueries::capacity*GpuReflectionSourceQueries::region_stride};
        for(unsigned n=0;n<3;++n) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,sizes[n],0};
            next->buffers[n]=SDL_CreateGPUBuffer(device,&info);require(next->buffers[n],SDL_GetError());
        }
        query_work=std::move(next);
    }
    void prepare_source_optical(bool stream=false) {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(!spirv || curved_vulkan_precision(device),"Resident source optics require Vulkan1.2 SignedZeroInfNanPreserve32");
        const unsigned char* code[]{spirv?native_reflection_index_frames_spirv:native_reflection_index_frames_dxil,
            spirv?native_reflection_index_optical_spirv:native_reflection_index_optical_dxil};
        const std::size_t sizes[]{spirv?sizeof(native_reflection_index_frames_spirv):sizeof(native_reflection_index_frames_dxil),
            spirv?sizeof(native_reflection_index_optical_spirv):sizeof(native_reflection_index_optical_dxil)};
        const char* names[]{"feature_frames_main","feature_optical_main"};
        for(unsigned stage=0;stage<2;++stage)if(!optical_pipelines[stage] && !(stage && stream)) {
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint=names[stage];info.code=code[stage];info.code_size=sizes[stage];
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.num_readwrite_storage_buffers=1;info.num_readonly_storage_buffers=3;info.num_uniform_buffers=stage?3:1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            optical_pipelines[stage]=create_gpu_compute_pipeline(device,&info);require(optical_pipelines[stage],SDL_GetError());
        }
        if(stream && !optical_stream_pipeline) {
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_optical_stream_main";
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.code=spirv?native_reflection_index_optical_stream_spirv:native_reflection_index_optical_stream_dxil;
            info.code_size=spirv?sizeof(native_reflection_index_optical_stream_spirv):sizeof(native_reflection_index_optical_stream_dxil);
            info.num_readonly_storage_buffers=4;info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=3;
            info.threadcount_x=GpuReflectionSourceOptical::stream_lanes;info.threadcount_y=info.threadcount_z=1;
            optical_stream_pipeline=create_gpu_compute_pipeline(device,&info);require(optical_stream_pipeline,SDL_GetError());
        }
        if(stream && !optical_schedule_pipeline) {
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_optical_schedule_main";
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.code=spirv?native_reflection_index_optical_schedule_spirv:native_reflection_index_optical_schedule_dxil;
            info.code_size=spirv?sizeof(native_reflection_index_optical_schedule_spirv):sizeof(native_reflection_index_optical_schedule_dxil);
            info.num_readonly_storage_buffers=1;info.num_readwrite_storage_buffers=3;info.num_uniform_buffers=1;
            info.threadcount_x=128;info.threadcount_y=info.threadcount_z=1;
            optical_schedule_pipeline=create_gpu_compute_pipeline(device,&info);require(optical_schedule_pipeline,SDL_GetError());
        }
#else
        (void)stream;
        require(false,"Resident reflection optical stages not compiled");
#endif
    }
    void make_optical_work() {
        if(optical_work)return;
        auto next=std::make_unique<QueryWork>();next->device=device;
        const unsigned sizes[]{GpuReflectionSourceQueries::capacity*GpuReflectionSourceOptical::frame_stride,
            GpuReflectionSourceQueries::capacity*GpuReflectionSourceOptical::result_stride};
        for(unsigned n=0;n<2;++n) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,sizes[n],0};
            next->buffers[n]=SDL_CreateGPUBuffer(device,&info);require(next->buffers[n],SDL_GetError());
        }
        optical_work=std::move(next);
    }
    void make_optical_stream_work() {
        if(optical_stream_work)return;
        static_assert(sizeof(SDL_GPUIndirectDispatchCommand)==12);
        auto next=std::make_unique<QueryWork>();next->device=device;
        const unsigned sizes[]{GpuReflectionSourceOptical::stream_task_capacity*8,12};
        for(unsigned n=0;n<2;++n) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
                |(n?SDL_GPU_BUFFERUSAGE_INDIRECT:0U),sizes[n],0};
            next->buffers[n]=SDL_CreateGPUBuffer(device,&info);require(next->buffers[n],SDL_GetError());
        }
        optical_stream_work=std::move(next);
    }
    void schedule_optical(SDL_GPUCommandBuffer* command,const GpuReflectionSourceQueries& q) {
        make_optical_stream_work();
        const SDL_GPUStorageBufferReadWriteBinding outputs[]{
            {optical_work->buffers[1],false,0,0,0},{optical_stream_work->buffers[0],false,0,0,0},
            {optical_stream_work->buffers[1],false,0,0,0}};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,outputs,3);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,optical_schedule_pipeline);
        auto* regions=static_cast<SDL_GPUBuffer*>(q.regions);SDL_BindGPUComputeStorageBuffers(pass,0,&regions,1);
        const std::array<unsigned,4> settings{q.count,0,0,0};
        SDL_PushGPUComputeUniformData(command,0,settings.data(),sizeof(settings));
        SDL_DispatchGPUCompute(pass,q.count,1,1);SDL_EndGPUComputePass(pass);
        // Pass boundary is required before reading the resident work map and
        // dispatch arguments. Never read them back or choose work on the CPU.
    }
    void prepare_source_local_roots(bool stream=false) {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        auto*& selected=stream?stream_root_pipeline:local_root_pipeline;if(selected)return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(!spirv || curved_vulkan_precision(device),"Resident source roots require Vulkan1.2 SignedZeroInfNanPreserve32");
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint=stream?"feature_jets_stream_main":"feature_jets_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.code=spirv?native_reflection_index_jets_spirv:native_reflection_index_jets_dxil;
        info.code_size=spirv?sizeof(native_reflection_index_jets_spirv):sizeof(native_reflection_index_jets_dxil);
        if(stream) {
            info.code=spirv?native_reflection_index_jets_stream_spirv:native_reflection_index_jets_stream_dxil;
            info.code_size=spirv?sizeof(native_reflection_index_jets_stream_spirv):sizeof(native_reflection_index_jets_stream_dxil);
        }
        info.num_readwrite_storage_buffers=1;info.num_readonly_storage_buffers=4;info.num_uniform_buffers=3;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        selected=create_gpu_compute_pipeline(device,&info);require(selected,SDL_GetError());
#else
        (void)stream;
        require(false,"Resident reflection local-root stage not compiled");
#endif
    }
    void clear_stream_roots(SDL_GPUCommandBuffer* command) {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        static_assert(GpuReflectionSourceLocalRoots::working_bytes==1572864);
        if(!root_clear_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_roots_clear_main";
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.code=spirv?native_reflection_index_roots_clear_spirv:native_reflection_index_roots_clear_dxil;
            info.code_size=spirv?sizeof(native_reflection_index_roots_clear_spirv):sizeof(native_reflection_index_roots_clear_dxil);
            info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            root_clear_pipeline=create_gpu_compute_pipeline(device,&info);require(root_clear_pipeline,SDL_GetError());
        }
        make_local_root_work();
        const SDL_GPUStorageBufferReadWriteBinding output{local_root_work->buffers[0],false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,root_clear_pipeline);
        const std::array<unsigned,4> settings{unsigned(GpuReflectionSourceLocalRoots::working_bytes),0,0,0};
        SDL_PushGPUComputeUniformData(command,0,settings.data(),sizeof(settings));
        SDL_DispatchGPUCompute(pass,(settings[0]/16+63)/64,1,1);SDL_EndGPUComputePass(pass);
#else
        (void)command;require(false,"Resident reflection root clearing not compiled");
#endif
    }
    void make_local_root_work() {
        if(local_root_work)return;
        auto next=std::make_unique<QueryWork>();next->device=device;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
            GpuReflectionSourceQueries::capacity*GpuReflectionSourceLocalRoots::result_stride,0};
        next->buffers[0]=SDL_CreateGPUBuffer(device,&info);require(next->buffers[0],SDL_GetError());
        local_root_work=std::move(next);
    }
    void prepare_source_witness(unsigned stage) {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        auto*& selected=stage==2?guide_pipeline:stage==1?folds_pipeline:witness_pipeline;if(selected)return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(!spirv || curved_vulkan_precision(device),"Resident source witnesses require Vulkan1.2 SignedZeroInfNanPreserve32");
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint=stage==2?"feature_guide_main":stage==1?"feature_folds_main":"feature_witness_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.code=spirv?native_reflection_index_witness_spirv:native_reflection_index_witness_dxil;
        info.code_size=spirv?sizeof(native_reflection_index_witness_spirv):sizeof(native_reflection_index_witness_dxil);
        if(stage==1) {
            info.code=spirv?native_reflection_index_folds_spirv:native_reflection_index_folds_dxil;
            info.code_size=spirv?sizeof(native_reflection_index_folds_spirv):sizeof(native_reflection_index_folds_dxil);
        }
        if(stage==2) {
            info.code=spirv?native_reflection_index_guide_spirv:native_reflection_index_guide_dxil;
            info.code_size=spirv?sizeof(native_reflection_index_guide_spirv):sizeof(native_reflection_index_guide_dxil);
        }
        info.num_readwrite_storage_buffers=1;info.num_readonly_storage_buffers=stage==2?2U:3U;info.num_uniform_buffers=stage==1?2U:3U;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        selected=create_gpu_compute_pipeline(device,&info);require(selected,SDL_GetError());
#else
        (void)stage;
        require(false,"Resident source witness stage not compiled");
#endif
    }
    void prepare_source_colour() {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        if(colour_pipeline)return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(!spirv || curved_vulkan_precision(device),"Resident source colour requires Vulkan1.2 SignedZeroInfNanPreserve32");
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_colour_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.code=spirv?native_reflection_index_colour_spirv:native_reflection_index_colour_dxil;
        info.code_size=spirv?sizeof(native_reflection_index_colour_spirv):sizeof(native_reflection_index_colour_dxil);
        info.num_readonly_storage_buffers=3;info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=2;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        colour_pipeline=create_gpu_compute_pipeline(device,&info);require(colour_pipeline,SDL_GetError());
#else
        require(false,"Resident reflection source colour stage not compiled");
#endif
    }
    void prepare_source_composition() {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        if(compose_pipeline)return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(!spirv || curved_vulkan_precision(device),"Resident pixel composition requires Vulkan1.2 SignedZeroInfNanPreserve32");
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_compose_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.code=spirv?native_reflection_index_compose_spirv:native_reflection_index_compose_dxil;
        info.code_size=spirv?sizeof(native_reflection_index_compose_spirv):sizeof(native_reflection_index_compose_dxil);
        info.num_readonly_storage_buffers=2;info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=2;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        compose_pipeline=create_gpu_compute_pipeline(device,&info);require(compose_pipeline,SDL_GetError());
#else
        require(false,"Resident reflection pixel composition stage not compiled");
#endif
    }
    void prepare_source_publication(bool diagnostic) {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        auto*& selected=diagnostic?publish_diagnostic_pipeline:publish_pipeline;
        if(selected)return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint=diagnostic?"feature_publish_diagnostics_main":"feature_publish_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.code=spirv?native_reflection_index_publish_spirv:native_reflection_index_publish_dxil;
        info.code_size=spirv?sizeof(native_reflection_index_publish_spirv):sizeof(native_reflection_index_publish_dxil);
        if(diagnostic) {
            info.code=spirv?native_reflection_index_publish_diagnostics_spirv:native_reflection_index_publish_diagnostics_dxil;
            info.code_size=spirv?sizeof(native_reflection_index_publish_diagnostics_spirv):sizeof(native_reflection_index_publish_diagnostics_dxil);
        }
        info.num_readonly_storage_buffers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
        if(diagnostic)info.num_readwrite_storage_buffers=2;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        selected=create_gpu_compute_pipeline(device,&info);require(selected,SDL_GetError());
#else
        require(false,"Resident complete reflection publication not compiled");
#endif
    }
    void clear_admission(SDL_GPUCommandBuffer* command,SDL_GPUBuffer* mask,unsigned pixels) {
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        require(mask && pixels,"Missing bounded diagnostic admission mask");
        if(!admission_clear_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_admission_clear_main";
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.code=spirv?native_reflection_index_admission_clear_spirv:native_reflection_index_admission_clear_dxil;
            info.code_size=spirv?sizeof(native_reflection_index_admission_clear_spirv):sizeof(native_reflection_index_admission_clear_dxil);
            info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            admission_clear_pipeline=create_gpu_compute_pipeline(device,&info);require(admission_clear_pipeline,SDL_GetError());
        }
        const std::array<unsigned,4> uniform{pixels,0,0,0};
        SDL_GPUStorageBufferReadWriteBinding output{mask,false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,admission_clear_pipeline);
        SDL_PushGPUComputeUniformData(command,0,uniform.data(),sizeof(uniform));
        constexpr unsigned row_pixels=65535U*64U;
        SDL_DispatchGPUCompute(pass,std::min((pixels+63)/64,65535U),(pixels+row_pixels-1)/row_pixels,1);SDL_EndGPUComputePass(pass);
#else
        (void)command;(void)mask;(void)pixels;
        require(false,"Diagnostic source admission not compiled");
#endif
    }
    void encode_source_index(SDL_GPUCommandBuffer* command,Images& images,unsigned bank) {
        const auto& r=*images.source_layout;
        SourceIndexUniform uniform{r.width,r.height,r.lobes,r.primary_prefix,r.record_prefix,r.path_stride,r.total_nodes,r.level_count,
            0,0,0,0,r.levels};
        for(unsigned level=0;level<r.level_count;++level) {
            uniform.work_level=level;
            SDL_GPUStorageBufferReadWriteBinding output{images.source_indices[bank],false,0,0,0};
            auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&output,1);require(pass,SDL_GetError());
            SDL_BindGPUComputePipeline(pass,level?source_reduce:source_tiles);
            if(!level) SDL_BindGPUComputeStorageBuffers(pass,0,&images.buffers[bank],1);
            SDL_PushGPUComputeUniformData(command,0,&uniform,sizeof(uniform));
            SDL_DispatchGPUCompute(pass,level?(r.levels[level][1]+7)/8:r.levels[0][1],
                level?(r.levels[level][2]+7)/8:r.levels[0][2],r.lobes);
            SDL_EndGPUComputePass(pass);
        }
    }
    void prepare_lobes() {
        if(lobe_pipeline) return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="reflection_lobes_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.num_samplers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
        info.num_readonly_storage_buffers=3;info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        info.code=reflection_lobes_shader::lobes_spirv;info.code_size=sizeof(reflection_lobes_shader::lobes_spirv);
#if defined(_WIN32)
        if(!spirv) {info.code=reflection_lobes_shader::lobes_dxil;info.code_size=sizeof(reflection_lobes_shader::lobes_dxil);}
#endif
        lobe_pipeline=create_gpu_compute_pipeline(device,&info);require(lobe_pipeline,SDL_GetError());
    }
    void prepare_paths(bool curved) {
        auto*& selected=curved?curved_pipeline:path_pipeline;
        if(selected) return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="reflection_paths_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.num_samplers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
        info.num_readonly_storage_buffers=3;info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        info.code=reflection_paths_shader::paths_spirv;info.code_size=sizeof(reflection_paths_shader::paths_spirv);
        if(curved) {
            if(spirv) require(curved_vulkan_precision(device),"Curved reflection history requires Vulkan1.2 SignedZeroInfNanPreserve32; retaining current full-quality radiance");
            info.code=reflection_curved_paths_shader::curved_paths_spirv;
            info.code_size=sizeof(reflection_curved_paths_shader::curved_paths_spirv);
        }
#if defined(_WIN32)
        if(!spirv) {info.code=reflection_paths_shader::paths_dxil;info.code_size=sizeof(reflection_paths_shader::paths_dxil);}
        if(curved && !spirv) {info.code=reflection_curved_paths_shader::curved_paths_dxil;info.code_size=sizeof(reflection_curved_paths_shader::curved_paths_dxil);}
#endif
        selected=create_gpu_compute_pipeline(device,&info);require(selected,SDL_GetError());
    }
};
GpuCalibratedReflectionHistory::GpuCalibratedReflectionHistory():state_(std::make_unique<State>()) {}
GpuCalibratedReflectionHistory::~GpuCalibratedReflectionHistory()=default;
void GpuCalibratedReflectionHistory::release_device() noexcept {state_.reset();}
void GpuCalibratedReflectionHistory::reset() noexcept {
    if(state_) {state_->valid=false;state_->publication_next=0;state_->publication_started=state_->publication_complete=false;state_->queries={};state_->optical={};state_->local_roots={};state_->witness={};state_->folds={};state_->source_guide={};state_->source_colour={};state_->source_composition={};if(state_->pending) state_->pending_reset=true;}
}
void GpuCalibratedReflectionHistory::discard() noexcept {
    if(state_) {state_->pending=false;state_->pending_reset=false;state_->candidate={};state_->pending_images.reset();
        state_->publication_next=0;state_->publication_started=state_->publication_complete=false;
        state_->pending_command=nullptr;state_->pending_sources_compatible=false;state_->queries={};state_->optical={};state_->local_roots={};state_->witness={};state_->folds={};state_->source_guide={};state_->source_colour={};state_->source_composition={};state_->pending_guide={};}
}
void GpuCalibratedReflectionHistory::commit() noexcept {
    if(!state_ || !state_->pending) return;
    if(state_->pending_reset) {discard();return;}
    if(state_->pending_images) state_->accepted_images=std::move(state_->pending_images);
    if(state_->publication_complete) {
        auto& images=*state_->accepted_images;
        std::swap(images.buffers[state_->pending_index],images.publication_buffer);
        if(images.admission_diagnostics)std::swap(images.admission_masks[state_->pending_index],images.publication_mask);
        state_->candidate.buffer=images.buffers[state_->pending_index];
    }
    state_->index=state_->pending_index;state_->accepted=state_->pending_settings;
    state_->accepted_triangles=state_->pending_triangles;state_->accepted_cube=state_->pending_cube;
    state_->accepted_roughness=state_->pending_roughness;
    state_->accepted_ground=state_->pending_ground;
    state_->accepted_liquid=state_->pending_liquid;
    state_->accepted_liquid_projection=state_->pending_liquid_projection;
    state_->accepted_liquid_clip=state_->pending_liquid_clip;
    state_->accepted_output=state_->candidate;
    state_->valid=true;state_->pending=false;state_->candidate={};
    state_->publication_next=0;state_->publication_started=state_->publication_complete=false;
    state_->pending_command=nullptr;state_->pending_sources_compatible=false;state_->queries={};state_->optical={};state_->local_roots={};state_->witness={};state_->folds={};state_->source_guide={};state_->source_colour={};state_->source_composition={};state_->pending_guide={};
    if(!state_->accepted.source_index_validation){state_->query_work.reset();state_->optical_work.reset();state_->optical_stream_work.reset();state_->local_root_work.reset();
        if(state_->accepted_images->publication_buffer) {
            SDL_ReleaseGPUBuffer(state_->device,state_->accepted_images->publication_buffer);state_->accepted_images->publication_buffer=nullptr;
        }
    }
}
shadows::GpuReflectionOutput GpuCalibratedReflectionHistory::output() const noexcept {
    auto output=fresh_current_output();
    if(output.buffer && state_->publication_complete) {
        const auto* images=state_->pending_images?state_->pending_images.get():state_->accepted_images.get();
        output.buffer=images->publication_buffer;
    }
    return output;
}
shadows::GpuReflectionOutput GpuCalibratedReflectionHistory::fresh_current_output() const noexcept {
    return state_ && state_->pending && !state_->pending_reset?state_->candidate:shadows::GpuReflectionOutput{};
}
shadows::GpuReflectionOutput GpuCalibratedReflectionHistory::accepted_output() const noexcept {
    return state_ && state_->valid?state_->accepted_output:shadows::GpuReflectionOutput{};
}
GpuReflectionSourceAdmission GpuCalibratedReflectionHistory::accepted_source_admission() const noexcept {
    if(!state_ || !state_->valid || state_->pending || !state_->accepted.source_admission_diagnostics
        || !state_->accepted_images)return {};
    const auto& images=*state_->accepted_images;
    return {state_->device,images.admission_masks[state_->index],images.width,images.height,images.width*images.height*4};
}
GpuReflectionSourceIndex GpuCalibratedReflectionHistory::source_index() const noexcept {
    if(!state_ || !state_->pending || state_->pending_reset) return {};
    const auto* images=state_->pending_images?state_->pending_images.get():state_->accepted_images.get();
    if(!images || !images->source_layout) return {};
    const auto bank=state_->pending_index;
    return {state_->device,images->source_indices[bank],state_->publication_complete?images->publication_buffer:images->buffers[bank],*images->source_layout};
}
GpuReflectionSourceIndex GpuCalibratedReflectionHistory::accepted_source_index() const noexcept {
    if(!state_ || !state_->valid || !state_->accepted_images || !state_->accepted_images->source_layout) return {};
    const auto& images=*state_->accepted_images;
    return {state_->device,images.source_indices[state_->index],images.buffers[state_->index],*images.source_layout};
}
GpuReflectionSourceQueries GpuCalibratedReflectionHistory::source_queries() const noexcept {
    return state_ && state_->pending && !state_->pending_reset?state_->queries:GpuReflectionSourceQueries{};
}
std::optional<std::uint64_t> GpuCalibratedReflectionHistory::source_query_allocation_bytes() const noexcept {
    if(!state_)return {};
    const auto bytes=working_image_bytes()+(state_->query_work?0:GpuReflectionSourceQueries::working_bytes);
    return bytes<=1024ULL*1024*1024?std::optional{bytes}:std::nullopt;
}
bool GpuCalibratedReflectionHistory::enqueue_source_queries(void* command,std::uint64_t first,std::uint32_t count,
    const ReflectionSourceQueryLimits& limits) {
    if(!state_)return false;
    auto& s=*state_;s.queries={};s.optical={};s.local_roots={};s.witness={};s.folds={};s.source_guide={};s.source_colour={};s.source_composition={};
    try {
        require(command && command==s.pending_command && s.pending && !s.pending_reset && s.valid
            && s.pending_sources_compatible && s.pending_settings.source_index_validation,
            "No compatible pending command/accepted reflection source; retaining CURRENT radiance");
        const auto source=accepted_source_index();require(source.buffer,"Accepted native source index unavailable");
        const auto total=std::uint64_t(s.candidate.width)*s.candidate.height*source.layout.lobes;
        require(!s.publication_complete && (!s.publication_started || first==s.publication_next),
            "Completed/gapped reflection publication cannot query another partial frame");
        require(count && count<=GpuReflectionSourceQueries::capacity && first<total && count<=total-first
            && first<=UINT32_MAX && limits.valid(),"Unbounded/invalid native source query range or quota");
        require(source_query_allocation_bytes().has_value(),"Native source-query scratch exceeds the combined one-GiB bound");
        s.prepare_source_queries();s.make_query_work();
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        const auto& guide=s.pending_guide;const auto& current=s.candidate.reflection_history;
        const auto pixels=s.candidate.width*s.candidate.height;
        SourceMappingUniform mapping{s.candidate.width,s.candidate.height,current.identity_offset/pixels,
            current.incoming_offset/pixels,source.layout.path_stride,s.pending_triangles,s.accepted_triangles,
            guide.history.previous_index_offset,guide.bytes,source.layout.lobes,unsigned(first),count,
            (current.scene_paths?1U:0U)|(current.curved_paths?2U:0U)|(current.curved_receivers?4U:0U),0,0,0,{},{}};
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c) {
            mapping.current_cube[r][c]=guide.current_cube[r*3+c];mapping.previous_cube[r][c]=guide.previous_cube[r*3+c];
        }
        const auto& r=source.layout;
        const SourceIndexUniform uniform{r.width,r.height,r.lobes,r.primary_prefix,r.record_prefix,r.path_stride,r.total_nodes,r.level_count,
            0,count,limits.leaves,limits.nodes,r.levels};
        const std::array<unsigned,4> domains{limits.regions,limits.supports,128,0};
        for(unsigned stage=0;stage<3;++stage) {
            SDL_GPUStorageBufferReadWriteBinding output{s.query_work->buffers[stage],false,0,0,0};
            auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,&output,1);require(pass,SDL_GetError());
            SDL_BindGPUComputePipeline(pass,s.query_pipelines[stage]);
            SDL_GPUBuffer* read[3]{};
            if(stage==0){read[0]=static_cast<SDL_GPUBuffer*>(s.candidate.buffer);read[1]=static_cast<SDL_GPUBuffer*>(guide.buffer);}
            else if(stage==1){read[0]=static_cast<SDL_GPUBuffer*>(source.buffer);read[1]=s.query_work->buffers[0];}
            else {read[0]=static_cast<SDL_GPUBuffer*>(source.source_buffer);read[1]=s.query_work->buffers[0];read[2]=s.query_work->buffers[1];}
            SDL_BindGPUComputeStorageBuffers(pass,0,read,stage==2?3:2);
            if(stage==0)SDL_PushGPUComputeUniformData(cb,0,&mapping,sizeof(mapping));
            else SDL_PushGPUComputeUniformData(cb,0,&uniform,sizeof(uniform));
            if(stage==2)SDL_PushGPUComputeUniformData(cb,1,domains.data(),sizeof(domains));
            SDL_DispatchGPUCompute(pass,stage==2?count:(count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        }
        s.queries={s.device,s.query_work->buffers[0],s.query_work->buffers[1],s.query_work->buffers[2],s.candidate.buffer,
            source,unsigned(first),count,limits};
        s.status="Native mapped source candidates/supports encoded; no root/colour acceptance";return true;
    }catch(const std::exception& e){s.status=e.what();return false;}
}
GpuReflectionSourceOptical GpuCalibratedReflectionHistory::source_optical() const noexcept {
    return state_ && state_->pending && !state_->pending_reset?state_->optical:GpuReflectionSourceOptical{};
}
std::optional<std::uint64_t> GpuCalibratedReflectionHistory::source_optical_allocation_bytes() const noexcept {
    const auto queries=source_query_allocation_bytes();if(!queries)return {};
    const auto bytes=*queries+(state_->optical_work?0:GpuReflectionSourceOptical::working_bytes);
    const auto complete=bytes+(state_->source_stream_encoding && !state_->optical_stream_work?GpuReflectionSourceOptical::stream_working_bytes:0);
    return complete<=1024ULL*1024*1024?std::optional{complete}:std::nullopt;
}
bool GpuCalibratedReflectionHistory::enqueue_source_optical(void* command,const ReflectionSourceOpticalLimits& limits) {
    if(!state_)return false;
    auto& s=*state_;s.optical={};s.local_roots={};s.witness={};s.folds={};s.source_guide={};s.source_colour={};s.source_composition={};
    try {
        require(command && command==s.pending_command && s.pending && !s.pending_reset && s.valid
            && s.pending_sources_compatible && s.queries.queries && limits.valid(),
            "No compatible pending native source query/optical quota; retaining CURRENT radiance");
        const auto& guide=s.pending_guide;const auto& q=s.queries;const auto& r=q.source.layout;
        // The current producer explicitly supplies its eye calibration. Old
        // optics must equal the calibration captured with the accepted bank;
        // absent optional planar calibration is refusal, never a guessed eye.
        require(guide.history.projection==s.accepted_liquid_projection
            && std::array{guide.history.near_plane,guide.history.far_plane}==s.accepted_liquid_clip,
            "Accepted eye calibration missing/mismatched for native source optics");
        require(source_optical_allocation_bytes().has_value(),"Native source optics exceed the combined one-GiB bound");
        const bool stream=s.source_stream_encoding && s.can_stream_optical();
        s.prepare_source_optical(stream);s.make_optical_work();auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        const unsigned pixels=s.candidate.width*s.candidate.height;
        SourceFrameUniform frame{s.accepted_triangles,guide.history.previous_vertex_offset,guide.history.previous_index_offset,guide.bytes,
            s.candidate.width,s.candidate.height,s.candidate.reflection_history.incoming_offset/pixels,r.path_stride,
            r.lobes,q.first,q.count,(s.candidate.reflection_history.scene_paths?1U:0U)
                |(s.candidate.reflection_history.curved_paths?2U:0U)|(s.candidate.reflection_history.curved_receivers?4U:0U),
            s.accepted_roughness,0,0,0,{},{},{},{},shadows::reflection_liquid_frame_words(guide.history)};
        for(unsigned c=0;c<4;++c)frame.projection[c]=float(guide.history.projection[c]);
        frame.extent_clip={float(r.width),float(r.height),float(guide.history.near_plane),float(guide.history.far_plane)};
        if(guide.history.previous_ground) {
            for(unsigned c=0;c<3;++c){frame.point[c]=float(guide.history.previous_ground->point[c]);frame.normal[c]=float(guide.history.previous_ground->normal[c]);}
            frame.point[3]=1;
        }
        const SourceIndexUniform index{r.width,r.height,r.lobes,r.primary_prefix,r.record_prefix,r.path_stride,r.total_nodes,r.level_count,
            0,q.count,q.limits.leaves,q.limits.nodes,r.levels};
        const std::array<unsigned,4> optical_limits{limits.boxes,limits.depth,128,0};SourceTargetUniform target;
        for(unsigned row=0;row<3;++row)for(unsigned c=0;c<3;++c) {
            target.current[row][c]=guide.current_cube[row*3+c];target.previous[row][c]=guide.previous_cube[row*3+c];
        }
        for(unsigned stage=0;stage<2;++stage) {
            if(stage && stream)s.schedule_optical(cb,q);
            SDL_GPUStorageBufferReadWriteBinding output{s.optical_work->buffers[stage],false,0,0,0};
            auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,&output,1);require(pass,SDL_GetError());
            SDL_BindGPUComputePipeline(pass,stage && stream?s.optical_stream_pipeline:s.optical_pipelines[stage]);
            SDL_GPUBuffer* reads[]{stage?static_cast<SDL_GPUBuffer*>(q.regions):static_cast<SDL_GPUBuffer*>(q.queries),
                stage?s.optical_work->buffers[0]:static_cast<SDL_GPUBuffer*>(guide.buffer),
                stage?static_cast<SDL_GPUBuffer*>(q.queries):static_cast<SDL_GPUBuffer*>(q.current_buffer),
                s.optical_stream_work?s.optical_stream_work->buffers[0]:nullptr};
            SDL_BindGPUComputeStorageBuffers(pass,0,reads,stage && stream?4:3);
            if(!stage)SDL_PushGPUComputeUniformData(cb,0,&frame,sizeof(frame));
            else {SDL_PushGPUComputeUniformData(cb,0,&index,sizeof(index));
                SDL_PushGPUComputeUniformData(cb,1,optical_limits.data(),sizeof(optical_limits));
                SDL_PushGPUComputeUniformData(cb,2,&target,sizeof(target));}
            if(stage && stream)SDL_DispatchGPUComputeIndirect(pass,s.optical_stream_work->buffers[1],0);
            else SDL_DispatchGPUCompute(pass,stage?128:(q.count+63)/64,stage?q.count:1,1);
            SDL_EndGPUComputePass(pass);
        }
        s.optical={q,s.optical_work->buffers[0],s.optical_work->buffers[1],limits};
        s.status="Native GPU accepted optical frames/continuum exclusions encoded; no root or colour acceptance";return true;
    }catch(const std::exception& e){s.status=e.what();return false;}
}
GpuReflectionSourceLocalRoots GpuCalibratedReflectionHistory::source_local_roots() const noexcept {
    return state_ && state_->pending && !state_->pending_reset && !state_->root_hull_encoded
        ?state_->local_roots:GpuReflectionSourceLocalRoots{};
}
GpuReflectionSourceRootHull GpuCalibratedReflectionHistory::source_root_hull() const noexcept {
    if(!state_ || !state_->pending || state_->pending_reset || !state_->root_hull_encoded || !state_->local_roots.results)return {};
    return {state_->local_roots.optical,state_->local_roots.results};
}
std::optional<std::uint64_t> GpuCalibratedReflectionHistory::source_local_root_allocation_bytes() const noexcept {
    const auto optical=source_optical_allocation_bytes();if(!optical)return {};
    const auto bytes=*optical+(state_->local_root_work?0:GpuReflectionSourceLocalRoots::working_bytes);
    return bytes<=1024ULL*1024*1024?std::optional{bytes}:std::nullopt;
}
bool GpuCalibratedReflectionHistory::enqueue_source_local_roots(void* command) {
    return enqueue_source_root_stage(command,false);
}
bool GpuCalibratedReflectionHistory::enqueue_source_root_hull(void* command) {
    return enqueue_source_root_stage(command,true);
}
bool GpuCalibratedReflectionHistory::enqueue_source_root_stage(void* command,bool whole_cover) {
    if(!state_)return false;
    auto& s=*state_;s.local_roots={};s.witness={};s.folds={};s.source_guide={};s.source_colour={};s.source_composition={};
    try {
        require(command && command==s.pending_command && s.pending && !s.pending_reset && s.valid
            && s.pending_sources_compatible && s.pending_settings.source_index_validation
            && s.optical.frames && s.optical.results && s.optical.queries.queries==s.queries.queries
            && s.optical.queries.regions==s.queries.regions && s.optical.queries.count==s.queries.count
            && s.optical.queries.first==s.queries.first,
            "No compatible pending SAME-command native optical batch; retaining CURRENT radiance");
        require(source_local_root_allocation_bytes().has_value(),"Native local roots exceed the combined one-GiB bound");
        const bool stream=whole_cover && s.source_stream_encoding;
        s.prepare_source_local_roots(stream);s.make_local_root_work();
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);const auto& q=s.queries;const auto& r=q.source.layout;
        const SourceIndexUniform index{r.width,r.height,r.lobes,r.primary_prefix,r.record_prefix,r.path_stride,r.total_nodes,r.level_count,
            0,q.count,q.limits.leaves,q.limits.nodes,r.levels};
        const std::array<unsigned,4> limits{GpuReflectionSourceLocalRoots::region_capacity,GpuReflectionSourceOptical::frame_stride,
            GpuReflectionSourceLocalRoots::record_stride,unsigned(whole_cover)};SourceTargetUniform target;
        for(unsigned row=0;row<3;++row)for(unsigned c=0;c<3;++c) {
            target.current[row][c]=s.pending_guide.current_cube[row*3+c];target.previous[row][c]=s.pending_guide.previous_cube[row*3+c];
        }
        SDL_GPUStorageBufferReadWriteBinding output{s.local_root_work->buffers[0],false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,stream?s.stream_root_pipeline:s.local_root_pipeline);
        SDL_GPUBuffer* reads[]{static_cast<SDL_GPUBuffer*>(s.optical.frames),static_cast<SDL_GPUBuffer*>(q.regions),
            static_cast<SDL_GPUBuffer*>(q.queries),static_cast<SDL_GPUBuffer*>(s.optical.results)};
        SDL_BindGPUComputeStorageBuffers(pass,0,reads,4);
        SDL_PushGPUComputeUniformData(cb,0,&index,sizeof(index));SDL_PushGPUComputeUniformData(cb,1,limits.data(),sizeof(limits));
        SDL_PushGPUComputeUniformData(cb,2,&target,sizeof(target));
        SDL_DispatchGPUCompute(pass,stream?(q.count+63)/64:(GpuReflectionSourceLocalRoots::region_capacity+63)/64,stream?1:q.count,1);SDL_EndGPUComputePass(pass);
        s.local_roots={s.optical,s.local_root_work->buffers[0]};
        s.root_hull_encoded=whole_cover;
        s.status=whole_cover?"Native GPU whole necessary-source hull classified; source membership/colour acceptance still required"
            :"Native GPU local-root hull classifications encoded; no global source or colour acceptance";return true;
    }catch(const std::exception& e){s.status=e.what();return false;}
}
GpuReflectionSourceWitness GpuCalibratedReflectionHistory::source_witness() const noexcept {
    return state_ && state_->pending && !state_->pending_reset?state_->witness:GpuReflectionSourceWitness{};
}
bool GpuCalibratedReflectionHistory::enqueue_source_witness(void* command) {
    return enqueue_source_guard_stage(command,0);
}
GpuReflectionSourceFolds GpuCalibratedReflectionHistory::source_folds() const noexcept {
    return state_ && state_->pending && !state_->pending_reset?state_->folds:GpuReflectionSourceFolds{};
}
bool GpuCalibratedReflectionHistory::enqueue_source_folds(void* command) {
    return enqueue_source_guard_stage(command,1);
}
GpuReflectionSourceGuide GpuCalibratedReflectionHistory::source_guide() const noexcept {
    return state_ && state_->pending && !state_->pending_reset?state_->source_guide:GpuReflectionSourceGuide{};
}
bool GpuCalibratedReflectionHistory::enqueue_source_guide(void* command) {
    return enqueue_source_guard_stage(command,2);
}
bool GpuCalibratedReflectionHistory::enqueue_source_guard_stage(void* command,unsigned stage) {
    if(!state_)return false;
    auto& s=*state_;const auto fold=s.folds;const auto witness=stage==2?fold.witness:s.witness;
    const auto roots=stage?GpuReflectionSourceLocalRoots{witness.hull.optical,witness.results}:s.local_roots;
    s.local_roots={};s.witness={};s.folds={};s.source_guide={};s.source_colour={};s.source_composition={};
    try {
        require(stage<=2 && (stage!=2 || (fold.results && fold.results==witness.results))
            && command && command==s.pending_command && s.pending && !s.pending_reset && s.valid
            && s.pending_sources_compatible && s.pending_settings.source_index_validation && s.root_hull_encoded
            && roots.results && roots.optical.frames==s.optical.frames && roots.optical.results==s.optical.results
            && roots.optical.queries.queries==s.queries.queries && roots.optical.queries.regions==s.queries.regions
            && roots.optical.queries.count==s.queries.count && roots.optical.queries.first==s.queries.first
            && s.local_root_work && roots.results==s.local_root_work->buffers[0],
            "No compatible SAME-command whole-source proof for native source guard; retaining CURRENT radiance");
        require(source_local_root_allocation_bytes().has_value(),"Native source witness exceeds the combined one-GiB bound");
        s.prepare_source_witness(stage);auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);const auto& q=s.queries;const auto& r=q.source.layout;
        const SourceIndexUniform index{r.width,r.height,r.lobes,r.primary_prefix,r.record_prefix,r.path_stride,r.total_nodes,r.level_count,
            0,q.count,q.limits.leaves,q.limits.nodes,r.levels};
        const std::array<unsigned,4> settings{GpuReflectionSourceWitness::result_stride,
            stage==2?GpuReflectionSourceGuide::record_offset:stage==1?GpuReflectionSourceFolds::record_offset:GpuReflectionSourceWitness::record_offset,
            GpuReflectionSourceWitness::record_bytes,0};SourceTargetUniform target;
        if(stage!=1)for(unsigned row=0;row<3;++row)for(unsigned c=0;c<3;++c) {
            target.current[row][c]=s.pending_guide.current_cube[row*3+c];target.previous[row][c]=s.pending_guide.previous_cube[row*3+c];
        }
        SDL_GPUStorageBufferReadWriteBinding output{static_cast<SDL_GPUBuffer*>(roots.results),false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,stage==2?s.guide_pipeline:stage==1?s.folds_pipeline:s.witness_pipeline);
        SDL_GPUBuffer* reads[]{static_cast<SDL_GPUBuffer*>(s.optical.frames),static_cast<SDL_GPUBuffer*>(q.queries),
            static_cast<SDL_GPUBuffer*>(q.source.source_buffer)};
        SDL_BindGPUComputeStorageBuffers(pass,0,reads,stage==2?2U:3U);SDL_PushGPUComputeUniformData(cb,0,&index,sizeof(index));
        SDL_PushGPUComputeUniformData(cb,1,settings.data(),sizeof(settings));
        if(stage!=1)SDL_PushGPUComputeUniformData(cb,2,&target,sizeof(target));
        SDL_DispatchGPUCompute(pass,(q.count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        if(stage==2)s.source_guide={fold,roots.results};
        else if(stage==1)s.folds={witness,roots.results};else s.witness={{roots.optical,roots.results},roots.results};
        s.status=stage==2?"Native finite-face/angular guide encoded; CURRENT colour/complete lobe gates remain required"
            :stage==1?"Native accepted-source fold guards encoded; angular/current-colour gates remain required"
            :"Native accepted-source footprint witness encoded; fold/angular/current-colour gates remain required";return true;
    }catch(const std::exception& e){s.status=e.what();return false;}
}
GpuReflectionSourceColour GpuCalibratedReflectionHistory::source_colour() const noexcept {
    return state_ && state_->pending && !state_->pending_reset?state_->source_colour:GpuReflectionSourceColour{};
}
bool GpuCalibratedReflectionHistory::enqueue_source_colour(void* command) {
    if(!state_)return false;
    auto& s=*state_;const auto guide=s.source_guide;
    s.local_roots={};s.witness={};s.folds={};s.source_guide={};s.source_colour={};s.source_composition={};
    try {
        const auto& q=guide.folds.witness.hull.optical.queries;
        require(command && command==s.pending_command && s.pending && !s.pending_reset && s.valid
            && s.pending_sources_compatible && s.pending_settings.source_index_validation && s.root_hull_encoded
            && s.pending_settings.weight>0 && guide.results && guide.results==guide.folds.results
            && guide.results==guide.folds.witness.results && s.local_root_work && guide.results==s.local_root_work->buffers[0]
            && guide.folds.witness.hull.optical.frames==s.optical.frames
            && q.queries==s.queries.queries && q.regions==s.queries.regions && q.first==s.queries.first && q.count==s.queries.count
            && q.current_buffer==s.candidate.buffer && q.source.device==s.device && s.accepted_images
            && q.source.source_buffer==s.accepted_output.buffer && q.source.source_buffer!=q.current_buffer,
            "No compatible SAME-command certified colour source and fresh CURRENT bank; retaining CURRENT radiance");
        require(source_local_root_allocation_bytes().has_value(),"Native source colour exceeds the combined one-GiB bound");
        s.prepare_source_colour();auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);const auto& r=q.source.layout;
        const unsigned pixels=s.candidate.width*s.candidate.height;const auto& current=s.candidate.reflection_history;
        const unsigned flags=unsigned(s.pending_settings.srgb)|(unsigned(current.scene_paths)<<1)
            |(unsigned(current.curved_receivers)<<2)
            |(current.curved_receivers && s.pending_guide.history.previous_liquid->material!=0?8U:0U);
        const SourceIndexUniform index{r.width,r.height,r.lobes,r.primary_prefix,r.record_prefix,r.path_stride,r.total_nodes,r.level_count,
            0,q.count,q.limits.leaves,q.limits.nodes,r.levels};
        const SourceColourUniform colour{s.candidate.width,s.candidate.height,current.identity_offset/pixels,current.incoming_offset/pixels,
            s.pending_triangles,q.first,GpuReflectionSourceColour::result_stride,GpuReflectionSourceColour::record_offset,
            GpuReflectionSourceColour::record_bytes,flags,q.count,0,s.pending_settings.weight,r.lobes,r.path_stride,0};
        SDL_GPUStorageBufferReadWriteBinding output{static_cast<SDL_GPUBuffer*>(guide.results),false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,s.colour_pipeline);
        SDL_GPUBuffer* reads[]{static_cast<SDL_GPUBuffer*>(q.current_buffer),static_cast<SDL_GPUBuffer*>(q.source.source_buffer),
            static_cast<SDL_GPUBuffer*>(q.queries)};
        SDL_BindGPUComputeStorageBuffers(pass,0,reads,3);
        SDL_PushGPUComputeUniformData(cb,0,&index,sizeof(index));SDL_PushGPUComputeUniformData(cb,1,&colour,sizeof(colour));
        SDL_DispatchGPUCompute(pass,(q.count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        s.source_colour={guide,guide.results};
        s.status="Certified incident RGB clamped to fresh CURRENT neighbours; complete-pixel/lobe composition still required";return true;
    }catch(const std::exception& e){s.status=e.what();return false;}
}
GpuReflectionSourceComposition GpuCalibratedReflectionHistory::source_composition() const noexcept {
    return state_ && state_->pending && !state_->pending_reset?state_->source_composition:GpuReflectionSourceComposition{};
}
bool GpuCalibratedReflectionHistory::enqueue_source_composition(void* command) {
    if(!state_)return false;
    auto& s=*state_;const auto colour=s.source_colour;
    s.local_roots={};s.witness={};s.folds={};s.source_guide={};s.source_colour={};s.source_composition={};
    try {
        const auto& q=colour.guide.folds.witness.hull.optical.queries;
        require(command && command==s.pending_command && s.pending && !s.pending_reset && s.valid
            && s.pending_sources_compatible && s.pending_settings.source_index_validation && s.root_hull_encoded
            && s.pending_settings.weight>0 && colour.results && colour.results==colour.guide.results
            && s.local_root_work && colour.results==s.local_root_work->buffers[0]
            && q.queries==s.queries.queries && q.regions==s.queries.regions && q.first==s.queries.first && q.count==s.queries.count
            && q.current_buffer==s.candidate.buffer && q.source.device==s.device && s.accepted_images
            && q.source.source_buffer==s.accepted_output.buffer && q.source.source_buffer!=q.current_buffer,
            "No compatible SAME-command incident colours and fresh CURRENT materials; retaining CURRENT pixel");
        const auto& r=q.source.layout;
        require(q.count && q.count%r.lobes==0 && q.first%r.lobes==0,
            "Pixel composition requires complete aligned lobe batches; retaining CURRENT pixel");
        require(source_local_root_allocation_bytes().has_value(),"Native pixel composition exceeds the combined one-GiB bound");
        s.prepare_source_composition();auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        const unsigned pixels=s.candidate.width*s.candidate.height;const auto& current=s.candidate.reflection_history;
        const unsigned flags=unsigned(s.pending_settings.srgb)|(unsigned(current.scene_paths)<<1)
            |(unsigned(current.curved_receivers)<<2)
            |(current.curved_receivers && s.pending_guide.history.previous_liquid->material!=0?8U:0U);
        const SourceIndexUniform index{r.width,r.height,r.lobes,r.primary_prefix,r.record_prefix,r.path_stride,r.total_nodes,r.level_count,
            0,q.count,q.limits.leaves,q.limits.nodes,r.levels};
        const SourceColourUniform compose{s.candidate.width,s.candidate.height,current.identity_offset/pixels,current.incoming_offset/pixels,
            s.pending_triangles,q.first,GpuReflectionSourceComposition::result_stride,GpuReflectionSourceComposition::record_offset,
            GpuReflectionSourceComposition::record_bytes,flags,q.count,0,s.pending_settings.weight,r.lobes,r.path_stride,0};
        SDL_GPUStorageBufferReadWriteBinding output{static_cast<SDL_GPUBuffer*>(colour.results),false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,s.compose_pipeline);
        SDL_GPUBuffer* reads[]{static_cast<SDL_GPUBuffer*>(q.current_buffer),static_cast<SDL_GPUBuffer*>(q.queries)};
        SDL_BindGPUComputeStorageBuffers(pass,0,reads,2);
        SDL_PushGPUComputeUniformData(cb,0,&index,sizeof(index));SDL_PushGPUComputeUniformData(cb,1,&compose,sizeof(compose));
        SDL_DispatchGPUCompute(pass,(q.count/r.lobes+63)/64,1,1);SDL_EndGPUComputePass(pass);
        s.source_composition={colour,colour.results};
        s.status="Whole CURRENT-material pixel packet staged; fresh bank and presentation remain unchanged";return true;
    }catch(const std::exception& e){s.status=e.what();return false;}
}
bool GpuCalibratedReflectionHistory::source_publication_complete() const noexcept {
    return state_ && state_->pending && !state_->pending_reset && state_->publication_complete;
}
std::optional<std::uint64_t> GpuCalibratedReflectionHistory::source_publication_allocation_bytes() const noexcept {
    if(!state_ || !state_->pending || state_->pending_reset || !state_->pending_settings.source_index_validation)return {};
    const auto* images=state_->pending_images?state_->pending_images.get():state_->accepted_images.get();
    const auto scratch=source_local_root_allocation_bytes();
    if(!images || !images->source_layout || !scratch)return {};
    const auto bytes=*scratch+(images->publication_buffer?0:images->storage_bytes)
        +(images->admission_diagnostics && !images->publication_mask?std::uint64_t(images->width)*images->height*4:0);
    return bytes<=1024ULL*1024*1024?std::optional{bytes}:std::nullopt;
}
bool GpuCalibratedReflectionHistory::enqueue_source_publication(void* command) {
    if(!state_)return false;
    auto& s=*state_;const auto composed=s.source_composition;
    try {
        const auto& q=composed.colour.guide.folds.witness.hull.optical.queries;
        require(command && command==s.pending_command && s.pending && !s.pending_reset && s.valid
            && s.pending_sources_compatible && s.pending_settings.source_index_validation && s.root_hull_encoded
            && s.pending_settings.weight>0 && !s.publication_complete && composed.results && s.local_root_work
            && composed.results==s.local_root_work->buffers[0] && composed.results==composed.colour.results
            && q.queries==s.queries.queries && q.regions==s.queries.regions && q.first==s.queries.first && q.count==s.queries.count
            && q.current_buffer==s.candidate.buffer && q.source.device==s.device
            && q.source.source_buffer==s.accepted_output.buffer && q.source.source_buffer!=q.current_buffer,
            "No compatible SAME-command whole-pixel packet for reflection publication");
        const auto& r=q.source.layout;const auto& current=s.candidate.reflection_history;
        const auto total=std::uint64_t(s.candidate.width)*s.candidate.height*r.lobes;
        require(q.first==s.publication_next && q.count && q.count%r.lobes==0 && q.first%r.lobes==0
            && q.count<=total-s.publication_next,"Reflection publication requires ordered full-frame aligned batches from zero");
        require(source_publication_allocation_bytes().has_value(),"Complete reflection publication exceeds the combined one-GiB bound");
        auto& images=*(s.pending_images?s.pending_images:s.accepted_images);
        s.prepare_source_publication(images.admission_diagnostics);
        if(!images.publication_buffer) {
            const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,images.storage_bytes,0};
            images.publication_buffer=SDL_CreateGPUBuffer(s.device,&info);require(images.publication_buffer,SDL_GetError());
        }
        if(images.admission_diagnostics && !images.publication_mask) {
            const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,images.width*images.height*4,0};
            images.publication_mask=SDL_CreateGPUBuffer(s.device,&info);require(images.publication_mask,SDL_GetError());
        }
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        if(!s.publication_started) {
            auto* copy=SDL_BeginGPUCopyPass(cb);require(copy,SDL_GetError());
            const SDL_GPUBufferLocation from{static_cast<SDL_GPUBuffer*>(s.candidate.buffer),0},to{images.publication_buffer,0};
            SDL_CopyGPUBufferToBuffer(copy,&from,&to,images.storage_bytes,false);SDL_EndGPUCopyPass(copy);
            if(images.admission_diagnostics)s.clear_admission(cb,images.publication_mask,images.width*images.height);
        }
        const unsigned pixels=s.candidate.width*s.candidate.height;
        const std::array<unsigned,8> uniform{s.candidate.width,s.candidate.height,current.incoming_offset/pixels,
            r.path_stride,q.first,q.count,r.lobes,0};
        SDL_GPUStorageBufferReadWriteBinding outputs[]{{images.publication_buffer,false,0,0,0},{images.publication_mask,false,0,0,0}};
        auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,outputs,images.admission_diagnostics?2:1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,images.admission_diagnostics?s.publish_diagnostic_pipeline:s.publish_pipeline);
        auto* packets=static_cast<SDL_GPUBuffer*>(composed.results);SDL_BindGPUComputeStorageBuffers(pass,0,&packets,1);
        SDL_PushGPUComputeUniformData(cb,0,uniform.data(),sizeof(uniform));
        SDL_DispatchGPUCompute(pass,(q.count/r.lobes+63)/64,1,1);SDL_EndGPUComputePass(pass);
        s.publication_started=true;s.publication_next+=q.count;s.publication_complete=s.publication_next==total;
        s.local_roots={};s.witness={};s.folds={};s.source_guide={};s.source_colour={};s.source_composition={};
        s.status=s.publication_complete?"Complete separately composed reflection frame ready; accepted image/index unchanged until presentation commit"
            :"Reflection batch staged in separate image; partial frame hidden and fresh CURRENT retained";
        return true;
    }catch(const std::exception& e){s.status=e.what();return false;}
}
bool GpuCalibratedReflectionHistory::enqueue_source_frame(void* command,SourceBatchObserver observer,void* context,
    ReflectionSourceStageObserver stages,void* stage_context) {
    if(!state_)return false;
    auto& s=*state_;
    if((!observer && context) || (!stages && stage_context) || !command || command!=s.pending_command || !s.pending || s.pending_reset
        || !s.pending_settings.source_index_validation || s.publication_started || s.publication_complete) {
        s.status="Full reflection source stream requires a new compatible pending indexed command";return false;
    }
    if(!s.valid || !s.pending_sources_compatible || s.pending_settings.weight==0) {
        s.status="Cold/cut/zero-weight source frame retains complete fresh CURRENT; no old radiance admitted";return true;
    }
    const auto source=source_index();
    if(!source.buffer || !source_frame_allocation_bytes()) {
        s.status="Complete reflection source frame has no bounded native image/index allocation";return false;
    }
    // Preserve the complete public scratch ABI and all mathematical guards.
    // Once-only clearing allows the stream's exact same hull writer to place
    // useful queries in adjacent GPU lanes instead of 127 inactive regions.
    try {s.clear_stream_roots(static_cast<SDL_GPUCommandBuffer*>(command));}
    catch(const std::exception& e) {s.status=e.what();return false;}
    s.source_stream_encoding=true;
    struct StreamScope {bool& active;~StreamScope(){active=false;}} scope{s.source_stream_encoding};
    const auto total=std::uint64_t(s.candidate.width)*s.candidate.height*source.layout.lobes;
    for(std::uint64_t first=0;first<total;) {
        const auto count=std::uint32_t(std::min<std::uint64_t>(GpuReflectionSourceQueries::capacity,total-first));
        const auto mark=[&](ReflectionSourceStage stage) {
            if(!stages)return true;
            try {stages(stage_context,{s.device,command,first,total,count,0,0,stage});return true;}
            catch(const std::exception& e){s.status=e.what();return false;}
        };
        if(!mark(ReflectionSourceStage::begin)
            || !enqueue_source_queries(command,first,count) || !mark(ReflectionSourceStage::queries)
            || !enqueue_source_optical(command) || !mark(ReflectionSourceStage::optical)
            || !enqueue_source_root_hull(command) || !mark(ReflectionSourceStage::roots)
            || !enqueue_source_witness(command) || !mark(ReflectionSourceStage::witness)
            || !enqueue_source_folds(command) || !mark(ReflectionSourceStage::folds)
            || !enqueue_source_guide(command) || !mark(ReflectionSourceStage::guide)
            || !enqueue_source_colour(command) || !mark(ReflectionSourceStage::colour)
            || !enqueue_source_composition(command) || !mark(ReflectionSourceStage::composition))return false;
        if(observer) {
            try {observer(context,command,s.source_composition);}
            catch(const std::exception& e){s.status=e.what();return false;}
        }
        if(!enqueue_source_publication(command) || !mark(ReflectionSourceStage::publication))return false;
        first+=count;
    }
    return source_publication_complete();
}
const std::string& GpuCalibratedReflectionHistory::status() const noexcept {
    static const std::string released{"Native reflection history released"};return state_?state_->status:released;
}
std::optional<std::uint64_t> GpuCalibratedReflectionHistory::source_frame_allocation_bytes() const noexcept {
    const auto publication=source_publication_allocation_bytes();if(!publication)return {};
    const auto bytes=*publication+(!state_->optical_stream_work && !state_->source_stream_encoding
        ?GpuReflectionSourceOptical::stream_working_bytes:0);
    return bytes<=1024ULL*1024*1024?std::optional{bytes}:std::nullopt;
}
bool GpuCalibratedReflectionHistory::can_allocate(std::uint32_t w,std::uint32_t h,std::uint32_t stride,bool source_index,bool admission_diagnostics) const noexcept {
    const auto bytes=allocation_bytes(w,h,stride,source_index,admission_diagnostics);
    return bytes && working_bound_fits(*bytes,0,false);
}
std::uint64_t GpuCalibratedReflectionHistory::working_image_bytes() const noexcept {
    return state_?(state_->accepted_images?state_->accepted_images->bytes():0)
        +(state_->pending_images?state_->pending_images->bytes():0)
        +(state_->query_work?GpuReflectionSourceQueries::working_bytes:0)
        +(state_->optical_work?GpuReflectionSourceOptical::working_bytes:0)
        +(state_->optical_stream_work?GpuReflectionSourceOptical::stream_working_bytes:0)
        +(state_->local_root_work?GpuReflectionSourceLocalRoots::working_bytes:0):0;
}
std::optional<std::uint64_t> GpuCalibratedReflectionHistory::allocation_bytes(std::uint32_t w,std::uint32_t h,std::uint32_t stride,bool source_index,bool admission_diagnostics) const noexcept {
    if(!state_ || !w || !h || w>16384 || h>16384 || (admission_diagnostics && !source_index)
        || (stride!=40 && stride!=52 && stride!=80 && stride!=88 && stride!=92 && stride!=96 && stride!=104
            && stride!=108 && stride!=124 && stride!=128 && stride!=444 && stride!=460 && stride!=540
            && stride!=556 && stride!=572 && stride!=576)) return {};
    const auto storage=std::uint64_t(w)*h*stride;
    if(storage>UINT32_MAX) return {};
    const auto index_layout=source_index?reflection_source_index_layout(w,h,stride):std::nullopt;
    if(source_index && !index_layout) return {};
    const auto* old=state_->accepted_images.get();
    const bool same=old && old->width==w && old->height==h && old->storage_bytes==storage && old->source_layout==index_layout
        && old->admission_diagnostics==admission_diagnostics;
    return (storage+(index_layout?index_layout->storage_bytes:0))*2+(old && !same?old->bytes():0)
        +(same && old->publication_buffer?storage:0)
        +(admission_diagnostics?std::uint64_t(w)*h*8:0)
        +(same && old->publication_mask?std::uint64_t(w)*h*4:0)
        +(state_->query_work?GpuReflectionSourceQueries::working_bytes:0)
        +(state_->optical_work?GpuReflectionSourceOptical::working_bytes:0)
        +(state_->optical_stream_work?GpuReflectionSourceOptical::stream_working_bytes:0)
        +(state_->local_root_work?GpuReflectionSourceLocalRoots::working_bytes:0);
}
bool GpuCalibratedReflectionHistory::initialize(void* device) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native reflection history requires a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        const bool spirv=(SDL_GetGPUShaderFormats(s->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="reflection_history_main";
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.num_samplers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
        info.num_readonly_storage_buffers=2;info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        info.code=calibrated_scene_shader::reflection_history_spirv;info.code_size=sizeof(calibrated_scene_shader::reflection_history_spirv);
#if defined(_WIN32)
        if(!spirv) {info.code=calibrated_scene_shader::reflection_history_dxil;info.code_size=sizeof(calibrated_scene_shader::reflection_history_dxil);}
#endif
        s->pipeline=create_gpu_compute_pipeline(s->device,&info);require(s->pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());
        s->status="Native reflected-radiance history ready";state_=std::move(s);return true;
    }catch(const std::exception& e) {const std::string error=e.what();s.reset();state_=std::make_unique<State>();state_->status=error;return false;}
}
bool GpuCalibratedReflectionHistory::enqueue(void* command,const shadows::GpuReflectionOutput& rays,
    void* ownership,const CalibratedReflectionHistorySettings& settings,const CalibratedReflectionLobeGeometry* lobes) {
    if(!state_) return false;
    try {
        auto& s=*state_;require(s.pipeline && command && !s.pending,"Invalid/pending native reflection history command");
        const bool separated=rays.reflection_history.incoming_offset!=0;
        const unsigned model_lobes=rays.reflection_history.model_lobes;
        const bool model_paths=rays.reflection_history.model_paths;
        const bool curved_paths=rays.reflection_history.curved_paths,curved_receivers=rays.reflection_history.curved_receivers;
        require(!curved_paths || settings.curved_validation,
            "Ordered curved paths require completed consumer/owner/performance qualification; retaining current full-quality radiance");
        const bool scene_paths=rays.reflection_history.scene_paths;
        const auto layout=shadows::native_reflection_history(rays.width,rays.height,rays.reflection_history.extent,separated,
            rays.water_layers,model_lobes,model_paths,scene_paths,curved_paths,curved_receivers);
        require(layout && rays.device==s.device && rays.buffer && ownership && rays.row_bytes==rays.width*4
            && rays.reflection_history==*layout,
            "Invalid native reflected-radiance device/layout/extent");
        require(model_lobes?lobes && lobes->valid() && lobes->history.model_lobes==model_lobes
            && lobes->history.model_paths==model_paths
            && lobes->history.scene_paths==scene_paths
            && lobes->history.curved_paths==curved_paths && lobes->history.curved_receivers==curved_receivers
            && lobes->history.extent==layout->extent && lobes->buffer!=rays.buffer:!lobes,
            "Invalid compact reflected-incident geometry/calibration");
        const unsigned prefix_stride=layout->motion_offset/(rays.width*rays.height);
        const auto index_layout=settings.source_index_validation?
            reflection_source_index_layout(rays.width,rays.height,layout->storage_bytes/(rays.width*rays.height)):std::nullopt;
        require(!settings.source_admission_diagnostics || settings.source_index_validation,
            "Admission diagnostics require explicit source-index staging");
        require(!settings.source_index_validation || (model_paths && index_layout
            && index_layout->lobes==model_lobes
            && index_layout->primary_prefix==layout->identity_offset/(rays.width*rays.height)
            && index_layout->record_prefix==layout->incoming_offset/(rays.width*rays.height)),
            "Resident reflection index requires an exact ordered native path layout");
        require(std::isfinite(settings.weight) && settings.weight>=0 && settings.weight<=.95F,"Invalid reflection history weight");
        if(s.accepted_images) for(auto* b:s.accepted_images->buffers)
            require(rays.buffer!=b && (!lobes || lobes->buffer!=b),"Aliased native reflection history source");
        if(s.accepted_images) for(auto* b:s.accepted_images->source_indices) if(b)
            require(rays.buffer!=b && (!lobes || lobes->buffer!=b),"Aliased resident reflection source index");
        if(s.accepted_images && s.accepted_images->publication_buffer)
            require(rays.buffer!=s.accepted_images->publication_buffer && (!lobes || lobes->buffer!=s.accepted_images->publication_buffer),
                "Aliased separate reflection publication image");
        if(s.accepted_images) {
            for(auto* b:s.accepted_images->admission_masks)if(b)
                require(rays.buffer!=b && (!lobes || lobes->buffer!=b),"Aliased diagnostic admission bank");
            const auto* b=s.accepted_images->publication_mask;
            if(b)require(rays.buffer!=b && (!lobes || lobes->buffer!=b),"Aliased diagnostic publication mask");
        }
        const bool same=s.accepted_images && s.accepted_images->width==rays.width && s.accepted_images->height==rays.height
            && s.accepted_images->storage_bytes==layout->storage_bytes && s.accepted_images->separated==separated
            && s.accepted_images->prefix_stride==prefix_stride && s.accepted_images->model_lobes==model_lobes
            && s.accepted_images->source_layout==index_layout
            && s.accepted_images->admission_diagnostics==settings.source_admission_diagnostics;
        const auto bytes=(std::uint64_t(layout->storage_bytes)+(index_layout?index_layout->storage_bytes:0))*2
            +(same && s.accepted_images->publication_buffer?layout->storage_bytes:0)
            +(settings.source_admission_diagnostics?std::uint64_t(rays.width)*rays.height*8:0)
            +(same && s.accepted_images->publication_mask?std::uint64_t(rays.width)*rays.height*4:0)
            +(s.query_work?GpuReflectionSourceQueries::working_bytes:0)
            +(s.optical_work?GpuReflectionSourceOptical::working_bytes:0)
            +(s.optical_stream_work?GpuReflectionSourceOptical::stream_working_bytes:0)
            +(s.local_root_work?GpuReflectionSourceLocalRoots::working_bytes:0);
        require(working_bound_fits(bytes,s.accepted_images?s.accepted_images->bytes():0,same),
            "Native reflection history exceeds the one-GiB working bound (not a free-VRAM guarantee)");
        auto next=same?std::unique_ptr<State::Images>{}:s.make_images(rays.width,rays.height,layout->storage_bytes,separated,prefix_stride,model_lobes,index_layout,settings.source_admission_diagnostics);
        auto& images=*(next?next:s.accepted_images);
        const bool reuse=s.valid && s.accepted_images && s.accepted.epoch==settings.epoch && s.accepted.srgb==settings.srgb
            && s.accepted_images->separated==separated && s.accepted_images->prefix_stride==prefix_stride
            && s.accepted_images->model_lobes==model_lobes
            && s.accepted_output.reflection_history.model_paths==model_paths
            && s.accepted_output.reflection_history.scene_paths==scene_paths
            && s.accepted_output.reflection_history.curved_paths==curved_paths
            && s.accepted_output.reflection_history.curved_receivers==curved_receivers
            && (!model_paths || s.accepted_cube==lobes->previous_cube)
            && (!scene_paths || s.accepted_ground==lobes->history.previous_ground)
            && (!curved_paths || s.accepted_liquid==lobes->history.previous_liquid)
            && (!curved_paths || (s.accepted_liquid_projection==lobes->history.projection
                && s.accepted_liquid_clip==std::array{lobes->history.near_plane,lobes->history.far_plane}))
            && rays.reflection_history.extent==std::array{s.accepted_images->width,s.accepted_images->height};
        const unsigned write=same?1-s.index:0;
        // A mandatory but unused old-image binding must still be DISTINCT
        // from current/output. SDL's D3D12 compute-buffer barriers are per
        // binding slot; binding the current SRV twice transitions it twice.
        // The spare ping-pong bank is bounded/alive and never read without
        // the reuse flag, so no extra allocation, copy or wait is needed.
        auto* previous=reuse?s.accepted_images->buffers[s.index]:images.buffers[1-write];
        bool capture_only=false;
#if defined(STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE)
        // Every indexed pending bank must remain freshly traced CURRENT data.
        // Otherwise a later CURRENT-neighbour clamp could mistake previously
        // blended incident RGB for a fresh observation. Normal, nonstaging
        // planar history keeps its existing consumer; this grants no RGB reuse.
        capture_only=index_layout.has_value() || (curved_paths && (!reuse || settings.weight==0));
#endif
        if(capture_only)s.prepare_capture();
        else if(model_paths) s.prepare_paths(curved_paths);else if(model_lobes) s.prepare_lobes();
        if(index_layout) s.prepare_source_index();
        SDL_GPUStorageBufferReadWriteBinding output{images.buffers[write],false,0,0,0};
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,&output,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,capture_only?s.capture_pipeline:curved_paths?s.curved_pipeline:model_paths?s.path_pipeline:model_lobes?s.lobe_pipeline:s.pipeline);
        const SDL_GPUTextureSamplerBinding owner{static_cast<SDL_GPUTexture*>(ownership),s.sampler};SDL_BindGPUComputeSamplers(pass,0,&owner,1);
        SDL_GPUBuffer* buffers[]{static_cast<SDL_GPUBuffer*>(rays.buffer),previous,lobes?static_cast<SDL_GPUBuffer*>(lobes->buffer):nullptr};
        SDL_BindGPUComputeStorageBuffers(pass,0,buffers,capture_only?1:model_lobes?3:2);
        const Uniform uniform{rays.width,rays.height,reuse?s.accepted_images->width:rays.width,
            reuse?s.accepted_images->height:rays.height,(reuse?1U:0U)|(settings.srgb?2U:0U)|(separated?4U:0U),settings.weight,prefix_stride,0};
        if(capture_only) {
            const unsigned record_prefix=layout->incoming_offset/(rays.width*rays.height);
            const std::array<unsigned,12> capture{rays.width,rays.height,model_lobes,prefix_stride,record_prefix,
                lobes->vertex_count/3,unsigned(curved_receivers),curved_receivers?lobes->history.previous_liquid->material:0U,
                curved_paths?64U:52U,unsigned(scene_paths),0,0};
            SDL_PushGPUComputeUniformData(cb,0,capture.data(),sizeof(capture));
        } else if(model_lobes) {
            LobeUniform u{rays.width,rays.height,uniform.previous_width,uniform.previous_height,model_lobes,uniform.flags,
                lobes->history.previous_vertex_offset,lobes->history.previous_index_offset,lobes->vertex_count/3,
                lobes->bytes,settings.weight,lobes->roughness,{},
                {float(lobes->history.near_plane),float(lobes->history.far_plane),0,0}};
            std::transform(lobes->history.projection.begin(),lobes->history.projection.end(),u.projection.begin(),[](double v){return float(v);});
            if(model_paths) {
                PathUniform path{u,reuse?s.accepted_triangles:0,unsigned(scene_paths),prefix_stride,unsigned(curved_receivers)};
                for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) {
                    path.current_cube[r][c]=lobes->current_cube[r*3+c];
                    path.previous_cube[r][c]=lobes->previous_cube[r*3+c];
                }
                if(scene_paths) {
                    const auto store=[](const shadows::RayReflectionGround& ground,std::array<float,4>& point,std::array<float,4>& normal) {
                        for(unsigned c=0;c<3;++c) {point[c]=float(ground.point[c]);normal[c]=float(ground.normal[c]);}
                        point[3]=1;
                    };
                    store(*lobes->current_ground,path.current_point,path.current_normal);
                    if(lobes->history.previous_ground) store(*lobes->history.previous_ground,path.previous_point,path.previous_normal);
                }
                if(curved_paths) {
                    CurvedPathUniform curved{path,shadows::reflection_liquid_frame_words(lobes->history),{}, {}};
                    for(unsigned r=0;r<3;++r) {
                        for(unsigned c=0;c<3;++c) curved.current_liquid_rotation[r][c]=float(lobes->current_liquid->world_to_view[r*3+c]);
                        curved.current_liquid_rotation[r][3]=float(lobes->current_liquid->offset[r]);
                    }
                    for(unsigned c=0;c<4;++c) curved.current_projection[c]=float(lobes->current_projection[c]);
                    SDL_PushGPUComputeUniformData(cb,0,&curved,sizeof(curved));
                } else SDL_PushGPUComputeUniformData(cb,0,&path,sizeof(path));
            } else SDL_PushGPUComputeUniformData(cb,0,&u,sizeof(u));
        } else SDL_PushGPUComputeUniformData(cb,0,&uniform,sizeof(uniform));
        SDL_DispatchGPUCompute(pass,(rays.width+7)/8,(rays.height+7)/8,1);SDL_EndGPUComputePass(pass);
        if(index_layout) s.encode_source_index(cb,images,write);
        if(images.admission_diagnostics)s.clear_admission(cb,images.admission_masks[write],rays.width*rays.height);
        s.pending_images=std::move(next);s.pending_index=write;s.pending_settings=settings;s.pending=true;s.pending_reset=false;
        s.publication_next=0;s.publication_started=s.publication_complete=false;
        s.pending_command=command;s.pending_sources_compatible=reuse;s.pending_guide=lobes?*lobes:CalibratedReflectionLobeGeometry{};s.queries={};s.optical={};s.local_roots={};s.witness={};s.folds={};s.source_guide={};s.source_colour={};s.source_composition={};
        s.pending_roughness=lobes?lobes->roughness:0;
        s.pending_triangles=lobes?lobes->vertex_count/3:0;s.pending_cube=lobes?lobes->current_cube:std::array<float,9>{};
        s.pending_ground=lobes?lobes->current_ground:std::nullopt;
        s.pending_liquid=lobes?lobes->current_liquid:std::nullopt;
        s.pending_liquid_projection=lobes?lobes->current_projection:std::array<double,4>{};
        s.pending_liquid_clip=lobes?lobes->current_clip:std::array<double,2>{};
        s.candidate=rays;s.candidate.buffer=images.buffers[write];s.status="Reflected radiance encoded; awaiting whole presentation acceptance";return true;
    }catch(const std::exception& e) {state_->status=e.what();return false;}
}
}
