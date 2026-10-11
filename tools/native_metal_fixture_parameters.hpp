#pragma once
#include "native_reflection_fixture.hpp"
#include "native_shadow_cutout_fixture.hpp"
#include <cstddef>

namespace native_metal_fixture {
// Full current Metal kernel layouts. The legacy indexed fixture initializes
// the additional native-colour/cube/liquid fields to zero, never shortens ABI.
struct alignas(16) ReflectionParameters {
    unsigned width{},height{},quality{},metallic{};
    float focal_x{},focal_y{},center_x{},center_y{};
    float roughness{},has_ground{};unsigned environment{},texel_count{};
    std::array<float,4> point{},normal{},water{},row0{},row1{},row2{};
    std::array<unsigned,4> backdrop{},enhanced_size{};
    std::array<float,4> enhanced_motion{},enhanced_plane{},enhanced_projection{},enhanced_palette{},keep0{},keep1{};
    std::array<unsigned,4> coverage{};
    std::array<float,4> primary_range{};
    std::array<unsigned,4> optical{},cube_info{};
    std::array<float,4> cube_row0{},cube_row1{},cube_row2{},source_colour{};
    std::array<unsigned,4> liquid_layers{};
};
static_assert(sizeof(ReflectionParameters)==416);
static_assert(offsetof(ReflectionParameters,coverage)==272);
static_assert(offsetof(ReflectionParameters,primary_range)==288);
static_assert(offsetof(ReflectionParameters,optical)==304);
static_assert(offsetof(ReflectionParameters,cube_info)==320);
static_assert(offsetof(ReflectionParameters,cube_row0)==336);
static_assert(offsetof(ReflectionParameters,cube_row2)==368);
static_assert(offsetof(ReflectionParameters,source_colour)==384);
static_assert(offsetof(ReflectionParameters,liquid_layers)==400);

struct alignas(16) ShadowParameters {
    unsigned width{},height{};float focal_x{},focal_y{};
    float center_x{},center_y{},has_ground{},ground_only{};
    std::array<float,4> point{},normal{};
    std::array<std::array<float,4>,16> lights{};
    std::array<unsigned,4> coverage{};
    std::array<float,4> primary_range{};
};
static_assert(sizeof(ShadowParameters)==352);
static_assert(offsetof(ShadowParameters,lights)==64);
static_assert(offsetof(ShadowParameters,coverage)==320);
static_assert(offsetof(ShadowParameters,primary_range)==336);

inline ShadowParameters convert(const native_reflection_fixture::ShadowParameters& p) {
    ShadowParameters m;
    m.width=unsigned(p.extent[0]);m.height=unsigned(p.extent[1]);m.focal_x=p.extent[2];m.focal_y=p.extent[3];
    m.center_x=p.center[0];m.center_y=p.center[1];m.has_ground=p.center[2];m.ground_only=p.center[3];
    m.point=p.point;m.normal=p.normal;m.lights=p.lights;m.coverage=p.coverage;
    m.primary_range=p.primary_range;
    return m;
}

inline ReflectionParameters convert(const native_reflection_fixture::Parameters& p) {
    // This independent fixture owns indexed materials, a 1-word empty cube
    // and one output image. Refuse unsupported inputs instead of silently
    // dropping them or claiming native-atlas/cube/multilayer GPU coverage.
    using native_reflection_fixture::require;
    require(p.material_info==std::array<unsigned,4>{},"Indexed Metal fixture cannot bind native material metadata");
    require(p.source_colour==std::array<float,4>{},"Indexed Metal fixture cannot bind calibrated liquid layers");
    require(p.liquid_layers==std::array<unsigned,4>{},"Indexed Metal fixture owns one output image");
    require(p.cube_info==std::array<unsigned,4>{},"Indexed Metal fixture cannot bind a resident cube");
    ReflectionParameters m;
    m.width=p.dimensions[0];m.height=p.dimensions[1];m.quality=p.dimensions[2];m.metallic=p.dimensions[3];
    m.focal_x=p.camera[0];m.focal_y=p.camera[1];m.center_x=p.camera[2];m.center_y=p.camera[3];
    m.roughness=p.settings[0];m.has_ground=p.settings[1];m.texel_count=unsigned(p.settings[2]);m.environment=p.environment[0];
    m.point=p.point;m.point[3]=p.settings[3];m.normal=p.normal;m.water=p.water;m.row0=p.row0;m.row1=p.row1;m.row2=p.row2;
    m.backdrop={p.environment[1],p.environment[2],p.environment[3],0};m.enhanced_size=p.enhanced_size;
    m.enhanced_motion=p.enhanced_motion;m.enhanced_plane=p.enhanced_plane;m.enhanced_projection=p.enhanced_projection;
    m.enhanced_palette=p.enhanced_palette;m.keep0=p.keep0;m.keep1=p.keep1;
    m.primary_range=p.primary_range;
    return m;
}
}
