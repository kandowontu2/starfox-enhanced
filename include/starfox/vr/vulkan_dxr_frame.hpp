#pragma once
#include "starfox/vr/vulkan_ray_producer.hpp"
#include "starfox/render/dxr_shadows.hpp"

namespace starfox::vr {
// One reusable eye's geometry -> DXR -> Vulkan output. Call retire only after
// output consumers finish and release EXTERNAL ownership. Device outlives us.
class VulkanDxrFrame {
public:
    enum class State {idle,producing,ready,error};
    ~VulkanDxrFrame() {close();}
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,VkPhysicalDevice,
        PFN_vkGetPhysicalDeviceProperties2,const std::array<uint8_t,8>&,
        VkQueue,uint32_t family,uint32_t vertices);
    VkDescriptorBufferInfo geometry() const {return producer_.output();}
    // Idle only; caller discards descriptors borrowing the old geometry first.
    bool resize_geometry(uint32_t vertices);
    // Coverage and callback's GPU inputs remain alive/unchanged until poll ready.
    bool begin(render::shadows::Camera,render::shadows::Vec3,
        std::optional<render::shadows::ReceiverPlane>,const VulkanEyeCommands::Record&,
        const render::shadows::DxrShadows::Coverage* coverage=nullptr);
    State poll();
    bool retire() {if(state_!=State::ready) return false;state_=State::idle;return true;}
    State state() const {return state_;}
    VulkanExternalShadow& output() {return output_;}
    uint64_t ready_value() const {return ready_value_;}
    VulkanEyeCommands::TimelineWait draw_wait() const {
        return {state_==State::ready?output_.ready():VK_NULL_HANDLE,ready_value_,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    }
    bool prepare_draw(const VkPhysicalDeviceMemoryProperties& memory,VkRenderPass pass,std::array<float,4> color) {
        return state_==State::ready && output_.prepare_draw(memory,pass,width_,height_,color,SceneBlend::shadow);
    }
    // Acquire/release outside render pass; draw inside. Submit with draw_wait().
    bool record_acquire(VkCommandBuffer) const;
    bool record_draw(VkCommandBuffer command,VkExtent2D extent) const {
        return state_==State::ready && output_.record_draw(command,extent);
    }
    bool record_release(VkCommandBuffer) const;
    void close();
private:
    std::unique_ptr<render::shadows::DxrShadows> dxr_;
    render::shadows::DxrShadows::ResidentGeometry geometry_;
    VulkanRayProducer producer_;
    VulkanExternalShadow output_;
    VkDevice device_{};PFN_vkGetDeviceProcAddr get_{};VkPhysicalDevice physical_{};
    VkQueue queue_{};
    PFN_vkGetPhysicalDeviceProperties2 properties_{};
    PFN_vkCmdPipelineBarrier barrier_{};uint32_t family_{};
    render::shadows::Camera camera_{};render::shadows::Vec3 light_{};
    std::optional<render::shadows::ReceiverPlane> ground_;
    const render::shadows::DxrShadows::Coverage* coverage_{};
    void* imported_resource_{};uint32_t width_{},height_{};
    uint64_t ready_value_{};
    State state_{State::idle};
};
}
