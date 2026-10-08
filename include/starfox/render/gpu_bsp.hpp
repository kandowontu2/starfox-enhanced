#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
struct GpuBspSettings {
    std::uint32_t tree_count{},node_count{},visibility_count{},face_count{};
    std::uint32_t output_count{},reserved[3]{};
};
struct GpuBspOutput {void* order{};void* results{};void* visibility{};void* points{};void* residuals{};};
// Node: uint4(visibility,fallthrough,alternate,firstFace),
//       uint4(faceCount,isLeaf,0,0). Missing links are UINT32_MAX.
// Tree: uint4(root,outputFirst,outputCapacity,workLimit).
// Tree output ranges must be disjoint and fit output_count.
// Results: uint2(count,status), status 0 succeeds; 1 invalid batch/range,
// 2 depth >64, 3 capacity exhausted, 4 work limit. Failure count is zero.
class GpuBsp {
public:
    GpuBsp();~GpuBsp();
    // Borrowed resident buffers on the caller's SDL command. No submit/wait or
    // readback. Consume before the next enqueue, cancel command on failure,
    // release before destroying device. Inputs must not alias owned outputs.
    GpuBspOutput enqueue(void* device,void* command,void* nodes,void* visibility,
        void* faces,void* trees,const GpuBspSettings& settings);
    // Single-model fused visibility/painter pass. Native points are int4;
    // continuous points are (float4 camera,float4 screen). Same source face
    // tests and failure statuses as separate stages; output visibility also
    // feeds clipping/material consumers. Rejects >4096 faces/multiple trees.
    GpuBspOutput enqueue_projected(void* device,void* command,void* nodes,
        void* points,void* visibility_faces,void* faces,void* trees,
        const GpuBspSettings& settings,std::uint32_t point_count,bool continuous);
    // Bounded single-group continuous projection/visibility/painter producer.
    // Exact same source arithmetic and outputs as separate stages; no implicit
    // fallbacks or submissions. Caller chooses the ordinary path for >128
    // points/faces, multiple trees and unsupported policies before encoding.
    GpuBspOutput enqueue_continuous_model(void* device,void* command,void* vertices,
        void* poses,std::uint32_t pose_count,void* nodes,void* visibility_faces,
        void* faces,void* trees,const GpuBspSettings&,std::uint32_t point_count,
        bool lossless_camera=true);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
static_assert(sizeof(GpuBspSettings)==32);
}
