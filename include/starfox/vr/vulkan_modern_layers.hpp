#pragma once
#include "starfox/vr/modern_frame.hpp"
#include "starfox/vr/vulkan_draw_packets.hpp"
#include "starfox/vr/vulkan_layer_filter.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <optional>
#include <string>
namespace starfox::vr {
class VulkanPipelineCache;
// MODERN's layers around the world (vr::ModernFrame) on the PCVR Vulkan
// renderer, in the flat renderer's order (modern::SdlFrameRenderer::record):
// stars, backgrounds, landscape art, tunnel surround | world | circle and
// colour math below the HUD | sprites, reticle, overlays over the HUD,
// shutter. Shared by the OpenXR and Steam Frame applications and the
// offscreen capture check; the world (VulkanSourceScene) and each host's own
// layers (menus, sandbox pointer, cockpit) stay with the caller.
// Keep alive until every eye that recorded it has completed.
class VulkanModernLayers {
public:
    // Borrowed; the cache (optional) must outlive later upload() calls.
    // srgb: the eye target is sRGB. Native 2D art (sprites, HUD, text) is
    // upscaled with xBRZ (VulkanLayerFilter) unless STARFOX_MODERN_XBRZ=0;
    // replacement (HD) art is drawn as it is.
    void initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
        VkRenderPass,VulkanPipelineCache* cache=nullptr,bool srgb=true);
    // After ModernFrame::prepare, once both eyes of the previous frame have
    // completed. Reset the frame whenever these layers start empty: retained
    // layers are only re-uploaded when the frame rebuilt them.
    bool upload(const ModernFrame&);
    // An eye camera as MODERN draws with it: reversed depth, smooth 2D
    // filtering (STARFOX_MODERN_FILTER=0 off), resolved two-colour dither
    // (effects.w bit 1, as the flat renderer shows it) and output pixels per source
    // pixel (4.4 fixed point, effects.w bits 8-19) for the ground dots: the
    // native view's 1/256 tangent across this target's height.
    static EyeCamera eye_camera(EyeCamera,VkExtent2D) noexcept;
    // A camera with the distance haze mode of its draws (VulkanSceneFog;
    // effects.w bits 2-3): 1 for the world. The layers set their own.
    static EyeCamera fogged(EyeCamera camera,unsigned mode) noexcept {
        camera.effects[3]=(camera.effects[3]&~12U)|((mode&3U)<<2);return camera;
    }
    // Writes the frame's haze (ModernFrame::fog; none = off) for its eyes,
    // whose near plane is near_metres. Between frames, like upload().
    static void apply_fog(VulkanSceneFog&,const ModernFrame&,float near_metres) noexcept;
    // Each takes eye_camera()'s result. world_effects: camera.effects x/y of
    // the backgrounds (the global world colour effect and its intensity).
    // Outside any render pass, before this frame's eyes record (once per
    // frame): the xBRZ layer at the eye's scale (eye_camera()'s result).
    bool record_filter(VkCommandBuffer,const EyeCamera&);
    bool record_stars(VkCommandBuffer,VkExtent2D,const EyeCamera&) const;
    bool record_backgrounds(VkCommandBuffer,VkExtent2D,const EyeCamera&,std::array<unsigned,2> world_effects={}) const;
    // Right after the world: the engine glow on the ground (ModernFrame::
    // engine_glow; depth-tested, additive, writes no depth).
    bool record_after_world(VkCommandBuffer,VkExtent2D,const EyeCamera&) const;
    bool record_below_hud(VkCommandBuffer,VkExtent2D,const EyeCamera&) const;
    bool record_hud(VkCommandBuffer,VkExtent2D,const EyeCamera&) const;
    // The background camera's effects.z: the landscape heading.
    uint32_t landscape_heading() const noexcept {return landscape_heading_;}
    const std::string& status() const noexcept {return status_;}
private:
    // A single-draw layer with its own blend: circle, colour math, shutter.
    struct Overlay {
        VulkanScenePipeline pipeline;VulkanSceneBuffer vertices;
        std::optional<SceneBlend> blend;Matrix4 model{};bool active{},over_hud{};
    };
    void upload_overlay(Overlay&,const std::optional<ModernFrame::Overlay>&);
    bool record_overlay(VkCommandBuffer,VkExtent2D,const EyeCamera&,const Overlay&,bool head_locked) const;
    VkDevice device_{};PFN_vkGetDeviceProcAddr get_{};
    VkPhysicalDeviceMemoryProperties memory_{};VkRenderPass pass_{};
    VulkanPipelineCache* cache_{};
    VulkanDrawPackets stars_,backgrounds_,textures_,tunnel_surround_,sprites_,reticle_,engine_glow_,cockpit_;
    VulkanLayerFilter layer_filter_;
    bool filter_enabled_{};
    Overlay circle_,colour_math_,shutter_;
    bool textures_active_{},reticle_active_{},engine_glow_active_{},cockpit_active_{};
    uint32_t landscape_heading_{},texture_mapping_{};
    std::string status_{"MODERN layers not initialized"};
};
}
