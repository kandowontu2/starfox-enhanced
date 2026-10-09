#pragma once
#include "starfox/platform/nintendo_3ds/frontend.hpp"
#include <algorithm>
#include <limits>

namespace starfox::platform::nintendo_3ds {
using Point3=std::array<float,3>;
struct Bounds3 {Point3 minimum,maximum;};

// Graphics-independent PICA200 projection contract. Rows multiply a column
// vector in cartridge camera coordinates (X-right, Y-up, Z-forward). Upload
// x/y/z/w by name to C3D_Mtx: C3D_FVec's in-memory order is not this row layout.
// The sideways LCD requires a clockwise quarter-turn, and PICA clips depth to
// [-w,0], not the desktop [0,w] or OpenGL [-w,w] interval.
class PicaProjection {
public:
    using Rows=std::array<std::array<float,4>,4>;
    using ClipPoint=std::array<double,4>;
    PicaProjection(const FramePlan& frame,unsigned eye) {
        if(frame.eye_count!=(frame.stereo?2U:1U) || eye>=frame.eye_count
            || !std::isfinite(frame.near_plane) || !std::isfinite(frame.far_plane)
            || frame.near_plane<=0 || frame.far_plane<=frame.near_plane
            || frame.far_plane>65536 || !std::isfinite(frame.focal_x)
            || !std::isfinite(frame.focal_y) || frame.focal_x<=0 || frame.focal_y<=0
            || !std::isfinite(frame.eyes[eye].x) || !std::isfinite(frame.eyes[eye].projection_offset))
            throw std::invalid_argument("Invalid 3DS GPU projection plan");
        const double horizontal=double(frame.focal_x)/(top_width*.5);
        const double vertical=double(frame.focal_y)/(screen_height*.5);
        const double depth_range=double(frame.far_plane)-frame.near_plane;
        const std::array<ClipPoint,4> source{{
            {0,vertical,0,0},
            {-horizontal,0,-double(frame.eyes[eye].projection_offset)/(top_width*.5),
                horizontal*frame.eyes[eye].x},
            {0,0,frame.near_plane/depth_range,-double(frame.near_plane)*frame.far_plane/depth_range},
            {0,0,1,0}}};
        for(unsigned row=0;row<4;++row) for(unsigned column=0;column<4;++column) {
            const auto value=source[row][column];
            if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max())
                throw std::invalid_argument("Unrepresentable 3DS GPU projection");
            rows_[row][column]=float(value);
        }
        for(unsigned column=0;column<4;++column) {
            planes_[0][column]=double(rows_[3][column])+rows_[0][column];
            planes_[1][column]=double(rows_[3][column])-rows_[0][column];
            planes_[2][column]=double(rows_[3][column])+rows_[1][column];
            planes_[3][column]=double(rows_[3][column])-rows_[1][column];
            planes_[4][column]=double(rows_[3][column])+rows_[2][column];
            planes_[5][column]=-double(rows_[2][column]);
        }
    }
    [[nodiscard]] const Rows& rows() const noexcept {return rows_;}
    [[nodiscard]] std::optional<ClipPoint> clip_position(Point3 point) const noexcept {
        for(float value:point) if(!std::isfinite(value)) return std::nullopt;
        ClipPoint result{};
        for(unsigned row=0;row<4;++row) {
            result[row]=rows_[row][3];
            for(unsigned column=0;column<3;++column) result[row]+=double(rows_[row][column])*point[column];
        }
        return result;
    }
    // Conservative test of a camera-local bound. The GPU still clips the
    // triangles; intersecting a bound never means all its points are visible.
    [[nodiscard]] bool intersects(const Bounds3& bounds) const {
        validate(bounds);
        for(const auto& plane:planes_) {
            double farthest=plane[3],magnitude=std::abs(plane[3]);
            for(unsigned axis=0;axis<3;++axis) {
                const double term=plane[axis]*(plane[axis]>=0?bounds.maximum[axis]:bounds.minimum[axis]);
                farthest+=term;magnitude+=std::abs(term);
            }
            // Float uniforms must not cull an on-edge bound through rounding.
            if(farthest < -1.e-5*std::max(1.,magnitude)) return false;
        }
        return true;
    }
    // Diagnostic line clipping happens before division by Z. Rejecting a
    // whole primitive when just one endpoint crosses the near plane loses
    // visible geometry and is not an acceptable game renderer strategy.
    [[nodiscard]] std::optional<std::array<std::array<float,2>,2>> project_segment(
        Point3 a,Point3 b) const noexcept {
        for(float value:a) if(!std::isfinite(value)) return std::nullopt;
        for(float value:b) if(!std::isfinite(value)) return std::nullopt;
        double begin=0,end=1;
        for(const auto& plane:planes_) {
            double da=plane[3],db=plane[3];
            for(unsigned axis=0;axis<3;++axis) {
                da+=plane[axis]*a[axis];db+=plane[axis]*b[axis];
            }
            if(da<0 && db<0) return std::nullopt;
            if((da<0)!=(db<0)) {
                const double crossing=da/(da-db);
                if(da<0) begin=std::max(begin,crossing);else end=std::min(end,crossing);
                if(begin>end) return std::nullopt;
            }
        }
        std::array<std::array<float,2>,2> result{};
        for(unsigned endpoint=0;endpoint<2;++endpoint) {
            const double t=endpoint?end:begin;
            ClipPoint clip{};
            for(unsigned row=0;row<4;++row) {
                clip[row]=rows_[row][3];
                for(unsigned axis=0;axis<3;++axis)
                    clip[row]+=double(rows_[row][axis])*(a[axis]+t*(double(b[axis])-a[axis]));
            }
            if(clip[3]<=0) return std::nullopt;
            // Undo the LCD quarter-turn to compare/render in top-left pixels.
            result[endpoint]={float(top_width*.5*(1-clip[1]/clip[3])),
                float(screen_height*.5*(1-clip[0]/clip[3]))};
        }
        return result;
    }
private:
    static void validate(const Bounds3& bounds) {
        for(unsigned axis=0;axis<3;++axis)
            if(!std::isfinite(bounds.minimum[axis]) || !std::isfinite(bounds.maximum[axis])
                || bounds.minimum[axis]>bounds.maximum[axis])
                throw std::invalid_argument("Invalid 3DS scene bound");
    }
    Rows rows_{};
    std::array<ClipPoint,6> planes_{};
};

// Prepare/cull geometry once for the UNION of active eyes. Culling against a
// mono frustum (or only the left eye) can erase the right eye's visible edge.
// No heap storage, per-eye scene copy, or second-eye work in mono mode.
class StereoFrustum {
public:
    explicit StereoFrustum(const FramePlan& frame) {
        eyes_[0].emplace(frame,0);
        if(frame.stereo) eyes_[1].emplace(frame,1);
    }
    [[nodiscard]] bool intersects(const Bounds3& bounds) const {
        return eyes_[0]->intersects(bounds) || (eyes_[1] && eyes_[1]->intersects(bounds));
    }
private:
    std::array<std::optional<PicaProjection>,2> eyes_;
};
} // namespace starfox::platform::nintendo_3ds
