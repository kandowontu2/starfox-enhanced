#include "starfox/render/gpu_bsp.hpp"
#include "starfox/render/gpu_scene_counters.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/bsp_portable.hpp"
#include <cstring>
#include <stdexcept>
#endif
namespace starfox::render {
struct GpuBsp::Impl {
    std::string status{"GPU BSP unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUBuffer *order{},*results{};
    std::uint32_t order_capacity{},result_capacity{};
    static void require(bool value) {if(!value) throw std::runtime_error(SDL_GetError());}
    ~Impl(){release();}
    void release() noexcept {
        if(order) SDL_ReleaseGPUBuffer(device,order);
        if(results) SDL_ReleaseGPUBuffer(device,results);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        order=results=nullptr;pipeline=nullptr;device=nullptr;order_capacity=result_capacity=0;
    }
    void initialize(SDL_GPUDevice* next) {
        if(device==next && pipeline) return;
        release();device=next;
        bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        if(!spirv && !dxil && !(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_MSL))
            throw std::runtime_error("GPU BSP requires Vulkan, Metal or D3D12");
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        info.code=spirv?bsp_shader::spirv:dxil?bsp_shader::dxil:reinterpret_cast<const Uint8*>(bsp_shader::metal);
        info.code_size=spirv?sizeof(bsp_shader::spirv):dxil?sizeof(bsp_shader::dxil):std::strlen(bsp_shader::metal);
        info.entrypoint=(spirv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=4;info.num_readwrite_storage_buffers=2;
        info.num_uniform_buffers=1;info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&info);require(pipeline);
    }
    void allocate(SDL_GPUBuffer*& buffer,std::uint32_t& capacity,std::uint32_t bytes) {
        if(capacity>=bytes) return;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes,0};
        auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
        if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        buffer=replacement;capacity=bytes;
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
        auto* pass=scene_counters::begin_compute_pass(cmd,nullptr,0,writes,2);Impl::require(pass);
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
}
