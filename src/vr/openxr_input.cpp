#include "starfox/vr/openxr_input.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <stdexcept>
#include <vector>

namespace starfox::vr {
namespace {
enum Action : std::size_t {
    steer = 0U, fire, bomb, boost, brake, menu, roll_left, roll_right, select,
    aim_left, aim_right, stick_left, stick_right,
    dpad_up, dpad_down, dpad_left, dpad_right, haptic,
};
constexpr const char* frame_interaction_extension =
    "XR_VALVE_frame_controller_interaction";

void check(XrResult result, const char* operation) {
    if (XR_FAILED(result))
        throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
}

bool active(const VrControls& controls, VrControlAction action) noexcept {
    return (controls.active_actions & vr_control_bit(action)) != 0U;
}
} // namespace

VrControls select_vr_control_sources(
    const VrControls& openxr, const VrControls& desktop) noexcept {
    VrControls selected;
    const auto use_openxr = [&](VrControlAction action) {
        return active(openxr, action);
    };
    const auto set_bool = [&](VrControlAction action, bool VrControls::*field) {
        if (use_openxr(action)) {
            selected.*field = openxr.*field;
            selected.active_actions |= vr_control_bit(action);
        } else if (active(desktop, action)) {
            selected.*field = desktop.*field;
            selected.active_actions |= vr_control_bit(action);
        }
    };

    if (use_openxr(VrControlAction::steer)) {
        selected.steer = openxr.steer;
        selected.active_actions |= vr_control_bit(VrControlAction::steer);
    } else if (active(desktop, VrControlAction::steer)) {
        selected.steer = desktop.steer;
        selected.active_actions |= vr_control_bit(VrControlAction::steer);
    }
    set_bool(VrControlAction::fire, &VrControls::fire);
    set_bool(VrControlAction::bomb, &VrControls::bomb);
    set_bool(VrControlAction::boost, &VrControls::boost);
    set_bool(VrControlAction::brake, &VrControls::brake);
    set_bool(VrControlAction::menu, &VrControls::menu);
    if (active(selected, VrControlAction::menu)) {
        selected.menu_pressed = use_openxr(VrControlAction::menu)
            ? openxr.menu_pressed : desktop.menu_pressed;
    }
    set_bool(VrControlAction::roll_left, &VrControls::roll_left);
    set_bool(VrControlAction::roll_right, &VrControls::roll_right);
    set_bool(VrControlAction::select, &VrControls::select);
    if (active(selected, VrControlAction::select)) {
        selected.select_pressed = use_openxr(VrControlAction::select)
            ? openxr.select_pressed : desktop.select_pressed;
    }
    set_bool(VrControlAction::stick_left, &VrControls::stick_left);
    set_bool(VrControlAction::stick_right, &VrControls::stick_right);

    // The original four-input reset chord. Only non-Frame targets produce it; with
    // the system layer the stick clicks and bumpers stay plain game actions.
    constexpr auto reset_chord = vr_control_bit(VrControlAction::roll_left)
        | vr_control_bit(VrControlAction::roll_right)
        | vr_control_bit(VrControlAction::stick_left)
        | vr_control_bit(VrControlAction::stick_right);
    if ((openxr.active_actions & reset_chord) == reset_chord)
        selected.reset_pressed = openxr.reset_pressed;
    else if ((desktop.active_actions & reset_chord) == reset_chord)
        selected.reset_pressed = desktop.reset_pressed;
    selected.view_down = openxr.view_down || desktop.view_down;
    selected.recentre_pressed = openxr.recentre_pressed || desktop.recentre_pressed;
    selected.recentre_height_pressed =
        openxr.recentre_height_pressed || desktop.recentre_height_pressed;
    selected.menu_chord_pressed = openxr.menu_chord_pressed || desktop.menu_chord_pressed;
    const auto& confirmation=openxr.menu_confirm_active?openxr:desktop;
    selected.menu_confirm=confirmation.menu_confirm;
    selected.menu_confirm_active=confirmation.menu_confirm_active;
    return selected;
}

OpenXrInput::~OpenXrInput() { close(); }

void OpenXrInput::stop_haptics() noexcept {
    sfvr_haptic_queue_clear(&haptic_queue_);
    rumble_queued_ = false;
    if (!session_ || !actions_[haptic]) return;
    bool failed = false;
    for (std::size_t index = 0; index < hands_.size(); ++index) {
        if (!haptic_started_hands_[index]) continue;
        XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
        info.action = actions_[haptic];
        info.subactionPath = hands_[index];
        const auto result = api_.stop_haptic(session_, &info);
        failed = failed || XR_FAILED(result);
        haptic_started_hands_[index] = false;
    }
    if (failed) status_ = "Stop OpenXR haptics failed";
}

bool OpenXrInput::haptics_available() const noexcept {
    return haptic_bound_hands_[0] || haptic_bound_hands_[1];
}

bool OpenXrInput::apply_haptics(
    const starfox::simulation::RumbleEffect& effect) noexcept {
    if (!session_ || !focused_ || !actions_[haptic] || !haptics_available()) return false;
    if (!effect.active()) {
        stop_haptics();
        return true;
    }
    sfvr_haptic_rumble(&haptic_queue_, SFVR_HAND_BOTH, effect.low_frequency,
        effect.high_frequency);
    rumble_queued_ = true;
    return true;
}

namespace {
struct HapticFlush {
    const std::array<bool, 2>& bound;
    std::array<bool, 2>& started;
    const std::array<XrPath, 2>& hands;
    XrSession session;
    XrAction action;
    const InputApi& api;
    bool rumble;
    bool failed{};
    XrResult failure{XR_SUCCESS};
};
void apply_flushed_pulse(int hand, float amplitude, float seconds, void* user) {
    auto& flush = *static_cast<HapticFlush*>(user);
    if (hand < 0 || hand > 1 || !flush.bound[static_cast<std::size_t>(hand)]) return;
    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = static_cast<XrDuration>(std::llround(
        static_cast<double>(seconds) * 1.0e6)) * 1000; // whole microseconds
    vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
    vibration.amplitude = amplitude;
    XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
    info.action = flush.action;
    info.subactionPath = flush.hands[static_cast<std::size_t>(hand)];
    const auto result = flush.api.apply_haptic(flush.session, &info,
        reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    if (XR_FAILED(result)) {
        flush.failed = true;
        flush.failure = result;
        return;
    }
    // Only rumble is "started": a lone system buzz is not cut short by the
    // gameplay stop calls the application makes while a menu is open.
    if (flush.rumble) flush.started[static_cast<std::size_t>(hand)] = true;
}
} // namespace

void OpenXrInput::flush_haptics() noexcept {
    const bool buzz = std::exchange(system_buzz_pending_, false);
    const bool rumble = std::exchange(rumble_queued_, false);
    if (!session_ || !focused_ || !actions_[haptic] || !haptics_available()) {
        sfvr_haptic_queue_clear(&haptic_queue_);
        return;
    }
    if (buzz) sfvr_haptic_event(&haptic_queue_, SFVR_HAND_BOTH, SFVR_HAPTIC_SYSTEM);
    HapticFlush flush{haptic_bound_hands_, haptic_started_hands_, hands_, session_,
        actions_[haptic], api_, rumble};
    sfvr_haptic_flush(&haptic_queue_, haptics_strength_, apply_flushed_pulse, &flush);
    if (flush.failed) {
        status_ = "Apply OpenXR haptics failed: " + std::to_string(flush.failure);
        stop_haptics();
    }
}

void OpenXrInput::close() noexcept {
    stop_haptics();
    for (auto& space : aim_spaces_) {
        if (space) api_.destroy_space(space);
        space = {};
    }
    if (set_) api_.destroy_set(set_); // Also destroys all child actions.
    session_ = {};
    set_ = {};
    actions_ = {};
    hands_ = {};
    haptic_profiles_ = {};
    frame_profile_ = {};
    haptic_bound_hands_ = {};
    haptic_started_hands_ = {};
    haptic_profile_count_ = 0U;
    sfvr_haptic_queue_clear(&haptic_queue_);
    system_buzz_pending_ = rumble_queued_ = false;
    controls_ = {};
    menu_armed_ = select_armed_ = reset_armed_ = false;
    system_.reset();
    focused_ = false;
}

bool OpenXrInput::initialize(
    XrInstance instance, XrSession session, bool frame_interaction_enabled) {
    close();
    if (!instance || !session) {
        status_ = "Missing input instance/session";
        return false;
    }
    try {
        session_ = session;
        const auto failed_initialization = [&](XrResult result,
                const char* operation) {
            status_ = std::string(operation) + ": " + std::to_string(result);
            close();
            return false;
        };
        XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strcpy(set.actionSetName, "starfox");
        std::strcpy(set.localizedActionSetName, "Star Fox");
        check(api_.create_set(instance, &set, &set_), "Create action set");

        const auto path = [&](const char* name) {
            XrPath value{};
            check(api_.path(instance, name, &value), "Resolve action path");
            return value;
        };
        hands_ = {path("/user/hand/left"), path("/user/hand/right")};
        const char* names[]{"steer", "fire", "bomb", "boost", "brake", "menu",
            "roll_left", "roll_right", "select", "aim_left", "aim_right",
            "stick_left", "stick_right", "dpad_up", "dpad_down",
            "dpad_left", "dpad_right", "rumble"};
        const char* labels[]{"Steer", "Fire", "Bomb", "Boost", "Brake", "Menu",
            "Roll Left", "Roll Right", "Select / Change View", "Left pointer",
            "Right pointer", "Left stick click", "Right stick click", "D-pad up",
            "D-pad down", "D-pad left", "D-pad right", "Rumble output"};
        // Only the Steam Frame player adds the D-pad and rumble actions. Everything
        // else keeps the original thirteen.
        const std::size_t action_count = frame_player_ ? actions_.size() : dpad_up;
        for (std::size_t i = 0; i < action_count; ++i) {
            XrActionCreateInfo action{XR_TYPE_ACTION_CREATE_INFO};
            std::strcpy(action.actionName, names[i]);
            std::strcpy(action.localizedActionName, labels[i]);
            action.actionType = i == steer ? XR_ACTION_TYPE_VECTOR2F_INPUT
                : i == aim_left || i == aim_right ? XR_ACTION_TYPE_POSE_INPUT
                : i == haptic ? XR_ACTION_TYPE_VIBRATION_OUTPUT
                : XR_ACTION_TYPE_BOOLEAN_INPUT;
            if (i == haptic) {
                action.countSubactionPaths = static_cast<std::uint32_t>(hands_.size());
                action.subactionPaths = hands_.data();
            }
            const auto result = api_.create_action(set_, &action, &actions_[i]);
            if (XR_FAILED(result)) return failed_initialization(result, "Create action");
        }

        const auto suggest = [&](const char* profile,
                std::initializer_list<std::pair<std::size_t, const char*>> bindings) {
            std::vector<XrActionSuggestedBinding> values;
            values.reserve(bindings.size());
            bool has_haptic = false;
            for (const auto& [action, name] : bindings) {
                if (actions_[action] == XR_NULL_HANDLE) continue; // Frame-player action
                if (action == haptic) has_haptic = true;
                values.push_back({actions_[action], path(name)});
            }
            XrInteractionProfileSuggestedBinding info{
                XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            info.interactionProfile = path(profile);
            const auto profile_path = info.interactionProfile;
            if(std::string_view(profile)=="/interaction_profiles/valve/frame_controller_valve")
                frame_profile_=profile_path;
            info.countSuggestedBindings = static_cast<std::uint32_t>(values.size());
            info.suggestedBindings = values.data();
            const auto result = api_.suggest(instance, &info);
            // Runtimes may omit profiles they do not implement.
            if (result == XR_ERROR_PATH_UNSUPPORTED) return false;
            check(result, "Suggest bindings");
            if (has_haptic && haptic_profile_count_ < haptic_profiles_.size())
                haptic_profiles_[haptic_profile_count_++] = profile_path;
            return true;
        };
        suggest("/interaction_profiles/khr/simple_controller", {
            {fire, "/user/hand/left/input/select/click"},
            {fire, "/user/hand/right/input/select/click"},
            {menu, "/user/hand/left/input/menu/click"},
            {menu, "/user/hand/right/input/menu/click"},
            {aim_left, "/user/hand/left/input/aim/pose"},
            {aim_right, "/user/hand/right/input/aim/pose"},
            {haptic, "/user/hand/left/output/haptic"},
            {haptic, "/user/hand/right/output/haptic"}});
        suggest("/interaction_profiles/oculus/touch_controller", {
            {steer, "/user/hand/left/input/thumbstick"},
            {fire, "/user/hand/right/input/a/click"},
            {bomb, "/user/hand/right/input/b/click"},
            {boost, "/user/hand/left/input/x/click"},
            {brake, "/user/hand/left/input/y/click"},
            {menu, "/user/hand/right/input/squeeze/value"},
            {roll_left, "/user/hand/left/input/trigger/value"},
            {roll_right, "/user/hand/right/input/trigger/value"},
            {select, "/user/hand/left/input/squeeze/value"},
            {stick_left, "/user/hand/left/input/thumbstick/click"},
            {stick_right, "/user/hand/right/input/thumbstick/click"},
            {aim_left, "/user/hand/left/input/aim/pose"},
            {aim_right, "/user/hand/right/input/aim/pose"},
            {haptic, "/user/hand/left/output/haptic"},
            {haptic, "/user/hand/right/output/haptic"}});
        // The Index profile is a Steam Frame addition; other targets never suggested it.
        if (frame_player_)
        suggest("/interaction_profiles/valve/index_controller", {
            {steer, "/user/hand/left/input/thumbstick"},
            {fire, "/user/hand/right/input/a/click"},
            {bomb, "/user/hand/right/input/b/click"},
            {boost, "/user/hand/left/input/a/click"},
            {brake, "/user/hand/left/input/b/click"},
            {menu, "/user/hand/right/input/squeeze/value"},
            {roll_left, "/user/hand/left/input/trigger/click"},
            {roll_right, "/user/hand/right/input/trigger/click"},
            {select, "/user/hand/left/input/squeeze/value"},
            {stick_left, "/user/hand/left/input/thumbstick/click"},
            {stick_right, "/user/hand/right/input/thumbstick/click"},
            {aim_left, "/user/hand/left/input/aim/pose"},
            {aim_right, "/user/hand/right/input/aim/pose"},
            {haptic, "/user/hand/left/output/haptic"},
            {haptic, "/user/hand/right/output/haptic"}});
        if (frame_interaction_enabled) {
            suggest("/interaction_profiles/valve/frame_controller_valve", {
                {steer, "/user/hand/left/input/thumbstick"},
                {fire, "/user/hand/right/input/x/click"},
                {bomb, "/user/hand/right/input/b/click"},
                {boost, "/user/hand/right/input/y/click"},
                {brake, "/user/hand/right/input/a/click"},
                {menu, "/user/hand/right/input/menu/click"},
                {roll_left, "/user/hand/left/input/bumper/click"},
                {roll_right, "/user/hand/right/input/bumper/click"},
                {select, "/user/hand/left/input/view/click"},
                {stick_left, "/user/hand/left/input/thumbstick/click"},
                {stick_right, "/user/hand/right/input/thumbstick/click"},
                {aim_left, "/user/hand/left/input/aim/pose"},
                {aim_right, "/user/hand/right/input/aim/pose"},
                {dpad_up, "/user/hand/left/input/dpad_up/click"},
                {dpad_down, "/user/hand/left/input/dpad_down/click"},
                {dpad_left, "/user/hand/left/input/dpad_left/click"},
                {dpad_right, "/user/hand/left/input/dpad_right/click"},
                {haptic, "/user/hand/left/output/haptic"},
                {haptic, "/user/hand/right/output/haptic"}});
        }

        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attach.countActionSets = 1;
        attach.actionSets = &set_;
        for (std::size_t hand = 0; hand < aim_spaces_.size(); ++hand) {
            XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            info.action = actions_[aim_left + hand];
            info.poseInActionSpace.orientation.w = 1;
            check(api_.create_space(session_, &info, &aim_spaces_[hand]),
                "Create pointer space");
        }
        const auto attach_result = api_.attach(session_, &attach);
        if (XR_FAILED(attach_result))
            return failed_initialization(attach_result, "Attach action set");
        status_ = frame_interaction_enabled
            ? "VR actions attached (Frame interaction enabled)"
            : "VR actions attached";
        return true;
    } catch (const std::exception& error) {
        status_ = error.what();
        close();
        return false;
    }
}

bool OpenXrInput::poll(bool focused, double now) {
    if (!set_ || !session_) {
        controls_ = {};
        focused_ = false;
        status_ = "VR input not initialized";
        return false;
    }
    if (!focused) {
        stop_haptics();
        controls_ = {};
        menu_armed_ = select_armed_ = reset_armed_ = false;system_.reset();
        focused_ = false;
        return true;
    }
    focused_ = false;
    try {
        const auto fail_poll = [&](XrResult result, const char* operation) {
            stop_haptics();
            controls_ = {};
            menu_armed_ = select_armed_ = reset_armed_ = false;system_.reset();
            focused_ = false;
            status_ = std::string(operation) + ": " + std::to_string(result);
            return false;
        };
        XrActiveActionSet active_set{set_, XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &active_set;
        const auto sync_result = api_.sync(session_, &sync);
        if (sync_result == XR_SESSION_NOT_FOCUSED) {
            stop_haptics();
            controls_ = {};
            menu_armed_ = select_armed_ = reset_armed_ = false;system_.reset();
            focused_ = false;
            return true;
        }
        if (XR_FAILED(sync_result)) return fail_poll(sync_result, "Sync actions");

        std::array<bool,2> haptic_bound{};
        bool frame_right=false;
        for (std::size_t hand = 0; hand < hands_.size(); ++hand) {
            XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
            if (XR_SUCCEEDED(api_.current_profile(session_, hands_[hand], &profile))) {
                if(hand==1) frame_right=frame_profile_!=XR_NULL_PATH
                    && profile.interactionProfile==frame_profile_;
                haptic_bound[hand] = std::find(haptic_profiles_.begin(),
                    haptic_profiles_.begin() + haptic_profile_count_,
                    profile.interactionProfile)
                    != haptic_profiles_.begin() + haptic_profile_count_;
            }
        }
        if (haptic_bound != haptic_bound_hands_) {
            stop_haptics();
            haptic_bound_hands_ = haptic_bound;
        }

        VrControls next;
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.action = actions_[steer];
        XrActionStateVector2f vector{XR_TYPE_ACTION_STATE_VECTOR2F};
        const auto steering_result = api_.vector(session_, &get, &vector);
        if (XR_FAILED(steering_result)) return fail_poll(steering_result, "Read steering");
        float x = 0.0F, y = 0.0F;
        const bool vector_active = vector.isActive != XR_FALSE;
        if (vector_active && std::isfinite(vector.currentState.x)
            && std::isfinite(vector.currentState.y)) {
            x = std::clamp(vector.currentState.x, -1.0F, 1.0F);
            y = std::clamp(vector.currentState.y, -1.0F, 1.0F);
        }
        const auto read_button = [&](std::size_t action, bool& pressed, bool& is_active) {
            get.action = actions_[action];
            XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
            const auto result = api_.boolean(session_, &get, &state);
            if (XR_FAILED(result)) return result;
            if (state.isActive) next.active_actions |= vr_control_bit(
                action == fire ? VrControlAction::fire
                : action == bomb ? VrControlAction::bomb
                : action == boost ? VrControlAction::boost
                : action == brake ? VrControlAction::brake
                : action == menu ? VrControlAction::menu
                : action == roll_left ? VrControlAction::roll_left
                : action == roll_right ? VrControlAction::roll_right
                : action == select ? VrControlAction::select
                : action == stick_left ? VrControlAction::stick_left
                : VrControlAction::stick_right);
            pressed = state.isActive && state.currentState;
            is_active = state.isActive != XR_FALSE;
            return XR_SUCCESS;
        };
        bool action_active{};
        auto result = read_button(fire, next.fire, action_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        result = read_button(bomb, next.bomb, action_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        result = read_button(boost, next.boost, action_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        result = read_button(brake, next.brake, action_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        next.menu_confirm=frame_right?next.brake:next.fire;
        next.menu_confirm_active=(next.active_actions & vr_control_bit(
            frame_right?VrControlAction::brake:VrControlAction::fire))!=0;
        bool menu_active{};
        result = read_button(menu, next.menu, menu_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        result = read_button(roll_left, next.roll_left, action_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        result = read_button(roll_right, next.roll_right, action_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        bool select_active{};
        result = read_button(select, next.select, select_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        next.menu_pressed = menu_active && menu_armed_ && next.menu && !controls_.menu;
        SystemLayerEvents system_events;
        if (system_layer_) {
            // Steam Frame. L View: tap / 1 s recentre / 3 s recentre+height, and
            // the Menu+View chord (standard section 1). The raw level is kept in
            // view_down; the game only sees the short press, as a one-poll tap
            // on release.
            const bool view_level = select_active && next.select;
            system_events = system_.update(view_level, menu_active && next.menu, now);
            next.view_down = view_level;
            next.select = next.select_pressed = system_events.view_tap;
            next.recentre_pressed = system_events.recentre || system_events.recentre_height;
            next.recentre_height_pressed = system_events.recentre_height;
            next.menu_chord_pressed = system_events.open_menu;
        } else {
            next.select_pressed = select_active && select_armed_ && next.select
                && !controls_.select;
        }

        bool stick_left_active{};
        result = read_button(stick_left, next.stick_left, stick_left_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        bool stick_right_active{};
        result = read_button(stick_right, next.stick_right, stick_right_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read button");
        bool dpad_up_pressed = false, dpad_down_pressed = false;
        bool dpad_left_pressed = false, dpad_right_pressed = false;
        const auto read_dpad = [&](std::size_t action, bool& down, bool& is_active) {
            if (actions_[action] == XR_NULL_HANDLE) {down = false;is_active = false;return XR_SUCCESS;}
            get.action = actions_[action];
            XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
            const auto result = api_.boolean(session_, &get, &state);
            if (XR_FAILED(result)) return result;
            down = state.isActive && state.currentState;
            is_active = state.isActive != XR_FALSE;
            return XR_SUCCESS;
        };
        bool up_active{}, down_active{}, left_active{}, right_active{};
        result = read_dpad(dpad_up, dpad_up_pressed, up_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read D-pad direction");
        result = read_dpad(dpad_down, dpad_down_pressed, down_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read D-pad direction");
        result = read_dpad(dpad_left, dpad_left_pressed, left_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read D-pad direction");
        result = read_dpad(dpad_right, dpad_right_pressed, right_active);
        if (XR_FAILED(result)) return fail_poll(result, "Read D-pad direction");
        if (dpad_up_pressed || dpad_down_pressed)
            y = dpad_up_pressed == dpad_down_pressed ? 0.0F : dpad_up_pressed ? 1.0F : -1.0F;
        if (dpad_left_pressed || dpad_right_pressed)
            x = dpad_left_pressed == dpad_right_pressed ? 0.0F : dpad_right_pressed ? 1.0F : -1.0F;
        const bool steer_active = vector_active || up_active || down_active
            || left_active || right_active;
        if (steer_active) next.active_actions |= vr_control_bit(VrControlAction::steer);
        const float radius = std::hypot(x, y);
        if (radius > 0.15F) {
            const float scale = (std::min(radius, 1.0F) - 0.15F) / (0.85F * radius);
            next.steer = {x * scale, y * scale};
        }

        if (!system_layer_) {
            const bool triggers = next.roll_left && next.roll_right;
            const bool sticks_active = stick_left_active && stick_right_active;
            next.reset_pressed = reset_armed_ && triggers && sticks_active
                && next.stick_left && next.stick_right;
            if (!triggers || !sticks_active || next.reset_pressed) reset_armed_ = false;
            else if (!next.stick_left && !next.stick_right) reset_armed_ = true;
            if (!select_active) select_armed_ = false;
            else if (!next.select) select_armed_ = true;
        }
        if (!menu_active) menu_armed_ = false;
        else if (!next.menu) menu_armed_ = true;
        controls_ = next;
        focused_ = true;
        status_ = "VR actions synchronized";
        if (system_events.recentre || system_events.recentre_height) system_buzz_pending_ = true;
        return true;
    } catch (const std::exception& error) {
        stop_haptics();
        controls_ = {};
        menu_armed_ = select_armed_ = reset_armed_ = false;system_.reset();
        focused_ = false;
        status_ = error.what();
        return false;
    }
}

std::array<std::optional<XrPosef>, 2> OpenXrInput::aim_poses(
    XrSpace base, XrTime time) const noexcept {
    std::array<std::optional<XrPosef>, 2> result;
    if (!base || time <= 0) return result;
    constexpr auto valid = XR_SPACE_LOCATION_POSITION_VALID_BIT
        | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
        | XR_SPACE_LOCATION_POSITION_TRACKED_BIT
        | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    for (unsigned hand = 0; hand < 2; ++hand) {
        if (!aim_spaces_[hand]) continue;
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(api_.locate(aim_spaces_[hand], base, time, &location))
            && (location.locationFlags & valid) == valid)
            result[hand] = location.pose;
    }
    return result;
}
} // namespace starfox::vr
