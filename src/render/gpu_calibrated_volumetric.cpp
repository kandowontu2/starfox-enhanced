#include "starfox/render/gpu_calibrated_volumetric.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/calibrated_volumetric.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include <bit>
#include <cmath>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Uniform {
    unsigned width,height,triangles,leaves;
    unsigned material_offset,material_bytes,flags,samples;
    std::array<float,4> projection,medium,albedo,ambient,sunlight,light;
};
static_assert(sizeof(Uniform)==128);
std::array<float,4> pack(shadows::Vec3 value) {return {float(value.x),float(value.y),float(value.z),0};}
bool finite(shadows::Vec3 p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
}
struct GpuCalibratedVolumetric::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUComputePipeline *leaves{},*merge{},*integrate{};
    SDL_GPUShader *vertex{},*fragment{};SDL_GPUGraphicsPipeline* compose{};SDL_GPUSampler* sampler{};
    SDL_GPUBuffer *nodes{},*integral{},*dummy{};unsigned node_bytes{},integral_bytes{};
    std::string status{"Native volumetric fog not initialized"};
    ~State() {
        if(!device) return;
        for(auto* pipeline:{leaves,merge,integrate}) if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        for(auto* buffer:{nodes,integral,dummy}) if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        if(compose) SDL_ReleaseGPUGraphicsPipeline(device,compose);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
    void grow(SDL_GPUBuffer*& buffer,unsigned& capacity,unsigned bytes) {
        if(buffer&&capacity>=bytes) return;
        const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,bytes,0};
        auto* next=SDL_CreateGPUBuffer(device,&info);require(next,SDL_GetError());
        if(buffer) SDL_ReleaseGPUBuffer(device,buffer);buffer=next;capacity=bytes;
    }
};
GpuCalibratedVolumetric::GpuCalibratedVolumetric():state_(std::make_unique<State>()){}
GpuCalibratedVolumetric::~GpuCalibratedVolumetric()=default;
void GpuCalibratedVolumetric::release_device() noexcept {state_.reset();}
const std::string& GpuCalibratedVolumetric::status()const noexcept {
    static const std::string released{"Native volumetric fog released"};return state_?state_->status:released;
}
bool GpuCalibratedVolumetric::initialize(void* device,int color_format) {
    release_device();auto next=std::make_unique<State>();
    try {
        require(device,"Native volumetric fog requires a GPU");next->device=static_cast<SDL_GPUDevice*>(device);
        next->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(next->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM||next->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            ||next->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB||next->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported native volumetric colour format");
        const auto formats=SDL_GetGPUShaderFormats(next->device);
        const bool spv=(formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(spv||(formats&SDL_GPU_SHADERFORMAT_DXIL)!=0,"Unsupported native volumetric shader format");
        const auto compute=[&](const Uint8* spirv,std::size_t spirv_size,const Uint8* dxil,std::size_t dxil_size,
            const char* entry,unsigned samplers,unsigned inputs,unsigned threads) {
            SDL_GPUComputePipelineCreateInfo info{};info.format=spv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.code=spv?spirv:dxil;info.code_size=spv?spirv_size:dxil_size;info.entrypoint=entry;
            info.num_samplers=samplers;info.num_readonly_storage_buffers=inputs;info.num_readwrite_storage_buffers=1;
            info.num_uniform_buffers=1;info.threadcount_x=threads;info.threadcount_y=threads==8?8:1;info.threadcount_z=1;
            auto* pipeline=create_gpu_compute_pipeline(next->device,&info);require(pipeline,SDL_GetError());return pipeline;
        };
#if defined(_WIN32)
#define FOG_COMPUTE(stage,entry,samplers,inputs,threads) compute(calibrated_volumetric_shader::stage##_spirv,sizeof(calibrated_volumetric_shader::stage##_spirv),calibrated_volumetric_shader::stage##_dxil,sizeof(calibrated_volumetric_shader::stage##_dxil),entry,samplers,inputs,threads)
#else
#define FOG_COMPUTE(stage,entry,samplers,inputs,threads) compute(calibrated_volumetric_shader::stage##_spirv,sizeof(calibrated_volumetric_shader::stage##_spirv),nullptr,0,entry,samplers,inputs,threads)
#endif
        next->leaves=FOG_COMPUTE(leaves,"fog_leaves_main",0,1,64);
        next->merge=FOG_COMPUTE(merge,"fog_merge_main",0,0,64);
        next->integrate=FOG_COMPUTE(integrate,"fog_integrate_main",2,2,8);
#undef FOG_COMPUTE
        SDL_GPUShaderCreateInfo shader{};shader.format=spv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        next->vertex=create_gpu_shader(next->device,&shader);require(next->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="fog_fragment_main";
        shader.num_samplers=2;shader.num_storage_buffers=1;shader.num_uniform_buffers=1;
        shader.code=calibrated_volumetric_shader::fragment_spirv;shader.code_size=sizeof(calibrated_volumetric_shader::fragment_spirv);
#if defined(_WIN32)
        if(!spv) {shader.code=calibrated_volumetric_shader::fragment_dxil;shader.code_size=sizeof(calibrated_volumetric_shader::fragment_dxil);}
#endif
        next->fragment=create_gpu_shader(next->device,&shader);require(next->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription target{};target.format=next->format;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=next->vertex;pipeline.fragment_shader=next->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=&target;pipeline.target_info.num_color_targets=1;
        next->compose=create_gpu_graphics_pipeline(next->device,&pipeline);require(next->compose,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        next->sampler=SDL_CreateGPUSampler(next->device,&sampler);require(next->sampler,SDL_GetError());
        unsigned capacity=0;next->grow(next->dummy,capacity,16);
        next->status="Native volumetric fog ready";state_=std::move(next);return true;
    } catch(const std::exception& error) {const std::string message=error.what();next.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedVolumetric::enqueue(void* command,void* source,void* ownership,void* surfaces,void* destination,
    const CalibratedRayGeometryOutput& geometry,const VolumetricMedium& medium,shadows::Vec3 light,void** integral_output) {
    if(integral_output) *integral_output=nullptr;if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.compose&&command&&source&&ownership&&surfaces&&destination&&source!=ownership&&source!=surfaces
            &&source!=destination&&ownership!=surfaces&&ownership!=destination&&surfaces!=destination,"Invalid native volumetric images");
        const auto pixels=std::uint64_t(geometry.width)*geometry.height;
        require(geometry.complete&&geometry.device==s.device&&geometry.width&&geometry.height
            &&geometry.width<=16384&&geometry.height<=16384&&pixels<=512ULL*1024*1024/16
            &&geometry.vertex_count%3==0&&geometry.vertex_count<=3'000'000
            &&(!geometry.vertex_count||geometry.buffer),"Invalid native volumetric geometry/extent");
        require(!geometry.material_bytes||(geometry.material_offset%16==0
            &&geometry.material_offset>=std::uint64_t(geometry.vertex_count)*16
            &&geometry.material_bytes>=std::uint64_t(geometry.vertex_count/3)*64
            &&std::uint64_t(geometry.material_offset)+geometry.material_bytes<=UINT32_MAX),"Invalid native volumetric material range");
        for(double value:geometry.projection) require(std::isfinite(value)&&std::abs(value)<=1.e8,"Invalid native volumetric projection");
        require(geometry.projection[0]>0&&geometry.projection[1]>0,"Invalid native volumetric focal lengths");
        const auto positive=[](shadows::Vec3 v) {return finite(v)&&v.x>=0&&v.y>=0&&v.z>=0;};
        require(positive(medium.albedo)&&medium.albedo.x<=1&&medium.albedo.y<=1&&medium.albedo.z<=1
            &&positive(medium.ambient)&&positive(medium.sunlight)&&medium.ambient.x<=1.e6&&medium.ambient.y<=1.e6&&medium.ambient.z<=1.e6
            &&medium.sunlight.x<=1.e6&&medium.sunlight.y<=1.e6&&medium.sunlight.z<=1.e6
            &&std::isfinite(medium.extinction)&&medium.extinction>=0&&medium.extinction<=1.e6
            &&std::isfinite(medium.anisotropy)&&std::abs(medium.anisotropy)<=.9
            &&std::isfinite(medium.maximum_distance)&&medium.maximum_distance>0&&medium.maximum_distance<=1.e8
            &&medium.samples>=1&&medium.samples<=128&&finite(light),"Invalid native volumetric medium/light");
        const double length=std::sqrt(shadows::dot(light,light));require(std::isfinite(length)&&length>=1.e-12,"Zero native volumetric light");
        light=light*(1/length);
        const auto triangles=geometry.vertex_count/3,leaves=std::bit_ceil(std::max(1U,(triangles+3)/4));
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB||s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const Uniform uniform{geometry.width,geometry.height,triangles,leaves,geometry.material_offset,geometry.material_bytes,unsigned(srgb),medium.samples,
            {float(geometry.projection[0]),float(geometry.projection[1]),float(geometry.projection[2]),float(geometry.projection[3])},
            {float(medium.extinction),float(medium.anisotropy),float(medium.maximum_distance),0},pack(medium.albedo),pack(medium.ambient),pack(medium.sunlight),pack(light)};
        require(uniform.projection[0]>0&&uniform.projection[1]>0&&uniform.medium[2]>0
            &&(medium.extinction==0||uniform.medium[0]>0),"Native volumetric inputs underflow float range");
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        if(medium.extinction>0) {
            require(!geometry.buffer||(geometry.buffer!=s.nodes&&geometry.buffer!=s.integral&&geometry.buffer!=s.dummy),"Native volumetric geometry aliases work storage");
            s.grow(s.nodes,s.node_bytes,leaves*64);s.grow(s.integral,s.integral_bytes,unsigned(pixels*16));
            auto* input=geometry.vertex_count?static_cast<SDL_GPUBuffer*>(geometry.buffer):s.dummy;
            SDL_GPUStorageBufferReadWriteBinding out{};out.buffer=s.nodes;out.cycle=true;
            unsigned build[]{triangles,leaves,0,0};SDL_PushGPUComputeUniformData(cmd,0,build,sizeof(build));
            auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&out,1);require(pass,SDL_GetError());
            SDL_BindGPUComputePipeline(pass,s.leaves);SDL_BindGPUComputeStorageBuffers(pass,0,&input,1);
            SDL_DispatchGPUCompute(pass,(leaves+63)/64,1,1);SDL_EndGPUComputePass(pass);
            out.cycle=false;
            for(unsigned first=leaves/2;first;first/=2) {
                build[2]=build[3]=first;SDL_PushGPUComputeUniformData(cmd,0,build,sizeof(build));
                pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&out,1);require(pass,SDL_GetError());SDL_BindGPUComputePipeline(pass,s.merge);
                SDL_DispatchGPUCompute(pass,(first+63)/64,1,1);SDL_EndGPUComputePass(pass);
            }
            out.buffer=s.integral;out.cycle=true;SDL_PushGPUComputeUniformData(cmd,0,&uniform,sizeof(uniform));
            pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&out,1);require(pass,SDL_GetError());SDL_BindGPUComputePipeline(pass,s.integrate);
            SDL_GPUBuffer* inputs[]{input,s.nodes};SDL_BindGPUComputeStorageBuffers(pass,0,inputs,2);
            const SDL_GPUTextureSamplerBinding guides[]{{static_cast<SDL_GPUTexture*>(ownership),s.sampler},{static_cast<SDL_GPUTexture*>(surfaces),s.sampler}};
            SDL_BindGPUComputeSamplers(pass,0,guides,2);SDL_DispatchGPUCompute(pass,(geometry.width+7)/8,(geometry.height+7)/8,1);SDL_EndGPUComputePass(pass);
        }
        SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* pass=SDL_BeginGPURenderPass(cmd,&target,1,nullptr);require(pass,SDL_GetError());SDL_BindGPUGraphicsPipeline(pass,s.compose);
        const SDL_GPUViewport viewport{0,0,float(geometry.width),float(geometry.height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(geometry.width),int(geometry.height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding images[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},{static_cast<SDL_GPUTexture*>(ownership),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,images,2);auto* volume=medium.extinction>0?s.integral:s.dummy;
        SDL_BindGPUFragmentStorageBuffers(pass,0,&volume,1);SDL_PushGPUFragmentUniformData(cmd,0,&uniform,sizeof(uniform));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        if(integral_output&&medium.extinction>0) *integral_output=s.integral;
        s.status="Native geometry-occluded volumetric fog encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
}
