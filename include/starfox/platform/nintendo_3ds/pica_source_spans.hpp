#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/render/software_renderer.hpp"

namespace starfox::platform::nintendo_3ds {
// Convert authored EX ink into native camera geometry, not a mono bitmap or
// an ordinary filled fan. The immutable active eye plan defines conservative
// preparation coverage; final clipping/projection still happens on the GPU.
std::vector<PicaVertex> pica_source_span_geometry(
    std::span<const render::ShapePrimitiveVertex>,const render::RenderPose&,
    double focal,std::array<double,2> source_origin,const FramePlan&,
    std::array<float,4> colour,unsigned vertex_budget=pica_vertex_limit);
}
