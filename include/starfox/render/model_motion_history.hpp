#pragma once

#include "starfox/render/gpu_scene.hpp"
#include <unordered_map>
#include <unordered_set>
#include <limits>

namespace starfox::render {

// History belongs to successfully submitted presentation frames, not native
// simulation ticks (which can contain several 60/240Hz presentation frames).
// Caller changes epoch on load-state, scene/camera cut, experience, and stereo
// configuration changes. Do not commit a failed/cancelled GPU submission.
class ModelMotionHistory {
public:
    struct Frame {
        std::uint64_t serial{}, epoch{};
        std::uint32_t width{}, height{};
    };

    // Returned pose is borrowed until commit/reset. Missing history is a
    // disocclusion/reset condition, NOT evidence of a zero-motion surface.
    const RenderPose* previous(const GpuModelDraw& draw, Frame frame) const {
        if (!valid_ || frame_.serial == std::numeric_limits<std::uint64_t>::max()
            || frame.serial != frame_.serial + 1 || frame.epoch != frame_.epoch
            || frame.width != frame_.width || frame.height != frame_.height
            || !draw.identity || !draw.shape) return nullptr;
        const auto found = models_.find(draw.identity->slot);
        if (found == models_.end()) return nullptr;
        const auto& old = found->second;
        if (old.identity != *draw.identity || old.shape != draw.shape
            // Static models still carry the advancing global animation counter.
            // Compare selected geometry, not the counter's raw value.
            || (!draw.shape->frames.empty() && old.pose.animation_frame % draw.shape->frames.size()
                != draw.pose.animation_frame % draw.shape->frames.size())
            || old.pose.explosion_progress != draw.pose.explosion_progress
            || old.pose.simple_scaled_sprite != draw.pose.simple_scaled_sprite
            || old.settings.render_scale != draw.settings.render_scale
            || old.settings.focal_length != draw.settings.focal_length)
            return nullptr;
        return &old.pose;
    }

    // Preparation never advances history. Current duplicate identities are
    // rejected too, not merely duplicates in the previous committed frame.
    std::vector<GpuSceneDraw> prepare(std::span<const GpuSceneDraw> draws,Frame frame) const {
        std::unordered_map<std::uint16_t,unsigned> counts;
        for(const auto& item:draws) if(const auto* draw=std::get_if<GpuModelDraw>(&item))
            if(draw->identity) ++counts[draw->identity->slot];
        std::vector<GpuSceneDraw> result(draws.begin(),draws.end());
        for(auto& item:result) if(auto* draw=std::get_if<GpuModelDraw>(&item)) {
            draw->geometry_depth=true;draw->previous_pose.reset();
            if(draw->identity && counts[draw->identity->slot]==1)
                if(const auto* pose=previous(*draw,frame)) draw->previous_pose=*pose;
        }
        return result;
    }
    void commit(std::span<const GpuSceneDraw> draws, Frame frame) {
        reset();
        if (!frame.width || !frame.height) return;
        // An ambiguous identity must invalidate BOTH draws, not select whichever
        // was last in painter order. Unidentified helpers never enter history.
        std::unordered_set<std::uint16_t> ambiguous;
        for (const auto& item : draws) {
            const auto* draw = std::get_if<GpuModelDraw>(&item);
            if (!draw || !draw->identity || !draw->shape) continue;
            const auto slot = draw->identity->slot;
            if (ambiguous.contains(slot)) continue;
            const auto [entry, inserted] = models_.emplace(slot,
                Entry{*draw->identity, draw->shape, draw->pose, draw->settings});
            if (!inserted) {
                models_.erase(entry);
                ambiguous.insert(slot);
            }
        }
        frame_ = frame;
        valid_ = true;
    }

    void reset() noexcept { models_.clear(); valid_ = false; }

private:
    struct Entry {
        GpuModelIdentity identity;
        const assets::Shape* shape{};
        RenderPose pose;
        RenderSettings settings;
    };
    std::unordered_map<std::uint16_t, Entry> models_;
    Frame frame_{};
    bool valid_{};
};
} // namespace starfox::render
