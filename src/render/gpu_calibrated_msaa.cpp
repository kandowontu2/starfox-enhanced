#include "starfox/render/gpu_calibrated_msaa.hpp"
#include "starfox/render/sdl_multisample.h"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool ok,const char* error) {if(!ok) throw std::runtime_error(error);}
bool valid_count(unsigned count) {return count==2 || count==4 || count==8;}
struct Uniform {unsigned width,height,count,index;};
}
std::optional<vr::EyeCamera> calibrated_msaa_sample_camera(const vr::EyeCamera& camera,
    unsigned width,unsigned height,unsigned count,unsigned index) noexcept {
    if(!width || !height || width>16384 || height>16384 || !valid_count(count) || index>=count) return {};
    for(const auto* matrix:{&camera.view,&camera.projection}) for(float value:*matrix) if(!std::isfinite(value)) return {};
    const auto& p=camera.projection;
    if(camera.view[3]!=0 || camera.view[7]!=0 || camera.view[11]!=0 || camera.view[15]!=1
        || p[0]<=0 || p[5]<=0 || p[1]!=0 || p[2]!=0 || p[3]!=0 || p[4]!=0 || p[6]!=0 || p[7]!=0
        || p[11]!=-1 || p[12]!=0 || p[13]!=0 || p[15]!=0 || p[10]>-1 || p[14]>=0) return {};
    constexpr std::array<std::array<int,2>,14> locations{{
#define SF_MSAA_SAMPLE(x,y) {x,y},
#include "starfox/render/msaa_samples.inc"
#undef SF_MSAA_SAMPLE
    }};
    const auto sample=locations[count-2+index];auto result=camera;
    result.projection[8]+=float(sample[0])/(8*width);
    result.projection[9]-=float(sample[1])/(8*height);
    return result;
}
struct GpuCalibratedMsaa::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUShader* vertex{};std::array<SDL_GPUShader*,5> fragments{};
    std::array<SDL_GPUGraphicsPipeline*,5> pipelines{};SDL_GPUSampler* sampler{};
    std::string status{"Native MSAA transport not initialized"};
    ~State() {
        if(!device) return;
        for(auto* p:pipelines) if(p) SDL_ReleaseGPUGraphicsPipeline(device,p);
        for(auto* f:fragments) if(f) SDL_ReleaseGPUShader(device,f);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
    void prepare(unsigned stage) {
        if(pipelines[stage]) return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUShaderCreateInfo shader{};shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        if(!vertex) {
            shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
            shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
            vertex=create_gpu_shader(device,&shader);require(vertex,SDL_GetError());
        }
        static const std::array<const Uint8*,5> code{calibrated_scene_shader::msaa_extract_spirv,
            calibrated_scene_shader::msaa_extract_surface_spirv,calibrated_scene_shader::msaa_resolve_spirv,calibrated_scene_shader::msaa_resolve_ink_spirv,
            calibrated_scene_shader::msaa_extract_motion_spirv};
        static const std::array<std::size_t,5> sizes{sizeof(calibrated_scene_shader::msaa_extract_spirv),
            sizeof(calibrated_scene_shader::msaa_extract_surface_spirv),sizeof(calibrated_scene_shader::msaa_resolve_spirv),sizeof(calibrated_scene_shader::msaa_resolve_ink_spirv),
            sizeof(calibrated_scene_shader::msaa_extract_motion_spirv)};
        static const std::array<const char*,5> entries{"msaa_extract_fragment_main","msaa_extract_surface_fragment_main","msaa_resolve_fragment_main","msaa_resolve_fragment_main",
            "msaa_extract_motion_fragment_main"};
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.num_uniform_buffers=1;shader.num_samplers=stage==4?4:stage==3?10:stage==2?8:3;
        shader.code=code[stage];shader.code_size=sizes[stage];shader.entrypoint=entries[stage];
#if defined(_WIN32)
        static const std::array<const Uint8*,5> dxil{calibrated_scene_shader::msaa_extract_dxil,
            calibrated_scene_shader::msaa_extract_surface_dxil,calibrated_scene_shader::msaa_resolve_dxil,calibrated_scene_shader::msaa_resolve_ink_dxil,
            calibrated_scene_shader::msaa_extract_motion_dxil};
        static const std::array<std::size_t,5> dxil_sizes{sizeof(calibrated_scene_shader::msaa_extract_dxil),
            sizeof(calibrated_scene_shader::msaa_extract_surface_dxil),sizeof(calibrated_scene_shader::msaa_resolve_dxil),sizeof(calibrated_scene_shader::msaa_resolve_ink_dxil),
            sizeof(calibrated_scene_shader::msaa_extract_motion_dxil)};
        if(!spirv) {shader.code=dxil[stage];shader.code_size=dxil_sizes[stage];}
#endif
        if(!fragments[stage]) {fragments[stage]=create_gpu_shader(device,&shader);require(fragments[stage],SDL_GetError());}
        SDL_GPUGraphicsPipelineCreateInfo info{};info.vertex_shader=vertex;info.fragment_shader=fragments[stage];
        info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;info.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        const SDL_GPUColorTargetDescription targets[]{{format,{}},{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,{}},{SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,{}},
            {SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,{}}};
        info.target_info.color_target_descriptions=targets;info.target_info.num_color_targets=stage==4?4:stage>=2?1:stage==1?3:2;
        pipelines[stage]=create_gpu_graphics_pipeline(device,&info);require(pipelines[stage],SDL_GetError());
    }
};
GpuCalibratedMsaa::GpuCalibratedMsaa():state_(std::make_unique<State>()) {}
GpuCalibratedMsaa::~GpuCalibratedMsaa()=default;
void GpuCalibratedMsaa::release_device() noexcept {state_.reset();}
const std::string& GpuCalibratedMsaa::status() const noexcept {
    static const std::string released{"Native MSAA transport released"};return state_?state_->status:released;
}
bool GpuCalibratedMsaa::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native MSAA transport requires a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        require(SDL_GetBooleanProperty(SDL_GetGPUDeviceProperties(s->device),STARFOX_SDL_MULTISAMPLE_READ,false),
            "Native MSAA transport requires the opted-in desktop shader-read capability");
        s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported native MSAA colour format");
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());
        s->status="Native MSAA sample transport ready";state_=std::move(s);return true;
    } catch(const std::exception& e) {
        const std::string error=e.what();s.reset();state_=std::make_unique<State>();state_->status=error;return false;
    }
}
bool GpuCalibratedMsaa::enqueue_sample(void* command,void* color,void* receiver,void* surfaces,
    void* sample_color,void* sample_receiver,void* sample_surfaces,unsigned w,unsigned h,unsigned count,unsigned index,
    void* motion,void* sample_motion) {
    if(!state_) return false;SDL_GPURenderPass* pass{};
    try {
        auto& s=*state_;require(s.device && command && color && receiver && sample_color && sample_receiver,
            "Incomplete native MSAA sample inputs");
        require(w && h && w<=16384 && h<=16384 && valid_count(count) && index<count,"Invalid native MSAA sample extent/index/count");
        require(bool(surfaces)==bool(sample_surfaces),"Native MSAA source/target surfaces must be supplied together");
        require(bool(motion)==bool(sample_motion) && (!motion || surfaces),"Native MSAA motion requires paired source/target motion and surfaces");
        const std::array inputs{color,receiver,surfaces,motion},outputs{sample_color,sample_receiver,sample_surfaces,sample_motion};
        for(unsigned i=0;i<4;++i) {
            if(inputs[i]) for(unsigned j=0;j<i;++j) require(inputs[i]!=inputs[j],"Aliased native MSAA source targets");
            if(outputs[i]) {
                for(unsigned j=0;j<i;++j) require(outputs[i]!=outputs[j],"Aliased native MSAA sample outputs");
                for(auto* source:inputs) require(outputs[i]!=source,"Native MSAA output aliases a source");
            }
        }
        const unsigned stage=motion?4:surfaces?1:0;s.prepare(stage);
        const unsigned target_count=motion?4:surfaces?3:2;SDL_GPUColorTargetInfo targets[4]{};
        for(unsigned i=0;i<target_count;++i) {targets[i].texture=static_cast<SDL_GPUTexture*>(outputs[i]);
            targets[i].load_op=SDL_GPU_LOADOP_DONT_CARE;targets[i].store_op=SDL_GPU_STOREOP_STORE;}
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);pass=SDL_BeginGPURenderPass(cmd,targets,target_count,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.pipelines[stage]);const SDL_GPUViewport viewport{0,0,float(w),float(h),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(w),int(h)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding bindings[]{{static_cast<SDL_GPUTexture*>(color),s.sampler},
            {static_cast<SDL_GPUTexture*>(receiver),s.sampler},{static_cast<SDL_GPUTexture*>(surfaces?surfaces:receiver),s.sampler},
            {static_cast<SDL_GPUTexture*>(motion),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,bindings,motion?4:3);const Uniform uniform{w,h,count,index};
        SDL_PushGPUFragmentUniformData(cmd,0,&uniform,sizeof(uniform));SDL_DrawGPUPrimitives(pass,3,1,0,0);
        SDL_EndGPURenderPass(pass);pass=nullptr;s.status="Exact native MSAA sample extracted";return true;
    } catch(const std::exception& e) {if(pass) SDL_EndGPURenderPass(pass);state_->status=e.what();return false;}
}
bool GpuCalibratedMsaa::enqueue_resolve(void* command,std::span<void* const> samples,void* destination,unsigned w,unsigned h,
    void* native_ink,void* native_ownership) {
    if(!state_) return false;SDL_GPURenderPass* pass{};
    try {
        auto& s=*state_;require(s.device && command && destination && valid_count(unsigned(samples.size()))
            && w && h && w<=16384 && h<=16384,"Invalid native MSAA resolve inputs");
        require(bool(native_ink)==bool(native_ownership) && (!native_ink || (native_ink!=destination
            && native_ownership!=destination && native_ownership!=native_ink)),"Invalid native MSAA ink/ownership targets");
        for(unsigned i=0;i<samples.size();++i) {
            require(samples[i] && samples[i]!=destination,"Null/aliased native MSAA resolve sample");
            for(unsigned j=0;j<i;++j) require(samples[i]!=samples[j],"Native MSAA resolve requires independent sample planes");
        }
        const unsigned stage=native_ink?3:2;s.prepare(stage);SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);pass=SDL_BeginGPURenderPass(cmd,&target,1,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.pipelines[stage]);const SDL_GPUViewport viewport{0,0,float(w),float(h),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(w),int(h)};SDL_SetGPUScissor(pass,&scissor);
        std::array<SDL_GPUTextureSamplerBinding,10> bindings{};
        for(unsigned i=0;i<8;++i) bindings[i]={static_cast<SDL_GPUTexture*>(samples[i<samples.size()?i:0]),s.sampler};
        if(native_ink) {bindings[8]={static_cast<SDL_GPUTexture*>(native_ink),s.sampler};bindings[9]={static_cast<SDL_GPUTexture*>(native_ownership),s.sampler};}
        SDL_BindGPUFragmentSamplers(pass,0,bindings.data(),native_ink?10:8);const Uniform uniform{w,h,unsigned(samples.size()),0};
        SDL_PushGPUFragmentUniformData(cmd,0,&uniform,sizeof(uniform));SDL_DrawGPUPrimitives(pass,3,1,0,0);
        SDL_EndGPURenderPass(pass);pass=nullptr;s.status="Native MSAA enhanced samples resolved in linear light";return true;
    } catch(const std::exception& e) {if(pass) SDL_EndGPURenderPass(pass);state_->status=e.what();return false;}
}
}
