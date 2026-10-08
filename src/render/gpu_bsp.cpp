#include "starfox/render/gpu_bsp.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/bsp_portable.hpp"
#include "shaders/generated/bsp_visibility_portable.hpp"
#include "shaders/generated/small_model_portable.hpp"
#include <cstring>
#include <stdexcept>
#endif
namespace starfox::render {
struct GpuBsp::Impl {
    std::string status{"GPU BSP unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{},*projected_pipeline{},*small_pipeline{};
    SDL_GPUBuffer *order{},*results{},*visibility{},*small_points{},*small_residuals{};
    std::uint32_t order_capacity{},result_capacity{},visibility_capacity{},small_point_capacity{},small_residual_capacity{};
    static void require(bool value) {if(!value) throw std::runtime_error(SDL_GetError());}
    ~Impl(){release();}
    void release() noexcept {
        if(order) SDL_ReleaseGPUBuffer(device,order);
        if(results) SDL_ReleaseGPUBuffer(device,results);
        if(visibility) SDL_ReleaseGPUBuffer(device,visibility);
        if(small_points) SDL_ReleaseGPUBuffer(device,small_points);
        if(small_residuals) SDL_ReleaseGPUBuffer(device,small_residuals);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(projected_pipeline) SDL_ReleaseGPUComputePipeline(device,projected_pipeline);
        if(small_pipeline) SDL_ReleaseGPUComputePipeline(device,small_pipeline);
        order=results=visibility=small_points=small_residuals=nullptr;
        pipeline=projected_pipeline=small_pipeline=nullptr;device=nullptr;
        order_capacity=result_capacity=visibility_capacity=small_point_capacity=small_residual_capacity=0;
    }
    void initialize_device(SDL_GPUDevice* next) {
        if(device!=next) {release();device=next;}
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        if(!spirv && !dxil && !(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_MSL))
            throw std::runtime_error("GPU BSP requires Vulkan, Metal or D3D12");
    }
    void initialize(SDL_GPUDevice* next) {
        initialize_device(next);
        if(pipeline) return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        info.code=spirv?bsp_shader::spirv:dxil?bsp_shader::dxil:reinterpret_cast<const Uint8*>(bsp_shader::metal);
        info.code_size=spirv?sizeof(bsp_shader::spirv):dxil?sizeof(bsp_shader::dxil):std::strlen(bsp_shader::metal);
        info.entrypoint=(spirv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=4;info.num_readwrite_storage_buffers=2;
        info.num_uniform_buffers=1;info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
        pipeline=create_gpu_compute_pipeline(device,&info);require(pipeline);
    }
    void initialize_projected(SDL_GPUDevice* next) {
        // Neither pipeline needs the other. Keep buffers and the first pipeline
        // alive when a recording switches paths on the same device.
        initialize_device(next);
        if(projected_pipeline) return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        info.code=spirv?bsp_visibility_shader::spirv:dxil?bsp_visibility_shader::dxil:reinterpret_cast<const Uint8*>(bsp_visibility_shader::metal);
        info.code_size=spirv?sizeof(bsp_visibility_shader::spirv):dxil?sizeof(bsp_visibility_shader::dxil):std::strlen(bsp_visibility_shader::metal);
        info.entrypoint=(spirv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=5;info.num_readwrite_storage_buffers=3;
        info.num_uniform_buffers=1;info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        projected_pipeline=create_gpu_compute_pipeline(device,&info);require(projected_pipeline);
    }
    void allocate(SDL_GPUBuffer*& buffer,std::uint32_t& capacity,std::uint32_t bytes) {
        if(capacity>=bytes) return;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes,0};
        auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
        if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        buffer=replacement;capacity=bytes;
    }
    void initialize_small(SDL_GPUDevice* next) {
        initialize_device(next);
        if(small_pipeline) return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        info.code=spirv?small_model_shader::spirv:dxil?small_model_shader::dxil:reinterpret_cast<const Uint8*>(small_model_shader::metal);
        info.code_size=spirv?sizeof(small_model_shader::spirv):dxil?sizeof(small_model_shader::dxil):std::strlen(small_model_shader::metal);
        info.entrypoint=(spirv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=6;info.num_readwrite_storage_buffers=5;
        info.num_uniform_buffers=1;info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        small_pipeline=create_gpu_compute_pipeline(device,&info);require(small_pipeline);
    }
#endif
};
GpuBsp::GpuBsp():impl_(std::make_unique<Impl>()){}
GpuBsp::~GpuBsp()=default;
const std::string& GpuBsp::status()const noexcept{return impl_->status;}
void GpuBsp::release_device()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
GpuBspOutput GpuBsp::enqueue(void* device,void* command,void* nodes,void* visibility,
    void* faces,void* trees,const GpuBspSettings& settings) {
    if(!device || !command || !nodes || !visibility || !faces || !trees
        || !settings.tree_count || settings.tree_count>65536 || !settings.output_count
        || settings.output_count>16U*1024*1024) {
        impl_->status="Invalid GPU BSP input";return {};
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        for(auto* input:{nodes,visibility,faces,trees})
            if(input==impl_->order || input==impl_->results) throw std::runtime_error("GPU BSP input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        impl_->allocate(impl_->order,impl_->order_capacity,settings.output_count*4);
        impl_->allocate(impl_->results,impl_->result_capacity,settings.tree_count*8);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_PushGPUComputeUniformData(cmd,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding writes[2]{};
        writes[0].buffer=impl_->order;writes[1].buffer=impl_->results;
        writes[0].cycle=writes[1].cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,writes,2);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(nodes),static_cast<SDL_GPUBuffer*>(visibility),
            static_cast<SDL_GPUBuffer*>(faces),static_cast<SDL_GPUBuffer*>(trees)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,4);
        SDL_DispatchGPUCompute(pass,(settings.tree_count+31)/32,1,1);SDL_EndGPUComputePass(pass);
        impl_->status="BSP painter order GPU resident";
        return {impl_->order,impl_->results};
    }catch(const std::exception& error){impl_->status=error.what();}
#endif
    return {};
}
GpuBspOutput GpuBsp::enqueue_projected(void* device,void* command,void* nodes,
    void* points,void* visibility_faces,void* faces,void* trees,
    const GpuBspSettings& settings,std::uint32_t point_count,bool continuous) {
    if(!device || !command || !nodes || !points || !visibility_faces || !faces || !trees
        || settings.tree_count!=1 || !settings.visibility_count || settings.visibility_count>4096
        || !settings.output_count || settings.output_count>16U*1024*1024
        || !point_count || point_count>4'000'000) {
        impl_->status="Invalid single-model projected BSP input";return {};
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        for(auto* input:{nodes,points,visibility_faces,faces,trees})
            if(input==impl_->order || input==impl_->results || input==impl_->visibility)
                throw std::runtime_error("Projected BSP input aliases output");
        impl_->initialize_projected(static_cast<SDL_GPUDevice*>(device));
        impl_->allocate(impl_->order,impl_->order_capacity,settings.output_count*4);
        impl_->allocate(impl_->results,impl_->result_capacity,8);
        impl_->allocate(impl_->visibility,impl_->visibility_capacity,settings.visibility_count*4);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        auto config=settings;config.reserved[0]=point_count;config.reserved[1]=continuous?1U:0U;config.reserved[2]=0;
        SDL_PushGPUComputeUniformData(cmd,0,&config,sizeof(config));
        SDL_GPUStorageBufferReadWriteBinding writes[3]{};
        writes[0].buffer=impl_->visibility;writes[1].buffer=impl_->order;writes[2].buffer=impl_->results;
        for(auto& write:writes) write.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,writes,3);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->projected_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(nodes),static_cast<SDL_GPUBuffer*>(points),
            static_cast<SDL_GPUBuffer*>(visibility_faces),static_cast<SDL_GPUBuffer*>(faces),static_cast<SDL_GPUBuffer*>(trees)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,5);
        SDL_DispatchGPUCompute(pass,1,1,1);SDL_EndGPUComputePass(pass);
        impl_->status="Visibility and BSP painter order fused GPU resident";
        return {impl_->order,impl_->results,impl_->visibility};
    }catch(const std::exception& error){impl_->status=error.what();}
#else
    (void)continuous;
#endif
    return {};
}
GpuBspOutput GpuBsp::enqueue_continuous_model(void* device,void* command,void* vertices,
    void* poses,std::uint32_t pose_count,void* nodes,void* visibility_faces,void* faces,
    void* trees,const GpuBspSettings& settings,std::uint32_t point_count,bool lossless_camera) {
    if(!device || !command || !vertices || !poses || !nodes || !visibility_faces || !faces || !trees
        || !pose_count || settings.tree_count!=1 || !settings.visibility_count || settings.visibility_count>128
        || !settings.output_count || settings.output_count>16U*1024*1024 || !point_count || point_count>128) {
        impl_->status="Invalid bounded continuous model input";return {};
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        for(auto* input:{vertices,poses,nodes,visibility_faces,faces,trees})
            if(input==impl_->order || input==impl_->results || input==impl_->visibility
                || input==impl_->small_points || input==impl_->small_residuals)
                throw std::runtime_error("Bounded continuous model input aliases output");
        impl_->initialize_small(static_cast<SDL_GPUDevice*>(device));
        impl_->allocate(impl_->order,impl_->order_capacity,settings.output_count*4);
        impl_->allocate(impl_->results,impl_->result_capacity,8);
        impl_->allocate(impl_->visibility,impl_->visibility_capacity,settings.visibility_count*4);
        impl_->allocate(impl_->small_points,impl_->small_point_capacity,point_count*32);
        impl_->allocate(impl_->small_residuals,impl_->small_residual_capacity,point_count*32);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const Uint32 config[]{point_count,pose_count,lossless_camera?2U:1U,0,
            settings.node_count,settings.visibility_count,settings.face_count,settings.output_count};
        SDL_PushGPUComputeUniformData(cmd,0,config,sizeof(config));
        SDL_GPUStorageBufferReadWriteBinding writes[5]{};
        writes[0].buffer=impl_->small_points;writes[1].buffer=impl_->small_residuals;
        writes[2].buffer=impl_->visibility;writes[3].buffer=impl_->order;writes[4].buffer=impl_->results;
        for(auto& write:writes) write.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,writes,5);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->small_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(vertices),static_cast<SDL_GPUBuffer*>(poses),
            static_cast<SDL_GPUBuffer*>(nodes),static_cast<SDL_GPUBuffer*>(visibility_faces),
            static_cast<SDL_GPUBuffer*>(faces),static_cast<SDL_GPUBuffer*>(trees)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,6);
        SDL_DispatchGPUCompute(pass,1,1,1);SDL_EndGPUComputePass(pass);
        impl_->status="Bounded projection, visibility and BSP painter GPU resident";
        return {impl_->order,impl_->results,impl_->visibility,impl_->small_points,impl_->small_residuals};
    }catch(const std::exception& error){impl_->status=error.what();}
#else
    (void)lossless_camera;
#endif
    return {};
}
}
