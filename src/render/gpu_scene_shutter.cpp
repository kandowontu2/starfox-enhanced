#include "starfox/render/gpu_scene_shutter.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <SDL3/SDL_gpu.h>
#include "shaders/generated/scene_shutter_portable.hpp"
#include <stdexcept>
#endif
namespace starfox::render {
struct GpuSceneShutter::Impl {
    std::string status{"GPU particle shutter unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};
    SDL_GPUComputePipeline* pipeline{};
    SDL_GPUTexture* texture{};
    unsigned width{},height{};
    ~Impl() {
        if(texture) SDL_ReleaseGPUTexture(device,texture);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
    }
#endif
};
GpuSceneShutter::GpuSceneShutter():impl_(std::make_unique<Impl>()){}
GpuSceneShutter::~GpuSceneShutter()=default;
void GpuSceneShutter::release_device() noexcept {impl_.reset();}
const std::string& GpuSceneShutter::status() const {
    static const std::string released{"GPU particle shutter released"};return impl_?impl_->status:released;
}
bool GpuSceneShutter::enqueue(void* command,const GpuCompositeOutput& input,const SceneFxFrame& fx,
    const MotionBlurSettings& s,float scale,void*& output) {
    output=nullptr;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_) impl_=std::make_unique<Impl>();
    try {
        const auto require=[](bool ok,const char* message) {if(!ok) throw std::runtime_error(message);};
        require(command&&input.device&&input.rgba&&input.rgba!=impl_->texture,"Invalid particle shutter handles");
        require(input.width&&input.height&&input.width<=16384&&input.height<=16384
            &&std::uint64_t(input.width)*input.height<=UINT32_MAX/16,"Invalid particle shutter extent");
        require(std::isfinite(scale)&&scale>0&&scale<=10&&std::isfinite(fx.camera[3])
            &&fx.camera[3]>=0&&fx.camera[3]<=scene_fx_capacity&&std::floor(fx.camera[3])==fx.camera[3],"Invalid particle shutter frame");
        require(std::isfinite(s.interval_seconds)&&s.interval_seconds>=0&&std::isfinite(s.exposure_seconds)
            &&s.exposure_seconds>=0&&std::isfinite(s.maximum_radius)&&s.maximum_radius>=0
            &&s.maximum_radius<=128&&s.samples>=1&&s.samples<=65,"Invalid particle shutter settings");
        struct Parameters {
            Uint32 width,height,samples,count;
            std::array<float,4> shutter;
            decltype(fx.data) data;
            std::array<std::array<float,4>,scene_fx_capacity> previous{};
        };
        static_assert(sizeof(Parameters)==3104);
        const bool moving=!s.paused&&s.interval_seconds>0&&s.interval_seconds<=.25
            &&s.exposure_seconds>0&&s.maximum_radius>0;
        const float ratio=moving?float(s.exposure_seconds/s.interval_seconds):0;
        require(std::isfinite(ratio)&&std::isfinite(float(s.exposure_seconds)),"Particle shutter exposure overflow");
        Parameters p{input.width,input.height,moving?s.samples:1U,unsigned(fx.camera[3]),
            {ratio,s.maximum_radius,float(s.exposure_seconds),scale},fx.data};
        for(unsigned n=0;n<p.count;++n) {
            const auto type=fx.data[n*3+1][0];
            require(type>=2&&type<=7&&std::floor(type)==type,"Particle shutter requires a particle-only pass");
            for(unsigned k=0;k<3;++k) for(float v:fx.data[n*3+k])
                require(std::isfinite(v),"Nonfinite particle payload");
            require(fx.data[n*3][2]>=0&&fx.data[n*3][3]>=0&&fx.data[n*3][3]<=1
                &&fx.data[n*3+1][1]>0,"Invalid particle radius, opacity or depth");
            const auto& prior=fx.motion_previous[n];
            if(moving&&prior.valid&&std::isfinite(prior.previous[0])&&std::isfinite(prior.previous[1])
                &&std::isfinite(prior.previous[2])&&prior.previous[2]>0)
                p.previous[n]={prior.previous[0],prior.previous[1],prior.previous[2],1};
        }
        if(!p.count) {output=input.rgba;impl_->status="GPU particle shutter empty bypass";return true;}
        require(input.packed&&input.surfaces,"Missing particle shutter coverage/depth");
        if(impl_->device&&impl_->device!=input.device) impl_=std::make_unique<Impl>();
        auto* device=static_cast<SDL_GPUDevice*>(input.device);impl_->device=device;
        if(!impl_->pipeline) {
            SDL_GPUComputePipelineCreateInfo info{};const auto formats=SDL_GetGPUShaderFormats(device);
            if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
                info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=scene_shutter_shader::spirv;
                info.code_size=sizeof(scene_shutter_shader::spirv);info.entrypoint="main";
            } else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
                info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=scene_shutter_shader::dxil;
                info.code_size=sizeof(scene_shutter_shader::dxil);info.entrypoint="main";
            } else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
                info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(scene_shutter_shader::metal);
                info.code_size=sizeof(scene_shutter_shader::metal)-1;info.entrypoint="main0";
            } else throw std::runtime_error("No particle shutter shader format");
            info.num_readonly_storage_textures=1;info.num_readonly_storage_buffers=2;
            info.num_readwrite_storage_textures=1;info.num_uniform_buffers=1;
            info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
            impl_->pipeline=create_gpu_compute_pipeline(device,&info);require(impl_->pipeline,SDL_GetError());
        }
        if(!impl_->texture||impl_->width!=input.width||impl_->height!=input.height) {
            if(impl_->texture) SDL_ReleaseGPUTexture(device,impl_->texture);
            impl_->texture=nullptr;
            SDL_GPUTextureCreateInfo t{};t.type=SDL_GPU_TEXTURETYPE_2D;t.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            t.width=input.width;t.height=input.height;t.layer_count_or_depth=t.num_levels=1;
            t.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            impl_->texture=SDL_CreateGPUTexture(device,&t);require(impl_->texture,SDL_GetError());
            impl_->width=input.width;impl_->height=input.height;
        }
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_GPUStorageTextureReadWriteBinding target{};target.texture=impl_->texture;
        SDL_PushGPUComputeUniformData(cb,0,&p,sizeof(p));
        auto* pass=SDL_BeginGPUComputePass(cb,&target,1,nullptr,0);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        auto* texture=static_cast<SDL_GPUTexture*>(input.rgba);
        SDL_BindGPUComputeStorageTextures(pass,0,&texture,1);
        SDL_GPUBuffer* buffers[]{static_cast<SDL_GPUBuffer*>(input.packed),static_cast<SDL_GPUBuffer*>(input.surfaces)};
        SDL_BindGPUComputeStorageBuffers(pass,0,buffers,2);
        SDL_DispatchGPUCompute(pass,(input.width+7)/8,(input.height+7)/8,1);SDL_EndGPUComputePass(pass);
        output=impl_->texture;impl_->status="GPU particle shutter submitted";return true;
    } catch(const std::exception& e) {impl_->status=e.what();return false;}
#else
    (void)command;(void)input;(void)fx;(void)s;(void)scale;return false;
#endif
}
}
