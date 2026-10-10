#pragma once
#include "starfox/render/gpu_scene.hpp"

namespace starfox::render {
// An explicitly complete zero-caster palette scene needs no geometry buffer.
// Incomplete, stale or native-payload descriptors are not interchangeable.
inline bool empty_indexed_ray_geometry(const GpuScene::RayGeometryOutput& geometry,
    void* device) noexcept {
    return device && geometry.complete && geometry.device==device
        && geometry.vertex_count==0 && !geometry.buffer
        && geometry.material_offset==0 && geometry.material_bytes==0
        && geometry.materials
        && geometry.materials->encoding==RayMaterialEncoding::indexed
        && geometry.materials->triangles.empty() && geometry.materials->texels.empty();
}
}
