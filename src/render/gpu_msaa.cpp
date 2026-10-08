#include "starfox/render/gpu_msaa.hpp"
#include "starfox/render/gpu_clip.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/msaa_portable.hpp"
#include "shaders/generated/msaa_pack_portable.hpp"
#endif
namespace starfox::render {
struct GpuMsaa::Impl {
    std::string status{"MSAA not initialized"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{},*pack_pipeline{};SDL_GPUTexture* output{},*empty_background{};
    SDL_GPUBuffer *packed{},*kinds{},*sample_buffer{};unsigned capacity{},sample_bytes{},sample_count{};
    unsigned width{},height{};
    bool indexed{};
    ~Impl() {
        if(!device) return;
        if(output) SDL_ReleaseGPUTexture(device,output);
        if(empty_background) SDL_ReleaseGPUTexture(device,empty_background);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(pack_pipeline) SDL_ReleaseGPUComputePipeline(device,pack_pipeline);
        if(packed) SDL_ReleaseGPUBuffer(device,packed);
        if(kinds) SDL_ReleaseGPUBuffer(device,kinds);
        if(sample_buffer) SDL_ReleaseGPUBuffer(device,sample_buffer);
    }
    void initialize(SDL_GPUDevice* d) {
        device=d;SDL_GPUComputePipelineCreateInfo p{};const auto formats=SDL_GetGPUShaderFormats(d);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {p.format=SDL_GPU_SHADERFORMAT_SPIRV;p.code=msaa_shader::spirv;p.code_size=sizeof(msaa_shader::spirv);p.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {p.format=SDL_GPU_SHADERFORMAT_DXIL;p.code=msaa_shader::dxil;p.code_size=sizeof(msaa_shader::dxil);p.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {p.format=SDL_GPU_SHADERFORMAT_MSL;p.code=reinterpret_cast<const Uint8*>(msaa_shader::metal);p.code_size=sizeof(msaa_shader::metal)-1;p.entrypoint="main0";}
        else throw std::runtime_error("MSAA requires compute shaders");
        p.num_readonly_storage_textures=1;p.num_readonly_storage_buffers=6;p.num_readwrite_storage_textures=1;p.num_readwrite_storage_buffers=1;p.num_uniform_buffers=1;
        p.threadcount_x=p.threadcount_y=8;p.threadcount_z=1;
        pipeline=create_gpu_compute_pipeline(d,&p);if(!pipeline) throw std::runtime_error(SDL_GetError());
        SDL_GPUTextureCreateInfo t{};t.type=SDL_GPU_TEXTURETYPE_2D;t.width=t.height=t.layer_count_or_depth=t.num_levels=1;
        t.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;t.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
        empty_background=SDL_CreateGPUTexture(d,&t);if(!empty_background) throw std::runtime_error(SDL_GetError());
    }
    void resize(unsigned w,unsigned h) {
        if(width==w && height==h && output) return;
        SDL_GPUTextureCreateInfo t{};t.type=SDL_GPU_TEXTURETYPE_2D;t.width=w;t.height=h;t.layer_count_or_depth=t.num_levels=1;
        t.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        t.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        auto* next=SDL_CreateGPUTexture(device,&t);if(!next) throw std::runtime_error(SDL_GetError());
        if(output) SDL_ReleaseGPUTexture(device,output);
        output=next;width=w;height=h;
    }
    void prepare_pack(unsigned count) {
        if(!pack_pipeline) {
            SDL_GPUComputePipelineCreateInfo p{};const auto formats=SDL_GetGPUShaderFormats(device);
            if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {p.format=SDL_GPU_SHADERFORMAT_SPIRV;p.code=msaa_pack_shader::spirv;p.code_size=sizeof(msaa_pack_shader::spirv);p.entrypoint="main";}
            else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {p.format=SDL_GPU_SHADERFORMAT_DXIL;p.code=msaa_pack_shader::dxil;p.code_size=sizeof(msaa_pack_shader::dxil);p.entrypoint="main";}
            else {p.format=SDL_GPU_SHADERFORMAT_MSL;p.code=reinterpret_cast<const Uint8*>(msaa_pack_shader::metal);p.code_size=sizeof(msaa_pack_shader::metal)-1;p.entrypoint="main0";}
            p.num_readonly_storage_buffers=4;p.num_readwrite_storage_buffers=2;p.num_uniform_buffers=1;
            p.threadcount_x=32;p.threadcount_y=p.threadcount_z=1;
            pack_pipeline=create_gpu_compute_pipeline(device,&p);if(!pack_pipeline) throw std::runtime_error(SDL_GetError());
        }
        if(capacity>=count) return;
        SDL_GPUBufferCreateInfo b{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,Uint32(count*128*sizeof(GpuMsaaTriangle)),0};
        auto* next=SDL_CreateGPUBuffer(device,&b);if(!next) throw std::runtime_error(SDL_GetError());
        b.size=count*4;auto* next_kinds=SDL_CreateGPUBuffer(device,&b);
        if(!next_kinds) {SDL_ReleaseGPUBuffer(device,next);throw std::runtime_error(SDL_GetError());}
        if(packed) SDL_ReleaseGPUBuffer(device,packed);
        if(kinds) SDL_ReleaseGPUBuffer(device,kinds);
        packed=next;kinds=next_kinds;capacity=count;
    }
    void prepare_samples(unsigned w,unsigned h,unsigned count,bool retain) {
        const auto bytes=retain?std::uint64_t(w)*h*count*8:16;
        if(bytes>UINT32_MAX) throw std::runtime_error("MSAA sample allocation exceeds buffer limit");
        if(sample_buffer && sample_bytes==bytes) return;
        SDL_GPUBufferCreateInfo b{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,Uint32(bytes),0};
        auto* next=SDL_CreateGPUBuffer(device,&b);if(!next) throw std::runtime_error(SDL_GetError());
        if(sample_buffer) SDL_ReleaseGPUBuffer(device,sample_buffer);
        sample_buffer=next;sample_bytes=Uint32(bytes);
    }
#endif
};
GpuMsaa::GpuMsaa():impl_(std::make_unique<Impl>()) {}
GpuMsaa::~GpuMsaa()=default;
GpuMsaaFaces GpuMsaa::pack_faces(void* device,void* command,void* clipped,void* materials,
    void* order,unsigned count,unsigned polygon_count,bool fractional,std::array<float,2> scale,const GpuSpanOrder* bsp,unsigned line_thickness,std::array<unsigned,4> repeated_masks) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!device || !command || !clipped || !materials || !count || count>16384 || !polygon_count
            || (!order && !bsp && count>polygon_count) || !std::isfinite(scale[0]) || !std::isfinite(scale[1]) || scale[0]<=0 || scale[1]<=0
            || line_thickness<1 || line_thickness>4 || (bsp && (order || !bsp->indices || !bsp->results || bsp->capacity!=count || bsp->first>UINT32_MAX-count)))
            throw std::runtime_error("Invalid MSAA clipped-face inputs");
        if((repeated_masks[2]==0 && (repeated_masks[0] || repeated_masks[1] || repeated_masks[3])) ||
            (repeated_masks[2]!=0 && (repeated_masks[2]>8192 || repeated_masks[1]==0 ||
                (repeated_masks[0]&3U) || (repeated_masks[1]&3U) ||
                !msaa_sample_count(repeated_masks[3]) ||
                std::uint64_t(count)*repeated_masks[2]*repeated_masks[1]*(repeated_masks[3]+1)+repeated_masks[0]>256U*1024*1024)))
            throw std::runtime_error("Invalid MSAA repeated-row mask layout");
        if(bsp) order=bsp->indices;
        if(impl_->device!=device || !impl_->pipeline) {impl_=std::make_unique<Impl>();impl_->initialize(static_cast<SDL_GPUDevice*>(device));}
        if((impl_->packed && (clipped==impl_->packed || materials==impl_->packed || order==impl_->packed))
            || (impl_->kinds && (clipped==impl_->kinds || materials==impl_->kinds || order==impl_->kinds))
            || (bsp && (bsp->results==impl_->packed || bsp->results==impl_->kinds)))
            throw std::runtime_error("Aliased MSAA clipped-face inputs");
        impl_->prepare_pack(count);auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        struct Settings {Uint32 count,polygons,fractional,ordered;float x,y;Uint32 pad[2];Uint32 line[4];Uint32 mask[4];};
        const Settings settings{count,polygon_count,fractional?1U:0U,bsp?2U:order?1U:0U,scale[0],scale[1],{bsp?bsp->first:0,bsp?bsp->tree_index:0},{line_thickness,repeated_masks[0],repeated_masks[1],repeated_masks[2]},{repeated_masks[3],0,0,0}};
        SDL_PushGPUComputeUniformData(cb,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding outputs[2]{};outputs[0].buffer=impl_->packed;outputs[1].buffer=impl_->kinds;
        outputs[0].cycle=outputs[1].cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cb,nullptr,0,outputs,2);if(!pass) throw std::runtime_error(SDL_GetError());
        SDL_BindGPUComputePipeline(pass,impl_->pack_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(clipped),static_cast<SDL_GPUBuffer*>(materials),static_cast<SDL_GPUBuffer*>(order?order:clipped),static_cast<SDL_GPUBuffer*>(bsp?bsp->results:clipped)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,4);SDL_DispatchGPUCompute(pass,(count+31)/32,1,1);SDL_EndGPUComputePass(pass);
        impl_->status="MSAA clipped faces packed resident";return {impl_->packed,impl_->kinds,count*128};
    } catch(const std::exception& e) {impl_->status=e.what();}
#else
    (void)device;(void)command;(void)clipped;(void)materials;(void)order;(void)count;(void)polygon_count;(void)fractional;(void)scale;(void)bsp;(void)line_thickness;
#endif
    return {};
}
void* GpuMsaa::enqueue(void* device,void* command,void* background,void* triangles,void* palette,
    unsigned w,unsigned h,unsigned count,unsigned samples,const GpuMsaaSamples* previous,bool retain_samples,void* texels,unsigned texel_bytes,bool packed_faces,const GpuMsaaLayer* layer) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->sample_count=0;
    try {
        if(!device || !command || (!background && !layer) || (!triangles && count) || (!triangles && !layer) || !palette || !w || !h || w>8192 || h>8192 || !msaa_sample_count(samples) || (texel_bytes && !texels) || (packed_faces && count%128)
            || (layer && (!layer->pixels || (layer->face_count && !layer->kinds)
                || (packed_faces && layer->face_count!=count/128))))
            throw std::runtime_error("Invalid MSAA inputs");
        if(impl_->device!=device || !impl_->pipeline) {impl_=std::make_unique<Impl>();impl_->initialize(static_cast<SDL_GPUDevice*>(device));}
        if(background && impl_->output==background) throw std::runtime_error("Aliased MSAA background");
        if(previous && (!previous->buffer || previous->device!=device || previous->width!=w || previous->height!=h || previous->count!=samples || previous->indexed!=(layer!=nullptr)))
            throw std::runtime_error("Mismatched MSAA continuation samples");
        const bool in_place=layer && retain_samples && previous && previous->buffer==impl_->sample_buffer;
        if(layer && layer->resolve_only && !in_place) throw std::runtime_error("MSAA resolve requires owned indexed samples");
        if(in_place && (impl_->width!=w || impl_->height!=h || impl_->sample_bytes!=std::uint64_t(w)*h*samples*8))
            throw std::runtime_error("Stale in-place MSAA continuation samples");
        if((previous && previous->buffer==impl_->sample_buffer && !in_place) || (triangles && triangles==impl_->sample_buffer) || palette==impl_->sample_buffer || (texels && texels==impl_->sample_buffer)
            || (layer && (layer->pixels==impl_->sample_buffer || (layer->kinds && layer->kinds==impl_->sample_buffer))))
            throw std::runtime_error("Aliased MSAA continuation samples");
        impl_->prepare_samples(w,h,samples,retain_samples);
        impl_->resize(w,h);auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        const std::array<Uint32,12> constants{w,h,count,samples,previous?1U:0U,retain_samples?1U:0U,texel_bytes,packed_faces?1U:0U,layer?1U:0U,layer?layer->face_count:0U,in_place?1U:0U,layer && layer->resolve_only?1U:0U};SDL_PushGPUComputeUniformData(cb,0,constants.data(),sizeof(constants));
        SDL_GPUStorageTextureReadWriteBinding target{};target.texture=impl_->output;
        SDL_GPUStorageBufferReadWriteBinding sample_target{};sample_target.buffer=impl_->sample_buffer;
        auto* pass=SDL_BeginGPUComputePass(cb,&target,1,&sample_target,1);if(!pass) throw std::runtime_error(SDL_GetError());
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        auto* color=static_cast<SDL_GPUTexture*>(background?background:impl_->empty_background);SDL_BindGPUComputeStorageTextures(pass,0,&color,1);
        auto* dummy=static_cast<SDL_GPUBuffer*>(palette);
        SDL_GPUBuffer* buffers[]{triangles?static_cast<SDL_GPUBuffer*>(triangles):dummy,dummy,previous && !in_place?static_cast<SDL_GPUBuffer*>(previous->buffer):dummy,texels?static_cast<SDL_GPUBuffer*>(texels):dummy,
            layer?static_cast<SDL_GPUBuffer*>(layer->pixels):dummy,layer && layer->kinds?static_cast<SDL_GPUBuffer*>(layer->kinds):dummy};
        SDL_BindGPUComputeStorageBuffers(pass,0,buffers,6);SDL_DispatchGPUCompute(pass,(w+7)/8,(h+7)/8,1);SDL_EndGPUComputePass(pass);
        impl_->sample_count=retain_samples?samples:0;
        impl_->indexed=layer!=nullptr;
        impl_->status="MSAA coverage and resolve encoded";return impl_->output;
    } catch(const std::exception& e) {impl_->status=e.what();}
#else
    (void)device;(void)command;(void)background;(void)triangles;(void)palette;(void)w;(void)h;(void)count;(void)samples;(void)previous;(void)retain_samples;(void)texels;(void)texel_bytes;(void)packed_faces;(void)layer;
#endif
    return nullptr;
}
GpuMsaaSamples GpuMsaa::sample_output() const {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(impl_->sample_count) return {impl_->device,impl_->sample_buffer,impl_->width,impl_->height,impl_->sample_count,impl_->indexed};
#endif
    return {};
}
void* GpuMsaa::resolve_scene(void* command,void* palette) {
    const auto previous=sample_output();
    if(!previous.indexed || !previous.buffer) {impl_->status="No retained indexed MSAA coverage";return nullptr;}
    const GpuMsaaLayer layer{palette,nullptr,0,true};
    return enqueue(previous.device,command,nullptr,nullptr,palette,previous.width,previous.height,0,previous.count,&previous,true,nullptr,0,false,&layer);
}
void GpuMsaa::release_device() noexcept {impl_=std::make_unique<Impl>();}
const std::string& GpuMsaa::status() const {return impl_->status;}
}
