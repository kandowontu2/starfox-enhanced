#include "starfox/render/gpu_calibrated_bloom.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <array>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool value,const char* error) {if(!value) throw std::runtime_error(error);}
struct Uniform {unsigned width,height,step,flags,reduced_width,reduced_height,model,world,stage,padding[3];};
static_assert(sizeof(Uniform)==48);
}
struct GpuCalibratedBloom::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUShader *vertex{},*fragment{};SDL_GPUGraphicsPipeline *float_pipeline{},*compose_pipeline{};
    SDL_GPUSampler* sampler{};std::array<SDL_GPUTexture*,4> images{};
    unsigned reduced_width{},reduced_height{};std::string status{"Native bloom not initialized"};
    void release_images() {
        for(auto& image:images) {if(image) SDL_ReleaseGPUTexture(device,image);image=nullptr;}
        reduced_width=reduced_height=0;
    }
    void prepare(unsigned width,unsigned height) {
        if(reduced_width==width && reduced_height==height) return;
        release_images();
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
        info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        for(auto& image:images) {image=SDL_CreateGPUTexture(device,&info);require(image,SDL_GetError());}
        reduced_width=width;reduced_height=height;
    }
    ~State() {
        if(!device) return;
        release_images();
        if(float_pipeline) SDL_ReleaseGPUGraphicsPipeline(device,float_pipeline);
        if(compose_pipeline) SDL_ReleaseGPUGraphicsPipeline(device,compose_pipeline);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
};
GpuCalibratedBloom::GpuCalibratedBloom():state_(std::make_unique<State>()) {}
GpuCalibratedBloom::~GpuCalibratedBloom()=default;
void GpuCalibratedBloom::release_device() noexcept {state_.reset();}
const std::string& GpuCalibratedBloom::status() const noexcept {
    static const std::string released{"Native bloom released"};return state_?state_->status:released;
}
bool GpuCalibratedBloom::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native bloom requires a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported native bloom format");
        const bool spirv=(SDL_GetGPUShaderFormats(s->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUShaderCreateInfo shader{};shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        s->vertex=create_gpu_shader(s->device,&shader);require(s->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="bloom_fragment_main";shader.num_samplers=4;shader.num_uniform_buffers=1;
        shader.code=calibrated_scene_shader::bloom_spirv;shader.code_size=sizeof(calibrated_scene_shader::bloom_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::bloom_dxil;shader.code_size=sizeof(calibrated_scene_shader::bloom_dxil);}
#endif
        s->fragment=create_gpu_shader(s->device,&shader);require(s->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription target{};
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=s->vertex;pipeline.fragment_shader=s->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=&target;pipeline.target_info.num_color_targets=1;
        target.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        s->float_pipeline=create_gpu_graphics_pipeline(s->device,&pipeline);require(s->float_pipeline,SDL_GetError());
        target.format=s->format;s->compose_pipeline=create_gpu_graphics_pipeline(s->device,&pipeline);require(s->compose_pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());
        s->status="Native bloom ready";state_=std::move(s);return true;
    } catch(const std::exception& error) {const std::string message=error.what();s.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedBloom::enqueue(void* command,void* source,void* ownership,void* destination,
    unsigned width,unsigned height,unsigned scale,unsigned model,unsigned world) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.compose_pipeline && command && source && ownership && destination && source!=ownership
            && source!=destination && ownership!=destination,"Invalid native bloom inputs");
        require(width && height && width<=32768 && height<=32768 && scale && scale<=32768 && model<=3 && world<=3,
            "Invalid native bloom settings");
        const unsigned step=2*scale,w=(width+step-1)/step,h=(height+step-1)/step;
        const bool active=model || world;
        if(active) s.prepare(w,h);
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const auto encode=[&](unsigned stage,SDL_GPUTexture* input,SDL_GPUTexture* target_image) {
            const bool compose=stage==5;const unsigned tw=compose?width:w,th=compose?height:h;
            SDL_GPUColorTargetInfo target{};target.texture=target_image;
            target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
            auto* pass=SDL_BeginGPURenderPass(static_cast<SDL_GPUCommandBuffer*>(command),&target,1,nullptr);require(pass,SDL_GetError());
            SDL_BindGPUGraphicsPipeline(pass,compose?s.compose_pipeline:s.float_pipeline);
            const SDL_GPUViewport viewport{0,0,float(tw),float(th),0,1};SDL_SetGPUViewport(pass,&viewport);
            const SDL_Rect scissor{0,0,int(tw),int(th)};SDL_SetGPUScissor(pass,&scissor);
            // Unused slots still use distinct, readable images: never bind a
            // render target as a sampler, even in an unexecuted shader branch.
            auto* read_core=compose && active?s.images[2]:input;
            auto* read_halo=compose && active?s.images[3]:input;
            const SDL_GPUTextureSamplerBinding textures[]{{input,s.sampler},{static_cast<SDL_GPUTexture*>(ownership),s.sampler},
                {read_core,s.sampler},{read_halo,s.sampler}};
            SDL_BindGPUFragmentSamplers(pass,0,textures,4);
            const Uniform uniform{width,height,step,srgb?1U:0U,w,h,model,world,stage,{}};
            SDL_PushGPUFragmentUniformData(static_cast<SDL_GPUCommandBuffer*>(command),0,&uniform,sizeof(uniform));
            SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        };
        if(active) {
            encode(0,static_cast<SDL_GPUTexture*>(source),s.images[0]);
            encode(1,s.images[0],s.images[1]);encode(2,s.images[1],s.images[2]);
            encode(3,s.images[2],s.images[1]);encode(4,s.images[1],s.images[3]);
        }
        encode(5,static_cast<SDL_GPUTexture*>(source),static_cast<SDL_GPUTexture*>(destination));
        s.status="Native bloom encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
}
