#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace starfox::render {
// Identity is independent of the compact render-list index. A particle can use
// {kind, birth_id, 0, 0}; weather uses {kind, cell_x, cell_y, cell_z}, retaining
// full signed cell coordinates rather than accepting hash collisions.
struct SceneMotionPoint {
    std::array<std::int64_t,4> identity{};
    std::array<float,3> projected{}; // stored X/Y, camera Z
    bool identified{};
};
struct SceneMotionSample {
    std::array<float,3> previous{};
    bool valid{};
};
class SceneMotionHistory {
public:
    struct Frame {
        std::uint64_t serial{},epoch{};
        unsigned width{},height{};
        bool paused{};
    };
    static bool valid_point(const SceneMotionPoint& point) noexcept {
        return point.identified && std::isfinite(point.projected[0])
            && std::isfinite(point.projected[1]) && std::isfinite(point.projected[2])
            && point.projected[2]>0;
    }
    std::vector<SceneMotionSample> prepare(std::span<const SceneMotionPoint> points,Frame frame) const {
        std::vector<SceneMotionSample> result(points.size());
        if(!valid_ || frame.paused || !frame.width || !frame.height
            || previous_.serial==UINT64_MAX || frame.serial!=previous_.serial+1
            || frame.epoch!=previous_.epoch || frame.width!=previous_.width || frame.height!=previous_.height)
            return result;
        for(std::size_t i=0;i<points.size();++i) {
            const auto& point=points[i];if(!valid_point(point)) continue;
            unsigned current_count=0,old_count=0;const SceneMotionPoint* old=nullptr;
            for(const auto& item:points) if(item.identified && item.identity==point.identity) ++current_count;
            for(const auto& item:points_) if(item.identified && item.identity==point.identity) {++old_count;old=&item;}
            if(current_count==1 && old_count==1 && valid_point(*old)) result[i]={old->projected,true};
        }
        return result;
    }
    // Call only for the frame actually displayed, never just after constructing
    // effects. A cancelled eye pair, pause, cut or missing presentation resets it.
    void commit(std::span<const SceneMotionPoint> points,Frame frame,bool presented) {
        if(!presented || frame.paused || !frame.width || !frame.height) {reset();return;}
        points_.assign(points.begin(),points.end());previous_=frame;valid_=true;
    }
    void reset() noexcept {points_.clear();valid_=false;}
private:
    std::vector<SceneMotionPoint> points_;
    Frame previous_{};
    bool valid_{};
};
// Apply off-axis projection to both ends independently: depth-changing
// particles have different previous/current disparities. Moving only the
// current point would manufacture an eye-separation velocity.
inline bool scene_motion_eye(SceneMotionPoint& current,SceneMotionSample& prior,
    double eye_x,double focal,double convergence) noexcept {
    if(!SceneMotionHistory::valid_point(current) || !std::isfinite(eye_x)
        || !std::isfinite(focal) || focal<=0 || !std::isfinite(convergence) || convergence<=0) return false;
    auto next=current.projected;auto old=prior.previous;
    const auto shift=[&](std::array<float,3>& point) {
        if(!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2]) || point[2]<=0) return false;
        const double x=point[0]+focal*eye_x*(1/convergence-1/double(point[2]));
        if(!std::isfinite(x) || std::abs(x)>std::numeric_limits<float>::max()) return false;
        point[0]=float(x);return true;
    };
    if(!shift(next) || (prior.valid && !shift(old))) return false;
    current.projected=next;if(prior.valid) prior.previous=old;return true;
}
}
