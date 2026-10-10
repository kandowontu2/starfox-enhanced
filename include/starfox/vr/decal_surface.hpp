#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <vector>

namespace starfox::vr {
struct DecalPoint {
    std::array<float,3> position;
    uint32_t pose{};
    auto operator<=>(const DecalPoint&) const = default;
};
using DecalSurface=std::vector<DecalPoint>;

// The inset rule below is a Steam Frame addition. Everywhere else decals keep
// the original exact-match rule, so the Frame loop switches it on at startup.
inline std::atomic<bool>& inset_decals_enabled() noexcept {
    static std::atomic<bool> enabled{false};
    return enabled;
}
inline void set_inset_decals(bool enabled) noexcept {inset_decals_enabled().store(enabled);}

// Authored painter-order overlays can repeat a face (character eyes), or be
// inset into it (BU_7's sign). With the inset rule on, the exact-match rule is
// extended to planar, convex backing polygons with a common transform. No
// camera/depth tolerance participates: epsilon only accommodates float rounding
// in local geometry.
inline bool decal_surface_contains(const DecalSurface& backing,const DecalSurface& overlay) {
    if(backing.size()<3 || overlay.size()<3) return false;
    if(backing.size()==overlay.size()) {
        auto a=backing,b=overlay;
        std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
        if(a==b) return true;
    }
    if(!inset_decals_enabled().load()) return false;
    using Vector=std::array<double,3>;
    const auto subtract=[](const auto& a,const auto& b) {
        return Vector{double(a[0])-b[0],double(a[1])-b[1],double(a[2])-b[2]};
    };
    const auto cross=[](const Vector& a,const Vector& b) {
        return Vector{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
    };
    const auto dot=[](const Vector& a,const Vector& b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    const auto& origin=backing.front();
    double extent=0,normal_length=0;Vector normal{};
    for(const auto& point:backing) {
        if(point.pose!=origin.pose) return false;
        const auto v=subtract(point.position,origin.position);
        extent=std::max(extent,std::sqrt(dot(v,v)));
    }
    for(size_t i=1;i+1<backing.size();++i) {
        const auto n=cross(subtract(backing[i].position,origin.position),subtract(backing[i+1].position,origin.position));
        const double length=std::sqrt(dot(n,n));
        if(length>normal_length) {normal=n;normal_length=length;}
    }
    if(!(normal_length>0) || !std::isfinite(extent)) return false;
    for(auto& component:normal) component/=normal_length;
    const double epsilon=extent*8*std::numeric_limits<float>::epsilon();
    const auto on_plane=[&](const DecalPoint& point) {
        return point.pose==origin.pose && std::abs(dot(normal,subtract(point.position,origin.position)))<=epsilon;
    };
    if(!std::all_of(backing.begin(),backing.end(),on_plane)
        || !std::all_of(overlay.begin(),overlay.end(),on_plane)) return false;
    for(size_t i=0;i<backing.size();++i) {
        const auto& start=backing[i].position;
        const auto edge=subtract(backing[(i+1)%backing.size()].position,start);
        const auto inward=cross(normal,edge);
        const double tolerance=epsilon*std::sqrt(dot(edge,edge));
        const auto inside=[&](const DecalPoint& point) {
            return dot(inward,subtract(point.position,start))>=-tolerance;
        };
        // Reject concave backing faces instead of accepting points merely
        // inside their bounding box or extending the source fan semantics.
        if(!std::all_of(backing.begin(),backing.end(),inside)
            || !std::all_of(overlay.begin(),overlay.end(),inside)) return false;
    }
    return true;
}
}
