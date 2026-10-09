#include "starfox/render/dxr_shadows.hpp"
#include "starfox/render/vulkan_hardware_rt.hpp"
#include <iostream>
#include <stdexcept>
#ifdef STARFOX_DXR
#error This diagnostic must compile without the Windows DXR backend.
#endif
int main() try {
    using namespace starfox::render::shadows;
    DxrShadows backend;
    const auto require=[](bool condition) {if(!condition) throw std::runtime_error("Unsupported DXR contract failed");};
    Scene scene;Camera camera{};Vec3 light{0,1,0};
    VulkanHardwareRt vulkan;
    require(!vulkan.available(nullptr) && vulkan.native_work_complete() && !vulkan.working_image_bytes());
    require(!vulkan.render_shadows(nullptr,scene,camera,light,{}) && !vulkan.shadow_output().buffer);
    require(vulkan.try_release_device());vulkan.release_device();
    require(vulkan.native_work_complete() && !vulkan.working_image_bytes());
    std::vector<std::uint8_t> mask{17,23};
    require(!backend.hardware_supported());
    require(!backend.available());
    require(backend.work_complete());
    require(!backend.render(scene,camera,light,std::nullopt,mask));
    require(mask==std::vector<std::uint8_t>{17,23}); // Caller-owned fallback data.
    for(bool deferred:{false,true}) {
        require(!backend.render_resident(scene,camera,light,std::nullopt,nullptr,nullptr,true,deferred));
        require(!backend.resident_output().resource && !backend.resident_output().device);
        require(!backend.prepare_shared_geometry(3).resource);
        require(!backend.export_geometry_handle() && !backend.export_resident_handle()
            && !backend.export_ready_fence_handle());
    }
    require(!backend.readback_resident(mask) && mask.empty());
    starfox::render::RayMaterials materials;std::array<std::uint32_t,256> palette{};
    require(!backend.render_reflections(scene,camera,materials,palette,0,mask) && mask.empty());
    DxrShadows::ReflectionInput reflection{&materials,palette,0,0,0,{},0,{1,0,0,0,1,0,0,0,1},nullptr,{},0};
    require(!backend.render_resident(scene,camera,light,{},nullptr,nullptr,true,true,&reflection));
    const RayReflectionHistory previous{64,{173,119},{100,110,83,60},12.8,5120};
    require(previous.valid());reflection.history=previous;
    require(!backend.render_resident(scene,camera,light,{},nullptr,nullptr,true,true,&reflection)
        && !backend.resident_output().reflection_history.storage_bytes);
    for(unsigned fault=0;fault<8;++fault) {
        auto malformed=previous;
        if(fault==0) malformed.previous_vertex_offset=0;
        if(fault==1) malformed.previous_vertex_offset=65;
        if(fault==2) malformed.extent[0]=0;
        if(fault==3) malformed.extent[1]=16385;
        if(fault==4) malformed.projection[1]=0;
        if(fault==5) malformed.projection[2]=std::numeric_limits<double>::infinity();
        if(fault==6) malformed.near_plane=-1;
        if(fault==7) malformed.far_plane=malformed.near_plane;
        require(!malformed.valid());
    }
    require(!native_reflection_history(0,192,{173,119}) && !native_reflection_history(16385,1,{173,119})
        && !native_reflection_history(16384,16384,{173,119}) && !native_reflection_history(256,192,{0,119})
        && native_reflection_history(16384,5041,{173,119}));
    const auto layout=native_reflection_history(256,192,{173,119});
    require(layout && layout->motion_offset==256*192*4 && layout->storage_bytes==256*192*52 && layout->extent==previous.extent
        && layout->identity_offset==256*192*20 && layout->witness_offset==256*192*36);
    std::cout<<"Unsupported DXR declines cleanly; no resources/handles or stale readback\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
