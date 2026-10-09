#pragma once
#include "starfox/render/volumetric_fog.hpp"
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
// Borrowed float4 buffer: linear scattering RGB, transmittance A. Valid until
// the next dispatch/release. Release this owner before destroying its device.
struct GpuVolumetricOutput {void* device{};void* buffer{};unsigned width{},height{};};
// Immutable packed source BVH, borrowed within one presentation transaction.
// Consumers have separate integral outputs; neither an eye translation nor an
// underlay requires rebuilding/reuploading this source scene. Keep the producer
// alive and do not republish its scene until all consumers have been submitted.
// Encoding/republication are serialized on the same ordered SDL device queue.
struct GpuVolumetricSceneOutput {
    void* device{};void* nodes{};void* triangles{};
    std::uint32_t node_count{},triangle_count{},node_bytes{},triangle_bytes{};
    bool complete{};
};
class GpuVolumetricFog {
public:
    GpuVolumetricFog();
    ~GpuVolumetricFog();
    bool render(void* device,const shadows::Scene&,VolumetricProjection,unsigned width,unsigned height,
        const VolumetricMedium&,shadows::Vec3 light,std::optional<VolumetricGround>,bool background_only=false,
        shadows::Vec3 eye_origin={});
    // Projection is this eye's off-axis projection. Origin/ground are expressed
    // in the shared source scene, not a second translated CPU scene. Primary
    // and light rays both use the origin; underlays omit only primary hits.
    bool render_resident(void* device,const GpuVolumetricSceneOutput&,VolumetricProjection,
        unsigned width,unsigned height,const VolumetricMedium&,shadows::Vec3 light,
        std::optional<VolumetricGround>,bool background_only=false,shadows::Vec3 eye_origin={});
    GpuVolumetricSceneOutput scene_output() const;
    GpuVolumetricOutput output() const;
    bool readback(std::vector<std::array<float,4>>&);
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
