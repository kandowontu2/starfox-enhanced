#pragma once

#include "starfox/render/shadow_scene.hpp"
#include "starfox/render/row_workers.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace starfox::render::shadows {
struct ReceiverPlane { Vec3 point, normal; };
// Explicit calibrated camera depth planes (+Z forward), not normalized ray
// distances. Omitting this keeps each legacy producer's original limits.
struct PrimaryRayRange {
    double near_depth{1},far_depth{65536};
    bool valid() const noexcept {
        return std::isfinite(near_depth) && std::isfinite(far_depth) && near_depth>=0
            && near_depth<far_depth && far_depth<=std::numeric_limits<float>::max();
    }
};
struct Camera {
    std::uint32_t width{}, height{};
    double focal_length{256}, center_x{112}, center_y{96};
    double focal_length_y{}; // Zero retains square-pixel legacy projection.
    unsigned quality{2}; // Medium preserves the original eight-ray disk.
    unsigned shadow_softness{2}; // Hard / Low / Medium (original) / High.
    double shadow_angular_radius() const {return shadow_softness==0?0.:shadow_softness==1?.0075:shadow_softness==3?.03:.015;}
    unsigned shadow_samples() const {return shadow_softness==0?1U:quality==1?4U:quality==3?16U:8U;}
    double vertical_focal_length() const {return focal_length_y==0?focal_length:focal_length_y;}
};

// Geometry lives in camera space. Ground is supplied only in scenes with an
// actual ground receiver; space and menu backgrounds must not receive shadows.
// One entry per requested render pixel, independent of palette darkness.
// ground_only shades the revealed plane behind models, retaining those models
// as casters. It deliberately produces an empty mask when no plane exists.
inline void render_mask(const Scene& scene, Camera camera, Vec3 toward_light,
    std::optional<ReceiverPlane> ground, std::vector<std::uint8_t>& mask,
    RowWorkers* workers = nullptr, bool diagnostic_float_light_samples = false,
    bool ground_only = false, std::optional<PrimaryRayRange> primary_range = std::nullopt) {
    mask.assign(static_cast<std::size_t>(camera.width)*camera.height, 0);
    const auto range=primary_range.value_or(PrimaryRayRange{});
    if(!range.valid()) return;
    if(ground_only && !ground) return;
    const auto length=std::sqrt(dot(toward_light,toward_light));
    if (camera.focal_length<=0 || camera.vertical_focal_length()<=0 || !std::isfinite(camera.vertical_focal_length()) || !std::isfinite(length) || length<=1e-10) return;
    toward_light=toward_light*(1.0/length);
    const auto reference=std::abs(toward_light.y)<.9?Vec3{0,1,0}:Vec3{1,0,0};
    auto tangent=cross(toward_light,reference);
    tangent=tangent*(1.0/std::sqrt(dot(tangent,tangent)));
    const auto bitangent=cross(toward_light,tangent);
    std::array<Vec3,16> light_samples;
    const auto sample_count=camera.shadow_samples();
    for (unsigned i=0;i<sample_count;++i) {
        // Fixed disk samples avoid temporal noise at high presentation FPS.
        // Angular spread makes penumbrae widen with caster/receiver distance.
        const auto radius=camera.shadow_angular_radius()*std::sqrt((i+.5)/sample_count);
        const auto angle=i*2.399963229728653;
        auto direction=toward_light+tangent*(radius*std::cos(angle))
            +bitangent*(radius*std::sin(angle));
        light_samples[i]=direction*(1.0/std::sqrt(dot(direction,direction)));
        // Diagnostic-only: DXR uploads the final normalized samples as float.
        // Ordinary software shadows retain their original double precision.
        if(diagnostic_float_light_samples) {
            auto& sample=light_samples[i];
            sample={float(sample.x),float(sample.y),float(sample.z)};
        }
    }
    // Keep all eight samples and full resolution, but prepare their constant
    // triangle terms once per mask rather than for every receiver pixel.
    std::array<Scene::DirectionQuery,16> queries;
    for (unsigned i=0;i<sample_count;++i) queries[i]=scene.prepare_direction(light_samples[i]);
    const auto render_rows = [&](std::uint32_t first, std::uint32_t last) {
    for (std::uint32_t y=first;y<last;++y) {
        for (std::uint32_t x=0;x<camera.width;++x) {
            const Vec3 ray{(double(x)+.5-camera.center_x)/camera.focal_length,
                (double(y)+.5-camera.center_y)/camera.vertical_focal_length(),1};
            std::optional<double> ground_distance;
            if (ground) {
                const auto denominator=dot(ray,ground->normal);
                if (std::abs(denominator)>1e-10) {
                    const auto depth=dot(ground->point,ground->normal)/denominator;
                    if (depth>range.near_depth && depth<range.far_depth) ground_distance=depth;
                }
            }
            // Geometry behind the ground cannot be the visible receiver.
            // Use that exact depth to bound traversal, keeping the same plane
            // arithmetic and equal-depth result as the unbounded search.
            auto distance=ground_only?ground_distance:scene.nearest({},ray,range.near_depth,ground_distance.value_or(range.far_depth));
            if (ground_distance && (!distance || *ground_distance<*distance)) distance=ground_distance;
            if (!distance) continue;
            const auto receiver=ray * (*distance);
            const auto bias=std::max(.1,*distance*1e-5);
            unsigned blocked=0;
            for (unsigned sample=0;sample<sample_count;++sample)
                blocked+=scene.occluded(receiver,light_samples[sample],bias,65536.0,nullptr,&queries[sample])?1U:0U;
            mask[static_cast<std::size_t>(y)*camera.width+x]=
                static_cast<std::uint8_t>(160U*blocked/sample_count);
        }
    }
    };
    if (workers != nullptr) workers->parallel_rows(camera.height, render_rows);
    else render_rows(0, camera.height);
}
} // namespace starfox::render::shadows
