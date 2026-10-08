#pragma once
#include "starfox/render/calibrated_game_scene.hpp"
#include "starfox/render/calibrated_motion.hpp"
#include "starfox/render/calibrated_ground.hpp"
#include <stdexcept>
#include <unordered_map>

namespace starfox::render {
// Match accepted source entities, not packet indices or a reusable object slot.
// Match authored primitive anchors/order as well as entity generation. GPU
// destruction/billboard payloads may change; topology, artwork, face membership
// and ambiguous keys may not. No vertices are transformed/projected on the CPU.
inline bool calibrated_primitive_correspondence(const vr::ShapeBatch& current,const vr::ShapeBatch& previous,
    bool& nonrigid,bool lines=false) {
    nonrigid=false;
    if((current.shared_vertices && !current.vertices.empty()) || (previous.shared_vertices && !previous.vertices.empty())
        || (current.shared_line_vertices && !current.line_vertices.empty()) || (previous.shared_line_vertices && !previous.line_vertices.empty())
        || !current.deferred.empty() || !previous.deferred.empty() || !current.same_texels(previous)) return false;
    const auto a=lines?current.line_view():current.vertex_view(),b=lines?previous.line_view():previous.vertex_view();
    if(a.size()!=b.size() || a.size()%(lines?2:3) || current.ranges.size()!=previous.ranges.size()) return false;
    for(std::size_t i=0;i<current.ranges.size();++i) {
        const auto& x=current.ranges[i];const auto& y=previous.ranges[i];
        if(x.source_face!=y.source_face || x.first_vertex!=y.first_vertex || x.vertex_count!=y.vertex_count
            || x.visibility_index!=y.visibility_index) return false;
    }
    constexpr unsigned supported=1U|2U|4U|4096U|32768U|134217728U|536870912U;
    const auto same=[](const auto& x,const auto& y) {return std::equal(std::begin(x),std::end(x),std::begin(y));};
    for(std::size_t i=0;i<a.size();++i) {
        const auto& x=a[i];const auto& y=b[i];
        // Local position/UV/corner are immutable primitive identifiers. Equal
        // vertex counts alone cannot establish correspondence after a reorder,
        // animation morph, LOD switch or BSP occurrence change.
        if((x.texture[3]&~supported) || !same(x.position,y.position) || !same(x.texture,y.texture)
            || !same(x.uv,y.uv) || !same(x.billboard,y.billboard) || x.visibility_enabled!=y.visibility_enabled
            || x.group_enabled!=y.group_enabled || x.color[3]!=y.color[3] || x.odd_color[3]!=y.odd_color[3]) return false;
        if(x.visibility_enabled==2) {
            // Rotation, object translation and interpolated phase are the
            // changing payload. The authored face normal, units and Q15 mode
            // still identify the same fragment of the same source shape.
            if(!same(x.group_b,y.group_b) || x.group_c[1]!=y.group_c[1] || x.group_c[2]!=y.group_c[2]
                || x.group_enabled) return false;
            nonrigid=true;
        } else {
            if(!same(x.visibility_a,y.visibility_a) || !same(x.visibility_b,y.visibility_b)
                || !same(x.visibility_c,y.visibility_c) || !same(x.group_b,y.group_b) || !same(x.group_c,y.group_c)) return false;
            if(x.texture[3]&134217728U) {
                // Source sprite size/depth are evaluated per accepted frame
                // before eye-facing expansion. Orientation/third component
                // and the original corner remain immutable.
                if(x.group_a[2]!=y.group_a[2] || x.group_enabled) return false;
                nonrigid=true;
            } else if(!same(x.group_a,y.group_a)) return false;
        }
        nonrigid|=(x.texture[3]&4U)!=0;
    }
    return true;
}
// Source identities/topology are independent of the calibrated eye. Prepare
// them once per accepted-history source, not once per eye/sample. This object
// borrows immutable frame geometry; it must stay within the caller's retained
// current/accepted frame lifetime and never survive a source/settings change.
// Camera-dependent mapping is still checked separately for each eye below.
class CalibratedGameMotionPreparation {
    struct Entry {
        vr::Matrix4 model{};
        std::optional<vr::EyeCamera> camera_override;
        CalibratedSceneMotionDraw source;
    };
    std::vector<Entry> entries_;
public:
    CalibratedGameMotionPreparation(const CalibratedGameFrame& current,const CalibratedGameFrame* accepted,
        std::span<const CalibratedScenePacket> packets) {
        if(packets.size()!=current.draws.size()) throw std::invalid_argument("Native motion packet count mismatch");
        for(std::size_t i=0;i<packets.size();++i)
            if(packets[i].packet!=&current.draws[i].packet)
                throw std::invalid_argument("Native motion packet identity mismatch");
        const auto old_packets=accepted?accepted->packets():std::vector<CalibratedScenePacket>{};
        std::vector<std::uint32_t> old_ray_first(old_packets.size(),std::numeric_limits<std::uint32_t>::max());
        std::uint64_t old_ray_count=0;
        for(std::size_t i=0;i<old_packets.size();++i) if(old_packets[i].ray_caster) {
            old_ray_first[i]=std::uint32_t(old_ray_count);
            old_ray_count+=old_packets[i].packet->geometry.vertex_view().size()/3
                +old_packets[i].packet->geometry.line_view().size(); // Two ribbon triangles per endpoint pair.
            if(old_ray_count>4'000'000/3) throw std::invalid_argument("Accepted native ray batch exceeds primitive bound");
        }
        struct Key {std::size_t index{},count{};};
        std::unordered_map<std::uint32_t,Key> now,old;
        std::size_t entry_count=0;
        for(std::size_t i=0;i<current.draws.size();++i) {
            const auto& draw=current.draws[i];auto& key=now[draw.source_key];key.index=i;++key.count;
            entry_count+=!draw.packet.geometry.vertex_view().empty()+!draw.packet.geometry.line_view().empty();
        }
        if(accepted) for(std::size_t i=0;i<accepted->draws.size();++i) {auto& key=old[accepted->draws[i].source_key];key.index=i;++key.count;}
        entries_.reserve(entry_count);
        for(std::size_t i=0;i<current.draws.size();++i) {
            const auto& draw=current.draws[i];CalibratedSceneMotionDraw match{};
            const auto prior=old.find(draw.source_key);
            bool valid=accepted && current.current && accepted->current && draw.source_key && draw.source_key<=0xffff
                && draw.layer==CalibratedGameLayer::model && draw.ray_caster && !draw.after_rays
                && draw.blend==vr::SceneBlend::opaque && now.at(draw.source_key).count==1
                && prior!=old.end() && prior->second.count==1;
            if(valid) {
                const auto& before=accepted->draws[prior->second.index];
                const auto a=current.current->transforms.find(simulation::ObjectHandle(draw.source_key));
                const auto b=accepted->current->transforms.find(simulation::ObjectHandle(draw.source_key));
                valid=a!=current.current->transforms.end() && b!=accepted->current->transforms.end()
                    && a->second.generation && a->second.generation==b->second.generation && a->second.shape==b->second.shape
                    && a->second.strategy_address==b->second.strategy_address && a->second.type==b->second.type
                    && before.layer==draw.layer && before.blend==draw.blend && before.depth_test==draw.depth_test
                    && before.ray_caster==draw.ray_caster && before.after_rays==draw.after_rays
                    && !draw.packet.preserve_native_colour && !before.packet.preserve_native_colour;
                bool nonrigid{};
                if(valid) valid=calibrated_primitive_correspondence(draw.packet.geometry,before.packet.geometry,nonrigid);
                if(valid && (a->second.explosion_progress || b->second.explosion_progress)) {
                    const auto vertices=draw.packet.geometry.vertex_view();
                    // Line-only shapes have no triangle payload. Their destruction
                    // correspondence is checked against the endpoints below.
                    valid=a->second.explosion_progress && b->second.explosion_progress
                        && std::all_of(vertices.begin(),vertices.end(),[](const auto& v){return v.visibility_enabled==2;});
                }
                if(valid) {
                    const auto& packet=old_packets[prior->second.index];
                    match.previous_model=packet.model_override.value_or(before.packet.model);
                    match.previous_camera_override=packet.camera_override;
                    if(nonrigid) match.previous_vertices=before.packet.geometry.vertex_view();
                    match.previous_ray_first_triangle=old_ray_first[prior->second.index];
                }
            }
            // An authored finite floor is not a reusable model/object slot. Sky,
            // ambiguous floor occurrences and liquids cannot borrow its motion.
            if(accepted && current.current && accepted->current && draw.source_key==calibrated_ground_source_key
                && draw.layer==CalibratedGameLayer::world && !draw.ray_caster && !draw.after_rays
                && draw.blend==vr::SceneBlend::opaque && !draw.packet.preserve_native_colour
                && now.at(draw.source_key).count==1 && prior!=old.end() && prior->second.count==1) {
                const auto& before=accepted->draws[prior->second.index];
                const auto& a=*current.current;const auto& b=*accepted->current;
                const auto& packet=old_packets[prior->second.index];
                const auto model=calibrated_ground_previous_model(draw.packet,before.packet,
                    packet.model_override.value_or(before.packet.model));
                valid=model && before.layer==draw.layer && before.blend==draw.blend
                    && before.depth_test==draw.depth_test && !before.ray_caster && !before.after_rays
                    && a.scene_epoch==b.scene_epoch && a.flow==b.flow && a.background_id==b.background_id
                    && current.settings.history_epoch==accepted->settings.history_epoch
                    && !timing::camera_transform_is_discontinuous(b.camera,a.camera);
                if(valid) {
                    match.previous_model=*model;match.previous_camera_override=packet.camera_override;
                }
            }
            match.valid=valid;
            const auto append=[&](const CalibratedSceneMotionDraw& source) {
                entries_.push_back({packets[i].model_override.value_or(draw.packet.model),packets[i].camera_override,source});
            };
            // upload_packets expands each nonempty triangle/line batch in this order.
            if(!draw.packet.geometry.vertex_view().empty()) append(match);
            if(!draw.packet.geometry.line_view().empty()) {
                // Raster lines are the ORIGINAL one-pixel segments, not the ray
                // caster's eye-facing ribbons. Interpolate accepted endpoints on
                // GPU; a pixel-centre rigid plane mapping invents off-segment motion.
                bool nonrigid{};
                match.previous_vertices={};
                if(match.valid) {
                    const auto& before=accepted->draws[prior->second.index];
                    match.valid=calibrated_primitive_correspondence(draw.packet.geometry,before.packet.geometry,nonrigid,true);
                    const auto a=current.current->transforms.find(simulation::ObjectHandle(draw.source_key));
                    const auto b=accepted->current->transforms.find(simulation::ObjectHandle(draw.source_key));
                    match.valid=match.valid && draw.layer==CalibratedGameLayer::model
                        && a!=current.current->transforms.end() && b!=accepted->current->transforms.end();
                    if(match.valid && (a->second.explosion_progress || b->second.explosion_progress))
                        match.valid=a->second.explosion_progress && b->second.explosion_progress
                            && std::all_of(draw.packet.geometry.line_view().begin(),draw.packet.geometry.line_view().end(),
                                [](const auto& v){return v.visibility_enabled==2;});
                    if(match.valid) match.previous_vertices=before.packet.geometry.line_view();
                    if(match.valid) match.previous_ray_first_triangle=old_ray_first[prior->second.index]
                        +std::uint32_t(before.packet.geometry.vertex_view().size()/3);
                }
                if(!match.valid) match.previous_ray_first_triangle=std::numeric_limits<std::uint32_t>::max();
                append(match);
            }
        }
    }
    std::vector<CalibratedSceneMotionDraw> draws(const vr::EyeCamera& camera,
        const vr::EyeCamera& previous_camera) const {
        std::vector<CalibratedSceneMotionDraw> result;result.reserve(entries_.size());
        for(const auto& entry:entries_) {
            auto match=entry.source;
            if(match.valid && !calibrated_motion_mapping(entry.camera_override.value_or(camera),
                match.previous_camera_override.value_or(previous_camera),entry.model,match.previous_model)) {
                match.valid=false;match.previous_vertices={};
                match.previous_ray_first_triangle=std::numeric_limits<std::uint32_t>::max();
            }
            result.push_back(match);
        }
        return result;
    }
};
inline std::vector<CalibratedSceneMotionDraw> calibrated_game_motion_draws(
    const CalibratedGameFrame& current,const CalibratedGameFrame* accepted,
    const vr::EyeCamera& camera,const vr::EyeCamera& previous_camera) {
    const auto packets=current.packets();
    return CalibratedGameMotionPreparation(current,accepted,packets).draws(camera,previous_camera);
}
inline bool calibrated_game_uses_effect_clock(const CalibratedGameSettings& s) noexcept {
    const auto selected=[](unsigned effect,unsigned intensity=100) {
        // Neighbourhood taps and the authored static coordinate transforms
        // need correspondence handling, but do NOT consume presentation time.
        // Only persistence and the eight animated shader FX actually do.
        return intensity && (persistence_mode(static_cast<Effect>(effect))
            || (effect>=unsigned(Effect::energy_shield) && effect<=unsigned(Effect::gravitational_lens))
            || effect>=effect_count);
    };
    constexpr unsigned animated_globals=(3U<<10)|(3U<<16); // authored Heat Haze and Film Grain
    return (s.global_enhancements&animated_globals) || s.phosphor || s.exposure || selected(s.manipulation,s.manipulation_intensity)
        || selected(s.world_effects[0],s.world_effects[1]) || selected(s.model_effects[0],s.model_effects[1])
        || selected(s.extra_effects[0]) || selected(s.extra_effects[1]) || selected(s.extra_effects[2]);
}
inline bool same_calibrated_presentation(const CalibratedGameFrame& a,const CalibratedGameFrame& b) {
    auto a_settings=a.settings,b_settings=b.settings;
    // A continuously ticking host FX clock is not motion when no time-driven
    // pass consumes it. Static previews must not cycle sample phase forever.
    if(!calibrated_game_uses_effect_clock(a_settings) && !calibrated_game_uses_effect_clock(b_settings))
        a_settings.effect_seconds=b_settings.effect_seconds=0;
    if(a.current!=b.current || a.previous!=b.previous || a.alpha!=b.alpha || a.clear!=b.clear
        || a_settings!=b_settings || a.source_light!=b.source_light || a.draws.size()!=b.draws.size()
        || a.scene_fx!=b.scene_fx || a.scene_fx_origin!=b.scene_fx_origin || a.scene_fx_source_to_rig!=b.scene_fx_source_to_rig) return false;
    for(std::size_t i=0;i<a.draws.size();++i) {
        const auto& x=a.draws[i];const auto& y=b.draws[i];
        if(x.packet.model!=y.packet.model || x.source_key!=y.source_key || x.layer!=y.layer
            || x.blend!=y.blend || x.depth_test!=y.depth_test || x.ray_caster!=y.ray_caster
            || x.after_rays!=y.after_rays || x.reflection_environment!=y.reflection_environment
            || !vr::same_draw_geometry(std::span(&x.packet,1),std::span(&y.packet,1))) return false;
    }
    return true;
}
}
