#include "starfox/render/gpu_calibrated_motion_blur.hpp"
#include "starfox/render/gpu_motion_blur.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/calibrated_motion_blur.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include <cmath>
#include <stdexcept>

namespace starfox::render {
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Uniform {unsigned width,height,srgb,padding;std::array<float,2> delta,jitter;};
static_assert(sizeof(Uniform)==32);
}
struct GpuCalibratedMotionBlur::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUComputePipeline* pack{};SDL_GPUShader *vertex{},*fragment{};
    SDL_GPUGraphicsPipeline* present{};SDL_GPUSampler* sampler{};
    SDL_GPUBuffer* zero{};
    std::array<SDL_GPUTexture*,2> colors{};std::array<SDL_GPUBuffer*,3> guides{};
    unsigned width{},height{},dispatches{};GpuMotionBlur reconstruction;
    std::string status{"Native motion blur not initialized"};
    ~State() {
        reconstruction.release_device();if(!device) return;
        for(auto* t:colors) if(t) SDL_ReleaseGPUTexture(device,t);
        for(auto* b:guides) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(zero) SDL_ReleaseGPUBuffer(device,zero);
        if(pack) SDL_ReleaseGPUComputePipeline(device,pack);
        if(present) SDL_ReleaseGPUGraphicsPipeline(device,present);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
    bool srgb() const noexcept {
        return format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    }
    void prepare() {
        if(!zero) {
            const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,4,0};
            zero=SDL_CreateGPUBuffer(device,&info);require(zero,SDL_GetError());
        }
        const auto formats=SDL_GetGPUShaderFormats(device);
        const bool spirv=(formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(spirv || (formats&SDL_GPU_SHADERFORMAT_DXIL),"Native motion blur requires SPIR-V or DXIL");
        if(!sampler) {
            SDL_GPUSamplerCreateInfo info{};info.min_filter=info.mag_filter=SDL_GPU_FILTER_NEAREST;
            info.address_mode_u=info.address_mode_v=info.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            sampler=SDL_CreateGPUSampler(device,&info);require(sampler,SDL_GetError());
        }
        if(!pack) {
            SDL_GPUComputePipelineCreateInfo info{};info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.entrypoint="motion_pack_main";info.code=calibrated_motion_blur_shader::pack_spirv;
            info.code_size=sizeof(calibrated_motion_blur_shader::pack_spirv);
#if defined(_WIN32)
            if(!spirv) {info.code=calibrated_motion_blur_shader::pack_dxil;info.code_size=sizeof(calibrated_motion_blur_shader::pack_dxil);}
#endif
            info.num_samplers=5;info.num_readonly_storage_buffers=1;
            info.num_readwrite_storage_textures=2;info.num_readwrite_storage_buffers=3;info.num_uniform_buffers=1;
            info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
            pack=create_gpu_compute_pipeline(device,&info);require(pack,SDL_GetError());
        }
        if(present) return;
        SDL_GPUShaderCreateInfo shader{};shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        if(!vertex) {
            shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
            shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
            vertex=create_gpu_shader(device,&shader);require(vertex,SDL_GetError());
        }
        if(!fragment) {
            shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="motion_present_main";
            shader.num_samplers=3;shader.num_uniform_buffers=1;
            shader.code=calibrated_motion_blur_shader::present_spirv;shader.code_size=sizeof(calibrated_motion_blur_shader::present_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_motion_blur_shader::present_dxil;shader.code_size=sizeof(calibrated_motion_blur_shader::present_dxil);}
#endif
            fragment=create_gpu_shader(device,&shader);require(fragment,SDL_GetError());
        }
        SDL_GPUColorTargetDescription target{};target.format=format;
        SDL_GPUGraphicsPipelineCreateInfo info{};info.vertex_shader=vertex;info.fragment_shader=fragment;
        info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;info.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions=&target;info.target_info.num_color_targets=1;
        present=create_gpu_graphics_pipeline(device,&info);require(present,SDL_GetError());
    }
    void resize(unsigned w,unsigned h) {
        if(width==w && height==h) return;
        std::array<SDL_GPUTexture*,2> next_colors{};std::array<SDL_GPUBuffer*,3> next_guides{};
        const auto release=[&] {
            for(auto* t:next_colors) if(t) SDL_ReleaseGPUTexture(device,t);
            for(auto* b:next_guides) if(b) SDL_ReleaseGPUBuffer(device,b);
        };
        try {
            for(auto*& t:next_colors) {
                SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                info.width=w;info.height=h;info.layer_count_or_depth=info.num_levels=1;
                info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
                t=SDL_CreateGPUTexture(device,&info);require(t,SDL_GetError());
            }
            for(unsigned i=0;i<3;++i) {
                SDL_GPUBufferCreateInfo info{};info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
                info.size=w*h*(i==2?16:4);next_guides[i]=SDL_CreateGPUBuffer(device,&info);require(next_guides[i],SDL_GetError());
            }
        } catch(...) {release();throw;}
        for(auto* t:colors) if(t) SDL_ReleaseGPUTexture(device,t);
        for(auto* b:guides) if(b) SDL_ReleaseGPUBuffer(device,b);
        colors=next_colors;guides=next_guides;width=w;height=h;
    }
};
GpuCalibratedMotionBlur::GpuCalibratedMotionBlur()=default;
GpuCalibratedMotionBlur::~GpuCalibratedMotionBlur()=default;
void GpuCalibratedMotionBlur::release_device() noexcept {state_.reset();}
unsigned GpuCalibratedMotionBlur::dispatch_count() const noexcept {return state_?state_->dispatches:0;}
const std::string& GpuCalibratedMotionBlur::status() const noexcept {
    static const std::string released{"Native motion blur released"};return state_?state_->status:released;
}
bool GpuCalibratedMotionBlur::initialize(void* device,int format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native motion blur requires a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        s->format=static_cast<SDL_GPUTextureFormat>(format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->srgb(),"Unsupported native motion blur colour format");
        s->status="Native motion blur ready (no shaders allocated)";state_=std::move(s);return true;
    } catch(const std::exception& e) {s->device=nullptr;s->status=e.what();state_=std::move(s);return false;}
}
bool GpuCalibratedMotionBlur::enqueue(void* command,void* source,void* underlay,void* ownership,
    void* surfaces,void* motion,void* destination,unsigned w,unsigned h,const MotionBlurSettings& settings,
    bool history,void*& output,std::array<float,2> delta,std::array<float,2> jitter,
    void* particles,float particle_scale,const shadows::GpuReflectionOutput* water) {
    output=nullptr;if(!state_) return false;auto& s=*state_;s.dispatches=0;
    try {
        require(s.device && command && source && w && h && w<=16384 && h<=16384,"Invalid native motion blur source/extent");
        require(std::isfinite(settings.interval_seconds) && settings.interval_seconds>=0
            && std::isfinite(settings.exposure_seconds) && settings.exposure_seconds>=0
            && std::isfinite(settings.maximum_radius) && settings.maximum_radius>=0 && settings.maximum_radius<=128
            && std::isfinite(settings.relative_depth_tolerance) && settings.relative_depth_tolerance>=0
            && settings.samples>=1 && settings.samples<=65,"Invalid native motion blur shutter");
        for(unsigned i=0;i<2;++i) require(std::isfinite(delta[i]) && std::abs(delta[i])<=2
            && std::isfinite(jitter[i]) && std::abs(jitter[i])<=1,"Invalid native motion blur sample offsets");
        require(!particles || (std::isfinite(particle_scale) && particle_scale>0 && particle_scale<=16384),
            "Invalid native joint particle scale");
        const bool identity=!history || settings.paused || settings.interval_seconds==0 || settings.interval_seconds>.25
            || settings.exposure_seconds==0 || settings.maximum_radius==0 || settings.samples==1;
        if(identity && !particles) {output=source;s.status="Native motion blur: identity bypass";return true;}
        if(water) {
            const auto layout=shadows::native_water_layers(w,h);
            require(layout && water->device==s.device && water->buffer && water->width==w && water->height==h
                && water->row_bytes==w*4 && water->water_layers==*layout,"Invalid native liquid shutter guides");
        }
        require(std::isfinite(float(settings.exposure_seconds/settings.interval_seconds)),
            "Native motion blur shutter exceeds float range");
        // 32-byte adapter + 44-byte reconstruction, plus 8 bytes of particle
        // candidate masks when joint exposure consumes a resident payload.
        // Bound the complete resident scratch, not just the staging textures.
        require(std::uint64_t(w)*h<=1024ULL*1024*1024/(particles?84:76),"Native motion blur exceeds its one-GiB scratch working bound");
        const std::array<void*,6> images{source,underlay,ownership,surfaces,motion,destination};
        for(unsigned i=0;i<images.size();++i) {
            require(images[i],"Missing native motion blur world/ownership/geometry input");
            for(unsigned j=i+1;j<images.size();++j) require(images[i]!=images[j],"Aliased native motion blur inputs");
            for(auto* t:s.colors) require(images[i]!=t,"Native motion blur staging feedback");
        }
        s.prepare();s.resize(w,h);auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        const Uniform uniform{w,h,unsigned(s.srgb()),water?water->water_layers.surface_offset:0,delta,jitter};SDL_PushGPUComputeUniformData(cb,0,&uniform,sizeof(uniform));
        SDL_GPUStorageTextureReadWriteBinding textures[2]{};for(unsigned i=0;i<2;++i) textures[i].texture=s.colors[i];
        SDL_GPUStorageBufferReadWriteBinding buffers[3]{};for(unsigned i=0;i<3;++i) buffers[i].buffer=s.guides[i];
        auto* compute=SDL_BeginGPUComputePass(cb,textures,2,buffers,3);require(compute,SDL_GetError());
        SDL_BindGPUComputePipeline(compute,s.pack);SDL_GPUTextureSamplerBinding inputs[5]{};
        for(unsigned i=0;i<5;++i) inputs[i]={static_cast<SDL_GPUTexture*>(images[i]),s.sampler};
        SDL_BindGPUComputeSamplers(compute,0,inputs,5);
        SDL_GPUBuffer* water_buffer=water?static_cast<SDL_GPUBuffer*>(water->buffer):s.zero;
        SDL_BindGPUComputeStorageBuffers(compute,0,&water_buffer,1);
        SDL_DispatchGPUCompute(compute,(w+7)/8,(h+7)/8,1);SDL_EndGPUComputePass(compute);++s.dispatches;
        const GpuCompositeOutput foreground{s.device,s.colors[0],s.guides[0],nullptr,w,h,s.guides[1],s.guides[2]};
        const GpuCompositeOutput background{s.device,s.colors[1],nullptr,nullptr,w,h};void* blurred{};
        const bool reconstructed=s.reconstruction.enqueue(command,foreground,background,settings,history,blurred,jitter,nullptr,particle_scale,particles);
        require(reconstructed,s.reconstruction.status().c_str());
        s.dispatches+=s.reconstruction.dispatch_count();
        SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* pass=SDL_BeginGPURenderPass(cb,&target,1,nullptr);require(pass,SDL_GetError());SDL_BindGPUGraphicsPipeline(pass,s.present);
        const SDL_GPUViewport viewport{0,0,float(w),float(h),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(w),int(h)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding bindings[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},
            {static_cast<SDL_GPUTexture*>(blurred),s.sampler},{static_cast<SDL_GPUTexture*>(ownership),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,bindings,3);SDL_PushGPUFragmentUniformData(cb,0,&uniform,sizeof(uniform));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);output=destination;
        s.status="Native motion blur: calibrated guides, linear centred-shutter reconstruction";return true;
    } catch(const std::exception& e) {s.status=e.what();return false;}
}
}
