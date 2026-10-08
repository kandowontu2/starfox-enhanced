#include "starfox/render/gpu_calibrated_scene_fx.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include "shaders/generated/calibrated_scene_fx.hpp"
#include "shaders/generated/calibrated_scene_portable.hpp"
#include <SDL3/SDL.h>
#include <cmath>
#include <stdexcept>
namespace starfox::render {
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct ProjectUniform {
    std::array<float,12> view_rows;
    std::array<float,4> projection;
    unsigned width,height,scale,count;
    std::array<float,4> origin_high{},origin_low{},player_high{},player_low{};
    unsigned has_player{},padding[3]{};
    std::array<float,12> previous_view_rows{};
    std::array<float,4> previous_projection{},previous_origin_high{},previous_origin_low{};
};
struct FragmentUniform {unsigned width,height,scale,flags;};
static_assert(sizeof(ProjectUniform)==256 && sizeof(FragmentUniform)==16);
static_assert(scene_fx_world_capacity==96 && scene_fx_capacity==48,"Update the native shader payload ABI with these capacities");
inline constexpr unsigned point_chunk=scene_fx_world_capacity*2;
static_assert(point_chunk*sizeof(std::array<float,4>)<=4096);
// This is lossless payload conversion, not point projection or camera motion.
// The shader subtracts/wraps before applying the small composed view matrix.
void split(std::array<double,3> source,std::array<float,4>& high,std::array<float,4>& low) {
    for(unsigned axis=0;axis<3;++axis) {high[axis]=float(source[axis]);low[axis]=float(source[axis]-double(high[axis]));}
}
ProjectUniform projection_settings(unsigned width,unsigned height,unsigned scale,const SceneFxWorldFrame& world,
    const vr::EyeCamera& eye,const vr::Matrix4& rig,std::array<double,3> origin) {
    require(width && height && width<=16384 && height<=16384 && scale && scale<=16384
        && valid_scene_fx_world_frame(world),"Invalid native world-effect payload/extent");
    for(double value:origin) require(std::isfinite(value) && std::abs(value)<=1.e9,"Invalid native source origin");
    for(float value:rig) require(std::isfinite(value) && std::abs(value)<=1.e4F,"Invalid native source-to-rig matrix");
    require(rig[3]==0 && rig[7]==0 && rig[11]==0 && rig[15]==1,"Native source-to-rig matrix must be affine");
    const auto composed=vr::model_eye_camera(eye,rig);require(bool(composed),"Invalid native tracked eye");
    const auto& p=eye.projection;
    for(float value:p) require(std::isfinite(value) && std::abs(value)<=1.e8F,"Invalid native scene perspective");
    require(p[0]>0 && p[5]>0 && p[1]==0 && p[2]==0 && p[3]==0 && p[4]==0 && p[6]==0
        && p[7]==0 && p[11]==-1 && p[12]==0 && p[13]==0 && p[15]==0 && p[10]<=-1 && p[14]<0,
        "Unsupported native scene perspective");
    const float fx=float(width)*p[0]*.5F,fy=float(height)*p[5]*.5F,ratio=fy/fx;
    require(std::isfinite(fx) && std::isfinite(fy) && fx>=1.e-4F && fy>=1.e-4F && fx<=1.e8F && fy<=1.e8F
        && std::isfinite(ratio) && ratio>=1.e-4F && ratio<=1.e4F,"Native scene focal ratio exceeds bounds");
    for(float value:composed->view) require(std::isfinite(value) && std::abs(value)<=1.e6F,"Native scene view exceeds bounds");
    ProjectUniform u{};u.view_rows=vr::scene_constants(*composed).view_rows;
    u.projection={p[0],p[5],p[8],p[9]};u.width=width;u.height=height;u.scale=scale;u.count=world.count;
    split(origin,u.origin_high,u.origin_low);
    if(world.player) {u.has_player=1;split(*world.player,u.player_high,u.player_low);}
    return u;
}
auto point_payload(const SceneFxWorldFrame& world) {
    std::array<std::array<float,4>,scene_fx_world_capacity*4> points{};
    for(unsigned n=0;n<world.count;++n) {
        const auto& p=world.points[n];split(p.position,points[n*4],points[n*4+1]);
        points[n*4+1][3]=p.radius;points[n*4+2]={p.strength,p.type,p.age,0};points[n*4+3]=p.extra;
    }
    return points;
}
}
struct GpuCalibratedSceneFx::State {
    SDL_GPUDevice* device{};SDL_GPUTextureFormat format{};
    SDL_GPUComputePipeline *project{},*particles{};SDL_GPUBuffer* particle_points{};
    SDL_GPUShader *vertex{},*fragment{};
    SDL_GPUGraphicsPipeline* compose{};SDL_GPUSampler* sampler{};SDL_GPUBuffer* points{};
    std::string status{"Native scene enhancements not initialized"};
    ~State() {
        if(!device) return;
        if(project) SDL_ReleaseGPUComputePipeline(device,project);
        if(particles) SDL_ReleaseGPUComputePipeline(device,particles);
        if(particle_points) SDL_ReleaseGPUBuffer(device,particle_points);
        if(compose) SDL_ReleaseGPUGraphicsPipeline(device,compose);
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(fragment) SDL_ReleaseGPUShader(device,fragment);
        if(sampler) SDL_ReleaseGPUSampler(device,sampler);
        if(points) SDL_ReleaseGPUBuffer(device,points);
    }
};
GpuCalibratedSceneFx::GpuCalibratedSceneFx():state_(std::make_unique<State>()) {}
GpuCalibratedSceneFx::~GpuCalibratedSceneFx()=default;
void GpuCalibratedSceneFx::release_device() noexcept {state_.reset();}
const std::string& GpuCalibratedSceneFx::status() const noexcept {
    static const std::string released{"Native scene enhancements released"};return state_?state_->status:released;
}
bool GpuCalibratedSceneFx::initialize(void* device,int color_format) {
    release_device();auto s=std::make_unique<State>();
    try {
        require(device,"Native scene enhancements require a GPU");s->device=static_cast<SDL_GPUDevice*>(device);
        s->format=static_cast<SDL_GPUTextureFormat>(color_format);
        require(s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
            || s->format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s->format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
            "Unsupported native scene enhancement format");
        const auto formats=SDL_GetGPUShaderFormats(s->device);const bool spirv=(formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        require(spirv || (formats&SDL_GPU_SHADERFORMAT_DXIL),"Native scene enhancements require SPIR-V or DXIL");
        SDL_GPUComputePipelineCreateInfo compute{};compute.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
        compute.num_readwrite_storage_buffers=1;compute.num_uniform_buffers=3;
        compute.threadcount_x=compute.threadcount_y=compute.threadcount_z=1;
        compute.entrypoint="scene_fx_project_main";
        compute.code=calibrated_scene_fx_shader::project_spirv;compute.code_size=sizeof(calibrated_scene_fx_shader::project_spirv);
#if defined(_WIN32)
        if(!spirv) {compute.code=calibrated_scene_fx_shader::project_dxil;compute.code_size=sizeof(calibrated_scene_fx_shader::project_dxil);}
#endif
        s->project=create_gpu_compute_pipeline(s->device,&compute);require(s->project,SDL_GetError());
        SDL_GPUShaderCreateInfo shader{};shader.format=compute.format;shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;
        shader.entrypoint="composite_vertex_main";
        shader.code=calibrated_scene_shader::composite_vertex_spirv;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_shader::composite_vertex_dxil;shader.code_size=sizeof(calibrated_scene_shader::composite_vertex_dxil);}
#endif
        s->vertex=create_gpu_shader(s->device,&shader);require(s->vertex,SDL_GetError());
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="scene_fx_fragment_main";
        shader.num_samplers=3;shader.num_storage_buffers=shader.num_uniform_buffers=1;
        shader.code=calibrated_scene_fx_shader::fragment_spirv;shader.code_size=sizeof(calibrated_scene_fx_shader::fragment_spirv);
#if defined(_WIN32)
        if(!spirv) {shader.code=calibrated_scene_fx_shader::fragment_dxil;shader.code_size=sizeof(calibrated_scene_fx_shader::fragment_dxil);}
#endif
        s->fragment=create_gpu_shader(s->device,&shader);require(s->fragment,SDL_GetError());
        SDL_GPUColorTargetDescription target{};target.format=s->format;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};pipeline.vertex_shader=s->vertex;pipeline.fragment_shader=s->fragment;
        pipeline.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pipeline.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;pipeline.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions=&target;pipeline.target_info.num_color_targets=1;
        s->compose=create_gpu_graphics_pipeline(s->device,&pipeline);require(s->compose,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s->sampler=SDL_CreateGPUSampler(s->device,&sampler);require(s->sampler,SDL_GetError());
        const SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            unsigned((2+scene_fx_capacity*3)*sizeof(std::array<float,4>)),0};
        s->points=SDL_CreateGPUBuffer(s->device,&buffer);require(s->points,SDL_GetError());
        s->status="Native scene enhancements ready";state_=std::move(s);return true;
    } catch(const std::exception& error) {const std::string message=error.what();s.reset();state_=std::make_unique<State>();state_->status=message;return false;}
}
bool GpuCalibratedSceneFx::enqueue(void* command,void* source,void* ownership,void* surfaces,void* destination,
    unsigned width,unsigned height,unsigned scale,const SceneFxWorldFrame& world,const vr::EyeCamera& eye,
    const vr::Matrix4& source_to_rig,std::array<double,3> origin,void** projected_output) {
    if(projected_output) *projected_output=nullptr;
    if(!state_) return false;
    try {
        auto& s=*state_;
        require(s.compose && command && source && ownership && surfaces && destination
            && source!=ownership && source!=surfaces && source!=destination && ownership!=surfaces
            && ownership!=destination && surfaces!=destination,"Invalid native scene enhancement inputs");
        const auto uniform=projection_settings(width,height,scale,world,eye,source_to_rig,origin);
        const auto points=point_payload(world);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        // Cycle at EVERY eye/sample projection. Encoded earlier readers retain
        // their buffer backing even when a later eye is recorded before submit.
        const SDL_GPUStorageBufferReadWriteBinding output{s.points,true,0,0,0};
        auto* compute=SDL_BeginGPUComputePass(cmd,nullptr,0,&output,1);require(compute,SDL_GetError());
        SDL_BindGPUComputePipeline(compute,s.project);SDL_PushGPUComputeUniformData(cmd,0,&uniform,sizeof(uniform));
        SDL_PushGPUComputeUniformData(cmd,1,points.data(),point_chunk*sizeof(points[0]));
        SDL_PushGPUComputeUniformData(cmd,2,points.data()+point_chunk,point_chunk*sizeof(points[0]));
        SDL_DispatchGPUCompute(compute,1,1,1);SDL_EndGPUComputePass(compute);
        SDL_GPUColorTargetInfo target{};target.texture=static_cast<SDL_GPUTexture*>(destination);
        target.load_op=SDL_GPU_LOADOP_DONT_CARE;target.store_op=SDL_GPU_STOREOP_STORE;
        auto* pass=SDL_BeginGPURenderPass(cmd,&target,1,nullptr);require(pass,SDL_GetError());
        SDL_BindGPUGraphicsPipeline(pass,s.compose);
        const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_SetGPUViewport(pass,&viewport);
        const SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUScissor(pass,&scissor);
        const SDL_GPUTextureSamplerBinding inputs[]{{static_cast<SDL_GPUTexture*>(source),s.sampler},
            {static_cast<SDL_GPUTexture*>(ownership),s.sampler},{static_cast<SDL_GPUTexture*>(surfaces),s.sampler}};
        const bool srgb=s.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || s.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const FragmentUniform fragment{width,height,scale,unsigned(srgb)};
        SDL_BindGPUFragmentSamplers(pass,0,inputs,3);SDL_BindGPUFragmentStorageBuffers(pass,0,&s.points,1);
        SDL_PushGPUFragmentUniformData(cmd,0,&fragment,sizeof(fragment));SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);
        if(projected_output) *projected_output=s.points;
        s.status="Native scene enhancements encoded";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
bool GpuCalibratedSceneFx::project_particles(void* command,unsigned width,unsigned height,unsigned scale,
    const SceneFxWorldFrame& world,const vr::EyeCamera& eye,const vr::Matrix4& rig,std::array<double,3> origin,
    const CalibratedSceneFxPrevious* previous,void*& output) {
    output=nullptr;if(!state_) return false;
    try {
        auto& s=*state_;require(s.device && s.project && command,"Invalid native particle projection owner/command");
        auto uniform=projection_settings(width,height,scale,world,eye,rig,origin);
        const auto points=point_payload(world);
        std::array<std::array<float,4>,point_chunk> before{};
        if(previous) {
            require(previous->world,"Missing accepted native particle source");
            const auto old=projection_settings(width,height,scale,*previous->world,previous->camera,previous->source_to_rig,previous->origin);
            uniform.previous_view_rows=old.view_rows;uniform.previous_projection=old.projection;
            uniform.previous_origin_high=old.origin_high;uniform.previous_origin_low=old.origin_low;
            for(unsigned n=0;n<world.count;++n) {
                const auto& point=world.points[n];
                if(point.type<2 || point.type>7 || point.identity==std::array<std::int64_t,4>{}) continue;
                // Match source identities BEFORE independent eye clipping. A
                // previous eye may have compacted/capped a different population.
                const auto same=[&](const SceneFxWorldPoint& p){return p.identity==point.identity;};
                if(std::count_if(world.points.begin(),world.points.begin()+world.count,same)!=1) continue;
                const auto& prior=*previous->world;
                if(std::count_if(prior.points.begin(),prior.points.begin()+prior.count,same)!=1) continue;
                const auto found=std::find_if(prior.points.begin(),prior.points.begin()+prior.count,same);
                if(found->type!=point.type) continue;
                split(found->position,before[n*2],before[n*2+1]);before[n*2+1][3]=1;
            }
        }
        if(!s.particles) {
            const bool spv=(SDL_GetGPUShaderFormats(s.device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUComputePipelineCreateInfo info{};info.format=spv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.entrypoint="scene_fx_project_main";info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=4;
            info.threadcount_x=info.threadcount_y=info.threadcount_z=1;
            info.code=calibrated_scene_fx_shader::particles_spirv;info.code_size=sizeof(calibrated_scene_fx_shader::particles_spirv);
#if defined(_WIN32)
            if(!spv) {info.code=calibrated_scene_fx_shader::particles_dxil;info.code_size=sizeof(calibrated_scene_fx_shader::particles_dxil);}
#endif
            s.particles=create_gpu_compute_pipeline(s.device,&info);require(s.particles,SDL_GetError());
        }
        if(!s.particle_points) {
            const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
                unsigned((2+scene_fx_capacity*4)*sizeof(std::array<float,4>)),0};
            s.particle_points=SDL_CreateGPUBuffer(s.device,&info);require(s.particle_points,SDL_GetError());
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const SDL_GPUStorageBufferReadWriteBinding target{s.particle_points,true,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&target,1);require(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,s.particles);SDL_PushGPUComputeUniformData(cmd,0,&uniform,sizeof(uniform));
        SDL_PushGPUComputeUniformData(cmd,1,points.data(),point_chunk*sizeof(points[0]));
        SDL_PushGPUComputeUniformData(cmd,2,points.data()+point_chunk,point_chunk*sizeof(points[0]));
        SDL_PushGPUComputeUniformData(cmd,3,before.data(),point_chunk*sizeof(before[0]));
        SDL_DispatchGPUCompute(pass,1,1,1);SDL_EndGPUComputePass(pass);output=s.particle_points;
        s.status="Native joint particles projected on GPU";return true;
    } catch(const std::exception& error) {state_->status=error.what();return false;}
}
}
