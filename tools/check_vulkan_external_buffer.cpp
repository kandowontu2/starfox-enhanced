#define VK_USE_PLATFORM_WIN32_KHR
#include "starfox/render/dxr_shadows.hpp"
#include <windows.h>
#include "starfox/vr/vulkan_loader.hpp"
#include "starfox/vr/vulkan_external_shadow.hpp"
#include "starfox/vr/vulkan_span_pipeline.hpp"
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>
#include "dxr_vulkan_readback.hpp"
#include "dxr_vulkan_draw.hpp"
#include "check_ray_expansion.hpp"
#include "starfox/vr/vulkan_ray_geometry.hpp"
#include <numeric>
#include "starfox/vr/source_ray_coverage.hpp"
#include "starfox/vr/vulkan_ray_producer.hpp"
#include "starfox/vr/vulkan_dxr_frame.hpp"
#include <chrono>
#include <thread>
#include "check_cartridge_dxr.hpp"
#include "starfox/vr/frame_wait.hpp"

int main(int argc,char** argv) {
    const bool cartridge=argc==6 && std::string_view(argv[1])=="--game";
    std::string requested=cartridge?"--dxr":argc==2?argv[1]:"";
    starfox::render::shadows::DxrShadows producer;
    using namespace starfox::render::shadows;
    Scene scene;scene.add({{-10,0,100},{10,0,100},{0,20,100}});scene.build();
    std::vector<uint8_t> reference;
    if(requested=="--dxr") {
        if(!producer.render_resident(scene,{133,79,100,64,35},{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}})) {
            std::cerr<<producer.status()<<'\n';return 11;
        }
        std::ostringstream id;
        for(const auto byte:producer.resident_output().adapter_luid) id<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
        requested=id.str();
        DxrShadows selected(producer.resident_output().adapter_luid);
        if(!selected.render_resident(scene,{133,79,100,64,35},{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}})
            || selected.resident_output().adapter_luid!=producer.resident_output().adapter_luid) return 17;
        std::vector<uint8_t> selected_reference;
        if(!selected.readback_resident(selected_reference)) return 18;
        DxrShadows missing(std::array<uint8_t,8>{});
        if(missing.available() || missing.resident_output().resource || missing.export_resident_handle()) return 19;
        if(!producer.readback_resident(reference)) return 14;
        if(selected_reference!=reference) return 20;
        const Camera nonsquare{133,79,100,64,35,60};
        std::vector<uint8_t> nonsquare_cpu,nonsquare_gpu;
        render_mask(scene,nonsquare,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},nonsquare_cpu);
        if(!selected.render(scene,nonsquare,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},nonsquare_gpu)
            || nonsquare_cpu!=nonsquare_gpu || nonsquare_gpu==reference) return 22;
        std::cout<<"Independent horizontal/vertical focal lengths match CPU rays exactly.\n";
        std::array<DxrShadows::TriangleCoverage,1> materials{};
        materials[0].uv={0,0,2,0,0,0};
        materials[0].u_mask=1;materials[0].flags=1;
        std::array<uint32_t,2> texels{0xffffffff,0xffffffff};
        DxrShadows::Coverage coverage{materials,texels};
        auto trace_coverage=[&](std::vector<uint8_t>& result) {
            return selected.render_resident(scene,nonsquare,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},nullptr,&coverage)
                && selected.readback_resident(result);
        };
        std::vector<uint8_t> covered_mask,cutout_mask;
        if(!trace_coverage(covered_mask) || covered_mask!=nonsquare_cpu) return 26;
        texels={0,0};
        if(!trace_coverage(covered_mask) || std::any_of(covered_mask.begin(),covered_mask.end(),[](auto x){return x!=0;})) return 27;
        texels={0xffffffff,0};
        if(!trace_coverage(cutout_mask) || cutout_mask==nonsquare_cpu
            || std::none_of(cutout_mask.begin(),cutout_mask.end(),[](auto x){return x!=0;})) return 28;
        // Geometric oracle: clipping at interpolated u=1 leaves this quadrilateral.
        Scene clipped;clipped.add({{-10,0,100},{0,0,100},{5,10,100}});
        clipped.add({{-10,0,100},{5,10,100},{0,20,100}});clipped.build();
        std::vector<uint8_t> clipped_cpu;
        render_mask(clipped,nonsquare,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},clipped_cpu);
        if(cutout_mask!=clipped_cpu) {std::cerr<<"Cutout differs from geometric oracle\n";return 29;}
        materials[0].offset=2;
        if(trace_coverage(covered_mask) || selected.resident_output().resource) return 30;
        materials[0].offset=0;texels={0xffffffff,0xffffffff};
        if(!trace_coverage(covered_mask) || covered_mask!=nonsquare_cpu) return 31;
        std::cout<<"DXR alpha coverage: opaque/transparent/cutout match independent geometry; invalid range rejects and recovers.\n";
        starfox::render::PackedFaces source_faces;
        source_faces.polygons={{0,3,0,1}};
        source_faces.corners={{0,1,0,0},{1,3,0,0},{2,1,0,0}};
        source_faces.primitives={starfox::render::PackedPrimitive::polygon};
        source_faces.materials.resize(1);
        auto& source_material=source_faces.materials[0];
        source_material.textured=1;source_material.u_mask=1;source_material.reserved0=uint32_t(-1);
        source_faces.texels={7,0}; // Nonzero index remains opaque even if palette colour is black.
        std::vector<std::array<uint32_t,4>> source_topology;
        std::string source_error;
        starfox::vr::SourceRayCoverage converted;
        if(!starfox::vr::source_ray_topology(source_faces,3,source_topology,source_error)
            || !starfox::vr::source_ray_coverage(source_faces,source_topology,converted)) return 32;
        auto converted_view=converted.view();
        if(!selected.render_resident(scene,nonsquare,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},nullptr,&converted_view)
            || !selected.readback_resident(covered_mask) || covered_mask!=clipped_cpu) return 33;
        source_material.texture_offset=2;
        if(starfox::vr::source_ray_coverage(source_faces,source_topology,converted)
            || converted.texels!=std::vector<uint32_t>{0xffffffff,0}) return 34;
        std::cout<<"Native indexed material conversion and signed texture scroll match cutout geometry on DXR.\n";
        // A disappearing last object must invalidate the borrowed output,
        // not expose the preceding frame's mask through a retained resource.
        if(selected.render_resident(Scene{},nonsquare,{-1,-1,-1},std::nullopt)
            || selected.resident_output().resource
            || selected.readback_resident(covered_mask) || !covered_mask.empty()) return 35;
        if(!selected.render_resident(scene,nonsquare,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},nullptr,&converted_view)
            || !selected.readback_resident(covered_mask) || covered_mask!=clipped_cpu) return 36;
        std::cout<<"Empty ray scene clears output eligibility and subsequent scene recovers exactly.\n";
        const DxrShadows::ResidentGeometry invalid_geometry{};
        if(selected.render_resident(scene,nonsquare,{-1,-1,-1},std::nullopt,&invalid_geometry)
            || selected.resident_output().resource) return 25;
        std::cout<<"Explicit adapter matches output; missing adapter refuses fallback.\n";
        const auto shadowed=std::count_if(reference.begin(),reference.end(),[](auto value){return value!=0;});
        if(shadowed==0 || shadowed==reference.size()) {std::cerr<<"Degenerate shadow reference\n";return 15;}
        std::cout<<"Reference shadowed pixels="<<shadowed<<'\n';
    }
    if((argc>2 && !cartridge) || (argc==2 && (requested.size()!=16 || requested.find_first_not_of("0123456789abcdef")!=std::string::npos))) {
        std::cerr<<"Expected optional 16-digit lowercase adapter LUID\n";return 6;
    }
    bool matched=false;
    starfox::vr::VulkanLoader loader;
    if(!loader.initialize()) {std::cerr<<loader.status()<<'\n';return 1;}
    const auto get=loader.get_instance_proc_addr();
    const auto create=reinterpret_cast<PFN_vkCreateInstance>(get(nullptr,"vkCreateInstance"));
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pApplicationInfo=&app;
    VkInstance instance{};
    if(!create || create(&info,nullptr,&instance)!=VK_SUCCESS) return 2;
    const auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(get(instance,"vkDestroyInstance"));
    const auto enumerate=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get(instance,"vkEnumeratePhysicalDevices"));
    const auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(get(instance,"vkGetPhysicalDeviceProperties2"));
    const auto external=reinterpret_cast<PFN_vkGetPhysicalDeviceExternalBufferProperties>(get(instance,"vkGetPhysicalDeviceExternalBufferProperties"));
    if(!enumerate || !properties || !external) {destroy(instance,nullptr);return 3;}
    uint32_t count{};
    if(enumerate(instance,&count,nullptr)!=VK_SUCCESS) {destroy(instance,nullptr);return 4;}
    std::vector<VkPhysicalDevice> devices(count);
    if(enumerate(instance,&count,devices.data())!=VK_SUCCESS) {destroy(instance,nullptr);return 5;}
    for(const auto device:devices) {
        VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&id;
        properties(device,&props);
        VkPhysicalDeviceExternalBufferInfo query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO};
        query.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        query.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
        VkExternalBufferProperties result{VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES};
        external(device,&query,&result);
        const auto& memory=result.externalMemoryProperties;
        std::ostringstream luid;
        for(const auto byte:id.deviceLUID) luid<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
        if(!requested.empty() && (!id.deviceLUIDValid || requested!=luid.str())) continue;
        matched|=id.deviceLUIDValid && (memory.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT);
        std::cout<<props.properties.deviceName<<" LUID-valid="<<id.deviceLUIDValid<<" LUID=";
        for(const auto byte:id.deviceLUID) std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
        std::cout<<std::dec<<" D3D12-buffer-import="<<bool(memory.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)
            <<" dedicated="<<bool(memory.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)<<'\n';
        if(!requested.empty() && matched) {
            const auto families=reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(get(instance,"vkGetPhysicalDeviceQueueFamilyProperties"));
            const auto create_device=reinterpret_cast<PFN_vkCreateDevice>(get(instance,"vkCreateDevice"));
            const auto device_proc=reinterpret_cast<PFN_vkGetDeviceProcAddr>(get(instance,"vkGetDeviceProcAddr"));
            uint32_t family_count{};families(device,&family_count,nullptr);
            std::vector<VkQueueFamilyProperties> queues(family_count);families(device,&family_count,queues.data());
            uint32_t family=0;
            while(family<family_count && (!queues[family].queueCount || !(queues[family].queueFlags&VK_QUEUE_GRAPHICS_BIT))) ++family;
            if(family==family_count) {destroy(instance,nullptr);return 8;}
            const float priority=1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex=family;queue.queueCount=1;queue.pQueuePriorities=&priority;
            const char* extensions[]{"VK_KHR_external_memory_win32","VK_KHR_external_semaphore_win32","VK_KHR_timeline_semaphore"};
            VkPhysicalDeviceTimelineSemaphoreFeatures timeline_feature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
            timeline_feature.timelineSemaphore=VK_TRUE;
            VkDeviceCreateInfo configuration{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            configuration.pNext=&timeline_feature;
            configuration.queueCreateInfoCount=1;configuration.pQueueCreateInfos=&queue;
            configuration.enabledExtensionCount=3;configuration.ppEnabledExtensionNames=extensions;
            VkDevice consumer{};
            if(create_device(device,&configuration,nullptr,&consumer)!=VK_SUCCESS) {destroy(instance,nullptr);return 9;}
            const auto destroy_device=reinterpret_cast<PFN_vkDestroyDevice>(device_proc(consumer,"vkDestroyDevice"));
            const bool ready=device_proc(consumer,"vkGetMemoryWin32HandlePropertiesKHR")
                && device_proc(consumer,"vkImportSemaphoreWin32HandleKHR");
            if(!ready) {destroy_device(consumer,nullptr);destroy(instance,nullptr);return 10;}
            if(cartridge) {
                const unsigned ticks=unsigned(std::stoul(argv[5]));
                const auto memory_query=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance,"vkGetPhysicalDeviceMemoryProperties"));
                const bool passed=ticks<=10000 && check_cartridge_dxr(consumer,device_proc,device,properties,memory_query,
                    producer.resident_output().adapter_luid,family,argv[2],argv[3],argv[4],ticks);
                destroy_device(consumer,nullptr);destroy(instance,nullptr);return passed?0:40;
            }
            {
                starfox::vr::VulkanSpanPipeline expansion;
                if(!expansion.initialize(consumer,device_proc,starfox::vr::SourceComputeStage::ray_expand)) {
                    std::cerr<<expansion.status()<<'\n';expansion.close();destroy_device(consumer,nullptr);destroy(instance,nullptr);return 23;
                }
                std::cout<<"Resident ray triangle expansion pipeline created.\n";
                VkPhysicalDeviceMemoryProperties memory{};
                reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance,"vkGetPhysicalDeviceMemoryProperties"))(device,&memory);
                if(!check_ray_expansion(consumer,device_proc,memory,props.properties.limits,family,expansion)) {
                    expansion.close();destroy_device(consumer,nullptr);destroy(instance,nullptr);return 24;
                }
                std::cout<<"GPU ray expansion dispatch, invalid-index and tail-guard checks passed.\n";
            }
            if(producer.resident_output().resource) {
                DxrShadows cold_geometry_owner(producer.resident_output().adapter_luid);
                auto& producer=cold_geometry_owner;
                const auto second_offset=std::lcm(uint64_t(256),std::lcm(uint64_t(48),uint64_t(props.properties.limits.minStorageBufferOffsetAlignment)));
                const auto geometry_bytes=second_offset+48;
                const auto geometry=producer.prepare_shared_geometry(uint32_t(geometry_bytes/16));
                const auto geometry_handle=producer.export_geometry_handle();
                const auto geometry_fence=producer.export_ready_fence_handle();
                starfox::vr::VulkanExternalShadow geometry_import;
                bool geometry_ok=geometry.resource && geometry.ready_value && !producer.resident_output().resource
                    && !producer.export_resident_handle() && geometry_import.initialize(consumer,device_proc,device,properties,
                    geometry.adapter_luid,geometry_handle,geometry_fence,geometry_bytes);
                if(geometry_handle) CloseHandle(geometry_handle);
                if(geometry_fence) CloseHandle(geometry_fence);
                const auto memory_properties=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance,"vkGetPhysicalDeviceMemoryProperties"));
                VkPhysicalDeviceMemoryProperties geometry_memory{};memory_properties(device,&geometry_memory);
                starfox::vr::VulkanSpanPipeline geometry_pipeline;
                std::array<starfox::vr::VulkanSourceStorage,3> geometry_buffers;
                std::array<starfox::vr::VulkanRayGeometry,2> geometry_bindings;
                geometry_ok &= geometry_pipeline.initialize(consumer,device_proc,starfox::vr::SourceComputeStage::ray_expand);
                const std::array<uint64_t,3> geometry_sizes{96,96,48};
                std::array<VkDescriptorBufferInfo,3> geometry_ranges{};
                for(unsigned i=0;geometry_ok && i<3;++i) {
                    geometry_ok=geometry_buffers[i].initialize(consumer,device_proc,geometry_memory,geometry_sizes[i]);
                    geometry_ranges[i]={geometry_buffers[i].buffer(),0,geometry_sizes[i]};
                }
                struct RayPoint {std::array<float,4> camera{},screen{};};
                static_assert(sizeof(RayPoint)==32);
                const std::array<RayPoint,3> ray_points{{{{-10,0,100,1},{}},{{10,0,100,1},{}},{{0,20,100,1},{}}}};
                const std::array<RayPoint,3> ray_tails{};
                const std::array<std::array<uint32_t,4>,3> ray_corners{{{0,0,0,0},{1,0,0,0},{2,0,0,0}}};
                const std::array<uint32_t,4> ray_triangle{0,1,2,0};
                const auto upload_geometry=[&](unsigned i,const auto& value) {
                    return geometry_buffers[i].upload(0,std::as_bytes(std::span(&value,1)));
                };
                geometry_ok &= upload_geometry(0,ray_points) && upload_geometry(1,ray_tails)
                    && upload_geometry(2,ray_corners);
                for(unsigned model=0;geometry_ok && model<2;++model)
                    geometry_ok=geometry_bindings[model].initialize(consumer,device_proc,geometry_memory,props.properties.limits,
                        geometry_pipeline,geometry_ranges,{geometry_import.buffer(),model?second_offset:0,48},
                        std::span(&ray_triangle,1),3,3,0);
                std::vector<uint8_t> previous_geometry_mask;
                for(unsigned frame=0;geometry_ok && frame<2;++frame) {
                    const float shift=float(frame)*5;
                    for(unsigned model=0;model<2;++model) {
                        auto transform=starfox::vr::VulkanRayGeometry::identity;transform[0][3]=shift+(model?25.F:-25.F);
                        geometry_ok &= geometry_bindings[model].update_transform(transform);
                    }
                    if(!geometry_ok) break;
                    std::vector<uint8_t> copied;
                    geometry_ok=read_imported_dxr(consumer,device,memory_properties,device_proc,family,geometry_import.buffer(),
                        geometry_import.ready(),frame?producer.resident_output().ready_value:geometry.ready_value,geometry_bytes,copied,[&](VkCommandBuffer command,VkBuffer staging) {
                            reinterpret_cast<PFN_vkCmdFillBuffer>(device_proc(consumer,"vkCmdFillBuffer"))(command,geometry_import.buffer(),0,geometry_bytes,0);
                            VkMemoryBarrier clear{VK_STRUCTURE_TYPE_MEMORY_BARRIER};clear.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;clear.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
                            reinterpret_cast<PFN_vkCmdPipelineBarrier>(device_proc(consumer,"vkCmdPipelineBarrier"))(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&clear,0,nullptr,0,nullptr);
                            for(auto& model:geometry_bindings) if(!model.record(command)) return false;
                            VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                            reinterpret_cast<PFN_vkCmdPipelineBarrier>(device_proc(consumer,"vkCmdPipelineBarrier"))(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
                            VkBufferCopy copy{0,0,geometry_bytes};
                            reinterpret_cast<PFN_vkCmdCopyBuffer>(device_proc(consumer,"vkCmdCopyBuffer"))(command,geometry_import.buffer(),staging,1,&copy);return true;
                        },VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT);
                    Scene expected_scene;
                    for(float model_shift:{shift-25,shift+25}) expected_scene.add({{-10+model_shift,0,100},{10+model_shift,0,100},{model_shift,20,100}});
                    expected_scene.build();
                    geometry_ok &= copied.size()==geometry_bytes;
                    if(geometry_ok) for(size_t i=48;i<second_offset;++i) geometry_ok &= copied[i]==0;
                    std::vector<uint8_t> expected,actual;
                    render_mask(expected_scene,{133,79,100,64,35},{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},expected);
                    if(frame && expected==previous_geometry_mask) {geometry_ok=false;break;}
                    geometry_ok &= producer.render_resident(Scene{},{133,79,100,64,35},{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},&geometry)
                        && producer.readback_resident(actual) && actual==expected;
                    previous_geometry_mask=actual;
                    const auto completion=producer.export_ready_fence_handle();
                    geometry_ok &= completion!=nullptr;if(completion) CloseHandle(completion);
                }
                for(auto& model:geometry_bindings) model.close();
                geometry_import.close();
                starfox::vr::VulkanRayProducer resident_producer;
                VkQueue producer_queue{};
                reinterpret_cast<PFN_vkGetDeviceQueue>(device_proc(consumer,"vkGetDeviceQueue"))(consumer,family,0,&producer_queue);
                const auto shared_handle=producer.export_geometry_handle();
                const auto shared_fence=producer.export_ready_fence_handle();
                geometry_ok &= resident_producer.initialize(consumer,device_proc,device,properties,
                    producer.resident_output().adapter_luid,shared_handle,shared_fence,geometry_bytes,producer_queue,family);
                if(shared_handle) CloseHandle(shared_handle);if(shared_fence) CloseHandle(shared_fence);
                for(unsigned model=0;geometry_ok && model<2;++model)
                    geometry_ok=geometry_bindings[model].initialize(consumer,device_proc,geometry_memory,props.properties.limits,
                        geometry_pipeline,geometry_ranges,{resident_producer.output().buffer,model?second_offset:0,48},
                        std::span(&ray_triangle,1),3,3,0);
                for(unsigned frame=0;geometry_ok && frame<3;++frame) {
                    const float shift=float(frame)*7;
                    for(unsigned model=0;model<2;++model) {
                        auto transform=starfox::vr::VulkanRayGeometry::identity;transform[0][3]=shift+(model?25.F:-25.F);
                        geometry_ok &= geometry_bindings[model].update_transform(transform);
                    }
                    geometry_ok &= resident_producer.submit(producer.resident_output().ready_value,[&](VkCommandBuffer command,VkExtent2D) {
                        for(auto& model:geometry_bindings) if(!model.record(command)) throw std::runtime_error("Resident model dispatch failed");
                    });
                    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                    while(geometry_ok) {
                        const auto state=resident_producer.poll();
                        if(state==starfox::vr::VulkanEyeCommands::Completion::complete) break;
                        if(state==starfox::vr::VulkanEyeCommands::Completion::error || std::chrono::steady_clock::now()>deadline) {geometry_ok=false;break;}
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    if(!geometry_ok) break;
                    Scene expected_scene;
                    for(float x:{shift-25,shift+25}) expected_scene.add({{-10+x,0,100},{10+x,0,100},{x,20,100}});
                    expected_scene.build();std::vector<uint8_t> expected,actual;
                    render_mask(expected_scene,{133,79,100,64,35},{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},expected);
                    geometry_ok=producer.render_resident(Scene{},{133,79,100,64,35},{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},&geometry)
                        && producer.readback_resident(actual) && actual==expected;
                    const auto completion=producer.export_ready_fence_handle();
                    geometry_ok &= completion!=nullptr;if(completion) CloseHandle(completion);
                }
                resident_producer.close();
                for(auto& model:geometry_bindings) model.close();
                starfox::vr::VulkanDxrFrame frame_renderer;
                std::vector<double> frame_ready_ms;
                starfox::vr::FrameWait frame_wait;
                geometry_ok &= frame_renderer.initialize(consumer,device_proc,device,properties,geometry.adapter_luid,
                    producer_queue,family,uint32_t(geometry_bytes/16));
                for(unsigned model=0;geometry_ok && model<2;++model)
                    geometry_ok=geometry_bindings[model].initialize(consumer,device_proc,geometry_memory,props.properties.limits,
                        geometry_pipeline,geometry_ranges,{frame_renderer.geometry().buffer,model?second_offset:0,48},
                        std::span(&ray_triangle,1),3,3,0);
                for(unsigned frame=0;geometry_ok && frame<13;++frame) {
                    for(unsigned model=0;model<2;++model) {
                        auto transform=starfox::vr::VulkanRayGeometry::identity;transform[0][3]=float(frame)*3+(model?25.F:-25.F);
                        if(model==0 && frame && frame<3) {
                            for(auto& binding:geometry_bindings) binding.close();
                            geometry_ok &= !frame_renderer.resize_geometry(1)
                                && frame_renderer.resize_geometry(uint32_t(geometry_bytes/16)+(frame==1?3:0));
                            for(unsigned part=0;geometry_ok && part<2;++part)
                                geometry_ok=geometry_bindings[part].initialize(consumer,device_proc,geometry_memory,props.properties.limits,
                                    geometry_pipeline,geometry_ranges,{frame_renderer.geometry().buffer,part?second_offset:0,48},
                                    std::span(&ray_triangle,1),3,3,0);
                        }
                        geometry_ok &= geometry_bindings[model].update_transform(transform);
                    }
                    const auto record=[&](VkCommandBuffer command,VkExtent2D) {
                        for(auto& model:geometry_bindings) if(!model.record(command)) throw std::runtime_error("Frame dispatch failed");
                    };
                    const auto frame_started=std::chrono::steady_clock::now();
                    geometry_ok &= frame_renderer.begin({133,79,100,64,35},{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},record)
                        && !frame_renderer.begin({133,79,100,64,35},{-1,-1,-1},std::nullopt,record) && !frame_renderer.retire()
                        && !frame_renderer.resize_geometry(uint32_t(geometry_bytes/16));
                    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                    while(geometry_ok && frame_renderer.poll()==starfox::vr::VulkanDxrFrame::State::producing) {
                        if(std::chrono::steady_clock::now()>deadline) {geometry_ok=false;break;}
                        frame_wait.pause();
                    }
                    geometry_ok &= frame_renderer.state()==starfox::vr::VulkanDxrFrame::State::ready;
                    if(frame>=3) frame_ready_ms.push_back(std::chrono::duration<double,std::milli>(
                        std::chrono::steady_clock::now()-frame_started).count());
                    std::vector<uint8_t> copied,expected;
                    if(geometry_ok) geometry_ok=read_imported_dxr(consumer,device,memory_properties,device_proc,family,
                        frame_renderer.output().buffer(),frame_renderer.output().ready(),frame_renderer.ready_value(),136*79,copied);
                    Scene expected_scene;
                    for(float x:{float(frame)*3-25,float(frame)*3+25}) expected_scene.add({{-10+x,0,100},{10+x,0,100},{x,20,100}});
                    expected_scene.build();render_mask(expected_scene,{133,79,100,64,35},{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},expected);
                    if(geometry_ok) for(unsigned y=0;y<79;++y) for(unsigned x=0;x<133;++x)
                        geometry_ok &= copied[y*136+x]==expected[y*133+x];
                    if(geometry_ok && frame==2) {
                        std::vector<uint8_t> drawn;
                        geometry_ok=draw_imported_dxr(consumer,device,memory_properties,device_proc,family,
                            frame_renderer.output(),frame_renderer.ready_value(),133,79,drawn,true,&frame_renderer);
                        if(geometry_ok) for(size_t pixel=0;pixel<expected.size();++pixel) {
                            for(unsigned channel=0;channel<3;++channel)
                                geometry_ok &= std::abs(double(drawn[pixel*4+channel])-64*(channel+1)*(1.-expected[pixel]/510.))<=1;
                            geometry_ok &= drawn[pixel*4+3]==255;
                        }
                    }
                    geometry_ok &= frame_renderer.retire();
                }
                frame_renderer.close();for(auto& model:geometry_bindings) model.close();
                for(auto& buffer:geometry_buffers) buffer.close();
                geometry_pipeline.close();
                if(!geometry_ok) {destroy_device(consumer,nullptr);destroy(instance,nullptr);return 26;}
                std::sort(frame_ready_ms.begin(),frame_ready_ms.end());
                std::cout<<"Resident two-triangle 133x79 begin-to-ready: n="<<frame_ready_ms.size()
                    <<" median_ms="<<frame_ready_ms[frame_ready_ms.size()/2]<<" max_ms="<<frame_ready_ms.back()
                    <<" (live bounded wait; excludes readback/draw; not game FPS).\n";
                std::cout<<"Resident producer to DXR matches three updates without geometry readback/staging.\n";
                std::cout<<"Combined frame producer/trace/output import and shadow draw match references; premature reuse rejected.\n";
                std::cout<<"Two Vulkan-expanded models plus zeroed alignment padding trace exactly across two updates.\n";
              for(unsigned cycle=0;cycle<5;++cycle) {
                const uint32_t widths[]{133,257,256,67,133};
                const uint32_t heights[]{79,131,128,43,79};
                if(!producer.render_resident(scene,{widths[cycle],heights[cycle],100,32,20},{-1,-1,-1},
                    ReceiverPlane{{0,25,0},{0,1,0}}) || !producer.readback_resident(reference)) {
                    destroy_device(consumer,nullptr);destroy(instance,nullptr);return 16;
                }
                const auto output=producer.resident_output();
                {
                    starfox::vr::VulkanExternalShadow shared;
                    const auto resource=producer.export_resident_handle();
                    const auto fence=producer.export_ready_fence_handle();
                    auto wrong_luid=output.adapter_luid;wrong_luid[0]^=255;
                    bool valid=!shared.initialize(consumer,device_proc,device,properties,wrong_luid,
                        resource,fence,uint64_t(output.row_bytes)*output.height);
                    valid &= !shared.initialize(consumer,device_proc,device,properties,output.adapter_luid,resource,fence,3);
                    valid &= shared.initialize(consumer,device_proc,device,properties,output.adapter_luid,
                        resource,fence,uint64_t(output.row_bytes)*output.height);
                    valid &= shared.descriptor()!=VK_NULL_HANDLE && shared.layout()!=VK_NULL_HANDLE;
                    valid &= !shared.initialize(consumer,device_proc,device,properties,output.adapter_luid,
                        resource,fence,uint64_t(output.row_bytes)*output.height);
                    if(resource) CloseHandle(resource);
                    if(fence) CloseHandle(fence);
                    std::vector<uint8_t> pixels;
                    const auto memory_properties=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance,"vkGetPhysicalDeviceMemoryProperties"));
                    if(valid) valid=read_imported_dxr(consumer,device,memory_properties,device_proc,family,
                        shared.buffer(),shared.ready(),producer.resident_output().ready_value,
                        uint64_t(output.row_bytes)*output.height,pixels);
                    if(valid) for(size_t y=0;y<output.height;++y) for(size_t x=0;x<output.width;++x)
                        valid &= pixels[y*output.row_bytes+x]==reference[y*output.width+x];
                    std::vector<uint8_t> rendered;
                    if(valid) valid=draw_imported_dxr(consumer,device,memory_properties,device_proc,family,shared,
                        producer.resident_output().ready_value,output.width,output.height,rendered,cycle%2!=0);
                    if(valid) for(size_t i=0;i<reference.size();++i) {
                        if(cycle%2) {
                            valid &= rendered[i*4+3]==255;
                            for(unsigned c=0;c<3;++c) {
                                const double expected=64*(c+1)*(1.-reference[i]/510.);
                                valid &= std::abs(double(rendered[i*4+c])-expected)<=1.;
                            }
                        } else valid &= rendered[i*4+3]==reference[i] && rendered[i*4]==0 && rendered[i*4+2]==0
                                && rendered[i*4+1]==(reference[i]?255:0);
                    }
                    if(valid) std::cout<<"Imported DXR "<<(cycle%2?"shadow composite":"coverage draw")<<" matches "<<reference.size()<<" reference pixels.\n";
                    shared.close();shared.close();
                    valid &= !shared.descriptor() && !shared.layout() && !shared.buffer() && !shared.ready();
                    if(!valid) {destroy_device(consumer,nullptr);destroy(instance,nullptr);return 21;}
                    std::cout<<"Renderer import component pixel match and wrong-adapter rejection pass.\n";
                }
                const auto handle=producer.export_resident_handle();
                const auto create_buffer=reinterpret_cast<PFN_vkCreateBuffer>(device_proc(consumer,"vkCreateBuffer"));
                const auto destroy_buffer=reinterpret_cast<PFN_vkDestroyBuffer>(device_proc(consumer,"vkDestroyBuffer"));
                const auto requirements=reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(device_proc(consumer,"vkGetBufferMemoryRequirements"));
                const auto handle_properties=reinterpret_cast<PFN_vkGetMemoryWin32HandlePropertiesKHR>(device_proc(consumer,"vkGetMemoryWin32HandlePropertiesKHR"));
                const auto allocate=reinterpret_cast<PFN_vkAllocateMemory>(device_proc(consumer,"vkAllocateMemory"));
                const auto release=reinterpret_cast<PFN_vkFreeMemory>(device_proc(consumer,"vkFreeMemory"));
                const auto bind=reinterpret_cast<PFN_vkBindBufferMemory>(device_proc(consumer,"vkBindBufferMemory"));
                VkBuffer buffer{};VkDeviceMemory imported{};
                const auto imported_ok=[&] {
                    if(!handle) return false;
                    VkExternalMemoryBufferCreateInfo shared{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
                    shared.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
                    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer_info.pNext=&shared;
                    buffer_info.size=uint64_t(output.row_bytes)*output.height;buffer_info.usage=query.usage;
                    if(create_buffer(consumer,&buffer_info,nullptr,&buffer)!=VK_SUCCESS) return false;
                    VkMemoryRequirements need{};requirements(consumer,buffer,&need);
                    VkMemoryWin32HandlePropertiesKHR allowed{VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR};
                    if(handle_properties(consumer,query.handleType,handle,&allowed)!=VK_SUCCESS) return false;
                    const auto types=need.memoryTypeBits&allowed.memoryTypeBits;
                    if(!types) return false;
                    unsigned type=0;while(!(types&(1U<<type))) ++type;
                    VkImportMemoryWin32HandleInfoKHR import_info{VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR};
                    import_info.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;import_info.handle=handle;
                    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
                    dedicated.buffer=buffer;dedicated.pNext=&import_info;
                    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.pNext=&dedicated;
                    allocation.allocationSize=need.size;allocation.memoryTypeIndex=type;
                    if(allocate(consumer,&allocation,nullptr,&imported)!=VK_SUCCESS) return false;
                    return bind(consumer,buffer,imported,0)==VK_SUCCESS;
                }();
                if(handle) CloseHandle(handle);
                if(!imported_ok) {
                    if(buffer) destroy_buffer(consumer,buffer,nullptr);
                    if(imported) release(consumer,imported,nullptr);
                    destroy_device(consumer,nullptr);destroy(instance,nullptr);std::cerr<<"DXR resource import/bind failed\n";return 12;
                }
                std::cout<<"Actual DXR resource imported and bound.\n";
                const auto fence_handle=producer.export_ready_fence_handle();
                const auto create_semaphore=reinterpret_cast<PFN_vkCreateSemaphore>(device_proc(consumer,"vkCreateSemaphore"));
                const auto destroy_semaphore=reinterpret_cast<PFN_vkDestroySemaphore>(device_proc(consumer,"vkDestroySemaphore"));
                const auto import_semaphore=reinterpret_cast<PFN_vkImportSemaphoreWin32HandleKHR>(device_proc(consumer,"vkImportSemaphoreWin32HandleKHR"));
                const auto counter=reinterpret_cast<PFN_vkGetSemaphoreCounterValueKHR>(device_proc(consumer,"vkGetSemaphoreCounterValueKHR"));
                VkSemaphore semaphore{};
                VkSemaphoreTypeCreateInfo type_info{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type_info.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
                VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};semaphore_info.pNext=&type_info;
                bool synchronized=fence_handle && counter && create_semaphore(consumer,&semaphore_info,nullptr,&semaphore)==VK_SUCCESS;
                if(synchronized) {
                    VkImportSemaphoreWin32HandleInfoKHR import_info{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR};
                    import_info.semaphore=semaphore;import_info.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;import_info.handle=fence_handle;
                    uint64_t value{};
                    synchronized=import_semaphore(consumer,&import_info)==VK_SUCCESS
                        && counter(consumer,semaphore,&value)==VK_SUCCESS
                        && value>=producer.resident_output().ready_value && producer.resident_output().ready_value!=0;
                }
                const auto first_reference=reference;
                for(unsigned frame=0;synchronized && frame<8;++frame) {
                    if(frame) {
                        // The previous copy completed and released external ownership.
                        // Keep the import alive while the producer updates the same buffer.
                        synchronized=producer.render_resident(scene,{output.width,output.height,100,32+float(frame)*2,20},
                            {-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}})
                            && producer.readback_resident(reference)
                            && producer.resident_output().resource==output.resource;
                        const auto ready_handle=synchronized?producer.export_ready_fence_handle():nullptr;
                        synchronized &= ready_handle!=nullptr;
                        if(ready_handle) CloseHandle(ready_handle);
                        // Deliberately move the projection so stale first-frame pixels fail.
                        synchronized &= reference!=first_reference;
                    }
                    if(!synchronized) break;
                    std::vector<uint8_t> copied;
                    const auto memory_properties=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance,"vkGetPhysicalDeviceMemoryProperties"));
                    synchronized=read_imported_dxr(consumer,device,memory_properties,device_proc,family,buffer,semaphore,
                        producer.resident_output().ready_value,uint64_t(output.row_bytes)*output.height,copied);
                    if(synchronized) for(size_t y=0;y<output.height;++y) for(size_t x=0;x<output.width;++x)
                        synchronized &= copied[y*output.row_bytes+x]==reference[y*output.width+x];
                    if(!synchronized) std::cerr<<"Copy mismatch at resize cycle "<<cycle<<", frame "<<frame<<'\n';
                }
                if(semaphore) destroy_semaphore(consumer,semaphore,nullptr);
                if(fence_handle) CloseHandle(fence_handle);
                destroy_buffer(consumer,buffer,nullptr);release(consumer,imported,nullptr);
                if(!synchronized) {destroy_device(consumer,nullptr);destroy(instance,nullptr);return 13;}
                std::cout<<"Imported D3D12 fence timeline reports producer completion.\n";
                std::cout<<"Vulkan GPU copy matches "<<reference.size()<<" pixels across 8 changing frames at "
                    <<output.width<<'x'<<output.height<<".\n";
              }
            }
            destroy_device(consumer,nullptr);
            std::cout<<"Matched consumer device created with Windows memory/semaphore import entry points.\n";
        }
    }
    destroy(instance,nullptr);
    std::cout<<"No presentation performed.\n";
    if(!requested.empty() && !matched) {std::cerr<<"No matching import-capable adapter\n";return 7;}
}
