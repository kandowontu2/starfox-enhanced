#include "starfox/render/gpu_calibrated_global.hpp"
#include "starfox/render/global_enhancements.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <cmath>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Uniform {unsigned width,height,scale,flags;unsigned selections;float seconds;unsigned padding[2];};
static_assert(sizeof(Uniform)==32);
}
struct GpuCalibratedGlobal::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUShader *vertex{},*fragment{};SDL_GPUGraphicsPipeline* pipeline{};SDL_GPUSampler* sampler{};
    std::string status{"Native global enhancements not initialized"};
    ~State() {
        if(!device) return;
        if(pipeline) SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
};
GpuCalibratedGlobal::GpuCalibratedGlobal():state_(std::make_unique<State>()) {}
GpuCalibratedGlobal::~GpuCalibratedGlobal()=default;
void GpuCalibratedGlobal::release_device() noexcept {state_.reset();}
const std::string& GpuCalibratedGlobal::status() const noexcept {
    static const std::string released{"Native global enhancements released"};return state_?state_->status:released;
}
bool GpuCalibratedGlobal::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native global enhancements require a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported native global enhancement format");
        const bool spirv=(SDL_GetGPUShaderFormats(s->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUShaderCreateInfo shader{};shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        s->vertex=create_gpu_shader(s->device,&shader);require(s->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="global_fragment_main";shader.num_samplers=2;shader.num_uniform_buffers=1;
        shader.code=calibrated_scene_shader::global_spirv;shader.code_size=sizeof(calibrated_scene_shader::global_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::global_dxil;shader.code_size=sizeof(calibrated_scene_shader::global_dxil);}
#endif
        s->fragment=create_gpu_shader(s->device,&shader);require(s->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription target{};target.format=s->format;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=s->vertex;pipeline.fragment_shader=s->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=&target;pipeline.target_info.num_color_targets=1;
        s->pipeline=create_gpu_graphics_pipeline(s->device,&pipeline);require(s->pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());
        s->status="Native global enhancements ready";state_=std::move(s);return true;
    } catch(const std::exception& error) {const std::string message=error.what();s.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedGlobal::enqueue(void* command,void* source,void* ownership,void* destination,
    unsigned width,unsigned height,unsigned scale,std::uint32_t selections,float seconds) {
    return enqueue_impl(command,source,ownership,destination,width,height,scale,selections,seconds,0,0,false);
}
bool GpuCalibratedGlobal::enqueue_appearance(void* command,void* source,void* ownership,void* destination,
    unsigned width,unsigned height,unsigned scale,unsigned contrast,unsigned chromatic) {
    return enqueue_impl(command,source,ownership,destination,width,height,scale,0,0,contrast,chromatic,true);
}
bool GpuCalibratedGlobal::enqueue_impl(void* command,void* source,void* ownership,void* destination,
    unsigned width,unsigned height,unsigned scale,std::uint32_t selections,float seconds,
    unsigned contrast,unsigned chromatic,bool appearance) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.pipeline && command && source && ownership && destination && source!=ownership
            && source!=destination && ownership!=destination,"Invalid native global enhancement inputs");
        require(width && height && width<=32768 && height<=32768 && scale && scale<=32768
            && (selections&~global_enhancement_mask)==0 && std::isfinite(seconds) && seconds>=0
            && contrast<=3 && chromatic<=3,
            "Invalid native global enhancement settings");
        SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* pass=SDL_BeginGPURenderPass(static_cast<SDL_GPUCommandBuffer*>(command),&target,1,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.pipeline);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},
            {static_cast<SDL_GPUTexture*>(ownership),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,2);
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const Uniform uniform{width,height,scale,(srgb?1U:0U)|(appearance?2U:0U),selections,seconds,{contrast,chromatic}};
        SDL_PushGPUFragmentUniformData(static_cast<SDL_GPUCommandBuffer*>(command),0,&uniform,sizeof(uniform));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.status="Native global enhancements encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
}
