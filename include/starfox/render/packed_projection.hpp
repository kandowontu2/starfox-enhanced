#pragma once
#include "starfox/render/gpu_projection.hpp"
#include "starfox/render/software_renderer.hpp"
#include <span>
namespace starfox::render {
class PreparedProjectionSource;
struct PackedProjection {
    bool continuous{};
    std::vector<NativeTransformVertex> native_vertices;
    NativeTransformPose native_pose{};
    std::vector<ContinuousTransformVertex> continuous_vertices;
    // Byte vertices use pose 0; word vertices use pose 1, bypassing both
    // object scale and header shift exactly as the source renderer does.
    // Poses 2/3 carry coefficient residuals for poses 0/1.
    std::array<ContinuousTransformPose,4> continuous_poses{};
    // High/low source Euler operands for a sequential X/Y/Z GPU transform.
    // translation.xyz hold third residuals: cosine in operand 0, sine in 1.
    // Separate from matrix poses so existing destruction records keep layout.
    std::array<ContinuousTransformPose,2> euler_operands{};
    std::vector<std::array<std::uint32_t,4>> visibility_faces;
    // Only a pair-scoped immutable animation source, never a packed eye pose.
    // Mutable destruction/axis expansion must materialize before editing.
    const PreparedProjectionSource* source{};
    [[nodiscard]] std::span<const NativeTransformVertex> native_input() const noexcept;
    [[nodiscard]] std::span<const ContinuousTransformVertex> continuous_input() const noexcept;
    [[nodiscard]] std::span<const std::array<std::uint32_t,4>> visibility_input() const noexcept;
    void own_source();
};
// Untransformed frame coordinates and source visibility descriptors only.
// Native coordinates include the exact word prescale; continuous coordinates
// keep scaling in EACH draw's GPU transform. Never retain across source mutation.
class PreparedProjectionSource {
public:
    PreparedProjectionSource(const assets::Shape&,const RenderPose&,const RenderSettings&);
    [[nodiscard]] bool matches(const assets::Shape&,const RenderPose&,const RenderSettings&) const noexcept;
    [[nodiscard]] bool byte_coordinates() const noexcept {return byte_coordinates_;}
    [[nodiscard]] bool continuous() const noexcept {return continuous_mode_;}
    [[nodiscard]] std::uint64_t storage_bytes() const noexcept;
    [[nodiscard]] std::span<const NativeTransformVertex> native_input() const noexcept {return native_;}
    [[nodiscard]] std::span<const ContinuousTransformVertex> continuous_input() const noexcept {return continuous_;}
    [[nodiscard]] std::span<const std::array<std::uint32_t,4>> visibility_input() const noexcept {return visibility_;}
private:
    const assets::Shape* source_{};
    std::size_t frame_{};
    double scale_{};
    bool byte_coordinates_{},continuous_mode_{};
    std::vector<NativeTransformVertex> native_;
    std::vector<ContinuousTransformVertex> continuous_;
    std::vector<std::array<std::uint32_t,4>> visibility_;
};
// Package the selected animation frame for GPU transform/projection. No CPU
// rotation or projection is performed per vertex. Native pre-scale/word
// quantization is currently done here; continuous scale stays in GPU matrices.
// Currently accepts source Q15 poses and continuous Q15/Euler poses. Rejects
// other projection/visibility combinations instead of silently changing them.
[[nodiscard]] PackedProjection pack_projection(const assets::Shape&,const RenderPose&,const RenderSettings&,
    const PreparedProjectionSource* source=nullptr);
// Rigid temporal correspondence using the same packed transforms as the GPU.
// Rejects changed topology/coordinates, native word wrapping, singular matrices,
// and mixed byte/word transforms that cannot share one camera-space mapping.
[[nodiscard]] std::optional<GpuProjection::MotionSurfaceSettings> pack_motion_surface(
    const PackedProjection& current,const PackedProjection& previous,
    std::uint32_t width,std::uint32_t height,std::uint32_t scale);
// Axis-collapse reduction groups in source order: maximum authored Z first,
// minimum authored Z second. GPU reduction must average transformed positions,
// not an object-space centroid (word wrapping can make those differ).
[[nodiscard]] std::array<std::vector<std::uint32_t>,2> pack_axis_groups(
    const assets::Shape&,std::uint32_t animation_frame);
}
