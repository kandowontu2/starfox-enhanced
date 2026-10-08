// Fixture-only readback. The renderer itself performs no CPU projection,
// geometry download or eye-image transfer through host memory.
#include "starfox/render/gpu_calibrated_scene.hpp"
#include "starfox/render/calibrated_environment.hpp"
#include "starfox/render/calibrated_ground.hpp"
#include "starfox/render/calibrated_game_motion.hpp"
#include "starfox/render/lava_surface.hpp"
#include "starfox/render/gpu_calibrated_depth.hpp"
#include "starfox/render/gpu_calibrated_msaa.hpp"
#include "starfox/render/gpu_calibrated_temporal_aa.hpp"
#include "starfox/render/temporal_aa.hpp"
#include "starfox/render/sdl_multisample.h"
#include "starfox/render/depth_enhancements.hpp"
#include "starfox/vr/scene_packet_validation.hpp"
#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/render/grid_projection.hpp"
#include "starfox/render/grid_line_sample.hpp"
#include "starfox/render/effects.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include "starfox/render/vulkan_hardware_rt.hpp"
#include "reflected_liquid_oracle.hpp"
#include "native_water_fixture.hpp"
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace {
using namespace starfox;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
constexpr unsigned width=256,height=192;
constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct Gpu {
    SDL_GPUDevice* device{};
    SDL_GPUTextureFormat format{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
    SDL_GPUTexture *color{},*depth{};SDL_GPUTransferBuffer* read{};SDL_GPUCommandBuffer* command{};
    ~Gpu() {
        if(device) {
            if(command) SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);
            if(color) SDL_ReleaseGPUTexture(device,color);if(depth) SDL_ReleaseGPUTexture(device,depth);
            if(read) SDL_ReleaseGPUTransferBuffer(device,read);SDL_DestroyGPUDevice(device);
        }
        SDL_Quit();
    }
    void initialize(const char* backend,bool reflection_interop=false) {
        check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        if(reflection_interop && std::string_view(backend)=="vulkan") {
            const auto props=SDL_CreateProperties();check(props,SDL_GetError());
            SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
            SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
            SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
            SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,backend);
            const bool bridge=render::shadows::SdlDxrShadows::request_vulkan_interop(props);
            if(bridge) device=SDL_CreateGPUDeviceWithProperties(props);
            SDL_DestroyProperties(props);check(bridge,"Windows Vulkan/DXR fixture interop unavailable");
        } else device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,true,backend);
        check(device,SDL_GetError());
        std::cout<<"Calibrated scene device: "<<backend<<" / "
            <<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<'\n';
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        color=SDL_CreateGPUTexture(device,&info);check(color,SDL_GetError());
        info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        depth=SDL_CreateGPUTexture(device,&info);check(depth,SDL_GetError());
        const SDL_GPUTransferBufferCreateInfo download{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*4,0};
        read=SDL_CreateGPUTransferBuffer(device,&download);check(read,SDL_GetError());
    }
    void change_color_format(SDL_GPUTextureFormat format) {
        check(!command && SDL_WaitForGPUIdle(device),"Cannot change a pending fixture target");
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=format;
        info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
        info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        auto* next=SDL_CreateGPUTexture(device,&info);check(next,SDL_GetError());
        SDL_ReleaseGPUTexture(device,color);color=next;this->format=format;
    }
    std::vector<unsigned char> finish() {
        auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
        SDL_GPUTextureRegion region{color,0,0,0,0,0,width,height,1};
        SDL_GPUTextureTransferInfo target{};target.transfer_buffer=read;target.pixels_per_row=width;target.rows_per_layer=height;
        SDL_DownloadFromGPUTexture(copy,&region,&target);SDL_EndGPUCopyPass(copy);
        const bool submitted=SDL_SubmitGPUCommandBuffer(command);command=nullptr;check(submitted,SDL_GetError());
        check(SDL_WaitForGPUIdle(device),SDL_GetError());
        auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,read,false));check(mapped,SDL_GetError());
        std::vector<unsigned char> pixels(mapped,mapped+width*height*4);SDL_UnmapGPUTransferBuffer(device,read);
        if(format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
            for(unsigned i=0;i<pixels.size();i+=4) std::swap(pixels[i],pixels[i+2]);
        return pixels;
    }
};
std::vector<vr::SceneVertex> quad(float x,float y,float z,float radius,std::array<float,4> color) {
    std::vector<vr::SceneVertex> result(6);
    constexpr int corners[][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
    for(unsigned i=0;i<6;++i) {
        auto& v=result[i];v.position[0]=x+corners[i][0]*radius;v.position[1]=y+corners[i][1]*radius;v.position[2]=z;
        std::copy(color.begin(),color.end(),v.color);std::copy(color.begin(),color.end(),v.odd_color);
    }
    return result;
}
std::vector<unsigned char> render(Gpu& gpu,render::GpuCalibratedScene& scene,std::span<const render::CalibratedSceneDraw> draws,
    const vr::EyeCamera& camera,std::array<float,4> clear={0,0,0,1}) {
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    const bool uploaded=scene.upload(gpu.command,draws);check(uploaded,scene.status().c_str());
    const bool encoded=scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,clear);check(encoded,scene.status().c_str());
    return gpu.finish();
}
// Independent analytic reference from the XR pose/FOV, not the renderer's
// matrix composer, fixed focal length or SBS eye-offset helper.
std::array<double,2> reference(const XrView& eye,std::array<double,3> world) {
    const auto& q=eye.pose.orientation;const double x=q.x,y=q.y,z=q.z,w=q.w;
    const double dx=world[0]-eye.pose.position.x,dy=world[1]-eye.pose.position.y,dz=world[2]-eye.pose.position.z;
    const double ex=(1-2*(y*y+z*z))*dx+2*(x*y+z*w)*dy+2*(x*z-y*w)*dz;
    const double ey=2*(x*y-z*w)*dx+(1-2*(x*x+z*z))*dy+2*(y*z+x*w)*dz;
    const double ez=2*(x*z+y*w)*dx+2*(y*z-x*w)*dy+(1-2*(x*x+y*y))*dz;
    const double l=std::tan(eye.fov.angleLeft),r=std::tan(eye.fov.angleRight),d=std::tan(eye.fov.angleDown),u=std::tan(eye.fov.angleUp);
    return {(ex/-ez-l)/(r-l)*width,(u-ey/-ez)/(u-d)*height};
}
vr::EyeCamera camera_for(XrView eye,bool) {
    auto camera=vr::eye_camera(eye,1,.05F,20.F);check(bool(camera),"Reference XR camera invalid");
    // SDL's graphics API always uses upward NDC: its Vulkan backend applies
    // a negative viewport height, unlike the direct headset Vulkan renderer.
    for(unsigned col=0;col<4;++col) camera->projection[col*4+1]*=-1;
    return *camera;
}
bool colored_at(const std::vector<unsigned char>& pixels,std::array<double,2> centre,unsigned channel) {
    const int x=int(std::floor(centre[0])),y=int(std::floor(centre[1]));
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        if(x+dx<0 || x+dx>=int(width) || y+dy<0 || y+dy>=int(height)) return false;
        const auto* p=&pixels[(unsigned(y+dy)*width+unsigned(x+dx))*4];
        for(unsigned c=0;c<3;++c) if(p[c]!=(c==channel?255:0)) return false;
    }
    return true;
}
std::array<double,3> world_eye(const XrView& eye,std::array<double,3> world) {
    const auto& q=eye.pose.orientation;const double x=q.x,y=q.y,z=q.z,w=q.w;
    const double dx=world[0]-eye.pose.position.x,dy=world[1]-eye.pose.position.y,dz=world[2]-eye.pose.position.z;
    return {(1-2*(y*y+z*z))*dx+2*(x*y+z*w)*dy+2*(x*z-y*w)*dz,
        2*(x*y-z*w)*dx+(1-2*(x*x+z*z))*dy+2*(y*z+x*w)*dz,
        2*(x*z+y*w)*dx+2*(y*z-x*w)*dy+(1-2*(x*x+y*y))*dz};
}
std::array<double,3> model_point(const vr::Matrix4& matrix,const vr::SceneVertex& vertex) {
    std::array<double,3> result;
    for(unsigned r=0;r<3;++r) result[r]=double(matrix[r])*vertex.position[0]
        +double(matrix[4+r])*vertex.position[1]+double(matrix[8+r])*vertex.position[2]+matrix[12+r];
    return result;
}
// Independent cartridge-word oracle, including negative floor division and
// 16-bit wrap. The fixture must not merely compare the two shared shaders.
std::array<double,3> debris_point(const vr::SceneVertex& vertex) {
    const auto word=[](int value){return int(std::bit_cast<std::int16_t>(std::uint16_t(value)));};
    const auto floor_div=[](std::int64_t value,int denominator) {
        return int(value>=0?value/denominator:-((-value+denominator-1)/denominator));
    };
    const std::array rows{vertex.visibility_a,vertex.visibility_b,vertex.visibility_c};
    const auto rotate=[&](const float* value) {
        std::array<double,3> result{};
        for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) {
            if(vertex.group_c[2]!=0) result[r]+=floor_div(
                std::int64_t(std::round(rows[r][c]*32768.))*word(int(std::round(value[c]))),32768);
            else result[r]+=double(rows[r][c])*value[c];
        }
        if(vertex.group_c[2]!=0) for(auto& component:result) component=word(int(component));
        return result;
    };
    auto result=rotate(vertex.position),direction=rotate(vertex.group_b);
    direction[1]=-std::abs(direction[1]);
    const int low=int(std::floor(vertex.group_c[0])),high=int(std::ceil(vertex.group_c[0]));
    const double fraction=vertex.group_c[0]-low;
    for(unsigned c=0;c<3;++c) {
        result[c]+=std::round(vertex.group_a[c]);
        if(vertex.group_c[2]!=0) result[c]=word(int(result[c]));
        // Fractional rendering interpolates the two *integer* offset steps.
        const int normal=int(std::round(direction[c]));
        result[c]+=std::lerp(double(floor_div(normal*low,4)),double(floor_div(normal*high,4)),fraction);
        result[c]*=(c==0?1:-1)/double(vertex.group_c[1]);
    }
    return result;
}
void native_line_ray_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    using Position=std::array<float,4>;
    std::array<XrView,2> views{XrView{XR_TYPE_VIEW},XrView{XR_TYPE_VIEW}};
    for(unsigned e=0;e<2;++e) {
        const float yaw=e?.075F:-.045F,roll=e?-.06F:.04F;
        views[e].pose.orientation={std::sin(yaw/2)*std::sin(roll/2),std::sin(yaw/2)*std::cos(roll/2),
            std::cos(yaw/2)*std::sin(roll/2),std::cos(yaw/2)*std::cos(roll/2)};
        views[e].pose.position={e?.044F:-.033F,.017F,.061F};
        views[e].fov={-.57F+e*.03F,.73F,.61F,-.49F};
    }
    const vr::Matrix4 model{.8F,.2F,.12F,0,-.1F,1.1F,.06F,0,.03F,.02F,.9F,0,-.11F,.02F,-1.1F,1};
    auto plane=quad(0,0,-4,1.5F,{0,0,1,1});
    std::vector<vr::SceneVertex> lines(10);
    const float endpoints[10][3]{{-.35F,.1F,0},{.35F,.1F,.35F},{-.1F,-.35F,0},{-.1F,.35F,.1F},
        {-.01F,.015F,1.3F},{.35F,.05F,0},{.21F,.1F,0},{.21F,.1F,0},
        {-.1F,.1F,2},{.1F,.15F,2}};
    for(unsigned i=0;i<lines.size();++i) {
        auto& v=lines[i];std::copy_n(endpoints[i],3,v.position);
        v.color[0]=v.odd_color[1]=v.color[3]=v.odd_color[3]=1;v.dither_scale=1;
        // Second endpoint differs: the source line uses its first/provoking ink.
        if(i%2) {v.color[0]=0;v.color[2]=1;}
    }
    std::array<render::CalibratedSceneDraw,2> draws{{{plane},{lines,{},model,vr::SceneTopology::lines}}};
    for(auto& draw:draws) draw.ray_caster=true;
    constexpr unsigned count=36,position_bytes=count*16,material_bytes=count/3*64,bytes=position_bytes+material_bytes;
    struct Download {SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer;
        ~Download(){if(buffer) SDL_ReleaseGPUTransferBuffer(device,buffer);}};
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes*2,0};
    Download read{gpu.device,SDL_CreateGPUTransferBuffer(gpu.device,&info)};check(read.buffer,SDL_GetError());
    std::array<render::CalibratedRayGeometryOutput,2> output;
    std::array<std::vector<Position>,2> expected;
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,draws),scene.status().c_str());
    check(scene.upload_cost()[0]==(plane.size()+lines.size())*160,"Lines uploaded CPU-expanded geometry");
    for(unsigned e=0;e<2;++e) {
        output[e]=scene.enqueue_ray_geometry(gpu.command,e,width,height,camera_for(views[e],d3d));
        check(output[e].complete && output[e].vertex_count==count,scene.status().c_str());
        // Independent quaternion/FOV oracle, never using downloaded geometry.
        const double fx=width/(std::tan(views[e].fov.angleRight)-std::tan(views[e].fov.angleLeft));
        const double fy=height/(std::tan(views[e].fov.angleUp)-std::tan(views[e].fov.angleDown));
        const auto native=[&](const vr::Matrix4& m,const vr::SceneVertex& v) {
            auto p=world_eye(views[e],model_point(m,v));return std::array<double,3>{p[0]*256,-p[1]*256,-p[2]*256};};
        for(const auto& v:plane) {const auto p=native(identity,v);expected[e].push_back({float(p[0]),float(p[1]),float(p[2]),1});}
        for(unsigned i=0;i<lines.size();i+=2) {
            auto a=native(model,lines[i]),b=native(model,lines[i+1]);bool shown=std::max(a[2],b[2])>=12.8;
            if(shown) for(unsigned end=0;end<2;++end) {
                auto& clipped=end?b:a;const auto& other=end?a:b;
                if(clipped[2]<12.8) {const double t=(12.8-clipped[2])/(other[2]-clipped[2]);
                    for(unsigned c=0;c<3;++c) clipped[c]=std::lerp(clipped[c],other[c],t);}
            }
            const double dx=shown?fx*(b[0]/b[2]-a[0]/a[2]):0,dy=shown?fy*(b[1]/b[2]-a[1]/a[2]):0;
            const double length=std::hypot(dx,dy);shown=shown && length>1.e-10;
            const double nx=shown?-dy/(length*2*fx):0,ny=shown?dx/(length*2*fy):0;
            std::array<Position,4> corners{};
            for(unsigned corner=0;corner<4;++corner) {
                const auto& p=corner==1 || corner==2?b:a;const double sign=corner>=2?1:-1;
                corners[corner]=shown?Position{float(p[0]+sign*nx*p[2]),float(p[1]+sign*ny*p[2]),float(p[2]),1}:Position{0,0,0,1};
            }
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) expected[e].push_back(corners[corner]);
        }
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output[e].buffer),0,bytes};
        const SDL_GPUTransferBufferLocation to{read.buffer,e*bytes};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    }
    check(output[0].buffer!=output[1].buffer,"Line geometry shares eye allocations");gpu.finish();
    const auto* mapped=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(mapped,SDL_GetError());
    for(unsigned e=0;e<2;++e) {
        const auto* positions=reinterpret_cast<const Position*>(mapped+e*bytes);
        const auto* materials=reinterpret_cast<const render::RayMaterial*>(mapped+e*bytes+position_bytes);
        for(unsigned v=0;v<count;++v) for(unsigned c=0;c<4;++c)
            check(std::abs(positions[v][c]-expected[e][v][c])<.0005,"Line width/pose/near clip differs from independent analytic oracle");
        for(unsigned t=0;t<count/3;++t) check(materials[t].reserved==2 && materials[t].dither==(t<2?0U:1U)
            && materials[t].even==(t<2?0xffff0000U:0xff0000ffU) && materials[t].odd==(t<2?0xffff0000U:0xff00ff00U),
            "Line material lost provoking colour/dither or triangle record order");
    }
    SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
    if(d3d) for(unsigned e=0;e<2;++e) {
        render::shadows::Scene oracle;
        for(unsigned i=0;i<count;i+=3) {
            const auto point=[](const Position& p){return render::shadows::Vec3{p[0],p[1],p[2]};};
            oracle.add({point(expected[e][i]),point(expected[e][i+1]),point(expected[e][i+2]),
                std::uint8_t(i<6?3:1),std::uint8_t(i<6?3:2),true});
        }
        oracle.build();const auto& p=output[e].projection;
        render::shadows::Camera camera{width,height,p[0],p[2],p[3],p[1]};camera.shadow_softness=0;
        const render::shadows::ReceiverPlane ground{{0,150,0},{0,1,0}};
        const render::shadows::Vec3 light{-.8,-1,-.5};
        const render::GpuScene::RayGeometryOutput geometry{output[e].device,output[e].buffer,count,true,
            output[e].materials,output[e].material_offset,output[e].material_bytes};
        const render::shadows::PrimaryRayRange clip{output[e].near_plane,*output[e].far_plane};
        render::shadows::SdlDxrShadows native;
        check(native.render_resident(gpu.device,{},camera,light,ground,&geometry,false,clip),native.status().c_str());
        std::vector<unsigned char> actual,wanted;check(native.readback(actual),native.status().c_str());
        render::shadows::render_mask(oracle,camera,light,ground,wanted,nullptr,true,false,clip);
        check(actual==wanted,"GPU line casters -> real DXR mask differs from independent ribbon oracle");
        render::RayMaterials indexed;for(const auto& t:oracle.triangles()) {
            render::RayMaterial m;m.even=t.reflection_even;m.odd=t.reflection_odd;m.dither=m.even!=m.odd;indexed.triangles.push_back(m);}
        std::array<std::uint32_t,256> palette{};palette[1]=0xff0000ffU;palette[2]=0xff00ff00U;palette[3]=0xffff0000U;
        render::shadows::DxrShadows reference;
        const render::shadows::DxrShadows::ReflectionInput input{&indexed,palette,0xff998877U};
        check(reference.render_resident(oracle,camera,{0,1,0},{},nullptr,nullptr,false,false,&input,false,clip),reference.status().c_str());
        check(reference.readback_resident(wanted),reference.status().c_str());
        check(native.render_reflections(gpu.device,camera,geometry,palette,0xff998877U,0,0,{},0,
            {1,0,0,0,1,0,0,0,1},nullptr,{},0,nullptr,false,clip),native.status().c_str());
        check(native.readback(actual),native.status().c_str());check(actual==wanted,"Native line materials -> real DXR reflections differ from independent oracle");
    }
    // Provoking source visibility suppresses every generated vertex. The
    // other endpoint's ordinary visibility must not revive a hidden line.
    std::vector<vr::SceneVertex> hidden(lines.begin(),lines.begin()+2);
    hidden[0].visibility_enabled=1;
    const float faces[3][3]{{-1,-1,-1},{1,1,-1},{1,-1,-1}};
    std::copy_n(faces[0],3,hidden[0].visibility_a);std::copy_n(faces[1],3,hidden[0].visibility_b);std::copy_n(faces[2],3,hidden[0].visibility_c);
    render::CalibratedSceneDraw draw{hidden,{},identity,vr::SceneTopology::lines};draw.ray_caster=true;
    vr::EyeCamera straight{identity,{1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-.05F,0}};
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());check(scene.upload(gpu.command,std::span(&draw,1)),scene.status().c_str());
    const auto hidden_output=scene.enqueue_ray_geometry(gpu.command,0,width,height,straight);check(hidden_output.complete,scene.status().c_str());
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(hidden_output.buffer),0,6*16};
    const SDL_GPUTransferBufferLocation to{read.buffer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);gpu.finish();
    const auto* zero=static_cast<const Position*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(zero,SDL_GetError());
    for(unsigned i=0;i<6;++i) check(zero[i]==Position{0,0,0,1},"Hidden line remained a ray caster");SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
    std::cout<<"Native line rays: full independent XR/FOV/affine oracle, one-pixel varying-depth ribbons, near clip, zero-length/behind/hidden lines, provoking dither/material order, independent eyes"
        <<(d3d?", real DXR masks/reflections":"")<<" passed.\n";
}
void native_billboard_ray_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    std::array<XrView,2> views{XrView{XR_TYPE_VIEW},XrView{XR_TYPE_VIEW}};
    for(unsigned e=0;e<2;++e) {const float yaw=e?.075F:-.045F,roll=e?-.06F:.04F;
        views[e].pose.orientation={std::sin(yaw/2)*std::sin(roll/2),std::sin(yaw/2)*std::cos(roll/2),std::cos(yaw/2)*std::sin(roll/2),std::cos(yaw/2)*std::cos(roll/2)};
        views[e].pose.position={e?.044F:-.033F,.017F,.061F};views[e].fov={-.57F,.73F,.61F,-.49F};}
    const vr::Matrix4 model{.8F/256,.2F/256,.12F/256,0,-.1F/256,1.1F/256,.06F/256,0,.03F/256,.02F/256,.9F/256,0,-.11F,.02F,-1.1F,1};
    const auto scale=[&](unsigned column) {return std::sqrt(double(model[column*4])*model[column*4]
        +double(model[column*4+1])*model[column*4+1]+double(model[column*4+2])*model[column*4+2]);};
    std::vector<std::uint32_t> texels(258);texels[1]=0xff0000ffU;texels[2]=0xff00ff00U;texels[3]=0xffff0000U;
    texels[256]=0x02010301U;texels[257]=0x01020003U;
    constexpr unsigned bytes=6*16+2*64+258*4;
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes*2,0};
    auto* read=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(read,SDL_GetError());
    for(unsigned aim:{0U,1U}) for(const auto size_depth:{std::array{72.6F,512.F},std::array{6000.F,256.F},std::array{.25F,128.F},std::array{72.F,64.F}}) {
        std::vector<vr::SceneVertex> vertices;
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {vr::SceneVertex v{};
            v.texture[1]=3;v.texture[2]=1;v.texture[3]=0x28000005U;
            v.billboard[0]=corner==1 || corner==2?1:-1;v.billboard[1]=corner<2?1:-1;
            v.uv[0]=v.billboard[0]>0?4:0;v.uv[1]=v.billboard[1]<0?2:0;
            v.group_a[0]=size_depth[0];v.group_a[1]=size_depth[1];v.group_b[2]=float(aim);vertices.push_back(v);}
        render::CalibratedSceneDraw draw{vertices,texels,model};draw.ray_caster=true;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,std::span(&draw,1)),scene.status().c_str());
        const auto cost=scene.upload_cost();std::array<render::CalibratedRayGeometryOutput,2> output;
        for(unsigned e=0;e<2;++e) {
            output[e]=scene.enqueue_ray_geometry(gpu.command,e,width,height,camera_for(views[e],d3d));
            check(output[e].complete && output[e].vertex_count==6 && output[e].material_bytes==2*64+texels.size()*4,scene.status().c_str());
            auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output[e].buffer),0,bytes};
            const SDL_GPUTransferBufferLocation to{read,e*bytes};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        }
        check(scene.upload_cost()==cost,"Indexed billboard repacked source texture/geometry on CPU");gpu.finish();
        const auto* data=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read,false));check(data,SDL_GetError());
        const double dimension=size_depth[1]>=128?std::min(240.,std::trunc(double(size_depth[0])*256/size_depth[1])):0;
        const double side=dimension*size_depth[1]/512;
        for(unsigned e=0;e<2;++e) {
            const auto* positions=reinterpret_cast<const std::array<float,4>*>(data+e*bytes);
            for(unsigned i=0;i<6;++i) {
                auto vertex=vertices[i];
                if(aim) {vertex.position[0]+=float(vertex.billboard[0]*side);vertex.position[1]+=float(vertex.billboard[1]*side);}
                auto expected=world_eye(views[e],model_point(model,vertex));
                if(!aim) {expected[0]+=vertex.billboard[0]*side*scale(0);expected[1]+=vertex.billboard[1]*side*scale(1);}
                for(unsigned c=0;c<3;++c) check(std::abs(positions[i][c]-(dimension>0?expected[c]*(c==0?256:-256):0))<.0005,
                    "Source billboard ray size/truncation/cap/aim basis differs from independent XR oracle");
            }
            const auto* materials=reinterpret_cast<const render::RayMaterial*>(data+e*bytes+96);
            for(unsigned t=0;t<2;++t) check(materials[t].reserved==3 && materials[t].textured==1 && materials[t].dither==(aim?0x20000001U:0x20000005U)
                && materials[t].offset==128 && materials[t].u_mask==3 && materials[t].v_mask==1,"Indexed billboard material lost flags/offset/palette ABI");
            check(std::memcmp(data+e*bytes+96+128,texels.data(),texels.size()*4)==0,"Indexed billboard changed packed indices or retained palette during GPU copy");
        }
        SDL_UnmapGPUTransferBuffer(gpu.device,read);
    }
    auto bad_vertices=quad(0,0,-1,.1F,{1,0,0,1});
    for(auto& v:bad_vertices) {v.texture[1]=3;v.texture[2]=1;v.texture[3]=0x20000001U;}
    texels[2]=(texels[2]&0xffffffU)|(127U<<24);
    render::CalibratedSceneDraw bad{bad_vertices,texels};bad.ray_caster=true;
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,std::span(&bad,1)),scene.status().c_str());
    const auto fractional=scene.enqueue_ray_geometry(gpu.command,0,width,height,camera_for(views[0],d3d));
    check(!fractional.complete && !fractional.buffer && !fractional.vertex_count,"Fractional indexed palette became an opaque/partial ray caster");
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    SDL_ReleaseGPUTransferBuffer(gpu.device,read);
    std::cout<<"Native indexed billboard rays: independent tracked poses, source truncation/240px cap/hidden sizes, EX aiming basis/wrap, original packed palette/index copy and zero extra CPU uploads passed.\n";
}
void calibrated_ray_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    using Position=std::array<float,4>;
    std::array<XrView,2> eyes{XrView{XR_TYPE_VIEW},XrView{XR_TYPE_VIEW}};
    for(unsigned e=0;e<2;++e) {
        const float yaw=e?.075F:-.045F,roll=e?-.06F:.04F;
        auto& q=eyes[e].pose.orientation;
        q={std::sin(yaw/2)*std::sin(roll/2),std::sin(yaw/2)*std::cos(roll/2),
            std::cos(yaw/2)*std::sin(roll/2),std::cos(yaw/2)*std::cos(roll/2)};
        eyes[e].pose.position={e?.044F:-.033F,.017F,.061F};
        eyes[e].fov={-.57F+e*.03F,.73F,.61F,-.49F};
    }
    std::array cameras{camera_for(eyes[0],d3d),camera_for(eyes[1],d3d)};
    auto first=quad(0,0,0,.13F,{1,0,0,1}),second=quad(0,0,0,.17F,{0,1,0,1});
    // Full affine transforms, not a shared central view with eye-X offsets.
    const vr::Matrix4 model0{.8F,.2F,.12F,0,-.1F,1.1F,.06F,0,.03F,.02F,.9F,0,-.11F,.02F,-1.1F,1};
    const vr::Matrix4 model1{1.1F,-.12F,.03F,0,.1F,.7F,-.08F,0,.02F,.03F,1.2F,0,.28F,.12F,-1.7F,1};
    auto ui=quad(-.1F,.1F,-.7F,.02F,{0,0,1,1});
    std::array<render::CalibratedSceneDraw,3> draws{{{first,{},model0},{ui},{second,{},model1}}};
    draws[0].ray_caster=draws[2].ray_caster=true;
    constexpr unsigned count=12,bytes=count*16;
    struct Download {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer;
        ~Download(){if(buffer) SDL_ReleaseGPUTransferBuffer(device,buffer);}
    };
    constexpr unsigned material_bytes=count/3*sizeof(render::RayMaterial);
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,(bytes+material_bytes)*2,0};
    Download read{gpu.device,SDL_CreateGPUTransferBuffer(gpu.device,&info)};check(read.buffer,SDL_GetError());
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,draws),scene.status().c_str());
    const auto upload_token=scene.upload_token();
    check(scene.upload_cost()[0]==18*sizeof(vr::SceneVertex),"Ray generation uploaded transformed CPU vertices");
    std::array<render::CalibratedRayGeometryOutput,2> rays;
    for(unsigned e=0;e<2;++e) {
        rays[e]=scene.enqueue_ray_geometry(gpu.command,e,width,height,cameras[e]);
        check(rays[e].complete && rays[e].vertex_count==count,scene.status().c_str());
        check(rays[e].material_offset==bytes && rays[e].materials && rays[e].materials->triangles.empty()
            && rays[e].materials->encoding==render::RayMaterialEncoding::native_rgba,
            "Native calibrated materials were missing or CPU packed");
        const auto& p=rays[e].projection;const auto& f=eyes[e].fov;
        const double l=std::tan(f.angleLeft),r=std::tan(f.angleRight),u=std::tan(f.angleUp),d=std::tan(f.angleDown);
        check(std::abs(p[0]-width/(r-l))<.0001 && std::abs(p[1]-height/(u-d))<.0001
            && std::abs(p[2]+width*l/(r-l))<.0001 && std::abs(p[3]-height*u/(u-d))<.0001,
            "Calibrated ray pinhole did not match asymmetric runtime FOV");
        check(std::abs(rays[e].near_plane-.05*256)<.00001 && rays[e].far_plane
            && std::abs(*rays[e].far_plane-20*256)<.2,"Calibrated clip distances lost units");
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,cameras[e]),scene.status().c_str());
    }
    check(rays[0].buffer!=rays[1].buffer,"Calibrated eyes shared one ray geometry allocation");
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    for(unsigned e=0;e<2;++e) {
        const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(rays[e].buffer),0,bytes};
        const SDL_GPUTransferBufferLocation to{read.buffer,e*bytes};SDL_DownloadFromGPUBuffer(copy,&from,&to);
        const SDL_GPUBufferRegion material_from{static_cast<SDL_GPUBuffer*>(rays[e].buffer),rays[e].material_offset,material_bytes};
        const SDL_GPUTransferBufferLocation material_to{read.buffer,bytes*2+e*material_bytes};SDL_DownloadFromGPUBuffer(copy,&material_from,&material_to);
    }
    SDL_EndGPUCopyPass(copy);const auto pixels=gpu.finish();check(scene.notify_submitted(upload_token),"Ray source submission token lost");
    const auto* positions=static_cast<const Position*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(positions,SDL_GetError());
    const auto* native_materials=reinterpret_cast<const render::RayMaterial*>(reinterpret_cast<const unsigned char*>(positions)+bytes*2);
    for(unsigned e=0;e<2;++e) for(unsigned triangle=0;triangle<count/3;++triangle) {
        const auto& material=native_materials[e*(count/3)+triangle];
        check(material.reserved==2 && !material.textured && !material.dither
            && material.even==(triangle<2?0xff0000ffU:0xff00ff00U) && material.odd==material.even,
            "GPU native material kind/colour/order diverged from retained source geometry");
    }
    std::array<std::vector<Position>,2> expected;
    for(unsigned e=0;e<2;++e) for(unsigned draw_index:{0U,2U}) for(const auto& vertex:draws[draw_index].vertices) {
        const auto world=model_point(draws[draw_index].model,vertex);const auto eye=world_eye(eyes[e],world);
        Position want{float(eye[0]*256),float(-eye[1]*256),float(-eye[2]*256),1};
        const auto& got=positions[e*count+expected[e].size()];
        for(unsigned c=0;c<4;++c) check(std::abs(got[c]-want[c])<.0002,"GPU calibrated ray transform disagrees with analytic pose/model oracle");
        expected[e].push_back(want);
        const auto projected=reference(eyes[e],world);
        const auto& p=rays[e].projection;
        check(std::abs(p[0]*got[0]/got[2]+p[2]-projected[0])<.0001
            && std::abs(p[1]*got[1]/got[2]+p[3]-projected[1])<.0001,"Ray/raster projection diverged");
    }
    SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
    check(colored_at(pixels,reference(eyes[1],{model0[12],model0[13],model0[14]}),0),"Ray fixture did not rasterize the calibrated source model");
    if(d3d) {
        // Actual shared SDL -> DXR geometry transport. The oracle starts with
        // independent XR/model math above, never downloaded GPU positions.
        render::shadows::SdlDxrShadows hardware[2];std::array<std::vector<unsigned char>,2> masks;
        const render::shadows::Vec3 light{-.8,-1,-.5};
        const render::shadows::ReceiverPlane ground{{0,100,0},{0,1,0}};
        for(unsigned e=0;e<2;++e) {
            render::shadows::Scene oracle;
            const auto point=[](const Position& p){return render::shadows::Vec3{p[0],p[1],p[2]};};
            for(unsigned i=0;i<count;i+=3) oracle.add({point(expected[e][i]),point(expected[e][i+1]),point(expected[e][i+2])});
            oracle.build();
            const auto& p=rays[e].projection;render::shadows::Camera camera{width,height,p[0],p[2],p[3],p[1]};camera.shadow_softness=0;
            std::vector<unsigned char> wanted;render::shadows::render_mask(oracle,camera,light,ground,wanted,nullptr,true);
            const render::GpuScene::RayGeometryOutput geometry{rays[e].device,rays[e].buffer,count,true};
            check(hardware[e].render_resident(gpu.device,{},camera,light,ground,&geometry),hardware[e].status().c_str());
            // Independent CPU-authored indexed colours are an oracle for the
            // GPU-authored native RGBA ABI, not a readback/reupload of it.
            render::RayMaterials indexed;indexed.triangles.resize(count/3);
            for(unsigned i=0;i<count/3;++i) indexed.triangles[i].even=indexed.triangles[i].odd=i<2?1:2;
            std::array<std::uint32_t,256> palette{};palette[1]=0xff0000ffU;palette[2]=0xff00ff00U;
            render::shadows::DxrShadows reference_reflections;
            const render::shadows::DxrShadows::ReflectionInput reflection_input{&indexed,palette,0xff998877U};
            const render::shadows::PrimaryRayRange clip{rays[e].near_plane,*rays[e].far_plane};
            check(reference_reflections.render_resident(oracle,camera,{0,1,0},{},nullptr,nullptr,false,false,&reflection_input,false,clip),
                reference_reflections.status().c_str());
            std::vector<unsigned char> expected_reflection;
            check(reference_reflections.readback_resident(expected_reflection),reference_reflections.status().c_str());
            const render::GpuScene::RayGeometryOutput native_geometry{rays[e].device,rays[e].buffer,count,true,rays[e].materials,rays[e].material_offset,rays[e].material_bytes};
            render::shadows::SdlDxrShadows native_reflections;
            check(native_reflections.render_reflections(gpu.device,camera,native_geometry,palette,0xff998877U,0,0,{},0,
                {1,0,0,0,1,0,0,0,1},nullptr,{},0,nullptr,false,clip),native_reflections.status().c_str());
            std::vector<unsigned char> reflected;check(native_reflections.readback(reflected),native_reflections.status().c_str());
            check(reflected==expected_reflection,"Native GPU RGBA reflection materials differ from independent indexed-colour oracle");
            check(std::count_if(reflected.begin()+3,reflected.end(),[](auto v){return v==255;})>20,
                "Native RGBA reflection fixture produced no visible surfaces");
            check(hardware[e].readback(masks[e]),hardware[e].status().c_str());
            if(masks[e]!=wanted) {
                unsigned different=0;for(unsigned i=0;i<wanted.size();++i) if(masks[e][i]!=wanted[i]) {
                    if(different<8) std::cerr<<"Ray mask eye "<<e<<" at "<<i%width<<','<<i/width
                        <<": GPU="<<unsigned(masks[e][i])<<" CPU="<<unsigned(wanted[i])<<'\n';
                    ++different;
                }
                std::cerr<<"Ray mask differing pixels="<<different<<", GPU shadows="
                    <<std::count_if(masks[e].begin(),masks[e].end(),[](auto v){return v!=0;})
                    <<", CPU shadows="<<std::count_if(wanted.begin(),wanted.end(),[](auto v){return v!=0;})<<'\n';
            }
            check(masks[e]==wanted,"Calibrated GPU geometry -> hardware DXR shadows differ from independent XR oracle");
            check(std::count_if(masks[e].begin(),masks[e].end(),[](auto v){return v!=0;})>20,"Calibrated DXR fixture produced no shadow");
            for(const auto range:{render::shadows::PrimaryRayRange{rays[e].near_plane,*rays[e].far_plane},
                render::shadows::PrimaryRayRange{320,900},render::shadows::PrimaryRayRange{1,160}}) {
                for(bool ground_only:{false,true}) {
                    render::shadows::render_mask(oracle,camera,light,ground,wanted,nullptr,true,ground_only,range);
                    check(hardware[e].render_resident(gpu.device,{},camera,light,ground,&geometry,ground_only,range),hardware[e].status().c_str());
                    std::vector<unsigned char> clipped;check(hardware[e].readback(clipped),hardware[e].status().c_str());
                    check(clipped==wanted,"Calibrated depth planes changed model/ground/shadow-caster coverage");
                    if(range.far_depth==160) check(std::none_of(clipped.begin(),clipped.end(),[](auto v){return v!=0;}),
                        "Calibrated far clip fixture did not exclude the entire primary scene");
                }
            }
            for(const auto range:{render::shadows::PrimaryRayRange{-1,10},render::shadows::PrimaryRayRange{20,10},
                render::shadows::PrimaryRayRange{0,INFINITY},render::shadows::PrimaryRayRange{NAN,10}})
                check(!hardware[e].render_resident(gpu.device,{},camera,light,ground,&geometry,false,range)
                    && !hardware[e].output().buffer,"Invalid primary range retained a stale calibrated shadow");
            // Restore this eye's baseline before checking independent owners.
            check(hardware[e].render_resident(gpu.device,{},camera,light,ground,&geometry),hardware[e].status().c_str());
        }
        check(masks[0]!=masks[1],"Different runtime poses produced identical ray images");
        std::vector<unsigned char> retained;check(hardware[0].readback(retained) && retained==masks[0],"Other-eye trace overwrote first-eye shadow image");
        // A normalized reflection ray must still obey depth planes, not radial
        // limits. At the image corners this plane is farther than 30 radially,
        // but its Z depth is 20 everywhere and must remain fully visible.
        render::shadows::Scene flat;
        flat.add({{-1000,-1000,20},{1000,-1000,20},{1000,1000,20}});
        flat.add({{-1000,-1000,20},{1000,1000,20},{-1000,1000,20}});
        render::RayMaterials materials;materials.triangles.resize(2);
        for(auto& material:materials.triangles) material.even=material.odd=1;
        std::array<std::uint32_t,256> palette{};palette.fill(0xffffffffU);
        render::shadows::DxrShadows reflections;
        const render::shadows::DxrShadows::ReflectionInput input{&materials,palette,0xff998877U};
        const render::shadows::Camera wide{13,9,3,6.5,4.5,2};
        for(const auto range:{render::shadows::PrimaryRayRange{10,30},render::shadows::PrimaryRayRange{21,30},
            render::shadows::PrimaryRayRange{1,19}}) {
            check(reflections.render_resident(flat,wide,{0,1,0},{},nullptr,nullptr,false,false,&input,false,range),reflections.status().c_str());
            std::vector<unsigned char> rgba;check(reflections.readback_resident(rgba),reflections.status().c_str());
            for(unsigned i=0;i<13*9;++i) {
                const unsigned char expected_pixel[4]{0x77,0x88,0x99,0xff};
                for(unsigned c=0;c<4;++c) check(rgba[i*4+c]==(range.near_depth==10?expected_pixel[c]:0),
                    "Reflection primary range used radial distances or ignored near/far depth planes");
            }
        }
    }
    // Entire selected batch must decline before emitting any partial casters.
    // Excluded HUD/specialized scenery is legal; unsupported selected casters
    // must not degrade into ordinary solid triangles.
    for(unsigned fault=0;fault<7;++fault) {
        auto invalid=draws;auto changed=first;
        if(fault==0) {for(auto& v:changed) v.texture[3]=4;invalid[0].vertices=changed;}
        if(fault==1) invalid[0].blend=vr::SceneBlend::alpha;
        if(fault==2) invalid[0].depth_test=false;
        if(fault==3) invalid[0].camera_override=cameras[0];
        if(fault==4) {changed.resize(6);for(auto& v:changed) v.color[3]=.5F;invalid[0].vertices=changed;}
        if(fault==5) invalid[0].ray_caster=invalid[2].ray_caster=false;
        if(fault==6) {invalid[0].topology=vr::SceneTopology::lines;for(auto& v:changed) v.texture[3]=32768;invalid[0].vertices=changed;}
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,invalid),scene.status().c_str());
        const auto bad=scene.enqueue_ray_geometry(gpu.command,0,width,height,cameras[0]);
        if(fault==5) {
            // Fog's empty-world contract: all casters explicitly deselected is
            // a complete empty batch, not unsupported geometry. It must still
            // publish the actual eye projection and no stale resident buffer.
            check(bad.complete && bad.device==gpu.device && !bad.buffer && !bad.vertex_count
                && bad.width==width && bad.height==height && bad.projection[0]>0 && bad.projection[1]>0,
                "Empty world failed to publish calibrated projection without stale casters");
        } else check(!bad.complete && !bad.buffer && !bad.vertex_count,"Unsupported batch published partial/stale calibrated casters");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(!scene.enqueue_ray_geometry(gpu.command,0,width,height,cameras[0]).complete,"Ray command reused an earlier source upload");
    check(scene.upload(gpu.command,draws),scene.status().c_str());
    auto bad_camera=cameras[0];bad_camera.projection[1]=.01F;
    check(!scene.enqueue_ray_geometry(gpu.command,0,width,height,bad_camera).complete,"Ray path discarded a projective matrix term");
    check(!scene.enqueue_ray_geometry(gpu.command,2,width,height,cameras[0]).complete,"Third eye accepted");
    auto infinite=cameras[0];infinite.projection[10]=-1;infinite.projection[14]=-.05F;
    const auto recovered=scene.enqueue_ray_geometry(gpu.command,0,width,height,infinite);
    check(recovered.complete && !recovered.far_plane,"Calibrated ray failure did not recover/infinite far lost");
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    // Source visibility is shared with rasterization: reversed faces become
    // degenerate ray triangles, not invisible geometry casting phantom shadows.
    auto hidden=first;
    for(auto& v:hidden) {
        v.visibility_enabled=1;
        std::copy(std::begin(first[0].position),std::end(first[0].position),v.visibility_a);
        std::copy(std::begin(first[2].position),std::end(first[2].position),v.visibility_b);
        std::copy(std::begin(first[1].position),std::end(first[1].position),v.visibility_c);
    }
    render::CalibratedSceneDraw invisible{hidden,{},model0};invisible.ray_caster=true;
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,std::span(&invisible,1)),scene.status().c_str());
    const auto suppressed=scene.enqueue_ray_geometry(gpu.command,0,width,height,cameras[0]);check(suppressed.complete,scene.status().c_str());
    copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(suppressed.buffer),0,6*16};
    const SDL_GPUTransferBufferLocation to{read.buffer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,cameras[0]),scene.status().c_str());
    const auto hidden_pixels=gpu.finish();
    positions=static_cast<const Position*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(positions,SDL_GetError());
    for(unsigned i=0;i<6;++i) check(positions[i]==Position{0,0,0,1},"Invisible source face remained a ray caster");
    SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
    check(std::count(hidden_pixels.begin(),hidden_pixels.end(),255)==width*height,"Hidden ray/raster face coverage diverged");
    // Signed source words, Q15 per-product floor, wrapped translations and
    // fractional explosion phase all occur before the full tracked eye view.
    // Exercise both exact cartridge and continuous destruction conventions.
    auto debris=first;
    const float source[6][3]{{-10,-10,0},{10,-10,0},{10,10,0},
        {32760,-32760,17},{-32760,32760,-19},{32769,-32769,23}};
    for(unsigned exact=0;exact<2;++exact) for(float phase:{0.F,8.5F,37.25F}) {
        for(unsigned i=0;i<debris.size();++i) {
            auto& v=debris[i];std::copy_n(source[i],3,v.position);v.visibility_enabled=2;
            const float rotation[3][3]{{.75F,.125F,-.25F},{-.25F,.75F,.125F},{.125F,-.25F,.75F}};
            std::copy_n(rotation[0],3,v.visibility_a);std::copy_n(rotation[1],3,v.visibility_b);std::copy_n(rotation[2],3,v.visibility_c);
            const float translation[3]{32760,-32760,512},normal[3]{-13,17,-21};
            std::copy_n(translation,3,v.group_a);std::copy_n(normal,3,v.group_b);
            v.group_c[0]=phase;v.group_c[1]=256;v.group_c[2]=float(exact);
        }
        render::CalibratedSceneDraw shattered{debris,{},model0};shattered.ray_caster=true;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,std::span(&shattered,1)),scene.status().c_str());
        const auto generated=scene.enqueue_ray_geometry(gpu.command,1,width,height,cameras[1]);
        check(generated.complete && generated.vertex_count==6,scene.status().c_str());
        copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        const SDL_GPUBufferRegion from_debris{static_cast<SDL_GPUBuffer*>(generated.buffer),0,6*16};
        SDL_DownloadFromGPUBuffer(copy,&from_debris,&to);SDL_EndGPUCopyPass(copy);
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,cameras[1]),scene.status().c_str());
        gpu.finish();positions=static_cast<const Position*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(positions,SDL_GetError());
        for(unsigned i=0;i<debris.size();++i) {
            const auto source_position=debris_point(debris[i]);std::array<double,3> world{};
            for(unsigned r=0;r<3;++r) {
                world[r]=model0[12+r];
                for(unsigned c=0;c<3;++c) world[r]+=double(model0[c*4+r])*source_position[c];
            }
            const auto eye=world_eye(eyes[1],world);
            for(unsigned c=0;c<3;++c) check(std::abs(positions[i][c]-eye[c]*(c==0?256:-256))<.015,
                "Ray destruction differs from independent signed-word/phase/XR oracle");
            check(positions[i][3]==1,"Invalid ray destruction homogeneous coordinate");
        }
        SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
    }
    if(d3d) {
        // A tilted mirror sends primary rays into a separately coloured,
        // offscreen wall. The expected pixel comes from that wall, not the
        // environment or the primary mirror's own colour.
        const float triangles[6][3]{{-2,2,-3},{2,2,-7},{0,-2,-5},{5,3,-2},{5,-3,-2},{5,0,-8}};
        std::vector<vr::SceneVertex> mirror(3),wall(3);
        const bool srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        const auto channel=[&](float code) {const float encoded=code/255;
            return !srgb?encoded:encoded<=.04045F?encoded/12.92F:std::pow((encoded+.055F)/1.055F,2.4F);};
        for(unsigned i=0;i<3;++i) {
            std::copy_n(triangles[i],3,mirror[i].position);std::fill_n(mirror[i].color,4,1);
            std::copy_n(triangles[i+3],3,wall[i].position);
            const float even[4]{channel(17),channel(34),channel(51),1},odd[4]{channel(85),channel(102),channel(119),1};
            std::copy_n(even,4,wall[i].color);std::copy_n(odd,4,wall[i].odd_color);wall[i].dither_scale=2;
        }
        std::array<render::CalibratedSceneDraw,2> native{{{mirror},{wall}}};
        native[0].ray_caster=native[1].ray_caster=true;native[0].preserve_native_colour=true;
        XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;
        view.fov={-std::atan(.02F),std::atan(.02F),std::atan(.005F),-std::atan(.005F)};
        const auto camera=camera_for(view,true);
        render::shadows::SdlDxrShadows reflected;
        std::array<std::uint32_t,256> unused_palette{};
        unsigned checked=0;
        using render::Effect;
        constexpr Effect styles[]{Effect::off,Effect::monochrome,Effect::sepia,Effect::thermal,Effect::pastel,
            Effect::posterized,Effect::ice,Effect::film,Effect::negative,Effect::solarized,Effect::amber,
            Effect::emerald,Effect::cyanotype,Effect::copper,Effect::lavender,Effect::cga,
            Effect::teal_orange,Effect::handheld,Effect::bleach_bypass,Effect::risograph,
            Effect::duotone,Effect::tritone,Effect::iridescent,Effect::noir,Effect::uv_glow,Effect::topographic};
        for(auto style:styles) for(unsigned intensity:{0U,1U,37U,100U,255U}) for(bool preserve:{false,true}) {
            native[1].effects_override=std::array{unsigned(style),intensity,0U,0U};native[1].preserve_native_colour=preserve;
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            check(scene.upload(gpu.command,native),scene.status().c_str());
            const auto output=scene.enqueue_ray_geometry(gpu.command,0,4,1,camera,1);
            check(output.complete && output.materials && output.material_offset==6*16,scene.status().c_str());
            const bool submitted=SDL_SubmitGPUCommandBuffer(gpu.command);gpu.command=nullptr;check(submitted,SDL_GetError());
            const auto& p=output.projection;const render::shadows::Camera projection{4,1,p[0],p[2],p[3],p[1]};
            const render::GpuScene::RayGeometryOutput geometry{output.device,output.buffer,output.vertex_count,true,output.materials,output.material_offset,output.material_bytes};
            const render::shadows::PrimaryRayRange clip{output.near_plane,*output.far_plane};
            check(reflected.render_reflections(gpu.device,projection,geometry,unused_palette,0xff998877U,0,0,{},0,
                {1,0,0,0,1,0,0,0,1},nullptr,{},0,nullptr,false,clip),reflected.status().c_str());
            std::vector<unsigned char> rgba;check(reflected.readback(rgba) && rgba.size()==16,reflected.status().c_str());
            render::Framebuffer pixel(1,1);pixel.enable_layer_tags(true);std::vector<unsigned char> scratch;
            for(unsigned x=0;x<4;++x) {
                std::vector<unsigned char> expected=x<2?std::vector<unsigned char>{17,34,51,255}:std::vector<unsigned char>{85,102,119,255};
                render::apply_effect(preserve?render::Effect::off:style,pixel,expected,scratch,static_cast<unsigned char>(intensity));
                for(unsigned c=0;c<4;++c) if(std::abs(int(rgba[x*4+c])-int(expected[c]))>1)
                    throw std::runtime_error("Native reflected wall mismatch: style="+std::to_string(unsigned(style))
                        +" intensity="+std::to_string(intensity)+" preserve="+std::to_string(preserve)
                        +" x="+std::to_string(x)+" channel="+std::to_string(c)
                        +" actual="+std::to_string(rgba[x*4+c])+" expected="+std::to_string(expected[c]));
                ++checked;
            }
            if(style==Effect::off && intensity==0 && !preserve) {
                auto malformed=geometry;malformed.material_offset=0;
                check(!reflected.render_reflections(gpu.device,projection,malformed,unused_palette,0xff998877U)
                    && !reflected.reflection_output().buffer,"CPU/native material ABI mismatch retained a stale reflection");
                auto contaminated=*output.materials;contaminated.texels.push_back(1);malformed=geometry;malformed.materials=&contaminated;
                check(!reflected.render_reflections(gpu.device,projection,malformed,unused_palette,0xff998877U)
                    && !reflected.reflection_output().buffer,"Native solid ABI silently accepted indexed texture metadata");
                render::shadows::VulkanHardwareRt unsupported;
                check(!unsupported.render_reflections(gpu.device,geometry,projection,unused_palette,0xff998877U,2,0,0,{})
                    && !unsupported.reflection_output().buffer && unsupported.status().find("material ABI")!=std::string::npos,
                    "Non-native ray producer interpreted RGBA records as palette indices or probed an unsupported device");
            }
        }
        std::cout<<"Native RGBA DXR reflections: "<<checked<<" offscreen-hit pixels, 25 palette styles + OFF, layer overrides, protected colours, sRGB="<<srgb<<" and 2-pixel dither passed.\n";
    }
    std::cout<<"Calibrated ray geometry: two full XR poses/FOVs, affine models, independent eye storage, explicit caster selection, source visibility, complete-batch rejection and clip/projection validation passed"
        <<"; signed-word/Q15/fractional destruction verified independently"
        <<(d3d?"; both GPU-resident DXR eye masks/reflections match independent CPU oracles":"")<<".\n";
}
void native_texture_ray_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    const float positions[6][3]{{-2,2,-3},{2,2,-7},{0,-2,-5},{5,3,-2},{5,-3,-2},{5,0,-8}};
    std::vector<vr::SceneVertex> mirror(3),wall(3);
    for(unsigned i=0;i<3;++i) {
        std::copy_n(positions[i],3,mirror[i].position);std::fill_n(mirror[i].color,4,1);
        std::copy_n(positions[i+3],3,wall[i].position);std::fill_n(wall[i].color,4,1);
    }
    std::array<std::uint32_t,5> texels{0xff010203U,0xffcb4311U,0,0xff5b2ab4U,0};
    std::array<render::CalibratedSceneDraw,2> draws{{{mirror},{wall,texels}}};
    draws[0].ray_caster=draws[1].ray_caster=true;draws[0].preserve_native_colour=true;
    XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;
    view.fov={-std::atan(.02F),std::atan(.02F),std::atan(.005F),-std::atan(.005F)};
    const auto camera=camera_for(view,true);
    constexpr unsigned bytes=6*16+2*64+5*4;
    struct Download {SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer;
        ~Download(){if(buffer) SDL_ReleaseGPUTransferBuffer(device,buffer);}};
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes*2,0};
    Download read{gpu.device,SDL_CreateGPUTransferBuffer(gpu.device,&info)};check(read.buffer,SDL_GetError());
    for(auto& v:wall) {v.uv[0]=.25F;v.uv[1]=1.25F;std::fill_n(v.texture,4,1);}
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,draws),scene.status().c_str());const auto cost=scene.upload_cost();
    std::array<render::CalibratedRayGeometryOutput,2> outputs;
    for(unsigned eye=0;eye<2;++eye) outputs[eye]=scene.enqueue_ray_geometry(gpu.command,eye,4,1,camera,1);
    check(outputs[0].complete && outputs[1].complete && outputs[0].buffer!=outputs[1].buffer,scene.status().c_str());
    check(scene.upload_cost()==cost,"Native ray textures caused an additional CPU upload");
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    for(unsigned eye=0;eye<2;++eye) {
        check(outputs[eye].material_offset==96 && outputs[eye].material_bytes==148,"Native texture extent metadata lost");
        const SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(outputs[eye].buffer),0,bytes};
        const SDL_GPUTransferBufferLocation dest{read.buffer,eye*bytes};SDL_DownloadFromGPUBuffer(copy,&source,&dest);
    }
    SDL_EndGPUCopyPass(copy);gpu.finish();
    const auto* data=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(data,SDL_GetError());
    for(unsigned eye=0;eye<2;++eye) {
        render::RayMaterial records[2];std::memcpy(records,data+eye*bytes+96,sizeof(records));
        check(records[0].reserved==2 && records[1].reserved==3 && records[1].textured==1
            && records[1].offset==132 && records[1].u_mask==1 && records[1].v_mask==1,
            "Native textured record order/offset/flags differ from raster source");
        for(unsigned corner=0;corner<3;++corner) check(records[1].uv[corner*2]==.25F && records[1].uv[corner*2+1]==1.25F,
            "Native textured UVs were dropped or transformed on CPU");
        check(std::memcmp(data+eye*bytes+96+128,texels.data(),texels.size()*4)==0,"Native texture GPU copy changed RGBA/alpha words");
    }
    SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
    render::shadows::SdlDxrShadows traced;
    std::array<std::uint32_t,256> unused_palette{};
    const bool target_srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    const auto decode=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    const auto encode=[](double c){return c<=.0031308?c*12.92:1.055*std::pow(c,1/2.4)-.055;};
    using render::Effect;
    constexpr Effect styles[]{Effect::off,Effect::monochrome,Effect::sepia,Effect::thermal,Effect::pastel,
        Effect::posterized,Effect::ice,Effect::film,Effect::negative,Effect::solarized,Effect::amber,
        Effect::emerald,Effect::cyanotype,Effect::copper,Effect::lavender,Effect::cga,
        Effect::teal_orange,Effect::handheld,Effect::bleach_bypass,Effect::risograph,
        Effect::duotone,Effect::tritone,Effect::iridescent,Effect::noir,Effect::uv_glow,Effect::topographic};
    unsigned checked=0;
    std::vector<std::uint32_t> indexed_texels(258);for(unsigned i=0;i<4;++i) indexed_texels[2+i]=texels[1+i];
    indexed_texels[257]=0x04030201U; // Palette starts at word 1; source index bytes are not alpha.
    if(d3d) for(bool indexed_source:{false,true}) for(auto style:styles) for(bool source_srgb:{false,true}) for(unsigned sample=0;sample<6;++sample) {
        const float u[]{.25F,1.25F,-.75F,2.25F,.25F,-.75F},v[]{.25F,.25F,.25F,.25F,1.25F,.25F};
        const bool clamp=sample==5,preserve=sample==4;
        const unsigned intensity=sample==0?37U:100U;
        for(auto& vertex:wall) {vertex.uv[0]=u[sample];vertex.uv[1]=v[sample];std::fill_n(vertex.texture,4,1);
            vertex.texture[3]=1U|(source_srgb?2U:0U)|(clamp?4U:0U)|(indexed_source?536870912U:0U);}
        draws[1].texels=indexed_source?std::span<const std::uint32_t>(indexed_texels):std::span<const std::uint32_t>(texels);
        draws[1].effects_override=std::array{unsigned(style),intensity,0U,0U};draws[1].preserve_native_colour=preserve;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto source_cost=scene.upload_cost();
        const auto output=scene.enqueue_ray_geometry(gpu.command,0,4,1,camera,1);check(output.complete,scene.status().c_str());
        check(scene.upload_cost()==source_cost,"Textured reflection repacked/uploaded source pixels on CPU");
        const bool submitted=SDL_SubmitGPUCommandBuffer(gpu.command);gpu.command=nullptr;check(submitted,SDL_GetError());
        const auto& p=output.projection;const render::shadows::Camera projection{4,1,p[0],p[2],p[3],p[1]};
        const render::GpuScene::RayGeometryOutput geometry{output.device,output.buffer,output.vertex_count,true,output.materials,output.material_offset,output.material_bytes};
        check(traced.render_reflections(gpu.device,projection,geometry,unused_palette,0xff998877U),traced.status().c_str());
        std::vector<unsigned char> rgba;check(traced.readback(rgba) && rgba.size()==16,traced.status().c_str());
        const unsigned x=unsigned(int(std::floor(u[sample])))&1,y=unsigned(int(std::floor(v[sample])))&1;
        const auto packed=clamp?0U:texels[1+y*2+x];
        std::vector<unsigned char> expected{119,136,153,255},scratch;
        if(packed>>24) {
            for(unsigned channel=0;channel<3;++channel) {
                double value=((packed>>(channel*8))&255)/255.;if(source_srgb) value=decode(value);if(target_srgb) value=encode(value);
                expected[channel]=static_cast<unsigned char>(std::lround(std::clamp(value,0.,1.)*255));
            }
            render::Framebuffer pixel(1,1);pixel.enable_layer_tags(true);
            render::apply_effect(preserve?Effect::off:style,pixel,expected,scratch,static_cast<unsigned char>(intensity));
        }
        for(unsigned pixel=0;pixel<4;++pixel) for(unsigned channel=0;channel<4;++channel)
            if(std::abs(int(rgba[pixel*4+channel])-int(expected[channel]))>1)
                throw std::runtime_error("Native texture reflected colour/wrap/cutout/style mismatch: style="+std::to_string(unsigned(style))
                    +" sample="+std::to_string(sample)+" source_srgb="+std::to_string(source_srgb)
                    +" channel="+std::to_string(channel)+" actual="+std::to_string(rgba[pixel*4+channel])+" expected="+std::to_string(expected[channel]));
        checked+=4;
        if(style==Effect::off && !source_srgb && sample==0) for(unsigned fault=0;fault<3;++fault) {
            auto bad=geometry;bad.material_bytes=fault==0?127:fault==1?geometry.material_bytes+1:128+16'000'004;
            check(!traced.render_reflections(gpu.device,projection,bad,unused_palette,0xff998877U)
                && !traced.reflection_output().buffer,"Invalid native texture extent preserved stale reflection output");
        }
    }
    if(d3d) {
        // Unlike the constant-UV boundary tests above, this wall spans many
        // texels. Derive its reflected hit on the x=5 plane analytically,
        // including the mirror's normal offset; never use downloaded positions
        // or the shader's barycentric result to choose an expected texel.
        std::array<std::uint32_t,65> gradient{};
        for(unsigned i=0;i<64;++i) gradient[i+1]=0xff000000U|(20+(i*7)%220)|((31+(i*11)%200)<<8)|((41+(i*13)%190)<<16);
        for(unsigned i=0;i<3;++i) {
            wall[i].uv[0]=i==2?48.F:-16.F;wall[i].uv[1]=2.25F;std::fill_n(wall[i].texture,4,1);
            wall[i].texture[1]=wall[i].texture[2]=7;wall[i].texture[3]=1U|(target_srgb?2U:0U);
        }
        draws[1].texels=gradient;draws[1].effects_override.reset();draws[1].preserve_native_colour=true;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());
        const auto output=scene.enqueue_ray_geometry(gpu.command,0,4,1,camera,1);check(output.complete,scene.status().c_str());
        const bool submitted=SDL_SubmitGPUCommandBuffer(gpu.command);gpu.command=nullptr;check(submitted,SDL_GetError());
        const auto& p=output.projection;const render::shadows::Camera projection{4,1,p[0],p[2],p[3],p[1]};
        const render::GpuScene::RayGeometryOutput geometry{output.device,output.buffer,output.vertex_count,true,output.materials,output.material_offset,output.material_bytes};
        check(traced.render_reflections(gpu.device,projection,geometry,unused_palette,0xff998877U),traced.status().c_str());
        std::vector<unsigned char> rgba;check(traced.readback(rgba) && rgba.size()==16,traced.status().c_str());
        std::array<unsigned,4> hit_texels{};
        for(unsigned pixel=0;pixel<4;++pixel) {
            const double dx=(pixel+.5-p[2])/p[0],depth=5/(1-dx),normal=std::sqrt(.5);
            const double origin_x=dx*depth+normal*.01,origin_z=depth-normal*.01;
            const double hit_z=origin_z+(5-origin_x)*dx;
            const double corner_weight=(hit_z-2)/6,uv=-16+64*corner_weight;
            hit_texels[pixel]=unsigned(int(std::floor(uv)))&7;
            const auto packed=gradient[1+2*8+hit_texels[pixel]];
            for(unsigned channel=0;channel<4;++channel) check(std::abs(int(rgba[pixel*4+channel])-int((packed>>(channel*8))&255))<=1,
                "Native varying-UV reflection selected the wrong texel versus analytic reflected-ray oracle");
        }
        std::sort(hit_texels.begin(),hit_texels.end());
        check(std::adjacent_find(hit_texels.begin(),hit_texels.end())==hit_texels.end(),"Varying-UV reflection fixture did not hit four distinct texels");
        std::cout<<"Native varying-UV reflection: four distinct offscreen texels match independent mirror/plane intersection oracle.\n";
    }
    if(d3d) {
        // Constant UVs make binary texture coverage independently decidable on
        // CPU: omit a cutout triangle entirely. The side caster is offscreen
        // for primary rays but shadows the floor revealed through that hole.
        const float front[3][3]{{-10,-10,-5},{10,-10,-5},{0,10,-5}},side[3][3]{{4,-10,-5},{6,-10,-5},{5,10,-5}};
        for(unsigned i=0;i<3;++i) {
            std::copy_n(front[i],3,mirror[i].position);std::copy_n(side[i],3,wall[i].position);
            std::fill_n(wall[i].texture,4,0);
        }
        draws[0].texels=texels;draws[1].texels={};draws[1].preserve_native_colour=true;
        draws[1].effects_override.reset();
        render::shadows::SdlDxrShadows shadows;
        const render::shadows::ReceiverPlane floor{{0,0,10},{0,0,-1}};
        const render::shadows::PrimaryRayRange clip{.1,20};
        unsigned shadow_pixels=0,lit_pixels=0;
        for(const auto size:{std::array{4U,1U},std::array{9U,3U}}) for(bool ground_only:{false,true}) for(unsigned sample=0;sample<3;++sample) {
            for(auto& vertex:mirror) {
                std::fill_n(vertex.texture,4,1);vertex.texture[3]=sample==2?5:1;
                vertex.uv[0]=sample==0?.25F:sample==1?1.25F:-.75F;vertex.uv[1]=.25F;
            }
            const auto selected=std::span(draws.data(),ground_only?1U:2U);
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            check(scene.upload(gpu.command,selected),scene.status().c_str());
            const auto output=scene.enqueue_ray_geometry(gpu.command,0,size[0],size[1],camera,1);check(output.complete,scene.status().c_str());
            const bool submitted=SDL_SubmitGPUCommandBuffer(gpu.command);gpu.command=nullptr;check(submitted,SDL_GetError());
            const auto& p=output.projection;render::shadows::Camera projection{size[0],size[1],p[0],p[2],p[3],p[1]};projection.shadow_softness=0;
            const render::GpuScene::RayGeometryOutput geometry{output.device,output.buffer,output.vertex_count,true,output.materials,output.material_offset,output.material_bytes};
            render::shadows::Scene oracle;
            const auto point=[](const float* v){return render::shadows::Vec3{v[0],-v[1],-v[2]};};
            if(sample==0) oracle.add({point(front[0]),point(front[1]),point(front[2])});
            if(!ground_only) oracle.add({point(side[0]),point(side[1]),point(side[2])});
            oracle.build();const render::shadows::Vec3 light=ground_only?render::shadows::Vec3{0,0,-1}:render::shadows::Vec3{1,0,-1};
            std::vector<unsigned char> expected,actual;
            render::shadows::render_mask(oracle,projection,light,floor,expected,nullptr,true,ground_only,clip);
            check(shadows.render_resident(gpu.device,{},projection,light,floor,&geometry,ground_only,clip),shadows.status().c_str());
            check(shadows.readback(actual) && actual==expected,"Native textured primary/secondary shadow coverage differs from independent omitted-hole oracle");
            check(shadows.output().buffer && !shadows.reflection_output().buffer,"Native alpha coverage emitted reflection bytes instead of a shadow mask");
            for(auto value:actual) {shadow_pixels+=value==160;lit_pixels+=value==0;}
            auto bad=geometry;bad.material_bytes=0;
            check(!shadows.render_resident(gpu.device,{},projection,light,floor,&bad,ground_only,clip)
                && !shadows.output().buffer,"Invalid native shadow material extent retained stale output");
        }
        check(shadow_pixels && lit_pixels,"Native cutout shadow fixture did not exercise both blocked and unblocked rays");
        std::cout<<"Native textured shadows: primary cutouts reveal offscreen-caster shadows; secondary cutouts/clamp holes, ground-only coverage and stale-output rejection match independent CPU oracle.\n";
    }
    // Fractional texture alpha is not silently turned into an opaque caster.
    texels[1]=(texels[1]&0xffffffU)|(127U<<24);
    for(auto& vertex:wall) std::fill_n(vertex.texture,4,1);
    draws[0].texels={};for(auto& vertex:mirror) std::fill_n(vertex.texture,4,0);draws[1].texels=texels;
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,draws),scene.status().c_str());
    check(!scene.enqueue_ray_geometry(gpu.command,0,4,1,camera,1).complete,"Fractional-alpha texture published partial/opaque native casters");
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    std::cout<<"Native textured ray records: independent eyes, retained RGBA/alpha copy, UVs, source upload reuse, extent rejection and fractional-alpha rejection passed"
        <<(d3d?"; "+std::to_string(checked)+" hardware reflected wrap/clamp/cutout/styled pixels, linear/sRGB source":"")<<".\n";
}
void projection_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    const std::array<std::array<double,3>,3> centres{{{-.16,.11,-.8},{.24,-.12,-1.7},{.05,.21,-3.6}}};
    std::array<std::vector<vr::SceneVertex>,3> geometry;
    std::array<render::CalibratedSceneDraw,3> draws;
    for(unsigned i=0;i<3;++i) {
        std::array<float,4> color{0,0,0,1};color[i]=1;
        geometry[i]=quad(float(centres[i][0]),float(centres[i][1]),float(centres[i][2]),float(.07*(i+1)),color);
        draws[i].vertices=geometry[i];
    }
    for(unsigned test=0;test<12;++test) {
        XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;
        const float theta=float(int(test%4)-1)*.08F;
        eye.pose.orientation.z=std::sin(theta/2);eye.pose.orientation.w=std::cos(theta/2);
        if(test>=8) {eye.pose.orientation.z=0;eye.pose.orientation.y=std::sin(theta/2);eye.pose.orientation.w=std::cos(theta/2);}
        eye.pose.position={float((test&1)? .055:-.035),float(test%3)*.01F,float(test%2)*.08F};
        eye.fov={-.62F+float(test%2)*.04F,.71F,.58F+float(test%3)*.03F,-.52F};
        const auto pixels=render(gpu,scene,draws,camera_for(eye,d3d));
        for(unsigned i=0;i<3;++i) check(colored_at(pixels,reference(eye,centres[i]),i),"Calibrated geometry disagrees with analytic XR pose/FOV projection");
    }
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.fov={-.6F,.6F,.5F,-.5F};
    // A full affine model includes rotation and non-uniform scale. The scene
    // must compose it with view, rather than baking eye-X into source poses.
    auto model_geometry=quad(0,0,0,.11F,{1,0,0,1});
    vr::Matrix4 model{0,.7F,0,0,-1.2F,0,0,0,0,0,1,0,.17F,-.08F,-1.2F,1};
    render::CalibratedSceneDraw model_draw{model_geometry,{},model};
    const auto modeled=render(gpu,scene,std::span(&model_draw,1),camera_for(eye,d3d));
    check(colored_at(modeled,reference(eye,{.17,-.08,-1.2}),0),"Calibrated model matrix not applied");
    auto far=quad(0,0,-2,.4F,{0,0,1,1}),near=quad(0,0,-.5F,.1F,{1,0,0,1});
    std::array<render::CalibratedSceneDraw,2> overlap{{{near},{far}}};
    const auto depth=render(gpu,scene,overlap,camera_for(eye,d3d));
    check(colored_at(depth,{width*.5,height*.5},0),"Calibrated depth did not retain nearest surface");
    auto behind=quad(0,0,.5F,.1F,{1,0,0,1});render::CalibratedSceneDraw hidden{behind};
    const auto empty=render(gpu,scene,std::span(&hidden,1),camera_for(eye,d3d));
    for(unsigned i=0;i<empty.size();i+=4) check(empty[i]==0 && empty[i+1]==0 && empty[i+2]==0,"Behind-eye geometry not clipped or stale frame retained");
}
void material_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.fov={-.6F,.6F,.5F,-.5F};const auto camera=camera_for(eye,d3d);
    auto geometry=quad(0,0,-1,.3F,{.4F,.4F,.4F,.5F});render::CalibratedSceneDraw draw{geometry};draw.depth_test=false;
    const float expected[]{.4F,.4F,0.F,.3F,0.F,.1F,.3F};
    for(unsigned mode=0;mode<7;++mode) {
        draw.blend=static_cast<vr::SceneBlend>(mode);
        const auto pixels=render(gpu,scene,std::span(&draw,1),camera,{.2F,.2F,.2F,1});
        check(std::abs(int(pixels[(height/2*width+width/2)*4])-int(std::round(expected[mode]*255)))<=1,"Calibrated native blend mode mismatch");
    }
    auto texture=quad(0,0,0,.1F,{1,1,1,1});
    const unsigned rgba=0xff0040ff; // Opaque orange-red, no palette substitution.
    for(auto& v:texture) {v.position[0]=v.position[1]=0;v.position[2]=-1;v.billboard[0]=v.uv[0];v.billboard[1]=v.uv[1];}
    constexpr float corners[][2]{{-.16F,-.16F},{.16F,-.16F},{.16F,.16F},{-.16F,-.16F},{.16F,.16F},{-.16F,.16F}};
    for(unsigned i=0;i<6;++i) {texture[i].billboard[0]=corners[i][0];texture[i].billboard[1]=corners[i][1];texture[i].uv[0]=texture[i].uv[1]=0;texture[i].texture[3]=1|4;}
    render::CalibratedSceneDraw textured{texture,std::span(&rgba,1)};
    const auto pixels=render(gpu,scene,std::span(&textured,1),camera);
    const auto offset=(height/2*width+width/2)*4;
    check(pixels[offset]==255 && pixels[offset+1]==64 && pixels[offset+2]==0,"Calibrated billboard/texture bindings wrong");
    auto dither=quad(0,0,-1,.3F,{1,0,0,1});
    for(auto& v:dither) {v.dither_scale=2;v.odd_color[0]=0;v.odd_color[1]=1;}
    render::CalibratedSceneDraw dithering{dither};const auto dithered=render(gpu,scene,std::span(&dithering,1),camera);
    for(unsigned y=85;y<100;++y) for(unsigned x=115;x<140;++x) {
        const auto p=(y*width+x)*4;const bool odd=((x/2)^(y/2))&1;
        check(dithered[p]==(odd?0:255) && dithered[p+1]==(odd?255:0),"Calibrated source dither scale not preserved");
    }
    auto line=quad(0,0,-1,.1F,{0,0,1,1});line.resize(2);
    render::CalibratedSceneDraw lines{line};lines.topology=vr::SceneTopology::lines;
    const auto line_pixels=render(gpu,scene,std::span(&lines,1),camera);unsigned count=0;
    for(unsigned i=2;i<line_pixels.size();i+=4) if(line_pixels[i]) ++count;
    check(count>10 && count<70,"Calibrated line pipeline not rendered");
    // Input failures invalidate publication and require caller cancellation.
    for(unsigned failure=0;failure<6;++failure) {
        auto bad=draw;auto vertices=geometry;bad.vertices=vertices;
        if(failure==0) vertices[0].texture[3]=8; // Requires validated native-packet API, not a raw draw.
        if(failure==1) {vertices[0].texture[3]=1;vertices[0].texture[0]=99;}
        if(failure==2) bad.model[15]=0;
        if(failure==3) vertices[0].position[0]=NAN;
        if(failure==4) bad.vertices=std::span(vertices).first(2);
        if(failure==5) vertices[0].texture[3]=0x80000000U;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.upload(gpu.command,std::span(&bad,1)),"Invalid/specialized calibrated payload accepted");
        check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera),"Failed upload published stale calibrated geometry");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    check(!render(gpu,scene,{},camera).empty(),"Empty calibrated frame/recovery failed");
}
void palette_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d,bool srgb) {
    using render::Effect;
    constexpr Effect styles[]{Effect::off,Effect::monochrome,Effect::sepia,Effect::thermal,Effect::pastel,
        Effect::posterized,Effect::ice,Effect::film,Effect::negative,Effect::solarized,Effect::amber,
        Effect::emerald,Effect::cyanotype,Effect::copper,Effect::lavender,Effect::cga,
        Effect::teal_orange,Effect::handheld,Effect::bleach_bypass,Effect::risograph,
        Effect::duotone,Effect::tritone,Effect::iridescent,Effect::noir,Effect::uv_glow,Effect::topographic};
    constexpr std::array<unsigned char,3> colors[]{
        {0,0,0},{1,1,1},{3,3,3},{4,4,4},{31,31,31},{32,32,32},{35,35,35},{36,36,36},
        {63,63,63},{64,64,64},{127,127,127},{128,128,128},{191,191,191},{192,192,192},{254,254,254},{255,255,255},
        {37,123,219},{210,89,31},{220,230,245},{9,25,45},{255,0,0},{0,255,0},{0,0,255},{127,128,129},
        {91,75,75},{90,0,0},{91,0,0},{150,150,150},{151,150,149},{12,155,70},{74,211,241},{231,37,181}};
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.fov={-.6F,.6F,.5F,-.5F};
    auto camera=camera_for(eye,d3d);
    const auto channel=[&](unsigned char code) {
        const float encoded=float(code)/255.F;
        return !srgb?encoded:encoded<=.04045F?encoded/12.92F:std::pow((encoded+.055F)/1.055F,2.4F);
    };
    // Use the existing independent flat CPU implementation as the oracle,
    // not another copy of the shader's palette equations.
    render::Framebuffer source(1,1);source.enable_layer_tags(true);
    const auto expected=[&](const auto& color,Effect style,unsigned intensity) {
        std::vector<unsigned char> rgba{color[0],color[1],color[2],73},scratch;
        render::apply_effect(style,source,rgba,scratch,static_cast<unsigned char>(intensity));
        return rgba;
    };
    unsigned comparisons=0;
    for(Effect style:styles) for(unsigned intensity:{0U,1U,37U,100U,255U}) {
        std::vector<std::vector<vr::SceneVertex>> geometry;geometry.reserve(128);
        std::vector<std::uint32_t> textures;textures.reserve(128);
        std::vector<render::CalibratedSceneDraw> draws;draws.reserve(128);
        camera.effects={unsigned(style),intensity,~0U,0}; // Renderer owns its colour-space bit.
        for(unsigned index=0;index<std::size(colors);++index) for(unsigned variant=0;variant<4;++variant) {
            const auto& color=colors[index];
            const float px=float((index%8)*32+16),py=float((index/8)*48+variant*12+6);
            auto vertices=quad(0,0,-1,1,{channel(color[0]),channel(color[1]),channel(color[2]),73.F/255});
            for(auto& v:vertices) {
                v.position[0]=(px+v.position[0]*6-width*.5F)*2*std::tan(.6F)/width;
                v.position[1]=(height*.5F-py-v.position[1]*4)*2*std::tan(.5F)/height;
                if(variant==1) {v.texture[3]=1U|(srgb?2U:0U);v.uv[0]=v.uv[1]=0;}
                if(variant==2) {
                    v.dither_scale=1;
                    for(unsigned c=0;c<3;++c) v.odd_color[c]=channel(color[(c+1)%3]);
                }
            }
            geometry.push_back(std::move(vertices));
            textures.push_back(unsigned(color[0])|(unsigned(color[1])<<8)|(unsigned(color[2])<<16)|(73U<<24));
            render::CalibratedSceneDraw draw{geometry.back()};
            if(variant==1) draw.texels=std::span(&textures.back(),1);
            // Every alternate producer uses an explicit per-layer override,
            // preserving the eye transform and correctly resetting sRGB bit.
            if(variant!=0) draw.effects_override=std::array{unsigned(style),intensity,0U,0U};
            draw.preserve_native_colour=variant==3;
            draws.push_back(draw);
        }
        const auto pixels=render(gpu,scene,draws,camera);
        for(unsigned index=0;index<std::size(colors);++index) for(unsigned variant=0;variant<4;++variant) {
            for(unsigned alternate=0;alternate<(variant==2?2U:1U);++alternate) {
                const unsigned px=(index%8)*32+16+alternate,py=(index/8)*48+variant*12+6;
                auto color=colors[index];
                if(variant==2 && ((px^py)&1U)) color={colors[index][1],colors[index][2],colors[index][0]};
                const auto want=expected(color,variant==3?Effect::off:style,intensity);
                const auto* got=&pixels[(py*width+px)*4];
                for(unsigned c=0;c<4;++c) {
                    if(std::abs(int(want[c])-int(got[c]))>1) throw std::runtime_error(
                        "Calibrated palette mismatch: style="+std::to_string(unsigned(style))+" intensity="+std::to_string(intensity)
                        +" srgb="+std::to_string(srgb)+" color="+std::to_string(index)+" producer="+std::to_string(variant)
                        +" channel="+std::to_string(c)+" actual="+std::to_string(got[c])+" expected="+std::to_string(want[c]));
                }
                ++comparisons;
            }
        }
    }
    const bool bgra=gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    std::cout<<"Calibrated "<<(bgra?"BGRA ":"RGBA ")<<(srgb?"sRGB":"UNORM")<<" palette: "<<comparisons
        <<" solid/textured/dither/native-UI CPU-oracle comparisons, 25 styles + OFF, intensity/band/red-channel boundaries passed.\n";
}
void source_model_tests(Gpu& gpu,render::GpuCalibratedScene& scene) {
    // Actual source-model packet preparation, reused without a second
    // geometry decoder, source shading implementation or eye-dependent CPU
    // transform. The renderer takes the unprojected packet, not a screen quad.
    assets::Shape shape;shape.header.shift=2;
    shape.vertices={{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0}};
    shape.faces={{-1,2,{0,0,-127},{0,1,2,3}}};
    render::Palette256 palette{};palette[2]={255,0,0,255};
    render::RenderPose pose;pose.scale=2;pose.x=24;pose.y=-32;pose.z=512;pose.roll=4096;
    pose.palette_override=2;pose.continuous_geometry=pose.subpixel_projection=true;
    for(float units:{1.F,256.F}) {
        vr::DrawPacket packet;std::string error;
        const bool built=vr::build_draw_packet(shape,pose,palette,0,1,false,units,packet,error);check(built,error.c_str());
        check(packet.geometry.vertices.size()==6 && packet.geometry.deferred.empty(),"Source-model packet not complete");
        render::CalibratedSceneDraw draw{packet.geometry.vertex_view(),packet.geometry.texel_view(),packet.model};
        XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.pose.position={16/units,8/units,64/units};eye.fov={-.6F,.6F,.5F,-.5F};
        auto camera=vr::eye_camera(eye,1,1/units,8192/units);check(bool(camera),"Source-model camera invalid");
        for(unsigned col=0;col<4;++col) camera->projection[col*4+1]*=-1;
        const auto pixels=render(gpu,scene,std::span(&draw,1),*camera);
        check(colored_at(pixels,reference(eye,{pose.x/units,-pose.y/units,-pose.z/units}),0),
            "Source-model transform/shading failed calibrated rendering or units scaled twice");
        // Reuse source destruction attributes through the same GPU shader.
        pose.explosion_progress=8;pose.explosion_phase=8.5;
        const bool destroyed=vr::build_draw_packet(shape,pose,palette,0,1,false,units,packet,error);check(destroyed,error.c_str());
        draw={packet.geometry.vertex_view(),packet.geometry.texel_view(),packet.model};
        const auto debris=render(gpu,scene,std::span(&draw,1),*camera);
        unsigned red=0;for(unsigned p=0;p<debris.size();p+=4) if(debris[p]==255 && !debris[p+1]) ++red;
        check(red>20,"Source-model destruction attributes lost in calibrated renderer");
        pose.explosion_progress=0;pose.explosion_phase.reset();
    }
}
std::vector<unsigned char> render_packets(Gpu& gpu,render::GpuCalibratedScene& scene,
    std::span<const render::CalibratedScenePacket> packets,const vr::EyeCamera& camera) {
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload_packets(gpu.command,packets),scene.status().c_str());
    const auto token=scene.upload_token();check(token!=0,"Native upload did not publish a token");
    check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera),scene.status().c_str());
    auto pixels=gpu.finish();check(scene.notify_submitted(token),"Submitted native upload not acknowledged");
    return pixels;
}
void native_packet_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    // Actual native packets, not RGBA screen images disguised as geometry.
    // With a 256-unit source focal length this camera maps source pixels 1:1.
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;
    eye.fov={-std::atan(.5F),std::atan(.5F),std::atan(.375F),-std::atan(.375F)};
    auto camera=camera_for(eye,d3d);
    simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
    ppu.bg2_character_base=0;ppu.bg2_screen_base=0x1000;ppu.bg2_screen_size=0;ppu.cgram[1]=31;
    for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255; // Tile 1, red index 1.
    for(unsigned tile=0;tile<1024;++tile) ppu.vram[0x2000+tile*2]=1;
    auto background=vr::background_tile_packet(ppu,vr::BackgroundLayer::bg2);
    background.model=vr::source_layer_matrix(128,96,2).value();
    render::CalibratedScenePacket bg{&background,vr::SceneBlend::opaque,false};
    const auto red=render_packets(gpu,scene,std::span(&bg,1),camera);
    for(unsigned p=0;p<red.size();p+=4)
        check(red[p]==255 && red[p+1]==0 && red[p+2]==0,"Native BG2 tile/palette not decoded across calibrated viewport");
    // Native OAM sprite sampling and ordering over the tile layer.
    ppu.main_screen=16;ppu.object_select=0;
    for(unsigned o=0;o<128;++o) ppu.oam[o*4+1]=248;
    ppu.oam[0]=40;ppu.oam[1]=32;ppu.oam[2]=2;ppu.oam[3]=0;ppu.cgram[129]=31<<5;
    for(unsigned y=0;y<8;++y) ppu.vram[64+y*2]=255;
    auto sprite=vr::source_sprite_packet(ppu);sprite.model=background.model;sprite.preserve_native_colour=true;
    render::CalibratedScenePacket obj{&sprite,vr::SceneBlend::alpha,false};
    const std::array layers{bg,obj};
    const auto layered=render_packets(gpu,scene,layers,camera);
    check(colored_at(layered,{44,36},1) && colored_at(layered,{60,36},0),"Native OBJ order/UV/palette incorrect");
    camera.effects={4,255,0,0};
    const auto styled=render_packets(gpu,scene,layers,camera);
    const unsigned sample=(36*width+60)*4;
    check(styled[sample]==styled[sample+1] && styled[sample+1]==styled[sample+2] && styled[sample]>30 && styled[sample]<110,
        "Native world layer did not receive the selected scene style");
    check(colored_at(styled,{44,36},1),"Native HUD/OBJ colour changed under world styling");camera.effects={};
    // Head/screen-locked source wipe covers every column and row, independent
    // of the tracked world pose/FOV; it must not leave outer image strips.
    simulation::WindowWipeState closed;closed.active=true;closed.logic=0xaa;
    closed.left.fill(16);closed.right.fill(239);
    auto shutter=vr::source_shutter_packet({},closed,0);
    render::CalibratedScenePacket wipe{&shutter,vr::SceneBlend::opaque,false,vr::EyeCamera{identity,identity}};
    const std::array shut_layers{bg,obj,wipe};
    eye.pose.position={.1F,.08F,.1F};eye.pose.orientation.z=std::sin(.1F);eye.pose.orientation.w=std::cos(.1F);
    const auto black=render_packets(gpu,scene,shut_layers,camera_for(eye,d3d));
    for(unsigned p=0;p<black.size();p+=4)
        check(black[p]==0 && black[p+1]==0 && black[p+2]==0,"Head-locked native wipe did not cover complete calibrated view");

    // Immutable photo data is copied only once, even for multiple subjects;
    // palette changes are shader draw data, not a re-uploaded photograph.
    render::BackdropImage image;image.width=image.height=4;image.pixels.assign(16,0xff0040ff);
    auto artwork=vr::make_backdrop_texture(image,false);
    vr::PhotographicBody body;body.center={96,112};auto first=vr::photographic_body_packet(artwork,body);
    body.center={160,112};auto second=vr::photographic_body_packet(artwork,body);
    std::array photos{render::CalibratedScenePacket{&first,vr::SceneBlend::alpha,false},
        render::CalibratedScenePacket{&second,vr::SceneBlend::alpha,false}};
    eye.pose={};eye.pose.orientation.w=1;
    auto photo_camera=vr::eye_camera(eye,1,.05F,8192.F).value();
    for(unsigned col=0;col<4;++col) photo_camera.projection[col*4+1]*=-1;
    const auto cold=render_packets(gpu,scene,photos,photo_camera);
    const auto photo_bytes=artwork->size()*4;
    check(scene.upload_cost()[1]==photo_bytes && scene.upload_cost()[2]==photo_bytes,"Immutable artwork not deduplicated within native frame");
    const auto cold_token=scene.upload_token();
    const auto warm=render_packets(gpu,scene,photos,photo_camera);
    check(cold==warm && scene.upload_cost()[1]==0 && scene.upload_cost()[2]==photo_bytes*2,"Submitted immutable artwork was uploaded again or changed output");
    check(!scene.notify_submitted(cold_token) && !scene.notify_submitted(0),"Stale/zero upload token acknowledged");
    const unsigned photo_sample=(height/2*width+(width/2-16))*4;
    check(cold[photo_sample]==255 && cold[photo_sample+1]==64 && cold[photo_sample+2]==0,"Photographic native body not sampled");
    body.center={96,112};body.response={.5F,1,1,1};first=vr::photographic_body_packet(artwork,body);
    const auto faded=render_packets(gpu,scene,photos,photo_camera);
    check(scene.upload_cost()[1]==0 && faded[photo_sample]>=126 && faded[photo_sample]<=129 && faded[photo_sample+1]==64,
        "Live photo palette/brightness change re-uploaded artwork or did not change shading");
    // An encoded-but-cancelled upload is not resident proof. A new payload
    // must be copied again and a stale cancelled token cannot validate it.
    image.pixels.assign(16,0xffff0000);auto fresh=vr::make_backdrop_texture(image,false);
    body.response={1,1,1,1};auto blue=vr::photographic_body_packet(fresh,body);
    render::CalibratedScenePacket blue_draw{&blue,vr::SceneBlend::alpha,false};
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload_packets(gpu.command,std::span(&blue_draw,1)),scene.status().c_str());
    const auto cancelled_token=scene.upload_token();SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    const auto recovered=render_packets(gpu,scene,std::span(&blue_draw,1),photo_camera);
    check(scene.upload_cost()[1]==fresh->size()*4 && !scene.notify_submitted(cancelled_token)
        && recovered[photo_sample]==0 && recovered[photo_sample+2]==255,"Cancelled artwork upload incorrectly treated as ready");
    vr::DrawPacket solid;solid.geometry.vertices=quad(0,0,-1,.2F,{1,0,0,1});
    solid.geometry.shared_texels=std::make_shared<const std::vector<uint32_t>>();
    render::CalibratedScenePacket solid_draw{&solid};
    const auto plain_solid=render_packets(gpu,scene,std::span(&solid_draw,1),camera);
    check(colored_at(plain_solid,{128,96},0),"Cached solid mesh with no texture data failed");
    auto moved_solid=solid;moved_solid.model[12]=.12F;moved_solid.model[13]=-.06F;
    render::CalibratedScenePacket moved_draw{&moved_solid};
    const auto expected_moved=render_packets(gpu,scene,std::span(&moved_draw,1),camera);
    solid_draw.model_override=moved_solid.model;
    check(render_packets(gpu,scene,std::span(&solid_draw,1),camera)==expected_moved && expected_moved!=plain_solid,
        "Native presentation model override bypassed geometry or differs from explicit model");
    check(solid.model==identity,"Native presentation model override mutated source geometry");
    // Malformed native payloads invalidate the current publication, rather
    // than accepting a bad tile/sprite/image or showing a stale previous frame.
    for(unsigned failure=0;failure<7;++failure) {
        auto bad=background;
        if(failure==0) bad.geometry.texels.resize(4);
        if(failure==1) {bad=sprite;bad.geometry.texels.resize(4);}
        if(failure==2) {bad=blue;auto vertices=std::make_shared<std::vector<vr::SceneVertex>>(*bad.geometry.shared_vertices);
            (*vertices)[0].texture[3]=0x80000000U;bad.geometry.shared_vertices=vertices;}
        render::CalibratedScenePacket invalid{&bad};
        if(failure==3) {invalid.camera_override=vr::EyeCamera{identity,identity};invalid.camera_override->view[0]=NAN;}
        if(failure>=4) {
            invalid.model_override=identity;
            if(failure==4) (*invalid.model_override)[0]=NAN;
            if(failure==5) (*invalid.model_override)[7]=.1F;
            if(failure==6) (*invalid.model_override)[15]=0;
        }
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.upload_packets(gpu.command,std::span(&invalid,1)) && !scene.upload_token(),"Malformed native calibrated payload accepted");
        check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera),"Failed native upload retained stale publication");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    solid_draw.model_override.reset();
    check(render_packets(gpu,scene,std::span(&solid_draw,1),camera)==plain_solid,
        "Native presentation model OFF/malformed recovery retained stale transform");
}
// Independent source projection/line walker, not a duplicate GPU row-bin
// implementation. Emit a simple marker per covered pixel for a reference
// native packet; both packets then undergo the same calibrated eye matrices.
vr::DrawPacket connected_reference(const vr::DrawPacket& source,unsigned& covered) {
    const auto& words=source.geometry.texels;
    timing::RenderTransform camera{};camera.x=int32_t(words[0]);camera.y=int32_t(words[1]);camera.z=int32_t(words[2]);
    simulation::MatrixQ15 matrix{};
    for(unsigned i=0;i<9;++i) matrix[i]=int16_t(words[3+i]);
    auto previous=std::array<int16_t,2>{int16_t(words[12]),int16_t(words[13])};
    std::array<bool,224*192> mask{};
    const auto marker=[&](int x,int y){if(x>=0 && y>=0 && x<224 && y<192) mask[unsigned(y)*224+unsigned(x)]=true;};
    const auto projected=render::project_source_grid(camera,matrix,224,192);
    for(std::size_t i=0;i<projected.count;++i) {
        const auto& point=projected.points[i];const std::array<int16_t,2> current{int16_t(point.x-1),point.y};
        marker(current[0],current[1]+2);
        for(unsigned step=0;step<=unsigned(std::max(int(current[0])-previous[0],0));++step) {
            const auto sample=render::grid_line_sample(current,previous,step);check(bool(sample),"Invalid independent source line sample");
            marker((*sample)[0],(*sample)[1]);
        }
        if(point.depth<512) marker(current[0]-1,current[1]+1);
        previous=current;
    }
    vr::DrawPacket packet=source;for(auto& v:packet.geometry.vertices) v.texture[3]=512;
    auto& payload=packet.geometry.texels;payload.assign(384,0);covered=0;
    for(unsigned y=0;y<192;++y) {
        std::vector<uint32_t> records;
        for(unsigned x=0;x<224;++x) if(mask[y*224+x]) {
            records.push_back(uint32_t(payload.size()));payload.insert(payload.end(),{1,x,y,0,0});++covered;
        }
        payload[y*2]=uint32_t(payload.size());payload[y*2+1]=uint32_t(records.size());
        payload.insert(payload.end(),records.begin(),records.end());
    }
    return packet;
}
void connected_grid_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;
    eye.fov={-std::atan(.5F),std::atan(.5F),std::atan(.375F),-std::atan(.375F)};
    vr::DrawPacket grid;grid.model=vr::source_layer_matrix(128,96,2).value();
    constexpr unsigned corners[6][2]{{0,0},{224,0},{224,192},{0,0},{224,192},{0,192}};
    for(const auto& corner:corners) {
        vr::SceneVertex v{};v.position[0]=float(corner[0]+16);v.position[1]=float(corner[1]+16);
        v.uv[0]=float(corner[0]);v.uv[1]=float(corner[1]);v.texture[3]=vr::gpu_connected_grid_flag;
        v.color[0]=v.color[3]=v.odd_color[0]=v.odd_color[3]=1;grid.geometry.vertices.push_back(v);
    }
    unsigned visible_frames=0;
    for(unsigned frame=0;frame<24;++frame) {
        const int y=frame%6==0?32767:frame%6==1?-32768:-256-int(frame%4)*128;
        const simulation::MatrixQ15 matrix=frame%2
            ?simulation::MatrixQ15{23170,23170,0,-23170,23170,0,0,0,32767}
            :simulation::MatrixQ15{32767,0,0,0,32767,0,0,0,32767};
        grid.geometry.texels={uint32_t(frame*17),uint32_t(y),uint32_t(frame*31)};
        for(auto word:matrix) grid.geometry.texels.push_back(uint32_t(int32_t(word)));
        grid.geometry.texels.push_back(uint32_t(int(frame%3)*100-100));grid.geometry.texels.push_back(uint32_t(int(frame)*7-64));
        unsigned covered{};auto reference_packet=connected_reference(grid,covered);visible_frames+=covered!=0;
        render::CalibratedScenePacket reference_draw{&reference_packet,vr::SceneBlend::opaque,false};
        render::CalibratedScenePacket gpu_draw{&grid,vr::SceneBlend::opaque,false};
        for(unsigned view=0;view<3;++view) {
            auto tracked=eye;
            if(view) {
                tracked.pose.position.x=view==1?-.05F:.075F;
                tracked.fov.angleRight+=.04F;tracked.fov.angleDown-=.025F;
                tracked.pose.orientation.z=std::sin(.015F);tracked.pose.orientation.w=std::cos(.015F);
            }
            const auto camera=camera_for(tracked,d3d);
            const auto expected=render_packets(gpu,scene,std::span(&reference_draw,1),camera);
            const auto computed=render_packets(gpu,scene,std::span(&gpu_draw,1),camera);
            check(expected==computed,"Calibrated compute ground differs from independent source projection/line walk");
            check(scene.upload_cost()[1]==14*4 && scene.upload_cost()[0]==6*sizeof(vr::SceneVertex),
                "Connected grid uploaded CPU-projected output instead of compact GPU input");
            check(render_packets(gpu,scene,std::span(&gpu_draw,1),camera)==computed,"Repeated/other-eye ground output changed source history");
        }
    }
    check(visible_frames>8,"Connected-grid fixtures failed to cover visible geometry");
    auto first_grid=grid,second_grid=grid;
    first_grid.geometry.texels[1]=uint32_t(-384);first_grid.geometry.texels[2]=73;
    second_grid.geometry.texels[1]=uint32_t(-1024);second_grid.geometry.texels[2]=183;
    unsigned first_covered{},second_covered{};
    auto first_reference=connected_reference(first_grid,first_covered),second_reference=connected_reference(second_grid,second_covered);
    render::CalibratedScenePacket first_draw{&first_grid,vr::SceneBlend::opaque,false},second_draw{&second_grid,vr::SceneBlend::opaque,false};
    render::CalibratedScenePacket first_ref{&first_reference,vr::SceneBlend::opaque,false},second_ref{&second_reference,vr::SceneBlend::opaque,false};
    const auto stationary=camera_for(eye,d3d);
    const auto first_expected=render_packets(gpu,scene,std::span(&first_ref,1),stationary);
    const auto second_expected=render_packets(gpu,scene,std::span(&second_ref,1),stationary);
    check(first_expected!=second_expected,"In-flight grid fixture did not change its source image");
    struct Download {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer;
        ~Download(){if(buffer) SDL_ReleaseGPUTransferBuffer(device,buffer);}
    };
    const SDL_GPUTransferBufferCreateInfo download{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*4,0};
    Download first_read{gpu.device,SDL_CreateGPUTransferBuffer(gpu.device,&download)};check(first_read.buffer,SDL_GetError());
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload_packets(gpu.command,std::span(&first_draw,1)),scene.status().c_str());
    const auto first_token=scene.upload_token();
    check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,stationary),scene.status().c_str());
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUTextureRegion region{gpu.color,0,0,0,0,0,width,height,1};
    SDL_GPUTextureTransferInfo destination{};destination.transfer_buffer=first_read.buffer;destination.pixels_per_row=width;destination.rows_per_layer=height;
    SDL_DownloadFromGPUTexture(copy,&region,&destination);SDL_EndGPUCopyPass(copy);
    const bool submitted=SDL_SubmitGPUCommandBuffer(gpu.command);gpu.command=nullptr;check(submitted,SDL_GetError());
    check(scene.notify_submitted(first_token),"In-flight grid submission not acknowledged");
    // Intentionally no fence wait between source-frame submissions.
    check(render_packets(gpu,scene,std::span(&second_draw,1),stationary)==second_expected,"New in-flight ground source changed output");
    auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,first_read.buffer,false));check(mapped,SDL_GetError());
    const bool retained=std::equal(first_expected.begin(),first_expected.end(),mapped);SDL_UnmapGPUTransferBuffer(gpu.device,first_read.buffer);
    check(retained,"New source-frame upload overwrote in-flight grid/vertex/texture inputs");
    // Two distinct producers in one frame cannot borrow one another's output.
    for(auto& v:second_grid.geometry.vertices) {v.color[0]=v.odd_color[0]=0;v.color[1]=v.odd_color[1]=1;}
    second_reference=connected_reference(second_grid,second_covered);
    const std::array multiple_reference{first_ref,second_ref},multiple_gpu{first_draw,second_draw};
    const auto multiple_expected=render_packets(gpu,scene,multiple_reference,stationary);
    check(render_packets(gpu,scene,multiple_gpu,stationary)==multiple_expected && scene.upload_cost()[1]==2*14*4,
        "Independent connected-grid producers shared or uploaded projected output");
    // Cancel an encoded frame; the next frame must recompute from its own
    // words, not reuse a result from an unsubmitted or earlier source frame.
    render::CalibratedScenePacket draw{&grid,vr::SceneBlend::opaque,false};
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload_packets(gpu.command,std::span(&draw,1)),scene.status().c_str());
    const auto token=scene.upload_token();SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    grid.geometry.texels[0]=183;unsigned covered{};auto reference=connected_reference(grid,covered);
    render::CalibratedScenePacket reference_draw{&reference,vr::SceneBlend::opaque,false};
    const auto camera=camera_for(eye,d3d);const auto expected=render_packets(gpu,scene,std::span(&reference_draw,1),camera);
    check(render_packets(gpu,scene,std::span(&draw,1),camera)==expected && !scene.notify_submitted(token),"Cancelled connected-grid output was published");
    for(unsigned fault=0;fault<4;++fault) {
        auto bad=grid;
        if(fault==0) bad.geometry.texels.pop_back();
        if(fault==1) bad.geometry.texels[3]=32768;
        if(fault==2) bad.geometry.vertices[0].texture[3]=512;
        if(fault==3) bad.geometry.line_vertices={bad.geometry.vertices[0],bad.geometry.vertices[1]};
        render::CalibratedScenePacket invalid{&bad};
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.upload_packets(gpu.command,std::span(&invalid,1)) && !scene.upload_token(),"Invalid compute grid accepted");
        check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera),"Invalid compute grid retained prior scene");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    std::array<render::CalibratedScenePacket,32> excessive;excessive.fill(draw);
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(!scene.upload_packets(gpu.command,excessive) && scene.status().find("output budget")!=std::string::npos,
        "Connected-grid GPU allocation budget not enforced before encoding");
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    check(render_packets(gpu,scene,std::span(&draw,1),camera)==expected,"Connected-grid validation failure did not recover");
}
}
void native_environment_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    constexpr unsigned size=16,cube_bytes=size*size*6*4,bytes=6*16+2*64+cube_bytes;
    constexpr unsigned colours[6][3]={{240,32,64},{32,224,96},{64,48,208},{192,176,32},{48,192,216},{208,64,176}};
    const bool srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    const auto linear=[&](unsigned code) {float value=float(code)/255;return srgb?(value<=.04045F?value/12.92F:std::pow((value+.055F)/1.055F,2.4F)):value;};
    std::array<std::vector<vr::SceneVertex>,6> walls;
    for(unsigned face=0;face<6;++face) for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
        const unsigned axis=face/2,a=(axis+1)%3,b=(axis+2)%3;
        vr::SceneVertex v{};float source[3]{};source[axis]=(face%2?-4.F:4.F);
        source[a]=(corner==1 || corner==2)?4.F:-4.F;source[b]=corner>=2?4.F:-4.F;
        v.position[0]=source[0];v.position[1]=-source[1];v.position[2]=-source[2];
        for(unsigned c=0;c<3;++c) v.color[c]=v.odd_color[c]=linear(colours[face][c]);
        v.color[3]=v.odd_color[3]=1;walls[face].push_back(v);
    }
    auto mirror=quad(0,0,-2,.7F,{1,1,1,1});
    auto excluded=quad(0,0,-.5F,20,{1,0,1,1});
    std::vector<render::CalibratedSceneDraw> draws;
    for(auto& wall:walls) {render::CalibratedSceneDraw draw{wall};draw.reflection_environment=true;draws.push_back(draw);}
    render::CalibratedSceneDraw model{mirror};model.ray_caster=true;draws.push_back(model);
    render::CalibratedSceneDraw ui{excluded};ui.after_rays=true;draws.push_back(ui);
    struct Download {SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer;~Download(){SDL_ReleaseGPUTransferBuffer(device,buffer);}};
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes*2,0};
    Download read{gpu.device,SDL_CreateGPUTransferBuffer(gpu.device,&info)};check(read.buffer,SDL_GetError());
    for(unsigned variant=0;variant<2;++variant) {
        for(unsigned face=0;face<6;++face) {
            for(auto& vertex:walls[face]) for(unsigned c=0;c<3;++c)
                vertex.color[c]=vertex.odd_color[c]=linear((colours[face][c]+variant*9)%256);
            draws[face].effects_override=std::array<unsigned,4>{variant?unsigned(render::Effect::negative):0U,100,0,0};
        }
        std::array<XrView,2> views{};std::array<vr::EyeCamera,2> cameras;
        std::array<render::CalibratedRayGeometryOutput,2> rays;
        std::array<std::vector<std::uint32_t>,2> expected;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto cost=scene.upload_cost();
        for(unsigned eye=0;eye<2;++eye) {
            views[eye]={XR_TYPE_VIEW};views[eye].pose.position={eye?.19F:-.13F,.08F,-.06F};
            const float angle=eye?.21F:-.17F;views[eye].pose.orientation={0,std::sin(angle/2),0,std::cos(angle/2)};
            views[eye].fov={-.45F,.45F,.3F,-.3F};cameras[eye]=camera_for(views[eye],d3d);
            rays[eye]=scene.enqueue_ray_geometry(gpu.command,eye,12,8,cameras[eye],1,size,{.01F,.02F,.03F,1});
            const auto& ray=rays[eye];check(ray.complete && ray.environment_face_size==size && ray.environment_offset==128
                && ray.material_bytes==128 && ray.material_offset==96,scene.status().c_str());
            expected[eye].resize(size*size*6);
            // Independent cube ray/room intersection oracle. It does not call
            // the production camera helper or read its matrices/packed pixels.
            const double origin[3]={views[eye].pose.position.x,-views[eye].pose.position.y,-views[eye].pose.position.z};
            for(unsigned face=0;face<6;++face) for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x) {
                const double u=(double(x)+.5)/size*2-1,v=(double(y)+.5)/size*2-1;
                double direction[3];
                if(face==0) {direction[0]=1;direction[1]=-v;direction[2]=-u;}
                else if(face==1) {direction[0]=-1;direction[1]=-v;direction[2]=u;}
                else if(face==2) {direction[0]=u;direction[1]=1;direction[2]=v;}
                else if(face==3) {direction[0]=u;direction[1]=-1;direction[2]=-v;}
                else if(face==4) {direction[0]=u;direction[1]=-v;direction[2]=1;}
                else {direction[0]=-u;direction[1]=-v;direction[2]=-1;}
                double nearest=1e30;unsigned hit=0;
                for(unsigned axis=0;axis<3;++axis) if(std::abs(direction[axis])>1e-12) {
                    const double t=((direction[axis]>0?4.:-4.)-origin[axis])/direction[axis];
                    if(t<nearest) {nearest=t;hit=axis*2+unsigned(direction[axis]<0);}
                }
                unsigned packed=0xff000000U;
                for(unsigned c=0;c<3;++c) {unsigned code=(colours[hit][c]+variant*9)%256;
                    if(variant) code=255-code;packed|=code<<(c*8);}
                expected[eye][face*size*size+y*size+x]=packed;
            }
            // Independent quaternion rotation: eye ray -> world source cube.
            const double yaw=angle;
            const std::array<double,9> rotation{std::cos(yaw),0,-std::sin(yaw),0,1,0,std::sin(yaw),0,std::cos(yaw)};
            for(unsigned at=0;at<9;++at) check(std::abs(ray.environment_rotation[at]-rotation[at])<1e-6,
                "Native environment dropped/mirrored tracked rotation");
        }
        check(rays[0].buffer!=rays[1].buffer && scene.upload_cost()==cost,"Native cube reused eye storage or uploaded CPU pixels");
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        for(unsigned eye=0;eye<2;++eye) {
            const SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(rays[eye].buffer),0,bytes};
            const SDL_GPUTransferBufferLocation dest{read.buffer,eye*bytes};SDL_DownloadFromGPUBuffer(copy,&source,&dest);
        }
        SDL_EndGPUCopyPass(copy);gpu.finish();
        const auto* data=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(data,SDL_GetError());
        for(unsigned eye=0;eye<2;++eye) for(unsigned pixel=0;pixel<expected[eye].size();++pixel) {
            unsigned actual;std::memcpy(&actual,data+eye*bytes+224+pixel*4,4);
            check(actual==expected[eye][pixel],"Native cube face orientation/translation/style/colour/HUD exclusion mismatch");
        }
        SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
        if(d3d) for(unsigned eye=0;eye<2;++eye) {
            const auto& ray=rays[eye];const render::GpuScene::RayGeometryOutput geometry{
                ray.device,ray.buffer,ray.vertex_count,true,ray.materials,ray.material_offset,ray.material_bytes};
            const render::shadows::Camera camera{12,8,ray.projection[0],ray.projection[2],ray.projection[3],ray.projection[1],2};
            const std::array<std::uint32_t,256> palette{};
            render::shadows::SdlDxrShadows native,cpu_reference;std::vector<std::uint8_t> actual,oracle;
            check(native.render_reflections(gpu.device,camera,geometry,palette,0xffabcdefU,0,0,{},size,
                ray.environment_rotation,nullptr,{},0,nullptr,false,{},ray.environment_offset,srgb?2U:1U),native.status().c_str());
            check(native.readback(actual),native.status().c_str());
            check(cpu_reference.render_reflections(gpu.device,camera,geometry,palette,0xffabcdefU,0,0,
                expected[eye],size,ray.environment_rotation,nullptr,{},0,nullptr,false,{},0,srgb?2U:1U),cpu_reference.status().c_str());
            check(cpu_reference.readback(oracle),cpu_reference.status().c_str());
            check(actual==oracle && std::any_of(actual.begin(),actual.end(),[](auto byte){return byte!=0;}),
                "Native resident cube DXR differs from independent authored cube reflection");
            check(!native.render_reflections(gpu.device,camera,geometry,palette,0,0,0,{},size,
                ray.environment_rotation,nullptr,{},0,nullptr,false,{},ray.environment_offset-4,srgb?2U:1U)
                && !native.reflection_output().buffer,"Invalid resident cube range retained stale output");
        }
    }
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,draws),scene.status().c_str());
    auto camera=vr::EyeCamera{identity,{1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-.01F,0}};
    for(unsigned bad:{7U,17U,1024U}) check(!scene.enqueue_ray_geometry(gpu.command,0,12,8,camera,1,bad).complete,
        "Invalid native capture size accepted");
    camera.view[0]=2;check(!scene.enqueue_ray_geometry(gpu.command,0,12,8,camera,1,size).complete,"Nonrigid tracked cube camera accepted");
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    for(unsigned bad=0;bad<3;++bad) {
        auto invalid=draws[0];if(bad==0) invalid.ray_caster=true;if(bad==1) invalid.after_rays=true;
        if(bad==2) invalid.camera_override=vr::EyeCamera{identity,identity};
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.upload(gpu.command,std::span(&invalid,1)),"Excluded source was allowed into reflection scenery");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    std::cout<<"Native environment cube: independent six-face tracked-position/orientation oracle, live colours/styles, native HUD/model exclusion, two resident eyes, no CPU image upload, invalid ranges/cameras and "
        <<(d3d?"actual resident DXR versus independently authored cube reflections":"GPU packing")<<" passed.\n";
}
void native_specular_material_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    // Two finite parallel 45-degree mirrors. Primary +Z reflects to +X,
    // strikes an OFFSCREEN model, then reflects back to +Z sky. Returning that
    // second model's green source ink is NOT a valid mirror implementation.
    const bool srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
        || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    const auto decode=[&](double code) {code/=255.;return !srgb?code:code<=.04045?code/12.92:std::pow((code+.055)/1.055,2.4);};
    const auto pack=[&](double value) {value=std::clamp(value,0.,1.);
        if(srgb) value=value<=.0031308?value*12.92:1.055*std::pow(value,1./2.4)-.055;
        return unsigned(std::lround(value*255));};
    const std::array<unsigned,3> primary{150,170,230},secondary{25,220,55},sky{208,196,181};
    std::array<std::vector<vr::SceneVertex>,2> planes;
    for(unsigned plane=0;plane<2;++plane) for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
        vr::SceneVertex v{};const double along=(corner==1 || corner==2)?1.:-1.;
        // DXR source +Y down/+Z forward -> native source +Y up/-Z forward.
        v.position[0]=float(plane?3+along:along*.7);
        v.position[1]=corner>=2?-2:2;
        v.position[2]=-float(plane?2+along:2+along*.7);
        for(unsigned c=0;c<3;++c) v.color[c]=v.odd_color[c]=float(decode(plane?secondary[c]:primary[c]));
        v.color[3]=v.odd_color[3]=1;planes[plane].push_back(v);
    }
    std::array<render::CalibratedSceneDraw,2> draws;
    for(unsigned p=0;p<2;++p) {draws[p].vertices=planes[p];draws[p].ray_caster=true;draws[p].reflective_material=true;}
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,draws),scene.status().c_str());
    std::array<render::CalibratedRayGeometryOutput,2> rays;
    constexpr unsigned w=12,h=8,size=8;
    for(unsigned eye=0;eye<2;++eye) {
        XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;view.pose.position.x=eye?.04F:-.04F;
        const float fov=std::atan(.1F);view.fov={-fov,fov,fov,-fov};
        rays[eye]=scene.enqueue_ray_geometry(gpu.command,eye,w,h,camera_for(view,d3d),1);
        check(rays[eye].complete,scene.status().c_str());
    }
    const auto cost=scene.upload_cost();gpu.finish();check(scene.upload_cost()==cost,"Native material uploaded extra source pixels");
    if(d3d) {
        // Diagnostic-only authored cube. Expected values below are analytic,
        // not derived from downloaded ray records or renderer colour helpers.
        unsigned skyWord=0xff000000U;for(unsigned c=0;c<3;++c) skyWord|=sky[c]<<(c*8);
        std::vector<unsigned> cube(size*size*6,skyWord);const std::array<std::uint32_t,256> palette{};
        for(unsigned eye=0;eye<2;++eye) {
            const auto& r=rays[eye];const render::GpuScene::RayGeometryOutput geometry{r.device,r.buffer,r.vertex_count,true,r.materials,r.material_offset,r.material_bytes};
            const render::shadows::Camera camera{w,h,r.projection[0],r.projection[2],r.projection[3],r.projection[1],2};
            render::shadows::SdlDxrShadows tracer;std::vector<unsigned char> actual;
            for(unsigned conductor:{0U,1U,2U,3U}) {
                check(tracer.render_reflections(gpu.device,camera,geometry,palette,0,0,conductor,cube,size,
                    {1,0,0,0,1,0,0,0,1},nullptr,{},0,nullptr,false,{},0,srgb?2U:1U,true),tracer.status().c_str());
                check(tracer.readback(actual) && actual.size()==w*h*4,tracer.status().c_str());
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    // Independent primary direction and both mirror normals.
                    const double dx=(2*(double(x)+.5)/w-1)*.1,dy=(2*(double(y)+.5)/h-1)*.1;
                    const double cosine=(1-dx)/std::sqrt(2*(dx*dx+dy*dy+1));
                    const double grazing=std::pow(1-cosine,5);
                    for(unsigned c=0;c<3;++c) {
                        double expected=decode(sky[c]);
                        if(conductor) {
                            const double fixed=conductor==2?std::array<double,3>{1,.766,.336}[c]
                                :std::array<double,3>{.955,.638,.538}[c];
                            const double first=conductor==1?decode(primary[c]):fixed;
                            const double second=conductor==1?decode(secondary[c]):fixed;
                            // Secondary transport is byte-packed before the
                            // primary cone/Fresnel consumes it; include that
                            // intermediate quantization in the independent oracle.
                            expected=decode(pack(expected*(second+(1-second)*grazing)));
                            expected*=first+(1-first)*grazing;
                        }
                        check(std::abs(int(actual[(y*w+x)*4+c])-int(pack(expected)))<=1,
                            "Native mirror/conductor transport failed independent two-bounce linear/sRGB oracle");
                    }
                    check(actual[(y*w+x)*4+3]==255,"Native material changed source opacity");
                }
            }
            check(tracer.render_reflections(gpu.device,camera,geometry,palette,0,0,0,cube,size,
                {1,0,0,0,1,0,0,0,1},nullptr,{},0,nullptr,false,{},0,srgb?2U:1U,false),tracer.status().c_str());
            check(tracer.readback(actual),tracer.status().c_str());
            for(unsigned pixel=0;pixel<w*h;++pixel) for(unsigned c=0;c<3;++c)
                check(actual[pixel*4+c]==secondary[c],"Ordinary dielectric secondary hit became a mirror");
        }
    }
    // Selection must never leak into headlocked/native/post-ray layers.
    for(unsigned bad=0;bad<4;++bad) {
        auto invalid=draws[0];if(bad==0) invalid.ray_caster=false;if(bad==1) invalid.after_rays=true;
        if(bad==2) invalid.preserve_native_colour=true;if(bad==3) invalid.camera_override=vr::EyeCamera{identity,identity};
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.upload(gpu.command,std::span(&invalid,1)),"Native material accepted an excluded layer");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    std::cout<<"Native reflective materials: independent eyes, explicit receiver/native-layer exclusion, "
        <<(d3d?"two-bounce mirror/palette/gold/copper linear/sRGB radiance oracle and dielectric restoration":"GPU source encoding")<<" passed.\n";
}
void native_ground_gradient_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
    ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
    for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
    for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
    auto original=vr::landscape_sphere_packet(ppu,{},112,false,false,232);
    vr::place_landscape_ground(original,-.75F,true);
    const render::CalibratedGroundGradient gradient{{.31F,.72F,.21F},{.04F,.19F,.035F},112,111};
    auto enhanced=original;
    check(render::apply_calibrated_ground(enhanced,gradient),"Plain native floor did not accept its gradient");
    check(original.geometry.texel_view().size()==render::calibrated_ground_offset
        && original.geometry.vertex_view().data()!=enhanced.geometry.vertex_view().data()
        && !(original.geometry.vertex_view().front().texture[3]&render::calibrated_ground_flag),
        "Ground extension modified retained source geometry");
    bool rejected=false;try {vr::ScenePacketValidator validator;validator.add(enhanced);}catch(const std::exception&) {rejected=true;}
    check(rejected,"Headset defaults accepted a native-only ground shader extension");
    unsigned compared=0,sky=0;std::vector<unsigned char> previous;
    for(float bank:{0.F,.19F,-.23F}) for(unsigned e=0;e<2;++e) {
        XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;
        eye.pose.position={e?.035F:-.028F,.027F,.04F};eye.fov={-.64F,.72F,.57F,-.52F};
        auto camera=camera_for(eye,d3d);
        // Native scenery uses an infinite far plane. The other fixture's
        // 20-metre model-only camera clips this 64-metre sky/ground surround.
        camera.projection[10]=-1;camera.projection[14]=-.05F;
        const float cosine=std::cos(bank),sine=std::sin(bank);
        enhanced.model={cosine,sine,0,0,-sine,cosine,0,0,0,0,1,0,.08F,0,-.12F,1};
        original.model=enhanced.model;
        render::CalibratedScenePacket off{&original,vr::SceneBlend::opaque,false};
        const auto baseline=render_packets(gpu,scene,std::span(&off,1),camera);
        render::CalibratedScenePacket draw{&enhanced,vr::SceneBlend::opaque,false};
        const auto pixels=render_packets(gpu,scene,std::span(&draw,1),camera);
        if(bank==0 && e==0) previous=pixels;
        if(bank==0 && e==1) check(pixels!=previous,"Native gradient collapsed the independent eye poses");
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            // Independent FOV ray and inverse authored bank, not the GPU's
            // camera/model composer or private receiver image.
            const double dx=std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),(x+.5)/width);
            const double dy=std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),(y+.5)/height);
            const double ox=eye.pose.position.x-.08,oy=eye.pose.position.y,oz=eye.pose.position.z+.12;
            const double localY=-sine*ox+cosine*oy,rayY=-sine*dx+cosine*dy;
            const auto at=(std::size_t(y)*width+x)*4;
            check(pixels[at+3]==255,"Native gradient changed source opacity");
            if(rayY>=.015) {
                // The runtime's calibrated far plane can clip the distant
                // sky sphere. Preserve that exact original coverage too.
                for(unsigned c=0;c<4;++c)
                    check(pixels[at+c]==baseline[at+c],"Ground gradient crossed into native sky");
                ++sky;continue;
            }
            if(rayY>-.02) continue;
            const double distance=(-.75-localY)/rayY;
            if(distance<.2 || distance>15) continue;
            const double localX=cosine*ox+sine*oy+(cosine*dx+sine*dy)*distance,localZ=oz-distance;
            if(localZ>-.3) continue;
            const double fraction=std::clamp((.75*512/std::hypot(localX,localZ))/111,0.,1.);
            for(unsigned c=0;c<3;++c) {
                const int expected=int(std::lround(std::lerp(double(gradient.far[c]),double(gradient.near[c]),fraction)*255));
                if(std::abs(int(pixels[at+c])-expected)>1)
                    throw std::runtime_error("Native ground oracle mismatch: bank="+std::to_string(bank)+" eye="+std::to_string(e)
                        +" pixel="+std::to_string(x)+","+std::to_string(y)+" channel="+std::to_string(c)
                        +" actual="+std::to_string(pixels[at+c])+" expected="+std::to_string(expected)
                        +" source="+std::to_string(baseline[at+c])+" fraction="+std::to_string(fraction));
            }
            ++compared;
        }
        check(render_packets(gpu,scene,std::span(&draw,1),camera)==pixels,"Held native gradient changed across retries");
        const auto restored=render_packets(gpu,scene,std::span(&off,1),camera);
        check(restored==baseline,"Native enhanced ground OFF failed exact source restoration");
    }
    check(compared>100000 && sky>100000,"Native gradient fixture missed ground/sky coverage");
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.fov={-.64F,.72F,.57F,-.52F};
    const auto camera=camera_for(eye,d3d);
    for(unsigned word:{0U,6U,7U}) {
        auto invalid=enhanced;invalid.geometry.texels[render::calibrated_ground_offset+word]=std::bit_cast<uint32_t>(word==7?0.F:std::numeric_limits<float>::infinity());
        render::CalibratedScenePacket bad{&invalid,vr::SceneBlend::opaque,false};
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.upload_packets(gpu.command,std::span(&bad,1)),"Invalid native ground metadata reached GPU encoding");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    enhanced.model=identity;render::CalibratedScenePacket draw{&enhanced,vr::SceneBlend::opaque,false};
    check(!render_packets(gpu,scene,std::span(&draw,1),camera).empty(),"Native ground validation failure did not recover");
    // Reflection misses must see this enhanced native receiver, not an
    // independently captured original floor or a flat screen image.
    constexpr unsigned cube_size=16,cube_bytes=cube_size*cube_size*6*4;
    vr::DrawPacket model;model.geometry.vertices=quad(0,0,-2,.4F,{1,1,1,1});
    draw.reflection_environment=true;
    render::CalibratedScenePacket caster{&model};caster.ray_caster=true;
    const std::array cube_draws{draw,caster};
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload_packets(gpu.command,cube_draws),scene.status().c_str());
    const auto ray=scene.enqueue_ray_geometry(gpu.command,0,8,8,camera,256,cube_size,{0,0,0,1});
    check(ray.complete && ray.environment_face_size==cube_size,scene.status().c_str());
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,cube_bytes,0};
    auto* read=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(read,SDL_GetError());
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUBufferRegion region{static_cast<SDL_GPUBuffer*>(ray.buffer),ray.material_offset+ray.environment_offset,cube_bytes};
    const SDL_GPUTransferBufferLocation target{read,0};SDL_DownloadFromGPUBuffer(copy,&region,&target);SDL_EndGPUCopyPass(copy);
    (void)gpu.finish();
    const auto* cube=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read,false));check(cube,SDL_GetError());
    for(unsigned y=0;y<cube_size/2;++y) for(unsigned x=0;x<cube_size;++x) {
        // The forward cube face uses (u,-v,+1) in the ray convention,
        // equivalent to (u,v,-1) in the native source's +Y-up convention.
        const double u=(x+.5)/cube_size*2-1,v=(y+.5)/cube_size*2-1;
        const double fraction=std::clamp(-v*512/std::hypot(u,1.)/111,0.,1.);
        const auto at=(4*cube_size*cube_size+y*cube_size+x)*4;
        for(unsigned c=0;c<3;++c) {
            const int expected=int(std::lround(std::lerp(double(gradient.far[c]),double(gradient.near[c]),fraction)*255));
            check(std::abs(int(cube[at+c])-expected)<=1,"Reflection cube retained pre-enhanced ground colour");
        }
        check(cube[at+3]==255,"Reflection environment changed ground opacity");
    }
    SDL_UnmapGPUTransferBuffer(gpu.device,read);SDL_ReleaseGPUTransferBuffer(gpu.device,read);
    std::cout<<"Native basic terrain: "<<compared<<" independent ground rays, "<<sky
        <<" protected sky pixels, 2 eyes/3 banks, four-format colour/opacity, enhanced reflection environment, retries/OFF/validation passed.\n";
}
namespace {
#include "check_calibrated_depth.inc"
#include "check_calibrated_aa_scene.inc"
#include "check_calibrated_msaa.inc"
#include "check_calibrated_msaa_guides.inc"
#include "check_calibrated_ground_surface.inc"
#include "check_calibrated_motion.inc"
#include "check_calibrated_ground_motion.inc"
#include "check_calibrated_ray_history.inc"
#include "check_reflected_hit_motion.inc"
#include "profile_reflected_liquids.inc"
}
int main(int argc,char** argv) try {
    const char* backend=argc>1?argv[1]:"direct3d12";check(std::string_view(backend)=="direct3d12" || std::string_view(backend)=="vulkan","Use direct3d12|vulkan");
    const bool responsive=std::find_if(argv+std::min(argc,2),argv+argc,[](const char* arg) {return std::string_view(arg)=="--responsive";})!=argv+argc;
    render::PreparationEvents events{[](void*) noexcept {SDL_PumpEvents();},nullptr};
    render::ScopedPreparationEvents preparation(responsive?&events:nullptr);
    const bool ordered_paths=argc>2 && std::string_view(argv[2])=="--reflection-paths-only";
    const bool scene_paths=argc>2 && std::string_view(argv[2])=="--scene-reflection-paths-only";
    const bool curved_paths=argc>2 && std::string_view(argv[2])=="--curved-reflection-paths-only";
    const bool secondary_lava=argc>2 && std::string_view(argv[2])=="--secondary-lava-only";
    const bool secondary_lava_indexed=argc>2 && std::string_view(argv[2])=="--secondary-lava-indexed-only";
    const bool reflection_history=argc>2 && std::string_view(argv[2])=="--reflection-history-only";
    const bool liquid_profile=argc>2 && std::string_view(argv[2])=="--liquid-profile-only";
    std::array<unsigned,3> profile_dimensions{};
    if(liquid_profile) {
        // Reject malformed diagnostics before creating devices or compiling
        // scene PSOs. This switch is not part of the shipped player CLI.
        check(argc==6,"Native liquid profile requires exactly WIDTH HEIGHT FRAMES");
        check(std::getenv("STARFOX_PROFILE_DXR_TIMINGS")!=nullptr,"Set STARFOX_PROFILE_DXR_TIMINGS for the explicit native timing diagnostic");
        const auto number=[](const char* text) {
            unsigned value{};const std::string_view source=text;
            const auto result=std::from_chars(source.data(),source.data()+source.size(),value);
            check(result.ec==std::errc{} && result.ptr==source.data()+source.size(),"Invalid native liquid profile integer");return value;
        };
        profile_dimensions={number(argv[3]),number(argv[4]),number(argv[5])};
        check(profile_dimensions[0]>=256 && profile_dimensions[0]<=1920
            && profile_dimensions[1]>=192 && profile_dimensions[1]<=1080
            && profile_dimensions[2]>=12 && profile_dimensions[2]<=120,
            "Native timing dimensions/frames outside bounded 256..1920 x 192..1080, 12..120");
    }
    Gpu gpu;gpu.initialize(backend,reflection_history || liquid_profile || ordered_paths || scene_paths || curved_paths || secondary_lava || secondary_lava_indexed);render::GpuCalibratedScene scene;
    const bool initialized=scene.initialize(gpu.device,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);check(initialized,scene.status().c_str());
    if(liquid_profile) {
        profile_reflected_liquids(gpu,scene,std::string_view(backend)=="direct3d12",profile_dimensions[0],profile_dimensions[1],profile_dimensions[2]);
        return 0;
    }
    if(argc>2 && std::string_view(argv[2])=="--ray-history-only") {
        native_ray_history_tests(gpu,scene,std::string_view(backend)=="direct3d12");return 0;
    }
    if(reflection_history) {
        native_reflected_hit_tests(gpu,scene,std::string_view(backend)=="direct3d12");return 0;
    }
    if(ordered_paths) {
        native_reflected_hit_tests(gpu,scene,std::string_view(backend)=="direct3d12",true);return 0;
    }
    if(scene_paths) {
        native_reflected_hit_tests(gpu,scene,std::string_view(backend)=="direct3d12",false,true);return 0;
    }
    if(curved_paths) {
        for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                              SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();gpu.change_color_format(format);
            check(scene.initialize(gpu.device,format),scene.status().c_str());
            native_reflected_hit_tests(gpu,scene,std::string_view(backend)=="direct3d12",false,false,false,false,true);
        }
        return 0;
    }
    if(secondary_lava || secondary_lava_indexed) {
        for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                              SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();gpu.change_color_format(format);
            check(scene.initialize(gpu.device,format),scene.status().c_str());
            native_reflected_hit_tests(gpu,scene,std::string_view(backend)=="direct3d12",false,false,true,secondary_lava_indexed);
        }
        return 0;
    }
    if(argc>2 && std::string_view(argv[2])=="--ground-motion-only") {
        native_ground_motion_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                              SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();gpu.change_color_format(format);
            check(scene.initialize(gpu.device,format),scene.status().c_str());
            native_ground_motion_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        }
        return 0;
    }
    if(argc>2 && std::string_view(argv[2])=="--ground-only") {
        native_ground_gradient_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_ground_surface_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_ground_surface_binding_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                              SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();gpu.change_color_format(format);
            check(scene.initialize(gpu.device,format),scene.status().c_str());
            native_ground_gradient_tests(gpu,scene,std::string_view(backend)=="direct3d12");
            native_ground_surface_tests(gpu,scene,std::string_view(backend)=="direct3d12");
            native_ground_surface_binding_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        }
        return 0;
    }
    if(argc>2 && std::string_view(argv[2])=="--motion-only") {
        native_motion_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                              SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();gpu.change_color_format(format);
            check(scene.initialize(gpu.device,format),scene.status().c_str());
            native_motion_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        }
        return 0;
    }
    if(argc>2 && std::string_view(argv[2])=="--depth-only") {
        native_depth_tests(gpu,scene,std::string_view(backend)=="direct3d12");return 0;
    }
    if(argc>2 && std::string_view(argv[2])=="--msaa-only") {
        native_msaa_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                              SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();gpu.change_color_format(format);
            check(scene.initialize(gpu.device,format),scene.status().c_str());
            native_msaa_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        }
        return 0;
    }
    if(argc>2 && std::string_view(argv[2])=="--msaa-guides-only") {
        native_msaa_guide_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                              SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();gpu.change_color_format(format);
            check(scene.initialize(gpu.device,format),scene.status().c_str());
            native_msaa_guide_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        }
        return 0;
    }
    projection_tests(gpu,scene,std::string_view(backend)=="direct3d12");material_tests(gpu,scene,std::string_view(backend)=="direct3d12");source_model_tests(gpu,scene);
    native_packet_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    connected_grid_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    calibrated_ray_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_ray_history_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    if(std::string_view(backend)=="direct3d12") native_reflected_hit_tests(gpu,scene,true);
    native_line_ray_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_billboard_ray_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_texture_ray_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_environment_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_specular_material_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    palette_tests(gpu,scene,std::string_view(backend)=="direct3d12",false);
    native_ground_gradient_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_ground_surface_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_ground_surface_binding_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_depth_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_aa_ownership_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_msaa_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_msaa_guide_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    native_motion_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
                          SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
        check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();gpu.change_color_format(format);
        const bool initialized_format=scene.initialize(gpu.device,format);check(initialized_format,scene.status().c_str());
        calibrated_ray_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_line_ray_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_billboard_ray_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_texture_ray_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_environment_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_specular_material_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        palette_tests(gpu,scene,std::string_view(backend)=="direct3d12",format!=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
        native_ground_gradient_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_ground_surface_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_ground_surface_binding_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_depth_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_aa_ownership_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_msaa_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_msaa_guide_tests(gpu,scene,std::string_view(backend)=="direct3d12");
        native_motion_tests(gpu,scene,std::string_view(backend)=="direct3d12");
    }
    check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());scene.release_device();
    if(responsive) {
        check(events.jobs>0 && events.pumps>=events.jobs,"No responsive shader compilation exercised");
        std::cout<<"Responsive GPU preparation: "<<events.jobs<<" joined jobs; "<<events.pumps<<" event pumps\n";
    }
    std::cout<<"Calibrated scene "<<backend<<": analytic XR pose/FOV, head tracking, models/destruction, native blends/textures/dither, BG2/OBJ GPU decoding, protected HUD, complete tracked wipe, immutable photo reuse/live palette/cancellation, GPU connected-ground projection/binning (72 calibrated source/eye comparisons), empty cached textures and malformed-payload recovery passed. Fixture, not physical Leia validation.\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
