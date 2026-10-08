#include "starfox/vr/vulkan_draw_packets.hpp"
#include "starfox/vr/scene_packet_validation.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/source_span_layout.hpp"
#include "starfox/vr/source_models.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include "starfox/vr/vulkan_scene_textures.hpp"
#include "starfox/vr/vulkan_connected_grid.hpp"
#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <cmath>
#include <bit>

namespace starfox::vr {
struct VulkanDrawPackets::State {
    struct Geometry {DrawPacket source;std::shared_ptr<VulkanSceneBuffer> triangles,lines;std::shared_ptr<VulkanConnectedGrid> grid;std::shared_ptr<VulkanSceneTextures> textures;bool ray_positions{};};
    struct Item {Matrix4 model;std::shared_ptr<Geometry> geometry;uint32_t key{};};
    struct Pipelines {VulkanSceneTextures layout;VulkanScenePipeline triangles,lines,photographs;
        bool triangles_ready{},lines_ready{},photographs_ready{};};
    VkDevice device{};PFN_vkGetDeviceProcAddr get{};VkRenderPass pass{};
    std::vector<Item> items;
    std::unordered_map<uint32_t,size_t> ray_items;
    std::shared_ptr<Pipelines> pipelines;
    std::shared_ptr<VulkanConnectedGridPipeline> grid_pipeline;
    std::size_t reused{},uploaded{};
    std::size_t vertex_uploads{},texture_uploads{};
    std::size_t grid_allocations{},grid_reuses{};
    bool keyed{};
    bool depth_test{true};
};
VulkanDrawPackets::VulkanDrawPackets()=default;
VulkanDrawPackets::~VulkanDrawPackets()=default;
void VulkanDrawPackets::close() noexcept {state_.reset();}
std::size_t VulkanDrawPackets::size() const noexcept {return state_?state_->items.size():0;}
std::size_t VulkanDrawPackets::reused_packets() const noexcept {return state_?state_->reused:0;}
std::size_t VulkanDrawPackets::uploaded_packets() const noexcept {return state_?state_->uploaded:0;}
std::size_t VulkanDrawPackets::uploaded_vertex_buffers() const noexcept {return state_?state_->vertex_uploads:0;}
std::size_t VulkanDrawPackets::uploaded_texture_buffers() const noexcept {return state_?state_->texture_uploads:0;}
std::size_t VulkanDrawPackets::allocated_grid_outputs() const noexcept {return state_?state_->grid_allocations:0;}
std::size_t VulkanDrawPackets::reused_grid_outputs() const noexcept {return state_?state_->grid_reuses:0;}
bool VulkanDrawPackets::ray_source(uint32_t key,RaySource& output) const {
    if(!state_ || !state_->keyed) return false;
    const auto found=state_->ray_items.find(key);
    if(found!=state_->ray_items.end()) {
        const auto& item=state_->items[found->second];
        const auto& geometry=*item.geometry;
        if(!geometry.triangles || !geometry.triangles->count() || !geometry.ray_positions) return false;
        output={{geometry.triangles->buffer(),0,uint64_t(geometry.triangles->count())*sizeof(SceneVertex)},
            geometry.triangles->count(),item.model};return true;
    }
    return false;
}
bool VulkanDrawPackets::update_models(std::span<const Matrix4> models) noexcept {
    if(!state_ || models.size()!=state_->items.size()) return false;
    for(const auto& model:models) if(!model_eye_camera(EyeCamera{},model)) return false;
    for(std::size_t i=0;i<models.size();++i) state_->items[i].model=models[i];
    return true;
}
bool VulkanDrawPackets::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& memory,VkRenderPass pass,std::span<const DrawPacket> packets,std::span<const uint32_t> object_keys,bool depth_test) {
    try {
        if(!device || !get || !pass || packets.size()>4096) throw std::runtime_error("Invalid native scene upload");
        if(!object_keys.empty()) {
            if(object_keys.size()!=packets.size()) throw std::runtime_error("Native scene key count mismatch");
            std::unordered_set<uint32_t> unique;
            for(auto key:object_keys) if(!unique.insert(key).second) throw std::runtime_error("Duplicate native scene object key");
        }
        // Shared with calibrated SDL rendering; no backend-specific
        // reinterpretation or allocation before all packets validate.
        ScenePacketValidator validator;
        for(const auto& packet:packets) validator.add(packet);
        auto next=std::make_unique<State>();next->items.reserve(packets.size());
        next->device=device;next->get=get;next->pass=pass;
        next->depth_test=depth_test;
        next->keyed=!object_keys.empty();
        const bool compatible=state_ && state_->device==device && state_->get==get && state_->pass==pass;
        if(compatible) next->grid_pipeline=state_->grid_pipeline;
        if(compatible && state_->depth_test==depth_test) next->pipelines=state_->pipelines;
        std::unordered_map<uint32_t,const State::Item*> prior;
        std::unordered_map<const std::vector<uint32_t>*,std::shared_ptr<VulkanSceneTextures>> shared_images;
        if(compatible) for(const auto& item:state_->items)
            if(!item.geometry->grid && item.geometry->source.geometry.shared_texels)
                shared_images.emplace(item.geometry->source.geometry.shared_texels.get(),item.geometry->textures);
        if(compatible && next->keyed && state_->keyed) {
            prior.reserve(state_->items.size());
            for(const auto& item:state_->items) prior.emplace(item.key,&item);
        }
        const uint32_t transparent=0;
        for(std::size_t packet_index=0;packet_index<packets.size();++packet_index) {
            const auto& packet=packets[packet_index];
            const auto& mesh=packet.geometry;
            const auto vertices=mesh.vertex_view();
            const auto texture_words=mesh.texel_view();
            if(vertices.empty() && mesh.line_view().empty()) continue;
            const auto index=next->items.size();
            const uint32_t key=next->keyed?object_keys[packet_index]:0;
            const State::Item* candidate=nullptr;
            if(next->keyed) {
                const auto found=prior.find(key);
                if(found!=prior.end()) candidate=found->second;
            } else if(compatible && !state_->keyed && index<state_->items.size()) candidate=&state_->items[index];
            if(candidate && same_draw_geometry(std::span<const DrawPacket>(&candidate->geometry->source,1),std::span<const DrawPacket>(&packet,1))) {
                if(candidate->geometry->grid) ++next->grid_reuses;
                next->items.push_back({packet.model,candidate->geometry,key});++next->reused;continue;
            }
            auto item=std::make_shared<State::Geometry>();item->source=packet;
            item->ray_positions=std::all_of(mesh.vertex_view().begin(),mesh.vertex_view().end(),[](const auto& vertex) {
                return vertex.visibility_enabled!=2 && !(vertex.texture[3]&~0x28000007U);
            });
            const auto texels=texture_words.empty()?std::span<const uint32_t>(&transparent,1):std::span<const uint32_t>(texture_words);
            if(!vertices.empty() && vertices.front().texture[3]==gpu_connected_grid_flag) {
                if(!next->grid_pipeline) {
                    if(compatible) next->grid_pipeline=state_->grid_pipeline;
                    if(!next->grid_pipeline) {
                        next->grid_pipeline=std::make_shared<VulkanConnectedGridPipeline>();
                        if(!next->grid_pipeline->initialize(device,get,cache_?cache_->get():VK_NULL_HANDLE))
                            throw std::runtime_error(next->grid_pipeline->status());
                    }
                }
                item->grid=std::make_shared<VulkanConnectedGrid>();
                if(!item->grid->initialize(device,get,memory,texels,next->grid_pipeline,
                    candidate?candidate->geometry->grid.get():nullptr)) throw std::runtime_error(item->grid->status());
                if(item->grid->reused_output()) {
                    ++next->grid_reuses;item->textures=candidate->geometry->textures;
                } else {
                    ++next->grid_allocations;
                    item->textures=std::make_shared<VulkanSceneTextures>();
                    if(!item->textures->initialize_external(device,get,item->grid->buffer(),item->grid->size())) throw std::runtime_error(item->textures->status());
                }
                ++next->texture_uploads;
            } else if(candidate && !candidate->geometry->grid && candidate->geometry->source.geometry.same_texels(mesh))
                item->textures=candidate->geometry->textures;
            else if(mesh.shared_texels && shared_images.contains(mesh.shared_texels.get()))
                item->textures=shared_images.at(mesh.shared_texels.get());
            else {
                item->textures=std::make_shared<VulkanSceneTextures>();
                if(!item->textures->initialize(device,get,memory,texels)) throw std::runtime_error(item->textures->status());
                ++next->texture_uploads;
            }
            if(!item->grid && mesh.shared_texels) shared_images.emplace(mesh.shared_texels.get(),item->textures);
            const auto prior_vertices=candidate?candidate->geometry->source.geometry.vertex_view():std::span<const SceneVertex>{};
            if(candidate && prior_vertices.size()==vertices.size()
                && (prior_vertices.data()==vertices.data() || std::equal(vertices.begin(),vertices.end(),prior_vertices.begin())))
                item->triangles=candidate->geometry->triangles;
            else if(!vertices.empty()) {
                item->triangles=std::make_shared<VulkanSceneBuffer>();
                if(!item->triangles->initialize(device,get,memory,vertices)) throw std::runtime_error(item->triangles->status());
                ++next->vertex_uploads;
            }
            const auto lines=mesh.line_view();
            const auto prior_lines=candidate?candidate->geometry->source.geometry.line_view():std::span<const SceneVertex>{};
            if(candidate && prior_lines.size()==lines.size()
                && (prior_lines.data()==lines.data() || std::equal(lines.begin(),lines.end(),prior_lines.begin())))
                item->lines=candidate->geometry->lines;
            else if(!lines.empty()) {
                item->lines=std::make_shared<VulkanSceneBuffer>();
                if(!item->lines->initialize(device,get,memory,lines)) throw std::runtime_error(item->lines->status());
                ++next->vertex_uploads;
            }
            next->items.push_back({packet.model,std::move(item),key});++next->uploaded;
        }
        if(!next->items.empty() && !next->pipelines) {
            auto pipelines=std::make_shared<State::Pipelines>();
            if(!pipelines->layout.initialize(device,get,memory,std::span<const uint32_t>(&transparent,1))) throw std::runtime_error(pipelines->layout.status());
            next->pipelines=std::move(pipelines);
        }
        // Most HUD/background passes never draw lines. Compile only topologies
        // actually present, retaining each pipeline when later packets change.
        for(const auto& item:next->items) {
            auto& pipelines=*next->pipelines;
            const auto layout=pipelines.layout.layout();
            const auto vertices=item.geometry->source.geometry.vertex_view();
            const bool photograph=!vertices.empty() && (vertices.front().texture[3]&backdrop_texture_flag)==backdrop_texture_flag;
            auto& triangles=photograph?pipelines.photographs:pipelines.triangles;
            auto& ready=photograph?pipelines.photographs_ready:pipelines.triangles_ready;
            if(item.geometry->triangles && !ready) {
                if(!triangles.initialize(device,get,pass,depth_test,SceneTopology::triangles,layout,
                    photograph?SceneBlend::alpha:SceneBlend::opaque,cache_)) throw std::runtime_error(triangles.status());
                ready=true;
            }
            if(item.geometry->lines && !pipelines.lines_ready) {
                if(!pipelines.lines.initialize(device,get,pass,depth_test,SceneTopology::lines,layout,SceneBlend::opaque,cache_))
                    throw std::runtime_error(pipelines.lines.status());
                pipelines.lines_ready=true;
            }
        }
        if(next->keyed) {
            next->ray_items.reserve(next->items.size());
            for(size_t i=0;i<next->items.size();++i) next->ray_items.emplace(next->items[i].key,i);
        }
        status_="Native scene uploaded";state_=std::move(next);return true;
    } catch(const std::exception& e) {status_=e.what();return false;}
}
bool VulkanDrawPackets::record(VkCommandBuffer commands,VkExtent2D extent,const EyeCamera& camera) const {
    return record_range(commands,extent,camera,0,size());
}
bool VulkanDrawPackets::record_compute(VkCommandBuffer commands) const {
    if(!state_ || !commands) return false;
    for(const auto& item:state_->items) if(item.geometry->grid && !item.geometry->grid->record(commands)) return false;
    return true;
}
bool VulkanDrawPackets::readback_connected_grid(std::size_t item,std::span<uint32_t> words) const {
    return state_ && item<state_->items.size() && state_->items[item].geometry->grid
        && state_->items[item].geometry->grid->readback(words);
}
bool VulkanDrawPackets::record_range(VkCommandBuffer commands,VkExtent2D extent,const EyeCamera& camera,
    std::size_t first,std::size_t count) const {
    if(!state_ || !commands || !extent.width || !extent.height) return false;
    if(first>state_->items.size() || count>state_->items.size()-first) return false;
    const auto items=std::span(state_->items).subspan(first,count);
    for(const auto& item:items) if(!model_eye_camera(camera,item.model) || (item.geometry->grid && !item.geometry->grid->prepared())) return false;
    for(const auto& item:items) {
        const auto& geometry=*item.geometry;
        auto draw_camera=camera;
        if(geometry.source.preserve_native_colour || (state_->keyed && is_source_shadow_pass(item.key))) draw_camera.effects={};
        const auto vertices=geometry.source.geometry.vertex_view();
        const bool photograph=!vertices.empty() && (vertices.front().texture[3]&backdrop_texture_flag)==backdrop_texture_flag;
        const auto& triangles=photograph?state_->pipelines->photographs:state_->pipelines->triangles;
        if(geometry.triangles && !triangles.record_model(commands,extent,geometry.triangles->buffer(),
            geometry.triangles->count(),draw_camera,item.model,geometry.textures->descriptor())) return false;
        if(geometry.lines && !state_->pipelines->lines.record_model(commands,extent,geometry.lines->buffer(),
            geometry.lines->count(),draw_camera,item.model,geometry.textures->descriptor())) return false;
    }
    return true;
}
}
