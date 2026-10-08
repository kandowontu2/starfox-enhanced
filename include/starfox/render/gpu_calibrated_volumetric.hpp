#pragma once
#include "starfox/render/gpu_calibrated_scene.hpp"
#include "starfox/render/volumetric_fog.hpp"
#include <memory>
#include <string>
namespace starfox::render {
// Encodes geometry-occluded single scattering into the caller's command.
// Geometry is the same resident, fully transformed native ray allocation;
// the depth/ownership planes belong to this exact eye/sample's raster.
// No CPU geometry repack, BVH construction, screen upload, submit or wait.
class GpuCalibratedVolumetric {
public:
    GpuCalibratedVolumetric();~GpuCalibratedVolumetric();
    GpuCalibratedVolumetric(const GpuCalibratedVolumetric&)=delete;
    GpuCalibratedVolumetric& operator=(const GpuCalibratedVolumetric&)=delete;
    bool initialize(void* device,int color_format);
    bool enqueue(void* command,void* source,void* ownership,void* surfaces,void* destination,
        const CalibratedRayGeometryOutput&,const VolumetricMedium&,shadows::Vec3 toward_light,
        void** integral_output=nullptr);
    // Borrowed float4 scattering RGB / transmittance A, valid in this command
    // before the next enqueue cycles its backing. Zero density returns null.
    // Empty geometry must be explicit: complete=true, vertex_count=0, with
    // valid eye extent/projection. Unsupported/incomplete batches are rejected.
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
