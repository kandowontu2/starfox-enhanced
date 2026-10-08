// Actual VulkanHardwareRt/pinned SDL bridge submission, not a separate shader
// dispatcher. Checks shadow and reflection material binding. Windows probes
// do NOT prove the Linux ABI, Deck or stage lifetime.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/vulkan_hardware_rt.hpp"
#include "native_ray_owner_fixture.hpp"
#include "starfox/render/vulkan_ray_support.hpp"
#include "starfox/render/sdl_vulkan_bridge.h"
#include "native_shadow_cutout_fixture.hpp"
#include "native_rgba_shadow_fixture.hpp"
#include "native_rgba_reflection_fixture.hpp"
#include "native_water_fixture.hpp"
#include "native_primary_range_fixture.hpp"
#include "native_environment_cube_fixture.hpp"
#include "native_ground_fixture.hpp"
#include "native_model_fixture.hpp"
#include "native_empty_fixture.hpp"
#include <SDL3/SDL.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Device {
    SDL_GPUDevice* device{};
    ~Device(){if(device)SDL_DestroyGPUDevice(device);SDL_Quit();}
};
struct Resources {
    SDL_GPUDevice* device{};SDL_GPUBuffer* geometry{};SDL_GPUTransferBuffer* upload{};SDL_GPUTransferBuffer* download{};
    ~Resources(){if(geometry)SDL_ReleaseGPUBuffer(device,geometry);if(upload)SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download)SDL_ReleaseGPUTransferBuffer(device,download);}
};
void wait(SDL_GPUDevice* device,SDL_GPUFence* fence) {
    require(fence!=nullptr,SDL_GetError());
    const bool completed=SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);
    require(completed,SDL_GetError());
}
}
int main(int argc,char** argv) try {
    bool integrated=false,water_only=false,environment_only=false,ground_only=false,model_only=false,empty_only=false;
    for(int i=1;i<argc;++i) {
        const std::string argument=argv[i];
        if(argument=="--integrated" && !integrated)integrated=true;
        else if(argument=="--native-water-only" && !water_only)water_only=true;
        else if(argument=="--native-environment-only" && !environment_only)environment_only=true;
        else if(argument=="--native-ground-only" && !ground_only)ground_only=true;
        else if(argument=="--native-model-only" && !model_only)model_only=true;
        else if(argument=="--native-empty-only" && !empty_only)empty_only=true;
        else require(false,"Usage: starfox_vulkan_shadow_owner_check [--integrated] [--native-water-only | --native-environment-only | --native-ground-only | --native-model-only | --native-empty-only]");
    }
    require(!water_only || !environment_only,"Choose water or environment fixture");
    require(!ground_only || (!water_only && !environment_only),"Choose native ground fixture alone");
    require(!model_only || (!water_only && !environment_only && !ground_only),"Choose native model fixture alone");
    require(!empty_only || (!water_only && !environment_only && !ground_only && !model_only),"Choose native empty fixture alone");
    Device owner;require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    const auto props=SDL_CreateProperties();require(props!=0,SDL_GetError());
    SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan");
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,integrated);
    require(starfox::render::shadows::request_vulkan_ray_query(props),"Could not request Vulkan ray-query features");
    owner.device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);
    require(owner.device!=nullptr,SDL_GetError());
    SDL_SetBooleanProperty(SDL_GetGPUDeviceProperties(owner.device),"starfox.vulkan.ray_query.enabled",true);
    const auto support=starfox::render::shadows::query_vulkan_ray_query(owner.device);
    if(!support.available){std::cerr<<support.status<<"; SKIP, not acceptance\n";return 2;}
    const auto* bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(
        SDL_GetGPUDeviceProperties(owner.device),STARFOX_SDL_VULKAN_BRIDGE,nullptr));
    require(bridge!=nullptr,"Native Vulkan device bridge missing");
    const auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceProperties"));
    require(properties!=nullptr,"Missing physical-device properties");VkPhysicalDeviceProperties device_properties{};
    properties(bridge->physical_device,&device_properties);
    std::cout<<"Actual SDL Vulkan shadow owner on "<<device_properties.deviceName<<'\n'<<std::flush;
    if(integrated && device_properties.deviceType!=VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU){std::cerr<<"Requested integrated GPU was not selected; SKIP\n";return 2;}
    auto fixture=native_reflection_fixture::make_fixture();
    constexpr unsigned payload_bytes=sizeof(fixture.vertices)+native_reflection_fixture::native_shadow_word_capacity*4;
    constexpr unsigned image_bytes=native_reflection_fixture::width*native_reflection_fixture::height*4;
    Resources resources;resources.device=owner.device;
    SDL_GPUBufferCreateInfo geometry_info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,payload_bytes,0};
    resources.geometry=SDL_CreateGPUBuffer(owner.device,&geometry_info);
    SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,payload_bytes,0};
    SDL_GPUTransferBufferCreateInfo download_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,image_bytes*6,0};
    resources.upload=SDL_CreateGPUTransferBuffer(owner.device,&upload_info);resources.download=SDL_CreateGPUTransferBuffer(owner.device,&download_info);
    require(resources.geometry && resources.upload && resources.download,SDL_GetError());
    starfox::render::shadows::NativeRayOwner rays;unsigned compositions=0;
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
    std::cout<<"Routing through the live calibrated SDL ray adapter (native Vulkan)\n"<<std::flush;
#endif
    if(environment_only || ground_only || model_only || empty_only) {
        using namespace starfox::render;using namespace starfox::render::shadows;
        Resources cube_resources;cube_resources.device=owner.device;
        constexpr unsigned cube_capacity=sizeof(fixture.vertices)+native_reflection_fixture::native_shadow_word_capacity*4+512U*512*6*4;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,cube_capacity,0};
        cube_resources.geometry=SDL_CreateGPUBuffer(owner.device,&info);
        SDL_GPUTransferBufferCreateInfo up{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,cube_capacity,0},down{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,image_bytes*6,0};
        cube_resources.upload=SDL_CreateGPUTransferBuffer(owner.device,&up);cube_resources.download=SDL_CreateGPUTransferBuffer(owner.device,&down);
        require(cube_resources.geometry && cube_resources.upload && cube_resources.download,SDL_GetError());
        const auto cube_dispatch=[&](const auto& p,const auto& input) {
            const unsigned vertices=input.source.empty_geometry?0:12;
            const unsigned vertex_bytes=vertices?sizeof(input.source.source.geometry.vertices):16,material_bytes=unsigned(input.source.source.words.size()*4),cube_bytes=unsigned(input.cube.size()*4);
            const unsigned used=vertex_bytes+material_bytes+cube_bytes;
            auto* upload=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(owner.device,cube_resources.upload,false));require(upload,SDL_GetError());
            if(vertices)std::memcpy(upload,input.source.source.geometry.vertices.data(),vertex_bytes);
            else std::memset(upload,0xa5,vertex_bytes);
            std::memcpy(upload+vertex_bytes,input.source.source.words.data(),material_bytes);
            std::memcpy(upload+vertex_bytes+material_bytes,input.cube.data(),cube_bytes);SDL_UnmapGPUTransferBuffer(owner.device,cube_resources.upload);
            auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command,SDL_GetError());auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
            SDL_GPUTransferBufferLocation from{cube_resources.upload,0};SDL_GPUBufferRegion to{cube_resources.geometry,0,used};
            SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
            RayMaterials materials;materials.encoding=RayMaterialEncoding::native_rgba;
            const GpuScene::RayGeometryOutput geometry{owner.device,cube_resources.geometry,vertices,true,&materials,vertex_bytes,material_bytes};
            Camera camera{64,48,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
            ResidentEnvironmentCube cube{material_bytes,input.size,{p.cube_row0[0],p.cube_row0[1],p.cube_row0[2],p.cube_row1[0],p.cube_row1[1],p.cube_row1[2],p.cube_row2[0],p.cube_row2[1],p.cube_row2[2]}};
            std::optional<ReceiverPlane> plane;if(p.settings[1])plane={{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
            RayWater water;water.time=p.water[0];water.reflection_strength=p.water[1];water.brightness=p.water[2];water.caustics=unsigned(p.water[3])>>5&3;
            water.material=unsigned(p.water[3])&15;water.mirror_models=unsigned(p.water[3])&16;
            if(p.source_colour[3])water.source_colour=std::array{p.source_colour[0],p.source_colour[1],p.source_colour[2]};
            water.world_to_view={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};water.camera_position={p.row0[3],p.row1[3],p.row2[3]};
            water.auxiliary_layers=p.liquid_layers[2]&1;water.surface_layers=p.liquid_layers[2]&2;
            std::array<unsigned,256> palette{};
            std::optional<PrimaryRayRange> range;if(p.primary_range[2])range=PrimaryRayRange{p.primary_range[0],p.primary_range[1]};
            if(!rays.render_reflections(owner.device,geometry,camera,palette,p.environment[0],p.dimensions[2],p.settings[0],p.dimensions[3],plane,nullptr,plane?&water:nullptr,p.settings[3]!=0,p.material_info[2],range,p.cube_info[3]?&cube:nullptr,p.material_info[3]!=0))throw std::runtime_error(rays.status());
            const auto output=rays.reflection_output();require(output.buffer && output.device==owner.device,"Missing native environment output");
            const unsigned bytes=p.liquid_layers[2]?output.water_layers.storage_bytes:image_bytes;require(bytes==(p.liquid_layers[2]?p.liquid_layers[3]*4:image_bytes),"Environment changed canonical water storage");
            command=SDL_AcquireGPUCommandBuffer(owner.device);require(command,SDL_GetError());copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
            SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(output.buffer),0,bytes};SDL_GPUTransferBufferLocation target{cube_resources.download,0};
            SDL_DownloadFromGPUBuffer(copy,&source,&target);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
            const auto* pixels=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(owner.device,cube_resources.download,false));require(pixels,SDL_GetError());
            std::vector<unsigned> result(pixels,pixels+bytes/4);SDL_UnmapGPUTransferBuffer(owner.device,cube_resources.download);return result;
        };
        if(empty_only) {
            native_reflection_fixture::run_native_empty(cube_dispatch);
            native_reflection_fixture::run_native_empty_transitions(cube_dispatch);
            native_reflection_fixture::run_native_empty_shadows([&](const auto& p,const auto& input) {
                auto ready=native_reflection_fixture::cube_parameters(input,1,0,0);ready.cube_info={};cube_dispatch(ready,input);
                RayMaterials materials;materials.encoding=RayMaterialEncoding::native_rgba;
                const GpuScene::RayGeometryOutput geometry{owner.device,cube_resources.geometry,0,true,&materials,16,16};
                Camera camera{64,48,p.extent[2],p.center[0],p.center[1],p.extent[3]};
                camera.shadow_softness=p.lights[0][3]==1?0:2;camera.quality=p.lights[0][3]==16?3:p.lights[0][3]==8?2:1;
                std::optional<ReceiverPlane> plane;if(p.center[2])plane={{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
                std::optional<PrimaryRayRange> range;if(p.primary_range[2])range=PrimaryRayRange{p.primary_range[0],p.primary_range[1]};
                const Scene empty;
                if(!rays.render_shadows(owner.device,empty,camera,{.34,-.25,-1},plane,p.coverage[2]?&geometry:nullptr,p.center[3]!=0,range))throw std::runtime_error(rays.status());
                const auto output=rays.shadow_output();require(output.buffer && output.device==owner.device,"Missing empty shadow output");
                auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command,SDL_GetError());auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
                SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(output.buffer),0,image_bytes};SDL_GPUTransferBufferLocation target{cube_resources.download,0};
                SDL_DownloadFromGPUBuffer(copy,&source,&target);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
                const auto* pixels=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(owner.device,cube_resources.download,false));require(pixels,SDL_GetError());
                std::vector<unsigned> result(pixels,pixels+image_bytes/4);SDL_UnmapGPUTransferBuffer(owner.device,cube_resources.download);return result;
            });
            const auto input=native_reflection_fixture::native_empty_input(2,true);
            auto p=native_reflection_fixture::cube_parameters(input,2,0,6);p.material_info[3]=1;
            const auto restore=[&]{cube_dispatch(p,input);require(rays.reflection_output().buffer,"Empty native recovery failed");};
            restore();RayMaterials materials;materials.encoding=RayMaterialEncoding::native_rgba;
            const GpuScene::RayGeometryOutput valid{owner.device,cube_resources.geometry,0,true,&materials,16,16};
            const Camera camera{64,48,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};std::array<unsigned,256> palette{};
            const ResidentEnvironmentCube cube{16,16};const Scene empty;
            unsigned declined=0;
            for(unsigned bad=0;bad<10;++bad) {
                auto invalid=valid;
                if(bad<4)invalid.material_offset=std::array{0U,4U,15U,20U}[bad];
                if(bad>=4 && bad<7)invalid.material_bytes=std::array{0U,4U,12U}[bad-4];
                if(bad==7)invalid.complete=false;if(bad==8)invalid.buffer=nullptr;
                if(bad==9)materials.triangles.resize(1);
                require(!rays.render_reflections(owner.device,invalid,camera,palette,p.environment[0],1,0,0,{},nullptr,nullptr,false,2,{},&cube)
                    && !rays.reflection_output().buffer,"Malformed empty reflection retained output");
                require(!rays.render_shadows(owner.device,empty,camera,{0,0,-1},{},&invalid)
                    && !rays.shadow_output().buffer,"Malformed empty shadow retained output");
                materials.triangles.clear();++declined;restore();
            }
            restore();rays.release_device();restore();rays.release_device();
            require(!rays.reflection_output().buffer && !rays.shadow_output().buffer,"Empty release retained output");
            std::cout<<"Native empty owner: "<<declined<<" malformed padded-header/complete/buffer/topology requests declined by both producers, outputs cleared, recovery and release/reinitialize passed; NOT Linux/application/FPS acceptance\n"<<std::flush;
            return 0;
        }
        if(model_only) {
            native_reflection_fixture::run_native_models(cube_dispatch);
            const auto input=native_reflection_fixture::cube_input(16,3,2);
            auto p=native_reflection_fixture::cube_parameters(input,2,0,0);p.material_info[3]=1;
            const auto restore=[&]{cube_dispatch(p,input);require(rays.reflection_output().buffer,"Native model recovery failed");};
            restore();
            RayMaterials materials;materials.encoding=RayMaterialEncoding::native_rgba;
            const unsigned vertex_bytes=sizeof(input.source.source.geometry.vertices),material_bytes=unsigned(input.source.source.words.size()*4);
            const GpuScene::RayGeometryOutput geometry{owner.device,cube_resources.geometry,12,true,&materials,vertex_bytes,material_bytes};
            const Camera camera{64,48,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};std::array<unsigned,256> palette{};
            unsigned declined=0;
            for(unsigned bad=0;bad<3;++bad) {
                materials.encoding=bad==1?RayMaterialEncoding::indexed:RayMaterialEncoding::native_rgba;
                const unsigned encoding=bad==0?0:bad==1?2:3;
                require(!rays.render_reflections(owner.device,geometry,camera,palette,p.environment[0],1,0,1,{},nullptr,nullptr,false,encoding,{},nullptr,true)
                    && !rays.reflection_output().buffer,"Invalid native model encoding retained output");
                materials.encoding=RayMaterialEncoding::native_rgba;++declined;restore();
            }
            cube_dispatch(p,input);rays.release_device();cube_dispatch(p,input);rays.release_device();
            require(!rays.reflection_output().buffer,"Native model release retained borrowed output");
            std::cout<<"Native model owner: "<<declined<<" invalid specular encodings declined, outputs cleared, valid recovery and release/reinitialize passed; Windows owner checks are NOT Linux/application/FPS acceptance\n"<<std::flush;
            return 0;
        }
        if(ground_only) {
            native_reflection_fixture::run_native_ground(cube_dispatch);
            const auto input=native_reflection_fixture::cube_input(16,3,2);
            auto p=native_reflection_fixture::cube_parameters(input,2,0,0);
            p.settings={0,1,0,1};p.point={0,180,0,0};p.normal={0,-1,0,0};p.water={1.3f,.7f,.72f,1};
            p.row0={1,0,0,29};p.row1={0,1,0,-3};p.row2={0,0,1,17};
            const auto restore=[&]{cube_dispatch(p,input);require(rays.reflection_output().buffer,"Valid native ground failed to recover");};
            RayMaterials materials;materials.encoding=RayMaterialEncoding::native_rgba;
            const unsigned vertex_bytes=sizeof(input.source.source.geometry.vertices),material_bytes=unsigned(input.source.source.words.size()*4);
            const GpuScene::RayGeometryOutput geometry{owner.device,cube_resources.geometry,12,true,&materials,vertex_bytes,material_bytes};
            const Camera camera{64,48,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
            const ReceiverPlane plane{{0,180,0},{0,-1,0}};const ResidentEnvironmentCube cube{material_bytes,16};
            std::array<unsigned,256> palette{};unsigned declined=0;
            for(unsigned material:{1U,2U,3U})for(unsigned encoding:{1U,2U})for(unsigned bad=0;bad<5;++bad) {
                p.water[3]=float(material);p.material_info[2]=p.cube_info[2]=encoding;restore();
                RayWater water;water.material=material;water.time=1.3f;water.reflection_strength=.7f;water.brightness=.72f;
                if(bad==0)water.world_to_view[0]=std::numeric_limits<float>::quiet_NaN();
                if(bad==1)water.world_to_view[4]=std::numeric_limits<float>::infinity();
                if(bad==2)water.world_to_view={};
                if(bad==3)water.world_to_view={1,0,0,1,0,0,0,0,1};
                if(bad==4)water.world_to_view={1,0,0,1,1.e-10f,0,0,0,1};
                require(!rays.render_reflections(owner.device,geometry,camera,palette,p.environment[0],1,0,0,plane,nullptr,&water,true,encoding,{},&cube)
                    && !rays.reflection_output().buffer && rays.reflection_output().water_layers==NativeWaterLayers{},"Invalid native ground transform retained borrowed output");
                ++declined;restore();
            }
            restore();rays.release_device();restore();rays.release_device();
            require(!rays.reflection_output().buffer,"Native ground release retained borrowed output");
            std::cout<<"Native ground owner: "<<declined<<" non-finite/singular transforms declined, output cleared, valid recovery and release/reinitialize passed; Windows owner checks are NOT Linux/application/FPS acceptance\n"<<std::flush;
            return 0;
        }
        native_reflection_fixture::run_native_environment(cube_dispatch);
        native_reflection_fixture::run_native_water_environment(cube_dispatch);
        const auto input=native_reflection_fixture::cube_input(16,3,2);
        const auto p=native_reflection_fixture::cube_parameters(input,2,0,0);
        const auto restore=[&]{cube_dispatch(p,input);require(rays.reflection_output().buffer,"Valid cube request failed to recover");};
        const unsigned vertex_bytes=sizeof(input.source.source.geometry.vertices),material_bytes=unsigned(input.source.source.words.size()*4);
        RayMaterials materials;materials.encoding=RayMaterialEncoding::native_rgba;
        const GpuScene::RayGeometryOutput geometry{owner.device,cube_resources.geometry,12,true,&materials,vertex_bytes,material_bytes};
        const Camera camera{64,48,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
        const ResidentEnvironmentCube valid{material_bytes,16};std::array<unsigned,256> palette{};
        unsigned declined=0;
        const auto reject=[&](const auto& g,const auto& cube,unsigned encoding,const GpuBackgroundDraw* bg=nullptr) {
            restore();require(!rays.render_reflections(owner.device,g,camera,palette,p.environment[0],1,0,0,{},bg,nullptr,false,encoding,{},&cube)
                && !rays.reflection_output().buffer && rays.reflection_output().water_layers==NativeWaterLayers{},"Malformed resident environment exposed stale output");++declined;
        };
        for(unsigned size:{0U,4U,9U,513U,1024U,0xffffffffU}) {auto cube=valid;cube.face_size=size;reject(geometry,cube,2);}
        for(unsigned offset:{0U,material_bytes-4,material_bytes+4,material_bytes+1,0xfffffffcu}) {auto cube=valid;cube.relative_offset=offset;reject(geometry,cube,2);}
        for(unsigned mode=0;mode<4;++mode) {
            auto cube=valid;
            if(mode==0)cube.rotation[0]=std::numeric_limits<float>::quiet_NaN();
            if(mode==1)cube.rotation[0]=std::numeric_limits<float>::infinity();
            if(mode==2)cube.rotation[0]=0;
            if(mode==3)cube.rotation[3]=.3f;
            reject(geometry,cube,2);
        }
        reject(geometry,valid,0);reject(geometry,valid,3);
        GpuBackgroundDraw bg{};reject(geometry,valid,2,&bg);
        RayMaterials indexed;auto bad=geometry;bad.materials=&indexed;reject(bad,valid,2);
        bad=geometry;bad.material_offset=0xfffffffcu;reject(bad,valid,2);
        const unsigned used=vertex_bytes+material_bytes+unsigned(input.cube.size()*4);
        SDL_GPUBufferCreateInfo short_info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,used-4,0};
        auto* shortened=SDL_CreateGPUBuffer(owner.device,&short_info);require(shortened,SDL_GetError());
        // Reuse the actual valid upload, truncating only the final cube texel.
        restore();auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command,SDL_GetError());auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        SDL_GPUTransferBufferLocation source{cube_resources.upload,0};SDL_GPUBufferRegion target{shortened,0,used-4};
        SDL_UploadToGPUBuffer(copy,&source,&target,false);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        bad=geometry;bad.buffer=shortened;reject(bad,valid,2);SDL_ReleaseGPUBuffer(owner.device,shortened);
        restore();rays.release_device();restore();
        rays.release_device();require(!rays.reflection_output().buffer && rays.reflection_output().water_layers==NativeWaterLayers{},"Cube release retained borrowed output");
        std::cout<<"Resident environment owner: "<<declined<<" invalid metadata/rotation/encoding/source-range requests declined, valid recovery and release/reinitialize passed; GPU-resident cube, Windows check is NOT Linux/application/FPS acceptance\n"<<std::flush;
        return 0;
    }
    unsigned water_submissions=0;
    native_reflection_fixture::Parameters last_water{};
    const auto water_dispatch=[&](const auto& p,const auto& input) {
        auto* upload=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(owner.device,resources.upload,false));require(upload,SDL_GetError());
        std::memset(upload,0,payload_bytes);std::memcpy(upload,input.source.geometry.vertices.data(),sizeof(input.source.geometry.vertices));
        std::memcpy(upload+sizeof(input.source.geometry.vertices),input.source.words.data(),input.source.words.size()*4);
        SDL_UnmapGPUTransferBuffer(owner.device,resources.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command,SDL_GetError());auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        SDL_GPUTransferBufferLocation from{resources.upload,0};SDL_GPUBufferRegion to{resources.geometry,0,payload_bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        starfox::render::RayMaterials materials;materials.encoding=starfox::render::RayMaterialEncoding::native_rgba;
        const starfox::render::GpuScene::RayGeometryOutput geometry{owner.device,resources.geometry,12,true,&materials,
            unsigned(sizeof(input.source.geometry.vertices)),unsigned(input.source.words.size()*4)};
        const starfox::render::shadows::Camera camera{native_reflection_fixture::width,native_reflection_fixture::height,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
        const starfox::render::shadows::ReceiverPlane plane{{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
        starfox::render::shadows::RayWater water;water.time=p.water[0];water.reflection_strength=p.water[1];water.brightness=p.water[2];
        water.caustics=(unsigned(p.water[3])>>5)&3;water.source_colour=std::array{p.source_colour[0],p.source_colour[1],p.source_colour[2]};
        water.world_to_view={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};
        water.camera_position={p.row0[3],p.row1[3],p.row2[3]};water.auxiliary_layers=(p.liquid_layers[2]&1)!=0;water.surface_layers=(p.liquid_layers[2]&2)!=0;
        std::array<unsigned,256> palette{};
        const auto range=p.primary_range[2]!=0?std::optional<starfox::render::shadows::PrimaryRayRange>{{p.primary_range[0],p.primary_range[1]}}:std::nullopt;
        if(!rays.render_reflections(owner.device,geometry,camera,palette,p.environment[0],1,0,0,plane,nullptr,&water,p.settings[3]!=0,p.material_info[2],range))throw std::runtime_error(rays.status());
        const auto output=rays.reflection_output();require(output.buffer && output.device==owner.device && output.row_bytes==camera.width*4,"Invalid water owner output");
        const auto expected_layers=p.liquid_layers[2]?*starfox::render::shadows::native_water_layers(camera.width,camera.height,water.auxiliary_layers):starfox::render::shadows::NativeWaterLayers{};
        require(output.water_layers==expected_layers,"Water owner layer offsets/extent changed");
        const unsigned bytes=output.water_layers.storage_bytes?output.water_layers.storage_bytes:image_bytes;
        command=SDL_AcquireGPUCommandBuffer(owner.device);require(command,SDL_GetError());copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(output.buffer),0,bytes};SDL_GPUTransferBufferLocation target{resources.download,0};
        SDL_DownloadFromGPUBuffer(copy,&source,&target);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        const auto* pixels=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(owner.device,resources.download,false));require(pixels,SDL_GetError());
        std::vector<unsigned> result(pixels,pixels+bytes/4);SDL_UnmapGPUTransferBuffer(owner.device,resources.download);++water_submissions;last_water=p;return result;
    };
    native_reflection_fixture::run_native_water(water_dispatch);
    native_reflection_fixture::run_native_water_ranges(water_dispatch);
    unsigned water_declines=0;
    {
        using namespace starfox::render;using namespace starfox::render::shadows;
        const auto& p=last_water;
        RayMaterials materials;materials.encoding=RayMaterialEncoding::native_rgba;
        const GpuScene::RayGeometryOutput geometry{owner.device,resources.geometry,12,true,&materials,
            unsigned(sizeof(fixture.vertices)),p.material_info[1]};
        const Camera camera{64,48,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
        const ReceiverPlane plane{{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
        RayWater water;water.time=p.water[0];water.reflection_strength=p.water[1];water.brightness=p.water[2];water.caustics=3;
        water.source_colour=std::array{.2f,.3f,.4f};water.auxiliary_layers=water.surface_layers=true;
        water.world_to_view={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};
        water.camera_position={p.row0[3],p.row1[3],p.row2[3]};
        const std::array<unsigned,256> palette{};
        const auto restore=[&] {
            if(!rays.render_reflections(owner.device,geometry,camera,palette,p.environment[0],1,0,0,plane,nullptr,&water,false,2))throw std::runtime_error(rays.status());
            require(rays.reflection_output().water_layers==*native_water_layers(64,48),"Water recovery lost full resident layers");
        };
        for(unsigned mode=0;mode<18;++mode) {
            restore();auto invalid=water;auto bad_camera=camera;auto bad_plane=std::optional(plane);auto bad_geometry=geometry;
            unsigned encoding=2;auto bad_materials=materials;
            if(mode==0)encoding=0;
            if(mode==1)encoding=3;
            if(mode==2)invalid.material=1;
            if(mode==3)bad_plane.reset();
            if(mode==4)(*invalid.source_colour)[0]=-.01f;
            if(mode==5)(*invalid.source_colour)[1]=1.01f;
            if(mode==6)(*invalid.source_colour)[2]=std::numeric_limits<float>::quiet_NaN();
            if(mode==7)invalid.world_to_view.fill(0);
            if(mode==8)invalid.world_to_view[1]=std::numeric_limits<float>::infinity();
            if(mode==9)invalid.camera_position[2]=std::numeric_limits<float>::infinity();
            if(mode==10)invalid.source_colour.reset();
            if(mode==11) {bad_materials.encoding=RayMaterialEncoding::indexed;bad_geometry.materials=&bad_materials;}
            if(mode==12)bad_camera.width=32768;
            if(mode==13)bad_camera.focal_length=std::numeric_limits<double>::quiet_NaN();
            if(mode==14)bad_camera.center_x=std::numeric_limits<double>::infinity();
            if(mode==15)bad_camera.height=0;
            if(mode==16)bad_plane->normal={0,0,0};
            if(mode==17)bad_plane->point.x=std::numeric_limits<double>::quiet_NaN();
            require(!rays.render_reflections(owner.device,bad_geometry,bad_camera,palette,p.environment[0],1,0,0,bad_plane,nullptr,&invalid,false,encoding),"Malformed native water request accepted");
            const auto output=rays.reflection_output();require(!output.device && !output.buffer && !output.width && !output.height && !output.row_bytes
                && output.water_layers==NativeWaterLayers{},"Malformed water request exposed stale colour/layer output");++water_declines;
        }
        restore();rays.release_device();const auto released=rays.reflection_output();require(!released.buffer && released.water_layers==NativeWaterLayers{},"Water release retained resident layers");
        restore();rays.release_device();require(!rays.reflection_output().buffer && rays.reflection_output().water_layers==NativeWaterLayers{},"Release/reinitialize retained native water output");
        std::cout<<"Native water owner: "<<water_declines<<" malformed colour/transform/layer requests declined, restored, release/reinitialize and output clearing passed\n"<<std::flush;
    }
    if(water_only)return 0;
    for(bool gpu_records:{false,true}) {
        const auto shadow_dispatch=[&](const auto& p,const auto& cutout,const auto& texels) {
            auto* upload=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(owner.device,resources.upload,false));require(upload!=nullptr,SDL_GetError());
            std::memset(upload,0,payload_bytes);
            std::memcpy(upload,cutout.vertices.data(),sizeof(cutout.vertices));
            std::memcpy(upload+sizeof(cutout.vertices),cutout.materials.data(),sizeof(cutout.materials));
            SDL_UnmapGPUTransferBuffer(owner.device,resources.upload);
            auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
            SDL_GPUTransferBufferLocation from{resources.upload,0};SDL_GPUBufferRegion to{resources.geometry,0,payload_bytes};
            SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
            starfox::render::RayMaterials materials;materials.triangles.assign(cutout.materials.size(),{});
            // In GPU-offset mode, poison the CPU records with opaque solids.
            // Only a real GPU range copy can produce the expected cutout mask.
            if(!gpu_records)std::memcpy(materials.triangles.data(),cutout.materials.data(),sizeof(cutout.materials));
            materials.texels.assign(texels.begin(),texels.end());
            const starfox::render::GpuScene::RayGeometryOutput geometry{owner.device,resources.geometry,
                unsigned(cutout.vertices.size()),true,p.coverage[2]?&materials:nullptr,gpu_records?unsigned(sizeof(cutout.vertices)):0};
            starfox::render::shadows::Camera camera{native_reflection_fixture::width,native_reflection_fixture::height,p.extent[2],p.center[0],p.center[1],p.extent[3]};
            const unsigned samples=unsigned(p.lights[0][3]);camera.quality=samples==4?1:samples==16?3:2;camera.shadow_softness=samples==1?0:2;
            const starfox::render::shadows::ReceiverPlane plane{{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
            const starfox::render::shadows::Scene empty;
            const auto range=p.primary_range[2]!=0?std::optional<starfox::render::shadows::PrimaryRayRange>{{p.primary_range[0],p.primary_range[1]}}:std::nullopt;
            if(!rays.render_shadows(owner.device,empty,camera,{.34,-.25,-1},plane,&geometry,p.center[3]!=0,range))
                throw std::runtime_error(rays.status());
            const auto output=rays.shadow_output();require(output.buffer && output.width==camera.width && output.height==camera.height,"Stale/missing Vulkan shadow output");
            command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
            SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(output.buffer),0,image_bytes};SDL_GPUTransferBufferLocation destination{resources.download,0};
            SDL_DownloadFromGPUBuffer(copy,&source,&destination);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
            const auto* pixels=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(owner.device,resources.download,false));require(pixels!=nullptr,SDL_GetError());
            std::vector<unsigned> result(pixels,pixels+camera.width*camera.height);SDL_UnmapGPUTransferBuffer(owner.device,resources.download);
            ++compositions;if(compositions%80==0)std::cout<<"  completed "<<compositions<<" actual owner submissions\n"<<std::flush;
            return result;
        };
        native_reflection_fixture::run_shadow_cutouts(fixture,shadow_dispatch);
        native_reflection_fixture::run_primary_ranges<true>(shadow_dispatch);
        std::cout<<(gpu_records?"GPU-offset records (CPU poison ignored)":"CPU-record upload")<<" owner masks passed\n"<<std::flush;
    }
    native_reflection_fixture::run_native_rgba_shadows([&](const auto& p,const auto& source) {
        const unsigned bytes=unsigned(source.words.size()*4),used=unsigned(sizeof(source.geometry.vertices))+bytes;
        auto* upload=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(owner.device,resources.upload,false));require(upload!=nullptr,SDL_GetError());
        std::memcpy(upload,source.geometry.vertices.data(),sizeof(source.geometry.vertices));
        std::memcpy(upload+sizeof(source.geometry.vertices),source.words.data(),bytes);SDL_UnmapGPUTransferBuffer(owner.device,resources.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
        SDL_GPUTransferBufferLocation from{resources.upload,0};SDL_GPUBufferRegion to{resources.geometry,0,used};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        starfox::render::RayMaterials materials;materials.encoding=starfox::render::RayMaterialEncoding::native_rgba;
        const starfox::render::GpuScene::RayGeometryOutput geometry{owner.device,resources.geometry,12,true,&materials,
            unsigned(sizeof(source.geometry.vertices)),bytes};
        starfox::render::shadows::Camera camera{native_reflection_fixture::width,native_reflection_fixture::height,p.extent[2],p.center[0],p.center[1],p.extent[3]};
        const unsigned samples=unsigned(p.lights[0][3]);camera.quality=samples==4?1:samples==16?3:2;camera.shadow_softness=samples==1?0:2;
        const starfox::render::shadows::ReceiverPlane plane{{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
        const starfox::render::shadows::Scene empty;
        if(!rays.render_shadows(owner.device,empty,camera,{.34,-.25,-1},plane,&geometry,p.center[3]!=0))
            throw std::runtime_error(rays.status());
        const auto output=rays.shadow_output();require(output.buffer && output.width==camera.width && output.height==camera.height,"Missing native RGBA shadow output");
        command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
        SDL_GPUBufferRegion image{static_cast<SDL_GPUBuffer*>(output.buffer),0,image_bytes};SDL_GPUTransferBufferLocation destination{resources.download,0};
        SDL_DownloadFromGPUBuffer(copy,&image,&destination);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        const auto* pixels=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(owner.device,resources.download,false));require(pixels!=nullptr,SDL_GetError());
        std::vector<unsigned> result(pixels,pixels+camera.width*camera.height);SDL_UnmapGPUTransferBuffer(owner.device,resources.download);
        ++compositions;if(compositions%80==0)std::cout<<"  completed "<<compositions<<" actual owner submissions\n"<<std::flush;
        return result;
    });
    unsigned reflections=0;
    native_reflection_fixture::run_native_rgba_reflections([&](const auto& p,const auto& source) {
        const auto& input=source.source;const unsigned bytes=unsigned(input.words.size()*4);
        auto* upload=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(owner.device,resources.upload,false));require(upload!=nullptr,SDL_GetError());
        std::memcpy(upload,input.geometry.vertices.data(),sizeof(input.geometry.vertices));
        std::memcpy(upload+sizeof(input.geometry.vertices),input.words.data(),bytes);SDL_UnmapGPUTransferBuffer(owner.device,resources.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
        SDL_GPUTransferBufferLocation from{resources.upload,0};SDL_GPUBufferRegion to{resources.geometry,0,unsigned(sizeof(input.geometry.vertices))+bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        starfox::render::RayMaterials materials;materials.encoding=starfox::render::RayMaterialEncoding::native_rgba;
        const starfox::render::GpuScene::RayGeometryOutput geometry{owner.device,resources.geometry,12,true,&materials,
            unsigned(sizeof(input.geometry.vertices)),bytes};
        const starfox::render::shadows::Camera camera{native_reflection_fixture::width,native_reflection_fixture::height,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
        std::optional<starfox::render::shadows::ReceiverPlane> plane;
        if(p.settings[1]!=0)plane={{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
        starfox::render::shadows::RayWater water;water.time=p.water[0];water.reflection_strength=p.water[1];water.brightness=p.water[2];
        water.material=std::uint8_t(unsigned(p.water[3])&15U);water.mirror_models=(unsigned(p.water[3])&16U)!=0;
        std::array<unsigned,256> unused_palette{};
        if(!rays.render_reflections(owner.device,geometry,camera,unused_palette,p.environment[0],1,0,0,
            plane,nullptr,plane || water.mirror_models?&water:nullptr,p.settings[3]!=0))throw std::runtime_error(rays.status());
        const auto output=rays.reflection_output();require(output.buffer && output.width==camera.width && output.height==camera.height,"Missing native reflection output");
        command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
        SDL_GPUBufferRegion image{static_cast<SDL_GPUBuffer*>(output.buffer),0,image_bytes};SDL_GPUTransferBufferLocation destination{resources.download,0};
        SDL_DownloadFromGPUBuffer(copy,&image,&destination);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        const auto* pixels=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(owner.device,resources.download,false));require(pixels!=nullptr,SDL_GetError());
        std::vector<unsigned> result(pixels,pixels+camera.width*camera.height);SDL_UnmapGPUTransferBuffer(owner.device,resources.download);
        ++reflections;if(reflections%320==0)std::cout<<"  completed "<<reflections<<" actual native reflection submissions\n"<<std::flush;return result;
    });
    unsigned indexed_reflections=0;
    for(bool gpu_records:{false,true}) {
    const auto indexed_dispatch=[&](const auto& p,const auto& input,const auto& ink) {
        auto* upload=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(owner.device,resources.upload,false));require(upload!=nullptr,SDL_GetError());
        std::memcpy(upload,input.vertices.data(),sizeof(input.vertices));std::memcpy(upload+sizeof(input.vertices),input.materials.data(),sizeof(input.materials));
        SDL_UnmapGPUTransferBuffer(owner.device,resources.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
        SDL_GPUTransferBufferLocation from{resources.upload,0};SDL_GPUBufferRegion to{resources.geometry,0,unsigned(sizeof(input.vertices)+sizeof(input.materials))};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        starfox::render::RayMaterials materials;materials.triangles.resize(4);
        if(!gpu_records)std::memcpy(materials.triangles.data(),input.materials.data(),sizeof(input.materials));
        materials.texels.assign(ink.begin(),ink.end());
        const starfox::render::GpuScene::RayGeometryOutput geometry{owner.device,resources.geometry,12,true,&materials,
            gpu_records?unsigned(sizeof(input.vertices)):0};
        const starfox::render::shadows::Camera camera{native_reflection_fixture::width,native_reflection_fixture::height,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
        std::optional<starfox::render::shadows::ReceiverPlane> plane;
        if(p.settings[1]!=0)plane={{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
        starfox::render::shadows::RayWater water;water.reflection_strength=water.brightness=1;
        water.material=unsigned(p.water[3])&15U;water.mirror_models=(unsigned(p.water[3])&16U)!=0;
        const auto range=p.primary_range[2]!=0?std::optional<starfox::render::shadows::PrimaryRayRange>{{p.primary_range[0],p.primary_range[1]}}:std::nullopt;
        if(!rays.render_reflections(owner.device,geometry,camera,input.palette,p.environment[0],1,0,0,
            plane,nullptr,water.material || water.mirror_models?&water:nullptr,p.settings[3]!=0,0,range))throw std::runtime_error(rays.status());
        const auto output=rays.reflection_output();require(output.buffer!=nullptr,"Missing indexed reflection owner output");
        command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
        SDL_GPUBufferRegion image{static_cast<SDL_GPUBuffer*>(output.buffer),0,image_bytes};SDL_GPUTransferBufferLocation destination{resources.download,0};
        SDL_DownloadFromGPUBuffer(copy,&image,&destination);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        const auto* pixels=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(owner.device,resources.download,false));require(pixels!=nullptr,SDL_GetError());
        std::vector<unsigned> result(pixels,pixels+camera.width*camera.height);SDL_UnmapGPUTransferBuffer(owner.device,resources.download);
        ++indexed_reflections;return result;
    };
    native_reflection_fixture::run_indexed_cutouts(fixture,indexed_dispatch);
    native_reflection_fixture::run_primary_ranges<false>(indexed_dispatch);
    }
    const auto opaque=native_reflection_fixture::native_shadow_source(0);
    auto* upload=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(owner.device,resources.upload,false));require(upload!=nullptr,SDL_GetError());
    std::memset(upload,0,payload_bytes);std::memcpy(upload,opaque.geometry.vertices.data(),sizeof(opaque.geometry.vertices));
    std::memcpy(upload+sizeof(opaque.geometry.vertices),opaque.words.data(),opaque.words.size()*4);SDL_UnmapGPUTransferBuffer(owner.device,resources.upload);
    auto* command=SDL_AcquireGPUCommandBuffer(owner.device);require(command!=nullptr,SDL_GetError());
    auto* copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,SDL_GetError());
    SDL_GPUTransferBufferLocation from{resources.upload,0};SDL_GPUBufferRegion to{resources.geometry,0,payload_bytes};
    SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);wait(owner.device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
    starfox::render::RayMaterials native;native.encoding=starfox::render::RayMaterialEncoding::native_rgba;
    const starfox::render::GpuScene::RayGeometryOutput valid{owner.device,resources.geometry,12,true,&native,
        unsigned(sizeof(opaque.geometry.vertices)),unsigned(opaque.words.size()*4)};
    const starfox::render::shadows::Scene empty;const starfox::render::shadows::Camera camera{64,48,96,32,24};
    const auto restore=[&] {
        if(!rays.render_shadows(owner.device,empty,camera,{0,0,-1},{},&valid))throw std::runtime_error(rays.status());
        require(rays.shadow_output().buffer!=nullptr,"Valid native request failed to restore output after decline");
    };
    std::array<unsigned,256> empty_palette{};
    const auto restore_reflection=[&] {
        if(!rays.render_reflections(owner.device,valid,camera,empty_palette,0xff29b50d,1,0,0,{}))throw std::runtime_error(rays.status());
        require(rays.reflection_output().buffer!=nullptr,"Valid native request failed to restore reflection after decline");
    };
    unsigned range_declines=0;
    using starfox::render::shadows::PrimaryRayRange;
    const double nan=std::numeric_limits<double>::quiet_NaN(),infinity=std::numeric_limits<double>::infinity();
    for(const auto range:std::array<PrimaryRayRange,10>{{{-1,2},{2,2},{3,2},{nan,65536},{1,nan},{1,infinity},
        {1,double(std::numeric_limits<float>::max())*2},{1,1+1e-9},{0,std::numeric_limits<double>::denorm_min()},{infinity,65536}}}) {
        restore();require(!rays.render_shadows(owner.device,empty,camera,{0,0,-1},{},&valid,false,range)
            && !rays.shadow_output().buffer && rays.shadow_output().width==0,"Invalid/collapsed primary range exposed stale shadow");
        restore_reflection();require(!rays.render_reflections(owner.device,valid,camera,empty_palette,0xff29b50d,1,0,0,{},nullptr,nullptr,false,0,range)
            && !rays.reflection_output().buffer && rays.reflection_output().water_layers==starfox::render::shadows::NativeWaterLayers{},"Invalid/collapsed primary range exposed stale reflection/layers");
        ++range_declines;
    }
    std::cout<<"Primary range owner: "<<range_declines<<" invalid/non-finite/float-collapsed requests declined by both producers, borrowed outputs cleared and valid recovery passed\n"<<std::flush;
    unsigned declines=0,reflection_declines=0;
    const auto decline=[&](const auto& invalid,const char* message) {
        restore();require(!rays.render_shadows(owner.device,empty,camera,{0,0,-1},{},&invalid)
            && !rays.shadow_output().buffer && !rays.status().empty(),message);++declines;
        restore_reflection();require(!rays.render_reflections(owner.device,invalid,camera,empty_palette,0xff29b50d,1,0,0,{})
            && !rays.reflection_output().buffer && !rays.status().empty(),"Malformed native request retained reflection output");++reflection_declines;
    };
    for(unsigned bytes:{0U,252U,257U,16'000'260U,payload_bytes+4U}) {
        auto invalid=valid;invalid.material_bytes=bytes;
        decline(invalid,"Malformed/truncated/oversized native payload retained shadow output");
    }
    for(unsigned offset:{0U,128U,195U,payload_bytes-128U,0xfffffffcu}) {
        auto invalid=valid;invalid.material_offset=offset;
        decline(invalid,"Invalid/out-of-buffer native offset retained shadow output");
    }
    for(unsigned mode=0;mode<5;++mode) {
        auto invalid=valid;
        if(mode==0)invalid.complete=false;
        if(mode==1)invalid.device=nullptr;
        if(mode==2)invalid.buffer=nullptr;
        if(mode==3)invalid.vertex_count=11;
        if(mode==4)invalid.vertex_count=12'000'003;
        decline(invalid,"Invalid native geometry ownership/topology retained shadow output");
    }
    auto cpu_override=native;cpu_override.triangles.resize(4);
    auto invalid=valid;invalid.materials=&cpu_override;
    decline(invalid,"Native CPU-record override retained shadow output");
    cpu_override.triangles.clear();cpu_override.texels={1};
    decline(invalid,"Native CPU-atlas override retained shadow output");
    cpu_override.texels.clear();cpu_override.encoding=static_cast<starfox::render::RayMaterialEncoding>(99);
    decline(invalid,"Unknown encoding retained shadow output");
    cpu_override.encoding=starfox::render::RayMaterialEncoding::indexed;cpu_override.texels={2};invalid.material_offset=0;
    restore();require(!rays.render_shadows(owner.device,empty,camera,{0,0,-1},{},&invalid) && !rays.shadow_output().buffer,
        "Indexed shadow topology mismatch retained prior output");
    restore();
    require(!rays.render_shadows(owner.device,empty,camera,{0,0,-1},{},nullptr,true) && !rays.shadow_output().buffer,
        "No-plane underlay retained prior shadow output");
    for(unsigned mode=0;mode<5;++mode) {
        restore_reflection();starfox::render::shadows::RayWater request;request.mirror_models=true;
        if(mode==0)request.source_colour=std::array{.2f,.3f,.4f};
        if(mode==1)request.auxiliary_layers=true;
        if(mode==2)request.surface_layers=true;
        if(mode==3)request.material=1; // A physical liquid/metal plane is required.
        if(mode==4)request.caustics=1;
        require(!rays.render_reflections(owner.device,valid,camera,empty_palette,0xff29b50d,1,0,0,{},nullptr,&request)
            && !rays.reflection_output().buffer,"Unsupported/no-plane liquid request retained reflection output");++reflection_declines;
    }
    restore_reflection();
    require(!rays.render_reflections(owner.device,valid,camera,empty_palette,0xff29b50d,1,0,0,{},nullptr,nullptr,true)
        && !rays.reflection_output().buffer,"No-plane reflection underlay retained prior output");
    restore_reflection();
    restore();
    rays.release_device();require(!rays.shadow_output().buffer && !rays.reflection_output().buffer,"Vulkan release retained borrowed ray output");
    std::cout<<"Actual pinned SDL/Vulkan shadow owner: "<<compositions<<" source/CPU-record/GPU-offset/native-RGBA mask submissions, "
        <<declines<<" malformed native shadow metadata declines, "<<reflection_declines<<" native reflection declines with valid recovery, "
        <<reflections<<" native reflection submissions, "<<indexed_reflections<<" indexed CPU/GPU-offset reflection submissions, "<<water_submissions<<" water submissions, slot reuse and release passed; "
        "Windows run is NOT Linux/Deck/full-stage/FPS acceptance\n";return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
