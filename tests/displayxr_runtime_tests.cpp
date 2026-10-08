#include "starfox/render/displayxr_runtime.hpp"
#include "starfox/render/displayxr_abi.hpp"
#include "starfox/render/displayxr_session.hpp"
#include "starfox/render/displayxr_recovery.hpp"
#include "starfox/render/calibrated_effects.hpp"
#include "starfox/render/effects.hpp"
#include "starfox/render/frame_persistence.hpp"
#include "starfox/render/global_enhancements.hpp"
#include "starfox/render/bloom.hpp"
#include "starfox/render/hdr_effect.hpp"
#include "starfox/render/chromatic_aberration.hpp"
#include "starfox/render/adaptive_exposure.hpp"
#include "starfox/render/camera_response.hpp"
#include "starfox/render/calibrated_game_scene.hpp"
#include "starfox/render/calibrated_ground.hpp"
#include "starfox/render/gpu_calibrated_msaa.hpp"
#include "starfox/render/calibrated_reconstructed_post_memory.hpp"
#include "starfox/render/calibrated_game_motion.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/render/calibrated_motion_history.hpp"
#include "starfox/render/motion_blur.hpp"
#include "starfox/render/temporal_aa.hpp"
#include "calibrated_pattern_oracle.hpp"
#include "calibrated_edge_oracle.hpp"
#include "starfox/render/volumetric_fog.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
#include "reflected_curved_owner_oracle.hpp"
#include "reflected_curved_source_guard.hpp"
#include <atomic>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include "starfox/render/sdl_dxr_shadows.hpp"
#include "starfox/render/vulkan_ray_support.hpp"
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>
#include <SDL3/SDL.h>
#include "starfox/render/displayxr_d3d12_presenter.hpp"
#include "starfox/render/gpu_calibrated_scene.hpp"
#include "starfox/render/displayxr_game_renderer.hpp"
#include "starfox/render/gpu_calibrated_reflection_history.hpp"
#include "starfox/render/fsr1_settings.hpp"
#include "starfox/render/calibrated_effects.hpp"
#include "starfox/render/displayxr_desktop.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/vr/draw_packet.hpp"
#include "starfox/render/sdl_d3d12_bridge.h"
#include "starfox/render/sdl_vulkan_bridge.h"
#include "starfox/render/displayxr_vulkan_binding.hpp"
#if defined(STARFOX_CURVED_OWNER_TRACE_AVAILABLE)
#include "calibrated_curved_trace_1_dxil.hpp"
#include "starfox/render/calibrated_environment.hpp"
#endif
#endif

namespace {
using namespace starfox::render;
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
bool native_taa_only{};
bool native_taa_water_only{};
bool native_taa_palette_only{};
bool native_taa_pattern_only{};
bool native_taa_edge_only{};
bool native_taa_reconstructed_post_only{};
bool native_taa_liquid_post_only{};
bool native_taa_local_layers_only{};
bool native_taa_ground_clock_only{};
bool native_taa_reflection_only{};
bool native_msaa_reflection_only{};
bool native_msaa_reflection_water_only{};
bool native_msaa_reflection_lava_only{};
bool native_curved_owner_only{};
bool native_source_frame_owner_only{};
bool native_curved_msaa_owner_only{};
bool native_curved_msaa_resolve_only{};
bool native_curved_lens_owner_only{};
bool native_curved_motion_owner_only{};
bool native_curved_camera_owner_only{};
bool native_curved_disocclusion_owner_only{};
bool native_fsr_only{};
bool native_dlss_only{};
bool native_dlss_failure_only{};
bool native_dlss_ray_samples_only{};
bool native_dlss_ray_mixed_only{};
bool native_dlss_ray_materials_only{};
bool native_dlss_ray_rough_only{};
bool native_dlss_ray_liquids_only{};
bool native_dlss_ray_liquid_primary_edge_only{};
bool native_dlss_ray_liquid_primary_wrap_only{};
bool native_dlss_panel_bound_only{};
bool native_dlss_reconstructed_post_only{};
bool native_dlss_liquid_post_only{};
bool native_scene_fx_only{};
bool native_fog_only{};
bool native_blur_only{};
bool native_ground_only{};
bool native_ground_cleanup_failure{};
#include "calibrated_aa_oracle.inc"
#endif
struct Mock {
    std::vector<XrExtensionProperties> extensions;
    const char* runtime_name="DisplayXR Runtime (based on Monado/XRT) 'v2.21.11'";
    const char* system_name="DisplayXR: Leia 3D Display";
    const char* missing{};
    XrResult create_result{XR_SUCCESS},destroy_result{XR_SUCCESS},system_result{XR_SUCCESS},properties_result{XR_SUCCESS};
    bool panel_confirmed{true},wrong_system_id{},zero_monitor{},unterminated{},bad_sample{};
    float scale=.5F,physical_width=.6F,viewer_z=.65F;
    unsigned query_eyes{2},read_eyes{2},image_width{1920},image_height{2160},extension_count_override{};
    DisplayXrBackend backend{DisplayXrBackend::direct3d12};
    Mock() {
        const std::array<const char*,5> names{displayxr::display_info_extension,displayxr::view_rig_extension,
            displayxr::window_extension,"XR_KHR_D3D12_enable","XR_KHR_vulkan_enable2"};
        const std::array<unsigned,5> versions{21,3,8,1,1};
        for(unsigned i=0;i<names.size();++i) {
            XrExtensionProperties p{XR_TYPE_EXTENSION_PROPERTIES};
            std::strcpy(p.extensionName,names[i]);p.extensionVersion=versions[i];extensions.push_back(p);
        }
    }
} mock;
struct NativeMock {
    XrResult create_result{XR_SUCCESS},space_result{XR_SUCCESS},locate_result{XR_SUCCESS};
    bool hardware{true},requestable{true},active{},ignore_request{},visible{true},valid_tracking{true};
    bool bad_pose{},bad_raw{},bad_physical{},tracking{true},ready{},loss{},frame_open{},session_open{},space_open{};
    unsigned columns{2},eyes{2},mode_count_override{},session_creates{},session_destroys{},space_creates{},space_destroys{};
    unsigned waits{},begins{},ends{},last_layers{},requests{},locates{};
    float head_x{12},pose_scale{1};
    float head_yaw{}; // Real located-eye quaternion, not a colour/motion proxy.
    float sample_x{},sample_y{}; // Fixture-only pixel offset in sixteenths (fractional for TAA).
    std::array<XrView,2> located_views{}; // The last located frame, not a later live head pose.
    DisplayXrRigSettings expected;
} native_mock;
XrFovf fixture_fov() {
    constexpr XrFovf base{-.5F,.6F,.4F,-.5F};
    if(!native_mock.sample_x && !native_mock.sample_y) return base;
    // Shift the actual float-valued XR FOV, not different decimal-double
    // angles. The independent reference must retain the centre view's
    // original frustum before translating its pixel sample in tangent space.
    const double l=std::tan(double(base.angleLeft)),r=std::tan(double(base.angleRight));
    const double d=std::tan(double(base.angleDown)),u=std::tan(double(base.angleUp));
    const double dx=native_mock.sample_x*(r-l)/(16*mock.image_width);
    const double dy=-native_mock.sample_y*(u-d)/(16*mock.image_height);
    return {float(std::atan(l+dx)),float(std::atan(r+dx)),float(std::atan(u+dy)),float(std::atan(d+dy))};
}
unsigned creates{},destroys{};
std::uint64_t checks{};
void check(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
void camera_light_tests() {
    check(calibrated_msaa_extent_supported(128,72,128,72,8,true,3,true),"Valid native MSAA working extent rejected");
    check(calibrated_msaa_extent_supported(1700,1700,1700,1700,8,false,0,true)
        && !calibrated_msaa_extent_supported(1700,1700,1700,1700,8,false,0,true,false,false,true),
        "Style-only liquid MSAA omitted its reusable raw float guide from the resident bound");
    check(calibrated_msaa_extent_supported(128,72,128,72,8,true,3,true,true),"Valid fog/MSAA working extent rejected");
    check(calibrated_msaa_extent_supported(1250,1250,1250,1250,8,true,3,true)
        && !calibrated_msaa_extent_supported(1250,1250,1250,1250,8,true,3,true,true),
        "Native fog/MSAA omitted retained integral/hierarchy storage from its working bound");
    check(!calibrated_msaa_extent_supported(16384,16384,16384,16384,8,true,3,true)
        && !calibrated_msaa_extent_supported(0,72,128,72,2,false,0,false)
        && !calibrated_msaa_extent_supported(128,72,128,72,3,false,0,false)
        && !calibrated_msaa_extent_supported(128,72,128,72,2,false,4,false),"Unbounded/invalid native MSAA allocation accepted");
    constexpr starfox::vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    starfox::vr::EyeCamera camera{identity,identity};
    const std::array<double,3> source{2,-3,5};
    check(calibrated_game_light(camera,source,{})==source,"Identity native light changed direction");
    camera.view[12]=17;camera.view[13]=-8;camera.view[14]=12;
    check(calibrated_game_light(camera,source,{})==source,"Eye translation changed directional light");
    // Independent analytic quarter-turn head roll, then a small response yaw.
    // The light must follow BOTH rotations, never the translated eye origin.
    camera.view[0]=0;camera.view[1]=1;camera.view[4]=-1;camera.view[5]=0;
    check(calibrated_game_light(camera,source,{})==std::array<double,3>{-3,-2,5},
        "Tracked head orientation did not rotate game-rig light");
    constexpr double yaw=.019;
    const std::array<double,3> expected{-3,-2*std::cos(yaw)-5*std::sin(yaw),
        5*std::cos(yaw)-2*std::sin(yaw)};
    const auto actual=calibrated_game_light(camera,source,{0,yaw,0});
    check(bool(actual),"Valid response/head light transform rejected");
    double norm=0;
    for(unsigned i=0;i<3;++i) {
        check(std::abs((*actual)[i]-expected[i])<1.e-6,"Response/head light transform differs from analytic rotation");
        norm+=(*actual)[i]*(*actual)[i];
    }
    check(std::abs(norm-38)<2.e-6,"Response/head light introduced scale");
    for(auto invalid:std::array<std::array<double,3>,6>{{{0,0,0},{1.e-7,0,0},{NAN,0,0},
        {0,INFINITY,0},{0,0,-INFINITY},{1000001,0,0}}})
        check(!calibrated_game_light(camera,invalid,{}),"Invalid source light accepted");
    for(auto invalid:std::array<std::array<double,3>,3>{{{NAN,0,0},{0,.051,0},{0,0,INFINITY}}})
        check(!calibrated_game_light(camera,source,invalid),"Invalid response light pose accepted");
    camera.view[0]=NAN;
    check(!calibrated_game_light(camera,source,{}),"Non-finite tracked light view accepted");
    camera.view=identity;camera.projection[8]=INFINITY;
    check(!calibrated_game_light(camera,source,{}),"Non-finite tracked light projection accepted");
}
void game_motion_identity_tests() {
    constexpr starfox::vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    auto projection=identity;projection[10]=projection[11]=-1;projection[14]=-.01F;projection[15]=0;
    const starfox::vr::EyeCamera camera{identity,projection};
    CalibratedGameFrame old;auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();
    old.previous=old.current=snapshot;
    for(unsigned key:{1U,2U}) {
        snapshot->transforms[starfox::simulation::ObjectHandle(key)].generation=4;
        snapshot->transforms[starfox::simulation::ObjectHandle(key)].shape=std::uint16_t(key+10);
        starfox::vr::DrawPacket packet;starfox::vr::SceneVertex vertex{};vertex.position[2]=-2;vertex.color[3]=1;
        packet.geometry.vertices.assign(3,vertex);if(key==1) packet.geometry.line_vertices.assign(2,vertex);
        old.draws.push_back({std::move(packet),CalibratedGameLayer::model,starfox::vr::SceneBlend::opaque,true,key,true,false});
    }
    auto now=old;now.draws[0].packet.model[12]=.1F;now.draws[1].packet.model[12]=-.2F;
    const auto valid=[&](const CalibratedGameFrame& frame,const CalibratedGameFrame* prior) {
        return calibrated_game_motion_draws(frame,prior,camera,camera);
    };
    auto matched=valid(now,&old);
    check(matched.size()==3 && matched[0].valid && matched[1].valid && matched[2].valid
        && matched[1].previous_vertices.data()==old.draws[0].packet.geometry.line_view().data(),
        "Rigid source entity did not retain accepted endpoints in expanded triangle/line order");
    std::swap(now.draws[0],now.draws[1]);matched=valid(now,&old);
    check(matched.size()==3 && matched[0].valid && matched[1].valid && matched[2].valid
        && matched[0].previous_model==old.draws[1].packet.model,"Motion matched packet indices instead of source entities");
    const auto invalid=valid(now,nullptr);check(std::none_of(invalid.begin(),invalid.end(),[](const auto& m){return m.valid;}),"Missing accepted frame inferred motion");
    for(unsigned fault=0;fault<11;++fault) {
        auto bad=old;auto state=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);bad.current=state;
        if(fault==0) state->transforms[1].generation++;
        if(fault==1) state->transforms[1].shape++;
        if(fault==2) state->transforms[1].strategy_address++;
        if(fault==3) state->transforms[1].type++;
        if(fault==4) state->transforms[1].explosion_progress=1;
        if(fault==5) bad.draws[0].packet.geometry.vertices[0].position[0]=.1F;
        if(fault==6) bad.draws[0].packet.geometry.vertices[0].visibility_enabled=2;
        if(fault==7) bad.draws[0].packet.geometry.vertices[0].texture[3]=4;
        if(fault==8) bad.draws.push_back(bad.draws[0]);
        if(fault==9) state->transforms.erase(1);
        if(fault==10) bad.draws[0].packet.model[0]=0;
        const auto rejected=valid(bad,&old);check(!rejected[0].valid && !rejected[1].valid && rejected[2].valid,
            "Recycled/deformed/ambiguous/missing/singular object inherited rigid history");
    }
    {
        auto before=old;auto prior_state=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);before.current=prior_state;
        prior_state->transforms[1].explosion_progress=3;
        before.draws[0].packet.geometry.ranges={{5,0,3,-1}};
        for(auto& v:before.draws[0].packet.geometry.vertices) {
            v.visibility_enabled=2;v.group_c[0]=3.5F;v.group_c[1]=256;v.group_b[2]=12;
        }
        auto after=before;auto now_state=std::make_shared<starfox::vr::GameSceneSnapshot>(*prior_state);after.current=now_state;
        now_state->transforms[1].explosion_progress=8;
        for(auto& v:after.draws[0].packet.geometry.vertices) {
            v.group_c[0]=8.25F;v.group_a[0]=13;v.visibility_a[0]=.95F;v.visibility_a[2]=.21F;
        }
        auto motion=valid(after,&before);
        check(motion[0].valid && motion[0].previous_vertices.data()==before.draws[0].packet.geometry.vertex_view().data()
            && !motion[1].valid && motion[1].previous_vertices.empty() && motion[2].valid && motion[2].previous_vertices.empty(),
            "Matched destruction did not retain its accepted raw primitive or leaked into line/rigid draws");
        for(unsigned fault=0;fault<8;++fault) {
            auto bad=after;auto& geometry=bad.draws[0].packet.geometry;
            if(fault==0) geometry.ranges[0].source_face++;
            if(fault==1) geometry.vertices[1].position[0]=.25F;
            if(fault==2) geometry.vertices[0].group_b[2]++;
            if(fault==3) geometry.vertices[0].group_c[1]=128;
            if(fault==4) geometry.vertices[0].group_c[2]=1;
            if(fault==5) geometry.vertices[0].group_enabled=1;
            if(fault==6) bad.draws[0].packet.preserve_native_colour=true;
            if(fault==7) geometry.vertices[0].color[3]=.5F;
            check(!valid(bad,&before)[0].valid,"Changed destruction identity/visibility/units/alpha inherited accepted primitive");
        }
        before=old;after=old;
        for(auto* frame:{&before,&after}) for(auto& v:frame->draws[0].packet.geometry.vertices) {
            v.texture[3]=4U|134217728U;v.billboard[0]=.01F;v.billboard[1]=-.01F;v.group_a[0]=280;v.group_a[1]=570;
        }
        for(auto& v:after.draws[0].packet.geometry.vertices) {v.group_a[0]=330;v.group_a[1]=635;}
        motion=valid(after,&before);
        check(motion[0].valid && motion[0].previous_vertices.size()==3,"Matched source billboard sizing/depth was treated as rigid or discarded");
        after.draws[0].packet.geometry.vertices[1].billboard[1]=.01F;
        check(!valid(after,&before)[0].valid,"Changed billboard corner/order inherited accepted geometry");
        after=old;after.draws[0].packet.geometry.vertices[0].color[0]=.2F;
        motion=valid(after,&old);
        check(motion[0].valid && motion[0].previous_vertices.empty(),"Source shading-only change copied a second rigid primitive stream");
    }
    {
        auto before=old;auto after=old;
        before.draws[0].packet.geometry.vertices.clear();after.draws[0].packet.geometry.vertices.clear();
        auto line_motion=valid(after,&before);
        check(line_motion.size()==2 && line_motion[0].valid && line_motion[0].previous_vertices.size()==2,
            "Line-only source object did not retain accepted endpoints");
        for(unsigned fault=0;fault<6;++fault) {
            auto bad=after;auto& geometry=bad.draws[0].packet.geometry;
            if(fault==0) geometry.line_vertices[0].position[0]+=.1F;
            if(fault==1) geometry.line_vertices[0].texture[3]=8;
            if(fault==2) geometry.line_vertices[0].color[3]=.5F;
            if(fault==3) geometry.line_vertices.pop_back();
            if(fault==4) geometry.line_vertices[0].billboard[0]=.1F;
            if(fault==5) bad.draws[0].packet.preserve_native_colour=true;
            check(!valid(bad,&before)[0].valid,"Changed line identity/topology/artwork inherited endpoint history");
        }
        for(auto* f:{&before,&after}) for(auto& v:f->draws[0].packet.geometry.line_vertices) {
            v.visibility_enabled=2;v.group_c[0]=f==&before?3.5F:8.25F;v.group_c[1]=256;v.group_b[2]=12;
        }
        auto a=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot),b=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);
        a->transforms[1].explosion_progress=3;b->transforms[1].explosion_progress=8;before.current=a;after.current=b;
        check(valid(after,&before)[0].valid,"Matched destroyed source line did not retain its accepted payload");
    }
    check(same_calibrated_presentation(old,old) && same_calibrated_presentation(old,CalibratedGameFrame(old)),"Identical held frame was not recognized");
    auto changed=old;changed.draws[0].packet.model[12]=.1F;
    check(!same_calibrated_presentation(old,changed),"Moved model inherited held presentation");
    changed=old;changed.settings.effect_seconds=1;
    check(same_calibrated_presentation(old,changed),"Unused FX clock jittered a static preview");
    changed.settings.global_enhancements=old.settings.global_enhancements=1U<<16;
    check(!same_calibrated_presentation(old,changed),"Animated effect inherited held presentation");
}
void game_motion_preparation_tests() {
    namespace vr=starfox::vr;
    constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    auto projection=identity;projection[10]=projection[11]=-1;projection[14]=-.01F;projection[15]=0;
    CalibratedGameFrame before;auto snapshot=std::make_shared<vr::GameSceneSnapshot>();
    before.current=before.previous=snapshot;
    for(unsigned key:{1U,2U}) {
        snapshot->transforms[starfox::simulation::ObjectHandle(key)].generation=4;
        snapshot->transforms[starfox::simulation::ObjectHandle(key)].shape=std::uint16_t(key+10);
        vr::DrawPacket packet;vr::SceneVertex vertex{};vertex.position[2]=-2;vertex.color[3]=1;
        if(key==2) {vertex.texture[3]=4;vertex.billboard[0]=.02F;}
        packet.geometry.vertices.assign(3,vertex);
        if(key==1) packet.geometry.line_vertices.assign(2,vertex);
        before.draws.push_back({std::move(packet),CalibratedGameLayer::model,vr::SceneBlend::opaque,true,key,true,false});
    }
    before.settings.camera_response_pose={-.005,.02,-.01};
    auto now=before;now.settings.camera_response_pose={0,.01,.005};
    now.draws[0].packet.model[12]=.15F;
    const auto descriptors=now.packets(),previous_descriptors=before.packets();
    const CalibratedGameMotionPreparation prepared(now,&before,descriptors);
    std::array<vr::EyeCamera,2> eyes{{{identity,projection},{identity,projection}}},old_eyes=eyes;
    eyes[0].view[12]=.07F;eyes[1].view[12]=-.09F;
    old_eyes[0].view[12]=.025F;old_eyes[1].view[12]=-.035F;
    eyes[0].projection[8]=.13F;eyes[1].projection[8]=-.17F;
    old_eyes[0].projection[8]=.09F;old_eyes[1].projection[8]=-.11F;
    for(unsigned eye=0;eye<2;++eye) {
        const auto matches=prepared.draws(eyes[eye],old_eyes[eye]);
        check(matches.size()==3 && std::all_of(matches.begin(),matches.end(),[](const auto& m){return m.valid;}),
            "Pair-prepared motion lost a calibrated asymmetric eye or expanded draw ordering");
        check(matches[0].previous_model==previous_descriptors[0].model_override.value()
            && matches[1].previous_model==matches[0].previous_model
            && matches[2].previous_model==previous_descriptors[1].model_override.value(),
            "Pair-prepared motion lost accepted camera-response model overrides");
        check(matches[0].previous_vertices.empty()
            && matches[1].previous_vertices.data()==before.draws[0].packet.geometry.line_view().data()
            && matches[2].previous_vertices.data()==before.draws[1].packet.geometry.vertex_view().data(),
            "Pair-prepared motion lost rigid/line/billboard accepted stream ownership");
    }
    for(bool old_fault:{false,true}) {
        auto bad=eyes[0],bad_old=old_eyes[0];
        (old_fault?bad_old:bad).projection[0]=-1;
        const auto rejected=prepared.draws(bad,bad_old);
        check(std::none_of(rejected.begin(),rejected.end(),[](const auto& m){return m.valid || !m.previous_vertices.empty();}),
            "Pair-prepared motion admitted a malformed per-eye current/accepted calibration");
        const auto unaffected=prepared.draws(eyes[1],old_eyes[1]);
        check(std::all_of(unaffected.begin(),unaffected.end(),[](const auto& m){return m.valid;}),
            "One rejected eye mutated the pair's prepared source correspondence");
    }
    // SDK and blur may refer to different accepted presentations. Preparing
    // once per source must never alias their model transforms/primitive spans.
    auto other=before;other.draws[0].packet.model[12]=-.2F;
    const CalibratedGameMotionPreparation other_history(now,&other,descriptors);
    const auto first=prepared.draws(eyes[0],old_eyes[0]),second=other_history.draws(eyes[0],old_eyes[0]);
    check(first[0].valid && second[0].valid && first[0].previous_model!=second[0].previous_model
        && first[1].previous_vertices.data()!=second[1].previous_vertices.data()
        && first[2].previous_vertices.data()!=second[2].previous_vertices.data(),
        "Different accepted native timelines shared source models or primitive streams");
    for(unsigned fault=0;fault<2;++fault) {
        auto bad=descriptors;if(fault) bad[0].packet=bad[1].packet;else bad.pop_back();
        bool rejected=false;
        try {(void)CalibratedGameMotionPreparation(now,&before,bad);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Pair-prepared motion borrowed mismatched current packet descriptors");
    }
}
void palette_history_policy_tests() {
    const CalibratedPostExtents hd{{{1920,1080},{1920,1080}}},uhd{{{3840,2160},{3840,2160}}};
    check(calibrated_reconstructed_post_extent_supported(hd,hd),"Full-HD DLAA post working images were incorrectly rejected");
    check(!calibrated_reconstructed_post_extent_supported(uhd,uhd),"Unbounded UHD DLAA post working images were admitted");
    const auto heavy=calibrated_msaa_working_bytes(1920,1080,1920,1080,8,true,0,true,true,true,true);
    check(heavy && !calibrated_reconstructed_post_extent_supported(hd,hd,*heavy),
        "Combined SDK/MSAA source+panel guard ignored selected sample/shutter/liquid/fog allocations");
    check(!calibrated_reconstructed_post_extent_supported(hd,hd,0,1024ULL*1024*1024)
        && !calibrated_reconstructed_post_extent_supported(hd,hd,UINT64_MAX)
        && !calibrated_reconstructed_post_extent_supported(hd,hd,0,UINT64_MAX),
        "Combined post bound omitted transactional old storage or overflowed arithmetic");
    for(unsigned fault=0;fault<3;++fault) {
        auto bad=hd;if(fault==0)bad[0][0]=0;if(fault==1)bad[1][1]=16385;if(fault==2)bad[0]={16384,16384};
        check(!calibrated_reconstructed_post_extent_supported(bad,hd)
            && !calibrated_reconstructed_post_extent_supported(hd,bad),"Invalid source or panel post extent was accepted");
    }
    // Independent authored shader IDs, not the policy under test. All other
    // effects remain conservative for MOTION correspondence. Clock identity
    // is separate: only authored animated IDs actually consume time.
    constexpr unsigned palettes[]{4,8,9,11,14,15,16,17,18,19,20,21,22,23,24,
        31,32,36,38,64,65,69,71,72,73};
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();
    for(unsigned effect=0;effect<=unsigned(effect_count)+1;++effect) for(unsigned intensity:{0U,35U,100U}) {
        const bool pointwise=std::find(std::begin(palettes),std::end(palettes),effect)!=std::end(palettes);
        const bool reject=effect && intensity && !pointwise;
        for(unsigned layer:{0U,1U}) {
            CalibratedPostEffects pass;
            if(layer) {pass.model=effect;pass.model_intensity=intensity;}
            else {pass.world=effect;pass.world_intensity=intensity;}
            check(calibrated_post_requires_motion_rejection(pass)==reject,"Post history policy lost a palette or admitted an untracked effect");
            check(calibrated_post_motion_rejection_layers(pass)==(reject?(layer?2U:1U):0U),
                "Untracked post effect rejected the other ownership layer");
            const bool pattern=effect==5 || effect==10 || effect==25 || effect==70;
            const bool edge=edge_oracle_style(effect);
            check(calibrated_post_motion_rejection_layers(pass,true)==(reject && !pattern?(layer?2U:1U):0U),
                "Phase-witness policy admitted an untracked neighbour/warp or rejected a supported pattern");
            check(calibrated_edge_effect(effect)==edge
                && calibrated_post_motion_rejection_layers(pass,true,true)==(reject && !pattern && !edge?(layer?2U:1U):0U),
                "Edge-witness policy admitted an untracked effect or omitted a supported branch");
            const std::array<CalibratedPostEffects,3> ordered{pass,{}, {}};
            check(calibrated_post_after_reconstruction(ordered)==(reject && !pattern && !edge),
                "Reconstruction-first policy omitted an active multi-source effect or moved a witnessed chain");
            check(calibrated_sdk_post_after_reconstruction(ordered)==(reject && !pattern),
                "SDK reconstruction policy assumed absent edge witnesses or moved its witnessed palette/pattern path");
            CalibratedGameFrame old;old.current=old.previous=snapshot;
            auto& settings=layer?old.settings.model_effects:old.settings.world_effects;
            settings={effect,intensity,0,0};auto now=old;now.settings.effect_seconds=2;
            const bool timed=intensity && (effect==46 || effect==47 || (effect>=84 && effect<=91) || effect>=effect_count);
            check(same_calibrated_presentation(now,old)==!timed,"Static post clock changed held identity or froze an authored animated effect");
        }
    }
    for(unsigned palette:palettes) {
        CalibratedGameSettings settings;settings.world_effects={palette,35,0,0};settings.model_effects={palette,100,0,0};
        const auto rejected=[](const auto& passes) {return std::any_of(passes.begin(),passes.end(),calibrated_post_requires_motion_rejection);};
        check(!rejected(calibrated_game_post_effects(settings)),"Combined world/model palettes rejected geometric motion");
        for(unsigned slot=0;slot<4;++slot) {
            auto mixed=settings;
            if(slot==0) mixed.world_effects={unsigned(Effect::ink),100,0,0};
            if(slot==1) mixed.manipulation=unsigned(Effect::ripple_warp);
            if(slot==2) mixed.extra_effects[0]=unsigned(Effect::kaleidoscope);
            if(slot==3) mixed.extra_effects[1]=unsigned(Effect::energy_shield);
            check(rejected(calibrated_game_post_effects(mixed)),"A palette masked an unsupported effect in another post pass");
            CalibratedGameFrame a;a.current=a.previous=snapshot;a.settings=mixed;auto b=a;b.settings.effect_seconds=2;
            check(same_calibrated_presentation(a,b)==(slot!=3),"A static chain advanced its unused clock or froze separate animated FX");
        }
        settings.extra_effects[2]=unsigned(Effect::radar_sweep);
        check(rejected(calibrated_game_post_effects(settings)),"Final world post pass omitted motion rejection");
    }
    for(unsigned global=0;global<13;++global) for(unsigned quality=1;quality<=3;++quality) {
        CalibratedGameFrame a;a.current=a.previous=snapshot;a.settings.global_enhancements=quality<<(global*2);
        auto b=a;b.settings.effect_seconds=2;
        check(same_calibrated_presentation(a,b)==(global!=5 && global!=8),"Global post clock misclassified authored Heat Haze/Film Grain or a static kernel");
    }
    std::array<CalibratedPostEffects,3> passes{{{5,10,35,100},{25,70,100,35},{8,7,100,100}}};
    const auto patterns=calibrated_pattern_guide(passes,4);
    check(patterns.valid() && patterns.scale==4 && patterns.passes[0]==CalibratedPatternPass{5,10,35,100}
        && patterns.passes[1]==CalibratedPatternPass{25,70,100,35} && patterns.passes[2]==CalibratedPatternPass{},
        "Pattern descriptor lost pass order, intensity, grid or canonicalization");
    for(unsigned fault=0;fault<5;++fault) {
        auto bad=patterns;
        if(fault==0) bad.scale=0;if(fault==1) bad.scale=16385;if(fault==2) bad.passes[0].world=7;
        if(fault==3) bad.passes[0].world_intensity=101;if(fault==4) bad.passes[2].model_intensity=1;
        check(!bad.valid(),"Malformed temporal pattern descriptor accepted");
    }
    for(unsigned style:edge_oracle_styles) for(unsigned scope:{1U,2U,3U}) {
        std::array<CalibratedPostEffects,3> edge_passes{{{scope&1?style:0,scope&2?style:0,35,100},{0,0,0,0},{6,39,100,35}}};
        const auto edge=calibrated_edge_guide(edge_passes,3);
        check(edge.valid() && edge.active() && edge.masks()==std::array<unsigned,2>{64U+unsigned(bool(scope&1)),64U+unsigned(bool(scope&2))}
            && edge.passes[0].world_intensity==(scope&1?35U:0U) && edge.passes[2]==CalibratedPatternPass{6,39,100,35},
            "Edge descriptor lost receiver, original slot, intensity or scale");
        for(unsigned fault=0;fault<5;++fault) {
            auto bad=edge;if(fault==0) bad.scale=0;if(fault==1) bad.scale=16385;
            if(fault==2) bad.passes[2].world=7;if(fault==3) bad.passes[2].model_intensity=101;
            if(fault==4) bad.passes[1].model_intensity=1;
            check(!bad.valid(),"Malformed edge descriptor accepted");
        }
        edge_passes[1]={7,50,100,0};
        const auto mixed=calibrated_edge_guide(edge_passes,3);
        check(mixed.masks()[0]==0 && mixed.masks()[1]==edge.masks()[1],"Untracked WORLD average invalidated unrelated MODEL or admitted WORLD edge history");
        edge_passes[1].model_intensity=100;
        check(!calibrated_edge_guide(edge_passes,3).active(),"Edge descriptor survived an untracked warp");
    }
}
void game_ground_motion_identity_tests() {
    namespace vr=starfox::vr;
    constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    auto projection=identity;projection[10]=projection[11]=-1;projection[14]=-.01F;projection[15]=0;
    const vr::EyeCamera camera{identity,projection};
    starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
    auto floor=vr::landscape_sphere_packet(ppu,{},112,false,false,232);
    vr::place_landscape_ground(floor,-.75F,true);
    CalibratedGameFrame original;auto snapshot=std::make_shared<vr::GameSceneSnapshot>();
    snapshot->scene_epoch=17;snapshot->background_id=3;original.previous=original.current=snapshot;
    original.draws.push_back({floor,CalibratedGameLayer::world,vr::SceneBlend::opaque,false,calibrated_ground_source_key});
    vr::DrawPacket actor;vr::SceneVertex vertex{};vertex.position[2]=-2;vertex.color[3]=1;
    actor.geometry.vertices.assign(3,vertex);snapshot->transforms[1].generation=4;snapshot->transforms[1].shape=17;
    original.draws.push_back({actor,CalibratedGameLayer::model,vr::SceneBlend::opaque,true,1,true});
    const auto match=[&](const CalibratedGameFrame& now,const CalibratedGameFrame* before) {
        return calibrated_game_motion_draws(now,before,camera,camera);
    };
    auto retained=match(original,&original);
    check(retained.size()==2 && retained[0].valid && retained[0].previous_vertices.empty() && retained[1].valid,
        "Authored original finite floor lacks rigid correspondence or copied CPU vertices");
    check(!match(original,nullptr)[0].valid,"Missing accepted floor invented history");
    {
        auto lines=original;
        lines.draws[0].packet.geometry.line_vertices.assign(2,lines.draws[0].packet.geometry.vertex_view().front());
        const auto expanded=match(lines,&lines);
        check(expanded.size()==3 && !expanded[0].valid && !expanded[1].valid && expanded[1].previous_vertices.empty()
            && expanded[2].valid,"Malformed floor/line packet inherited ground/actor history or rejected an unrelated actor");
    }
    auto linear=original;
    linear.draws[0].packet=vr::landscape_sphere_packet(ppu,{},112,true,false,232);
    vr::place_landscape_ground(linear.draws[0].packet,-.75F,true);
    check(match(linear,&linear)[0].valid,"sRGB source finite floor lost authored correspondence");
    CalibratedGroundGradient linear_gradient{{.3F,.6F,.2F},{.05F,.2F,.03F},112,111};
    check(apply_calibrated_ground(linear.draws[0].packet,linear_gradient) && match(linear,&linear)[0].valid,
        "Enhanced sRGB source finite floor lost authored correspondence");
    for(unsigned material:{0U,8U}) {
        auto old=original;
        CalibratedGroundGradient gradient{{.3F,.6F,.2F},{.05F,.2F,.03F},112,111};
        gradient.material=material;gradient.origin={123,-415};gradient.seconds=1;
        check(apply_calibrated_ground(old.draws[0].packet,gradient),"Ground identity fixture metadata failed");
        constexpr float angle=.21F;const float c=std::cos(angle),s=std::sin(angle);
        old.draws[0].packet.model={c,s,0,0,-s,c,0,0,0,0,1,0,.05F,-.12F,.1F,1};
        auto now=old;auto& words=now.draws[0].packet.geometry.texels;
        words[calibrated_ground_offset-1]=std::bit_cast<std::uint32_t>(-1.125F);
        words[calibrated_ground_offset+9]=std::bit_cast<std::uint32_t>(187.F);
        words[calibrated_ground_offset+10]=std::bit_cast<std::uint32_t>(-287.F);
        words[calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(9.F);
        words[1]^=17; // Sky palette changes do not alter the enhanced floor colour.
        now.draws[0].packet.model[12]+=.15F;
        retained=match(now,&old);check(retained[0].valid && retained[1].valid,"Static finite ground rejected height/origin/unused-clock change");
        const auto& m=retained[0].previous_model;
        const double dx=material==8?.25:0,dy=.375,dz=material==8?-.5:0;
        check(std::abs(m[12]-(.05+c*dx-s*dy))<1.e-6 && std::abs(m[13]-(-.12+s*dx+c*dy))<1.e-6
            && std::abs(m[14]-(.1+dz))<1.e-6,"Ground accepted-local origin/height mapping differs from analytic rotation");
        for(unsigned fault=0;fault<18;++fault) {
            auto bad=now;auto state=std::make_shared<vr::GameSceneSnapshot>(*snapshot);bad.current=state;
            auto& p=bad.draws[0].packet;auto& data=p.geometry.texels;
            if(fault==0) bad.draws[0].source_key=0;
            if(fault==1) bad.draws.push_back(bad.draws[0]);
            if(fault==2) state->scene_epoch++;
            if(fault==3) state->background_id++;
            if(fault==4) bad.settings.history_epoch++;
            if(fault==5) bad.draws[0].after_rays=true;
            if(fault==6) bad.draws[0].blend=vr::SceneBlend::alpha;
            if(fault==7) p.preserve_native_colour=true;
            if(fault==8) data[15]&=~0x10000000U;
            if(fault==9) data[calibrated_ground_offset-1]=std::bit_cast<std::uint32_t>(NAN);
            if(fault==10) data[calibrated_ground_offset+9]=std::bit_cast<std::uint32_t>(INFINITY);
            if(fault==11) data[calibrated_ground_offset]=std::bit_cast<std::uint32_t>(.8F);
            if(fault==12) data[calibrated_ground_offset+13]=1;
            if(fault==13) {p.geometry.vertices.assign(p.geometry.vertex_view().begin(),p.geometry.vertex_view().end());p.geometry.shared_vertices.reset();p.geometry.vertices[0].position[0]+=.1F;}
            if(fault==14) data.pop_back();
            if(fault==15) p.model[0]=p.model[1]=0;
            if(fault==16) bad.draws[0].depth_test=true;
            if(fault==17) state->flow=starfox::simulation::GameFlowState::game_over;
            const auto rejected=match(bad,&old);
            check(!rejected[0].valid && rejected[1].valid,"Ambiguous/new/invalid/animated/changed floor inherited motion or reset a dry model");
        }
    }
    for(unsigned material:{5U,6U,7U,9U}) {
        auto liquid=original;CalibratedGroundGradient gradient{{.3F,.6F,.2F},{.05F,.2F,.03F},112,111};
        gradient.material=material;check(apply_calibrated_ground(liquid.draws[0].packet,gradient),"Liquid identity fixture failed");
        check(!match(liquid,&liquid)[0].valid && match(liquid,&liquid)[1].valid,
            "Fluid or reflected radiance borrowed finite-floor rigid history");
    }
    auto changed=original;changed.draws[0].packet.geometry.texels[1]^=17;
    check(!match(changed,&original)[0].valid,"Original atlas/palette change inherited stale floor colour history");
    std::swap(original.draws[0],original.draws[1]);
    check(match(original,&original)[1].valid,"Finite floor matched draw index rather than authored identity");
}
void ground_metadata_stability_tests() {
    starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
    auto raw=starfox::vr::landscape_sphere_packet(ppu,{},112,false,false,232);
    starfox::vr::place_landscape_ground(raw,-.75F,true);
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();
    for(unsigned material:{0U,5U,6U,7U,8U,9U}) for(unsigned motion=0;motion<4;++motion) {
        CalibratedGroundGradient gradient{{.3F,.6F,.2F},{.05F,.2F,.03F},112,111};
        gradient.material=material;gradient.motion=motion;gradient.origin={123,-415};gradient.seconds=1;
        auto first=raw;check(apply_calibrated_ground(first,gradient),"Ground metadata fixture failed");
        gradient.seconds=9;auto tick=raw;check(apply_calibrated_ground(tick,gradient),"Ground metadata tick fixture failed");
        const bool stationary=!motion && (material==0 || material==8);
        check(starfox::vr::same_draw_geometry(std::span(&first,1),std::span(&tick,1))==stationary,
            "Unused static ground clock changed upload bytes, or active terrain clock was discarded");
        const auto words=first.geometry.texel_view();
        check(std::bit_cast<float>(words[calibrated_ground_offset+11])==(stationary?0:1),"Ground producer retained an unused clock or removed an active one");
        check(std::bit_cast<float>(words[calibrated_ground_offset+9])==(!motion && material==0?0:123)
            && std::bit_cast<float>(words[calibrated_ground_offset+10])==(!motion && material==0?0:-415),
            "Ground producer discarded active world texture coordinates or retained unused gradient origins");
        CalibratedGameFrame a;a.current=a.previous=snapshot;a.settings.ground_motion=motion;
        a.draws.push_back({first,CalibratedGameLayer::world,starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        auto b=a;b.settings.effect_seconds=9;b.draws.front().packet=tick;
        check(same_calibrated_presentation(a,b)==stationary,"Fresh static terrain lost held identity or animated terrain was frozen");
        gradient.origin={187,-287};auto moved=raw;check(apply_calibrated_ground(moved,gradient),"Ground origin fixture failed");
        check(starfox::vr::same_draw_geometry(std::span(&tick,1),std::span(&moved,1))==(!motion && material==0),
            "Gradient upload retained unused origins or real world-texture movement was discarded");
    }
}
void game_motion_history_tests() {
    constexpr starfox::vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const std::array<starfox::vr::EyeCamera,2> cameras{{{identity,identity},{identity,identity}}};
    auto source=std::make_shared<CalibratedGameFrame>();
    source->previous=source->current=std::make_shared<starfox::vr::GameSceneSnapshot>();
    source->settings.motion_blur=2;source->presentation_seconds=100;
    CalibratedMotionHistory history;
    check(history.prepare(source,cameras) && history.interval()==0 && !history.accepted(),"First native blur invented presentation history");
    history.settle(true);
    auto moved=std::make_shared<CalibratedGameFrame>(*source);moved->alpha=.5;moved->presentation_seconds=100+1./120;
    check(history.prepare(moved,cameras,{.25F,-.125F}) && std::abs(history.interval()-1./120)<1.e-12
        && history.accepted()==source,"Native blur interval used source/FX ticks or accepted encoded work");
    const auto interval=history.interval();
    check(history.accepted()==source && history.interval()==interval,"A native image wait changed accepted shutter time");
    history.settle(true);
    check(history.accepted()==moved && history.jitter()==std::array<float,2>{.25F,-.125F},"Native shutter accepted a different eye/sample snapshot");
    auto held=std::make_shared<CalibratedGameFrame>(*moved);held->presentation_seconds+=1./240;
    check(history.prepare(held,cameras) && history.interval()==0,"Stationary held presentation incurred shutter work");
    history.settle(true);
    auto tracked=cameras;tracked[1].view[12]=.01F;
    auto head=std::make_shared<CalibratedGameFrame>(*held);head->presentation_seconds+=1./60;
    check(history.prepare(head,tracked) && std::abs(history.interval()-1./60)<1.e-12,"Tracked-eye motion was mistaken for a held source scene");
    history.settle(false);
    check(!history.accepted() && history.interval()==0,"Rejected eye pair retained candidate motion history");
    for(unsigned fault=0;fault<7;++fault) {
        check(history.prepare(source,cameras),"Native motion clock reset could not prepare");history.settle(true);
        auto bad=std::make_shared<CalibratedGameFrame>(*moved);
        if(fault==0) bad->presentation_seconds=100;
        if(fault==1) bad->presentation_seconds=99;
        if(fault==2) bad->presentation_seconds=101;
        if(fault==3) bad->settings.exposure_paused=true;
        if(fault==4) bad->settings.history_epoch++;
        if(fault==5) bad->settings.aa_quality++;
        if(fault==6) {auto state=std::make_shared<starfox::vr::GameSceneSnapshot>(*bad->current);state->scene_epoch++;bad->current=state;}
        check(history.prepare(bad,cameras) && history.interval()==0,"Cut/pause/settings/stale clock inherited native motion");
        history.settle(true);
        if(fault==3) check(!history.accepted(),"Paused frame seeded stale shutter history");
    }
    auto bad=std::make_shared<CalibratedGameFrame>(*source);bad->presentation_seconds=NAN;
    check(!history.prepare(bad,cameras) && !history.prepare(source,cameras,{INFINITY,0}),"Nonfinite native shutter clock/sample accepted");
    check(calibrated_msaa_extent_supported(1050,1050,1050,1050,8,true,3,true,true)
        && !calibrated_msaa_extent_supported(1050,1050,1050,1050,8,true,3,true,true,true),
        "Combined native blur/MSAA/fog omitted retained guides/shutter/underlay storage");
    check(calibrated_msaa_extent_supported(128,72,128,72,8,true,3,true,true,true,true),
        "Valid liquid-layer/MSAA/shutter working extent rejected");
    check(calibrated_msaa_extent_supported(970,970,970,970,8,true,3,true,true,true)
        && !calibrated_msaa_extent_supported(970,970,970,970,8,true,3,true,true,true,true),
        "Native liquid auxiliary planes omitted from complete retained working bound");
}
void recovery_tests() {
    DisplayXrRecovery retry;
    using namespace std::chrono;
    auto now=DisplayXrRecovery::Clock::time_point{}+seconds(100);
    check(!retry.due(now) && !retry.due(now+hours(24)) && !retry.retrying(),
        "Ordinary OFF launch schedules native runtime probes");
    retry.request(now);
    check(retry.due(now) && !retry.due(now-milliseconds(1)),"Explicit native request not due/clock regression probed early");
    for(unsigned delay:{2U,4U,8U,16U,30U,30U,30U}) {
        retry.failed(now);
        check(!retry.due(now+seconds(delay)-milliseconds(1)) && retry.due(now+seconds(delay)),
            "Native reconnect retry did not obey bounded backoff");
        now+=seconds(delay);
    }
    retry.connected();
    check(!retry.due(now+hours(24)) && !retry.retrying(),"Connected panel still schedules discovery");
    retry.failed(now);
    check(!retry.due(now+seconds(29)) && retry.due(now+seconds(30)),"Unhealthy attach erased reconnect backoff");
    retry.connected();retry.healthy();
    retry.failed(now);
    check(!retry.due(now+seconds(1)) && retry.due(now+seconds(2)),"Disconnect did not start a fresh recovery window");
    retry.stop();retry.failed(now);
    check(!retry.due(now+hours(24)) && !retry.retrying(),"Manual native OFF/renderer choice restarted SR");
    retry.request(now);
    check(retry.due(now),"User re-enable did not cancel the old retry deadline");
}
XrInstance current{};
const auto session_handle=reinterpret_cast<XrSession>(std::uintptr_t(0x1000));
const auto space_handle=reinterpret_cast<XrSpace>(std::uintptr_t(0x2000));
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
// Fixture-only native Vulkan allocations. GPU completion precedes teardown.
struct VulkanObject {
    VkDevice device{};PFN_vkGetDeviceProcAddr get{};
    VkImage image{};VkBuffer buffer{};VkDeviceMemory memory{};
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};VkEvent event{};
    ~VulkanObject() {
        if(!device || !get) return;
        if(fence) reinterpret_cast<PFN_vkDestroyFence>(get(device,"vkDestroyFence"))(device,fence,nullptr);
        if(event) reinterpret_cast<PFN_vkDestroyEvent>(get(device,"vkDestroyEvent"))(device,event,nullptr);
        if(pool) reinterpret_cast<PFN_vkDestroyCommandPool>(get(device,"vkDestroyCommandPool"))(device,pool,nullptr);
        if(image) reinterpret_cast<PFN_vkDestroyImage>(get(device,"vkDestroyImage"))(device,image,nullptr);
        if(buffer) reinterpret_cast<PFN_vkDestroyBuffer>(get(device,"vkDestroyBuffer"))(device,buffer,nullptr);
        if(memory) reinterpret_cast<PFN_vkFreeMemory>(get(device,"vkFreeMemory"))(device,memory,nullptr);
    }
};
struct GpuMock {
    struct Readback {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command;
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,2> buffers;
        std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT,2> footprints{};
        bool pending{};
    } readback;
    struct VulkanReadback {
        std::unique_ptr<VulkanObject> command;
        std::array<std::unique_ptr<VulkanObject>,2> buffers;
        bool pending{};
    } vk_readback;
    struct Eye {
        XrSwapchainCreateInfo info{};
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> images;
        std::vector<std::unique_ptr<VulkanObject>> vk_images;
        unsigned next{},index{},acquires{},waits{},releases{},submitted_index{};
        bool open{},acquired{},ready{};
    };
    std::array<Eye,2> eyes;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> retired_images;
    std::vector<std::unique_ptr<VulkanObject>> retired_vk_images;
    const StarfoxSdlVulkanBridgeV2* vk_native{};
    VkInstance vk_instance{};VkPhysicalDevice vk_physical{};VkDevice vk_device{};
    PFN_vkGetInstanceProcAddr vk_get{};VkQueue vk_queue{};unsigned vk_family{};
    unsigned vk_instance_creates{},vk_device_creates{};
    ID3D12Device* native{};ID3D12CommandQueue* queue{};
    std::int64_t format{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB};
    bool enabled{},timeout_right{},bad_index{},fail_release{},fail_end{},bad_image{},integrated{};
    int fail_create{-1},fail_images{-1};
    unsigned requirements{},swapchain_creates{},swapchain_destroys{},projections{};
} gpu_mock;
std::atomic<bool> gpu_blocked{false};
#include "displayxr_vulkan_fixture.inc"
void gpu_consume_projection() {
    if(mock.backend==DisplayXrBackend::vulkan) {vulkan_consume_projection();return;}
    auto& read=gpu_mock.readback;
    check(!read.pending,"Fixture compositor reused a pending readback");
    read={};
    check(SUCCEEDED(gpu_mock.native->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_ID3D12CommandAllocator,
        reinterpret_cast<void**>(read.allocator.GetAddressOf()))),"Fixture compositor allocator failed");
    check(SUCCEEDED(gpu_mock.native->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,read.allocator.Get(),nullptr,
        IID_ID3D12GraphicsCommandList,reinterpret_cast<void**>(read.command.GetAddressOf()))),"Fixture compositor list failed");
    for(unsigned i=0;i<2;++i) {
        const auto& eye=gpu_mock.eyes[i];auto* texture=eye.images[eye.submitted_index].Get();
        D3D12_RESOURCE_DESC desc{};
#if defined(__MINGW32__)
        texture->GetDesc(&desc);
#else
        desc=texture->GetDesc();
#endif
        UINT64 bytes{};gpu_mock.native->GetCopyableFootprints(&desc,0,1,0,&read.footprints[i],nullptr,nullptr,&bytes);
        D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;
        buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
        check(SUCCEEDED(gpu_mock.native->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,IID_ID3D12Resource,reinterpret_cast<void**>(read.buffers[i].GetAddressOf()))),"Fixture compositor readback failed");
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource=texture;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
        read.command->ResourceBarrier(1,&barrier);
        D3D12_TEXTURE_COPY_LOCATION source{},target{};source.pResource=texture;source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        target.pResource=read.buffers[i].Get();target.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;target.PlacedFootprint=read.footprints[i];
        read.command->CopyTextureRegion(&target,0,0,0,&source,nullptr);
        std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);read.command->ResourceBarrier(1,&barrier);
    }
    check(SUCCEEDED(read.command->Close()),"Fixture compositor list close failed");
    ID3D12CommandList* lists[]{read.command.Get()};gpu_mock.queue->ExecuteCommandLists(1,lists);read.pending=true;
}
unsigned gpu_eye(XrSwapchain swapchain) {
    const auto value=reinterpret_cast<std::uintptr_t>(swapchain);
    check(value==0x4000 || value==0x4001,"Unexpected runtime swapchain");return unsigned(value-0x4000);
}
XrResult XRAPI_CALL gpu_requirements(XrInstance,XrSystemId,XrGraphicsRequirementsD3D12KHR* out) {
    ++gpu_mock.requirements;
#if defined(__MINGW32__)
    gpu_mock.native->GetAdapterLuid(&out->adapterLuid);
#else
    out->adapterLuid=gpu_mock.native->GetAdapterLuid();
#endif
    out->minFeatureLevel=D3D_FEATURE_LEVEL_11_0;return XR_SUCCESS;
}
XrResult XRAPI_CALL gpu_formats(XrSession,unsigned capacity,unsigned* count,std::int64_t* formats) {
    *count=1;if(capacity) formats[0]=gpu_mock.format;return XR_SUCCESS;
}
XrResult XRAPI_CALL gpu_create_swapchain(XrSession session,const XrSwapchainCreateInfo* info,XrSwapchain* out) {
    check(session==session_handle && info && info->type==XR_TYPE_SWAPCHAIN_CREATE_INFO && !info->next
        && !info->createFlags && info->sampleCount==1 && info->arraySize==1 && info->mipCount==1 && info->faceCount==1
        && info->usageFlags==(XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT),
        "Native eye swapchain lacks required transfer usage");
    const unsigned eye=gpu_mock.eyes[0].open?1:0;
    if(int(eye)==gpu_mock.fail_create) return XR_ERROR_RUNTIME_FAILURE;
    auto& target=gpu_mock.eyes[eye];check(!target.open,"Swapchain replaced while live");
    target.info=*info;
    if(mock.backend==DisplayXrBackend::vulkan) {
        for(unsigned i=0;i<3;++i) target.vk_images.push_back(vulkan_image(info->width,info->height,static_cast<VkFormat>(info->format)));
        vulkan_prepare_images(target.vk_images);
        target.open=true;++gpu_mock.swapchain_creates;
        *out=reinterpret_cast<XrSwapchain>(std::uintptr_t(0x4000+eye));return XR_SUCCESS;
    }
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=info->width;desc.Height=info->height;desc.DepthOrArraySize=desc.MipLevels=1;
    desc.Format=static_cast<DXGI_FORMAT>(info->format);desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if(gpu_mock.bad_image) ++desc.Width;
    target.images.resize(3);
    for(auto& image:target.images) check(SUCCEEDED(gpu_mock.native->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_ID3D12Resource,reinterpret_cast<void**>(image.GetAddressOf()))),"Fixture native image allocation failed");
    target.open=true;++gpu_mock.swapchain_creates;
    *out=reinterpret_cast<XrSwapchain>(std::uintptr_t(0x4000+eye));return XR_SUCCESS;
}
XrResult XRAPI_CALL gpu_destroy_swapchain(XrSwapchain swapchain) {
    auto& eye=gpu_mock.eyes[gpu_eye(swapchain)];
    check(eye.open && !gpu_blocked,"Runtime image destroyed while GPU work pending");
    if(gpu_mock.readback.pending) for(auto& image:eye.images) gpu_mock.retired_images.push_back(std::move(image));
    if(gpu_mock.vk_readback.pending) for(auto& image:eye.vk_images) gpu_mock.retired_vk_images.push_back(std::move(image));
    eye.vk_images.clear();
    eye.images.clear();eye.open=eye.acquired=eye.ready=false;++gpu_mock.swapchain_destroys;return XR_SUCCESS;
}
XrResult XRAPI_CALL gpu_images(XrSwapchain swapchain,unsigned capacity,unsigned* count,XrSwapchainImageBaseHeader* out) {
    const auto index=gpu_eye(swapchain);auto& eye=gpu_mock.eyes[index];check(eye.open,"Enumerating a dead swapchain");
    *count=unsigned(mock.backend==DisplayXrBackend::vulkan?eye.vk_images.size():eye.images.size());if(!capacity) return XR_SUCCESS;
    if(int(index)==gpu_mock.fail_images) return XR_ERROR_RUNTIME_FAILURE;
    check(capacity==*count,"Unexpected native image capacity");
    if(mock.backend==DisplayXrBackend::vulkan) {
        auto* images=reinterpret_cast<XrSwapchainImageVulkan2KHR*>(out);
        for(unsigned i=0;i<capacity;++i) {
            check(images[i].type==XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR && !images[i].next,"Uninitialized Vulkan eye image");
            images[i].image=gpu_mock.bad_image?VK_NULL_HANDLE:eye.vk_images[i]->image;
        }
        return XR_SUCCESS;
    }
    auto* images=reinterpret_cast<XrSwapchainImageD3D12KHR*>(out);
    for(unsigned i=0;i<capacity;++i) {
        check(images[i].type==XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR && !images[i].next,"Uninitialized graphics-specific eye image");
        images[i].texture=eye.images[i].Get();
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL gpu_acquire(XrSwapchain swapchain,const XrSwapchainImageAcquireInfo*,unsigned* index) {
    auto& eye=gpu_mock.eyes[gpu_eye(swapchain)];check(eye.open && !eye.acquired,"Eye acquired twice without release");
    ++eye.acquires;eye.index=(eye.next++*2)%unsigned(mock.backend==DisplayXrBackend::vulkan?eye.vk_images.size():eye.images.size());eye.acquired=true;eye.ready=false;
    *index=gpu_mock.bad_index?99:eye.index;return XR_SUCCESS;
}
XrResult XRAPI_CALL gpu_wait(XrSwapchain swapchain,const XrSwapchainImageWaitInfo* info) {
    const auto index=gpu_eye(swapchain);auto& eye=gpu_mock.eyes[index];
    check(eye.acquired && !eye.ready && info->timeout>=0 && info->timeout<=100000000,"Bad image wait/retry");++eye.waits;
    if(index==1 && gpu_mock.timeout_right) return XR_TIMEOUT_EXPIRED;
    eye.ready=true;return XR_SUCCESS;
}
XrResult XRAPI_CALL gpu_release(XrSwapchain swapchain,const XrSwapchainImageReleaseInfo*) {
    auto& eye=gpu_mock.eyes[gpu_eye(swapchain)];
    check(eye.acquired && eye.ready && !gpu_blocked,"Eye released before wait/GPU completion");
    if(gpu_mock.fail_release) {gpu_mock.fail_release=false;return XR_ERROR_RUNTIME_FAILURE;}
    eye.acquired=eye.ready=false;eye.submitted_index=eye.index;++eye.releases;return XR_SUCCESS;
}
#endif
XrResult XRAPI_CALL enumerate(const char* layer,std::uint32_t capacity,std::uint32_t* count,XrExtensionProperties* out) {
    check(!layer,"Unexpected API layer");
    *count=mock.extension_count_override?mock.extension_count_override:unsigned(mock.extensions.size());
    if(capacity) {
        check(capacity>=mock.extensions.size(),"Extension buffer too small");
        std::copy(mock.extensions.begin(),mock.extensions.end(),out);
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL create(const XrInstanceCreateInfo* info,XrInstance* out) {
    check(info && info->type==XR_TYPE_INSTANCE_CREATE_INFO && !info->next,"Invalid instance info");
    check(info->enabledApiLayerCount==0 && info->enabledExtensionCount==4,"Unexpected layers or enabled extensions");
    check(std::string_view(info->enabledExtensionNames[0])==displayxr::display_info_extension
        && std::string_view(info->enabledExtensionNames[1])==displayxr::view_rig_extension
        && std::string_view(info->enabledExtensionNames[2])==displayxr::window_extension,"Missing native display interfaces");
    check(std::string_view(info->enabledExtensionNames[3])==(mock.backend==DisplayXrBackend::direct3d12
        ?"XR_KHR_D3D12_enable":"XR_KHR_vulkan_enable2"),"Wrong graphics API enabled");
    if(XR_FAILED(mock.create_result)) return mock.create_result;
    check(current==XR_NULL_HANDLE,"Leaked previous instance");
    current=reinterpret_cast<XrInstance>(std::uintptr_t(++creates));*out=current;return XR_SUCCESS;
}
XrResult XRAPI_CALL destroy(XrInstance instance) {
    check(instance==current && instance!=XR_NULL_HANDLE,"Destroying an invalid instance");
    if(XR_FAILED(mock.destroy_result)) return mock.destroy_result;
    ++destroys;current=XR_NULL_HANDLE;return XR_SUCCESS;
}
XrResult XRAPI_CALL properties(XrInstance instance,XrInstanceProperties* out) {
    check(instance==current && out->type==XR_TYPE_INSTANCE_PROPERTIES,"Bad instance property query");
    std::strcpy(out->runtimeName,mock.runtime_name);out->runtimeVersion=XR_MAKE_VERSION(2,21,11);return XR_SUCCESS;
}
XrResult XRAPI_CALL system(XrInstance instance,const XrSystemGetInfo* info,XrSystemId* out) {
    check(instance==current && info->formFactor==XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,"Bad form factor/instance");
    if(XR_FAILED(mock.system_result)) return mock.system_result;
    *out=42;return XR_SUCCESS;
}
XrResult XRAPI_CALL display_properties(XrInstance instance,XrSystemId id,XrSystemProperties* out) {
    check(instance==current && id==42,"Wrong display system");
    if(XR_FAILED(mock.properties_result)) return mock.properties_result;
    out->systemId=mock.wrong_system_id?99:id;
    std::strcpy(out->systemName,mock.system_name);
    auto* physical=static_cast<displayxr::DisplayInfo*>(out->next);
    check(physical && physical->type==displayxr::display_info_type,"Wrong physical-display ABI");
    auto* desktop=static_cast<displayxr::DesktopInfo*>(physical->next);
    check(desktop && desktop->type==displayxr::desktop_info_type && !desktop->next,"Wrong desktop-display ABI");
    physical->pixel_width=3840;physical->pixel_height=2160;
    physical->display_size_meters={mock.physical_width,.34F};physical->nominal_viewer={0,.03F,mock.viewer_z};
    physical->view_scale_x=mock.scale;physical->view_scale_y=1;
    desktop->rect={{-3840,-120},{3840,2160}};desktop->primary=XR_FALSE;
    desktop->panel_confirmed=mock.panel_confirmed?XR_TRUE:XR_FALSE;
    if(!mock.zero_monitor) std::strcpy(desktop->device_name,"\\\\.\\DISPLAY2");
    if(mock.unterminated) std::fill(std::begin(desktop->device_name),std::end(desktop->device_name),'x');
    return XR_SUCCESS;
}
XrResult XRAPI_CALL views(XrInstance instance,XrSystemId id,XrViewConfigurationType type,
    std::uint32_t capacity,std::uint32_t* count,XrViewConfigurationView* out) {
    check(instance==current && id==42 && type==XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,"Invalid view query");
    *count=capacity?mock.read_eyes:mock.query_eyes;
    for(unsigned i=0;i<capacity;++i) {
        check(out[i].type==XR_TYPE_VIEW_CONFIGURATION_VIEW,"Missing initialized view type");
        out[i].recommendedImageRectWidth=mock.image_width;out[i].recommendedImageRectHeight=mock.image_height;
        out[i].maxImageRectWidth=4096;out[i].maxImageRectHeight=4096;
        out[i].recommendedSwapchainSampleCount=mock.bad_sample?8:1;out[i].maxSwapchainSampleCount=4;
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL create_session(XrInstance instance,const XrSessionCreateInfo* info,XrSession* out) {
    check(instance==current && info->systemId==42,"Bad native session system");
    const auto* window=static_cast<const displayxr::WindowBinding*>(info->next);
    check(window && window->type==displayxr::window_binding_type && window->window
        && !window->transparent && !window->shared_texture && !window->readback,"Bad panel window binding");
    const auto* graphics=static_cast<const XrBaseInStructure*>(window->next);
    check(graphics && graphics->type==(mock.backend==DisplayXrBackend::direct3d12
        ?XR_TYPE_GRAPHICS_BINDING_D3D12_KHR:XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR),"Graphics binding lost from chain");
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
    if(gpu_mock.enabled) {
        if(mock.backend==DisplayXrBackend::vulkan) {
            const auto* binding=reinterpret_cast<const XrGraphicsBindingVulkan2KHR*>(graphics);
            check(binding->instance==gpu_mock.vk_instance && binding->physicalDevice==gpu_mock.vk_physical && binding->device==gpu_mock.vk_device
                && binding->queueFamilyIndex==gpu_mock.vk_family && !binding->queueIndex && !binding->next
                && gpu_mock.requirements && gpu_mock.vk_instance_creates>=2 && gpu_mock.vk_device_creates==1,
                "Vulkan presenter skipped creation requirements or selected a foreign device/queue");
        } else {
        const auto* binding=reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(graphics);
        check(binding->device==gpu_mock.native && binding->queue==gpu_mock.queue && !binding->next && gpu_mock.requirements>0,
            "Presenter created a second GPU device/queue or skipped requirements");
        }
    }
#endif
    if(XR_FAILED(native_mock.create_result)) return native_mock.create_result;
    check(!native_mock.session_open,"Leaked graphics session");
    native_mock.session_open=true;native_mock.ready=true;++native_mock.session_creates;*out=session_handle;return XR_SUCCESS;
}
XrResult XRAPI_CALL destroy_session(XrSession session) {
    check(session==session_handle && native_mock.session_open && !native_mock.space_open
        && !native_mock.frame_open,"Session destroyed before dependent resources/frame");
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
    if(gpu_mock.enabled) check(!gpu_mock.eyes[0].open && !gpu_mock.eyes[1].open,"Session destroyed before native eye images");
#endif
    native_mock.session_open=false;++native_mock.session_destroys;return XR_SUCCESS;
}
XrResult XRAPI_CALL create_space(XrSession session,const XrReferenceSpaceCreateInfo* info,XrSpace* out) {
    check(session==session_handle && info->referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL
        && info->poseInReferenceSpace.orientation.w==1,"Wrong native camera space");
    if(XR_FAILED(native_mock.space_result)) return native_mock.space_result;
    native_mock.space_open=true;++native_mock.space_creates;*out=space_handle;return XR_SUCCESS;
}
XrResult XRAPI_CALL destroy_space(XrSpace space) {
    check(space==space_handle && native_mock.space_open,"Bad space cleanup");
    native_mock.space_open=false;++native_mock.space_destroys;return XR_SUCCESS;
}
XrResult XRAPI_CALL blend_modes(XrInstance instance,XrSystemId id,XrViewConfigurationType,
    unsigned capacity,unsigned* count,XrEnvironmentBlendMode* modes) {
    check(instance==current && id==42,"Bad blend query");*count=1;
    if(capacity) modes[0]=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;return XR_SUCCESS;
}
XrResult XRAPI_CALL poll(XrInstance instance,XrEventDataBuffer* buffer) {
    check(instance==current && buffer->type==XR_TYPE_EVENT_DATA_BUFFER,"Bad event query");
    if(native_mock.loss) {native_mock.loss=false;buffer->type=XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING;return XR_SUCCESS;}
    if(native_mock.ready) {
        native_mock.ready=false;
        auto* event=reinterpret_cast<XrEventDataSessionStateChanged*>(buffer);
        *event={XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};event->session=session_handle;event->state=XR_SESSION_STATE_READY;
        return XR_SUCCESS;
    }
    return XR_EVENT_UNAVAILABLE;
}
XrResult XRAPI_CALL begin_session(XrSession session,const XrSessionBeginInfo* info) {
    check(session==session_handle && info->primaryViewConfigurationType==XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,"Wrong native session type");
    return XR_SUCCESS;
}
XrResult XRAPI_CALL end_session(XrSession session) {check(session==session_handle,"Bad session end");return XR_SUCCESS;}
XrResult XRAPI_CALL wait_frame(XrSession session,const XrFrameWaitInfo*,XrFrameState* out) {
    check(session==session_handle && !native_mock.frame_open,"Waiting during an active frame");
    ++native_mock.waits;out->predictedDisplayTime=native_mock.waits*10000000;
    out->predictedDisplayPeriod=10000000;out->shouldRender=native_mock.visible?XR_TRUE:XR_FALSE;return XR_SUCCESS;
}
XrResult XRAPI_CALL begin_frame(XrSession session,const XrFrameBeginInfo*) {
    check(session==session_handle && !native_mock.frame_open,"Overlapping native frames");
    native_mock.frame_open=true;++native_mock.begins;return XR_SUCCESS;
}
XrResult XRAPI_CALL end_frame(XrSession session,const XrFrameEndInfo* info) {
    check(session==session_handle && native_mock.frame_open && info->displayTime==native_mock.waits*10000000,"Bad native frame end");
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
    if(gpu_mock.enabled && info->layerCount) {
        check(info->layerCount==1 && info->layers && !gpu_blocked,"Half/incomplete native layer submitted");
        const auto* layer=reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[0]);
        check(layer && layer->type==XR_TYPE_COMPOSITION_LAYER_PROJECTION && !layer->next && layer->viewCount==2
            && layer->space==space_handle && layer->views,"Invalid native projection layer");
        for(unsigned eye=0;eye<2;++eye) {
            const auto& view=layer->views[eye];const auto& target=gpu_mock.eyes[eye];
            check(view.type==XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW && !view.next
                && view.subImage.swapchain==reinterpret_cast<XrSwapchain>(std::uintptr_t(0x4000+eye))
                && !view.subImage.imageArrayIndex && !view.subImage.imageRect.offset.x && !view.subImage.imageRect.offset.y
                && view.subImage.imageRect.extent.width==int(target.info.width) && view.subImage.imageRect.extent.height==int(target.info.height)
                && !target.acquired && view.pose.position.x==native_mock.located_views[eye].pose.position.x
                && view.fov.angleLeft==native_mock.located_views[eye].fov.angleLeft
                && view.fov.angleUp==native_mock.located_views[eye].fov.angleUp,"Native projection altered calibrated poses/FOV or lost an eye");
        }
        ++gpu_mock.projections;
        gpu_consume_projection();
    }
#endif
    native_mock.last_layers=info->layerCount;native_mock.frame_open=false;++native_mock.ends;
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
    if(gpu_mock.enabled && gpu_mock.fail_end) {gpu_mock.fail_end=false;return XR_ERROR_RUNTIME_FAILURE;}
#endif
    return XR_SUCCESS;
}
XrResult XRAPI_CALL locate(XrSession session,const XrViewLocateInfo* info,XrViewState* state,
    unsigned capacity,unsigned* count,XrView* eyes) {
    check(session==session_handle && native_mock.frame_open && info->space==space_handle && capacity==2,"Bad calibrated locate");
    const auto* rig=static_cast<const displayxr::CameraRig*>(info->next);
    const auto& expected=native_mock.expected;
    check(rig && rig->type==displayxr::camera_rig_type && !rig->next
        && rig->meters_to_virtual==expected.meters_to_virtual && rig->ipd_factor==expected.ipd_factor
        && rig->parallax_factor==expected.parallax_factor && rig->vertical_fov==expected.vertical_fov
        && rig->convergence_diopters==1/expected.convergence && rig->pose.position.x==expected.pose.position.x,
        "Missing/per-frame camera rig or double world scaling");
    ++native_mock.locates;
    if(XR_FAILED(native_mock.locate_result)) return native_mock.locate_result;
    state->viewStateFlags=native_mock.valid_tracking?XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT:0;
    auto* raw=static_cast<displayxr::RawViews*>(state->next);
    check(raw && raw->type==displayxr::raw_views_type && !raw->next,"Missing physical view result chain");
    raw->eye_count=native_mock.bad_raw?1:2;raw->canvas_rect={{128,96},{3200,1800}};raw->canvas_size={.5F,.283F};
    raw->display_plane.orientation.w=1;
    raw->eyes[0]={-.032F,.03F,.65F};raw->eyes[1]={.032F,.03F,.65F};
    if(native_mock.bad_physical) raw->eyes[0].z=std::numeric_limits<float>::infinity();
    raw->tracking=native_mock.tracking?XR_TRUE:XR_FALSE;raw->sample_time_ns=info->displayTime;
    *count=2;
    for(unsigned eye=0;eye<2;++eye) {
        check(eyes[eye].type==XR_TYPE_VIEW,"Uninitialized eye output");
        eyes[eye].pose={{0,std::sin(native_mock.head_yaw*.5F),0,std::cos(native_mock.head_yaw*.5F)},
            {(native_mock.head_x+(eye?8.F:-8.F))*native_mock.pose_scale,
            2*native_mock.pose_scale,4*native_mock.pose_scale}};
        eyes[eye].fov=fixture_fov();
        if(native_mock.bad_pose) eyes[eye].pose.position.x=std::numeric_limits<float>::quiet_NaN();
        native_mock.located_views[eye]=eyes[eye];
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL enumerate_modes(XrSession session,unsigned capacity,unsigned* count,displayxr::RenderingMode* out) {
    check(session==session_handle && native_mock.session_open && capacity>=1,"Bad native mode query");
    *count=native_mock.mode_count_override?native_mock.mode_count_override:1;
    check(out[0].type==displayxr::rendering_mode_type && !out[0].next,"Incorrect rendering-mode ABI");
    out[0].index=7;std::strcpy(out[0].name,"LeiaSR");out[0].eye_count=native_mock.eyes;
    out[0].scale_x=.5F;out[0].scale_y=1;out[0].hardware_3d=native_mock.hardware?XR_TRUE:XR_FALSE;
    out[0].columns=native_mock.columns;out[0].rows=1;out[0].view_width=mock.image_width;out[0].view_height=mock.image_height;
    out[0].active=native_mock.active?XR_TRUE:XR_FALSE;out[0].requestable=native_mock.requestable?XR_TRUE:XR_FALSE;
    return XR_SUCCESS;
}
XrResult XRAPI_CALL request_mode(XrSession session,unsigned index) {
    check(session==session_handle && index==7 && native_mock.requestable,"Wrong or unauthorized display-mode request");
    ++native_mock.requests;if(!native_mock.ignore_request) native_mock.active=true;return XR_SUCCESS;
}
XrResult XRAPI_CALL get(XrInstance instance,const char* name,PFN_xrVoidFunction* out) {
    *out=nullptr;
    if(mock.missing && std::string_view(name)==mock.missing) return XR_ERROR_FUNCTION_UNSUPPORTED;
    if(std::string_view(name)!="xrEnumerateInstanceExtensionProperties" && std::string_view(name)!="xrCreateInstance")
        check(instance==current && current!=XR_NULL_HANDLE,"Using a stale dispatch instance");
#define ENTRY(n,fn) if(std::string_view(name)==n) {*out=reinterpret_cast<PFN_xrVoidFunction>(fn);return XR_SUCCESS;}
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
    if(gpu_mock.enabled) {
        ENTRY("xrGetVulkanGraphicsRequirements2KHR",vulkan_requirements)
        ENTRY("xrCreateVulkanInstanceKHR",vulkan_create_instance)
        ENTRY("xrGetVulkanGraphicsDevice2KHR",vulkan_get_physical)
        ENTRY("xrCreateVulkanDeviceKHR",vulkan_create_device)
        ENTRY("xrGetD3D12GraphicsRequirementsKHR",gpu_requirements)
        ENTRY("xrEnumerateSwapchainFormats",gpu_formats)
        ENTRY("xrCreateSwapchain",gpu_create_swapchain)
        ENTRY("xrDestroySwapchain",gpu_destroy_swapchain)
        ENTRY("xrEnumerateSwapchainImages",gpu_images)
        ENTRY("xrAcquireSwapchainImage",gpu_acquire)
        ENTRY("xrWaitSwapchainImage",gpu_wait)
        ENTRY("xrReleaseSwapchainImage",gpu_release)
    }
#endif
    ENTRY("xrEnumerateInstanceExtensionProperties",enumerate)
    ENTRY("xrCreateInstance",create)
    ENTRY("xrDestroyInstance",destroy)
    ENTRY("xrGetInstanceProperties",properties)
    ENTRY("xrGetSystem",system)
    ENTRY("xrGetSystemProperties",display_properties)
    ENTRY("xrEnumerateViewConfigurationViews",views)
    ENTRY("xrCreateSession",create_session)
    ENTRY("xrDestroySession",destroy_session)
    ENTRY("xrCreateReferenceSpace",create_space)
    ENTRY("xrDestroySpace",destroy_space)
    ENTRY("xrEnumerateEnvironmentBlendModes",blend_modes)
    ENTRY("xrPollEvent",poll)
    ENTRY("xrBeginSession",begin_session)
    ENTRY("xrEndSession",end_session)
    ENTRY("xrWaitFrame",wait_frame)
    ENTRY("xrBeginFrame",begin_frame)
    ENTRY("xrEndFrame",end_frame)
    ENTRY("xrLocateViews",locate)
    ENTRY("xrEnumerateDisplayRenderingModesDXR",enumerate_modes)
    ENTRY("xrRequestDisplayRenderingModeDXR",request_mode)
#undef ENTRY
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
void empty(const DisplayXrRuntime& runtime) {
    check(!runtime.detected() && runtime.instance()==XR_NULL_HANDLE && runtime.system()==XR_NULL_SYSTEM_ID,"Failed probe retained hardware");
    check(runtime.get_instance_proc()==nullptr && runtime.panel().pixel_width==0
        && runtime.panel().device_name.empty() && runtime.views()[0].recommendedImageRectWidth==0,"Failed probe retained properties");
    check(current==XR_NULL_HANDLE && creates==destroys,"Failed probe leaked an instance");
    check(runtime.status().starts_with("Leia SR unavailable:"),"Missing actionable unavailable status");
}
void native_tests(DisplayXrRuntime& runtime) {
    for(auto backend:{DisplayXrBackend::direct3d12,DisplayXrBackend::vulkan}) {
        runtime.close();mock=Mock{};mock.backend=backend;native_mock=NativeMock{};
        check(runtime.initialize_with_api(get,backend),"Native runtime fixture failed");
        DisplayXrSession session;
        XrBaseInStructure graphics{backend==DisplayXrBackend::direct3d12
            ?XR_TYPE_GRAPHICS_BINDING_D3D12_KHR:XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        auto* window=reinterpret_cast<void*>(std::uintptr_t(0x3000));
        check(session.initialize(runtime,&graphics,window),"Calibrated session rejected");
        check(native_mock.requests==1 && !session.running(),"Native mode not requested or READY bypassed");
        check(!session.begin_frame(native_mock.expected) && native_mock.waits==0,"Frame begun before READY");
        check(session.poll_events() && session.running(),"READY did not begin stereo session");
        auto frame=session.begin_frame(native_mock.expected);
        check(frame && frame->xr.should_render && frame->physical.tracking==XR_TRUE,"Calibrated frame not located");
        check(frame->cameras[0].view[12]==-4 && frame->cameras[1].view[12]==-20
            && frame->cameras[0].view[13]==-2 && frame->cameras[0].view[14]==-4,
            "Head position re-anchored, eye separation collapsed or world scaling doubled");
        check(frame->cameras[0].projection[8]!=0 && frame->cameras[0].projection[9]!=0,
            "Asymmetric calibrated FOV replaced with fixed SBS projection");
        check(frame->physical.canvas_rect.offset.x==128 && frame->physical.canvas_rect.offset.y==96,
            "Native panel canvas position lost");
        XrCompositionLayerBaseHeader layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        const std::array<const XrCompositionLayerBaseHeader*,1> layers{&layer};
        check(session.end_frame(layers) && native_mock.last_layers==1,"Valid projection layer suppressed");
        native_mock.head_x=44;native_mock.tracking=false;native_mock.expected.convergence=2048;
        native_mock.expected.pose.position.x=5;native_mock.expected.ipd_factor=2;native_mock.expected.parallax_factor=.5F;
        frame=session.begin_frame(native_mock.expected);
        check(frame && frame->xr.should_render && frame->physical.tracking==XR_FALSE
            && frame->cameras[0].view[12]==-36 && frame->cameras[1].view[12]==-52,
            "Nominal tracking fallback rejected or subsequent head motion canceled");
        check(session.end_frame(layers),"Second calibrated frame did not finish");
        check(session.begin_frame(native_mock.expected).has_value(),"Native mode-loss fixture failed");
        native_mock.active=false;
        check(!session.end_frame(layers) && native_mock.last_layers==0 && !native_mock.frame_open,
            "Native mode lost mid-frame still submitted a visible layer");
        native_mock.active=true;
        auto invalid=native_mock.expected;invalid.meters_to_virtual=0;
        const auto waits=native_mock.waits;
        check(!session.begin_frame(invalid) && native_mock.waits==waits,"Invalid rig reached runtime");
        for(unsigned failure=0;failure<5;++failure) {
            native_mock.visible=failure!=0;native_mock.valid_tracking=failure!=1;
            native_mock.bad_pose=failure==2;native_mock.bad_raw=failure==3;
            native_mock.bad_physical=failure==4;
            frame=session.begin_frame(native_mock.expected);
            check(frame && !frame->xr.should_render,"Invalid/invisible native frame rendered");
            check(session.end_frame(layers) && native_mock.last_layers==0,"Invalid frame submitted a visible layer");
        }
        native_mock.visible=true;native_mock.valid_tracking=true;native_mock.bad_pose=false;native_mock.bad_raw=false;
        native_mock.bad_physical=false;
        native_mock.locate_result=XR_ERROR_RUNTIME_FAILURE;
        check(!session.begin_frame(native_mock.expected) && !native_mock.frame_open,"Locate failure left an active frame");
        native_mock.locate_result=XR_SUCCESS;native_mock.active=false;
        check(!session.begin_frame(native_mock.expected) && !native_mock.frame_open,"Hardware-mode loss silently rendered fixed SBS");
        native_mock.active=true;
        check(session.begin_frame(native_mock.expected).has_value(),"Native session failed to recover");
        session.close();
        check(!native_mock.frame_open && native_mock.ends==native_mock.begins
            && native_mock.space_creates==native_mock.space_destroys
            && native_mock.session_creates==native_mock.session_destroys,"Active-frame close leaked native resources");
        for(unsigned failure=0;failure<8;++failure) {
            native_mock=NativeMock{};
            if(failure==0) native_mock.hardware=false;
            if(failure==1) native_mock.columns=1;
            if(failure==2) native_mock.eyes=1;
            if(failure==3) native_mock.requestable=false;
            if(failure==4) native_mock.ignore_request=true;
            if(failure==5) native_mock.mode_count_override=65;
            if(failure==6) native_mock.create_result=XR_ERROR_GRAPHICS_DEVICE_INVALID;
            if(failure==7) native_mock.space_result=XR_ERROR_RUNTIME_FAILURE;
            check(!session.initialize(runtime,&graphics,window),"Incompatible native graphics/mode accepted");
            check(session.handle()==XR_NULL_HANDLE && !native_mock.session_open && !native_mock.space_open
                && native_mock.session_creates==native_mock.session_destroys
                && native_mock.space_creates==native_mock.space_destroys,"Initialization failure leaked native session");
        }
        native_mock=NativeMock{};mock.missing="xrEndFrame";
        check(!session.initialize(runtime,&graphics,window) && native_mock.session_creates==0,"Missing frame entry point accepted");
        mock.missing=nullptr;
        auto wrong=graphics;wrong.type=backend==DisplayXrBackend::direct3d12
            ?XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR:XR_TYPE_GRAPHICS_BINDING_D3D12_KHR;
        check(!session.initialize(runtime,&wrong,window) && native_mock.session_creates==0,"Wrong graphics backend accepted");
        native_mock.active=true;native_mock.requestable=false;
        check(session.initialize(runtime,&graphics,window) && native_mock.requests==0,"Existing native mode needlessly re-requested");
        check(session.poll_events(),"Recovering session could not poll");
        native_mock.loss=true;
        check(session.poll_events() && session.exit_requested() && !session.begin_frame(native_mock.expected),"Display/instance loss ignored");
        session.close();runtime.close();empty(runtime);
        check(native_mock.session_creates==native_mock.session_destroys && native_mock.space_creates==native_mock.space_destroys,
            "Recovered session leaked resources");
    }
}
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
std::vector<Microsoft::WRL::ComPtr<ID3D12InfoQueue>> native_validation_queues;
void capture_native_d3d12_errors(ID3D12Device* native) {
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> queue;
    check(SUCCEEDED(native->QueryInterface(IID_ID3D12InfoQueue,reinterpret_cast<void**>(queue.GetAddressOf()))),
        "D3D12 debug messages unavailable");
    check(queue->GetNumMessagesDiscardedByMessageCountLimit()==0,"Native presenter validation overflowed before capture began");
    D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
    D3D12_MESSAGE_SEVERITY noncritical[]{D3D12_MESSAGE_SEVERITY_WARNING,D3D12_MESSAGE_SEVERITY_INFO,D3D12_MESSAGE_SEVERITY_MESSAGE};
    D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
    filter.DenyList.NumSeverities=3;filter.DenyList.pSeverityList=noncritical;
    check(SUCCEEDED(queue->AddStorageFilterEntries(&filter)),"Native presenter critical-message capture failed");
    // PRIVATE diagnostic device only. Never clear stored messages, deny error
    // IDs or enlarge its bounded queue. Retain the native device/queue through
    // SDL-owner destruction AND actual SDK teardown, not only image readback.
    if(std::none_of(native_validation_queues.begin(),native_validation_queues.end(),
            [&](const auto& held){return held.Get()==queue.Get();}))
        native_validation_queues.push_back(std::move(queue));
}
void validate_native_d3d12_errors(const char* stage) {
    for(const auto& queue:native_validation_queues) {
        const auto discarded=queue->GetNumMessagesDiscardedByMessageCountLimit();
        std::cerr<<"D3D12 validation queue at "<<stage<<": stored="<<queue->GetNumStoredMessages()
            <<" discarded="<<discarded<<" limit="<<queue->GetMessageCountLimit()<<'\n';
        if(discarded) {
            SIZE_T filter_bytes{};queue->GetStorageFilter(nullptr,&filter_bytes);
            std::vector<unsigned char> filter_storage(filter_bytes);
            auto* storage_filter=reinterpret_cast<D3D12_INFO_QUEUE_FILTER*>(filter_storage.data());
            check(SUCCEEDED(queue->GetStorageFilter(storage_filter,&filter_bytes)),"D3D12 storage filter unavailable");
            std::cerr<<"D3D12 active storage filter allow/deny severities="
                <<storage_filter->AllowList.NumSeverities<<'/'<<storage_filter->DenyList.NumSeverities<<'\n';
            check(SUCCEEDED(queue->PushEmptyRetrievalFilter()),"D3D12 overflow diagnostic retrieval failed");
            for(UINT64 i=0;i<std::min<UINT64>(queue->GetNumStoredMessages(),4);++i) {
                SIZE_T bytes{};check(SUCCEEDED(queue->GetMessage(i,nullptr,&bytes)),"D3D12 retained message size unavailable");
                std::vector<unsigned char> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                check(SUCCEEDED(queue->GetMessage(i,message,&bytes)),"D3D12 retained message unavailable");
                std::cerr<<"D3D12 retained severity="<<message->Severity<<" ID="<<message->ID
                    <<": "<<message->pDescription<<'\n';
            }
            queue->PopRetrievalFilter();
        }
        D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
        D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
        check(SUCCEEDED(queue->PushRetrievalFilter(&filter)),"D3D12 error filter failed");
        struct Pop {ID3D12InfoQueue* queue;~Pop(){queue->PopRetrievalFilter();}} pop{queue.Get()};
        const auto count=queue->GetNumStoredMessagesAllowedByRetrievalFilter();
        for(UINT64 i=0;i<std::min<UINT64>(count,16);++i) {
            SIZE_T bytes=0;check(SUCCEEDED(queue->GetMessage(i,nullptr,&bytes)),"D3D12 debug message size unavailable");
            std::vector<unsigned char> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            check(SUCCEEDED(queue->GetMessage(i,message,&bytes)),"D3D12 debug message unavailable");
            std::cerr<<"D3D12 validation ID "<<message->ID<<" at "<<stage<<": "<<message->pDescription<<'\n';
        }
        check(discarded==0,"Native presenter debug messages overflowed; validation is incomplete");
        check(count==0,"Native presenter emitted D3D12 errors (including allocator/fence reuse and SDK teardown)");
    }
    std::cout<<"Native presenter: zero D3D12 critical/discarded messages at "<<stage<<std::endl;
}
struct GpuDevice {
    SDL_GPUDevice* device{};
    const StarfoxSdlD3D12XrBridgeV1* bridge{};
    ~GpuDevice() {
        if(device) SDL_WaitForGPUIdle(device);
        // The mocked compositor owns native readback lists/resources beyond
        // xrDestroySwapchain. Retire them while the device/driver is alive,
        // including assertion-failure unwinding, not at process shutdown.
        gpu_mock=GpuMock{};
        if(device) SDL_DestroyGPUDevice(device);
        SDL_Quit();
    }
};
struct EyeSources {
    SDL_GPUDevice* device;
    std::array<SDL_GPUTexture*,2> textures{};
    std::array<SDL_GPUTransferBuffer*,2> uploads{};
    std::array<std::vector<unsigned char>,2> pixels;
    SDL_GPUCommandBuffer* command{};
    std::array<unsigned,2> extent{};
    explicit EyeSources(SDL_GPUDevice* owner):device(owner) {}
    ~EyeSources() {
        if(command) SDL_CancelGPUCommandBuffer(command);
        SDL_WaitForGPUIdle(device);
        for(auto* texture:textures) if(texture) SDL_ReleaseGPUTexture(device,texture);
        for(auto* upload:uploads) if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
    }
    void initialize(const DisplayXrD3D12Presenter& presenter,unsigned seed=0,bool wrong_right_extent=false) {
        extent=presenter.eye_extent(0);
        command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());
        for(unsigned eye=0;eye<2;++eye) {
            const auto width=extent[0]+unsigned(eye && wrong_right_extent),height=extent[1];
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
            info.format=static_cast<SDL_GPUTextureFormat>(presenter.color_format());
            info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
            info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            textures[eye]=SDL_CreateGPUTexture(device,&info);check(textures[eye],SDL_GetError());
            pixels[eye].resize(width*height*4);
            for(unsigned i=0;i<pixels[eye].size();++i) pixels[eye][i]=static_cast<unsigned char>((i*37+i/width+eye*103+seed*59)%251);
            SDL_GPUTransferBufferCreateInfo transfer{};transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;transfer.size=pixels[eye].size();
            uploads[eye]=SDL_CreateGPUTransferBuffer(device,&transfer);check(uploads[eye],SDL_GetError());
            auto* mapped=SDL_MapGPUTransferBuffer(device,uploads[eye],false);check(mapped,SDL_GetError());
            std::memcpy(mapped,pixels[eye].data(),pixels[eye].size());SDL_UnmapGPUTransferBuffer(device,uploads[eye]);
            auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
            SDL_GPUTextureTransferInfo source{};source.transfer_buffer=uploads[eye];source.pixels_per_row=width;source.rows_per_layer=height;
            SDL_GPUTextureRegion region{};region.texture=textures[eye];region.w=width;region.h=height;region.d=1;
            SDL_UploadToGPUTexture(copy,&source,&region,false);SDL_EndGPUCopyPass(copy);
        }
        const bool submitted=SDL_SubmitGPUCommandBuffer(command);command=nullptr;check(submitted,SDL_GetError());
    }
    void verify_compositor_pixels() {
        // The mocked runtime, not the app presenter, read its own images in
        // xrEndFrame on the real shared queue. This CPU map is fixture proof.
        check(SDL_WaitForGPUIdle(device),SDL_GetError());
        if(mock.backend==DisplayXrBackend::vulkan) {vulkan_verify_pixels(pixels);return;}
        auto& read=gpu_mock.readback;
        check(read.pending,"Mocked compositor did not consume a projection");
        for(unsigned eye=0;eye<2;++eye) {
            void* mapped{};D3D12_RANGE range{0,SIZE_T(read.footprints[eye].Footprint.RowPitch)*extent[1]};
            check(SUCCEEDED(read.buffers[eye]->Map(0,&range,&mapped)),"Compositor fixture map failed");
            bool exact=true;
            for(unsigned y=0;y<extent[1];++y) exact&=std::memcmp(
                static_cast<unsigned char*>(mapped)+read.footprints[eye].Footprint.RowPitch*y,
                pixels[eye].data()+extent[0]*4*y,extent[0]*4)==0;
            const D3D12_RANGE no_write{};read.buffers[eye]->Unmap(0,&no_write);check(exact,"Native compositor received wrong/crossed/stale eye pixels");
        }
        read.pending=false;gpu_mock.retired_images.clear();
    }
    void render_calibrated_geometry(const DisplayXrD3D12Presenter& presenter,const DisplayXrD3D12Frame& frame) {
        struct Targets {
            SDL_GPUDevice* device;std::array<SDL_GPUTexture*,2> depth{};std::array<SDL_GPUTransferBuffer*,2> read{};
            ~Targets() {
                SDL_WaitForGPUIdle(device);
                for(auto* z:depth) if(z) SDL_ReleaseGPUTexture(device,z);
                for(auto* r:read) if(r) SDL_ReleaseGPUTransferBuffer(device,r);
            }
        } targets{device};
        GpuCalibratedScene scene;const bool ready=scene.initialize(device,presenter.color_format());check(ready,scene.status().c_str());
        // Source-space geometry shared by both eyes. No projected screen
        // quads, fixed eye offsets, advancing animation or color-image warp.
        std::array<std::array<starfox::vr::SceneVertex,6>,2> vertices{};
        constexpr int corners[][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(unsigned object=0;object<2;++object) for(unsigned i=0;i<6;++i) {
            auto& v=vertices[object][i];v.position[0]=float(object?22:-12)+corners[i][0]*7.F;
            v.position[1]=float(object?-14:12)+corners[i][1]*7.F;v.position[2]=object?-180.F:-100.F;
            v.color[object]=1;v.color[3]=1;std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        // A real compute-owned ground producer, encoded once and consumed
        // by both calibrated eyes before the runtime handoff.
        starfox::vr::DrawPacket grid;grid.model=starfox::vr::source_layer_matrix(128,96,100).value();
        // Banked source rows keep measurable line coverage at this deliberately
        // tiny 128x72 runtime extent, rather than aliasing thin horizontal rows.
        grid.geometry.texels={17,uint32_t(-384),73,23170,23170,0,uint32_t(-23170),23170,0,0,0,32767,uint32_t(-50),32};
        constexpr unsigned grid_corners[6][2]{{0,0},{224,0},{224,192},{0,0},{224,192},{0,192}};
        for(const auto& corner:grid_corners) {
            starfox::vr::SceneVertex v{};v.position[0]=float(corner[0]+16);v.position[1]=float(corner[1]+16);
            v.uv[0]=float(corner[0]);v.uv[1]=float(corner[1]);v.texture[3]=starfox::vr::gpu_connected_grid_flag;
            v.color[2]=v.color[3]=v.odd_color[2]=v.odd_color[3]=1;grid.geometry.vertices.push_back(v);
        }
        std::array<starfox::vr::DrawPacket,2> models;
        for(unsigned object=0;object<2;++object) models[object].geometry.vertices.assign(vertices[object].begin(),vertices[object].end());
        const std::array<CalibratedScenePacket,3> draws{{{&grid,starfox::vr::SceneBlend::opaque,false},{&models[0]},{&models[1]}}};
        command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());
        const bool uploaded=scene.upload_packets(command,draws);check(uploaded,scene.status().c_str());
        const auto token=scene.upload_token();
        for(unsigned eye=0;eye<2;++eye) {
            SDL_GPUTextureCreateInfo z{};z.type=SDL_GPU_TEXTURETYPE_2D;z.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            z.width=extent[0];z.height=extent[1];z.layer_count_or_depth=z.num_levels=1;z.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            targets.depth[eye]=SDL_CreateGPUTexture(device,&z);check(targets.depth[eye],SDL_GetError());
            const bool rendered=scene.enqueue_eye(command,textures[eye],targets.depth[eye],extent[0],extent[1],frame.cameras[eye]);
            check(rendered,scene.status().c_str());
            const SDL_GPUTransferBufferCreateInfo read{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,extent[0]*extent[1]*4,0};
            targets.read[eye]=SDL_CreateGPUTransferBuffer(device,&read);check(targets.read[eye],SDL_GetError());
            auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
            const SDL_GPUTextureRegion source{textures[eye],0,0,0,0,0,extent[0],extent[1],1};
            SDL_GPUTextureTransferInfo dest{};dest.transfer_buffer=targets.read[eye];dest.pixels_per_row=extent[0];dest.rows_per_layer=extent[1];
            SDL_DownloadFromGPUTexture(copy,&source,&dest);SDL_EndGPUCopyPass(copy);
        }
        const bool submitted=SDL_SubmitGPUCommandBuffer(command);command=nullptr;check(submitted,SDL_GetError());
        check(scene.notify_submitted(token),"Native scene/ground upload submission not acknowledged");
        check(SDL_WaitForGPUIdle(device),SDL_GetError());
        for(unsigned eye=0;eye<2;++eye) {
            auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,targets.read[eye],false));check(mapped,SDL_GetError());
            pixels[eye].assign(mapped,mapped+extent[0]*extent[1]*4);SDL_UnmapGPUTransferBuffer(device,targets.read[eye]);
            unsigned blue=0;
            for(unsigned i=0;i<pixels[eye].size();i+=4) blue+=pixels[eye][i]==0 && pixels[eye][i+1]==0 && pixels[eye][i+2]==255;
            if(blue<=15) std::cerr<<"Native ground fixture eye="<<eye<<" blue="<<blue<<'\n';
            check(blue>15,"Native calibrated compute ground was omitted from an eye");
            const auto& view=frame.calibrated.xr.views[eye];
            for(unsigned object=0;object<2;++object) {
                const double z=double(view.pose.position.z)-(object?-180:-100);
                const double x=double(object?22:-12)-view.pose.position.x,y=double(object?-14:12)-view.pose.position.y;
                const double left=std::tan(view.fov.angleLeft),right=std::tan(view.fov.angleRight);
                const double down=std::tan(view.fov.angleDown),up=std::tan(view.fov.angleUp);
                const int sx=int((x/z-left)/(right-left)*extent[0]),sy=int((up-y/z)/(up-down)*extent[1]);
                check(sx>0 && sx<int(extent[0])-1 && sy>0 && sy<int(extent[1])-1,"Calibrated fixture geometry offscreen");
                const auto offset=(unsigned(sy)*extent[0]+unsigned(sx))*4;
                check(pixels[eye][offset+object]==255 && pixels[eye][offset+(1-object)]==0,
                    "Presenter camera was not consumed by calibrated geometry renderer");
            }
        }
        check(pixels[0]!=pixels[1],"Calibrated geometry eyes collapsed to a flat duplicate");
    }
};
struct QueueGate {
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    explicit QueueGate(GpuDevice& gpu) {
        check(SUCCEEDED(gpu_mock.native->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_ID3D12Fence,
            reinterpret_cast<void**>(fence.GetAddressOf()))),"GPU stall fence allocation failed");
        check(gpu.bridge->with_queue(gpu.device,[](void* user,void* queue) {
            return SUCCEEDED(static_cast<ID3D12CommandQueue*>(queue)->Wait(static_cast<ID3D12Fence*>(user),1));
        },fence.Get()),"GPU stall queue wait failed");gpu_blocked=true;
    }
    void unblock() {gpu_blocked=false;check(SUCCEEDED(fence->Signal(1)),"GPU stall release failed");}
    ~QueueGate() {gpu_blocked=false;if(fence) fence->Signal(1);}
};
const StarfoxSdlD3D12BridgeV2* original_ray_bridge{};
unsigned ray_copy_calls{},failed_ray_copy{};
bool fail_ray_copy(void* command,void* source,void* destination,std::uint32_t bytes) {
    if(++ray_copy_calls==failed_ray_copy) {SDL_SetError("Injected native ray output-copy failure");return false;}
    return original_ray_bridge->copy_buffer(command,source,destination,bytes);
}
struct RayCopyFault {
    SDL_PropertiesID props;StarfoxSdlD3D12BridgeV2 bridge;
    RayCopyFault(SDL_GPUDevice* device,unsigned call):props(SDL_GetGPUDeviceProperties(device)) {
        original_ray_bridge=static_cast<const StarfoxSdlD3D12BridgeV2*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_BRIDGE,nullptr));
        check(original_ray_bridge,"Ray fault requires the real native mask bridge");
        bridge=*original_ray_bridge;bridge.copy_buffer=fail_ray_copy;ray_copy_calls=0;failed_ray_copy=call;
        check(SDL_SetPointerProperty(props,STARFOX_SDL_D3D12_BRIDGE,&bridge),"Cannot install local ray-copy failure fixture");
    }
    ~RayCopyFault() {SDL_SetPointerProperty(props,STARFOX_SDL_D3D12_BRIDGE,const_cast<StarfoxSdlD3D12BridgeV2*>(original_ray_bridge));failed_ray_copy=0;}
};
DisplayXrSubmission finish_presenter(DisplayXrD3D12Presenter& presenter,DisplayXrSubmission outcome) {
    for(unsigned i=0;i<600 && outcome==DisplayXrSubmission::waiting;++i) {SDL_Delay(5);outcome=presenter.finish_frame();}
    check(outcome!=DisplayXrSubmission::waiting,"GPU copy completion did not resolve");return outcome;
}
#include "calibrated_camera_response_oracle.inc"
#include "calibrated_msaa_owner_oracle.inc"
#include "calibrated_temporal_owner_oracle.inc"
#include "calibrated_reflection_owner_checks.inc"
#include "reflection_source_stage_timings.hpp"
#include "calibrated_curved_owner_checks.inc"
#include "calibrated_dlss_sdk_fixture.inc"
#include "calibrated_dlss_ray_sample_oracle.inc"
#include "calibrated_reconstructed_post_oracle.inc"
#include "calibrated_taa_liquid_post_oracle.inc"
#include "calibrated_scene_fx_owner_oracle.inc"
#include "calibrated_volumetric_owner_oracle.inc"
#include "calibrated_motion_blur_owner_oracle.inc"
#include "calibrated_ground_receiver_owner_oracle.inc"
#include "calibrated_fsr_owner_oracle.inc"
#include "calibrated_dlss_owner_oracle.inc"
#include "displayxr_vulkan_presenter_tests.inc"
void gpu_presenter_tests(DisplayXrRuntime& runtime,const char* rom_path=nullptr,const char* symbols_path=nullptr) {
    GpuDevice gpu;check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    gpu.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,true,"direct3d12");check(gpu.device,SDL_GetError());
    if(native_taa_pattern_only) std::cout<<"Native pattern adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu.device),
        SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<" driver="<<SDL_GetGPUDeviceDriver(gpu.device)<<std::endl;
    const auto props=SDL_GetGPUDeviceProperties(gpu.device);
    auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_DEVICE,nullptr));check(native,"Native SDL device missing");
    capture_native_d3d12_errors(native);
    gpu.bridge=static_cast<const StarfoxSdlD3D12XrBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_XR_BRIDGE,nullptr));
    check(gpu.bridge && gpu.bridge->version==1 && gpu.bridge->queue,"Native SDL XR bridge missing");
    auto* queue=static_cast<ID3D12CommandQueue*>(gpu.bridge->queue(gpu.device));check(queue,"Native SDL queue missing");
    auto reset=[&] {
        check(!native_mock.session_open && !native_mock.frame_open && !gpu_mock.readback.pending,"Fixture reset leaked active work");
        runtime.close();mock=Mock{};mock.image_width=128;mock.image_height=72;
        native_mock=NativeMock{};gpu_mock=GpuMock{};gpu_mock.enabled=true;gpu_mock.native=native;gpu_mock.queue=queue;
    };
    auto* window=reinterpret_cast<void*>(std::uintptr_t(0x3000));
    unsigned exact_frames{};
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                    DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM}) {
        reset();gpu_mock.format=format;check(runtime.initialize_with_api(get),"GPU fixture runtime failed");
        DisplayXrD3D12Presenter presenter;
        check(presenter.initialize(runtime,gpu.device,window),presenter.status().c_str());
        check(!presenter.running() && presenter.eye_extent(0)==std::array<std::uint32_t,2>{128,72}
            && presenter.eye_extent(2)==std::array<std::uint32_t,2>{},"Presenter extent/READY handling invalid");
        check(presenter.poll_events() && presenter.running(),"GPU presenter did not start on READY");
        gpu_mock.eyes[1].next=1; // eyes intentionally acquire different indices
        for(unsigned i=0;i<4;++i) {
            native_mock.head_x=12+float(i)*7;
            auto frame=presenter.begin_frame(native_mock.expected);
            check(frame && frame->calibrated.xr.should_render && frame->cameras[0].projection[5]>0
                && frame->cameras[0].projection[9]==-frame->calibrated.cameras[0].projection[9]
                && frame->cameras[0].view==frame->calibrated.cameras[0].view,"D3D12 calibrated camera was re-anchored or Y adapted incorrectly");
            EyeSources sources(gpu.device);sources.initialize(presenter,i);
            const auto outcome=finish_presenter(presenter,presenter.submit_frame(sources.textures[0],sources.textures[1]));
            check(outcome==DisplayXrSubmission::submitted && !presenter.frame_pending(),presenter.status().c_str());
            sources.verify_compositor_pixels();++exact_frames;
        }
        check(presenter.close() && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys
            && native_mock.session_creates==native_mock.session_destroys,"GPU presentation cleanup leaked native resources");
    }
    // Actual calibrated geometry -> SDL eye images -> runtime-owned images ->
    // mocked compositor, not just colored transport payloads. Fixture-only
    // readback verifies both analytic positions and exact compositor bytes.
    reset();check(runtime.initialize_with_api(get),"Geometry runtime failed");
    {
        DisplayXrD3D12Presenter presenter;check(presenter.initialize(runtime,gpu.device,window) && presenter.poll_events(),presenter.status().c_str());
        for(unsigned i=0;i<4;++i) {
            native_mock.head_x=float(i)*5;
            const auto frame=presenter.begin_frame(native_mock.expected);check(frame && frame->calibrated.xr.should_render,"Geometry frame failed");
            EyeSources sources(gpu.device);sources.initialize(presenter);sources.render_calibrated_geometry(presenter,*frame);
            check(finish_presenter(presenter,presenter.submit_frame(sources.textures[0],sources.textures[1]))==DisplayXrSubmission::submitted,
                presenter.status().c_str());sources.verify_compositor_pixels();
        }
        check(presenter.close(),"Geometry presenter cleanup failed");
    }
    // Second-eye timeout: keep the first acquired/ready image, never acquire
    // either eye again, and never write before the second wait succeeds.
    reset();check(runtime.initialize_with_api(get),"Timeout runtime failed");
    {
        DisplayXrD3D12Presenter presenter;check(presenter.initialize(runtime,gpu.device,window) && presenter.poll_events(),presenter.status().c_str());
        check(presenter.begin_frame(native_mock.expected).has_value(),"Timeout frame failed");
        EyeSources sources(gpu.device);sources.initialize(presenter);
        gpu_mock.timeout_right=true;
        check(presenter.submit_frame(sources.textures[0],sources.textures[1])==DisplayXrSubmission::waiting
            && gpu_mock.eyes[0].acquires==1 && gpu_mock.eyes[1].acquires==1 && gpu_mock.projections==0,"Timed-out eye rendered/reacquired");
        check(!presenter.begin_frame(native_mock.expected) && presenter.frame_pending()
            && !presenter.poll_events(),"Overlapping begin/poll destroyed pending frame");
        check(presenter.submit_frame(sources.textures[0],sources.textures[1])==DisplayXrSubmission::waiting
            && gpu_mock.eyes[0].acquires==1 && gpu_mock.eyes[1].acquires==1,"Timeout retry reacquired an image");
        gpu_mock.timeout_right=false;
        check(finish_presenter(presenter,presenter.submit_frame(sources.textures[0],sources.textures[1]))==DisplayXrSubmission::submitted,
            presenter.status().c_str());sources.verify_compositor_pixels();
        check(gpu_mock.eyes[0].waits==1 && gpu_mock.eyes[1].waits==3,"Ready eye waited again or second eye retry lost");
        check(presenter.close(),"Timeout presenter cleanup failed");
    }
    // A real queue stall proves nonblocking finish/cancellation preserve both
    // runtime images until the actual SDL fence can complete.
    reset();check(runtime.initialize_with_api(get),"GPU-stall runtime failed");
    {
        DisplayXrD3D12Presenter presenter;check(presenter.initialize(runtime,gpu.device,window) && presenter.poll_events(),presenter.status().c_str());
        check(presenter.begin_frame(native_mock.expected).has_value(),"GPU-stall frame failed");
        EyeSources sources(gpu.device);sources.initialize(presenter);
        QueueGate gate(gpu);
        check(presenter.submit_frame(sources.textures[0],sources.textures[1])==DisplayXrSubmission::waiting
            && presenter.finish_frame()==DisplayXrSubmission::waiting && gpu_mock.projections==0
            && gpu_mock.eyes[0].releases==0 && gpu_mock.eyes[1].releases==0,"GPU-pending eye released/submitted early");
        check(presenter.cancel_frame()==DisplayXrSubmission::waiting && presenter.frame_pending(),"Cancellation ignored in-flight GPU work");
        gate.unblock();check(finish_presenter(presenter,DisplayXrSubmission::waiting)==DisplayXrSubmission::submitted
            && native_mock.last_layers==0 && gpu_mock.projections==0 && !presenter.frame_pending(),"Cancelled GPU frame submitted a projection");
        check(presenter.close(),"GPU-stall cleanup failed");
    }
    // A failed producer submission without a returned fence is uncertain,
    // not idle. Recovery inserts an ordered fence and polls it nonblocking.
    reset();check(runtime.initialize_with_api(get),"Uncertain producer runtime failed");
    {
        DisplayXrD3D12Presenter presenter;
        check(presenter.initialize(runtime,gpu.device,window) && presenter.poll_events(),presenter.status().c_str());
        check(presenter.begin_frame(native_mock.expected).has_value(),"Uncertain producer frame failed");
        QueueGate gate(gpu);presenter.retain_scene_submission(nullptr);
        check(!presenter.try_close() && !presenter.try_close() && presenter.frame_pending()
            && gpu_mock.swapchain_destroys==0 && presenter.presented_frames()==0,
            "Uncertain producer cleanup freed native dependencies before ordered completion");
        gate.unblock();bool closed=false;
        for(unsigned attempt=0;!closed && attempt<10000;++attempt) {closed=presenter.try_close();if(!closed) SDL_Delay(1);}
        check(closed && native_mock.last_layers==0 && gpu_mock.projections==0
            && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,
            "Ordered uncertain-submission recovery leaked images or submitted a layer");
    }
    for(unsigned failure=0;failure<7;++failure) {
        reset();
        if(failure==0) mock.missing="xrReleaseSwapchainImage";
        if(failure==1) gpu_mock.fail_create=1;
        if(failure==2) gpu_mock.fail_images=1;
        if(failure==3) gpu_mock.bad_image=true;
        if(failure==4) gpu_mock.format=DXGI_FORMAT_R32_FLOAT;
        if(failure==5) native_mock.ignore_request=true;
        check(runtime.initialize_with_api(get),"Failure fixture runtime failed");
        DisplayXrD3D12Presenter presenter;
        if(failure<6) {
            check(!presenter.initialize(runtime,gpu.device,window) && !presenter.frame_pending()
                && !native_mock.session_open && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,
                "Native swapchain/init failure leaked resources");
        } else check(!presenter.initialize(runtime,gpu.device,nullptr),"Missing panel window accepted");
        mock.missing=nullptr;gpu_mock.fail_create=gpu_mock.fail_images=-1;gpu_mock.bad_image=false;
        gpu_mock.format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;native_mock.ignore_request=false;
        check(presenter.initialize(runtime,gpu.device,window) && presenter.close(),"Presenter did not recover after initialization failure");
    }
    for(unsigned failure=0;failure<6;++failure) {
        reset();check(runtime.initialize_with_api(get),"Frame failure runtime failed");
        DisplayXrD3D12Presenter presenter;check(presenter.initialize(runtime,gpu.device,window) && presenter.poll_events(),presenter.status().c_str());
        if(failure==0) {
            native_mock.bad_raw=true;
            const auto frame=presenter.begin_frame(native_mock.expected);
            check(frame && !frame->calibrated.xr.should_render && !presenter.frame_pending() && native_mock.last_layers==0,
                "Invalid calibrated frame acquired/rendered eye images");
        } else {
            check(presenter.begin_frame(native_mock.expected).has_value(),"Failure frame failed to begin");
            EyeSources sources(gpu.device);sources.initialize(presenter,0,failure==1);
            if(failure==2) gpu_mock.bad_index=true;
            if(failure==3) gpu_mock.fail_release=true;
            if(failure==4) native_mock.active=false;
            if(failure==5) gpu_mock.fail_end=true;
            const auto outcome=finish_presenter(presenter,presenter.submit_frame(sources.textures[0],sources.textures[1]));
            check(outcome==DisplayXrSubmission::failed && !presenter.frame_pending(),"Frame failure retained/accepted an incomplete frame");
            if(failure==5) sources.verify_compositor_pixels();
            else check(native_mock.last_layers==0 && gpu_mock.projections==0,"Failed partial frame submitted a visible layer");
        }
        check(presenter.close() && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,"Failed-frame close leaked images");
    }
    // Close an in-flight frame: completion may block teardown, never free the
    // native images immediately. A helper only releases the deliberate stall.
    reset();check(runtime.initialize_with_api(get),"Teardown runtime failed");
    {
        DisplayXrD3D12Presenter presenter;check(presenter.initialize(runtime,gpu.device,window) && presenter.poll_events(),presenter.status().c_str());
        check(presenter.begin_frame(native_mock.expected).has_value(),"Teardown frame failed");
        EyeSources sources(gpu.device);sources.initialize(presenter);QueueGate gate(gpu);
        check(presenter.submit_frame(sources.textures[0],sources.textures[1])==DisplayXrSubmission::waiting,"Teardown stall did not hold copy");
        std::jthread release([&] {SDL_Delay(25);gate.unblock();});
        check(presenter.close() && !presenter.frame_pending() && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys
            && native_mock.last_layers==0,"In-flight teardown leaked/submitted native images");
    }
    // Complete game packets -> independent SDL eyes -> native runtime images.
    // Source/two-eye ownership survives both image waits and queue stalls.
    reset();check(runtime.initialize_with_api(get),"Game renderer runtime failed");
    {
        native_mock.expected=calibrated_game_rig();native_mock.head_x=0;native_mock.pose_scale=1.F/256.F;
        DisplayXrGameRenderer renderer;
        check(renderer.initialize(runtime,gpu.device,window,nullptr,dlss_fixture::api) && renderer.poll_events(),renderer.status().c_str());
        check(!renderer.source_sample_diagnostic_image(0) && !renderer.source_raster_diagnostic_camera(0)
            && !renderer.reflection_liquid_inputs(0).ownership
            && !renderer.set_source_sample_diagnostic_offset({}) && !renderer.enable_curved_reflection_validation(true)
            && !renderer.enable_source_reflection_validation(true)
            && !renderer.set_source_stage_observer([](void*,const ReflectionSourceStageEvent&){})
            && !renderer.set_source_stage_observer(nullptr,&renderer),
            "Primary/curved diagnostics were enabled by default");
        check(renderer.enable_reflection_diagnostics(true) && !renderer.source_sample_diagnostic_image(0)
            && !renderer.source_raster_diagnostic_camera(0),"Primary diagnostics exposed an unencoded image/camera");
        std::shared_ptr<const CalibratedGameFrame> source,enhanced_source;
        if(rom_path && symbols_path) {
            const auto rom=starfox::assets::RomImage::load(rom_path);
            const auto symbols=starfox::assets::SymbolMap::load(symbols_path);
            starfox::simulation::GameSimulation game(rom,symbols,"LEVEL1_1",{},true);game.set_god_mode(true);
            starfox::audio::Spc700Audio audio;
            for(unsigned i=0;i<700;++i) {const auto tick=game.tick({});(void)audio.render_logic_tick(tick.audio_port_writes);
                game.synchronize_apu_output_ports(audio.output_ports());}
            starfox::vr::GameSceneHistory history(game,rom,symbols);CalibratedGameScene assembler(rom,symbols);
            const auto completed_tick=history.current();history.capture();
            check(history.current()->revision==completed_tick->revision+1
                && history.current()->scene_epoch==completed_tick->scene_epoch
                && history.current()->scene_epoch==game.scene_revision(),
                "Completed native tick confused scene identity with the per-tick revision");
            CalibratedGameSettings settings;settings.srgb=renderer.srgb_target();
            if(native_fog_only) settings.volumetric_fog=3;
            source=std::make_shared<const CalibratedGameFrame>(assembler.assemble(history.previous(),history.current(),game,1,settings));
            std::unordered_map<std::string,std::vector<std::uint8_t>> artwork;
            const CalibratedBackdropLoader loader=[&](unsigned,std::string_view path)->std::span<const std::uint8_t> {
                auto& bytes=artwork[std::string(path)];
                if(bytes.empty()) {std::ifstream file(std::string(path),std::ios::binary);check(bool(file),"Missing native-owner enhanced artwork");
                    bytes.assign(std::istreambuf_iterator<char>(file),{});}
                return bytes;
            };
            settings.enhanced_sky=true;
            enhanced_source=std::make_shared<const CalibratedGameFrame>(assembler.assemble(history.previous(),history.current(),game,1,settings,loader));
        } else {
            auto frame=std::make_shared<CalibratedGameFrame>();
            frame->current=frame->previous=std::make_shared<starfox::vr::GameSceneSnapshot>();
            frame->settings.srgb=renderer.srgb_target();frame->clear={0,0,0,1};
            starfox::vr::DrawPacket model;
            constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
            for(const auto& corner:corners) {
                starfox::vr::SceneVertex v{};v.position[0]=corner[0]*.1F;v.position[1]=corner[1]*.1F;v.position[2]=-2;
                v.color[0]=v.color[3]=v.odd_color[0]=v.odd_color[3]=1;model.geometry.vertices.push_back(v);
            }
            frame->draws.push_back({std::move(model),CalibratedGameLayer::model,starfox::vr::SceneBlend::opaque,true});source=frame;
        }
        gpu_mock.timeout_right=true;
        check(renderer.submit(source)==DisplayXrSubmission::waiting && renderer.frame_pending()
            && renderer.retained_frame()==source,"Game source/eye images not retained through runtime wait");
        check(!renderer.source_sample_diagnostic_image(0) && !renderer.source_raster_diagnostic_camera(0)
            && !renderer.set_source_sample_diagnostic_offset({.25F,0}) && !renderer.enable_reflection_diagnostics(false)
            && !renderer.enable_source_reflection_validation(true) && !renderer.enable_source_reflection_validation(false),
            "Pending frame exposed/mutated borrowed primary diagnostics");
        check(renderer.submit(source)==DisplayXrSubmission::failed && renderer.retained_frame()==source
            && !renderer.poll_events(),"Next game frame replaced waiting source state");
        check(renderer.poll()==DisplayXrSubmission::waiting && gpu_mock.eyes[0].acquires==1
            && gpu_mock.eyes[1].acquires==1,"Game renderer reacquired a waiting eye");
        gpu_mock.timeout_right=false;
        auto outcome=renderer.poll();
        for(unsigned attempt=0;outcome==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);outcome=renderer.poll();}
        check(outcome==DisplayXrSubmission::submitted && !renderer.frame_pending() && !renderer.retained_frame(),renderer.status().c_str());
        check(renderer.source_sample_diagnostic_image(0) && renderer.source_raster_diagnostic_camera(0)
            && !renderer.source_sample_diagnostic_image(2) && !renderer.source_sample_diagnostic_image(0,1)
            && !renderer.source_raster_diagnostic_camera(2),"Completed primary diagnostic image/camera bounds are invalid");
        check(renderer.enable_reflection_diagnostics(false) && !renderer.source_sample_diagnostic_image(0)
            && !renderer.source_raster_diagnostic_camera(0) && !renderer.reflection_liquid_inputs(0).ownership,
            "Disabled primary diagnostics exposed borrowed state");
        check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
        const auto consume_game_images=[&] {
            auto& read=gpu_mock.readback;check(read.pending,"Compositor did not receive complete game eyes");
            std::array<std::vector<unsigned char>,2> pixels;
            for(unsigned eye=0;eye<2;++eye) {
                void* mapped{};const auto size=mock.image_width*mock.image_height*4;
                const D3D12_RANGE range{0,SIZE_T(read.footprints[eye].Footprint.RowPitch)*mock.image_height};
                check(SUCCEEDED(read.buffers[eye]->Map(0,&range,&mapped)),"Game compositor map failed");pixels[eye].resize(size);
                for(unsigned row=0;row<mock.image_height;++row) std::memcpy(pixels[eye].data()+row*mock.image_width*4,
                    static_cast<unsigned char*>(mapped)+row*read.footprints[eye].Footprint.RowPitch,mock.image_width*4);
                const D3D12_RANGE no_write{};read.buffers[eye]->Unmap(0,&no_write);
            }
            read.pending=false;gpu_mock.retired_images.clear();return pixels;
        };
        const auto pixels=consume_game_images();
        check(pixels[0]!=pixels[1],"Calibrated game handoff collapsed to flat/crossed eyes");
        if(rom_path && symbols_path) for(unsigned quality:{1U,2U,3U}) {
            auto traced=std::make_shared<CalibratedGameFrame>(*source);
            traced->settings.ray_tracing=traced->settings.reflections=quality;
            outcome=renderer.submit(traced);
            for(unsigned attempt=0;outcome==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);outcome=renderer.poll();}
            check(outcome==DisplayXrSubmission::submitted,renderer.status().c_str());
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());const auto actual=consume_game_images();
            check(actual[0]!=actual[1] && actual[0]!=pixels[0] && actual[1]!=pixels[1],
                "Real cartridge RT/reflections bypassed models or collapsed calibrated eyes");
            check(traced->current==source->current && traced->previous==source->previous,
                "Native ray evaluation replaced the retained cartridge source tick");
        }
        if(rom_path && symbols_path) for(const auto& artwork_source:{source,enhanced_source}) if(artwork_source) {
            const auto render_owned=[&](std::shared_ptr<const CalibratedGameFrame> frame) {
                auto result=renderer.submit(std::move(frame));
                for(unsigned attempt=0;result==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::submitted && SDL_WaitForGPUIdle(gpu.device),renderer.status().c_str());
                return consume_game_images();
            };
            auto dielectric=std::make_shared<CalibratedGameFrame>(*artwork_source);
            dielectric->settings.ray_tracing=dielectric->settings.reflections=3;
            const auto reference=render_owned(dielectric);
            auto no_rays=std::make_shared<CalibratedGameFrame>(*artwork_source);
            const auto unstylized=render_owned(no_rays);
            for(unsigned quality:{1U,2U,3U}) {
                auto selected=std::make_shared<CalibratedGameFrame>(*no_rays);
                selected->settings.aa_type=3;selected->settings.aa_quality=quality;
                const auto actual=render_owned(selected);
                check(actual!=unstylized && actual[0]!=actual[1],"Cartridge SSAA bypassed actual raster samples or collapsed the eyes");
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=3;at<actual[eye].size();at+=4)
                    check(actual[eye][at]==unstylized[eye][at],"Cartridge SSAA changed native alpha");
                check(render_owned(selected)==actual,"Held original/enhanced cartridge SSAA jittered");
                check(selected->current==source->current && selected->previous==source->previous,"SSAA replaced the retained cartridge tick");
            }
            check(render_owned(no_rays)==unstylized,"Cartridge SSAA OFF did not restore native resolution/image");
            std::cout<<"Cartridge SSAA: original/enhanced scenery, all raster qualities, independent eyes, alpha/held source and exact OFF passed.\n";
            if(native_dlss_only) {
                bool reconstructed=false;
                for(unsigned model:{0U,1U})for(unsigned quality=1;quality<=4;++quality) {
                    auto selected=std::make_shared<CalibratedGameFrame>(*dielectric);
                    selected->settings.dlss_mode=quality;selected->settings.dlss_model=model;
                    const auto calls=dlss_fixture::evaluations.size();const auto actual=render_owned(selected);
                    check(dlss_fixture::evaluations.size()==calls+2 && actual[0]!=actual[1],
                        "Cartridge DLSS did not consume two actual SDK eyes");
                    reconstructed|=actual!=reference;
                    for(unsigned eye=0;eye<2;++eye)for(unsigned at=3;at<actual[eye].size();at+=4)
                        check(actual[eye][at]==reference[eye][at],"Cartridge DLSS changed authored opacity");
                    const auto accepted_calls=dlss_fixture::evaluations.size();
                    check(render_owned(selected)==actual && dlss_fixture::evaluations.size()==accepted_calls,
                        "Held cartridge DLSS reevaluated or jittered original/enhanced artwork");
                    check(selected->current==source->current && selected->previous==source->previous,
                        "Cartridge DLSS advanced the retained source tick");
                    check(render_owned(dielectric)==reference,"Cartridge DLSS OFF did not restore the exact native image");
                }
                check(reconstructed,"Cartridge DLSS never reconstructed the actual model/scene image");
                std::cout<<"Cartridge DLSS: both SDK models/all quality modes, original/enhanced scenery, real RT/reflections, "
                    "independent eyes, authored opacity, held source and exact OFF passed.\n";
            }
            for(unsigned style=0;style<starfox::render::effect_count;++style) {
                if(!starfox::render::calibrated_composite_effect(style)) continue;
                auto selected=std::make_shared<CalibratedGameFrame>(*no_rays);
                selected->settings.world_effects={style,65,0,0};selected->settings.model_effects={style,100,0,0};
                const auto actual=render_owned(selected);
                check(actual[0]!=unstylized[0] && actual[1]!=unstylized[1] && actual[0]!=actual[1],
                    "Cartridge drawn style was bypassed without RT or collapsed native eye images");
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=3;at<unstylized[eye].size();at+=4)
                    check(actual[eye][at]==unstylized[eye][at],"Cartridge drawn style changed source alpha");
                check(selected->current==source->current && selected->previous==source->previous,
                    "Cartridge drawn style advanced or replaced retained source snapshots");
            }
            check(render_owned(no_rays)==unstylized,"Cartridge drawn-style OFF failed exact restoration");
            for(auto material:starfox::render::effect_order) {
                if(!starfox::render::material(material)) continue;
                auto selected=std::make_shared<CalibratedGameFrame>(*dielectric);selected->settings.material=unsigned(material);
                const auto actual=render_owned(selected);
                check(actual[0]!=reference[0] && actual[1]!=reference[1] && actual[0]!=actual[1],
                    "Actual cartridge material bypassed native models or collapsed enhanced/original stereo eyes");
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=3;at<reference[eye].size();at+=4)
                    check(actual[eye][at]==reference[eye][at],"Cartridge material changed native source alpha");
                check(selected->current==source->current && selected->previous==source->previous,
                    "Cartridge material advanced or replaced its retained source snapshots");
            }
            check(render_owned(dielectric)==reference,"Cartridge material OFF failed to restore exact native dielectric image");
        }
        if(enhanced_source) {
            auto enhanced=std::make_shared<CalibratedGameFrame>(*enhanced_source);
            enhanced->settings.ray_tracing=enhanced->settings.reflections=2;
            outcome=renderer.submit(enhanced);
            for(unsigned attempt=0;outcome==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);outcome=renderer.poll();}
            check(outcome==DisplayXrSubmission::submitted,renderer.status().c_str());
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());const auto actual=consume_game_images();
            check(actual[0]!=actual[1] && actual[0]!=pixels[0] && actual[1]!=pixels[1],
                "Enhanced cartridge sky/ray owner bypassed the native source or collapsed eyes");
            check(enhanced->current==source->current && enhanced->previous==source->previous,
                "Enhanced ray scenery advanced the cartridge source tick");
            outcome=renderer.submit(source);
            for(unsigned attempt=0;outcome==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);outcome=renderer.poll();}
            check(outcome==DisplayXrSubmission::submitted && SDL_WaitForGPUIdle(gpu.device),renderer.status().c_str());
            check(consume_game_images()==pixels,"Enhanced native source did not restore the exact original scene");
        }
        // The game renderer must retain its source and both rendered images
        // even after native image acquisition succeeds but the GPU is stalled.
        // Retry through the owner, not directly through its hidden presenter.
        {
            QueueGate gate(gpu);
            check(renderer.submit(source)==DisplayXrSubmission::waiting && renderer.frame_pending()
                && renderer.retained_frame()==source,"GPU stall discarded the native game source");
            const auto left_acquires=gpu_mock.eyes[0].acquires,right_acquires=gpu_mock.eyes[1].acquires;
            const auto projections=gpu_mock.projections;
            check(renderer.poll()==DisplayXrSubmission::waiting && renderer.poll()==DisplayXrSubmission::waiting
                && renderer.retained_frame()==source && gpu_mock.projections==projections
                && gpu_mock.eyes[0].acquires==left_acquires && gpu_mock.eyes[1].acquires==right_acquires,
                "GPU stall submitted or reacquired native game eyes");
            check(renderer.submit(source)==DisplayXrSubmission::failed && renderer.retained_frame()==source,
                "Replacement source overwrote in-flight game eyes");
            gate.unblock();outcome=renderer.poll();
            for(unsigned attempt=0;outcome==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);outcome=renderer.poll();}
            check(outcome==DisplayXrSubmission::submitted && !renderer.frame_pending() && !renderer.retained_frame(),
                "Stalled game frame did not complete through its owner");
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
            check(consume_game_images()==pixels,"GPU wait retry altered or crossed the retained game images");
        }
        // Invalid tracking must consume no stale game image or source snapshot.
        native_mock.valid_tracking=false;
        check(renderer.submit(source)==DisplayXrSubmission::submitted && native_mock.last_layers==0
            && !renderer.frame_pending() && !renderer.retained_frame(),"Invalid game tracking presented stale eyes");
        // Teardown also waits for the same complete game upload/copy rather
        // than releasing the targets under its queued native work.
        native_mock.valid_tracking=true;
        {
            auto supersampled=std::make_shared<CalibratedGameFrame>(*source);
            supersampled->settings.aa_type=3;supersampled->settings.aa_quality=3;
            QueueGate gate(gpu);
            check(renderer.submit(supersampled)==DisplayXrSubmission::waiting && renderer.retained_frame()==supersampled,
                "Game teardown fixture did not retain a pending frame");
            std::jthread release([&] {SDL_Delay(25);gate.unblock();});
            check(renderer.close() && !renderer.frame_pending() && !renderer.retained_frame()
                && native_mock.last_layers==0 && !gpu_mock.readback.pending
                && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,"Game renderer freed live images during teardown");
        }
        // Live recovery cannot synchronously wait for either producer or copy.
        // In particular, an XR-image timeout can leave only the scene producer
        // queued: no native copy fence exists yet to protect its source.
        for(unsigned algorithm:{3U,6U}) for(bool image_wait:{false,true}) {
            check(renderer.initialize(runtime,gpu.device,window) && renderer.poll_events(),renderer.status().c_str());
            auto supersampled=std::make_shared<CalibratedGameFrame>(*source);
            supersampled->settings.aa_type=algorithm;supersampled->settings.aa_quality=3;
            gpu_mock.timeout_right=image_wait;QueueGate gate(gpu);
            check(renderer.submit(supersampled)==DisplayXrSubmission::waiting && renderer.retained_frame()==supersampled,
                "Nonblocking close fixture did not retain its source");
            const auto destroyed=gpu_mock.swapchain_destroys,projections=gpu_mock.projections;
            const auto started=std::chrono::steady_clock::now();
            for(unsigned attempt=0;attempt<3;++attempt)
                check(!renderer.try_close() && renderer.retained_frame()==supersampled && renderer.frame_pending()
                    && gpu_mock.swapchain_destroys==destroyed && gpu_mock.projections==projections,
                    "Nonblocking recovery released a pending producer/copy or presented stale eyes");
            check(std::chrono::steady_clock::now()-started<std::chrono::milliseconds(100),
                "Live recovery blocked waiting for the stalled GPU");
            gate.unblock();
            bool closed=false;
            for(unsigned attempt=0;!closed && attempt<10000;++attempt) {closed=renderer.try_close();if(!closed) SDL_Delay(1);}
            check(closed && !renderer.retained_frame() && !renderer.frame_pending()
                && native_mock.last_layers==0 && gpu_mock.projections==projections
                && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,
                "Nonblocking close did not drain both submissions without visible layers");
            gpu_mock.timeout_right=false;
        }
        std::cout<<"Native game renderer: "<<(rom_path?"cartridge":"synthetic")
            <<" owned packets consumed by the real-queue compositor; image/GPU wait retries, byte-exact retained images, independent eyes, invalid tracking and in-flight close passed.\n";
    }
    // The live owner, not merely the standalone composite fixture, must retain
    // BOTH native ray producers and the before/composite/after eye submissions.
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                     DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM}) {
        reset();gpu_mock.format=format;native_mock.expected=calibrated_game_rig();native_mock.head_x=0;native_mock.pose_scale=1.F/256.F;
        if(native_dlss_only) {mock.image_width=native_dlss_panel_bound_only?1920:320;mock.image_height=native_dlss_panel_bound_only?1080:192;}
        if(native_taa_pattern_only) {mock.image_width=192;mock.image_height=448;}
        if(native_taa_liquid_post_only || native_dlss_liquid_post_only) {mock.image_width=256;mock.image_height=224;}
        check(runtime.initialize_with_api(get),"Ray owner runtime failed");DisplayXrGameRenderer renderer;
        check(renderer.initialize(runtime,gpu.device,window,nullptr,dlss_fixture::api) && renderer.poll_events(),renderer.status().c_str());
        auto source=std::make_shared<CalibratedGameFrame>();
        source->current=source->previous=std::make_shared<starfox::vr::GameSceneSnapshot>();
        source->settings.srgb=renderer.srgb_target();source->clear={.07F,.11F,.18F,1};
        const auto add_quad=[&](float x,float y,float z,float radius,std::array<float,4> color,bool caster,bool after,bool depth=true) {
            starfox::vr::DrawPacket p;
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                starfox::vr::SceneVertex v{};v.position[0]=x+((corner==1 || corner==2)?radius:-radius);
                v.position[1]=y+(corner>=2?radius:-radius);v.position[2]=z;
                std::copy(color.begin(),color.end(),v.color);std::copy(color.begin(),color.end(),v.odd_color);p.geometry.vertices.push_back(v);
            }
            source->draws.push_back({std::move(p),after?CalibratedGameLayer::native_ui:CalibratedGameLayer::model,
                starfox::vr::SceneBlend::opaque,depth,0,caster,after});
        };
        add_quad(0,0,-2,.7F,{.2F,.4F,.7F,1},true,false);
        add_quad(.13F,.06F,-1.1F,.07F,{.7F,.5F,.2F,1},true,false);
        add_quad(-.08F,-.05F,-.8F,.025F,{1,0,0,1},false,false); // emissive foreground
        add_quad(.04F,.1F,-.7F,.035F,{1,1,0,1},false,true,false); // native HUD ink
        add_quad(.95F,.2F,-2.5F,.24F,{.78F,.82F,.87F,1},false,false);
        source->draws.back().layer=CalibratedGameLayer::world;
        for(auto& v:source->draws.back().packet.geometry.vertices) {
            v.color[0]=v.odd_color[0]=v.position[1]>.2F?.8F:.15F;
            v.color[1]=v.odd_color[1]=v.position[0]>.95F?.7F:.25F;
        }
        // Authored scenery BEHIND the real camera: it is not visible in the
        // primary image, but mirrors must capture it without UI/models.
        add_quad(0,0,3,16,{.12F,.64F,.28F,1},false,false,false);
        source->draws.back().layer=CalibratedGameLayer::world;
        source->draws.back().reflection_environment=true;
        ReflectionSourceStageTimings source_timings(native_source_frame_owner_only
            && std::getenv("STARFOX_TRACE_SOURCE_STAGE_TIMESTAMPS"));
        const auto finish=[&](DisplayXrSubmission outcome) {
            for(unsigned attempt=0;outcome==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);outcome=renderer.poll();}
            // Report before assertion unwinding: a driver fault in pending
            // cleanup must not erase the original completion failure. This is
            // fixture-only logging, not another poll, wait or extended budget.
            if(native_source_frame_owner_only && outcome!=DisplayXrSubmission::submitted)
                std::cerr<<"Source owner completion failed outcome="<<unsigned(outcome)
                    <<" pending="<<renderer.frame_pending()<<" status="<<renderer.status()<<std::endl;
            if(outcome!=DisplayXrSubmission::submitted && source_timings.active()) {
                // Preserve the ORIGINAL failed outcome/deadline. This is the
                // same blocking cleanup the owner would do during unwinding,
                // not more acceptance polls or a successful frame retry.
                const bool retired=renderer.close();
                if(retired)source_timings.retired(std::cerr,"original-deadline-failed-cleanup",
                    SUCCEEDED(gpu_mock.native->GetDeviceRemovedReason()));
                throw std::runtime_error("Original source owner completion deadline FAILED; cleanup timing is diagnostic only");
            }
            check(outcome==DisplayXrSubmission::submitted,renderer.status().c_str());
            if(source_timings.active())source_timings.retired(std::cout,"exact-submission-complete");
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());auto& read=gpu_mock.readback;
            check(read.pending,"Ray owner did not submit complete images");std::array<std::vector<unsigned char>,2> pixels;
            for(unsigned eye=0;eye<2;++eye) {
                void* mapped{};const D3D12_RANGE range{0,SIZE_T(read.footprints[eye].Footprint.RowPitch)*mock.image_height};
                check(SUCCEEDED(read.buffers[eye]->Map(0,&range,&mapped)),"Ray compositor map failed");pixels[eye].resize(mock.image_width*mock.image_height*4);
                for(unsigned row=0;row<mock.image_height;++row) std::memcpy(pixels[eye].data()+row*mock.image_width*4,
                    static_cast<unsigned char*>(mapped)+row*read.footprints[eye].Footprint.RowPitch,mock.image_width*4);
                const D3D12_RANGE no_write{};read.buffers[eye]->Unmap(0,&no_write);
                if(format==DXGI_FORMAT_B8G8R8A8_UNORM || format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
                    for(unsigned at=0;at<pixels[eye].size();at+=4) std::swap(pixels[eye][at],pixels[eye][at+2]);
            }
            read.pending=false;gpu_mock.retired_images.clear();return pixels;
        };
        const auto plain=finish(renderer.submit(source));unsigned protected_ink=0;bool camera_changed=false;
        if(native_taa_only || native_scene_fx_only || native_fog_only || native_blur_only || native_ground_only || native_fsr_only || native_dlss_only) {
            std::shared_ptr<const CalibratedGameFrame> effects;
            if(native_dlss_panel_bound_only) native_reconstructed_post_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),"D3D12 SDK",true,true);
            else if(native_dlss_ray_samples_only) native_dlss_ray_sample_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                renderer,gpu.device,renderer.srgb_target(),"D3D12",native_dlss_ray_materials_only,native_dlss_ray_rough_only,native_dlss_ray_mixed_only,native_dlss_ray_liquids_only,native_dlss_ray_liquid_primary_edge_only,native_dlss_ray_liquid_primary_wrap_only);
            else if(native_dlss_liquid_post_only) for(unsigned model:{0U,1U}) effects=native_liquid_post_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),"D3D12",model);
            else if(native_dlss_reconstructed_post_only) native_reconstructed_post_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),"D3D12 SDK",true);
            else if(native_dlss_only) effects=native_dlss_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target());
            else if(native_fsr_only) effects=native_fsr_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                renderer,renderer.srgb_target(),true,"D3D12");
            else if(native_ground_only) effects=native_ground_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                renderer.srgb_target(),true,"D3D12");
            else if(native_blur_only) effects=native_blur_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
            else if(native_fog_only) effects=native_volumetric_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
            else if(native_scene_fx_only) effects=native_scene_fx_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
            else if(native_curved_owner_only) {
                for(unsigned quality=native_curved_msaa_owner_only?1U:0U;quality<=(native_curved_msaa_owner_only?3U:0U);++quality) {
                    check(renderer.enable_reflection_diagnostics(false),
                        "Could not reset ordered-liquid diagnostic selection between sample counts");
                    effects=native_curved_reflection_owner_checks(
                        [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                        [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),quality,native_curved_msaa_resolve_only,native_curved_lens_owner_only,native_curved_motion_owner_only,native_curved_camera_owner_only,native_curved_disocclusion_owner_only,native_source_frame_owner_only,&source_timings);
                }
            }
            else if(native_taa_reflection_only) effects=native_reflection_owner_checks(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
            else if(native_taa_ground_clock_only) native_taa_ground_clock_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},renderer.srgb_target(),"D3D12");
            else if(native_taa_local_layers_only) native_taa_local_layers_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
            else if(native_taa_edge_only) native_taa_edge_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
            else if(native_taa_liquid_post_only) effects=native_liquid_post_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),"D3D12");
            else if(native_taa_reconstructed_post_only) native_reconstructed_post_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),"D3D12");
            else if(native_taa_pattern_only) native_taa_pattern_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
            else if(native_taa_palette_only) native_taa_palette_owner_oracle(
                [&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
            else native_taa_owner_oracle([&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
                [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12",native_taa_water_only);
            auto held=std::make_shared<CalibratedGameFrame>(*source);held->settings.aa_type=4;held->settings.aa_quality=3;
            if(effects) held=std::make_shared<CalibratedGameFrame>(*effects);
            const auto wait_seed=held;
            if(native_blur_only) {
                held=std::make_shared<CalibratedGameFrame>(*held);held->presentation_seconds+=1./120;
                held->draws[1].packet.model[12]+=.13F;
            }
            const auto expected=finish(renderer.submit(held));
            if(native_blur_only) (void)finish(renderer.submit(wait_seed));
            {
                QueueGate gate(gpu);check(renderer.submit(held)==DisplayXrSubmission::waiting,"TAA GPU wait released working images");
                const auto locates=native_mock.locates;
                check(renderer.poll()==DisplayXrSubmission::waiting && renderer.retained_frame()==held
                    && native_mock.locates==locates && !renderer.enable_curved_reflection_validation(false),
                    "Native effect retry reacquired cameras/source or changed curved validation while pending");
                gate.unblock();check(finish(renderer.poll())==expected,"TAA GPU wait changed held eyes");
            }
            {
                auto cancelled=held;
                if(native_blur_only) {cancelled=std::make_shared<CalibratedGameFrame>(*held);cancelled->presentation_seconds+=1./120;cancelled->draws[1].packet.model[12]+=.13F;}
                QueueGate gate(gpu);check(renderer.submit(cancelled)==DisplayXrSubmission::waiting,"Native effect cancellation did not retain pending images");
                check(!renderer.try_close() && renderer.retained_frame()==cancelled,"Native effect in-flight close freed its source/history");
                gate.unblock();bool closed=false;
                for(unsigned attempt=0;!closed && attempt<10000;++attempt) {closed=renderer.try_close();if(!closed) SDL_Delay(1);}
                check(closed && !renderer.retained_frame() && !renderer.frame_pending(),"TAA close did not retire pending images");
            }
            std::cout<<"D3D12 TAA owner: real GPU stall/retry and in-flight nonblocking cancellation retired without reacquiring poses or releasing live history.\n";
            check(renderer.close(),renderer.status().c_str());runtime.close();continue;
        }
        bool aa_changed=false;
        for(unsigned type:{0U,1U,2U,5U}) for(unsigned quality:{1U,2U,3U}) {
            auto selected=std::make_shared<CalibratedGameFrame>(*source);
            selected->settings.aa_type=type;selected->settings.aa_quality=quality;
            const auto actual=finish(renderer.submit(selected));
            aa_changed|=actual!=plain;
            check(actual[0]!=actual[1],"Native AA collapsed calibrated eyes");
            for(unsigned e=0;e<2;++e) for(unsigned at=0;at<actual[e].size();at+=4) {
                check(actual[e][at+3]==plain[e][at+3],"Native owned AA changed opacity");
                const bool ink=plain[e][at]==255 && plain[e][at+2]==0 && (plain[e][at+1]==0 || plain[e][at+1]==255);
                if(ink) check(std::equal(actual[e].begin()+at,actual[e].begin()+at+4,plain[e].begin()+at),"Native owned AA blurred HUD/emissive ink");
            }
            check(finish(renderer.submit(selected))==actual,"Held native AA wobbled or resampled the eyes");
            if(type==5 && quality==3) {
                gpu_mock.timeout_right=true;check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Native AA wait lost retained source");
                check(renderer.poll()==DisplayXrSubmission::waiting,"Native AA wait resubmitted/acquired eyes");
                gpu_mock.timeout_right=false;check(finish(renderer.poll())==actual,"Native AA retry changed held output");
                selected->settings.depth_enhancements=15;selected->settings.ray_tracing=2;selected->settings.reflections=2;
                selected->settings.bloom_model=2;selected->settings.contrast=1;selected->settings.global_enhancements=global_enhancement_mask;
                check(finish(renderer.submit(selected))!=actual,"Combined native AA/depth/rays/appearance bypassed passes");
            }
            check(finish(renderer.submit(source))==plain,"Native AA OFF failed exact restoration");
        }
        check(aa_changed,"Native AA settings never changed the actual eye-owner image");
        std::cout<<"Native AA owner format "<<unsigned(format)<<": all 4 spatial algorithms/3 qualities, visible smoothing, native eyes, exact ink/alpha, held waits/OFF and combined depth/rays/appearance passed.\n";
        bool ssaa_changed=false;
        for(unsigned quality:{1U,2U,3U}) {
            auto selected=std::make_shared<CalibratedGameFrame>(*source);
            selected->settings.aa_type=3;selected->settings.aa_quality=quality;
            const auto actual=finish(renderer.submit(selected));ssaa_changed|=actual!=plain;
            check(actual[0]!=actual[1],"Native SSAA flattened independent calibrated eyes");
            unsigned fractional_edges=0;
            for(unsigned e=0;e<2;++e) for(unsigned at=0;at<actual[e].size();at+=4) {
                check(actual[e][at+3]==plain[e][at+3],"Native SSAA changed alpha");
                const bool ink=plain[e][at]==255 && plain[e][at+2]==0 && (plain[e][at+1]==0 || plain[e][at+1]==255);
                if(ink) check(std::equal(actual[e].begin()+at,actual[e].begin()+at+4,plain[e].begin()+at),"Native SSAA blurred HUD/emissive native coverage");
                else fractional_edges+=!std::equal(actual[e].begin()+at,actual[e].begin()+at+3,plain[e].begin()+at);
            }
            check(fractional_edges>0,"Native SSAA did not produce fractional silhouette coverage");
            check(finish(renderer.submit(selected))==actual,"Held native SSAA jittered");
            gpu_mock.timeout_right=true;
            check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Native SSAA wait lost source");
            check(renderer.poll()==DisplayXrSubmission::waiting,"Native SSAA pending frame reacquired/resampled eyes");
            gpu_mock.timeout_right=false;check(finish(renderer.poll())==actual,"Native SSAA retry changed completed eyes");
            if(quality==2) {
                selected->settings.depth_enhancements=15;selected->settings.ray_tracing=selected->settings.reflections=2;
                selected->settings.bloom_model=2;selected->settings.contrast=2;
                selected->settings.global_enhancements=global_enhancement_mask;
                check(finish(renderer.submit(selected))!=actual,"Native SSAA bypassed combined ray/depth/appearance effects");
                QueueGate gate(gpu);
                check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Native SSAA GPU stall released working images");
                gate.unblock();(void)finish(renderer.poll());
            }
            check(finish(renderer.submit(source))==plain,"Native SSAA OFF did not restore exact native extent/image");
        }
        check(ssaa_changed,"Native SSAA was a no-op");
        native_msaa_owner_oracle([&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
            [&]{return finish(DisplayXrSubmission::submitted);},renderer,source,true,"D3D12");
        native_taa_owner_oracle([&](std::shared_ptr<const CalibratedGameFrame> frame){return finish(renderer.submit(std::move(frame)));},
            [&]{return finish(DisplayXrSubmission::submitted);},renderer,renderer.srgb_target(),true,"D3D12");
        for(unsigned algorithm:{3U,6U}) {
            auto enhanced=std::make_shared<CalibratedGameFrame>(*source);
            enhanced->settings.aa_type=algorithm;enhanced->settings.aa_quality=3;
            enhanced->settings.contrast=2;enhanced->settings.bloom_model=3;
            const auto finished=finish(renderer.submit(enhanced));
            auto overlay=std::make_shared<CalibratedGameFrame>(*enhanced);
            auto panel=source->draws[0];panel.after_rays=true;panel.ray_caster=false;panel.depth_test=false;
            panel.layer=CalibratedGameLayer::native_ui;panel.packet.preserve_native_colour=true;panel.blend=starfox::vr::SceneBlend::alpha;
            for(auto& v:panel.packet.geometry.vertices) {
                v.position[0]*=.35F;v.position[1]*=.35F;v.position[2]=-.8F;
                v.color[0]=v.color[1]=v.color[2]=v.odd_color[0]=v.odd_color[1]=v.odd_color[2]=0;
                v.color[3]=v.odd_color[3]=.75F;
            }
            overlay->draws.push_back(panel);
            auto labels=std::make_shared<CalibratedGameFrame>(*source);labels->clear={0,0,0,1};
            for(auto& draw:labels->draws) for(auto& v:draw.packet.geometry.vertices)
                for(unsigned c=0;c<3;++c) v.color[c]=v.odd_color[c]=0;
            panel.blend=starfox::vr::SceneBlend::opaque;
            for(auto& v:panel.packet.geometry.vertices) {v.color[1]=v.odd_color[1]=1;v.color[3]=v.odd_color[3]=1;}
            labels->draws.push_back(panel);
            const auto coverage=finish(renderer.submit(labels));
            const auto actual=finish(renderer.submit(overlay));unsigned covered=0;
            const auto decode=[](double c) {return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
            const auto encode=[](double c) {return c<=.0031308?12.92*c:1.055*std::pow(c,1/2.4)-.055;};
            for(unsigned e=0;e<2;++e) for(unsigned at=0;at<actual[e].size();at+=4) {
                if(coverage[e][at+1]!=255) {check(std::equal(actual[e].begin()+at,actual[e].begin()+at+4,finished[e].begin()+at),"Native SSAA menu overlay changed pixels outside native coverage");continue;}
                ++covered;
                for(unsigned c=0;c<3;++c) {
                    const double value=finished[e][at+c]/255.;
                    const auto expected=std::lround((source->settings.srgb?encode(decode(value)*.25):value*.25)*255);
                    check(std::abs(int(actual[e][at+c])-int(expected))<=1,"Native SSAA translucent UI restored an unenhanced background instead of dimming the finished scene");
                }
                check(actual[e][at+3]==finished[e][at+3],"Native SSAA translucent UI changed opacity");
            }
            check(covered>0,"SSAA translucent overlay oracle did not cover pixels");
            check(finish(renderer.submit(overlay))==actual,"Native SSAA translucent preview panel wobbled");
            check(finish(renderer.submit(source))==plain,"Native SSAA translucent overlay OFF failed restoration");
            std::cout<<"Native "<<(algorithm==3?"SSAA":"MSAA")<<" overlay format "<<unsigned(format)<<": "<<covered<<" native-coverage pixels dim the actual finished enhanced eyes, with exact outside/alpha/held/OFF.\n";
        }
        std::cout<<"Native SSAA owner format "<<unsigned(format)<<": all 3 raster scales, fractional edges, actual eyes, native ink/alpha, held/image/GPU waits, ray/depth/appearance and OFF extent restoration passed.\n";
        {
            auto depth_source=std::make_shared<CalibratedGameFrame>(*source);
            for(auto& v:depth_source->draws.front().packet.geometry.vertices) {
                v.dither_scale=1;v.odd_color[0]=.7F;v.odd_color[1]=.2F;v.odd_color[2]=.4F;
            }
            const auto reference=finish(renderer.submit(depth_source));bool changed=false;
            for(unsigned modes=1;modes<16;++modes) {
                auto selected=std::make_shared<CalibratedGameFrame>(*depth_source);
                selected->settings.depth_enhancements=modes;
                const auto actual=finish(renderer.submit(selected));changed|=actual!=reference;
                check(actual[0]!=actual[1],"Native depth controls collapsed calibrated eyes");
                for(unsigned e=0;e<2;++e) for(unsigned at=0;at<actual[e].size();at+=4) {
                    check(actual[e][at+3]==reference[e][at+3],"Native owned depth effect changed source opacity");
                    const bool ink=reference[e][at]==255 && reference[e][at+2]==0
                        && (reference[e][at+1]==0 || reference[e][at+1]==255);
                    if(ink) check(std::equal(actual[e].begin()+at,actual[e].begin()+at+4,reference[e].begin()+at),
                        "Native owned depth effect blurred/darkened emissive or HUD ink");
                }
                check(finish(renderer.submit(selected))==actual,"Held native depth controls resampled or jittered");
                if(modes==15) {
                    gpu_mock.timeout_right=true;
                    check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,
                        "Native depth compositor wait lost retained guide/source state");
                    check(renderer.poll()==DisplayXrSubmission::waiting,"Native depth compositor wait reacquired/resampled eyes");
                    gpu_mock.timeout_right=false;check(finish(renderer.poll())==actual,"Native depth retry changed retained surfaces");
                    QueueGate gate(gpu);
                    check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,
                        "Native depth queue wait lost retained source/guide images");
                    gate.unblock();check(finish(renderer.poll())==actual,"Native depth GPU retry changed output");
                    selected->settings.ray_tracing=selected->settings.reflections=3;
                    selected->settings.material=unsigned(Effect::gold_metal);
                    selected->settings.bloom_model=selected->settings.bloom_world=2;
                    selected->settings.contrast=selected->settings.chromatic=2;
                    selected->settings.global_enhancements=global_enhancement_mask;
                    selected->settings.camera_response_pose={.008,-.012,.007};
                    selected->settings.camera_response_modes=63;
                    const auto combined=finish(renderer.submit(selected));
                    check(combined[0]!=combined[1] && combined!=actual,"Combined native depth/ray/global/bloom/response bypassed output");
                }
                check(finish(renderer.submit(depth_source))==reference,"Native owned depth OFF failed exact restoration");
            }
            check(changed,"Native depth menu settings did not reach the actual eye owner");
            std::cout<<"Native depth owner format "<<unsigned(format)<<": all AO/DOF modes, calibrated eyes, exact HUD/emissive/alpha, retained image/GPU waits, ray/material/global/bloom/response combination and OFF passed.\n";
        }
        for(unsigned scope:{0U,1U,2U}) for(unsigned quality:{1U,2U,3U}) for(bool combined:{false,true}) {
            CameraResponse tracker;const unsigned modes=quality<<(scope*2);
            (void)tracker.update(0,1,modes,100,0,1);
            (void)tracker.update(.1,1,modes,68,1,1);
            const auto pose=tracker.update(.15,1,modes,68,1,1);
            auto selected=std::make_shared<CalibratedGameFrame>(*source);
            selected->settings.camera_response_modes=modes;selected->settings.camera_response_pose={pose.pitch,pose.yaw,pose.roll};
            if(combined) {
                selected->settings.ray_tracing=selected->settings.reflections=3;
                selected->settings.material=unsigned(Effect::gold_metal);
                selected->settings.bloom_world=2;selected->settings.bloom_model=3;
                selected->settings.contrast=selected->settings.chromatic=2;
                selected->settings.global_enhancements=global_enhancement_mask;
            }
            const auto actual=finish(renderer.submit(selected));
            const auto reference=response_reference_frame(selected);
            check(actual==finish(renderer.submit(reference)),"Native camera response differs from independent world/model Euler oracle");
            check(actual[0]!=actual[1],"Native camera response collapsed calibrated eyes");camera_changed|=!combined && actual!=plain;
            check(finish(renderer.submit(selected))==actual,"Held native camera response moved or jittered");
            const auto packets=selected->packets();
            const auto reference_packets=reference->packets();
            for(unsigned i=0;i<packets.size();++i) {
                const auto& draw=selected->draws[i];
                const bool moved=!draw.after_rays && (draw.layer==CalibratedGameLayer::world || draw.layer==CalibratedGameLayer::model);
                check(bool(packets[i].model_override)==moved,"Native response moved UI/screen or failed to move emissive world geometry");
                const auto& camera=packets[i].camera_override;const auto& expected_camera=reference_packets[i].camera_override;
                check(bool(camera)==bool(expected_camera) && (!camera || (camera->view==expected_camera->view
                    && camera->projection==expected_camera->projection && camera->effects==expected_camera->effects)),"Native response replaced tracked/explicit eye camera");
            }
            if(scope==2 && quality==3 && combined) {
                gpu_mock.timeout_right=true;check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Camera response image wait lost frame/pose");
                check(renderer.poll()==DisplayXrSubmission::waiting,"Camera response image wait reacquired images");
                gpu_mock.timeout_right=false;check(finish(renderer.poll())==actual,"Camera response retry resampled events/pose");
                QueueGate gate(gpu);check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Camera response queue wait lost source/pose");
                gate.unblock();check(finish(renderer.poll())==actual,"Camera response GPU retry changed source transform");
            }
            check(finish(renderer.submit(source))==plain,"Native camera response OFF failed exact restoration");
        }
        check(camera_changed,"Native camera-response geometry never moved");
        std::cout<<"Native camera response owner format "<<unsigned(format)<<": impact/recoil/bank LOW/MED/HIGH, independent Euler world/model oracle, unchanged tracked views/UI, emissive geometry, ray/material/environment/appearance/global/bloom combinations, held phase and image/GPU retries passed.\n";
        std::array<std::vector<unsigned char>,2> traced;
        for(unsigned quality:{1U,2U,3U}) {
            auto ray_source=std::make_shared<CalibratedGameFrame>(*source);
            ray_source->settings.ray_tracing=ray_source->settings.reflections=quality;
            traced=finish(renderer.submit(ray_source));
            check(traced[0]!=traced[1],"Native ray owner collapsed both calibrated eyes");
            for(unsigned eye=0;eye<2;++eye) {
                unsigned changed=0;
                for(unsigned at=0;at<plain[eye].size();at+=4) {
                    const bool ink=plain[eye][at]==255 && plain[eye][at+2]==0 && (plain[eye][at+1]==0 || plain[eye][at+1]==255);
                    if(ink) {check(std::equal(plain[eye].begin()+at,plain[eye].begin()+at+4,traced[eye].begin()+at),
                        "Native ray owner modified HUD/emissive ink");++protected_ink;}
                    changed+=!std::equal(plain[eye].begin()+at,plain[eye].begin()+at+4,traced[eye].begin()+at);
                }
                check(changed>20,"Native RT/reflections option had no visible model effect");
            }
        }
        check(protected_ink>30,"Native ray owner fixture did not exercise protected ink");
        unsigned styles=0;
        for(unsigned style=0;style<starfox::render::effect_count;++style) {
            if(!starfox::render::calibrated_post_effect(style) && !starfox::render::calibrated_palette_effect(style)) continue;
            ++styles;
            for(unsigned selection:{0U,1U,2U}) {
                auto selected=std::make_shared<CalibratedGameFrame>(*source);
                if(selection!=1) selected->settings.world_effects={style,100,0,0};
                if(selection!=2) selected->settings.model_effects={style,100,0,0};
                const auto descriptors=selected->packets();
                for(unsigned index=0;index<descriptors.size();++index) {
                    const auto& draw=selected->draws[index];
                    const unsigned expected=draw.after_rays || draw.packet.preserve_native_colour?0:
                        draw.layer==CalibratedGameLayer::world?1:draw.layer==CalibratedGameLayer::model && draw.ray_caster?2:0;
                    check(descriptors[index].effect_layer==expected,"Native effect descriptor lost independent source ownership");
                }
                const auto actual=finish(renderer.submit(selected));
                const auto detail=std::string("Native drawn style ")+std::string(starfox::render::effect_names[style])
                    +" selection="+std::to_string(selection);
                check(actual[0]!=plain[0] && actual[1]!=plain[1] && actual[0]!=actual[1],
                    (detail+" did not affect its selected source layer without RT").c_str());
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<plain[eye].size();at+=4) {
                    const bool ink=plain[eye][at]==255 && plain[eye][at+2]==0 && (plain[eye][at+1]==0 || plain[eye][at+1]==255);
                    if(ink) check(std::equal(plain[eye].begin()+at,plain[eye].begin()+at+4,actual[eye].begin()+at),
                        "Native drawn style changed protected HUD/emissive ink");
                    check(actual[eye][at+3]==plain[eye][at+3],"Native drawn style changed source opacity");
                }
                selected->settings.model_effects[1]=selected->settings.world_effects[1]=0;
                check(finish(renderer.submit(selected))==plain,"Zero native style intensity retained old colour/output");
            }
        }
        check(styles==47 && finish(renderer.submit(source))==plain,"Native drawn/palette fixture omitted a selection or failed OFF restoration");
        for(unsigned selection=0;selection<5;++selection) {
            auto selected=std::make_shared<CalibratedGameFrame>(*source);
            selected->settings.effect_seconds=.375F;
            if(selection==0 || selection==4) selected->settings.manipulation=unsigned(starfox::render::Effect::prism_split);
            if(selection==1 || selection==4) selected->settings.extra_effects[0]=unsigned(starfox::render::Effect::heat_wake);
            if(selection==2 || selection==4) selected->settings.extra_effects[1]=unsigned(starfox::render::Effect::energy_shield);
            if(selection==3 || selection==4) selected->settings.extra_effects[2]=unsigned(starfox::render::Effect::arc_lightning);
            if(selection==4) {
                selected->settings.world_effects={unsigned(starfox::render::Effect::sepia),35,0,0};
                selected->settings.model_effects={unsigned(starfox::render::Effect::posterized),100,0,0};
                selected->settings.ray_tracing=selected->settings.reflections=2;
                selected->settings.material=unsigned(starfox::render::Effect::silver);
            }
            const auto actual=finish(renderer.submit(selected));
            check(actual[0]!=plain[0] && actual[1]!=plain[1] && actual[0]!=actual[1],
                "Native distortion/special-FX category bypassed or flattened its selected eye layer");
            check(finish(renderer.submit(selected))==actual,"Retained native animation time jittered");
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<plain[eye].size();at+=4) {
                const bool ink=plain[eye][at]==255 && plain[eye][at+2]==0 && (plain[eye][at+1]==0 || plain[eye][at+1]==255);
                if(ink) check(std::equal(plain[eye].begin()+at,plain[eye].begin()+at+4,actual[eye].begin()+at),
                    "Native distortion/special-FX sequence changed protected HUD/emissive ink");
                check(actual[eye][at+3]==plain[eye][at+3],"Native distortion/special-FX sequence changed source opacity");
            }
            check(finish(renderer.submit(source))==plain,"Native effect categories OFF failed exact restoration");
        }
        // Collect independent native raster references and geometric layer
        // labels, then replay a moving sequence through temporal presentation.
        // The oracle never reads the renderer's private receiver/history image.
        using EyeBytes=std::array<std::vector<unsigned char>,2>;
        std::vector<std::shared_ptr<CalibratedGameFrame>> temporal_sources;
        std::vector<EyeBytes> raw_temporal,label_temporal;
        const auto native_channel=[&](float encoded) {
            return !source->settings.srgb?encoded:encoded<=.04045F?encoded/12.92F:std::pow((encoded+.055F)/1.055F,2.4F);
        };
        for(unsigned frame=0;frame<18;++frame) {
            auto moving=std::make_shared<CalibratedGameFrame>(*source);
            auto tick=std::make_shared<starfox::vr::GameSceneSnapshot>(*source->current);
            tick->revision=frame;tick->scene_epoch=frame>=6?2:1;
            tick->flow=frame>=8?starfox::simulation::GameFlowState::stage_results:starfox::simulation::GameFlowState::gameplay;
            tick->camera.x=frame>=9?6000:0;tick->paused=frame==11;
            moving->current=moving->previous=tick;
            moving->settings.effect_seconds=frame==11?10.F/60:float(frame)/60;
            if(frame==14) moving->settings.effect_seconds+=2;
            if(frame==15) moving->settings.effect_seconds=.01F;
            moving->settings.history_epoch=frame>=16?2:1;
            const double response_frame=frame==11?10:frame;
            moving->settings.camera_response_modes=63;
            moving->settings.camera_response_pose={response_frame*.0007,-response_frame*.0004,response_frame*.001};
            for(auto& v:moving->draws[0].packet.geometry.vertices) v.position[0]+=.2F*float(frame==11?10:frame);
            for(auto& v:moving->draws[4].packet.geometry.vertices) for(unsigned c=0;c<3;++c)
                v.color[c]=v.odd_color[c]=native_channel(float(220-((frame==11?10:frame)*13)%200)/255);
            if(frame==4) {
                moving->draws[3]=moving->draws[0];moving->draws[3].after_rays=true;
                moving->draws[3].ray_caster=false;moving->draws[3].depth_test=false;
                for(auto& v:moving->draws[3].packet.geometry.vertices) {
                    v.position[0]-=.6F;v.position[2]=-.7F;
                    v.color[0]=v.odd_color[0]=1;v.color[1]=v.odd_color[1]=1;v.color[2]=v.odd_color[2]=0;
                }
            }
            auto labels=std::make_shared<CalibratedGameFrame>(*moving);
            labels->clear={0,0,native_channel(1.F/255),1};
            for(auto& draw:labels->draws) {
                const unsigned group=draw.after_rays || draw.packet.preserve_native_colour?0:
                    draw.layer==CalibratedGameLayer::world?1:draw.layer==CalibratedGameLayer::model && draw.ray_caster?2:0;
                for(auto& v:draw.packet.geometry.vertices) {
                    v.color[0]=v.color[1]=v.odd_color[0]=v.odd_color[1]=0;
                    v.color[2]=v.odd_color[2]=native_channel(float(group)/255);
                }
            }
            raw_temporal.push_back(finish(renderer.submit(moving)));
            label_temporal.push_back(finish(renderer.submit(labels)));
            temporal_sources.push_back(moving);
        }
        unsigned long long aa_order_samples=0;bool aa_order_changed=false;
        for(unsigned frame:{0U,4U,11U}) for(unsigned combination:{0U,1U,2U}) {
            auto selected=std::make_shared<CalibratedGameFrame>(*temporal_sources[frame]);
            selected->settings.contrast=combination?2:0;selected->settings.chromatic=combination?1:0;
            selected->settings.global_enhancements=combination==2?global_enhancement_mask:0;
            selected->settings.bloom_model=combination?3:0;selected->settings.bloom_world=combination?2:0;
            const auto input=finish(renderer.submit(selected));
            for(unsigned type:{0U,1U,2U}) for(unsigned quality:{1U,2U,3U}) {
                selected->settings.aa_type=type;selected->settings.aa_quality=quality;
                const auto actual=finish(renderer.submit(selected));aa_order_changed|=actual!=input;
                for(unsigned eye=0;eye<2;++eye) {
                    Framebuffer labels(mock.image_width,mock.image_height);labels.enable_layer_tags(true);
                    for(unsigned at=0;at<label_temporal[frame][eye].size();at+=4)
                        labels.layer_tags()[at/4]=unsigned(label_temporal[frame][eye][at+2]==2?PixelLayer::three_d:PixelLayer::two_d);
                    const auto expected=native_spatial_aa_oracle(input[eye],labels,type,quality);
                    for(unsigned at=0;at<actual[eye].size();++at) {
                        const bool exact=at%4==3 || labels.layer_tags()[at/4]==unsigned(PixelLayer::two_d);
                        check(std::abs(int(actual[eye][at])-int(expected[at]))<=unsigned(exact?0:1),"Native AA owner differs from finished-effect image/order oracle");
                        ++aa_order_samples;
                    }
                }
            }
        }
        check(aa_order_changed,"Native AA order oracle did not exercise a changed image");
        std::cout<<"Native AA post-effect order format "<<unsigned(format)<<": "<<aa_order_samples<<" byte-oracle checks; 3 spatial types/qualities, geometric labels, appearance/global/bloom-before-AA, UI/alpha and held source passed.\n";
        // Contrast/channel separation consume the actual independent eye
        // images and the geometric ownership oracle, never a flat SBS image.
        for(unsigned contrast=0;contrast<4;++contrast) for(unsigned chromatic=0;chromatic<4;++chromatic)
            for(unsigned frame:{0U,4U,11U}) for(bool combined:{false,true}) {
            auto selected=std::make_shared<CalibratedGameFrame>(*temporal_sources[frame]);
            selected->settings.contrast=contrast;selected->settings.chromatic=chromatic;
            selected->settings.global_enhancements=combined?global_enhancement_mask:0;
            selected->settings.bloom_model=combined?3:0;selected->settings.bloom_world=combined?2:0;
            const auto actual=finish(renderer.submit(selected));
            for(unsigned eye=0;eye<2;++eye) {
                Framebuffer ownership(mock.image_width,mock.image_height);ownership.enable_layer_tags(true);
                for(unsigned at=0;at<label_temporal[frame][eye].size();at+=4) {
                    const auto group=label_temporal[frame][eye][at+2];
                    ownership.layer_tags()[at/4]=unsigned(group==0?PixelLayer::two_d:group==1?PixelLayer::background:PixelLayer::three_d);
                }
                auto expected=raw_temporal[frame][eye];std::vector<unsigned char> scratch;
                apply_hdr_effect(ownership,expected,static_cast<unsigned char>(contrast));
                apply_chromatic_aberration(ownership,expected,scratch,static_cast<unsigned char>(chromatic));
                apply_global_enhancements(selected->settings.global_enhancements,ownership,expected,scratch,selected->settings.effect_seconds);
                BloomPass oracle;oracle.apply(selected->settings.bloom_model,selected->settings.bloom_world,ownership,expected);
                for(unsigned at=0;at<actual[eye].size();at+=4) {
                    if(label_temporal[frame][eye][at+2]==0) check(std::equal(raw_temporal[frame][eye].begin()+at,
                        raw_temporal[frame][eye].begin()+at+4,actual[eye].begin()+at),"Native appearance altered protected UI/emissive ink");
                    for(unsigned c=0;c<3;++c) check(std::abs(int(actual[eye][at+c])-expected[at+c])<=unsigned(combined?2:1),
                        "Native appearance owner differs from independent per-eye flat quality/order oracle");
                    check(actual[eye][at+3]==raw_temporal[frame][eye][at+3],"Native appearance owner changed source alpha");
                }
            }
            if(frame==11) check(finish(renderer.submit(selected))==actual,"Held native appearance shimmered");
        }
        check(finish(renderer.submit(source))==plain,"Native appearance OFF failed exact source restoration");
        std::cout<<"Native appearance owner format "<<unsigned(format)<<": all 16 contrast/chromatic pairs, independent eye/geometry CPU oracle, appearance-before-global-before-bloom order, protected taps/world/UI/alpha, held frame and exact OFF passed.\n";
        // Bloom uses the independently rasterized source/ownership below,
        // not a readback of the native renderer's private receiver or halos.
        for(unsigned model=0;model<4;++model) for(unsigned world=0;world<4;++world)
            for(unsigned frame:{0U,4U,11U}) for(bool globals:{false,true}) {
            auto selected=std::make_shared<CalibratedGameFrame>(*temporal_sources[frame]);
            selected->settings.bloom_model=model;selected->settings.bloom_world=world;
            selected->settings.global_enhancements=globals?global_enhancement_mask:0;
            const auto actual=finish(renderer.submit(selected));
            for(unsigned eye=0;eye<2;++eye) {
                Framebuffer ownership(mock.image_width,mock.image_height);ownership.enable_layer_tags(true);
                for(unsigned at=0;at<label_temporal[frame][eye].size();at+=4) {
                    const auto group=label_temporal[frame][eye][at+2];
                    ownership.layer_tags()[at/4]=unsigned(group==0?PixelLayer::two_d:group==1?PixelLayer::background:PixelLayer::three_d);
                }
                auto expected=raw_temporal[frame][eye];std::vector<unsigned char> scratch;
                apply_global_enhancements(selected->settings.global_enhancements,ownership,expected,scratch,selected->settings.effect_seconds);
                BloomPass oracle;oracle.apply(model,world,ownership,expected);
                for(unsigned at=0;at<actual[eye].size();at+=4) {
                    if(label_temporal[frame][eye][at+2]==0) check(std::equal(raw_temporal[frame][eye].begin()+at,
                        raw_temporal[frame][eye].begin()+at+4,actual[eye].begin()+at),"Native bloom altered protected UI/emissive ink");
                    for(unsigned c=0;c<3;++c) check(std::abs(int(actual[eye][at+c])-expected[at+c])<=1,
                        "Native bloom owner differs from per-eye flat quality/global-order oracle");
                    check(actual[eye][at+3]==raw_temporal[frame][eye][at+3],"Native bloom owner changed source alpha");
                }
            }
            if(frame==11) check(finish(renderer.submit(selected))==actual,"Held native bloom frame shimmered");
        }
        check(finish(renderer.submit(source))==plain,"Native bloom OFF failed exact source restoration");
        std::cout<<"Native bloom owner format "<<unsigned(format)<<": all 16 2D/3D quality pairs, independent eye/geometry CPU oracle, global-before-bloom order, protected/overlapping UI/alpha, held frame and exact OFF passed.\n";
        // Global effects use this independent geometric ownership oracle as
        // well, not a private native receiver readback. It includes bright
        // models, world, alpha, emissive/UI and an overlapping UI frame.
        for(unsigned choice=0;choice<=global_enhancement_count;++choice) for(unsigned quality:{1U,2U,3U})
            for(unsigned frame:{0U,4U,11U}) {
            auto selected=std::make_shared<CalibratedGameFrame>(*temporal_sources[frame]);
            selected->settings.global_enhancements=choice<global_enhancement_count?quality<<(choice*2):quality*0x01555555U;
            const auto actual=finish(renderer.submit(selected));
            for(unsigned eye=0;eye<2;++eye) {
                starfox::render::Framebuffer ownership(mock.image_width,mock.image_height);ownership.enable_layer_tags(true);
                for(unsigned at=0;at<label_temporal[frame][eye].size();at+=4) {
                    const auto group=label_temporal[frame][eye][at+2];
                    ownership.layer_tags()[at/4]=unsigned(group==0?starfox::render::PixelLayer::two_d:
                        group==1?starfox::render::PixelLayer::background:starfox::render::PixelLayer::three_d);
                }
                auto expected=raw_temporal[frame][eye];std::vector<unsigned char> scratch;
                apply_global_enhancements(selected->settings.global_enhancements,ownership,expected,scratch,selected->settings.effect_seconds);
                for(unsigned at=0;at<actual[eye].size();at+=4) {
                    if(label_temporal[frame][eye][at+2]==0) check(std::equal(raw_temporal[frame][eye].begin()+at,
                        raw_temporal[frame][eye].begin()+at+4,actual[eye].begin()+at),"Native global effects changed protected UI/emissive ink");
                    for(unsigned c=0;c<3;++c) check(std::abs(int(actual[eye][at+c])-expected[at+c])<=1,
                        "Native global owner differs from independent per-eye flat oracle");
                    check(actual[eye][at+3]==raw_temporal[frame][eye][at+3],"Native global effects changed opacity");
                }
            }
            if(frame==11) check(finish(renderer.submit(selected))==actual,"Held global animation phase jittered");
        }
        check(finish(renderer.submit(source))==plain,"Native global OFF failed exact source restoration");
        std::cout<<"Native global owner format "<<unsigned(format)<<": all 13 enhancements and LOW/MED/HIGH, combined selections, independent eye/geometry flat oracle, overlapping/protected UI/alpha, held phase and exact OFF passed.\n";
        // Meter actual calibrated geometry, not a synthetic flat eye upload.
        // The label geometry independently defines UI/world/model ownership.
        for(unsigned quality:{1U,2U,3U}) for(bool combined:{false,true}) {
            std::array<AdaptiveExposure,2> meters;
            std::array<FramePersistence,2> world_memory,model_memory,phosphor_memory;
            bool visible=false;
            for(unsigned frame=0;frame<temporal_sources.size();++frame) {
                auto selected=std::make_shared<CalibratedGameFrame>(*temporal_sources[frame]);
                selected->settings.exposure=quality;selected->settings.exposure_paused=frame==11;
                if(combined) {
                    selected->settings.global_enhancements=global_enhancement_mask;
                    selected->settings.bloom_model=3;selected->settings.bloom_world=2;
                    selected->settings.manipulation=unsigned(Effect::long_exposure);
                    selected->settings.world_effects={unsigned(Effect::trails),100,0,0};selected->settings.phosphor=2;
                }
                if(frame==6 || frame==8 || frame==9 || frame==16) {
                    for(auto& m:meters) m.reset();for(auto& m:world_memory) m.reset();
                    for(auto& m:model_memory) m.reset();for(auto& m:phosphor_memory) m.reset();
                }
                auto outcome=DisplayXrSubmission::failed;
                if(frame==2) {
                    gpu_mock.timeout_right=true;outcome=renderer.submit(selected);
                    check(outcome==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Exposure image wait lost source");
                    check(renderer.poll()==DisplayXrSubmission::waiting && renderer.poll()==DisplayXrSubmission::waiting,"Exposure image wait re-metered a pending frame");
                    gpu_mock.timeout_right=false;outcome=renderer.poll();
                } else if(frame==3) {
                    QueueGate gate(gpu);outcome=renderer.submit(selected);
                    check(outcome==DisplayXrSubmission::waiting && renderer.poll()==DisplayXrSubmission::waiting,"Exposure GPU wait submitted early");gate.unblock();
                } else outcome=renderer.submit(selected);
                const auto actual=finish(outcome);
                for(unsigned eye=0;eye<2;++eye) {
                    Framebuffer ownership(mock.image_width,mock.image_height);ownership.enable_layer_tags(true);
                    for(unsigned at=0;at<label_temporal[frame][eye].size();at+=4) {
                        const unsigned group=label_temporal[frame][eye][at+2];ownership.layer_tags()[at/4]=unsigned(group==0?PixelLayer::two_d:group==1?PixelLayer::background:PixelLayer::three_d);
                    }
                    auto expected=raw_temporal[frame][eye];std::vector<unsigned char> scratch;
                    if(combined) {
                        apply_global_enhancements(selected->settings.global_enhancements,ownership,expected,scratch,selected->settings.effect_seconds);
                        BloomPass bloom;bloom.apply(3,2,ownership,expected);
                        world_memory[eye].apply(ownership,expected,PersistenceMode::trails,false,true,selected->settings.effect_seconds,1,100);
                        model_memory[eye].apply(ownership,expected,PersistenceMode::long_exposure,true,false,selected->settings.effect_seconds,1,100);
                    }
                    const auto before=expected;
                    meters[eye].apply(ownership,expected,quality,selected->settings.effect_seconds,1,selected->settings.exposure_paused);
                    visible|=expected!=before;
                    if(combined) phosphor_memory[eye].apply(ownership,expected,PersistenceMode::phosphor,true,true,selected->settings.effect_seconds,1,100,2);
                    for(unsigned at=0;at<actual[eye].size();at+=4) {
                        if(label_temporal[frame][eye][at+2]==0) check(std::equal(actual[eye].begin()+at,actual[eye].begin()+at+4,raw_temporal[frame][eye].begin()+at),"Exposure changed protected UI/emissive ink");
                        for(unsigned c=0;c<3;++c) check(std::abs(int(actual[eye][at+c])-expected[at+c])<=unsigned(combined?2:1),"Native exposure owner differs from independent per-eye CPU/order oracle");
                        check(actual[eye][at+3]==raw_temporal[frame][eye][at+3],"Exposure owner changed source opacity");
                    }
                }
                if(frame==11) check(finish(renderer.submit(selected))==actual,"Paused native exposure kept adapting");
            }
            check(visible,"Actual native owner bypassed adaptive exposure");
            check(finish(renderer.submit(source))==plain,"Native exposure OFF failed exact restoration");
        }
        {
            const auto exposure_frame=[&](unsigned i) {auto f=std::make_shared<CalibratedGameFrame>(*temporal_sources[i]);f->settings.exposure=3;return f;};
            (void)finish(renderer.submit(exposure_frame(0)));(void)finish(renderer.submit(exposure_frame(1)));
            gpu_mock.fail_end=true;auto outcome=renderer.submit(exposure_frame(2));
            for(unsigned i=0;outcome==DisplayXrSubmission::waiting && i<3000;++i) {SDL_Delay(1);outcome=renderer.poll();}
            check(outcome==DisplayXrSubmission::failed && !renderer.retained_frame(),"Rejected frame committed native exposure");
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());gpu_mock.readback.pending=false;gpu_mock.retired_images.clear();
            const auto fresh=finish(renderer.submit(exposure_frame(3)));
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<fresh[eye].size();++at)
                check(std::abs(int(fresh[eye][at])-raw_temporal[3][eye][at])<=unsigned(at%4==3?0:1),"Rejected native presentation seeded stale exposure stops");
        }
        std::cout<<"Native exposure owner format "<<unsigned(format)<<": LOW/MED/HIGH, independent geometry/eye flat meter, global/bloom/trails-before-exposure-before-phosphor order, UI/alpha, pause/cut/flow/camera/state resets, rejected presentation, image/GPU waits and exact OFF passed.\n";
        for(unsigned mode:{1U,2U,3U,4U,5U}) for(unsigned scope:{0U,1U,2U}) {
            check(finish(renderer.submit(source))==plain,"Temporal test failed to restore OFF");
            std::array<starfox::render::FramePersistence,2> world_memory,model_memory,phosphor_memory;
            const unsigned phosphor=mode>=3?mode-2:0,model_mode=phosphor?1:mode;
            const bool have_model=phosphor?scope!=0:scope!=1,have_world=phosphor?scope==2:scope!=0;
            bool observed_model=false,observed_world=false;
            for(unsigned frame=0;frame<temporal_sources.size();++frame) {
                auto selected=std::make_shared<CalibratedGameFrame>(*temporal_sources[frame]);
                const auto effect=model_mode==1?starfox::render::Effect::trails:starfox::render::Effect::long_exposure;
                const unsigned world_mode=phosphor?2:scope==2?3-mode:mode;
                if(have_model) selected->settings.manipulation=unsigned(effect);
                if(have_world) selected->settings.world_effects={unsigned(world_mode==1?starfox::render::Effect::trails:starfox::render::Effect::long_exposure),100,0,0};
                selected->settings.phosphor=frame==12?0:phosphor;
                if(phosphor && scope==2) selected->settings.global_enhancements=global_enhancement_mask;
                selected->settings.manipulation_intensity=frame==12?0:100;
                selected->settings.world_effects[1]=frame==12?0:100;
                if(frame==6 || frame==8 || frame==9 || frame==12 || frame==13 || frame==16) {
                    for(auto& memory:world_memory) memory.reset();for(auto& memory:model_memory) memory.reset();
                    for(auto& memory:phosphor_memory) memory.reset();
                }
                auto outcome=DisplayXrSubmission::failed;
                if(frame==2) {
                    gpu_mock.timeout_right=true;outcome=renderer.submit(selected);
                    check(outcome==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Temporal image wait lost its source");
                    check(renderer.poll()==DisplayXrSubmission::waiting && renderer.poll()==DisplayXrSubmission::waiting,
                        "Temporal compositor retry advanced a pending frame");gpu_mock.timeout_right=false;outcome=renderer.poll();
                } else if(frame==3) {
                    QueueGate gate(gpu);outcome=renderer.submit(selected);
                    check(outcome==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Temporal GPU stall lost its source");
                    check(renderer.poll()==DisplayXrSubmission::waiting,"Temporal GPU stall was counted as complete");gate.unblock();
                } else outcome=renderer.submit(selected);
                const auto actual=finish(outcome);
                for(unsigned eye=0;eye<2;++eye) {
                    starfox::render::Framebuffer ownership(mock.image_width,mock.image_height);ownership.enable_layer_tags(true);
                    for(unsigned at=0;at<label_temporal[frame][eye].size();at+=4) {
                        const auto group=label_temporal[frame][eye][at+2];check(group<=2,"Independent native ownership labels changed");
                        ownership.layer_tags()[at/4]=unsigned(group==0?starfox::render::PixelLayer::two_d:
                            group==1?starfox::render::PixelLayer::background:starfox::render::PixelLayer::three_d);
                    }
                    auto expected=raw_temporal[frame][eye];
                    std::vector<unsigned char> scratch;
                    apply_global_enhancements(selected->settings.global_enhancements,ownership,expected,scratch,selected->settings.effect_seconds);
                    world_memory[eye].apply(ownership,expected,!have_world?starfox::render::PersistenceMode::off:
                        static_cast<starfox::render::PersistenceMode>(world_mode),false,true,selected->settings.effect_seconds,1,selected->settings.world_effects[1]);
                    model_memory[eye].apply(ownership,expected,!have_model?starfox::render::PersistenceMode::off:
                        static_cast<starfox::render::PersistenceMode>(model_mode),true,false,selected->settings.effect_seconds,1,selected->settings.manipulation_intensity);
                    phosphor_memory[eye].apply(ownership,expected,!selected->settings.phosphor?starfox::render::PersistenceMode::off:
                        starfox::render::PersistenceMode::phosphor,true,true,selected->settings.effect_seconds,1,100,selected->settings.phosphor);
                    for(unsigned at=0;at<actual[eye].size();at+=4) {
                        const unsigned group=label_temporal[frame][eye][at+2];
                        if(group==0) check(std::equal(raw_temporal[frame][eye].begin()+at,raw_temporal[frame][eye].begin()+at+4,actual[eye].begin()+at),
                            "Native history changed protected UI/emissive ink");
                        for(unsigned c=0;c<3;++c) check(std::abs(int(actual[eye][at+c])-expected[at+c])<=
                            (source->settings.srgb || selected->settings.global_enhancements?1:0),
                            "Actual native owner differs from independent moving per-eye float history");
                        check(actual[eye][at+3]==raw_temporal[frame][eye][at+3],"Native history changed opacity");
                        if(!std::equal(actual[eye].begin()+at,actual[eye].begin()+at+3,raw_temporal[frame][eye].begin()+at)) {
                            observed_world|=group==1;observed_model|=group==2;
                        }
                    }
                }
                if(frame==11) check(finish(renderer.submit(selected))==actual,"Paused temporal phase kept changing the image");
            }
            check(observed_world || observed_model,"Native owner silently bypassed temporal history");
            check(finish(renderer.submit(source))==plain,"Native temporal OFF did not restore the exact source");
        }
        // A rejected end-frame must not seed the next temporal image, even if
        // all GPU history writes completed. Recover without changing options
        // or cartridge scene identity so only presentation acceptance can reset.
        for(auto effect:{starfox::render::Effect::trails,starfox::render::Effect::long_exposure}) {
            check(finish(renderer.submit(source))==plain,"Failed-presentation test did not start from OFF");
            const auto temporal_frame=[&](unsigned index) {
                auto frame=std::make_shared<CalibratedGameFrame>(*temporal_sources[index]);
                frame->settings.manipulation=unsigned(effect);frame->settings.phosphor=2;return frame;
            };
            (void)finish(renderer.submit(temporal_frame(0)));(void)finish(renderer.submit(temporal_frame(1)));
            gpu_mock.fail_end=true;auto outcome=renderer.submit(temporal_frame(2));
            for(unsigned attempt=0;outcome==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);outcome=renderer.poll();}
            check(outcome==DisplayXrSubmission::failed && !renderer.frame_pending() && !renderer.retained_frame(),
                "Rejected native presentation committed or retained pending history");
            // The test compositor made its inspection copy before reporting
            // end-frame failure. Drain and discard that rejected observation;
            // it is not a successfully presented reference image.
            check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
            gpu_mock.readback.pending=false;gpu_mock.retired_images.clear();
            const auto fresh=finish(renderer.submit(temporal_frame(3)));
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<fresh[eye].size();++at)
                check(std::abs(int(fresh[eye][at])-raw_temporal[3][eye][at])<=(source->settings.srgb && at%4!=3?1:0),
                    "Rejected presentation seeded stale native temporal history");
        }
        std::cout<<"Native temporal owner format "<<unsigned(format)<<": independent moving eye/geometry oracle, trails/long exposure/all CRT phosphor qualities, independent combined histories, source-tick continuity, UI clearing, pause, scene/flow/camera/state/intensity resets, rejected-frame recovery and image/GPU waits passed.\n";
        for(unsigned fault=0;fault<23;++fault) {
            auto bad=std::make_shared<CalibratedGameFrame>(*source);
            if(fault==0) bad->settings.world_effects[0]=starfox::render::effect_count;
            if(fault==1) bad->settings.model_effects[1]=101;
            if(fault==2) bad->settings.manipulation=unsigned(starfox::render::Effect::sepia);
            if(fault==3) bad->settings.manipulation_intensity=101;
            if(fault==4) bad->settings.extra_effects[1]=unsigned(starfox::render::Effect::twist);
            if(fault==5) bad->settings.effect_seconds=NAN;
            if(fault==6) bad->settings.effect_seconds=-1;
            if(fault==7) bad->settings.phosphor=4;
            if(fault==8) bad->settings.global_enhancements=1U<<26;
            if(fault==9) bad->settings.bloom_model=4;
            if(fault==10) bad->settings.bloom_world=4;
            if(fault==11) bad->settings.contrast=4;
            if(fault==12) bad->settings.chromatic=4;
            if(fault==13) bad->settings.exposure=4;
            if(fault==14) bad->settings.camera_response_modes=64;
            if(fault==15) bad->settings.camera_response_pose[0]=NAN;
            if(fault==16) bad->settings.camera_response_pose[2]=.1;
            if(fault==17) bad->settings.depth_enhancements=16;
            if(fault==18) bad->settings.aa_quality=4;
            if(fault==19) bad->settings.aa_type=7;
            if(fault>=20) bad->settings.convergence_source_units=fault==20?0:fault==21?NAN:65536;
            check(renderer.submit(bad)==DisplayXrSubmission::failed && !renderer.frame_pending(),
                "Malformed native style partially submitted or retained a source frame");
        }
        std::array<std::vector<unsigned char>,2> previous_material;
        for(auto material:starfox::render::effect_order) {
            if(!starfox::render::material(material)) continue;
            auto selected=std::make_shared<CalibratedGameFrame>(*source);
            selected->settings.material=unsigned(material);
            selected->settings.reflections=3;
            check(finish(renderer.submit(selected))==plain,"Native material applied with ray tracing OFF");
            selected->settings.ray_tracing=3;
            const auto descriptors=selected->packets();
            for(const auto& p:descriptors) check(p.reflective_material==(p.ray_caster && starfox::render::reflective_material(material)),
                "Native material descriptor selected world/portrait/HUD or omitted a model");
            const auto material_pixels=finish(renderer.submit(selected));
            check(material_pixels[0]!=traced[0] && material_pixels[1]!=traced[1],"Selected native material retained ordinary dielectric reflections");
            if(!previous_material[0].empty()) check(material_pixels!=previous_material,"Native material selection did not change traced radiance");
            previous_material=material_pixels;
            auto combination=std::make_shared<CalibratedGameFrame>(*selected);
            combination->settings.world_effects={unsigned(starfox::render::Effect::watercolour),35,0,0};
            combination->settings.model_effects={unsigned(starfox::render::Effect::negative),100,0,0};
            const auto combined=finish(renderer.submit(combination));
            check(combined[0]!=material_pixels[0] && combined[1]!=material_pixels[1] && combined[0]!=combined[1],
                "Native material/styles combination reused old finished eye images");
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<plain[eye].size();at+=4) {
                const bool ink=plain[eye][at]==255 && plain[eye][at+2]==0 && (plain[eye][at+1]==0 || plain[eye][at+1]==255);
                if(ink) check(std::equal(plain[eye].begin()+at,plain[eye].begin()+at+4,material_pixels[eye].begin()+at),
                    "Native material changed protected HUD/emissive ink");
                if(ink) check(std::equal(plain[eye].begin()+at,plain[eye].begin()+at+4,combined[eye].begin()+at),
                    "Combined native material/styles changed protected HUD/emissive ink");
                check(material_pixels[eye][at+3]==plain[eye][at+3],"Native material made a solid model transparent");
                check(combined[eye][at+3]==plain[eye][at+3],"Combined native material/styles changed source opacity");
            }
            selected->settings.reflections=0;
            for(const auto& p:selected->packets()) check(!p.reflective_material,"Native material stayed active without reflections");
            auto shadow_only=std::make_shared<CalibratedGameFrame>(*selected);shadow_only->settings.material=0;
            check(finish(renderer.submit(selected))==finish(renderer.submit(shadow_only)),
                "Native material applied while reflective surfaces were OFF");
        }
        auto invalid_material=std::make_shared<CalibratedGameFrame>(*source);
        invalid_material->settings.material=unsigned(starfox::render::Effect::cel_drawn);
        check(renderer.submit(invalid_material)==DisplayXrSubmission::failed && !renderer.frame_pending(),
            "Malformed native material retained or partially submitted a frame");
        auto changed_sky=std::make_shared<CalibratedGameFrame>(*source);
        changed_sky->settings.ray_tracing=changed_sky->settings.reflections=3;
        for(auto& v:changed_sky->draws.back().packet.geometry.vertices) {
            const std::array<float,4> colour{.16F,.25F,.83F,1};
            std::copy(colour.begin(),colour.end(),v.color);std::copy(colour.begin(),colour.end(),v.odd_color);
        }
        const auto changed_reflection=finish(renderer.submit(changed_sky));
        check(changed_reflection[0]!=traced[0] && changed_reflection[1]!=traced[1],
            "Live ray owner reflected stale scenery instead of the new source palette");
        auto restored_sky=std::make_shared<CalibratedGameFrame>(*source);
        restored_sky->settings.ray_tracing=restored_sky->settings.reflections=3;
        check(finish(renderer.submit(restored_sky))==traced,"Live scenery toggle/restoration did not recover exact reflections");
        auto space=std::make_shared<CalibratedGameFrame>(*source);
        auto space_tick=std::make_shared<starfox::vr::GameSceneSnapshot>(*source->current);
        space_tick->dots_mode=-1;space->current=space_tick;space->settings.ray_tracing=3;space->settings.reflections=0;
        check(finish(renderer.submit(space))==plain,"Space RT without reflections changed the scene or introduced shadows");
        auto ray_source=std::make_shared<CalibratedGameFrame>(*source);ray_source->settings.ray_tracing=ray_source->settings.reflections=3;
        ray_source->settings.material=unsigned(starfox::render::Effect::silver);
        ray_source->settings.world_effects={unsigned(starfox::render::Effect::sepia),35,0,0};
        ray_source->settings.model_effects={unsigned(starfox::render::Effect::cel_drawn),100,0,0};
        ray_source->settings.manipulation=unsigned(starfox::render::Effect::trails);
        ray_source->settings.extra_effects={unsigned(starfox::render::Effect::long_exposure),
            unsigned(starfox::render::Effect::energy_shield),unsigned(starfox::render::Effect::arc_lightning)};
        ray_source->settings.effect_seconds=.375F;
        ray_source->settings.phosphor=3;
        ray_source->settings.global_enhancements=global_enhancement_mask;
        ray_source->settings.bloom_model=3;ray_source->settings.bloom_world=2;
        ray_source->settings.contrast=2;ray_source->settings.chromatic=3;
        const auto retained_effects=finish(renderer.submit(ray_source));
        // Image waits and GPU stalls retain all passes, both history profiles
        // and the phase. Re-polling cannot re-encode, age or commit history.
        gpu_mock.timeout_right=true;
        check(renderer.submit(ray_source)==DisplayXrSubmission::waiting && renderer.retained_frame()==ray_source,
            "Ray owner dropped sources during image wait");
        check(renderer.poll()==DisplayXrSubmission::waiting && renderer.retained_frame()==ray_source,"Ray owner replaced waiting images");
        gpu_mock.timeout_right=false;check(finish(renderer.poll())==retained_effects,"Image wait changed retained ray/style/FX/history eyes or phase");
        {
            QueueGate gate(gpu);check(renderer.submit(ray_source)==DisplayXrSubmission::waiting && renderer.retained_frame()==ray_source,
                "Ray owner did not retain a blocked source");
            const auto started=std::chrono::steady_clock::now();
            for(unsigned attempt=0;attempt<3;++attempt) check(!renderer.try_close() && renderer.retained_frame()==ray_source,
                "Ray owner freed in-flight producers/eye images");
            check(std::chrono::steady_clock::now()-started<std::chrono::milliseconds(100),"Native ray cleanup blocked the event loop");
            gate.unblock();bool closed=false;
            for(unsigned attempt=0;!closed && attempt<10000;++attempt) {closed=renderer.try_close();if(!closed) SDL_Delay(1);}
            check(closed && !renderer.retained_frame() && native_mock.last_layers==0,"Ray owner cleanup did not drain without visible layers");
        }
        // A failure AFTER a native trace and queued fence wait must retire
        // that work before cleanup; returning false is not completion proof.
        {
            RayCopyFault fault(gpu.device,3);DisplayXrGameRenderer failing;
            check(failing.initialize(runtime,gpu.device,window) && failing.poll_events(),failing.status().c_str());
            QueueGate gate(gpu);
            check(failing.submit(ray_source)==DisplayXrSubmission::failed && ray_copy_calls==3
                && failing.frame_pending() && failing.retained_frame()==ray_source,"Native failure lost ordered retirement ownership");
            const auto started=std::chrono::steady_clock::now();
            for(unsigned attempt=0;attempt<3;++attempt) check(!failing.try_close() && failing.retained_frame()==ray_source,
                "Failed trace resources were destroyed before ordered retirement");
            check(std::chrono::steady_clock::now()-started<std::chrono::milliseconds(100),"Failed trace cleanup blocked on native GPU work");
            gate.unblock();bool closed=false;
            for(unsigned attempt=0;!closed && attempt<10000;++attempt) {closed=failing.try_close();if(!closed) SDL_Delay(1);}
            check(closed && !failing.retained_frame() && !failing.frame_pending() && native_mock.last_layers==0,
                "Failed native trace did not safely retire without a stale visible frame");
        }
        std::cout<<"Native ray owner format "<<unsigned(format)<<": low/medium/high resident traces, all 21 materials and RT/reflection gates, 47 drawn/palette styles and separate distortion/special-FX categories, ordered combined ray/material/style/warp/FX passes, protected ink/opacity, held phase, independent eyes, image/GPU waits and failed-import nonblocking retirement passed.\n";
    }
    // Ordinary desktop owner: actual SDL HWND placement/restore, exact-LUID
    // device creation, native calibrated submission and safe disconnect.
    // Discovery/compositor remain mocked; no physical panel is claimed here.
    reset();
    {
        DisplayXrDesktop desktop;
        mock.panel_confirmed=false;
        check(!desktop.prepare_with_api(get) && !desktop.active(),"Desktop accepted unconfirmed panel hardware");
        mock.panel_confirmed=true;
        mock.system_result=XR_ERROR_FORM_FACTOR_UNAVAILABLE;mock.destroy_result=XR_ERROR_RUNTIME_FAILURE;
        check(!desktop.prepare_with_api(get) && desktop.close_pending(),"Failed-discovery instance lost its cleanup owner");
        const auto discovery_instances=creates;
        check(!desktop.prepare_with_api(get) && creates==discovery_instances && desktop.close_pending(),
            "Failed discovery retried over a still-live runtime instance");
        mock.destroy_result=XR_SUCCESS;mock.system_result=XR_SUCCESS;
        check(desktop.try_close() && !desktop.close_pending(),"Failed discovery instance could not drain");
        const bool desktop_prepared=desktop.prepare_with_api(get);
        check(desktop_prepared,desktop.status().c_str());
        const auto props=SDL_CreateProperties();check(props,"Desktop adapter properties unavailable");
        check(desktop.configure_gpu_properties(props),"Desktop did not configure its required GPU");
        SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"direct3d12");
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
        auto* selected=SDL_CreateGPUDeviceWithProperties(props);check(selected,SDL_GetError());
        auto* selected_native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(selected),STARFOX_SDL_D3D12_DEVICE,nullptr));
        check(selected_native,"LUID-selected native GPU unavailable");LUID expected{},actual{};
        // SDL/native adapter creation can replace the active storage-filter
        // stack even when D3D12 returns the same device identity. Preserve all
        // existing messages, recapture the current critical filter before the
        // selected owner does any work, and retain distinct queues until SDK
        // shutdown. This must not clear/enlarge a queue or hide error IDs.
        std::cout<<"D3D12 selected diagnostic owner reuses native device="<<(selected_native==native)<<std::endl;
        capture_native_d3d12_errors(selected_native);
#if defined(__MINGW32__)
        native->GetAdapterLuid(&expected);selected_native->GetAdapterLuid(&actual);
#else
        expected=native->GetAdapterLuid();actual=selected_native->GetAdapterLuid();
#endif
        check(expected.LowPart==actual.LowPart && expected.HighPart==actual.HighPart,"Native display selected a different adapter");
        SDL_DestroyGPUDevice(selected);
        SDL_SetNumberProperty(props,"starfox.d3d12.adapter_luid_low",0xffffffffU);
        SDL_SetNumberProperty(props,"starfox.d3d12.adapter_luid_high",0x7fffffff);
        check(!SDL_CreateGPUDeviceWithProperties(props),"Missing display adapter silently used a different GPU");
        SDL_DestroyProperties(props);
        auto* panel=SDL_CreateWindow("Leia desktop fixture",320,240,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
        check(panel,SDL_GetError());int x{},y{},width{},height{};
        check(SDL_GetWindowPosition(panel,&x,&y) && SDL_GetWindowSize(panel,&width,&height),SDL_GetError());
        native_mock.expected=calibrated_game_rig();native_mock.head_x=0;native_mock.pose_scale=1.F/256.F;
        const bool desktop_attached=desktop.attach(panel,gpu.device,dlss_fixture::api);
        check(desktop_attached && desktop.active(),desktop.status().c_str());
        auto frame=std::make_shared<CalibratedGameFrame>();
        frame->current=frame->previous=std::make_shared<starfox::vr::GameSceneSnapshot>();
        frame->settings.srgb=desktop.srgb_target();frame->clear={.25F,.25F,.25F,1};
        if(native_dlss_only) frame->settings.dlss_mode=1;
        starfox::render::Framebuffer ui(256,224);ui.set(128,112,1);
        std::array<starfox::render::Rgba8,256> ink{};ink[1]={255,220,64,255};
        append_calibrated_host_ui(*frame,ui,ink,15,std::array<int,4>{12,20,244,223});
        gpu_mock.timeout_right=true;
        const auto desktop_ends=dlss_fixture::finished_frames;
        const bool desktop_presented=desktop.present(frame);
        check(desktop_presented && desktop.frame_pending() && desktop.presented_frames()==0,desktop.status().c_str());
        check(desktop.present(nullptr) && desktop.frame_pending() && gpu_mock.eyes[0].acquires==1
            && gpu_mock.eyes[1].acquires==1,"Desktop replaced/reacquired pending game UI");
        if(native_dlss_only) check(dlss_fixture::finished_frames==desktop_ends,"SDK cleanup ran before the pending compositor pair completed");
        gpu_mock.timeout_right=false;
        for(unsigned attempt=0;desktop.frame_pending() && attempt<10000;++attempt) {
            SDL_Delay(1);const bool continued=desktop.present(nullptr);check(continued,desktop.status().c_str());
        }
        check(!desktop.frame_pending() && gpu_mock.projections==1 && desktop.presented_frames()==1,
            "Desktop counted waiting iterations as presented frames or failed to submit host UI");
        if(native_dlss_only) check(dlss_fixture::finished_frames==desktop_ends+1,"SDK cleanup ran more than once while polling a compositor pair");
        check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());gpu_mock.readback.pending=false;gpu_mock.retired_images.clear();
        native_mock.active=false;
        check(!desktop.present(frame),"Disconnected desktop panel consumed another visible game frame");
        check(desktop.try_close() && !desktop.active() && !desktop.close_pending()
            && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,
            "Desktop disconnect did not clean native resources");
        int restored_x{},restored_y{},restored_width{},restored_height{};
        check(SDL_GetWindowPosition(panel,&restored_x,&restored_y) && SDL_GetWindowSize(panel,&restored_width,&restored_height)
            && restored_x==x && restored_y==y && restored_width==width && restored_height==height
            && !(SDL_GetWindowFlags(panel)&SDL_WINDOW_BORDERLESS),"Desktop did not restore ordinary window placement");
        const auto generation=desktop.generation();
        const auto sessions=native_mock.session_creates;
        mock.panel_confirmed=false;
        check(!desktop.prepare_with_api(get) && desktop.generation()==generation
            && native_mock.session_creates==sessions,"Offline retry created a session or reused old hardware");
        mock.panel_confirmed=true;
        check(desktop.prepare_with_api(get) && desktop.attach(panel,gpu.device,dlss_fixture::api)
            && desktop.generation()==generation+1 && native_mock.session_creates==sessions+1,
            "Reconnected desktop did not create a fresh calibrated session");
        frame->settings.srgb=desktop.srgb_target();
        {
            QueueGate gate(gpu);
            const auto inflight_ends=dlss_fixture::finished_frames;
            check(desktop.present(frame) && desktop.frame_pending(),desktop.status().c_str());
            const auto destroyed=gpu_mock.swapchain_destroys;
            check(!desktop.try_close() && !desktop.active() && desktop.close_pending()
                && gpu_mock.swapchain_destroys==destroyed && !desktop.present(frame),
                "Disconnected desktop reused/destroyed an in-flight session");
            const auto instances=creates;
            check(!desktop.prepare_with_api(get) && creates==instances,
                "Reconnect probe replaced the runtime before the GPU drained");
            if(native_dlss_only) check(dlss_fixture::finished_frames==inflight_ends,"SDK resources were collected before in-flight GPU work drained");
            gate.unblock();bool closed=false;
            for(unsigned attempt=0;!closed && attempt<10000;++attempt) {closed=desktop.try_close();if(!closed) SDL_Delay(1);}
            check(closed && !desktop.close_pending() && native_mock.last_layers==0,
                "Desktop reconnect cleanup retained a layer or leaked the previous owner");
            if(native_dlss_only) check(dlss_fixture::finished_frames==inflight_ends+1,"Closing the cancelled native pair did not finish SDK cleanup exactly once");
        }
        check(desktop.prepare_with_api(get) && desktop.attach(panel,gpu.device,dlss_fixture::api)
            && desktop.generation()==generation+2,"Desktop did not reconnect after a pending-frame drain");
        native_mock.ready=false;
        const auto now=DisplayXrDesktop::Clock::now();
        check(desktop.present(frame,now) && desktop.presented_frames()==0
            && !desktop.present(frame,now+std::chrono::seconds(6)) && desktop.try_close(),
            "Missing READY stayed unavailable indefinitely or counted a visible frame");
        check(desktop.prepare_with_api(get) && desktop.attach(panel,gpu.device,dlss_fixture::api),desktop.status().c_str());
        gpu_mock.timeout_right=true;
        check(desktop.present(frame,now) && desktop.frame_pending()
            && !desktop.present(nullptr,now+std::chrono::seconds(6)) && desktop.presented_frames()==0,
            "Runtime image wait did not expire or presented an incomplete frame");
        bool drained=false;
        for(unsigned attempt=0;!drained && attempt<10000;++attempt) {drained=desktop.try_close();if(!drained) SDL_Delay(1);}
        check(drained && !desktop.close_pending() && native_mock.last_layers==0,
            "Expired native image wait did not drain without a layer");
        gpu_mock.timeout_right=false;
        check(desktop.prepare_with_api(get) && desktop.attach(panel,gpu.device),desktop.status().c_str());
        native_mock.loss=true;
        check(!desktop.present(frame),"Runtime instance loss continued presenting old calibrated eyes");
        mock.destroy_result=XR_ERROR_RUNTIME_FAILURE;
        check(!desktop.try_close() && desktop.close_pending() && !desktop.active(),
            "Failed instance destruction discarded its cleanup owner");
        const auto retained_instances=creates;
        check(!desktop.prepare_with_api(get) && creates==retained_instances,
            "Recovery started a new probe before old instance destruction succeeded");
        mock.destroy_result=XR_SUCCESS;
        check(desktop.try_close() && !desktop.close_pending(),"Instance-destroy retry failed");
        check(gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,
            "Reconnect cycles leaked native images");
        SDL_DestroyWindow(panel);
        std::cout<<"Native desktop: confirmed-panel gating, exact GPU LUID, real HWND bounds/restore, host UI, pending-image retention and disconnect passed. Mocked hardware/compositor.\n";
    }
    check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
    validate_native_d3d12_errors("completed GPU-owner work");
    runtime.close();gpu_mock=GpuMock{};gpu_blocked=false;
    std::cout<<"Native D3D12 presenter: "<<exact_frames<<" byte-exact transport frames and 4 calibrated geometry/GPU-ground stereo frames consumed on the real SDL queue; "
        <<"rotating per-eye image indices, calibrated poses/FOV/Y, timeouts, real GPU stalls, cancellation, init/frame failures and in-flight teardown passed. "
        <<"Mocked XR compositor, not physical Leia rendering validation.\n";
}
#endif
}

int main(int argc,char** argv) try {
    game_motion_history_tests();
    std::cout.setf(std::ios::unitbuf);
    (void)argc;(void)argv;
    using namespace starfox::render;
    CalibratedEffectClock fx_clock;
    check(fx_clock.sample(10,false)==0 && fx_clock.sample(10.5,false)==.5F,"Native FX clock did not follow presentation time");
    check(fx_clock.sample(11,true)==.5F && fx_clock.sample(20,true)==.5F
        && fx_clock.sample(21,false)==.5F && fx_clock.sample(21.25,false)==.75F,
        "Native FX clock advanced during pause or replayed paused time on resume");
    check(fx_clock.sample(NAN,false)==.75F && fx_clock.sample(-1,false)==.75F,"Invalid native FX timestamp changed retained phase");
    camera_light_tests();game_motion_identity_tests();game_motion_preparation_tests();palette_history_policy_tests();game_ground_motion_identity_tests();ground_metadata_stability_tests();recovery_tests();
    DisplayXrRuntime runtime;
    check(!runtime.initialize_with_api(nullptr),"Null dispatch accepted");empty(runtime);
    for(auto backend:{DisplayXrBackend::direct3d12,DisplayXrBackend::vulkan}) {
        mock=Mock{};mock.backend=backend;
        check(runtime.initialize_with_api(get,backend),"Compatible mocked panel rejected");
        check(runtime.detected() && runtime.panel().pixel_width==3840 && !runtime.panel().primary
            && runtime.panel().desktop_rect.offset.x==-3840 && runtime.panel().view_scale[0]==.5F,"Physical panel geometry lost");
        check(runtime.initialize_with_api(get,backend),"Reinitialization failed");
        runtime.close();empty(runtime);
    }
    const auto reject=[&](const char* reason) {
        check(!runtime.initialize_with_api(get),reason);empty(runtime);
    };
    mock=Mock{};mock.panel_confirmed=false;reject("Unconfirmed monitor accepted");
    mock=Mock{};mock.system_name="DisplayXR: Simulated 3D Display";reject("Simulated display accepted");
    mock=Mock{};mock.runtime_name="Other XR runtime";reject("Non-DisplayXR runtime accepted");
    mock=Mock{};mock.runtime_name="DisplayXR-compatible simulator";reject("Misleading runtime name accepted");
    mock=Mock{};mock.extensions[0].extensionVersion=17;reject("Old display ABI accepted");
    mock=Mock{};mock.extensions[1].extensionVersion=2;reject("Old camera-rig ABI accepted");
    mock=Mock{};mock.extensions[2].extensionVersion=7;reject("Old window ABI accepted");
    mock=Mock{};mock.extensions.erase(mock.extensions.begin()+3);reject("Missing D3D12 extension accepted");
    mock=Mock{};mock.extension_count_override=8192;reject("Unbounded extension allocation accepted");
    mock=Mock{};mock.extensions.clear();reject("Empty extension list accepted");
    mock=Mock{};mock.create_result=XR_ERROR_RUNTIME_FAILURE;reject("Create failure ignored");
    mock=Mock{};mock.system_result=XR_ERROR_FORM_FACTOR_UNAVAILABLE;reject("Missing hardware ignored");
    mock=Mock{};mock.properties_result=XR_ERROR_RUNTIME_FAILURE;reject("Property query failure ignored");
    mock=Mock{};mock.wrong_system_id=true;reject("Mismatched panel identity accepted");
    mock=Mock{};mock.zero_monitor=true;reject("Missing OS monitor identity accepted");
    mock=Mock{};mock.unterminated=true;reject("Unterminated monitor name accepted");
    for(float value:{0.F,-1.F,9.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        mock=Mock{};mock.scale=value;reject("Invalid view scale accepted");
    }
    mock=Mock{};mock.physical_width=0;reject("Zero physical width accepted");
    mock=Mock{};mock.viewer_z=0;reject("Viewer on the panel accepted");
    mock=Mock{};mock.query_eyes=1;reject("Non-stereo configuration accepted");
    mock=Mock{};mock.read_eyes=1;reject("Eye-count change accepted");
    mock=Mock{};mock.image_width=0;reject("Zero swapchain width accepted");
    mock=Mock{};mock.image_width=8192;reject("Oversized recommendation accepted");
    mock=Mock{};mock.bad_sample=true;reject("Unsupported sample count accepted");
    mock=Mock{};mock.missing="xrGetSystemProperties";reject("Missing property entry point accepted");
    mock=Mock{};mock.missing="xrDestroyInstance";
    check(!runtime.initialize_with_api(get) && !runtime.detected() && runtime.instance()==current
        && current!=XR_NULL_HANDLE && runtime.panel().pixel_width==0,"Broken cleanup dropped the live instance");
    const auto before_pending=creates;
    check(!runtime.initialize_with_api(get) && creates==before_pending,"Discovery overwrote a pending live instance");
    mock.missing=nullptr;runtime.close();empty(runtime);
    mock=Mock{};check(runtime.initialize_with_api(get),"Destroy-failure fixture failed");
    mock.destroy_result=XR_ERROR_RUNTIME_FAILURE;runtime.close();
    check(!runtime.detected() && runtime.instance()==current && !runtime.initialize_with_api(get),
        "Destroy failure discarded handle or permitted reinitialization");
    mock.destroy_result=XR_SUCCESS;runtime.close();empty(runtime);
    mock=Mock{};
    check(!runtime.initialize_with_api(get,static_cast<DisplayXrBackend>(42)),"Unknown backend accepted");empty(runtime);
    check(runtime.initialize_with_api(get),"Discovery did not recover after failures");
    runtime.close();empty(runtime);
    native_tests(runtime);
#ifdef STARFOX_DISPLAYXR_GPU_FIXTURE
    bool vulkan{};std::vector<const char*> inputs;const char* dlss_directory{};
    for(int i=1;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--taa-only") native_taa_only=true;
        else if(option=="--taa-water-only") native_taa_only=native_taa_water_only=true;
        else if(option=="--taa-palette-only") native_taa_only=native_taa_palette_only=true;
        else if(option=="--taa-pattern-only") native_taa_only=native_taa_pattern_only=true;
        else if(option=="--taa-edge-only") native_taa_only=native_taa_edge_only=true;
        else if(option=="--taa-reconstructed-post-only") native_taa_only=native_taa_reconstructed_post_only=true;
        else if(option=="--taa-liquid-post-only") native_taa_only=native_taa_liquid_post_only=true;
        else if(option=="--taa-local-layers-only") native_taa_only=native_taa_local_layers_only=true;
        else if(option=="--taa-ground-clock-only") native_taa_only=native_taa_ground_clock_only=true;
        else if(option=="--taa-reflection-only") native_taa_only=native_taa_reflection_only=true;
        else if(option=="--msaa-reflection-only") native_taa_only=native_taa_reflection_only=native_msaa_reflection_only=true;
        else if(option=="--msaa-reflection-water-only") native_taa_only=native_taa_reflection_only=native_msaa_reflection_only=native_msaa_reflection_water_only=true;
        else if(option=="--msaa-reflection-lava-only") native_taa_only=native_taa_reflection_only=native_msaa_reflection_only=native_msaa_reflection_lava_only=true;
        else if(option=="--curved-owner-only") native_taa_only=native_curved_owner_only=true;
        else if(option=="--source-frame-owner-only") native_taa_only=native_curved_owner_only=native_source_frame_owner_only=true;
        else if(option=="--source-motion-owner-only") native_taa_only=native_curved_owner_only=native_source_frame_owner_only=native_curved_motion_owner_only=true;
        else if(option=="--source-camera-owner-only") native_taa_only=native_curved_owner_only=native_source_frame_owner_only=native_curved_motion_owner_only=native_curved_camera_owner_only=true;
        else if(option=="--source-disocclusion-owner-only") native_taa_only=native_curved_owner_only=native_source_frame_owner_only=native_curved_motion_owner_only=native_curved_camera_owner_only=native_curved_disocclusion_owner_only=true;
        else if(option=="--curved-msaa-owner-only") native_taa_only=native_curved_owner_only=native_curved_msaa_owner_only=true;
        else if(option=="--curved-msaa-resolve-only") native_taa_only=native_curved_owner_only=native_curved_msaa_owner_only=native_curved_msaa_resolve_only=true;
        else if(option=="--curved-lens-owner-only") native_taa_only=native_curved_owner_only=native_curved_lens_owner_only=true;
        else if(option=="--curved-motion-owner-only") native_taa_only=native_curved_owner_only=native_curved_motion_owner_only=true;
        else if(option=="--curved-camera-owner-only") native_taa_only=native_curved_owner_only=native_curved_motion_owner_only=native_curved_camera_owner_only=true;
        else if(option=="--curved-disocclusion-owner-only") native_taa_only=native_curved_owner_only=native_curved_motion_owner_only=native_curved_camera_owner_only=native_curved_disocclusion_owner_only=true;
        else if(option=="--fsr-only") native_fsr_only=true;
        else if(option=="--dlss-sdk") {check(i+1<argc,"--dlss-sdk requires an explicit SDK directory");native_dlss_only=true;dlss_directory=argv[++i];}
        else if(option=="--dlss-failure-only") native_dlss_failure_only=true;
        else if(option=="--dlss-ray-samples-only") native_dlss_ray_samples_only=true;
        else if(option=="--dlss-ray-materials-only") native_dlss_ray_samples_only=native_dlss_ray_materials_only=true;
        else if(option=="--dlss-ray-rough-only") native_dlss_ray_samples_only=native_dlss_ray_materials_only=native_dlss_ray_rough_only=true;
        else if(option=="--dlss-ray-mixed-only") native_dlss_ray_samples_only=native_dlss_ray_materials_only=native_dlss_ray_mixed_only=true;
        else if(option=="--dlss-ray-liquids-only") native_dlss_ray_samples_only=native_dlss_ray_materials_only=native_dlss_ray_liquids_only=true;
        else if(option=="--dlss-ray-liquid-primary-edge-only") native_dlss_ray_samples_only=native_dlss_ray_materials_only=native_dlss_ray_liquids_only=native_dlss_ray_liquid_primary_edge_only=true;
        else if(option=="--dlss-ray-liquid-primary-wrap-only") native_dlss_ray_samples_only=native_dlss_ray_materials_only=native_dlss_ray_liquids_only=native_dlss_ray_liquid_primary_wrap_only=true;
        else if(option=="--dlss-panel-bound-only") native_dlss_panel_bound_only=true;
        else if(option=="--dlss-reconstructed-post-only") native_dlss_reconstructed_post_only=true;
        else if(option=="--dlss-liquid-post-only") native_dlss_liquid_post_only=true;
        else if(option=="--scene-fx-only") native_scene_fx_only=true;
        else if(option=="--fog-only") native_fog_only=true;
        else if(option=="--blur-only") native_blur_only=true;
        else if(option=="--ground-only") native_ground_only=true;
        else if(option=="--ground-cleanup-failure") native_ground_only=native_ground_cleanup_failure=true;
        else if(option=="--vulkan") vulkan=true;
        else if(option=="--integrated") vulkan_use_integrated=true;
        else inputs.push_back(argv[i]);
    }
    std::unique_ptr<dlss_fixture::Sdk> sdk;
    check(!native_curved_owner_only || !vulkan,"Ordered curved owner staging currently requires D3D12");
    check(!native_dlss_reconstructed_post_only || dlss_directory,"--dlss-reconstructed-post-only requires the explicit real --dlss-sdk directory");
    check(!native_dlss_liquid_post_only || dlss_directory,"--dlss-liquid-post-only requires the explicit real --dlss-sdk directory");
    check(!native_dlss_ray_samples_only || dlss_directory,"--dlss-ray-samples-only requires the explicit --dlss-sdk directory");
    check(!native_dlss_failure_only || dlss_directory,"--dlss-failure-only requires the explicit --dlss-sdk directory");
    check(!native_dlss_panel_bound_only || dlss_directory,"--dlss-panel-bound-only requires the explicit --dlss-sdk directory");
    if(dlss_directory) {check(!vulkan,"Official native SDK fixture requires D3D12");sdk=std::make_unique<dlss_fixture::Sdk>(dlss_directory);}
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
    check(vulkan && !dlss_directory,"Native Vulkan calibrated probe requires --vulkan and no D3D12 SDK override");
    std::cout<<"Live calibrated native Vulkan adapter/presentation probe; mocked XR dispatch, not Linux/panel/FPS acceptance\n";
#endif
    if(vulkan) gpu_vulkan_presenter_tests(inputs.size()>1?inputs[0]:nullptr,inputs.size()>1?inputs[1]:nullptr);
    else gpu_presenter_tests(runtime,inputs.size()>1?inputs[0]:nullptr,inputs.size()>1?inputs[1]:nullptr);
    if(sdk) sdk->finish();
    if(!vulkan) validate_native_d3d12_errors("SDK and SDL-owner teardown");
#endif
    std::cout<<"DisplayXR physical-panel gating, both graphics APIs, malformed/old interfaces, failure cleanup and recovery passed ("
        <<checks<<" assertions). Native rig/session chains, asymmetric FOV, retained tracking and lifecycle also passed. "
        <<"Mocked dispatch, not physical display/presentation validation.\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
