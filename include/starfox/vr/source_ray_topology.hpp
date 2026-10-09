#pragma once
#include "starfox/render/packed_faces.hpp"
#include <string>

namespace starfox::vr {
// Each entry contains three source corner indices and the source face ID.
// GPU expansion can fetch transformed points through corners[index].x and
// retain UV/material provenance. No camera-visible/BSP output is used here:
// a back-facing/offscreen surface may still cast a shadow.
inline bool source_ray_topology(const render::PackedFaces& faces,uint32_t point_count,
    std::vector<std::array<uint32_t,4>>& output,std::string& error) {
    if(faces.polygons.size()!=faces.primitives.size() || faces.polygons.size()>1'000'000) {
        error="Invalid ray face metadata";return false;
    }
    std::vector<std::array<uint32_t,4>> next;
    for(size_t face=0;face<faces.polygons.size();++face) {
        const auto& polygon=faces.polygons[face];
        const uint64_t first=polygon[0],count=polygon[1];
        if(first>faces.corners.size() || count>faces.corners.size()-first) {
            error="Ray corner range out of bounds";return false;
        }
        if(faces.primitives[face]!=render::PackedPrimitive::polygon) continue;
        if(count<3 || count>32 || next.size()+count-2>4'000'000) {
            error="Invalid ray polygon size";return false;
        }
        for(uint64_t corner=first;corner<first+count;++corner) if(faces.corners[corner][0]>=point_count) {
            error="Ray point index out of bounds";return false;
        }
        for(uint32_t corner=1;corner+1<count;++corner)
            next.push_back({uint32_t(first),uint32_t(first)+corner,uint32_t(first)+corner+1,uint32_t(face)});
    }
    output=std::move(next);error.clear();return true;
}
}
