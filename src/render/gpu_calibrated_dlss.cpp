#include "starfox/render/gpu_calibrated_dlss.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include "starfox/render/sdl_d3d12_bridge.h"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include "shaders/generated/calibrated_dlss.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstring>

namespace starfox::render {
namespace {
void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}
struct Uniform {unsigned width,height,srgb,rejected_layers{};float z,w;std::array<float,2> delta;
    unsigned water_width{},water_height{},water_count{},surface_offset{};
    unsigned coverage_factor{1},coverage_samples{1},model_reflections{},coverage_padding{};
    unsigned pattern_scale{1},pattern_history{},pattern_padding[2]{};
    std::array<CalibratedPatternPass,3> pattern_passes{};};
static_assert(sizeof(Uniform)==128 && sizeof(CalibratedPatternPass)==16);
static_assert(unsigned(Effect::dithered)==5 && unsigned(Effect::night_vision)==10
    && unsigned(Effect::scanlines)==25 && unsigned(Effect::crt_phosphor)==70);
bool has_patterns(const CalibratedPatternGuide& guide) {
    return std::any_of(guide.passes.begin(),guide.passes.end(),[](const auto& p){return p.world || p.model;});
}
}
struct GpuCalibratedDlss::State {
    SDL_GPUDevice* device{};CalibratedDlssApi api;unsigned viewport{},mode{},model{};
    Fsr1Extent input{},output{};bool srgb{},evaluated{},pending{},accepted{},force_reset{true};
    bool sdk_attempted{};
    unsigned accepted_slot{},pending_slot{},last_index{};
    SDL_GPUTexture *color{},*depth{},*motion{},*exposure{},*bias{};std::array<SDL_GPUTexture*,2> reconstructed{};
    std::array<SDL_GPUTexture*,2> phases{};
    CalibratedPatternGuide accepted_patterns{},pending_patterns{};
    unsigned accepted_factor{1},accepted_samples{1},pending_factor{1},pending_samples{1};
    bool accepted_rays{},pending_rays{},accepted_model_reflections{true},pending_model_reflections{true};
    SDL_GPUBuffer* empty_water{};
    SDL_GPUShader *vertex{},*pack{},*pack_msaa{},*present{};SDL_GPUGraphicsPipeline *pack_pipeline{},*pack_msaa_pipeline{},*present_pipeline{};
    SDL_GPUShader *pack_patterns{},*pack_msaa_patterns{};
    SDL_GPUGraphicsPipeline *pattern_pipeline{},*pattern_msaa_pipeline{};
    SDL_GPUSampler* sampler{};const StarfoxSdlD3D12ComputeBridgeV1* bridge{};
    std::string status{"Native DLSS not initialized"};
    ~State(){
        if(!device) return;
        for(auto* t:{color,depth,motion,exposure,bias,reconstructed[0],reconstructed[1],phases[0],phases[1]}) if(t) SDL_ReleaseGPUTexture(device,t);
        if(empty_water) SDL_ReleaseGPUBuffer(device,empty_water);
        for(auto* p:{pack_pipeline,pack_msaa_pipeline,present_pipeline,pattern_pipeline,pattern_msaa_pipeline}) if(p) SDL_ReleaseGPUGraphicsPipeline(device,p);
        for(auto* s:{vertex,pack,pack_msaa,present,pack_patterns,pack_msaa_patterns}) if(s) SDL_ReleaseGPUShader(device,s);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
    }
    void prepare_patterns() {
        if(pattern_pipeline && pattern_msaa_pipeline) return;
        require(std::uint64_t(input.width)*input.height*25+std::uint64_t(output.width)*output.height*8<=1024ULL*1024*1024,
            "Native DLSS pattern witnesses exceed working image bounds");
        for(auto& t:phases) if(!t) {
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
            info.width=input.width;info.height=input.height;info.layer_count_or_depth=info.num_levels=1;
            info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            t=SDL_CreateGPUTexture(device,&info);require(t,SDL_GetError());
        }
#if defined(_WIN32)
        SDL_GPUShaderCreateInfo shader{};shader.format=SDL_GPU_SHADERFORMAT_DXIL;shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;
        shader.entrypoint="dlss_pack_main";shader.num_uniform_buffers=1;shader.num_samplers=6;shader.num_storage_buffers=8;
        if(!pack_patterns) {
            shader.code=calibrated_dlss_shader::pack_patterns_dxil;shader.code_size=sizeof(calibrated_dlss_shader::pack_patterns_dxil);
            pack_patterns=create_gpu_shader(device,&shader);require(pack_patterns,SDL_GetError());
        }
        if(!pack_msaa_patterns) {
            shader.code=calibrated_dlss_shader::pack_msaa_patterns_dxil;shader.code_size=sizeof(calibrated_dlss_shader::pack_msaa_patterns_dxil);
            pack_msaa_patterns=create_gpu_shader(device,&shader);require(pack_msaa_patterns,SDL_GetError());
        }
#endif
        SDL_GPUColorTargetDescription packed[5]{};
        packed[0].format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;packed[1].format=SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        packed[2].format=SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;packed[3].format=SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        packed[4].format=SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=vertex;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
        pipeline.target_info.color_target_descriptions=packed;pipeline.target_info.num_color_targets=5;
        if(!pattern_pipeline) {
            pipeline.fragment_shader=pack_patterns;pattern_pipeline=create_gpu_graphics_pipeline(device,&pipeline);require(pattern_pipeline,SDL_GetError());
        }
        if(!pattern_msaa_pipeline) {
            pipeline.fragment_shader=pack_msaa_patterns;pattern_msaa_pipeline=create_gpu_graphics_pipeline(device,&pipeline);require(pattern_msaa_pipeline,SDL_GetError());
        }
    }
    bool restore(SDL_GPUCommandBuffer* command,unsigned slot,void* ink,void* owners,void* destination) {
        require(command && ink && owners && destination,"Missing native DLSS presentation image");
        require(ink!=destination && owners!=destination && destination!=reconstructed[slot],"Aliased native DLSS presentation image");
        SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* pass=SDL_BeginGPURenderPass(command,&target,1,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,present_pipeline);
        const SDL_GPUViewport view{0,0,float(output.width),float(output.height),0,1};SDL_SetGPUViewport(pass,&view);
        const SDL_Rect scissor{0,0,int(output.width),int(output.height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{reconstructed[slot],sampler},
            {static_cast<SDL_GPUTexture*>(owners),sampler},{static_cast<SDL_GPUTexture*>(ink),sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,3);
        const Uniform uniform{output.width,output.height,srgb,0,0,0,{}};
        SDL_PushGPUFragmentUniformData(command,0,&uniform,sizeof(uniform));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);return true;
    }
};
GpuCalibratedDlss::GpuCalibratedDlss():state_(std::make_unique<State>()){}
GpuCalibratedDlss::~GpuCalibratedDlss(){
    if(!retire() && state_ && state_->device) {
        // Device loss/failed SDK retirement is not permission to free images
        // still referenced by a native queue. Retain them for process teardown.
        std::cerr<<"native-dlss-retirement: "<<state_->status<<'\n';state_.release();
    }
}
bool GpuCalibratedDlss::retire() noexcept {
    if(!state_ || !state_->device) return true;
    auto& s=*state_;
    if(!SDL_WaitForGPUIdle(s.device)){s.status=SDL_GetError();return false;}
    if(s.evaluated) {
        char error[512]{};
        if(s.api.release(s.api.module,s.viewport,error,sizeof(error))!=0){s.status=error;return false;}
        s.evaluated=false;
    }
    state_.reset();return true;
}
Fsr1Extent GpuCalibratedDlss::input_extent() const noexcept{return state_?state_->input:Fsr1Extent{};}
GpuCalibratedDlss::Inputs GpuCalibratedDlss::resident_inputs() const noexcept {
    return state_?Inputs{state_->color,state_->depth,state_->motion,state_->exposure,state_->bias,
        (has_patterns(state_->pending_patterns) || state_->pending_rays)?state_->phases[state_->pending_slot]:nullptr,
        state_->accepted && (has_patterns(state_->accepted_patterns) || state_->accepted_rays)?state_->phases[state_->accepted_slot]:nullptr}:Inputs{};
}
const std::string& GpuCalibratedDlss::status() const noexcept {
    static const std::string released{"Native DLSS retired"};return state_?state_->status:released;
}
bool GpuCalibratedDlss::initialize(void* device,int format,CalibratedDlssApi api,unsigned viewport,
    unsigned mode,unsigned model,Fsr1Extent output) {
    if(!retire()) return false;auto next=std::make_unique<State>();
    try {
#if !defined(_WIN32)
        require(false,"Native SDK DLSS requires Windows D3D12");
#endif
        require(device && api.complete(),"Native DLSS requires the host's trusted SDK");
        require(viewport>=100 && mode>=1 && mode<=4 && model<=1,"Invalid native DLSS viewport/mode/model");
        require(!model || api.configure_model,"Native SDK does not support model selection");
        require(output.width && output.height && output.width<=16384 && output.height<=16384
            && std::uint64_t(output.width)*output.height<=64ULL*1024*1024,"Native DLSS output exceeds image bounds");
        next->device=static_cast<SDL_GPUDevice*>(device);next->api=api;next->viewport=viewport;
        next->mode=mode;next->model=model;next->output=output;
        require(std::strcmp(SDL_GetGPUDeviceDriver(next->device),"direct3d12")==0,"Native SDK DLSS requires D3D12, not Vulkan");
        const auto properties=SDL_GetGPUDeviceProperties(next->device);
        next->bridge=static_cast<const StarfoxSdlD3D12ComputeBridgeV1*>(SDL_GetPointerProperty(properties,STARFOX_SDL_D3D12_COMPUTE_BRIDGE,nullptr));
        auto* native=SDL_GetPointerProperty(properties,STARFOX_SDL_D3D12_DEVICE,nullptr);
        require(native && next->bridge && next->bridge->version==1 && next->bridge->dispatch,"Native DLSS device/compute bridge missing");
        char error[512]{};require(api.bind(api.module,native,error,sizeof(error))==0,error);
        const auto configured=api.configure_model?api.configure_model(api.module,viewport,mode,model,output.width,output.height,
            &next->input.width,&next->input.height,error,sizeof(error)):api.configure(api.module,viewport,mode,output.width,output.height,
            &next->input.width,&next->input.height,error,sizeof(error));
        require(configured==0,error);
        require(next->input.width && next->input.height && next->input.width<=output.width && next->input.height<=output.height
            && std::uint64_t(next->input.width)*next->input.height*17+std::uint64_t(output.width)*output.height*8<=1024ULL*1024*1024,
            "Native SDK returned unsupported working dimensions");
        const auto target_format=static_cast<SDL_GPUTextureFormat>(format);
        next->srgb=target_format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || target_format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        require(target_format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || target_format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || next->srgb,
            "Unsupported native DLSS colour format");
        const auto texture=[&](SDL_GPUTextureFormat f,Fsr1Extent extent,bool output_uav=false) {
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=f;
            info.width=extent.width;info.height=extent.height;info.layer_count_or_depth=info.num_levels=1;
            info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            if(output_uav) info.usage|=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
            auto* t=SDL_CreateGPUTexture(next->device,&info);require(t,SDL_GetError());return t;
        };
        next->color=texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,next->input);
        next->depth=texture(SDL_GPU_TEXTUREFORMAT_R32_FLOAT,next->input);
        next->motion=texture(SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT,next->input);
        next->bias=texture(SDL_GPU_TEXTUREFORMAT_R8_UNORM,next->input);
        next->exposure=texture(SDL_GPU_TEXTUREFORMAT_R32_FLOAT,{1,1});
        SDL_GPUBufferCreateInfo empty{};empty.usage=SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;empty.size=16;
        next->empty_water=SDL_CreateGPUBuffer(next->device,&empty);require(next->empty_water,SDL_GetError());
        for(auto& t:next->reconstructed)t=texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,output,true);
        SDL_GPUShaderCreateInfo shader{};shader.format=SDL_GPU_SHADERFORMAT_DXIL;shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;
        shader.entrypoint="dlss_vertex_main";
#if defined(_WIN32)
        shader.code=calibrated_dlss_shader::vertex_dxil;shader.code_size=sizeof(calibrated_dlss_shader::vertex_dxil);
        next->vertex=create_gpu_shader(next->device,&shader);require(next->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.num_uniform_buffers=1;shader.num_samplers=5;shader.entrypoint="dlss_pack_main";
        shader.num_storage_buffers=8;
        shader.code=calibrated_dlss_shader::pack_dxil;shader.code_size=sizeof(calibrated_dlss_shader::pack_dxil);
        next->pack=create_gpu_shader(next->device,&shader);require(next->pack,SDL_GetError());
        shader.code=calibrated_dlss_shader::pack_msaa_dxil;shader.code_size=sizeof(calibrated_dlss_shader::pack_msaa_dxil);
        next->pack_msaa=create_gpu_shader(next->device,&shader);require(next->pack_msaa,SDL_GetError());
        shader.num_samplers=3;shader.num_storage_buffers=0;shader.entrypoint="dlss_present_main";
        shader.code=calibrated_dlss_shader::present_dxil;shader.code_size=sizeof(calibrated_dlss_shader::present_dxil);
        next->present=create_gpu_shader(next->device,&shader);require(next->present,SDL_GetError());
#endif
        SDL_GPUColorTargetDescription packed[4]{};
        packed[0].format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;packed[1].format=SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        packed[2].format=SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
        packed[3].format=SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=next->vertex;pipeline.fragment_shader=next->pack;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.target_info.color_target_descriptions=packed;pipeline.target_info.num_color_targets=4;
        next->pack_pipeline=create_gpu_graphics_pipeline(next->device,&pipeline);require(next->pack_pipeline,SDL_GetError());
        pipeline.fragment_shader=next->pack_msaa;
        next->pack_msaa_pipeline=create_gpu_graphics_pipeline(next->device,&pipeline);require(next->pack_msaa_pipeline,SDL_GetError());
        SDL_GPUColorTargetDescription target{};target.format=target_format;
        pipeline.fragment_shader=next->present;pipeline.target_info.color_target_descriptions=&target;pipeline.target_info.num_color_targets=1;
        next->present_pipeline=create_gpu_graphics_pipeline(next->device,&pipeline);require(next->present_pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        next->sampler=SDL_CreateGPUSampler(next->device,&sampler);require(next->sampler,SDL_GetError());
        next->status="Native independent SDK eye ready";state_=std::move(next);return true;
    } catch(const std::exception& e){const std::string failure=e.what();next.reset();state_=std::make_unique<State>();state_->status=failure;return false;}
}
bool GpuCalibratedDlss::enqueue(void* command,void* scene,void* owners,void* surfaces,void* motion,
    void* ink,void* native_owners,void* destination,StarfoxDlssFrameV1 f,std::array<float,2> previous_jitter,
    std::span<const shadows::GpuReflectionOutput> water,unsigned rejected_layers,CalibratedDlssCoverage coverage,
    CalibratedPatternGuide patterns,bool model_reflections) {
    if(!state_) return false;auto& s=*state_;
    try {
        require(command && s.pack_pipeline && !s.pending,"Native DLSS is not ready or has unsettled work");
        const std::array<void*,7> images{scene,owners,surfaces,motion,ink,native_owners,destination};
        for(unsigned i=0;i<images.size();++i){require(images[i],"Missing native DLSS image");
            for(unsigned j=i+1;j<images.size();++j) require(images[i]!=images[j],"Aliased native DLSS images");}
        require(f.size==sizeof(f) && f.reset<=1 && f.width==s.input.width && f.height==s.input.height
            && f.output_width==s.output.width && f.output_height==s.output.height,"Native DLSS frame extent/ABI mismatch");
        require(!s.evaluated || f.frame_index>s.last_index,"Native DLSS frame index must advance after evaluation");
        vr::Matrix4 projection{};std::copy(std::begin(f.view_to_clip),std::end(f.view_to_clip),projection.begin());
        require(calibrated_dlss_math::perspective(projection),"Invalid native DLSS projection");
        for(float v:{f.jitter[0],f.jitter[1],previous_jitter[0],previous_jitter[1]})
            require(std::isfinite(v) && std::abs(v)<=.5F,"Invalid native DLSS raster jitter");
        require(rejected_layers<=3,"Invalid native DLSS rejected ownership layers");
        require(patterns.valid(),"Invalid native DLSS pattern descriptor");
        require(coverage.factor>=1 && coverage.factor<=4 && (coverage.samples==1 || coverage.samples==2
            || coverage.samples==4 || coverage.samples==8) && (coverage.samples==1 || coverage.factor==1)
            && (coverage.ownership || (coverage.factor==1 && coverage.samples==1))
            && coverage.ownership!=destination,"Invalid native DLSS raster coverage");
        require(water.empty() || water.size()==1 || water.size()==2 || water.size()==4 || water.size()==8,
            "Invalid native DLSS liquid sample count");
        std::array<SDL_GPUBuffer*,8> water_buffers{};water_buffers.fill(s.empty_water);
        Uniform uniform{s.input.width,s.input.height,s.srgb,rejected_layers,projection[10],projection[14],
            {f.jitter[0]-previous_jitter[0],f.jitter[1]-previous_jitter[1]}};
        uniform.coverage_factor=coverage.factor;uniform.coverage_samples=coverage.samples;
        uniform.model_reflections=model_reflections;
        for(unsigned i=0;i<water.size();++i) {
            const auto& layer=water[i];
            const auto layout=shadows::native_water_layers(layer.width,layer.height,layer.water_layers.world_offset!=0);
            require(layout && layer.device==s.device && layer.buffer && layer.row_bytes==layer.width*4
                && (layer.water_layers==shadows::NativeWaterLayers{} || layer.water_layers==*layout)
                && layer.width%s.input.width==0 && layer.height%s.input.height==0,
                "Invalid native DLSS ray layout/device/extent");
            const auto factor=layer.width/s.input.width;
            require(factor>=1 && factor<=4 && layer.height/s.input.height==factor,
                "Native DLSS ray grid must match the raster samples");
            require(factor==coverage.factor && water.size()==coverage.samples,
                "Native DLSS ray samples require matching raster ownership");
            if(i) require(layer.width==uniform.water_width && layer.height==uniform.water_height
                && layer.water_layers.surface_offset==uniform.surface_offset,"Mismatched native DLSS liquid sample grids");
            else {uniform.water_width=layer.width;uniform.water_height=layer.height;
                uniform.water_count=unsigned(water.size());uniform.surface_offset=layer.water_layers.surface_offset;}
            water_buffers[i]=static_cast<SDL_GPUBuffer*>(layer.buffer);
        }
        const bool patterned=has_patterns(patterns) || !water.empty();
        const bool key_changed=s.accepted && (patterns!=s.accepted_patterns || coverage.factor!=s.accepted_factor
            || coverage.samples!=s.accepted_samples
            || patterned!=(has_patterns(s.accepted_patterns) || s.accepted_rays)
            || model_reflections!=s.accepted_model_reflections);
        f.viewport=s.viewport;f.reset|=s.force_reset || key_changed;
        s.pending_slot=s.accepted?1-s.accepted_slot:0;
        s.pending_patterns=patterns;s.pending_factor=coverage.factor;s.pending_samples=coverage.samples;
        s.pending_rays=!water.empty();s.pending_model_reflections=model_reflections;
        if(patterned) s.prepare_patterns();
        uniform.pattern_scale=patterns.scale;uniform.pattern_passes=patterns.passes;
        uniform.pattern_history=s.accepted && !f.reset && (has_patterns(s.accepted_patterns) || s.accepted_rays);
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_GPUColorTargetInfo targets[5]{};targets[0].texture=s.color;targets[1].texture=s.depth;targets[2].texture=s.motion;targets[3].texture=s.bias;
        const unsigned count=patterned?5:4;targets[4].texture=s.phases[s.pending_slot];
        for(unsigned i=0;i<count;++i){targets[i].load_op=SDL_GPU_LOADOP_DONT_CARE;targets[i].store_op=SDL_GPU_STOREOP_STORE;}
        auto* pass=SDL_BeginGPURenderPass(cb,targets,count,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,patterned?(coverage.samples>1?s.pattern_msaa_pipeline:s.pattern_pipeline)
            :(coverage.samples>1?s.pack_msaa_pipeline:s.pack_pipeline));
        const SDL_GPUViewport viewport{0,0,float(s.input.width),float(s.input.height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(s.input.width),int(s.input.height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(scene),s.sampler},{static_cast<SDL_GPUTexture*>(owners),s.sampler},
            {static_cast<SDL_GPUTexture*>(surfaces),s.sampler},{static_cast<SDL_GPUTexture*>(motion),s.sampler},
            {static_cast<SDL_GPUTexture*>(coverage.ownership?coverage.ownership:owners),s.sampler},
            {s.phases[1-s.pending_slot],s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,patterned?6:5);
        SDL_BindGPUFragmentStorageBuffers(pass,0,water_buffers.data(),unsigned(water_buffers.size()));
        SDL_PushGPUFragmentUniformData(cb,0,&uniform,sizeof(uniform));SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        SDL_GPUColorTargetInfo exposure{};exposure.texture=s.exposure;exposure.load_op=SDL_GPU_LOADOP_CLEAR;
        exposure.store_op=SDL_GPU_STOREOP_STORE;exposure.clear_color={1,0,0,1};
        pass=SDL_BeginGPURenderPass(cb,&exposure,1,nullptr);require(pass,SDL_GetError());SDL_EndGPURenderPass(pass);
        StarfoxDlssFrameV2 rejection{};rejection.size=sizeof(rejection);rejection.frame=f;
        struct Callback {State& state;StarfoxDlssFrameV2& frame;char error[512]{};} callback{s,rejection};
        void* resources[]{s.color,s.depth,s.motion,s.reconstructed[s.pending_slot],s.exposure,s.bias};
        const bool ok=s.bridge->dispatch(cb,resources,6,3,[](void* user,void* list,void*const* textures,unsigned count)->bool {
            auto& c=*static_cast<Callback*>(user);if(count!=6) return false;auto& f=c.frame.frame;
            f.command=list;f.color=textures[0];f.depth=textures[1];f.motion=textures[2];f.output=textures[3];f.exposure=textures[4];
            for(unsigned i=0;i<5;++i)f.states[i]=i==3?0x8U:0x40U;
            c.frame.current_color_bias=textures[5];c.frame.bias_state=0x40U;
            // Even failed evaluation may allocate SDK resources and mutate its
            // temporal state. Retirement/reset must cover that attempt too.
            c.state.evaluated=true;c.state.sdk_attempted=true;c.state.last_index=f.frame_index;c.state.force_reset=true;
            return c.state.api.evaluate_rejection(c.state.api.module,&c.frame,c.error,sizeof(c.error))==0;
        },&callback);
        require(ok,callback.error[0]?callback.error:SDL_GetError());s.pending=true;
        s.restore(cb,s.pending_slot,ink,native_owners,destination);
        s.status="Native SDK eye and full-panel protected ink encoded";return true;
    } catch(const std::exception& e){s.pending=false;s.force_reset=true;s.status=e.what();return false;}
}
bool GpuCalibratedDlss::enqueue_accepted(void* command,void* ink,void* owners,void* destination) {
    if(!state_) return false;
    try {auto& s=*state_;require(s.accepted && !s.pending,"No accepted native DLSS eye");
        s.restore(static_cast<SDL_GPUCommandBuffer*>(command),s.accepted_slot,ink,owners,destination);
        s.status="Accepted native SDK eye retained without reevaluation";return true;
    } catch(const std::exception& e){state_->status=e.what();return false;}
}
void GpuCalibratedDlss::commit() noexcept {
    if(state_ && state_->pending){state_->accepted_slot=state_->pending_slot;state_->accepted=true;state_->pending=false;state_->force_reset=false;
        state_->sdk_attempted=false;
        state_->accepted_patterns=state_->pending_patterns;state_->accepted_factor=state_->pending_factor;
        state_->accepted_rays=state_->pending_rays;state_->accepted_model_reflections=state_->pending_model_reflections;
        state_->accepted_samples=state_->pending_samples;}
}
void GpuCalibratedDlss::discard() noexcept {if(state_){state_->pending=false;state_->force_reset=true;state_->sdk_attempted=false;}}
bool GpuCalibratedDlss::has_recorded_sdk_work() const noexcept {return state_ && state_->sdk_attempted;}
}
