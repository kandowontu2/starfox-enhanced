#pragma once
#include "starfox/vr/draw_packet.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace starfox::render {
// Native-only extension to a plain, GPU-flattened landscape packet. The
// headset validator rejects this extension unless explicitly enabled.
// Bit 29 is unused for TILE vertices (ordinary RGBA/indexed textures have a
// separate producer and interpretation). Bit 31 belongs to source particles.
inline constexpr std::uint32_t calibrated_ground_flag=0x20000000U;
inline constexpr std::size_t calibrated_ground_offset=272+16384+1;
inline constexpr std::size_t calibrated_ground_words=14;
// Authored finite BG2 floor identity, separate from object slots, grid and UI.
// Multiple occurrences remain ambiguous and must not share temporal history.
inline constexpr std::uint32_t calibrated_ground_source_key=0x40000U;
struct CalibratedGroundGradient {
    // Authored (not linearized) RGB, normalized to 0..1. Live source palette
    // endpoints are metadata, not a CPU-rendered terrain or eye image.
    std::array<float,3> far{},near{};
    float horizon{112},span{111};
    // 0 keeps the inexpensive palette gradient; 5 is water, 6/7 mirror/gold,
    // 8 is red sand, 9 is lava. Ray-enabled water uses analytic transmission.
    // Surface coordinates are retained source-world metadata, not an eye
    // image or CPU-evaluated waves. All eyes and reflection faces share them.
    unsigned material{};
    std::array<float,2> origin{};
    float seconds{},brightness{1};
    unsigned motion{};
};
inline bool apply_calibrated_ground(vr::DrawPacket& packet,const CalibratedGroundGradient& gradient) {
    for(const auto& endpoint:{gradient.far,gradient.near}) for(float value:endpoint)
        if(!std::isfinite(value) || value<0 || value>1)
            throw std::invalid_argument("Invalid calibrated ground endpoint");
    if(!std::isfinite(gradient.horizon) || !std::isfinite(gradient.span)
        || gradient.horizon<0 || gradient.horizon>223 || gradient.span<=0 || gradient.span>4096)
        throw std::invalid_argument("Invalid calibrated ground gradient extent");
    if((gradient.material!=0 && (gradient.material<5 || gradient.material>9))
        || gradient.motion>3 || !std::isfinite(gradient.seconds) || gradient.seconds<0
        || !std::isfinite(gradient.brightness) || gradient.brightness<0 || gradient.brightness>1)
        throw std::invalid_argument("Invalid calibrated ground surface");
    for(float value:gradient.origin) if(!std::isfinite(value) || std::abs(value)>65536)
        throw std::invalid_argument("Invalid calibrated ground origin");
    auto& words=packet.geometry.texels;
    if(packet.geometry.shared_texels || words.size()!=calibrated_ground_offset
        || !(words[15]&0x10000000U) || (words[15]&0x80000000U)
        || packet.geometry.vertex_view().empty()) return false;
    // The tile header's flag word is already fully occupied by authored
    // sky/atlas controls. Tag the native vertex ABI instead; never reinterpret
    // an existing landscape flag as an enhanced-ground setting.
    static thread_local std::shared_ptr<const std::vector<vr::SceneVertex>> cached_source,cached_flagged;
    if(!packet.geometry.shared_vertices || packet.geometry.shared_vertices!=cached_source) {
        const auto source=packet.geometry.vertex_view();
        auto flagged=std::make_shared<std::vector<vr::SceneVertex>>(source.begin(),source.end());
        for(auto& vertex:*flagged) vertex.texture[3]|=calibrated_ground_flag;
        cached_source=packet.geometry.shared_vertices;cached_flagged=std::move(flagged);
    }
    packet.geometry.vertices.clear();packet.geometry.shared_vertices=cached_flagged;
    for(float value:gradient.far) words.push_back(std::bit_cast<std::uint32_t>(value));
    for(float value:gradient.near) words.push_back(std::bit_cast<std::uint32_t>(value));
    words.push_back(std::bit_cast<std::uint32_t>(gradient.horizon));
    words.push_back(std::bit_cast<std::uint32_t>(gradient.span));
    words.push_back(gradient.material);
    // The static gradient never samples world coordinates; static red sand
    // samples its origin but not time. Canonicalize unused values BEFORE the
    // upload/held-frame exact comparison, not by weakening geometry identity.
    // Preserve active clocks/coordinates for liquids and animated receivers.
    const bool stationary=!gradient.motion && (gradient.material==0 || gradient.material==8);
    for(float value:gradient.origin)
        words.push_back(std::bit_cast<std::uint32_t>(stationary && gradient.material==0?0.F:value));
    words.push_back(std::bit_cast<std::uint32_t>(stationary?0.F:gradient.seconds));
    words.push_back(std::bit_cast<std::uint32_t>(gradient.brightness));
    words.push_back(gradient.motion);
    return true;
}
inline unsigned calibrated_ground_material(const vr::DrawPacket& packet) noexcept {
    const auto vertices=packet.geometry.vertex_view();const auto words=packet.geometry.texel_view();
    if(vertices.empty() || (vertices.front().texture[3]&(calibrated_ground_flag|8U))!=(calibrated_ground_flag|8U)) return 0;
    const auto offset=std::size_t(vertices.front().texture[0])+calibrated_ground_offset;
    if(offset>words.size() || words.size()-offset!=calibrated_ground_words) return 0;
    return words[offset+8];
}
inline bool calibrated_ground_ray_surface(unsigned material) noexcept {
    return (material>=5 && material<=7) || material==9;
}
struct CalibratedGroundMotionSource {
    float height{};
    unsigned material{},motion{};
    std::array<float,2> origin{};
    bool enhanced{};
};
// Recognize the authored GPU-flattened floor, not arbitrary sky triangles,
// colour, packet index or a model that happens to look like ground.
inline std::optional<CalibratedGroundMotionSource> calibrated_ground_motion_source(
    const vr::DrawPacket& packet) noexcept {
    const auto& g=packet.geometry;const auto v=g.vertex_view();const auto w=g.texel_view();
    if(packet.preserve_native_colour || v.empty() || v.size()%3 || !g.line_view().empty()
        || !g.deferred.empty() || !g.ranges.empty() || (g.shared_vertices && !g.vertices.empty())
        || (g.shared_texels && !g.texels.empty()) || v.front().texture[0]!=0
        || (v.front().texture[3]&8U)==0
        || (w.size()!=calibrated_ground_offset && w.size()!=calibrated_ground_offset+calibrated_ground_words)
        || !(w[15]&0x10000000U) || (w[15]&0x80000000U)) return {};
    CalibratedGroundMotionSource result;
    result.height=std::bit_cast<float>(w[calibrated_ground_offset-1]);
    result.enhanced=w.size()!=calibrated_ground_offset;
    if(!std::isfinite(result.height) || result.height>=0 || result.height< -8) return {};
    constexpr unsigned floor=0x10000000U;
    bool flattened=false;
    for(const auto& vertex:v) {
        const auto flags=vertex.texture[3];flattened|=(flags&floor)!=0;
        if(vertex.texture[0]!=0 || (flags&8U)==0 || (flags&~(2U|8U|floor|calibrated_ground_flag))
            || bool(flags&calibrated_ground_flag)!=result.enhanced
            || vertex.visibility_enabled || vertex.group_enabled) return {};
    }
    if(!flattened) return {};
    if(result.enhanced) {
        const auto at=calibrated_ground_offset;
        result.material=w[at+8];result.motion=w[at+13];
        if((result.material!=0 && (result.material<5 || result.material>9)) || result.motion>3) return {};
        for(unsigned i=0;i<2;++i) {
            result.origin[i]=std::bit_cast<float>(w[at+9+i]);
            if(!std::isfinite(result.origin[i]) || std::abs(result.origin[i])>65536) return {};
        }
        for(unsigned i=0;i<8;++i) {
            const float value=std::bit_cast<float>(w[at+i]);
            if(!std::isfinite(value) || value<0 || (i<6?value>1:i==6?value>223:value==0 || value>4096)) return {};
        }
        const float seconds=std::bit_cast<float>(w[at+11]),brightness=std::bit_cast<float>(w[at+12]);
        if(!std::isfinite(seconds) || seconds<0 || !std::isfinite(brightness) || brightness<0 || brightness>1) return {};
    }
    return result;
}
// Previous local coordinates of the SAME finite floor point. Heights are
// captured source metadata; static red-sand texture coordinates also retain
// their authored X/Z origin. No CPU vertices or per-pixel flow are generated.
// Fluid waves and view-dependent reflection radiance need their own history.
inline std::optional<vr::Matrix4> calibrated_ground_previous_model(
    const vr::DrawPacket& current,const vr::DrawPacket& previous,vr::Matrix4 model) noexcept {
    const auto now=calibrated_ground_motion_source(current),old=calibrated_ground_motion_source(previous);
    if(!now || !old || now->enhanced!=old->enhanced || now->material!=old->material
        || now->motion || old->motion || (now->material!=0 && now->material!=8)) return {};
    const auto a=current.geometry.vertex_view(),b=previous.geometry.vertex_view();
    if(a.size()!=b.size() || (a.data()!=b.data() && !std::equal(a.begin(),a.end(),b.begin()))) return {};
    const auto x=current.geometry.texel_view(),y=previous.geometry.texel_view();
    if(now->enhanced) {
        // Enhanced floor colour ignores the original sky's palette/atlas.
        // Immutable endpoints/material/ramp remain required. Time is unused
        // for these static materials and origin is accounted for below.
        for(unsigned i=0;i<=8;++i) if(x[calibrated_ground_offset+i]!=y[calibrated_ground_offset+i]) return {};
        if(x[calibrated_ground_offset+12]!=y[calibrated_ground_offset+12]) return {};
    } else if(!std::equal(x.begin(),x.begin()+calibrated_ground_offset-1,y.begin())) return {};
    const float delta[3]{now->material==8?(now->origin[0]-old->origin[0])/256.F:0.F,
        old->height-now->height,now->material==8?-(now->origin[1]-old->origin[1])/256.F:0.F};
    for(unsigned row=0;row<3;++row) {
        const double value=double(model[12+row])+double(model[row])*delta[0]
            +double(model[4+row])*delta[1]+double(model[8+row])*delta[2];
        if(!std::isfinite(value) || std::abs(value)>1.e8) return {};
        model[12+row]=float(value);
    }
    return model;
}
}
