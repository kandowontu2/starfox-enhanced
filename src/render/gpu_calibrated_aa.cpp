#include "starfox/render/gpu_calibrated_aa.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "../../third_party/smaa/AreaTex.h"
#include "../../third_party/smaa/SearchTex.h"
#include <array>
#include <cstring>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Uniform {unsigned width,height,type,quality,srgb,stage,padding[2];};
static_assert(sizeof(Uniform)==32);
}
struct GpuCalibratedAa::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};SDL_GPUShader* vertex{};
    std::array<SDL_GPUShader*,5> fragments{};std::array<SDL_GPUGraphicsPipeline*,5> pipelines{};
    SDL_GPUSampler* sampler{};SDL_GPUTexture *area{},*search{},*edges{},*weights{};
    unsigned width{},height{};bool lut_ready{};
    std::string status{"Native AA not initialized"};
    ~State() {
        if(!device) return;
        for(auto* p:pipelines) if(p) SDL_ReleaseGPUGraphicsPipeline(device,p);
        for(auto* f:fragments) if(f) SDL_ReleaseGPUShader(device,f);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);if(sampler) SDL_ReleaseGPUSampler(device,sampler);
        for(auto* t:{area,search,edges,weights}) if(t) SDL_ReleaseGPUTexture(device,t);
    }
    SDL_GPUTexture* texture(unsigned w,unsigned h,SDL_GPUTextureFormat f,bool target) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.width=w;info.height=h;
        info.layer_count_or_depth=info.num_levels=1;info.format=f;info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if(target) info.usage|=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        auto* result=SDL_CreateGPUTexture(device,&info);require(result,SDL_GetError());return result;
    }
    void pipeline(unsigned stage) {
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
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.num_samplers=6;shader.num_uniform_buffers=1;
        static const std::array<const Uint8*,5> spv{calibrated_scene_shader::aa_spatial_spirv,calibrated_scene_shader::aa_edges_spirv,
            calibrated_scene_shader::aa_weights_spirv,calibrated_scene_shader::aa_blend_spirv,calibrated_scene_shader::aa_supersample_spirv};
        static const std::array<std::size_t,5> spv_size{sizeof(calibrated_scene_shader::aa_spatial_spirv),sizeof(calibrated_scene_shader::aa_edges_spirv),
            sizeof(calibrated_scene_shader::aa_weights_spirv),sizeof(calibrated_scene_shader::aa_blend_spirv),sizeof(calibrated_scene_shader::aa_supersample_spirv)};
        static const std::array<const char*,5> entry{"aa_spatial_fragment_main","aa_edges_fragment_main","aa_weights_fragment_main","aa_blend_fragment_main","aa_supersample_fragment_main"};
        shader.entrypoint=entry[stage];shader.code=spv[stage];shader.code_size=spv_size[stage];
#if defined(_WIN32)
        static const std::array<const Uint8*,5> dxil{calibrated_scene_shader::aa_spatial_dxil,calibrated_scene_shader::aa_edges_dxil,
            calibrated_scene_shader::aa_weights_dxil,calibrated_scene_shader::aa_blend_dxil,calibrated_scene_shader::aa_supersample_dxil};
        static const std::array<std::size_t,5> dxil_size{sizeof(calibrated_scene_shader::aa_spatial_dxil),sizeof(calibrated_scene_shader::aa_edges_dxil),
            sizeof(calibrated_scene_shader::aa_weights_dxil),sizeof(calibrated_scene_shader::aa_blend_dxil),sizeof(calibrated_scene_shader::aa_supersample_dxil)};
        if(!spirv) {shader.code=dxil[stage];shader.code_size=dxil_size[stage];}
#endif
        if(!fragments[stage]) fragments[stage]=create_gpu_shader(device,&shader);require(fragments[stage],SDL_GetError());
        SDL_GPUColorTargetDescription target{};target.format=stage==1 || stage==2?SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:format;
        SDL_GPUGraphicsPipelineCreateInfo info{};info.vertex_shader=vertex;info.fragment_shader=fragments[stage];
        info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;info.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions=&target;info.target_info.num_color_targets=1;
        pipelines[stage]=create_gpu_graphics_pipeline(device,&info);require(pipelines[stage],SDL_GetError());
    }
    void prepare_smaa(unsigned w,unsigned h) {
        for(unsigned s=1;s<4;++s) pipeline(s);
        if(!area) area=texture(AREATEX_WIDTH,AREATEX_HEIGHT,SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,false);
        if(!search) search=texture(SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,SDL_GPU_TEXTUREFORMAT_R8_UNORM,false);
        if(!lut_ready) {
            // Immutable upstream LUTs are queued once on the same ordered SDL
            // queue, independently of a caller's cancellable eye command.
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,AREATEX_SIZE+SEARCHTEX_SIZE,0};
            auto* upload=SDL_CreateGPUTransferBuffer(device,&info);require(upload,SDL_GetError());
            auto* bytes=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));
            if(!bytes) {SDL_ReleaseGPUTransferBuffer(device,upload);throw std::runtime_error(SDL_GetError());}
            std::memcpy(bytes,areaTexBytes,AREATEX_SIZE);std::memcpy(bytes+AREATEX_SIZE,searchTexBytes,SEARCHTEX_SIZE);
            SDL_UnmapGPUTransferBuffer(device,upload);auto* cb=SDL_AcquireGPUCommandBuffer(device);
            if(!cb) {SDL_ReleaseGPUTransferBuffer(device,upload);throw std::runtime_error(SDL_GetError());}
            auto* copy=SDL_BeginGPUCopyPass(cb);
            if(!copy) {SDL_CancelGPUCommandBuffer(cb);SDL_ReleaseGPUTransferBuffer(device,upload);throw std::runtime_error(SDL_GetError());}
            SDL_GPUTextureTransferInfo from{upload,0,0,0};SDL_GPUTextureRegion to{area,0,0,0,0,0,AREATEX_WIDTH,AREATEX_HEIGHT,1};
            SDL_UploadToGPUTexture(copy,&from,&to,false);from.offset=AREATEX_SIZE;
            to.texture=search;to.w=SEARCHTEX_WIDTH;to.h=SEARCHTEX_HEIGHT;SDL_UploadToGPUTexture(copy,&from,&to,false);
            SDL_EndGPUCopyPass(copy);const bool submitted=SDL_SubmitGPUCommandBuffer(cb);SDL_ReleaseGPUTransferBuffer(device,upload);
            require(submitted,SDL_GetError());lut_ready=true;
        }
        if(width==w && height==h) return;
        auto* next_edges=texture(w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,true);
        SDL_GPUTexture* next_weights{};
        try {next_weights=texture(w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,true);}
        catch(...) {SDL_ReleaseGPUTexture(device,next_edges);throw;}
        if(edges) SDL_ReleaseGPUTexture(device,edges);if(weights) SDL_ReleaseGPUTexture(device,weights);
        edges=next_edges;weights=next_weights;width=w;height=h;
    }
};
GpuCalibratedAa::GpuCalibratedAa():state_(std::make_unique<State>()) {}
GpuCalibratedAa::~GpuCalibratedAa()=default;
void GpuCalibratedAa::release_device() noexcept {state_.reset();}
const std::string& GpuCalibratedAa::status() const noexcept {
    static const std::string released{"Native AA released"};return state_?state_->status:released;
}
bool GpuCalibratedAa::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native AA requires a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,"Unsupported native AA format");
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_LINEAR;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());s->pipeline(0);
        s->status="Native spatial AA ready";state_=std::move(s);return true;
    } catch(const std::exception& e) {const std::string message=e.what();s.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedAa::enqueue(void* command,void* source,void* ownership,void* destination,
    unsigned w,unsigned h,unsigned type,unsigned quality) {
    if(!state_) return false;
    try {
        auto& s=*state_;require(s.device && s.pipelines[0] && command && source && ownership && destination
            && source!=ownership && source!=destination && destination!=ownership,"Invalid native AA inputs");
        require(w && h && w<=32768 && h<=32768 && quality<=3 && (quality==0 || calibrated_spatial_aa_supported(type)),"Unsupported native AA settings");
        require(source!=s.edges && source!=s.weights && destination!=s.edges && destination!=s.weights && ownership!=s.edges && ownership!=s.weights,"Aliased native AA intermediates");
        if(type==5 && quality) s.prepare_smaa(w,h);
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        const unsigned passes=type==5 && quality?3:1;
        for(unsigned stage=0;stage<passes;++stage) {
            const unsigned pipeline=passes==1?0:stage+1;
            SDL_GPUColorTargetInfo target{};target.texture=passes==1 || stage==2?static_cast<SDL_GPUTexture*>(destination):stage==0?s.edges:s.weights;
            target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
            auto* pass=SDL_BeginGPURenderPass(cb,&target,1,nullptr);require(pass,SDL_GetError());SDL_BindGPUGraphicsPipeline(pass,s.pipelines[pipeline]);
            const SDL_GPUViewport viewport{0,0,float(w),float(h),0,1};SDL_SetGPUViewport(pass,&viewport);
            const SDL_Rect scissor{0,0,int(w),int(h)};SDL_SetGPUScissor(pass,&scissor);
            auto* color=static_cast<SDL_GPUTexture*>(source);
            // Unused bindings must not alias a current render target either.
            const SDL_GPUTextureSamplerBinding inputs[]{{color,s.sampler},{static_cast<SDL_GPUTexture*>(ownership),s.sampler},
                {passes==3 && stage>0?s.edges:color,s.sampler},{passes==3 && stage>1?s.weights:color,s.sampler},
                {passes==3?s.area:color,s.sampler},{passes==3?s.search:color,s.sampler}};
            SDL_BindGPUFragmentSamplers(pass,0,inputs,6);const Uniform uniform{w,h,type,quality,unsigned(srgb),stage,{}};
            SDL_PushGPUFragmentUniformData(cb,0,&uniform,sizeof(uniform));SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        }
        s.status=passes==3?"Native upstream SMAA 1x encoded":"Native spatial AA encoded";return true;
    } catch(const std::exception& e) {state_->status=e.what();return false;}
}
bool GpuCalibratedAa::enqueue_supersample(void* command,void* supersampled,void* high_ownership,
    void* native_ink,void* native_ownership,void* destination,unsigned w,unsigned h,unsigned factor) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.device && command && supersampled && high_ownership && native_ink && native_ownership && destination,
            "Invalid native SSAA images");
        const std::array<void*,5> images{supersampled,high_ownership,native_ink,native_ownership,destination};
        for(unsigned i=0;i<images.size();++i) for(unsigned j=i+1;j<images.size();++j)
            require(images[i]!=images[j],"Aliased native SSAA images");
        require(factor>=2 && calibrated_sample_extent_supported(w,h,factor),
            "Unsupported native SSAA extent or sample factor");
        for(auto* image:images) require(image!=s.edges && image!=s.weights,"Aliased native SSAA intermediates");
        s.pipeline(4);
        SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        auto* pass=SDL_BeginGPURenderPass(cb,&target,1,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.pipelines[4]);
        const SDL_GPUViewport viewport{0,0,float(w),float(h),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(w),int(h)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding inputs[]{
            {static_cast<SDL_GPUTexture*>(supersampled),s.sampler},
            {static_cast<SDL_GPUTexture*>(native_ownership),s.sampler},
            {static_cast<SDL_GPUTexture*>(native_ink),s.sampler},
            {static_cast<SDL_GPUTexture*>(high_ownership),s.sampler},
            {static_cast<SDL_GPUTexture*>(native_ink),s.sampler},
            {static_cast<SDL_GPUTexture*>(native_ink),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,inputs,6);
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const Uniform uniform{w,h,factor,0,unsigned(srgb),4,{}};
        SDL_PushGPUFragmentUniformData(cb,0,&uniform,sizeof(uniform));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.status="Native supersampled eye resolved";return true;
    } catch(const std::exception& e) {state_->status=e.what();return false;}
}
}
