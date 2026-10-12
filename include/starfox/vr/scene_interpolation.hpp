#pragma once
#include "starfox/vr/source_pose_interpolation.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/steering_matrix.hpp"
namespace starfox::vr {
simulation::MatrixQ15 landscape_scene_view(const GameSceneSnapshot&,const GameSceneSnapshot&,double alpha);
// Changes presentation space only. Scripted strategies retain source camera.
Matrix4 presentation_instrument_matrix(const GameSceneSnapshot&,const GameSceneSnapshot&,
    double alpha,const PresentationPreferences&);
Matrix4 presentation_scene_matrix(const GameSceneSnapshot&,const GameSceneSnapshot&,
    double alpha,const PresentationPreferences&);
// Display path for the cockpit: the pilot pose, camera and cabin follow a
// quadratic B-spline through the last three ticks (older, previous, current),
// so velocity no longer steps every 20 Hz tick; it trails the linear path by
// half a tick. Scene content stays linear; the scene matrix moves it onto the
// smoothed camera. Outside the pilot view these match the two-snapshot forms.
Matrix4 presentation_instrument_matrix(const GameSceneSnapshot& older,const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,double alpha,const PresentationPreferences&);
// With Follow ship rotation, `follow_attitude` (from CockpitFollowEase) replaces
// the ship attitude that turns the world; the seat stays on the source attitude.
Matrix4 presentation_scene_matrix(const GameSceneSnapshot& older,const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,double alpha,const PresentationPreferences&,
    const simulation::MatrixQ15* follow_attitude=nullptr);
// The smoothed source ship attitude for Follow ship rotation, and whether it
// continues from the previous tick. Empty outside a following pilot view.
struct CockpitAttitude {simulation::MatrixQ15 rotation{};bool continuous{};};
std::optional<CockpitAttitude> cockpit_follow_attitude(const GameSceneSnapshot& older,
    const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha,const PresentationPreferences&);
// Eases the attitude that Follow ship rotation turns the world by, once per
// display frame, so banks and rolls reach the view smoothly. It snaps on cuts
// and never trails the source by more than 90 degrees, so fast rolls keep their direction.
class CockpitFollowEase {
public:
    static constexpr double time_constant_seconds=.1;
    static constexpr double maximum_lag_degrees=90;
    void reset() noexcept {state_.reset();}
    [[nodiscard]] simulation::MatrixQ15 update(const simulation::MatrixQ15& target,bool continuous,double seconds) noexcept;
private:
    std::optional<std::array<double,4>> state_;
    double seconds_{};
};
// Evaluate once per source input consumption from the completed source state,
// never from XR time, interpolated display orientation, or physical head pose.
std::optional<SteeringMatrix> cockpit_steering_matrix(const GameSceneSnapshot&,const PresentationPreferences&);
Matrix4 landscape_camera_motion(const GameSceneSnapshot&,const GameSceneSnapshot&,double alpha);
}
