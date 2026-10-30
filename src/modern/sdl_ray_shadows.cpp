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
// vr::ShadowView / vr::shadow_model_transform (shadow_camera.hpp, which pulls
// in OpenXR): native eye axes are +Y down, +Z forward, in source units.
bool rigid(const vr::EyeCamera& eye) {
    const auto& m=eye.view;
    for(auto value:m) if(!std::isfinite(value)) return false;
    if(m[3]!=0 || m[7]!=0 || m[11]!=0 || m[15]!=1) return false;
    for(unsigned a=0;a<3;++a) for(unsigned b=0;b<3;++b) {
        double product=0;for(unsigned k=0;k<3;++k) product+=double(m[k*4+a])*m[k*4+b];
        if(std::abs(product-(a==b?1.:0.))>1e-4) return false;
    }
    return true;
}
Vec3 eye_direction(const vr::EyeCamera& eye,Vec3 p) {
    const auto& m=eye.view;
    return {m[0]*p.x+m[4]*p.y+m[8]*p.z,-(m[1]*p.x+m[5]*p.y+m[9]*p.z),-(m[2]*p.x+m[6]*p.y+m[10]*p.z)};
}
std::optional<std::array<std::array<float,4>,3>> model_rows(const vr::EyeCamera& eye,const vr::Matrix4& placement,double units) {
    if(!std::isfinite(units) || units<=0) return {};
    const auto composed=vr::model_eye_camera(eye,placement);
    if(!composed) return {};
    const auto& m=composed->view;
    constexpr double sign[]{1,-1,-1};
    std::array<std::array<float,4>,3> rows{};
    for(unsigned row=0;row<3;++row) {
        for(unsigned column=0;column<3;++column)
            rows[row][column]=float(sign[row]*m[column*4+row]*sign[column]*256./units);
        rows[row][3]=float(sign[row]*m[12+row]*256.);
        for(float value:rows[row]) if(!std::isfinite(value)) return {};
    }
    return rows;
}
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
        require(rigid(eye),"MODERN ray shadows need a rigid eye view");
        auto light=eye_direction(eye,toward_light);
        const double length=std::sqrt(render::shadows::dot(light,light));
        require(std::isfinite(length) && length>1e-9,"Invalid MODERN shadow light");
        light_=light*(1/length);
        struct Item {SdlSourceModel::RaySource source;Settings settings;};
        std::vector<Item> items;std::vector<Object> objects;
        for(const auto& candidate:world.ray_casters()) {
            if(vr::is_source_shadow_pass(candidate.key) || (caster && !caster(candidate.key))) continue;
            SdlSourceModel::RaySource source;
            if(!candidate.model->ray_source(source)) continue;
            const auto rows=model_rows(eye,candidate.placement,candidate.units);
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
    struct Candidate {PointLight light;double priority{};};
    std::vector<Candidate> found;
    const auto length=[](const std::array<float,3>& v){return std::sqrt(double(v[0])*v[0]+double(v[1])*v[1]+double(v[2])*v[2]);};
    const auto is_compute=[&](uint32_t key) {
        return std::any_of(models.begin(),models.end(),[&](const auto& model){return model.key==key;});
    };
    // `light` holds the look: colour, reach, hardness and so on.
    const auto place=[&](std::size_t packet,uint32_t key,PointLight light,float strength,float back=0) {
        if(packet>=packets.size()) return;
        // The object's origin and forward axis in native eye space. A
        // compute-route model keeps its placement in its projection pose
        // (camera-space translation and rotation rows, model units), not in
        // the packet matrix: map the pose through the packet like its points.
        const vr::SourceComputeModel* compute=nullptr;
        for(const auto& model:models) if(model.key==key) {compute=&model;break;}
        auto rows=model_rows(eye,packets[packet].model,compute?double(compute->units):256.);
        if(!rows) return;
        if(compute) {
            const auto& pose=compute->model.projection.continuous_poses[0];
            const auto apply=[&](const std::array<double,3>& p,bool point) {
                std::array<float,3> out{};
                for(unsigned row=0;row<3;++row)
                    out[row]=float((*rows)[row][0]*p[0]+(*rows)[row][1]*p[1]+(*rows)[row][2]*p[2]+(point?(*rows)[row][3]:0.F));
                return out;
            };
            const auto origin=apply({pose.translation[0],pose.translation[1],pose.translation[2]},true);
            const auto axis=apply({pose.row0[2],pose.row1[2],pose.row2[2]},false);
            for(unsigned row=0;row<3;++row) {(*rows)[row][3]=origin[row];(*rows)[row][2]=axis[row];}
        }
        std::array<float,3> forward{(*rows)[0][2],(*rows)[1][2],(*rows)[2][2]};
        const double forward_length=length(forward);
        if(forward_length>1e-9) for(auto& value:forward) value=float(value/forward_length);
        // `back` moves the light toward the object's tail (an engine).
        light.position={float((*rows)[0][3]-forward[0]*back),float((*rows)[1][3]-forward[1]*back),float((*rows)[2][3]-forward[2]*back)};
        // CPU-built packets (beam lines, sprites) may carry their geometry
        // already placed: light from the centre of their own vertices.
        const auto& geometry=packets[packet].geometry;
        if(!is_compute(key) && (!geometry.vertex_view().empty() || !geometry.line_view().empty())) {
            double centre[3]{};std::size_t count=0;
            for(const auto& list:{geometry.vertex_view(),geometry.line_view()}) for(const auto& vertex:list) {
                for(unsigned c=0;c<3;++c) centre[c]+=vertex.position[c];
                ++count;
            }
            if(const auto placed=vr::model_eye_camera(eye,packets[packet].model);placed && count) {
                const auto& m=placed->view;
                for(auto& value:centre) value/=double(count);
                // Scene metres (+Y up, -Z forward) to native eye units.
                constexpr double sign[]{1,-1,-1};
                for(unsigned row=0;row<3;++row)
                    light.position[row]=float(sign[row]*(m[row]*centre[0]+m[4+row]*centre[1]+m[8+row]*centre[2]+m[12+row])*256.);
            }
        }
        if(light.projected) {
            // A pool on the ground stretched along the flight direction:
            // the forward axis flattened across the sun's rays.
            const double along=forward[0]*light_.x+forward[1]*light_.y+forward[2]*light_.z;
            std::array<float,3> axis{float(forward[0]-light_.x*along),float(forward[1]-light_.y*along),float(forward[2]-light_.z*along)};
            const double axis_length=length(axis);
            if(axis_length>1e-6) {for(auto& value:axis) value=float(value/axis_length);light.axis=axis;}
            else light.aspect=1;
        }
        if(light.projected && light.lift>0) {
            // The pool's centre (along the sun) stays put; the light is
            // traced at the lifted point (the shader measures the height
            // from the real light, `lift` below it).
            light.position={float(light.position[0]+light_.x*light.lift),float(light.position[1]+light_.y*light.lift),
                float(light.position[2]+light_.z*light.lift)};
        }
        light.radius*=radius_scale;
        for(auto& value:light.colour) value*=strength;
        if(const auto own=object_index_.find(key);own!=object_index_.end() && !light.self_shadowed) light.object=own->second;
        const double distance=length(light.position);
        if(!std::isfinite(distance)) return;
        found.push_back({light,double(strength)*light.radius/(distance+light.radius)});
    };
    // A ship's half wingspan in native units (compute models: projected
    // vertex X through the pose's X column), for its engine glow's size.
    const auto half_span=[&](uint32_t key)->float {
        for(const auto& model:models) if(model.key==key) {
            const auto& projection=model.model.projection;
            double widest=0;
            for(const auto& vertex:projection.continuous_vertices) {
                const auto& pose=projection.continuous_poses[std::min<std::size_t>(vertex.pose,projection.continuous_poses.size()-1)];
                const double column=std::sqrt(double(pose.row0[0])*pose.row0[0]+double(pose.row1[0])*pose.row1[0]
                    +double(pose.row2[0])*pose.row2[0]);
                widest=std::max(widest,std::abs(double(vertex.x))*column);
            }
            return model.units>0?float(widest*256./model.units):0.F;
        }
        return 0.F;
    };
    const auto coloured=[](std::array<float,3> colour,float radius,float hardness) {
        PointLight light;light.colour=colour;light.radius=radius;light.hardness=hardness;return light;
    };
    // Authored coloured lights by emitter class: each kind has its own
    // colour, reach and edge, chosen for the look rather than taken from the art.
    for(std::size_t index=0;index<packets.size() && index<handles.size();++index) {
        const uint32_t key=handles[index];
        if(vr::is_source_shadow_pass(key) || key>=0x20000U) continue;
        const unsigned kind=emitter?emitter(key):EmitterNone;
        switch(kind) {
        case EmitterLaser: // enemy lasers: yellow, on the ground and anything they pass
            place(index,key,coloured({1.F,.82F,.3F},1000.F,0.F),1.F);break;
        case EmitterArwingLaser: // every Arwing shot (laser, twin laser, plasma): blue
            place(index,key,coloured({.3F,.5F,1.F},1000.F,0.F),1.F);break;
        case EmitterExplosion: // a large dark orange/red light over everything near
            place(index,key,coloured({.85F,.22F,.04F},3200.F,.2F),1.8F);break;
        case EmitterPlasma: // enemy ring shots and plasma balls: blue
            place(index,key,coloured({.3F,.5F,1.F},1000.F,0.F),1.F);break;
        case EmitterBoost: { // the boost flame: blue on the floor and walls (tunnels), never on ships
            auto flame=coloured({.3F,.55F,1.F},700.F,.2F);flame.background_only=true;
            place(index,key,flame,1.4F);
            break;
        }
        case EmitterEngine: {
            // The engine's glow on the ground at the tail of the ship's
            // shadow: round, soft from its centre, about the ship's width
            // across, fading as the ship climbs (red/yellow flicker). Traced
            // to the engine with the ship's own hull as an occluder, so the
            // side toward the ship is cut off along the hull, a little
            // differently from the sun's shadow (the light is much nearer).
            // The headset, without ray tracing, draws vr::ModernFrame::engine_glow.
            // The light sits just behind the tail (0.3 half spans behind the
            // origin) and is traced 2 half spans up the sun's rays from there:
            // the cut follows the fuselage's shadow, the wings' stays lit.
            constexpr float engine_back=.3F,engine_lift=2.F;
            const bool red=(tick/2U)%2U==0U;
            const float span=half_span(key);
            auto light=coloured(red?std::array<float,3>{1.F,.35F,.1F}:std::array<float,3>{1.F,.75F,.2F},
                span>0?span*2.F:40.F,0.F);
            // Its present size near the ground (low flight is 30-55 units
            // up; a half span is 35), three times as wide and gone by about
            // two wingspans up (4.5 half spans).
            light.projected=true;light.fade_height=span>0?span*4.5F:160.F;light.spread=3.F;
            light.self_shadowed=true;light.lift=span>0?span*engine_lift:40.F;
            place(index,key,light,1.6F,span>0?span*engine_back:20.F);
            break;
        }
        case EmitterBomb: // the smart bomb and enemy bombs: blue-white
            place(index,key,coloured({.6F,.75F,1.F},2000.F,.5F),.8F);break;
        default: break;
        }
    }
    if(bomb && bomb->radius>0) {
        // The detonation has no world position on the cartridge (a screen
        // circle): a large hard-edged light along the view ray through its centre.
        constexpr double depth=2400;
        PointLight light;
        light.position={float((bomb->centre_x-128.)/256.*depth),float((bomb->centre_y-112.)/256.*depth),float(depth)};
        light.radius=std::max(8000.F,bomb->radius*48.F)*radius_scale;light.hardness=.6F;light.colour=bomb->colour;
        for(auto& value:light.colour) value*=1.F;
        found.push_back({light,1e9});
    }
    std::sort(found.begin(),found.end(),[](const auto& a,const auto& b){return a.priority>b.priority;});
    std::vector<PointLight> result;
    for(std::size_t i=0;i<found.size() && i<limit;++i) result.push_back(found[i].light);
    return result;
}
}
