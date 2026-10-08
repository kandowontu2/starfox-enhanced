#include "starfox/render/gpu_calibrated_persistence.hpp"
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
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Uniform {unsigned width,height,selection,flags;float decay,intensity;unsigned padding[2];};
static_assert(sizeof(Uniform)==32);
}
struct GpuCalibratedPersistence::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUShader *vertex{},*fragment{};SDL_GPUGraphicsPipeline* pipeline{};SDL_GPUSampler* sampler{};
    struct Images {unsigned width,height;std::array<SDL_GPUTexture*,2> textures{};};
    std::vector<Images> images;
    CalibratedPersistenceSettings accepted{},pending_settings{};
    unsigned width{},height{},index{},pending_image{};bool valid{},pending{};
    std::string status{"Calibrated persistence not initialized"};
    ~State() {
        if(!device) return;
        if(pipeline) SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
        for(const auto& image:images) for(auto* t:image.textures) if(t) SDL_ReleaseGPUTexture(device,t);
    }
    unsigned image_pair(unsigned w,unsigned h) {
        for(unsigned i=0;i<images.size();++i) if(images[i].width==w && images[i].height==h) return i;
        Images next{w,h};
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        info.width=w;info.height=h;info.layer_count_or_depth=info.num_levels=1;
        for(auto& texture:next.textures) {
            texture=SDL_CreateGPUTexture(device,&info);
            if(!texture) {for(auto* t:next.textures) if(t) SDL_ReleaseGPUTexture(device,t);throw std::runtime_error(SDL_GetError());}
        }
        try {images.push_back(next);} catch(...) {for(auto* t:next.textures) SDL_ReleaseGPUTexture(device,t);throw;}
        return unsigned(images.size()-1);
    }
};
GpuCalibratedPersistence::GpuCalibratedPersistence():state_(std::make_unique<State>()) {}
GpuCalibratedPersistence::~GpuCalibratedPersistence()=default;
void GpuCalibratedPersistence::release_device() noexcept {state_.reset();}
void GpuCalibratedPersistence::reset() noexcept {if(state_) state_->valid=false;}
void GpuCalibratedPersistence::cancel() noexcept {if(state_) state_->pending=false;}
void GpuCalibratedPersistence::discard() noexcept {if(state_) {state_->pending=false;state_->valid=false;}}
void GpuCalibratedPersistence::commit() noexcept {
    if(!state_ || !state_->pending) return;
    auto& s=*state_;s.accepted=s.pending_settings;s.index=1-s.index;s.valid=true;s.pending=false;
    s.width=s.images[s.pending_image].width;s.height=s.images[s.pending_image].height;
}
const std::string& GpuCalibratedPersistence::status() const noexcept {
    static const std::string released{"Calibrated persistence released"};return state_?state_->status:released;
}
std::uint64_t GpuCalibratedPersistence::working_image_bytes() const noexcept {
    std::uint64_t bytes=0;
    if(state_) for(const auto& images:state_->images)
        for(auto* texture:images.textures) if(texture) bytes+=std::uint64_t(images.width)*images.height*16;
    return bytes;
}
bool GpuCalibratedPersistence::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native persistence requires a GPU");s->device=static_cast<SDL_GPUDevice*>(device);s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported native persistence format");
        const bool spirv=(SDL_GetGPUShaderFormats(s->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUShaderCreateInfo shader{};shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        s->vertex=create_gpu_shader(s->device,&shader);require(s->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="persistence_fragment_main";shader.num_samplers=3;shader.num_uniform_buffers=1;
        shader.code=calibrated_scene_shader::persistence_spirv;shader.code_size=sizeof(calibrated_scene_shader::persistence_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::persistence_dxil;shader.code_size=sizeof(calibrated_scene_shader::persistence_dxil);}
#endif
        s->fragment=create_gpu_shader(s->device,&shader);require(s->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription targets[2]{};targets[0].format=s->format;targets[1].format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=s->vertex;pipeline.fragment_shader=s->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=targets;pipeline.target_info.num_color_targets=2;
        s->pipeline=create_gpu_graphics_pipeline(s->device,&pipeline);require(s->pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());
        s->status="Native per-eye persistence ready";state_=std::move(s);return true;
    } catch(const std::exception& error) {const std::string message=error.what();s.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedPersistence::enqueue(void* command,void* source,void* ownership,void* destination,
    unsigned width,unsigned height,const CalibratedPersistenceSettings& settings) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.pipeline && !s.pending && command && source && ownership && destination
            && source!=ownership && source!=destination && ownership!=destination,"Invalid/pending native persistence inputs");
        require(width && height && width<=32768 && height<=32768 && settings.scale && settings.scale<=32768
            && settings.mode>=1 && settings.mode<=3 && settings.intensity<=100
            && settings.quality>=1 && settings.quality<=3
            && settings.intensity && (settings.models || settings.world)
            && std::isfinite(settings.seconds) && settings.seconds>=0,"Invalid native persistence settings");
        const auto& old=s.accepted;
        const bool reset=!s.valid || s.width!=width || s.height!=height || settings.epoch!=old.epoch || settings.scale!=old.scale
            || settings.mode!=old.mode || settings.models!=old.models || settings.world!=old.world
            || (settings.mode==3 && settings.quality!=old.quality)
            || settings.seconds<old.seconds || settings.seconds-old.seconds>1;
        const float decay=reset?0.F:settings.mode==2?1.F:
            float(std::exp2(-(settings.seconds-old.seconds)/(settings.mode==3?.03*settings.quality:.35)));
        const auto pair=s.image_pair(width,height);auto& images=s.images[pair].textures;
        SDL_GPUColorTargetInfo targets[2]{};targets[0].texture=static_cast<SDL_GPUTexture*>(destination);targets[1].texture=images[1-s.index];
        for(auto& t:targets) {t.load_op=SDL_GPU_LOADOP_DONT_CARE;t.store_op=SDL_GPU_STOREOP_STORE;}
        auto* pass=SDL_BeginGPURenderPass(static_cast<SDL_GPUCommandBuffer*>(command),targets,2,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.pipeline);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},
            {static_cast<SDL_GPUTexture*>(ownership),s.sampler},{images[s.index],s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,3);
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const Uniform uniform{width,height,(settings.models?1U:0U)|(settings.world?2U:0U),
            (srgb?1U:0U)|(reset?2U:0U)|(settings.mode==3?4U:0U),decay,float(settings.intensity),{}};
        SDL_PushGPUFragmentUniformData(static_cast<SDL_GPUCommandBuffer*>(command),0,&uniform,sizeof(uniform));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.pending=true;s.pending_settings=settings;s.pending_image=pair;s.status="Native persistence encoded; awaiting presentation";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
}
