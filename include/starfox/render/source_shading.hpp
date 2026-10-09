#pragma once

#include "starfox/compat/bit_cast.hpp"
#include "starfox/render/software_renderer.hpp"
#include <bit>

namespace starfox::render {
struct SourceShading {
    std::size_t depth_band{};
    std::array<std::int8_t,3> light{73,73,73};
};
// Source lighting is sampled at the completed logic tick, independently of
// interpolated geometry and headset eye position.
inline SourceShading source_shading(const RenderPose& pose) noexcept {
    SourceShading result;
    const auto depth=pose.use_source_lighting_state?pose.source_depth:pose.z;
    while(result.depth_band<pose.depth_thresholds.size()
        && depth>=pose.depth_thresholds[result.depth_band]) ++result.depth_band;
    if(pose.use_rotation_matrix) {
        const auto& matrix=pose.use_source_lighting_state?pose.source_lighting_matrix:pose.rotation_matrix;
        // initlight consumes rows, unlike point rotation's columns.
        const auto transformed=simulation::transform_q15(simulation::transpose_q15(matrix),{18'917,18'917,18'917});
        for(std::size_t i=0;i<result.light.size();++i)
            result.light[i]=starfox::bit_cast<std::int8_t>(static_cast<std::uint8_t>(starfox::bit_cast<std::uint16_t>(transformed[i])>>8U));
    }
    return result;
}
}
