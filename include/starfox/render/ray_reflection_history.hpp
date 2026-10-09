#pragma once
#include "starfox/render/native_water_layers.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
namespace starfox::render::shadows {
struct RayReflectionGround {
    std::array<double,3> point{},normal{};
    bool operator==(const RayReflectionGround&) const=default;
    bool valid() const noexcept {
        double length=0;
        for(unsigned i=0;i<3;++i) {
            if(!std::isfinite(point[i]) || std::abs(point[i])>1.e12
                || !std::isfinite(normal[i]) || std::abs(normal[i])>1.e12) return false;
            length+=normal[i]*normal[i];
        }
        return std::isfinite(length) && length>1.e-20;
    }
};
struct RayReflectionLiquid {
    RayReflectionGround ground;
    std::array<double,9> world_to_view{1,0,0,0,1,0,0,0,1};
    std::array<double,3> offset{};
    double time{};
    unsigned material{}; // 0 water, 3 lava; never a flat-plane approximation.
    bool operator==(const RayReflectionLiquid&) const=default;
    bool valid() const noexcept {
        if(!ground.valid() || !std::isfinite(time) || time<0 || time>1.e12
            || (material!=0 && material!=3)) return false;
        for(double value:world_to_view) if(!std::isfinite(value) || std::abs(value)>1.e12) return false;
        for(double value:offset) if(!std::isfinite(value) || std::abs(value)>1.e12) return false;
        const auto& m=world_to_view;
        const double determinant=m[0]*(m[4]*m[8]-m[5]*m[7])
            +m[1]*(m[5]*m[6]-m[3]*m[8])+m[2]*(m[3]*m[7]-m[4]*m[6]);
        return std::isfinite(determinant) && std::abs(determinant)>1.e-12;
    }
};
// Explicit accepted triangle correspondence in the SAME resident allocation
// and primitive order as the current ray caster stream. XYZ is already in
// the previous eye's +Y-down/+Z-forward units; W=1 accepts the old vertex.
// This is secondary radiance correspondence, not primary receiver velocity.
struct RayReflectionHistory {
    std::uint32_t previous_vertex_offset{};
    std::array<unsigned,2> extent{};
    std::array<double,4> projection{}; // fx, fy, cx, cy in previous pixels.
    double near_plane{},far_plane{};
    std::uint32_t previous_index_offset{}; // Explicit accepted primitive mapping; zero is motion-only, no colour reuse.
    bool separated{}; // Ground: incident RGB and current base/material response.
    std::optional<RayReflectionGround> previous_ground{};
    std::optional<RayReflectionLiquid> previous_liquid{};
    unsigned model_lobes{}; // 0 legacy planes, 1 sharp or 8 fixed rough quadrature rays.
    // Explicit ordered planar specular witnesses. Opt-in producer ABI; the
    // single-hit history consumer must reject it until full-path validation.
    bool model_paths{};
    // Ordered finite models plus an actual analytic mirror/gold plane.
    // Carries CURRENT base radiance separately; never approximates a liquid.
    bool scene_paths{};
    // Opt-in native MODEL -> animated water/lava ordered producer. A distinct
    // 64-byte path ABI also retains CURRENT accumulated light and liquid-hop
    // identity. Not enabled reusable colour or a substitute for scene paths.
    bool curved_paths{};
    // Full liquid/model primary ownership and canonical water prefix. This
    // opt-in extends curved_paths; the MODEL-only 28-byte prefix is unchanged.
    bool curved_receivers{};
    bool valid() const noexcept {
        if(!previous_vertex_offset || previous_vertex_offset%16 || !extent[0] || !extent[1]
            || extent[0]>16384 || extent[1]>16384 || !std::isfinite(near_plane)
            || !std::isfinite(far_plane) || near_plane<=0 || far_plane<=near_plane
            || far_plane>1.e12 || (previous_index_offset && (previous_index_offset%4
                || previous_index_offset<=previous_vertex_offset))) return false;
        if(previous_ground && (!separated || !previous_ground->valid())) return false;
        if(previous_liquid && (!separated || previous_ground || !previous_liquid->valid())) return false;
        if(model_lobes && (!separated || (model_lobes!=1 && model_lobes!=8)
            || (previous_ground && !scene_paths) || (previous_liquid && !curved_paths))) return false;
        if(model_paths && !model_lobes) return false;
        if(scene_paths && !model_paths) return false;
        if(curved_paths && (!model_paths || scene_paths || !previous_liquid)) return false;
        if(curved_receivers && !curved_paths) return false;
        for(double value:projection) if(!std::isfinite(value) || std::abs(value)>1.e12) return false;
        return projection[0]>0 && projection[1]>0;
    }
};
// Exact native old-liquid input ABI, also used by opt-in diagnostics. Keeping
// this packing in one place prevents a diagnostic from replaying rounded or
// differently ordered frame values instead of the bytes the shader consumed.
inline std::array<float,32> reflection_liquid_frame_words(const RayReflectionHistory& history) noexcept {
    std::array<float,32> frame{};
    if(!history.previous_liquid) return frame;
    const auto& old=*history.previous_liquid;
    for(unsigned i=0;i<3;++i) {
        frame[i]=float(old.ground.point[i]);frame[4+i]=float(old.ground.normal[i]);
        for(unsigned j=0;j<3;++j) frame[8+i*4+j]=float(old.world_to_view[i*3+j]);
        frame[11+i*4]=float(old.offset[i]);
    }
    for(unsigned i=0;i<4;++i) frame[20+i]=float(history.projection[i]);
    frame[24]=float(history.extent[0]);frame[25]=float(history.extent[1]);
    frame[26]=float(history.near_plane);frame[27]=float(history.far_plane);
    frame[28]=float(old.time);frame[29]=float(old.material);
    return frame;
}
// Optional float4 plane: previous reflected pixel X/Y, previous primary
// receiver depth, validity. Invalid/no-hit/multibounce/rough samples are zero.
// Identity uint4: current primary/secondary and explicitly matched accepted
// primary/secondary indices. Witness float4: current secondary barycentrics,
// current primary depth, finite secondary-hit validity. No order inference.
// One allocation/fence/lifetime with RGBA. OFF retains the original RGBA ABI.
struct NativeReflectionHistory {
    std::uint32_t motion_offset{},storage_bytes{};
    std::array<unsigned,2> extent{};
    std::uint32_t identity_offset{},witness_offset{};
    // Optional separated radiance. incoming is encoded RGBA (A=255 for a
    // finite secondary hit); base/weight are CURRENT linear float4 planes.
    // Final RGB = base.rgb + incoming.rgb * weight.rgb. Do not transport the
    // base, Fresnel/metal tint, transmission or direct lighting with a hit.
    // base.w=1 validates this decomposition; weight.w=0 is reserved.
    std::uint32_t incoming_offset{},base_offset{},weight_offset{};
    // Compact MODEL-only ABI: one current primary uint/depth and response
    // float4, followed by model_lobes uint3 records per pixel (secondary ID,
    // UNORM16 barycentrics, incoming RGBA). No eight full motion images.
    // identity/witness/weight/incoming offsets address these planes. Legacy
    // motion_offset remains the four-byte prefix boundary, not a motion plane.
    unsigned model_lobes{};
    // Replaces each uint3 lobe with a 52-byte ordered path record; all other
    // offsets still address the shared primary prefix. Existing consumers
    // must not interpret its terminal/throughput words as legacy bary/RGBA.
    bool model_paths{};
    // Mixed analytic mirror/gold + finite models: 44-byte current prefix
    // (RGBA/primary/depth/response/base), followed by the same ordered paths.
    // Ground hops have an explicit identity; the additive current base is
    // never transported. One/eight records use 96/460 bytes per pixel.
    bool scene_paths{};
    // Distinct 64-byte current-base/ordered-liquid record. Existing 52-byte
    // consumers must explicitly decline it until complete-path qualification.
    bool curved_paths{};
    // Canonical RGBA/optional water planes, then current primary ID/depth,
    // response/base and 64-byte ordered lobes. Never aliases MODEL-only data.
    bool curved_receivers{};
    bool operator==(const NativeReflectionHistory&) const=default;
};
inline std::optional<NativeReflectionHistory> native_reflection_history(
    unsigned width,unsigned height,std::array<unsigned,2> previous_extent,bool separated=false,
    NativeWaterLayers liquid={},unsigned model_lobes=0,bool model_paths=false,bool scene_paths=false,
    bool curved_paths=false,bool curved_receivers=false) noexcept {
    const auto count=std::uint64_t(width)*height;
    unsigned liquid_prefix=4;
    if(liquid!=NativeWaterLayers{}) {
        const auto canonical=native_water_layers(width,height,liquid.world_offset!=0);
        if(!separated || !canonical || liquid!=*canonical) return std::nullopt;
        liquid_prefix=liquid.world_offset?24:20;
    }
    if(model_lobes) {
        // 52-byte ordered record: four mirror IDs, kind/count, terminal ID,
        // exact float3 feature/direction, incident RGBA and CURRENT float3
        // secondary throughput. Primary response stays in the existing prefix.
        const unsigned prefix=curved_receivers?liquid_prefix+40:scene_paths?44:28;
        const unsigned stride=prefix+(curved_paths?64:model_paths?52:12)*model_lobes;
        if(!separated || (liquid!=NativeWaterLayers{} && !curved_receivers) || (model_lobes!=1 && model_lobes!=8)
            || (scene_paths && !model_paths)
            || (curved_paths && (!model_paths || scene_paths))
            || (curved_receivers && !curved_paths)
            || !count || width>16384 || height>16384 || !previous_extent[0] || !previous_extent[1]
            || previous_extent[0]>16384 || previous_extent[1]>16384
            || count>std::numeric_limits<std::uint32_t>::max()/stride) return std::nullopt;
        const unsigned primary=curved_receivers?liquid_prefix:4;
        return NativeReflectionHistory{std::uint32_t(count*primary),std::uint32_t(count*stride),previous_extent,
            std::uint32_t(count*primary),std::uint32_t(count*(primary+4)),std::uint32_t(count*prefix),
            curved_receivers?std::uint32_t(count*(primary+24)):scene_paths?std::uint32_t(count*28):0,
            std::uint32_t(count*(primary+8)),model_lobes,model_paths,scene_paths,curved_paths,curved_receivers};
    }
    if(model_paths || scene_paths || curved_paths || curved_receivers) return std::nullopt;
    const unsigned prefix=liquid_prefix;
    // Append history AFTER the unchanged liquid ABI: RGBA, optional hidden
    // world RGBA and current surface float4. Those prefix planes are not
    // filtered or transported with the secondary hit. liquid.storage_bytes
    // remains its canonical prefix extent; storage_bytes below owns the
    // complete single allocation and lifetime (104/108 bytes per pixel).
    const unsigned stride=prefix+(separated?84:48);
    if(!count || width>16384 || height>16384 || !previous_extent[0] || !previous_extent[1]
        || previous_extent[0]>16384 || previous_extent[1]>16384
        || count>std::numeric_limits<std::uint32_t>::max()/stride) return std::nullopt;
    return NativeReflectionHistory{std::uint32_t(count*prefix),std::uint32_t(count*stride),previous_extent,
        std::uint32_t(count*(prefix+16)),std::uint32_t(count*(prefix+32)),separated?std::uint32_t(count*(prefix+48)):0,
        separated?std::uint32_t(count*(prefix+52)):0,separated?std::uint32_t(count*(prefix+68)):0};
}
} // namespace starfox::render::shadows
