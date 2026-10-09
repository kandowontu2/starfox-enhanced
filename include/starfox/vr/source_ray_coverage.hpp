#pragma once

#include "starfox/compat/bit_cast.hpp"
#include "starfox/vr/source_ray_topology.hpp"
#include "starfox/render/dxr_shadows.hpp"
#include <bit>

namespace starfox::vr {
// Coverage follows source index-zero transparency, not palette RGB/alpha.
// Input is the original face topology, never the camera-culled output.
struct SourceRayCoverage {
    std::vector<render::shadows::DxrShadows::TriangleCoverage> triangles;
    std::vector<uint32_t> texels;
    render::shadows::DxrShadows::Coverage view() const {return {triangles,texels};}
};
inline bool source_ray_coverage(const render::PackedFaces& faces,
    std::span<const std::array<uint32_t,4>> topology,SourceRayCoverage& output) {
    if(faces.materials.size()!=faces.polygons.size() || topology.size()>4'000'000
        || faces.texels.size()>16'000'000) return false;
    SourceRayCoverage next;
    next.triangles.reserve(topology.size());
    next.texels.reserve(faces.texels.size());
    for(auto index:faces.texels) next.texels.push_back(index?0xffffffffU:0U);
    for(const auto& triangle:topology) {
        if(triangle[3]>=faces.materials.size()) return false;
        const auto& material=faces.materials[triangle[3]];
        const auto& polygon=faces.polygons[triangle[3]];
        render::shadows::DxrShadows::TriangleCoverage coverage;
        if(material.textured>1) return false;
        coverage.flags=material.textured;
        if(coverage.flags) {
            if(material.u_mask>4095 || material.v_mask>4095
                || (material.u_mask&(material.u_mask+1)) || (material.v_mask&(material.v_mask+1))
                || uint64_t(material.texture_offset)+uint64_t(material.u_mask+1)*(material.v_mask+1)>faces.texels.size()) return false;
            coverage.offset=material.texture_offset;coverage.u_mask=material.u_mask;coverage.v_mask=material.v_mask;
        }
        for(unsigned corner=0;corner<3;++corner) {
            const auto index=triangle[corner];
            if(index>=faces.corners.size() || index<polygon[0] || uint64_t(index)>=uint64_t(polygon[0])+polygon[1]) return false;
            if(coverage.flags) for(unsigned axis=0;axis<2;++axis) {
                const auto scroll=starfox::bit_cast<int32_t>(axis?material.reserved1:material.reserved0);
                const double uv=double(faces.corners[index][axis+1])+scroll;
                if(uv < -1e8 || uv > 1e8) return false;
                coverage.uv[corner*2+axis]=float(uv);
            }
        }
        next.triangles.push_back(coverage);
    }
    output=std::move(next);return true;
}
}
