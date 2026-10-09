#include "starfox/render/gpu_calibrated_ray_composite.hpp"
#include "starfox/render/effect_types.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace starfox::render {
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Settings {
    std::uint32_t width,height,shadow_row,flags;
    float shadow_strength,reflection_strength;
    std::uint32_t reflection_row,material;
    std::uint32_t material_scale;float seconds;std::uint32_t padding[2];
    std::array<std::uint32_t,4> effects;
};
static_assert(sizeof(Settings)==64);
}
struct GpuCalibratedRayComposite::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUShader *vertex{},*fragment{};SDL_GPUGraphicsPipeline* pipeline{};
    SDL_GPUShader* water_fragment{};SDL_GPUGraphicsPipeline* water_pipeline{};
    SDL_GPUShader* water_panel_fragment{};SDL_GPUGraphicsPipeline* water_panel_pipeline{};
    SDL_GPUShader* witness_fragment{};SDL_GPUGraphicsPipeline* witness_pipeline{};
    SDL_GPUSampler* sampler{};SDL_GPUBuffer* zero{};SDL_GPUTransferBuffer* upload{};
    struct Resolved {SDL_GPUTexture* texture;unsigned width,height,slot;SDL_GPUTextureFormat format;};
    std::vector<Resolved> resolved;
    std::uint64_t witness_bytes{};
    std::string status{"Calibrated ray compositor not initialized"};
    SDL_GPUTexture* scratch(unsigned width,unsigned height,unsigned slot,
        SDL_GPUTextureFormat override_format=SDL_GPU_TEXTUREFORMAT_INVALID) {
        const auto selected=override_format==SDL_GPU_TEXTUREFORMAT_INVALID?format:override_format;
        auto found=std::find_if(resolved.begin(),resolved.end(),[&](const auto& image) {
            return image.width==width && image.height==height && image.slot==slot && image.format==selected;
        });
        if(found!=resolved.end()) return found->texture;
        const std::uint64_t bytes=selected==SDL_GPU_TEXTUREFORMAT_R32_FLOAT?std::uint64_t(width)*height*4:0;
        // Retained unequal eye/resize banks must not grow without a bound.
        // This caps owned payload, not available VRAM or driver allocations.
        require(bytes<=1024ULL*1024*1024-witness_bytes,"Calibrated edge atlases exceed the one-GiB retained working bound");
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=selected;
        info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
        info.sample_count=SDL_GPU_SAMPLECOUNT_1;
        auto* texture=SDL_CreateGPUTexture(device,&info);require(texture,SDL_GetError());
        try {resolved.push_back({texture,width,height,slot,selected});}
        catch(...) {SDL_ReleaseGPUTexture(device,texture);throw;}
        witness_bytes+=bytes;
        return texture;
    }
    void prepare_witness() {
        if(witness_pipeline) return;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        if(!witness_fragment) {
            SDL_GPUShaderCreateInfo shader{};shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;
            shader.entrypoint="composite_witness_main";shader.num_samplers=3;
            shader.num_storage_buffers=2;shader.num_uniform_buffers=1;
            shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            shader.code=calibrated_scene_shader::composite_witness_spirv;
            shader.code_size=sizeof(calibrated_scene_shader::composite_witness_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_scene_shader::composite_witness_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_witness_dxil);}
#endif
            witness_fragment=create_gpu_shader(device,&shader);require(witness_fragment,SDL_GetError());
        }
        SDL_GPUColorTargetDescription targets[2]{};targets[0].format=format;targets[1].format=SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=vertex;pipeline.fragment_shader=witness_fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=targets;pipeline.target_info.num_color_targets=2;
        witness_pipeline=create_gpu_graphics_pipeline(device,&pipeline);require(witness_pipeline,SDL_GetError());
    }
    ~State() {
        if(!device) return;
        if(pipeline) SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
        if(water_pipeline) SDL_ReleaseGPUGraphicsPipeline(device,water_pipeline);
        if(water_panel_pipeline) SDL_ReleaseGPUGraphicsPipeline(device,water_panel_pipeline);
        if(witness_pipeline) SDL_ReleaseGPUGraphicsPipeline(device,witness_pipeline);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(water_fragment) SDL_ReleaseGPUShader(device,water_fragment);
        if(water_panel_fragment) SDL_ReleaseGPUShader(device,water_panel_fragment);
        if(witness_fragment) SDL_ReleaseGPUShader(device,witness_fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
        if(zero) SDL_ReleaseGPUBuffer(device,zero);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        for(const auto& image:resolved) SDL_ReleaseGPUTexture(device,image.texture);
    }
};
GpuCalibratedRayComposite::GpuCalibratedRayComposite():state_(std::make_unique<State>()) {}
GpuCalibratedRayComposite::~GpuCalibratedRayComposite()=default;
void GpuCalibratedRayComposite::release_device() noexcept {state_.reset();}
const std::string& GpuCalibratedRayComposite::status() const noexcept {
    static const std::string released{"Calibrated ray compositor released"};return state_?state_->status:released;
}
bool GpuCalibratedRayComposite::initialize(void* device,int color_format) {
    release_device();auto next=std::make_unique<State>();
    try {
        require(device,"Calibrated ray compositor requires a GPU");next->device=static_cast<SDL_GPUDevice*>(device);
        next->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(next->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || next->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || next->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || next->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported calibrated ray target format");
        const bool spirv=(SDL_GetGPUShaderFormats(next->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        SDL_GPUShaderCreateInfo shader{};
        shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        next->vertex=create_gpu_shader(next->device,&shader);require(next->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="composite_fragment_main";
        shader.num_samplers=shader.num_storage_buffers=2;shader.num_uniform_buffers=1;
        shader.code=calibrated_scene_shader::composite_fragment_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_fragment_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_fragment_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_fragment_dxil);}
#endif
        next->fragment=create_gpu_shader(next->device,&shader);require(next->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription target{};target.format=next->format;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=next->vertex;pipeline.fragment_shader=next->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
        pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=&target;pipeline.target_info.num_color_targets=1;
        next->pipeline=create_gpu_graphics_pipeline(next->device,&pipeline);require(next->pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        next->sampler=SDL_CreateGPUSampler(next->device,&sampler);require(next->sampler,SDL_GetError());
        const SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,4,0};
        next->zero=SDL_CreateGPUBuffer(next->device,&buffer);require(next->zero,SDL_GetError());
        const SDL_GPUTransferBufferCreateInfo upload{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,4,0};
        next->upload=SDL_CreateGPUTransferBuffer(next->device,&upload);require(next->upload,SDL_GetError());
        next->status="Calibrated native ray compositor ready";state_=std::move(next);return true;
    } catch(const std::exception& error) {
        // Destroy the partial GPU state; preserve only the diagnostic.
        const std::string failure=error.what();next.reset();state_=std::make_unique<State>();state_->status=failure;return false;
    }
}
bool GpuCalibratedRayComposite::enqueue(void* command,void* source,void* receiver,void* destination,
    unsigned width,unsigned height,const shadows::GpuShadowOutput* shadow,const shadows::GpuReflectionOutput* reflection,
    float shadow_strength,float reflection_strength,unsigned material,unsigned material_scale,CalibratedPostEffects effects,bool water_world,
    CalibratedEdgeCapture capture) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.pipeline && command && source && receiver && destination && width && height,
            "Incomplete calibrated ray composite inputs");
        require(source!=receiver && source!=destination && receiver!=destination,"Aliased calibrated ray targets");
        if(capture.destination) {
            require(capture.slot<3 && !water_world && capture.destination!=source && capture.destination!=receiver
                && capture.destination!=destination && (!capture.prior || (capture.prior!=capture.destination
                    && capture.prior!=source && capture.prior!=receiver && capture.prior!=destination)),"Invalid/aliased calibrated edge witness");
        } else require(!capture.prior && !capture.slot,"Incomplete calibrated edge witness");
        const auto max=std::numeric_limits<std::uint32_t>::max();
        require(std::uint64_t(width)*height*4<=max,"Calibrated ray target exceeds buffer address space");
        require(std::isfinite(shadow_strength) && shadow_strength>=0 && shadow_strength<=1
            && std::isfinite(reflection_strength) && reflection_strength>=0 && reflection_strength<=1,
            "Invalid calibrated ray strength");
        require((material==0 || (valid_material(material) && decorative_material(static_cast<Effect>(material))))
            && material_scale && material_scale<=32768,"Invalid calibrated decorative material/scale");
        require((!effects.world || calibrated_composite_effect(effects.world)) && (!effects.model || calibrated_composite_effect(effects.model))
            && effects.world_intensity<=100 && effects.model_intensity<=100
            && std::isfinite(effects.seconds) && effects.seconds>=0,"Invalid calibrated post effect/intensity/time");
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        Settings settings{width,height,0,srgb?4U:0U,shadow_strength,reflection_strength,0,material,material_scale,effects.seconds,{},
            {effects.world,effects.model,effects.world_intensity,effects.model_intensity}};
        if(capture.destination) settings.padding[1]=capture.slot|(capture.prior?4U:0U);
        if(shadow) {
            require(shadow->device==s.device && shadow->buffer && shadow->width==width && shadow->height==height,
                "Calibrated shadow device/extent mismatch");
            require(!shadow->packed_row_bytes || (shadow->packed_row_bytes>=width && shadow->packed_row_bytes%4==0
                && std::uint64_t(shadow->packed_row_bytes)*height<=max),"Invalid calibrated shadow row stride");
            settings.shadow_row=shadow->packed_row_bytes;settings.flags|=1U;
            if(!settings.shadow_row) settings.flags|=8U;
        }
        if(reflection) {
            require(reflection->device==s.device && reflection->buffer && reflection->width==width && reflection->height==height,
                "Calibrated reflection device/extent mismatch");
            require(reflection->row_bytes>=std::uint64_t(width)*4 && reflection->row_bytes%4==0
                && std::uint64_t(reflection->row_bytes)*height<=max,"Invalid calibrated reflection row stride");
            settings.reflection_row=reflection->row_bytes;settings.flags|=2U;
            if(s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
                settings.flags|=4U;
        }
        if(water_world) {
            const auto layout=shadows::native_water_layers(width,height);
            require(reflection && layout && reflection->water_layers==*layout && reflection->row_bytes==width*4
                && !shadow && !material && !effects.active(),"Incomplete native water underlay layers");
            settings.padding[0]=layout->world_offset;settings.flags|=16U;
        }
        if(effects.active() && (shadow || reflection || material)) {
            // Flat styles read an already resolved 8-bit ray/material image.
            // Fusing them with float ray blends can tip discrete bands/edge
            // thresholds differently. Resolve on the GPU, then style that
            // exact target-format input; no CPU pixels, wait or geometry pass.
            // Keep each negotiated eye extent until release; resizing one eye
            // must not destroy scratch still referenced by the other command.
            auto* resolved=s.scratch(width,height,0);
            if(!enqueue(command,source,receiver,resolved,width,height,shadow,reflection,
                shadow_strength,reflection_strength,material,material_scale)) return false;
            if(!enqueue(command,resolved,receiver,destination,width,height,nullptr,nullptr,
                1,1,0,material_scale,effects,false,capture)) return false;
            s.status="Calibrated resolved ray/material and independent source styles encoded";return true;
        }
        // Optional bindings are real initialized resources, not null handles.
        // Cycling the tiny transfer preserves earlier in-flight encodes.
        auto* mapped=SDL_MapGPUTransferBuffer(s.device,s.upload,true);require(mapped,SDL_GetError());
        std::memset(mapped,0,4);SDL_UnmapGPUTransferBuffer(s.device,s.upload);
        auto* copy=SDL_BeginGPUCopyPass(static_cast<SDL_GPUCommandBuffer*>(command));require(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{s.upload,0};const SDL_GPUBufferRegion to{s.zero,0,4};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        if(capture.destination) s.prepare_witness();
        SDL_GPUColorTargetInfo targets[2]{};targets[0].texture=static_cast<SDL_GPUTexture*>(destination);
        targets[1].texture=static_cast<SDL_GPUTexture*>(capture.destination);
        for(auto& target:targets) {target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;}
        auto* pass=SDL_BeginGPURenderPass(static_cast<SDL_GPUCommandBuffer*>(command),targets,capture.destination?2:1,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,capture.destination?s.witness_pipeline:s.pipeline);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},{static_cast<SDL_GPUTexture*>(receiver),s.sampler},
            {static_cast<SDL_GPUTexture*>(capture.prior?capture.prior:source),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,capture.destination?3:2);
        SDL_GPUBuffer* buffers[]{shadow?static_cast<SDL_GPUBuffer*>(shadow->buffer):s.zero,reflection?static_cast<SDL_GPUBuffer*>(reflection->buffer):s.zero};
        SDL_BindGPUFragmentStorageBuffers(pass,0,buffers,2);
        SDL_PushGPUFragmentUniformData(static_cast<SDL_GPUCommandBuffer*>(command),0,&settings,sizeof(settings));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.status="Calibrated native ray composite encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
bool GpuCalibratedRayComposite::enqueue_water_guides(void* command,void* receiver,void* surfaces,
    void* destination_receiver,void* destination_surfaces,unsigned width,unsigned height,
    const shadows::GpuReflectionOutput& water) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.pipeline && command && receiver && destination_receiver && destination_surfaces,
            "Incomplete native water guide inputs");
        require(receiver!=destination_receiver && receiver!=destination_surfaces
            && destination_receiver!=destination_surfaces && (!surfaces || (surfaces!=receiver
                && surfaces!=destination_receiver && surfaces!=destination_surfaces)),"Aliased native water guide targets");
        const auto layout=shadows::native_water_layers(width,height,water.water_layers.world_offset!=0);
        require(layout && water.device==s.device && water.buffer && water.width==width && water.height==height
            && water.row_bytes==width*4 && water.water_layers==*layout,"Invalid native water guide layout/device/extent");
        if(!s.water_fragment) {
            const bool spirv=(SDL_GetGPUShaderFormats(s.device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUShaderCreateInfo shader{};shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;
            shader.entrypoint="water_guides_fragment_main";
            shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            shader.num_samplers=2;shader.num_storage_buffers=shader.num_uniform_buffers=1;
            shader.code=calibrated_scene_shader::water_guides_spirv;
            shader.code_size=sizeof(calibrated_scene_shader::water_guides_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_scene_shader::water_guides_dxil;shader.code_size=sizeof(calibrated_scene_shader::water_guides_dxil);}
#endif
            s.water_fragment=create_gpu_shader(s.device,&shader);require(s.water_fragment,SDL_GetError());
        }
        if(!s.water_pipeline) {
            SDL_GPUColorTargetDescription targets[2]{};
            targets[0].format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            targets[1].format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
            SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=s.vertex;pipeline.fragment_shader=s.water_fragment;
            pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
            pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
            pipeline.target_info.color_target_descriptions=targets;pipeline.target_info.num_color_targets=2;
            s.water_pipeline=create_gpu_graphics_pipeline(s.device,&pipeline);require(s.water_pipeline,SDL_GetError());
        }
        SDL_GPUColorTargetInfo targets[2]{};
        targets[0].texture=static_cast<SDL_GPUTexture*>(destination_receiver);
        targets[1].texture=static_cast<SDL_GPUTexture*>(destination_surfaces);
        for(auto& target:targets) {target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;}
        auto* pass=SDL_BeginGPURenderPass(static_cast<SDL_GPUCommandBuffer*>(command),targets,2,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.water_pipeline);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(receiver),s.sampler},
            {static_cast<SDL_GPUTexture*>(surfaces?surfaces:receiver),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,2);
        auto* buffer=static_cast<SDL_GPUBuffer*>(water.buffer);SDL_BindGPUFragmentStorageBuffers(pass,0,&buffer,1);
        const std::array<std::uint32_t,4> settings{width,height,layout->surface_offset,surfaces?1U:0U};
        SDL_PushGPUFragmentUniformData(static_cast<SDL_GPUCommandBuffer*>(command),0,settings.data(),sizeof(settings));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.status="Native liquid ownership/depth guides encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
bool GpuCalibratedRayComposite::enqueue_water_panel_guides(void* command,void* receiver,void* surfaces,
    void* destination_receiver,void* destination_surfaces,unsigned width,unsigned height,
    const vr::EyeCamera& camera,const CalibratedGroundRayReceiver& ground) {
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.pipeline && command && receiver && surfaces && destination_receiver && destination_surfaces,
            "Incomplete panel liquid guide inputs");
        require(width && height && width<=16384 && height<=16384
            && std::uint64_t(width)*height<=64ULL*1024*1024,"Invalid panel liquid guide extent");
        require(receiver!=surfaces && receiver!=destination_receiver && receiver!=destination_surfaces
            && surfaces!=destination_receiver && surfaces!=destination_surfaces && destination_receiver!=destination_surfaces,
            "Aliased panel liquid guide targets");
        const auto& water=ground.transport;
        require(water.material==0 && std::isfinite(water.time) && water.time>=0
            && std::all_of(water.world_to_view.begin(),water.world_to_view.end(),[](float v){return std::isfinite(v);})
            && std::all_of(water.camera_position.begin(),water.camera_position.end(),[](float v){return std::isfinite(v);})
            && std::all_of(camera.projection.begin(),camera.projection.end(),[](float v){return std::isfinite(v);})
            && camera.projection[0]>0 && camera.projection[5]>0 && camera.projection[14]<0,
            "Invalid panel liquid coordinates/projection/time");
        const auto& r=water.world_to_view;
        const double determinant=double(r[0])*(double(r[4])*r[8]-double(r[5])*r[7])
            -double(r[1])*(double(r[3])*r[8]-double(r[5])*r[6])+double(r[2])*(double(r[3])*r[7]-double(r[4])*r[6]);
        require(std::isfinite(determinant) && std::abs(determinant)>1.e-12
            && std::isfinite(ground.plane.point.x) && std::isfinite(ground.plane.point.y) && std::isfinite(ground.plane.point.z)
            && std::isfinite(ground.plane.normal.x) && std::isfinite(ground.plane.normal.y) && std::isfinite(ground.plane.normal.z)
            && shadows::dot(ground.plane.normal,ground.plane.normal)>1.e-12,"Degenerate panel liquid transform/plane");
        struct PanelSettings {
            unsigned width,height,padding[2];
            std::array<float,4> projection,point,normal;
            std::array<std::array<float,4>,3> world;
            std::array<float,4> depth_time;
        } settings{width,height,{},
            {width*camera.projection[0]/2,height*camera.projection[5]/2,
                width*(1-camera.projection[8])/2,height*(1+camera.projection[9])/2},
            {float(ground.plane.point.x),float(ground.plane.point.y),float(ground.plane.point.z),0},
            {float(ground.plane.normal.x),float(ground.plane.normal.y),float(ground.plane.normal.z),0},{},
            {camera.projection[10],camera.projection[14],water.time,0}};
        static_assert(sizeof(PanelSettings)==128);
        for(unsigned row=0;row<3;++row) settings.world[row]={r[row*3],r[row*3+1],r[row*3+2],water.camera_position[row]};
        if(!s.water_panel_fragment) {
            const bool spirv=(SDL_GetGPUShaderFormats(s.device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUShaderCreateInfo shader{};shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;
            shader.entrypoint="water_guides_fragment_main";shader.num_samplers=2;shader.num_uniform_buffers=1;
            shader.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            shader.code=calibrated_scene_shader::water_panel_guides_spirv;
            shader.code_size=sizeof(calibrated_scene_shader::water_panel_guides_spirv);
#if defined(_WIN32)
            if(!spirv) {shader.code=calibrated_scene_shader::water_panel_guides_dxil;shader.code_size=sizeof(calibrated_scene_shader::water_panel_guides_dxil);}
#endif
            s.water_panel_fragment=create_gpu_shader(s.device,&shader);require(s.water_panel_fragment,SDL_GetError());
        }
        if(!s.water_panel_pipeline) {
            SDL_GPUColorTargetDescription targets[2]{};targets[0].format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            targets[1].format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
            SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=s.vertex;pipeline.fragment_shader=s.water_panel_fragment;
            pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
            pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
            pipeline.target_info.color_target_descriptions=targets;pipeline.target_info.num_color_targets=2;
            s.water_panel_pipeline=create_gpu_graphics_pipeline(s.device,&pipeline);require(s.water_panel_pipeline,SDL_GetError());
        }
        SDL_GPUColorTargetInfo targets[2]{};targets[0].texture=static_cast<SDL_GPUTexture*>(destination_receiver);
        targets[1].texture=static_cast<SDL_GPUTexture*>(destination_surfaces);
        for(auto& target:targets) {target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;}
        auto* pass=SDL_BeginGPURenderPass(static_cast<SDL_GPUCommandBuffer*>(command),targets,2,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.water_panel_pipeline);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding textures[]{{static_cast<SDL_GPUTexture*>(receiver),s.sampler},{static_cast<SDL_GPUTexture*>(surfaces),s.sampler}};
        SDL_BindGPUFragmentSamplers(pass,0,textures,2);
        SDL_PushGPUFragmentUniformData(static_cast<SDL_GPUCommandBuffer*>(command),0,&settings,sizeof(settings));
        SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        s.status="Unjittered analytic panel liquid guides encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
bool GpuCalibratedRayComposite::enqueue_sequence(void* command,void* source,void* receiver,void* destination,
    unsigned width,unsigned height,const shadows::GpuShadowOutput* shadow,const shadows::GpuReflectionOutput* reflection,
    float shadow_strength,float reflection_strength,unsigned material,unsigned material_scale,
    const std::array<CalibratedPostEffects,3>& effects,void** edge_witness,unsigned witness_bank) {
    if(edge_witness) *edge_witness=nullptr;
    if(!state_) return false;
    try {
        require(state_->pipeline && command && source && receiver && destination && width && height,
            "Incomplete calibrated effect sequence");
        require(source!=receiver && source!=destination && receiver!=destination,"Aliased calibrated effect sequence");
        require(witness_bank<4,"Invalid calibrated edge witness bank");
        require(std::uint64_t(width)*height*4<=std::numeric_limits<std::uint32_t>::max(),
            "Calibrated effect sequence exceeds buffer address space");
        require(std::isfinite(shadow_strength) && shadow_strength>=0 && shadow_strength<=1
            && std::isfinite(reflection_strength) && reflection_strength>=0 && reflection_strength<=1
            && (material==0 || (valid_material(material) && decorative_material(static_cast<Effect>(material))))
            && material_scale && material_scale<=32768,"Invalid calibrated effect sequence strength/material/scale");
        unsigned count=0;
        for(const auto& effect:effects) {
            require((!effect.world || calibrated_composite_effect(effect.world)) && (!effect.model || calibrated_composite_effect(effect.model))
                && effect.world_intensity<=100 && effect.model_intensity<=100
                && std::isfinite(effect.seconds) && effect.seconds>=0,"Invalid calibrated effect sequence selection/time");
            if(effect.active()) ++count;
        }
        if(!count) return enqueue(command,source,receiver,destination,width,height,shadow,reflection,
            shadow_strength,reflection_strength,material,material_scale);
        const bool witness=edge_witness && calibrated_edge_guide(effects,material_scale).active();
        unsigned encoded=0;void* input=source;void* prior_witness=nullptr;
        for(unsigned i=0;i<effects.size();++i) if(const auto& effect=effects[i];effect.active()) {
            void* output=encoded+1==count?destination:state_->scratch(width,height,1+encoded%2);
            void* atlas=witness?state_->scratch(width,height,3+witness_bank*2+encoded%2,SDL_GPU_TEXTUREFORMAT_R32_FLOAT):nullptr;
            if(!enqueue(command,input,receiver,output,width,height,encoded?nullptr:shadow,encoded?nullptr:reflection,
                encoded?1:shadow_strength,encoded?1:reflection_strength,encoded?0:material,material_scale,effect,false,
                {prior_witness,atlas,witness?i:0})) return false;
            input=output;prior_witness=atlas;++encoded;
        }
        if(edge_witness) *edge_witness=prior_witness;
        state_->status="Calibrated primary/manipulation/special-FX sequence encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
}
