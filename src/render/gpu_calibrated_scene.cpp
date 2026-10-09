#include "starfox/render/gpu_calibrated_scene.hpp"
#include "starfox/render/calibrated_effects.hpp"
#include "starfox/render/calibrated_environment.hpp"
#include "starfox/render/calibrated_ground.hpp"
#include "starfox/render/calibrated_motion.hpp"
#include "starfox/vr/scene_packet_validation.hpp"
#include "starfox/render/sdl_multisample.h"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <unordered_map>
namespace starfox::render {
namespace {
// Raw compute loads must agree with the exact native graphics vertex ABI.
static_assert(sizeof(vr::SceneVertex)==160 && offsetof(vr::SceneVertex,position)==0
    && offsetof(vr::SceneVertex,color)==12 && offsetof(vr::SceneVertex,odd_color)==28
    && offsetof(vr::SceneVertex,dither_scale)==44 && offsetof(vr::SceneVertex,uv)==128
    && offsetof(vr::SceneVertex,visibility_a)==48 && offsetof(vr::SceneVertex,visibility_b)==60
    && offsetof(vr::SceneVertex,visibility_c)==72 && offsetof(vr::SceneVertex,visibility_enabled)==84
    && offsetof(vr::SceneVertex,group_a)==88 && offsetof(vr::SceneVertex,group_b)==100
    && offsetof(vr::SceneVertex,group_c)==112 && offsetof(vr::SceneVertex,group_enabled)==124
    && offsetof(vr::SceneVertex,texture)==136 && offsetof(vr::SceneVertex,billboard)==152);
void require(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
void ensure(bool ok,const char* error) {if(!ok) throw std::runtime_error(error);}
bool valid_camera(const vr::EyeCamera& camera) {
    for(const auto* m:{&camera.view,&camera.projection}) for(float value:*m)
        if(!std::isfinite(value)) return false;
    return camera.view[3]==0 && camera.view[7]==0 && camera.view[11]==0 && camera.view[15]==1;
}
struct MotionUniform {
    vr::Matrix4 mapping;
    std::array<float,4> projection;
    float width,height;unsigned valid,padding;
};
static_assert(sizeof(MotionUniform)==96);
struct PreviousGeometryUniform {vr::Matrix4 view;unsigned count,padding[3];};
static_assert(sizeof(PreviousGeometryUniform)==80);
void validate_previous_geometry(std::span<const vr::SceneVertex> vertices) {
    constexpr unsigned supported=1U|2U|4U|4096U|32768U|134217728U|536870912U|0x80000000U;
    for(const auto& v:vertices) {
        ensure(!(v.texture[3]&~supported),"Previous calibrated primitive requires an unsupported source transform");
        for(const std::span<const float> row:{std::span<const float>(v.position),std::span<const float>(v.visibility_a),
            std::span<const float>(v.visibility_b),std::span<const float>(v.visibility_c),std::span<const float>(v.group_a),
            std::span<const float>(v.group_b),std::span<const float>(v.group_c),std::span<const float>(v.billboard)})
            for(float value:row) ensure(std::isfinite(value) && std::abs(value)<=1.e8F,"Invalid previous calibrated vertex payload");
        ensure(v.visibility_enabled<=2 && v.group_enabled<=1,"Invalid previous calibrated visibility");
        if(v.visibility_enabled==2) ensure(v.group_c[1]>0 && v.group_c[0]>=0 && v.group_c[0]<=255,
            "Invalid previous calibrated destruction payload");
    }
}
SDL_GPUSampleCount native_sample_count(unsigned samples) {
    switch(samples) {
    case 1:return SDL_GPU_SAMPLECOUNT_1;
    case 2:return SDL_GPU_SAMPLECOUNT_2;
    case 4:return SDL_GPU_SAMPLECOUNT_4;
    case 8:return SDL_GPU_SAMPLECOUNT_8;
    default:throw std::runtime_error("Native MSAA requires exactly 1, 2, 4 or 8 samples");
    }
}
void validate(const CalibratedSceneDraw& draw) {
    ensure(unsigned(draw.topology)<=unsigned(vr::SceneTopology::lines)
        && unsigned(draw.blend)<=unsigned(vr::SceneBlend::alpha),"Invalid calibrated scene draw mode");
    const unsigned primitive=draw.topology==vr::SceneTopology::lines?2:3;
    ensure(draw.vertices.size()%primitive==0,"Incomplete calibrated scene primitive");
    const vr::EyeCamera identity{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},{}};
    ensure(bool(vr::model_eye_camera(identity,draw.model)),"Invalid calibrated model matrix");
    constexpr unsigned supported=1|2|4|4096|32768|134217728U|536870912U;
    for(const auto& v:draw.vertices) {
        ensure(!(v.texture[3]&~supported),"Specialized calibrated scene payload not integrated");
        for(const auto row:{std::span(v.position),std::span(v.visibility_a),std::span(v.visibility_b),
            std::span(v.visibility_c),std::span(v.group_a),std::span(v.group_b),std::span(v.group_c)})
            for(float value:row) ensure(std::isfinite(value) && std::abs(value)<=1.e8F,"Invalid calibrated vertex position");
        for(const auto row:{std::span(v.color),std::span(v.odd_color)})
            for(float value:row) ensure(std::isfinite(value) && value>=0 && value<=1,"Invalid calibrated vertex color");
        for(const auto row:{std::span(v.uv),std::span(v.billboard)})
            for(float value:row) ensure(std::isfinite(value) && std::abs(value)<=65536,"Invalid calibrated texture coordinate");
        ensure(v.visibility_enabled<=2 && v.group_enabled<=1 && v.dither_scale<=4096,"Invalid calibrated source visibility");
        if(v.visibility_enabled==2) ensure(v.group_c[1]>0 && v.group_c[0]>=0 && v.group_c[0]<=255,
            "Invalid calibrated destruction payload");
        if(v.texture[3]&1) {
            const std::uint64_t w=std::uint64_t(v.texture[1])+1,h=std::uint64_t(v.texture[2])+1;
            ensure(w<=4096 && h<=4096 && !(w&(w-1)) && !(h&(h-1)),"Invalid calibrated texture dimensions");
            const auto words=(v.texture[3]&536870912U)?256+(w*h+3)/4:w*h;
            ensure(std::uint64_t(v.texture[0])+words<=draw.texels.size(),"Incomplete calibrated texture payload");
        }
    }
}
}
struct GpuCalibratedScene::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUShader *vertex{},*fragment{},*receiver_fragment{},*surface_fragment{},*motion_fragment{},*motion_vertex{},
        *msaa_fragment{},*msaa_receiver_fragment{},*msaa_surface_fragment{},*msaa_guide_vertex{},*msaa_motion_vertex{},*msaa_motion_fragment{};
    SDL_GPUComputePipeline* connected[2]{};
    SDL_GPUComputePipeline* ray_pipeline{};
    SDL_GPUComputePipeline* environment_pipeline{};
    SDL_GPUTexture *environment_colour{},*environment_depth{};
    SDL_GPUSampler* environment_sampler{};unsigned environment_size{};
    SDL_GPUBuffer* rays[2]{};unsigned ray_capacity[2]{};
    RayMaterials ray_materials{{},{},RayMaterialEncoding::native_rgba}; // No CPU colour/vertex packing.
    SDL_GPUGraphicsPipeline* pipelines[2][2][7][2][4][4]{};
    SDL_GPUShader* ground_fragment[2][4]{}; // Lazy single/MSAA x colour/receiver/surface/motion.
    struct Item {
        SDL_GPUBuffer *vertices{},*texels{},*grid_output{};SDL_GPUTransferBuffer* upload{};
        SDL_GPUBuffer* bound_texels{};
        unsigned vertex_bytes{},texel_bytes{},upload_bytes{},count{};
        vr::Matrix4 model{};vr::SceneTopology topology{};vr::SceneBlend blend{};bool depth{};
        std::optional<vr::EyeCamera> camera_override;bool preserve_native_colour{};
        std::optional<std::array<unsigned,4>> effects_override;
        bool ray_caster{},ray_supported{},after_rays{},reflection_environment{},reflective_material{};
        unsigned effect_layer{};bool ground_surface{},ground_receiver{},transport_ground{};
        unsigned ray_texture_bytes{};
        SDL_GPUBuffer* previous_vertices{};SDL_GPUTransferBuffer* previous_upload{};
        unsigned previous_capacity{};std::uint64_t previous_encoded{};
        std::vector<vr::SceneVertex> previous_source;
    };
    struct ImmutableTexture {
        SDL_GPUDevice* device;std::shared_ptr<const std::vector<std::uint32_t>> source;
        SDL_GPUBuffer* buffer{};SDL_GPUTransferBuffer* upload{};
        std::uint64_t used{},encoded{};bool ready{};
        ~ImmutableTexture() {
            if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        }
    };
    std::unordered_map<const std::vector<std::uint32_t>*,std::unique_ptr<ImmutableTexture>> immutable;
    std::uint64_t generation{},token{},submitted_token{};std::array<std::uint64_t,3> cost{};
    std::vector<Item> items;std::size_t used{};void* command{};
    std::string status{"Calibrated scene not initialized"};
    ~State() {release();}
    void release() noexcept {
        if(device) {
            for(auto& item:items) {
                if(item.vertices) SDL_ReleaseGPUBuffer(device,item.vertices);
                if(item.texels) SDL_ReleaseGPUBuffer(device,item.texels);
                if(item.grid_output) SDL_ReleaseGPUBuffer(device,item.grid_output);
                if(item.upload) SDL_ReleaseGPUTransferBuffer(device,item.upload);
                if(item.previous_vertices) SDL_ReleaseGPUBuffer(device,item.previous_vertices);
                if(item.previous_upload) SDL_ReleaseGPUTransferBuffer(device,item.previous_upload);
            }
            for(auto& variant:pipelines) for(auto& topology:variant) for(auto& blend:topology) for(auto& depth:blend) for(auto& targets:depth) for(auto& p:targets) {
                if(p) SDL_ReleaseGPUGraphicsPipeline(device,p);p=nullptr;
            }
            for(auto& variant:ground_fragment) for(auto*& shader:variant) {
                if(shader) SDL_ReleaseGPUShader(device,shader);shader=nullptr;
            }
            for(auto*& p:connected) {if(p) SDL_ReleaseGPUComputePipeline(device,p);p=nullptr;}
            if(ray_pipeline) SDL_ReleaseGPUComputePipeline(device,ray_pipeline);
            if(environment_pipeline) SDL_ReleaseGPUComputePipeline(device,environment_pipeline);
            if(environment_colour) SDL_ReleaseGPUTexture(device,environment_colour);
            if(environment_depth) SDL_ReleaseGPUTexture(device,environment_depth);
            if(environment_sampler) SDL_ReleaseGPUSampler(device,environment_sampler);
            for(auto*& b:rays) {if(b) SDL_ReleaseGPUBuffer(device,b);b=nullptr;}
            if(vertex) SDL_ReleaseGPUShader(device,vertex);
            if(fragment) SDL_ReleaseGPUShader(device,fragment);
            if(receiver_fragment) SDL_ReleaseGPUShader(device,receiver_fragment);
            if(surface_fragment) SDL_ReleaseGPUShader(device,surface_fragment);
            if(motion_fragment) SDL_ReleaseGPUShader(device,motion_fragment);
            if(motion_vertex) SDL_ReleaseGPUShader(device,motion_vertex);
            if(msaa_motion_vertex) SDL_ReleaseGPUShader(device,msaa_motion_vertex);
            if(msaa_motion_fragment) SDL_ReleaseGPUShader(device,msaa_motion_fragment);
            if(msaa_fragment) SDL_ReleaseGPUShader(device,msaa_fragment);
            if(msaa_receiver_fragment) SDL_ReleaseGPUShader(device,msaa_receiver_fragment);
            if(msaa_surface_fragment) SDL_ReleaseGPUShader(device,msaa_surface_fragment);
            if(msaa_guide_vertex) SDL_ReleaseGPUShader(device,msaa_guide_vertex);
        }
        immutable.clear();
        vertex=fragment=receiver_fragment=surface_fragment=motion_fragment=msaa_fragment=msaa_receiver_fragment=msaa_surface_fragment=nullptr;
        msaa_guide_vertex=nullptr;
        motion_vertex=nullptr;
        msaa_motion_vertex=msaa_motion_fragment=nullptr;
        device=nullptr;items.clear();used=0;command=nullptr;token=submitted_token=0;cost={};
        ray_pipeline=nullptr;ray_capacity[0]=ray_capacity[1]=0;
        environment_pipeline=nullptr;environment_colour=environment_depth=nullptr;
        environment_sampler=nullptr;environment_size=0;
    }
    void previous_geometry(SDL_GPUCommandBuffer* cmd,Item& item,std::span<const vr::SceneVertex> source,bool continued) {
        const bool same=item.previous_encoded==token && item.previous_source.size()==source.size()
            && std::equal(source.begin(),source.end(),item.previous_source.begin());
        if(same) return;
        ensure(!continued,"Previous geometry must be encoded with the original source command");
        const unsigned bytes=unsigned(source.size_bytes());
        if(bytes>item.previous_capacity) {
            const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,bytes,0};
            auto* buffer=SDL_CreateGPUBuffer(device,&info);require(buffer);
            const SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0};
            auto* upload=SDL_CreateGPUTransferBuffer(device,&transfer);
            if(!upload) {SDL_ReleaseGPUBuffer(device,buffer);require(false);}
            if(item.previous_vertices) SDL_ReleaseGPUBuffer(device,item.previous_vertices);
            if(item.previous_upload) SDL_ReleaseGPUTransferBuffer(device,item.previous_upload);
            item.previous_vertices=buffer;item.previous_upload=upload;item.previous_capacity=bytes;
        }
        auto* mapped=SDL_MapGPUTransferBuffer(device,item.previous_upload,true);require(mapped);
        std::memcpy(mapped,source.data(),bytes);SDL_UnmapGPUTransferBuffer(device,item.previous_upload);
        auto* pass=SDL_BeginGPUCopyPass(cmd);require(pass);
        const SDL_GPUTransferBufferLocation from{item.previous_upload,0};const SDL_GPUBufferRegion to{item.previous_vertices,0,bytes};
        SDL_UploadToGPUBuffer(pass,&from,&to,true);SDL_EndGPUCopyPass(pass);
        item.previous_source.assign(source.begin(),source.end());item.previous_encoded=token;cost[0]+=bytes;
    }
    void prepare_environment(unsigned size) {
        if(!environment_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.entrypoint="environment_pack_main";
            info.code=calibrated_scene_shader::environment_pack_spirv;
            info.code_size=sizeof(calibrated_scene_shader::environment_pack_spirv);
#if defined(_WIN32)
            if(!spirv) {info.code=calibrated_scene_shader::environment_pack_dxil;
                info.code_size=sizeof(calibrated_scene_shader::environment_pack_dxil);}
#endif
            info.num_samplers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
            info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
            environment_pipeline=create_gpu_compute_pipeline(device,&info);require(environment_pipeline);
        }
        if(!environment_sampler) {
            SDL_GPUSamplerCreateInfo info{};info.min_filter=info.mag_filter=SDL_GPU_FILTER_NEAREST;
            info.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            info.address_mode_u=info.address_mode_v=info.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            environment_sampler=SDL_CreateGPUSampler(device,&info);require(environment_sampler);
        }
        if(environment_size==size) return;
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=format;
        info.width=info.height=size;info.layer_count_or_depth=info.num_levels=1;
        info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        auto* colour=SDL_CreateGPUTexture(device,&info);require(colour);
        info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        auto* depth=SDL_CreateGPUTexture(device,&info);
        if(!depth) {SDL_ReleaseGPUTexture(device,colour);require(false);}
        if(environment_colour) SDL_ReleaseGPUTexture(device,environment_colour);
        if(environment_depth) SDL_ReleaseGPUTexture(device,environment_depth);
        environment_colour=colour;environment_depth=depth;environment_size=size;
    }
    SDL_GPUComputePipeline* connected_pipeline(unsigned stage) {
        auto*& result=connected[stage];if(result) return result;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.entrypoint=stage?"connected_rows_main":"connected_project_main";
        info.code=stage?calibrated_scene_shader::connected_rows_spirv:calibrated_scene_shader::connected_project_spirv;
        info.code_size=stage?sizeof(calibrated_scene_shader::connected_rows_spirv):sizeof(calibrated_scene_shader::connected_project_spirv);
#if defined(_WIN32)
        if(!spirv) {
            info.code=stage?calibrated_scene_shader::connected_rows_dxil:calibrated_scene_shader::connected_project_dxil;
            info.code_size=stage?sizeof(calibrated_scene_shader::connected_rows_dxil):sizeof(calibrated_scene_shader::connected_project_dxil);
        }
#endif
        info.num_readonly_storage_buffers=info.num_readwrite_storage_buffers=1;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        result=create_gpu_compute_pipeline(device,&info);require(result);return result;
    }
    SDL_GPUGraphicsPipeline* pipeline(const Item& item,bool receiver=false,bool surfaces=false,unsigned samples=1,bool motion=false) {
        const auto sample_count=native_sample_count(samples);
        auto& result=pipelines[item.ground_surface][unsigned(item.topology)][unsigned(item.blend)][item.depth][motion?3:surfaces?2:unsigned(receiver)][unsigned(sample_count)];
        if(result) return result;
        if(samples>1 && receiver && !motion) {
            if(!msaa_guide_vertex) {
                const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
                SDL_GPUShaderCreateInfo shader{};shader.num_uniform_buffers=1;shader.num_storage_buffers=1;
                shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
                shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="vertex_textured_main";
                shader.code=calibrated_scene_shader::vertex_msaa_guides_spirv;shader.code_size=sizeof(calibrated_scene_shader::vertex_msaa_guides_spirv);
#if defined(_WIN32)
                if(!spirv) {shader.code=calibrated_scene_shader::vertex_msaa_guides_dxil;shader.code_size=sizeof(calibrated_scene_shader::vertex_msaa_guides_dxil);}
#endif
                msaa_guide_vertex=create_gpu_shader(device,&shader);require(msaa_guide_vertex);
            }
            auto& shader_handle=surfaces?msaa_surface_fragment:msaa_receiver_fragment;
            if(!shader_handle && !item.ground_surface) {
                const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
                SDL_GPUShaderCreateInfo shader{};shader.num_uniform_buffers=surfaces?2:1;shader.num_storage_buffers=1;
                shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
                shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;
                shader.entrypoint=surfaces?"fragment_surface_msaa_main":"fragment_receiver_msaa_main";
                shader.code=surfaces?calibrated_scene_shader::surface_msaa_spirv:calibrated_scene_shader::receiver_msaa_spirv;
                shader.code_size=surfaces?sizeof(calibrated_scene_shader::surface_msaa_spirv):sizeof(calibrated_scene_shader::receiver_msaa_spirv);
#if defined(_WIN32)
                if(!spirv) {
                    shader.code=surfaces?calibrated_scene_shader::surface_msaa_dxil:calibrated_scene_shader::receiver_msaa_dxil;
                    shader.code_size=surfaces?sizeof(calibrated_scene_shader::surface_msaa_dxil):sizeof(calibrated_scene_shader::receiver_msaa_dxil);
                }
#endif
                shader_handle=create_gpu_shader(device,&shader);require(shader_handle);
            }
        }
        if(samples>1 && !receiver && !msaa_fragment && !item.ground_surface) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUShaderCreateInfo shader{};shader.num_uniform_buffers=shader.num_storage_buffers=1;
            shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="fragment_textured_main";
            shader.code=calibrated_scene_shader::fragment_msaa_spirv;shader.code_size=sizeof(calibrated_scene_shader::fragment_msaa_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_scene_shader::fragment_msaa_dxil;shader.code_size=sizeof(calibrated_scene_shader::fragment_msaa_dxil);}
#endif
            msaa_fragment=create_gpu_shader(device,&shader);require(msaa_fragment);
        }
        if(samples==1 && receiver && !surfaces && !receiver_fragment && !item.ground_surface) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUShaderCreateInfo shader{};shader.num_uniform_buffers=shader.num_storage_buffers=1;
            shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="fragment_receiver_main";
            shader.code=calibrated_scene_shader::receiver_spirv;shader.code_size=sizeof(calibrated_scene_shader::receiver_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_scene_shader::receiver_dxil;shader.code_size=sizeof(calibrated_scene_shader::receiver_dxil);}
#endif
            receiver_fragment=create_gpu_shader(device,&shader);require(receiver_fragment);
        }
        if(samples==1 && surfaces && !motion && !surface_fragment && !item.ground_surface) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUShaderCreateInfo shader{};shader.num_uniform_buffers=2;shader.num_storage_buffers=1;
            shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="fragment_surface_main";
            shader.code=calibrated_scene_shader::surface_spirv;shader.code_size=sizeof(calibrated_scene_shader::surface_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_scene_shader::surface_dxil;shader.code_size=sizeof(calibrated_scene_shader::surface_dxil);}
#endif
            surface_fragment=create_gpu_shader(device,&shader);require(surface_fragment);
        }
        auto& motion_vs=samples>1?msaa_motion_vertex:motion_vertex;
        auto& motion_fs=samples>1?msaa_motion_fragment:motion_fragment;
        if(motion && !motion_vs) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUShaderCreateInfo shader{};shader.num_uniform_buffers=2;shader.num_storage_buffers=2;
            shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="vertex_motion_main";
            shader.code=samples>1?calibrated_scene_shader::vertex_motion_msaa_spirv:calibrated_scene_shader::vertex_motion_spirv;
            shader.code_size=samples>1?sizeof(calibrated_scene_shader::vertex_motion_msaa_spirv):sizeof(calibrated_scene_shader::vertex_motion_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=samples>1?calibrated_scene_shader::vertex_motion_msaa_dxil:calibrated_scene_shader::vertex_motion_dxil;
                shader.code_size=samples>1?sizeof(calibrated_scene_shader::vertex_motion_msaa_dxil):sizeof(calibrated_scene_shader::vertex_motion_dxil);}
#endif
            motion_vs=create_gpu_shader(device,&shader);require(motion_vs);
        }
        if(motion && !motion_fs && !item.ground_surface) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUShaderCreateInfo shader{};shader.num_uniform_buffers=3;shader.num_storage_buffers=1;
            shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint=samples>1?"fragment_motion_msaa_main":"fragment_motion_main";
            shader.code=samples>1?calibrated_scene_shader::motion_msaa_spirv:calibrated_scene_shader::motion_spirv;
            shader.code_size=samples>1?sizeof(calibrated_scene_shader::motion_msaa_spirv):sizeof(calibrated_scene_shader::motion_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=samples>1?calibrated_scene_shader::motion_msaa_dxil:calibrated_scene_shader::motion_dxil;
                shader.code_size=samples>1?sizeof(calibrated_scene_shader::motion_msaa_dxil):sizeof(calibrated_scene_shader::motion_dxil);}
#endif
            motion_fs=create_gpu_shader(device,&shader);require(motion_fs);
        }
        SDL_GPUGraphicsPipelineCreateInfo info{};info.vertex_shader=motion?motion_vs:samples>1 && receiver?msaa_guide_vertex:vertex;
        info.fragment_shader=motion?motion_fs:samples>1?(surfaces?msaa_surface_fragment:receiver?msaa_receiver_fragment:msaa_fragment)
            :surfaces?surface_fragment:receiver?receiver_fragment:fragment;
        if(item.ground_surface) {
            const unsigned kind=motion?3:surfaces?2:receiver?1:0;
            auto*& handle=ground_fragment[samples>1][kind];
            if(!handle) {
                const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
                SDL_GPUShaderCreateInfo shader{};shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;
                shader.num_uniform_buffers=motion?3:surfaces?2:1;shader.num_storage_buffers=1;
                shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
                shader.entrypoint=motion?(samples>1?"fragment_motion_msaa_main":"fragment_motion_main"):
                    surfaces?(samples>1?"fragment_surface_msaa_main":"fragment_surface_main"):
                    receiver?(samples>1?"fragment_receiver_msaa_main":"fragment_receiver_main"):"fragment_textured_main";
                const unsigned char* codes[2][4]={{calibrated_scene_shader::ground_fragment_spirv,
                    calibrated_scene_shader::ground_receiver_spirv,calibrated_scene_shader::ground_surface_spirv,calibrated_scene_shader::ground_motion_spirv},
                    {calibrated_scene_shader::ground_fragment_msaa_spirv,calibrated_scene_shader::ground_receiver_msaa_spirv,
                    calibrated_scene_shader::ground_surface_msaa_spirv,calibrated_scene_shader::ground_motion_msaa_spirv}};
                const std::size_t sizes[2][4]={{sizeof(calibrated_scene_shader::ground_fragment_spirv),
                    sizeof(calibrated_scene_shader::ground_receiver_spirv),sizeof(calibrated_scene_shader::ground_surface_spirv),sizeof(calibrated_scene_shader::ground_motion_spirv)},
                    {sizeof(calibrated_scene_shader::ground_fragment_msaa_spirv),sizeof(calibrated_scene_shader::ground_receiver_msaa_spirv),
                    sizeof(calibrated_scene_shader::ground_surface_msaa_spirv),sizeof(calibrated_scene_shader::ground_motion_msaa_spirv)}};
                shader.code=codes[samples>1][kind];shader.code_size=sizes[samples>1][kind];
#if defined(_WIN32)
                if(!spirv) {
                    const unsigned char* native_codes[2][4]={{calibrated_scene_shader::ground_fragment_dxil,
                        calibrated_scene_shader::ground_receiver_dxil,calibrated_scene_shader::ground_surface_dxil,calibrated_scene_shader::ground_motion_dxil},
                        {calibrated_scene_shader::ground_fragment_msaa_dxil,calibrated_scene_shader::ground_receiver_msaa_dxil,
                        calibrated_scene_shader::ground_surface_msaa_dxil,calibrated_scene_shader::ground_motion_msaa_dxil}};
                    const std::size_t native_sizes[2][4]={{sizeof(calibrated_scene_shader::ground_fragment_dxil),
                        sizeof(calibrated_scene_shader::ground_receiver_dxil),sizeof(calibrated_scene_shader::ground_surface_dxil),sizeof(calibrated_scene_shader::ground_motion_dxil)},
                        {sizeof(calibrated_scene_shader::ground_fragment_msaa_dxil),sizeof(calibrated_scene_shader::ground_receiver_msaa_dxil),
                        sizeof(calibrated_scene_shader::ground_surface_msaa_dxil),sizeof(calibrated_scene_shader::ground_motion_msaa_dxil)}};
                    shader.code=native_codes[samples>1][kind];shader.code_size=native_sizes[samples>1][kind];
                }
#endif
                handle=create_gpu_shader(device,&shader);require(handle);
            }
            info.fragment_shader=handle;
        }
        SDL_GPUVertexBufferDescription binding{0,sizeof(vr::SceneVertex),SDL_GPU_VERTEXINPUTRATE_VERTEX,0};
        const SDL_GPUVertexAttribute attributes[]{
            {0,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(vr::SceneVertex,position)},
            {1,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,offsetof(vr::SceneVertex,color)},
            {2,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,offsetof(vr::SceneVertex,odd_color)},
            {3,0,SDL_GPU_VERTEXELEMENTFORMAT_UINT,offsetof(vr::SceneVertex,dither_scale)},
            {4,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(vr::SceneVertex,visibility_a)},
            {5,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(vr::SceneVertex,visibility_b)},
            {6,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(vr::SceneVertex,visibility_c)},
            {7,0,SDL_GPU_VERTEXELEMENTFORMAT_UINT,offsetof(vr::SceneVertex,visibility_enabled)},
            {8,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(vr::SceneVertex,group_a)},
            {9,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(vr::SceneVertex,group_b)},
            {10,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(vr::SceneVertex,group_c)},
            {11,0,SDL_GPU_VERTEXELEMENTFORMAT_UINT,offsetof(vr::SceneVertex,group_enabled)},
            {12,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(vr::SceneVertex,uv)},
            {13,0,SDL_GPU_VERTEXELEMENTFORMAT_UINT4,offsetof(vr::SceneVertex,texture)},
            {14,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(vr::SceneVertex,billboard)}};
        info.vertex_input_state={&binding,1,attributes,15};
        info.primitive_type=item.topology==vr::SceneTopology::lines?SDL_GPU_PRIMITIVETYPE_LINELIST:SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
        info.multisample_state.sample_count=sample_count;
        info.depth_stencil_state.enable_depth_test=info.depth_stencil_state.enable_depth_write=item.depth;
        info.depth_stencil_state.compare_op=SDL_GPU_COMPAREOP_LESS;
        SDL_GPUColorTargetDescription color{};color.format=format;
        auto& blend=color.blend_state;
        if(item.blend!=vr::SceneBlend::opaque) {
            blend.enable_blend=true;
            blend.src_color_blendfactor=SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            blend.dst_color_blendfactor=(item.blend==vr::SceneBlend::half_add || item.blend==vr::SceneBlend::half_subtract)
                ?SDL_GPU_BLENDFACTOR_SRC_ALPHA:SDL_GPU_BLENDFACTOR_ONE;
            blend.color_blend_op=(item.blend==vr::SceneBlend::subtract || item.blend==vr::SceneBlend::half_subtract)
                ?SDL_GPU_BLENDOP_REVERSE_SUBTRACT:SDL_GPU_BLENDOP_ADD;
            blend.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ZERO;blend.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
            blend.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
            if(item.blend==vr::SceneBlend::alpha) {
                blend.dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                blend.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
                blend.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            } else if(item.blend==vr::SceneBlend::shadow) {
                blend.src_color_blendfactor=SDL_GPU_BLENDFACTOR_ZERO;
                blend.dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            }
        }
        const SDL_GPUColorTargetDescription targets[]{color,{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,{}},
            {SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,{}},{SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,{}}};
        info.target_info={targets,motion?4U:surfaces?3U:receiver?2U:1U,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,true,0,0,0};
        result=create_gpu_graphics_pipeline(device,&info);require(result);return result;
    }
    SDL_GPUComputePipeline* ray_geometry_pipeline() {
        if(ray_pipeline) return ray_pipeline;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        info.entrypoint="ray_geometry_main";
        info.code=calibrated_scene_shader::ray_geometry_spirv;
        info.code_size=sizeof(calibrated_scene_shader::ray_geometry_spirv);
#if defined(_WIN32)
        if(!spirv) {info.code=calibrated_scene_shader::ray_geometry_dxil;info.code_size=sizeof(calibrated_scene_shader::ray_geometry_dxil);}
#endif
        info.num_readonly_storage_buffers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        ray_pipeline=create_gpu_compute_pipeline(device,&info);require(ray_pipeline);return ray_pipeline;
    }
};
GpuCalibratedScene::GpuCalibratedScene():state_(std::make_unique<State>()) {}
GpuCalibratedScene::~GpuCalibratedScene()=default;
void GpuCalibratedScene::release_device() noexcept {state_->release();}
const std::string& GpuCalibratedScene::status() const noexcept {return state_->status;}
bool GpuCalibratedScene::initialize(void* device,int color_format) {
    state_->release();
    try {
        ensure(device,"Missing calibrated scene device");state_->device=static_cast<SDL_GPUDevice*>(device);
        const auto format=static_cast<SDL_GPUTextureFormat>(color_format);
        ensure(format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported calibrated scene color format");
        ensure(SDL_GPUTextureSupportsFormat(state_->device,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET),"Calibrated scene depth target unsupported");
        state_->format=format;
        const auto formats=SDL_GetGPUShaderFormats(state_->device);const bool spirv=(formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        ensure(spirv || (formats&SDL_GPU_SHADERFORMAT_DXIL),"Calibrated scene requires SPIR-V or DXIL");
        SDL_GPUShaderCreateInfo shader{};shader.num_uniform_buffers=1;shader.num_storage_buffers=1;
        shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        for(unsigned stage=0;stage<2;++stage) {
            shader.stage=stage?SDL_GPU_SHADERSTAGE_FRAGMENT:SDL_GPU_SHADERSTAGE_VERTEX;
            shader.entrypoint=stage?"fragment_textured_main":"vertex_textured_main";
            shader.code=stage?calibrated_scene_shader::fragment_spirv:calibrated_scene_shader::vertex_spirv;
            shader.code_size=stage?sizeof(calibrated_scene_shader::fragment_spirv):sizeof(calibrated_scene_shader::vertex_spirv);
#if defined(_WIN32)
            if(!spirv) {
                shader.code=stage?calibrated_scene_shader::fragment_dxil:calibrated_scene_shader::vertex_dxil;
                shader.code_size=stage?sizeof(calibrated_scene_shader::fragment_dxil):sizeof(calibrated_scene_shader::vertex_dxil);
            }
#endif
            auto*& target=stage?state_->fragment:state_->vertex;
            target=create_gpu_shader(state_->device,&shader);require(target);
        }
        state_->status="Calibrated scene ready";return true;
    } catch(const std::exception& e) {state_->status=e.what();state_->release();return false;}
}
bool GpuCalibratedScene::upload(void* command,std::span<const CalibratedSceneDraw> draws) {
    state_->used=0;state_->command=nullptr;
    try {
        ensure(state_->device && command,"Missing calibrated scene upload command");
        ensure(draws.size()<=4096,"Calibrated scene exceeds draw budget");
        std::uint64_t vertices=0,texels=0;
        for(const auto& draw:draws) {
            vertices+=draw.vertices.size();texels+=std::max(std::size_t(1),draw.texels.size());
            ensure(vertices<=4'000'000 && texels<=4'000'000,"Calibrated scene exceeds upload budget");validate(draw);
        }
        return upload_impl(command,draws,{});
    } catch(const std::exception& e) {state_->status=e.what();state_->token=0;state_->cost={};return false;}
}
bool GpuCalibratedScene::upload_packets(void* command,std::span<const CalibratedScenePacket> packets) {
    state_->used=0;state_->command=nullptr;state_->token=0;state_->cost={};
    try {
        ensure(state_->device && command && packets.size()<=4096,"Invalid calibrated native-packet upload");
        vr::ScenePacketValidator validator(true,true);
        std::uint64_t compute_words=0;
        std::vector<CalibratedSceneDraw> draws;std::vector<std::shared_ptr<const std::vector<std::uint32_t>>> keys;
        draws.reserve(packets.size()*2);keys.reserve(packets.size()*2);
        for(const auto& p:packets) {
            ensure(p.packet && unsigned(p.blend)<=unsigned(vr::SceneBlend::alpha),"Invalid calibrated native-packet descriptor");
            ensure(!p.ray_caster || !p.after_rays,"Post-ray native layer selected as a caster");
            ensure(!p.reflection_environment || (!p.ray_caster && !p.after_rays && !p.camera_override),
                "Headlocked/model/post-ray layer selected as reflection scenery");
            ensure(!p.reflective_material || (p.ray_caster && !p.after_rays && !p.camera_override && !p.packet->preserve_native_colour),
                "Native/UI layer selected as reflective material");
            ensure(!p.ground_receiver || (!p.ray_caster && !p.after_rays && !p.camera_override
                && !p.packet->preserve_native_colour && p.blend==vr::SceneBlend::opaque
                && calibrated_ground_ray_surface(calibrated_ground_material(*p.packet))),
                "Nonliquid/nonmetal/protected/model layer selected as ray ground");
            ensure(p.effect_layer<=2 && (!p.effect_layer || (!p.after_rays && !p.camera_override && !p.packet->preserve_native_colour)),
                "Protected/post-ray native layer selected for effects");
            if(p.camera_override) ensure(valid_camera(*p.camera_override),"Invalid calibrated layer camera");
            if(p.model_override) {
                const vr::EyeCamera identity{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},{}};
                ensure(bool(vr::model_eye_camera(identity,*p.model_override)),"Invalid calibrated presentation model");
            }
            validator.add(*p.packet);
            const auto& geometry=p.packet->geometry;
            if(!geometry.vertex_view().empty() && geometry.vertex_view().front().texture[3]==vr::gpu_connected_grid_flag) {
                compute_words+=vr::connected_grid_output_words;
                ensure(compute_words<=4'000'000,"Calibrated connected grids exceed output budget");
            }
            for(unsigned lines=0;lines<2;++lines) {
                const auto vertices=lines?geometry.line_view():geometry.vertex_view();if(vertices.empty()) continue;
                draws.push_back({vertices,geometry.texel_view(),p.model_override.value_or(p.packet->model),
                    lines?vr::SceneTopology::lines:vr::SceneTopology::triangles,p.blend,p.depth_test,
                    p.camera_override,p.packet->preserve_native_colour,p.effects_override,p.ray_caster,p.after_rays,p.reflection_environment,p.reflective_material,p.effect_layer,p.ground_receiver});
                keys.push_back(geometry.shared_texels);
            }
        }
        return upload_impl(command,draws,keys);
    } catch(const std::exception& e) {state_->status=e.what();return false;}
}
std::uint64_t GpuCalibratedScene::upload_token() const noexcept {return state_->token;}
std::array<std::uint64_t,3> GpuCalibratedScene::upload_cost() const noexcept {return state_->cost;}
bool GpuCalibratedScene::supports_multisample(unsigned samples,bool receiver,bool surfaces) const noexcept {
    if(!state_->device || (samples!=1 && samples!=2 && samples!=4 && samples!=8)) return false;
    const auto count=native_sample_count(samples);
    if(surfaces && !receiver) return false;
    if(samples>1 && receiver && !SDL_GetBooleanProperty(SDL_GetGPUDeviceProperties(state_->device),STARFOX_SDL_MULTISAMPLE_READ,false)) return false;
    return SDL_GPUTextureSupportsSampleCount(state_->device,state_->format,count)
        && SDL_GPUTextureSupportsSampleCount(state_->device,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,count)
        && (!receiver || SDL_GPUTextureSupportsSampleCount(state_->device,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,count))
        && (!surfaces || SDL_GPUTextureSupportsSampleCount(state_->device,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,count));
}
bool GpuCalibratedScene::notify_submitted(std::uint64_t token) noexcept {
    if(!token || token!=state_->token) return false;
    for(auto& [key,texture]:state_->immutable) if(texture->encoded==token) texture->ready=true;
    state_->submitted_token=token;
    // SDL recycles command-buffer addresses after submission. Pointer equality
    // is not proof that a later command owns this upload.
    state_->command=nullptr;
    return true;
}
bool GpuCalibratedScene::upload_impl(void* command,std::span<const CalibratedSceneDraw> draws,
    std::span<const std::shared_ptr<const std::vector<std::uint32_t>>> immutable_texels) {
    state_->token=state_->submitted_token=0;state_->cost={};
    try {
        ensure(immutable_texels.empty() || immutable_texels.size()==draws.size(),"Calibrated immutable data count mismatch");
        ++state_->generation;ensure(state_->generation!=0,"Calibrated upload generation exhausted");
        state_->items.resize(std::max(state_->items.size(),draws.size()));
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        for(unsigned i=0;i<draws.size();++i) {
            const auto& draw=draws[i];auto& item=state_->items[i];
            ensure(!draw.ray_caster || !draw.after_rays,"Post-ray layer selected as a caster");
            ensure(!draw.reflection_environment || (!draw.ray_caster && !draw.after_rays && !draw.camera_override),
                "Headlocked/model/post-ray draw selected as reflection scenery");
            ensure(!draw.reflective_material || (draw.ray_caster && !draw.after_rays && !draw.camera_override && !draw.preserve_native_colour),
                "Native/UI draw selected as reflective material");
            ensure(draw.effect_layer<=2 && (!draw.effect_layer || (!draw.after_rays && !draw.camera_override && !draw.preserve_native_colour)),
                "Protected/post-ray draw selected for effects");
            item.count=unsigned(draw.vertices.size());item.model=draw.model;
            item.topology=draw.topology;item.blend=draw.blend;item.depth=draw.depth_test;
            item.camera_override=draw.camera_override;item.preserve_native_colour=draw.preserve_native_colour;
            item.effects_override=draw.effects_override;
            item.ray_caster=draw.ray_caster;
            item.after_rays=draw.after_rays;
            item.reflection_environment=draw.reflection_environment;
            item.reflective_material=draw.reflective_material;
            item.effect_layer=draw.effect_layer;
            item.ground_surface=false;item.transport_ground=false;item.ground_receiver=draw.ground_receiver;
            // Validation requires a homogeneous, single-header native ground
            // packet. Do not scan every ordinary model's vertex stream just
            // to select this optional landscape shader.
            if(!draw.vertices.empty()
                && (draw.vertices.front().texture[3]&(calibrated_ground_flag|8U))==(calibrated_ground_flag|8U)) {
                const auto offset=std::size_t(draw.vertices.front().texture[0])+calibrated_ground_offset;
                ensure(offset<=draw.texels.size() && draw.texels.size()-offset>=calibrated_ground_words,
                    "Incomplete calibrated ground metadata");
                item.ground_surface=draw.texels[offset+8]!=0 || draw.texels[offset+13]==3;
                item.transport_ground=calibrated_ground_ray_surface(draw.texels[offset+8]);
            }
            ensure(!draw.ground_receiver || (item.transport_ground && !draw.ray_caster && !draw.after_rays
                && !draw.camera_override && !draw.preserve_native_colour && draw.blend==vr::SceneBlend::opaque),
                "Nonliquid/nonmetal/protected/model draw selected as ray ground");
            item.ray_texture_bytes=0;
            item.ray_supported=draw.depth_test
                && draw.blend==vr::SceneBlend::opaque && !draw.camera_override;
            if(draw.ray_caster) for(const auto& v:draw.vertices)
                item.ray_supported=item.ray_supported && (v.texture[3]&~(7U|134217728U|536870912U))==0
                    && (!(v.texture[3]&4U) || (v.texture[3]&1U))
                    && (!(v.texture[3]&(134217728U|536870912U)) || (v.texture[3]&1U))
                    && ((v.texture[3]&1U) || (v.color[3]==1 && (!v.dither_scale || v.odd_color[3]==1)));
            if(draw.ray_caster && item.ray_supported && std::any_of(draw.vertices.begin(),draw.vertices.end(),
                [](const auto& v){return (v.texture[3]&1U)!=0;})) {
                item.ray_texture_bytes=unsigned(draw.texels.size_bytes());
                // Indexed payloads contain packed index bytes after their RGBA
                // palette. Those bytes are NOT alpha. Check the actual colour
                // region(s), then reuse the unchanged resident raster payload.
                std::vector<std::array<unsigned,2>> checked_colour_regions;
                for(const auto& v:draw.vertices) if(v.texture[3]&1U) {
                    const auto colours=(v.texture[3]&536870912U)?256U:(v.texture[1]+1U)*(v.texture[2]+1U);
                    ensure(std::uint64_t(v.texture[0])+colours<=draw.texels.size(),"Calibrated ray texture colour region invalid");
                    const std::array region{v.texture[0],colours};
                    if(std::find(checked_colour_regions.begin(),checked_colour_regions.end(),region)!=checked_colour_regions.end()) continue;
                    checked_colour_regions.push_back(region);
                    for(auto texel:draw.texels.subspan(v.texture[0],colours)) item.ray_supported=item.ray_supported
                        && ((texel>>24)==0 || (texel>>24)==255);
                }
            }
            if(!item.count) continue;
            const unsigned vb=unsigned(draw.vertices.size_bytes()),tb=unsigned(std::max(std::size_t(4),draw.texels.size_bytes()));
            const auto grow=[&](SDL_GPUBuffer*& buffer,unsigned& capacity,unsigned need,SDL_GPUBufferUsageFlags usage) {
                if(capacity>=need) return;
                SDL_GPUBufferCreateInfo info{usage,need,0};auto* next=SDL_CreateGPUBuffer(state_->device,&info);require(next);
                if(buffer) SDL_ReleaseGPUBuffer(state_->device,buffer);buffer=next;capacity=need;
            };
            grow(item.vertices,item.vertex_bytes,vb,SDL_GPU_BUFFERUSAGE_VERTEX|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ);
            const auto image_source=immutable_texels.empty()?nullptr:immutable_texels[i];
            // Solid cached meshes may deliberately share an empty texel vector.
            // Use the dummy binding, never memcpy four bytes from empty data.
            const auto key=image_source && !image_source->empty()?image_source:nullptr;
            const unsigned texture_bytes=key?0:tb;
            if(key) {
                ensure(key->size()==draw.texels.size() && key->data()==draw.texels.data(),"Calibrated immutable artwork mismatch");
                auto& cached=state_->immutable[key.get()];
                if(!cached) {cached=std::make_unique<State::ImmutableTexture>();cached->device=state_->device;cached->source=key;}
                cached->used=state_->generation;
                if(!cached->ready && cached->encoded!=state_->generation) {
                    if(!cached->buffer) {
                        SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,tb,0};
                        cached->buffer=SDL_CreateGPUBuffer(state_->device,&buffer);require(cached->buffer);
                    }
                    if(!cached->upload) {
                        const SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,tb,0};
                        cached->upload=SDL_CreateGPUTransferBuffer(state_->device,&transfer);require(cached->upload);
                    }
                    auto* mapped=SDL_MapGPUTransferBuffer(state_->device,cached->upload,true);require(mapped);
                    std::memcpy(mapped,draw.texels.data(),tb);SDL_UnmapGPUTransferBuffer(state_->device,cached->upload);
                    auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
                    const SDL_GPUTransferBufferLocation source{cached->upload,0};const SDL_GPUBufferRegion dest{cached->buffer,0,tb};
                    SDL_UploadToGPUBuffer(copy,&source,&dest,true);SDL_EndGPUCopyPass(copy);
                    cached->encoded=state_->generation;state_->cost[1]+=tb;
                } else state_->cost[2]+=tb;
                item.bound_texels=cached->buffer;
            } else {
                grow(item.texels,item.texel_bytes,tb,SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ);item.bound_texels=item.texels;
            }
            if(item.upload_bytes<vb+texture_bytes) {
                SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,vb+texture_bytes,0};
                auto* next=SDL_CreateGPUTransferBuffer(state_->device,&info);require(next);
                if(item.upload) SDL_ReleaseGPUTransferBuffer(state_->device,item.upload);
                item.upload=next;item.upload_bytes=vb+texture_bytes;
            }
            auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(state_->device,item.upload,true));require(mapped);
            std::memcpy(mapped,draw.vertices.data(),vb);
            if(texture_bytes) {
                if(draw.texels.empty()) std::memset(mapped+vb,0,4);else std::memcpy(mapped+vb,draw.texels.data(),tb);
            }
            SDL_UnmapGPUTransferBuffer(state_->device,item.upload);
            auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
            SDL_GPUTransferBufferLocation source{item.upload,0};SDL_GPUBufferRegion dest{item.vertices,0,vb};
            SDL_UploadToGPUBuffer(copy,&source,&dest,true);
            if(texture_bytes) {source.offset=vb;dest={item.texels,0,tb};SDL_UploadToGPUBuffer(copy,&source,&dest,true);}
            SDL_EndGPUCopyPass(copy);state_->cost[0]+=vb;state_->cost[1]+=texture_bytes;
            if(draw.vertices.front().texture[3]==vr::gpu_connected_grid_flag) {
                if(!item.grid_output) {
                    SDL_GPUBufferCreateInfo output{SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                        |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,vr::connected_grid_output_words*4,0};
                    item.grid_output=SDL_CreateGPUBuffer(state_->device,&output);require(item.grid_output);
                }
                // Separate passes establish compute-write -> compute-read ->
                // graphics-read ordering on SDL's same device/queue. Cycle only
                // the first write so prior in-flight eyes retain their output.
                for(unsigned stage=0;stage<2;++stage) {
                    auto* pipeline=state_->connected_pipeline(stage);
                    SDL_GPUStorageBufferReadWriteBinding output{item.grid_output,stage==0,0,0,0};
                    auto* compute=SDL_BeginGPUComputePass(cmd,nullptr,0,&output,1);require(compute);
                    SDL_BindGPUComputePipeline(compute,pipeline);
                    SDL_BindGPUComputeStorageBuffers(compute,0,&item.bound_texels,1);
                    SDL_DispatchGPUCompute(compute,stage?3:4,1,1);SDL_EndGPUComputePass(compute);
                }
                item.bound_texels=item.grid_output;
            } else if(item.grid_output) {
                SDL_ReleaseGPUBuffer(state_->device,item.grid_output);item.grid_output=nullptr;
            }
            state_->pipeline(item);
        }
        for(std::size_t i=draws.size();i<state_->items.size();++i) if(auto*& output=state_->items[i].grid_output;output) {
            SDL_ReleaseGPUBuffer(state_->device,output);output=nullptr;
        }
        for(auto it=state_->immutable.begin();it!=state_->immutable.end();) {
            if(it->second->used!=state_->generation) it=state_->immutable.erase(it);else ++it;
        }
        state_->used=draws.size();state_->command=command;state_->token=state_->generation;state_->status="Calibrated scene uploaded";return true;
    } catch(const std::exception& e) {state_->status=e.what();return false;}
}
bool GpuCalibratedScene::enqueue_eye(void* command,void* color,void* depth,unsigned width,unsigned height,
    const vr::EyeCamera& camera,std::array<float,4> clear,CalibratedScenePhase phase,void* receiver,
    std::uint64_t continuation_token,void* surfaces,bool aa_ownership,const CalibratedSceneMultisample& multisample,
    void* motion,const CalibratedSceneMotion* previous) {
    SDL_GPURenderPass* pass{};
    try {
        const bool continued=phase==CalibratedScenePhase::after_rays && continuation_token
            && continuation_token==state_->token && continuation_token==state_->submitted_token;
        const bool original=phase!=CalibratedScenePhase::after_rays && state_->token && !state_->submitted_token
            && command==state_->command;
        ensure(state_->device && command && (original || continued) && color && depth && color!=depth,
            "Invalid calibrated eye targets/command");
        ensure(unsigned(phase)<=unsigned(CalibratedScenePhase::upscale_world)
            && (!receiver || ((phase==CalibratedScenePhase::before_rays || phase==CalibratedScenePhase::after_rays || phase==CalibratedScenePhase::world_only
                || phase==CalibratedScenePhase::upscale_scene || phase==CalibratedScenePhase::upscale_world)
                && receiver!=color && receiver!=depth)),
            "Invalid calibrated receiver target/phase");
        ensure(!surfaces || (receiver && surfaces!=receiver && surfaces!=color && surfaces!=depth),
            "Invalid calibrated surface target");
        ensure(bool(motion)==bool(previous),"Native motion target/history must be paired");
        ensure(!motion || (surfaces && motion!=color && motion!=depth && motion!=receiver && motion!=surfaces
            && motion!=multisample.resolve_color),"Invalid native motion targets");
        ensure(!previous || (previous->draws.size()==state_->used && valid_camera(previous->previous_camera)
            && previous->previous_width && previous->previous_height
            && previous->previous_width<=32768 && previous->previous_height<=32768),"Invalid native motion history layout");
        std::uint64_t previous_bytes=0;
        if(previous) for(unsigned i=0;i<state_->used;++i) {
            const auto& prior=previous->draws[i];
            ensure(!prior.valid || state_->items[i].topology!=vr::SceneTopology::lines || !prior.previous_vertices.empty(),
                "Native line motion requires accepted endpoint geometry");
            if(prior.previous_vertices.empty()) continue;
            ensure(prior.valid && prior.previous_vertices.size()==state_->items[i].count,"Invalid native previous primitive layout");
            previous_bytes+=prior.previous_vertices.size_bytes();
            ensure(previous_bytes<=256ULL*1024*1024,"Native accepted primitive stream exceeds its 256-MiB source bound");
            validate_previous_geometry(prior.previous_vertices);
            if(continued) {
                const auto& item=state_->items[i];
                ensure(item.previous_encoded==state_->token && item.previous_source.size()==prior.previous_vertices.size()
                    && std::equal(prior.previous_vertices.begin(),prior.previous_vertices.end(),item.previous_source.begin()),
                    "Previous geometry must be encoded with the original source command");
            }
        }
        ensure(!aa_ownership || receiver,"Native AA ownership requires the raster receiver");
        ensure(multisample.samples==1 || supports_multisample(multisample.samples,receiver!=nullptr,surfaces!=nullptr),
            "Native target formats do not support the requested MSAA count");
        ensure(multisample.samples>1 || (!multisample.resolve_color && !multisample.retain_samples),
            "Single-sample native raster does not accept MSAA resolve/retention");
        ensure(!multisample.resolve_color || (multisample.resolve_color!=color && multisample.resolve_color!=depth
            && multisample.resolve_color!=receiver && multisample.resolve_color!=surfaces),"Native MSAA resolve aliases a raster target");
        ensure(multisample.samples==1 || !receiver || multisample.retain_samples,
            "Native MSAA guides require retained colour samples for per-sample composition");
        ensure(multisample.samples==1 || phase!=CalibratedScenePhase::before_rays || multisample.retain_samples,
            "Ordered native MSAA continuation requires retained samples");
        ensure(width && height && width<=32768 && height<=32768 && valid_camera(camera),"Invalid calibrated eye extent/camera");
        if(surfaces) {
            const auto& p=camera.projection;
            ensure(p[0]>0 && p[5]>0 && p[1]==0 && p[2]==0 && p[3]==0 && p[4]==0 && p[6]==0
                && p[7]==0 && p[11]==-1 && p[12]==0 && p[13]==0 && p[15]==0 && p[10]<=-1 && p[14]<0,
                "Unsupported calibrated surface perspective projection");
        }
        for(float value:clear) ensure(std::isfinite(value) && value>=0 && value<=1,"Invalid calibrated clear color");
        std::vector<vr::SceneConstants> constants;constants.reserve(state_->used);
        std::vector<MotionUniform> motion_constants;if(previous) motion_constants.reserve(state_->used);
        std::vector<PreviousGeometryUniform> previous_geometry;if(previous) previous_geometry.reserve(state_->used);
        for(unsigned i=0;i<state_->used;++i) {
            const auto& item=state_->items[i];auto view=item.camera_override.value_or(camera);
            if(item.effects_override) view.effects=*item.effects_override;
            if(item.preserve_native_colour) view.effects={};
            // Deferred styles are evaluated on the actual composed eye. Do not
            // tint them first or let excluded emissive/native ink inherit them.
            if(receiver && calibrated_composite_effect(view.effects[0])) view.effects[0]=0;
            // Reserved calibrated shader bit: inputs/outputs are linear when
            // the runtime negotiated an sRGB render target. Palette equations
            // still run in the same authored 8-bit domain as the flat renderer.
            const bool srgb=state_->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                || state_->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
            view.effects[2]=(view.effects[2]&~1U)|unsigned(srgb);
            if(receiver) view.effects[3]=(view.effects[3]&~0x80000000U)
                |(item.ray_caster && item.ray_supported && !item.after_rays?0x80000000U:0U);
            // Reserved receiver-only bit. The regular raster/ray colours still
            // use the exact source styling; selected metals change transport,
            // not the primary source alpha or an unrelated world/UI packet.
            view.effects[3]=(view.effects[3]&~0x40000000U)|(item.reflective_material?0x40000000U:0U);
            view.effects[3]=(view.effects[3]&~0x04000000U)|(receiver && item.ground_receiver?0x04000000U:0U);
            view.effects[3]=(view.effects[3]&~0x00400000U)
                |(phase==CalibratedScenePhase::environment && item.transport_ground?0x00400000U:0U);
            view.effects[3]=(view.effects[3]&~0x03000000U)|(item.effect_layer<<24);
            view.effects[3]=(view.effects[3]&~0x08000000U)|(aa_ownership?0x08000000U:0U);
            if(multisample.samples>1 && receiver) view.effects[3]=(view.effects[3]&~0x30000000U)
                |(unsigned(native_sample_count(multisample.samples))<<28);
            const auto eye=vr::model_eye_camera(view,item.model);
            ensure(bool(eye),"Invalid calibrated model-eye transform");constants.push_back(vr::scene_constants(*eye));
            if(previous) {
                const auto& prior=previous->draws[i];MotionUniform uniform{};
                PreviousGeometryUniform geometry{};
                geometry.padding[0]=unsigned(item.topology==vr::SceneTopology::lines);
                if(prior.valid) {
                    const auto& old_camera=prior.previous_camera_override.value_or(previous->previous_camera);
                    const auto mapping=calibrated_motion_mapping(view,old_camera,item.model,prior.previous_model);
                    ensure(bool(mapping),"Invalid native rigid motion transform/projection");
                    uniform.mapping=*mapping;const auto& p=old_camera.projection;
                    uniform.projection={p[0],p[5],p[8],p[9]};uniform.valid=1;
                    if(!prior.previous_vertices.empty()) {
                        const auto old_eye=vr::model_eye_camera(old_camera,prior.previous_model);
                        ensure(bool(old_eye),"Invalid native previous primitive model-eye transform");
                        geometry.view=old_eye->view;geometry.count=unsigned(prior.previous_vertices.size());uniform.valid=2;
                    }
                }
                uniform.width=float(previous->previous_width);uniform.height=float(previous->previous_height);
                motion_constants.push_back(uniform);
                previous_geometry.push_back(geometry);
            }
        }
        // All layouts/transforms/payloads are validated before any previous
        // source copies or raster begin. Reuse is exact-content + upload token,
        // never pointer identity; a cancelled source upload is copied again.
        if(previous) for(unsigned i=0;i<state_->used;++i) if(!previous->draws[i].previous_vertices.empty())
            state_->previous_geometry(static_cast<SDL_GPUCommandBuffer*>(command),state_->items[i],previous->draws[i].previous_vertices,continued);
        SDL_GPUColorTargetInfo targets[4]{};auto& target=targets[0];target.texture=static_cast<SDL_GPUTexture*>(color);
        target.clear_color={clear[0],clear[1],clear[2],clear[3]};
        target.load_op=phase==CalibratedScenePhase::after_rays?SDL_GPU_LOADOP_LOAD:SDL_GPU_LOADOP_CLEAR;
        target.store_op=SDL_GPU_STOREOP_STORE;
        if(multisample.resolve_color) {
            target.resolve_texture=static_cast<SDL_GPUTexture*>(multisample.resolve_color);
            target.store_op=multisample.retain_samples?SDL_GPU_STOREOP_RESOLVE_AND_STORE:SDL_GPU_STOREOP_RESOLVE;
        }
        if(receiver) {targets[1].texture=static_cast<SDL_GPUTexture*>(receiver);
            targets[1].load_op=phase==CalibratedScenePhase::after_rays?SDL_GPU_LOADOP_LOAD:SDL_GPU_LOADOP_CLEAR;
            targets[1].store_op=SDL_GPU_STOREOP_STORE;
            targets[1].clear_color.b=1.F/255;}
        if(surfaces) {targets[2].texture=static_cast<SDL_GPUTexture*>(surfaces);
            targets[2].load_op=target.load_op;targets[2].store_op=SDL_GPU_STOREOP_STORE;}
        if(motion) {targets[3].texture=static_cast<SDL_GPUTexture*>(motion);
            targets[3].load_op=target.load_op;targets[3].store_op=SDL_GPU_STOREOP_STORE;}
        // Never cycle an XR eye's completed texture when rendering the other
        // eye. The caller supplies two independent color/depth targets.
        SDL_GPUDepthStencilTargetInfo z{};z.texture=static_cast<SDL_GPUTexture*>(depth);z.clear_depth=1;
        z.load_op=target.load_op;z.store_op=SDL_GPU_STOREOP_STORE;
        z.stencil_load_op=SDL_GPU_LOADOP_DONT_CARE;z.stencil_store_op=SDL_GPU_STOREOP_DONT_CARE;
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);pass=SDL_BeginGPURenderPass(cmd,targets,motion?4:surfaces?3:receiver?2:1,&z);require(pass);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        for(unsigned i=0;i<state_->used;++i) {
            const auto& item=state_->items[i];if(!item.count) continue;
            if(phase==CalibratedScenePhase::environment) {if(!item.reflection_environment) continue;}
            else if(phase==CalibratedScenePhase::world_only || phase==CalibratedScenePhase::upscale_world) {if(item.effect_layer!=1 || item.after_rays) continue;}
            else if(phase==CalibratedScenePhase::upscale_scene) {if(item.effect_layer==0 || item.after_rays) continue;}
            else if(phase!=CalibratedScenePhase::all && item.after_rays!=(phase==CalibratedScenePhase::after_rays)) continue;
            SDL_BindGPUGraphicsPipeline(pass,state_->pipeline(item,receiver!=nullptr,surfaces!=nullptr,multisample.samples,motion!=nullptr));
            const SDL_GPUBufferBinding vertices{item.vertices,0};SDL_BindGPUVertexBuffers(pass,0,&vertices,1);
            SDL_BindGPUVertexStorageBuffers(pass,0,&item.bound_texels,1);SDL_BindGPUFragmentStorageBuffers(pass,0,&item.bound_texels,1);
            if(motion) {
                auto* prior=previous_geometry[i].count?item.previous_vertices:item.vertices;
                SDL_BindGPUVertexStorageBuffers(pass,1,&prior,1);
                SDL_PushGPUVertexUniformData(cmd,1,&previous_geometry[i],sizeof(PreviousGeometryUniform));
            }
            SDL_PushGPUVertexUniformData(cmd,0,&constants[i],sizeof(vr::SceneConstants));
            SDL_PushGPUFragmentUniformData(cmd,0,&constants[i],sizeof(vr::SceneConstants));
            if(surfaces) {
                struct SurfaceUniform {float width,height,units;unsigned eligible;};
                const SurfaceUniform uniform{float(width),float(height),256,
                    unsigned(item.blend==vr::SceneBlend::opaque && !item.after_rays && !item.camera_override
                        && !item.preserve_native_colour)*(motion && item.topology==vr::SceneTopology::lines?2U:1U)};
                SDL_PushGPUFragmentUniformData(cmd,1,&uniform,sizeof(uniform));
            }
            if(motion) SDL_PushGPUFragmentUniformData(cmd,2,&motion_constants[i],sizeof(MotionUniform));
            SDL_DrawGPUPrimitives(pass,item.count,1,0,0);
        }
        SDL_EndGPURenderPass(pass);pass=nullptr;state_->status="Calibrated eye rendered";return true;
    } catch(const std::exception& e) {if(pass) SDL_EndGPURenderPass(pass);state_->status=e.what();return false;}
}
CalibratedRayGeometryOutput GpuCalibratedScene::enqueue_ray_geometry(void* command,unsigned index,
    unsigned width,unsigned height,const vr::EyeCamera& camera,float units,unsigned environment_size,
    std::array<float,4> environment_clear,const CalibratedSceneMotion* previous) {
    try {
        ensure(state_->device && command && command==state_->command && state_->token && index<2,
            "Invalid calibrated ray command/eye");
        ensure(width && height && width<=16384 && height<=16384 && valid_camera(camera)
            && std::isfinite(units) && units>0 && units<=1.e6F,"Invalid calibrated ray extent/camera/units");
        ensure(!environment_size || (environment_size>=8 && environment_size<=512 && !(environment_size&(environment_size-1))),
            "Invalid calibrated environment cube size");
        const auto environment=environment_size?calibrated_environment_cameras(camera):std::optional<CalibratedEnvironmentCameras>{};
        ensure(!environment_size || bool(environment),"Calibrated environment requires a rigid tracked eye");
        const auto& p=camera.projection;
        ensure(p[0]>0 && p[5]>0 && p[1]==0 && p[2]==0 && p[3]==0 && p[4]==0 && p[6]==0
            && p[7]==0 && p[11]==-1 && p[12]==0 && p[13]==0 && p[15]==0
            && p[10]<=-1 && p[14]<0,"Unsupported calibrated ray perspective projection");
        CalibratedRayGeometryOutput result;
        result.device=state_->device;result.width=width;result.height=height;
        // Keep centre and per-MSAA-sample native cameras on the SAME double
        // pixel calibration. Rounding 1 +/- projection-centre in float first
        // changes old curved-hit footprints and their conservative tap guards.
        result.projection={double(width)*p[0]/2,double(height)*p[5]/2,
            double(width)*(1.0-p[8])/2,double(height)*(1.0+p[9])/2};
        result.near_plane=double(p[14])/p[10]*units;
        if(p[10]<-1) result.far_plane=double(p[14])/(p[10]+1)*units;
        if(previous) {
            ensure(previous->draws.size()==state_->used && previous->previous_width && previous->previous_height
                && previous->previous_width<=16384 && previous->previous_height<=16384,
                "Invalid calibrated ray history layout/extent");
            ensure(bool(calibrated_motion_mapping(camera,previous->previous_camera,
                vr::Matrix4{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},
                vr::Matrix4{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1})),"Invalid calibrated ray previous camera");
            const auto& old=previous->previous_camera.projection;
            result.previous_extent={previous->previous_width,previous->previous_height};
            result.previous_projection={double(previous->previous_width)*old[0]/2,double(previous->previous_height)*old[5]/2,
                double(previous->previous_width)*(1.0-old[8])/2,double(previous->previous_height)*(1.0+old[9])/2};
            result.previous_near_plane=double(old[14])/old[10]*units;
            if(old[10]<-1) result.previous_far_plane=double(old[14])/(old[10]+1)*units;
        }
        std::uint64_t count=0,texture_bytes=0;
        std::uint64_t previous_bytes=0;
        for(unsigned i=0;i<state_->used;++i) {
            const auto& item=state_->items[i];if(!item.ray_caster || !item.count) continue;
            ensure(item.ray_supported,"Calibrated ray caster coverage/material path unsupported; complete batch declined");
            ensure(bool(vr::model_eye_camera(camera,item.model)),"Invalid calibrated ray model-eye transform");
            count+=std::uint64_t(item.count)*(item.topology==vr::SceneTopology::lines?3:1);
            texture_bytes+=item.ray_texture_bytes;
            if(previous) {
                const auto& prior=previous->draws[i];
                ensure(prior.previous_ray_first_triangle==UINT32_MAX || (prior.valid
                    && std::uint64_t(prior.previous_ray_first_triangle)
                        +(item.topology==vr::SceneTopology::lines?item.count:item.count/3)<=4'000'000/3),
                    "Invalid accepted ray primitive index range");
                ensure(prior.previous_vertices.empty() || (prior.valid && prior.previous_vertices.size()==item.count),
                    "Invalid calibrated ray previous primitive correspondence");
                ensure(!prior.valid || item.topology!=vr::SceneTopology::lines || !prior.previous_vertices.empty(),
                    "Previous calibrated ray lines require accepted endpoints");
                if(prior.valid) {
                    const auto& old_camera=prior.previous_camera_override.value_or(previous->previous_camera);
                    ensure(old_camera.projection==previous->previous_camera.projection
                        && bool(calibrated_motion_mapping(camera,old_camera,item.model,prior.previous_model)),
                        "Incompatible calibrated ray previous model/eye");
                }
                if(!prior.previous_vertices.empty()) {
                    previous_bytes+=prior.previous_vertices.size_bytes();
                    ensure(previous_bytes<=256ULL*1024*1024,"Previous calibrated ray primitives exceed source budget");
                    validate_previous_geometry(prior.previous_vertices);
                    for(const auto& v:prior.previous_vertices) ensure((v.texture[3]&~(7U|134217728U|536870912U))==0
                        && (!(v.texture[3]&(4U|134217728U|536870912U)) || (v.texture[3]&1U)),
                        "Previous calibrated ray primitive requires an unsupported source transform");
                }
            }
        }
        // Fog can illuminate an empty world. Publish an explicit empty batch
        // with the validated actual-eye projection, never a previous buffer.
        const bool analytic_receiver=std::any_of(state_->items.begin(),state_->items.begin()+state_->used,
            [](const auto& item){return item.ground_receiver;});
        if(!count && !environment_size && !analytic_receiver) {
            result.complete=true;state_->status="Empty calibrated GPU ray geometry encoded";return result;
        }
        ensure(count<=4'000'000 && count%3==0,"Oversized calibrated ray caster batch");
        ensure(texture_bytes<=16'000'000,"Calibrated ray textures exceed resident budget");
        // An environment/analytic-only receiver has no vertices or triangle records.
        // Reserve aligned header space so the shared buffer/cube ABI remains
        // explicit, without manufacturing a caster or reusing a prior batch.
        const unsigned material_offset=std::max(16U,unsigned(count*16)),
            material_bytes=std::max(16U,unsigned(count/3*64+texture_bytes)),
            cube_bytes=environment_size*environment_size*6*4,
            previous_offset=previous && count?(material_offset+material_bytes+cube_bytes+15U)&~15U:0,
            previous_index_offset=previous_offset?previous_offset+unsigned(count*16):0,
            bytes=previous_offset?previous_index_offset+unsigned(count/3*4):material_offset+material_bytes+cube_bytes;
        auto*& buffer=state_->rays[index];auto& capacity=state_->ray_capacity[index];
        if(capacity<bytes) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes,0};
            auto* next=SDL_CreateGPUBuffer(state_->device,&info);require(next);
            if(buffer) SDL_ReleaseGPUBuffer(state_->device,buffer);buffer=next;capacity=bytes;
        }
        auto* pipeline=count?state_->ray_geometry_pipeline():nullptr;auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        unsigned first=0,texture_offset=unsigned(count/3*64);
        for(unsigned i=0;i<state_->used;++i) {
            const auto& item=state_->items[i];if(!item.ray_caster || !item.count) continue;
            const auto model_eye=vr::model_eye_camera(camera,item.model);
            auto effects=item.effects_override.value_or(camera.effects);
            if(item.preserve_native_colour) effects={};
            if(calibrated_post_effect(effects[0])) effects[0]=0;
            const bool srgb=state_->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                || state_->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
            effects[2]=(effects[2]&~1U)|unsigned(srgb);
            const bool lines=item.topology==vr::SceneTopology::lines;
            struct Uniforms {std::array<float,12> view;unsigned count,first;float units;unsigned materials;
                std::array<unsigned,4> effects;unsigned texture_offset,line_mode;std::array<float,2> focal;
                float near_plane;unsigned previous_mode,previous_index_offset,previous_first;} uniforms{
                vr::scene_constants(*model_eye).view_rows,item.count,first,units,material_offset,effects,texture_offset,
                unsigned(lines),{float(result.projection[0]),float(result.projection[1])},float(result.near_plane),0,0,UINT32_MAX};
            static_assert(sizeof(Uniforms)==112);
            SDL_GPUStorageBufferReadWriteBinding output{buffer,first==0,0,0,0};
            auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&output,1);require(pass);
            SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,&item.vertices,1);
            SDL_PushGPUComputeUniformData(cmd,0,&uniforms,sizeof(uniforms));
            SDL_DispatchGPUCompute(pass,(item.count/(lines?2:3)+63)/64,1,1);SDL_EndGPUComputePass(pass);
            if(previous_offset) {
                auto& mutable_item=state_->items[i];const auto& prior=previous->draws[i];
                auto* source=item.vertices;
                uniforms.previous_mode=prior.valid?1U:2U;
                if(prior.valid) {
                    const auto old_eye=vr::model_eye_camera(prior.previous_camera_override.value_or(previous->previous_camera),prior.previous_model);
                    uniforms.view=vr::scene_constants(*old_eye).view_rows;
                    if(!prior.previous_vertices.empty()) {
                        state_->previous_geometry(cmd,mutable_item,prior.previous_vertices,false);
                        source=item.previous_vertices;
                    }
                }
                uniforms.first=previous_offset/16+first;
                uniforms.materials=previous_offset;uniforms.previous_index_offset=previous_index_offset;
                uniforms.previous_first=prior.valid?prior.previous_ray_first_triangle:UINT32_MAX;
                uniforms.focal={float(result.previous_projection[0]),float(result.previous_projection[1])};
                uniforms.near_plane=float(result.previous_near_plane);
                // First current write cycles the shared allocation once; all
                // following previous/current/cube writes retain its contents.
                output.cycle=false;
                pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&output,1);require(pass);
                SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,&source,1);
                SDL_PushGPUComputeUniformData(cmd,0,&uniforms,sizeof(uniforms));
                SDL_DispatchGPUCompute(pass,(item.count/(lines?2:3)+63)/64,1,1);SDL_EndGPUComputePass(pass);
            }
            first+=item.count*(lines?3:1);
            texture_offset+=item.ray_texture_bytes;
        }
        // Copy already-uploaded raster texture words after the first cycling
        // compute write. Never cycle this copy: both records and the other
        // item's texture region must survive in the same eye's allocation.
        if(texture_bytes) {
            auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
            texture_offset=material_offset+unsigned(count/3*64);
            for(unsigned i=0;i<state_->used;++i) {
                const auto& item=state_->items[i];if(!item.ray_caster || !item.count || !item.ray_texture_bytes) continue;
                const SDL_GPUBufferLocation source{item.bound_texels,0},dest{buffer,texture_offset};
                SDL_CopyGPUBufferToBuffer(copy,&source,&dest,item.ray_texture_bytes,false);
                texture_offset+=item.ray_texture_bytes;
            }
            SDL_EndGPUCopyPass(copy);
        }
        result.buffer=buffer;result.vertex_count=unsigned(count);result.complete=true;
        result.materials=&state_->ray_materials;result.material_offset=material_offset;
        result.material_bytes=material_bytes;
        result.previous_vertex_offset=previous_offset;
        result.previous_index_offset=previous_index_offset;
        if(environment_size) {
            state_->prepare_environment(environment_size);
            const bool srgb=state_->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                || state_->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
            for(unsigned face=0;face<6;++face) {
                ensure(enqueue_eye(command,state_->environment_colour,state_->environment_depth,environment_size,environment_size,
                    environment->faces[face],environment_clear,CalibratedScenePhase::environment),state_->status.c_str());
                const std::array<unsigned,4> settings{environment_size,material_offset+material_bytes,face,unsigned(srgb)};
                const SDL_GPUStorageBufferReadWriteBinding target{buffer,false,0,0,0};
                auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&target,1);require(pass);
                SDL_BindGPUComputePipeline(pass,state_->environment_pipeline);
                const SDL_GPUTextureSamplerBinding source{state_->environment_colour,state_->environment_sampler};
                SDL_BindGPUComputeSamplers(pass,0,&source,1);
                SDL_PushGPUComputeUniformData(cmd,0,settings.data(),sizeof(settings));
                SDL_DispatchGPUCompute(pass,(environment_size+7)/8,(environment_size+7)/8,1);SDL_EndGPUComputePass(pass);
            }
            result.environment_offset=material_bytes;result.environment_face_size=environment_size;
            result.environment_rotation=environment->ray_to_cube;
        }
        state_->status="Calibrated GPU ray geometry encoded";return result;
    } catch(const std::exception& e) {state_->status=e.what();return {};}
}
}
