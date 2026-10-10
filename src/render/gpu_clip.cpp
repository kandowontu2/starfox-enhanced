#include "starfox/render/gpu_clip.hpp"
#include "starfox/compat/bit_cast.hpp"
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/gpu_scene_counters.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "starfox/render/gpu_dispatch.hpp"
#include "starfox/render/span_clear_policy.hpp"
#include "shaders/generated/clip_portable.hpp"
#include "shaders/generated/clip_continuous_portable.hpp"
#include "shaders/generated/clip_small_portable.hpp"
#include "shaders/generated/clip_continuous_small_portable.hpp"
#include "shaders/generated/clip_continuous_identity_portable.hpp"
#include "shaders/generated/clip_continuous_cached_portable.hpp"
#include "shaders/generated/clip_continuous_radix16_portable.hpp"
#include "shaders/generated/clip_continuous_interior_portable.hpp"
#include "shaders/generated/spans_portable.hpp"
#include "shaders/generated/spans_colour_xy_portable.hpp"
#include "shaders/generated/spans_cooperative_portable.hpp"
#include "shaders/generated/span_clear_portable.hpp"
#include <cstring>
#include <bit>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#endif
namespace starfox::render {
struct GpuClip::Impl {
    std::string status{"Native GPU clipping unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUBuffer* output{};std::uint32_t capacity{};
    SDL_GPUComputePipeline* spans_pipeline{};
    SDL_GPUComputePipeline* colour_spans_pipeline{};unsigned reported_span_tracing{};
    SDL_GPUComputePipeline* cooperative_spans_pipeline{};
    SDL_GPUComputePipeline* span_clear_pipeline{};
    SDL_GPUBuffer* spans{};std::uint32_t spans_capacity{};
    unsigned reported_span_clear_modes{};
    SDL_GPUBuffer* masks{};std::uint32_t masks_capacity{};
    NativeClipSettings settings{};
    SDL_GPUComputePipeline* continuous_pipeline{};bool continuous{};
    SDL_GPUComputePipeline* small_pipeline{};SDL_GPUComputePipeline* small_continuous_pipeline{};
    SDL_GPUComputePipeline* identity_pipeline{};bool reported_identity{};
    SDL_GPUComputePipeline* cached_pipeline{};bool reported_cached{};
    SDL_GPUComputePipeline* radix16_pipeline{};bool reported_radix16{};
    SDL_GPUComputePipeline* interior_pipeline{};bool reported_interior{};
    unsigned reported_clip_modes{};
    ~Impl(){release();}
    static void require(bool ok){if(!ok) throw std::runtime_error(SDL_GetError());}
    void release() noexcept {
        if(small_pipeline) SDL_ReleaseGPUComputePipeline(device,small_pipeline);
        if(small_continuous_pipeline) SDL_ReleaseGPUComputePipeline(device,small_continuous_pipeline);
        small_pipeline=small_continuous_pipeline=nullptr;reported_clip_modes=0;
        if(identity_pipeline) SDL_ReleaseGPUComputePipeline(device,identity_pipeline);
        identity_pipeline=nullptr;reported_identity=false;
        if(cached_pipeline) SDL_ReleaseGPUComputePipeline(device,cached_pipeline);
        cached_pipeline=nullptr;reported_cached=false;
        if(radix16_pipeline) SDL_ReleaseGPUComputePipeline(device,radix16_pipeline);
        radix16_pipeline=nullptr;reported_radix16=false;
        if(interior_pipeline) SDL_ReleaseGPUComputePipeline(device,interior_pipeline);
        interior_pipeline=nullptr;reported_interior=false;
        if(spans) SDL_ReleaseGPUBuffer(device,spans);
        if(masks) SDL_ReleaseGPUBuffer(device,masks);
        masks=nullptr;masks_capacity=0;
        if(spans_pipeline) SDL_ReleaseGPUComputePipeline(device,spans_pipeline);
        if(colour_spans_pipeline) SDL_ReleaseGPUComputePipeline(device,colour_spans_pipeline);
        colour_spans_pipeline=nullptr;reported_span_tracing=0;
        if(cooperative_spans_pipeline) SDL_ReleaseGPUComputePipeline(device,cooperative_spans_pipeline);
        cooperative_spans_pipeline=nullptr;
        if(span_clear_pipeline) SDL_ReleaseGPUComputePipeline(device,span_clear_pipeline);
        span_clear_pipeline=nullptr;reported_span_clear_modes=0;
        if(continuous_pipeline) SDL_ReleaseGPUComputePipeline(device,continuous_pipeline);
        continuous_pipeline=nullptr;continuous=false;
        spans=nullptr;spans_pipeline=nullptr;spans_capacity=0;settings={};
        if(output) SDL_ReleaseGPUBuffer(device,output);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        output=nullptr;pipeline=nullptr;device=nullptr;capacity=0;
    }
    void initialize(SDL_GPUDevice* next) {
        if(device==next) return;
        release();
        if(!(SDL_GetGPUShaderFormats(next)&(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_MSL)))
            throw std::runtime_error("Native clipping requires Vulkan, Metal or D3D12");
        device=next;
    }
    enum class Stage { native, continuous, spans };
    SDL_GPUComputePipeline* prepare_stage(Stage stage) {
        auto& target=stage==Stage::native?pipeline:stage==Stage::continuous?continuous_pipeline:spans_pipeline;
        if(target) return target;
        // Policy switches within an ordered recording keep existing pipelines
        // and outputs alive. A clip-only or specialized path must not compile
        // unrelated native/continuous/span shaders just to initialize a device.
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        const bool intel_dxil=dxil && SDL_GetNumberProperty(SDL_GetGPUDeviceProperties(device),
            "starfox.gpu.vendor_id",0)==0x8086;
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        if(spirv) {
            if(stage==Stage::native) {info.code=clip_shader::spirv;info.code_size=sizeof(clip_shader::spirv);}
            else if(stage==Stage::continuous) {info.code=clip_continuous_shader::spirv;info.code_size=sizeof(clip_continuous_shader::spirv);}
            else {info.code=spans_shader::spirv;info.code_size=sizeof(spans_shader::spirv);}
        }
#if defined(_WIN32)
        else if(dxil) {
            if(stage==Stage::native) {info.code=clip_shader::dxil;info.code_size=sizeof(clip_shader::dxil);}
            else if(stage==Stage::continuous) {
                info.code=intel_dxil?clip_continuous_shader::intel_dxil:clip_continuous_shader::dxil;
                info.code_size=intel_dxil?sizeof(clip_continuous_shader::intel_dxil):sizeof(clip_continuous_shader::dxil);
            } else {info.code=spans_shader::dxil;info.code_size=sizeof(spans_shader::dxil);}
        }
#endif
#if defined(__APPLE__)
        else if(stage==Stage::native) {info.code=reinterpret_cast<const Uint8*>(clip_shader::metal);info.code_size=std::strlen(clip_shader::metal);}
        else if(stage==Stage::continuous) {info.code=reinterpret_cast<const Uint8*>(clip_continuous_shader::metal);info.code_size=std::strlen(clip_continuous_shader::metal);}
        else {info.code=reinterpret_cast<const Uint8*>(spans_shader::metal);info.code_size=std::strlen(spans_shader::metal);}
#else
        else throw std::runtime_error("Native clipping shader unavailable for this platform");
#endif
        info.entrypoint=(spirv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=stage==Stage::spans?4:stage==Stage::continuous?6:5;
        info.num_readwrite_storage_buffers=stage==Stage::spans?2:1;
        info.num_uniform_buffers=1;info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
        const bool trace=SDL_getenv("STARFOX_TRACE_GPU_MODEL_DISPATCH")!=nullptr;
        const auto* name=stage==Stage::native?"native":stage==Stage::continuous?"continuous":"spans";
        if(trace) std::cerr<<"clip-pipeline: creating "<<name
            <<(stage==Stage::continuous && intel_dxil?" intel-compact":"")<<'\n';
        target=create_gpu_compute_pipeline(device,&info);require(target);
        if(trace) std::cerr<<"clip-pipeline: ready "<<name<<'\n';
        return target;
    }
    SDL_GPUComputePipeline* prepare_colour_spans() {
        if(colour_spans_pipeline) return colour_spans_pipeline;
        SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(device);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;
            info.code=spans_colour_xy_shader::spirv;info.code_size=sizeof(spans_colour_xy_shader::spirv);
        }
#if defined(_WIN32)
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;
            info.code=spans_colour_xy_shader::dxil;info.code_size=sizeof(spans_colour_xy_shader::dxil);
        }
#endif
        else throw std::runtime_error("Colour span experiment requires executed Vulkan/D3D12 backends");
        info.entrypoint="main";info.num_readonly_storage_buffers=4;
        info.num_readwrite_storage_buffers=2;info.num_uniform_buffers=1;
        info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
        colour_spans_pipeline=create_gpu_compute_pipeline(device,&info);
        require(colour_spans_pipeline);return colour_spans_pipeline;
    }
    SDL_GPUComputePipeline* prepare_cooperative_spans() {
        if(cooperative_spans_pipeline) return cooperative_spans_pipeline;
        SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(device);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;
            info.code=spans_cooperative_shader::spirv;info.code_size=sizeof(spans_cooperative_shader::spirv);
        }
#if defined(_WIN32)
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;
            info.code=spans_cooperative_shader::dxil;info.code_size=sizeof(spans_cooperative_shader::dxil);
        }
#endif
        else throw std::runtime_error("Cooperative span experiment requires executed Vulkan/D3D12 backends");
        info.entrypoint="main";info.num_readonly_storage_buffers=4;
        info.num_readwrite_storage_buffers=2;info.num_uniform_buffers=1;
        info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
        cooperative_spans_pipeline=create_gpu_compute_pipeline(device,&info);
        require(cooperative_spans_pipeline);return cooperative_spans_pipeline;
    }
    SDL_GPUComputePipeline* prepare_span_clear() {
        if(span_clear_pipeline) return span_clear_pipeline;
        SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(device);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;
            info.code=span_clear_shader::spirv;info.code_size=sizeof(span_clear_shader::spirv);
        }
#if defined(_WIN32)
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;
            info.code=span_clear_shader::dxil;info.code_size=sizeof(span_clear_shader::dxil);
        }
#endif
#if defined(__APPLE__)
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;
            info.code=reinterpret_cast<const Uint8*>(span_clear_shader::metal);
            info.code_size=std::strlen(span_clear_shader::metal);
        }
#endif
        else throw std::runtime_error("Parallel span clear unavailable for this platform");
        info.entrypoint=info.format==SDL_GPU_SHADERFORMAT_MSL?"main0":"main";
        info.num_readwrite_storage_buffers=2;info.num_uniform_buffers=1;
        info.threadcount_x=128;info.threadcount_y=info.threadcount_z=1;
        span_clear_pipeline=create_gpu_compute_pipeline(device,&info);
        require(span_clear_pipeline);return span_clear_pipeline;
    }
    SDL_GPUComputePipeline* prepare_small(bool fractional) {
        auto& target=fractional?small_continuous_pipeline:small_pipeline;
        if(target) return target;
        SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(device);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;
            info.code=fractional?clip_continuous_small_shader::spirv:clip_small_shader::spirv;
            info.code_size=fractional?sizeof(clip_continuous_small_shader::spirv):sizeof(clip_small_shader::spirv);
        }
        else throw std::runtime_error("Small clipping experiment requires Vulkan");
        info.entrypoint=info.format==SDL_GPU_SHADERFORMAT_MSL?"main0":"main";
        info.num_readonly_storage_buffers=fractional?6:5;info.num_readwrite_storage_buffers=1;
        info.num_uniform_buffers=1;info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
        target=create_gpu_compute_pipeline(device,&info);require(target);return target;
    }
    SDL_GPUComputePipeline* prepare_continuous_variant(bool cached,bool radix16=false,bool interior=false) {
        auto& target=interior?interior_pipeline:radix16?radix16_pipeline:cached?cached_pipeline:identity_pipeline;
        if(target)return target;
        SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(device);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;
            info.code=interior?clip_continuous_interior_shader::spirv:radix16?clip_continuous_radix16_shader::spirv:cached?clip_continuous_cached_shader::spirv:clip_continuous_identity_shader::spirv;
            info.code_size=interior?sizeof(clip_continuous_interior_shader::spirv):radix16?sizeof(clip_continuous_radix16_shader::spirv):cached?sizeof(clip_continuous_cached_shader::spirv):sizeof(clip_continuous_identity_shader::spirv);
        }
#if defined(_WIN32)
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;
            const bool intel=SDL_GetNumberProperty(SDL_GetGPUDeviceProperties(device),"starfox.gpu.vendor_id",0)==0x8086;
            if(interior) {
                info.code=intel?clip_continuous_interior_shader::intel_dxil:clip_continuous_interior_shader::dxil;
                info.code_size=intel?sizeof(clip_continuous_interior_shader::intel_dxil):sizeof(clip_continuous_interior_shader::dxil);
            } else if(radix16) {
                info.code=intel?clip_continuous_radix16_shader::intel_dxil:clip_continuous_radix16_shader::dxil;
                info.code_size=intel?sizeof(clip_continuous_radix16_shader::intel_dxil):sizeof(clip_continuous_radix16_shader::dxil);
            } else if(cached) {
                info.code=intel?clip_continuous_cached_shader::intel_dxil:clip_continuous_cached_shader::dxil;
                info.code_size=intel?sizeof(clip_continuous_cached_shader::intel_dxil):sizeof(clip_continuous_cached_shader::dxil);
            } else {
                info.code=intel?clip_continuous_identity_shader::intel_dxil:clip_continuous_identity_shader::dxil;
                info.code_size=intel?sizeof(clip_continuous_identity_shader::intel_dxil):sizeof(clip_continuous_identity_shader::dxil);
            }
        }
#endif
#if defined(__APPLE__)
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;
            const auto* metal=interior?clip_continuous_interior_shader::metal:radix16?clip_continuous_radix16_shader::metal:cached?clip_continuous_cached_shader::metal:clip_continuous_identity_shader::metal;
            info.code=reinterpret_cast<const Uint8*>(metal);info.code_size=std::strlen(metal);
        }
#endif
        else throw std::runtime_error("Continuous clipping variant unavailable for this platform");
        info.entrypoint=info.format==SDL_GPU_SHADERFORMAT_MSL?"main0":"main";
        info.num_readonly_storage_buffers=6;info.num_readwrite_storage_buffers=1;
        info.num_uniform_buffers=1;info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
        target=create_gpu_compute_pipeline(device,&info);require(target);return target;
    }
#endif
};
GpuClip::GpuClip():impl_(std::make_unique<Impl>()){}
std::uint32_t GpuClip::mask_buffer_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_->masks_capacity;
#else
    return 0;
#endif
}
GpuClip::~GpuClip()=default;
const std::string& GpuClip::status()const noexcept{return impl_->status;}
void GpuClip::release_device()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
void* GpuClip::enqueue(void* device,void* command,void* points,void* corners,
    void* polygons,void* visibility,const NativeClipSettings& settings,bool continuous,
    void* projection_params,std::uint32_t projection_count,void* point_residuals,std::uint32_t residual_count,
    std::uint32_t verified_source_corners,std::uint32_t render_scale,std::array<std::uint32_t,2> raster_size) {
    if(!device || !command || !points || !corners || !polygons || !visibility
        || !settings.polygon_count || settings.polygon_count>65536
        || settings.width<=0 || settings.width>32767 || settings.height<=0 || settings.height>32767) {
        impl_->status="Invalid native clipping input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(points==impl_->output || corners==impl_->output || polygons==impl_->output || visibility==impl_->output
            || (projection_params && projection_params==impl_->output) || (point_residuals && point_residuals==impl_->output))
            throw std::runtime_error("Native clipping input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        // Opt-in only. Do not infer an untested Metal fast path from compiled
        // payloads, and let the explicit full-reference selector win.
        const bool interior=continuous && SDL_getenv("STARFOX_TEST_INTERIOR_CLIP")
            && (SDL_GetGPUShaderFormats(impl_->device)&(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL))!=0
            && !SDL_getenv("STARFOX_TEST_FULL_CLIP");
        const bool radix16=!interior && continuous && SDL_getenv("STARFOX_TEST_RADIX16_CLIP")
            && !SDL_getenv("STARFOX_TEST_FULL_CLIP");
        const bool cached=!interior && !radix16 && continuous && SDL_getenv("STARFOX_TEST_CACHED_PROJECTION_CLIP")
            && !SDL_getenv("STARFOX_TEST_FULL_CLIP");
        const bool identity=!interior && !radix16 && !cached && continuous && SDL_getenv("STARFOX_TEST_IDENTITY_CLIP")
            && !SDL_getenv("STARFOX_TEST_FULL_CLIP");
        const bool small=!interior && !radix16 && !cached && !identity && verified_source_corners!=0 && verified_source_corners<=4
            // The reflected-water executable exposes D3D12 device loss with
            // compact scratch despite its standalone oracle passing. Do not
            // select that backend (or unexecuted Metal) based on theory alone.
            && (SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0
            && SDL_getenv("STARFOX_TEST_SMALL_CLIP") && !SDL_getenv("STARFOX_TEST_FULL_CLIP");
        auto* selected_pipeline=(interior || radix16 || cached || identity)?impl_->prepare_continuous_variant(cached,radix16,interior):small?impl_->prepare_small(continuous):impl_->prepare_stage(continuous?Impl::Stage::continuous:Impl::Stage::native);
        if(impl_->capacity<settings.polygon_count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,settings.polygon_count*129*16,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->output) SDL_ReleaseGPUBuffer(impl_->device,impl_->output);
            impl_->output=replacement;impl_->capacity=settings.polygon_count;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const bool trace=SDL_getenv("STARFOX_TRACE_GPU_MODEL_DISPATCH")!=nullptr;
        if(trace) std::cerr<<"clip-enqueue: uniforms continuous="<<continuous
            <<" polygons="<<settings.polygon_count<<" points="<<settings.point_count
            <<" corners="<<settings.corner_count<<" residuals="<<residual_count<<'\n';
        struct {NativeClipSettings clip;float stored_scale[2];Uint32 padding[2];} uniforms{settings,{},{}};
        uniforms.clip.reserved[0]=projection_params?projection_count:0;
        uniforms.clip.reserved[1]=continuous && point_residuals?residual_count:0;
        // Same stored scale as enqueue_spans' pointAt (rasterScale or renderScale).
        const bool custom=raster_size[0] || raster_size[1];
        uniforms.stored_scale[0]=custom?float(raster_size[0])/settings.width:float(render_scale);
        uniforms.stored_scale[1]=custom?float(raster_size[1])/settings.height:float(render_scale);
        SDL_PushGPUComputeUniformData(cmd,0,&uniforms,continuous?sizeof(uniforms):sizeof(uniforms.clip));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->output;binding.cycle=true;
        if(trace) std::cerr<<"clip-enqueue: begin\n";
        auto* pass=scene_counters::begin_compute_pass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        if(trace) std::cerr<<"clip-enqueue: bind pipeline\n";
        SDL_BindGPUComputePipeline(pass,selected_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(points),static_cast<SDL_GPUBuffer*>(corners),
            static_cast<SDL_GPUBuffer*>(polygons),static_cast<SDL_GPUBuffer*>(visibility),
            static_cast<SDL_GPUBuffer*>(projection_params?projection_params:points),
            static_cast<SDL_GPUBuffer*>(point_residuals?point_residuals:points)};
        if(trace) std::cerr<<"clip-enqueue: bind buffers\n";
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,continuous?6:5);
        if(trace) std::cerr<<"clip-enqueue: dispatch\n";
        SDL_DispatchGPUCompute(pass,(settings.polygon_count+31)/32,1,1);
        if(trace) std::cerr<<"clip-enqueue: end pass\n";
        SDL_EndGPUComputePass(pass);
        if(interior && !impl_->reported_interior && SDL_getenv("STARFOX_TEST_CLIP_INTERIOR_RESULT")) {
            std::cerr<<"clip-interior: exact binary64 output shortcut\n";impl_->reported_interior=true;
        }
        if(radix16 && !impl_->reported_radix16 && SDL_getenv("STARFOX_TEST_CLIP_RADIX16_RESULT")) {
            std::cerr<<"clip-radix16: exact binary64 division\n";impl_->reported_radix16=true;
        }
        if(cached && !impl_->reported_cached && SDL_getenv("STARFOX_TEST_CLIP_PROJECTION_CACHE_RESULT")) {
            std::cerr<<"clip-projection-cache: exact front raw screens\n";impl_->reported_cached=true;
        }
        if(identity && !impl_->reported_identity && SDL_getenv("STARFOX_TEST_CLIP_IDENTITY_RESULT")) {
            std::cerr<<"clip-identity: exact interior planes\n";impl_->reported_identity=true;
        }
        const auto mode=1U<<((small?2U:0U)+(continuous?1U:0U));
        if(SDL_getenv("STARFOX_TEST_CLIP_SCRATCH_RESULT") && !(impl_->reported_clip_modes&mode)) {
            std::cerr<<"clip-scratch: "<<(small?"small":"full")<<" continuous="<<continuous<<'\n';
            impl_->reported_clip_modes|=mode;
        }
        if(trace) std::cerr<<"clip-enqueue: done\n";
        impl_->settings=settings;
        impl_->continuous=continuous;
        impl_->status=continuous?"Continuous screen clipping GPU resident":"Native screen clipping GPU resident";return impl_->output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuClip::enqueue_spans(void* command,void* materials,bool winding_independent,std::uint32_t render_scale,const GpuSpanOrder* order,std::uint32_t line_thickness,void* source_texels,std::uint32_t source_texel_bytes,void** masked_texels,std::array<std::uint32_t,2> raster_size,bool reuse_span_scratch,unsigned msaa_samples,bool parallel_clear_requested) {
    const bool msaa_masks=msaa_samples!=0;
    if(masked_texels) *masked_texels=nullptr;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(render_scale<1 || render_scale>max_gpu_render_scale) throw std::runtime_error("Invalid span render scale");
        if(msaa_masks && !masked_texels) throw std::runtime_error("MSAA span samples require mask storage");
        if(msaa_samples!=0 && msaa_samples!=2 && msaa_samples!=4 && msaa_samples!=8) throw std::runtime_error("Invalid span sample count");
        if(!command || !materials || !impl_->settings.polygon_count || !impl_->output)
            throw std::runtime_error("Solid span emission requires clipped polygons and materials");
        if(materials==impl_->spans || materials==impl_->output)
            throw std::runtime_error("Solid span materials alias scratch data");
        if(order && (!order->indices || !order->results || !order->capacity || order->capacity>65536
            || order->first>UINT32_MAX-order->capacity
            || order->indices==impl_->spans || order->results==impl_->spans))
            throw std::runtime_error("Invalid or aliased BSP span order");
        const auto& s=impl_->settings;
        const auto slots=order?order->capacity:s.polygon_count;
        const bool custom=raster_size[0] || raster_size[1];
        if(custom && (!raster_size[0] || !raster_size[1] || !impl_->continuous))
            throw std::runtime_error("Custom raster size requires continuous geometry and two positive dimensions");
        const auto width=custom?raster_size[0]:Uint32(s.width)*render_scale,height=custom?raster_size[1]:Uint32(s.height)*render_scale;
        if(width>32767 || height>32767) throw std::runtime_error("Scaled span viewport exceeds fixed-point bounds");
        const auto bytes=std::uint64_t(slots)*height*96;
        if(bytes>256U*1024*1024) throw std::runtime_error("Solid span batch exceeds 256 MiB scratch budget");
        if(impl_->spans_capacity<bytes) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,Uint32(bytes),0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->spans) SDL_ReleaseGPUBuffer(impl_->device,impl_->spans);
            impl_->spans=replacement;impl_->spans_capacity=Uint32(bytes);
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const auto mask_stride=((width+31)/32)*4;
        const auto mask_bytes=masked_texels?std::uint64_t(slots)*height*mask_stride*(msaa_samples+1)+source_texel_bytes:4;
        if(mask_bytes>256U*1024*1024 || (source_texel_bytes&3U)
            || (source_texel_bytes && !source_texels) || (source_texels && source_texels==impl_->masks)
            || materials==impl_->masks || (order && (order->indices==impl_->masks || order->results==impl_->masks)))
            throw std::runtime_error("Invalid or oversized span mask storage");
        if(impl_->masks_capacity<mask_bytes) {
            if(SDL_getenv("STARFOX_TRACE_MSAA_MASKS")) std::cerr<<"span masks: "<<mask_bytes<<" bytes; "<<slots<<" slots, "<<width<<"x"<<height<<", "<<(msaa_samples+1)<<" planes\n";
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,Uint32(mask_bytes),0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->masks) SDL_ReleaseGPUBuffer(impl_->device,impl_->masks);
            impl_->masks=replacement;impl_->masks_capacity=Uint32(mask_bytes);
        }
        const bool copied=masked_texels && source_texel_bytes;
        if(copied) {
            auto* copy=SDL_BeginGPUCopyPass(cmd);Impl::require(copy);
            SDL_GPUBufferLocation from{static_cast<SDL_GPUBuffer*>(source_texels),0},to{impl_->masks,0};
            SDL_CopyGPUBufferToBuffer(copy,&from,&to,source_texel_bytes,true);SDL_EndGPUCopyPass(copy);
        }
        const Uint32 rows=slots*height;
        const Uint32 mask_words=msaa_masks?Uint32((mask_bytes-source_texel_bytes)/4):0;
        const bool force_full=SDL_getenv("STARFOX_TEST_FULL_SPAN_CLEAR")!=nullptr;
        const bool force_parallel=(parallel_clear_requested && !SDL_getenv("STARFOX_TEST_SERIAL_SPAN_CLEAR")) || SDL_getenv("STARFOX_TEST_PARALLEL_SPAN_CLEAR")!=nullptr;
        const bool force_bounds=SDL_getenv("STARFOX_TEST_SPAN_BOUNDS_CLEAR")!=nullptr;
        const auto clear_mode=span_clear_mode(rows,mask_words,force_full,force_parallel,force_bounds);
        const bool parallel_clear=clear_mode==SpanClearMode::parallel;
        const bool bounds_clear=clear_mode!=SpanClearMode::full;
        const bool cooperative_trace=SDL_getenv("STARFOX_TEST_COOPERATIVE_SPAN_TRACE")
            && !SDL_getenv("STARFOX_TEST_FULL_SPAN_TRACE")
            && (SDL_GetGPUShaderFormats(impl_->device)&(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL));
        const bool colour_trace=!cooperative_trace && SDL_getenv("STARFOX_TEST_COLOUR_SPAN_TRACE")
            && !SDL_getenv("STARFOX_TEST_FULL_SPAN_TRACE")
            && (SDL_GetGPUShaderFormats(impl_->device)&(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL));
        auto* spans_pipeline=cooperative_trace?impl_->prepare_cooperative_spans():colour_trace?impl_->prepare_colour_spans():impl_->prepare_stage(Impl::Stage::spans);
        Uint32 settings[]{slots,width,height,winding_independent?1U:0U,
            render_scale,impl_->continuous?1U:0U,order?1U:0U,s.polygon_count,
            order?order->first:0U,order?order->tree_index:0U,line_thickness,bounds_clear?1U:0U,
            masked_texels?1U:0U,source_texel_bytes,mask_stride,custom?1U:0U,
            starfox::bit_cast<Uint32>(float(width)/s.width),starfox::bit_cast<Uint32>(float(height)/s.height),msaa_samples,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding bindings[2]{};
        // Ordered model batches rasterize these rows before the next model
        // overwrites them. Cycling would retain a full polygon*height buffer
        // per terrain tile until submission completes (several GiB at 4x).
        bindings[0].buffer=impl_->spans;bindings[0].cycle=!reuse_span_scratch;
        bindings[1].buffer=impl_->masks;bindings[1].cycle=!copied;
        if(parallel_clear) {
            const auto dispatch=linear_gpu_dispatch128(std::max(rows,mask_words));
            const Uint32 clear_settings[]{rows,mask_words,source_texel_bytes,dispatch.row_stride};
            SDL_PushGPUComputeUniformData(cmd,0,clear_settings,sizeof(clear_settings));
            // A minimal writer needs no clipped/material/order inputs. It owns
            // the first span cycle and folds MSAA mask clearing into this pass,
            // preserving any texel prefix copied before it.
            const auto mask_cycle=bindings[1].cycle;
            if(!msaa_masks) bindings[1].cycle=false;
            auto* pipeline=impl_->prepare_span_clear();
            auto* clear=scene_counters::begin_compute_pass(cmd,nullptr,0,bindings,2);Impl::require(clear);
            SDL_BindGPUComputePipeline(clear,pipeline);
            SDL_DispatchGPUCompute(clear,dispatch.x,dispatch.y,1);SDL_EndGPUComputePass(clear);
            bindings[0].cycle=false;bindings[1].cycle=msaa_masks?false:mask_cycle;
            settings[11]|=2U;
            SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        }
        if(msaa_masks && !parallel_clear) {
            // Clearing every sample plane in the single per-face tracer thread
            // serialized megabytes of stores. Distribute it over the GPU first.
            settings[19]=1;SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
            const auto span_cycle=bindings[0].cycle;bindings[0].cycle=false;
            auto* clear_pipeline=impl_->prepare_stage(Impl::Stage::spans);
            auto* clear=scene_counters::begin_compute_pass(cmd,nullptr,0,bindings,2);Impl::require(clear);
            SDL_BindGPUComputePipeline(clear,clear_pipeline);
            SDL_GPUBuffer* clear_inputs[]{impl_->output,static_cast<SDL_GPUBuffer*>(materials),
                static_cast<SDL_GPUBuffer*>(order?order->indices:materials),static_cast<SDL_GPUBuffer*>(order?order->results:materials)};
            SDL_BindGPUComputeStorageBuffers(clear,0,clear_inputs,4);
            const auto groups=Uint32(((mask_bytes-source_texel_bytes)/4+31)/32);
            SDL_DispatchGPUCompute(clear,std::min(groups,65535U),(groups+65534U)/65535U,1);SDL_EndGPUComputePass(clear);
            bindings[0].cycle=span_cycle;bindings[1].cycle=false;
            settings[19]=0;SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        }
        auto* pass=scene_counters::begin_compute_pass(cmd,nullptr,0,bindings,2);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,spans_pipeline);
        SDL_GPUBuffer* inputs[]{impl_->output,static_cast<SDL_GPUBuffer*>(materials),
            static_cast<SDL_GPUBuffer*>(order?order->indices:materials),
            static_cast<SDL_GPUBuffer*>(order?order->results:materials)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,4);
        if(cooperative_trace) SDL_DispatchGPUCompute(pass,std::min(slots,65535U),(slots+65534U)/65535U,1);
        else SDL_DispatchGPUCompute(pass,(slots+31)/32,1,1);
        SDL_EndGPUComputePass(pass);
        const auto trace_bit=cooperative_trace?4U:colour_trace?2U:1U;
        if(SDL_getenv("STARFOX_TEST_SPAN_TRACE_RESULT") && !(impl_->reported_span_tracing&trace_bit)) {
            std::cerr<<"span-tracing: "<<(cooperative_trace?"cooperative rows":colour_trace?"colour XY-only":"full UV")<<'\n';
            impl_->reported_span_tracing|=trace_bit;
        }
        const auto clear_bit=1U<<static_cast<unsigned>(clear_mode);
        if(SDL_getenv("STARFOX_TEST_SPAN_CLEAR_RESULT") && !(impl_->reported_span_clear_modes&clear_bit)) {
            std::cerr<<"span-clear: "<<(parallel_clear?"parallel":bounds_clear?"bounds":"full")<<'\n';
            std::cerr<<"span-clear-policy: "<<(force_full || force_parallel || force_bounds?"forced":"automatic")
                <<" rows="<<rows<<" mask-words="<<mask_words<<'\n';
            impl_->reported_span_clear_modes|=clear_bit;
        }
        if(masked_texels) *masked_texels=impl_->masks;
        impl_->status="Native clipping and spans GPU resident";return impl_->spans;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
}
