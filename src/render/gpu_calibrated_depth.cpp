#include "starfox/render/gpu_calibrated_depth.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <cmath>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Uniform {
    unsigned width,height,scale,flags;
    unsigned modes;float focal_y;unsigned padding[2];
    float camera[4];
};
static_assert(sizeof(Uniform)==48);
}
struct GpuCalibratedDepth::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUShader *vertex{},*fragment{};SDL_GPUGraphicsPipeline* pipeline{};SDL_GPUSampler* sampler{};
    std::string status{"Native depth enhancements not initialized"};
    ~State() {
        if(!device) return;
        if(pipeline) SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
};
GpuCalibratedDepth::GpuCalibratedDepth():state_(std::make_unique<State>()) {}
GpuCalibratedDepth::~GpuCalibratedDepth()=default;
void GpuCalibratedDepth::release_device() noexcept {state_.reset();}
const std::string& GpuCalibratedDepth::status() const noexcept {
    static const std::string released{"Native depth enhancements released"};return state_?state_->status:released;
}
bool GpuCalibratedDepth::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native depth enhancements require a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported native depth enhancement format");
        const bool spirv=(SDL_GetGPUShaderFormats(s->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUShaderCreateInfo shader{};shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        s->vertex=create_gpu_shader(s->device,&shader);require(s->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="depth_fragment_main";shader.num_samplers=3;shader.num_uniform_buffers=1;
        shader.code=calibrated_scene_shader::depth_spirv;shader.code_size=sizeof(calibrated_scene_shader::depth_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::depth_dxil;shader.code_size=sizeof(calibrated_scene_shader::depth_dxil);}
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
        s->status="Native depth enhancements ready";state_=std::move(s);return true;
    } catch(const std::exception& error) {const std::string message=error.what();s.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedDepth::enqueue(void* command,void* source,void* ownership,void* surfaces,void* destination,
    unsigned width,unsigned height,unsigned scale,unsigned modes,const vr::EyeCamera& eye,float focus) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.pipeline && command && source && ownership && surfaces && destination
            && source!=ownership && source!=surfaces && source!=destination && ownership!=surfaces
            && ownership!=destination && surfaces!=destination,"Invalid native depth enhancement inputs");
        require(width && height && width<=32768 && height<=32768 && scale && scale<=32768
            && modes<=15 && std::isfinite(focus) && focus>0 && focus<=1.e8F,"Invalid native depth enhancement settings");
        const auto& p=eye.projection;
        for(float value:p) require(std::isfinite(value),"Nonfinite native depth projection");
        require(p[0]>0 && p[5]>0 && p[1]==0 && p[2]==0 && p[3]==0 && p[4]==0 && p[6]==0
            && p[7]==0 && p[11]==-1 && p[12]==0 && p[13]==0 && p[15]==0 && p[10]<=-1 && p[14]<0,
            "Unsupported native depth perspective projection");
        // Surface samples are indexed at integer pixels; their corresponding
        // raster rays are through pixel centres, not their top-left corners.
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const Uniform uniform{width,height,scale,unsigned(srgb),modes,float(height)*p[5]/2,{},
            {float(width)*(1-p[8])/2-.5F,float(height)*(1+p[9])/2-.5F,float(width)*p[0]/2,focus}};
        SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        auto* pass=SDL_BeginGPURenderPass(cmd,&target,1,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.pipeline);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},
            {static_cast<SDL_GPUTexture*>(ownership),s.sampler},{static_cast<SDL_GPUTexture*>(surfaces),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,3);SDL_PushGPUFragmentUniformData(cmd,0,&uniform,sizeof(uniform));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.status="Native depth enhancements encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
}
