#pragma once
#include <openxr/openxr.h>
#include "starfox/simulation/rumble_sequencer.hpp"
#include "starfox/vr/system_layer.hpp"
#include "sfvr/sfvr_haptics.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <optional>
namespace starfox::vr {
struct InputApi {
    PFN_xrCreateActionSet create_set{xrCreateActionSet};
    PFN_xrDestroyActionSet destroy_set{xrDestroyActionSet};
    PFN_xrCreateAction create_action{xrCreateAction};
    PFN_xrStringToPath path{xrStringToPath};
    PFN_xrSuggestInteractionProfileBindings suggest{xrSuggestInteractionProfileBindings};
    PFN_xrAttachSessionActionSets attach{xrAttachSessionActionSets};
    PFN_xrSyncActions sync{xrSyncActions};
    PFN_xrGetActionStateBoolean boolean{xrGetActionStateBoolean};
    PFN_xrGetActionStateVector2f vector{xrGetActionStateVector2f};
    PFN_xrGetCurrentInteractionProfile current_profile{xrGetCurrentInteractionProfile};
    PFN_xrApplyHapticFeedback apply_haptic{xrApplyHapticFeedback};
    PFN_xrStopHapticFeedback stop_haptic{xrStopHapticFeedback};
    PFN_xrCreateActionSpace create_space{xrCreateActionSpace};
    PFN_xrDestroySpace destroy_space{xrDestroySpace};
    PFN_xrLocateSpace locate{xrLocateSpace};
};
struct VrControls {
    XrVector2f steer{};
    bool fire{},bomb{},boost{},brake{},menu{},menu_pressed{},roll_left{},roll_right{};
    // `select` / `select_pressed` are the L View *short press*: a one-poll tap
    // reported on release (see SystemLayer). `view_down` is the raw level.
    bool select{},select_pressed{},view_down{};
    bool stick_left{},stick_right{};
    // Non-Frame targets keep the original four-input reset chord (both
    // bumpers/triggers plus both stick clicks).
    bool reset_pressed{};
    // System layer one-shots (standard section 1), Steam Frame only. The recentre events are
    // applied by the application at the next stereo frame boundary;
    // recentre_height_pressed also recalibrates standing height.
    bool recentre_pressed{},recentre_height_pressed{},menu_chord_pressed{};
    std::uint32_t active_actions{};
    // Physical menu confirmation is independent of gameplay fire on Frame.
    bool menu_confirm{},menu_confirm_active{};
};

enum class VrControlAction : std::uint32_t {
    steer = 1U << 0U,
    fire = 1U << 1U,
    bomb = 1U << 2U,
    boost = 1U << 3U,
    brake = 1U << 4U,
    menu = 1U << 5U,
    roll_left = 1U << 6U,
    roll_right = 1U << 7U,
    select = 1U << 8U,
    stick_left = 1U << 9U,
    stick_right = 1U << 10U,
};
constexpr std::uint32_t vr_control_bit(VrControlAction action) noexcept {
    return static_cast<std::uint32_t>(action);
}
[[nodiscard]] VrControls select_vr_control_sources(
    const VrControls& openxr, const VrControls& desktop) noexcept;

inline void desktop_face_buttons(VrControls& controls,bool steam_frame,
    bool south,bool east,bool west,bool north) noexcept {
    controls.fire=steam_frame?west:south;controls.bomb=east;
    controls.boost=steam_frame?north:west;controls.brake=steam_frame?south:north;
    controls.menu_confirm=south;controls.menu_confirm_active=true;
}

// SDL's level-state sampler also runs while the XR session is unfocused; the
// caller discards those controls but retains these edge states for resume.
// With the system layer (Steam Frame only) the View short/hold and Menu+View
// chord timing matches the OpenXR path. Without it, every other target keeps its
// original behaviour: Select on the press edge and the four-input chord resets
// the game (the application still opens its menu from Menu+Select edges).
class DesktopControlEdges {
public:
    explicit DesktopControlEdges(bool system_layer=false) noexcept
        : system_layer_(system_layer) {}
    [[nodiscard]] VrControls sample(VrControls controls,
        double now=SystemLayer::steady_seconds()) noexcept {
        controls.menu_pressed=controls.menu&&!menu_;
        menu_=controls.menu;
        if(!system_layer_) {
            controls.select_pressed=controls.select&&!select_;
            const bool reset=controls.roll_left&&controls.roll_right
                &&controls.stick_left&&controls.stick_right;
            controls.reset_pressed=reset&&!reset_;
            select_=controls.select;reset_=reset;
            return controls;
        }
        const bool view=controls.select;
        const auto events=system_.update(view,controls.menu,now);
        controls.view_down=view;
        controls.select=controls.select_pressed=events.view_tap;
        controls.recentre_pressed=events.recentre||events.recentre_height;
        controls.recentre_height_pressed=events.recentre_height;
        controls.menu_chord_pressed=events.open_menu;
        return controls;
    }
    void reset() noexcept {menu_=select_=reset_=false;system_.reset();}
private:
    bool system_layer_{};
    bool menu_{},select_{},reset_{};
    SystemLayer system_;
};

class OpenXrInput {
public:
    explicit OpenXrInput(InputApi api={}):api_(api) {}
    ~OpenXrInput();
    OpenXrInput(const OpenXrInput&)=delete;
    OpenXrInput& operator=(const OpenXrInput&)=delete;
    // Attach before session begin. OpenXR permits attachment only once per
    // session; reinitialization requires a fresh caller-owned session.
    // The Steam Frame player's extras: the Index profile, the D-pad and rumble
    // actions and their bindings. Call before initialize(); off by default, which
    // keeps the original thirteen actions and the Simple and Touch profiles.
    void set_frame_player(bool enabled) noexcept {frame_player_=enabled;}
    [[nodiscard]] bool frame_player() const noexcept {return frame_player_;}
    bool initialize(XrInstance,XrSession,bool frame_interaction_enabled=false);
    // `now` is monotonic seconds for the View hold timing; tests inject it.
    bool poll(bool focused,double now=SystemLayer::steady_seconds());
    // Steam Frame system layer (View hold recentre, Menu+View chord, haptic
    // buzz). Off by default, which keeps the original grip/reset-chord input.
    void set_system_layer(bool enabled) noexcept {system_layer_=enabled;}
    [[nodiscard]] bool system_layer() const noexcept {return system_layer_;}
    [[nodiscard]] bool focused() const noexcept {return focused_;}
    // All OpenXR haptic output goes through one sfvr_haptic_queue. The authored
    // dual-band sample is queued with sfvr_haptic_rumble (max(low, high), the
    // native 40 ms pulse, both hands); the system buzz (SFVR_HAPTIC_SYSTEM) is
    // queued by poll() on a hold step. Nothing reaches xrApplyHapticFeedback
    // until flush_haptics(), which the caller runs once per frame: overlapping
    // pulses coalesce to the strongest, and the HAPTICS STRENGTH setting scales
    // the result.
    bool apply_haptics(const starfox::simulation::RumbleEffect&) noexcept;
    void flush_haptics() noexcept;
    // Stops started rumble and drops queued rumble. A pending system buzz stays.
    void stop_haptics() noexcept;
    [[nodiscard]] bool haptics_available() const noexcept;
    // User strength 0..1, applied to every OpenXR haptic output. 1 (the default)
    // leaves haptics as they were; the Steam Frame menu default is 0.6.
    void set_haptics_strength(float strength) noexcept {
        haptics_strength_=std::isfinite(strength)?std::clamp(strength,0.F,1.F):1.F;
    }
    [[nodiscard]] float haptics_strength() const noexcept {return haptics_strength_;}
    std::array<std::optional<XrPosef>,2> aim_poses(XrSpace base,XrTime time) const noexcept;
    void close() noexcept;
    const VrControls& controls() const noexcept {return controls_;}
    const std::string& status() const noexcept {return status_;}
private:
    InputApi api_;
    XrPath frame_profile_{};
    XrSession session_{};XrActionSet set_{};std::array<XrAction,18> actions_{};
    std::array<XrSpace,2> aim_spaces_{};
    std::array<XrPath,2> hands_{};
    std::array<XrPath,4> haptic_profiles_{};
    std::array<bool,2> haptic_bound_hands_{},haptic_started_hands_{};
    std::uint32_t haptic_profile_count_{};
    sfvr_haptic_queue haptic_queue_{};
    bool system_buzz_pending_{},rumble_queued_{};
    VrControls controls_{};bool menu_armed_{},select_armed_{},reset_armed_{};
    bool frame_player_{};
    bool system_layer_{};
    SystemLayer system_;
    float haptics_strength_{1.F};
    bool focused_{};
    std::string status_;
};
}
