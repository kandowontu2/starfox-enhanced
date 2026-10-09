#pragma once
#include "starfox/vr/vulkan_ray_geometry.hpp"
#include "starfox/vr/vulkan_draw_packets.hpp"
#include "starfox/vr/shadow_camera.hpp"

namespace starfox::vr {
// Ordinary legacy triangle candidates only; caller resolves texture coverage.
// Borrows the graphics vertices, output and pipeline. Finish GPU work before
// update/close, and reacquire RaySource after every packet collection update.
class VulkanLegacyRayGeometry {
public:
    bool initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory,
        const VkPhysicalDeviceLimits& limits,const VulkanSpanPipeline& pipeline,
        const VulkanDrawPackets::RaySource& source,VkDescriptorBufferInfo output,const EyeCamera& eye) {
        if(ready_ || !source.count || source.count%3 || source.count>4'000'000
            || source.vertices.range<uint64_t(source.count)*sizeof(SceneVertex)) return false;
        const auto rows=transform(source,eye);if(!rows) return false;
        std::vector<std::array<uint32_t,4>> corners(source.count),topology(source.count/3);
        for(uint32_t i=0;i<source.count;++i) corners[i]={i,0,0,0};
        for(uint32_t i=0;i<source.count/3;++i) topology[i]={i*3,i*3+1,i*3+2,i};
        if(!corners_.initialize(device,get,memory,corners.size()*16)
            || !corners_.upload(0,std::as_bytes(std::span(corners)))) {close();return false;}
        // Residual input is never accessed in interleaved mode; bind the same
        // retained allocation, not an extra per-model dummy buffer.
        const std::array<VkDescriptorBufferInfo,3> inputs{source.vertices,source.vertices,
            VkDescriptorBufferInfo{corners_.buffer(),0,corners_.size()}};
        if(!geometry_.initialize(device,get,memory,limits,pipeline,inputs,output,topology,source.count,source.count,
            0x80000000U|uint32_t(sizeof(SceneVertex)),*rows)) {close();return false;}
        source_=source;ready_=true;return true;
    }
    bool update(const VulkanDrawPackets::RaySource& source,const EyeCamera& eye) {
        if(!ready_ || source.count!=source_.count || source.vertices.buffer!=source_.vertices.buffer
            || source.vertices.offset!=source_.vertices.offset || source.vertices.range!=source_.vertices.range) return false;
        const auto rows=transform(source,eye);if(!rows || !geometry_.update_transform(*rows)) return false;
        source_=source;return true;
    }
    bool record(VkCommandBuffer command) const {return ready_ && geometry_.record(command);}
    void close() {geometry_.close();corners_.close();source_={};ready_=false;}
private:
    static std::optional<VulkanRayGeometry::Transform> transform(const VulkanDrawPackets::RaySource& source,const EyeCamera& eye) {
        auto rows=shadow_model_transform(eye,source.model,1);
        // Legacy vertices already use XR +Y up/-Z forward. Undo the native
        // input basis conversion, retaining output shadow basis and units.
        if(rows) for(auto& row:*rows) {row[1]=-row[1];row[2]=-row[2];}
        return rows;
    }
    VulkanSourceStorage corners_;
    VulkanRayGeometry geometry_;
    VulkanDrawPackets::RaySource source_{};bool ready_{};
};
}
