#include "starfox/render/gpu_calibrated_exposure.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace starfox::render {
namespace {
void require(bool ok,const char* error) {if(!ok) throw std::runtime_error(error);}
struct Uniform {unsigned width,height,quality,flags;float delta;unsigned padding[3];};
static_assert(sizeof(Uniform)==32);
}
struct GpuCalibratedExposure::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUComputePipeline *meter{},*reduce{};SDL_GPUShader *vertex{},*fragment{};
    SDL_GPUGraphicsPipeline* apply{};SDL_GPUSampler* sampler{};
    std::array<SDL_GPUTexture*,2> stops{};
    struct Histogram {unsigned width,height;SDL_GPUTexture* texture;};std::vector<Histogram> histograms;
    CalibratedExposureSettings accepted{},pending_settings{};
    unsigned width{},height{},pending_width{},pending_height{},index{};bool valid{},pending{};
    std::string status{"Native adaptive exposure not initialized"};
    ~State() {
        if(!device) return;
        if(meter) SDL_ReleaseGPUComputePipeline(device,meter);if(reduce) SDL_ReleaseGPUComputePipeline(device,reduce);
        if(apply) SDL_ReleaseGPUGraphicsPipeline(device,apply);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
        for(auto* t:stops) if(t) SDL_ReleaseGPUTexture(device,t);
        for(auto h:histograms) SDL_ReleaseGPUTexture(device,h.texture);
    }
    SDL_GPUTexture* histogram(unsigned w,unsigned h) {
        for(auto old:histograms) if(old.width==w && old.height==h) return old.texture;
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R32_UINT;
        const unsigned tiles=((w+31)/32)*((h+31)/32);
        info.width=64*((tiles+4095)/4096);info.height=std::min(tiles,4096U);info.layer_count_or_depth=info.num_levels=1;
        info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
        require(SDL_GPUTextureSupportsFormat(device,info.format,info.type,info.usage),"Native exposure histogram storage format unsupported");
        auto* next=SDL_CreateGPUTexture(device,&info);require(next,SDL_GetError());
        try {histograms.push_back({w,h,next});} catch(...) {SDL_ReleaseGPUTexture(device,next);throw;}return next;
    }
};
GpuCalibratedExposure::GpuCalibratedExposure():state_(std::make_unique<State>()) {}
GpuCalibratedExposure::~GpuCalibratedExposure()=default;
void GpuCalibratedExposure::release_device() noexcept {state_.reset();}
void GpuCalibratedExposure::discard() noexcept {if(state_) {state_->pending=false;state_->valid=false;}}
void GpuCalibratedExposure::cancel() noexcept {if(state_) state_->pending=false;}
void GpuCalibratedExposure::commit() noexcept {
    if(!state_ || !state_->pending) return;
    auto& s=*state_;s.accepted=s.pending_settings;s.width=s.pending_width;s.height=s.pending_height;
    s.index=1-s.index;s.valid=true;s.pending=false;
}
const std::string& GpuCalibratedExposure::status() const noexcept {
    static const std::string released{"Native adaptive exposure released"};return state_?state_->status:released;
}
std::uint64_t GpuCalibratedExposure::working_image_bytes() const noexcept {
    std::uint64_t bytes=0;
    if(state_) {
        for(auto* stop:state_->stops) if(stop) bytes+=4;
        for(const auto& histogram:state_->histograms) {
            const auto tiles=((histogram.width+31)/32)*((histogram.height+31)/32);
            bytes+=std::uint64_t(64*((tiles+4095)/4096))*std::min(tiles,4096U)*4;
        }
    }
    return bytes;
}
bool GpuCalibratedExposure::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native adaptive exposure requires a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,"Unsupported native exposure format");
        const bool spirv=(SDL_GetGPUShaderFormats(s->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUComputePipelineCreateInfo compute{};compute.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        compute.num_samplers=2;compute.num_readwrite_storage_textures=1;compute.num_uniform_buffers=1;
        compute.threadcount_x=compute.threadcount_y=8;compute.threadcount_z=1;
        compute.entrypoint="exposure_meter_main";compute.code=calibrated_scene_shader::exposure_meter_spirv;compute.code_size=sizeof(calibrated_scene_shader::exposure_meter_spirv);
#if defined(_WIN32)
        if(!spirv) {compute.code=calibrated_scene_shader::exposure_meter_dxil;compute.code_size=sizeof(calibrated_scene_shader::exposure_meter_dxil);}
#endif
        s->meter=create_gpu_compute_pipeline(s->device,&compute);require(s->meter,SDL_GetError());
        compute.num_samplers=1;compute.num_readonly_storage_textures=1;
        compute.entrypoint="exposure_reduce_main";compute.code=calibrated_scene_shader::exposure_reduce_spirv;compute.code_size=sizeof(calibrated_scene_shader::exposure_reduce_spirv);
#if defined(_WIN32)
        if(!spirv) {compute.code=calibrated_scene_shader::exposure_reduce_dxil;compute.code_size=sizeof(calibrated_scene_shader::exposure_reduce_dxil);}
#endif
        s->reduce=create_gpu_compute_pipeline(s->device,&compute);require(s->reduce,SDL_GetError());
        SDL_GPUShaderCreateInfo shader{};shader.format=compute.format;shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        s->vertex=create_gpu_shader(s->device,&shader);require(s->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="exposure_apply_fragment_main";shader.num_samplers=3;shader.num_uniform_buffers=1;
        shader.code=calibrated_scene_shader::exposure_apply_spirv;shader.code_size=sizeof(calibrated_scene_shader::exposure_apply_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::exposure_apply_dxil;shader.code_size=sizeof(calibrated_scene_shader::exposure_apply_dxil);}
#endif
        s->fragment=create_gpu_shader(s->device,&shader);require(s->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription target{};target.format=s->format;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=s->vertex;pipeline.fragment_shader=s->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=&target;pipeline.target_info.num_color_targets=1;
        s->apply=create_gpu_graphics_pipeline(s->device,&pipeline);require(s->apply,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R32_FLOAT;info.width=info.height=info.layer_count_or_depth=info.num_levels=1;
        info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
        require(SDL_GPUTextureSupportsFormat(s->device,info.format,info.type,info.usage),"Native exposure stops format unsupported");
        for(auto& t:s->stops) {t=SDL_CreateGPUTexture(s->device,&info);require(t,SDL_GetError());}
        s->status="Native adaptive exposure ready";state_=std::move(s);return true;
    } catch(const std::exception& error) {const std::string message=error.what();s.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedExposure::enqueue(void* command,void* source,void* ownership,void* destination,
    unsigned width,unsigned height,const CalibratedExposureSettings& settings) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.apply && !s.pending && command && source && ownership && destination
            && source!=ownership && source!=destination && ownership!=destination,"Invalid/pending native exposure inputs");
        require(width && height && width<=16384 && height<=16384
            && settings.quality>=1 && settings.quality<=3 && std::isfinite(settings.seconds) && settings.seconds>=0,"Invalid native exposure settings");
        const bool reset=!s.valid || s.width!=width || s.height!=height || s.accepted.quality!=settings.quality
            || s.accepted.epoch!=settings.epoch || settings.seconds<s.accepted.seconds || settings.seconds-s.accepted.seconds>1;
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const Uniform uniform{width,height,settings.quality,(srgb?1U:0U)|(reset?2U:0U),reset || settings.paused?0.F:settings.seconds-s.accepted.seconds,{}};
        auto* hist=s.histogram(width,height);auto* buffer=static_cast<SDL_GPUCommandBuffer*>(command);
        const auto dispatch=[&](SDL_GPUComputePipeline* pipeline,SDL_GPUTexture* output,
            const std::array<SDL_GPUTextureSamplerBinding,2>& inputs,unsigned x,unsigned y) {
            const SDL_GPUStorageTextureReadWriteBinding target{output,0,0,false};
            auto* pass=SDL_BeginGPUComputePass(buffer,&target,1,nullptr,0);require(pass,SDL_GetError());
            SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeSamplers(pass,0,inputs.data(),inputs.size());
            SDL_PushGPUComputeUniformData(buffer,0,&uniform,sizeof(uniform));SDL_DispatchGPUCompute(pass,x,y,1);SDL_EndGPUComputePass(pass);
        };
        dispatch(s.meter,hist,{{{static_cast<SDL_GPUTexture*>(source),s.sampler},{static_cast<SDL_GPUTexture*>(ownership),s.sampler}}},(width+31)/32,(height+31)/32);
        {
            const SDL_GPUStorageTextureReadWriteBinding output{s.stops[1-s.index],0,0,false};
            auto* pass=SDL_BeginGPUComputePass(buffer,&output,1,nullptr,0);require(pass,SDL_GetError());
            SDL_BindGPUComputePipeline(pass,s.reduce);
            const SDL_GPUTextureSamplerBinding accepted{s.stops[s.index],s.sampler};
            SDL_BindGPUComputeSamplers(pass,0,&accepted,1);SDL_BindGPUComputeStorageTextures(pass,0,&hist,1);
            SDL_PushGPUComputeUniformData(buffer,0,&uniform,sizeof(uniform));SDL_DispatchGPUCompute(pass,1,1,1);SDL_EndGPUComputePass(pass);
        }
        SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* pass=SDL_BeginGPURenderPass(buffer,&target,1,nullptr);require(pass,SDL_GetError());SDL_BindGPUGraphicsPipeline(pass,s.apply);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding inputs[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},{static_cast<SDL_GPUTexture*>(ownership),s.sampler},{s.stops[1-s.index],s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,inputs,3);SDL_PushGPUFragmentUniformData(buffer,0,&uniform,sizeof(uniform));SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.pending=true;s.pending_settings=settings;s.pending_width=width;s.pending_height=height;s.status="Native adaptive exposure encoded; awaiting presentation";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
}
