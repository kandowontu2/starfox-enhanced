#include "starfox/render/gpu_calibrated_temporal_aa.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Uniform {
    unsigned width,height,flags,rejected_layers;float weight;unsigned pattern_scale,edge_masks[2];
    float jitter[2];unsigned padding3[2];std::array<CalibratedPatternPass,3> patterns;
};
static_assert(sizeof(CalibratedPatternPass)==16 && sizeof(Uniform)==96);
static_assert(unsigned(Effect::dithered)==5 && unsigned(Effect::night_vision)==10
    && unsigned(Effect::scanlines)==25 && unsigned(Effect::crt_phosphor)==70);
unsigned pattern_flag(const CalibratedPatternGuide& patterns) noexcept {
    return std::any_of(patterns.passes.begin(),patterns.passes.end(),[](const auto& p){return p.world || p.model;})?8U:0U;
}
constexpr std::uint64_t history_budget=1024ULL*1024*1024;
}
struct GpuCalibratedTemporalAa::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUShader *vertex{},*fragment{};SDL_GPUGraphicsPipeline* pipeline{};SDL_GPUSampler* sampler{};
    SDL_GPUShader* present_fragment{};SDL_GPUGraphicsPipeline* present_pipeline{};
    SDL_GPUBuffer* absent_liquid{};
    struct Images {
        SDL_GPUDevice* device;unsigned width,height;
        std::array<SDL_GPUTexture*,2> colour{},metadata{};
        ~Images() {for(auto* t:{colour[0],colour[1],metadata[0],metadata[1]}) if(t) SDL_ReleaseGPUTexture(device,t);}
        std::uint64_t bytes() const noexcept {return std::uint64_t(width)*height*40;}
    };
    std::unique_ptr<Images> accepted_images,pending_images;
    CalibratedTemporalAaSettings accepted{},pending_settings{};
    unsigned index{},pending_index{};bool valid{},pending{},pending_reset{};
    std::string status{"Native TAA resolve not initialized"};
    ~State() {
        if(!device) return;
        pending_images.reset();accepted_images.reset();
        if(pipeline) SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
        if(present_pipeline) SDL_ReleaseGPUGraphicsPipeline(device,present_pipeline);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(present_fragment) SDL_ReleaseGPUShader(device,present_fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
        if(absent_liquid) SDL_ReleaseGPUBuffer(device,absent_liquid);
    }
    std::unique_ptr<Images> make_images(unsigned w,unsigned h) {
        auto next=std::make_unique<Images>();next->device=device;next->width=w;next->height=h;
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.width=w;info.height=h;
        info.layer_count_or_depth=info.num_levels=1;
        info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.format=format;
        for(auto& t:next->colour) {t=SDL_CreateGPUTexture(device,&info);require(t,SDL_GetError());}
        info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        for(auto& t:next->metadata) {t=SDL_CreateGPUTexture(device,&info);require(t,SDL_GetError());}
        return next;
    }
};
GpuCalibratedTemporalAa::GpuCalibratedTemporalAa():state_(std::make_unique<State>()) {}
GpuCalibratedTemporalAa::~GpuCalibratedTemporalAa()=default;
void GpuCalibratedTemporalAa::release_device() noexcept {state_.reset();}
void GpuCalibratedTemporalAa::reset() noexcept {
    if(state_) {state_->valid=false;if(state_->pending) state_->pending_reset=true;}
}
void GpuCalibratedTemporalAa::discard() noexcept {
    if(state_) {state_->pending=false;state_->pending_reset=false;state_->pending_images.reset();}
}
void GpuCalibratedTemporalAa::commit() noexcept {
    if(!state_ || !state_->pending) return;
    auto& s=*state_;
    if(s.pending_reset) {discard();return;} // A reset during a GPU/XR wait cannot be undone by old work.
    if(s.pending_images) s.accepted_images=std::move(s.pending_images);
    s.index=s.pending_index;s.accepted=s.pending_settings;s.valid=true;s.pending=false;
}
void* GpuCalibratedTemporalAa::accepted_colour(unsigned w,unsigned h,std::uint64_t epoch) const noexcept {
    if(!state_ || !state_->valid || state_->pending || !state_->accepted_images
        || state_->accepted.epoch!=epoch || state_->accepted_images->width!=w || state_->accepted_images->height!=h) return nullptr;
    return state_->accepted_images->colour[state_->index];
}
const std::string& GpuCalibratedTemporalAa::status() const noexcept {
    static const std::string released{"Native TAA resolve released"};return state_?state_->status:released;
}
bool GpuCalibratedTemporalAa::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native TAA resolve requires a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported native TAA colour format");
        const bool spirv=(SDL_GetGPUShaderFormats(s->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUShaderCreateInfo shader{};shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        s->vertex=create_gpu_shader(s->device,&shader);require(s->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="temporal_aa_fragment_main";
        shader.num_samplers=8;shader.num_uniform_buffers=shader.num_storage_buffers=1;
        shader.code=calibrated_scene_shader::temporal_aa_spirv;shader.code_size=sizeof(calibrated_scene_shader::temporal_aa_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::temporal_aa_dxil;shader.code_size=sizeof(calibrated_scene_shader::temporal_aa_dxil);}
#endif
        s->fragment=create_gpu_shader(s->device,&shader);require(s->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription targets[3]{};targets[0].format=targets[1].format=s->format;
        targets[2].format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=s->vertex;pipeline.fragment_shader=s->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=targets;pipeline.target_info.num_color_targets=3;
        s->pipeline=create_gpu_graphics_pipeline(s->device,&pipeline);require(s->pipeline,SDL_GetError());
        shader.entrypoint="temporal_present_fragment_main";shader.num_storage_buffers=0;
        shader.code=calibrated_scene_shader::temporal_present_spirv;shader.code_size=sizeof(calibrated_scene_shader::temporal_present_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::temporal_present_dxil;shader.code_size=sizeof(calibrated_scene_shader::temporal_present_dxil);}
#endif
        s->present_fragment=create_gpu_shader(s->device,&shader);require(s->present_fragment,SDL_GetError());
        pipeline.fragment_shader=s->present_fragment;pipeline.target_info.num_color_targets=1;
        s->present_pipeline=create_gpu_graphics_pipeline(s->device,&pipeline);require(s->present_pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());
        // Mandatory binding when liquid is absent. The disabled shader branch
        // never loads it, so no initialization upload/submission is needed.
        const SDL_GPUBufferCreateInfo empty{SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,4,0};
        s->absent_liquid=SDL_CreateGPUBuffer(s->device,&empty);require(s->absent_liquid,SDL_GetError());
        s->status="Native per-eye TAA resolve ready";state_=std::move(s);return true;
    } catch(const std::exception& e) {const std::string message=e.what();s.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedTemporalAa::enqueue(void* command,void* source,void* ownership,void* surface,
    void* motion,void* destination,unsigned w,unsigned h,const CalibratedTemporalAaSettings& settings,
    const shadows::GpuReflectionOutput* liquid,void* edge_witness) {
    if(!state_) return false;
    try {
        auto& s=*state_;require(s.pipeline && !s.pending && command,"Invalid/pending native TAA command");
        const std::array<void*,5> inputs{source,ownership,surface,motion,destination};
        for(unsigned i=0;i<inputs.size();++i) {
            require(inputs[i],"Missing native TAA image");
            for(unsigned j=i+1;j<inputs.size();++j) require(inputs[i]!=inputs[j],"Aliased native TAA images");
            if(s.accepted_images) for(auto* t:{s.accepted_images->colour[0],s.accepted_images->colour[1],
                s.accepted_images->metadata[0],s.accepted_images->metadata[1]}) require(inputs[i]!=t,"Aliased native TAA history");
        }
        require(w && h && w<=16384 && h<=16384 && std::isfinite(settings.weight)
            && settings.rejected_layers<=3 && settings.patterns.valid() && settings.edges.valid(),"Invalid native TAA settings");
        require(settings.edges.active()==bool(edge_witness),"Missing/unselected native TAA edge witness");
        if(edge_witness) {
            for(auto* input:inputs) require(edge_witness!=input,"Aliased native TAA edge witness");
            if(s.accepted_images) for(auto* t:{s.accepted_images->colour[0],s.accepted_images->colour[1],
                s.accepted_images->metadata[0],s.accepted_images->metadata[1]}) require(edge_witness!=t,"Aliased native TAA edge history");
        }
        if(liquid) {
            const auto layout=shadows::native_water_layers(w,h,liquid->water_layers.world_offset!=0);
            require(layout && liquid->device==s.device && liquid->buffer && liquid->width==w && liquid->height==h
                && liquid->row_bytes==w*4 && (liquid->water_layers==shadows::NativeWaterLayers{}
                    || liquid->water_layers==*layout),"Invalid native TAA ray device/layout/extent");
        }
        const bool same_extent=s.accepted_images && s.accepted_images->width==w && s.accepted_images->height==h;
        const std::uint64_t bytes=std::uint64_t(w)*h*40;
        require(bytes<=history_budget && (same_extent || !s.accepted_images || bytes<=history_budget-s.accepted_images->bytes()),
            "Native TAA history exceeds the one-GiB working bound (not a free-VRAM guarantee)");
        auto next=same_extent?std::unique_ptr<State::Images>{}:s.make_images(w,h);
        auto& images=*(next?next:s.accepted_images);
        const bool reuse=s.valid && same_extent && settings.epoch==s.accepted.epoch
            && settings.patterns==s.accepted.patterns && settings.edges==s.accepted.edges
            && settings.model_reflections==s.accepted.model_reflections;
        const unsigned prior=same_extent?s.index:0,write=1-prior;
        SDL_GPUColorTargetInfo targets[3]{};targets[0].texture=static_cast<SDL_GPUTexture*>(destination);
        targets[1].texture=images.colour[write];targets[2].texture=images.metadata[write];
        for(auto& t:targets) {t.load_op=SDL_GPU_LOADOP_DONT_CARE;t.store_op=SDL_GPU_STOREOP_STORE;}
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        auto* pass=SDL_BeginGPURenderPass(cb,targets,3,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.pipeline);
        const SDL_GPUViewport viewport{0,0,float(w),float(h),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(w),int(h)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},
            {static_cast<SDL_GPUTexture*>(ownership),s.sampler},{static_cast<SDL_GPUTexture*>(surface),s.sampler},
            {static_cast<SDL_GPUTexture*>(motion),s.sampler},{images.colour[prior],s.sampler},{images.metadata[prior],s.sampler},
            {static_cast<SDL_GPUTexture*>(edge_witness?edge_witness:ownership),s.sampler},
            {static_cast<SDL_GPUTexture*>(edge_witness?edge_witness:ownership),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,8);
        auto* buffer=liquid?static_cast<SDL_GPUBuffer*>(liquid->buffer):s.absent_liquid;
        SDL_BindGPUFragmentStorageBuffers(pass,0,&buffer,1);
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const auto masks=settings.edges.masks();
        const Uniform uniform{w,h,(srgb?1U:0U)|(reuse?2U:0U)|(liquid?4U:0U)|pattern_flag(settings.patterns)|(edge_witness?16U:0U)|(settings.model_reflections?32U:0U),settings.rejected_layers,
            std::clamp(settings.weight,0.F,.95F),settings.patterns.scale,{masks[0],masks[1]},{},{},settings.patterns.passes};
        SDL_PushGPUFragmentUniformData(cb,0,&uniform,sizeof(uniform));SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.pending_images=std::move(next);s.pending_index=write;s.pending_settings=settings;s.pending=true;s.pending_reset=false;
        s.status="Native TAA encoded; awaiting whole presentation acceptance";return true;
    } catch(const std::exception& e) {state_->status=e.what();return false;}
}
bool GpuCalibratedTemporalAa::enqueue_presentation(void* command,void* resolved,void* ownership,void* surface,
    void* native_ink,void* native_ownership,void* destination,unsigned w,unsigned h,std::array<float,2> jitter,
    const CalibratedPatternGuide& patterns,const CalibratedEdgeGuide& edges,void* edge_witness,void* centre_edge_witness) {
    if(!state_) return false;
    try {
        auto& s=*state_;require(s.present_pipeline && command,"Invalid native TAA presentation command");
        const std::array<void*,6> images{resolved,ownership,surface,native_ink,native_ownership,destination};
        for(unsigned i=0;i<images.size();++i) {
            require(images[i],"Missing native TAA presentation image");
            for(unsigned j=i+1;j<images.size();++j) require(images[i]!=images[j],"Aliased native TAA presentation images");
        }
        require(w && h && w<=16384 && h<=16384 && std::isfinite(jitter[0]) && std::isfinite(jitter[1])
            && std::abs(jitter[0])<=.5F && std::abs(jitter[1])<=.5F && patterns.valid() && edges.valid(),"Invalid native TAA presentation extent/jitter");
        require(edges.active()==bool(edge_witness) && edges.active()==bool(centre_edge_witness),"Incomplete native TAA centre edge witness");
        if(edge_witness) {
            require(edge_witness!=centre_edge_witness,"Aliased native TAA AA/centre witnesses");
            for(auto* image:images) require(edge_witness!=image && centre_edge_witness!=image,"Aliased native TAA presentation witness");
        }
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);SDL_GPUColorTargetInfo target{};
        target.texture=static_cast<SDL_GPUTexture*>(destination);target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* pass=SDL_BeginGPURenderPass(cb,&target,1,nullptr);require(pass,SDL_GetError());SDL_BindGPUGraphicsPipeline(pass,s.present_pipeline);
        const SDL_GPUViewport viewport{0,0,float(w),float(h),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(w),int(h)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(resolved),s.sampler},
            {static_cast<SDL_GPUTexture*>(ownership),s.sampler},{static_cast<SDL_GPUTexture*>(surface),s.sampler},
            {static_cast<SDL_GPUTexture*>(native_ink),s.sampler},{static_cast<SDL_GPUTexture*>(native_ownership),s.sampler},
            {static_cast<SDL_GPUTexture*>(native_ink),s.sampler},
            {static_cast<SDL_GPUTexture*>(edge_witness?edge_witness:ownership),s.sampler},
            {static_cast<SDL_GPUTexture*>(centre_edge_witness?centre_edge_witness:native_ownership),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,8);
        const auto masks=edges.masks();
        const Uniform uniform{w,h,pattern_flag(patterns)|(edge_witness?16U:0U),0,0,patterns.scale,{masks[0],masks[1]},
            {jitter[0],jitter[1]},{},patterns.passes};
        SDL_PushGPUFragmentUniformData(cb,0,&uniform,sizeof(uniform));SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.status="Native TAA presentation reconstructed; protected centre ink retained";return true;
    } catch(const std::exception& e) {state_->status=e.what();return false;}
}
}
