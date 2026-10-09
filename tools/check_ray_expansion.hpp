#pragma once
#include "starfox/vr/vulkan_source_storage.hpp"
#include "starfox/vr/vulkan_ray_bindings.hpp"
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include <bit>
#include <cmath>
inline bool check_ray_expansion(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& properties,const VkPhysicalDeviceLimits& limits,uint32_t family,
    const starfox::vr::VulkanSpanPipeline& pipeline) {
    using namespace starfox::vr;
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name))
    LOAD(vkCreateCommandPool);LOAD(vkDestroyCommandPool);LOAD(vkAllocateCommandBuffers);LOAD(vkResetCommandPool);
    LOAD(vkBeginCommandBuffer);LOAD(vkEndCommandBuffer);LOAD(vkCmdPipelineBarrier);LOAD(vkGetDeviceQueue);LOAD(vkQueueSubmit);LOAD(vkQueueWaitIdle);
#undef LOAD
    std::array<VulkanSourceStorage,6> buffers;
    VulkanSceneBuffer legacy_vertices;
    VulkanRayBindings bindings;
    VkCommandPool pool{};VkCommandBuffer command{};
    VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
    const bool result=[&] {
        const std::array<uint64_t,6> sizes{3*sizeof(SceneVertex),96,48,65*16,66*48,64};
        for(unsigned i=0;i<6;++i) if(!buffers[i].initialize(device,get,properties,sizes[i])) return false;
        std::array<VkDescriptorBufferInfo,6> ranges{};
        for(unsigned i=0;i<6;++i) ranges[i]={buffers[i].buffer(),0,sizes[i]};
        auto truncated=ranges;truncated[4].range=65*48-1;
        if(bindings.initialize(device,get,limits,pipeline,truncated,65,3,3)) return false;
        if(!bindings.initialize(device,get,limits,pipeline,ranges,65,3,3)) return false;
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=family;
        if(vkCreateCommandPool(device,&cp,nullptr,&pool)!=VK_SUCCESS) return false;
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.commandBufferCount=1;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        if(vkAllocateCommandBuffers(device,&ca,&command)!=VK_SUCCESS) return false;
        struct Point {std::array<float,4> camera{},screen{};};
        struct Settings {uint32_t count,points,corners,mode;std::array<float,4> rows[3];};
        static_assert(sizeof(Point)==32 && sizeof(Settings)==64);
        static_assert(offsetof(SceneVertex,texture)+12==148 && offsetof(SceneVertex,billboard)==152 && sizeof(SceneVertex)==160);
        static_assert(offsetof(SceneVertex,group_a)==88);
        for(unsigned mode=0;mode<11;++mode) {
            std::array<Point,3> points{},tails{};
            for(unsigned p=0;p<3;++p) {
                points[p].camera={float(p+1),float(p+2),float(p+3),1};
                tails[p].camera={.25F,-.5F,.125F,1};
                if(mode==2 || mode==3) {
                    for(unsigned c=0;c<3;++c) {
                        const auto bits=std::bit_cast<uint64_t>(double(p+c+1)+.125);
                        tails[p].camera[c]=std::bit_cast<float>(uint32_t(bits));tails[p].screen[c]=std::bit_cast<float>(uint32_t(bits>>32));
                    }
                    tails[p].camera[3]=3;
                }
            }
            std::array<std::array<uint32_t,4>,3> corners{{{0,0,0,0},{1,0,0,0},{2,0,0,0}}};
            std::array<std::array<uint32_t,4>,65> triangles;triangles.fill({0,1,2,0});triangles[63][1]=99;
            // A valid corner with an invalid point index exercises the second guard.
            if(mode==3) corners[2][0]=99;
            Settings settings{65,3,3,mode>=4?0x80000000U|(mode!=5?uint32_t(sizeof(SceneVertex)):8U):mode,{{0,2,0,10},{-1,0,0,20},{0,0,.5F,30}}};
            std::array<std::array<float,4>,66*3> output;output.fill({99,99,99,99});
            const auto upload=[&](unsigned index,const auto& values){return buffers[index].upload(0,std::as_bytes(std::span(&values,1)));};
            if(!upload(0,points)||!upload(1,tails)||!upload(2,corners)||!upload(3,triangles)||!upload(4,output)||!upload(5,settings)) return false;
            if(mode==4 || mode>=6) {
                std::array<SceneVertex,3> legacy{};
                for(unsigned p=0;p<3;++p) {
                    legacy[p].position[0]=float(p+1);legacy[p].position[1]=float(p+2);legacy[p].position[2]=float(p+3);
                    legacy[p].color[0]=99;legacy[p].texture[0]=0xffffffffU;
                    if(mode>=6) {legacy[p].texture[3]=4;legacy[p].billboard[0]=float(p)-1;legacy[p].billboard[1]=float(p)*.5F;}
                    if(mode>=7) {legacy[p].texture[3]|=134217728U;legacy[p].group_a[0]=mode==8?1000:mode==10?.1F:200;
                        legacy[p].group_a[1]=mode==8?128:mode==9?127:256;}
                }
                if(!legacy_vertices.initialize(device,get,properties,legacy)) return false;
                bindings.close();
                auto legacy_ranges=ranges;legacy_ranges[0]={legacy_vertices.buffer(),0,sizeof(legacy)};
                if(!bindings.initialize(device,get,limits,pipeline,legacy_ranges,65,3,3)) return false;
            }
            if(vkResetCommandPool(device,pool,0)!=VK_SUCCESS) return false;
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            if(vkBeginCommandBuffer(command,&begin)!=VK_SUCCESS || !bindings.record(command)) return false;
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            if(vkEndCommandBuffer(command)!=VK_SUCCESS) return false;
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
            if(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE)!=VK_SUCCESS || vkQueueWaitIdle(queue)!=VK_SUCCESS) return false;
            if(!buffers[4].readback(0,std::as_writable_bytes(std::span(output)))) return false;
            for(unsigned t=0;t<66;++t) for(unsigned p=0;p<3;++p) {
                std::array<float,4> expected{99,99,99,99};
                if(t<65) {
                    expected={10+2*(p+2+(mode==1?-.5F:mode==2||mode==3?.125F:0)),20-(p+1+(mode==1?.25F:mode==2||mode==3?.125F:0)),30+.5F*(p+3+(mode>=1&&mode<=3?.125F:0)),1};
                    if(mode>=6) {const float scale=mode==7?100:mode==8?60:1;expected[0]+=(float(p)-1)*scale;expected[1]-=float(p)*scale;}
                    if(t==63 || mode==3 || mode==5 || mode>=9) expected={0,0,0,0};
                }
                for(unsigned c=0;c<4;++c) if(output[t*3+p][c]!=expected[c]) return false;
            }
        }
        return true;
    }();
    bindings.close();
    if(pool) vkDestroyCommandPool(device,pool,nullptr);
    return result;
}
