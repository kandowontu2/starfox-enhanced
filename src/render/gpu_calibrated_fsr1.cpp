#include "starfox/render/gpu_calibrated_fsr1.hpp"
#include "starfox/render/gpu_fsr1.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <stdexcept>
namespace starfox::render {
namespace {void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}}
struct GpuCalibratedFsr1::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};bool srgb{};
    GpuFsr1 fsr;SDL_GPUShader *vertex{},*fragment{};SDL_GPUGraphicsPipeline* pipeline{};SDL_GPUSampler* sampler{};
    std::string status{"Native FSR not initialized"};
    ~State(){
        fsr.release_device();if(!device) return;
        if(pipeline) SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
};
GpuCalibratedFsr1::GpuCalibratedFsr1():state_(std::make_unique<State>()){}
GpuCalibratedFsr1::~GpuCalibratedFsr1()=default;
void GpuCalibratedFsr1::release_device() noexcept{state_.reset();}
const std::string& GpuCalibratedFsr1::status() const noexcept {
    static const std::string released{"Native FSR released"};return state_?state_->status:released;
}
bool GpuCalibratedFsr1::initialize(void* device,int format) {
    release_device();auto next=std::make_unique<State>();
    try {
        require(device,"Native FSR requires a GPU");next->device=static_cast<SDL_GPUDevice*>(device);
        next->format=static_cast<SDL_GPUTextureFormat>(format);
        next->srgb=next->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || next->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        require(next->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || next->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || next->srgb,
            "Unsupported native FSR format");
        const bool spirv=(SDL_GetGPUShaderFormats(next->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUShaderCreateInfo shader{};shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv){shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        next->vertex=create_gpu_shader(next->device,&shader);require(next->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="aa_upscale_fragment_main";shader.num_samplers=3;shader.num_uniform_buffers=1;
        shader.code=calibrated_scene_shader::aa_upscale_spirv;shader.code_size=sizeof(calibrated_scene_shader::aa_upscale_spirv);
#if defined(_WIN32)
        if(!spirv){shader.code=calibrated_scene_shader::aa_upscale_dxil;shader.code_size=sizeof(calibrated_scene_shader::aa_upscale_dxil);}
#endif
        next->fragment=create_gpu_shader(next->device,&shader);require(next->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription target{};target.format=next->format;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=next->vertex;pipeline.fragment_shader=next->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.target_info.color_target_descriptions=&target;pipeline.target_info.num_color_targets=1;
        next->pipeline=create_gpu_graphics_pipeline(next->device,&pipeline);require(next->pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        next->sampler=SDL_CreateGPUSampler(next->device,&sampler);require(next->sampler,SDL_GetError());
        next->status="Native FSR ready";state_=std::move(next);return true;
    } catch(const std::exception& e){const std::string failure=e.what();next.reset();state_=std::make_unique<State>();state_->status=failure;return false;}
}
bool GpuCalibratedFsr1::enqueue(void* command,void* scene,Fsr1Extent input,void* ink,void* ownership,void* destination,Fsr1Extent output) {
    if(!state_) return false;
    try {
        auto& s=*state_;require(s.pipeline && command,"Native FSR is not ready");
        const std::array<void*,4> images{scene,ink,ownership,destination};
        for(unsigned i=0;i<images.size();++i){require(images[i],"Missing native FSR image");
            for(unsigned j=i+1;j<images.size();++j) require(images[i]!=images[j],"Aliased native FSR images");}
        require(input.width && input.height && output.width>=input.width && output.height>=input.height
            && output.width<=16384 && output.height<=16384 && std::uint64_t(output.width)*output.height<=64ULL*1024*1024,
            "Native FSR exceeds its working image bounds");
        auto* world=s.fsr.enqueue(s.device,command,scene,input,output,.2F,s.srgb);require(world,s.fsr.status().c_str());
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* pass=SDL_BeginGPURenderPass(cb,&target,1,nullptr);require(pass,SDL_GetError());SDL_BindGPUGraphicsPipeline(pass,s.pipeline);
        const SDL_GPUViewport viewport{0,0,float(output.width),float(output.height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(output.width),int(output.height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(world),s.sampler},
            {static_cast<SDL_GPUTexture*>(ownership),s.sampler},{static_cast<SDL_GPUTexture*>(ink),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,3);
        const std::array<unsigned,8> settings{output.width,output.height,0,0,0,0,0,0};SDL_PushGPUFragmentUniformData(cb,0,settings.data(),sizeof(settings));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);s.status="Native per-eye EASU/RCAS and protected ink encoded";return true;
    } catch(const std::exception& e){state_->status=e.what();return false;}
}
}
