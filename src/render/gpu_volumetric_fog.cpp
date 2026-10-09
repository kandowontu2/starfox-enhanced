#include "starfox/render/gpu_volumetric_fog.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <SDL3/SDL_gpu.h>
#include "shaders/generated/volumetric_portable.hpp"
#include <cstring>
#include <stdexcept>
#endif
namespace starfox::render {
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {
void require_fog(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
struct Float4 {float x{},y{},z{},w{};};
struct FogParameters {Float4 extent,projection,medium,albedo,ambient,sunlight,light,point,normal,origin;};
static_assert(sizeof(FogParameters)==160);
Float4 packed(shadows::Vec3 v) {return {float(v.x),float(v.y),float(v.z),0};}
}
struct GpuVolumetricFog::Impl {
    SDL_GPUDevice* device{};
    SDL_GPUComputePipeline* pipeline{};
    SDL_GPUBuffer* buffers[3]{};
    Uint32 capacity[3]{};
    SDL_GPUTransferBuffer *upload{},*download{};
    Uint32 upload_capacity{},download_capacity{};
    SDL_GPUCommandBuffer* command{};
    SDL_GPUFence* fence{};
    unsigned width{},height{};bool valid{};
    GpuVolumetricSceneOutput scene;
    std::string status{"GPU volumetric fog not initialized"};
    ~Impl() {
        if(!device) return;
        if(command) SDL_CancelGPUCommandBuffer(command);
        if(fence) {SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);}
        for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
    }
    void finish() {
        if(fence) {require_fog(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);fence=nullptr;}
    }
    void initialize(SDL_GPUDevice* d) {
        device=d;SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(d);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=volumetric_shader::spirv;
            info.code_size=sizeof(volumetric_shader::spirv);info.entrypoint="main";
        } else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=volumetric_shader::dxil;
            info.code_size=sizeof(volumetric_shader::dxil);info.entrypoint="main";
        } else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(volumetric_shader::metal);
            info.code_size=sizeof(volumetric_shader::metal)-1;info.entrypoint="main0";
        } else throw std::runtime_error("No supported GPU fog shader format");
        info.num_readonly_storage_buffers=2;info.num_readwrite_storage_buffers=1;
        info.num_uniform_buffers=1;info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        pipeline=create_gpu_compute_pipeline(d,&info);require_fog(pipeline);
        status=std::string("GPU fog: ")+SDL_GetGPUDeviceDriver(d);
    }
    void buffer(unsigned i,Uint32 bytes) {
        if(buffers[i]&&capacity[i]>=bytes) return;
        if(buffers[i]) SDL_ReleaseGPUBuffer(device,buffers[i]);
        buffers[i]=nullptr;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
            |(i==2?SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE:0U),bytes,0};
        buffers[i]=SDL_CreateGPUBuffer(device,&info);require_fog(buffers[i]);capacity[i]=bytes;
    }
    void transfer(SDL_GPUTransferBuffer*& b,Uint32& cap,Uint32 bytes,SDL_GPUTransferBufferUsage usage) {
        if(b&&cap>=bytes) return;
        if(b) SDL_ReleaseGPUTransferBuffer(device,b);
        b=nullptr;
        SDL_GPUTransferBufferCreateInfo info{usage,bytes,0};
        b=SDL_CreateGPUTransferBuffer(device,&info);require_fog(b);cap=bytes;
    }
    FogParameters parameters(VolumetricProjection projection,unsigned w,unsigned h,
        const VolumetricMedium& medium,shadows::Vec3 light,std::optional<VolumetricGround> ground,
        bool background_only,shadows::Vec3 origin,unsigned node_count) const {
        const auto pixels=std::uint64_t(w)*h;
        const auto finite=[](shadows::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
        // Zero distance validates the medium/light without reading any geometry.
        // The resident source was packed/validated by its producer already.
        const shadows::Scene empty;
        if(!w||!h||w>16384||h>16384||pixels>UINT32_MAX/16
            ||!std::isfinite(projection.focal_x)||projection.focal_x<=0
            ||!std::isfinite(projection.focal_y)||projection.focal_y<=0
            ||!std::isfinite(projection.center_x)||!std::isfinite(projection.center_y)
            ||!integrate_volumetric_fog(medium,origin,{0,0,1},0,light,empty)
            ||(ground&&(!finite(ground->point)||!finite(ground->normal)
                ||!std::isfinite(shadows::dot(ground->normal,ground->normal))
                ||shadows::dot(ground->normal,ground->normal)<1e-20)))
            throw std::runtime_error("Invalid GPU fog inputs");
        light=light*(1/std::sqrt(shadows::dot(light,light)));
        FogParameters p{{float(w),float(h),float(node_count),float(medium.samples)},
            {float(projection.focal_x),float(projection.focal_y),float(projection.center_x),float(projection.center_y)},
            {float(medium.extinction),float(medium.anisotropy),float(medium.maximum_distance),ground?1.f:0.f},
            packed(medium.albedo),packed(medium.ambient),packed(medium.sunlight),packed(light),{}, {},packed(origin)};
        if(ground) {p.point=packed(ground->point);p.normal=packed(ground->normal);}
        p.light.w=background_only?1.f:0.f;
        const Float4 packed_values[]{p.extent,p.projection,p.medium,p.albedo,p.ambient,p.sunlight,p.light,p.point,p.normal,p.origin};
        for(const auto& v:packed_values)
            if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.w))
                throw std::runtime_error("GPU fog inputs exceed float range");
        if(p.projection.x<=0||p.projection.y<=0||p.medium.z<=0)
            throw std::runtime_error("GPU fog inputs underflow float range");
        return p;
    }
    void dispatch(const FogParameters& p,unsigned w,unsigned h,SDL_GPUBuffer* nodes,SDL_GPUBuffer* triangles) {
        SDL_PushGPUComputeUniformData(command,0,&p,sizeof(p));
        SDL_GPUStorageBufferReadWriteBinding out{};out.buffer=buffers[2];
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&out,1);require_fog(pass);
        SDL_GPUBuffer* input[]{nodes,triangles};
        SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,input,2);
        SDL_DispatchGPUCompute(pass,(w+7)/8,(h+7)/8,1);SDL_EndGPUComputePass(pass);
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require_fog(fence);
        width=w;height=h;valid=true;
        status=std::string("GPU fog: ")+SDL_GetGPUDeviceDriver(device);
    }
    void render(const shadows::Scene& source,VolumetricProjection projection,unsigned w,unsigned h,
        const VolumetricMedium& medium,shadows::Vec3 light,std::optional<VolumetricGround> ground,
        bool background_only,shadows::Vec3 origin) {
        finish();valid=false;scene={};
        auto p=parameters(projection,w,h,medium,light,ground,background_only,origin,0);
        std::vector<shadows::Scene::GpuNode> nodes;
        std::vector<shadows::Scene::GpuTriangle> triangles;
        if(!source.pack_gpu(nodes,triangles)) throw std::runtime_error("GPU fog scene must be built");
        const auto node_count=nodes.size();
        const auto triangle_count=triangles.size();
        if(node_count>16777216) throw std::runtime_error("GPU fog node count exceeds exact float range");
        p.extent.z=float(node_count);
        if(nodes.empty()) nodes.resize(1);
        if(triangles.empty()) triangles.resize(1);
        const auto total=(std::uint64_t(nodes.size())+triangles.size())*48;
        if(total>UINT32_MAX) throw std::runtime_error("GPU fog scene upload too large");
        const Uint32 nb=Uint32(nodes.size()*48),tb=Uint32(triangles.size()*48);
        buffer(0,nb);buffer(1,tb);buffer(2,Uint32(std::uint64_t(w)*h*16));
        transfer(upload,upload_capacity,Uint32(total),SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
        auto* data=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));require_fog(data);
        std::memcpy(data,nodes.data(),nb);std::memcpy(data+nb,triangles.data(),tb);
        SDL_UnmapGPUTransferBuffer(device,upload);
        command=SDL_AcquireGPUCommandBuffer(device);require_fog(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require_fog(copy);
        for(unsigned i=0;i<2;++i) {
            SDL_GPUTransferBufferLocation source{upload,i?nb:0};
            SDL_GPUBufferRegion target{buffers[i],0,i?tb:nb};SDL_UploadToGPUBuffer(copy,&source,&target,false);
        }
        SDL_EndGPUCopyPass(copy);
        dispatch(p,w,h,buffers[0],buffers[1]);
        scene={device,buffers[0],buffers[1],Uint32(node_count),Uint32(triangle_count),nb,tb,true};
    }
    void render_resident(const GpuVolumetricSceneOutput& source,VolumetricProjection projection,unsigned w,unsigned h,
        const VolumetricMedium& medium,shadows::Vec3 light,std::optional<VolumetricGround> ground,
        bool background_only,shadows::Vec3 origin) {
        finish();valid=false;scene={};
        if(!source.complete || source.device!=device || !source.nodes || !source.triangles
            || source.nodes==source.triangles || source.nodes==buffers[2] || source.triangles==buffers[2]
            || source.node_count>16777216 || bool(source.node_count)!=bool(source.triangle_count)
            || source.node_bytes<std::max(1U,source.node_count)*std::uint64_t(48)
            || source.triangle_bytes<std::max(1U,source.triangle_count)*std::uint64_t(48))
            throw std::runtime_error("Invalid resident GPU fog scene");
        const auto p=parameters(projection,w,h,medium,light,ground,background_only,origin,source.node_count);
        buffer(2,Uint32(std::uint64_t(w)*h*16));
        command=SDL_AcquireGPUCommandBuffer(device);require_fog(command);
        dispatch(p,w,h,static_cast<SDL_GPUBuffer*>(source.nodes),static_cast<SDL_GPUBuffer*>(source.triangles));
        scene=source;
    }
    void read(std::vector<std::array<float,4>>& result) {
        if(!valid) throw std::runtime_error("No GPU fog output");
        finish();const Uint32 bytes=width*height*16;
        transfer(download,download_capacity,bytes,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        command=SDL_AcquireGPUCommandBuffer(device);require_fog(command);
        auto* pass=SDL_BeginGPUCopyPass(command);require_fog(pass);
        SDL_GPUBufferRegion source{buffers[2],0,bytes};SDL_GPUTransferBufferLocation target{download,0};
        SDL_DownloadFromGPUBuffer(pass,&source,&target);SDL_EndGPUCopyPass(pass);
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require_fog(fence);finish();
        const auto* data=static_cast<const std::array<float,4>*>(SDL_MapGPUTransferBuffer(device,download,false));require_fog(data);
        result.assign(data,data+std::size_t(width)*height);SDL_UnmapGPUTransferBuffer(device,download);
    }
};
#else
struct GpuVolumetricFog::Impl {std::string status{"GPU fog unavailable"};};
#endif
GpuVolumetricFog::GpuVolumetricFog():impl_(std::make_unique<Impl>()){}
GpuVolumetricFog::~GpuVolumetricFog()=default;
void GpuVolumetricFog::release_device() noexcept {impl_.reset();}
const std::string& GpuVolumetricFog::status() const {static const std::string released{"GPU fog released"};return impl_?impl_->status:released;}
bool GpuVolumetricFog::render(void* device,const shadows::Scene& scene,VolumetricProjection projection,
    unsigned w,unsigned h,const VolumetricMedium& medium,shadows::Vec3 light,std::optional<VolumetricGround> ground,
    bool background_only,shadows::Vec3 eye_origin) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!device) {if(impl_) {impl_->valid=false;impl_->scene={};}return false;}
    if(!impl_||impl_->device!=device) impl_=std::make_unique<Impl>();
    try {
        if(!impl_->pipeline) impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        impl_->render(scene,projection,w,h,medium,light,ground,background_only,eye_origin);return true;
    } catch(const std::exception& e) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->valid=false;impl_->scene={};impl_->status=e.what();return false;
    }
#else
    (void)device;(void)scene;(void)projection;(void)w;(void)h;(void)medium;(void)light;(void)ground;(void)background_only;(void)eye_origin;return false;
#endif
}
bool GpuVolumetricFog::render_resident(void* device,const GpuVolumetricSceneOutput& source,VolumetricProjection projection,
    unsigned w,unsigned h,const VolumetricMedium& medium,shadows::Vec3 light,std::optional<VolumetricGround> ground,
    bool background_only,shadows::Vec3 eye_origin) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!device) {if(impl_) {impl_->valid=false;impl_->scene={};}return false;}
    if(!impl_||impl_->device!=device) impl_=std::make_unique<Impl>();
    try {
        if(!impl_->pipeline) impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        impl_->render_resident(source,projection,w,h,medium,light,ground,background_only,eye_origin);return true;
    } catch(const std::exception& e) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->valid=false;impl_->scene={};impl_->status=e.what();return false;
    }
#else
    (void)device;(void)source;(void)projection;(void)w;(void)h;(void)medium;(void)light;(void)ground;(void)background_only;(void)eye_origin;return false;
#endif
}
GpuVolumetricSceneOutput GpuVolumetricFog::scene_output() const {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(impl_&&impl_->valid) return impl_->scene;
#endif
    return {};
}
GpuVolumetricOutput GpuVolumetricFog::output() const {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(impl_&&impl_->valid) return {impl_->device,impl_->buffers[2],impl_->width,impl_->height};
#endif
    return {};
}
bool GpuVolumetricFog::readback(std::vector<std::array<float,4>>& result) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_) return false;
    try {impl_->read(result);return true;}
    catch(const std::exception& e) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=e.what();return false;
    }
#else
    (void)result;return false;
#endif
}
}
