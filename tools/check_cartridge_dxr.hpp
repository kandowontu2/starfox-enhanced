#pragma once
#include "starfox/vr/source_ray_policy.hpp"
#include "starfox/vr/source_shadow_environment.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/vulkan_mixed_ray_scene.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/audio/stem_mixer.hpp"
#include "starfox/vr/vulkan_dxr_frame.hpp"
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
inline bool check_cartridge_dxr(VkDevice device,PFN_vkGetDeviceProcAddr get,VkPhysicalDevice physical,
    PFN_vkGetPhysicalDeviceProperties2 properties,PFN_vkGetPhysicalDeviceMemoryProperties memory_properties,
    const std::array<uint8_t,8>& luid,uint32_t family,const char* rom_path,const char* symbols_path,
    const char* level,unsigned ticks) {
    using namespace starfox;using namespace starfox::vr;
    VkRenderPass pass{};
    const auto destroy_pass=reinterpret_cast<PFN_vkDestroyRenderPass>(get(device,"vkDestroyRenderPass"));
    try {
        auto rom=assets::RomImage::load(rom_path);auto symbols=assets::SymbolMap::load(symbols_path);
        simulation::GameSimulation game(rom,symbols,level,{},true);
        game.set_experience(game.peek_meter_state().extended
            ?simulation::Experience::starfox_ex:simulation::Experience::original);
        audio::Spc700Audio audio;audio::Msu1Audio msu;std::vector<int16_t> pcm;
        const auto tick=[&] {
            const auto result=game.tick({});
            starfox::audio::render_mixed_tick(audio,msu,result.audio_port_writes,game.map().take_msu_register_writes(),100,100,pcm);
            game.synchronize_apu_output_ports(audio.output_ports());
        };
        const auto checkpoint=symbols.find("MAPRESTART").at(0);
        unsigned warmup=0;while(!game.map().peek_ram_word(checkpoint).value() && warmup++<3000) tick();
        if(warmup>=3000) return false;
        GameSceneHistory history(game,rom,symbols);
        for(unsigned i=0;i<ticks;++i) {tick();history.capture();}
        SourceModels models(rom,symbols,true,true);
        auto packets=models.assemble_world_interpolated(*history.previous(),*history.current(),.5,false,true);
        SourceRayPolicy(symbols).apply(packets,*history.current());
        std::cout<<"Ray source scene: camera-y="<<history.current()->camera.y
            <<" ground="<<history.current()->shadow_height<<" background="<<history.current()->background_id<<'\n';
        for(const auto& object:history.current()->objects) {
            std::cout<<"Ray source object: handle="<<object.handle<<" shape="<<object.presentation.shape
                <<" strategy="<<object.object.strategy_address<<" pose="<<object.source_pose.x<<','
                <<object.source_pose.y<<','<<object.source_pose.z<<'\n';
        }
        VkAttachmentDescription attachment{};attachment.format=VK_FORMAT_R8G8B8A8_UNORM;
        attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colour{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&colour;
        VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};info.attachmentCount=1;info.pAttachments=&attachment;
        info.subpassCount=1;info.pSubpasses=&subpass;
        if(reinterpret_cast<PFN_vkCreateRenderPass>(get(device,"vkCreateRenderPass"))(device,&info,nullptr,&pass)!=VK_SUCCESS) return false;
        bool success=[&] {
            VkPhysicalDeviceMemoryProperties memory{};memory_properties(physical,&memory);
            VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};properties(physical,&props);
            VkQueue queue{};reinterpret_cast<PFN_vkGetDeviceQueue>(get(device,"vkGetDeviceQueue"))(device,family,0,&queue);
            VulkanSourceScene scene;
            if(!scene.initialize(device,get,memory,props.properties.limits,pass,packets)) {std::cerr<<scene.status()<<'\n';return false;}
            VulkanMixedRayScene::Plan plan;SourceRayCoverage materials;
            if(!VulkanMixedRayScene::plan(packets,scene,props.properties.limits.minStorageBufferOffsetAlignment,plan)
                || !plan.bytes || !VulkanMixedRayScene::coverage(packets,plan,materials)) {std::cerr<<"Cartridge ray plan/coverage rejected\n";return false;}
            VulkanSpanPipeline expansion;if(!expansion.initialize(device,get,SourceComputeStage::ray_expand)) return false;
            VulkanDxrFrame frame;
            if(!frame.initialize(device,get,physical,properties,luid,queue,family,uint32_t(plan.bytes/16))) return false;
            VulkanMixedRayScene rays;
            const Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            const EyeCamera eye{identity,identity};
            const auto& now=*history.current();const auto& before=*history.previous();
            double fraction=.5;
            if(before.flow!=now.flow || timing::camera_transform_is_discontinuous(before.camera,now.camera)) fraction=1.;
            const auto source_view=simulation::interpolate_rotation_matrix_q15(before.view_matrix,now.view_matrix,fraction);
            const auto source_camera=timing::interpolate(before.camera,now.camera,fraction);
            const auto environment=source_shadow_environment(source_view,source_camera.y,now.shadow_height,now.shadows_enabled);
            const auto shadow_view=ShadowView::from_eye(eye);
            if(!environment || !shadow_view) return false;
            unsigned checked_ground_poses=0;
            if(environment->ground) {
                const auto poses=interpolate_scene_poses(before,now,.5,{},true);
                for(std::size_t i=0;i<poses.size();++i) {
                    if(now.objects[i].object.strategy_flags[0]&4U) continue;
                    const render::shadows::Vec3 point{poses[i].x/256.,-poses[i].y/256.,-poses[i].z/256.};
                    if(std::abs(render::shadows::dot(point-environment->ground->point,environment->ground->normal))>1e-8) {
                        std::cerr<<"Ray receiver differs from native flattened shadow pose\n";return false;
                    }
                    ++checked_ground_poses;
                }
            }
            if(!rays.initialize(device,get,memory,props.properties.limits,expansion,scene,frame.geometry(),plan,eye)) return false;
            const auto coverage=materials.view();std::vector<uint8_t> previous;
            std::size_t model_only_shadowed=0,ground_changed=0;
            // Third trace isolates analytic-ground coverage using identical
            // resident geometry/materials, rather than guessing from density.
            for(unsigned repeat=0;repeat<3;++repeat) {
                if(!frame.begin({256,224,256,128,112},shadow_view->direction(environment->light),
                    environment->ground && repeat<2?std::optional(shadow_view->receiver(*environment->ground)):std::nullopt,
                    [&](VkCommandBuffer command,VkExtent2D) {if(!scene.record_compute(command)||!rays.record(command,false)) throw std::runtime_error("Cartridge geometry dispatch failed");},&coverage)) return false;
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
                while(frame.poll()==VulkanDxrFrame::State::producing) {
                    if(std::chrono::steady_clock::now()>deadline) {frame.close();return false;}
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if(frame.state()!=VulkanDxrFrame::State::ready) return false;
                std::vector<uint8_t> mask;
                if(!read_imported_dxr(device,physical,memory_properties,get,family,frame.output().buffer(),frame.output().ready(),frame.ready_value(),256*224,mask)) return false;
                if(mask.size()!=256*224 || std::any_of(mask.begin(),mask.end(),[](auto x){return x>160;})) return false;
                if(repeat!=1) {
                    // Diagnostic artifact only: a mask is not a scene capture.
                    const auto directory=std::filesystem::path("tmp")/"cartridge-ray-masks";
                    std::filesystem::create_directories(directory);
                    auto name=std::filesystem::path(rom_path).stem().string()+"-"+
                        std::string(level)+"-"+std::to_string(ticks)+(repeat==2?"-models.bmp":"-ground.bmp");
                    // Level is a symbol supplied by the caller, never a path.
                    if(name.find_first_of("/\\")!=std::string::npos) return false;
                    std::ofstream file(directory/name,std::ios::binary);
                    std::array<unsigned char,54> header{};header[0]='B';header[1]='M';
                    const auto word=[&](unsigned offset,uint32_t value) {
                        for(unsigned byte=0;byte<4;++byte) header[offset+byte]=static_cast<unsigned char>(value>>(byte*8));
                    };
                    word(2,54+256*224*3);word(10,54);word(14,40);word(18,256);word(22,224);
                    header[26]=1;header[28]=24;word(34,256*224*3);
                    file.write(reinterpret_cast<const char*>(header.data()),header.size());
                    for(int y=223;y>=0;--y) for(unsigned x=0;x<256;++x) {
                        const char pixel=static_cast<char>(mask[y*256+x]);
                        const char rgb[]{pixel,pixel,pixel};file.write(rgb,3);
                    }
                    if(!file) return false;
                }
                if(repeat==1 && previous!=mask) {std::cerr<<"Cartridge ray output changed for identical input\n";return false;}
                if(repeat==2) {
                    model_only_shadowed=std::count_if(mask.begin(),mask.end(),[](auto x){return x!=0;});
                    for(std::size_t i=0;i<mask.size();++i) ground_changed+=mask[i]!=previous[i];
                    if(!environment->ground && ground_changed) {std::cerr<<"Disabled ground changed ray output\n";return false;}
                } else previous=std::move(mask);
                if(!frame.retire()) return false;
            }
            std::cout<<"Cartridge DXR: level="<<level<<" tick="<<ticks<<" triangles="<<plan.bytes/48
                <<" shadowed="<<std::count_if(previous.begin(),previous.end(),[](auto x){return x!=0;})
                <<" ground-poses="<<checked_ground_poses
                <<" model-only-shadowed="<<model_only_shadowed<<" ground-changed="<<ground_changed
                <<"; two complete GPU geometry/material/trace/import runs agree. Source receiver plane; no headset parity claim.\n";
            return true;
        }();
        destroy_pass(device,pass,nullptr);return success;
    } catch(const std::exception& error) {std::cerr<<"Cartridge DXR failure: "<<error.what()<<'\n';if(pass) destroy_pass(device,pass,nullptr);return false;}
}
