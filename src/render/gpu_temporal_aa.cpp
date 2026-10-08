#include "starfox/render/gpu_temporal_aa.hpp"
#include "starfox/render/gpu_composite.hpp"
#include <stdexcept>
#include <cmath>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/temporal_aa_portable.hpp"
#include "shaders/generated/temporal_surfaces_portable.hpp"
#endif
namespace starfox::render {
struct GpuTemporalSurfaces::Impl {
    std::string status{"Temporal surfaces not initialized"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUBuffer *packed{},*surfaces{};unsigned width{},height{};
    ~Impl() {
        if(!device) return;
        if(packed) SDL_ReleaseGPUBuffer(device,packed);
        if(surfaces) SDL_ReleaseGPUBuffer(device,surfaces);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
    }
    void initialize(SDL_GPUDevice* d,unsigned w,unsigned h) {
        device=d;SDL_GPUComputePipelineCreateInfo p{};const auto formats=SDL_GetGPUShaderFormats(d);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {p.format=SDL_GPU_SHADERFORMAT_SPIRV;p.code=temporal_surfaces_shader::spirv;p.code_size=sizeof(temporal_surfaces_shader::spirv);p.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {p.format=SDL_GPU_SHADERFORMAT_DXIL;p.code=temporal_surfaces_shader::dxil;p.code_size=sizeof(temporal_surfaces_shader::dxil);p.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {p.format=SDL_GPU_SHADERFORMAT_MSL;p.code=reinterpret_cast<const Uint8*>(temporal_surfaces_shader::metal);p.code_size=sizeof(temporal_surfaces_shader::metal)-1;p.entrypoint="main0";}
        else throw std::runtime_error("Temporal surfaces require compute shaders");
        p.num_readonly_storage_buffers=4;p.num_readwrite_storage_buffers=2;p.num_uniform_buffers=1;
        p.threadcount_x=p.threadcount_y=8;p.threadcount_z=1;
        pipeline=create_gpu_compute_pipeline(d,&p);if(!pipeline) throw std::runtime_error(SDL_GetError());
        SDL_GPUBufferCreateInfo b{};b.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        b.size=w*h*4;packed=SDL_CreateGPUBuffer(d,&b);b.size=w*h*16;surfaces=SDL_CreateGPUBuffer(d,&b);
        if(!packed || !surfaces) throw std::runtime_error(SDL_GetError());
        width=w;height=h;
    }
#endif
};
GpuTemporalSurfaces::GpuTemporalSurfaces():impl_(std::make_unique<Impl>()) {}
GpuTemporalSurfaces::~GpuTemporalSurfaces()=default;
void GpuTemporalSurfaces::release_device() noexcept {impl_=std::make_unique<Impl>();}
const std::string& GpuTemporalSurfaces::status() const {return impl_->status;}
bool GpuTemporalSurfaces::enqueue(void* command,const GpuCompositeOutput& source,void* protected_pixels,
    std::array<float,2> jitter,GpuCompositeOutput& output,std::array<std::uint32_t,2> output_extent) {
    output={};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(output_extent==std::array<std::uint32_t,2>{}) output_extent={source.width,source.height};
        if(!command || !source.device || !source.packed || !source.surfaces || !source.geometry_depth || !protected_pixels
            || !source.width || !source.height || source.width>8192 || source.height>8192
            || !output_extent[0] || !output_extent[1] || output_extent[0]>8192 || output_extent[1]>8192
            || !std::isfinite(jitter[0]) || !std::isfinite(jitter[1]) || std::abs(jitter[0])>1 || std::abs(jitter[1])>1)
            throw std::runtime_error("Invalid temporal surface inputs");
        if(impl_->device!=source.device || impl_->width!=output_extent[0] || impl_->height!=output_extent[1]) {
            auto next=std::make_unique<Impl>();next->initialize(static_cast<SDL_GPUDevice*>(source.device),output_extent[0],output_extent[1]);
            impl_=std::move(next);
        }
        if(source.packed==impl_->packed || source.surfaces==impl_->surfaces || protected_pixels==impl_->packed)
            throw std::runtime_error("Aliased temporal surface inputs");
        struct Constants {Uint32 width,height;std::array<float,2> jitter;Uint32 source_width,source_height,pad0,pad1;};
        const Constants constants{output_extent[0],output_extent[1],jitter,source.width,source.height,0,0};
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_PushGPUComputeUniformData(cb,0,&constants,sizeof(constants));
        SDL_GPUStorageBufferReadWriteBinding writes[2]{};writes[0].buffer=impl_->packed;writes[1].buffer=impl_->surfaces;
        auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,writes,2);if(!pass) throw std::runtime_error(SDL_GetError());
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        SDL_GPUBuffer* reads[]{static_cast<SDL_GPUBuffer*>(source.packed),static_cast<SDL_GPUBuffer*>(source.surfaces),
            static_cast<SDL_GPUBuffer*>(source.geometry_depth),static_cast<SDL_GPUBuffer*>(protected_pixels)};
        SDL_BindGPUComputeStorageBuffers(pass,0,reads,4);
        SDL_DispatchGPUCompute(pass,(output_extent[0]+7)/8,(output_extent[1]+7)/8,1);SDL_EndGPUComputePass(pass);
        output=source;output.packed=impl_->packed;output.surfaces=impl_->surfaces;
        output.width=output_extent[0];output.height=output_extent[1];
        if(output_extent!=std::array<std::uint32_t,2>{source.width,source.height}) {
            // Only ownership/normals are reconstructed here. Do not advertise
            // differently sized colour, depth or motion as full-size guides.
            output.rgba=nullptr;output.geometry_depth=nullptr;output.motion=nullptr;
        }
        impl_->status="Temporal surfaces aligned";return true;
    } catch(const std::exception& e) {impl_->status=e.what();}
#else
    (void)command;(void)source;(void)protected_pixels;(void)jitter;(void)output_extent;
#endif
    return false;
}
struct GpuTemporalAa::Impl {
    std::string status{"TAA not initialized"};
    bool valid{},pending{};unsigned current{},width{},height{};std::uint64_t epoch{},next_epoch{};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUTexture* colors[2]{},*meta[2]{},*presentation{};
    ~Impl() {
        if(!device) return;
        for(auto* t:colors) if(t) SDL_ReleaseGPUTexture(device,t);
        for(auto* t:meta) if(t) SDL_ReleaseGPUTexture(device,t);
        if(presentation) SDL_ReleaseGPUTexture(device,presentation);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
    }
    void initialize(SDL_GPUDevice* d,unsigned w,unsigned h) {
        device=d;
        SDL_GPUComputePipelineCreateInfo p{};const auto formats=SDL_GetGPUShaderFormats(d);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {p.format=SDL_GPU_SHADERFORMAT_SPIRV;p.code=temporal_aa_shader::spirv;p.code_size=sizeof(temporal_aa_shader::spirv);p.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {p.format=SDL_GPU_SHADERFORMAT_DXIL;p.code=temporal_aa_shader::dxil;p.code_size=sizeof(temporal_aa_shader::dxil);p.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {p.format=SDL_GPU_SHADERFORMAT_MSL;p.code=reinterpret_cast<const Uint8*>(temporal_aa_shader::metal);p.code_size=sizeof(temporal_aa_shader::metal)-1;p.entrypoint="main0";}
        else throw std::runtime_error("TAA requires compute shaders");
        p.num_readonly_storage_textures=3;p.num_readonly_storage_buffers=3;
        p.num_readwrite_storage_textures=2;p.num_uniform_buffers=1;
        p.threadcount_x=p.threadcount_y=8;p.threadcount_z=1;
        pipeline=create_gpu_compute_pipeline(d,&p);if(!pipeline) throw std::runtime_error(SDL_GetError());
        for(unsigned i=0;i<2;++i) for(unsigned channel=0;channel<2;++channel) {
            SDL_GPUTextureCreateInfo t{};t.type=SDL_GPU_TEXTURETYPE_2D;
            t.format=channel?SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT:SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            t.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            t.width=w;t.height=h;t.layer_count_or_depth=1;t.num_levels=1;
            auto*& output=channel?meta[i]:colors[i];output=SDL_CreateGPUTexture(d,&t);
            if(!output) throw std::runtime_error(SDL_GetError());
        }
        SDL_GPUTextureCreateInfo display{};display.type=SDL_GPU_TEXTURETYPE_2D;
        display.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        display.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        display.width=w;display.height=h;display.layer_count_or_depth=1;display.num_levels=1;
        presentation=SDL_CreateGPUTexture(d,&display);if(!presentation) throw std::runtime_error(SDL_GetError());
        width=w;height=h;
    }
#endif
};
GpuTemporalAa::GpuTemporalAa():impl_(std::make_unique<Impl>()) {}
GpuTemporalAa::~GpuTemporalAa()=default;
void* GpuTemporalAa::enqueue(void* device,void* command,void* color,void* depth,void* motion,
    void* packed,const GpuTemporalAaSettings& s) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!device || !command || !color || !depth || !motion || !packed || !s.width || !s.height
            || s.width>8192 || s.height>8192 || impl_->pending)
            throw std::runtime_error("Invalid or uncommitted TAA inputs");
        for(auto v:s.projection) if(!std::isfinite(v)) throw std::runtime_error("Invalid TAA projection");
        for(auto v:s.previous_z) if(!std::isfinite(v)) throw std::runtime_error("Invalid TAA camera mapping");
        for(auto v:s.jitter) if(!std::isfinite(v)) throw std::runtime_error("Invalid TAA jitter");
        if(!std::isfinite(s.weight)) throw std::runtime_error("Invalid TAA history weight");
        if(s.projection[0]==0 || s.projection[1]==0) throw std::runtime_error("Zero TAA focal length");
        if(impl_->device!=device || impl_->width!=s.width || impl_->height!=s.height) {
            impl_=std::make_unique<Impl>();impl_->initialize(static_cast<SDL_GPUDevice*>(device),s.width,s.height);
        }
        const unsigned next=1-impl_->current;
        for(auto* t:impl_->colors) if(t==color) throw std::runtime_error("Aliased TAA color input");
        struct Constants {Uint32 w,h,reset,pad;float weight,padding[3];std::array<float,4> projection,z,jitter;};
        static_assert(sizeof(Constants)==80);
        Constants constants{s.width,s.height,!impl_->valid || impl_->epoch!=s.epoch,0,s.weight,{},s.projection,s.previous_z,s.jitter};
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_PushGPUComputeUniformData(cb,0,&constants,sizeof(constants));
        SDL_GPUStorageTextureReadWriteBinding outputs[2]{};
        outputs[0].texture=impl_->colors[next];outputs[1].texture=impl_->meta[next];
        auto* pass=SDL_BeginGPUComputePass(cb,outputs,2,nullptr,0);
        if(!pass) throw std::runtime_error(SDL_GetError());
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        SDL_GPUTexture* textures[]{static_cast<SDL_GPUTexture*>(color),impl_->colors[impl_->current],impl_->meta[impl_->current]};
        SDL_GPUBuffer* buffers[]{static_cast<SDL_GPUBuffer*>(depth),static_cast<SDL_GPUBuffer*>(motion),static_cast<SDL_GPUBuffer*>(packed)};
        SDL_BindGPUComputeStorageTextures(pass,0,textures,3);SDL_BindGPUComputeStorageBuffers(pass,0,buffers,3);
        SDL_DispatchGPUCompute(pass,(s.width+7)/8,(s.height+7)/8,1);SDL_EndGPUComputePass(pass);
        if(s.jitter[0]!=0 || s.jitter[1]!=0) {
            constants.pad=1;SDL_PushGPUComputeUniformData(cb,0,&constants,sizeof(constants));
            outputs[0].texture=impl_->presentation;outputs[1].texture=impl_->meta[impl_->current];
            pass=SDL_BeginGPUComputePass(cb,outputs,2,nullptr,0);
            if(!pass) throw std::runtime_error(SDL_GetError());
            SDL_BindGPUComputePipeline(pass,impl_->pipeline);
            textures[0]=textures[1]=impl_->colors[next];textures[2]=impl_->meta[next];
            SDL_BindGPUComputeStorageTextures(pass,0,textures,3);SDL_BindGPUComputeStorageBuffers(pass,0,buffers,3);
            SDL_DispatchGPUCompute(pass,(s.width+7)/8,(s.height+7)/8,1);SDL_EndGPUComputePass(pass);
        }
        impl_->pending=true;impl_->next_epoch=s.epoch;impl_->status="TAA frame encoded";
        return (s.jitter[0]!=0 || s.jitter[1]!=0)?impl_->presentation:impl_->colors[next];
    } catch(const std::exception& e) {impl_->status=e.what();impl_->valid=false;}
#else
    (void)device;(void)command;(void)color;(void)depth;(void)motion;(void)packed;(void)s;
#endif
    return nullptr;
}
void GpuTemporalAa::commit() noexcept {if(impl_->pending) {impl_->current=1-impl_->current;impl_->epoch=impl_->next_epoch;impl_->valid=true;impl_->pending=false;}}
void GpuTemporalAa::discard() noexcept {impl_->pending=false;impl_->valid=false;}
void GpuTemporalAa::release_device() noexcept {impl_=std::make_unique<Impl>();}
const std::string& GpuTemporalAa::status() const {return impl_->status;}
}
