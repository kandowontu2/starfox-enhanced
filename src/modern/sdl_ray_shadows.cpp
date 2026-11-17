#include "starfox/modern/sdl_ray_shadows.hpp"
#include "starfox/vr/scene_camera.hpp"
#include "starfox/vr/source_models.hpp"
#include "modern/shaders/generated/ray_shadow.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
namespace starfox::modern {
namespace {
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what);}
std::string sdl_error(const char* what) {return std::string(what)+": "+SDL_GetError();}
// ray_shadow.hlsl Settings.
struct Settings {
    uint32_t count,points,corners,mode;
    uint32_t points_word,residuals_word,corners_word,first_triangle;
    uint32_t object,pad[3];
    std::array<std::array<float,4>,3> rows;
    std::array<float,4> light;
};
static_assert(sizeof(Settings)==112);
struct Object {uint32_t low[3],first,high[3],count;};
static_assert(sizeof(Object)==32);
using render::shadows::Vec3;
}
SdlRayShadows::~SdlRayShadows() {release();}
void SdlRayShadows::release() noexcept {
    if(device_) {
        if(pipeline_) SDL_ReleaseGPUComputePipeline(device_,pipeline_);
        for(auto* buffer:{triangles_,objects_}) if(buffer) SDL_ReleaseGPUBuffer(device_,buffer);
    }
    pipeline_=nullptr;triangles_=objects_=nullptr;device_=nullptr;
    triangle_capacity_=object_capacity_=object_count_=triangle_count_=0;uploads_.release();
}
bool SdlRayShadows::record(SDL_GPUDevice* device,SDL_GPUCommandBuffer* command,const SdlSourceScene& world,
    const vr::EyeCamera& eye,render::shadows::Vec3 toward_light,const std::function<bool(uint32_t)>& caster) {
    object_count_=triangle_count_=0;object_index_.clear();
    try {
        require(device && command,"Missing MODERN ray shadow device");
        if(device!=device_) {release();device_=device;}
        require(vr::ray_rigid(eye),"MODERN ray shadows need a rigid eye view");
        auto light=vr::ray_eye_direction(eye,toward_light);
        const double length=std::sqrt(render::shadows::dot(light,light));
        require(std::isfinite(length) && length>1e-9,"Invalid MODERN shadow light");
        light_=light*(1/length);
        struct Item {SdlSourceModel::RaySource source;Settings settings;};
        std::vector<Item> items;std::vector<Object> objects;
        for(const auto& candidate:world.ray_casters()) {
            if(vr::is_source_shadow_pass(candidate.key) || (caster && !caster(candidate.key))) continue;
            SdlSourceModel::RaySource source;
            if(!candidate.model->ray_source(source)) continue;
            const auto rows=vr::ray_model_rows(eye,candidate.placement,candidate.units);
            if(!rows) continue;
            Settings settings{source.triangles,source.points,source.corners,source.mode,
                source.points_word,source.residuals_word,source.corners_word,triangle_count_,uint32_t(objects.size()),{},*rows,
                {float(light_.x),float(light_.y),float(light_.z),0.F}};
            // Count bit 31 marks large scenery (vr::painter_scenery): the
            // fragment shader ignores it for points within its bounds
            // (tunnels, hangars, arches never shade their own inside).
            object_index_[candidate.key]=uint32_t(objects.size());
            objects.push_back({{~0U,~0U,~0U},triangle_count_,{0,0,0},source.triangles|(candidate.enclosing?0x80000000U:0U)});
            triangle_count_+=source.triangles;
            items.push_back({source,settings});
        }
        if(items.empty()) {object_index_.clear();status_="MODERN ray shadows: no casters";return true;}
        const auto grow=[&](SDL_GPUBuffer*& buffer,uint32_t& capacity,uint64_t bytes) {
            require(bytes<=256ULL*1024*1024,"MODERN shadow casters exceed the buffer budget");
            if(bytes<=capacity) return;
            if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
            capacity=uint32_t(std::max<uint64_t>(bytes,uint64_t(capacity)*2));
            SDL_GPUBufferCreateInfo info{};info.size=capacity;
            info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
            buffer=SDL_CreateGPUBuffer(device,&info);
            require(buffer!=nullptr,sdl_error("Create MODERN shadow caster buffer"));
        };
        grow(triangles_,triangle_capacity_,uint64_t(triangle_count_)*64);
        grow(objects_,object_capacity_,uint64_t(objects.size())*sizeof(Object));
        if(!pipeline_) {
            const auto formats=SDL_GetGPUShaderFormats(device);
            const bool spirv=(formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
            info.code=spirv?ray_shadow_shader::spirv_compute:ray_shadow_shader::dxil_compute;
            info.code_size=spirv?sizeof(ray_shadow_shader::spirv_compute):sizeof(ray_shadow_shader::dxil_compute);
            require(spirv || (formats&SDL_GPU_SHADERFORMAT_DXIL)!=0,"MODERN ray shadows need SPIR-V or DXIL");
            info.entrypoint="main";
            info.num_readonly_storage_buffers=2;info.num_readwrite_storage_buffers=2;info.num_uniform_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            pipeline_=SDL_CreateGPUComputePipeline(device,&info);
            require(pipeline_!=nullptr,sdl_error("Create MODERN ray shadow pipeline"));
        }
        // Seed the object table (open bounds, triangle ranges) every frame.
        uploads_.add(objects_,0,objects.data(),uint32_t(objects.size()*sizeof(Object)),true);
        require(uploads_.flush(device,command),uploads_.status());
        SDL_GPUStorageBufferReadWriteBinding writable[2]{};
        writable[0].buffer=triangles_;writable[0].cycle=true;
        writable[1].buffer=objects_;
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,writable,2);
        require(pass!=nullptr,sdl_error("Begin MODERN ray shadow pass"));
        SDL_BindGPUComputePipeline(pass,pipeline_);
        for(const auto& item:items) {
            SDL_GPUBuffer* readable[]{item.source.arena,item.source.topology};
            SDL_BindGPUComputeStorageBuffers(pass,0,readable,2);
            SDL_PushGPUComputeUniformData(command,0,&item.settings,sizeof(item.settings));
            SDL_DispatchGPUCompute(pass,(item.settings.count+63)/64,1,1);
        }
        SDL_EndGPUComputePass(pass);
        object_count_=uint32_t(objects.size());
        status_="MODERN ray shadows recorded";return true;
    } catch(const std::exception& error) {uploads_.clear();object_count_=triangle_count_=0;status_=error.what();return false;}
}
}
namespace starfox::modern {
std::vector<SdlRayShadows::PointLight> SdlRayShadows::lights(const vr::EyeCamera& eye,
    std::span<const vr::SourceComputeModel> models,std::span<const vr::DrawPacket> packets,std::span<const uint32_t> handles,
    const EmitterKind& emitter,std::optional<BombFlash> bomb,std::size_t limit,float radius_scale,uint32_t tick) const {
    return vr::ray_point_lights(eye,light_,object_index_,models,packets,handles,emitter,bomb,limit,radius_scale,tick);
}
}
