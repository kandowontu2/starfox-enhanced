#include "starfox/render/gpu_motion_blur.hpp"
#include "starfox/render/scene_enhancements.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <SDL3/SDL_gpu.h>
#include "shaders/generated/motion_blur_portable.hpp"
#include "shaders/generated/motion_blur_resident_portable.hpp"
#include <stdexcept>
#endif
namespace starfox::render {
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {
void check(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
struct Parameters {
    Uint32 width,height,stage,sample,samples,history;
    float jitter_x{},jitter_y{};
    float shutter,radius,tolerance,time;
};
static_assert(sizeof(Parameters)==48);
}
struct GpuMotionBlur::Impl {
    SDL_GPUDevice* device{};
    SDL_GPUComputePipeline *pipeline{},*resident_pipeline{};
    SDL_GPUBuffer* buffers[3]{};
    SDL_GPUTexture* texture{};
    unsigned width{},height{};
    bool particle_masks{};
    unsigned dispatches{};
    std::string status{"GPU motion blur not initialized"};
    ~Impl() {
        if(!device) return;
        for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(texture) SDL_ReleaseGPUTexture(device,texture);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(resident_pipeline) SDL_ReleaseGPUComputePipeline(device,resident_pipeline);
    }
    void initialize(SDL_GPUDevice* d,bool resident) {
        device=d;SDL_GPUComputePipelineCreateInfo p{};
        const auto formats=SDL_GetGPUShaderFormats(d);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            p.format=SDL_GPU_SHADERFORMAT_SPIRV;p.code=resident?motion_blur_resident_shader::spirv:motion_blur_shader::spirv;
            p.code_size=resident?sizeof(motion_blur_resident_shader::spirv):sizeof(motion_blur_shader::spirv);p.entrypoint="main";
        } else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
            p.format=SDL_GPU_SHADERFORMAT_DXIL;p.code=resident?motion_blur_resident_shader::dxil:motion_blur_shader::dxil;
            p.code_size=resident?sizeof(motion_blur_resident_shader::dxil):sizeof(motion_blur_shader::dxil);p.entrypoint="main";
        } else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
            p.format=SDL_GPU_SHADERFORMAT_MSL;p.code=reinterpret_cast<const Uint8*>(resident?motion_blur_resident_shader::metal:motion_blur_shader::metal);
            p.code_size=resident?sizeof(motion_blur_resident_shader::metal)-1:sizeof(motion_blur_shader::metal)-1;p.entrypoint="main0";
        } else throw std::runtime_error("No motion blur shader format");
        p.num_readonly_storage_textures=2;p.num_readonly_storage_buffers=resident?4:3;
        p.num_readwrite_storage_textures=1;p.num_readwrite_storage_buffers=3;p.num_uniform_buffers=2;
        p.threadcount_x=p.threadcount_y=8;p.threadcount_z=1;
        auto*& target=resident?resident_pipeline:pipeline;
        target=create_gpu_compute_pipeline(d,&p);check(target);
    }
    void resize(unsigned w,unsigned h,bool particles) {
        if(w==width&&h==height&&texture&&buffers[0]&&buffers[1]&&buffers[2]) {
            if(particle_masks==particles) return;
            SDL_GPUBufferCreateInfo b{};
            b.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
            b.size=w*h*(particles?16:8);
            // Allocate first: failure leaves the existing resources usable on
            // retry. SDL defers the old buffer's destruction for queued work.
            auto* replacement=SDL_CreateGPUBuffer(device,&b);check(replacement);
            SDL_ReleaseGPUBuffer(device,buffers[0]);buffers[0]=replacement;
            particle_masks=particles;return;
        }
        width=height=0;
        if(texture) SDL_ReleaseGPUTexture(device,texture);
        texture=nullptr;
        for(auto*& b:buffers) {if(b) SDL_ReleaseGPUBuffer(device,b);b=nullptr;}
        SDL_GPUTextureCreateInfo t{};t.type=SDL_GPU_TEXTURETYPE_2D;t.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        t.width=w;t.height=h;t.layer_count_or_depth=t.num_levels=1;
        t.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        texture=SDL_CreateGPUTexture(device,&t);check(texture);
        for(unsigned i=0;i<3;++i) {
            SDL_GPUBufferCreateInfo b{};b.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
            // Model-only frames need nearest depth and an affected flag, but
            // no particle candidate masks. Do not retain their extra memory.
            b.size=w*h*(i==0&&!particles?8:16);buffers[i]=SDL_CreateGPUBuffer(device,&b);check(buffers[i]);
        }
        width=w;height=h;particle_masks=particles;
    }
};
#else
struct GpuMotionBlur::Impl {std::string status{"GPU motion blur unavailable"};};
#endif
GpuMotionBlur::GpuMotionBlur():impl_(std::make_unique<Impl>()){}
GpuMotionBlur::~GpuMotionBlur()=default;
void GpuMotionBlur::release_device() noexcept {impl_.reset();}
const std::string& GpuMotionBlur::status() const {static const std::string s{"GPU motion blur released"};return impl_?impl_->status:s;}
unsigned GpuMotionBlur::dispatch_count() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_?impl_->dispatches:0;
#else
    return 0;
#endif
}
bool GpuMotionBlur::enqueue(void* command,const GpuCompositeOutput& input,
    const GpuCompositeOutput& underlay,const MotionBlurSettings& s,bool history,void*& output,std::array<float,2> guide_jitter,
    const SceneFxFrame* particles,float particle_scale,void* resident_particles) {
    output=nullptr;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_) impl_=std::make_unique<Impl>();
        impl_->dispatches=0;
    try {
        struct ParticleParameters {
            Uint32 count{};float scale{},exposure{},pad{};
            std::array<std::array<float,4>,144> data{};
            std::array<std::array<float,4>,48> previous{};
        } particle;
        static_assert(sizeof(ParticleParameters)==3088);
        if(resident_particles) {
            if(particles || !std::isfinite(particle_scale) || particle_scale<=0 || particle_scale>16384)
                throw std::runtime_error("Invalid resident joint particle binding/scale");
            particle.count=48;particle.scale=particle_scale;particle.exposure=float(s.exposure_seconds);
            if(!std::isfinite(particle.exposure)) throw std::runtime_error("Particle exposure overflow");
        }
        if(particles) {
            const auto count=particles->camera[3];
            if(!std::isfinite(count)||count<0||count>48||std::floor(count)!=count
                ||!std::isfinite(particle_scale)||particle_scale<=0||particle_scale>10)
                throw std::runtime_error("Invalid joint exposure particle frame");
            particle.count=unsigned(count);particle.scale=particle_scale;particle.exposure=float(s.exposure_seconds);
            if(!std::isfinite(particle.exposure)) throw std::runtime_error("Particle exposure overflow");
            particle.data=particles->data;
            for(unsigned n=0;n<particle.count;++n) {
                const auto type=particle.data[n*3+1][0];
                if(type<2||type>7||std::floor(type)!=type) throw std::runtime_error("Joint exposure requires particles only");
                for(unsigned k=0;k<3;++k) for(float v:particle.data[n*3+k])
                    if(!std::isfinite(v)) throw std::runtime_error("Nonfinite joint particle payload");
                if(particle.data[n*3][2]<0||particle.data[n*3][3]<0||particle.data[n*3][3]>1||particle.data[n*3+1][1]<=0)
                    throw std::runtime_error("Invalid joint particle radius/opacity/depth");
                const auto& prior=particles->motion_previous[n];
                if(prior.valid&&std::isfinite(prior.previous[0])&&std::isfinite(prior.previous[1])
                    &&std::isfinite(prior.previous[2])&&prior.previous[2]>0)
                    particle.previous[n]={prior.previous[0],prior.previous[1],prior.previous[2],1};
            }
        }
        if(!std::isfinite(guide_jitter[0]) || !std::isfinite(guide_jitter[1])
            || std::abs(guide_jitter[0])>1 || std::abs(guide_jitter[1])>1
            || !command||!input.device||!input.rgba||input.rgba==impl_->texture
            ||!input.width||!input.height||input.width>16384||input.height>16384
            ||std::uint64_t(input.width)*input.height>UINT32_MAX/16
            ||!std::isfinite(s.interval_seconds)||s.interval_seconds<0||!std::isfinite(s.exposure_seconds)||s.exposure_seconds<0
            ||!std::isfinite(s.maximum_radius)||s.maximum_radius<0||s.maximum_radius>128
            ||!std::isfinite(s.relative_depth_tolerance)||s.relative_depth_tolerance<0||s.samples<1||s.samples>65)
            throw std::runtime_error("Invalid GPU motion blur inputs");
        if(impl_->device&&impl_->device!=input.device) impl_=std::make_unique<Impl>();
        const bool identity=!history||s.paused||s.interval_seconds==0||s.interval_seconds>.25
            ||s.exposure_seconds==0||s.maximum_radius==0||s.samples==1;
        // No scratch allocation, shader initialization or copy for an identity
        // frame. The caller already owns this resident colour texture.
        if(identity&&!particle.count) {output=input.rgba;impl_->status="GPU motion blur: identity bypass";return true;}
        if(!input.packed||(!identity&&(!input.geometry_depth||!input.motion))||underlay.rgba==impl_->texture
            ||underlay.device!=input.device||!underlay.rgba||underlay.width!=input.width||underlay.height!=input.height)
            throw std::runtime_error("Missing GPU motion blur guides or underlay");
        const double shutter=identity?0:s.exposure_seconds/s.interval_seconds;
        if(!std::isfinite(float(shutter))) throw std::runtime_error("Motion blur shutter exceeds float range");
        if(resident_particles?!impl_->resident_pipeline:!impl_->pipeline)
            impl_->initialize(static_cast<SDL_GPUDevice*>(input.device),resident_particles!=nullptr);
        impl_->resize(input.width,input.height,particle.count>0);
        const bool surface_depth=!input.geometry_depth&&input.surfaces;
        Parameters p{input.width,input.height,0,0,identity?1U:s.samples,
            (history?1U:0U)|((input.geometry_depth||surface_depth)?2U:0U)|(surface_depth?4U:0U),guide_jitter[0],guide_jitter[1],
            float(shutter),s.maximum_radius,s.relative_depth_tolerance,0};
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_PushGPUComputeUniformData(cb,1,&particle,sizeof(particle));
        SDL_GPUTexture* textures[]{static_cast<SDL_GPUTexture*>(input.rgba),static_cast<SDL_GPUTexture*>(underlay.rgba)};
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(input.packed),
            static_cast<SDL_GPUBuffer*>(input.geometry_depth?input.geometry_depth:surface_depth?input.surfaces:input.packed),
            static_cast<SDL_GPUBuffer*>(input.motion?input.motion:input.packed),static_cast<SDL_GPUBuffer*>(resident_particles)};
        SDL_GPUStorageTextureReadWriteBinding target{};target.texture=impl_->texture;
        SDL_GPUStorageBufferReadWriteBinding targets[3]{};
        for(unsigned i=0;i<3;++i) targets[i].buffer=impl_->buffers[i];
        for(unsigned sample=0;sample<(identity?1:s.samples);++sample) {
            p.sample=sample;p.time=identity?0:float(sample)/float(s.samples-1)-.5f;
            // Resolve clears its own accumulators for the following sample;
            // the compute-pass boundary orders that clear before scattering.
            for(unsigned stage=sample?1:0;stage<4;++stage) {
                p.stage=stage;SDL_PushGPUComputeUniformData(cb,0,&p,sizeof(p));
                auto* pass=SDL_BeginGPUComputePass(cb,&target,1,targets,3);check(pass);
                SDL_BindGPUComputePipeline(pass,resident_particles?impl_->resident_pipeline:impl_->pipeline);
                SDL_BindGPUComputeStorageTextures(pass,0,textures,2);
                SDL_BindGPUComputeStorageBuffers(pass,0,inputs,resident_particles?4:3);
                SDL_DispatchGPUCompute(pass,(input.width+7)/8,(input.height+7)/8,1);SDL_EndGPUComputePass(pass);
                ++impl_->dispatches;
            }
        }
        output=impl_->texture;impl_->status=std::string("GPU motion blur: ")+SDL_GetGPUDeviceDriver(impl_->device);return true;
    } catch(const std::exception& e) {impl_->status=e.what();return false;}
#else
    (void)command;(void)input;(void)underlay;(void)s;(void)history;(void)guide_jitter;(void)particles;(void)particle_scale;(void)resident_particles;return false;
#endif
}
}
