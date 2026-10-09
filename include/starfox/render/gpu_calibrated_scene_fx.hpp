#pragma once
#include "starfox/render/scene_enhancements.hpp"
#include "starfox/vr/eye_camera.hpp"
#include <memory>
#include <string>
namespace starfox::render {
struct CalibratedSceneFxPrevious {
    const SceneFxWorldFrame* world{};
    vr::EyeCamera camera;
    vr::Matrix4 source_to_rig;
    std::array<double,3> origin{};
};
// Resident native scene enhancements. Raw source-space event points are
// projected/compacted on the GPU independently for every tracked eye/sample.
// source_to_rig maps wrapped (point-source_origin) to the game's metre rig;
// only its small affine matrix is composed on the CPU, never individual points.
// Encodes only. The existing frame owner owns submission and retained images.
class GpuCalibratedSceneFx {
public:
    GpuCalibratedSceneFx();~GpuCalibratedSceneFx();
    GpuCalibratedSceneFx(const GpuCalibratedSceneFx&)=delete;
    GpuCalibratedSceneFx& operator=(const GpuCalibratedSceneFx&)=delete;
    bool initialize(void* device,int color_format);
    // Optional GPU-only output: 2 float4 camera/meta rows then 48*3 point rows.
    // Borrowed from this owner; bind/copy it in the SAME command before the
    // next enqueue cycles the backing. No CPU readback is done by this class.
    bool enqueue(void* command,void* source,void* ownership,void* surfaces,void* destination,
        unsigned width,unsigned height,unsigned scale,const SceneFxWorldFrame&,
        const vr::EyeCamera&,const vr::Matrix4& source_to_rig,std::array<double,3> source_origin,
        void** projected_output=nullptr);
    // Particle-only joint exposure payload, projected on GPU from accepted
    // source points/eye matrices. Stable unique identities establish prior
    // correspondence; newly emitted/ambiguous points remain current-only.
    // Layout: camera, focal-Y/X ratio, 144 point rows, 48 previous xyz/valid
    // rows. Previous xy are real pixels; current y retains focal-X coordinates.
    // Borrowed until next projection; consume in the same ordered command.
    bool project_particles(void* command,unsigned width,unsigned height,unsigned scale,
        const SceneFxWorldFrame&,const vr::EyeCamera&,const vr::Matrix4& source_to_rig,
        std::array<double,3> source_origin,const CalibratedSceneFxPrevious*,void*& output);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
