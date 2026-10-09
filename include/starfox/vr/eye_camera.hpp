#pragma once
#include <openxr/openxr.h>
#include <array>
#include <optional>
namespace starfox::vr {
// Column-major matrices multiplying column vectors. World/view coordinates
// are right-handed (+Y up, forward -Z), as in OpenXR LOCAL space. Projection
// uses Vulkan's downward framebuffer Y and [0,1] depth, with positive viewport
// height. Do not apply a second Y inversion in the renderer.
using Matrix4 = std::array<float,16>;
struct EyeCamera {
    Matrix4 view;
    Matrix4 projection;
    std::array<unsigned,4> effects{};
};
// Affine view stores three rows; projection remains a full column-major matrix.
// Leave 16 bytes for per-draw effects within Vulkan's guaranteed 128 bytes.
struct SceneConstants {
    std::array<float,12> view_rows;
    Matrix4 projection;
    std::array<unsigned,4> effects;
};
static_assert(sizeof(SceneConstants)==128);
inline SceneConstants scene_constants(const EyeCamera& camera) noexcept {
    SceneConstants result{};
    for(unsigned row=0;row<3;++row) for(unsigned col=0;col<4;++col)
        result.view_rows[row*4+col]=camera.view[col*4+row];
    result.projection=camera.projection;result.effects=camera.effects;
    return result;
}
// Capture one stereo midpoint, then retain it: never cancel subsequent head
// movement or collapse the eye separation. Compositor poses stay unmodified.
class PositionAnchor {
public:
    bool apply(std::array<XrView,2>& views,float translation_scale=1.F) noexcept;
    // Re-capture the centre at the next apply(). keep_height retains the
    // previous vertical origin, so a recentre moves yaw and horizontal position
    // only; the first capture (no previous origin) always takes the live height.
    void reset(bool recenter_heading=false,bool keep_height=false) noexcept {
        if(origin_) height_=origin_->y;
        keep_height_=keep_height && (origin_.has_value() || keep_height_);
        origin_.reset();capture_heading_|=recenter_heading;
    }
    XrPosef anchored(XrPosef pose) const noexcept;
private:
    std::optional<XrVector3f> origin_;
    XrVector3f translation_offset_{};
    float yaw_{},height_{};
    bool capture_heading_{},keep_height_{};
};
// Pose translation is in metres; units_per_metre converts it to the scene's
// units. Near/far are already in scene units. nullopt far selects an infinite
// far plane. Invalid/non-finite tracking or projection inputs are rejected.
std::optional<EyeCamera> eye_camera(const XrView&,float units_per_metre,
    float near_plane,std::optional<float> far_plane=std::nullopt) noexcept;
// Compose only the small matrices on the CPU; all model vertex transforms
// remain in the GPU vertex shader. Model maps object-local to world space.
std::optional<EyeCamera> model_eye_camera(const EyeCamera&,const Matrix4& model) noexcept;
// Headset-free stereo cameras for SBS. All distances are scene units; aspect
// is the unsqueezed scene aspect, including when packing Half SBS.
std::optional<std::array<EyeCamera,2>> sbs_eye_cameras(float vertical_fov,
    float aspect,float separation,float convergence,float near_plane,
    std::optional<float> far_plane=std::nullopt) noexcept;
}
