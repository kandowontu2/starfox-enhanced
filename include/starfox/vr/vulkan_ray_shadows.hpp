#pragma once
#include "starfox/vr/ray_lighting.hpp"
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include "starfox/vr/vulkan_source_scene.hpp"
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
namespace starfox::vr {
// MODERN's ray-traced sun shadows and coloured lights on a headset: the
// flat renderer's modern::SdlRayShadows in Vulkan. Each frame a compute pass
// (MODERN's ray_shadow.hlsl, its SPIR-V shared) expands every physical
// compute-route world model into seat-space triangles (the world's native
// axes, before the head pose) and reduces one bounding box per object; the
// textured scene fragments (scene.hlsl STARFOX_SCENE_RT, set 2) rebuild each
// pixel's seat position from its eye's inverse clip transform and trace the
// sun and the point lights against them. Register binding() with
// set_scene_rays (after the fog) before creating the scene pipelines; it
// must outlive them.
class VulkanRayShadows {
public:
    // The flat renderer's look settings (modern::SdlFrameRenderer::ShadowSettings).
    struct Settings {
        float brightness{.5F};bool models{true};unsigned samples{1};float radius{};
        unsigned light_steps{0};float light_strength{.6F};float light_reach{1.F};
    };
    struct LightInputs {RayEmitterKind emitter;std::optional<RayBombFlash> bomb;uint32_t tick{};};
    static constexpr std::size_t max_point_lights=16;
    VulkanRayShadows()=default;
    ~VulkanRayShadows();
    VulkanRayShadows(const VulkanRayShadows&)=delete;
    VulkanRayShadows& operator=(const VulkanRayShadows&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,const VkPhysicalDeviceLimits&);
    void close() noexcept;
    const SceneRayBinding& binding() const noexcept {return binding_;}
    // Once per frame, after `world` is initialized and only once every eye
    // that drew the previous frame has completed. `toward_light` is the sun
    // in world axes (vr::ray_sun), empty = no sun shadows; `receivers` =
    // the frame has a surface the sun lands on (the landscape ground, the
    // corridor, the boss room); `caster` filters object keys; `lights` (null
    // = off) adds the coloured point lights. False (with status) on failure,
    // which leaves the frame untraced.
    bool prepare(const VulkanSourceScene& world,const SourceModelPackets& packets,std::optional<render::shadows::Vec3> toward_light,
        bool receivers,const std::function<bool(uint32_t)>& caster,const Settings&,const LightInputs* lights=nullptr);
    // Outside a render pass, after world.record_compute in the same command
    // buffer: expands the casters for both eyes.
    bool record_casters(VkCommandBuffer) const;
    // Before a later submission's eye reads them (the casters were recorded
    // in an earlier command buffer).
    void record_read_barrier(VkCommandBuffer) const;
    // Each eye before it draws: its camera as drawn (reversed depth) and
    // target size. Its draws carry the eye in camera.effects.w bit 4.
    void set_eye(unsigned eye,const EyeCamera&,VkExtent2D) noexcept;
    // Anything traced this frame.
    bool active() const noexcept {return active_;}
    bool sun_active() const noexcept {return sun_active_;}
    std::size_t point_light_count() const noexcept {return point_lights_.size();}
    uint32_t object_count() const noexcept {return object_count_;}
    uint32_t triangle_count() const noexcept {return triangle_count_;}
    const std::string& status() const noexcept {return status_;}
private:
    struct Buffer {VkBuffer buffer{};VkDeviceMemory memory{};VkDeviceSize size{};void* mapped{};};
    struct Dispatch {VkDescriptorSet set{};uint32_t offset{},groups{};};
    bool create_buffer(Buffer&,VkDeviceSize,VkBufferUsageFlags,bool host);
    void destroy_buffer(Buffer&) noexcept;
    bool grow(Buffer&,VkDeviceSize,VkBufferUsageFlags,bool host);
    void write_bindings();
    VkDevice device_{};PFN_vkGetDeviceProcAddr get_{};
    VkPhysicalDeviceMemoryProperties memory_{};VkPhysicalDeviceLimits limits_{};
    VkDescriptorSetLayout scene_layout_{},input_layout_{},output_layout_{},settings_layout_{};
    VkDescriptorPool scene_pool_{},dispatch_pool_{};
    uint32_t dispatch_capacity_{};
    VkDescriptorSet scene_set_{},output_set_{},settings_set_{};
    VkPipelineLayout pipeline_layout_{};VkPipeline pipeline_{};
    Buffer uniforms_,triangles_,objects_,topology_,settings_;
    VkDeviceSize settings_stride_{};
    std::vector<Dispatch> dispatches_;
    std::vector<std::array<uint32_t,8>> object_seed_;
    SceneRayBinding binding_{};
    Settings settings_values_{};
    render::shadows::Vec3 light_{0,-1,0};
    std::vector<RayPointLight> point_lights_;
    std::unordered_map<uint32_t,uint32_t> object_index_;
    uint32_t object_count_{},triangle_count_{};
    bool active_{},sun_active_{};
    std::string status_{"Ray shadows not initialized"};
};
}
