#include "starfox/vr/openxr_input.hpp"
#include "starfox/vr/startup_menu.hpp"
#include "starfox/vr/frame_menu.hpp"
#include "starfox/vr/game_input.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <source_location>
#include <vector>
using namespace starfox::vr;
namespace {
void require(bool value,const std::source_location where=std::source_location::current()) {if(!value) throw std::runtime_error("VR input assertion failed at line "+std::to_string(where.line()));}
template<class T>T handle(uintptr_t n) {return reinterpret_cast<T>(n);}
unsigned created{},destroyed{},suggestions{},syncs{},fail_create{};
bool fail_read{},active=true,held{},attach_failed{},unsupported{};
bool independent_buttons{};std::array<bool,18> button_states{};
unsigned haptic_applies{},haptic_stops{};bool fail_haptic{},fail_haptic_stop{};
float haptic_amplitude{};XrDuration haptic_duration{};
bool haptic_api_valid=true;
bool expect_original_bindings{};
XrResult sync_result=XR_SUCCESS;XrVector2f axis{};
std::array<XrPath,2> mock_hands{},mock_profiles{};
XrResult XRAPI_PTR create_set(XrInstance,const XrActionSetCreateInfo* info,XrActionSet* out) {
    require(std::strcmp(info->actionSetName,"starfox")==0);*out=handle<XrActionSet>(1);return XR_SUCCESS;
}
XrResult XRAPI_PTR destroy_set(XrActionSet) {++destroyed;return XR_SUCCESS;}
XrResult XRAPI_PTR create_action(XrActionSet,const XrActionCreateInfo* info,XrAction* out) {
    ++created;if(fail_create && created==fail_create) return XR_ERROR_RUNTIME_FAILURE;
    const unsigned index=(created-1)%18;
    const auto expected_type=(index==9 || index==10)?XR_ACTION_TYPE_POSE_INPUT
        :index==0?XR_ACTION_TYPE_VECTOR2F_INPUT
        :index==17?XR_ACTION_TYPE_VIBRATION_OUTPUT:XR_ACTION_TYPE_BOOLEAN_INPUT;
    require(info->actionType==expected_type);
    if(index==17) require(info->countSubactionPaths==2);
    *out=handle<XrAction>(index+1);return XR_SUCCESS;
}
std::vector<std::string> paths;
XrResult XRAPI_PTR path(XrInstance,const char* name,XrPath* out) {
    require(name[0]=='/');paths.emplace_back(name);*out=paths.size();
    if(std::strcmp(name,"/user/hand/left")==0) mock_hands[0]=*out;
    if(std::strcmp(name,"/user/hand/right")==0) mock_hands[1]=*out;
    return XR_SUCCESS;
}
XrPath path_value(const char* name) {
    for(std::size_t i=paths.size();i>0;--i)
        if(paths[i-1]==name) return static_cast<XrPath>(i);
    return XR_NULL_PATH;
}
XrResult XRAPI_PTR suggest(XrInstance,const XrInteractionProfileSuggestedBinding* info) {
    ++suggestions;
    if(expect_original_bindings) {
        // Simple and Touch only, with no haptic or Index entries.
        require(info->countSuggestedBindings==6 || info->countSuggestedBindings==13);
        const auto& name=paths.at(info->interactionProfile-1);
        require(name=="/interaction_profiles/khr/simple_controller"
            || name=="/interaction_profiles/oculus/touch_controller");
        for(unsigned i=0;i<info->countSuggestedBindings;++i)
            require(info->suggestedBindings[i].action!=handle<XrAction>(18));
        return XR_SUCCESS;
    }
    require(info->countSuggestedBindings==8
        || info->countSuggestedBindings==15 || info->countSuggestedBindings==19);
    if(info->countSuggestedBindings==19) {
        require(paths.at(info->interactionProfile-1)
            == "/interaction_profiles/valve/frame_controller_valve");
        const std::array<std::pair<unsigned,const char*>,19> expected{{
            {0,"/user/hand/left/input/thumbstick"},
            {1,"/user/hand/right/input/x/click"},
            {2,"/user/hand/right/input/b/click"},
            {3,"/user/hand/right/input/y/click"},
            {4,"/user/hand/right/input/a/click"},
            {5,"/user/hand/right/input/menu/click"},
            {6,"/user/hand/left/input/bumper/click"},
            {7,"/user/hand/right/input/bumper/click"},
            {8,"/user/hand/left/input/view/click"},
            {11,"/user/hand/left/input/thumbstick/click"},
            {12,"/user/hand/right/input/thumbstick/click"},
            {9,"/user/hand/left/input/aim/pose"},
            {10,"/user/hand/right/input/aim/pose"},
            {13,"/user/hand/left/input/dpad_up/click"},
            {14,"/user/hand/left/input/dpad_down/click"},
            {15,"/user/hand/left/input/dpad_left/click"},
            {16,"/user/hand/left/input/dpad_right/click"},
            {17,"/user/hand/left/output/haptic"},
            {17,"/user/hand/right/output/haptic"}}};
        for(unsigned i=0;i<info->countSuggestedBindings;++i) {
            const auto& binding=info->suggestedBindings[i];
            const auto action=static_cast<unsigned>(reinterpret_cast<uintptr_t>(binding.action)-1);
            const auto& name=paths.at(binding.binding-1);
            // The Frame profile has dedicated View/Menu actions. It must not
            // inherit the Touch left-grip Select / right-squeeze Menu aliases.
            require(name!="/user/hand/left/input/squeeze/value"
                && name!="/user/hand/right/input/squeeze/value");
            require(action==expected[i].first && name==expected[i].second);
        }
    } else if(info->countSuggestedBindings==15) {
        const auto& profile=paths.at(info->interactionProfile-1);
        const bool index=profile=="/interaction_profiles/valve/index_controller";
        require(index || profile=="/interaction_profiles/oculus/touch_controller");
        const std::array<const char*,15> expected{
            "/user/hand/left/input/thumbstick", "/user/hand/right/input/a/click",
            "/user/hand/right/input/b/click",
            index?"/user/hand/left/input/a/click":"/user/hand/left/input/x/click",
            index?"/user/hand/left/input/b/click":"/user/hand/left/input/y/click",
            "/user/hand/right/input/squeeze/value",
            index?"/user/hand/left/input/trigger/click":"/user/hand/left/input/trigger/value",
            index?"/user/hand/right/input/trigger/click":"/user/hand/right/input/trigger/value",
            "/user/hand/left/input/squeeze/value", "/user/hand/left/input/aim/pose",
            "/user/hand/right/input/aim/pose", "/user/hand/left/input/thumbstick/click",
            "/user/hand/right/input/thumbstick/click",
            "/user/hand/left/output/haptic",
            "/user/hand/right/output/haptic"};
        std::array<bool,13> seen{};
        for(unsigned i=0;i<info->countSuggestedBindings;++i) {
            const auto& binding=info->suggestedBindings[i];const auto& name=paths.at(binding.binding-1);
            const auto action=reinterpret_cast<uintptr_t>(binding.action)-1;
            if(i>=13U) require(action==17U && name==expected[i]);
            else {
                require(action<13U);require(!seen[action]);seen[action]=true;
                require(name==expected[action]);
            }
        }
    } else {
        require(paths.at(info->interactionProfile-1)
            =="/interaction_profiles/khr/simple_controller");
        require(info->suggestedBindings[6].action==handle<XrAction>(18)
            && paths.at(info->suggestedBindings[6].binding-1)=="/user/hand/left/output/haptic");
        require(info->suggestedBindings[7].action==handle<XrAction>(18)
            && paths.at(info->suggestedBindings[7].binding-1)=="/user/hand/right/output/haptic");
    }
    return unsupported?XR_ERROR_PATH_UNSUPPORTED:XR_SUCCESS;
}
XrResult XRAPI_PTR attach(XrSession,const XrSessionActionSetsAttachInfo* info) {require(info->countActionSets==1);return attach_failed?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;}
XrResult XRAPI_PTR sync(XrSession,const XrActionsSyncInfo* info) {++syncs;require(info->countActiveActionSets==1);return sync_result;}
XrResult XRAPI_PTR boolean(XrSession,const XrActionStateGetInfo* info,XrActionStateBoolean* out) {
    if(fail_read && info->action==handle<XrAction>(6)) return XR_ERROR_RUNTIME_FAILURE;
    const auto action=reinterpret_cast<uintptr_t>(info->action);
    out->isActive=active;
    out->currentState=action>=14 && action<=17
        ?independent_buttons && button_states.at(action)
        :independent_buttons?button_states.at(action):held;
    return XR_SUCCESS;
}
XrResult XRAPI_PTR vector(XrSession,const XrActionStateGetInfo* info,XrActionStateVector2f* out) {
    require(info->action==handle<XrAction>(1));out->isActive=active;out->currentState=axis;return XR_SUCCESS;
}
XrResult XRAPI_PTR current_profile(XrSession,XrPath hand,XrInteractionProfileState* out) {
    const auto index=hand==mock_hands[0]?0U:hand==mock_hands[1]?1U:2U;
    require(index<2U);out->interactionProfile=mock_profiles[index];return XR_SUCCESS;
}
XrResult XRAPI_PTR apply_haptic(XrSession,const XrHapticActionInfo* info,
    const XrHapticBaseHeader* header) {
    haptic_api_valid &= info->action==handle<XrAction>(18);
    const auto* vibration=reinterpret_cast<const XrHapticVibration*>(header);
    haptic_api_valid &= vibration->frequency==XR_FREQUENCY_UNSPECIFIED;
    haptic_amplitude=vibration->amplitude;haptic_duration=vibration->duration;
    ++haptic_applies;
    return fail_haptic?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
XrResult XRAPI_PTR stop_haptic(XrSession,const XrHapticActionInfo* info) {
    haptic_api_valid &= info->action==handle<XrAction>(18);++haptic_stops;
    return fail_haptic_stop?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
XrResult XRAPI_PTR create_space(XrSession,const XrActionSpaceCreateInfo* info,XrSpace* out) {
    require(info->poseInActionSpace.orientation.w==1);
    *out=handle<XrSpace>(1);return XR_SUCCESS;
}
XrResult XRAPI_PTR destroy_space(XrSpace) {return XR_SUCCESS;}
bool pointer_tracked=true;
XrResult XRAPI_PTR locate(XrSpace,XrSpace,XrTime,XrSpaceLocation* out) {
    out->locationFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if(pointer_tracked) out->locationFlags|=XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    out->pose.orientation.w=1;return XR_SUCCESS;
}
}

// The original runtime menu, exactly as the PCVR and Quest loop uses it.
void original_menu_tests() {
    {
        StartupMenu menu;using Page=StartupMenu::Page;
        VrControls press;press.fire=true;
        const auto click=[&] {menu.sample({},true);menu.sample(press,true);};
        require(menu.labels()[0].find("EXPERIENCE")==0 && menu.labels()[3]=="OPTIONS" && menu.labels()[4]=="START GAME");
        menu.selection=1;click();require(!menu.unlocked_pace);menu.sample(press,true);require(!menu.unlocked_pace);
        menu.selection=2;click();require(!menu.msu_music && menu.labels()[2]=="MSU-1 MUSIC: NOT FOUND");
        menu.msu_available=true;click();require(menu.msu_music);
        menu.sample(press,true);require(menu.msu_music);
        click();require(!menu.msu_music);
        menu.selection=3;click();require(menu.page==Page::options && menu.selection==0);
        menu.sample(press,true);require(menu.page==Page::options);
        click();require(menu.page==Page::cheats);
        require(menu.labels()[0].find("GOD MODE")==0 && menu.labels()[1].find("LEVEL SELECT")==0 && menu.labels()[6]=="BACK");
        click();require(menu.god_mode);menu.sample(press,true);require(menu.god_mode);
        menu.selection=2;
        for(unsigned expected:{1U,2U,0U}) {click();require(menu.default_laser==expected);menu.sample(press,true);require(menu.default_laser==expected);}
        menu.selection=1;menu.level_choices[0]={0,11,21};click();require(menu.selected_level==11);
        menu.selection=3;click();require(menu.infinite_bombs);
        menu.selection=4;click();require(menu.infinite_boost);
        menu.selection=5;click();require(menu.infinite_lives);
        menu.sample(press,true);require(menu.infinite_lives);
        menu.selection=6;click();require(menu.page==Page::options && menu.open);
        menu.selection=1;click();require(menu.crosshair_colour==1);
        menu.selection=2;click();require(menu.swap_face_buttons);
        menu.sample(press,true);require(menu.swap_face_buttons);
        VrControls original;original.fire=true;original.bomb=true;original.menu=true;
        original.steer={.25F,-.5F};
        auto swapped=menu.gameplay_controls(original);
        require(!swapped.fire && !swapped.bomb && swapped.boost && swapped.brake && swapped.menu);
        require(swapped.steer.x==original.steer.x && swapped.steer.y==original.steer.y);
        click();require(!menu.swap_face_buttons);
        require(menu.gameplay_controls(original).fire && menu.gameplay_controls(original).bomb);
        menu.selection=3;click();require(menu.music_volume==0);
        click();require(menu.music_volume==10);
        menu.selection=4;click();require(menu.sfx_volume==0);
        menu.selection=5;click();require(menu.language==1);
        require(menu.localized_labels()[5]==U"言語: 日本語");
        require(menu.first_visible_row()==0);
        menu.selection=6;require(menu.first_visible_row()==1);
        menu.selection=7;require(menu.first_visible_row()==2);
        menu.selection=8;require(menu.first_visible_row()==3);
        VrControls down;down.steer.y=-1;
        menu.sample({},true);menu.sample(down,true);require(menu.selection==9);
        menu.sample(down,true);require(menu.selection==9);
        VrControls up;up.steer.y=1;
        menu.sample({},true);menu.sample(up,true);require(menu.selection==8);
        for(unsigned language=0;language<6;++language) {
            menu.language=language;
            for(const auto page:{Page::main,Page::options,Page::cheats,Page::three_d,Page::two_d}) {
                menu.page=page;
                require(menu.localized_labels().size()==menu.row_count());
                for(const auto& label:menu.localized_labels()) require(!label.empty());
                if(menu.language>=1 && menu.language<=4 && (page==Page::two_d || page==Page::three_d))
                    require(!menu.localized_labels().front().starts_with(U"WORLD EFFECT")
                        && !menu.localized_labels().front().starts_with(U"MODEL EFFECT"));
            }
        }
        menu.page=Page::options;menu.selection=6;click();require(menu.page==Page::three_d && !menu.ray_tracing);
        require(menu.row_count()==4 && menu.labels().back()=="BACK" && !menu.preview);
        click();require(menu.model_effect==1 && !menu.ray_tracing);
        menu.selection=2;click();require(menu.preview);menu.sample(press,true);require(menu.preview);
        click();require(!menu.preview);
        menu.selection=3;click();require(!menu.ray_tracing && menu.page==Page::options);
        menu.ray_tracing_available=true;click();require(menu.page==Page::three_d);
        menu.selection=3;click();require(menu.ray_tracing);
        menu.sample(press,true);require(menu.ray_tracing);
        menu.selection=4;click();require(menu.page==Page::options && menu.selection==6);
        menu.selection=7;click();require(menu.page==Page::two_d);
        click();require(menu.world_effect==4);
        menu.selection=1;click();require(menu.world_intensity==0);
        menu.selection=2;click();require(menu.preview);
        menu.selection=3;click();require(menu.enhanced_sky && menu.page==Page::two_d && menu.row_count()==5);
        menu.sample(press,true);require(menu.enhanced_sky);
        menu.selection=4;click();require(menu.page==Page::options);
        menu.selection=8;click();require(menu.steer_sensitivity_index==1
            && menu.labels()[8]=="STICK SENSITIVITY: 40%");
        VrControls steer;steer.steer={1.F,-.5F};
        const auto softened=menu.gameplay_controls(steer);
        require(std::abs(softened.steer.x-.4F)<.001F
            && std::abs(softened.steer.y+.2F)<.001F);
        menu.selection=9;click();require(menu.page==Page::main && menu.selection==3);
        menu.alternate_available=true;menu.selection=0;click();require(menu.extended && menu.selected_level==0);
        menu.open_runtime();require(menu.selection==4 && menu.page==Page::main && menu.labels()[4]=="RESUME");
        menu.selection=0;click();require(menu.extended && menu.labels()[0].find("LOCKED")!=std::string::npos);
        menu.selection=4;click();require(!menu.open);
        menu.language=5;menu.default_laser=2;menu.crosshair_colour=7;
        for(unsigned style:{14U,15U,16U}) {
            menu.model_effect=style;menu.world_effect=style;
            StartupMenu effect_copy;
            require(effect_copy.restore_preferences(menu.preferences()));
            require(effect_copy.model_effect==style && effect_copy.world_effect==style);
            require(StartupMenu::style_name(style)!="OFF");
        }
        require(StartupMenu::next_style(13)==14 && StartupMenu::next_style(16)==0);
        require(StartupMenu::next_style(13,true)==14 && StartupMenu::next_style(16,true)==0);
        menu.music_volume=30;menu.sfx_volume=70;menu.msu_music=true;
        const auto preferences=menu.preferences();
        StartupMenu restored;
        require(restored.restore_preferences(preferences));
        require(restored.preferences()==preferences);
        require(restored.steer_sensitivity_index==1);
        require(restored.ray_tracing && !restored.ray_tracing_available);
        require(restored.enhanced_sky);
        require(!restored.ray_tracing_enabled());
        restored.page=Page::three_d;
        require(restored.row_count()==4 && restored.labels().back()=="BACK" && !restored.preview);
        restored.ray_tracing_available=true;
        require(restored.ray_tracing_enabled());
        restored.ray_tracing=false;
        require(!restored.ray_tracing_enabled());
        restored.ray_tracing=true;restored.ray_tracing_available=false;
        restored.page=Page::main;
        std::array<uint8_t,16> legacy{};std::copy(preferences.begin(),preferences.begin()+16,legacy.begin());
        legacy[4]=1;legacy[11]&=1;legacy[15]&=1;
        StartupMenu migrated;require(migrated.restore_preferences(legacy) && !migrated.ray_tracing && !migrated.infinite_lives);
        auto version4=preferences;version4[4]=4;version4[11]&=3;
        require(migrated.restore_preferences(version4) && !migrated.enhanced_sky
            && migrated.model_effect==menu.model_effect && migrated.world_effect==menu.world_effect);
        require(restored.open && !restored.runtime && !restored.extended
            && !restored.alternate_available && restored.page==Page::main
            && restored.selection==0 && restored.selected_level==0);
        // Invalid fields reject the entire record without partial mutation.
        for(size_t index=0;index<preferences.size();++index) {
            auto corrupt=preferences;corrupt[index]=255;
            const auto revision=restored.revision;
            require(!restored.restore_preferences(corrupt));
            require(restored.preferences()==preferences && restored.revision==revision);
        }
        require(!restored.restore_preferences(std::span(preferences).first(15)));
        require(restored.preferences()==preferences);
    }
}
int main() try {
    original_menu_tests();
    {
        FrameMenu menu;using Page=FrameMenu::Page;
        VrControls press;press.fire=true;
        const auto click=[&] {menu.sample({},true);menu.sample(press,true);};
        require(menu.labels()[0].find("EXPERIENCE")==0 && menu.labels()[3]=="OPTIONS" && menu.labels()[4]=="START GAME");
        menu.selection=1;click();require(!menu.unlocked_pace);menu.sample(press,true);require(!menu.unlocked_pace);
        menu.selection=2;click();require(!menu.msu_music && menu.labels()[2]=="MSU-1 MUSIC: NOT FOUND");
        menu.msu_available=true;click();require(menu.msu_music);
        menu.sample(press,true);require(menu.msu_music);
        click();require(!menu.msu_music);
        menu.selection=3;click();require(menu.page==Page::options && menu.selection==0);
        menu.sample(press,true);require(menu.page==Page::options);
        click();require(menu.page==Page::cheats);
        require(menu.labels()[0].find("GOD MODE")==0 && menu.labels()[1].find("LEVEL SELECT")==0 && menu.labels()[6]=="BACK");
        click();require(menu.god_mode);menu.sample(press,true);require(menu.god_mode);
        menu.selection=2;
        for(unsigned expected:{1U,2U,0U}) {click();require(menu.default_laser==expected);menu.sample(press,true);require(menu.default_laser==expected);}
        menu.selection=1;menu.level_choices[0]={0,11,21};click();require(menu.selected_level==11);
        menu.selection=3;click();require(menu.infinite_bombs);
        menu.selection=4;click();require(menu.infinite_boost);
        menu.selection=5;click();require(menu.infinite_lives);
        menu.sample(press,true);require(menu.infinite_lives);
        menu.selection=6;click();require(menu.page==Page::options && menu.open);
        menu.selection=1;click();require(menu.crosshair_colour==1);
        menu.selection=2;click();require(menu.swap_face_buttons);
        menu.sample(press,true);require(menu.swap_face_buttons);
        VrControls original;original.fire=true;original.bomb=true;original.menu=true;
        original.steer={.25F,-.5F};
        auto swapped=menu.gameplay_controls(original);
        require(!swapped.fire && !swapped.bomb && swapped.boost && swapped.brake && swapped.menu);
        require(swapped.steer.x==original.steer.x && swapped.steer.y==original.steer.y);
        click();require(!menu.swap_face_buttons);
        require(menu.gameplay_controls(original).fire && menu.gameplay_controls(original).bomb);
        menu.selection=3;click();require(menu.music_volume==0);
        click();require(menu.music_volume==10);
        menu.selection=4;click();require(menu.sfx_volume==0);
        menu.selection=5;click();require(menu.language==1);
        require(menu.localized_labels()[5]==U"言語: 日本語");
        require(menu.first_visible_row()==0);
        menu.selection=6;require(menu.first_visible_row()==1);
        menu.selection=7;require(menu.first_visible_row()==2);
        menu.selection=8;require(menu.first_visible_row()==3);
        VrControls down;down.steer.y=-1;
        menu.sample({},true);menu.sample(down,true);require(menu.selection==9);
        menu.sample(down,true);require(menu.selection==9);
        VrControls up;up.steer.y=1;
        menu.sample({},true);menu.sample(up,true);require(menu.selection==8);
        for(unsigned language=0;language<6;++language) {
            menu.language=language;
            for(const auto page:{Page::main,Page::options,Page::cheats,Page::three_d,Page::two_d,Page::presentation,Page::exit_confirmation,Page::reset_confirmation}) {
                menu.page=page;
                require(menu.localized_labels().size()==menu.row_count());
                for(const auto& label:menu.localized_labels()) require(!label.empty());
                if(menu.language>=1 && menu.language<=4 && (page==Page::two_d || page==Page::three_d))
                    require(!menu.localized_labels().front().starts_with(U"WORLD EFFECT")
                        && !menu.localized_labels().front().starts_with(U"MODEL EFFECT"));
            }
        }
        menu.page=Page::options;menu.selection=6;click();require(menu.page==Page::three_d && !menu.ray_tracing);
        require(menu.row_count()==4 && menu.labels().back()=="BACK" && !menu.preview);
        click();require(menu.model_effect==1 && !menu.ray_tracing);
        menu.selection=2;click();require(menu.preview);menu.sample(press,true);require(menu.preview);
        click();require(!menu.preview);
        menu.selection=3;click();require(!menu.ray_tracing && menu.page==Page::options);
        menu.ray_tracing_available=true;click();require(menu.page==Page::three_d);
        menu.selection=3;click();require(menu.ray_tracing);
        menu.sample(press,true);require(menu.ray_tracing);
        menu.selection=4;click();require(menu.page==Page::options && menu.selection==6);
        menu.selection=7;click();require(menu.page==Page::two_d);
        click();require(menu.world_effect==4);
        menu.selection=1;click();require(menu.world_intensity==0);
        menu.selection=2;click();require(menu.preview);
        menu.selection=3;click();require(menu.enhanced_sky && menu.page==Page::two_d && menu.row_count()==5);
        menu.sample(press,true);require(menu.enhanced_sky);
        menu.selection=4;click();require(menu.page==Page::options);
        menu.selection=8;click();require(menu.steer_sensitivity_index==1
            && menu.labels()[8]=="STICK SENSITIVITY: 40%");
        VrControls steer;steer.steer={1.F,-.5F};
        const auto softened=menu.gameplay_controls(steer);
        require(std::abs(softened.steer.x-.4F)<.001F
            && std::abs(softened.steer.y+.2F)<.001F);
        menu.selection=11;click();require(menu.page==Page::main && menu.selection==3);
        menu.alternate_available=true;menu.selection=0;click();require(menu.extended && menu.selected_level==0);
        menu.open_runtime();require(menu.selection==4 && menu.page==Page::main && menu.labels()[4]=="RESUME");
        menu.selection=0;click();require(menu.extended && menu.labels()[0].find("LOCKED")!=std::string::npos);
        menu.selection=4;click();require(!menu.open);
        menu.language=5;menu.default_laser=2;menu.crosshair_colour=7;
        for(unsigned style:{14U,15U,16U}) {
            menu.model_effect=style;menu.world_effect=style;
            FrameMenu effect_copy;
            require(effect_copy.restore_preferences(menu.preferences()));
            require(effect_copy.model_effect==style && effect_copy.world_effect==style);
            require(FrameMenu::style_name(style)!="OFF");
        }
        require(FrameMenu::next_style(13)==14 && FrameMenu::next_style(16)==0);
        require(FrameMenu::next_style(13,true)==14 && FrameMenu::next_style(16,true)==0);
        menu.music_volume=30;menu.sfx_volume=70;menu.msu_music=true;
        const auto preferences=menu.preferences();
        FrameMenu restored;
        require(restored.restore_preferences(preferences));
        require(restored.preferences()==preferences);
        require(restored.steer_sensitivity_index==1);
        require(restored.ray_tracing && !restored.ray_tracing_available);
        require(restored.enhanced_sky);
        require(!restored.ray_tracing_enabled());
        restored.page=Page::three_d;
        require(restored.row_count()==4 && restored.labels().back()=="BACK" && !restored.preview);
        restored.ray_tracing_available=true;
        require(restored.ray_tracing_enabled());
        restored.ray_tracing=false;
        require(!restored.ray_tracing_enabled());
        restored.ray_tracing=true;restored.ray_tracing_available=false;
        restored.page=Page::main;
        std::array<uint8_t,16> legacy{};std::copy(preferences.begin(),preferences.begin()+16,legacy.begin());
        legacy[4]=1;legacy[11]&=1;legacy[15]&=1;
        FrameMenu migrated;require(migrated.restore_preferences(legacy) && !migrated.ray_tracing && !migrated.infinite_lives);
        std::array<uint8_t,20> version4{};std::copy_n(preferences.begin(),20,version4.begin());version4[4]=4;version4[11]&=3;
        require(migrated.restore_preferences(version4) && !migrated.enhanced_sky
            && migrated.model_effect==menu.model_effect && migrated.world_effect==menu.world_effect);
        require(restored.open && !restored.runtime && !restored.extended
            && !restored.alternate_available && restored.page==Page::main
            && restored.selection==0 && restored.selected_level==0);
        require(!migrated.presentation.cockpit && migrated.presentation.translation_scale()==1.F && migrated.presentation.scale()==1.F);
        for(unsigned version=1;version<=5;++version) {
            auto old=preferences;old[4]=uint8_t(version);old[11]&=version==1?1:version<5?3:255;old[15]&=version<3?1:3;
            migrated.presentation={true,false,5,50,-50,100,true};
            require(migrated.restore_preferences(std::span(old).first(version<4?16:20)));
            require(!migrated.presentation.cockpit && migrated.presentation.translation_scale()==1.F && migrated.presentation.scale()==1.F
                && migrated.presentation.origin_x==0 && migrated.presentation.origin_y==0 && migrated.presentation.origin_z==0
                && !migrated.presentation.follow_ship_rotation);
        }
        FrameMenu presentation_menu;presentation_menu.page=Page::presentation;
        presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.presentation.cockpit && !presentation_menu.presentation.follow_ship_rotation);
        require(presentation_menu.row_count()==10 && presentation_menu.labels()[1]=="FOLLOW SHIP ROTATION: OFF");
        presentation_menu.selection=1;presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.presentation.follow_ship_rotation && presentation_menu.labels()[1]=="FOLLOW SHIP ROTATION: ON");
        require(migrated.restore_preferences(presentation_menu.preferences()) && migrated.presentation.follow_ship_rotation);
        auto version6=presentation_menu.preferences();version6[4]=6;
        require(migrated.restore_preferences(std::span(version6).first(26)) && migrated.presentation.cockpit
            && !migrated.presentation.follow_ship_rotation);
        require(!migrated.restore_preferences(version6)); // Version and size must agree.
        require(presentation_menu.preferences()[4]==9 && presentation_menu.preferences().size()==29);
        // REFRESH RATE: 90 Hz default, 120 Hz, then the system setting; saved in v9.
        require(presentation_menu.labels()[8]=="REFRESH RATE: 90 HZ" && presentation_menu.refresh_target()==90.F);
        presentation_menu.selection=8;presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.labels()[8]=="REFRESH RATE: 120 HZ" && presentation_menu.refresh_target()==120.F);
        {FrameMenu saved;require(saved.restore_preferences(presentation_menu.preferences()) && saved.refresh_target()==120.F);}
        presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.labels()[8]=="REFRESH RATE: SYSTEM" && !presentation_menu.refresh_target());
        {FrameMenu saved;require(saved.restore_preferences(presentation_menu.preferences()) && !saved.refresh_target());}
        {auto version8=presentation_menu.preferences();version8[4]=8;FrameMenu older;older.refresh_choice=1;
            require(older.restore_preferences(std::span(version8).first(28)) && older.refresh_target()==90.F);
            auto invalid=presentation_menu.preferences();invalid[28]=3;require(!older.restore_preferences(invalid));}
        presentation_menu.refresh_override=144.F;
        require(presentation_menu.labels()[8]=="REFRESH RATE: 144 HZ ENV" && presentation_menu.refresh_target()==144.F);
        presentation_menu.refresh_override.reset();
        presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.refresh_target()==90.F);
        presentation_menu.selection=7;presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.recenter_revision==1);
        presentation_menu.presentation={true,false,5,-100,100,35};
        auto calibrated_v6=presentation_menu.preferences();calibrated_v6[4]=6;
        migrated.presentation.follow_ship_rotation=true;
        require(migrated.restore_preferences(std::span(calibrated_v6).first(26))
            && migrated.presentation.origin_x==-100 && migrated.presentation.origin_y==100
            && migrated.presentation.origin_z==35 && migrated.presentation.scale()==2.F
            && migrated.presentation.translation_scale()==0.F && !migrated.presentation.follow_ship_rotation);
        require(migrated.restore_preferences(presentation_menu.preferences()) && migrated.presentation.origin_z==35
            && migrated.presentation.origin_x==-100 && migrated.presentation.translation_scale()==0.F && migrated.presentation.scale()==2.F);
        presentation_menu.presentation.head_translation=3;
        require(migrated.restore_preferences(presentation_menu.preferences())
            && migrated.presentation.translation_scale()==1.5F);
        presentation_menu.page=Page::main;presentation_menu.selection=5;
        presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.page==Page::exit_confirmation && presentation_menu.selection==0 && !presentation_menu.exit_requested);
        presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.page==Page::main && !presentation_menu.exit_requested);
        presentation_menu.sample({},true);presentation_menu.sample(press,true);
        presentation_menu.selection=1;presentation_menu.sample({},true);presentation_menu.sample(press,true);
        require(presentation_menu.exit_requested);
        // Version 8 adds the haptic strength (percent, default 60).
        {
            FrameMenu haptic;
            require(haptic.haptics_percent==60 && haptic.haptics_strength()==.6F);
            haptic.page=Page::options;haptic.selection=9;
            require(haptic.labels()[9]=="HAPTICS STRENGTH: 60%" && haptic.row_count()==12
                && haptic.labels()[10]=="VR PRESENTATION" && haptic.labels()[11]=="BACK");
            for(const unsigned expected:{70U,80U,90U,100U,0U,10U}) {
                haptic.sample({},true);haptic.sample(press,true);require(haptic.haptics_percent==expected);
            }
            haptic.haptics_percent=35;
            FrameMenu copy;require(copy.restore_preferences(haptic.preferences()) && copy.haptics_percent==35);
            require(copy.preferences()==haptic.preferences());
            // v7 -> v8 keeps every existing value and gets the 0.6 default.
            FrameMenu old;old.language=3;old.god_mode=true;old.default_laser=2;old.msu_music=true;
            old.music_volume=30;old.sfx_volume=70;old.crosshair_colour=5;old.swap_face_buttons=true;
            old.infinite_bombs=true;old.infinite_boost=true;old.infinite_lives=true;old.model_effect=14;
            old.world_effect=16;old.model_intensity=50;old.world_intensity=75;old.steer_sensitivity_index=2;
            old.ray_tracing=true;old.enhanced_sky=true;old.unlocked_pace=false;
            old.presentation={true,3,2,10,-20,30,true};
            auto v7=old.preferences();v7[4]=7;
            FrameMenu migrated7;migrated7.haptics_percent=15;
            require(migrated7.restore_preferences(std::span(v7).first(27)) && migrated7.haptics_percent==60);
            require(migrated7.language==3 && migrated7.god_mode && migrated7.default_laser==2 && migrated7.msu_music
                && migrated7.music_volume==30 && migrated7.sfx_volume==70 && migrated7.crosshair_colour==5
                && migrated7.swap_face_buttons && migrated7.infinite_bombs && migrated7.infinite_boost
                && migrated7.infinite_lives && migrated7.model_effect==14 && migrated7.world_effect==16
                && migrated7.model_intensity==50 && migrated7.world_intensity==75
                && migrated7.steer_sensitivity_index==2 && migrated7.ray_tracing && migrated7.enhanced_sky
                && !migrated7.unlocked_pace && migrated7.presentation.cockpit
                && migrated7.presentation.follow_ship_rotation && migrated7.presentation.origin_x==10
                && migrated7.presentation.origin_y==-20 && migrated7.presentation.origin_z==30);
            require(migrated7.preferences()[4]==9 && migrated7.preferences()[27]==60 && migrated7.preferences()[28]==0);
            require(!migrated7.restore_preferences(v7)); // v7 header with a v9-sized record.
            auto bad=old.preferences();bad[27]=101;require(!migrated7.restore_preferences(bad));
        }
        // Physical B (bomb) is back on every page, in addition to the BACK rows.
        {
            VrControls b;b.bomb=true;
            FrameMenu back_menu;
            const auto press_b=[&] {back_menu.sample({},true);back_menu.sample(b,true);};
            require(back_menu.page==Page::main && !back_menu.runtime);
            press_b();require(back_menu.page==Page::main && back_menu.open); // Nothing before the game.
            struct Route {Page page;Page parent;unsigned parent_row;};
            for(const auto route:{Route{Page::options,Page::main,3},Route{Page::cheats,Page::options,0},
                Route{Page::three_d,Page::options,6},Route{Page::two_d,Page::options,7},
                Route{Page::presentation,Page::options,10},Route{Page::exit_confirmation,Page::main,5},
                Route{Page::reset_confirmation,Page::main,5}}) {
                back_menu.page=route.page;back_menu.selection=0;
                const auto revision=back_menu.revision;
                press_b();
                require(back_menu.page==route.parent && back_menu.selection==route.parent_row
                    && back_menu.revision!=revision && !back_menu.exit_requested && !back_menu.reset_requested);
                // B is not a confirm and is one-shot: held B does nothing more.
                back_menu.page=route.page;back_menu.sample(b,true);require(back_menu.page==route.page);
            }
            // A View short press (select_pressed) goes back too.
            VrControls view_tap;view_tap.select=view_tap.select_pressed=true;
            back_menu.page=Page::options;back_menu.sample(view_tap,true);require(back_menu.page==Page::main);
            // B held while the runtime menu opens must be released first.
            back_menu.open_runtime();back_menu.sample(b,true);require(back_menu.open);
            press_b();require(!back_menu.open); // Runtime main page: B resumes.
            // B only goes back; confirmation is unaffected by gameplay B/bomb.
            back_menu.open_runtime();back_menu.page=Page::exit_confirmation;back_menu.selection=1;
            back_menu.sample({},true);back_menu.sample(b,true);
            require(!back_menu.exit_requested && back_menu.page==Page::main);
            // Focus loss disarms B.
            back_menu.page=Page::options;back_menu.sample({},true);back_menu.sample({},false);
            back_menu.sample(b,true);require(back_menu.page==Page::options);
        }
        // Reset game: runtime-only main row with a confirm; no input chord.
        {
            FrameMenu reset_menu;
            require(reset_menu.row_count()==6 && reset_menu.labels().back()=="QUIT TO STEAM"
                && reset_menu.labels()[5]!="RESET GAME");
            reset_menu.open_runtime();
            require(reset_menu.row_count()==7 && reset_menu.labels()[4]=="RESUME"
                && reset_menu.labels()[5]=="RESET GAME" && reset_menu.labels()[6]=="QUIT TO STEAM");
            reset_menu.selection=5;reset_menu.sample({},true);reset_menu.sample(press,true);
            require(reset_menu.page==Page::reset_confirmation && reset_menu.selection==0
                && !reset_menu.reset_requested && reset_menu.title()=="RESET GAME?"
                && reset_menu.labels()[1]=="YES / RESET GAME");
            reset_menu.sample({},true);reset_menu.sample(press,true); // NO / BACK
            require(reset_menu.page==Page::main && reset_menu.selection==5 && !reset_menu.reset_requested);
            reset_menu.sample({},true);reset_menu.sample(press,true);
            reset_menu.selection=1;reset_menu.sample({},true);reset_menu.sample(press,true);
            require(reset_menu.reset_requested && !reset_menu.exit_requested);
            reset_menu.reset_requested=false;
            reset_menu.page=Page::main;reset_menu.selection=6;reset_menu.sample({},true);reset_menu.sample(press,true);
            require(reset_menu.page==Page::exit_confirmation && reset_menu.title()=="QUIT TO STEAM?"
                && reset_menu.labels()[1]=="YES / QUIT TO STEAM");
            reset_menu.sample({},true);reset_menu.sample(press,true); // NO / BACK returns to the quit row.
            require(reset_menu.page==Page::main && reset_menu.selection==6);
        }
        // Invalid fields reject the entire record without partial mutation.
        for(size_t index=0;index<preferences.size();++index) {
            auto corrupt=preferences;corrupt[index]=255;
            const auto revision=restored.revision;
            require(!restored.restore_preferences(corrupt));
            require(restored.preferences()==preferences && restored.revision==revision);
        }
        require(!restored.restore_preferences(std::span(preferences).first(15)));
        require(restored.preferences()==preferences);
    }
    InputApi api{create_set,destroy_set,create_action,path,suggest,attach,sync,boolean,
        vector,current_profile,apply_haptic,stop_haptic,create_space,destroy_space,locate};
    OpenXrInput input(api);
    input.set_frame_player(true); // The Frame player's actions and profiles; the original set is tested below.
    input.set_system_layer(true); // The Steam Frame input; the original input is tested below.
    require(!input.poll(true));require(input.initialize(handle<XrInstance>(1),handle<XrSession>(2)));
    require(created==18 && suggestions==3);
    independent_buttons=true;
    // The old four-input reset chord (both bumpers + both stick clicks) is
    // gone: those inputs stay plain game actions and trigger no system work.
    button_states[12]=button_states[13]=true;
    require(input.poll(true));
    button_states[7]=button_states[8]=true;
    for(const double at:{0.0,.2,.7,1.5,4.0}) {
        require(input.poll(true,200.0+at));
        const auto& chord=input.controls();
        require(chord.roll_left && chord.roll_right && chord.stick_left && chord.stick_right);
        require(!chord.menu_pressed && !chord.select && !chord.select_pressed && !chord.view_down
            && !chord.recentre_pressed && !chord.recentre_height_pressed && !chord.menu_chord_pressed);
    }
    button_states[12]=button_states[13]=button_states[7]=button_states[8]=false;require(input.poll(true));
    button_states[14]=true;
    require(input.poll(true) && input.controls().steer.y>0.99F);
    button_states[14]=false;button_states[15]=true;
    require(input.poll(true) && input.controls().steer.y<-.99F);
    button_states[15]=false;button_states[16]=button_states[17]=true;
    require(input.poll(true) && input.controls().steer.x==0.0F);
    button_states[16]=button_states[17]=false;
    require(input.poll(false));require(input.poll(true));
    independent_buttons=false;
    require(input.poll(false)); // Start the existing focus tests with unarmed buttons.
    require(input.aim_poses(handle<XrSpace>(2),1)[0].has_value());
    require(!input.aim_poses(XR_NULL_HANDLE,1)[0].has_value());
    pointer_tracked=false;require(!input.aim_poses(handle<XrSpace>(2),1)[0].has_value());pointer_tracked=true;
    held=true;axis={1,1};require(input.poll(true));
    require(input.controls().fire && !input.controls().menu_pressed);
    require(!input.controls().select && !input.controls().select_pressed && input.controls().view_down);
    require(std::abs(std::hypot(input.controls().steer.x,input.controls().steer.y)-1)<1e-6);
    held=false;axis={.1F,0};require(input.poll(true));require(input.controls().steer.x==0);
    held=true;require(input.poll(true));require(input.controls().menu_pressed);
    require(!input.controls().select_pressed); // Menu + View is the chord, not a Select tap.
    require(input.poll(true));require(!input.controls().menu_pressed);
    require(!input.controls().select_pressed);
    const auto old_syncs=syncs;require(input.poll(false));require(syncs==old_syncs && !input.controls().fire);
    require(input.poll(true));require(!input.controls().menu_pressed);
    active=false;require(input.poll(true));require(!input.controls().fire && input.controls().steer.x==0);
    active=true;axis={std::numeric_limits<float>::quiet_NaN(),0};require(input.poll(true));require(input.controls().steer.x==0);
    require(!input.controls().menu_pressed); // Reconnected held button is not a fresh press.
    require(!input.controls().select_pressed);
    fail_read=true;require(!input.poll(true));require(!input.controls().fire && !input.controls().menu);
    require(input.status().find("Read button")!=std::string::npos);
    fail_read=false;sync_result=XR_SESSION_NOT_FOCUSED;
    require(input.poll(true) && !input.focused() && !input.controls().fire);
    sync_result=XR_ERROR_RUNTIME_FAILURE;require(!input.poll(true));
    require(!input.focused() && !input.controls().fire);
    sync_result=XR_SUCCESS;require(input.poll(true) && input.focused());
    input.close();require(destroyed==1);
    created=0;fail_create=3;require(!input.initialize(handle<XrInstance>(1),handle<XrSession>(3)));require(destroyed==2);
    fail_create=0;created=0;attach_failed=true;require(!input.initialize(handle<XrInstance>(1),handle<XrSession>(4)));require(destroyed==3);
    created=0;attach_failed=false;unsupported=true;require(input.initialize(handle<XrInstance>(1),handle<XrSession>(5)));
    input.close();require(destroyed==4);
    unsupported=false;created=0;const auto frame_suggestions=suggestions;
    sync_result=XR_SUCCESS;
    require(input.initialize(handle<XrInstance>(1),handle<XrSession>(6),true));
    require(created==18 && suggestions==frame_suggestions+4);
    mock_profiles={path_value("/interaction_profiles/oculus/touch_controller"),
        path_value("/interaction_profiles/oculus/touch_controller")};
    require(input.poll(true) && input.haptics_available());
    const starfox::simulation::RumbleEffect authored{0x1111U,0x8888U,40U};
    require(input.haptics_strength()==1.F); // Unscaled unless the menu says otherwise.
    input.set_haptics_strength(1.F);
    require(input.apply_haptics(authored) && haptic_applies==0); // Queued, not yet sent.
    input.flush_haptics();require(haptic_applies==2);
    require(std::abs(haptic_amplitude-float(0x8888U)/65535.F)<.00001F
        && haptic_duration==40'000'000 && haptic_api_valid);
    const auto stops_before_focus=haptic_stops;
    require(input.poll(false) && !input.focused()
        && haptic_stops==stops_before_focus+2);
    const auto applies_before_unfocused=haptic_applies;
    require(!input.apply_haptics(authored) && haptic_applies==applies_before_unfocused);
    require(input.poll(true) && input.focused());
    require(input.apply_haptics(authored));input.flush_haptics();
    fail_haptic=true;
    require(input.apply_haptics(authored));input.flush_haptics(); // The failure surfaces at the flush.
    require(haptic_stops==stops_before_focus+4
        && input.status().find("Apply OpenXR haptics failed")!=std::string::npos);
    fail_haptic=false;
    require(input.apply_haptics(authored));input.flush_haptics();
    fail_haptic_stop=true;input.stop_haptics();
    require(haptic_stops==stops_before_focus+6
        && input.status().find("Stop OpenXR haptics failed")!=std::string::npos);
    fail_haptic_stop=false;
    mock_profiles={path_value("/interaction_profiles/valve/frame_controller_valve"),
        path_value("/interaction_profiles/valve/frame_controller_valve")};
    require(input.poll(true) && input.haptics_available());
    independent_buttons=true;button_states={};button_states[5]=true; // Frame A -> brake
    require(input.poll(true) && input.controls().brake && !input.controls().fire
        && input.controls().menu_confirm && input.controls().menu_confirm_active);
    button_states[5]=false;button_states[2]=true; // Frame X -> fire
    require(input.poll(true) && input.controls().fire && !input.controls().menu_confirm);
    independent_buttons=false;button_states={};
    require(input.apply_haptics(authored)); // The advertised Frame binding is usable too.
    {
        // --- Haptic strength: every OpenXR output is scaled, 0 silences it.
        input.set_haptics_strength(.5F);
        auto applies=haptic_applies;
        require(input.apply_haptics(authored) && haptic_applies==applies); // Nothing before the flush.
        input.flush_haptics();
        require(haptic_applies==applies+2
            && std::abs(haptic_amplitude-float(0x8888U)/65535.F*.5F)<.00001F && haptic_duration==40'000'000);
        input.flush_haptics();require(haptic_applies==applies+2); // An empty queue sends nothing.
        input.set_haptics_strength(.25F);
        require(input.apply_haptics({0xFFFFU,0x0000U,40U}));input.flush_haptics();
        require(std::abs(haptic_amplitude-.25F)<.00001F);
        // Overlapping pulses in one frame coalesce into the strongest, once per hand.
        input.set_haptics_strength(1.F);applies=haptic_applies;
        require(input.apply_haptics({0x2000U,0x1000U,40U}) && input.apply_haptics({0x8000U,0x4000U,40U}));
        input.flush_haptics();
        require(haptic_applies==applies+2 && std::abs(haptic_amplitude-float(0x8000U)/65535.F)<.00001F);
        input.set_haptics_strength(0.F);
        applies=haptic_applies;
        require(input.apply_haptics(authored));input.flush_haptics();require(haptic_applies==applies);
        input.set_haptics_strength(7.F);require(input.haptics_strength()==1.F);
        input.set_haptics_strength(-1.F);require(input.haptics_strength()==0.F);
        input.set_haptics_strength(std::numeric_limits<float>::quiet_NaN());require(input.haptics_strength()==1.F);

        // --- System layer through the fake runtime. Frame action handles:
        // menu 6, View 9. Time is injected, so the holds are exact.
        independent_buttons=true;button_states={};
        const auto view=[&](bool down) {button_states[9]=down;};
        const auto menu_button=[&](bool down) {button_states[6]=down;};
        const auto poll_at=[&](double at) {require(input.poll(true,at));input.flush_haptics();return input.controls();};
        require(input.poll(false,0.0));
        poll_at(10.0); // Released: armed.
        input.set_haptics_strength(1.F);

        // Short press: nothing while down, a one-poll Select tap on release.
        view(true);
        auto c=poll_at(10.0);
        require(!c.select && !c.select_pressed && c.view_down && !c.recentre_pressed);
        view(false);c=poll_at(10.4);
        require(c.select && c.select_pressed && !c.view_down && !c.recentre_pressed);
        c=poll_at(10.42);require(!c.select && !c.select_pressed);
        applies=haptic_applies;

        // 1 s hold: recentre (height kept) and a 0.6 / 80 ms buzz on both hands.
        view(true);c=poll_at(20.0);
        c=poll_at(20.99);require(!c.recentre_pressed && haptic_applies==applies);
        c=poll_at(21.0);
        require(c.recentre_pressed && !c.recentre_height_pressed && !c.select && haptic_applies==applies+2
            && std::abs(haptic_amplitude-.6F)<.00001F && haptic_duration==80'000'000 && haptic_api_valid);
        c=poll_at(22.9);require(!c.recentre_pressed && haptic_applies==applies+2);
        // 3 s hold: recentre + recalibrate height, buzz again.
        c=poll_at(23.0);
        require(c.recentre_pressed && c.recentre_height_pressed && haptic_applies==applies+4
            && std::abs(haptic_amplitude-.6F)<.00001F && haptic_duration==80'000'000);
        c=poll_at(30.0);require(!c.recentre_pressed && !c.recentre_height_pressed && haptic_applies==applies+4);
        // Releasing a hold is never also a Select tap.
        view(false);c=poll_at(30.1);require(!c.select && !c.select_pressed && !c.recentre_pressed);

        // The buzz is scaled by the strength setting; 0 is silent but still recentres.
        input.set_haptics_strength(.5F);applies=haptic_applies;
        view(true);poll_at(40.0);c=poll_at(41.0);
        require(c.recentre_pressed && haptic_applies==applies+2 && std::abs(haptic_amplitude-.3F)<.00001F);
        input.set_haptics_strength(0.F);applies=haptic_applies;
        view(false);poll_at(41.1);view(true);poll_at(42.0);c=poll_at(43.0);
        require(c.recentre_pressed && haptic_applies==applies);
        input.set_haptics_strength(1.F);
        view(false);poll_at(43.1);

        // A gameplay stop (menu open, rumble end) does not cut the system buzz short.
        applies=haptic_applies;auto stopped=haptic_stops;
        view(true);poll_at(50.0);poll_at(51.0);input.stop_haptics();
        require(haptic_applies==applies+2 && haptic_stops==stopped);
        view(false);poll_at(51.1);
        // A stop between the poll that queues the buzz and the frame's flush keeps it.
        view(true);poll_at(52.0);applies=haptic_applies;
        require(input.poll(true,53.0) && haptic_applies==applies);
        input.stop_haptics();input.flush_haptics();
        require(haptic_applies==applies+2 && haptic_duration==80'000'000);
        view(false);poll_at(53.1);
        // Rumble and the buzz in the same frame coalesce: one pulse per hand,
        // strongest amplitude, longest duration.
        view(true);poll_at(54.0);applies=haptic_applies;
        require(input.poll(true,55.0) && input.apply_haptics(authored));
        input.flush_haptics();
        require(haptic_applies==applies+2 && std::abs(haptic_amplitude-.6F)<.00001F
            && haptic_duration==80'000'000);
        view(false);poll_at(55.1);

        // Menu + View held 0.5 s opens the runtime menu once; the View press is
        // swallowed (no tap, no recentre) even when held for seconds afterwards.
        menu_button(true);view(true);
        c=poll_at(60.0);require(c.menu_pressed && !c.menu_chord_pressed && !c.select);
        c=poll_at(60.49);require(!c.menu_chord_pressed);
        c=poll_at(60.5);require(c.menu_chord_pressed && !c.recentre_pressed);
        c=poll_at(60.6);require(!c.menu_chord_pressed);
        menu_button(false);applies=haptic_applies;
        c=poll_at(62.0);c=poll_at(64.0);
        require(!c.recentre_pressed && !c.recentre_height_pressed && haptic_applies==applies);
        view(false);c=poll_at(64.1);require(!c.select && !c.select_pressed);
        // View first, then Menu: still the chord, never a tap.
        view(true);poll_at(70.0);menu_button(true);c=poll_at(70.2);
        c=poll_at(70.7);require(c.menu_chord_pressed);
        view(false);c=poll_at(70.8);require(!c.select_pressed);
        menu_button(false);poll_at(70.9);
        // A short Menu + View touch (< 0.5 s) opens nothing and sends no tap.
        menu_button(true);view(true);poll_at(80.0);
        view(false);menu_button(false);c=poll_at(80.3);
        require(!c.menu_chord_pressed && !c.select_pressed && !c.recentre_pressed);

        // Held through focus loss: nothing fires on resume until it is released.
        view(true);poll_at(90.0);
        require(input.poll(false,91.0));
        c=poll_at(92.0);require(!c.recentre_pressed && !c.select);
        c=poll_at(95.0);require(!c.recentre_pressed && !c.recentre_height_pressed);
        view(false);c=poll_at(95.1);require(!c.select_pressed);
        view(true);poll_at(96.0);view(false);c=poll_at(96.2);require(c.select_pressed);

        // R Menu is still the game's Start, independent of the View timing.
        menu_button(true);c=poll_at(100.0);require(c.menu_pressed && c.menu);
        menu_button(false);poll_at(100.1);
        independent_buttons=false;button_states={};
    }
    mock_profiles={XR_NULL_PATH,XR_NULL_PATH};
    const auto applies_before_unbound=haptic_applies;
    require(input.poll(true) && input.focused() && !input.haptics_available());
    require(!input.apply_haptics(authored) && haptic_applies==applies_before_unbound);
    input.close();require(destroyed==5);
    {
        // Every target except the Steam Frame keeps the original thirteen actions
        // and the Simple and Touch profiles, with no D-pad, rumble or Index entries.
        created=0;const auto before=suggestions;expect_original_bindings=true;
        OpenXrInput plain(api);
        require(!plain.frame_player());
        require(plain.initialize(handle<XrInstance>(1),handle<XrSession>(9)));
        require(created==13 && suggestions==before+2);
        mock_profiles={path_value("/interaction_profiles/oculus/touch_controller"),
            path_value("/interaction_profiles/oculus/touch_controller")};
        require(plain.poll(true) && plain.focused() && !plain.haptics_available());
        require(!plain.apply_haptics({0x1111U,0x8888U,40U}));
        plain.close();expect_original_bindings=false;
    }
    {
        // Every target except the Steam Frame keeps the original input: Select on
        // the press edge, the four-input reset chord, and none of the system layer.
        OpenXrInput legacy(api);
        created=0;expect_original_bindings=true;
        require(legacy.initialize(handle<XrInstance>(1),handle<XrSession>(8)));
        expect_original_bindings=false;
        require(!legacy.system_layer() && legacy.haptics_strength()==1.F);
        independent_buttons=true;button_states={};
        require(legacy.poll(true,0.0) && !legacy.controls().reset_pressed);
        button_states[12]=button_states[13]=true;
        require(legacy.poll(true,0.1) && !legacy.controls().reset_pressed);
        button_states[7]=button_states[8]=true;
        require(legacy.poll(true,0.2) && !legacy.controls().reset_pressed); // Sticks first is not the chord.
        button_states[12]=button_states[13]=false;require(legacy.poll(true,0.3));
        button_states[12]=true;require(legacy.poll(true,0.4) && !legacy.controls().reset_pressed);
        button_states[13]=true;require(legacy.poll(true,0.5) && legacy.controls().reset_pressed);
        require(legacy.poll(true,0.6) && !legacy.controls().reset_pressed); // Never repeat while held.
        button_states={};require(legacy.poll(true,0.7));
        // Rumble stays off: the original input has no haptic action, even on Touch.
        mock_profiles={path_value("/interaction_profiles/oculus/touch_controller"),
            path_value("/interaction_profiles/oculus/touch_controller")};
        require(legacy.poll(true,0.8) && !legacy.haptics_available());
        const auto direct_applies=haptic_applies;
        require(!legacy.apply_haptics(authored) && haptic_applies==direct_applies);
        legacy.flush_haptics();require(haptic_applies==direct_applies);
        // View is a plain Select on its press edge. Holding it recentres nothing,
        // and Menu + View is only two buttons (the app opens its menu on the edge).
        button_states[9]=true;
        require(legacy.poll(true,10.0));
        require(legacy.controls().select && legacy.controls().select_pressed && legacy.controls().view_down==false);
        for(const double at:{10.1,11.0,13.5,20.0}) {
            require(legacy.poll(true,at));
            const auto& held_view=legacy.controls();
            require(held_view.select && !held_view.select_pressed && !held_view.recentre_pressed
                && !held_view.recentre_height_pressed && !held_view.menu_chord_pressed);
        }
        button_states[9]=false;require(legacy.poll(true,20.1) && !legacy.controls().select);
        const auto legacy_applies=haptic_applies;
        button_states[6]=true;button_states[9]=true;
        require(legacy.poll(true,30.0));
        require(legacy.controls().menu_pressed && legacy.controls().select_pressed
            && !legacy.controls().menu_chord_pressed);
        require(legacy.poll(true,31.0) && !legacy.controls().menu_chord_pressed
            && haptic_applies==legacy_applies); // No system buzz.
        button_states={};independent_buttons=false;
        legacy.close();
    }

    {
        const auto bit=vr_control_bit;
        VrControls xr,pad;
        xr.active_actions=bit(VrControlAction::steer)|bit(VrControlAction::fire)
            |bit(VrControlAction::menu);
        pad.active_actions=xr.active_actions;
        xr.steer={0.0F,0.0F};pad.steer={0.8F,0.0F};
        xr.fire=false;pad.fire=true;
        xr.menu=true;xr.menu_pressed=false;pad.menu=true;pad.menu_pressed=true;
        auto selected=select_vr_control_sources(xr,pad);
        require(selected.steer.x==0.0F && !selected.fire && !selected.menu_pressed);
        xr.active_actions &= ~bit(VrControlAction::fire);
        xr.active_actions &= ~bit(VrControlAction::menu);
        selected=select_vr_control_sources(xr,pad);
        require(selected.fire && selected.menu_pressed);

        // A usable XR source wins only for its own active action. The desktop
        // can still supply Menu when its XR binding is inactive.
        xr.active_actions=bit(VrControlAction::fire);
        xr.fire=false;xr.menu=false;xr.menu_pressed=false;
        pad.menu=true;pad.menu_pressed=true;
        selected=select_vr_control_sources(xr,pad);
        require(!selected.fire && selected.menu && selected.menu_pressed);

        for(bool frame:{false,true}) {
            VrControls face;
            desktop_face_buttons(face,frame,true,false,false,false);
            require(face.menu_confirm && face.menu_confirm_active);
            require(face.fire==!frame && face.brake==frame && !face.boost && !face.bomb);
            desktop_face_buttons(face,frame,false,false,true,false);
            require(!face.menu_confirm && face.fire==frame && face.boost==!frame);
            desktop_face_buttons(face,frame,false,false,false,true);
            require(face.boost==frame && face.brake==!frame);
        }
        xr.menu_confirm_active=true;xr.menu_confirm=false;
        pad.menu_confirm_active=true;pad.menu_confirm=true;
        require(!select_vr_control_sources(xr,pad).menu_confirm);
        xr.menu_confirm_active=false;
        require(select_vr_control_sources(xr,pad).menu_confirm);
        FrameMenu physical_menu;physical_menu.selection=5;
        VrControls physical_a;desktop_face_buttons(physical_a,true,true,false,false,false);
        physical_menu.sample({},true);physical_menu.sample(physical_a,true);
        require(physical_menu.page==FrameMenu::Page::exit_confirmation);

        DesktopControlEdges edges(true);
        VrControls held;
        held.active_actions=bit(VrControlAction::menu)|bit(VrControlAction::select)
            |bit(VrControlAction::roll_left)|bit(VrControlAction::roll_right)
            |bit(VrControlAction::stick_left)|bit(VrControlAction::stick_right);
        held.menu=held.select=held.roll_left=held.roll_right=true;
        held.stick_left=held.stick_right=true;
        auto edge=edges.sample(held,50.0);
        require(edge.menu_pressed && !edge.select_pressed && edge.view_down && !edge.menu_chord_pressed);
        // Held since before arming (e.g. through focus resume): never a hold.
        edge=edges.sample(held,60.0);
        require(!edge.menu_pressed && !edge.select_pressed && !edge.recentre_pressed
            && !edge.menu_chord_pressed);
        selected=select_vr_control_sources({},edge);
        require(selected.menu && !selected.menu_pressed && !selected.select
            && selected.view_down && !selected.menu_chord_pressed);
        static_cast<void>(edges.sample({},61.0));
        // Menu + View held 0.5 s opens the runtime menu, once, with no Select.
        edge=edges.sample(held,70.0);
        require(edge.menu_pressed && !edge.menu_chord_pressed && !edge.select_pressed);
        require(!edges.sample(held,70.49).menu_chord_pressed);
        edge=edges.sample(held,70.5);
        require(edge.menu_chord_pressed && !edge.recentre_pressed
            && select_vr_control_sources({},edge).menu_chord_pressed);
        require(!edges.sample(held,70.6).menu_chord_pressed);
        edge=edges.sample({},75.0);
        require(!edge.select_pressed && !edge.recentre_pressed); // Chord View never taps.
        // L View alone: short press is a tap on release; holds recentre.
        VrControls view_only;view_only.active_actions=bit(VrControlAction::select);view_only.select=true;
        require(!edges.sample(view_only,80.0).select_pressed);
        VrControls idle;idle.active_actions=bit(VrControlAction::select);
        edge=edges.sample(idle,80.3);
        require(edge.select_pressed && edge.select && !edge.recentre_pressed
            && select_vr_control_sources({},edge).select_pressed);
        require(!edges.sample({},80.4).select);
        static_cast<void>(edges.sample(view_only,90.0));
        edge=edges.sample(view_only,90.99);
        require(!edge.recentre_pressed && !edge.select);
        edge=edges.sample(view_only,91.0);
        require(edge.recentre_pressed && !edge.recentre_height_pressed && !edge.select);
        require(!edges.sample(view_only,92.5).recentre_pressed);
        edge=edges.sample(view_only,93.0);
        require(edge.recentre_pressed && edge.recentre_height_pressed
            && select_vr_control_sources({},edge).recentre_height_pressed);
        edge=edges.sample({},93.1);
        require(!edge.select_pressed && !edge.recentre_pressed); // A hold is not also a tap.
        // Both bumpers + both stick clicks are ordinary game inputs again.
        VrControls four;four.roll_left=four.roll_right=four.stick_left=four.stick_right=true;
        four.active_actions=bit(VrControlAction::roll_left)|bit(VrControlAction::roll_right)
            |bit(VrControlAction::stick_left)|bit(VrControlAction::stick_right);
        for(const double at:{100.0,100.1,103.0,106.0}) {
            edge=edges.sample(four,at);
            require(edge.roll_left && edge.roll_right && edge.stick_left && edge.stick_right
                && !edge.menu_pressed && !edge.select_pressed && !edge.recentre_pressed
                && !edge.menu_chord_pressed);
        }
        // Without the system layer the desktop fallback keeps the original edges.
        DesktopControlEdges plain;
        edge=plain.sample(held,200.0);
        require(edge.menu_pressed && edge.select_pressed && edge.reset_pressed
            && !edge.view_down && !edge.recentre_pressed && !edge.menu_chord_pressed);
        static_cast<void>(plain.sample(held,200.1)); // Held while unfocused; discard this sample.
        edge=plain.sample(held,200.2); // Same level after focus resume is not a new press.
        require(!edge.menu_pressed && !edge.select_pressed && !edge.reset_pressed);
        selected=select_vr_control_sources({},edge);
        require(selected.menu && !selected.menu_pressed
            && selected.select && !selected.select_pressed && !selected.reset_pressed);
        for(const double at:{201.0,202.5,205.0}) {
            edge=plain.sample(held,at); // Long holds do nothing special.
            require(edge.select && !edge.select_pressed && !edge.recentre_pressed
                && !edge.recentre_height_pressed && !edge.menu_chord_pressed);
        }
        static_cast<void>(plain.sample({},206.0));
        edge=plain.sample(held,207.0);
        require(edge.menu_pressed && edge.select_pressed && edge.reset_pressed);
    }
    VrGameInput game_input;VrControls controls;
    controls.fire=true;controls.bomb=true;controls.boost=true;controls.brake=true;
    controls.roll_left=true;controls.roll_right=true;controls.steer={-1,1};
    controls.menu=true;game_input.sample(controls);
    auto tick=game_input.consume();
    const auto expected=starfox::input::y|starfox::input::a|starfox::input::x|starfox::input::b
        |starfox::input::left_shoulder|starfox::input::right_shoulder|starfox::input::left|starfox::input::up;
    require(tick.held==expected && tick.pressed==expected && tick.released==0);
    game_input.sample(controls);tick=game_input.consume();require(tick.pressed==0 && tick.held==expected);
    controls.menu_pressed=true;game_input.sample(controls);tick=game_input.consume();require(tick.pressed==starfox::input::start);
    controls.menu_pressed=false;game_input.sample(controls);tick=game_input.consume();require(tick.pressed==0 && (tick.held&starfox::input::start));
    game_input.sample({});tick=game_input.consume();require(tick.held==0 && tick.released==(expected|starfox::input::start));
    controls={};controls.fire=true;game_input.sample(controls);game_input.sample({});
    tick=game_input.consume();require(tick.held==0 && tick.pressed==starfox::input::y && tick.released==starfox::input::y);
    game_input.reset();tick=game_input.consume();require(tick.held==0 && tick.pressed==0 && tick.released==0);
    controls={};controls.select=true;game_input.sample(controls);
    require(game_input.consume().held==0); // Held during focus regain is ignored.
    controls.select_pressed=true;game_input.sample(controls);
    tick=game_input.consume();require(tick.pressed==starfox::input::select);
    controls.select_pressed=false;game_input.sample(controls);
    tick=game_input.consume();require(tick.pressed==0 && tick.held==starfox::input::select);
    game_input.sample({});tick=game_input.consume();require(tick.released==starfox::input::select);
    {
        VrGameInput face_buttons;
        controls={};controls.fire=true;face_buttons.sample(controls);
        require(face_buttons.consume().held==starfox::input::y); // Frame A fires.
        face_buttons.reset();controls={};controls.bomb=true;face_buttons.sample(controls);
        require(face_buttons.consume().held==starfox::input::a); // Frame B bombs.
        face_buttons.reset();controls={};controls.boost=true;face_buttons.sample(controls);
        require(face_buttons.consume().held==starfox::input::x); // Frame X boosts.
        face_buttons.reset();controls={};controls.brake=true;face_buttons.sample(controls);
        require(face_buttons.consume().held==starfox::input::b); // Frame Y brakes.
    }
    std::cout<<"VR action lifecycle, focus, deadzone and edge tests passed (injected runtime)\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
