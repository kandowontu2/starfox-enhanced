#pragma once
#include "starfox/vr/vulkan_ray_bindings.hpp"
#include "starfox/vr/vulkan_source_storage.hpp"
#include <cmath>

namespace starfox::vr {
// Retained topology/settings and descriptors for one resident model. Borrows
// source/output buffers and pipeline. Caller must retain them, synchronize
// projection writes before record(), and finish GPU use before update/close.
// Output may be a subrange of an externally shared acceleration-structure input.
// Geometry only: material coverage remains the ray scene's responsibility.
class VulkanRayGeometry {
public:
    using Transform=std::array<std::array<float,4>,3>;
    static constexpr Transform identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    VulkanRayGeometry()=default;
    ~VulkanRayGeometry() {close();}
    VulkanRayGeometry(const VulkanRayGeometry&)=delete;
    VulkanRayGeometry& operator=(const VulkanRayGeometry&)=delete;
    bool initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
        const VkPhysicalDeviceMemoryProperties& memory,const VkPhysicalDeviceLimits& limits,
        const VulkanSpanPipeline& pipeline,const std::array<VkDescriptorBufferInfo,3>& source,
        VkDescriptorBufferInfo output,std::span<const std::array<uint32_t,4>> topology,
        uint32_t points,uint32_t corners,uint32_t residual_mode,const Transform& transform=identity) {
        if(ready_ || topology.empty() || topology.size()>65535U*64 || !points || !corners
            || !finite(transform)) return false;
        if(residual_mode&0x80000000U) {
            const auto stride=residual_mode&0x7fffffffU;
            if(stride<12 || stride>4096 || stride%4 || source[0].range<uint64_t(points-1)*stride+12) return false;
        }
        for(const auto& triangle:topology)
            for(unsigned i=0;i<3;++i) if(triangle[i]>=corners) return false;
        Settings next{uint32_t(topology.size()),points,corners,residual_mode,transform};
        if(!topology_.initialize(device,get,memory,topology.size_bytes())
            || !uniform_.initialize(device,get,memory,sizeof(Settings))) {close();return false;}
        const std::array<VkDescriptorBufferInfo,6> ranges{source[0],source[1],source[2],
            VkDescriptorBufferInfo{topology_.buffer(),0,topology.size_bytes()},output,
            VkDescriptorBufferInfo{uniform_.buffer(),0,sizeof(Settings)}};
        if(!bindings_.initialize(device,get,limits,pipeline,ranges,next.count,points,corners)
            || !topology_.upload(0,std::as_bytes(topology))
            || !uniform_.upload(0,std::as_bytes(std::span(&next,1)))) {close();return false;}
        settings_=next;ready_=true;return true;
    }
    // A repeated transform does not map/upload; topology and descriptors stay put.
    bool update_transform(const Transform& transform) {
        if(!ready_ || !finite(transform)) return false;
        if(settings_.rows==transform) return true;
        auto next=settings_;next.rows=transform;
        if(!uniform_.upload(0,std::as_bytes(std::span(&next,1)))) return false;
        settings_=next;return true;
    }
    bool record(VkCommandBuffer command) const {return ready_ && bindings_.record(command);}
    uint32_t vertex_count() const noexcept {return ready_?settings_.count*3:0;}
    void close() noexcept {
        bindings_.close();uniform_.close();topology_.close();settings_={};ready_=false;
    }
private:
    struct Settings {uint32_t count{},points{},corners{},mode{};Transform rows{};};
    static_assert(sizeof(Settings)==64);
    static bool finite(const Transform& transform) {
        for(const auto& row:transform) for(float value:row) if(!std::isfinite(value)) return false;
        return true;
    }
    VulkanSourceStorage topology_,uniform_;
    VulkanRayBindings bindings_;
    Settings settings_{};bool ready_{};
};
}
