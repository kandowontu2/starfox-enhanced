#pragma once
// Interface subset adapted from DisplayXR runtime v2.21.11:
// src/external/openxr_includes/openxr/XR_DXR_display_info.h
// src/external/openxr_includes/openxr/XR_DXR_view_rig.h
// src/external/openxr_includes/openxr/XR_DXR_win32_window_binding.h
// Copyright 2025-2026 The DisplayXR Project. SPDX-License-Identifier: Apache-2.0
// Modified for Star Fox Enhanced: C++ namespace, field names/defaults, subset.
// Full license and provenance: third_party/displayxr/LICENSE.txt and NOTICE.txt.
// These DXR structure values are provisional, not Khronos-registered values.
// Keep layouts pinned to the corresponding extension versions. No SR SDK code.
#include <openxr/openxr.h>
#include <cstdint>

namespace starfox::render::displayxr {
inline constexpr const char* display_info_extension="XR_DXR_display_info";
inline constexpr const char* view_rig_extension="XR_DXR_view_rig";
inline constexpr const char* window_extension="XR_DXR_win32_window_binding";
inline constexpr auto display_info_type=static_cast<XrStructureType>(1004999003);
inline constexpr auto desktop_info_type=static_cast<XrStructureType>(1004999211);
inline constexpr auto window_binding_type=static_cast<XrStructureType>(1004999001);
inline constexpr auto rendering_mode_type=static_cast<XrStructureType>(1004999008);
inline constexpr auto camera_rig_type=static_cast<XrStructureType>(1004999141);
inline constexpr auto raw_views_type=static_cast<XrStructureType>(1004999142);
struct DisplayInfo {
    XrStructureType type{display_info_type};
    void* next{};
    XrExtent2Df display_size_meters{};
    XrVector3f nominal_viewer{};
    float view_scale_x{},view_scale_y{};
    std::uint32_t pixel_width{},pixel_height{};
};
struct DesktopInfo {
    XrStructureType type{desktop_info_type};
    void* next{};
    XrRect2Di rect{};
    char device_name[128]{};
    XrBool32 primary{},panel_confirmed{};
};
using ReadbackCallback=void (*)(const std::uint8_t*,std::uint32_t,std::uint32_t,void*);
struct WindowBinding {
    XrStructureType type{window_binding_type};
    const void* next{};
    void* window{};
    ReadbackCallback readback{};
    void* readback_user{};
    void* shared_texture{};
    XrBool32 transparent{};
};
struct RenderingMode {
    XrStructureType type{rendering_mode_type};
    void* next{};
    std::uint32_t index{};
    char name[XR_MAX_SYSTEM_NAME_SIZE]{};
    std::uint32_t eye_count{};
    float scale_x{},scale_y{};
    XrBool32 hardware_3d{};
    std::uint32_t columns{},rows{},view_width{},view_height{};
    XrBool32 active{},requestable{};
};
using EnumerateRenderingModes=XrResult (XRAPI_PTR *)(XrSession,std::uint32_t,std::uint32_t*,RenderingMode*);
using RequestRenderingMode=XrResult (XRAPI_PTR *)(XrSession,std::uint32_t);
struct CameraRig {
    XrStructureType type{camera_rig_type};
    const void* next{};
    XrPosef pose{};
    float ipd_factor{},parallax_factor{},convergence_diopters{},vertical_fov{},meters_to_virtual{};
};
struct RawViews {
    XrStructureType type{raw_views_type};
    void* next{};
    XrVector3f eyes[8]{};
    std::uint32_t eye_count{};
    XrPosef display_plane{};
    XrRect2Di canvas_rect{};
    XrExtent2Df canvas_size{};
    std::int64_t sample_time_ns{};
    XrBool32 tracking{};
};
}
