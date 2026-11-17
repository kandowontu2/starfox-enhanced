#include "starfox/vr/vulkan_modern_layers.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
namespace starfox::vr {
namespace {
constexpr Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
}
void VulkanModernLayers::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& memory,VkRenderPass pass,VulkanPipelineCache* cache,bool srgb) {
    device_=device;get_=get;memory_=memory;pass_=pass;cache_=cache;
    for(auto* layer:{&stars_,&backgrounds_,&textures_,&tunnel_surround_,&sprites_,&reticle_,&engine_glow_,&cockpit_}) layer->set_pipeline_cache(cache);
    static const bool xbrz=[] {
        const char* value=std::getenv("STARFOX_MODERN_XBRZ");return !(value && std::string_view(value)=="0");
    }();
    // Without the filter the sprites draw natively, as before.
    filter_enabled_=xbrz && layer_filter_.initialize(device,get,memory,pass,srgb,cache);
    status_=xbrz && !filter_enabled_?"MODERN xBRZ layer unavailable: "+layer_filter_.status():"MODERN layers ready";
}
EyeCamera VulkanModernLayers::eye_camera(EyeCamera camera,VkExtent2D extent) noexcept {
    camera=reversed_depth_camera(camera);
    static const bool filter=[] {
        const char* value=std::getenv("STARFOX_MODERN_FILTER");return !(value && std::string_view(value)=="0");
    }();
    const float source_pixel=float(extent.height)*std::abs(camera.projection[5])/512.F;
    camera.effects[3]=(filter?1U:0U)|2U|(uint32_t(std::clamp(source_pixel*16.F,16.F,4095.F))<<8);
    return camera;
}
void VulkanModernLayers::upload_overlay(Overlay& overlay,const std::optional<ModernFrame::Overlay>& plan) {
    overlay.active=plan.has_value();
    if(!overlay.active) return;
    if(!overlay.blend || *overlay.blend!=plan->blend) {
        if(!overlay.pipeline.initialize(device_,get_,pass_,false,SceneTopology::triangles,VK_NULL_HANDLE,plan->blend,cache_))
            throw std::runtime_error(overlay.pipeline.status());
        overlay.blend=plan->blend;
    }
    if(!overlay.vertices.initialize(device_,get_,memory_,plan->packet.geometry.vertex_view()))
        throw std::runtime_error(overlay.vertices.status());
    overlay.model=plan->packet.model;overlay.over_hud=plan->over_hud;
}
bool VulkanModernLayers::upload(const ModernFrame& plan) {
    try {
        if(!device_) throw std::runtime_error("MODERN layers not initialized");
        const auto layer=[&](VulkanDrawPackets& packets,std::span<const DrawPacket> items,std::span<const uint32_t> keys={}) {
            if(!packets.initialize(device_,get_,memory_,pass_,items,keys,false)) throw std::runtime_error(packets.status());
        };
        // The surrounding starfield: the distant environment, drawn first.
        layer(stars_,plan.stars,plan.star_keys);
        if(plan.layers_rebuilt) {
            layer(tunnel_surround_,plan.tunnel_surround);
            if(!filter_enabled_) layer(sprites_,plan.sprites);
            else {
                // modern::SdlFrameRenderer::prepare's split: native art
                // (triangles without the resident-image bit 23, and lines)
                // through the xBRZ layer, replacement art drawn as it is.
                std::vector<DrawPacket> native,replaced;
                for(const auto& packet:plan.sprites) {
                    const auto vertices=packet.geometry.vertex_view();
                    std::vector<SceneVertex> kept,moved;
                    for(std::size_t i=0;i+2<vertices.size();i+=3) {
                        auto& into=(vertices[i].texture[3]&8388608U)?moved:kept;
                        into.insert(into.end(),vertices.begin()+std::ptrdiff_t(i),vertices.begin()+std::ptrdiff_t(i+3));
                    }
                    if(moved.empty()) {native.push_back(packet);continue;}
                    auto hd=packet;hd.geometry.shared_vertices.reset();hd.geometry.vertices=std::move(moved);
                    hd.geometry.line_vertices.clear();hd.geometry.shared_line_vertices.reset();
                    replaced.push_back(std::move(hd));
                    auto rest=packet;rest.geometry.shared_vertices.reset();rest.geometry.vertices=std::move(kept);
                    native.push_back(std::move(rest));
                }
                if(!layer_filter_.upload(native,plan.sprite_layer)) throw std::runtime_error(layer_filter_.status());
                layer(sprites_,replaced);
            }
        }
        if(plan.backgrounds_rebuilt) layer(backgrounds_,plan.backgrounds);
        if(!backgrounds_.update_models(plan.background_models)) throw std::runtime_error("MODERN background motion update failed");
        if(plan.textures_rebuilt) layer(textures_,plan.textures);
        textures_active_=!plan.textures.empty();
        if(textures_active_ && !textures_.update_models(plan.texture_models))
            throw std::runtime_error("MODERN landscape texture motion update failed");
        reticle_active_=plan.reticle.has_value();
        if(reticle_active_) {
            reticle_.set_blend(plan.reticle->blend);
            layer(reticle_,std::span(&plan.reticle->packet,1));
        }
        // First person: MHUD's wing indicators (ModernFrame::cockpit).
        cockpit_active_=plan.cockpit.has_value();
        if(cockpit_active_) layer(cockpit_,std::span(&*plan.cockpit,1));
        engine_glow_active_=plan.engine_glow.has_value();
        if(engine_glow_active_) {
            engine_glow_.set_blend(plan.engine_glow->blend);
            if(!engine_glow_.initialize(device_,get_,memory_,pass_,std::span(&plan.engine_glow->packet,1),{},true,false))
                throw std::runtime_error(engine_glow_.status());
        }
        upload_overlay(circle_,plan.circle);
        upload_overlay(colour_math_,plan.colour_math);
        std::optional<ModernFrame::Overlay> shutter;
        if(plan.shutter) shutter=ModernFrame::Overlay{*plan.shutter,SceneBlend::opaque,true};
        upload_overlay(shutter_,shutter);
        landscape_heading_=plan.landscape_heading;texture_mapping_=plan.texture_mapping;
        status_="MODERN layers uploaded";return true;
    } catch(const std::exception& error) {status_=error.what();return false;}
}
bool VulkanModernLayers::record_overlay(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera,
    const Overlay& overlay,bool head_locked) const {
    // Head-locked overlays are normalized-device quads (identity camera).
    return head_locked?overlay.pipeline.record(command,extent,overlay.vertices.buffer(),overlay.vertices.count(),EyeCamera{identity,identity})
        :overlay.pipeline.record_model(command,extent,overlay.vertices.buffer(),overlay.vertices.count(),camera,overlay.model);
}
void VulkanModernLayers::apply_fog(VulkanSceneFog& fog,const ModernFrame& plan,float near_metres) noexcept {
    if(plan.fog) fog.set(plan.fog->colour,plan.fog->start,plan.fog->end,near_metres);
    else fog.clear();
}
bool VulkanModernLayers::record_filter(VkCommandBuffer command,const EyeCamera& camera) {
    if(!filter_enabled_) return true;
    // xBRZ at the eye's output pixels per source pixel (effects.w bits 8-19).
    const float pixel=float((camera.effects[3]>>8)&4095U)/16.F;
    if(!layer_filter_.filter(command,unsigned(std::lround(pixel)))) {status_=layer_filter_.status();return false;}
    return true;
}
bool VulkanModernLayers::record_stars(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera) const {
    return stars_.record(command,extent,camera);
}
bool VulkanModernLayers::record_backgrounds(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera,
    std::array<unsigned,2> world_effects) const {
    // The source camera heading for the landscape ground (effects.z); the
    // haze on the landscape's ground and sky dome (mode 2), as the flat
    // renderer draws its backgrounds.
    auto world=fogged(camera,0);
    world.effects={world_effects[0],world_effects[1],landscape_heading_,world.effects[3]};
    if(!backgrounds_.record(command,extent,fogged(world,2))) return false;
    if(textures_active_) {
        auto mapped=fogged(world,2);mapped.effects[2]=texture_mapping_;
        if(!textures_.record(command,extent,mapped)) return false;
    }
    // Masks only repeated background artwork; gameplay models stay free to
    // occupy the full view.
    return tunnel_surround_.record(command,extent,world);
}
bool VulkanModernLayers::record_after_world(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera) const {
    return !engine_glow_active_ || engine_glow_.record(command,extent,fogged(camera,0));
}
bool VulkanModernLayers::record_below_hud(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera) const {
    // MHUD above the world, below the circle and the HUD (as SdlFrameRenderer).
    if(cockpit_active_ && !cockpit_.record(command,extent,camera)) return false;
    if(circle_.active && !circle_.over_hud && !record_overlay(command,extent,camera,circle_,false)) return false;
    return !(colour_math_.active && !colour_math_.over_hud && !record_overlay(command,extent,camera,colour_math_,true));
}
bool VulkanModernLayers::record_hud(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera) const {
    if(filter_enabled_ && !layer_filter_.record(command,extent,camera)) return false;
    if(!sprites_.record(command,extent,camera)) return false;
    if(reticle_active_ && !reticle_.record(command,extent,camera)) return false;
    if(circle_.active && circle_.over_hud && !record_overlay(command,extent,camera,circle_,false)) return false;
    if(colour_math_.active && colour_math_.over_hud && !record_overlay(command,extent,camera,colour_math_,true)) return false;
    return !(shutter_.active && !record_overlay(command,extent,camera,shutter_,true));
}
}
